#ifndef RECORDDIALOG_H
#define RECORDDIALOG_H

#include <QDialog>
#include <QHash>
#include <QVariant>

class BureauTableModel;
class QLabel;

// Форма додавання / редагування одного запису.
// Поля будуються автоматично з опису моделі (тип поля, обов'язковість, довідники).
class RecordDialog : public QDialog
{
    Q_OBJECT
public:
    // row = -1 — новий запис
    RecordDialog(BureauTableModel *model, int row, const QString &tableTitle,
                 QWidget *parent = nullptr);

    // колонка -> значення
    QHash<int, QVariant> values() const;

protected:
    void accept() override;

private:
    QString validate() const;

    BureauTableModel *model_;
    QHash<int, QWidget *> editors_;
    QLabel *errorLabel_;
};

#endif // RECORDDIALOG_H
