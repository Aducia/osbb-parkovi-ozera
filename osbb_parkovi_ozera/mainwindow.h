#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QList>
#include <QMainWindow>
#include <QStringList>

class BureauTableModel;
class DashboardPage;
class QButtonGroup;
class QLabel;
class QStackedWidget;
class QVBoxLayout;
class TablePage;

// Головне вікно: бічна панель із логотипом ЖК та навігацією, заголовок сторінки,
// сторінки (Головна, таблиці БД, Звіти), меню та рядок стану.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    bool logoutRequested() const { return logout_; }

signals:
    void closed();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildMenu();
    QWidget *buildSidebar();
    void addPage(const QString &section, const QString &title, const QString &subtitle, QWidget *page);
    TablePage *addTablePage(const QString &section, const QString &title, const QString &subtitle,
                            BureauTableModel *model);
    void showPage(int index);
    void changePassword();
    void logout();
    void about();

    BureauTableModel *makeModel(const QString &table);
    BureauTableModel *buildRequests();
    BureauTableModel *buildWriteOffs();
    BureauTableModel *buildPayments();
    BureauTableModel *buildSurcharges();
    BureauTableModel *buildStaff();
    BureauTableModel *buildFlats();
    BureauTableModel *buildMaterials();
    BureauTableModel *buildWorkTypes();

    QStackedWidget *stack_ = nullptr;
    QButtonGroup *nav_ = nullptr;
    QVBoxLayout *navLayout_ = nullptr;
    QLabel *title_ = nullptr;
    QLabel *subtitle_ = nullptr;
    QStringList titles_;
    QStringList subtitles_;
    DashboardPage *dashboard_ = nullptr;
    QList<TablePage *> tablePages_;
    bool logout_ = false;
};

#endif // MAINWINDOW_H
