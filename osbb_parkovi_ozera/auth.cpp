#include "auth.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QSettings>
#include <QUuid>

namespace {

QString g_user;
Auth::Role g_role = Auth::Viewer;

QString iniPath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("users.ini"));
}

QString hashOf(const QString &salt, const QString &password)
{
    return QString::fromLatin1(
        QCryptographicHash::hash((salt + password).toUtf8(), QCryptographicHash::Sha256).toHex());
}

void writeUser(QSettings &s, const QString &login, const QString &password, const QString &role)
{
    const QString salt = QUuid::createUuid().toString(QUuid::WithoutBraces);
    s.beginGroup(login);
    s.setValue(QStringLiteral("salt"), salt);
    s.setValue(QStringLiteral("hash"), hashOf(salt, password));
    s.setValue(QStringLiteral("role"), role);
    s.endGroup();
}

bool validLogin(const QString &user)
{
    return !user.isEmpty() && !user.contains(u'/') && !user.contains(u'\\');
}

bool checkPassword(const QString &user, const QString &password, QString *role = nullptr)
{
    if (!validLogin(user))
        return false;
    QSettings s(iniPath(), QSettings::IniFormat);
    s.beginGroup(user);
    if (!s.contains(QStringLiteral("hash")))
        return false;
    const bool ok = hashOf(s.value(QStringLiteral("salt")).toString(), password)
                    == s.value(QStringLiteral("hash")).toString();
    if (ok && role)
        *role = s.value(QStringLiteral("role")).toString();
    return ok;
}

} // namespace

void Auth::init()
{
    QSettings s(iniPath(), QSettings::IniFormat);
    if (s.childGroups().isEmpty()) {
        writeUser(s, QStringLiteral("admin"), QStringLiteral("admin123"), QStringLiteral("admin"));
        writeUser(s, QStringLiteral("viewer"), QStringLiteral("viewer123"), QStringLiteral("viewer"));
        s.sync();
    }
}

bool Auth::login(const QString &user, const QString &password)
{
    QString role;
    if (!checkPassword(user, password, &role))
        return false;
    g_user = user;
    g_role = (role == QStringLiteral("admin")) ? Admin : Viewer;
    return true;
}

void Auth::logout()
{
    g_user.clear();
    g_role = Viewer;
}

bool Auth::changePassword(const QString &oldPassword, const QString &newPassword)
{
    if (g_user.isEmpty() || newPassword.isEmpty() || !checkPassword(g_user, oldPassword))
        return false;
    QSettings s(iniPath(), QSettings::IniFormat);
    writeUser(s, g_user, newPassword, g_role == Admin ? QStringLiteral("admin") : QStringLiteral("viewer"));
    s.sync();
    return s.status() == QSettings::NoError;
}

QString Auth::currentUser() { return g_user; }
Auth::Role Auth::currentRole() { return g_role; }
bool Auth::isAdmin() { return g_role == Admin; }

QString Auth::roleName(Role role)
{
    return role == Admin ? QStringLiteral("Адміністрація правління") : QStringLiteral("Перегляд");
}
