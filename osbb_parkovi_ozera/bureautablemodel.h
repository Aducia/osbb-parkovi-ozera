#ifndef BUREAUTABLEMODEL_H
#define BUREAUTABLEMODEL_H

#include <QHash>
#include <QList>
#include <QSet>
#include <QSqlRecord>
#include <QSqlTableModel>
#include <QStringList>
#include <QVariant>
#include <functional>

// Модель однієї таблиці БД на основі QSqlTableModel (режим OnManualSubmit).
// Додає:
//  * типи полів: текст, ціле, гроші, кількість, дата, зовнішній ключ, вибір зі списку;
//  * зовнішні ключі: у таблиці показується назва, а не код; у редакторі — список;
//  * поля лише для читання, обов'язкові поля, значення за замовчуванням;
//  * автододавання: вибір у списку (напр. тип доплати) підставляє число (суму доплати);
//  * обчислювані поля (борг, сума) — не записуються в БД;
//  * підсвічування незбережених, нових і позначених до видалення рядків.
class BureauTableModel : public QSqlTableModel
{
    Q_OBJECT
public:
    enum FieldKind { Text, Integer, Money, Quantity, Date, ForeignKey, Choice };

    struct LookupItem {
        qlonglong id = 0;
        QString label;      // повний текст для випадного списку
        QVariant extra;     // додаткове значення (4-та колонка запиту), напр. сума доплати
    };

    using ComputeFn = std::function<QVariant(const QList<double> &)>;

    // Роль для сортування: дати — у форматі ISO, числа — як числа, ключі — за назвою
    static constexpr int SortRole = Qt::UserRole + 1;

    explicit BureauTableModel(const QString &table, QObject *parent = nullptr);

    // ---- налаштування
    void setFieldKind(const QString &field, FieldKind kind);
    // lookupSql повертає: код, коротка назва для таблиці [, повна назва для списку [, додаткове значення]]
    void setForeignKey(const QString &field, const QString &lookupSql);
    void setChoices(const QString &field, const QStringList &values);
    void setDefaultValue(const QString &field, const QVariant &value);
    // Коли в полі sourceField вибрано запис довідника, у полі targetField з'являється його «додаткове значення»
    void setAutoFill(const QString &sourceField, const QString &targetField);
    void setReadOnlyFields(const QStringList &fields);
    void setRequiredFields(const QStringList &fields);
    // Обчислюване поле: fn отримує значення полів-залежностей (в порядку dependencies)
    void setComputed(const QString &field, const QStringList &dependencies, ComputeFn fn,
                     FieldKind kind = Money, bool positiveIsBad = false);
    void setEditable(bool editable);

    // ---- відомості про колонки
    QString fieldName(int column) const;
    int fieldColumn(const QString &field) const;
    FieldKind kind(int column) const;
    bool isReadOnlyColumn(int column) const;
    bool isRequired(int column) const;
    bool isComputed(int column) const;
    bool isEditable() const { return editable_; }
    QList<LookupItem> lookupItems(int column) const;
    QStringList choices(int column) const;
    QVariant defaultValue(int column) const;
    int autoFillTarget(int sourceColumn) const;
    QVariant lookupExtra(int column, qlonglong id) const;

    // Значення первинного ключа рядка (у QSqlTableModel primaryValues() захищена)
    QSqlRecord rowKey(int row) const { return primaryValues(row); }

    bool isRowInserted(int row) const;
    bool isRowDeleted(int row) const;

    void refreshLookups();

    // QSqlTableModel
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

public slots:
    bool select() override;

protected:
    bool insertRowIntoTable(const QSqlRecord &values) override;
    bool updateRowInTable(int row, const QSqlRecord &values) override;

private:
    struct Lookup {
        QString sql;
        QList<LookupItem> items;
        QHash<qlonglong, QString> shortLabels;
    };
    struct Computed {
        int column = -1;
        QList<int> dependencies;
        ComputeFn fn;
        bool positiveIsBad = false;
    };

    void loadLookups();
    int baseColumnCount() const;
    QVariant rawValue(int row, int column) const;
    QVariant computedValue(int row, const Computed &c) const;
    QString displayText(int column, const QVariant &raw) const;
    QSqlRecord withoutComputed(const QSqlRecord &values) const;

    QStringList fields_;               // поля таблиці в порядку колонок (як їх бачить драйвер)
    QStringList virtualNames_;         // обчислювані поля, яких немає в описі таблиці
    QHash<int, FieldKind> kinds_;
    QHash<int, Lookup> lookups_;
    QHash<int, QStringList> choices_;
    QHash<int, QVariant> defaults_;
    QHash<int, int> autoFill_;         // колонка-джерело -> колонка-ціль
    QHash<int, Computed> computed_;
    QSet<int> readOnly_;
    QSet<int> required_;
    bool editable_ = true;
};

#endif // BUREAUTABLEMODEL_H
