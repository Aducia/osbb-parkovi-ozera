#ifndef REPORTMODEL_H
#define REPORTMODEL_H

#include <QSqlQueryModel>

// Модель результатів запиту (звіти, головна сторінка) з форматуванням:
// дробові числа — 2 знаки після коми, дати РРРР-ММ-ДД — як ДД.ММ.РРРР,
// числа вирівнюються праворуч, порожні значення — «—».
class ReportModel : public QSqlQueryModel
{
    Q_OBJECT
public:
    explicit ReportModel(QObject *parent = nullptr);

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    // Значення для експорту в CSV (без розділювачів тисяч, з комою в дробах)
    QString exportText(const QModelIndex &index) const;

    static QString formatDate(const QString &iso);
};

#endif // REPORTMODEL_H
