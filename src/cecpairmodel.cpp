/***************************************************************************
  cecpairmodel.cpp
  ----------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#include "cecpairmodel.h"

#include <QApplication>
#include <QComboBox>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>

#include "ecproject.h"
#include "table_delegate_utils.h"

namespace {

QString partitionLabel(int meth)
{
    switch (meth)
    {
        case 2: return CecPairModel::tr("H\xe2\x82\x82O only");
        case 3: return CecPairModel::tr("CO\xe2\x82\x82 only");
        default: return CecPairModel::tr("H\xe2\x82\x82O and CO\xe2\x82\x82");
    }
}

} // namespace

CecPairModel::CecPairModel(EcProject* ecProject, QObject* parent)
    : QAbstractTableModel(parent), ecProject_(ecProject)
{
    reload();
}

void CecPairModel::reload()
{
    beginResetModel();
    pairs_ = ecProject_->cecPairs();
    //> An empty list is the project saying nothing, in which case the engine
    //> derives the same-instrument default. Show that default rather than an
    //> empty table, so what the user sees is what the run will do.
    if (pairs_.isEmpty())
    {
        pairs_ = MeasurementRecords::defaultCecPairs(ecProject_->gasColumns());
    }
    endResetModel();
}

void CecPairModel::restoreDefaults()
{
    beginResetModel();
    pairs_ = MeasurementRecords::defaultCecPairs(ecProject_->gasColumns());
    endResetModel();
    commit();
}

void CecPairModel::commit()
{
    ecProject_->setCecPairs(pairs_);
}

int CecPairModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : pairs_.size();
}

int CecPairModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QString CecPairModel::labelFor(int recordIndex) const
{
    const auto& gases = ecProject_->gasColumns();
    if (recordIndex < 1 || recordIndex > gases.size())
    {
        return tr("(not set)");
    }
    return MeasurementRecords::gasLabel(gases, recordIndex - 1);
}

QVector<QPair<int, QString>> CecPairModel::channelChoices(const QString& slug) const
{
    QVector<QPair<int, QString>> out;
    const auto& gases = ecProject_->gasColumns();
    for (int i = 0; i < gases.size(); ++i)
    {
        if (gases.at(i).slug.compare(slug, Qt::CaseInsensitive) != 0) { continue; }
        out.append(qMakePair(i + 1, MeasurementRecords::gasLabel(gases, i)));
    }
    return out;
}

QVector<QPair<int, QString>> CecPairModel::extraChoices(int row) const
{
    QVector<QPair<int, QString>> out;
    if (row < 0 || row >= pairs_.size()) { return out; }

    const auto& pair = pairs_.at(row);
    const auto& gases = ecProject_->gasColumns();
    for (int i = 0; i < gases.size(); ++i)
    {
        //> The pairing's own two channels are already targets one and two.
        //> Offering them again would partition the same series twice under two
        //> names.
        if (i + 1 == pair.carbonIndex || i + 1 == pair.waterIndex) { continue; }
        if (gases.at(i).rawColumn <= 0) { continue; }
        out.append(qMakePair(i + 1, MeasurementRecords::gasLabel(gases, i)));
    }
    return out;
}

bool CecPairModel::crossAnalyser(int row) const
{
    if (row < 0 || row >= pairs_.size()) { return false; }
    const auto& pair = pairs_.at(row);
    const auto& gases = ecProject_->gasColumns();
    if (pair.carbonIndex < 1 || pair.carbonIndex > gases.size()) { return false; }
    if (pair.waterIndex < 1 || pair.waterIndex > gases.size()) { return false; }
    return gases.at(pair.carbonIndex - 1).instrumentId
           != gases.at(pair.waterIndex - 1).instrumentId;
}

QVariant CecPairModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= pairs_.size()) { return {}; }
    const auto& pair = pairs_.at(index.row());

    if (role == Qt::CheckStateRole && index.column() == Use)
    {
        return pair.meth == 0 ? Qt::Unchecked : Qt::Checked;
    }

    if (role == Qt::UserRole)
    {
        switch (index.column())
        {
            case Carbon: return pair.carbonIndex;
            case Water: return pair.waterIndex;
            case Partition: return pair.meth == 0 ? 1 : pair.meth;
            case Extra:
            {
                //> The record indices, not the labels. Two channels of one
                //> species carry the same label, so a caller matching on text
                //> would tick both and write one.
                QVariantList out;
                for (int idx : pair.extraIndices) { out << idx; }
                return out;
            }
            default: return {};
        }
    }

    if (role == Qt::DecorationRole && index.column() == Warning && crossAnalyser(index.row()))
    {
        return QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning);
    }

    if (role == Qt::ToolTipRole)
    {
        if (index.column() == Warning && crossAnalyser(index.row()))
        {
            return tr("<b>Two analysers in one pairing.</b> The octants are the "
                      "signs of the vertical wind, the water and the carbon "
                      "dioxide together, so this pairing builds them from two "
                      "instruments \xe2\x80\x93 which reach the sensor at "
                      "different time lags and through different spectral "
                      "responses. Use the water on the carbon channel's own "
                      "analyser where the site has one.");
        }
        if (index.column() == Extra)
        {
            return tr("<b>Additional species:</b> partitioned in this pairing's "
                      "octants, without changing them. Zahn et al. define the "
                      "octants on the vertical wind, the water and the carbon "
                      "dioxide alone and reuse them unchanged for the second "
                      "scalar, so any species measured alongside can be split "
                      "the same way. Carbonyl sulfide is the useful case: its "
                      "plant uptake is stomatal and irreversible, so the "
                      "stomatal component is a cleaner photosynthesis proxy "
                      "than the CO\xe2\x82\x82 one.");
        }
        if (index.column() == Partition)
        {
            return tr("<b>Partition:</b> which of the pairing's own two fluxes "
                      "are reported. Both channels are read whichever you "
                      "choose \xe2\x80\x93 the octants need them together.");
        }
    }

    if (role != Qt::DisplayRole && role != Qt::EditRole) { return {}; }

    switch (index.column())
    {
        case Carbon: return labelFor(pair.carbonIndex);
        case Water:
            return pair.waterIndex > 0
                       ? labelFor(pair.waterIndex)
                       : tr("(same analyser)");
        case Partition: return partitionLabel(pair.meth == 0 ? 1 : pair.meth);
        case Extra:
        {
            QStringList names;
            for (int idx : pair.extraIndices) { names << labelFor(idx); }
            return names.isEmpty() ? tr("(none)") : names.join(QStringLiteral(", "));
        }
        default: return {};
    }
}

bool CecPairModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() >= pairs_.size()) { return false; }
    auto& pair = pairs_[index.row()];

    if (role == Qt::CheckStateRole && index.column() == Use)
    {
        //> Off is meth zero, and switching back on restores water-and-carbon
        //> rather than whatever it was: the partition column is disabled while
        //> the row is off, so there is nothing for the user to have chosen.
        pair.meth = (value.toInt() == Qt::Checked) ? 1 : 0;
        emit dataChanged(index, this->index(index.row(), ColumnCount - 1));
        commit();
        return true;
    }

    if (role != Qt::EditRole && role != Qt::UserRole) { return false; }

    switch (index.column())
    {
        case Carbon: pair.carbonIndex = value.toInt(); break;
        case Water: pair.waterIndex = value.toInt(); break;
        case Partition: pair.meth = value.toInt(); break;
        case Extra:
        {
            QVector<int> extras;
            for (const auto& v : value.toList())
            {
                const int idx = v.toInt();
                if (idx > 0 && extras.size() < 4) { extras.append(idx); }
            }
            pair.extraIndices = extras;
            break;
        }
        default: return false;
    }

    emit dataChanged(this->index(index.row(), 0),
                     this->index(index.row(), ColumnCount - 1));
    commit();
    return true;
}

QVariant CecPairModel::headerData(int section, Qt::Orientation orientation,
                                  int role) const
{
    if (orientation == Qt::Vertical)
    {
        if (role == Qt::DisplayRole) { return section + 1; }
        return {};
    }
    if (role != Qt::DisplayRole) { return {}; }
    switch (section)
    {
        case Use: return tr("Use");
        case Carbon: return tr("CO\xe2\x82\x82 channel");
        case Water: return tr("H\xe2\x82\x82O channel");
        case Partition: return tr("Partition");
        case Extra: return tr("Additional species");
        case Warning: return QString();
        default: return {};
    }
}

Qt::ItemFlags CecPairModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) { return Qt::NoItemFlags; }
    auto f = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    if (index.column() == Use) { return f | Qt::ItemIsUserCheckable; }
    if (index.column() == Warning) { return f; }

    const bool on = index.row() < pairs_.size() && pairs_.at(index.row()).meth != 0;
    if (!on) { return Qt::ItemIsSelectable; }
    return f | Qt::ItemIsEditable;
}

void CecPairModel::addPair()
{
    CecPairRecord pair;
    const auto carbon = channelChoices(QStringLiteral("co2"));
    if (!carbon.isEmpty()) { pair.carbonIndex = carbon.first().first; }
    beginInsertRows(QModelIndex(), pairs_.size(), pairs_.size());
    pairs_.append(pair);
    endInsertRows();
    commit();
}

void CecPairModel::removePair(int row)
{
    if (row < 0 || row >= pairs_.size()) { return; }
    beginRemoveRows(QModelIndex(), row, row);
    pairs_.remove(row);
    endRemoveRows();
    commit();
}

// ---------------------------------------------------------------- delegate --

CecPairDelegate::CecPairDelegate(CecPairModel* model, QObject* parent)
    : QStyledItemDelegate(parent), model_(model)
{
}

QWidget* CecPairDelegate::createEditor(QWidget* parent,
                                       const QStyleOptionViewItem& option,
                                       const QModelIndex& index) const
{
    Q_UNUSED(option)
    if (index.column() == CecPairModel::Extra) { return nullptr; }

    auto combo = new QComboBox(parent);
    TableDelegateUtils::prepareComboEditor(combo, parent);

    if (index.column() == CecPairModel::Partition)
    {
        combo->addItem(partitionLabel(1), 1);
        combo->addItem(partitionLabel(2), 2);
        combo->addItem(partitionLabel(3), 3);
    }
    else
    {
        const auto slug = index.column() == CecPairModel::Carbon
                              ? QStringLiteral("co2")
                              : QStringLiteral("h2o");
        if (index.column() == CecPairModel::Water)
        {
            //> Zero is a real choice, not a blank: it tells the engine to take
            //> the water on the carbon channel's own analyser, which keeps
            //> following the site if the records are re-ordered.
            combo->addItem(tr("(same analyser)"), 0);
        }
        for (const auto& choice : model_->channelChoices(slug))
        {
            combo->addItem(choice.second, choice.first);
        }
    }

    TableDelegateUtils::showPopupQueued(combo);
    return combo;
}

void CecPairDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    auto combo = qobject_cast<QComboBox*>(editor);
    if (!combo) { return; }
    const int wanted = index.data(Qt::UserRole).toInt();
    const int at = combo->findData(wanted);
    combo->setCurrentIndex(at >= 0 ? at : 0);
}

void CecPairDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                   const QModelIndex& index) const
{
    auto combo = qobject_cast<QComboBox*>(editor);
    if (!combo) { return; }
    model->setData(index, combo->currentData(), Qt::UserRole);
}

void CecPairDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                            const QModelIndex& index) const
{
    const int column = index.column();
    const bool combo = column == CecPairModel::Carbon
                       || column == CecPairModel::Water
                       || column == CecPairModel::Partition
                       || column == CecPairModel::Extra;
    if (combo && (index.flags() & Qt::ItemIsEditable))
    {
        TableDelegateUtils::paintComboCell(painter, option, index);
        return;
    }
    QStyledItemDelegate::paint(painter, option, index);
}

QSize CecPairDelegate::sizeHint(const QStyleOptionViewItem& option,
                                const QModelIndex& index) const
{
    const QSize base = QStyledItemDelegate::sizeHint(option, index);
    const int column = index.column();
    if (column == CecPairModel::Carbon || column == CecPairModel::Water
        || column == CecPairModel::Partition || column == CecPairModel::Extra)
    {
        return TableDelegateUtils::comboCellSizeHint(option, base);
    }
    return base;
}

bool CecPairDelegate::editorEvent(QEvent* event, QAbstractItemModel* model,
                                  const QStyleOptionViewItem& option,
                                  const QModelIndex& index)
{
    //> The extra species are a set, not one of a list, so the cell opens a
    //> checkable menu rather than a combo. A combo with checkable items closes
    //> on the first click and would take four passes to select four species.
    if (index.column() == CecPairModel::Extra
        && event->type() == QEvent::MouseButtonRelease
        && (index.flags() & Qt::ItemIsEditable))
    {
        const auto choices = model_->extraChoices(index.row());
        if (choices.isEmpty()) { return true; }

        QVector<int> chosen;
        for (const auto& v : index.data(Qt::UserRole).toList())
        {
            chosen.append(v.toInt());
        }

        QMenu menu;
        QVector<QAction*> actions;
        for (const auto& choice : choices)
        {
            auto action = menu.addAction(choice.second);
            action->setCheckable(true);
            action->setData(choice.first);
            action->setChecked(chosen.contains(choice.first));
            actions.append(action);
        }
        auto mouse = static_cast<QMouseEvent*>(event);
        menu.exec(mouse->globalPosition().toPoint());

        QVariantList picked;
        for (auto action : actions)
        {
            if (action->isChecked()) { picked << action->data(); }
        }
        model->setData(index, picked, Qt::EditRole);
        return true;
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}
