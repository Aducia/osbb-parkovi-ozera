#ifndef INVOICEDIALOG_H
#define INVOICEDIALOG_H

#include <QDialog>
#include <QHash>

class QComboBox;
class QDoubleSpinBox;
class QLabel;

// Формування індивідуального рахунку власнику квартири за заявку:
// вартість робіт за видом + матеріали «за рахунок власника» + необов'язкова доплата
// (платний виклик підрядника, спецбригада). Результат — рядок у таблиці «оплати»
// з рахунком «Індивідуальний».
class InvoiceDialog : public QDialog
{
    Q_OBJECT
public:
    explicit InvoiceDialog(QWidget *parent = nullptr);

    bool created() const { return created_; }

private slots:
    void requestChanged();
    void surchargeChanged();
    void createInvoice();

private:
    struct Info {
        qlonglong flatId = 0;
        QString flat;
        QString work;
        double workCost = 0.0;
    };

    void updateTotals();

    QComboBox *request_;
    QComboBox *surcharge_;
    QDoubleSpinBox *surchargeAmount_;
    QLabel *flatLabel_;
    QLabel *workLabel_;
    QLabel *materialsLabel_;
    QLabel *totalLabel_;
    QLabel *noteLabel_;
    QLabel *errorLabel_;
    QHash<qlonglong, Info> infos_;
    double materials_ = 0.0;
    bool created_ = false;
};

#endif // INVOICEDIALOG_H
