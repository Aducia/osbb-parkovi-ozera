#ifndef REPORTSPAGE_H
#define REPORTSPAGE_H

#include <QWidget>

class QComboBox;
class QDateEdit;
class QLabel;
class QPushButton;
class QTableView;
class ReportModel;

// Сторінка «Звіти»: вибір звіту, період, формування, підсумки, експорт у CSV.
// Запити — ті самі, що у sql/reports.sql.
class ReportsPage : public QWidget
{
    Q_OBJECT
public:
    explicit ReportsPage(QWidget *parent = nullptr);

private slots:
    void reportChanged();
    void presetChanged();
    void run();
    void exportCsv();

private:
    QComboBox *combo_;
    QLabel *description_;
    QComboBox *preset_;
    QDateEdit *from_;
    QDateEdit *to_;
    QPushButton *runButton_;
    QPushButton *csvButton_;
    QTableView *view_;
    QLabel *summary_;
    ReportModel *model_;
};

#endif // REPORTSPAGE_H
