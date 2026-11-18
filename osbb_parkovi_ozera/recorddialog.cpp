#include "recorddialog.h"

#include "bureaudelegate.h"
#include "bureautablemodel.h"
#include "database.h"
#include "style.h"

#include <QComboBox>
#include <QDate>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

RecordDialog::RecordDialog(BureauTableModel *model, int row, const QString &tableTitle,
                           QWidget *parent)
    : QDialog(parent)
    , model_(model)
{
    const bool isNew = (row < 0);
    setWindowTitle(isNew ? QStringLiteral("Новий запис — ") + tableTitle
                         : QStringLiteral("Редагування — ") + tableTitle);
    setMinimumWidth(480);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(12);

    auto *title = new QLabel(isNew ? QStringLiteral("Новий запис") : QStringLiteral("Редагування запису"), this);
    title->setObjectName(QStringLiteral("panelTitle"));
    root->addWidget(title);

    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(10);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    for (int col = 0; col < model->columnCount(); ++col) {
        // Код (автонумерація) і борг (обчислюється) у формі не редагуються
        if (model->isReadOnlyColumn(col))
            continue;

        QWidget *editor = BureauDelegate::makeEditor(model, col, this);
        const QVariant value = isNew ? model->defaultValue(col)
                                     : model->data(model->index(row, col), Qt::EditRole);
        BureauDelegate::setEditorValue(editor, model, col, value);

        QString label = Database::fieldTitle(model->fieldName(col));
        if (model->isRequired(col))
            label += QStringLiteral(" *");
        form->addRow(label + QStringLiteral(":"), editor);
        editors_.insert(col, editor);
    }
    root->addLayout(form);

    // Автододавання: вибір у списку (напр. типу доплати) підставляє суму в числове поле.
    // Підключаємо після заповнення полів, щоб відкриття форми не змінювало значень.
    for (auto it = editors_.cbegin(); it != editors_.cend(); ++it) {
        const int source = it.key();
        const int target = model->autoFillTarget(source);
        auto *combo = qobject_cast<QComboBox *>(it.value());
        auto *spin = editors_.contains(target) ? qobject_cast<QDoubleSpinBox *>(editors_.value(target)) : nullptr;
        if (target < 0 || !combo || !spin)
            continue;
        connect(combo, &QComboBox::currentIndexChanged, this, [model, combo, spin, source](int) {
            const QVariant id = combo->currentData();
            if (!id.isValid())
                return;
            const QVariant extra = model->lookupExtra(source, id.toLongLong());
            if (extra.isValid() && !extra.isNull())
                spin->setValue(extra.toDouble());
        });
    }

    auto *note = new QLabel(QStringLiteral("* — обов'язкові поля"), this);
    note->setObjectName(QStringLiteral("hint"));
    root->addWidget(note);

    errorLabel_ = new QLabel(this);
    errorLabel_->setObjectName(QStringLiteral("errorLabel"));
    errorLabel_->setWordWrap(true);
    errorLabel_->hide();
    root->addWidget(errorLabel_);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch();
    auto *cancel = new QPushButton(QStringLiteral("Скасувати"), this);
    auto *save = new QPushButton(QStringLiteral("Зберегти"), this);
    Style::setKind(save, QStringLiteral("primary"));
    save->setDefault(true);
    save->setMinimumWidth(110);
    buttons->addWidget(cancel);
    buttons->addWidget(save);
    root->addLayout(buttons);

    connect(save, &QPushButton::clicked, this, &RecordDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &RecordDialog::reject);
}

QHash<int, QVariant> RecordDialog::values() const
{
    QHash<int, QVariant> result;
    for (auto it = editors_.cbegin(); it != editors_.cend(); ++it)
        result.insert(it.key(), BureauDelegate::editorValue(it.value(), model_, it.key()));
    return result;
}

QString RecordDialog::validate() const
{
    // Обов'язкові поля
    for (auto it = editors_.cbegin(); it != editors_.cend(); ++it) {
        if (model_->isRequired(it.key())
            && BureauDelegate::isEditorEmpty(it.value(), model_, it.key())) {
            return QStringLiteral("Заповніть поле «%1».")
                .arg(Database::fieldTitle(model_->fieldName(it.key())));
        }
    }

    const QHash<int, QVariant> v = values();
    auto valueOf = [&](const QString &field) {
        const int col = model_->fieldColumn(field);
        return col >= 0 ? v.value(col) : QVariant();
    };

    // Ті самі правила, що й CHECK-обмеження в БД
    for (const QString &field : {QStringLiteral("кількість_робітників"), QStringLiteral("тривалість"),
                                 QStringLiteral("кількість")}) {
        const QVariant x = valueOf(field);
        if (!x.isNull() && x.toDouble() <= 0)
            return QStringLiteral("Поле «%1» має бути більшим за нуль.").arg(Database::fieldTitle(field));
    }

    const QVariant start = valueOf(QStringLiteral("дата_початку"));
    const QVariant end = valueOf(QStringLiteral("дата_закінчення"));
    if (!end.isNull() && start.isNull())
        return QStringLiteral("Вкажіть дату початку: без неї не можна задати дату завершення.");
    if (!start.isNull() && !end.isNull() && end.toString() < start.toString())
        return QStringLiteral("Дата завершення не може бути раніше дати початку.");

    if (valueOf(QStringLiteral("тип_локації")).toString() == QStringLiteral("Квартира")
        && model_->fieldColumn(QStringLiteral("квартира")) >= 0
        && valueOf(QStringLiteral("квартира")).isNull())
        return QStringLiteral("Для локації «Квартира» вкажіть квартиру.");

    if (valueOf(QStringLiteral("рахунок")).toString() == QStringLiteral("Індивідуальний")
        && valueOf(QStringLiteral("квартира")).isNull())
        return QStringLiteral("Для індивідуального рахунку вкажіть квартиру.");

    if (model_->fieldColumn(QStringLiteral("сплачено")) >= 0) {
        const double due = valueOf(QStringLiteral("нараховано")).toDouble()
                           + valueOf(QStringLiteral("доплата")).toDouble();
        if (valueOf(QStringLiteral("сплачено")).toDouble() > due + 0.001)
            return QStringLiteral("Сплачена сума не може перевищувати нараховану разом з доплатою.");
    }
    return {};
}

void RecordDialog::accept()
{
    const QString error = validate();
    if (!error.isEmpty()) {
        errorLabel_->setText(error);
        errorLabel_->show();
        return;
    }
    errorLabel_->hide();
    QDialog::accept();
}
