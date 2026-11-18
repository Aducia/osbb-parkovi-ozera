#include "bureautablemodel.h"

#include "database.h"

#include <QColor>
#include <QDate>
#include <QDebug>
#include <QFont>
#include <QLocale>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

BureauTableModel::BureauTableModel(const QString &table, QObject *parent)
    : QSqlTableModel(parent, QSqlDatabase::database())
{
    setTable(table);
    setEditStrategy(QSqlTableModel::OnManualSubmit);

    const QSqlRecord rec = database().record(table);
    for (int i = 0; i < rec.count(); ++i)
        fields_ << rec.fieldName(i);
}

// ---------------------------------------------------------------- налаштування

void BureauTableModel::setFieldKind(const QString &field, FieldKind kind)
{
    const int col = fieldColumn(field);
    if (col >= 0)
        kinds_[col] = kind;
}

void BureauTableModel::setForeignKey(const QString &field, const QString &lookupSql)
{
    const int col = fieldColumn(field);
    if (col < 0)
        return;
    kinds_[col] = ForeignKey;
    lookups_[col].sql = lookupSql;
}

void BureauTableModel::setChoices(const QString &field, const QStringList &values)
{
    const int col = fieldColumn(field);
    if (col < 0)
        return;
    kinds_[col] = Choice;
    choices_[col] = values;
}

void BureauTableModel::setDefaultValue(const QString &field, const QVariant &value)
{
    const int col = fieldColumn(field);
    if (col >= 0)
        defaults_[col] = value;
}

void BureauTableModel::setAutoFill(const QString &sourceField, const QString &targetField)
{
    const int source = fieldColumn(sourceField);
    const int target = fieldColumn(targetField);
    if (source >= 0 && target >= 0)
        autoFill_[source] = target;
}

void BureauTableModel::setReadOnlyFields(const QStringList &fields)
{
    readOnly_.clear();
    for (const QString &f : fields) {
        const int col = fieldColumn(f);
        if (col >= 0)
            readOnly_.insert(col);
    }
}

void BureauTableModel::setRequiredFields(const QStringList &fields)
{
    required_.clear();
    for (const QString &f : fields) {
        const int col = fieldColumn(f);
        if (col >= 0)
            required_.insert(col);
    }
}

void BureauTableModel::setComputed(const QString &field, const QStringList &dependencies,
                                   ComputeFn fn, FieldKind kind, bool positiveIsBad)
{
    // Залежно від версії драйвера SQLite обчислюване поле може бути або не бути
    // в описі таблиці. Якщо його немає — додаємо віртуальну колонку в кінець.
    int col = fields_.indexOf(field);
    if (col < 0) {
        int v = virtualNames_.indexOf(field);
        if (v < 0) {
            virtualNames_ << field;
            v = virtualNames_.size() - 1;
        }
        col = fields_.size() + v;
    }

    Computed c;
    c.column = col;
    c.fn = std::move(fn);
    c.positiveIsBad = positiveIsBad;
    for (const QString &dep : dependencies)
        c.dependencies << fieldColumn(dep);
    computed_.insert(col, c);
    kinds_[col] = kind;
}

void BureauTableModel::setEditable(bool editable)
{
    editable_ = editable;
}

// ---------------------------------------------------------------- відомості про колонки

int BureauTableModel::baseColumnCount() const
{
    return QSqlTableModel::columnCount();
}

QString BureauTableModel::fieldName(int column) const
{
    if (column >= 0 && column < fields_.size())
        return fields_.at(column);
    return virtualNames_.value(column - fields_.size());
}

int BureauTableModel::fieldColumn(const QString &field) const
{
    const int col = fields_.indexOf(field);
    if (col >= 0)
        return col;
    const int v = virtualNames_.indexOf(field);
    return v >= 0 ? fields_.size() + v : -1;
}

BureauTableModel::FieldKind BureauTableModel::kind(int column) const
{
    return kinds_.value(column, Text);
}

bool BureauTableModel::isComputed(int column) const
{
    return computed_.contains(column);
}

bool BureauTableModel::isReadOnlyColumn(int column) const
{
    return !editable_ || readOnly_.contains(column) || isComputed(column);
}

bool BureauTableModel::isRequired(int column) const
{
    return required_.contains(column);
}

QList<BureauTableModel::LookupItem> BureauTableModel::lookupItems(int column) const
{
    return lookups_.value(column).items;
}

QStringList BureauTableModel::choices(int column) const
{
    return choices_.value(column);
}

QVariant BureauTableModel::defaultValue(int column) const
{
    return defaults_.value(column);
}

int BureauTableModel::autoFillTarget(int sourceColumn) const
{
    return autoFill_.value(sourceColumn, -1);
}

QVariant BureauTableModel::lookupExtra(int column, qlonglong id) const
{
    const auto lookup = lookups_.constFind(column);
    if (lookup == lookups_.cend())
        return {};
    for (const LookupItem &item : lookup->items) {
        if (item.id == id)
            return item.extra;
    }
    return {};
}

bool BureauTableModel::isRowInserted(int row) const
{
    return QSqlTableModel::headerData(row, Qt::Vertical, Qt::DisplayRole).toString() == QStringLiteral("*");
}

bool BureauTableModel::isRowDeleted(int row) const
{
    return QSqlTableModel::headerData(row, Qt::Vertical, Qt::DisplayRole).toString() == QStringLiteral("!");
}

// ---------------------------------------------------------------- довідники для зовнішніх ключів

void BureauTableModel::loadLookups()
{
    for (auto it = lookups_.begin(); it != lookups_.end(); ++it) {
        Lookup &lookup = it.value();
        lookup.items.clear();
        lookup.shortLabels.clear();

        QSqlQuery q(database());
        if (!q.exec(lookup.sql)) {
            qWarning() << "Lookup error:" << q.lastError().text() << lookup.sql;
            continue;
        }
        const int columns = q.record().count();
        while (q.next()) {
            LookupItem item;
            item.id = q.value(0).toLongLong();
            const QString shortLabel = q.value(1).toString();
            item.label = columns > 2 ? q.value(2).toString() : shortLabel;
            if (columns > 3)
                item.extra = q.value(3);
            lookup.items << item;
            lookup.shortLabels.insert(item.id, shortLabel);
        }
    }
}

void BureauTableModel::refreshLookups()
{
    loadLookups();
    const int rows = rowCount();
    if (rows == 0)
        return;
    for (auto it = lookups_.cbegin(); it != lookups_.cend(); ++it)
        emit dataChanged(index(0, it.key()), index(rows - 1, it.key()));
}

// ---------------------------------------------------------------- значення

QVariant BureauTableModel::rawValue(int row, int column) const
{
    return QSqlTableModel::data(QSqlTableModel::index(row, column), Qt::EditRole);
}

QVariant BureauTableModel::computedValue(int row, const Computed &c) const
{
    QList<double> values;
    for (int dep : c.dependencies)
        values << (dep >= 0 ? rawValue(row, dep).toDouble() : 0.0);
    return c.fn ? c.fn(values) : QVariant();
}

QString BureauTableModel::displayText(int column, const QVariant &raw) const
{
    const FieldKind k = kind(column);
    if (raw.isNull() || (raw.typeId() == QMetaType::QString && raw.toString().isEmpty()))
        return (k == ForeignKey || k == Date || k == Choice) ? QStringLiteral("—") : QString();

    switch (k) {
    case ForeignKey: {
        const qlonglong id = raw.toLongLong();
        const auto lookup = lookups_.constFind(column);
        if (lookup != lookups_.cend()) {
            const auto label = lookup->shortLabels.constFind(id);
            if (label != lookup->shortLabels.cend())
                return label.value();
        }
        return QStringLiteral("#") + raw.toString();
    }
    case Date: {
        const QDate d = QDate::fromString(raw.toString(), Qt::ISODate);
        return d.isValid() ? d.toString(QStringLiteral("dd.MM.yyyy")) : raw.toString();
    }
    case Money:
        return QLocale().toString(raw.toDouble(), 'f', 2);
    case Quantity: {
        // до 3 знаків після коми, без зайвих нулів
        QString text = QLocale().toString(raw.toDouble(), 'f', 3);
        const QString point = QLocale().decimalPoint();
        if (text.contains(point)) {
            while (text.endsWith(u'0'))
                text.chop(1);
            if (text.endsWith(point))
                text.chop(point.size());
        }
        return text;
    }
    case Integer:
        return QString::number(raw.toLongLong());
    case Text:
    case Choice:
        break;
    }
    return raw.toString();
}

int BureauTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return baseColumnCount() + virtualNames_.size();
}

QVariant BureauTableModel::data(const QModelIndex &idx, int role) const
{
    if (!idx.isValid())
        return {};
    const int row = idx.row();
    const int col = idx.column();
    const auto computedIt = computed_.constFind(col);
    const bool computed = (computedIt != computed_.cend());
    if (!computed && col >= baseColumnCount())
        return {};

    auto raw = [&]() { return computed ? computedValue(row, computedIt.value()) : rawValue(row, col); };

    switch (role) {
    case Qt::EditRole:
        return raw();

    case Qt::DisplayRole:
        return displayText(col, raw());

    case SortRole: {
        const QVariant v = raw();
        if (v.isNull())
            return {};
        switch (kind(col)) {
        case ForeignKey:
            return displayText(col, v);
        case Money:
        case Quantity:
        case Integer:
            return v.toDouble();
        default:
            return v.toString();
        }
    }

    case Qt::TextAlignmentRole:
        switch (kind(col)) {
        case Money:
        case Quantity:
        case Integer:
            return int(Qt::AlignRight | Qt::AlignVCenter);
        case Date:
            return int(Qt::AlignCenter);
        default:
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }

    case Qt::ForegroundRole:
        if (isRowDeleted(row))
            return QColor(0x9A, 0x9A, 0x9E);
        if (computed && computedIt->positiveIsBad) {
            const QVariant v = raw();
            if (!v.isNull())
                return v.toDouble() > 0.005 ? QColor(0xB9, 0x1C, 0x1C) : QColor(0x2F, 0x8F, 0x4A);
        }
        return {};

    case Qt::BackgroundRole:
        if (isRowDeleted(row))
            return QColor(0xFB, 0xDD, 0xDD);
        if (isRowInserted(row))
            return QColor(0xD6, 0xF2, 0xD6);
        if (!computed && isDirty(idx))
            return QColor(0xFF, 0xF0, 0xBF);
        return {};

    case Qt::FontRole:
        if (isRowDeleted(row)) {
            QFont f;
            f.setStrikeOut(true);
            return f;
        }
        return {};

    case Qt::ToolTipRole:
        if (isRowDeleted(row))
            return QStringLiteral("Рядок буде видалено після натискання «Зберегти зміни»");
        if (isRowInserted(row))
            return QStringLiteral("Новий рядок — ще не збережено");
        if (computed)
            return QStringLiteral("Обчислюється автоматично");
        if (isDirty(idx))
            return QStringLiteral("Змінено — ще не збережено");
        return {};

    default:
        return computed ? QVariant() : QSqlTableModel::data(idx, role);
    }
}

bool BureauTableModel::setData(const QModelIndex &idx, const QVariant &value, int role)
{
    if (!idx.isValid() || isComputed(idx.column()) || idx.column() >= baseColumnCount())
        return false;

    const bool ok = QSqlTableModel::setData(idx, value, role);
    if (!ok || role != Qt::EditRole)
        return ok;

    // Автододавання: вибір запису довідника підставляє число (напр. суму доплати)
    const int target = autoFillTarget(idx.column());
    if (target >= 0 && !value.isNull()) {
        const QVariant extra = lookupExtra(idx.column(), value.toLongLong());
        if (extra.isValid() && !extra.isNull())
            QSqlTableModel::setData(index(idx.row(), target), extra.toDouble(), Qt::EditRole);
    }

    // Перераховуємо обчислювані поля, які залежать від зміненого
    for (auto it = computed_.cbegin(); it != computed_.cend(); ++it) {
        if (it->dependencies.contains(idx.column())) {
            const QModelIndex changed = index(idx.row(), it->column);
            emit dataChanged(changed, changed);
        }
    }
    return true;
}

QVariant BureauTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole)
            return Database::fieldTitle(fieldName(section));
        if (role == Qt::ToolTipRole) {
            if (isComputed(section))
                return QStringLiteral("Обчислюється автоматично");
            if (isRequired(section))
                return QStringLiteral("Обов’язкове поле");
            return {};
        }
        if (section >= baseColumnCount())
            return {};
    }
    return QSqlTableModel::headerData(section, orientation, role);
}

Qt::ItemFlags BureauTableModel::flags(const QModelIndex &idx) const
{
    if (!idx.isValid())
        return Qt::NoItemFlags;
    const int col = idx.column();
    if (isComputed(col) || col >= baseColumnCount())
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    Qt::ItemFlags f = QSqlTableModel::flags(idx);
    if (isReadOnlyColumn(col) || isRowDeleted(idx.row()))
        f &= ~Qt::ItemIsEditable;
    return f;
}

// ---------------------------------------------------------------- читання і запис

bool BureauTableModel::select()
{
    loadLookups();
    if (!QSqlTableModel::select())
        return false;
    // Завантажуємо всі рядки, щоб пошук і сортування працювали по всій таблиці
    while (canFetchMore())
        fetchMore();
    return true;
}

QSqlRecord BureauTableModel::withoutComputed(const QSqlRecord &values) const
{
    QSqlRecord rec(values);
    // Обчислювані поля не можна записувати в БД (якщо драйвер їх показує в записі)
    for (auto it = computed_.cbegin(); it != computed_.cend(); ++it) {
        if (it->column < rec.count())
            rec.setGenerated(it->column, false);
    }
    return rec;
}

bool BureauTableModel::insertRowIntoTable(const QSqlRecord &values)
{
    return QSqlTableModel::insertRowIntoTable(withoutComputed(values));
}

bool BureauTableModel::updateRowInTable(int row, const QSqlRecord &values)
{
    return QSqlTableModel::updateRowInTable(row, withoutComputed(values));
}
