#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>

class QLabel;
class QLineEdit;

// Вікно входу: логін і пароль перевіряються через Auth (users.ini).
class LoginDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LoginDialog(QWidget *parent = nullptr);

protected:
    void accept() override;

private:
    QLineEdit *userEdit_;
    QLineEdit *passwordEdit_;
    QLabel *errorLabel_;
};

#endif // LOGINDIALOG_H
