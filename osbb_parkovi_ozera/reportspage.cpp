#include "reportspage.h"

#include "reportmodel.h"
#include "style.h"

#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTableView>
#include <QTextStream>
#include <QVBoxLayout>
#include <utility>

namespace {

struct Total {
    QString label;
    bool money;
};

struct Report {
    QString title;
    QString description;
    QString sql;
    bool usesPeriod;
    QString totalSql;          // підсумковий запит (одна строка)
    QList<Total> totals;       // підписи колонок підсумку
};

const QList<Report> &reports()
{
    static const QList<Report> list = {
        {QStringLiteral("Заявки за період"),
         QStringLiteral("Заявки, зареєстровані у вибраному періоді: пріоритет, статус, місце, вид робіт, виконавець і терміни."),
         QStringLiteral(R"SQL(
SELECT z.код AS "№", z.дата_заявки AS "Дата", z.пріоритет AS "Пріоритет", z.статус AS "Статус",
       z.тип_локації AS "Тип локації", z.місце AS "Місце",
       k.корпус || ', кв. ' || k.номер AS "Квартира",
       v.вид_роботи AS "Вид роботи", p.ПІБ AS "Виконавець",
       z.дата_початку AS "Початок", z.дата_закінчення AS "Завершення"
FROM заявки z
JOIN види_робіт v ON v.код = z.вид_роботи
LEFT JOIN персонал_підрядники p ON p.код = z.виконавець
LEFT JOIN квартири k ON k.код = z.квартира
WHERE z.дата_заявки BETWEEN :d1 AND :d2
ORDER BY z.дата_заявки, z.код
)SQL"),
         true,
         QStringLiteral(R"SQL(
SELECT COUNT(*),
       COALESCE(SUM(CASE WHEN пріоритет = 'Аварійна' THEN 1 ELSE 0 END), 0),
       COALESCE(SUM(CASE WHEN статус IN ('Нова', 'Призначена', 'В роботі') THEN 1 ELSE 0 END), 0)
FROM заявки
WHERE дата_заявки BETWEEN :d1 AND :d2
)SQL"),
         {{QStringLiteral("Заявок"), false}, {QStringLiteral("Аварійних"), false}, {QStringLiteral("Відкритих"), false}}},

        {QStringLiteral("Вартість заявок за період"),
         QStringLiteral("Вартість робіт за довідником, матеріали із загального фонду та за рахунок власників, виставлені й сплачені суми по кожній заявці."),
         QStringLiteral(R"SQL(
SELECT z.код AS "№", z.дата_заявки AS "Дата", v.вид_роботи AS "Вид роботи", z.статус AS "Статус",
       v.вартість AS "Робота, грн",
       (SELECT COALESCE(SUM(s.сума), 0) FROM списання_матеріалів s
         WHERE s.заявка = z.код AND s.джерело = 'Загальний фонд') AS "Матеріали (загальний фонд), грн",
       (SELECT COALESCE(SUM(s.сума), 0) FROM списання_матеріалів s
         WHERE s.заявка = z.код AND s.джерело = 'Власник квартири') AS "Матеріали (власник), грн",
       (SELECT COALESCE(SUM(o.нараховано + o.доплата), 0) FROM оплати o
         WHERE o.заявка = z.код) AS "Виставлено власнику, грн",
       (SELECT COALESCE(SUM(o.сплачено), 0) FROM оплати o
         WHERE o.заявка = z.код) AS "Сплачено, грн"
FROM заявки z
JOIN види_робіт v ON v.код = z.вид_роботи
WHERE z.дата_заявки BETWEEN :d1 AND :d2 AND z.статус <> 'Скасована'
ORDER BY z.дата_заявки, z.код
)SQL"),
         true,
         QStringLiteral(R"SQL(
SELECT COUNT(*), COALESCE(SUM(v.вартість), 0),
       COALESCE(SUM((SELECT COALESCE(SUM(s.сума), 0) FROM списання_матеріалів s
                      WHERE s.заявка = z.код AND s.джерело = 'Загальний фонд')), 0)
FROM заявки z
JOIN види_робіт v ON v.код = z.вид_роботи
WHERE z.дата_заявки BETWEEN :d1 AND :d2 AND z.статус <> 'Скасована'
)SQL"),
         {{QStringLiteral("Заявок"), false}, {QStringLiteral("Вартість робіт"), true}, {QStringLiteral("Матеріали з загального фонду"), true}}},

        {QStringLiteral("Оплати за період"),
         QStringLiteral("Усі нарахування й платежі за період по обох рахунках — загальний фонд та індивідуальні рахунки квартир, з доплатами й боргом."),
         QStringLiteral(R"SQL(
SELECT o.дата AS "Дата", o.рахунок AS "Рахунок",
       k.корпус || ', кв. ' || k.номер AS "Квартира", k.власник AS "Власник",
       o.призначення AS "Призначення",
       o.нараховано AS "Нараховано, грн", o.доплата AS "Доплата, грн",
       o.сплачено AS "Сплачено, грн",
       o.нараховано + o.доплата - o.сплачено AS "Борг, грн"
FROM оплати o
LEFT JOIN квартири k ON k.код = o.квартира
WHERE o.дата BETWEEN :d1 AND :d2
ORDER BY o.дата, o.код
)SQL"),
         true,
         QStringLiteral(R"SQL(
SELECT COALESCE(SUM(нараховано + доплата), 0), COALESCE(SUM(сплачено), 0),
       COALESCE(SUM(нараховано + доплата - сплачено), 0), COALESCE(SUM(доплата), 0)
FROM оплати
WHERE дата BETWEEN :d1 AND :d2
)SQL"),
         {{QStringLiteral("Нараховано (з доплатами)"), true}, {QStringLiteral("Сплачено"), true}, {QStringLiteral("Борг"), true}, {QStringLiteral("у т.ч. доплати"), true}}},

        {QStringLiteral("Загальний фонд за період"),
         QStringLiteral("Цільові внески на утримання будинку й території та централізовані витрати на матеріали із загального фонду; сальдо = сплачені внески − витрати."),
         QStringLiteral(R"SQL(
SELECT 'Цільові внески: нараховано' AS "Стаття",
       COALESCE(SUM(нараховано + доплата), 0) AS "Сума, грн"
FROM оплати WHERE рахунок = 'Загальний фонд' AND дата BETWEEN :d1 AND :d2
UNION ALL
SELECT 'Цільові внески: сплачено', COALESCE(SUM(сплачено), 0)
FROM оплати WHERE рахунок = 'Загальний фонд' AND дата BETWEEN :d1 AND :d2
UNION ALL
SELECT 'Заборгованість за внесками', COALESCE(SUM(нараховано + доплата - сплачено), 0)
FROM оплати WHERE рахунок = 'Загальний фонд' AND дата BETWEEN :d1 AND :d2
UNION ALL
SELECT 'Витрати на матеріали із загального фонду', COALESCE(SUM(сума), 0)
FROM списання_матеріалів WHERE джерело = 'Загальний фонд' AND дата BETWEEN :d1 AND :d2
)SQL"),
         true,
         QStringLiteral(R"SQL(
SELECT (SELECT COALESCE(SUM(сплачено), 0) FROM оплати
         WHERE рахунок = 'Загальний фонд' AND дата BETWEEN :d1 AND :d2),
       (SELECT COALESCE(SUM(сума), 0) FROM списання_матеріалів
         WHERE джерело = 'Загальний фонд' AND дата BETWEEN :d1 AND :d2),
       (SELECT COALESCE(SUM(сплачено), 0) FROM оплати
         WHERE рахунок = 'Загальний фонд' AND дата BETWEEN :d1 AND :d2)
     - (SELECT COALESCE(SUM(сума), 0) FROM списання_матеріалів
         WHERE джерело = 'Загальний фонд' AND дата BETWEEN :d1 AND :d2)
)SQL"),
         {{QStringLiteral("Надходження"), true}, {QStringLiteral("Витрати на матеріали"), true}, {QStringLiteral("Сальдо"), true}}},

        {QStringLiteral("Списання матеріалів за період"),
         QStringLiteral("Скільки й на яку суму списано кожного матеріалу, з розподілом між загальним фондом і власниками квартир; залишок на складі."),
         QStringLiteral(R"SQL(
SELECT m.назва AS "Матеріал", m.одиниця AS "Од.",
       SUM(s.кількість) AS "Списано", SUM(s.сума) AS "Сума, грн",
       SUM(CASE WHEN s.джерело = 'Загальний фонд' THEN s.сума ELSE 0 END) AS "Загальний фонд, грн",
       SUM(CASE WHEN s.джерело = 'Власник квартири' THEN s.сума ELSE 0 END) AS "Власники, грн",
       m.залишок AS "Залишок на складі"
FROM списання_матеріалів s
JOIN матеріали m ON m.код = s.матеріал
WHERE s.дата BETWEEN :d1 AND :d2
GROUP BY m.код
ORDER BY SUM(s.сума) DESC, m.назва
)SQL"),
         true,
         QStringLiteral(R"SQL(
SELECT COUNT(DISTINCT матеріал), COUNT(*), COALESCE(SUM(сума), 0)
FROM списання_матеріалів
WHERE дата BETWEEN :d1 AND :d2
)SQL"),
         {{QStringLiteral("Матеріалів"), false}, {QStringLiteral("Списань"), false}, {QStringLiteral("Сума списання"), true}}},

        {QStringLiteral("Заборгованість квартир"),
         QStringLiteral("Квартири з боргом: окремо за загальним фондом (внески) та індивідуальними рахунками. Період не використовується."),
         QStringLiteral(R"SQL(
SELECT k.корпус || ', кв. ' || k.номер AS "Квартира", k.власник AS "Власник", k.телефон AS "Телефон",
       SUM(CASE WHEN o.рахунок = 'Загальний фонд'
                THEN o.нараховано + o.доплата - o.сплачено ELSE 0 END) AS "Борг: загальний фонд, грн",
       SUM(CASE WHEN o.рахунок = 'Індивідуальний'
                THEN o.нараховано + o.доплата - o.сплачено ELSE 0 END) AS "Борг: індивідуальний, грн",
       SUM(o.нараховано + o.доплата - o.сплачено) AS "Разом, грн"
FROM квартири k
JOIN оплати o ON o.квартира = k.код
GROUP BY k.код
HAVING SUM(o.нараховано + o.доплата - o.сплачено) > 0.005
ORDER BY SUM(o.нараховано + o.доплата - o.сплачено) DESC
)SQL"),
         false,
         QStringLiteral(R"SQL(
SELECT COUNT(*), COALESCE(SUM(d), 0)
FROM (SELECT SUM(нараховано + доплата - сплачено) AS d
        FROM оплати WHERE квартира IS NOT NULL
        GROUP BY квартира HAVING SUM(нараховано + доплата - сплачено) > 0.005)
)SQL"),
         {{QStringLiteral("Квартир з боргом"), false}, {QStringLiteral("Загальний борг"), true}}},

        {QStringLiteral("Персонал та підрядники"),
         QStringLiteral("Штатні працівники й підрядні організації з кількістю призначених, відкритих і виконаних заявок."),
         QStringLiteral(R"SQL(
SELECT p.код AS "Код", p.ПІБ AS "ПІБ / організація", p.тип AS "Тип", p.посада AS "Посада",
       p.телефон AS "Телефон", p.стаж AS "Стаж, років",
       COUNT(z.код) AS "Заявок усього",
       COALESCE(SUM(CASE WHEN z.статус IN ('Нова', 'Призначена', 'В роботі') THEN 1 ELSE 0 END), 0) AS "У роботі",
       COALESCE(SUM(CASE WHEN z.статус = 'Виконана' THEN 1 ELSE 0 END), 0) AS "Виконано"
FROM персонал_підрядники p
LEFT JOIN заявки z ON z.виконавець = p.код
GROUP BY p.код
ORDER BY p.тип, p.посада, p.ПІБ
)SQL"),
         false,
         QStringLiteral(R"SQL(
SELECT COUNT(*), COALESCE(SUM(CASE WHEN тип = 'Підрядник' THEN 1 ELSE 0 END), 0)
FROM персонал_підрядники
)SQL"),
         {{QStringLiteral("Записів"), false}, {QStringLiteral("з них підрядників"), false}}},

        {QStringLiteral("Види робіт та кошторисні норми"),
         QStringLiteral("Довідник видів робіт: вартість, тривалість, норма матеріалу та кількість заявок."),
         QStringLiteral(R"SQL(
SELECT v.вид_роботи AS "Вид робіт", v.вартість AS "Вартість, грн", v.тривалість AS "Тривалість, днів",
       m.назва AS "Матеріал за нормою", v.норма_матеріалу AS "Норма", m.одиниця AS "Од.",
       COUNT(z.код) AS "Заявок", v.вартість * COUNT(z.код) AS "Сума робіт, грн"
FROM види_робіт v
LEFT JOIN матеріали m ON m.код = v.матеріал
LEFT JOIN заявки z ON z.вид_роботи = v.код AND z.статус <> 'Скасована'
GROUP BY v.код
ORDER BY COUNT(z.код) DESC, v.вид_роботи
)SQL"),
         false,
         QStringLiteral(R"SQL(
SELECT COUNT(*), COALESCE(SUM(v.вартість), 0)
FROM заявки z
JOIN види_робіт v ON v.код = z.вид_роботи
WHERE z.статус <> 'Скасована'
)SQL"),
         {{QStringLiteral("Заявок"), false}, {QStringLiteral("Сума робіт за довідником"), true}}},

    };
    return list;
}

enum Preset { Custom, ThisMonth, LastMonth, ThisYear, AllTime };

// Підставляє межі періоду в запит. Дати формує QDate (лише цифри й «-»), тому ін'єкції неможливі;
// підстановка тексту дозволяє використовувати :d1 і :d2 у запиті скільки завгодно разів.
QString applyPeriod(QString sql, const QDate &from, const QDate &to)
{
    sql.replace(QStringLiteral(":d1"), QStringLiteral("'") + from.toString(Qt::ISODate) + QStringLiteral("'"));
    sql.replace(QStringLiteral(":d2"), QStringLiteral("'") + to.toString(Qt::ISODate) + QStringLiteral("'"));
    return sql;
}

QString formatNumber(const QVariant &v, bool money)
{
    if (money)
        return QLocale().toString(v.toDouble(), 'f', 2) + QStringLiteral(" грн");
    const double d = v.toDouble();
    return (d == static_cast<qlonglong>(d)) ? QString::number(static_cast<qlonglong>(d))
                                            : QLocale().toString(d, 'f', 1);
}

} // namespace

ReportsPage::ReportsPage(QWidget *parent)
    : QWidget(parent)
{
    model_ = new ReportModel(this);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 18, 24, 18);
    root->setSpacing(12);

    // Панель параметрів
    auto *params = new QFrame(this);
    params->setObjectName(QStringLiteral("panel"));
    auto *paramsLayout = new QVBoxLayout(params);
    paramsLayout->setContentsMargins(16, 12, 16, 12);
    paramsLayout->setSpacing(8);

    auto *row1 = new QHBoxLayout();
    row1->setSpacing(8);
    row1->addWidget(new QLabel(QStringLiteral("Звіт:"), params));
    combo_ = new QComboBox(params);
    for (const Report &r : reports())
        combo_->addItem(r.title);
    combo_->setMinimumWidth(300);
    row1->addWidget(combo_, 1);

    row1->addSpacing(12);
    row1->addWidget(new QLabel(QStringLiteral("Період:"), params));
    preset_ = new QComboBox(params);
    preset_->addItem(QStringLiteral("Довільний"), Custom);
    preset_->addItem(QStringLiteral("Поточний місяць"), ThisMonth);
    preset_->addItem(QStringLiteral("Попередній місяць"), LastMonth);
    preset_->addItem(QStringLiteral("Поточний рік"), ThisYear);
    preset_->addItem(QStringLiteral("Увесь час"), AllTime);
    row1->addWidget(preset_);

    row1->addWidget(new QLabel(QStringLiteral("з"), params));
    from_ = new QDateEdit(params);
    row1->addWidget(from_);
    row1->addWidget(new QLabel(QStringLiteral("по"), params));
    to_ = new QDateEdit(params);
    row1->addWidget(to_);
    for (QDateEdit *e : {from_, to_}) {
        e->setCalendarPopup(true);
        e->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
        e->setDateRange(QDate(2000, 1, 1), QDate(2100, 12, 31));
    }
    paramsLayout->addLayout(row1);

    auto *row2 = new QHBoxLayout();
    description_ = new QLabel(params);
    description_->setObjectName(QStringLiteral("hint"));
    description_->setWordWrap(true);
    row2->addWidget(description_, 1);
    runButton_ = new QPushButton(QStringLiteral("Сформувати"), params);
    Style::setKind(runButton_, QStringLiteral("primary"));
    csvButton_ = new QPushButton(QStringLiteral("Експорт у CSV"), params);
    row2->addWidget(runButton_);
    row2->addWidget(csvButton_);
    paramsLayout->addLayout(row2);
    root->addWidget(params);

    // Результат
    view_ = new QTableView(this);
    view_->setModel(model_);
    view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    view_->setAlternatingRowColors(true);
    view_->setWordWrap(false);
    view_->verticalHeader()->hide();
    view_->verticalHeader()->setDefaultSectionSize(28);
    view_->horizontalHeader()->setStretchLastSection(true);
    view_->horizontalHeader()->setHighlightSections(false);
    view_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    root->addWidget(view_, 1);

    summary_ = new QLabel(this);
    summary_->setObjectName(QStringLiteral("summary"));
    summary_->hide();
    root->addWidget(summary_);

    connect(combo_, &QComboBox::currentIndexChanged, this, &ReportsPage::reportChanged);
    connect(preset_, &QComboBox::currentIndexChanged, this, &ReportsPage::presetChanged);
    for (QDateEdit *e : {from_, to_}) {
        connect(e, &QDateEdit::dateChanged, this, [this] {
            const QSignalBlocker block(preset_);
            preset_->setCurrentIndex(preset_->findData(Custom));
        });
    }
    connect(runButton_, &QPushButton::clicked, this, &ReportsPage::run);
    connect(csvButton_, &QPushButton::clicked, this, &ReportsPage::exportCsv);

    preset_->setCurrentIndex(preset_->findData(ThisYear));   // викликає presetChanged()
    reportChanged();
}

void ReportsPage::presetChanged()
{
    const QDate today = QDate::currentDate();
    QDate from, to;
    switch (preset_->currentData().toInt()) {
    case ThisMonth:
        from = QDate(today.year(), today.month(), 1);
        to = from.addMonths(1).addDays(-1);
        break;
    case LastMonth:
        from = QDate(today.year(), today.month(), 1).addMonths(-1);
        to = from.addMonths(1).addDays(-1);
        break;
    case ThisYear:
        from = QDate(today.year(), 1, 1);
        to = QDate(today.year(), 12, 31);
        break;
    case AllTime:
        from = QDate(2000, 1, 1);
        to = QDate(2100, 12, 31);
        break;
    default:
        return;     // довільний — дати не змінюємо
    }
    {
        const QSignalBlocker b1(from_);
        const QSignalBlocker b2(to_);
        from_->setDate(from);
        to_->setDate(to);
    }
    run();
}

void ReportsPage::reportChanged()
{
    const Report &r = reports().at(combo_->currentIndex());
    const bool period = r.usesPeriod;
    preset_->setEnabled(period);
    from_->setEnabled(period);
    to_->setEnabled(period);
    description_->setText(r.description + (period ? QString() : QStringLiteral(" Період не використовується.")));
    run();
}

void ReportsPage::run()
{
    const Report &r = reports().at(combo_->currentIndex());
    if (r.usesPeriod && from_->date() > to_->date()) {
        QMessageBox::warning(this, QStringLiteral("Період"),
                             QStringLiteral("Дата початку періоду пізніша за дату кінця."));
        return;
    }

    QSqlQuery q;
    if (!q.exec(applyPeriod(r.sql, from_->date(), to_->date()))) {
        QMessageBox::warning(this, QStringLiteral("Помилка запиту"), q.lastError().text());
        return;
    }
    model_->setQuery(std::move(q));
    while (model_->canFetchMore())
        model_->fetchMore();

    view_->resizeColumnsToContents();
    for (int c = 0; c < model_->columnCount(); ++c)
        view_->setColumnWidth(c, qMin(view_->columnWidth(c) + 16, 360));

    // Підсумок
    QStringList parts;
    parts << QStringLiteral("Рядків у звіті: %1").arg(model_->rowCount());
    if (!r.totalSql.isEmpty()) {
        QSqlQuery t;
        if (t.exec(applyPeriod(r.totalSql, from_->date(), to_->date())) && t.next()) {
            for (int i = 0; i < r.totals.size(); ++i)
                parts << r.totals.at(i).label + QStringLiteral(": ")
                             + formatNumber(t.value(i), r.totals.at(i).money);
        }
    }
    if (r.usesPeriod) {
        parts << QStringLiteral("Період: %1 – %2")
                     .arg(from_->date().toString(QStringLiteral("dd.MM.yyyy")),
                          to_->date().toString(QStringLiteral("dd.MM.yyyy")));
    }
    summary_->setText(parts.join(QStringLiteral("     ·     ")));
    summary_->show();
}

void ReportsPage::exportCsv()
{
    if (model_->rowCount() == 0) {
        QMessageBox::information(this, QStringLiteral("Експорт"),
                                 QStringLiteral("Звіт порожній. Спочатку сформуйте звіт з даними."));
        return;
    }

    const QString suggested = QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
                                  .filePath(combo_->currentText() + QStringLiteral(".csv"));
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Експорт у CSV"), suggested,
                                                QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive))
        path += QStringLiteral(".csv");

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::warning(this, QStringLiteral("Експорт"),
                             QStringLiteral("Не вдалося створити файл:\n") + file.errorString());
        return;
    }

    // UTF-8 з BOM і роздільник ";" — файл правильно відкривається в Excel
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out.setGenerateByteOrderMark(true);

    auto quote = [](QString s) {
        s.replace(u'"', QStringLiteral("\"\""));
        return QStringLiteral("\"") + s + QStringLiteral("\"");
    };

    QStringList row;
    for (int c = 0; c < model_->columnCount(); ++c)
        row << quote(model_->headerData(c, Qt::Horizontal).toString());
    out << row.join(u';') << "\n";

    for (int r = 0; r < model_->rowCount(); ++r) {
        row.clear();
        for (int c = 0; c < model_->columnCount(); ++c)
            row << quote(model_->exportText(model_->index(r, c)));
        out << row.join(u';') << "\n";
    }
    out.flush();
    file.close();

    QMessageBox::information(this, QStringLiteral("Експорт"),
                             QStringLiteral("Звіт збережено:\n") + QDir::toNativeSeparators(path));
}
