#include "bureaudelegate.h"

#include "bureautablemodel.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QSpinBox>

BureauDelegate::BureauDelegate(BureauTableModel *model, QObject *parent)
    : QStyledItemDelegate(parent)
    , model_(model)
{
}

QDate BureauDelegate::emptyDate()
{
    return QDate(2000, 1, 1);
}

QWidget *BureauDelegate::makeEditor(const BureauTableModel *model, int column, QWidget *parent)
{
    const bool required = model->isRequired(column);

    switch (model->kind(column)) {
    case BureauTableModel::Choice: {
        auto *combo = new QComboBox(parent);
        const QStringList values = model->choices(column);
        for (const QString &v : values)
            combo->addItem(v, v);
        combo->setMinimumContentsLength(14);
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        return combo;
    }
    case BureauTableModel::ForeignKey: {
        auto *combo = new QComboBox(parent);
        if (!required)
            combo->addItem(QStringLiteral("—"), QVariant());
        const auto items = model->lookupItems(column);
        for (const auto &item : items)
            combo->addItem(item.label, item.id);
        combo->setMaxVisibleItems(15);
        combo->setMinimumContentsLength(20);
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        return combo;
    }
    case BureauTableModel::Date: {
        auto *edit = new QDateEdit(parent);
        edit->setCalendarPopup(true);
        edit->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
        edit->setMaximumDate(QDate(2100, 12, 31));
        if (required) {
            edit->setMinimumDate(QDate(1990, 1, 1));
        } else {
            // мінімальна дата означає «не вказано» (NULL)
            edit->setMinimumDate(emptyDate());
            edit->setSpecialValueText(QStringLiteral("—"));
        }
        return edit;
    }
    case BureauTableModel::Money: {
        auto *spin = new QDoubleSpinBox(parent);
        spin->setRange(0.0, 1000000000.0);
        spin->setDecimals(2);
        spin->setSingleStep(100.0);
        spin->setGroupSeparatorShown(true);
        spin->setAlignment(Qt::AlignRight);
        return spin;
    }
    case BureauTableModel::Quantity: {
        auto *spin = new QDoubleSpinBox(parent);
        spin->setRange(0.0, 1000000000.0);
        spin->setDecimals(3);
        spin->setSingleStep(1.0);
        spin->setGroupSeparatorShown(true);
        spin->setAlignment(Qt::AlignRight);
        return spin;
    }
    case BureauTableModel::Integer: {
        auto *spin = new QSpinBox(parent);
        spin->setRange(0, 1000000);
        spin->setAlignment(Qt::AlignRight);
        return spin;
    }
    case BureauTableModel::Text:
        break;
    }
    return new QLineEdit(parent);
}

void BureauDelegate::setEditorValue(QWidget *editor, const BureauTableModel *model, int column,
                                    const QVariant &value)
{
    if (auto *combo = qobject_cast<QComboBox *>(editor)) {
        if (model->kind(column) == BureauTableModel::Choice) {
            combo->setCurrentIndex(combo->findData(value.toString()));
            return;
        }
        int found = -1;
        for (int i = 0; i < combo->count(); ++i) {
            const QVariant id = combo->itemData(i);
            if (value.isNull() ? !id.isValid()
                               : (id.isValid() && id.toLongLong() == value.toLongLong())) {
                found = i;
                break;
            }
        }
        combo->setCurrentIndex(found);
        return;
    }
    if (auto *date = qobject_cast<QDateEdit *>(editor)) {
        QDate d = QDate::fromString(value.toString(), Qt::ISODate);
        if (!d.isValid())
            d = model->isRequired(column) ? QDate::currentDate() : date->minimumDate();
        date->setDate(d);
        return;
    }
    if (auto *spin = qobject_cast<QDoubleSpinBox *>(editor)) {
        spin->setValue(value.toDouble());
        return;
    }
    if (auto *spin = qobject_cast<QSpinBox *>(editor)) {
        spin->setValue(value.toInt());
        return;
    }
    if (auto *line = qobject_cast<QLineEdit *>(editor))
        line->setText(value.toString());
}

QVariant BureauDelegate::editorValue(QWidget *editor, const BureauTableModel *model, int column)
{
    if (auto *combo = qobject_cast<QComboBox *>(editor)) {
        if (combo->currentIndex() < 0)
            return {};
        if (model->kind(column) == BureauTableModel::Choice)
            return combo->currentData().toString();
        const QVariant id = combo->currentData();
        return id.isValid() ? QVariant(id.toLongLong()) : QVariant();
    }
    if (auto *date = qobject_cast<QDateEdit *>(editor)) {
        if (!model->isRequired(column) && date->date() == date->minimumDate())
            return {};
        return date->date().toString(Qt::ISODate);
    }
    if (auto *spin = qobject_cast<QDoubleSpinBox *>(editor))
        return spin->value();
    if (auto *spin = qobject_cast<QSpinBox *>(editor))
        return spin->value();
    if (auto *line = qobject_cast<QLineEdit *>(editor)) {
        const QString text = line->text().trimmed();
        if (text.isEmpty() && !model->isRequired(column))
            return {};
        return text;
    }
    return {};
}

bool BureauDelegate::isEditorEmpty(QWidget *editor, const BureauTableModel *model, int column)
{
    if (qobject_cast<QLineEdit *>(editor))
        return qobject_cast<QLineEdit *>(editor)->text().trimmed().isEmpty();
    return editorValue(editor, model, column).isNull();
}

QWidget *BureauDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                      const QModelIndex &index) const
{
    Q_UNUSED(option);
    // Індекс приходить від проксі-моделі; колонки в проксі і моделі збігаються.
    return makeEditor(model_, index.column(), parent);
}

void BureauDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    setEditorValue(editor, model_, index.column(), index.data(Qt::EditRole));
}

void BureauDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                  const QModelIndex &index) const
{
    // Порожнє обов'язкове поле не записуємо, щоб не затерти значення
    if (model_->isRequired(index.column()) && isEditorEmpty(editor, model_, index.column()))
        return;
    model->setData(index, editorValue(editor, model_, index.column()), Qt::EditRole);
}
