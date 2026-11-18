#include "auth.h"
#include "database.h"
#include "logindialog.h"
#include "mainwindow.h"
#include "style.h"

#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ЖК «Паркові Озера»"));
    QApplication::setApplicationVersion(QStringLiteral("1.0"));
    QLocale::setDefault(QLocale(QLocale::Ukrainian, QLocale::Ukraine));
    Style::apply(app);

    // Українські підписи стандартних елементів Qt (календар, контекстні меню), якщо є переклад
    QTranslator qtTranslator;
    if (qtTranslator.load(QLocale(), QStringLiteral("qtbase"), QStringLiteral("_"),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtTranslator);

    // 1. База даних: parkovi_ozera.db поруч з exe (створюється автоматично)
    QString error;
    if (!Database::open(&error)) {
        QMessageBox::critical(nullptr, QStringLiteral("Помилка бази даних"),
                              QStringLiteral("Не вдалося відкрити базу даних SQLite.\n\n") + error);
        return 1;
    }

    // 2. Облікові записи: users.ini поруч з exe (створюється автоматично)
    Auth::init();

    // Програма завершується явно, коли закривається головне вікно,
    // щоб після «Вийти з облікового запису» можна було знову показати вхід.
    QApplication::setQuitOnLastWindowClosed(false);

    for (;;) {
        LoginDialog login;
        if (login.exec() != QDialog::Accepted)
            return 0;

        MainWindow window;
        QObject::connect(&window, &MainWindow::closed, &app, &QCoreApplication::quit,
                         Qt::QueuedConnection);
        window.show();
        app.exec();

        if (!window.logoutRequested())
            return 0;
        Auth::logout();
    }
}
