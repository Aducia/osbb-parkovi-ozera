#include "invoicedialog.h"

#include "database.h"
#include "style.h"

#include <QComboBox>
#include <QDate>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlError>
#include <QSqlQuery>
#include <QVBoxLayout>

namespace {

QString money(double value)
{
    return QLocale().toString(value, 'f', 2) + QStringLiteral(" грн");
}

} // namespace

InvoiceDialog::InvoiceDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Рахунок власнику квартири"));
    setMinimumWidth(560);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("Індивідуальний рахунок за заявку"), this);
    title->setObjectName(QStringLiteral("panelTitle"));
    root->addWidget(title);

    auto *hint = new QLabel(QStringLiteral(
        "Рахунок формується для заявок по квартирах: вартість робіт за довідником "
        "+ матеріали, списані за рахунок власника, + доплата за платний виклик (за потреби)."), this);
    hint->setObjectName(QStringLiteral("hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);

    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(10);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    request_ = new QComboBox(this);
    request_->setMinimumContentsLength(38);
    request_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    form->addRow(QStringLiteral("Заявка:"), request_);

    flatLabel_ = new QLabel(this);
    form->addRow(QStringLiteral("Квартира:"), flatLabel_);
    workLabel_ = new QLabel(this);
    form->addRow(QStringLiteral("Вартість робіт:"), workLabel_);
    materialsLabel_ = new QLabel(this);
    form->addRow(QStringLiteral("Матеріали за рахунок власника:"), materialsLabel_);

    surcharge_ = new QComboBox(this);
    surcharge_->addItem(QStringLiteral("— без доплати —"), QVariant());
    form->addRow(QStringLiteral("Тип доплати:"), surcharge_);

    surchargeAmount_ = new QDoubleSpinBox(this);
    surchargeAmount_->setRange(0.0, 1000000.0);
    surchargeAmount_->setDecimals(2);
    surchargeAmount_->setSingleStep(100.0);
    surchargeAmount_->setSuffix(QStringLiteral(" грн"));
    surchargeAmount_->setGroupSeparatorShown(true);
    form->addRow(QStringLiteral("Сума доплати:"), surchargeAmount_);

    totalLabel_ = new QLabel(this);
    totalLabel_->setObjectName(QStringLiteral("summary"));
    form->addRow(QStringLiteral("Разом до сплати:"), totalLabel_);
    root->addLayout(form);

    noteLabel_ = new QLabel(this);
    noteLabel_->setObjectName(QStringLiteral("pendingLabel"));
    noteLabel_->setWordWrap(true);
    noteLabel_->hide();
    root->addWidget(noteLabel_);

    errorLabel_ = new QLabel(this);
    errorLabel_->setObjectName(QStringLiteral("errorLabel"));
    errorLabel_->setWordWrap(true);
    errorLabel_->hide();
    root->addWidget(errorLabel_);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch();
    auto *cancel = new QPushButton(QStringLiteral("Скасувати"), this);
    auto *create = new QPushButton(QStringLiteral("Сформувати рахунок"), this);
    Style::setKind(create, QStringLiteral("primary"));
    create->setDefault(true);
    buttons->addWidget(cancel);
    buttons->addWidget(create);
    root->addLayout(buttons);

    // Довідник доплат
    QSqlQuery tariffs;
    if (tariffs.exec(QStringLiteral("SELECT код, назва, сума FROM доплати ORDER BY назва"))) {
        while (tariffs.next()) {
            surcharge_->addItem(QStringLiteral("%1 — %2").arg(tariffs.value(1).toString(),
                                                              money(tariffs.value(2).toDouble())),
                                tariffs.value(0));
            surcharge_->setItemData(surcharge_->count() - 1, tariffs.value(2), Qt::UserRole + 1);
        }
    }

    // Заявки по квартирах (крім скасованих)
    QSqlQuery requests;
    if (requests.exec(QStringLiteral(
            "SELECT z.код, k.корпус || ', кв. ' || k.номер, v.вид_роботи, v.вартість, z.квартира, z.дата_заявки "
            "FROM заявки z "
            "JOIN квартири k ON k.код = z.квартира "
            "JOIN види_робіт v ON v.код = z.вид_роботи "
            "WHERE z.статус <> 'Скасована' ORDER BY z.код DESC"))) {
        while (requests.next()) {
            Info info;
            info.flat = requests.value(1).toString();
            info.work = requests.value(2).toString();
            info.workCost = requests.value(3).toDouble();
            info.flatId = requests.value(4).toLongLong();
            const qlonglong id = requests.value(0).toLongLong();
            infos_.insert(id, info);
            request_->addItem(QStringLiteral("№%1 — %2 — %3").arg(id).arg(info.flat, info.work), id);
        }
    }

    connect(request_, &QComboBox::currentIndexChanged, this, &InvoiceDialog::requestChanged);
    connect(surcharge_, &QComboBox::currentIndexChanged, this, &InvoiceDialog::surchargeChanged);
    connect(surchargeAmount_, &QDoubleSpinBox::valueChanged, this, [this] { updateTotals(); });
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(create, &QPushButton::clicked, this, &InvoiceDialog::createInvoice);

    if (request_->count() == 0) {
        create->setEnabled(false);
        errorLabel_->setText(QStringLiteral("Немає заявок по квартирах. Спочатку внесіть заявку з типом локації «Квартира»."));
        errorLabel_->show();
    }
    requestChanged();
}

void InvoiceDialog::requestChanged()
{
    materials_ = 0.0;
    noteLabel_->hide();
    if (request_->currentIndex() < 0) {
        flatLabel_->clear();
        workLabel_->clear();
        materialsLabel_->clear();
        updateTotals();
        return;
    }

    const qlonglong id = request_->currentData().toLongLong();
    const Info info = infos_.value(id);
    flatLabel_->setText(info.flat);
    workLabel_->setText(money(info.workCost));

    QSqlQuery q;
    q.prepare(QStringLiteral("SELECT COALESCE(SUM(сума), 0) FROM списання_матеріалів "
                             "WHERE заявка = ? AND джерело = 'Власник квартири'"));
    q.addBindValue(id);
    if (q.exec() && q.next())
        materials_ = q.value(0).toDouble();
    materialsLabel_->setText(money(materials_));

    QSqlQuery existing;
    existing.prepare(QStringLiteral("SELECT COUNT(*) FROM оплати WHERE заявка = ? AND рахунок = 'Індивідуальний'"));
    existing.addBindValue(id);
    if (existing.exec() && existing.next() && existing.value(0).toInt() > 0) {
        noteLabel_->setText(QStringLiteral("Для цієї заявки вже виставлено рахунків: %1. "
                                           "Новий рахунок буде додатковим.").arg(existing.value(0).toInt()));
        noteLabel_->show();
    }
    updateTotals();
}

void InvoiceDialog::surchargeChanged()
{
    const QVariant amount = surcharge_->currentData(Qt::UserRole + 1);
    if (surcharge_->currentData().isValid())
        surchargeAmount_->setValue(amount.toDouble());
    else
        surchargeAmount_->setValue(0.0);
    updateTotals();
}

void InvoiceDialog::updateTotals()
{
    double work = 0.0;
    if (request_->currentIndex() >= 0)
        work = infos_.value(request_->currentData().toLongLong()).workCost;
    totalLabel_->setText(money(work + materials_ + surchargeAmount_->value()));
}

void InvoiceDialog::createInvoice()
{
    if (request_->currentIndex() < 0)
        return;

    const qlonglong id = request_->currentData().toLongLong();
    const Info info = infos_.value(id);
    const double charged = info.workCost + materials_;

    QSqlQuery q;
    q.prepare(QStringLiteral(
        "INSERT INTO оплати (дата, рахунок, квартира, заявка, призначення, нараховано, тип_доплати, доплата, сплачено) "
        "VALUES (?, 'Індивідуальний', ?, ?, ?, ?, ?, ?, 0)"));
    q.addBindValue(QDate::currentDate().toString(Qt::ISODate));
    q.addBindValue(info.flatId);
    q.addBindValue(id);
    q.addBindValue(QStringLiteral("Рахунок за заявку №%1: %2").arg(id).arg(info.work));
    q.addBindValue(charged);
    const QVariant tariff = surcharge_->currentData();
    q.addBindValue(tariff.isValid() ? tariff : QVariant(QMetaType::fromType<qlonglong>()));
    q.addBindValue(surchargeAmount_->value());

    if (!q.exec()) {
        errorLabel_->setText(Database::friendlyError(q.lastError()));
        errorLabel_->show();
        return;
    }
    created_ = true;
    accept();
}
