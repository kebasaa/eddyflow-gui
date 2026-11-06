/***************************************************************************
  cecpairmodel.cpp
  ----------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#include "cecpairmodel.h"

#include <QApplication>
#include <QComboBox>
#include <QPainter>
#include <QStyle>

#include "ecproject.h"
#include "table_delegate_utils.h"

namespace {

bool isSpecies(const QVector<GasRecord>& gases, int recordIndex, const QString& slug)
{
    if (recordIndex < 1 || recordIndex > gases.size()) { return false; }
    return gases.at(recordIndex - 1).slug.compare(slug, Qt::CaseInsensitive) == 0;
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
    auto pairs = ecProject_->cecPairs();
    //> An empty list is the project saying nothing, in which case the engine
    //> derives the same-instrument default. Show that default rather than an
    //> empty table, so what the user sees is what the run will do.
    if (pairs.isEmpty())
    {
        pairs = MeasurementRecords::defaultCecPairs(ecProject_->gasColumns());
    }
    rows_ = MeasurementRecords::sortCecRowsForDisplay(
        ecProject_->gasColumns(),
        MeasurementRecords::cecRowsFromPairs(ecProject_->gasColumns(), pairs));
    refreshSuffixes();
    endResetModel();
}

void CecPairModel::restoreDefaults()
{
    const auto& gases = ecProject_->gasColumns();
    beginResetModel();
    rows_ = normalised(MeasurementRecords::cecRowsFromPairs(
        gases, MeasurementRecords::defaultCecPairs(gases)));
    refreshSuffixes();
    endResetModel();
    commit();
}

QVector<CecFluxRow> CecPairModel::normalised(const QVector<CecFluxRow>& rows) const
{
    const auto& gases = ecProject_->gasColumns();
    return MeasurementRecords::sortCecRowsForDisplay(
        gases, MeasurementRecords::cecRowsFromPairs(
                   gases, MeasurementRecords::cecPairsFromRows(gases, rows)));
}

/// The suffix the engine will give each row's columns, so that several
/// versions of one gas can be told apart before the run as well as after it.
void CecPairModel::refreshSuffixes()
{
    const auto& gases = ecProject_->gasColumns();
    const auto pairs = MeasurementRecords::cecPairsFromRows(gases, rows_);
    const auto suffixes = MeasurementRecords::cecPairSuffixes(gases, pairs);
    const auto pairOf = MeasurementRecords::cecRowPairIndices(gases, rows_);
    suffixes_.clear();
    for (int i = 0; i < rows_.size(); ++i)
    {
        const int at = pairOf.value(i, -1);
        suffixes_.append(at >= 0 && at < suffixes.size() ? suffixes.at(at) : QString());
    }
}

void CecPairModel::commit()
{
    ecProject_->setCecPairs(
        MeasurementRecords::cecPairsFromRows(ecProject_->gasColumns(), rows_));
}

/// Take \a candidate if the engine could run it, refuse it with the reason if
/// not. Taken through the pairings and back, so a row that joins an existing
/// pairing moves next to it at once rather than on the next reload.
bool CecPairModel::apply(const QVector<CecFluxRow>& candidate)
{
    const auto& gases = ecProject_->gasColumns();
    QString reason;
    if (!MeasurementRecords::cecRowsFit(gases, candidate, &reason))
    {
        emit editRefused(reason);
        return false;
    }
    const auto rows = normalised(candidate);
    if (rows.size() == rows_.size())
    {
        //> The usual edit: one cell of one row. The rows are re-sorted
        //> alphabetically after it, so the edited row may move; a layout
        //> change carries the selection and any open editor along with it,
        //> which a reset in the middle of setModelData would not. candidate
        //> still has the old positions, so it says where each row went.
        emit layoutAboutToBeChanged();
        const auto before = persistentIndexList();
        rows_ = rows;
        refreshSuffixes();
        QModelIndexList after;
        for (const auto& old : before)
        {
            const int moved = old.row() < candidate.size()
                                  ? rowIn(rows_, candidate.at(old.row()))
                                  : -1;
            after.append(moved >= 0 ? index(moved, old.column()) : QModelIndex());
        }
        changePersistentIndexList(before, after);
        emit layoutChanged();
        if (!rows_.isEmpty())
        {
            //> The suffix and the warnings of other rows can change with it.
            emit dataChanged(index(0, 0), index(rows_.size() - 1, ColumnCount - 1));
        }
    }
    else
    {
        beginResetModel();
        rows_ = rows;
        refreshSuffixes();
        endResetModel();
    }
    commit();
    return true;
}

int CecPairModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : rows_.size();
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

QString CecPairModel::fluxLabel(const CecFluxRow& row) const
{
    //> The water of a pairing whose analyser carries none: the engine reads
    //> the designated hygrometer, which only the engine resolves.
    if (row.fluxIndex == 0) { return tr("H\xe2\x82\x82O (designated hygrometer)"); }
    return labelFor(row.fluxIndex);
}

QString CecPairModel::octantLabel(int recordIndex) const
{
    if (recordIndex == 0) { return tr("CO\xe2\x82\x82 (designated)"); }
    if (isSpecies(ecProject_->gasColumns(), recordIndex, QStringLiteral("cos")))
    {
        return tr("%1 \xe2\x80\x93 experimental").arg(labelFor(recordIndex));
    }
    return labelFor(recordIndex);
}

QVector<QPair<int, QString>> CecPairModel::choices(int column) const
{
    QVector<QPair<int, QString>> out;
    const auto& gases = ecProject_->gasColumns();

    if (column == Flux)
    {
        for (const int i : MeasurementRecords::gasDisplayOrder(gases))
        {
            out.append(qMakePair(i + 1, MeasurementRecords::gasLabel(gases, i)));
        }
        return out;
    }

    const auto add = [&](const QString& slug)
    {
        for (int i = 0; i < gases.size(); ++i)
        {
            if (gases.at(i).rawColumn <= 0) { continue; }
            if (gases.at(i).slug.compare(slug, Qt::CaseInsensitive) != 0) { continue; }
            out.append(qMakePair(i + 1, column == Octants ? octantLabel(i + 1)
                                                          : labelFor(i + 1)));
        }
    };
    if (column == Water)
    {
        add(QStringLiteral("h2o"));
    }
    else if (column == Octants)
    {
        //> CO2 first: it is the method as published. Carbonyl sulfide after,
        //> labelled experimental in the list itself.
        add(QStringLiteral("co2"));
        add(QStringLiteral("cos"));
    }
    return out;
}

bool CecPairModel::crossAnalyser(int row) const
{
    if (row < 0 || row >= rows_.size()) { return false; }
    const auto& r = rows_.at(row);
    const auto& gases = ecProject_->gasColumns();
    if (r.octantIndex < 1 || r.octantIndex > gases.size()) { return false; }
    const auto& octantInstrument = gases.at(r.octantIndex - 1).instrumentId;
    if (!MeasurementRecords::isRealInstrument(octantInstrument)) { return false; }

    const int water = MeasurementRecords::resolvedCecWater(gases, r.octantIndex,
                                                           r.waterIndex);
    //> No water on the octant gas's analyser: the engine reads the designated
    //> hygrometer, which is somewhere else by construction.
    if (water < 1 || water > gases.size()) { return true; }
    const auto& waterInstrument = gases.at(water - 1).instrumentId;
    if (!MeasurementRecords::isRealInstrument(waterInstrument)) { return false; }
    return octantInstrument != waterInstrument;
}

bool CecPairModel::experimental(int row) const
{
    if (row < 0 || row >= rows_.size()) { return false; }
    return MeasurementRecords::isCecExperimental(ecProject_->gasColumns(), rows_.at(row));
}

bool CecPairModel::anyExperimental() const
{
    for (int i = 0; i < rows_.size(); ++i)
    {
        if (experimental(i)) { return true; }
    }
    return false;
}

/// What the row's flux becomes, in the words of the columns it comes out in.
QString CecPairModel::fluxTooltip(const CecFluxRow& row) const
{
    const auto& gases = ecProject_->gasColumns();
    const int water = MeasurementRecords::resolvedCecWater(gases, row.octantIndex,
                                                           row.waterIndex);
    const bool cosOctants = isSpecies(gases, row.octantIndex, QStringLiteral("cos"));
    const auto experimentalNote = tr(
        "<p><i>Experimental.</i> Zahn et al. (2022) define the method for water "
        "and CO\xe2\x82\x82 only.</p>");

    if (row.fluxIndex == 0 || (water > 0 && row.fluxIndex == water))
    {
        return tr("<b>Water, split into evaporation and transpiration.</b> "
                  "<i>E_cec</i> is the non-stomatal part \xe2\x80\x93 soil and "
                  "wet surfaces \xe2\x80\x93 and <i>Tr_cec</i> the part that left "
                  "through stomata, both also as mm h<sup>-1</sup> "
                  "(<i>E_cec_ET</i>, <i>Tr_cec_ET</i>). <i>r_ET_cec</i> is "
                  "E/Tr. Both are positive by construction; a downward total "
                  "(dewfall) is refused as wrong-signed.")
               + (cosOctants ? experimentalNote : QString());
    }

    const bool isCos = isSpecies(gases, row.fluxIndex, QStringLiteral("cos"));
    const bool isCo2 = isSpecies(gases, row.fluxIndex, QStringLiteral("co2"));

    if (isCo2 && row.fluxIndex == row.octantIndex)
    {
        return tr("<b>CO\xe2\x82\x82, split into respiration and "
                  "photosynthesis.</b> <i>Reco_cec</i> is the non-stomatal "
                  "part (soil, stems, leaves in the dark), positive; "
                  "<i>P_cec</i> the part taken up through stomata, negative. "
                  "<i>NEE_cec</i> is their sum and <i>r_Fc_cec</i> their "
                  "ratio.");
    }

    if (isCos)
    {
        return tr("<b>Carbonyl sulfide, split into canopy and ground.</b> "
                  "Written as <i>cos_stomatal_cec</i>, "
                  "<i>cos_nonstomatal_cec</i>, <i>cos_total_cec</i>, "
                  "<i>r_cos_cec</i> and <i>qc_cec_cos</i>."
                  "<ul>"
                  "<li><b>Stomatal</b> is uptake by leaves through their "
                  "stomata, and is expected to be negative. Scaled by the leaf "
                  "relative uptake it is a photosynthesis proxy independent of "
                  "the CO\xe2\x82\x82 partition.</li>"
                  "<li><b>Non-stomatal</b> is everything else: the ground, "
                  "which takes COS up or gives it off, and canopy exchange that "
                  "is not through stomata \xe2\x80\x93 a senescent canopy "
                  "giving COS off lands here. Either sign is plausible.</li>"
                  "<li><i>r_cos_cec</i> is non-stomatal over stomatal: positive "
                  "when the ground takes COS up alongside the canopy, negative "
                  "when it gives COS off.</li>"
                  "<li><i>cec_sign_cos</i> is 1 when the stomatal part came out "
                  "as emission \xe2\x80\x93 noise, a senescent canopy, or soil "
                  "uptake misattributed by COS octants. It is reported either "
                  "way.</li>"
                  "</ul>")
               + (cosOctants
                      ? tr("<p>Sorted by <b>COS octants</b>: soil uptake carried "
                           "in moist updrafts counts as stomatal here. Compare "
                           "with a row sorted by CO\xe2\x82\x82, and check "
                           "<i>cec_noisy_octants</i>: where it is 1 the sort was "
                           "noise.</p>")
                      : tr("<p>Sorted by CO\xe2\x82\x82 octants, so the sign of "
                           "COS plays no part in the sorting and the ground may "
                           "go either way.</p>"))
               + experimentalNote;
    }

    //> Anything else - methane, nitrous oxide, a second CO2 sorted by another
    //> channel's octants - is split the same way and named generically.
    return tr("<b>Split into stomatal and non-stomatal parts</b> in the octants "
              "of this row's octant gas and water, and written as "
              "<i>&lt;gas&gt;_stomatal_cec</i>, "
              "<i>&lt;gas&gt;_nonstomatal_cec</i>, <i>&lt;gas&gt;_total_cec</i> "
              "and <i>r_&lt;gas&gt;_cec</i>. The octants say which air passed "
              "through stomata; they say nothing about how this gas is "
              "exchanged, so read the split as \"carried by stomatal air\" and "
              "\"carried by the rest\".")
           + experimentalNote;
}

QVariant CecPairModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rows_.size()) { return {}; }
    const auto& row = rows_.at(index.row());
    const bool cross = crossAnalyser(index.row());
    const bool isExperimental = experimental(index.row());

    if (role == Qt::UserRole)
    {
        switch (index.column())
        {
            case Flux: return row.fluxIndex;
            case Water: return row.waterIndex;
            case Octants: return row.octantIndex;
            default: return {};
        }
    }

    if (role == Qt::DecorationRole && index.column() == Warning)
    {
        if (cross) { return QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning); }
        if (isExperimental)
        {
            return QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation);
        }
        return {};
    }

    if (role == Qt::ToolTipRole)
    {
        switch (index.column())
        {
            case Flux: return fluxTooltip(row);
            case Water:
                return tr("<b>Water:</b> the moist updrafts this row's parcels are "
                          "taken from. <i>(same analyser)</i> reads the H\xe2\x82\x82O "
                          "on the octant gas's own analyser, and keeps doing so if "
                          "the records are re-ordered.");
            case Octants:
                return isSpecies(ecProject_->gasColumns(), row.octantIndex,
                                 QStringLiteral("cos"))
                           ? tr("<b>Octants from carbonyl sulfide \xe2\x80\x93 "
                                "experimental.</b> A moist updraft that lost COS "
                                "counts as having passed through stomata. Sound "
                                "only where the ground is a net COS source or "
                                "negligible: many soils take COS up, and that "
                                "uptake is then counted as the canopy's. Where "
                                "the soil gives COS off, COS and CO\xe2\x82\x82 "
                                "octants sort alike."
                                "<p>COS is noisy at 10 Hz. Where its sign in the "
                                "updrafts is mostly noise, the sort is close to "
                                "random: COS's own split cancels and is refused "
                                "as singular, while every other gas of the row's "
                                "pairing comes out near half stomatal, half not "
                                "\xe2\x80\x93 with a quality code that looks fine. "
                                "Those periods are flagged 1 in "
                                "<i>cec_noisy_octants</i>, and the run log warns "
                                "(Warning 124).</p>")
                           : tr("<b>Octants from CO\xe2\x82\x82</b>, as Zahn et "
                                "al. (2022): a moist updraft that lost CO\xe2\x82\x82 "
                                "passed through stomata, one that gained it came "
                                "off the ground.");
            case Suffix:
                return tr("<b>Columns end in:</b> every column this row "
                          "produces carries this suffix \xe2\x80\x93 "
                          "<i>cos_stomatal_cec%1</i>, for instance \xe2\x80\x93 "
                          "and so do the other rows sorted by the same octant "
                          "gas and water. Splitting one gas several ways gives "
                          "each version its own.")
                    .arg(suffixes_.value(index.row()));
            case Warning:
            {
                QString text;
                if (cross)
                {
                    text += tr("<p><b>Two analysers in one pairing.</b> The "
                               "octants are the signs of the vertical wind, the "
                               "water and the octant gas together, so this row "
                               "builds them from two instruments \xe2\x80\x93 which "
                               "reach the sensor at different time lags and "
                               "through different spectral responses. Use the "
                               "water on the octant gas's own analyser where the "
                               "site has one.</p>");
                }
                if (isExperimental)
                {
                    text += tr("<p><b>Experimental.</b> This row goes beyond Zahn "
                               "et al. (2022), who partition water and CO\xe2\x82\x82 "
                               "by CO\xe2\x82\x82 octants. Read it alongside a "
                               "published partition, not instead of one.</p>");
                }
                return text.isEmpty() ? QVariant() : QVariant(text);
            }
            default: return {};
        }
    }

    if (role != Qt::DisplayRole && role != Qt::EditRole) { return {}; }

    switch (index.column())
    {
        case Flux:
        {
            const auto label = fluxLabel(row);
            if (row.fluxIndex > 0 && isExperimental
                && !isSpecies(ecProject_->gasColumns(), row.octantIndex,
                              QStringLiteral("cos")))
            {
                return tr("%1 \xe2\x80\x93 experimental").arg(label);
            }
            return label;
        }
        case Water:
            return row.waterIndex > 0 ? labelFor(row.waterIndex)
                                      : tr("(same analyser)");
        case Octants: return octantLabel(row.octantIndex);
        case Suffix: return suffixes_.value(index.row());
        default: return {};
    }
}

bool CecPairModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() >= rows_.size()) { return false; }
    if (role != Qt::EditRole && role != Qt::UserRole) { return false; }

    const auto& gases = ecProject_->gasColumns();
    auto candidate = rows_;
    auto& row = candidate[index.row()];
    const int picked = value.toInt();

    switch (index.column())
    {
        case Flux:
        {
            if (picked == row.fluxIndex) { return false; }
            //> A different flux is usually a different analyser, so the water
            //> and the octant gas follow it there - the same-instrument default
            //> the whole table starts from. Where that version of the flux is
            //> already in the table, the row keeps its own water and octant
            //> gas, and failing that takes the next combination not yet
            //> there: choosing a gas a second time is how a second version of
            //> it is made, and it must not land on the first and be refused.
            auto others = rows_;
            others.remove(index.row());
            const auto fits = [&](const CecFluxRow& r)
            {
                auto with = others;
                with.append(r);
                return MeasurementRecords::cecRowsFit(gases, with);
            };
            const CecFluxRow byDefault{picked,
                                       MeasurementRecords::defaultCecWater(gases, picked),
                                       MeasurementRecords::defaultCecOctant(gases, picked)};
            const CecFluxRow kept{picked, row.waterIndex, row.octantIndex};
            CecFluxRow next = byDefault;
            if (!fits(byDefault))
            {
                if (fits(kept)) { next = kept; }
                //> Nothing at all: left at the default, which apply() then
                //> refuses with the engine's own reason.
                else { MeasurementRecords::cecNextFreeVariant(
                           gases, others, picked, byDefault.waterIndex,
                           byDefault.octantIndex, &next); }
            }
            row = next;
            break;
        }
        case Water:
            if (picked == row.waterIndex) { return false; }
            row.waterIndex = picked;
            break;
        case Octants:
            if (picked == row.octantIndex) { return false; }
            row.octantIndex = picked;
            break;
        default: return false;
    }
    return apply(candidate);
}

QVariant CecPairModel::headerData(int section, Qt::Orientation orientation,
                                  int role) const
{
    if (orientation == Qt::Vertical)
    {
        if (role == Qt::DisplayRole) { return section + 1; }
        return {};
    }
    if (role == Qt::ToolTipRole)
    {
        switch (section)
        {
            case Flux:
                return tr("<b>Flux to partition:</b> any gas channel. Water and "
                          "CO\xe2\x82\x82 are split as Zahn et al. (2022) describe; "
                          "anything else \xe2\x80\x93 carbonyl sulfide above all "
                          "\xe2\x80\x93 is split in the same octants and is "
                          "experimental. Hover a cell for the columns it "
                          "produces and how to read them.");
            case Water:
                return tr("<b>Water channel:</b> selects the moist updrafts. "
                          "Defaults to the H\xe2\x82\x82O on the flux's own "
                          "analyser.");
            case Octants:
                return tr("<b>Octants from:</b> the gas whose sign sorts each "
                          "moist updraft into stomatal and non-stomatal. "
                          "CO\xe2\x82\x82 is the published method and the "
                          "default; carbonyl sulfide is experimental. Rows that "
                          "share an octant gas and a water are one pairing, and "
                          "their columns share one suffix.");
            case Suffix:
                return tr("<b>Columns end in:</b> the suffix of every output "
                          "column the row produces: the analyser of the octant "
                          "gas, with <i>_cos</i> for COS octants, and \xe2\x80\x93 "
                          "where two rows would otherwise share it \xe2\x80\x93 "
                          "<i>_h2o_</i> and the water's analyser, or a number.");
            default: return {};
        }
    }
    if (role != Qt::DisplayRole) { return {}; }
    switch (section)
    {
        case Flux: return tr("Flux to partition");
        case Water: return tr("Water channel");
        case Octants: return tr("Octants from");
        case Suffix: return tr("Columns end in");
        case Warning: return QString();
        default: return {};
    }
}

Qt::ItemFlags CecPairModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) { return Qt::NoItemFlags; }
    auto f = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    if (index.column() == Warning || index.column() == Suffix) { return f; }
    return f | Qt::ItemIsEditable;
}

int CecPairModel::addPair()
{
    const auto& gases = ecProject_->gasColumns();

    QVector<int> taken;
    for (const auto& row : rows_) { taken.append(row.fluxIndex); }

    //> Carbonyl sulfide first: the table already shows water and CO2 for
    //> every analyser by default, so the channel someone is most likely
    //> adding a row for is the one this table was extended for.
    QVector<int> order;
    for (const int i : MeasurementRecords::gasDisplayOrder(gases))
    {
        if (isSpecies(gases, i + 1, QStringLiteral("cos"))) { order.append(i + 1); }
    }
    for (const int i : MeasurementRecords::gasDisplayOrder(gases))
    {
        if (!order.contains(i + 1)) { order.append(i + 1); }
    }

    //> Fluxes not yet in the table before those that are, and each through
    //> the next combination there is room for: once every channel has a row,
    //> Add goes on offering second versions - COS by COS octants, say - until
    //> the engine's limits are reached.
    QVector<int> untakenFirst;
    for (const int flux : order)
    {
        if (!taken.contains(flux)) { untakenFirst.append(flux); }
    }
    for (const int flux : order)
    {
        if (taken.contains(flux)) { untakenFirst.append(flux); }
    }

    for (const int flux : untakenFirst)
    {
        CecFluxRow row;
        if (!MeasurementRecords::cecNextFreeVariant(
                gases, rows_, flux, MeasurementRecords::defaultCecWater(gases, flux),
                MeasurementRecords::defaultCecOctant(gases, flux), &row))
        {
            continue;
        }
        auto candidate = rows_;
        candidate.append(row);
        return apply(candidate) ? rowOf(row) : -1;
    }
    emit editRefused(tr("There is no room for another row: the engine runs at "
                        "most %1 combinations of octant gas and water, each "
                        "carrying at most %2 fluxes besides its own. Remove a "
                        "row, or give it the octant gas and water of another.")
                         .arg(MeasurementRecords::cecMaxPairs)
                         .arg(MeasurementRecords::cecMaxExtras));
    return -1;
}

int CecPairModel::duplicatePair(int row)
{
    if (row < 0 || row >= rows_.size()) { return -1; }
    const auto& gases = ecProject_->gasColumns();
    const auto from = rows_.at(row);

    //> The same flux, starting from this row's own water and octant gas: that
    //> combination is taken, so the search moves to the next octant gas -
    //> CO2 octants become COS octants - and only then to another water.
    CecFluxRow next;
    if (from.fluxIndex <= 0
        || !MeasurementRecords::cecNextFreeVariant(gases, rows_, from.fluxIndex,
                                                   from.waterIndex, from.octantIndex,
                                                   &next))
    {
        emit editRefused(tr("%1 cannot be split another way here: every "
                            "combination of octant gas and water it could take "
                            "is already in the table, or the engine's limits "
                            "are reached.").arg(fluxLabel(from)));
        return -1;
    }
    auto candidate = rows_;
    candidate.append(next);
    return apply(candidate) ? rowOf(next) : -1;
}

int CecPairModel::rowOf(const CecFluxRow& row) const
{
    return rowIn(rows_, row);
}

int CecPairModel::rowIn(const QVector<CecFluxRow>& rows, const CecFluxRow& row) const
{
    const auto& gases = ecProject_->gasColumns();
    const int water = MeasurementRecords::resolvedCecWater(gases, row.octantIndex,
                                                           row.waterIndex);
    for (int i = 0; i < rows.size(); ++i)
    {
        const auto& r = rows.at(i);
        if (r.octantIndex != row.octantIndex) { continue; }
        if (MeasurementRecords::resolvedCecWater(gases, r.octantIndex, r.waterIndex) != water)
        { continue; }
        //> The water row of its pairing reads as the water itself.
        if (r.fluxIndex == row.fluxIndex || (r.fluxIndex == water && row.fluxIndex == water))
        {
            return i;
        }
    }
    return -1;
}

void CecPairModel::removePair(int row)
{
    if (row < 0 || row >= rows_.size()) { return; }
    auto candidate = rows_;
    candidate.remove(row);
    apply(candidate);
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
    if (index.column() == CecPairModel::Warning
        || index.column() == CecPairModel::Suffix)
    {
        return nullptr;
    }

    auto combo = new QComboBox(parent);
    TableDelegateUtils::prepareComboEditor(combo, parent);

    if (index.column() == CecPairModel::Water)
    {
        //> Zero is a real choice, not a blank: it tells the engine to take
        //> the water on the octant gas's own analyser, which keeps following
        //> the site if the records are re-ordered.
        combo->addItem(tr("(same analyser)"), 0);
    }
    for (const auto& choice : model_->choices(index.column()))
    {
        combo->addItem(choice.second, choice.first);
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
    if (index.column() != CecPairModel::Warning
        && (index.flags() & Qt::ItemIsEditable))
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
    if (index.column() != CecPairModel::Warning
        && index.column() != CecPairModel::Suffix)
    {
        return TableDelegateUtils::comboCellSizeHint(option, base);
    }
    return base;
}
