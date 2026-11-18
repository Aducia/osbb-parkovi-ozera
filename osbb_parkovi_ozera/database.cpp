#include "database.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

namespace {

// Версія схеми зберігається в PRAGMA user_version.
// Якщо версія не збігається — файл створений іншою версією програми:
// його копія зберігається, а нова БД створюється з поточної схеми.
constexpr int kSchemaVersion = 2;

const QStringList &tableNames()
{
    static const QStringList names = {
        QStringLiteral("матеріали"), QStringLiteral("види_робіт"),
        QStringLiteral("персонал_підрядники"), QStringLiteral("квартири"),
        QStringLiteral("заявки"), QStringLiteral("списання_матеріалів"),
        QStringLiteral("доплати"), QStringLiteral("оплати")};
    return names;
}

bool execSql(const QString &sql, QString *error = nullptr)
{
    QSqlQuery q(QSqlDatabase::database());
    if (q.exec(sql))
        return true;
    if (error)
        *error = q.lastError().text() + QStringLiteral("\n") + sql;
    return false;
}

QVariant scalar(const QString &sql)
{
    QSqlQuery q(QSqlDatabase::database());
    if (q.exec(sql) && q.next())
        return q.value(0);
    return {};
}

// CREATE TRIGGER завершується лише після "END;" (всередині BEGIN ... END є свої ';').
bool statementComplete(const QString &statement)
{
    const QString t = statement.trimmed().toUpper();
    if (!t.startsWith(QStringLiteral("CREATE TRIGGER")))
        return true;
    static const QRegularExpression endRe(QStringLiteral("\\bEND$"));
    return endRe.match(t).hasMatch();
}

// Розбиває SQL-скрипт на окремі команди: ';' поза лапками,
// коментарі "--" до кінця рядка пропускаються.
QStringList splitSqlScript(const QString &script)
{
    QStringList result;
    QString current;
    bool inSingle = false, inDouble = false, inComment = false;

    for (int i = 0; i < script.size(); ++i) {
        const QChar c = script.at(i);
        const QChar next = (i + 1 < script.size()) ? script.at(i + 1) : QChar();

        if (inComment) {
            if (c == u'\n') {
                inComment = false;
                current += c;
            }
            continue;
        }
        if (!inSingle && !inDouble && c == u'-' && next == u'-') {
            inComment = true;
            ++i;
            continue;
        }
        if (c == u'\'' && !inDouble)
            inSingle = !inSingle;
        else if (c == u'"' && !inSingle)
            inDouble = !inDouble;

        if (c == u';' && !inSingle && !inDouble && statementComplete(current)) {
            if (!current.trimmed().isEmpty())
                result << current.trimmed();
            current.clear();
            continue;
        }
        current += c;
    }
    if (!current.trimmed().isEmpty())
        result << current.trimmed();
    return result;
}

bool enableForeignKeys(QString *error)
{
    return execSql(QStringLiteral("PRAGMA foreign_keys = ON"), error);
}

} // namespace

QString Database::filePath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("parkovi_ozera.db"));
}

QString Database::displayName()
{
    return QStringLiteral("SQLite — ") + QDir::toNativeSeparators(filePath());
}

bool Database::runScript(const QString &resourcePath, QString *error)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        *error = QStringLiteral("Не знайдено вбудований скрипт ") + resourcePath;
        return false;
    }
    const QString script = QString::fromUtf8(file.readAll());
    file.close();

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.transaction()) {
        *error = db.lastError().text();
        return false;
    }
    for (const QString &statement : splitSqlScript(script)) {
        QString err;
        if (!execSql(statement, &err)) {
            db.rollback();
            *error = QStringLiteral("Помилка у скрипті ") + resourcePath + QStringLiteral(":\n") + err;
            return false;
        }
    }
    // Відкладені перевірки (якщо є) виконуються під час COMMIT
    if (!db.commit()) {
        *error = db.lastError().text();
        db.rollback();
        return false;
    }
    return true;
}

bool Database::open(QString *error)
{
    QSqlDatabase db = QSqlDatabase::contains()
                          ? QSqlDatabase::database()
                          : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    if (db.isOpen())
        return true;

    const QString path = filePath();
    db.setDatabaseName(path);
    if (!db.open()) {
        *error = db.lastError().text();
        return false;
    }
    if (!enableForeignKeys(error))
        return false;

    const int version = scalar(QStringLiteral("PRAGMA user_version")).toInt();
    if (version != kSchemaVersion) {
        const int existingTables = scalar(QStringLiteral(
            "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%'")).toInt();

        if (existingTables > 0) {
            // Файл створений іншою версією програми: зберігаємо копію і створюємо нову БД.
            db.close();
            const QString backup = QDir(QCoreApplication::applicationDirPath()).filePath(
                QStringLiteral("parkovi_ozera_old_%1.db")
                    .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))));
            if (!QFile::rename(path, backup)) {
                *error = QStringLiteral("Знайдено БД іншої версії, але її не вдалося перейменувати:\n") + path;
                return false;
            }
            if (!db.open()) {
                *error = db.lastError().text();
                return false;
            }
            if (!enableForeignKeys(error))
                return false;
        }

        if (!runScript(QStringLiteral(":/sql/schema.sql"), error))
            return false;
        if (!execSql(QStringLiteral("PRAGMA user_version = %1").arg(kSchemaVersion), error))
            return false;
    }

    // Тестові дані — лише в повністю порожню БД
    int rows = 0;
    for (const QString &table : tableNames())
        rows += scalar(QStringLiteral("SELECT COUNT(*) FROM ") + table).toInt();
    if (rows == 0 && !runScript(QStringLiteral(":/sql/seed.sql"), error))
        return false;

    return true;
}

QString Database::fieldTitle(const QString &field)
{
    static const QHash<QString, QString> titles = {
        {QStringLiteral("код"), QStringLiteral("Код")},
        {QStringLiteral("назва"), QStringLiteral("Назва")},
        {QStringLiteral("одиниця"), QStringLiteral("Од. виміру")},
        {QStringLiteral("ціна"), QStringLiteral("Ціна, грн")},
        {QStringLiteral("залишок"), QStringLiteral("Залишок на складі")},
        {QStringLiteral("вид_роботи"), QStringLiteral("Вид роботи")},
        {QStringLiteral("вартість"), QStringLiteral("Вартість роботи, грн")},
        {QStringLiteral("кількість_робітників"), QStringLiteral("Кількість робітників")},
        {QStringLiteral("тривалість"), QStringLiteral("Тривалість, днів")},
        {QStringLiteral("матеріал"), QStringLiteral("Матеріал")},
        {QStringLiteral("норма_матеріалу"), QStringLiteral("Норма матеріалу")},
        {QStringLiteral("ПІБ"), QStringLiteral("ПІБ / організація")},
        {QStringLiteral("тип"), QStringLiteral("Тип")},
        {QStringLiteral("посада"), QStringLiteral("Посада")},
        {QStringLiteral("телефон"), QStringLiteral("Телефон")},
        {QStringLiteral("адреса"), QStringLiteral("Адреса")},
        {QStringLiteral("паспорт"), QStringLiteral("Паспорт / ЄДРПОУ")},
        {QStringLiteral("стаж"), QStringLiteral("Стаж, років")},
        {QStringLiteral("корпус"), QStringLiteral("Корпус")},
        {QStringLiteral("під_їзд"), QStringLiteral("Під’їзд")},
        {QStringLiteral("номер"), QStringLiteral("№ квартири")},
        {QStringLiteral("площа"), QStringLiteral("Площа, м²")},
        {QStringLiteral("власник"), QStringLiteral("Власник")},
        {QStringLiteral("дата_заявки"), QStringLiteral("Дата заявки")},
        {QStringLiteral("джерело"), QStringLiteral("Джерело")},
        {QStringLiteral("тип_локації"), QStringLiteral("Тип локації")},
        {QStringLiteral("місце"), QStringLiteral("Місце / адреса")},
        {QStringLiteral("квартира"), QStringLiteral("Квартира")},
        {QStringLiteral("опис"), QStringLiteral("Опис")},
        {QStringLiteral("пріоритет"), QStringLiteral("Пріоритет")},
        {QStringLiteral("статус"), QStringLiteral("Статус")},
        {QStringLiteral("виконавець"), QStringLiteral("Виконавець")},
        {QStringLiteral("дата_початку"), QStringLiteral("Дата початку")},
        {QStringLiteral("дата_закінчення"), QStringLiteral("Дата завершення")},
        {QStringLiteral("дата"), QStringLiteral("Дата")},
        {QStringLiteral("заявка"), QStringLiteral("Заявка")},
        {QStringLiteral("кількість"), QStringLiteral("Кількість")},
        {QStringLiteral("сума"), QStringLiteral("Сума, грн")},
        {QStringLiteral("рахунок"), QStringLiteral("Рахунок")},
        {QStringLiteral("призначення"), QStringLiteral("Призначення")},
        {QStringLiteral("нараховано"), QStringLiteral("Нараховано, грн")},
        {QStringLiteral("тип_доплати"), QStringLiteral("Тип доплати")},
        {QStringLiteral("доплата"), QStringLiteral("Доплата, грн")},
        {QStringLiteral("сплачено"), QStringLiteral("Сплачено, грн")},
        {QStringLiteral("борг"), QStringLiteral("Борг, грн")},
    };
    return titles.value(field, field);
}

QString Database::friendlyError(const QSqlError &error)
{
    QString text = error.databaseText();
    if (text.isEmpty())
        text = error.text();

    if (text.contains(QStringLiteral("FOREIGN KEY constraint failed"))) {
        return QStringLiteral("Порушено зв’язок між таблицями.\n\n"
                              "Можливі причини:\n"
                              "• запис використовується в інших таблицях (наприклад, квартира, працівник "
                              "або вид робіт указані в заявці), тому його не можна видалити;\n"
                              "• вибрано посилання на запис, якого не існує.");
    }

    static const QRegularExpression notNull(QStringLiteral("NOT NULL constraint failed: [^.\\s]+\\.(\\S+)"));
    const QRegularExpressionMatch nn = notNull.match(text);
    if (nn.hasMatch())
        return QStringLiteral("Не заповнено обов’язкове поле «%1».").arg(fieldTitle(nn.captured(1)));

    if (text.contains(QStringLiteral("UNIQUE constraint failed"))) {
        if (text.contains(QStringLiteral("персонал_підрядники.паспорт")))
            return QStringLiteral("Працівник із таким паспортом уже є в базі.");
        if (text.contains(QStringLiteral("квартири.корпус")))
            return QStringLiteral("Квартира з таким номером у цьому корпусі вже є.");
        if (text.contains(QStringLiteral("матеріали.назва")))
            return QStringLiteral("Матеріал з такою назвою вже є на складі.");
        if (text.contains(QStringLiteral("види_робіт.вид_роботи")))
            return QStringLiteral("Такий вид робіт уже є в довіднику.");
        if (text.contains(QStringLiteral("доплати.назва")))
            return QStringLiteral("Доплата з такою назвою вже є.");
        return QStringLiteral("Запис з таким значенням уже існує.");
    }

    static const QHash<QString, QString> checks = {
        {QStringLiteral("ck_mat_price"), QStringLiteral("Ціна не може бути від’ємною.")},
        {QStringLiteral("ck_mat_stock"), QStringLiteral("Недостатньо матеріалу на складі: залишок не може бути від’ємним. "
                                                        "Перевірте кількість списання або поповніть залишок у довіднику «Матеріали».")},
        {QStringLiteral("ck_work_cost"), QStringLiteral("Вартість не може бути від’ємною.")},
        {QStringLiteral("ck_work_workers"), QStringLiteral("Кількість робітників має бути більшою за нуль.")},
        {QStringLiteral("ck_work_duration"), QStringLiteral("Тривалість має бути більшою за нуль.")},
        {QStringLiteral("ck_work_norm"), QStringLiteral("Норма матеріалу не може бути від’ємною.")},
        {QStringLiteral("ck_staff_type"), QStringLiteral("Тип має бути «Штатний» або «Підрядник».")},
        {QStringLiteral("ck_staff_experience"), QStringLiteral("Стаж не може бути від’ємним.")},
        {QStringLiteral("ck_flat_entrance"), QStringLiteral("Номер під’їзду має бути більшим за нуль.")},
        {QStringLiteral("ck_flat_number"), QStringLiteral("Номер квартири має бути більшим за нуль.")},
        {QStringLiteral("ck_flat_area"), QStringLiteral("Площа має бути більшою за нуль.")},
        {QStringLiteral("ck_req_date"), QStringLiteral("Дата заявки має бути коректною датою.")},
        {QStringLiteral("ck_req_source"), QStringLiteral("Недопустиме джерело заявки.")},
        {QStringLiteral("ck_req_location"), QStringLiteral("Недопустимий тип локації.")},
        {QStringLiteral("ck_req_priority"), QStringLiteral("Недопустимий пріоритет заявки.")},
        {QStringLiteral("ck_req_status"), QStringLiteral("Недопустимий статус заявки.")},
        {QStringLiteral("ck_req_start"), QStringLiteral("Дата початку має бути коректною датою.")},
        {QStringLiteral("ck_req_end"), QStringLiteral("Дата завершення має бути коректною датою.")},
        {QStringLiteral("ck_req_period"), QStringLiteral("Дата завершення не може бути раніше дати початку, "
                                                         "і не може бути вказана без дати початку.")},
        {QStringLiteral("ck_req_flat"), QStringLiteral("Для локації «Квартира» потрібно вказати квартиру.")},
        {QStringLiteral("ck_write_date"), QStringLiteral("Дата списання має бути коректною датою.")},
        {QStringLiteral("ck_write_qty"), QStringLiteral("Кількість для списання має бути більшою за нуль.")},
        {QStringLiteral("ck_write_price"), QStringLiteral("Ціна не може бути від’ємною.")},
        {QStringLiteral("ck_write_source"), QStringLiteral("Джерело оплати має бути «Загальний фонд» або «Власник квартири».")},
        {QStringLiteral("ck_surcharge_sum"), QStringLiteral("Сума доплати не може бути від’ємною.")},
        {QStringLiteral("ck_pay_date"), QStringLiteral("Дата оплати має бути коректною датою.")},
        {QStringLiteral("ck_pay_account"), QStringLiteral("Рахунок має бути «Загальний фонд» або «Індивідуальний».")},
        {QStringLiteral("ck_pay_charged"), QStringLiteral("Нарахована сума не може бути від’ємною.")},
        {QStringLiteral("ck_pay_surcharge"), QStringLiteral("Доплата не може бути від’ємною.")},
        {QStringLiteral("ck_pay_paid"), QStringLiteral("Сплачена сума не може бути від’ємною.")},
        {QStringLiteral("ck_pay_limit"), QStringLiteral("Сплачена сума не може перевищувати нараховану з доплатою.")},
        {QStringLiteral("ck_pay_flat"), QStringLiteral("Для індивідуального рахунку потрібно вказати квартиру.")},
    };
    if (text.contains(QStringLiteral("CHECK constraint failed"))) {
        for (auto it = checks.cbegin(); it != checks.cend(); ++it) {
            if (text.contains(it.key()))
                return it.value();
        }
        return QStringLiteral("Значення не відповідає обмеженням бази даних.\n\n") + text;
    }

    return QStringLiteral("Помилка бази даних:\n") + text;
}
