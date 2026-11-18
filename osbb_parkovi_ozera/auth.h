#ifndef AUTH_H
#define AUTH_H

#include <QString>

// Автентифікація: облікові записи зберігаються у users.ini поруч з exe,
// пароль — як SHA-256 від (сіль + пароль). Окремої таблиці користувачів у БД немає.
class Auth
{
public:
    enum Role { Viewer, Admin };

    // Створює users.ini з користувачами за замовчуванням, якщо файлу немає:
    // admin / admin123 (повний доступ), viewer / viewer123 (перегляд і звіти).
    static void init();

    static bool login(const QString &user, const QString &password);
    static void logout();
    static bool changePassword(const QString &oldPassword, const QString &newPassword);

    static QString currentUser();
    static Role currentRole();
    static bool isAdmin();
    static QString roleName(Role role);
};

#endif // AUTH_H
