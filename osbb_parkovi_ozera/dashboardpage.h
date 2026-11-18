#ifndef DASHBOARDPAGE_H
#define DASHBOARDPAGE_H

#include <QWidget>

class QLabel;
class QTableView;
class ReportModel;

// Головна сторінка: ключові показники і короткі списки
// (відкриті заявки, заборгованість квартир).
class DashboardPage : public QWidget
{
    Q_OBJECT
public:
    explicit DashboardPage(QWidget *parent = nullptr);

public slots:
    void refresh();

private:
    QWidget *makeCard(const QString &title, const QString &hint, const QString &accent, QLabel **value);
    QWidget *makePanel(const QString &title, ReportModel *model, QTableView **view);

    QLabel *openValue_ = nullptr;
    QLabel *urgentValue_ = nullptr;
    QLabel *debtValue_ = nullptr;
    QLabel *balanceValue_ = nullptr;
    ReportModel *requestsModel_;
    ReportModel *debtModel_;
    QTableView *requestsView_ = nullptr;
    QTableView *debtView_ = nullptr;
};

#endif // DASHBOARDPAGE_H
