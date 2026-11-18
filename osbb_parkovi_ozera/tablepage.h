#ifndef TABLEPAGE_H
#define TABLEPAGE_H

#include <QHash>
#include <functional>
#include <QSqlRecord>
#include <QVariant>
#include <QWidget>

class BureauTableModel;
class QComboBox;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QSortFilterProxyModel;
class QTableView;

// Сторінка однієї таблиці: пошук, сортування, додавання / редагування через форму,
// редагування прямо в таблиці, видалення, збереження та скасування змін.
class TablePage : public QWidget
{
    Q_OBJECT
public:
    TablePage(BureauTableModel *model, const QString &title, QWidget *parent = nullptr);

    QString title() const { return title_; }
    bool hasPendingChanges() const;
    bool saveChanges();      // з повідомленням про помилку
    void discardChanges();
    void activate();         // викликається, коли сторінку відкривають

    // Початкове сортування (напр. нові записи зверху: колонка 0, за спаданням)
    void setSort(int column, Qt::SortOrder order);
    // Додаткова кнопка дії (напр. «Рахунок власнику»); handler повертає true,
    // якщо в БД щось змінилося: тоді таблиця перечитується.
    void setExtraAction(const QString &text, const QString &toolTip, std::function<bool()> handler);

    // Діалоги українською (не залежать від перекладів Qt)
    enum PendingChoice { SaveChoice, DiscardChoice, CancelChoice };
    static PendingChoice askPending(QWidget *parent, const QString &text);
    static bool confirm(QWidget *parent, const QString &title, const QString &text,
                        const QString &yesText);

signals:
    void dataSaved();

private slots:
    void addRecord();
    void editRecord();
    void deleteRecords();
    void save();
    void revert();
    void reload();
    void applyFilter();
    void updateState();
    void runExtra();

private:
    QList<int> selectedRows() const;     // рядки моделі (не проксі)
    bool resolvePendingBeforeForm();
    bool submitForm(int row, const QHash<int, QVariant> &values);
    int findRow(const QSqlRecord &primaryKey) const;
    void fitColumns();
    void markSaved();

    BureauTableModel *model_;
    QSortFilterProxyModel *proxy_;
    QString title_;
    bool canEdit_;
    bool columnsFitted_ = false;
    bool loaded_ = false;
    int seenVersion_ = 0;     // версія даних, яку бачить ця сторінка

    QTableView *view_;
    QLineEdit *search_;
    QComboBox *searchColumn_;
    QLabel *countLabel_;
    QLabel *pendingLabel_;
    QPushButton *addButton_ = nullptr;
    QPushButton *editButton_ = nullptr;
    QPushButton *deleteButton_ = nullptr;
    QPushButton *saveButton_ = nullptr;
    QPushButton *revertButton_ = nullptr;
    QPushButton *reloadButton_;
    QPushButton *extraButton_ = nullptr;
    QHBoxLayout *actions_ = nullptr;
    std::function<bool()> extraHandler_;
};

#endif // TABLEPAGE_H
