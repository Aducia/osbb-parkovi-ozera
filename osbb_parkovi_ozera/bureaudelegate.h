#ifndef BUREAUDELEGATE_H
#define BUREAUDELEGATE_H

#include <QDate>
#include <QStyledItemDelegate>

class BureauTableModel;

// Редактори комірок таблиці та полів форми залежно від типу поля:
// зовнішній ключ — випадний список, дата — календар, гроші/цілі — лічильники.
// Ті самі статичні функції використовує форма RecordDialog.
class BureauDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit BureauDelegate(BureauTableModel *model, QObject *parent = nullptr);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;

    static QWidget *makeEditor(const BureauTableModel *model, int column, QWidget *parent);
    static void setEditorValue(QWidget *editor, const BureauTableModel *model, int column,
                               const QVariant &value);
    static QVariant editorValue(QWidget *editor, const BureauTableModel *model, int column);
    // true, якщо в редакторі нічого не вибрано / не введено
    static bool isEditorEmpty(QWidget *editor, const BureauTableModel *model, int column);

    // Дата-«порожнє значення» для необов'язкових дат (показується як «—»)
    static QDate emptyDate();

private:
    BureauTableModel *model_;
};

#endif // BUREAUDELEGATE_H
