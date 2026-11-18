#include "dashboardpage.h"

#include "reportmodel.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableView>
#include <QVBoxLayout>

namespace {

// Відкрита заявка: ще не виконана і не скасована
const char *kOpenStatuses = "('Нова', 'Призначена', 'В роботі')";

QVariant scalar(const QString &sql, int column = 0)
{
    QSqlQuery q;
    if (q.exec(sql) && q.next())
        return q.value(column);
    return {};
}

QString money(double value)
{
    return QLocale().toString(value, 'f', 2) + QStringLiteral(" грн");
}

} // namespace

DashboardPage::DashboardPage(QWidget *parent)
    : QWidget(parent)
{
    requestsModel_ = new ReportModel(this);
    debtModel_ = new ReportModel(this);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 18, 24, 18);
    root->setSpacing(16);

    auto *cards = new QGridLayout();
    cards->setSpacing(16);
    cards->addWidget(makeCard(QStringLiteral("Відкриті заявки"), QStringLiteral("нові, призначені, у роботі"),
                              QStringLiteral("#86A9DF"), &openValue_), 0, 0);
    cards->addWidget(makeCard(QStringLiteral("Аварійні та термінові"), QStringLiteral("серед відкритих заявок"),
                              QStringLiteral("#D46A6A"), &urgentValue_), 0, 1);
    cards->addWidget(makeCard(QStringLiteral("Заборгованість квартир"), QStringLiteral("внески та індивідуальні рахунки"),
                              QStringLiteral("#5D5D61"), &debtValue_), 0, 2);
    cards->addWidget(makeCard(QStringLiteral("Баланс загального фонду"), QStringLiteral("внески сплачено − матеріали з фонду"),
                              QStringLiteral("#69CC69"), &balanceValue_), 0, 3);
    root->addLayout(cards);

    root->addWidget(makePanel(QStringLiteral("Відкриті заявки (спершу аварійні)"), requestsModel_, &requestsView_), 1);
    root->addWidget(makePanel(QStringLiteral("Заборгованість за квартирами"), debtModel_, &debtView_), 1);
}

QWidget *DashboardPage::makeCard(const QString &title, const QString &hint, const QString &accent,
                                 QLabel **value)
{
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("card"));
    // кольорова смужка зліва
    card->setStyleSheet(QStringLiteral("QFrame#card { border-left: 4px solid %1; }").arg(accent));
    card->setMinimumHeight(104);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(4);

    auto *titleLabel = new QLabel(title, card);
    titleLabel->setObjectName(QStringLiteral("cardTitle"));
    *value = new QLabel(QStringLiteral("—"), card);
    (*value)->setObjectName(QStringLiteral("cardValue"));
    auto *hintLabel = new QLabel(hint, card);
    hintLabel->setObjectName(QStringLiteral("cardHint"));

    layout->addWidget(titleLabel);
    layout->addWidget(*value);
    layout->addWidget(hintLabel);
    layout->addStretch();
    return card;
}

QWidget *DashboardPage::makePanel(const QString &title, ReportModel *model, QTableView **view)
{
    auto *panel = new QFrame(this);
    panel->setObjectName(QStringLiteral("panel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 12, 16, 16);
    layout->setSpacing(10);

    auto *titleLabel = new QLabel(title, panel);
    titleLabel->setObjectName(QStringLiteral("panelTitle"));
    layout->addWidget(titleLabel);

    *view = new QTableView(panel);
    (*view)->setModel(model);
    (*view)->setEditTriggers(QAbstractItemView::NoEditTriggers);
    (*view)->setSelectionBehavior(QAbstractItemView::SelectRows);
    (*view)->setAlternatingRowColors(true);
    (*view)->setWordWrap(false);
    (*view)->verticalHeader()->hide();
    (*view)->verticalHeader()->setDefaultSectionSize(28);
    (*view)->horizontalHeader()->setStretchLastSection(true);
    (*view)->horizontalHeader()->setHighlightSections(false);
    (*view)->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    layout->addWidget(*view);
    return panel;
}

void DashboardPage::refresh()
{
    const QString open = QString::fromUtf8(kOpenStatuses);

    openValue_->setText(scalar(QStringLiteral("SELECT COUNT(*) FROM заявки WHERE статус IN ") + open).toString());
    urgentValue_->setText(scalar(QStringLiteral(
        "SELECT COUNT(*) FROM заявки WHERE пріоритет IN ('Аварійна', 'Термінова') AND статус IN ") + open).toString());
    debtValue_->setText(money(scalar(QStringLiteral(
        "SELECT COALESCE(SUM(нараховано + доплата - сплачено), 0) FROM оплати")).toDouble()));
    balanceValue_->setText(money(scalar(QStringLiteral(
        "SELECT COALESCE((SELECT SUM(сплачено) FROM оплати WHERE рахунок = 'Загальний фонд'), 0) "
        "- COALESCE((SELECT SUM(сума) FROM списання_матеріалів WHERE джерело = 'Загальний фонд'), 0)")).toDouble()));

    requestsModel_->setQuery(QStringLiteral(
        "SELECT z.код AS \"№\", z.пріоритет AS \"Пріоритет\", z.статус AS \"Статус\", "
        "z.місце AS \"Місце\", v.вид_роботи AS \"Вид роботи\", p.ПІБ AS \"Виконавець\", "
        "z.дата_заявки AS \"Дата заявки\" "
        "FROM заявки z "
        "JOIN види_робіт v ON v.код = z.вид_роботи "
        "LEFT JOIN персонал_підрядники p ON p.код = z.виконавець "
        "WHERE z.статус IN ") + open + QStringLiteral(
        " ORDER BY CASE z.пріоритет WHEN 'Аварійна' THEN 0 WHEN 'Термінова' THEN 1 ELSE 2 END, "
        "z.дата_заявки, z.код"));

    debtModel_->setQuery(QStringLiteral(
        "SELECT k.корпус || ', кв. ' || k.номер AS \"Квартира\", k.власник AS \"Власник\", "
        "SUM(o.нараховано + o.доплата) AS \"Нараховано, грн\", SUM(o.сплачено) AS \"Сплачено, грн\", "
        "SUM(o.нараховано + o.доплата - o.сплачено) AS \"Борг, грн\" "
        "FROM оплати o JOIN квартири k ON k.код = o.квартира "
        "GROUP BY k.код HAVING SUM(o.нараховано + o.доплата - o.сплачено) > 0.005 "
        "ORDER BY SUM(o.нараховано + o.доплата - o.сплачено) DESC"));

    for (QTableView *view : {requestsView_, debtView_}) {
        view->resizeColumnsToContents();
        for (int c = 0; c < view->model()->columnCount(); ++c)
            view->setColumnWidth(c, qMin(view->columnWidth(c) + 16, 320));
    }
}
