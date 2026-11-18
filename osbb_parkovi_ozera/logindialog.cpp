#include "logindialog.h"

#include "auth.h"
#include "style.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Вхід — ЖК «Паркові Озера»"));
    setFixedWidth(520);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Шапка
    auto *header = new QFrame(this);
    header->setObjectName(QStringLiteral("loginHeader"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(24, 20, 24, 20);
    headerLayout->setSpacing(20);

    // Логотип ЖК — у лівому верхньому куті
    headerLayout->addWidget(Style::logoLabel(170, header), 0, Qt::AlignLeft | Qt::AlignTop);

    auto *titles = new QVBoxLayout();
    titles->setSpacing(4);
    auto *title = new QLabel(QStringLiteral("Система управління житловим фондом ОСББ"), header);
    title->setObjectName(QStringLiteral("loginTitle"));
    title->setWordWrap(true);
    auto *subtitle = new QLabel(QStringLiteral("ЖК «Паркові Озера»"), header);
    subtitle->setObjectName(QStringLiteral("loginSubtitle"));
    auto *prompt = new QLabel(QStringLiteral("Увійдіть, щоб продовжити"), header);
    prompt->setObjectName(QStringLiteral("loginPrompt"));

    titles->addStretch();
    titles->addWidget(title);
    titles->addWidget(subtitle);
    titles->addStretch();
    titles->addWidget(prompt);
    headerLayout->addLayout(titles, 1);
    root->addWidget(header);

    // Форма
    auto *body = new QWidget(this);
    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(24, 20, 24, 20);
    bodyLayout->setSpacing(12);

    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);

    userEdit_ = new QLineEdit(body);
    userEdit_->setPlaceholderText(QStringLiteral("логін"));
    userEdit_->setText(QStringLiteral("admin"));
    form->addRow(QStringLiteral("Логін:"), userEdit_);

    passwordEdit_ = new QLineEdit(body);
    passwordEdit_->setPlaceholderText(QStringLiteral("пароль"));
    passwordEdit_->setEchoMode(QLineEdit::Password);
    form->addRow(QStringLiteral("Пароль:"), passwordEdit_);

    auto *showPassword = new QCheckBox(QStringLiteral("Показати пароль"), body);
    connect(showPassword, &QCheckBox::toggled, this, [this](bool on) {
        passwordEdit_->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
    });
    form->addRow(QString(), showPassword);
    bodyLayout->addLayout(form);

    errorLabel_ = new QLabel(body);
    errorLabel_->setObjectName(QStringLiteral("errorLabel"));
    errorLabel_->setWordWrap(true);
    errorLabel_->hide();
    bodyLayout->addWidget(errorLabel_);

    auto *hint = new QLabel(QStringLiteral(
        "Облікові записи за замовчуванням:\n"
        "admin / admin123 — адміністрація правління (повний доступ)\n"
        "viewer / viewer123 — лише перегляд і звіти"), body);
    hint->setObjectName(QStringLiteral("hint"));
    bodyLayout->addWidget(hint);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch();
    auto *cancel = new QPushButton(QStringLiteral("Скасувати"), body);
    auto *ok = new QPushButton(QStringLiteral("Увійти"), body);
    Style::setKind(ok, QStringLiteral("primary"));
    ok->setDefault(true);
    ok->setMinimumWidth(110);
    buttons->addWidget(cancel);
    buttons->addWidget(ok);
    bodyLayout->addLayout(buttons);
    root->addWidget(body);

    connect(ok, &QPushButton::clicked, this, &LoginDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &LoginDialog::reject);

    passwordEdit_->setFocus();
}

void LoginDialog::accept()
{
    const QString user = userEdit_->text().trimmed();
    if (user.isEmpty() || passwordEdit_->text().isEmpty()) {
        errorLabel_->setText(QStringLiteral("Введіть логін і пароль."));
        errorLabel_->show();
        return;
    }
    if (!Auth::login(user, passwordEdit_->text())) {
        errorLabel_->setText(QStringLiteral("Неправильний логін або пароль."));
        errorLabel_->show();
        passwordEdit_->clear();
        passwordEdit_->setFocus();
        return;
    }
    QDialog::accept();
}
