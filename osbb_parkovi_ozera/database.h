#ifndef DATABASE_H
#define DATABASE_H

#include <QString>

class QSqlError;

// Робота з файлом бази даних SQLite (parkovi_ozera.db поруч з exe).
class Database
{
public:
    // Відкриває БД, вмикає зовнішні ключі, за потреби створює схему
    // (ресурс :/sql/schema.sql) і додає тестові дані в порожню БД (:/sql/seed.sql).
    static bool open(QString *error);

    static QString filePath();
    static QString displayName();

    // Перекладає повідомлення SQLite у зрозумілий користувачу текст.
    static QString friendlyError(const QSqlError &error);

    // Людська назва поля для заголовків таблиць і форм.
    static QString fieldTitle(const QString &field);

private:
    static bool runScript(const QString &resourcePath, QString *error);
};

#endif // DATABASE_H
