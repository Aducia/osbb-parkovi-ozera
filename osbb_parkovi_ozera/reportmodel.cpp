#include "reportmodel.h"

#include <QDate>
#include <QLocale>
#include <QRegularExpression>

namespace {

bool isIsoDate(const QVariant &v)
{
    static const QRegularExpression re(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}$"));
    return v.typeId() == QMetaType::QString && re.match(v.toString()).hasMatch();
}

bool isNumber(const QVariant &v)
{
    switch (v.typeId()) {
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Double:
        return true;
    default:
        return false;
    }
}

} // namespace

ReportModel::ReportModel(QObject *parent)
    : QSqlQueryModel(parent)
{
}

QString ReportModel::formatDate(const QString &iso)
{
    const QDate d = QDate::fromString(iso, Qt::ISODate);
    return d.isValid() ? d.toString(QStringLiteral("dd.MM.yyyy")) : iso;
}

QVariant ReportModel::data(const QModelIndex &index, int role) const
{
    const QVariant v = QSqlQueryModel::data(index, Qt::DisplayRole);

    if (role == Qt::DisplayRole) {
        if (v.isNull())
            return QStringLiteral("—");
        if (v.typeId() == QMetaType::Double)
            return QLocale().toString(v.toDouble(), 'f', 2);
        if (isIsoDate(v))
            return formatDate(v.toString());
        return v;
    }
    if (role == Qt::TextAlignmentRole) {
        if (isNumber(v))
            return int(Qt::AlignRight | Qt::AlignVCenter);
        if (isIsoDate(v))
            return int(Qt::AlignCenter);
        return int(Qt::AlignLeft | Qt::AlignVCenter);
    }
    return QSqlQueryModel::data(index, role);
}

QString ReportModel::exportText(const QModelIndex &index) const
{
    const QVariant v = QSqlQueryModel::data(index, Qt::DisplayRole);
    if (v.isNull())
        return {};
    if (v.typeId() == QMetaType::Double) {
        QLocale locale;
        locale.setNumberOptions(QLocale::OmitGroupSeparator);
        return locale.toString(v.toDouble(), 'f', 2);
    }
    if (isIsoDate(v))
        return formatDate(v.toString());
    return v.toString();
}
