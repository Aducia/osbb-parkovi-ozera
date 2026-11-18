#include "tablepage.h"

#include "bureaudelegate.h"
#include "bureautablemodel.h"
#include "database.h"
#include "recorddialog.h"
#include "style.h"

#include <QAbstractButton>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QSqlError>
#include <QTableView>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>

namespace {
// Зростає після кожного збереження на будь-якій сторінці: інші сторінки
// при відкритті перечитують дані, лише якщо версія змінилася.
int g_dataVersion = 0;
}

// ---------------------------------------------------------------- діалоги

TablePage::PendingChoice TablePage::askPending(QWidget *parent, const QString &text)
{
    QMessageBox box(QMessageBox::Question, QStringLiteral("Незбережені зміни"), text,
                    QMessageBox::NoButton, parent);
    QPushButton *save = box.addButton(QStringLiteral("Зберегти"), QMessageBox::AcceptRole);
    QPushButton *discard = box.addButton(QStringLiteral("Не зберігати"), QMessageBox::DestructiveRole);
    QPushButton *cancel = box.addButton(QStringLiteral("Скасувати"), QMessageBox::RejectRole);
    box.setDefaultButton(save);
    box.setEscapeButton(cancel);
    box.exec();
    if (box.clickedButton() == save)
        return SaveChoice;
    if (box.clickedButton() == discard)
        return DiscardChoice;
    return CancelChoice;
}

bool TablePage::confirm(QWidget *parent, const QString &title, const QString &text,
                        const QString &yesText)
{
    QMessageBox box(QMessageBox::Question, title, text, QMessageBox::NoButton, parent);
    QPushButton *yes = box.addButton(yesText, QMessageBox::AcceptRole);
    QPushButton *no = box.addButton(QStringLiteral("Скасувати"), QMessageBox::RejectRole);
    box.setDefaultButton(no);
    box.setEscapeButton(no);
    box.exec();
    return box.clickedButton() == yes;
}

// ---------------------------------------------------------------- побудова сторінки

TablePage::TablePage(BureauTableModel *model, const QString &title, QWidget *parent)
    : QWidget(parent)
    , model_(model)
    , title_(title)
    , canEdit_(model->isEditable())
{
    proxy_ = new QSortFilterProxyModel(this);
    proxy_->setSourceModel(model_);
    proxy_->setSortRole(BureauTableModel::SortRole);
    proxy_->setSortLocaleAware(true);
    proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxy_->setFilterKeyColumn(-1);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 18, 24, 18);
    root->setSpacing(12);

    // Рядок пошуку
    auto *searchRow = new QHBoxLayout();
    searchRow->setSpacing(8);
    search_ = new QLineEdit(this);
    search_->setPlaceholderText(QStringLiteral("Пошук…  (Ctrl+F)"));
    search_->setClearButtonEnabled(true);
    search_->setMinimumWidth(320);
    searchRow->addWidget(search_);

    searchColumn_ = new QComboBox(this);
    searchColumn_->addItem(QStringLiteral("Усі поля"), -1);
    for (int col = 0; col < model_->columnCount(); ++col)
        searchColumn_->addItem(model_->headerData(col, Qt::Horizontal).toString(), col);
    searchRow->addWidget(searchColumn_);
    searchRow->addStretch();

    countLabel_ = new QLabel(this);
    countLabel_->setObjectName(QStringLiteral("countLabel"));
    searchRow->addWidget(countLabel_);
    root->addLayout(searchRow);

    // Рядок дій
    auto *actions = new QHBoxLayout();
    actions_ = actions;
    actions->setSpacing(8);
    if (canEdit_) {
        addButton_ = new QPushButton(QStringLiteral("+  Додати"), this);
        addButton_->setToolTip(QStringLiteral("Новий запис через форму (Ctrl+N)"));
        Style::setKind(addButton_, QStringLiteral("primary"));
        editButton_ = new QPushButton(QStringLiteral("Редагувати"), this);
        editButton_->setToolTip(QStringLiteral("Відкрити вибраний запис у формі (Enter)"));
        deleteButton_ = new QPushButton(QStringLiteral("Видалити"), this);
        deleteButton_->setToolTip(QStringLiteral("Видалити вибрані записи (Delete)"));
        Style::setKind(deleteButton_, QStringLiteral("danger"));
        actions->addWidget(addButton_);
        actions->addWidget(editButton_);
        actions->addWidget(deleteButton_);
    }
    actions->addStretch();

    pendingLabel_ = new QLabel(QStringLiteral("● Є незбережені зміни"), this);
    pendingLabel_->setObjectName(QStringLiteral("pendingLabel"));
    pendingLabel_->hide();
    actions->addWidget(pendingLabel_);

    if (canEdit_) {
        saveButton_ = new QPushButton(QStringLiteral("Зберегти зміни"), this);
        saveButton_->setToolTip(QStringLiteral("Записати зміни в базу даних (Ctrl+S)"));
        Style::setKind(saveButton_, QStringLiteral("success"));
        revertButton_ = new QPushButton(QStringLiteral("Скасувати зміни"), this);
        actions->addWidget(saveButton_);
        actions->addWidget(revertButton_);
    }
    reloadButton_ = new QPushButton(QStringLiteral("Оновити"), this);
    reloadButton_->setToolTip(QStringLiteral("Перечитати дані з бази (F5)"));
    actions->addWidget(reloadButton_);
    root->addLayout(actions);

    // Таблиця
    view_ = new QTableView(this);
    view_->setModel(proxy_);
    view_->setItemDelegate(new BureauDelegate(model_, view_));
    view_->setSortingEnabled(true);
    view_->sortByColumn(0, Qt::AscendingOrder);
    view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view_->setAlternatingRowColors(true);
    view_->setWordWrap(false);
    view_->verticalHeader()->hide();
    view_->verticalHeader()->setDefaultSectionSize(30);
    view_->horizontalHeader()->setStretchLastSection(true);
    view_->horizontalHeader()->setHighlightSections(false);
    view_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    view_->setEditTriggers(canEdit_ ? (QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed)
                                    : QAbstractItemView::NoEditTriggers);
    root->addWidget(view_, 1);

    auto *hint = new QLabel(canEdit_
        ? QStringLiteral("Подвійний клік по комірці — швидке редагування в таблиці (зміни підсвічуються, "
                         "записуються кнопкою «Зберегти зміни»). Кнопка «Редагувати» відкриває форму.")
        : QStringLiteral("Режим перегляду: змінювати дані може лише адміністратор."), this);
    hint->setObjectName(QStringLiteral("hint"));
    hint->setWordWrap(true);
    root->addWidget(hint);

    // Сигнали
    connect(search_, &QLineEdit::textChanged, this, &TablePage::applyFilter);
    connect(searchColumn_, &QComboBox::currentIndexChanged, this, &TablePage::applyFilter);
    connect(reloadButton_, &QPushButton::clicked, this, &TablePage::reload);
    if (canEdit_) {
        connect(addButton_, &QPushButton::clicked, this, &TablePage::addRecord);
        connect(editButton_, &QPushButton::clicked, this, &TablePage::editRecord);
        connect(deleteButton_, &QPushButton::clicked, this, &TablePage::deleteRecords);
        connect(saveButton_, &QPushButton::clicked, this, &TablePage::save);
        connect(revertButton_, &QPushButton::clicked, this, &TablePage::revert);
    }

    connect(view_->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TablePage::updateState);
    connect(model_, &QAbstractItemModel::dataChanged, this, &TablePage::updateState);
    connect(model_, &QAbstractItemModel::headerDataChanged, this, &TablePage::updateState);
    connect(model_, &QAbstractItemModel::rowsInserted, this, &TablePage::updateState);
    connect(model_, &QAbstractItemModel::rowsRemoved, this, &TablePage::updateState);
    connect(model_, &QAbstractItemModel::modelReset, this, &TablePage::updateState);
    connect(proxy_, &QAbstractItemModel::layoutChanged, this, &TablePage::updateState);

    // Гарячі клавіші (діють, коли фокус усередині сторінки)
    auto shortcut = [this](const QKeySequence &keys, auto slot) {
        auto *sc = new QShortcut(keys, this);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, slot);
    };
    shortcut(QKeySequence::Find, [this] { search_->setFocus(); search_->selectAll(); });
    shortcut(QKeySequence(Qt::Key_F5), [this] { reload(); });
    if (canEdit_) {
        shortcut(QKeySequence::New, [this] { addRecord(); });
        shortcut(QKeySequence::Save, [this] { save(); });
        shortcut(QKeySequence(Qt::Key_Delete), [this] {
            if (view_->hasFocus())   // не під час редагування комірки
                deleteRecords();
        });
        shortcut(QKeySequence(Qt::Key_Return), [this] {
            if (view_->hasFocus())
                editRecord();
        });
    }

    updateState();
}

// ---------------------------------------------------------------- стан

bool TablePage::hasPendingChanges() const
{
    return model_->isDirty();
}

void TablePage::updateState()
{
    const bool dirty = model_->isDirty();
    pendingLabel_->setVisible(dirty);
    if (saveButton_)
        saveButton_->setEnabled(dirty);
    if (revertButton_)
        revertButton_->setEnabled(dirty);

    const int selected = view_->selectionModel()->selectedRows().size();
    if (editButton_)
        editButton_->setEnabled(selected == 1);
    if (deleteButton_)
        deleteButton_->setEnabled(selected > 0);

    const int total = model_->rowCount();
    const int shown = proxy_->rowCount();
    countLabel_->setText(shown == total
                             ? QStringLiteral("Записів: %1").arg(total)
                             : QStringLiteral("Знайдено: %1 з %2").arg(shown).arg(total));
}

void TablePage::applyFilter()
{
    proxy_->setFilterKeyColumn(searchColumn_->currentData().toInt());
    proxy_->setFilterFixedString(search_->text().trimmed());
    updateState();
}

void TablePage::fitColumns()
{
    view_->resizeColumnsToContents();
    for (int col = 0; col < proxy_->columnCount(); ++col) {
        const int w = view_->columnWidth(col);
        view_->setColumnWidth(col, std::clamp(w + 16, 70, 320));
    }
    columnsFitted_ = true;
}

void TablePage::activate()
{
    // Дані могли змінитися на інших сторінках (наприклад, довідники)
    if (!model_->isDirty() && (!loaded_ || seenVersion_ != g_dataVersion)) {
        model_->select();
        loaded_ = true;
        seenVersion_ = g_dataVersion;
    } else {
        model_->refreshLookups();
    }
    if (!columnsFitted_)
        fitColumns();
    updateState();
    view_->setFocus();
}

QList<int> TablePage::selectedRows() const
{
    QList<int> rows;
    const auto indexes = view_->selectionModel()->selectedRows();
    for (const QModelIndex &idx : indexes) {
        const int row = proxy_->mapToSource(idx).row();
        if (row >= 0 && !rows.contains(row))
            rows << row;
    }
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    return rows;
}

int TablePage::findRow(const QSqlRecord &primaryKey) const
{
    for (int row = 0; row < model_->rowCount(); ++row) {
        if (model_->rowKey(row) == primaryKey)
            return row;
    }
    return -1;
}

// ---------------------------------------------------------------- збереження

bool TablePage::saveChanges()
{
    if (!model_->isDirty())
        return true;
    if (model_->submitAll()) {
        markSaved();
        updateState();
        return true;
    }
    QMessageBox::warning(this, QStringLiteral("Не вдалося зберегти"),
                         Database::friendlyError(model_->lastError())
                             + QStringLiteral("\n\nРядки з помилкою залишилися незбереженими (підсвічені). "
                                              "Виправте їх і натисніть «Зберегти зміни» ще раз "
                                              "або «Скасувати зміни»."));
    updateState();
    return false;
}

void TablePage::discardChanges()
{
    model_->revertAll();
    updateState();
}

void TablePage::save()
{
    // Якщо комірка ще редагується — фокус на таблицю, щоб значення записалося в модель
    view_->setFocus();
    saveChanges();
}

void TablePage::revert()
{
    if (!model_->isDirty())
        return;
    if (confirm(this, QStringLiteral("Скасування змін"),
                QStringLiteral("Відкинути всі незбережені зміни в цій таблиці?"),
                QStringLiteral("Відкинути")))
        discardChanges();
}

void TablePage::reload()
{
    if (model_->isDirty()) {
        if (!confirm(this, QStringLiteral("Оновлення"),
                     QStringLiteral("У таблиці є незбережені зміни. Відкинути їх і перечитати дані?"),
                     QStringLiteral("Відкинути й оновити")))
            return;
        model_->revertAll();
    }
    model_->select();
    updateState();
}

bool TablePage::resolvePendingBeforeForm()
{
    if (!model_->isDirty())
        return true;
    switch (askPending(this, QStringLiteral("У таблиці є незбережені зміни.\n"
                                            "Зберегти їх перед відкриттям форми?"))) {
    case SaveChoice:
        return saveChanges();
    case DiscardChoice:
        discardChanges();
        return true;
    case CancelChoice:
        break;
    }
    return false;
}

bool TablePage::submitForm(int row, const QHash<int, QVariant> &values)
{
    int target = row;
    if (target < 0) {
        target = model_->rowCount();
        if (!model_->insertRow(target)) {
            QMessageBox::warning(this, QStringLiteral("Помилка"),
                                 Database::friendlyError(model_->lastError()));
            return false;
        }
    }
    // У порядку колонок: значення з форми (зокрема змінена вручну доплата)
    // записуються після автододавання від довідника
    QList<int> columns = values.keys();
    std::sort(columns.begin(), columns.end());
    for (int col : columns)
        model_->setData(model_->index(target, col), values.value(col), Qt::EditRole);

    if (model_->submitAll()) {
        markSaved();
        updateState();
        return true;
    }
    const QString error = Database::friendlyError(model_->lastError());
    model_->revertAll();
    QMessageBox::warning(this, QStringLiteral("Не вдалося зберегти"),
                         error + QStringLiteral("\n\nВиправте дані у формі й спробуйте ще раз."));
    updateState();
    return false;
}

// ---------------------------------------------------------------- дії

void TablePage::addRecord()
{
    if (!canEdit_ || !resolvePendingBeforeForm())
        return;
    RecordDialog dialog(model_, -1, title_, this);
    while (dialog.exec() == QDialog::Accepted) {
        if (submitForm(-1, dialog.values())) {
            if (view_->horizontalHeader()->sortIndicatorOrder() == Qt::DescendingOrder)
                view_->scrollToTop();
            else
                view_->scrollToBottom();
            return;
        }
    }
}

void TablePage::editRecord()
{
    if (!canEdit_)
        return;
    const QList<int> rows = selectedRows();
    if (rows.size() != 1)
        return;
    if (model_->isRowDeleted(rows.first()))
        return;

    // Запам'ятовуємо первинний ключ: після збереження інших змін рядки перечитуються
    const bool inserted = model_->isRowInserted(rows.first());
    const QSqlRecord key = model_->rowKey(rows.first());
    if (!resolvePendingBeforeForm())
        return;
    const int row = inserted ? -1 : findRow(key);
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("Редагування"),
                                 QStringLiteral("Запис не знайдено. Виберіть його ще раз."));
        return;
    }

    RecordDialog dialog(model_, row, title_, this);
    while (dialog.exec() == QDialog::Accepted) {
        const int current = findRow(key);
        if (current < 0 || submitForm(current, dialog.values()))
            return;
    }
}

void TablePage::deleteRecords()
{
    if (!canEdit_)
        return;
    const QList<int> rows = selectedRows();
    if (rows.isEmpty())
        return;

    const QString question = rows.size() == 1
        ? QStringLiteral("Видалити вибраний запис?")
        : QStringLiteral("Видалити вибрані записи (%1)?").arg(rows.size());
    if (!confirm(this, QStringLiteral("Видалення"), question, QStringLiteral("Видалити")))
        return;

    const bool hadPending = model_->isDirty();
    for (int row : rows)
        model_->removeRow(row);

    if (hadPending) {
        // Є інші незбережені зміни — видалення застосується разом з ними
        QMessageBox::information(this, QStringLiteral("Видалення"),
                                 QStringLiteral("Записи позначено до видалення. "
                                                "Натисніть «Зберегти зміни», щоб застосувати."));
        updateState();
        return;
    }
    if (model_->submitAll()) {
        markSaved();
    } else {
        const QString error = Database::friendlyError(model_->lastError());
        model_->revertAll();
        QMessageBox::warning(this, QStringLiteral("Не вдалося видалити"), error);
    }
    updateState();
}

void TablePage::markSaved()
{
    ++g_dataVersion;
    seenVersion_ = g_dataVersion;   // власна модель уже перечитана після submitAll()
    emit dataSaved();
}

void TablePage::setSort(int column, Qt::SortOrder order)
{
    view_->sortByColumn(column, order);
}

void TablePage::setExtraAction(const QString &text, const QString &toolTip, std::function<bool()> handler)
{
    if (!canEdit_)
        return;     // у режимі перегляду додаткові дії недоступні
    extraHandler_ = std::move(handler);
    extraButton_ = new QPushButton(text, this);
    extraButton_->setToolTip(toolTip);
    Style::setKind(extraButton_, QStringLiteral("accent"));
    actions_->insertWidget(actions_->indexOf(deleteButton_) + 1, extraButton_);
    connect(extraButton_, &QPushButton::clicked, this, &TablePage::runExtra);
}

void TablePage::runExtra()
{
    if (!canEdit_ || !extraHandler_ || !resolvePendingBeforeForm())
        return;
    if (extraHandler_()) {
        model_->select();
        markSaved();
        if (view_->horizontalHeader()->sortIndicatorOrder() == Qt::DescendingOrder)
            view_->scrollToTop();
        else
            view_->scrollToBottom();
        updateState();
    }
}
