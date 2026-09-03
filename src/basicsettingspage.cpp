/***************************************************************************
  basicsettingspage.cpp
  -------------------
  Copyright © 2007-2011, Eco2s team, Antonio Forgione
  Copyright © 2011-2018, LI-COR Biosciences, Antonio Forgione
  Copyright © 2026,      ETH Zurich, Jonathan Muller

  This file is part of EddyFlow®.

  EddyFlow (TM) is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version. You should have received a copy
  of the GNU General Public License along with EddyFlow (R). If not,
  see <http://www.gnu.org/licenses/>.

  EddyFlow® contains additional Open Source Components. The licenses
  and/or notices these Components can be found in the file LIBRARIES.txt.

  EddyFlow® is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.
****************************************************************************/

#include "basicsettingspage.h"

#include "measurement_record.h"
#include "table_delegate_utils.h"

#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QAbstractTableModel>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QLocale>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QNetworkReply>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QSpinBox>
#include <QtConcurrentRun>
#include <QTimeEdit>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVector>
#include <QIcon>
#include <QPixmap>
#include <QPointer>
#include <QSize>
#include <QToolButton>
#include <QHeaderView>
#include <QStyledItemDelegate>
#include <QTableView>

#include <cmath>

#include "gas_metadata.h"

#if defined(Q_OS_MACOS)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#endif

#if defined(Q_OS_MACOS)
#pragma clang diagnostic pop
#endif

#include "QProgressIndicator.h"

#include "advancedsettingspage.h"
#include "advprocessingoptions.h"
#include "advsettingscontainer.h"
#include "biommetadatareader.h"
#include "clicklabel.h"
#include "configstate.h"
#include "customclearlineedit.h"
#include "defs.h"
#include "dirbrowsewidget.h"
#include "dlproject.h"
#include "ecproject.h"
#include "fileformatwidget.h"
#include "globalsettings.h"
#include "infomessage.h"
#include "process.h"
#include "rawfilenamedialog.h"
#include "smartfluxbar.h"
#include "splitter.h"
#include "widget_utils.h"
#include "windfilter_tablemodel.h"
#include "windfilter_tableview.h"
#include "windfilter_view.h"

// for the qobject_cast in handleCrossWindAndAngleOfAttackUpdate()
#include "mainwidget.h"

const QString BasicSettingsPage::FLAG_POLICY_STRING_0 = QObject::tr("Above threshold");
const QString BasicSettingsPage::FLAG_POLICY_STRING_1 = QObject::tr("Below threshold");

namespace {

enum class VariableTableRowKind
{
    Gas,
    Cell,
    Ambient
};

struct VariableTableRow
{
    QComboBox* combo = nullptr;
    const char* updateSlot = nullptr;
    VariableTableRowKind kind = VariableTableRowKind::Cell;
    VariableTableRole role = VariableTableRole::Co2;
    QString tooltip;
    //> Species this row is pinned to, empty when the row takes whatever the
    //> site measured. The first three gas rows are pinned; the fourth is the
    //> open slot, which is why its molecular weight and diffusivity are
    //> editable and the others' are not.
    QString species;
};

struct VariableTableCandidate
{
    VariableTableRow row;
    int comboIndex = -1;
    int rawColumn = -1;
    QString variableText;
    QString sourceText;
    //> Species of this particular candidate, for a row that is not pinned.
    //> Resolved once when the rows are built: the candidate text is the only
    //> place it appears, and the row no longer owns a combo to re-read it
    //> from.
    QString candidateSpecies;
};

QString variableCandidateName(const QString& text, VariableTableRowKind kind)
{
    QString variableText = text;
    const QString fromToken = QObject::tr("from ");
    const int fromIndex = variableText.indexOf(fromToken, 0, Qt::CaseInsensitive);
    if (fromIndex >= 0)
    {
        variableText = variableText.left(fromIndex).trimmed();
    }

    if (kind == VariableTableRowKind::Gas)
    {
        return variableText.section(QLatin1Char(' '), 0, 0).trimmed();
    }
    return variableText.trimmed();
}

/// Whether a candidate is a placeholder rather than a measured column.
///
/// "None" and the 1000 sentinel. At file scope because the page asks it too:
/// the moisture dropdown offers candidates as well as records, and a
/// placeholder must not become a selectable H2O.
bool isNoneCandidateColumn(const VariableCandidateItem& item)
{
    if (item.rawColumn == 0 || item.rawColumn == 1000) { return true; }
    const QString text = item.text.trimmed();
    return text.isEmpty()
        || text.compare(QObject::tr("None"), Qt::CaseInsensitive) == 0;
}

QString variableCandidateSource(const QString& text)
{
    const QString fromToken = QObject::tr("from ");
    const int fromIndex = text.indexOf(fromToken, 0, Qt::CaseInsensitive);
    if (fromIndex < 0)
    {
        return QString();
    }
    QString sourceText = text.mid(fromIndex + fromToken.size()).trimmed();
    const QString rawPrefix = QObject::tr("raw data files: Column #");
    if (sourceText.startsWith(rawPrefix, Qt::CaseInsensitive))
    {
        return QObject::tr("raw data files");
    }
    return sourceText;
}

class BasicVariableSelectionModel final : public QAbstractTableModel
{
public:
    enum Column
    {
        Active = 0,
        Variable,
        Selection,
        //> Which H2O measurement corrects this gas. Blank and not editable on
        //> the H2O row itself and on anything that is not a gas.
        Moisture,
        MolecularWeight,
        Diffusivity,
        ColumnCount
    };

    BasicVariableSelectionModel(QObject* parent,
                                BasicSettingsPage* page,
                                const QVector<VariableTableRow>& rows,
                                bool molecularColumns,
                                const QString& selectionHeader,
                                QDoubleSpinBox* gasMw = nullptr,
                                QDoubleSpinBox* gasDiff = nullptr)
        : QAbstractTableModel(parent),
          page_(page),
          rows_(rows),
          molecularColumns_(molecularColumns),
          selectionHeader_(selectionHeader),
          gasMw_(gasMw),
          gasDiff_(gasDiff)
    {
        rebuildVisibleRows();
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : visibleRows_.size();
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override
    {
        if (parent.isValid()) { return 0; }
        return molecularColumns_ ? ColumnCount : Selection + 1;
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
    {
        if (role == Qt::ToolTipRole && orientation == Qt::Horizontal)
        {
            switch (section)
            {
                case Active: return tr("Check the rows to include in flux computation.");
                case Variable: return tr("Available variable candidates from the raw file description.");
                case Selection: return selectionHeader_;
                case Moisture: return tr("Water vapour measurement used to correct this gas. "
                                         "Defaults to the H2O from the same instrument.");
                case MolecularWeight: return tr("Molecular weight used for gas calculations.");
                case Diffusivity: return tr("Molecular diffusivity in air used for gas calculations.");
                default: return QVariant();
            }
        }
        if (role != Qt::DisplayRole) { return QVariant(); }
        if (orientation == Qt::Vertical) { return section + 1; }
        switch (section)
        {
            case Active: return tr("Active");
            case Variable: return tr("Variable");
            case Selection: return selectionHeader_;
            case Moisture: return tr("Moisture data");
            case MolecularWeight: return tr("Molecular weight");
            case Diffusivity: return tr("Molecular diffusivity in air");
            default: return QVariant();
        }
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= visibleRows_.size())
        {
            return QVariant();
        }

        const auto& row = rowAt(index.row());
        if (role == Qt::TextAlignmentRole)
        {
            return (index.column() == MolecularWeight || index.column() == Diffusivity)
                    ? static_cast<int>(Qt::AlignRight | Qt::AlignVCenter)
                    : static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
        }
        if (index.column() == Active && role == Qt::CheckStateRole)
        {
            return isActive(row) ? Qt::Checked : Qt::Unchecked;
        }
        //> Before the role filter below, which drops everything but display,
        //> edit and tooltip.
        //>
        //> On the Variable column, never on Moisture: paintComboCell fills
        //> that cell and draws a combo box from DisplayRole alone, so a
        //> decoration there is discarded and the mark would be dead code that
        //> reads as working. Variable falls through to the default painter,
        //> which honours it - and it is the gas the mark is about.
        if (role == Qt::DecorationRole)
        {
            if (index.column() == Variable && !crossAnalyserWater(row).isEmpty())
            {
                return crossAnalyserIcon();
            }
            //> The RH row of the ambient table, when a biomet humidity is
            //> standing in for what the hygrometers measured. Same column and
            //> the same reasoning as above: that table is built without the
            //> molecular columns, so Variable is present and painted by the
            //> default painter, which honours a decoration.
            if (index.column() == Variable && biometRhOverride(row))
            {
                return crossAnalyserIcon();
            }
            return QVariant();
        }
        if (role != Qt::DisplayRole && role != Qt::EditRole && role != Qt::ToolTipRole)
        {
            return QVariant();
        }

        switch (index.column())
        {
            case Active:
                return role == Qt::ToolTipRole ? row.row.tooltip : QVariant();
            case Variable:
                if (role == Qt::ToolTipRole
                    && row.row.kind == VariableTableRowKind::Gas)
                {
                    QString tooltip = row.row.tooltip;
                    tooltip += QStringLiteral("\n\n");
                    tooltip += tr("A gas may be measured more than once. Each column becomes its own record, and its columns are numbered - h2o_1, h2o_2 - so the two never share a name.");
                    //> What the triangle beside this row means, in the one
                    //> place a user is likely to look for it.
                    const auto waterInstrument = crossAnalyserWater(row);
                    if (!waterInstrument.isEmpty())
                    {
                        tooltip += QStringLiteral("\n\n");
                        tooltip += tr("This gas is corrected with the H2O measured by %1, "
                                      "a different analyser. The water vapour flux term and "
                                      "the mixing ratio conversion both use that analyser's "
                                      "cell, so both are approximations.")
                                       .arg(waterInstrument);
                    }
                    return tooltip;
                }
                if (role == Qt::ToolTipRole)
                {
                    //> What the triangle on the RH row means. Same placement
                    //> rule as the gas one above: beside the mark, where
                    //> someone puzzled by it will point.
                    if (biometRhOverride(row))
                    {
                        QString tooltip = row.row.tooltip;
                        if (!tooltip.isEmpty()) { tooltip += QStringLiteral("\n\n"); }
                        tooltip += tr("This biomet humidity replaces what every "
                                      "hygrometer measured: their mole fraction, "
                                      "mixing ratio and molar density are reported "
                                      "from it, and every gas is WPL-corrected with "
                                      "it. The fluxes themselves are unaffected.");
                        return tooltip;
                    }
                    return row.row.tooltip;
                }
                return row.variableText;
            case Selection:
                if (role == Qt::ToolTipRole)
                {
                    return row.row.tooltip;
                }
                return row.sourceText.isEmpty() ? tr("raw data files") : row.sourceText;
            case Moisture:
                if (role == Qt::ToolTipRole)
                {
                    return moistureAvailable(row)
                            ? tr("Water vapour measurement used to correct this gas.")
                            : QVariant();
                }
                return moistureText(row);
            case MolecularWeight:
                if (role == Qt::ToolTipRole)
                {
                    return row.row.tooltip;
                }
                return molecularText(row, true);
            case Diffusivity:
                if (role == Qt::ToolTipRole)
                {
                    return row.row.tooltip;
                }
                return molecularText(row, false);
            default:
                return QVariant();
        }
    }

    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= visibleRows_.size())
        {
            return false;
        }

        // By value, not by reference: rebuildVisibleRows() clears the vector
        // this would otherwise point into. It survived only because clear()
        // keeps its capacity and the rebuild happened to reproduce the same
        // rows - which stops being true as soon as selection changes them.
        const auto row = rowAt(index.row());
        if (index.column() == Active && role == Qt::CheckStateRole)
        {
            const bool checked = value.toInt() == Qt::Checked;
            const auto slug = gasSlug(row);

            if (!slug.isEmpty() && page_)
            {
                if (checked)
                {
                    // Blocked here rather than at save: the engine reads only
                    // the first N records and drops the rest silently, so a
                    // project that exceeds the limit would process without
                    // the gases the user thought they had selected.
                    const auto blocked = page_->gasLimitBlockReason(row.rawColumn);
                    if (!blocked.isEmpty())
                    {
                        WidgetUtils::warning(QApplication::activeWindow(),
                            tr("Gas limit reached"), blocked);
                        return false;
                    }
                    page_->addGasRecord(slug, row.rawColumn);
                }
                else { page_->removeGasRecord(slug, row.rawColumn); }

                // Keep the hidden combo on the first measurement of this
                // species: the legacy col_* fields and everything downstream
                // still read it, and they can only express one.
                const int firstCol = page_->firstGasColumn(slug);
                auto& mutableRow = mutableRowAt(index.row());
                const int comboIndex = firstCol > 0
                        ? comboIndexForColumn(mutableRow, firstCol)
                        : noneIndex(mutableRow);
                if (comboIndex >= 0) { applyComboIndex(mutableRow, comboIndex); }

                beginResetModel();
                rebuildVisibleRows();
                endResetModel();
                return true;
            }

            //> Cell temperatures, cell pressures and diagnostics are records
            //> too. They used to be written only as col_int_t_1 and friends,
            //> which are retired - without this the selection would be lost
            //> on save.
            const auto nonGasSlug = BasicSettingsPage::nonGasSlugForRole(
                static_cast<int>(row.row.role));
            if (!nonGasSlug.isEmpty() && page_)
            {
                if (checked) { page_->addNonGasRecord(nonGasSlug, row.rawColumn); }
                else { page_->removeNonGasRecord(nonGasSlug, row.rawColumn); }
            }

            auto& mutableRow = mutableRowAt(index.row());
            const int comboIndex = checked ? mutableRow.comboIndex : noneIndex(mutableRow);
            if (comboIndex < 0)
            {
                return false;
            }
            applyComboIndex(mutableRow, comboIndex);
            beginResetModel();
            rebuildVisibleRows();
            endResetModel();
            return true;
        }

        if (role != Qt::EditRole)
        {
            return false;
        }

        if (index.column() == Moisture && moistureAvailable(row) && page_)
        {
            //> The value is a raw column, not a record index: the dropdown
            //> offers H2O columns the project has not activated, and those
            //> have no index until they are.
            const int gasIdx = gasRecordIndex(row);
            const bool recordsChanged =
                page_->setMoistureColumnForGas(gasIdx, value.toInt());
            if (!recordsChanged)
            {
                emit dataChanged(index, index, { Qt::DisplayRole, Qt::EditRole });
            }

            //> Everything after the edit waits for the next turn of the event
            //> loop, because this runs inside the delegate's setModelData -
            //> which QAbstractItemView calls from commitData(), with the combo
            //> editor still open and still in the view's editor map.
            //>
            //> Resetting there makes the view release its editors from within
            //> the call that is committing one. The data was right immediately
            //> either way - isActive() reads the records live - but the
            //> pending relayout was dropped, so a column switched on by this
            //> selection only appeared ticked at the next external repaint,
            //> such as minimising the window and coming back. The Active
            //> column gets away with the same reset because a click on the
            //> check indicator has no editor open.
            //>
            //> One queued call, not two, so the order is fixed: the table
            //> redraws first, and the dialog then appears over a table that
            //> already tells the truth. Raised from inside commitData it would
            //> spin a nested event loop in that same place.
            //>
            //> gasIdx survives the reset: addGasRecord appends, so no existing
            //> record moves.
            QPointer<BasicVariableSelectionModel> model(this);
            QPointer<BasicSettingsPage> page(page_);
            QTimer::singleShot(0, this, [model, page, gasIdx, recordsChanged]()
            {
                if (model && recordsChanged) { model->refresh(); }
                if (page) { page->warnOnCrossAnalyserMoisture(gasIdx); }
            });
            return true;
        }

        //> Straight onto the record, for any active gas. Both used to set one
        //> of two shared spin boxes, whose value went to the project-wide
        //> gas_mw / gas_diff - keys the writer deletes - so the edit was lost
        //> on save and the engine used its own default instead.
        if (isActive(row) && page_
            && (index.column() == MolecularWeight || index.column() == Diffusivity))
        {
            if (index.column() == MolecularWeight)
            {
                page_->setGasMolecularWeight(gasRecordIndex(row), value.toDouble());
            }
            else
            {
                page_->setGasDiffusivity(gasRecordIndex(row), value.toDouble());
            }
            emit dataChanged(index, index, { Qt::DisplayRole, Qt::EditRole });
            return true;
        }

        return false;
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        if (!index.isValid()) { return Qt::NoItemFlags; }
        Qt::ItemFlags itemFlags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
        const auto& row = rowAt(index.row());
        if (index.column() == Active)
        {
            // A row that cannot be checked should look that way, rather than
            // taking a click and refusing it.
            const auto slug = gasSlug(row);
            if (!slug.isEmpty() && page_ && !isActive(row)
                && !page_->gasLimitBlockReason(row.rawColumn).isEmpty())
            {
                return Qt::ItemIsSelectable;
            }
            return itemFlags | Qt::ItemIsUserCheckable;
        }
        if (index.column() == Moisture && moistureAvailable(row))
        {
            itemFlags |= Qt::ItemIsEditable;
        }
        //> Editable on any active gas, not only the open slot: a species the
        //> tables carry no diffusivity for needs its own value whichever row
        //> it sits in, and two such gases need two different ones.
        if (isActive(row)
            && (index.column() == MolecularWeight || index.column() == Diffusivity))
        {
            itemFlags |= Qt::ItemIsEditable;
        }
        return itemFlags;
    }

    const VariableTableCandidate& rowAt(int row) const
    {
        return visibleRows_.at(row);
    }

    void refresh()
    {
        beginResetModel();
        rebuildVisibleRows();
        endResetModel();
    }

private:
    VariableTableCandidate& mutableRowAt(int row)
    {
        return visibleRows_[row];
    }

    //> "None" and the 1000 sentinel are placeholders, not measurements.
    static bool isNoneCandidate(const VariableCandidateItem& item)
    {
        return isNoneCandidateColumn(item);
    }

    int noneIndex(const VariableTableCandidate& row) const
    {
        const auto items = candidatesFor(row.row);
        for (int i = 0; i < items.size(); ++i)
        {
            if (isNoneCandidate(items.at(i))) { return i; }
        }
        return -1;
    }

    //> The candidate list for a row. Ambient roles still keep theirs in a
    //> combo, because col_air_t, col_air_p and the biomet columns are live
    //> keys; every other role reads the page's list.
    QVector<VariableCandidateItem> candidatesFor(const VariableTableRow& row) const
    {
        if (row.combo)
        {
            QVector<VariableCandidateItem> items;
            items.reserve(row.combo->count());
            for (int i = 0; i < row.combo->count(); ++i)
            {
                items.append({ row.combo->itemData(i).toInt(),
                               row.combo->itemText(i) });
            }
            return items;
        }
        return page_ ? page_->candidatesForRole(static_cast<int>(row.role))
                     : QVector<VariableCandidateItem>{};
    }

    //> Index into the project's gas record list for this row, or -1.
    //>
    //> Looked up by what the row measures, not by where the row sits. The four
    //> species rows used to carry a fixed index each, which worked only while
    //> the record list reserved a position for every one of them - including
    //> the gases the site does not have. It no longer does, so a row that
    //> named its index would edit whichever gas had moved into it.
    int gasRecordIndex(const VariableTableCandidate& row) const
    {
        const auto slug = gasSlug(row);
        if (slug.isEmpty() || !page_) { return -1; }
        return page_->gasRecordIndexFor(slug, row.rawColumn);
    }

    //> Whether this row gets a moisture choice at all.
    //>
    //> Blank and not editable on the H2O row - water is not corrected with
    //> itself - on inactive gases, and on everything that is not a gas.
    bool moistureAvailable(const VariableTableCandidate& row) const
    {
        if (!molecularColumns_) { return false; }
        if (gasSlug(row) == QLatin1String("h2o")) { return false; }
        if (gasRecordIndex(row) < 0) { return false; }
        if (!isActive(row)) { return false; }
        return !page_ || page_->hasMoistureCandidates();
    }

    //> Analyser of the hygrometer correcting this row's gas, when it is not
    //> the one measuring it. Empty for every other row, which is what decides
    //> whether the row is marked.
    QString crossAnalyserWater(const VariableTableCandidate& row) const
    {
        if (!page_ || !isActive(row)) { return QString(); }
        const int idx = gasRecordIndex(row);
        if (idx < 0) { return QString(); }
        return page_->crossAnalyserWaterInstrument(idx);
    }

    //> Whether this row is the RH row while a biomet humidity is standing in
    //> for the hygrometers. The condition itself belongs to the page, shared
    //> with the dialog and the tooltip; only "is this the RH row" is local.
    bool biometRhOverride(const VariableTableCandidate& row) const
    {
        if (!page_) { return false; }
        if (row.row.role != VariableTableRole::Rh) { return false; }
        return page_->biometRhOverrideActive();
    }

    //> The mark itself, built once.
    //>
    //> data() is called for every visible cell on every repaint, so scaling a
    //> pixmap in it would do that work continuously. Twelve pixels and the
    //> mac device-pixel-ratio branch follow AdvOutputOptions::setRequiredIcon,
    //> which is the existing inline warning in this interface.
    static QIcon crossAnalyserIcon()
    {
        static QIcon icon = []
        {
            QPixmap pixmap(QStringLiteral(":/icons/msg-warning"));
#if defined(Q_OS_MACOS)
            pixmap.setDevicePixelRatio(2.0);
#endif
            return QIcon(pixmap.scaled(QSize(12, 12),
                                       Qt::KeepAspectRatio,
                                       Qt::SmoothTransformation));
        }();
        return icon;
    }

    //> Label of the H2O currently correcting this gas.
    QVariant moistureText(const VariableTableCandidate& row) const
    {
        if (!moistureAvailable(row)) { return QVariant(); }
        if (!page_) { return QVariant(); }
        return page_->moistureLabelForGas(gasRecordIndex(row));
    }

    //> Engine slug for a gas row, empty for anything that is not a gas.
    //>
    //> A pinned row answers with its own species; the open slot answers with
    //> whatever the site measured, resolved when the rows were built.
    QString gasSlug(const VariableTableCandidate& row) const
    {
        if (row.row.kind != VariableTableRowKind::Gas) { return QString(); }
        return row.row.species.isEmpty() ? row.candidateSpecies : row.row.species;
    }

    //> Whether this row takes its species from the data rather than being
    //> pinned to one. Only such a row gets editable molecular weight and
    //> diffusivity, and only it can raise a diffusivity-provenance warning.
    static bool speciesIsOpen(const VariableTableCandidate& row)
    {
        return row.row.kind == VariableTableRowKind::Gas
               && row.row.species.isEmpty();
    }

    //> Whether this candidate is selected.
    //>
    //> Gases answer from the project's records, which is what allows more than
    //> one measurement of a species; everything else still answers from its
    //> combo, since those roles remain single-valued.
    bool isActive(const VariableTableCandidate& row) const
    {
        const auto slug = gasSlug(row);
        if (!slug.isEmpty() && page_)
        {
            return page_->gasRecordExists(slug, row.rawColumn);
        }
        //> Cell and diagnostic rows answer from their records for the same
        //> reason gas rows do: the column they used to be written to is
        //> retired, and a record can name the analyser as well as the column.
        const auto nonGasSlug = BasicSettingsPage::nonGasSlugForRole(
            static_cast<int>(row.row.role));
        if (!nonGasSlug.isEmpty() && page_)
        {
            return page_->nonGasRecordExists(nonGasSlug, row.rawColumn);
        }
        return isComboActive(row);
    }

    //> Only the ambient rows still answer from a combo; everything else is
    //> record-driven and never reaches here.
    bool isComboActive(const VariableTableCandidate& row) const
    {
        if (!row.row.combo) { return false; }
        const auto items = candidatesFor(row.row);
        return row.row.combo->currentIndex() >= 0
                && row.row.combo->currentIndex() == row.comboIndex
                && row.comboIndex >= 0 && row.comboIndex < items.size()
                && !isNoneCandidate(items.at(row.comboIndex));
    }

    int comboIndexForColumn(const VariableTableCandidate& row, int rawColumn) const
    {
        const auto items = candidatesFor(row.row);
        for (int i = 0; i < items.size(); ++i)
        {
            if (items.at(i).rawColumn == rawColumn) { return i; }
        }
        return -1;
    }

    void applyComboIndex(VariableTableCandidate& row, int comboIndex)
    {
        //> A flux row has no combo: its selection lives in the record that
        //> setData has already written.
        if (row.row.combo) { row.row.combo->setCurrentIndex(comboIndex); }
        if (row.row.updateSlot)
        {
            QMetaObject::invokeMethod(page_, row.row.updateSlot, Qt::DirectConnection, Q_ARG(int, comboIndex));
        }
        //> Both of these are species questions, not slot questions: they fire
        //> for whichever row takes its species from the data.
        if (speciesIsOpen(row))
        {
            const QString species = gasSlug(row);
            QMetaObject::invokeMethod(page_, "showGasDiffusivityWarning",
                                      Qt::DirectConnection, Q_ARG(QString, species));
            QMetaObject::invokeMethod(page_, "applyGasAbsoluteLimitMin",
                                      Qt::DirectConnection,
                                      Q_ARG(int, gasRecordIndex(row)),
                                      Q_ARG(QString, species));
        }
    }

    void rebuildVisibleRows()
    {
        visibleRows_.clear();
        for (const auto& row : rows_)
        {
            const auto items = candidatesFor(row);
            for (int i = 0; i < items.size(); ++i)
            {
                if (isNoneCandidate(items.at(i))) { continue; }

                VariableTableCandidate candidate;
                candidate.row = row;
                candidate.comboIndex = i;
                candidate.rawColumn = items.at(i).rawColumn;
                const QString name = variableCandidateName(items.at(i).text, row.kind);
                candidate.variableText = name + tr(" (col %1)").arg(candidate.rawColumn);
                candidate.sourceText = variableCandidateSource(items.at(i).text);
                if (row.kind == VariableTableRowKind::Gas && row.species.isEmpty())
                {
                    candidate.candidateSpecies = GasMetadata::normaliseFormula(name);
                }
                visibleRows_.append(candidate);
            }
        }
    }

    QVariant molecularText(const VariableTableCandidate& row, bool weight) const
    {
        if (!molecularColumns_) { return QVariant(); }
        if (row.row.kind != VariableTableRowKind::Gas) { return QVariant(); }

        //> An active gas shows its record's value, which is what the file
        //> carries and what the engine reads. It used to show the two shared
        //> spin boxes for the open slot and a species constant otherwise -
        //> so a second non-standard gas could not have its own constants at
        //> all, and what the spin boxes held was discarded on save.
        if (isActive(row) && page_)
        {
            const int idx = gasRecordIndex(row);
            const qreal v = weight ? page_->gasMolecularWeight(idx)
                                   : page_->gasDiffusivity(idx);
            if (v > 0.0) { return QString::number(v, 'f', weight ? 4 : 5); }
        }

        //> An inactive row has no record yet, so it previews the species
        //> constant it would be given.
        const GasMetadata::GasEntry* gas = GasMetadata::findSpecies(gasSlug(row));
        if (!gas) { return QVariant(); }
        const qreal v = weight ? gas->molecularWeight : gas->diffusivity;
        if (v <= 0.0) { return QVariant(); }
        return QString::number(v, 'f', weight ? 4 : 5);
    }

    BasicSettingsPage* page_;
    QVector<VariableTableRow> rows_;
    QVector<VariableTableCandidate> visibleRows_;
    bool molecularColumns_;
    QString selectionHeader_;
    QDoubleSpinBox* gasMw_;
    QDoubleSpinBox* gasDiff_;
};

class BasicVariableSelectionDelegate final : public QStyledItemDelegate
{
public:
    explicit BasicVariableSelectionDelegate(QObject* parent = nullptr,
                                            BasicSettingsPage* page = nullptr)
        : QStyledItemDelegate(parent), page_(page)
    {}

    QWidget* createEditor(QWidget* parent,
                          const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override
    {
        Q_UNUSED(option)
        const auto model = dynamic_cast<const BasicVariableSelectionModel*>(index.model());
        if (!model) { return nullptr; }

        if (index.column() == BasicVariableSelectionModel::Moisture)
        {
            if (!page_) { return nullptr; }
            auto editor = new QComboBox(parent);
            for (const auto& choice : page_->moistureChoices())
            {
                //> The raw column travels as the item's data, never the label.
                //> Not the record index: the list offers H2O columns the
                //> project has not activated, and those have no record to
                //> index until the selection creates one.
                editor->addItem(choice.second, choice.first);
            }
            TableDelegateUtils::prepareComboEditor(editor, parent);
            //> Commit as soon as the user picks, rather than whenever the view
            //> next decides editing has ended.
            //>
            //> A QComboBox editor with no such connection commits on focus-out
            //> alone. Choosing from the popup closed the popup and updated the
            //> combo, and nothing reached the model - so the H2O column this
            //> selection switches on stayed unticked until the user clicked
            //> away or, as reported, minimised the window and came back. The
            //> value did save, eventually, which is what made it look like a
            //> repaint problem rather than a commit that had not happened.
            //>
            //> The three sibling delegates - variable, irga and anem - all
            //> connect activated to the same commitData/closeEditor pair. This
            //> is that pattern; a lambda rather than a slot because this class
            //> is declared in the .cpp with no Q_OBJECT.
            //>
            //> Cast away the constness createEditor is declared with, because
            //> the two signals are non-const members. The siblings avoid it
            //> only by naming a slot, which needs the moc this class does not
            //> have; the emit happens later, from the event loop, when nothing
            //> is treating the delegate as const.
            auto* self = const_cast<BasicVariableSelectionDelegate*>(this);
            connect(editor, QOverload<int>::of(&QComboBox::activated), self,
                    [self, editor](int)
                    {
                        emit self->commitData(editor);
                        emit self->closeEditor(editor,
                                               QAbstractItemDelegate::NoHint);
                    });
            TableDelegateUtils::showPopupQueued(editor);
            return editor;
        }

        if (index.column() == BasicVariableSelectionModel::MolecularWeight
            || index.column() == BasicVariableSelectionModel::Diffusivity)
        {
            auto editor = new QDoubleSpinBox(parent);
            editor->setDecimals(index.column() == BasicVariableSelectionModel::MolecularWeight ? 4 : 5);
            editor->setRange(0.0, index.column() == BasicVariableSelectionModel::MolecularWeight ? 1000.0 : 1.0);
            editor->setSingleStep(index.column() == BasicVariableSelectionModel::MolecularWeight ? 1.0 : 0.1);
            return editor;
        }

        return nullptr;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override
    {
        if (auto combo = qobject_cast<QComboBox*>(editor))
        {
            // Preselect what the row currently resolves to, so opening the
            // list and closing it again cannot change anything.
            const auto text = index.data(Qt::DisplayRole).toString();
            const int at = combo->findText(text);
            if (at >= 0) { combo->setCurrentIndex(at); }
            return;
        }
        if (auto spin = qobject_cast<QDoubleSpinBox*>(editor))
        {
            spin->setValue(index.data(Qt::EditRole).toDouble());
        }
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override
    {
        if (auto combo = qobject_cast<QComboBox*>(editor))
        {
            model->setData(index, combo->currentData().toInt(), Qt::EditRole);
            return;
        }
        if (auto spin = qobject_cast<QDoubleSpinBox*>(editor))
        {
            model->setData(index, spin->value(), Qt::EditRole);
        }
    }

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        if (index.column() == BasicVariableSelectionModel::Moisture
            && (index.flags() & Qt::ItemIsEditable))
        {
            TableDelegateUtils::paintComboCell(painter, option, index);
            return;
        }
        QStyledItemDelegate::paint(painter, option, index);
    }

    //> Wide enough for the label and the arrow drawn over it. Without this the
    //> column sizes to the text alone and elides the instrument, which is the
    //> part that tells one analyser's water from another's.
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override
    {
        const QSize base = QStyledItemDelegate::sizeHint(option, index);
        if (index.column() == BasicVariableSelectionModel::Moisture
            && (index.flags() & Qt::ItemIsEditable))
        {
            return TableDelegateUtils::comboCellSizeHint(option, base);
        }
        return base;
    }

private:
    BasicSettingsPage* page_ = nullptr;
};

void configureBasicVariablesTable(QTableView* table)
{
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::DoubleClicked
                           | QAbstractItemView::SelectedClicked
                           | QAbstractItemView::EditKeyPressed);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(BasicVariableSelectionModel::Active, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(BasicVariableSelectionModel::Variable, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(BasicVariableSelectionModel::Selection, QHeaderView::Stretch);
    if (table->model() && table->model()->columnCount() > BasicVariableSelectionModel::MolecularWeight)
    {
        // Guarded with the molecular columns: the ambient table stops at
        // Selection, so these sections do not exist there.
        table->horizontalHeader()->setSectionResizeMode(BasicVariableSelectionModel::Moisture, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(BasicVariableSelectionModel::MolecularWeight, QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(BasicVariableSelectionModel::Diffusivity, QHeaderView::ResizeToContents);
    }
    table->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
}

} // namespace



namespace {
const QString kH2oSlug = QStringLiteral("h2o");

//> There is deliberately no table of reserved record positions here any more.
//> CO2, H2O and CH4 used to own the first three, so that a record's index was
//> the engine's gas slot - which meant carrying an empty record for every gas
//> the site did not measure. Records name their own species, so a position
//> means nothing and the list holds only what is measured. EddyPro's fixed
//> four-slot layout still exists, but it is rebuilt where it is needed, in
//> EcProject::writeEddyProCompatibleKeys().
} // namespace


/// Species of the gas in the open slot, as a slug.
///
/// Read from the record rather than from a combo's current text, which is
/// what this used to be: the fourth-gas combo is gone with the rest, and the
/// record is where the species has lived since Phase 5. GasMetadata::findGas
/// normalises both sides, so a slug matches a display formula.
///
/// Named for the slot's role rather than its position, but still that
/// position: migration pins CO2, H2O and CH4 to records 0..2, so the fourth is
/// the open one. Deliberately does *not* gate on the record having a column -
/// a gas can be named before its column is chosen, and the species is what the
/// absolute-limit floor and the diffusivity warning key on.
QString BasicSettingsPage::openGasSpecies() const
{
    if (!ecProject_) { return QString(); }
    const int slot = openGasRecordIndex();
    if (slot < 0) { return QString(); }
    return ecProject_->gasColumns().at(slot).slug;
}

/// Canonical instrument id of the analyser measuring \a rawColumn.
///
/// Read from the metadata rather than parsed out of the table's display text:
/// the label is translated and formatted for reading, while the id is what
/// both the project file and the same-analyser rule match on.
QString BasicSettingsPage::canonicalInstrumentForColumn(int rawColumn) const
{
    if (!dlProject_ || rawColumn <= 0) { return QString(); }
    const auto vars = dlProject_->variables();
    if (!vars || rawColumn > vars->size()) { return QString(); }
    const auto instrument = vars->at(rawColumn - 1).instrument();
    if (instrument.isEmpty()) { return MeasurementRecords::noneInstrument(); }
    return dlProject_->canonicalInstrumentId(instrument);
}

bool BasicSettingsPage::gasRecordExists(const QString& slug, int rawColumn) const
{
    if (!ecProject_) { return false; }
    for (const auto& rec : ecProject_->gasColumns())
    {
        if (rec.slug == slug && rec.rawColumn == rawColumn) { return true; }
    }
    return false;
}

int BasicSettingsPage::firstGasColumn(const QString& slug) const
{
    if (!ecProject_) { return -1; }
    for (const auto& rec : ecProject_->gasColumns())
    {
        if (rec.slug == slug && rec.rawColumn > 0) { return rec.rawColumn; }
    }
    return -1;
}

/// Why a gas measured on \a rawColumn cannot be added, empty if it can.
///
/// Checked before the record is created rather than after: the engine reads
/// only the first MaxNumGases records and the first MaxGasesPerInstrument per
/// instrument, and drops the rest without saying so.
QString BasicSettingsPage::gasLimitBlockReason(int rawColumn) const
{
    if (!ecProject_) { return QString(); }
    const auto& gases = ecProject_->gasColumns();

    int configured = 0;
    for (const auto& rec : gases)
    {
        if (rec.rawColumn > 0) { ++configured; }
    }
    if (configured >= Defs::MAX_GASES)
    {
        return tr("This project already uses the maximum of %1 gas "
                  "measurements.").arg(Defs::MAX_GASES);
    }

    const auto instrument = canonicalInstrumentForColumn(rawColumn);
    if (!MeasurementRecords::isRealInstrument(instrument))
    {
        return QString();
    }
    int onInstrument = 0;
    for (const auto& rec : gases)
    {
        if (rec.rawColumn > 0 && rec.instrumentId == instrument)
        {
            ++onInstrument;
        }
    }
    if (onInstrument >= Defs::MAX_GASES_PER_INSTRUMENT)
    {
        return tr("%1 already provides the maximum of %2 gas measurements. "
                  "Deselect one of its gases to add another.")
                .arg(instrument).arg(Defs::MAX_GASES_PER_INSTRUMENT);
    }
    return QString();
}

/// Add a measurement of \a slug at \a rawColumn.
///
/// The first four record positions are the historical slots and stay put even
/// when empty, because the engine maps record i to gas slot firstGas+i-1;
/// reordering them would move each gas's settings onto a different species.
/// Additional measurements are appended after those four.
void BasicSettingsPage::addGasRecord(const QString& slug, int rawColumn)
{
    if (!ecProject_ || slug.isEmpty() || rawColumn <= 0) { return; }
    if (gasRecordExists(slug, rawColumn)) { return; }
    if (!gasLimitBlockReason(rawColumn).isEmpty()) { return; }

    //> Appended, whatever the species. CO2, H2O and CH4 used to be filled into
    //> reserved positions so that record order stayed the engine's slot order,
    //> and a gas the site did not measure held its place with an empty record.
    //> Nothing reads a species from a position any more - the record says what
    //> it is - so there is no slot to reserve and no reason to keep a record
    //> for a gas that is not there.
    auto gases = ecProject_->gasColumns();
    GasRecord rec;
    rec.slug = slug;
    rec.rawColumn = rawColumn;
    rec.instrumentId = canonicalInstrumentForColumn(rawColumn);
    //> Seeded with this species' processing settings rather than left at the
    //> -1 sentinel. An unset setting is written as no key at all, and the
    //> engine reads an absent al_min/al_max as "not configured" and declines
    //> the absolute-limits test - silently, with a 9 in a packed flag string
    //> as the only trace. The spike limit, the discontinuity limits and both
    //> time-lag windows go the same way.
    //>
    //> migrateLegacyGasSettings fills these once, on a legacy upgrade, so
    //> until now only the records that survived that upgrade had any. Moving
    //> a gas to a different column creates a *new* record, which is how a
    //> project ended up carrying a full settings block for one gas and none
    //> for the others.
    rec.proc = ecProject_->defaultGasProcessing(slug);
    gases.append(rec);
    ecProject_->setGasColumns(gases);
}

/// Slug for a non-gas row, or empty when the row is not one.
///
/// Cell temperatures and pressures and the instrument diagnostics are
/// records too. They used to be written only as col_int_t_1 and friends,
/// which could name one column each and said nothing about which analyser
/// it came from - the same limitation the gas records were introduced to
/// remove, and it applies just as much to a site with two closed-path
/// analysers, each with its own cell.
QString BasicSettingsPage::nonGasSlugForRole(int role)
{
    switch (static_cast<VariableTableRole>(role))
    {
        case VariableTableRole::IntTc: return QStringLiteral("cell_t");
        case VariableTableRole::IntT1: return QStringLiteral("int_t_1");
        case VariableTableRole::IntT2: return QStringLiteral("int_t_2");
        case VariableTableRole::IntP: return QStringLiteral("int_p");
        case VariableTableRole::Diag7500: return QStringLiteral("diag_75");
        case VariableTableRole::Diag7200: return QStringLiteral("diag_72");
        case VariableTableRole::Diag7700: return QStringLiteral("diag_77");
        default: return QString();
    }
}

/// Whether \a slug names a diagnostic rather than a cell measurement.
static bool isDiagnosticSlug(const QString& slug)
{
    return slug.startsWith(QLatin1String("diag_"));
}

void BasicSettingsPage::addNonGasRecord(const QString& slug, int rawColumn)
{
    if (!ecProject_ || slug.isEmpty() || rawColumn <= 0) { return; }

    auto records = isDiagnosticSlug(slug) ? ecProject_->diagColumns()
                                          : ecProject_->cellColumns();
    for (const auto& rec : records)
    {
        if (rec.slug == slug && rec.rawColumn == rawColumn) { return; }
    }

    MeasurementRecord rec;
    rec.slug = slug;
    rec.rawColumn = rawColumn;
    rec.instrumentId = canonicalInstrumentForColumn(rawColumn);
    records.append(rec);

    if (isDiagnosticSlug(slug)) { ecProject_->setDiagColumns(records); }
    else { ecProject_->setCellColumns(records); }
}

void BasicSettingsPage::removeNonGasRecord(const QString& slug, int rawColumn)
{
    if (!ecProject_ || slug.isEmpty()) { return; }

    const bool isDiag = isDiagnosticSlug(slug);
    auto records = isDiag ? ecProject_->diagColumns() : ecProject_->cellColumns();
    auto gases = ecProject_->gasColumns();
    bool gasesChanged = false;

    for (int i = records.size() - 1; i >= 0; --i)
    {
        if (records.at(i).slug != slug || records.at(i).rawColumn != rawColumn)
        {
            continue;
        }
        records.removeAt(i);

        //> The same shift the gas records take: cellRef is a 1-based index
        //> into the cell list, so removing one moves every later cell block
        //> under the gases that named it. Only the cell list is referenced -
        //> nothing points at a diagnostic - but the loop is written once and
        //> guarded rather than duplicated.
        if (isDiag) { continue; }
        for (auto& gas : gases)
        {
            if (gas.cellRef == i + 1) { gas.cellRef = 0; }
            else if (gas.cellRef > i + 1) { --gas.cellRef; }
            else { continue; }
            gasesChanged = true;
        }
    }

    if (isDiag) { ecProject_->setDiagColumns(records); }
    else { ecProject_->setCellColumns(records); }
    if (gasesChanged) { ecProject_->setGasColumns(gases); }
}

/// Position of the record for \a slug at \a rawColumn, or -1.
///
/// The variable table's rows resolve their record through this. A row knows
/// what it measures and which column it came from, which together name exactly
/// one record - the same pair gasRecordExists() matches on.
int BasicSettingsPage::gasRecordIndexFor(const QString& slug,
                                         int rawColumn) const
{
    if (!ecProject_ || slug.isEmpty()) { return -1; }
    const auto& gases = ecProject_->gasColumns();
    for (int i = 0; i < gases.size(); ++i)
    {
        if (gases.at(i).slug == slug && gases.at(i).rawColumn == rawColumn)
        {
            return i;
        }
    }
    return -1;
}

/// Whether a non-gas record for \a slug at \a rawColumn exists.
bool BasicSettingsPage::nonGasRecordExists(const QString& slug,
                                           int rawColumn) const
{
    if (!ecProject_ || slug.isEmpty()) { return false; }
    const auto& records = isDiagnosticSlug(slug) ? ecProject_->diagColumns()
                                                 : ecProject_->cellColumns();
    for (const auto& rec : records)
    {
        if (rec.slug == slug && rec.rawColumn == rawColumn) { return true; }
    }
    return false;
}

void BasicSettingsPage::removeGasRecord(const QString& slug, int rawColumn)
{
    if (!ecProject_) { return; }
    auto gases = ecProject_->gasColumns();
    for (int i = 0; i < gases.size(); ++i)
    {
        if (gases.at(i).slug != slug || gases.at(i).rawColumn != rawColumn)
        {
            continue;
        }
        //> Erased, whichever position it held. CO2, H2O and CH4 used to be
        //> emptied in place instead, so that the records after them kept their
        //> index - which is also how a project ended up naming a gas it does
        //> not measure and shipping a column of error codes for it.
        gases.remove(i);

        //> Everything that pointed past the hole moves down with it. These are
        //> 1-based indices into this same list, and validateReferences() below
        //> only clears the ones that now point nowhere - it cannot tell that a
        //> still-valid index means a different gas than it did a moment ago.
        for (auto& gas : gases)
        {
            if (gas.moistureRef == i + 1) { gas.moistureRef = 0; }
            else if (gas.moistureRef > i + 1) { --gas.moistureRef; }
        }
        break;
    }
    // References are 1-based indices into this list, so anything pointing at a
    // record that just went away has to be reset before it is read again.
    auto cells = ecProject_->cellColumns();
    MeasurementRecords::validateReferences(gases, cells);
    ecProject_->setGasColumns(gases);
}

/// Whether the project has any H2O to offer.
bool BasicSettingsPage::hasMoistureCandidates() const
{
    if (!ecProject_) { return false; }
    for (const auto& rec : ecProject_->gasColumns())
    {
        if (rec.slug == kH2oSlug && rec.rawColumn > 0) { return true; }
    }
    //> A column the raw file description names counts, whether or not it has
    //> been switched on. Asking only about records hid the moisture combo
    //> entirely on a project whose H2O columns are all inactive - which is
    //> exactly the project where the choice matters, since the gas is being
    //> corrected with another analyser's water for want of it.
    for (const auto& item : candidatesForRole(static_cast<int>(VariableTableRole::H2o)))
    {
        if (item.rawColumn > 0 && !isNoneCandidateColumn(item)) { return true; }
    }
    return false;
}

/// The H2O measurements a gas may be corrected with, as (raw column, label).
///
/// Keyed on the raw column, not on the 1-based record index it used to carry.
/// The list holds columns that have no record yet, and an index into the record
/// list cannot name one of those - there is nothing to point at until it is
/// activated. The column names both kinds, and `moistureRef` goes on storing an
/// index internally.
///
/// Every H2O the raw file description knows about, not only the ones checked in
/// the variable table. A site whose gas sits on an analyser with its own
/// hygrometer should be able to say so without first hunting down that row; the
/// selection switches the column on.
QVector<QPair<int, QString>> BasicSettingsPage::moistureChoices() const
{
    QVector<QPair<int, QString>> out;
    if (!ecProject_) { return out; }

    const auto labelFor = [this](int rawColumn, bool active)
    {
        auto label = tr("H%1O (col %2)").arg(QChar(0x2082)).arg(rawColumn);
        //> Both kinds take the analyser from the metadata, so an active and an
        //> inactive entry for the same instrument read alike.
        const auto instrument = canonicalInstrumentForColumn(rawColumn);
        if (MeasurementRecords::isRealInstrument(instrument))
        {
            label += QStringLiteral(" — ") + instrument;
        }
        //> Choosing this one also switches the column on. Said here, because a
        //> selection that quietly adds a measured gas to the project is the
        //> kind of side effect a user should see coming.
        if (!active) { label += QStringLiteral(" ") + tr("(not yet selected)"); }
        return label;
    };

    QVector<int> seen;
    const auto& gases = ecProject_->gasColumns();
    for (const auto& rec : gases)
    {
        if (rec.slug != kH2oSlug || rec.rawColumn <= 0) { continue; }
        if (seen.contains(rec.rawColumn)) { continue; }
        seen.append(rec.rawColumn);
        out.append(qMakePair(rec.rawColumn, labelFor(rec.rawColumn, true)));
    }

    for (const auto& item : candidatesForRole(static_cast<int>(VariableTableRole::H2o)))
    {
        if (item.rawColumn <= 0 || isNoneCandidateColumn(item)) { continue; }
        if (seen.contains(item.rawColumn)) { continue; }
        seen.append(item.rawColumn);
        out.append(qMakePair(item.rawColumn, labelFor(item.rawColumn, false)));
    }

    //> The biomet relative humidity, last, because it is the fallback rather
    //> than a measurement in the sample stream. Keyed on the ambient combos'
    //> own numbering, where a biomet column is col + 1000 - so it cannot
    //> collide with a raw column, and setMoistureColumnForGas can tell the two
    //> apart without a second parameter.
    if (biometRhAvailable())
    {
        out.append(qMakePair(ecProject_->biomParamColRh(), biometMoistureLabel()));
    }
    return out;
}

/// What the biomet reads as in the Moisture column.
///
/// One spelling, shared by the dropdown entry and the cell that shows the
/// current choice: moistureLabelForGas finds the selected entry by matching
/// this text, so two spellings would leave the cell blank and the delegate
/// would read that as nothing selected.
QString BasicSettingsPage::biometMoistureLabel()
{
    return tr("Biomet RH (site humidity)");
}

int BasicSettingsPage::moistureRefForGas(int gasRecordIndex) const
{
    if (!ecProject_) { return 0; }
    // Resolved, not raw: a stored 0 means "auto", and what the user needs to
    // see is which H2O that actually lands on.
    return MeasurementRecords::resolveMoistureRef(ecProject_->gasColumns(),
                                                  gasRecordIndex,
                                                  biometRhAvailable());
}

QString BasicSettingsPage::moistureLabelForGas(int gasRecordIndex) const
{
    if (!ecProject_) { return QString(); }
    const int ref = moistureRefForGas(gasRecordIndex);
    const auto& gases = ecProject_->gasColumns();
    //> The biomet is not a record, so it has no raw column to match on below.
    if (ref == MeasurementRecords::biometMoistureRef) { return biometMoistureLabel(); }
    if (ref <= 0 || ref > gases.size()) { return QString(); }

    //> Matched on the record's raw column, which is what the choices are keyed
    //> on now. This compared the reference index against choice.first while
    //> that was also an index; keeping it that way once the key changed would
    //> have matched a column number against a record position and shown the
    //> wrong water - or none, which the delegate reads as "no preselection".
    const int rawColumn = gases.at(ref - 1).rawColumn;
    for (const auto& choice : moistureChoices())
    {
        if (choice.first == rawColumn) { return choice.second; }
    }
    return QString();
}

/// Correct \a gasRecordIndex with the H2O measured at \a rawColumn, switching
/// that column on if it is not a record yet.
///
/// Returns whether the record list changed, which the caller needs: activating
/// a column alters a *different* row of the variable table, and a cell-scoped
/// dataChanged would leave that row's checkbox stale.
///
/// The dropdown offers every H2O the raw file description names, so a user can
/// pair a gas with the hygrometer on its own analyser without first finding
/// that row and checking it. Choosing an inactive one is taken as asking for it.
bool BasicSettingsPage::setMoistureColumnForGas(int gasRecordIndex, int rawColumn)
{
    if (!ecProject_ || rawColumn <= 0) { return false; }
    {
        const auto& gases = ecProject_->gasColumns();
        if (gasRecordIndex < 0 || gasRecordIndex >= gases.size()) { return false; }
    }

    //> The biomet relative humidity, which is not an analyser channel and so
    //> has no gas record to point at. Biomet columns carry col + 1000 through
    //> this interface already - the ambient combos build their item data that
    //> way - so the dropdown can offer it without a numbering of its own.
    //> The engine's -1 says the same thing on its side.
    if (rawColumn > 1000)
    {
        auto gases = ecProject_->gasColumns();
        if (gases.at(gasRecordIndex).moistureRef != MeasurementRecords::biometMoistureRef)
        {
            gases[gasRecordIndex].moistureRef =
                MeasurementRecords::biometMoistureRef;
            ecProject_->setGasColumns(gases);
        }
        return false;
    }

    bool recordsChanged = false;
    int waterIndex = gasRecordIndexFor(kH2oSlug, rawColumn);
    if (waterIndex < 0)
    {
        //> Asked before adding, not after. addGasRecord declines silently when
        //> an instrument is at its limit, which would leave the dropdown
        //> showing a choice that did nothing at all.
        const auto blocked = gasLimitBlockReason(rawColumn);
        if (!blocked.isEmpty())
        {
            //> Queued, like every other dialog this selection can raise: the
            //> caller is the delegate's setModelData, and a modal dialog there
            //> spins a nested event loop inside QAbstractItemView::commitData
            //> while the combo editor is still open.
            QPointer<BasicSettingsPage> self(this);
            QTimer::singleShot(0, this, [self, blocked]()
            {
                if (!self) { return; }
                WidgetUtils::warning(QApplication::activeWindow(),
                                     tr("Cannot Add This Water Vapour Measurement"),
                                     blocked);
            });
            return false;
        }
        //> Through addGasRecord, which seeds the new record's per-gas
        //> processing settings from its species. A GasRecord built here would
        //> arrive with every setting at the -1 sentinel, and the writer emits
        //> no key for those - which the engine reads as "not configured" and
        //> answers by declining the test. That is the defect that cost CO2 and
        //> H2O their absolute limits, and it must not come back through a
        //> second door.
        addGasRecord(kH2oSlug, rawColumn);
        waterIndex = gasRecordIndexFor(kH2oSlug, rawColumn);
        if (waterIndex < 0) { return false; }
        recordsChanged = true;
    }

    //> Appended, so no existing 1-based reference moves. Re-read after the add.
    auto gases = ecProject_->gasColumns();
    if (gasRecordIndex < 0 || gasRecordIndex >= gases.size()) { return recordsChanged; }
    if (gases.at(gasRecordIndex).moistureRef != waterIndex + 1)
    {
        gases[gasRecordIndex].moistureRef = waterIndex + 1;
        ecProject_->setGasColumns(gases);
    }
    //> The cross-analyser dialog is the caller's to raise, after it has
    //> redrawn the table. Raised here it would open from inside the delegate's
    //> setModelData, over a table still showing the state before the choice.
    return recordsChanged;
}

/// The analyser of the hygrometer correcting \a gasRecordIndex, when that is
/// not the analyser measuring the gas. Empty when the two agree.
///
/// A legitimate configuration - it is what a site with one hygrometer and two
/// analysers has - and a compromise. The engine honours it: the water vapour
/// flux term is taken at that hygrometer's own time lag, and the dilution to a
/// mixing ratio uses the humidity in its cell. Both were silently declined
/// until recently, while the mean WPL terms used the borrowed water anyway, so
/// a gas came out corrected by a water the rest of the code held it did not
/// share.
///
/// One predicate, because three things ask it: the dialog raised when the user
/// chooses such a pairing, the warning triangle marking one that already
/// exists, and that triangle's tooltip. Three copies of the test would be three
/// chances for the table to disagree with the message.
QString BasicSettingsPage::crossAnalyserWaterInstrument(int gasRecordIndex) const
{
    if (!ecProject_) { return QString(); }
    const auto& gases = ecProject_->gasColumns();
    if (gasRecordIndex < 0 || gasRecordIndex >= gases.size()) { return QString(); }

    const auto& gas = gases.at(gasRecordIndex);
    //> Water is not corrected with itself.
    if (gas.slug == kH2oSlug) { return QString(); }

    //> Resolved, not the stored reference: 0 means "auto", and what matters is
    //> the hygrometer the engine will actually use. Mirrors ResolveGasRef.
    const int ref = MeasurementRecords::resolveMoistureRef(
        gases, gasRecordIndex, biometRhAvailable());
    if (ref <= 0 || ref > gases.size()) { return QString(); }
    const auto& water = gases.at(ref - 1);

    //> `other` and `none` are not identities - many unrelated variables carry
    //> them - so a pairing involving one says nothing about analysers.
    if (!MeasurementRecords::isRealInstrument(gas.instrumentId)
        || !MeasurementRecords::isRealInstrument(water.instrumentId)
        || gas.instrumentId == water.instrumentId)
    {
        return QString();
    }
    return water.instrumentId;
}

/// Tell the user their choice of hygrometer crosses analysers.
///
/// Raised from setMoistureColumnForGas alone, so it answers an explicit
/// selection. It used to run from the resolution step as well, which meant
/// opening an already-configured project raised a dialog about a decision
/// nobody had just made. A pairing that is merely *already* the case is marked
/// by the triangle in the variable table instead.
///
/// The text describes the choice and no more. It used to state that the gas's
/// own analyser carried no H2O column - true of the inherited case, and not of
/// a user picking another analyser's water while their own has one.
void BasicSettingsPage::warnOnCrossAnalyserMoisture(int gasRecordIndex)
{
    const QString waterInstrument = crossAnalyserWaterInstrument(gasRecordIndex);
    if (waterInstrument.isEmpty()) { return; }

    const auto& gases = ecProject_->gasColumns();
    const auto& gas = gases.at(gasRecordIndex);

    QString species = gas.slug.toUpper();
    if (species.isEmpty()) { species = tr("This gas"); }

    WidgetUtils::information(
        QApplication::activeWindow(),
        tr("Water Vapour From a Different Analyser"),
        tr("<b>%1</b> is measured by <b>%2</b>, but the H<sub>2</sub>O you "
           "selected is measured by <b>%3</b>."
           "<p>EddyFlow will use that hygrometer's own time lag for the water "
           "vapour flux term, and the humidity in its cell to convert "
           "<b>%1</b> to a mixing ratio. Both describe the air inside a "
           "different analyser, so both are approximations.</p>"
           "<p>If <b>%2</b> also measures H<sub>2</sub>O, selecting that "
           "column instead will give a more accurate correction.</p>")
            .arg(species, gas.instrumentId, waterInstrument));
}

/// Whether a biomet relative humidity column is overriding the hygrometers.
///
/// One predicate for the dialog, the triangle and the tooltip. Three copies of
/// a condition is how the three come to disagree about when to appear.
///
/// Both halves matter. A biomet RH column alone is not the case being warned
/// about - with no hygrometer it is the *cure*, and showNoHumidityWarning says
/// so. The two warnings are opposites and must never both fire, which is why
/// they test the same two things and reach opposite conclusions.
bool BasicSettingsPage::biometRhAvailable() const
{
    return ecProject_ && ecProject_->biomParamColRh() > 0;
}

bool BasicSettingsPage::biometRhOverrideActive() const
{
    if (!ecProject_) { return false; }
    if (!biometRhAvailable()) { return false; }
    return ecProject_->biometRhOverride();
}

/// Tell the user their biomet RH replaces what the hygrometers measured.
///
/// Raised from updateRhCombo alone, so it answers a selection the user just
/// made. Opening a project that already has both is marked by the triangle on
/// the RH row instead - the rule established for the cross-analyser water
/// warning above, for the same reason: a dialog about a decision nobody just
/// took is noise.
///
/// No once-per-session flag, also as above. It fires on an action, and an
/// action repeated deserves the same answer.
void BasicSettingsPage::warnOnBiometRhOverride()
{
    WidgetUtils::information(
        QApplication::activeWindow(),
        tr("Every Gas Will Use the Biomet Humidity"),
        tr("<p>Every gas's moisture source has been set to the biomet "
           "relative humidity sensor. That is the humidity EddyFlow will use "
           "to correct all of them &mdash; the WPL density term, the drift "
           "correction, and the LI-7700 multipliers where they apply.</p>"
           "<p>The hygrometers still report what they measured. Their "
           "<i>mole fraction</i>, <i>mixing ratio</i> and <i>molar density</i> "
           "columns are unaffected, and the biomet humidity appears beside "
           "them as <i>h2o_biomet_*</i> so the two can be compared.</p>"
           "<p>Unticking returns every gas to <b>Automatic</b>. It cannot "
           "restore what each gas named before, because that is not kept.</p>"));
}

/// Enable and check the box from the project.
///
/// Enabled only while a biomet RH column is selected: without one there is
/// nothing to override, and a tickable box that did nothing would be worse
/// than an absent one.
void BasicSettingsPage::refreshBiometRhOverrideBox()
{
    if (!ecProject_ || !biometRhOverrideBox) { return; }
    QSignalBlocker blocker(biometRhOverrideBox);
    biometRhOverrideBox->setEnabled(biometRhAvailable());
    biometRhOverrideBox->setChecked(biometRhOverrideActive());
}

/// Point every gas at the biomet, or return them all to automatic.
void BasicSettingsPage::onBiometRhOverrideToggled(bool on)
{
    if (!ecProject_) { return; }
    if (!ecProject_->setBiometRhOverride(on)) { return; }

    //> Every Moisture cell just changed, and the RH row's triangle with them.
    refreshVariableTables();
    //> Only on the way in. Unticking is a return to the default and says so on
    //> the box itself; a dialog for it would be noise.
    if (on) { warnOnBiometRhOverride(); }
}

/// Molecular weight of a gas record, or its species default.
///
/// The record is the authority; the species table supplies the value the
/// record does not carry. Only an override reaches the file, so a project
/// whose gas is later given better constants picks them up.
qreal BasicSettingsPage::gasMolecularWeight(int gasRecordIndex) const
{
    if (!ecProject_) { return -1.0; }
    const auto& gases = ecProject_->gasColumns();
    if (gasRecordIndex < 0 || gasRecordIndex >= gases.size()) { return -1.0; }
    if (gases.at(gasRecordIndex).mw > 0.0) { return gases.at(gasRecordIndex).mw; }
    const auto* gas = GasMetadata::findSpecies(gases.at(gasRecordIndex).slug);
    return gas ? gas->molecularWeight : -1.0;
}

qreal BasicSettingsPage::gasDiffusivity(int gasRecordIndex) const
{
    if (!ecProject_) { return -1.0; }
    const auto& gases = ecProject_->gasColumns();
    if (gasRecordIndex < 0 || gasRecordIndex >= gases.size()) { return -1.0; }
    if (gases.at(gasRecordIndex).diff > 0.0) { return gases.at(gasRecordIndex).diff; }
    const auto* gas = GasMetadata::findSpecies(gases.at(gasRecordIndex).slug);
    return gas ? gas->diffusivity : -1.0;
}

/// Store a molecular weight on the record, or clear it back to the default.
///
/// This used to be setGeneralColGasMw, which writes the project-wide gas_mw -
/// a key writeMeasurementRecords deletes before saving, so every value the
/// user typed was discarded and the engine fell back to its own default. For
/// any species but CO2, H2O, CH4 and N2O that default was nitrous oxide's.
///
/// A value matching the species default is stored as "no override", so the
/// file stays free of redundant keys and fuzzyCompare does not see a change
/// where the user made none.
void BasicSettingsPage::setGasMolecularWeight(int gasRecordIndex, qreal value)
{
    if (!ecProject_) { return; }
    auto gases = ecProject_->gasColumns();
    if (gasRecordIndex < 0 || gasRecordIndex >= gases.size()) { return; }

    const auto* gas = GasMetadata::findSpecies(gases.at(gasRecordIndex).slug);
    const qreal stored =
        (gas && qFuzzyCompare(value, gas->molecularWeight)) ? -1.0 : value;
    if (qFuzzyCompare(gases.at(gasRecordIndex).mw, stored)) { return; }
    gases[gasRecordIndex].mw = stored;
    ecProject_->setGasColumns(gases);
}

void BasicSettingsPage::setGasDiffusivity(int gasRecordIndex, qreal value)
{
    if (!ecProject_) { return; }
    auto gases = ecProject_->gasColumns();
    if (gasRecordIndex < 0 || gasRecordIndex >= gases.size()) { return; }

    const auto* gas = GasMetadata::findSpecies(gases.at(gasRecordIndex).slug);
    const qreal stored =
        (gas && qFuzzyCompare(value, gas->diffusivity)) ? -1.0 : value;
    if (qFuzzyCompare(gases.at(gasRecordIndex).diff, stored)) { return; }
    gases[gasRecordIndex].diff = stored;
    ecProject_->setGasColumns(gases);
}

BasicSettingsPage::BasicSettingsPage(QWidget *parent, DlProject *dlProject, EcProject *ecProject, ConfigState* config) :
    QWidget(parent),
    findFileProgressWidget(nullptr),
    dlProject_(dlProject),
    ecProject_(ecProject),
    configState_(config),
    rawFilenameDialog(nullptr),
    suffixList_(QStringList()),
    httpManager_(nullptr),
    httpReply_(nullptr),
    magneticDeclinationFetchProgress(nullptr),
    currentRawDataList_(QStringList()),
    currentFilteredRawDataList_(QStringList()),
    biomList_(QList<BiomItem>()),
    fluxVariablesModel_(nullptr),
    ambientVariablesModel_(nullptr),
    fluxVariablesTable_(nullptr),
    ambientVariablesTable_(nullptr)
{
    findFileProgressWidget = new QProgressIndicator;
    findFileProgressWidget->setAnimationDelay(40);
    findFileProgressWidget->setDisplayedWhenStopped(false);
    findFileProgressWidget->setFixedSize(21, 21);
    findFileProgressWidget->setColor(QColor(46, 98, 152));

    magneticDeclinationFetchProgress = new QProgressIndicator;
    magneticDeclinationFetchProgress->setAnimationDelay(40);
    magneticDeclinationFetchProgress->setDisplayedWhenStopped(false);
    magneticDeclinationFetchProgress->setFixedSize(21, 21);
    magneticDeclinationFetchProgress->setColor(QColor(46, 98, 152));

    datapathLabel = new ClickLabel(tr("Raw data directory :"), this);
    datapathLabel->setToolTip(tr("<b>Raw data directory:</b> Use the <i>Browse...</i> button to specify the folder that contains the raw data. If data are also contained in subfolders, select the <i>Search in subfolders</i> box."));

    datapathBrowse = new DirBrowseWidget;
    datapathBrowse->disableClearAction();
    datapathBrowse->setToolTip(datapathLabel->toolTip());
    datapathBrowse->setDialogTitle(tr("Select the Raw Data Directory"));
    datapathBrowse->setDialogWorkingDir(WidgetUtils::getDialogPathHint(QStringLiteral("raw_data_dir")));

    recursionCheckBox = new QCheckBox;
    recursionCheckBox->setText(tr("Search in subfolders"));
    recursionCheckBox->setToolTip(tr("<b>Search in subfolders:</b> Check this box if data are in subfolders in the selected directory. EddyFlow will process files that are in the <i>Raw data directory</i> and <i>subfolders</i> if you check this box."));

    filesFound = new QLabel;
    filesFound->setProperty("greyLabel", true);

    filePrototypeLabel = new ClickLabel;
    filePrototypeLabel->setText(tr("Raw file name format :"));
    filePrototypeLabel->setToolTip(tr("<b>Raw file name format:</b> For raw files other than GHG, your entry in this field should provide a template of the file names that EddyFlow uses to retrieve the timestamp. You must indicate which part of the file name represent the year (<i>yy</i> or <i>yyyy</i>), month (<i>mm</i>, if using <i>dd</i> for the day, omit if using <i>ddd</i>), day (<i>dd</i> for the day of the month, <i>ddd</i> for the day of the year), hour (<i>HH</i>), minute (<i>MM</i>), and the extension of the file. The question mark can match any single character. For example, for a file name of the type: '2015-05-27_1030_mysite-12.raw', a valid raw file name format is: 'yyyy-mm-dd_HHMM_mysite-??.raw'. Remember to include the file extension!"));

    filePrototypeEdit = new FileFormatWidget;
    filePrototypeEdit->setReadOnly(false);
    filePrototypeEdit->setToolTip(tr("Open the <i>Raw file name format</i> dialog box to change the raw file name format."));
    filePrototypeEdit->setButtonText(tr("Set..."));
    filePrototypeEdit->disableClickAction();

    outpathLabel = new ClickLabel(tr("Output directory :"), this);
    outpathLabel->setToolTip(tr("<b>Output directory:</b> Specify where the output files will be stored. Click the <i>Browse...</i> button and navigate to the desired directory. You can also type/edit it directly from this text box. Note that the software will create subfolders inside the selected output directory."));

    outpathBrowse = new DirBrowseWidget;
    outpathBrowse->setReadOnly(false);
    outpathBrowse->setToolTip(outpathLabel->toolTip());
    outpathBrowse->setDialogTitle(tr("Select the Output Directory"));
    outpathBrowse->setDialogWorkingDir(WidgetUtils::getDialogPathHint(QStringLiteral("output_dir")));

    idLabel = new ClickLabel(tr("Output ID :"));
    idLabel->setToolTip(tr("<b>Output ID:</b> Enter the ID. This string will be appended to each output file name so a short ID is recommended. Note that characters that result in file names that are unacceptable to the commonest operating systems (this includes | \\ / : ; ? * ' \" < > CR LF TAB SPACE and other non readable characters) are not permitted."));

    idEdit = new CustomClearLineEdit;
    idEdit->setToolTip(idLabel->toolTip());
    idEdit->setMaxLength(255);
    idEdit->setMaximumWidth(outpathBrowse->returnLineEditWidth());

    // prevent filesystem's illegal characters and whitespace insertion:
    // exclude the first 33 (from 0 to 32) ASCII chars, including
    // '\0'(NUL),'\a'(BEL),'\b'(BS),'\t'(TAB),'\n'(LF),'\v'(VT),'\f'(FF),'\r'(CR) and ' '(SPACE)
    // plus the following:
    // '|', '\', '/', ':', ';', '?', '*', '"', ''', '`', '<', '>'
    QString idRegexp = QStringLiteral("[^\\000-\\040|\\\\/:;\\?\\*\"'`<>]+");

    idEdit->setRegExp(idRegexp);

    avgIntervalLabel = new ClickLabel(tr("Flux averaging interval :"), this);
    avgIntervalLabel->setToolTip(tr("<b>Flux averaging interval:</b> This is the time span over which fluxes will be averaged. The flux averaging interval can be shorter than, equal to, or longer than the raw file duration. Set <i>0</i> to use the input file duration as the flux averaging interval, in which case <i>File as is</i> appears in the field."));
    avgIntervalSpin = new QSpinBox(this);
    avgIntervalSpin->setRange(0, 1440);
    avgIntervalSpin->setSingleStep(1);
    avgIntervalSpin->setValue(30);
    avgIntervalSpin->setSuffix(QStringLiteral(" [min]"));
    avgIntervalSpin->setAccelerated(true);
    avgIntervalSpin->setSpecialValueText(tr("File as is"));
    avgIntervalSpin->setToolTip(avgIntervalLabel->toolTip());
    avgIntervalSpin->setMinimumWidth(110);
    avgIntervalSpin->setMaximumWidth(125);

    maxLackLabel = new ClickLabel(tr("Missing samples allowance :"), this);
    maxLackLabel->setToolTip(tr("<b>Missing sample allowance:</b> Enter the "
                                "maximum percentage of missing data allowed "
                                "for each variable, for each averaging interval. "
                                "If the percentage of missing values exceeds "
                                "this threshold for a given variable, all "
                                "results that need that variable will not "
                                "be computed. Data can be 'missing' either "
                                "because absent in the raw data files, or "
                                "because eliminated during one of the raw "
                                "data screening procedures, e.g. de-spiking. "
                                "This is also the file's own completeness "
                                "threshold, and the allowance for every "
                                "instrument that is not given one of its own "
                                "under <i>Missing samples allowance, per "
                                "instrument</i>, which is listed below once a "
                                "metadata file describing the instruments has "
                                "been loaded."));
    maxLackSpin = new QSpinBox;
    maxLackSpin->setRange(0, 99);
    maxLackSpin->setSingleStep(1);
    maxLackSpin->setValue(10);
    maxLackSpin->setAccelerated(true);
    maxLackSpin->setSuffix(tr("  [%]", "Percentage"));
    maxLackSpin->setToolTip(maxLackLabel->toolTip());
    maxLackSpin->setMinimumWidth(110);
    maxLackSpin->setMaximumWidth(125);

    //> Filled by refreshInstrMaxLackRows once a metadata file is loaded. It
    //> starts empty and hidden, because a project with no metadata has no
    //> instruments to state an allowance for.
    instrLackLayout_ = new QGridLayout;
    instrLackLayout_->setContentsMargins(0, 0, 0, 0);
    instrLackLayout_->setVerticalSpacing(3);
    instrLackContainer_ = new QWidget(this);
    instrLackContainer_->setLayout(instrLackLayout_);
    instrLackContainer_->hide();

    lockedIcon = new QLabel;
    auto pixmap_2x = QPixmap(QStringLiteral(":/icons/vlink-locked"));
#if defined(Q_OS_MACOS)
    pixmap_2x.setDevicePixelRatio(2.0);
#endif
    lockedIcon->setPixmap(pixmap_2x);

    startDateLabel = new ClickLabel(this);
    startDateLabel->setText(tr("Start :"));
    startDateLabel->setToolTip(tr("<b>Start:</b> Starting date of the dataset to process. This may or may not coincide with the date of the first raw file. It is used to select a subset of available raw data for processing."));
    startDateEdit = new QDateEdit;
    startDateEdit->setToolTip(startDateLabel->toolTip());
    startDateEdit->setCalendarPopup(true);
    startDateEdit->setDisplayFormat(WidgetUtils::eddyDateFormat());
    startDateEdit->setMinimumWidth(100);
    WidgetUtils::customizeCalendar(startDateEdit->calendarWidget());

    startTimeEdit = new QTimeEdit;
    startTimeEdit->setDisplayFormat(QStringLiteral("hh:mm"));
    startTimeEdit->setMinimumWidth(60);
    startTimeEdit->setAccelerated(true);

    auto startDateContainer = new QHBoxLayout;
    startDateContainer->addWidget(startDateEdit);
    startDateContainer->insertSpacing(1, 10);
    startDateContainer->addWidget(startTimeEdit);
    startDateContainer->addStretch();
    startDateContainer->setContentsMargins(0, 0, 0, 0);

    endDateLabel = new ClickLabel(this);
    endDateLabel->setText(tr("End :"));
    endDateLabel->setToolTip(tr("<b>End:</b> Ending date of the dataset to process. This may or may not coincide with the date of the last raw file. It is used to select a subset of available raw data for processing."));
    endDateEdit = new QDateEdit;
    endDateEdit->setToolTip(endDateLabel->toolTip());
    endDateEdit->setCalendarPopup(true);
    endDateEdit->setDisplayFormat(WidgetUtils::eddyDateFormat());
    endDateEdit->setMinimumWidth(100);
    WidgetUtils::customizeCalendar(endDateEdit->calendarWidget());

    endTimeEdit = new QTimeEdit;
    endTimeEdit->setDisplayFormat(QStringLiteral("hh:mm"));
    endTimeEdit->setMinimumWidth(60);
    endTimeEdit->setAccelerated(true);

    auto endDateContainer = new QHBoxLayout;
    endDateContainer->addWidget(endDateEdit);
    endDateContainer->insertSpacing(1, 10);
    endDateContainer->addWidget(endTimeEdit);
    endDateContainer->addStretch();
    endDateContainer->setContentsMargins(0, 0, 0, 0);

    subsetCheckBox = new QCheckBox;
    subsetCheckBox->setText(tr("Select a different period"));
    subsetCheckBox->setToolTip(tr("<b>Select a different period:</b> Select this option if you only want to process a subset of data in the raw data directory. Leave it blank to process all available raw data."));
    subsetCheckBox->setStyleSheet(QStringLiteral("QCheckBox {margin-left: 13px}"));

    dateRangeDetectButton = new QPushButton(tr("Detect Dataset Dates"));
    dateRangeDetectButton->setProperty("mdButton", true);
    dateRangeDetectButton->setMinimumWidth(dateRangeDetectButton->sizeHint().width());
    dateRangeDetectButton->setMaximumWidth(dateRangeDetectButton->sizeHint().width());
    dateRangeDetectButton->setToolTip(tr("<b>Detect Dataset Dates:</b> Click this button to ask EddyFlow to retrieve the starting and ending date of the raw dataset contained in the <i>Raw data directory</i>. You can override this automatic setting by using the <i>Select a different period</i> option."));

    crossWindCheckBox = new QCheckBox(tr("Cross wind correction of sonic temperature applied by the anemometer firmware"));
    crossWindCheckBox->setToolTip(tr("<b>Cross-wind correction for sonic temperature:</b> Check this box if the crosswind correction is applied internally by the anemometer firmware before outputting sonic temperature. Be aware that some anemometers do apply the correction internally, others not, and others provide it as an option.<br />"
                                     "Users of Gill WindMaster and WindMaster Pro: the crosswind correction is not applied internally in anemometer units of type 1352, while it is available in the firmware of later types 1561 and 1590."));
    crossWindCheckBox->setProperty("paddedCheckbox", true);

    northLabel = new QLabel(tr("North reference :"));
    northLabel->setToolTip(tr("<b>North reference:</b> Indicate whether you want the outputs to be referenced to magnetic or geographic north. If you choose geographic north, EddyFlow can retrieve the Magnetic Declination at your site from NOAA (U.S. National Oceanic and Atmospheric Administration) online resources (http://www.ngdc.noaa.gov/geomag-web/calculators/calculateDeclination). You can also enter the magnetic declination manually. EddyFlow assumes that north is assessed at the site using the compass, so that everything you provide to the software is with respect to local geographic north. If, instead, your measurements are taken with respect to due north, than just select <i>Use magnetic North</i> or enter a declination of zero degrees."));

    useMagneticNRadio = new QRadioButton;
    useMagneticNRadio->setText(tr("Use magnetic North"));
    useGeographicNRadio = new QRadioButton;
    useGeographicNRadio->setText(tr("Use geographic North"));

    auto northRadioGroup = new QButtonGroup(this);
    northRadioGroup->addButton(useMagneticNRadio, 0);
    northRadioGroup->addButton(useGeographicNRadio, 1);

    declinationLabel = new ClickLabel(tr("Magnetic declination :"));
    declinationEdit = new QLineEdit;
    declinationEdit->setText(tr("000%1 00' E").arg(Defs::DEGREE));
    QString dec_pattern = tr("(?:(0\\d\\d)%1\\s([0-5]\\d)'\\s(E|W))|").arg(Defs::DEGREE);
            dec_pattern += tr("(?:(1[0-7]\\d)%1\\s([0-5]\\d)'\\s(E|W))|").arg(Defs::DEGREE);
            dec_pattern += tr("(?:(180)%1\\s(00)'\\s(E|W))").arg(Defs::DEGREE);
    QRegularExpression decRx(dec_pattern);
    auto decValidator = new QRegularExpressionValidator(decRx, declinationEdit);
    declinationEdit->setValidator(decValidator);
    declinationEdit->setInputMask(tr("000%1 00' >A;x").arg(Defs::DEGREE));
    declinationEdit->setAlignment(Qt::AlignRight);
    declinationEdit->setMinimumWidth(110);
    declinationEdit->setMaximumWidth(125);

    declinationDateLabel = new ClickLabel(this);
    declinationDateLabel->setText(tr("On :"));
    declinationDateLabel->setToolTip(tr("<b> Date :</b> Date used to retrieve the magnetic declination from the NOAA website."));
    declinationDateEdit = new QDateEdit;
    declinationDateEdit->setToolTip(declinationDateLabel->toolTip());
    declinationDateEdit->setCalendarPopup(true);
    declinationDateEdit->setDisplayFormat(WidgetUtils::eddyDateFormat());
    declinationDateEdit->setMinimumWidth(100);
    WidgetUtils::customizeCalendar(declinationDateEdit->calendarWidget());
    // NOTE: manage NOAA website API limitation, where current last day available is 2019-12-31
    // compare http://www.ngdc.noaa.gov/geomag-web/#declination
    declinationDateEdit->setMaximumDate(QDate(2019, 12, 31));

    decChangingLabel = new QLabel;
    decChangingLabel->setTextFormat(Qt::PlainText);
    decChangingLabel->setObjectName(QStringLiteral("citeLabel"));

    declinationFetchButton = new QPushButton(tr("Fetch from NOAA"));
    declinationFetchButton->setProperty("mdButton", true);
    declinationFetchButton->setMinimumWidth(declinationFetchButton->sizeHint().width());
    declinationFetchButton->setMaximumWidth(declinationFetchButton->sizeHint().width());

    createQuestionMark();

    auto fileGroupTitle = new QLabel(tr("Files Info"));
    fileGroupTitle->setProperty("groupTitle", true);

    auto filesInfoLayout = new QGridLayout;
    filesInfoLayout->addWidget(datapathLabel, 1, 0, Qt::AlignRight);
    filesInfoLayout->addWidget(datapathBrowse, 1, 2, 1, 3);
    filesInfoLayout->addWidget(filesFound, 2, 4, 1, 1, Qt::AlignRight);
    filesInfoLayout->addWidget(findFileProgressWidget, 2, 4, 1, 1, Qt::AlignCenter);
    filesInfoLayout->addWidget(recursionCheckBox, 2, 2, 1, 2);
    filesInfoLayout->addWidget(subsetCheckBox, 3, 3, 1, 2, Qt::AlignLeft);
    filesInfoLayout->addWidget(dateRangeDetectButton, 3, 2, 1, 1, Qt::AlignLeft);
    filesInfoLayout->addWidget(startDateLabel, 4, 0, Qt::AlignRight);
    filesInfoLayout->addLayout(startDateContainer, 4, 2, 1, 2);
    filesInfoLayout->addWidget(lockedIcon, 4, 1, 2, 1, Qt::AlignCenter);
    filesInfoLayout->addWidget(endDateLabel, 5, 0, Qt::AlignRight);
    filesInfoLayout->addLayout(endDateContainer, 5, 2, 1, 2);

    filesInfoLayout->addWidget(filePrototypeLabel, 6, 0, Qt::AlignRight);
    filesInfoLayout->addWidget(questionMark_3, 6, 1);
    filesInfoLayout->addWidget(filePrototypeEdit, 6, 2, 1, 3);
    filesInfoLayout->addWidget(outpathLabel, 7, 0, Qt::AlignRight);
    filesInfoLayout->addWidget(outpathBrowse, 7, 2, 1, 3);
    filesInfoLayout->addWidget(idLabel, 8, 0, Qt::AlignRight);
    filesInfoLayout->addWidget(idEdit, 8, 2, 1, 2);

    filesInfoLayout->addWidget(maxLackLabel, 1, 5, Qt::AlignRight);
    filesInfoLayout->addWidget(maxLackSpin, 1, 7, 1, 1);
    filesInfoLayout->addWidget(avgIntervalLabel, 2, 5, Qt::AlignRight);
    filesInfoLayout->addWidget(avgIntervalSpin, 2, 7);
    filesInfoLayout->addWidget(northLabel, 4, 5, Qt::AlignRight);
    filesInfoLayout->addWidget(useMagneticNRadio, 4, 7, 1, 2);
    filesInfoLayout->addWidget(useGeographicNRadio, 5, 7, 1, 2);
    filesInfoLayout->addWidget(declinationFetchButton, 5, 9, 1, 2);
    filesInfoLayout->addWidget(declinationLabel, 6, 5, Qt::AlignRight);
    filesInfoLayout->addWidget(declinationEdit, 6, 7);
    filesInfoLayout->addWidget(declinationDateLabel, 6, 8, Qt::AlignRight);
    filesInfoLayout->addWidget(declinationDateEdit, 6, 9);
    filesInfoLayout->addWidget(questionMark_4, 6, 10, Qt::AlignLeft);
    filesInfoLayout->addWidget(decChangingLabel, 7, 7, 1, -1);
    filesInfoLayout->addWidget(magneticDeclinationFetchProgress, 8, 7);
    filesInfoLayout->addWidget(instrLackContainer_, 9, 0, 1, 11);
    filesInfoLayout->setRowStretch(10, 1);
    filesInfoLayout->setRowMinimumHeight(1, 21);
    filesInfoLayout->setRowMinimumHeight(2, 21);
    filesInfoLayout->setRowMinimumHeight(3, 21);
    filesInfoLayout->setRowMinimumHeight(4, 21);
    filesInfoLayout->setRowMinimumHeight(5, 21);
    filesInfoLayout->setRowMinimumHeight(6, 21);
    filesInfoLayout->setRowMinimumHeight(7, 21);
    filesInfoLayout->setRowMinimumHeight(8, 21);
    filesInfoLayout->setRowMinimumHeight(9, 21);
    filesInfoLayout->setColumnMinimumWidth(1, 10);
    filesInfoLayout->setColumnMinimumWidth(5, 150);
    filesInfoLayout->setColumnStretch(0, 1);
    filesInfoLayout->setColumnStretch(1, 0);
    filesInfoLayout->setColumnStretch(2, 0);
    filesInfoLayout->setColumnStretch(3, 0);
    filesInfoLayout->setColumnStretch(4, 1);
    filesInfoLayout->setColumnStretch(5, 1);
    filesInfoLayout->setColumnStretch(6, 0);
    filesInfoLayout->setColumnStretch(7, 1);
    filesInfoLayout->setColumnStretch(8, 0);
    filesInfoLayout->setColumnStretch(9, 1);
    filesInfoLayout->setColumnStretch(10, 0);
    filesInfoLayout->setVerticalSpacing(3);
    filesInfoLayout->setContentsMargins(0, 0, 50, 15);

    anemRefLabel = new ClickLabel(tr("Master Anemometer :"), this);
    anemRefLabel->setToolTip(tr("<b>Master anemometer:</b> Select the sonic anemometer from which wind and sonic temperature data should be used for calculating fluxes."));
    anemRefCombo = new QComboBox;
    anemRefCombo->setToolTip(anemRefLabel->toolTip());

    anemFlagLabel = new ClickLabel(tr("Anemometer Diagnostics :"), this);
    anemFlagLabel->setToolTip(tr("<b>anemometer Diagnostics:</b> Select the anemometer diagnostics that will be used to filter records for flux computation. Records will be excluded when corresponding diagnostic variables indicate data quality cannot be ensured."));
    anemFlagCombo = new QComboBox;
    anemFlagCombo->setToolTip(anemFlagLabel->toolTip());

    tsRefLabel = new ClickLabel(tr("Fast temperature reading (alternative to sonic temp) :"), this);
    tsRefLabel->setToolTip(tr("<b>Fast temperature reading:</b> If raw files contain valid readings of air temperature collected at high frequency (e.g. by a thermocouple), you can use any of them in place of sonic temperature. In this case, corrections specific to sonic temperature (cross-wind correction, humidity correction), will not be applied."));
    tsRefCombo = new QComboBox;
    tsRefCombo->setToolTip(tsRefLabel->toolTip());

    const QString fluxVariableTooltip = tr("Select the variables to be used for calculating fluxes, among those available.");
    const QString diagnosticVariableTooltip = tr("Select the variables to be used for diagnostics of this gas analyzer.");





    gasMwLabel = new ClickLabel(tr("Molecular weight :"), this);
    gasMw = new QDoubleSpinBox;
    gasMw->setDecimals(4);
    gasMw->setRange(0.0, 1000.0);
    gasMw->setSingleStep(1.0);
    gasMw->setAccelerated(true);
    gasMw->setSuffix(QStringLiteral(" [g/mol]"));
    gasMw->setMinimumWidth(130);

    gasDiffLabel = new ClickLabel(tr("Molecular diffusivity in air :"), this);
    gasDiff = new QDoubleSpinBox;
    gasDiff->setDecimals(5);
    gasDiff->setRange(0.0, 1.0);
    gasDiff->setSingleStep(0.1);
    gasDiff->setAccelerated(true);
    gasDiff->setSuffix(QStringLiteral(" [%1]").arg(Defs::CM2_S_STRING));
    gasDiff->setMinimumWidth(130);

    moreButton = new QPushButton;
    moreButton->setObjectName(QStringLiteral("foldButton"));
    moreButton->setCheckable(true);
    moreButton->setChecked(false);
    moreButton->setAutoDefault(false);
    moreButton->setFlat(true);
    { QIcon foldIcon; foldIcon.addPixmap(QPixmap(QStringLiteral(":/icons/fold-down")), QIcon::Normal, QIcon::Off); foldIcon.addPixmap(QPixmap(QStringLiteral(":/icons/fold-down-hover")), QIcon::Active, QIcon::Off); foldIcon.addPixmap(QPixmap(QStringLiteral(":/icons/fold-up")), QIcon::Normal, QIcon::On); foldIcon.addPixmap(QPixmap(QStringLiteral(":/icons/fold-up-hover")), QIcon::Active, QIcon::On); moreButton->setIcon(foldIcon); moreButton->setIconSize(QSize(16, 9)); }

    auto extensionLayout = new QGridLayout;
    extensionLayout->setContentsMargins(0, 0, 0, 0);
    extensionLayout->addWidget(gasMwLabel, 0, 0, Qt::AlignRight);
    extensionLayout->addWidget(gasMw, 0, 1);
    extensionLayout->addWidget(gasDiffLabel, 1, 0, Qt::AlignRight);
    extensionLayout->addWidget(gasDiff, 1, 1);
    gasExtension = new QWidget;
    gasExtension->setLayout(extensionLayout);
    gasExtension->hide();





    airTRefCombo = new QComboBox;
    airTRefCombo->setToolTip(fluxVariableTooltip);

    airPRefCombo = new QComboBox;
    airPRefCombo->setToolTip(fluxVariableTooltip);

    rhCombo = new QComboBox;
    rhCombo->setToolTip(fluxVariableTooltip);

    rgCombo = new QComboBox;
    rgCombo->setToolTip(fluxVariableTooltip);

    lwinCombo = new QComboBox;
    lwinCombo->setToolTip(fluxVariableTooltip);

    ppfdCombo = new QComboBox;
    ppfdCombo->setToolTip(fluxVariableTooltip);




    flag1Label = new ClickLabel(tr("Flag 1 :"), this);
    flag1Label->setObjectName(QStringLiteral("flag1Label"));
    flag1Label->setToolTip(tr("<b>Flags:</b> Each column of the raw data file that was not tagged as <i>to be ignored</i> can be used as a mask to filter out individual high frequency records. Up to ten flags can be specified. Note that an entire record (that is, all variables measured at a certain time instant, one line of raw data) is eliminated any time a flag variable does not comply with its quality criterion."));
    flag1Label->setProperty("optionalField", true);
    flag1VarCombo = new QComboBox;
    flag1VarCombo->setObjectName(QStringLiteral("flag1Combo"));
    flag1VarCombo->setMaxVisibleItems(20);
    flag1VarCombo->setToolTip(flag1Label->toolTip());
    flag1UnitLabel = new QLabel;
    flag1UnitLabel->setObjectName(QStringLiteral("flag1UnitLabel"));
    flag1UnitLabel->setProperty("flagLabel", true);
    flag1ThresholdSpin = new QDoubleSpinBox;
    flag1ThresholdSpin->setDecimals(10);
    flag1ThresholdSpin->setRange(-9999999.0, 9999999.0);
    flag1ThresholdSpin->setSingleStep(1.0);
    flag1ThresholdSpin->setAccelerated(true);
    flag1PolicyCombo = new QComboBox;
    flag1PolicyCombo->addItem(FLAG_POLICY_STRING_0);
    flag1PolicyCombo->addItem(FLAG_POLICY_STRING_1);

    flag2Label = new ClickLabel(tr("Flag 2 :"), this);
    flag2Label->setObjectName(QStringLiteral("flag2Label"));
    flag2Label->setToolTip(tr("<b>Flags:</b> Each column of the raw data file that was not tagged as <i>to be ignored</i> can be used as a mask to filter out individual high frequency records. Up to ten flags can be specified. Note that an entire record (that is, all variables measured at a certain time instant, one line of raw data) is eliminated any time a flag variable does not comply with its quality criterion."));
    flag2Label->setProperty("optionalField", true);
    flag2VarCombo = new QComboBox;
    flag2VarCombo->setObjectName(QStringLiteral("flag2Combo"));
    flag2VarCombo->setMaxVisibleItems(20);
    flag2VarCombo->setToolTip(flag2Label->toolTip());
    flag2UnitLabel = new QLabel;
    flag2UnitLabel->setObjectName(QStringLiteral("flag2UnitLabel"));
    flag2UnitLabel->setProperty("flagLabel", true);
    flag2ThresholdSpin = new QDoubleSpinBox;
    flag2ThresholdSpin->setDecimals(10);
    flag2ThresholdSpin->setRange(-9999999.0, 9999999.0);
    flag2ThresholdSpin->setSingleStep(1.0);
    flag2ThresholdSpin->setAccelerated(true);
    flag2PolicyCombo = new QComboBox;
    flag2PolicyCombo->addItem(FLAG_POLICY_STRING_0);
    flag2PolicyCombo->addItem(FLAG_POLICY_STRING_1);

    flag3Label = new ClickLabel(tr("Flag 3 :"), this);
    flag3Label->setObjectName(QStringLiteral("flag3Label"));
    flag3Label->setToolTip(tr("<b>Flags:</b> Each column of the raw data file that was not tagged as <i>to be ignored</i> can be used as a mask to filter out individual high frequency records. Up to ten flags can be specified. Note that an entire record (that is, all variables measured at a certain time instant, one line of raw data) is eliminated any time a flag variable does not comply with its quality criterion."));
    flag3Label->setProperty("optionalField", true);
    flag3VarCombo = new QComboBox;
    flag3VarCombo->setObjectName(QStringLiteral("flag3Combo"));
    flag3VarCombo->setMaxVisibleItems(20);
    flag3VarCombo->setToolTip(flag3Label->toolTip());
    flag3UnitLabel = new QLabel;
    flag3UnitLabel->setObjectName(QStringLiteral("flag3UnitLabel"));
    flag3UnitLabel->setProperty("flagLabel", true);
    flag3ThresholdSpin = new QDoubleSpinBox;
    flag3ThresholdSpin->setDecimals(10);
    flag3ThresholdSpin->setRange(-9999999.0, 9999999.0);
    flag3ThresholdSpin->setSingleStep(1.0);
    flag3ThresholdSpin->setAccelerated(true);
    flag3PolicyCombo = new QComboBox;
    flag3PolicyCombo->addItem(FLAG_POLICY_STRING_0);
    flag3PolicyCombo->addItem(FLAG_POLICY_STRING_1);

    flag4Label = new ClickLabel(tr("Flag 4 :"), this);
    flag4Label->setObjectName(QStringLiteral("flag4Label"));
    flag4Label->setToolTip(tr("<b>Flags:</b> Each column of the raw data file that was not tagged as <i>to be ignored</i> can be used as a mask to filter out individual high frequency records. Up to ten flags can be specified. Note that an entire record (that is, all variables measured at a certain time instant, one line of raw data) is eliminated any time a flag variable does not comply with its quality criterion."));
    flag4Label->setProperty("optionalField", true);
    flag4VarCombo = new QComboBox;
    flag4VarCombo->setObjectName(QStringLiteral("flag4Combo"));
    flag4VarCombo->setMaxVisibleItems(20);
    flag4VarCombo->setToolTip(flag4Label->toolTip());
    flag4UnitLabel = new QLabel;
    flag4UnitLabel->setObjectName(QStringLiteral("flag4UnitLabel"));
    flag4UnitLabel->setProperty("flagLabel", true);
    flag4ThresholdSpin = new QDoubleSpinBox;
    flag4ThresholdSpin->setDecimals(10);
    flag4ThresholdSpin->setRange(-9999999.0, 9999999.0);
    flag4ThresholdSpin->setSingleStep(1.0);
    flag4ThresholdSpin->setAccelerated(true);
    flag4PolicyCombo = new QComboBox;
    flag4PolicyCombo->addItem(FLAG_POLICY_STRING_0);
    flag4PolicyCombo->addItem(FLAG_POLICY_STRING_1);

    flag5Label = new ClickLabel(tr("Flag 5 :"), this);
    flag5Label->setObjectName(QStringLiteral("flag5Label"));
    flag5Label->setToolTip(tr("<b>Flags:</b> Each column of the raw data file that was not tagged as <i>to be ignored</i> can be used as a mask to filter out individual high frequency records. Up to ten flags can be specified. Note that an entire record (that is, all variables measured at a certain time instant, one line of raw data) is eliminated any time a flag variable does not comply with its quality criterion."));
    flag5Label->setProperty("optionalField", true);
    flag5VarCombo = new QComboBox;
    flag5VarCombo->setObjectName(QStringLiteral("flag5Combo"));
    flag5VarCombo->setMaxVisibleItems(20);
    flag5VarCombo->setToolTip(flag5Label->toolTip());
    flag5UnitLabel = new QLabel;
    flag5UnitLabel->setObjectName(QStringLiteral("flag5UnitLabel"));
    flag5UnitLabel->setProperty("flagLabel", true);
    flag5ThresholdSpin = new QDoubleSpinBox;
    flag5ThresholdSpin->setDecimals(10);
    flag5ThresholdSpin->setRange(-9999999.0, 9999999.0);
    flag5ThresholdSpin->setSingleStep(1.0);
    flag5ThresholdSpin->setAccelerated(true);
    flag5PolicyCombo = new QComboBox;
    flag5PolicyCombo->addItem(FLAG_POLICY_STRING_0);
    flag5PolicyCombo->addItem(FLAG_POLICY_STRING_1);

    flag6Label = new ClickLabel(tr("Flag 6 :"), this);
    flag6Label->setObjectName(QStringLiteral("flag6Label"));
    flag6Label->setToolTip(tr("<b>Flags:</b> Each column of the raw data file that was not tagged as <i>to be ignored</i> can be used as a mask to filter out individual high frequency records. Up to ten flags can be specified. Note that an entire record (that is, all variables measured at a certain time instant, one line of raw data) is eliminated any time a flag variable does not comply with its quality criterion."));
    flag6Label->setProperty("optionalField", true);
    flag6VarCombo = new QComboBox;
    flag6VarCombo->setObjectName(QStringLiteral("flag6Combo"));
    flag6VarCombo->setMaxVisibleItems(20);
    flag6VarCombo->setToolTip(flag6Label->toolTip());
    flag6UnitLabel = new QLabel;
    flag6UnitLabel->setObjectName(QStringLiteral("flag6UnitLabel"));
    flag6UnitLabel->setProperty("flagLabel", true);
    flag6ThresholdSpin = new QDoubleSpinBox;
    flag6ThresholdSpin->setDecimals(10);
    flag6ThresholdSpin->setRange(-9999999.0, 9999999.0);
    flag6ThresholdSpin->setSingleStep(1.0);
    flag6ThresholdSpin->setAccelerated(true);
    flag6PolicyCombo = new QComboBox;
    flag6PolicyCombo->addItem(FLAG_POLICY_STRING_0);
    flag6PolicyCombo->addItem(FLAG_POLICY_STRING_1);

    flag7Label = new ClickLabel(tr("Flag 7 :"), this);
    flag7Label->setObjectName(QStringLiteral("flag7Label"));
    flag7Label->setToolTip(tr("<b>Flags:</b> Each column of the raw data file that was not tagged as <i>to be ignored</i> can be used as a mask to filter out individual high frequency records. Up to ten flags can be specified. Note that an entire record (that is, all variables measured at a certain time instant, one line of raw data) is eliminated any time a flag variable does not comply with its quality criterion."));
    flag7Label->setProperty("optionalField", true);
    flag7VarCombo = new QComboBox;
    flag7VarCombo->setObjectName(QStringLiteral("flag7Combo"));
    flag7VarCombo->setMaxVisibleItems(20);
    flag7VarCombo->setToolTip(flag7Label->toolTip());
    flag7UnitLabel = new QLabel;
    flag7UnitLabel->setObjectName(QStringLiteral("flag7UnitLabel"));
    flag7UnitLabel->setProperty("flagLabel", true);
    flag7ThresholdSpin = new QDoubleSpinBox;
    flag7ThresholdSpin->setDecimals(10);
    flag7ThresholdSpin->setRange(-9999999.0, 9999999.0);
    flag7ThresholdSpin->setSingleStep(1.0);
    flag7ThresholdSpin->setAccelerated(true);
    flag7PolicyCombo = new QComboBox;
    flag7PolicyCombo->addItem(FLAG_POLICY_STRING_0);
    flag7PolicyCombo->addItem(FLAG_POLICY_STRING_1);

    flag8Label = new ClickLabel(tr("Flag 8 :"), this);
    flag8Label->setObjectName(QStringLiteral("flag8Label"));
    flag8Label->setToolTip(tr("<b>Flags:</b> Each column of the raw data file that was not tagged as <i>to be ignored</i> can be used as a mask to filter out individual high frequency records. Up to ten flags can be specified. Note that an entire record (that is, all variables measured at a certain time instant, one line of raw data) is eliminated any time a flag variable does not comply with its quality criterion."));
    flag8Label->setProperty("optionalField", true);
    flag8VarCombo = new QComboBox;
    flag8VarCombo->setMaxVisibleItems(20);
    flag8VarCombo->setToolTip(flag8Label->toolTip());
    flag8VarCombo->setObjectName(QStringLiteral("flag8Combo"));
    flag8UnitLabel = new QLabel;
    flag8UnitLabel->setObjectName(QStringLiteral("flag8UnitLabel"));
    flag8UnitLabel->setProperty("flagLabel", true);
    flag8ThresholdSpin = new QDoubleSpinBox;
    flag8ThresholdSpin->setDecimals(10);
    flag8ThresholdSpin->setRange(-9999999.0, 9999999.0);
    flag8ThresholdSpin->setSingleStep(1.0);
    flag8ThresholdSpin->setAccelerated(true);
    flag8PolicyCombo = new QComboBox;
    flag8PolicyCombo->addItem(FLAG_POLICY_STRING_0);
    flag8PolicyCombo->addItem(FLAG_POLICY_STRING_1);

    flag9Label = new ClickLabel(tr("Flag 9 :"), this);
    flag9Label->setObjectName(QStringLiteral("flag9Label"));
    flag9Label->setToolTip(tr("<b>Flags:</b> Each column of the raw data file that was not tagged as <i>to be ignored</i> can be used as a mask to filter out individual high frequency records. Up to ten flags can be specified. Note that an entire record (that is, all variables measured at a certain time instant, one line of raw data) is eliminated any time a flag variable does not comply with its quality criterion."));
    flag9Label->setProperty("optionalField", true);
    flag9Label->setToolTip(flag9Label->toolTip());
    flag9VarCombo = new QComboBox;
    flag9VarCombo->setObjectName(QStringLiteral("flag9Combo"));
    flag9VarCombo->setToolTip(flag9Label->toolTip());
    flag9VarCombo->setMaxVisibleItems(20);
    flag9UnitLabel = new QLabel;
    flag9UnitLabel->setObjectName(QStringLiteral("flag9UnitLabel"));
    flag9UnitLabel->setProperty("flagLabel", true);
    flag9ThresholdSpin = new QDoubleSpinBox;
    flag9ThresholdSpin->setDecimals(10);
    flag9ThresholdSpin->setRange(-9999999.0, 9999999.0);
    flag9ThresholdSpin->setSingleStep(1.0);
    flag9ThresholdSpin->setAccelerated(true);
    flag9PolicyCombo = new QComboBox;
    flag9PolicyCombo->addItem(FLAG_POLICY_STRING_0);
    flag9PolicyCombo->addItem(FLAG_POLICY_STRING_1);

    flag10Label = new ClickLabel(tr("Flag 10 :"), this);
    flag10Label->setObjectName(QStringLiteral("flag10Label"));
    flag10Label->setToolTip(tr("<b>Flags:</b> Each column of the raw data file that was not tagged as <i>to be ignored</i> can be used as a mask to filter out individual high frequency records. Up to ten flags can be specified. Note that an entire record (that is, all variables measured at a certain time instant, one line of raw data) is eliminated any time a flag variable does not comply with its quality criterion."));
    flag10Label->setProperty("optionalField", true);
    flag10Label->setToolTip(flag10Label->toolTip());
    flag10VarCombo = new QComboBox;
    flag10VarCombo->setObjectName(QStringLiteral("flag10Combo"));
    flag10VarCombo->setMaxVisibleItems(20);
    flag10VarCombo->setToolTip(flag10VarCombo->toolTip());
    flag10UnitLabel = new QLabel;
    flag10UnitLabel->setObjectName(QStringLiteral("flag10UnitLabel"));
    flag10UnitLabel->setProperty("flagLabel", true);
    flag10ThresholdSpin = new QDoubleSpinBox;
    flag10ThresholdSpin->setDecimals(10);
    flag10ThresholdSpin->setRange(-9999999.0, 9999999.0);
    flag10ThresholdSpin->setSingleStep(1.0);
    flag10ThresholdSpin->setAccelerated(true);
    flag10PolicyCombo = new QComboBox;
    flag10PolicyCombo->addItem(FLAG_POLICY_STRING_0);
    flag10PolicyCombo->addItem(FLAG_POLICY_STRING_1);

    auto varTitle_1 = new QLabel(tr("Gas, cell, and diagnostic measurements (eddy data)"));
    varTitle_1->setProperty("groupLabel", true);

    primaryInstrumentCombo = new QComboBox;
    primaryInstrumentCombo->setToolTip(
        tr("<b>Primary analyser:</b> the instrument whose gases are listed "
           "first in the output.<br><br>"
           "This changes column names and order only — no flux value "
           "depends on it. A species measured on two analysers is numbered by "
           "this order, so the primary's carbon dioxide is "
           "<i>co2_1_flux</i> and the other analyser's is <i>co2_2_flux</i>. "
           "In the FLUXNET file the primary also takes the unsuffixed "
           "standard names (<i>CO2</i>, <i>H2O</i>), and its hygrometer "
           "supplies the unsuffixed <i>H</i>, <i>LE</i> and <i>ET</i> while "
           "the others become <i>H_2</i>, <i>LE_2</i>, <i>ET_2</i>.<br><br>"
           "The mapping is written out in the <i>column_legend</i> file "
           "beside the results."));
    auto primaryInstrumentLabel = new QLabel(tr("Primary analyser:"));
    primaryInstrumentLabel->setToolTip(primaryInstrumentCombo->toolTip());
    biometRhOverrideBox = new QCheckBox(tr("Override instrument H2O measurements"));
    biometRhOverrideBox->setToolTip(
        tr("<b>Use the biomet relative humidity for every gas.</b>"
           "<p>Sets each gas's moisture source to the biomet sensor, in one "
           "step, instead of picking it per gas in the Moisture column.</p>"
           "<p>Unticking returns every gas to <i>Automatic</i>. It cannot put "
           "back what each gas named before &mdash; that is not kept &mdash; "
           "so set them again afterwards if they were not all automatic.</p>"
           "<p>The hygrometers still report what they measured. Only which "
           "humidity <i>corrects</i> each gas changes.</p>"));

    auto primaryInstrumentLayout = new QHBoxLayout;
    primaryInstrumentLayout->setContentsMargins(0, 0, 0, 0);
    primaryInstrumentLayout->setSpacing(6);
    primaryInstrumentLayout->addWidget(primaryInstrumentLabel);
    primaryInstrumentLayout->addWidget(primaryInstrumentCombo);
    primaryInstrumentLayout->addSpacing(18);
    primaryInstrumentLayout->addWidget(biometRhOverrideBox);
    primaryInstrumentLayout->addStretch();
    auto varTitle_2 = new QLabel(tr("Ambient measurements (eddy or biomet data, used for flux correction and calculation of other parameters)"));
    varTitle_2->setProperty("groupLabel", true);

    const QVector<VariableTableRow> fluxRows = {
        //> No update slot: every row in this table is driven by records now.
        //> The slots that used to sit here wrote col_co2 .. col_diag_77,
        //> which are retired - invoking them would set state nothing saves.
        //> The combo remains only as the candidate list the model reads.
        //> The first three rows are pinned to a species; the fourth takes
        //> whatever the site measured. That is the only difference between
        //> them - there is no "fourth gas" kind any more, and nothing here
        //> assumes the open slot holds N2O.
        { nullptr, nullptr, VariableTableRowKind::Gas, VariableTableRole::Co2, fluxVariableTooltip,
          QStringLiteral("co2") },
        { nullptr, nullptr, VariableTableRowKind::Gas, VariableTableRole::H2o, fluxVariableTooltip,
          QStringLiteral("h2o") },
        { nullptr, nullptr, VariableTableRowKind::Gas, VariableTableRole::Ch4, fluxVariableTooltip,
          QStringLiteral("ch4") },
        { nullptr, nullptr, VariableTableRowKind::Gas, VariableTableRole::Gas4, fluxVariableTooltip,
          QString() },
        { nullptr, nullptr, VariableTableRowKind::Cell, VariableTableRole::IntTc, fluxVariableTooltip, QString() },
        { nullptr, nullptr, VariableTableRowKind::Cell, VariableTableRole::IntT1, fluxVariableTooltip, QString() },
        { nullptr, nullptr, VariableTableRowKind::Cell, VariableTableRole::IntT2, fluxVariableTooltip, QString() },
        { nullptr, nullptr, VariableTableRowKind::Cell, VariableTableRole::IntP, fluxVariableTooltip, QString() },
        { nullptr, nullptr, VariableTableRowKind::Cell, VariableTableRole::Diag7500, diagnosticVariableTooltip, QString() },
        { nullptr, nullptr, VariableTableRowKind::Cell, VariableTableRole::Diag7200, diagnosticVariableTooltip, QString() },
        { nullptr, nullptr, VariableTableRowKind::Cell, VariableTableRole::Diag7700, diagnosticVariableTooltip, QString() }
    };
    const QVector<VariableTableRow> ambientRows = {
        { airTRefCombo, "updateAirTRefCombo", VariableTableRowKind::Ambient, VariableTableRole::AmbientT, fluxVariableTooltip, QString() },
        { airPRefCombo, "updateAirPRefCombo", VariableTableRowKind::Ambient, VariableTableRole::AmbientP, fluxVariableTooltip, QString() },
        { rhCombo, "updateRhCombo", VariableTableRowKind::Ambient, VariableTableRole::Rh, fluxVariableTooltip, QString() },
        { rgCombo, "updateRgCombo", VariableTableRowKind::Ambient, VariableTableRole::Rg, fluxVariableTooltip, QString() },
        { lwinCombo, "updateLwinCombo", VariableTableRowKind::Ambient, VariableTableRole::Lwin, fluxVariableTooltip, QString() },
        { ppfdCombo, "updatePpfdCombo", VariableTableRowKind::Ambient, VariableTableRole::Ppfd, fluxVariableTooltip, QString() }
    };

    fluxVariablesModel_ = new BasicVariableSelectionModel(this,
                                                          this,
                                                          fluxRows,
                                                          true,
                                                          tr("Instrument"),
                                                          gasMw,
                                                          gasDiff);
    ambientVariablesModel_ = new BasicVariableSelectionModel(this,
                                                             this,
                                                             ambientRows,
                                                             false,
                                                             tr("Source"));
    auto variableDelegate = new BasicVariableSelectionDelegate(this, this);
    fluxVariablesTable_ = new QTableView;
    fluxVariablesTable_->setModel(fluxVariablesModel_);
    fluxVariablesTable_->setItemDelegate(variableDelegate);
    configureBasicVariablesTable(fluxVariablesTable_);
    fluxVariablesTable_->setMinimumHeight(270);
    ambientVariablesTable_ = new QTableView;
    ambientVariablesTable_->setModel(ambientVariablesModel_);
    ambientVariablesTable_->setItemDelegate(variableDelegate);
    configureBasicVariablesTable(ambientVariablesTable_);
    ambientVariablesTable_->setMinimumHeight(165);

    const QList<QWidget*> hiddenVariableControls = {
        airTRefCombo, airPRefCombo, rhCombo, rgCombo, lwinCombo, ppfdCombo,
        moreButton, gasExtension
    };
    for (auto widget : hiddenVariableControls)
    {
        widget->setParent(this);
        widget->hide();
    }

    auto varLayout = new QVBoxLayout;
    varLayout->setContentsMargins(15, 0, 0, 0);
    varLayout->setSpacing(6);
    varLayout->addWidget(varTitle_1);
    varLayout->addLayout(primaryInstrumentLayout);
    varLayout->addWidget(fluxVariablesTable_);
    varLayout->addSpacing(6);
    varLayout->addWidget(varTitle_2);
    varLayout->addWidget(ambientVariablesTable_);
    varLayout->addStretch();

    auto descLabel = new QLabel(tr("Optionally, each variable in raw data files can be "
                      "used as a mask, to filter out individual "
                      "raw records that do not conform quality "
                      "criteria. Select the variable to be used "
                      "as a flag, and define the quality "
                      "criterion by entering a threshold value "
                      "for that variable and whether to discard "
                      "records if they are above or below the thresholds.<br>"
                      "Note: If you describe more than one flag, EddyFlow will "
                      "eliminate all records flagged by at least one test. "
                      "Note also that currently the same variable cannot "
                      "be used in two different flag definitions. "
                      "The result of such an operation is unpredictable; "
                      "most likely, only the latest flag definition with "
                      "the same variable will have an effect."));
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet(QStringLiteral("QLabel {margin-left: 15px;}"));

    auto flagLabel = new QLabel(tr("Flags"));
    flagLabel->setProperty("groupLabel", true);
    auto unitLabel = new QLabel(tr("Unit"));
    unitLabel->setProperty("groupLabel", true);
    auto thresholdLabel = new QLabel(tr("Threshold"));
    thresholdLabel->setProperty("groupLabel", true);
    auto policyLabel = new QLabel(tr("Policy: Discard if"));
    policyLabel->setProperty("groupLabel", true);

    auto descLayout = new QHBoxLayout;
    descLayout->addWidget(questionMark_2, 0);
    descLayout->addWidget(descLabel, 1);

    auto flagLayout = new QGridLayout;
    flagLayout->addLayout(descLayout, 0, 1, 1, 4);
    flagLayout->addWidget(flagLabel, 1, 1);
    flagLayout->addWidget(unitLabel, 1, 3);
    flagLayout->addWidget(thresholdLabel, 1, 2);
    flagLayout->addWidget(policyLabel, 1, 4);
    flagLayout->addWidget(flag1Label, 2, 0, Qt::AlignRight);
    flagLayout->addWidget(flag1VarCombo, 2, 1);
    flagLayout->addWidget(flag1ThresholdSpin, 2, 2);
    flagLayout->addWidget(flag1UnitLabel, 2, 3, Qt::AlignCenter);
    flagLayout->addWidget(flag1PolicyCombo, 2, 4);
    flagLayout->addWidget(flag2Label, 3, 0, Qt::AlignRight);
    flagLayout->addWidget(flag2VarCombo, 3, 1);
    flagLayout->addWidget(flag2ThresholdSpin, 3, 2);
    flagLayout->addWidget(flag2UnitLabel, 3, 3, Qt::AlignCenter);
    flagLayout->addWidget(flag2PolicyCombo, 3, 4);
    flagLayout->addWidget(flag3Label, 4, 0, Qt::AlignRight);
    flagLayout->addWidget(flag3VarCombo, 4, 1);
    flagLayout->addWidget(flag3ThresholdSpin, 4, 2);
    flagLayout->addWidget(flag3UnitLabel, 4, 3, Qt::AlignCenter);
    flagLayout->addWidget(flag3PolicyCombo, 4, 4);
    flagLayout->addWidget(flag4Label, 5, 0, Qt::AlignRight);
    flagLayout->addWidget(flag4VarCombo, 5, 1);
    flagLayout->addWidget(flag4ThresholdSpin, 5, 2);
    flagLayout->addWidget(flag4UnitLabel, 5, 3, Qt::AlignCenter);
    flagLayout->addWidget(flag4PolicyCombo, 5, 4);
    flagLayout->addWidget(flag5Label, 6, 0, Qt::AlignRight);
    flagLayout->addWidget(flag5VarCombo, 6, 1);
    flagLayout->addWidget(flag5ThresholdSpin, 6, 2);
    flagLayout->addWidget(flag5UnitLabel, 6, 3, Qt::AlignCenter);
    flagLayout->addWidget(flag5PolicyCombo, 6, 4);
    flagLayout->addWidget(flag6Label, 7, 0, Qt::AlignRight);
    flagLayout->addWidget(flag6VarCombo, 7, 1);
    flagLayout->addWidget(flag6ThresholdSpin, 7, 2);
    flagLayout->addWidget(flag6UnitLabel, 7, 3, Qt::AlignCenter);
    flagLayout->addWidget(flag6PolicyCombo, 7, 4);
    flagLayout->addWidget(flag7Label, 8, 0, Qt::AlignRight);
    flagLayout->addWidget(flag7VarCombo, 8, 1);
    flagLayout->addWidget(flag7ThresholdSpin, 8, 2);
    flagLayout->addWidget(flag7UnitLabel, 8, 3, Qt::AlignCenter);
    flagLayout->addWidget(flag7PolicyCombo, 8, 4);
    flagLayout->addWidget(flag8Label, 9, 0, Qt::AlignRight);
    flagLayout->addWidget(flag8VarCombo, 9, 1);
    flagLayout->addWidget(flag8ThresholdSpin, 9, 2);
    flagLayout->addWidget(flag8UnitLabel, 9, 3, Qt::AlignCenter);
    flagLayout->addWidget(flag8PolicyCombo, 9, 4);
    flagLayout->addWidget(flag9Label, 10, 0, Qt::AlignRight);
    flagLayout->addWidget(flag9VarCombo, 10, 1);
    flagLayout->addWidget(flag9ThresholdSpin, 10, 2);
    flagLayout->addWidget(flag9UnitLabel, 10, 3, Qt::AlignCenter);
    flagLayout->addWidget(flag9PolicyCombo, 10, 4);
    flagLayout->addWidget(flag10Label, 11, 0, Qt::AlignRight);
    flagLayout->addWidget(flag10VarCombo, 11, 1);
    flagLayout->addWidget(flag10ThresholdSpin, 11, 2);
    flagLayout->addWidget(flag10UnitLabel, 11, 3, Qt::AlignCenter);
    flagLayout->addWidget(flag10PolicyCombo, 11, 4);
    flagLayout->setVerticalSpacing(3);
    flagLayout->setContentsMargins(15, 0, 0, 0);
    flagLayout->setRowStretch(12, 1);
    flagLayout->setColumnMinimumWidth(3, 100);
    flagLayout->setColumnStretch(0, 1);
    flagLayout->setColumnStretch(1, 2);
    flagLayout->setColumnStretch(2, 1);
    flagLayout->setColumnStretch(3, 0);
    flagLayout->setColumnStretch(4, 1);
    flagLayout->setColumnStretch(5, 1);

    createWindFilterArea();

    auto varList = new QGroupBox;
    varList->setObjectName(QStringLiteral("simpleGroupBox2"));
    varList->setFlat(true);
    varList->setLayout(varLayout);

    auto flagList = new QGroupBox;
    flagList->setObjectName(QStringLiteral("simpleGroupBox2"));
    flagList->setFlat(true);
    flagList->setLayout(flagLayout);

    auto windFilterBox = new QGroupBox;
    windFilterBox->setObjectName(QStringLiteral("simpleGroupBox2"));
    windFilterBox->setFlat(true);
    windFilterBox->setLayout(windFilterLayout);

    auto varTab = new QTabWidget;
    varTab->addTab(varList, tr("Variables"));
    varTab->addTab(flagList, tr("Flags"));
    varTab->addTab(windFilterBox, tr("Wind Filter"));

    auto referenceGroupTitle = new QLabel(tr("Select Items for Flux Computation"));
    referenceGroupTitle->setProperty("groupTitle", true);

    auto varContainerLayout = new QGridLayout;
    varContainerLayout->addWidget(anemRefLabel, 0, 0, Qt::AlignRight);
    varContainerLayout->addWidget(anemRefCombo, 0, 1);
    varContainerLayout->addWidget(crossWindCheckBox, 1, 1);
    varContainerLayout->addWidget(anemFlagLabel, 2, 0, Qt::AlignRight);
    varContainerLayout->addWidget(anemFlagCombo, 2, 1);
    varContainerLayout->addWidget(tsRefLabel, 3, 0, Qt::AlignRight);
    varContainerLayout->addWidget(tsRefCombo, 3, 1);
    varContainerLayout->addWidget(varTab, 4, 0, 1, -1);
    varContainerLayout->setColumnStretch(0, 1);
    varContainerLayout->setColumnStretch(1, 2);
    varContainerLayout->setColumnStretch(2, 1);

    auto varAreaLayout = new QVBoxLayout;
    varAreaLayout->addWidget(referenceGroupTitle);
    varAreaLayout->addWidget(WidgetUtils::getContainerScrollArea(this, varContainerLayout));

    auto varArea = new QWidget;
    varArea->setLayout(varAreaLayout);

    auto splitter = new Splitter(Qt::Vertical, this);
    splitter->addWidget(WidgetUtils::getContainerScrollArea(this, filesInfoLayout));
    splitter->addWidget(varArea);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->handle(1)->setToolTip(tr("Handle the separator."));
    splitter->setContentsMargins(15, 5, 15, 0);

    smartfluxBar_ = new SmartFluxBar(ecProject_, configState_);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(smartfluxBar_);
    mainLayout->addWidget(fileGroupTitle);
    mainLayout->addWidget(splitter);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(mainLayout);

    connect(ecProject_, &EcProject::ecProjectNew,
            this, &BasicSettingsPage::reset);
    connect(ecProject_, &EcProject::ecProjectChanged,
            this, &BasicSettingsPage::refresh);
    connect(ecProject_, &EcProject::ecProjectModified,
            this, &BasicSettingsPage::updateWindFilterModel);

    connect(datapathLabel, &ClickLabel::clicked,
            this, [=](){ datapathBrowse->focusAndSelect(); });
    connect(datapathBrowse, &DirBrowseWidget::clearRequested,
            this, &BasicSettingsPage::clearDataSelection);
    connect(datapathBrowse, &DirBrowseWidget::pathChanged,
            this, &BasicSettingsPage::updateDataPath);
    connect(datapathBrowse, &DirBrowseWidget::pathSelected,
            this, &BasicSettingsPage::datapathSelected);

    connect(recursionCheckBox, &QCheckBox::toggled,
            this, &BasicSettingsPage::updateRecursion);

    connect(filePrototypeLabel, &ClickLabel::clicked,
            filePrototypeEdit, &FileFormatWidget::focusAndSelect);
    connect(filePrototypeEdit, &FileFormatWidget::pathChanged,
            this, &BasicSettingsPage::updateFilePrototype);
    connect(filePrototypeEdit->button(), &QPushButton::clicked,
            this, &BasicSettingsPage::showSetPrototype);
    connect(filePrototypeEdit, &FileFormatWidget::clearRequested,
            this, &BasicSettingsPage::clearFilePrototype);

    connect(outpathLabel, &ClickLabel::clicked,
            this, [=](){ outpathBrowse->focusAndSelect(); });
    connect(outpathBrowse, &DirBrowseWidget::pathSelected,
            this, &BasicSettingsPage::outpathBrowseSelected);
    connect(outpathBrowse, &DirBrowseWidget::pathChanged,
            this, &BasicSettingsPage::updateOutPath);

    connect(idLabel, &ClickLabel::clicked,
            this, &BasicSettingsPage::onIdLabelClicked);
    connect(idEdit, &CustomClearLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setGeneralId(s); });

    connect(avgIntervalLabel, &ClickLabel::clicked,
            this, &BasicSettingsPage::onAvgLenLabelClicked);
    connect(avgIntervalSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &BasicSettingsPage::updateAvrgLen);

    connect(maxLackLabel, &ClickLabel::clicked,
            this, &BasicSettingsPage::onMaxLackLabelClicked);
    connect(maxLackSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &BasicSettingsPage::updateMaxLack);

    connect(subsetCheckBox, &QCheckBox::toggled,
            this, &BasicSettingsPage::updateSubsetSelection);
//    connect(subsetCheckBox, &QCheckBox::toggled,
//            dateRangeDetectButton, &QPushButton::setDisabled);

    connect(dateRangeDetectButton, &QPushButton::clicked,
            this, &BasicSettingsPage::dateRangeDetect);

    connect(startDateLabel, &ClickLabel::clicked,
            this, &BasicSettingsPage::onStartDateLabelClicked);
    connect(startDateEdit, &QDateEdit::dateChanged,
            this, &BasicSettingsPage::updateStartDate);

    connect(endDateLabel, &ClickLabel::clicked,
            this, &BasicSettingsPage::onEndDateLabelClicked);
    connect(endDateEdit, &QDateEdit::dateChanged,
            this, &BasicSettingsPage::updateEndDate);
    connect(endDateEdit, &QDateEdit::dateChanged,
            this, &BasicSettingsPage::alignDeclinationDate);

    connect(startTimeEdit, &QTimeEdit::timeChanged,
            this, &BasicSettingsPage::updateStartTime);
    connect(endTimeEdit, &QTimeEdit::timeChanged,
            this, &BasicSettingsPage::updateEndTime);

    connect(anemRefLabel, &ClickLabel::clicked,
            this, &BasicSettingsPage::onClickAnemRefLabel);
    connect(anemRefCombo, &QComboBox::currentTextChanged,
            this, &BasicSettingsPage::updateAnemRefCombo);

    connect(anemFlagLabel, &ClickLabel::clicked,
            this, &BasicSettingsPage::onClickAnemFlagLabel);
    connect(anemFlagCombo, QOverload<int>::of(&QComboBox::activated),
            this, &BasicSettingsPage::updateAnemFlagCombo);




    connect(gasMw, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &BasicSettingsPage::updateGasMw);
    connect(gasDiff, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &BasicSettingsPage::updateGasDiff);






    connect(airTRefCombo, QOverload<int>::of(&QComboBox::activated),
            this, &BasicSettingsPage::updateAirTRefCombo);

    connect(airPRefCombo, QOverload<int>::of(&QComboBox::activated),
            this, &BasicSettingsPage::updateAirPRefCombo);

    //> activated, not currentIndexChanged: refreshPrimaryInstrumentCombo sets
    //> the index programmatically whenever the records change, and that must
    //> not be mistaken for the user choosing a new primary and reordering the
    //> list underneath them.
    connect(primaryInstrumentCombo, QOverload<int>::of(&QComboBox::activated),
            this, &BasicSettingsPage::onPrimaryInstrumentChanged);

    connect(biometRhOverrideBox, &QCheckBox::toggled,
            this, &BasicSettingsPage::onBiometRhOverrideToggled);

    connect(rhCombo, QOverload<int>::of(&QComboBox::activated),
            this, &BasicSettingsPage::updateRhCombo);

    connect(rgCombo, QOverload<int>::of(&QComboBox::activated),
            this, &BasicSettingsPage::updateRgCombo);

    connect(lwinCombo, QOverload<int>::of(&QComboBox::activated),
            this, &BasicSettingsPage::updateLwinCombo);

    connect(ppfdCombo, QOverload<int>::of(&QComboBox::activated),
            this, &BasicSettingsPage::updatePpfdCombo);




    connect(tsRefLabel, &ClickLabel::clicked,
            this, &BasicSettingsPage::onClickTsRefLabel);
    connect(tsRefCombo, QOverload<int>::of(&QComboBox::activated),
            this, &BasicSettingsPage::updateTsRefCombo);

    connect(crossWindCheckBox, &QCheckBox::toggled,
            this, &BasicSettingsPage::updateCrossWind);

    connect(flag1VarCombo, QOverload<int>::of(&QComboBox::activated),
            this, &BasicSettingsPage::updateFlag1Combo);
    connect(flag1ThresholdSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &BasicSettingsPage::updateFlag1Threshold);
    connect(flag1PolicyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BasicSettingsPage::updateFlag1Policy);

    connect(flag2VarCombo, QOverload<int>::of(&QComboBox::activated),
            this, &BasicSettingsPage::updateFlag2Combo);
    connect(flag2ThresholdSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &BasicSettingsPage::updateFlag2Threshold);
    connect(flag2PolicyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BasicSettingsPage::updateFlag2Policy);

    connect(flag3VarCombo, QOverload<int>::of(&QComboBox::activated),
            this, &BasicSettingsPage::updateFlag3Combo);
    connect(flag3ThresholdSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &BasicSettingsPage::updateFlag3Threshold);
    connect(flag3PolicyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BasicSettingsPage::updateFlag3Policy);

    connect(flag4VarCombo, QOverload<int>::of(&QComboBox::activated),
            this, &BasicSettingsPage::updateFlag4Combo);
    connect(flag4ThresholdSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &BasicSettingsPage::updateFlag4Threshold);
    connect(flag4PolicyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BasicSettingsPage::updateFlag4Policy);

    connect(flag5VarCombo, QOverload<int>::of(&QComboBox::activated),
            this, &BasicSettingsPage::updateFlag5Combo);
    connect(flag5ThresholdSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &BasicSettingsPage::updateFlag5Threshold);
    connect(flag5PolicyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BasicSettingsPage::updateFlag5Policy);

    connect(flag6VarCombo, QOverload<int>::of(&QComboBox::activated),
            this, &BasicSettingsPage::updateFlag6Combo);
    connect(flag6ThresholdSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &BasicSettingsPage::updateFlag6Threshold);
    connect(flag6PolicyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BasicSettingsPage::updateFlag6Policy);

    connect(flag7VarCombo, QOverload<int>::of(&QComboBox::activated),
            this, &BasicSettingsPage::updateFlag7Combo);
    connect(flag7ThresholdSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &BasicSettingsPage::updateFlag7Threshold);
    connect(flag7PolicyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BasicSettingsPage::updateFlag7Policy);

    connect(flag8VarCombo, QOverload<int>::of(&QComboBox::activated),
            this, &BasicSettingsPage::updateFlag8Combo);
    connect(flag8ThresholdSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &BasicSettingsPage::updateFlag8Threshold);
    connect(flag8PolicyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BasicSettingsPage::updateFlag8Policy);

    connect(flag9VarCombo, QOverload<int>::of(&QComboBox::activated),
            this, &BasicSettingsPage::updateFlag9Combo);
    connect(flag9ThresholdSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &BasicSettingsPage::updateFlag9Threshold);
    connect(flag9PolicyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BasicSettingsPage::updateFlag9Policy);

    connect(flag10VarCombo, QOverload<int>::of(&QComboBox::activated),
            this, &BasicSettingsPage::updateFlag10Combo);
    connect(flag10ThresholdSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &BasicSettingsPage::updateFlag10Threshold);
    connect(flag10PolicyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BasicSettingsPage::updateFlag10Policy);

    connect(moreButton, &QPushButton::toggled,
            gasExtension, &QWidget::setVisible);

    connect(northRadioGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, &BasicSettingsPage::northRadioClicked);
    connect(northRadioGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, &BasicSettingsPage::updateUseGeoNorth);

    connect(declinationLabel, &ClickLabel::clicked,
            this, &BasicSettingsPage::onClickDeclinationLabel);
    connect(declinationEdit, &QLineEdit::textChanged,
            this, &BasicSettingsPage::updateMagDec);

    connect(declinationDateLabel, &ClickLabel::clicked,
            this, &BasicSettingsPage::onDeclinationDateLabelClicked);
    connect(declinationDateEdit, &QDateEdit::dateChanged,
            this, &BasicSettingsPage::updateDeclinationDate);

    connect(declinationFetchButton, &QPushButton::clicked,
            this, &BasicSettingsPage::fetchMagneticDeclination);

    auto combo_list = QWidgetList() << flag1VarCombo
                                    << flag2VarCombo
                                    << flag3VarCombo
                                    << flag4VarCombo
                                    << flag5VarCombo
                                    << flag6VarCombo
                                    << flag7VarCombo
                                    << flag8VarCombo
                                    << flag9VarCombo
                                    << flag10VarCombo;
    for (auto widget :combo_list)
    {
        auto combo = dynamic_cast<QComboBox *>(widget);
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &BasicSettingsPage::updateFlagUnit);
    }

    auto label_list = QWidgetList() << flag1Label
                                    << flag2Label
                                    << flag3Label
                                    << flag4Label
                                    << flag5Label
                                    << flag6Label
                                    << flag7Label
                                    << flag8Label
                                    << flag9Label
                                    << flag10Label;
    for (auto widget : label_list)
    {
        auto label = dynamic_cast<ClickLabel *>(widget);
        connect(label, &ClickLabel::clicked,
                this, &BasicSettingsPage::onClickFlagLabel);
    }

    connect(smartfluxBar_, &SmartFluxBar::showSmartfluxBarRequest,
            static_cast<MainWidget *>(parent), &MainWidget::showSmartfluxBarRequest);

    connect(smartfluxBar_, &SmartFluxBar::saveSilentlyRequest,
            static_cast<MainWidget *>(parent), &MainWidget::saveSilentlyRequest);

    connect(smartfluxBar_, &SmartFluxBar::saveRequest,
            static_cast<MainWidget *>(parent), &MainWidget::saveRequest);

    // other inits
    QTimer::singleShot(0, this, &BasicSettingsPage::reset);
    QTimer::singleShot(0, this, &BasicSettingsPage::init);
}

BasicSettingsPage::~BasicSettingsPage()
{
    delete httpReply_;
    delete httpManager_;
}

void BasicSettingsPage::init()
{
    findFileProgressWidget->stopAnimation();
    magneticDeclinationFetchProgress->stopAnimation();
}

void BasicSettingsPage::datapathSelected(const QString& dir_path)
{
    // do nothing if dir is equal to the existing now
//    if (QDir::fromNativeSeparators(dir_path) == ecProject_->screenDataPath()) { return; }

    // warning dialog
    if (handleVariableReset() == QMessageBox::Cancel) { return; }

    currentRawDataList_.clear();

    FileUtils::cleanSmfDirRecursively(configState_->general.env);

    datapathBrowse->setPath(dir_path);

    WidgetUtils::rememberDialogPath(QStringLiteral("raw_data_dir"), dir_path, false);

    updateMetadataRead(true);

    if (!currentRawDataList_.isEmpty()
        || ecProject_->generalFileType() != Defs::RawFileType::GHG)
    {
        setPrototype();
    }
}

void BasicSettingsPage::showSetPrototype()
{
    bool dialogOn = true;
    setPrototype(dialogOn);
}

void BasicSettingsPage::setPrototype(bool showDialog)
{
    // GHG case
    if (ecProject_->generalFileType() == Defs::RawFileType::GHG)
    {
        // test if the raw data folder contains different file prototypes

        // get the ghg suffixes in case of raw data path selection
        // or empty raw data list
        if (currentRawDataList_.isEmpty())
        {
            updateFilesFound(ecProject_->screenRecurse());
        }

        suffixList_ = getAvailableGhgSuffixes();

        // case 1: suffixes all identical
        // then define the prototype using the std GHG timestamp
        if (suffixList_.size() == 1)
        {
            updateFilePrototypeEdit(Defs::GHG_TIMESTAMP_FORMAT + suffixList_.first());

            if (showDialog)
            {
                askRawFilenamePrototype();
            }
            updateFilesFound(ecProject_->screenRecurse());
            return;
        }
        // case 2: at least 2 different suffixes or no files found
        else
        {
            askRawFilenamePrototype();
            updateFilesFound(ecProject_->screenRecurse());
            return;
        }
    }

    // non GHG cases
    askRawFilenamePrototype();
    updateFilesFound(ecProject_->screenRecurse());
}

QStringList BasicSettingsPage::getAvailableGhgSuffixes()
{
    // progressWidget_4->startAnimation();
    QFuture<QStringList> future = QtConcurrent::run(&FileUtils::getGhgFileSuffixList, currentRawDataList_);
    while (!future.isFinished())
    {
        QCoreApplication::processEvents();
    }
    // progressWidget_4->stopAnimation();
    return future.result();
}

// search and open at least one zip file, extract and read the metadata files
// inside it. with that information populate the group of combobox
// with correct values and update the processing project
void BasicSettingsPage::captureEmbeddedMetadata(EmbeddedFileFlags type)
{
    QString ghgFormat = QStringLiteral("*.") + Defs::GHG_NATIVE_DATA_FILE_EXT;
    QString mdFormat = QStringLiteral("*.") + Defs::METADATA_FILE_EXT;
    QString biometMdFormat = QStringLiteral("*%1.%2")
                            .arg(Defs::DEFAULT_BIOMET_SUFFIX)
                            .arg(Defs::METADATA_FILE_EXT);

    findFileProgressWidget->startAnimation();
    currentRawDataList_ = FileUtils::getFiles(datapathBrowse->path(), ghgFormat, ecProject_->screenRecurse());
    findFileProgressWidget->stopAnimation();

    auto filesCount = currentRawDataList_.count();
    updateFilesFoundLabel(filesCount);
    updateProjectFilesFound(filesCount);

    QString mdFile;
    QString biometMdFile;
    bool hasMd = false;
    bool hasBiometMd = false;
    for (const auto &zipFile : currentRawDataList_)
    {
        if (!hasMd && (type & rawEmbeddedFile))
        {
            hasMd = FileUtils::zipContainsFiletype(zipFile, mdFormat);

            if (hasMd)
            {
                mdFile = zipFile;
            }
        }

        if (!hasBiometMd && (type & biometEmbeddedFile))
        {
            hasBiometMd = FileUtils::zipContainsFiletype(zipFile, biometMdFormat);

            if (hasBiometMd)
            {
                biometMdFile = zipFile;
            }
        }

        if ((type & rawEmbeddedFile) && (type & biometEmbeddedFile))
        {
            if (hasMd && hasBiometMd)
            {
                break;
            }
        }
        else if ((type & rawEmbeddedFile) && !(type & biometEmbeddedFile))
        {
            if (hasMd)
            {
                break;
            }
        }
        else if (!(type & rawEmbeddedFile) && (type & biometEmbeddedFile))
        {
            if (hasBiometMd)
            {
                break;
            }
        }
    }

    // lastEmbeddedMdFileRead_ is a caching variable
    lastEmbeddedMdFileRead_ = mdFile;

    QString homePath;
    homePath = configState_->general.env;

    // dir from where we recover extracted metadata file
    QString smfDir = homePath + QLatin1Char('/') + Defs::SMF_FILE_DIR;
    QDir mdDir(smfDir);

    if (type & rawEmbeddedFile)
    {
        if (mdFile.isEmpty())
        {
            if (this->isVisible())
            {
                WidgetUtils::warning(QApplication::activeWindow(),
                                     tr("Raw Data Missing"),
                                     tr("The selected directory doesn't "
                                        "contain any valid LI-COR GHG data."));
                clearFilesFound();
                return;
            }
            else
            {
                // NOTE: discard silently
                return;
            }
        }
        else
        {
            //> Distinguish an unreadable archive from one that simply carries no
            //> metadata. Falling through to the smf/ listing would report "no
            //> valid GHG data", or pick up a .metadata left behind by an
            //> earlier archive, since smf/ is only cleaned when the data path
            //> changes. Warn regardless of visibility: unlike "nothing found
            //> here", a corrupt archive is a hard failure.
            if (!FileUtils::zipExtract(mdFile, smfDir))
            {
                WidgetUtils::warning(QApplication::activeWindow(),
                                     tr("Raw Data Unreadable"),
                                     tr("The following LI-COR GHG file could "
                                        "not be extracted and may be corrupt:"
                                        "<p><b>%1</b></p>")
                                     .arg(QDir::toNativeSeparators(mdFile)));
                return;
            }

            QStringList mdFilters;
            mdFilters << QStringLiteral("*.") + Defs::METADATA_FILE_EXT;
            mdDir.setNameFilters(mdFilters);
            mdDir.setFilter(QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot);
            mdDir.setSorting(QDir::Name | QDir::DirsLast);

            QStringList mdFileList = mdDir.entryList();

            if (!mdFileList.isEmpty())
            {
                for (const auto &str : mdFileList)
                {
                    // raw embedded metadata
                    if (!str.contains(Defs::DEFAULT_BIOMET_SUFFIX))
                    {
                        if (type & rawEmbeddedFile)
                        {
                            mdFile = smfDir + QLatin1Char('/') + str;
                            readEmbeddedMetadata(mdFile);
                        }
                    }
                }
            }
            else
            {
                // NOTE: discard silently
            }
        }
    }

    if (type & biometEmbeddedFile)
    {
        if (biometMdFile.isEmpty())
        {
            // prevent the message in case of smartflux cfg
            if (this->isVisible() && !configState_->project.smartfluxMode)
            {
                WidgetUtils::warning(QApplication::activeWindow(),
                                     tr("Biomet Data Missing"),
                                     tr("The selected directory doesn't "
                                        "contain any valid LI-COR GHG biomet "
                                        "data."));
                // NOTE: to avoid because unexpected
                // clearFilesFound();
                return;
            }
            else
            {
                // NOTE: discard silently
                return;
            }
        }
        else
        {
            //> Same reasoning as the raw branch above: report the archive
            //> that failed rather than letting a stale or missing extraction
            //> masquerade as absent biomet metadata.
            if (!FileUtils::zipExtract(biometMdFile, smfDir))
            {
                WidgetUtils::warning(QApplication::activeWindow(),
                                     tr("Biomet Data Unreadable"),
                                     tr("The following LI-COR GHG biomet file "
                                        "could not be extracted and may be "
                                        "corrupt:<p><b>%1</b></p>")
                                     .arg(QDir::toNativeSeparators(biometMdFile)));
                return;
            }

            QStringList mdFilters;
            mdFilters << QStringLiteral("*.") + Defs::METADATA_FILE_EXT;
            mdDir.setNameFilters(mdFilters);
            mdDir.setFilter(QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot);
            mdDir.setSorting(QDir::Name | QDir::DirsLast);
            QStringList mdFileList = mdDir.entryList();

            if (!mdFileList.isEmpty())
            {
                for (const auto &str : mdFileList)
                {
                    if (str.contains(Defs::DEFAULT_BIOMET_SUFFIX))
                    {
                        if (type & biometEmbeddedFile)
                        {
                            if (!mdFileList.filter(Defs::DEFAULT_BIOMET_SUFFIX).isEmpty())
                            {
                                biometMdFile = smfDir + QLatin1Char('/') + str;
                                readBiomEmbMetadata(biometMdFile);
                            }
                        }
                    }
                }
            }
            else
            {
                // NOTE: discard silently
            }
        }
    }
}

void BasicSettingsPage::readEmbeddedMetadata(const QString& mdFile)
{
    bool modified = false;
    if (dlProject_->loadProject(mdFile, false, &modified))
    {
        parseMetadataProject(true);
    }
    else
    {
        WidgetUtils::warning(QApplication::activeWindow(),
                             tr("Metadata Error"),
                             tr("Error reading raw data "
                                "metadata information!"));
        emit updateMetadataReadResult(false);
    }
}

void BasicSettingsPage::readAlternativeMetadata(const QString& mdFile, bool firstReading)
{
    bool modified = false;
    if (dlProject_->loadProject(mdFile, true, &modified, firstReading))
    {
        parseMetadataProject(false);
    }
    else
    {
        WidgetUtils::warning(QApplication::activeWindow(),
                             tr("Metadata Error"),
                             tr("Error reading raw data "
                                "metadata information!"));
        emit updateMetadataReadResult(false);
    }
}

void BasicSettingsPage::readBiomEmbMetadata(const QString& mdFile)
{
    // clear the list if necessary
    while (!biomList_.isEmpty())
    {
        biomList_.clear();
    }

    BiomMetadataReader reader(&biomList_);

    if (reader.readEmbMetadata(mdFile))
    {
        parseBiomMetadata();
    }
    else
    {
        WidgetUtils::warning(QApplication::activeWindow(),
                             tr("Metadata Error"),
                             tr("Error reading biomet "
                                "metadata information!"));
        emit updateMetadataReadResult(false);
    }
}

bool BasicSettingsPage::readBiomAltMetadata(const QString& mdFile)
{
    // clear the list if necessary
    while (!biomList_.isEmpty())
    {
        biomList_.clear();
    }

    BiomMetadataReader reader(&biomList_);

    if (reader.readAltMetadata(mdFile))
    {
        if (!biomList_.isEmpty())
        {
            parseBiomMetadata();
            return true;
        }
        else
        {
            // NOTE: notice to put somewhere else or to silently ignore
//            WidgetUtils::warning(QApplication::activeWindow(),
//                                 tr("Metadata Error"),
//                                 tr("No biomet "
//                                    "information!"));
//            emit updateMetadataReadResult(false);
            return false;
        }
    }
    else
    {
        WidgetUtils::warning(QApplication::activeWindow(),
                             tr("Metadata Error"),
                             tr("Error reading biomet "
                                "information!"));
        return false;
    }
}

void BasicSettingsPage::parseMetadataProject(bool isEmbedded)
{
    AnemDescList *adl = dlProject_->anems();
    VariableDescList *vdl = dlProject_->variables();

    clearVarsCombo();
    clearFlagVars();
    clearFlagUnits();

    QList<QString> anemList;

    // parse anemometers
    int k = 0;
    for (const auto & anem : *adl)
    {
        ++k;
        const QString anemUiModel = anem.model();
        const QString anemDataModel = DlProject::toIniAnemModel(anemUiModel);
        const QString fullUiAnemModel = anemUiModel + tr(" [Anemometer ") + QString::number(k) + QStringLiteral("]");
        const QString fullDataAnemModel = anemDataModel + QStringLiteral("_") + QString::number(k);

        anemList << fullDataAnemModel;

        if (isEmbedded)
        {
            if (AnemDesc::isGoodAnemometer(anem))
            {
                anemRefLabel->setEnabled(true);
                anemRefCombo->setEnabled(true);
                crossWindCheckBox->setEnabled(true);
                anemRefCombo->addItem(fullUiAnemModel, fullDataAnemModel);
            }
        }
        else
        {
            if (AnemDesc::isGoodAnemometer(anem)
                && anem.hasGoodWindComponents())
            {
                anemRefLabel->setEnabled(true);
                anemRefCombo->setEnabled(true);
                crossWindCheckBox->setEnabled(true);
                anemRefCombo->addItem(fullUiAnemModel, fullDataAnemModel);
            }
        }
    }

    if (!anemList.isEmpty())
    {
        // pick the anemometer:
        // if the last master sonic is still available, just pick it
        // else, pick the first and change it accordingly
        if (anemList.contains(ecProject_->generalColMasterSonic()))
        {
            anemRefCombo->setCurrentIndex(anemList.indexOf(ecProject_->generalColMasterSonic()));
        }
        else
        {
            anemRefCombo->setCurrentIndex(0);
            ecProject_->setGeneralColMasterSonic(anemList.first());
        }
    }

    // parse described variables
    k = 0;
    for (const auto &var : *vdl)
    {
        ++k;
        const QString varName = var.variable();
        const QString instrType = var.instrument();
        QString measureType = var.measureType();

        const QString ignoreFlag = var.ignore();
        const QString numericFlag = var.numeric();

        // 1.3 condition
        bool isCustomLabel = (varName != VariableDesc::getVARIABLE_VAR_STRING_0())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_1())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_2())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_3())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_4())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_5())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_6())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_7())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_8())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_9())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_10())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_11())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_12())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_13())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_15())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_19())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_20())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_21())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_22())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_23())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_24())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_25())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_26())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_27())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_28())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_29())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_30())
                             //> Signal strength is not a custom label either.
                             //> Left out of this list, an AGC column reached
                             //> the isGoodGas branch below and was offered as
                             //> a custom fourth gas - a percentage selectable
                             //> as a species to compute a flux of.
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_35())
                             && (varName != VariableDesc::getVARIABLE_VAR_STRING_36());

        if (ignoreFlag == QLatin1String("no")
            && numericFlag == QLatin1String("yes"))
        {
            QString varString;

            // 1.1 condition
            if (!instrType.isEmpty())
            {
                // 1.2 condition
                if (!measureType.isEmpty())
                {
                    if (measureType == VariableDesc::getVARIABLE_MEASURE_TYPE_STRING_3())
                    {
                        measureType.clear();
                    }
                    else
                    {
                        measureType.append(QLatin1Char(' '));
                    }
                }

                if (instrType != tr("Other"))
                {
                    QStringList instrList = instrType.split(QLatin1Char(':'));
                    QString instrModel = instrList.at(1).trimmed();
                    QString instrStrNumber = instrList.at(0);
                    int instrNumber = instrStrNumber.mid(5).toInt();

                    QString instrTypeStr;
                    if (instrList.at(0).split(QLatin1Char(' ')).at(0) == tr("Sonic"))
                    {
                        instrTypeStr = tr("Anemometer ");
                    }
                    else if (instrList.at(0).split(QLatin1Char(' ')).at(0) == tr("Irga"))
                    {
                        instrTypeStr = tr("Gas analyzer ");
                    }
                    else
                    {
                        instrTypeStr = QLatin1Char(' ');
                    }

                    varString = varName
                                + QLatin1Char(' ')
                                + measureType
                                + tr("from ")
                                + instrModel
                                + QStringLiteral(" [")
                                + instrTypeStr
                                + QString::number(instrNumber)
                                + QStringLiteral("]");
                }
                else
                {
                    varString = varName
                                + QLatin1Char(' ')
                                + measureType
                                + tr("from other instruments");

                }
            }
            else
            {
                varString = varName
                        + tr(" from raw data files: Column # ")
                        + QString::number(k);
            }

            // gas, custom labels and cell measures section
            if (varName == VariableDesc::getVARIABLE_VAR_STRING_5()
                || varName == VariableDesc::getVARIABLE_VAR_STRING_6()
                || varName == VariableDesc::getVARIABLE_VAR_STRING_7()
                || varName == VariableDesc::getVARIABLE_VAR_STRING_8()
                || varName == VariableDesc::getVARIABLE_VAR_STRING_19()
                || varName == VariableDesc::getVARIABLE_VAR_STRING_20()
                || varName == VariableDesc::getVARIABLE_VAR_STRING_21()
                || varName == VariableDesc::getVARIABLE_VAR_STRING_22()
                || varName == VariableDesc::getVARIABLE_VAR_STRING_23()
                || varName == VariableDesc::getVARIABLE_VAR_STRING_24()
                || varName == VariableDesc::getVARIABLE_VAR_STRING_9()
                || varName == VariableDesc::getVARIABLE_VAR_STRING_10()
                || varName == VariableDesc::getVARIABLE_VAR_STRING_11()
                || varName == VariableDesc::getVARIABLE_VAR_STRING_15()
                || isCustomLabel)
            {
                const bool isCellVar = (varName == VariableDesc::getVARIABLE_VAR_STRING_9()
                                     || varName == VariableDesc::getVARIABLE_VAR_STRING_10()
                                     || varName == VariableDesc::getVARIABLE_VAR_STRING_11());
                if (!instrType.isEmpty() || isCustomLabel || isCellVar)
                {
                    // CO2
                    // 1.2 and 1.3 conditions
                    if (varName == VariableDesc::getVARIABLE_VAR_STRING_5()
                            && VariableDesc::isGoodGas(var, isCustomLabel))
                    {
                                                addCandidate(VariableTableRole::Co2, k, varString);
                    }

                    // H2O
                    else if (varName == VariableDesc::getVARIABLE_VAR_STRING_6()
                             && VariableDesc::isGoodGas(var, isCustomLabel))
                    {
                                                addCandidate(VariableTableRole::H2o, k, varString);
                    }

                    // CH4
                    else if (varName == VariableDesc::getVARIABLE_VAR_STRING_7()
                             && VariableDesc::isGoodGas(var, isCustomLabel))
                    {
                                                addCandidate(VariableTableRole::Ch4, k, varString);
                    }

                    // 4th gas o custom gas
                    else if (VariableDesc::isGoodGas(var, isCustomLabel))
                    {
                                                addCandidate(VariableTableRole::Gas4, k, varString);

                        gasMwLabel->setEnabled(true);
                        gasDiffLabel->setEnabled(true);
                        gasExtension->setVisible(true);
                        moreButton->setChecked(true);
                        updateGeometry();

                        updateFourthGasSettings(openGasSpecies());
                        //> Seed the "last species" so that merely reloading a
                        //> project does not overwrite a custom absolute-limit
                        //> minimum with the species default. Against the record
                        //> the species came from, since the seed is per record.
                        lastAbsLimitSpecies_.insert(
                            openGasRecordIndex(),
                            openGasSpecies().split(QLatin1Char(' ')).first());
                    }

                    // cell temp in
                    else if (varName == VariableDesc::getVARIABLE_VAR_STRING_9()
                        && VariableDesc::isGoodTemperature(var))
                    {
                                                addCandidate(VariableTableRole::IntT1, k, varString);
                    }

                    // cell temp out
                    else if (varName == VariableDesc::getVARIABLE_VAR_STRING_10()
                        && VariableDesc::isGoodTemperature(var))
                    {
                                                addCandidate(VariableTableRole::IntT2, k, varString);
                    }

                    // cell press 1
                    else if (varName == VariableDesc::getVARIABLE_VAR_STRING_11()
                        && VariableDesc::isGoodPressure(var))
                    {
                                                addCandidate(VariableTableRole::IntP, k, varString);
                    }

                    // avg cell temp
                    else if (varName == VariableDesc::getVARIABLE_VAR_STRING_15()
                        && VariableDesc::isGoodTemperature(var))
                    {
                                                addCandidate(VariableTableRole::IntTc, k, varString);
                    } // if
                } // if
            } // if

            // ambient temperatures and diagnostics
            else if (varName == VariableDesc::getVARIABLE_VAR_STRING_12()
                     || varName == VariableDesc::getVARIABLE_VAR_STRING_13()
                     || varName == VariableDesc::getVARIABLE_VAR_STRING_25()
                     || varName == VariableDesc::getVARIABLE_VAR_STRING_26()
                     || varName == VariableDesc::getVARIABLE_VAR_STRING_27()
                     || varName == VariableDesc::getVARIABLE_VAR_STRING_28()
                     || varName == VariableDesc::getVARIABLE_VAR_STRING_30())
            {
                // ambient temp
                if (varName == VariableDesc::getVARIABLE_VAR_STRING_12()
                    && VariableDesc::isGoodTemperature(var))
                {
                    airTRefCombo->setEnabled(true);
                    airTRefCombo->addItem(varString, k);
                }
                // fast temp
                else if (varName == VariableDesc::getVARIABLE_VAR_STRING_28()
                    && VariableDesc::isGoodTemperature(var, VariableDesc::AnalogType::FAST))
                {
                    tsRefLabel->setEnabled(true);
                    tsRefCombo->setEnabled(true);
                    tsRefCombo->addItem(varString, k);
                }
                // ambient press
                else if (varName == VariableDesc::getVARIABLE_VAR_STRING_13()
                    && VariableDesc::isGoodPressure(var))
                {
                    airPRefCombo->setEnabled(true);
                    airPRefCombo->addItem(varString, k);
                }
                // 7500A diagnostics
                else if (varName == VariableDesc::getVARIABLE_VAR_STRING_25()
                    && instrType != QLatin1String("Other"))
                {
                                        addCandidate(VariableTableRole::Diag7500, k, varString);
                }
                // 7200 diagnostics
                else if (varName == VariableDesc::getVARIABLE_VAR_STRING_26()
                    && instrType != QLatin1String("Other"))
                {
                                        addCandidate(VariableTableRole::Diag7200, k, varString);
                }
                // 7700 diagnostics
                else if (varName == VariableDesc::getVARIABLE_VAR_STRING_27()
                    && instrType != tr("Other"))
                {
                                        addCandidate(VariableTableRole::Diag7700, k, varString);
                }
                // Anem diagnostics
                else if (varName == VariableDesc::getVARIABLE_VAR_STRING_30()
                         && instrType != tr("Other"))
                {
                    anemFlagLabel->setEnabled(true);
                    anemFlagCombo->setEnabled(true);
                    anemFlagCombo->addItem(varString, k);
                }
            } // else if

            // flags
            if (!varName.isEmpty())
            {
                auto flag_vars_list = QWidgetList()
                        << flag1VarCombo
                        << flag2VarCombo
                        << flag3VarCombo
                        << flag4VarCombo
                        << flag5VarCombo
                        << flag6VarCombo
                        << flag7VarCombo
                        << flag8VarCombo
                        << flag9VarCombo
                        << flag10VarCombo;

                for (auto w : flag_vars_list)
                {
                    auto combo = dynamic_cast<QComboBox *>(w);
                    combo->setEnabled(true);
                    combo->addItem(varString, k);
                }

                auto flag_labels = QWidgetList()
                        << flag1Label
                        << flag2Label
                        << flag3Label
                        << flag4Label
                        << flag5Label
                        << flag6Label
                        << flag7Label
                        << flag8Label
                        << flag9Label
                        << flag10Label
                        << flag1ThresholdSpin
                        << flag2ThresholdSpin
                        << flag3ThresholdSpin
                        << flag4ThresholdSpin
                        << flag5ThresholdSpin
                        << flag6ThresholdSpin
                        << flag7ThresholdSpin
                        << flag8ThresholdSpin
                        << flag9ThresholdSpin
                        << flag10ThresholdSpin
                        << flag1UnitLabel
                        << flag2UnitLabel
                        << flag3UnitLabel
                        << flag4UnitLabel
                        << flag5UnitLabel
                        << flag6UnitLabel
                        << flag7UnitLabel
                        << flag8UnitLabel
                        << flag9UnitLabel
                        << flag10UnitLabel
                        << flag1PolicyCombo
                        << flag2PolicyCombo
                        << flag3PolicyCombo
                        << flag4PolicyCombo
                        << flag5PolicyCombo
                        << flag6PolicyCombo
                        << flag7PolicyCombo
                        << flag8PolicyCombo
                        << flag9PolicyCombo
                        << flag10PolicyCombo;

                for (auto w : flag_labels)
                {
                    w->setEnabled(true);
                }
            }
        } // if (ignore == no && numeric == yes)
    } // for

    addNoneStr_1();
    filterVariables();
    //> The instrument set has just been re-read, and the allowance rows are
    //> keyed by position in it. Rebuilt here rather than patched, so a
    //> reordered or shortened list cannot leave a row writing to a slot that
    //> now belongs to a different device.
    refreshInstrMaxLackRows();
    emit updateMetadataReadResult(true);
}

void BasicSettingsPage::parseBiomMetadata()
{
    clearBiometCombo();

    for (const auto &bi : biomList_)
    {
        QString varString = bi.type_
                            + tr("' from biomet files: ")
                            + tr("Column # ")
                            + QString::number(bi.col_);

        //> Asked of the whole base name rather than by substring. RG and
        //> SW_IN are the same measurement and both land on Global Radiation;
        //> LW_IN and LWIN likewise on the longwave row.
        switch (BiomMetadataReader::varType(bi.type_))
        {
            case BiomMetadataReader::VarType::AirTemperature:
                varString.prepend(tr("Ambient Temperature '"));
                airTRefCombo->setEnabled(true);
                //> Offset by 1000 to mark it as a biomet column rather than a
                //> raw one, the way this combo has always distinguished them.
                airTRefCombo->addItem(varString, bi.col_ + 1000);
                break;
            case BiomMetadataReader::VarType::AirPressure:
                varString.prepend(tr("Ambient Pressure '"));
                airPRefCombo->setEnabled(true);
                airPRefCombo->addItem(varString, bi.col_ + 1000);
                break;
            case BiomMetadataReader::VarType::RelativeHumidity:
                varString.prepend(tr("Ambient Relative Humidity '"));
                rhCombo->setEnabled(true);
                rhCombo->addItem(varString, bi.col_);
                break;
            case BiomMetadataReader::VarType::GlobalRadiation:
                varString.prepend(tr("Global Radiation '"));
                rgCombo->setEnabled(true);
                rgCombo->addItem(varString, bi.col_);
                break;
            case BiomMetadataReader::VarType::LongwaveIncoming:
                varString.prepend(tr("Longwave Incoming Radiation '"));
                lwinCombo->setEnabled(true);
                lwinCombo->addItem(varString, bi.col_);
                break;
            case BiomMetadataReader::VarType::Par:
                varString.prepend(tr("Photosynthetically Active Radiation '"));
                ppfdCombo->setEnabled(true);
                ppfdCombo->addItem(varString, bi.col_);
                break;
            case BiomMetadataReader::VarType::Unknown:
                break;
        }
    }

    addNoneStr_2();
    emit updateMetadataReadResult(true);
}

/// \fn void BasicSettingsPage::addNoneToVariables()
/// \brief Add 'None' item to variables combobox
void BasicSettingsPage::addNoneStr_1()
{
    auto combo_list = QWidgetList()
            << anemFlagCombo
            << tsRefCombo
            << flag1VarCombo
            << flag2VarCombo
            << flag3VarCombo
            << flag4VarCombo
            << flag5VarCombo
            << flag6VarCombo
            << flag7VarCombo
            << flag8VarCombo
            << flag9VarCombo
            << flag10VarCombo;

    for (auto w : combo_list)
    {
        auto combo = dynamic_cast<QComboBox *>(w);
        if (combo->findData(0) < 0)
        {
            combo->addItem(tr("None"), 0);
        }
    }

    if (ecProject_->generalUseBiomet() == 0)
    {
        addNoneStr_2();
    }
}
void BasicSettingsPage::addNoneStr_2()
{
    auto combo_list = QWidgetList()
            << airTRefCombo
            << airPRefCombo
            << rhCombo
            << rgCombo
            << lwinCombo
            << ppfdCombo;

    for (auto w : combo_list)
    {
        QComboBox *combo = dynamic_cast<QComboBox*>(w);
        if (combo->findData(0) < 0)
        {
            combo->addItem(tr("None"), 0);
        }
    }
}

void BasicSettingsPage::clearBiometCombo()
{
    auto combo_list = QWidgetList()
            << rhCombo
            << rgCombo
            << lwinCombo
            << ppfdCombo;

    for (auto w : combo_list)
    {
        QComboBox *combo = dynamic_cast<QComboBox*>(w);
        combo->clear();
        combo->setEnabled(false);
    }

    QString biometStr = QStringLiteral("biomet");
    QString noneStr = tr("None");

    for (int n = airTRefCombo->count() - 1; n >= 0; --n)
    {
        if (airTRefCombo->itemText(n).contains(biometStr))
            airTRefCombo->removeItem(n);
    }

    if (airTRefCombo->count() == 1
        && airTRefCombo->currentText().contains(noneStr))
    {
        airTRefCombo->setEnabled(false);
    }

    for (int n = airPRefCombo->count() - 1; n >= 0; --n)
    {
        if (airPRefCombo->itemText(n).contains(biometStr))
            airPRefCombo->removeItem(n);
    }

    if (airPRefCombo->count() == 1
        && airPRefCombo->currentText().contains(noneStr))
    {
        airPRefCombo->setEnabled(false);
    }
}

void BasicSettingsPage::clearVarsCombo()
{
    auto label_list = QWidgetList()
                         << anemRefLabel
                         << anemFlagLabel
                         << gasMwLabel
                         << gasDiffLabel
                         << tsRefLabel;
    for (auto widget : label_list)
    {
        auto label = dynamic_cast<QLabel *>(widget);
        label->setEnabled(false);
    }

    auto combo_list = QWidgetList()
                        << anemRefCombo
                        << anemFlagCombo
                        << airTRefCombo
                        << airPRefCombo
                        << rhCombo
                        << rgCombo
                        << lwinCombo
                        << ppfdCombo
                        << tsRefCombo;
    for (auto widget : combo_list)
    {
        auto combo = dynamic_cast<QComboBox *>(widget);
        combo->clear();
        combo->setEnabled(false);
    }

    //> The flux-table roles keep their candidates here rather than in a
    //> combo, so they are cleared alongside.
    clearCandidates();

    gasMw->setEnabled(false);
    gasDiff->setEnabled(false);
}

void BasicSettingsPage::clearFlagVars()
{
    auto label_list = QWidgetList()
            << flag1Label
            << flag2Label
            << flag3Label
            << flag4Label
            << flag5Label
            << flag6Label
            << flag7Label
            << flag8Label
            << flag9Label
            << flag10Label;

    for (auto label : label_list)
    {
        label->setEnabled(false);
    }

    auto combo_list = QWidgetList()
            << flag1VarCombo
            << flag2VarCombo
            << flag3VarCombo
            << flag4VarCombo
            << flag5VarCombo
            << flag6VarCombo
            << flag7VarCombo
            << flag8VarCombo
            << flag9VarCombo
            << flag10VarCombo;
    for (auto widget : combo_list)
    {
        auto combo = dynamic_cast<QComboBox *>(widget);
        combo->clear();
        combo->setEnabled(false);
    }
}

void BasicSettingsPage::clearFlagUnits()
{
    auto widget_list = QWidgetList()
                         << flag1UnitLabel
                         << flag2UnitLabel
                         << flag3UnitLabel
                         << flag4UnitLabel
                         << flag5UnitLabel
                         << flag6UnitLabel
                         << flag7UnitLabel
                         << flag8UnitLabel
                         << flag9UnitLabel
                         << flag10UnitLabel;
    for (auto w : widget_list)
    {
        auto label = dynamic_cast<QLabel *>(w);
        label->clear();
    }
}

void BasicSettingsPage::clearFlagThresholdsAndPolicies()
{
    auto spin_list = QWidgetList()
                        << flag1ThresholdSpin
                        << flag2ThresholdSpin
                        << flag3ThresholdSpin
                        << flag4ThresholdSpin
                        << flag5ThresholdSpin
                        << flag6ThresholdSpin
                        << flag7ThresholdSpin
                        << flag8ThresholdSpin
                        << flag9ThresholdSpin
                        << flag10ThresholdSpin;

    for (auto widget : spin_list)
    {
        auto spin = dynamic_cast<QDoubleSpinBox *>(widget);
        spin->setEnabled(false);
        spin->setValue(-9999.0);
    }

    auto label_list = QWidgetList()
                         << flag1UnitLabel
                         << flag2UnitLabel
                         << flag3UnitLabel
                         << flag4UnitLabel
                         << flag5UnitLabel
                         << flag6UnitLabel
                         << flag7UnitLabel
                         << flag8UnitLabel
                         << flag9UnitLabel
                         << flag10UnitLabel;
    for (auto widget: label_list)
    {
        auto label = dynamic_cast<QLabel *>(widget);
        label->setEnabled(false);
    }

    auto combo_list = QWidgetList()
                        << flag1PolicyCombo
                        << flag2PolicyCombo
                        << flag3PolicyCombo
                        << flag4PolicyCombo
                        << flag5PolicyCombo
                        << flag6PolicyCombo
                        << flag7PolicyCombo
                        << flag8PolicyCombo
                        << flag9PolicyCombo
                        << flag10PolicyCombo;

    for (auto widget : combo_list)
    {
        auto combo = dynamic_cast<QComboBox*>(widget);
        combo->setEnabled(false);
        combo->setCurrentIndex(1);
    }
}

/// \fn /// Drop candidates an instrument cannot actually produce.
///
/// The rules are physics, not presentation: an open-path analyser measures a
/// molar density and cannot report a mole fraction or a mixing ratio, and a
/// cell temperature or pressure only exists on a closed-path instrument.
/// Offering those combinations would let a user build a project the engine
/// cannot compute.
///
/// Matching is on the candidate label, which carries the instrument model -
/// the same text the combos used to be filtered by.
void BasicSettingsPage::filterVariables()
{
    const auto li6262Str = QStringLiteral("LI-6262");
    const auto li7000Str = QStringLiteral("LI-7000");
    const auto li7200Str = QStringLiteral("LI-7200");
    const auto li7200RSStr = QStringLiteral("LI-7200RS");
    const auto li7500Str = QStringLiteral("LI-7500");
    const auto li7500AStr = QStringLiteral("LI-7500A");
    const auto li7500RSStr = QStringLiteral("LI-7500RS");
    const auto li7500DSStr = QStringLiteral("LI-7500DS");
    const auto li7700Str = QStringLiteral("LI-7700");
    const auto fractionStr = QStringLiteral("fraction");
    const auto ratioStr = QStringLiteral("ratio");
    const auto genericStr = tr("Generic");
    const auto openPathStr1 = QStringLiteral("open");
    const auto openPathStr2 = QStringLiteral("OP");
    const auto anemStr = QStringLiteral("Anemometer");

    const auto any = [](const QString& text, std::initializer_list<QString> needles)
    {
        for (const auto& needle : needles)
        {
            if (text.contains(needle)) { return true; }
        }
        return false;
    };
    //> The LI-7500 family and the LI-7700 are open path, as is anything
    //> whose label says so.
    const auto isOpenPath = [&](const QString& t)
    {
        return any(t, { li7500Str, li7500AStr, li7500RSStr, li7500DSStr,
                        li7700Str, openPathStr1, openPathStr2 });
    };
    const auto isNotDensity = [&](const QString& t)
    {
        return any(t, { fractionStr, ratioStr });
    };

    //> An open-path analyser reports a molar density; a mole fraction or
    //> mixing ratio from one is not a measurement it can make.
    pruneCandidates(VariableTableRole::Co2, [&](const QString& t)
        { return isOpenPath(t) && isNotDensity(t); });
    pruneCandidates(VariableTableRole::H2o, [&](const QString& t)
        { return isOpenPath(t) && isNotDensity(t); });

    //> CH4 follows the same rule as CO2 and H2O: an open-path analyser
    //> reports a molar density, so a mole fraction or mixing ratio from one
    //> is not a measurement it can make. Everything else is allowed.
    //>
    //> This used to keep CH4 only from the LI-7700, an open path, or an
    //> instrument whose label said "Generic". The engine has never agreed:
    //> MetadataFileValidation accepts CH4 from the MIRO MGA series, the
    //> Aerodyne TILDAS and the Campbell EC155 and TGA200A, and this interface
    //> offers all of them as instruments. Their display strings contain none
    //> of those three words, so every CH4 column from a QCL or OA-ICOS
    //> analyser was dropped from the row - and could not be rescued through
    //> the open row either, which is reached only after CH4 has already
    //> claimed the column. A site could not build here a project the engine
    //> processes perfectly well, and this project's own regression fixtures
    //> are such a site.
    pruneCandidates(VariableTableRole::Ch4, [&](const QString& t)
        { return isOpenPath(t) && isNotDensity(t); });

    //> The fourth slot is for a gas no LI-COR analyser measures.
    pruneCandidates(VariableTableRole::Gas4, [&](const QString& t)
    {
        return any(t, { li6262Str, li7000Str, li7200Str, li7200RSStr,
                        li7500Str, li7500AStr, li7500RSStr, li7500DSStr,
                        li7700Str });
    });

    //> Cell temperatures and pressures exist only on a closed-path
    //> instrument, and not on the two oldest closed-path models.
    pruneCandidates(VariableTableRole::IntT1, [&](const QString& t)
    {
        return any(t, { li6262Str, li7000Str, li7500Str, li7500AStr,
                        li7500RSStr, li7500DSStr, li7700Str });
    });
    pruneCandidates(VariableTableRole::IntT2, [&](const QString& t)
    {
        return any(t, { li6262Str, li7000Str, li7500Str, li7500AStr,
                        li7500RSStr, li7500DSStr, li7700Str });
    });
    pruneCandidates(VariableTableRole::IntTc, [&](const QString& t)
    {
        return any(t, { li7500Str, li7500AStr, li7500RSStr, li7500DSStr,
                        li7700Str });
    });
    pruneCandidates(VariableTableRole::IntP, [&](const QString& t)
    {
        return any(t, { li7500Str, li7500AStr, li7500RSStr, li7500DSStr,
                        li7700Str });
    });

    //> A diagnostic belongs to the analyser that emits it - these keep only
    //> their own, where the others discard theirs.
    pruneCandidates(VariableTableRole::Diag7500, [&](const QString& t)
    {
        return !any(t, { li7500Str, li7500AStr, li7500RSStr, li7500DSStr });
    });
    pruneCandidates(VariableTableRole::Diag7200, [&](const QString& t)
    {
        return !any(t, { li7200Str, li7200RSStr });
    });
    pruneCandidates(VariableTableRole::Diag7700, [&](const QString& t)
    {
        return !t.contains(li7700Str);
    });

    //> The anemometer diagnostic keeps a visible combo of its own.
    //>
    //> Iterated backwards, like pruneCandidates: removing while stepping
    //> forwards skips the element after each removal, so two adjacent
    //> filterable entries left the second one in place. That was true of
    //> every one of these loops before they were converted.
    const auto noneStr = tr("None");
    for (int i = anemFlagCombo->count() - 1; i >= 0; --i)
    {
        const auto text = anemFlagCombo->itemText(i);
        if (text.contains(noneStr)) { continue; }
        if (!text.contains(anemStr)) { anemFlagCombo->removeItem(i); }
    }
}

/// Remove every candidate for \a role whose label satisfies \a drop.
///
/// "None" placeholders are never removed, and the list is walked backwards so
/// that a removal cannot skip the entry after it.
void BasicSettingsPage::pruneCandidates(
    VariableTableRole role, const std::function<bool(const QString&)>& drop)
{
    auto& items = fluxCandidates_[static_cast<int>(role)];
    const auto noneStr = tr("None");
    for (int i = items.size() - 1; i >= 0; --i)
    {
        if (items.at(i).text.contains(noneStr)) { continue; }
        if (drop(items.at(i).text)) { items.removeAt(i); }
    }
}

void BasicSettingsPage::preselectDensityVariables(QComboBox* combo)
{
    const auto densityStr = QStringLiteral("density");
    const auto ratioStr = QStringLiteral("ratio");

    // select the first density var or the first item if not present
    for (int i = 0; i < combo->count(); ++i)
    {
        if (combo->itemText(i).contains(ratioStr))
        {
            combo->setCurrentIndex(i);
            break;
        }
        else if (combo->itemText(i).contains(densityStr))
        {
            combo->setCurrentIndex(i);
        }
        else
        {
            combo->setCurrentIndex(0);
        }
    }
}

void BasicSettingsPage::preselect7700Variables(QComboBox* combo)
{
    const auto li7700Str = QStringLiteral("LI-7700");

    // preselect 7700 air temp var if present (not always possible)
    for (auto i = 0; i < combo->count(); ++i)
    {
        if (combo->itemText(i).contains(li7700Str))
        {
            combo->setCurrentIndex(i);
            break;
        }
        else
        {
            combo->setCurrentIndex(0);
        }
    }
}

void BasicSettingsPage::outpathBrowseSelected(const QString& dir_path)
{
    outpathBrowse->setPath(dir_path);

    WidgetUtils::rememberDialogPath(QStringLiteral("output_dir"), dir_path, false);
}

void BasicSettingsPage::updateDataPath(const QString& dp)
{
    ecProject_->setScreenDataPath(QDir::cleanPath(dp));
}

void BasicSettingsPage::updateOutPath(const QString& dp)
{
    ecProject_->setGeneralOutPath(QDir::cleanPath(dp));
}

void BasicSettingsPage::onAvgLenLabelClicked()
{
    avgIntervalSpin->setFocus();
    avgIntervalSpin->selectAll();
}

void BasicSettingsPage::updateRecursion(bool b)
{
    ecProject_->setScreenRecurse(b);

    QTimer::singleShot(0, this, &BasicSettingsPage::runUpdateFilesFound);
}

void BasicSettingsPage::updateSubsetSelection(bool b)
{
    ecProject_->setGeneralSubset(b);

    auto widget_list = QWidgetList()
            << startDateLabel
            << startDateEdit
            << startTimeEdit
            << lockedIcon
            << endDateLabel
            << endDateEdit
            << endTimeEdit;

    for (auto w : widget_list)
    {
        w->setEnabled(b);
    }
    if (b)
    {
        alignDeclinationDate(endDateEdit->date());
    }
}

void BasicSettingsPage::updateAvrgLen(int n)
{
    ecProject_->setScreenAvrgLen(n);
}

void BasicSettingsPage::reset()
{
    // save the modified flag to prevent side effects of setting widgets
    bool oldmod = ecProject_->modified();
    ecProject_->blockSignals(true);

    recursionCheckBox->setChecked(ecProject_->defaultSettings.screenGeneral.recurse);
    datapathBrowse->clear();
    clearFilesFound();

    filePrototypeEdit->clear();
    outpathBrowse->clear();
    idEdit->clear();

    maxLackSpin->setValue(ecProject_->defaultSettings.screenSetting.max_lack);
    avgIntervalSpin->setValue(ecProject_->defaultSettings.screenSetting.avrg_len);
    useMagneticNRadio->setChecked(!ecProject_->defaultSettings.screenGeneral.use_geo_north);

    subsetCheckBox->setChecked(ecProject_->defaultSettings.projectGeneral.subset);
    startDateLabel->setEnabled(false);
    startDateEdit->setEnabled(false);
    startTimeEdit->setEnabled(false);
    endDateLabel->setEnabled(false);
    endDateEdit->setEnabled(false);
    endTimeEdit->setEnabled(false);

    startDateEdit->setDate(QDate(2000, 1, 1));
    startTimeEdit->setTime(QTime(0, 0));
    endDateEdit->setDate(QDate::currentDate());
    endTimeEdit->setTime(QTime::currentTime());
    forceEndDatePolicy();
    forceEndTimePolicy();

    if (httpReply_)
    {
        httpReply_->abort();
    }

    declinationLabel->setEnabled(false);
    declinationEdit->setEnabled(false);
    declinationEdit->setText(strDeclination(0.0));
    declinationDateLabel->setEnabled(false);
    declinationDateEdit->setDate(QDate::fromString(ecProject_->generalEndDate(), Qt::ISODate));
    declinationDateEdit->setEnabled(false);
    declinationFetchButton->setEnabled(false);
    decChangingLabel->clear();

    crossWindCheckBox->setEnabled(false);
    crossWindCheckBox->setChecked(!ecProject_->defaultSettings.screenSetting.cross_wind);
    moreButton->setChecked(false);

    clearVarsCombo();
    clearFlagVars();
    clearFlagUnits();
    clearFlagThresholdsAndPolicies();

    windFilterApplyCheckbox->setChecked(false);
    windFilterTableModel_->clear();

    // restore modified flag
    ecProject_->setModified(oldmod);
    ecProject_->blockSignals(false);
}

void BasicSettingsPage::refresh()
{
    // save the modified flag to prevent side effects of setting widgets
    bool oldmod = ecProject_->modified();
    ecProject_->blockSignals(true);

    recursionCheckBox->setChecked(ecProject_->screenRecurse());
    clearFilesFound();

    if (FileUtils::existsPath(ecProject_->screenDataPath()))
    {
        datapathBrowse->setPath(ecProject_->screenDataPath());
//        updateFilesFound(ecProject_->screenRecurse());
    }
    else
    {
        datapathBrowse->clear();
        clearFilesFound();
        updateDataPath(QString());
    }

    filePrototypeEdit->setText(ecProject_->generalFilePrototype());

    // NOTE: we should leave non existing path, in this case,
    // but sometime it's unsafe because the engine can't create
    // subdirectory everywhere
    if (FileUtils::existsPath(ecProject_->generalOutPath()))
    {
        outpathBrowse->setPath(ecProject_->generalOutPath());
    }
    else
    {
        outpathBrowse->clear();
        updateOutPath(QString());
    }

    if (idEdit->text() != ecProject_->generalId())
    {
        idEdit->setText(ecProject_->generalId());
    }

    maxLackSpin->setValue(ecProject_->screenMaxLack());
    //> After the spin above, so the per-instrument rows are seeded from a
    //> project that has already been read.
    refreshInstrMaxLackRows();
    avgIntervalSpin->setValue(ecProject_->screenAvrgLen());

    subsetCheckBox->setChecked(ecProject_->generalSubset());
    startDateLabel->setEnabled(subsetCheckBox->isChecked());
    startDateEdit->setEnabled(subsetCheckBox->isChecked());
    startTimeEdit->setEnabled(subsetCheckBox->isChecked());
    endDateLabel->setEnabled(subsetCheckBox->isChecked());
    endDateEdit->setEnabled(subsetCheckBox->isChecked());
    endTimeEdit->setEnabled(subsetCheckBox->isChecked());

    startDateEdit->setDate(QDate::fromString(ecProject_->generalStartDate(), Qt::ISODate));
    startTimeEdit->setTime(QTime::fromString(ecProject_->generalStartTime(), QStringLiteral("hh:mm")));
    endDateEdit->setDate(QDate::fromString(ecProject_->generalEndDate(), Qt::ISODate));
    endTimeEdit->setTime(QTime::fromString(ecProject_->generalEndTime(), QStringLiteral("hh:mm")));

    crossWindCheckBox->setChecked(!ecProject_->screenCrossWind());

    if (ecProject_->screenUseGeoNorth())
    {
        useGeographicNRadio->setChecked(true);

        declinationLabel->setEnabled(true);
        declinationLabel->setToolTip(tr("<b>Magnetic Declination:</b> Based upon the latitude and longitudinal coordinates entered, EddyFlow determines the magnetic declination from the U.S. NOAA (National Oceanic and Atmospheric Organization) internet resources (U.S. National Geophysical Data Center)."));
        declinationEdit->setEnabled(true);
        declinationDateLabel->setEnabled(true);
        declinationDateEdit->setEnabled(true);
        declinationDateEdit->setDate(QDate::fromString(ecProject_->screenDecDate(), Qt::ISODate));
        declinationFetchButton->setEnabled(true);
    }
    else
    {
        useMagneticNRadio->setChecked(true);

        declinationLabel->setEnabled(false);
        declinationEdit->setEnabled(false);
        declinationDateLabel->setEnabled(false);
        declinationDateEdit->setEnabled(false);

        // NOTE: manage NOAA website API limitation, where current last day available is 2019-12-31
        // compare http://www.ngdc.noaa.gov/geomag-web/#declination
        if (ecProject_->generalSubset())
        {
            if (endDateEdit->date().year() <= 2019)
            {
                declinationDateEdit->setDate(endDateEdit->date());
            }
            else
            {
                declinationDateEdit->setDate(QDate(2019, 12, 31));
            }
        }
        else
        {
            declinationDateEdit->setDate(QDate::fromString(ecProject_->screenDecDate(), Qt::ISODate));
        }
        declinationFetchButton->setEnabled(false);
    }
    declinationEdit->setText(strDeclination(ecProject_->screenMagDec()));

    flag1ThresholdSpin->setValue(ecProject_->screenFlag1Threshold());
    flag1PolicyCombo->setCurrentIndex(ecProject_->screenFlag1Upper());
    flag2ThresholdSpin->setValue(ecProject_->screenFlag2Threshold());
    flag2PolicyCombo->setCurrentIndex(ecProject_->screenFlag2Upper());
    flag3ThresholdSpin->setValue(ecProject_->screenFlag3Threshold());
    flag3PolicyCombo->setCurrentIndex(ecProject_->screenFlag3Upper());
    flag4ThresholdSpin->setValue(ecProject_->screenFlag4Threshold());
    flag4PolicyCombo->setCurrentIndex(ecProject_->screenFlag4Upper());
    flag5ThresholdSpin->setValue(ecProject_->screenFlag5Threshold());
    flag5PolicyCombo->setCurrentIndex(ecProject_->screenFlag5Upper());
    flag6ThresholdSpin->setValue(ecProject_->screenFlag6Threshold());
    flag6PolicyCombo->setCurrentIndex(ecProject_->screenFlag6Upper());
    flag7ThresholdSpin->setValue(ecProject_->screenFlag7Threshold());
    flag7PolicyCombo->setCurrentIndex(ecProject_->screenFlag7Upper());
    flag8ThresholdSpin->setValue(ecProject_->screenFlag8Threshold());
    flag8PolicyCombo->setCurrentIndex(ecProject_->screenFlag8Upper());
    flag9ThresholdSpin->setValue(ecProject_->screenFlag9Threshold());
    flag9PolicyCombo->setCurrentIndex(ecProject_->screenFlag9Upper());
    flag10ThresholdSpin->setValue(ecProject_->screenFlag10Threshold());
    flag10PolicyCombo->setCurrentIndex(ecProject_->screenFlag10Upper());

    moreButton->setChecked(false);
    updateFourthGasSettings(openGasSpecies());
    //> From the record, which is what the file carries. These used to read
    //> generalGasMw / generalGasDiff - the project-wide keys the writer
    //> deletes - so the spin boxes came back empty after every save and the
    //> round trip was broken at both ends.
    {
        const int open = openGasRecordIndex();
        const qreal mw = gasMolecularWeight(open);
        const qreal diff = gasDiffusivity(open);
        if (mw > 0.0) { gasMw->setValue(mw); }
        if (diff > 0.0) { gasDiff->setValue(diff); }
    }

    windFilterApplyCheckbox->setChecked(ecProject_->windFilterApply());
    updateWindFilterModel();
    resizeWindFilterRows();

    // restore modified flag
    ecProject_->setModified(oldmod);
    ecProject_->blockSignals(false);
}

void BasicSettingsPage::partialRefresh()
{
    // save the modified flag to prevent side effects of setting widgets
    bool oldmod = ecProject_->modified();
    ecProject_->blockSignals(true);

    subsetCheckBox->setChecked(ecProject_->generalSubset());
    startDateLabel->setEnabled(subsetCheckBox->isChecked());
    startDateEdit->setEnabled(subsetCheckBox->isChecked());
    startTimeEdit->setEnabled(subsetCheckBox->isChecked());
    endDateLabel->setEnabled(subsetCheckBox->isChecked());
    endDateEdit->setEnabled(subsetCheckBox->isChecked());
    endTimeEdit->setEnabled(subsetCheckBox->isChecked());

    startDateEdit->setDate(QDate::fromString(ecProject_->generalStartDate(), Qt::ISODate));
    startTimeEdit->setTime(QTime::fromString(ecProject_->generalStartTime(), QStringLiteral("hh:mm")));
    endDateEdit->setDate(QDate::fromString(ecProject_->generalEndDate(), Qt::ISODate));
    endTimeEdit->setTime(QTime::fromString(ecProject_->generalEndTime(), QStringLiteral("hh:mm")));

    if (ecProject_->screenUseGeoNorth())
    {
        useGeographicNRadio->setChecked(true);

        declinationLabel->setEnabled(true);
        declinationLabel->setToolTip(tr("<b>Magnetic Declination:</b> Based upon the latitude and longitudinal coordinates entered, EddyFlow determines the magnetic declination from the U.S. NOAA (National Oceanic and Atmospheric Organization) internet resources (U.S. National Geophysical Data Center)."));
        declinationEdit->setEnabled(true);
        declinationDateLabel->setEnabled(true);
        declinationDateEdit->setEnabled(true);
        declinationDateEdit->setDate(QDate::fromString(ecProject_->screenDecDate(), Qt::ISODate));
        declinationFetchButton->setEnabled(true);
    }
    else
    {
        useMagneticNRadio->setChecked(true);

        declinationLabel->setEnabled(false);
        declinationEdit->setEnabled(false);
        declinationDateLabel->setEnabled(false);
        declinationDateEdit->setEnabled(false);

        // NOTE: manage NOAA website API limitation, where current last day available is 2019-12-31
        // compare http://www.ngdc.noaa.gov/geomag-web/#declination
        if (ecProject_->generalSubset())
        {
            if (endDateEdit->date().year() <= 2019)
            {
                declinationDateEdit->setDate(endDateEdit->date());
            }
            else
            {
                declinationDateEdit->setDate(QDate(2019, 12, 31));
            }
        }
        else
        {
            declinationDateEdit->setDate(QDate::fromString(ecProject_->screenDecDate(), Qt::ISODate));
        }
        declinationFetchButton->setEnabled(false);
    }
    declinationEdit->setText(strDeclination(ecProject_->screenMagDec()));

    // restore modified flag
    ecProject_->setModified(oldmod);
    ecProject_->blockSignals(false);
}

void BasicSettingsPage::onStartDateLabelClicked()
{
    startDateEdit->setFocus();
    WidgetUtils::showCalendarOf(startDateEdit);
}

void BasicSettingsPage::onEndDateLabelClicked()
{
    endDateEdit->setFocus();
    WidgetUtils::showCalendarOf(endDateEdit);
}

void BasicSettingsPage::updateStartDate(const QDate& d)
{
    ecProject_->setGeneralStartDate(d.toString(Qt::ISODate));
    forceEndDatePolicy();
}

void BasicSettingsPage::updateStartTime(const QTime& t)
{
    ecProject_->setGeneralStartTime(t.toString(QStringLiteral("hh:mm")));
    forceEndTimePolicy();
}

void BasicSettingsPage::updateEndDate(const QDate& d)
{
    ecProject_->setGeneralEndDate(d.toString(Qt::ISODate));
}

void BasicSettingsPage::updateEndTime(const QTime& t)
{
    ecProject_->setGeneralEndTime(t.toString(QStringLiteral("hh:mm")));
}

void BasicSettingsPage::onMaxLackLabelClicked()
{
    maxLackSpin->setFocus(Qt::ShortcutFocusReason);
    maxLackSpin->selectAll();
}

void BasicSettingsPage::updateMaxLack(int n)
{
    ecProject_->setScreenMaxLack(n);
}

// NOTE: it's correct only up to 9 anem (?), to extend
void BasicSettingsPage::updateAnemRefCombo(const QString& s)
{
    if (!s.isEmpty())
    {
        QString modelStr = s.section(QLatin1Char('['), 0, 0).trimmed();
        ecProject_->setGeneralColMasterSonic(DlProject::toIniAnemModel(modelStr)
                                      + QStringLiteral("_")
                                      + s.mid(s.indexOf(QLatin1Char(']')) - 1, 1));
    }
}

/// The anemometer diagnostic keeps a visible combo of its own, so unlike the
/// other diagnostics it is not driven by the variable table. It still stores
/// a record: col_diag_anem is retired, and writing only there would lose the
/// selection on save.
///
/// Single-valued, because a combo can name one column - any previous
/// diag_anem record is replaced rather than added to.
void BasicSettingsPage::updateAnemFlagCombo(int i)
{
    if (!ecProject_) { return; }
    const int column = anemFlagCombo->itemData(i).toInt();

    auto records = ecProject_->diagColumns();
    for (int k = records.size() - 1; k >= 0; --k)
    {
        if (records.at(k).slug == QLatin1String("diag_anem"))
        {
            records.removeAt(k);
        }
    }
    if (column > 0)
    {
        MeasurementRecord rec;
        rec.slug = QStringLiteral("diag_anem");
        rec.rawColumn = column;
        rec.instrumentId = canonicalInstrumentForColumn(column);
        records.append(rec);
    }
    ecProject_->setDiagColumns(records);
}

/// Put the anemometer diagnostic combo back on whatever its record names.
QVector<VariableCandidateItem> BasicSettingsPage::candidatesForRole(int role) const
{
    return fluxCandidates_.value(role);
}

void BasicSettingsPage::addCandidate(VariableTableRole role, int rawColumn,
                                     const QString& text)
{
    fluxCandidates_[static_cast<int>(role)].append({ rawColumn, text });
}

void BasicSettingsPage::clearCandidates()
{
    fluxCandidates_.clear();
}

void BasicSettingsPage::restoreAnemFlagFromRecord()
{
    if (!ecProject_ || !anemFlagCombo) { return; }

    int column = 0;
    for (const auto& rec : ecProject_->diagColumns())
    {
        if (rec.slug == QLatin1String("diag_anem") && rec.rawColumn > 0)
        {
            column = rec.rawColumn;
            break;
        }
    }

    const int index = anemFlagCombo->findData(column);
    anemFlagCombo->setCurrentIndex(index >= 0 ? index : 0);
}

void BasicSettingsPage::updateFourthGasSettings(const QString& s)
{
    if (s.isEmpty()) { return; }

    const QString gasStr = s.split(QLatin1Char(' ')).first();
    const GasMetadata::GasEntry* gas = GasMetadata::findGas(gasStr);

    if (!gas)
    {
        // Unknown gas: let the user fill in everything manually.
        gasMw->setEnabled(true);
        gasDiff->setEnabled(true);
        return;
    }

    switch (gas->status)
    {
        case GasMetadata::DiffusivityStatus::Reviewed:
        case GasMetadata::DiffusivityStatus::ModelBased:
        case GasMetadata::DiffusivityStatus::Calculated:
            gasMw->setEnabled(false);
            gasMw->setValue(gas->molecularWeight);
            gasDiff->setEnabled(false);
            gasDiff->setValue(gas->diffusivity);
            break;
        case GasMetadata::DiffusivityStatus::Manual:
            gasMw->setEnabled(false);
            gasMw->setValue(gas->molecularWeight);
            gasDiff->setValue(0.0);
            gasDiff->setEnabled(true);
            break;
    }
}

void BasicSettingsPage::applyGasAbsoluteLimitMin(int gasIndex,
                                                 const QString& species)
{
    if (!ecProject_) { return; }
    if (species.isEmpty()) { return; }
    const QString gasStr = species.split(QLatin1Char(' ')).first();

    //> Only apply the default when the species actually changes. If the user
    //> has set a custom value and then re-selects the same gas, the value is
    //> left alone. This used to track a single bool for "is it N2O", which
    //> could only ever distinguish two cases; the species itself is what the
    //> floor belongs to.
    //>
    //> Kept per record, not as one string: with more than one open-species row
    //> a single "last species" cannot tell which record changed, so the second
    //> row to be edited would be treated as a repeat and skipped.
    if (lastAbsLimitSpecies_.value(gasIndex) == gasStr) { return; }
    lastAbsLimitSpecies_.insert(gasIndex, gasStr);

    //> Onto the record. This used to write setScreenParamAlGas4Min() - the
    //> retired fourth-slot flat key, which the engine reads only as a legacy
    //> fallback and then overrides from gas_N_al_min. The floor was derived
    //> from the species correctly and then discarded.
    //>
    //> The whole block, not the floor alone. Every threshold in it belongs to
    //> a species, so once the species changes the rest are the previous gas's
    //> and describe nothing about this one. Setting only alMin left a record
    //> stating N2O's floor beside CO2's ceiling, discontinuity limits and
    //> time-lag window - a mixture that was never any gas's.
    //>
    //> defaultGasProcessing carries the species floor itself, so it is not
    //> applied separately here.
    auto gases = ecProject_->gasColumns();
    if (gasIndex < 0 || gasIndex >= gases.size()) { return; }
    gases[gasIndex].proc = ecProject_->defaultGasProcessing(gasStr);
    ecProject_->setGasColumns(gases);
}

void BasicSettingsPage::showGasDiffusivityWarning(const QString& species)
{
    if (species.isEmpty()) { return; }

    const QString gasStr = species.split(QLatin1Char(' ')).first();
    const GasMetadata::GasEntry* gas = GasMetadata::findGas(gasStr);
    if (!gas) { return; }

    if (gas->status == GasMetadata::DiffusivityStatus::Reviewed) { return; }

    QString title, msg;
    switch (gas->status)
    {
        case GasMetadata::DiffusivityStatus::ModelBased:
            title = tr("Model-Based Diffusivity");
            msg   = tr("The diffusivity of <b>%1</b> used by EddyFlow is a model-based estimate "
                       "(Massman 1998). It has never been directly measured in air. "
                       "You can override it in the spinbox below.").arg(gasStr);
            break;
        case GasMetadata::DiffusivityStatus::Calculated:
            title = tr("Calculated Diffusivity");
            msg   = tr("The diffusivity of <b>%1</b> was calculated using the Chapman-Enskog "
                       "equation. If you have a measured value, you can override it in the "
                       "spinbox below.").arg(gasStr);
            break;
        case GasMetadata::DiffusivityStatus::Manual:
            title = tr("Diffusivity Not Available");
            msg   = tr("Molecular weight is available for <b>%1</b>, but no diffusivity data is available. "
                       "Please enter the diffusivity in cm²/s manually in the spinbox "
                       "below.").arg(gasStr);
            break;
        case GasMetadata::DiffusivityStatus::Reviewed:
        default:
            return;
    }
    WidgetUtils::information(QApplication::activeWindow(), title, msg);
}

/// Say, once, when a project measures gases but has no humidity at all.
///
/// The engine raises the same thing as warning 104, and will keep doing so for
/// anyone running it from the command line. This exists because the console
/// output is read after a run, and this is knowable before one starts.
///
/// The two conditions cannot be identical, and that is deliberate rather than
/// an oversight. The engine tests whether biomet RH is *in range for a given
/// averaging period*, which only the data can answer. All that is visible here
/// is whether an RH column has been selected at all. So a project that selects
/// one whose data turns out to be missing is a case only the engine can
/// report - which is why its warning stays regardless.
void BasicSettingsPage::showNoHumidityWarning()
{
    if (ecProject_->gasColumns().isEmpty()) { return; }

    for (const auto& gas : ecProject_->gasColumns())
    {
        if (gas.slug.compare(QLatin1String("h2o"), Qt::CaseInsensitive) == 0)
        {
            return;
        }
    }
    //> A biomet relative humidity column is enough: the engine computes the
    //> moist-air correction from it, so this is not the case being warned about.
    if (ecProject_->biomParamColRh() > 0) { return; }

    if (noHumidityWarned_) { return; }
    noHumidityWarned_ = true;

    WidgetUtils::information(
        QApplication::activeWindow(),
        tr("No Humidity Measurement"),
        tr("<p>This project measures gases but has no humidity: no gas record "
           "is water, and no biomet relative humidity column is selected. "
           "Two things follow, and neither is visible in the output.</p>"
           "<p>Air density and heat capacity are computed for <b>dry air</b>, "
           "so density is overestimated and every density-based correction "
           "carries that bias.</p>"
           "<p>And the humidity correction to sensible heat flux cannot be "
           "applied, so the reported H is the uncorrected <b>buoyancy "
           "flux</b>, not a true sensible heat flux. Over a wet surface the "
           "two differ by several percent.</p>"
           "<p>A biomet relative humidity sensor is enough to remove both.</p>"));
}

void BasicSettingsPage::updateAirTRefCombo(int i)
{
    auto colNum = airTRefCombo->itemData(i).toInt();

    // raw data
    if (colNum > 0 && colNum < 1000)
    {
        ecProject_->setGeneralColAirT(colNum);
        ecProject_->setBiomParamColAirT(1000);
    }
    // biomet data
    else if (colNum > 1000)
    {
        ecProject_->setGeneralColAirT(0);
        ecProject_->setBiomParamColAirT(colNum);
    }
    // none (0)
    else
    {
        ecProject_->setGeneralColAirT(0);
        ecProject_->setBiomParamColAirT(1000);
    }
}

void BasicSettingsPage::updateAirPRefCombo(int i)
{
    int colNum = airPRefCombo->itemData(i).toInt();

    // raw data
    if (colNum > 0 && colNum < 1000)
    {
        ecProject_->setGeneralColAirP(colNum);
        ecProject_->setBiomParamColAirP(1000);
    }
    // biomet data
    else if (colNum > 1000)
    {
        ecProject_->setGeneralColAirP(0);
        ecProject_->setBiomParamColAirP(colNum);
    }
    // none ()
    else
    {
        ecProject_->setGeneralColAirP(0);
        ecProject_->setBiomParamColAirP(1000);
    }
}

void BasicSettingsPage::updateRhCombo(int i)
{
    auto colNum = rhCombo->itemData(i).toInt();
    ecProject_->setBiomParamColRh(colNum);

    //> No dialog here. Selecting a biomet RH column overrides nothing on its
    //> own now - it makes the biomet *available*, as a choice in the Moisture
    //> column and as the automatic fallback. The tickbox is the action with a
    //> consequence, and it carries the warning.
    //>
    //> The tables are rebuilt because the Moisture dropdown gains or loses its
    //> biomet entry with this, and the tickbox is enabled by the same test.
    refreshVariableTables();
    refreshBiometRhOverrideBox();
}

void BasicSettingsPage::updateRgCombo(int i)
{
    ecProject_->setBiomParamColRg(rgCombo->itemData(i).toInt());
}

void BasicSettingsPage::updateLwinCombo(int i)
{
    ecProject_->setBiomParamColLwin(lwinCombo->itemData(i).toInt());
}

void BasicSettingsPage::updatePpfdCombo(int i)
{
    ecProject_->setBiomParamColPpfd(ppfdCombo->itemData(i).toInt());
}

void BasicSettingsPage::updateTsRefCombo(int i)
{
    ecProject_->setGeneralColTs(tsRefCombo->itemData(i).toInt());
    emit fastTemperatureSelected();
}

void BasicSettingsPage::updateFlagUnit(int i)
{
    QComboBox* senderCombo = qobject_cast<QComboBox *>(sender());

    QString comboName = senderCombo->objectName();
    QString flagName = comboName.left(comboName.indexOf(QLatin1String("Combo")));
    QString flagUnitLabelName = flagName + QStringLiteral("UnitLabel");

    QLabel *unitLabel = this->findChild<QLabel *>(flagUnitLabelName);

    if (senderCombo->currentText() == tr("None")
        || senderCombo->currentText().isEmpty())
    {
        unitLabel->clear();
    }
    else
    {
        // check if valid index position in the list (i.e., 0 <= i < size())
        int k = senderCombo->itemData(i).toInt() - 1;
        if (k >= 0 && k < dlProject_->variables()->size())
        {
            const VariableDesc var = dlProject_->variables()->at(k);
            unitLabel->setText(getFlagUnit(var));
        }
    }
}

QString BasicSettingsPage::getFlagUnit(const VariableDesc& varStr)
{
    QString var = varStr.variable();
    QString measureType = varStr.measureType();
    QString conversionType = varStr.conversionType();
    QString inputUnitType = varStr.inputUnit();
    QString outputUnitType = varStr.outputUnit();

    if (var == VariableDesc::getVARIABLE_VAR_STRING_0()
        || var == VariableDesc::getVARIABLE_VAR_STRING_1()
        || var == VariableDesc::getVARIABLE_VAR_STRING_2()
        || var == VariableDesc::getVARIABLE_VAR_STRING_4())
    {
        return QStringLiteral("[m/s]");
    }
    else if (var == VariableDesc::getVARIABLE_VAR_STRING_3()
        || var == VariableDesc::getVARIABLE_VAR_STRING_9()
        || var == VariableDesc::getVARIABLE_VAR_STRING_10()
        || var == VariableDesc::getVARIABLE_VAR_STRING_12()
        || var == VariableDesc::getVARIABLE_VAR_STRING_15()
        || var == VariableDesc::getVARIABLE_VAR_STRING_28())
    {
        return QStringLiteral("[K]");
    }
    else if (var == VariableDesc::getVARIABLE_VAR_STRING_11()
        || var == VariableDesc::getVARIABLE_VAR_STRING_13())
    {
        return QStringLiteral("[Pa]");
    }
    else if (var == VariableDesc::getVARIABLE_VAR_STRING_5()
             || var == VariableDesc::getVARIABLE_VAR_STRING_7()
             || var == VariableDesc::getVARIABLE_VAR_STRING_8()
             || var == VariableDesc::getVARIABLE_VAR_STRING_19()
             || var == VariableDesc::getVARIABLE_VAR_STRING_20()
             || var == VariableDesc::getVARIABLE_VAR_STRING_21()
             || var == VariableDesc::getVARIABLE_VAR_STRING_22()
             || var == VariableDesc::getVARIABLE_VAR_STRING_23()
             || var == VariableDesc::getVARIABLE_VAR_STRING_24())
    {
        if (measureType == VariableDesc::getVARIABLE_MEASURE_TYPE_STRING_0())
        {
            return QStringLiteral("[%1]").arg(Defs::MMOL_M3_STRING);
        }
        else if (measureType == VariableDesc::getVARIABLE_MEASURE_TYPE_STRING_1()
                 || measureType == VariableDesc::getVARIABLE_MEASURE_TYPE_STRING_2())
        {
            return QStringLiteral("[%1]").arg(Defs::UMOL_MOL_STRING);
        }
    }
    else if (var == VariableDesc::getVARIABLE_VAR_STRING_6())
    {
        if (measureType == VariableDesc::getVARIABLE_MEASURE_TYPE_STRING_0())
        {
            return QStringLiteral("[%1]").arg(Defs::MMOL_M3_STRING);
        }
        else if (measureType == VariableDesc::getVARIABLE_MEASURE_TYPE_STRING_1()
                 || measureType == VariableDesc::getVARIABLE_MEASURE_TYPE_STRING_2())
        {
            return QStringLiteral("[%1]").arg(Defs::MMOL_MOL_STRING);
        }
    }
    else if (var == VariableDesc::getVARIABLE_VAR_STRING_29())
    {
        return QStringLiteral("[%1]").arg(Defs::M3_S_STRING);
    }
    else if (var == VariableDesc::getVARIABLE_VAR_STRING_25()
             || var == VariableDesc::getVARIABLE_VAR_STRING_26()
             || var == VariableDesc::getVARIABLE_VAR_STRING_27()
             || var == VariableDesc::getVARIABLE_VAR_STRING_30())
    {
        return QStringLiteral("[-]");
    }
    else
    {
        if (conversionType.isEmpty()
            || conversionType == VariableDesc::getVARIABLE_CONVERSION_TYPE_STRING_2())
        {
            if (!inputUnitType.isEmpty())
                return QStringLiteral("[%1]").arg(inputUnitType);
        }
        else
        {
            if (!outputUnitType.isEmpty())
                return QStringLiteral("[%1]").arg(outputUnitType);
        }
    }
    return QStringLiteral("[-]");
}

void BasicSettingsPage::updateFlag1Combo(int i)
{
    ecProject_->setScreenFlag1Col(flag1VarCombo->itemData(i).toInt());
}

void BasicSettingsPage::updateFlag2Combo(int i)
{
    ecProject_->setScreenFlag2Col(flag2VarCombo->itemData(i).toInt());
}

void BasicSettingsPage::updateFlag3Combo(int i)
{
    ecProject_->setScreenFlag3Col(flag3VarCombo->itemData(i).toInt());
}

void BasicSettingsPage::updateFlag4Combo(int i)
{
    ecProject_->setScreenFlag4Col(flag4VarCombo->itemData(i).toInt());
}

void BasicSettingsPage::updateFlag5Combo(int i)
{
    ecProject_->setScreenFlag5Col(flag5VarCombo->itemData(i).toInt());
}

void BasicSettingsPage::updateFlag6Combo(int i)
{
    ecProject_->setScreenFlag6Col(flag6VarCombo->itemData(i).toInt());
}

void BasicSettingsPage::updateFlag7Combo(int i)
{
    ecProject_->setScreenFlag7Col(flag7VarCombo->itemData(i).toInt());
}

void BasicSettingsPage::updateFlag8Combo(int i)
{
    ecProject_->setScreenFlag8Col(flag8VarCombo->itemData(i).toInt());
}

void BasicSettingsPage::updateFlag9Combo(int i)
{
    ecProject_->setScreenFlag9Col(flag9VarCombo->itemData(i).toInt());
}

void BasicSettingsPage::updateFlag10Combo(int i)
{
    ecProject_->setScreenFlag10Col(flag10VarCombo->itemData(i).toInt());
}

void BasicSettingsPage::onClickAnemRefLabel()
{
    anemRefCombo->setFocus();
    anemRefCombo->showPopup();
}

void BasicSettingsPage::onClickAnemFlagLabel()
{
    anemFlagCombo->setFocus();
    anemFlagCombo->showPopup();
}

void BasicSettingsPage::onClickTsRefLabel()
{
    tsRefCombo->setFocus();
    tsRefCombo->showPopup();
}

void BasicSettingsPage::createQuestionMark()
{
    questionMark_1 = new QPushButton;
    questionMark_2 = new QPushButton;
    questionMark_3 = new QPushButton;
    questionMark_4 = new QPushButton;

    auto btn_list = QWidgetList() << questionMark_1
                                  << questionMark_2
                                  << questionMark_3
                                  << questionMark_4;
    for (auto btn : btn_list)
    {
        btn->setObjectName(QStringLiteral("questionMarkImg"));
        static_cast<QPushButton*>(btn)->setFlat(true);
        static_cast<QPushButton*>(btn)->setIcon(QIcon(QStringLiteral(":/icons/qm-enabled")));
        static_cast<QPushButton*>(btn)->setIconSize(QSize(12, 12));
    }

    connect(questionMark_1, &QPushButton::clicked,
            this, &BasicSettingsPage::onlineHelpTrigger_2);
    connect(questionMark_2, &QPushButton::clicked,
            this, &BasicSettingsPage::onlineHelpTrigger_3);
    connect(questionMark_3, &QPushButton::clicked,
            this, &BasicSettingsPage::onlineHelpTrigger_4);
    connect(questionMark_4, &QPushButton::clicked,
            this, &BasicSettingsPage::onlineHelpTrigger_5);
}

void BasicSettingsPage::onlineHelpTrigger_2()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("http://www.licor.com/env/support/EddyFlow/topics/using-prev-results.html")));
}

void BasicSettingsPage::onlineHelpTrigger_3()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("http://www.licor.com/env/support/EddyFlow/topics/flags.html")));
}

void BasicSettingsPage::onlineHelpTrigger_4()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("http://www.licor.com/env/support/EddyFlow/topics/raw-file-name-format.html")));
}

void BasicSettingsPage::onlineHelpTrigger_5()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("http://www.licor.com/env/support/EddyFlow/topics/declination.html")));
}

void BasicSettingsPage::updateCrossWind(bool b)
{
    ecProject_->setScreenCrossWind(!b);
}

void BasicSettingsPage::updateMetadataRead(bool firstReading)
{
    // save the modified flag to prevent side effects of setting widgets
    bool oldmod = ecProject_->modified();
    ecProject_->blockSignals(true);

    // alternative metadata
    if (ecProject_->generalUseAltMdFile())
    {
        QString mdFile(ecProject_->generalMdFilepath());
        if (!mdFile.isEmpty())
        {
            if (QFile::exists(mdFile))
            {
                updateFilesFound(ecProject_->screenRecurse());
                readAlternativeMetadata(mdFile, firstReading);
                reloadSelectedItems_1();
            }
            else
            {
                ecProject_->setGeneralMdFilepath(QString());
                clearFilesFound();
            }
        }
    }
    // embedded metadata
    else
    {
        if (!datapathBrowse->path().isEmpty()
            && ecProject_->generalFileType() == Defs::RawFileType::GHG)
        {
            // re-capture metadata if dataDir exists, otherwise discard silently
            QDir dataDir(datapathBrowse->path());
            if (dataDir.exists())
            {
                captureEmbeddedMetadata(rawEmbeddedFile);
                reloadSelectedItems_1();
            }
            else
            {
                datapathBrowse->clear();
                clearFilesFound();
            }
        }
    }

    // biomet vars
    switch (ecProject_->generalUseBiomet())
    {
        case 0:
            break;
        case 1:
            if (!datapathBrowse->path().isEmpty()
                && ecProject_->generalFileType() == Defs::RawFileType::GHG)
            {
                // re-capture metadata if dataDir exists, otherwise discard silently
                QDir dataDir(datapathBrowse->path());
                if (dataDir.exists())
                {
                    captureEmbeddedMetadata(biometEmbeddedFile);
                    reloadSelectedItems_2();
                }
            }
            break;
        case 2:
        {
            QString biomDataFile = ecProject_->generalBiomFile();
            if (!biomDataFile.isEmpty())
            {
                if (QFile::exists(biomDataFile))
                {
                    if (readBiomAltMetadata(biomDataFile))
                    {
                        reloadSelectedItems_2();
                    }
                }
            }
            break;
        }
        case 3:
        {
            QStringList biomFileList = FileUtils::getFiles(ecProject_->generalBiomDir(),
                                                      QStringLiteral("*.") + ecProject_->generalBiomExt(),
                                                      ecProject_->generalBiomRecurse());

            for (const auto &file : biomFileList)
            {
                if (readBiomAltMetadata(file))
                {
                    reloadSelectedItems_2();
                    break;
                }
                else
                {
                    continue;
                }
            }
            break;
        }
        default:
            break;
    }

    // restore modified flag
    ecProject_->setModified(oldmod);
    ecProject_->blockSignals(false);
}

void BasicSettingsPage::reloadSelectedItems_1()
{
    addNoneStr_1();

    QString currDataStr = ecProject_->generalColMasterSonic();
    int currItemIndex = anemRefCombo->findData(currDataStr);
    QString selectedDataStr = WidgetUtils::currentComboItemData(anemRefCombo).toString();
    int selectedItemIndex = anemRefCombo->findData(selectedDataStr);

    if (currDataStr.isEmpty() && !selectedDataStr.isEmpty())
    {
        anemRefCombo->setCurrentIndex(selectedItemIndex);
        ecProject_->setGeneralColMasterSonic(selectedDataStr);
    }
    else if (!currDataStr.isEmpty() && currItemIndex != -1)
    {
        anemRefCombo->setCurrentIndex(currItemIndex);
        ecProject_->setGeneralColMasterSonic(currDataStr);
    }
    else
    {
        anemRefCombo->setCurrentIndex(0);
        ecProject_->setGeneralColMasterSonic(anemRefCombo->itemData(0).toString());
    }
//
    int currData = ecProject_->generalColTs();
    currItemIndex = tsRefCombo->findData(currData);
    if (currItemIndex >= 0)
    {
        tsRefCombo->setCurrentIndex(currItemIndex);
        ecProject_->setGeneralColTs(currData);
    }
    else
    {
        // preselect something better
        preselect7700Variables(tsRefCombo);
        ecProject_->setGeneralColTs(WidgetUtils::currentComboItemData(tsRefCombo).toInt());
    }
//
    //> The stanzas that used to sit here read col_co2 .. col_diag_anem back
    //> out of the project and wrote them into the hidden combos, which is
    //> how those keys were kept alive. All of it is retired: the variable
    //> table drives records directly, and reloadSelectedItems_1 no longer
    //> has anything to reload for these roles.
    //>
    //> The anemometer diagnostic keeps its own visible combo, so it is
    //> restored from its record rather than from col_diag_anem.
    restoreAnemFlagFromRecord();
//
    if (ecProject_->generalUseBiomet() == 0)
    {
        currData = ecProject_->generalColAirT();
        currItemIndex = airTRefCombo->findData(currData);

        if (currItemIndex >= 0)
        {
            airTRefCombo->setCurrentIndex(currItemIndex);
            ecProject_->setGeneralColAirT(currData);
        }
        else
        {
            // select something better
            preselect7700Variables(airTRefCombo);
            updateAirTRefCombo(airTRefCombo->currentIndex());
        }
    //
        currData = ecProject_->generalColAirP();
        currItemIndex = airPRefCombo->findData(currData);
        if (currItemIndex >= 0)
        {
            airPRefCombo->setCurrentIndex(currItemIndex);
            ecProject_->setGeneralColAirP(currData);
        }
        else
        {
            // select something better
            preselect7700Variables(airPRefCombo);
            updateAirPRefCombo(airPRefCombo->currentIndex());
        }
    }
    //
    currData = ecProject_->screenFlag1Col();
    currItemIndex = flag1VarCombo->findData(currData);
    auto noneIndex = flag1VarCombo->findData(0);

    if (currItemIndex >= 0)
    {
        flag1VarCombo->setCurrentIndex(currItemIndex);
        ecProject_->setScreenFlag1Col(currData);
    }
    else
    {
        flag1VarCombo->setCurrentIndex(noneIndex);
        ecProject_->setScreenFlag1Col(0);
    }
//
    currData = ecProject_->screenFlag2Col();
    currItemIndex = flag2VarCombo->findData(currData);
    noneIndex = flag2VarCombo->findData(0);
    if (currItemIndex >= 0)
    {
        flag2VarCombo->setCurrentIndex(currItemIndex);
        ecProject_->setScreenFlag2Col(currData);
    }
    else
    {
        flag2VarCombo->setCurrentIndex(noneIndex);
        ecProject_->setScreenFlag2Col(0);
    }
//
    currData = ecProject_->screenFlag3Col();
    currItemIndex = flag3VarCombo->findData(currData);
    noneIndex = flag3VarCombo->findData(0);
    if (currItemIndex >= 0)
    {
        flag3VarCombo->setCurrentIndex(currItemIndex);
        ecProject_->setScreenFlag3Col(currData);
    }
    else
    {
        flag3VarCombo->setCurrentIndex(noneIndex);
        ecProject_->setScreenFlag3Col(0);
    }
//
    currData = ecProject_->screenFlag4Col();
    currItemIndex = flag4VarCombo->findData(currData);
    noneIndex = flag4VarCombo->findData(0);
    if (currItemIndex >= 0)
    {
        flag4VarCombo->setCurrentIndex(currItemIndex);
        ecProject_->setScreenFlag4Col(currData);
    }
    else
    {
        flag4VarCombo->setCurrentIndex(noneIndex);
        ecProject_->setScreenFlag4Col(0);
    }
//
    currData = ecProject_->screenFlag5Col();
    currItemIndex = flag5VarCombo->findData(currData);
    noneIndex = flag5VarCombo->findData(0);
    if (currItemIndex >= 0)
    {
        flag5VarCombo->setCurrentIndex(currItemIndex);
        ecProject_->setScreenFlag5Col(currData);
    }
    else
    {
        flag5VarCombo->setCurrentIndex(noneIndex);
        ecProject_->setScreenFlag5Col(0);
    }
//
    currData = ecProject_->screenFlag6Col();
    currItemIndex = flag6VarCombo->findData(currData);
    noneIndex = flag6VarCombo->findData(0);
    if (currItemIndex >= 0)
    {
        flag6VarCombo->setCurrentIndex(currItemIndex);
        ecProject_->setScreenFlag6Col(currData);
    }
    else
    {
        flag6VarCombo->setCurrentIndex(noneIndex);
        ecProject_->setScreenFlag6Col(0);
    }
//
    currData = ecProject_->screenFlag7Col();
    currItemIndex = flag7VarCombo->findData(currData);
    noneIndex = flag7VarCombo->findData(0);
    if (currItemIndex >= 0)
    {
        flag7VarCombo->setCurrentIndex(currItemIndex);
        ecProject_->setScreenFlag7Col(currData);
    }
    else
    {
        flag7VarCombo->setCurrentIndex(noneIndex);
        ecProject_->setScreenFlag7Col(0);
    }
//
    currData = ecProject_->screenFlag8Col();
    currItemIndex = flag8VarCombo->findData(currData);
    noneIndex = flag8VarCombo->findData(0);
    if (currItemIndex >= 0)
    {
        flag8VarCombo->setCurrentIndex(currItemIndex);
        ecProject_->setScreenFlag8Col(currData);
    }
    else
    {
        flag8VarCombo->setCurrentIndex(noneIndex);
        ecProject_->setScreenFlag8Col(0);
    }
//
    currData = ecProject_->screenFlag9Col();
    currItemIndex = flag9VarCombo->findData(currData);
    noneIndex = flag9VarCombo->findData(0);
    if (currItemIndex >= 0)
    {
        flag9VarCombo->setCurrentIndex(currItemIndex);
        ecProject_->setScreenFlag9Col(currData);
    }
    else
    {
        flag9VarCombo->setCurrentIndex(noneIndex);
        ecProject_->setScreenFlag9Col(0);
    }
//
    currData = ecProject_->screenFlag10Col();
    currItemIndex = flag10VarCombo->findData(currData);
    noneIndex = flag10VarCombo->findData(0);
    if (currItemIndex >= 0)
    {
        flag10VarCombo->setCurrentIndex(currItemIndex);
        ecProject_->setScreenFlag10Col(currData);
    }
    else
    {
        flag10VarCombo->setCurrentIndex(noneIndex);
        ecProject_->setScreenFlag10Col(0);
    }

    flag1ThresholdSpin->setValue(ecProject_->screenFlag1Threshold());
    flag1PolicyCombo->setCurrentIndex(ecProject_->screenFlag1Upper());
    flag2ThresholdSpin->setValue(ecProject_->screenFlag2Threshold());
    flag2PolicyCombo->setCurrentIndex(ecProject_->screenFlag2Upper());
    flag3ThresholdSpin->setValue(ecProject_->screenFlag3Threshold());
    flag3PolicyCombo->setCurrentIndex(ecProject_->screenFlag3Upper());
    flag4ThresholdSpin->setValue(ecProject_->screenFlag4Threshold());
    flag4PolicyCombo->setCurrentIndex(ecProject_->screenFlag4Upper());
    flag5ThresholdSpin->setValue(ecProject_->screenFlag5Threshold());
    flag5PolicyCombo->setCurrentIndex(ecProject_->screenFlag5Upper());
    flag6ThresholdSpin->setValue(ecProject_->screenFlag6Threshold());
    flag6PolicyCombo->setCurrentIndex(ecProject_->screenFlag6Upper());
    flag7ThresholdSpin->setValue(ecProject_->screenFlag7Threshold());
    flag7PolicyCombo->setCurrentIndex(ecProject_->screenFlag7Upper());
    flag8ThresholdSpin->setValue(ecProject_->screenFlag8Threshold());
    flag8PolicyCombo->setCurrentIndex(ecProject_->screenFlag8Upper());
    flag9ThresholdSpin->setValue(ecProject_->screenFlag9Threshold());
    flag9PolicyCombo->setCurrentIndex(ecProject_->screenFlag9Upper());
    flag10ThresholdSpin->setValue(ecProject_->screenFlag10Threshold());
    flag10PolicyCombo->setCurrentIndex(ecProject_->screenFlag10Upper());

    //> Seed before resolving: a project that has never had records gets its
    //> auto-selection here, and one that was migrated gets its species and
    //> instrument filled in there. Neither touches the other's work.
    seedGasRecordsFromMetadata();
    resolveMigratedGasRecords();
    //> Before the instruments are synced, so a record that is about to be
    //> dropped is not first given an analyser, and before the variable tables
    //> are built, so a row cannot be drawn from a record that is going.
    pruneStaleNonGasRecords();
    syncSignalStrengthRecords();
    //> Third of the same kind, and here for the reason the other two are:
    //> updateMetadataRead is invoked through a synchronous request before
    //> anything is written - see MainWindow::upgradeProjectInPlace - so a
    //> record resolved here is resolved whether or not the user opens this
    //> page. That guarantee is the point. The comment there records what
    //> happened the last time something in this family ran only when the user
    //> happened to visit Basic Settings first.
    syncNonGasRecordInstruments();

    refreshVariableTables();

    //> After the records are settled: the list of analysers to choose from is
    //> exactly the set the gas records name, and that is only known now.
    refreshPrimaryInstrumentCombo();
    refreshInstrMaxLackRows();
    refreshBiometRhOverrideBox();

    //> After the records are settled, so the check sees the project
    //> as it will be processed rather than mid-selection.
    showNoHumidityWarning();
}

/// Auto-select plausible measurements the first time a metadata file is read.
///
/// This is what preselectDensityVariables() used to do, moved onto the
/// records. The old version wrote its choice into col_co2 and friends while
/// the variable table's checkboxes read the records - so **the preselection
/// was invisible**: a freshly parsed metadata file preselected CO2 and H2O
/// into state the user could not see and had not asked for. Seeding records
/// preserves the behaviour and finally shows it.
///
/// Only runs on an empty record list, so it can never overwrite a user's
/// selection or a migrated project's.
void BasicSettingsPage::seedGasRecordsFromMetadata()
{
    if (!ecProject_ || !dlProject_) { return; }
    if (!ecProject_->gasColumns().isEmpty()) { return; }

    const auto* variables = dlProject_->variables();
    if (!variables || variables->isEmpty()) { return; }

    //> A mixing ratio is preferred over a mole fraction, and either over a
    //> density - the same order the combo-text match ("ratio", then
    //> "density") produced, stated against the measure type rather than
    //> against a translated label.
    const auto rank = [](const VariableDesc& var)
    {
        const auto type = var.measureType();
        if (type == VariableDesc::getVARIABLE_MEASURE_TYPE_STRING_2()) { return 0; }
        if (type == VariableDesc::getVARIABLE_MEASURE_TYPE_STRING_1()) { return 1; }
        if (type == VariableDesc::getVARIABLE_MEASURE_TYPE_STRING_0()) { return 2; }
        return 3;
    };

    //> The best candidate for one species, as a 1-based raw column.
    const auto bestColumn = [&](const QString& speciesName, bool anyGas)
    {
        int column = -1;
        int best = 100;
        for (int i = 0; i < variables->size(); ++i)
        {
            const auto& var = variables->at(i);
            const auto name = var.variable();
            const bool custom = VariableDesc::isCustomVariable(name);
            if (!VariableDesc::isGoodGas(var, custom)) { continue; }
            if (anyGas)
            {
                //> The fourth slot took whatever gas was left over - any
                //> good gas that is not one of the first three.
                if (name == VariableDesc::getVARIABLE_VAR_STRING_5()
                    || name == VariableDesc::getVARIABLE_VAR_STRING_6()
                    || name == VariableDesc::getVARIABLE_VAR_STRING_7())
                {
                    continue;
                }
            }
            else if (name != speciesName)
            {
                continue;
            }
            if (rank(var) < best) { best = rank(var); column = i + 1; }
        }
        return column;
    };

    QVector<GasRecord> gases;
    const struct { QString name; bool anyGas; QString slug; } roles[] = {
        { VariableDesc::getVARIABLE_VAR_STRING_5(), false, QStringLiteral("co2") },
        { VariableDesc::getVARIABLE_VAR_STRING_6(), false, QStringLiteral("h2o") },
        { VariableDesc::getVARIABLE_VAR_STRING_7(), false, QStringLiteral("ch4") },
        { QString(),                                true,  QString() },
    };
    bool anyFound = false;
    for (const auto& role : roles)
    {
        //> Only the gases the site actually has. All four used to be appended
        //> whatever the metadata said, so that record i stayed the engine's
        //> slot firstGas+i-1 - and an absent one still reached every output as
        //> a column of error codes. Records name their own species now, so a
        //> gas that is not measured simply has no record.
        GasRecord rec;
        rec.rawColumn = bestColumn(role.name, role.anyGas);
        if (rec.rawColumn <= 0) { continue; }

        anyFound = true;
        const auto& var = variables->at(rec.rawColumn - 1);
        rec.slug = role.anyGas
            ? GasMetadata::normaliseFormula(var.variable())
            : role.slug;
        rec.instrumentId = canonicalInstrumentForColumn(rec.rawColumn);
        //> The same seeding addGasRecord does, for the same reason. This is the
        //> other way a record comes into existence - the preselection that runs
        //> when a metadata file is read - and it left proc at the -1 sentinel.
        //> An unset setting is written as no key at all, so those records
        //> reached the engine with no absolute limits, no spike limit, no
        //> discontinuity limits and no time-lag windows, and the engine
        //> declined every test that needs them.
        //>
        //> The roles table above is co2, h2o, ch4 and one other, so it was
        //> exactly the site's main gases that lost their settings while the
        //> ones the user ticked by hand kept theirs. The pages show species
        //> defaults for a sentinel field, so nothing on screen said so.
        rec.proc = ecProject_->defaultGasProcessing(rec.slug);
        gases.append(rec);
    }

    if (!anyFound) { return; }
    ecProject_->setGasColumns(gases);
}

/// Fill in what the project file alone could not say about a migrated project.
///
/// A pre-record file names a raw column number and nothing else, so
/// migrateLegacyColumnsToRecords leaves every species and instrument blank -
/// most damagingly for the fourth slot, whose species was never written down
/// anywhere but the metadata. This runs after every parseMetadataProject()
/// and every project load, which is the only point where both files are in
/// hand, and must run before the upgraded project is saved.
void BasicSettingsPage::resolveMigratedGasRecords()
{
    if (!ecProject_ || !dlProject_) { return; }

    const auto* variables = dlProject_->variables();
    if (!variables) { return; }

    auto gases = ecProject_->gasColumns();
    bool changed = false;

    for (auto& gas : gases)
    {
        if (gas.rawColumn <= 0) { continue; }
        if (!gas.slug.isEmpty() && !gas.instrumentId.isEmpty()) { continue; }

        const int index = gas.rawColumn - 1;
        if (index < 0 || index >= variables->size()) { continue; }
        const auto& variable = variables->at(index);

        if (gas.slug.isEmpty())
        {
            // The engine matches on the ASCII slug, so it has to come through
            // normaliseFormula: several species carry Unicode subscripts in
            // their display strings.
            const auto slug =
                GasMetadata::normaliseFormula(variable.variable());
            if (!slug.isEmpty())
            {
                gas.slug = slug;
                changed = true;
            }
        }
        if (gas.instrumentId.isEmpty())
        {
            const auto id =
                dlProject_->canonicalInstrumentId(variable.instrument());
            if (!id.isEmpty())
            {
                gas.instrumentId = id;
                changed = true;
            }
        }
    }

    if (changed) { ecProject_->setGasColumns(gases); }
}

void BasicSettingsPage::reloadSelectedItems_2()
{
    addNoneStr_1();

    int currData = ecProject_->generalColAirT();
    int currItemIndex = airTRefCombo->findData(currData);
    int currData2 = ecProject_->biomParamColAirT();
    int currItemIndex2 = airTRefCombo->findData(currData2);
    int noneIndex = airTRefCombo->findData(0);

    if (currData > 0 && currItemIndex != -1)
    {
        airTRefCombo->setCurrentIndex(currItemIndex);
        ecProject_->setGeneralColAirT(currData);
        ecProject_->setBiomParamColAirT(1000);
    }
    else if (currData2 > 1000 && currItemIndex2 != -1)
    {
        airTRefCombo->setCurrentIndex(currItemIndex2);
        ecProject_->setGeneralColAirT(0);
        ecProject_->setBiomParamColAirT(currData2);
    }
    else if (currData == 0 && currData2 == 1000)
    {
        // select none
        airTRefCombo->setCurrentIndex(noneIndex);
        ecProject_->setGeneralColAirT(0);
        ecProject_->setBiomParamColAirT(1000);
    }
    else
    {
        // select something better
        preselect7700Variables(airTRefCombo);
        updateAirTRefCombo(airTRefCombo->currentIndex());
    }
//
    currData = ecProject_->generalColAirP();
    currItemIndex = airPRefCombo->findData(currData);
    currData2 = ecProject_->biomParamColAirP();
    currItemIndex2 = airPRefCombo->findData(currData2);
    noneIndex = airPRefCombo->findData(0);

    if (currData > 0)
    {
        airPRefCombo->setCurrentIndex(currItemIndex);
        ecProject_->setGeneralColAirP(currData);
        ecProject_->setBiomParamColAirP(1000);
    }
    else if (currData2 > 1000)
    {
        airPRefCombo->setCurrentIndex(currItemIndex2);
        ecProject_->setGeneralColAirP(0);
        ecProject_->setBiomParamColAirP(currData2);
    }
    else if (currData == 0 && currData2 == 1000)
    {
        // select none
        airPRefCombo->setCurrentIndex(noneIndex);
        ecProject_->setGeneralColAirP(0);
        ecProject_->setBiomParamColAirP(1000);
    }
    else
    {
        // select something better
        preselect7700Variables(airPRefCombo);
        updateAirPRefCombo(airPRefCombo->currentIndex());
    }
//
    currData = ecProject_->biomParamColRh();
    currItemIndex = rhCombo->findData(currData);
    int firstIndexData = rhCombo->itemData(0).toInt();
    if (currItemIndex >= 0)
    {
        rhCombo->setCurrentIndex(currItemIndex);
        ecProject_->setBiomParamColRh(currData);
    }
    else
    {
        rhCombo->setCurrentIndex(0);
        ecProject_->setBiomParamColRh(firstIndexData);
    }
//
    currData = ecProject_->biomParamColRg();
    currItemIndex = rgCombo->findData(currData);
    firstIndexData = rgCombo->itemData(0).toInt();
    if (currItemIndex >= 0)
    {
        rgCombo->setCurrentIndex(currItemIndex);
        ecProject_->setBiomParamColRg(currData);
    }
    else
    {
        rgCombo->setCurrentIndex(0);
        ecProject_->setBiomParamColRg(firstIndexData);
    }
//
    currData = ecProject_->biomParamColLwin();
    currItemIndex = lwinCombo->findData(currData);
    firstIndexData = lwinCombo->itemData(0).toInt();
    if (currItemIndex >= 0)
    {
        lwinCombo->setCurrentIndex(currItemIndex);
        ecProject_->setBiomParamColLwin(currData);
    }
    else
    {
        lwinCombo->setCurrentIndex(0);
        ecProject_->setBiomParamColLwin(firstIndexData);
    }
//
    currData = ecProject_->biomParamColPpfd();
    currItemIndex = ppfdCombo->findData(currData);
    firstIndexData = ppfdCombo->itemData(0).toInt();
    if (currItemIndex >= 0)
    {
        ppfdCombo->setCurrentIndex(currItemIndex);
        ecProject_->setBiomParamColPpfd(currData);
    }
    else
    {
        ppfdCombo->setCurrentIndex(0);
        ecProject_->setBiomParamColPpfd(firstIndexData);
    }

    refreshVariableTables();
}

void BasicSettingsPage::refreshVariableTables()
{
    if (moreButton) { moreButton->hide(); }
    if (gasExtension) { gasExtension->hide(); }

    if (auto model = dynamic_cast<BasicVariableSelectionModel*>(fluxVariablesModel_))
    {
        model->refresh();
    }
    if (auto model = dynamic_cast<BasicVariableSelectionModel*>(ambientVariablesModel_))
    {
        model->refresh();
    }
}

/// Re-read the analyser of every cell and diagnostic record from the metadata.
///
/// `addNonGasRecord` resolves the instrument once, when the record is created,
/// and the answer was then never revisited. A cell column selected before its
/// metadata row named an instrument kept the empty answer for the life of the
/// project, and the engine reads an empty `cell_N_instr` as "belongs to no
/// analyser in particular".
///
/// That is not a cosmetic gap. Cell records are matched to gases by analyser,
/// so an untagged cell temperature is shared by every gas while a tagged cell
/// pressure reaches only its own - and a gas left without one falls back to
/// *ambient* pressure. On a site with two analysers that silently computed one
/// of them against the other's cell, or against the open air.
///
/// Refill the primary-analyser list and point it at the one leading now.
///
/// The choices are the analysers the gas records actually name, so the list
/// tracks the Variables table: select a gas on a second analyser and it
/// appears here, deselect the last one and it goes. `none` and `other` are
/// excluded — many unrelated variables carry "Other", so it identifies no
/// instrument, which is the same carve-out the moisture pairing makes.
///
/// The current entry is derived from record order rather than stored, so a
/// project edited outside this interface still shows the analyser whose
/// columns it will actually produce.
///
/// Rebuild the per-instrument missing-samples allowance rows.
///
/// The engine keys the allowance by the instrument's position in the
/// .metadata - anemometers first, then analysers, one shared counter - which is
/// exactly the order DlProject::saveProject writes instr_<K>_* in. Walking the
/// two lists in that order here is what keeps the two files agreeing; deriving
/// the number any other way would hand one instrument's allowance to another.
///
/// Rebuilt wholesale rather than patched, for the same reason as the primary
/// instrument list: the set can change in any way when the metadata is
/// reloaded, and a stale row would go on writing to a slot that now belongs to
/// a different device.
///
/// The known weakness, worth stating: the key is positional, so reordering the
/// instruments in the Metadata File Editor remaps the allowances. Only
/// rewriting the whole block on every metadata load - which is what this does -
/// keeps that from going unnoticed.
void BasicSettingsPage::refreshInstrMaxLackRows()
{
    if (!ecProject_ || !dlProject_ || !instrLackLayout_) { return; }

    while (auto item = instrLackLayout_->takeAt(0))
    {
        delete item->widget();
        delete item;
    }

    //> Model first, id as a qualifier - not the other way round. An id is free
    //> text and is very often a bare number, so leading with it produced rows
    //> labelled "0" that named nothing the user could recognise, and gave no
    //> clue whether they were looking at a sonic or an analyser.
    const auto describe = [](const QString& model, const QString& id,
                             const QString& fallback)
    {
        auto name = model.trimmed();
        if (name.isEmpty()) { name = fallback; }
        const auto tag = id.trimmed();
        if (tag.isEmpty()) { return name; }
        return QStringLiteral("%1 (%2)").arg(name, tag);
    };

    struct Row { QString label; QString category; };
    QList<Row> rows;
    for (const auto& anem : *dlProject_->anems())
    {
        rows.append({describe(anem.model(), anem.id(), tr("Anemometer")),
                     tr("Anemometers")});
    }
    for (const auto& irga : *dlProject_->irgas())
    {
        rows.append({describe(irga.model(), irga.id(), tr("Gas analyzer")),
                     tr("Gas analyzers")});
    }

    //> Nothing to show without a metadata file.
    if (rows.isEmpty())
    {
        instrLackContainer_->hide();
        return;
    }

    //> The engine holds instr_1..MAX_INSTRUMENTS and matches its keys by name,
    //> so a row past that would write an allowance nothing reads - and
    //> EcProject would not even read it back. The metadata file itself already
    //> warns when it describes more instruments than that.
    if (rows.size() > Defs::MAX_INSTRUMENTS)
    {
        rows = rows.mid(0, Defs::MAX_INSTRUMENTS);
    }

    const auto tip = tr("<b>Per-instrument missing sample allowance:</b> How "
                        "much of its own expected data this instrument may be "
                        "missing. An instrument that samples slower than the "
                        "station cannot fill every row of the raw file, and "
                        "what it does record is judged against what it should "
                        "have produced at its own acquisition frequency - "
                        "which you state in the Metadata File Editor, on the "
                        "instrument itself. Left at <i>Same as above</i>, an "
                        "instrument uses the allowance above and follows it "
                        "when it changes.");

    auto title = new QLabel(tr("Missing samples allowance, per instrument :"),
                            instrLackContainer_);
    //> Styled as a section header, like the other headings on this page: the
    //> block appears only once a metadata file is loaded, and an unstyled line
    //> of text among the spin boxes above it read as part of them.
    title->setProperty("groupLabel", true);
    title->setToolTip(tip);
    instrLackLayout_->addWidget(title, 0, 0, 1, 2);

    int row = 1;
    int slot = 1;
    QString currentCategory;
    for (const auto& r : rows)
    {
        //> Anemometers first, then analysers - the order the slots are keyed
        //> in. Said out loud so a sonic cannot be mistaken for an analyser.
        if (r.category != currentCategory)
        {
            currentCategory = r.category;
            auto heading = new QLabel(currentCategory, instrLackContainer_);
            //> An objectName, not a property: the stylesheet selects it
            //> as QLabel#citeLabel. Grey, so the heading reads as a
            //> divider rather than as another instrument.
            heading->setObjectName(QStringLiteral("citeLabel"));
            heading->setToolTip(tip);
            instrLackLayout_->addWidget(heading, row, 0, 1, 2, Qt::AlignLeft);
            ++row;
        }

        auto label = new QLabel(r.label, instrLackContainer_);
        label->setToolTip(tip);

        auto spin = new QSpinBox(instrLackContainer_);
        //> -1 is "states nothing"; the project drops the key entirely for it,
        //> which is what makes the instrument follow the global allowance
        //> rather than freeze today's value.
        spin->setRange(-1, 99);
        spin->setSpecialValueText(tr("Same as above"));
        spin->setSuffix(tr("  [%]", "Percentage"));
        spin->setAccelerated(true);
        spin->setMinimumWidth(130);
        spin->setMaximumWidth(145);
        spin->setToolTip(tip);
        spin->setValue(ecProject_->screenInstrMaxLack(slot));

        const int thisSlot = slot;
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [=](int value)
                {
                    ecProject_->setScreenInstrMaxLack(thisSlot, value);
                });

        instrLackLayout_->addWidget(label, row, 0, Qt::AlignRight);
        instrLackLayout_->addWidget(spin, row, 1);
        ++row;
        ++slot;
    }
    instrLackLayout_->setColumnStretch(2, 1);
    instrLackContainer_->show();
}

/// There is no "automatic" entry, and that is a consequence of the design
/// rather than an omission. Record order *is* the setting: no flag is written
/// alongside it, so "nobody has chosen" and "this analyser happens to be
/// first" are the same file and the same output. An entry for the first would
/// be indistinguishable from the second after a reload and would do nothing
/// when selected. The box instead always names the analyser that leads, which
/// for an untouched project is exactly the automatic answer - said out loud.
void BasicSettingsPage::refreshPrimaryInstrumentCombo()
{
    if (!ecProject_ || !primaryInstrumentCombo) { return; }

    QStringList instruments;
    for (const auto& rec : ecProject_->gasColumns())
    {
        if (!MeasurementRecords::isRealInstrument(rec.instrumentId)) { continue; }
        if (instruments.contains(rec.instrumentId)) { continue; }
        instruments.append(rec.instrumentId);
    }

    const auto leading = ecProject_->primaryGasInstrument();

    //> Rebuilt wholesale rather than patched: the set can change in any way
    //> when records change, and a stale entry here would silently reorder the
    //> records the next time the user opened the list.
    QSignalBlocker blocker(primaryInstrumentCombo);
    primaryInstrumentCombo->clear();
    if (instruments.isEmpty())
    {
        primaryInstrumentCombo->addItem(tr("(no gas selected)"), QString());
    }
    for (const auto& id : instruments)
    {
        primaryInstrumentCombo->addItem(id, id);
    }

    //> Fewer than two analysers is no choice at all - the only one there is
    //> already leads, whatever the user might click.
    primaryInstrumentCombo->setEnabled(instruments.size() > 1);

    const int idx = primaryInstrumentCombo->findData(leading);
    primaryInstrumentCombo->setCurrentIndex(idx >= 0 ? idx : 0);
}

/// Reorder the gas records so the chosen analyser leads.
///
/// Everything the choice controls follows from that order, so there is nothing
/// else to write - see EcProject::setPrimaryGasInstrument. The tables are
/// rebuilt afterwards because the rows are drawn from the record list and half
/// of them have just moved.
void BasicSettingsPage::onPrimaryInstrumentChanged()
{
    if (!ecProject_ || !primaryInstrumentCombo) { return; }

    const auto chosen =
        primaryInstrumentCombo->currentData().toString();
    if (!ecProject_->setPrimaryGasInstrument(chosen)) { return; }

    refreshVariableTables();
    //> The list itself does not change, but the entry that is current does:
    //> "Automatic" now resolves to whichever analyser was just moved to the
    //> front, and leaving the box reading "Automatic" would misdescribe it.
    refreshPrimaryInstrumentCombo();
    refreshInstrMaxLackRows();
}

/// The metadata variable a non-gas record's column must carry to still be that
/// record, or empty for a slug this does not police.
///
/// The names are the display strings, which is what VariableDesc::variable()
/// holds; the ini tokens are DlProject's business.
QString BasicSettingsPage::variableForNonGasSlug(const QString& slug)
{
    if (slug == QLatin1String("cell_t"))
        { return VariableDesc::getVARIABLE_VAR_STRING_15(); }
    if (slug == QLatin1String("int_t_1"))
        { return VariableDesc::getVARIABLE_VAR_STRING_9(); }
    if (slug == QLatin1String("int_t_2"))
        { return VariableDesc::getVARIABLE_VAR_STRING_10(); }
    if (slug == QLatin1String("int_p"))
        { return VariableDesc::getVARIABLE_VAR_STRING_11(); }
    if (slug == QLatin1String("diag_75"))
        { return VariableDesc::getVARIABLE_VAR_STRING_25(); }
    if (slug == QLatin1String("diag_72"))
        { return VariableDesc::getVARIABLE_VAR_STRING_26(); }
    if (slug == QLatin1String("diag_77"))
        { return VariableDesc::getVARIABLE_VAR_STRING_27(); }
    if (slug == QLatin1String("diag_anem"))
        { return VariableDesc::getVARIABLE_VAR_STRING_30(); }
    return QString();
}

/// Drop cell and diagnostic records whose column no longer measures what the
/// record says it does.
///
/// A record is a pair - a raw column and what that column is - and only the
/// first half is stored. Re-declare that column in the Raw File Description
/// and the record survives the edit, still claiming the old measurement. The
/// column then leaves the variable table, so the row that would show the
/// record is gone and the record cannot be un-ticked: it is invisible here and
/// visible only in the project file.
///
/// Left there, the engine refused the project. A diagnostic column re-declared
/// as AGC kept its diag_72 record, so a real diagnostic elsewhere on the same
/// analyser made two records competing for the one diagnostic slot, and
/// MetadataFileValidation aborted the run over a record the user had no way to
/// find. The engine now treats such a record as inert; this stops the file
/// carrying it in the first place.
///
/// Gas records are deliberately not touched. The same staleness is possible
/// there, but a gas record carries a block of per-species processing settings
/// and dropping one silently discards them - a separate decision from this.
void BasicSettingsPage::pruneStaleNonGasRecords()
{
    if (!ecProject_ || !dlProject_) { return; }

    const auto vars = dlProject_->variables();
    if (!vars) { return; }

    const auto prune = [&](QVector<MeasurementRecord>& records)
    {
        bool changed = false;
        for (int i = records.size() - 1; i >= 0; --i)
        {
            const auto& rec = records.at(i);
            const auto expected = variableForNonGasSlug(rec.slug);
            //> A slug this does not know is left alone rather than dropped.
            if (expected.isEmpty()) { continue; }
            //> A column number past the end of the metadata says nothing
            //> about the record: no metadata is loaded, or a shorter file is
            //> open for the moment. Only an actual contradiction removes it.
            if (rec.rawColumn <= 0 || rec.rawColumn > vars->size()) { continue; }
            if (vars->at(rec.rawColumn - 1).variable() == expected) { continue; }
            records.removeAt(i);
            changed = true;
        }
        return changed;
    };

    auto cells = ecProject_->cellColumns();
    if (prune(cells)) { ecProject_->setCellColumns(cells); }

    auto diags = ecProject_->diagColumns();
    if (prune(diags)) { ecProject_->setDiagColumns(diags); }
}

/// Rebuild the signal-strength records from the raw file description.
///
/// Wholesale, on every metadata read, for the same reason refreshInstrMaxLackRows
/// rebuilds rather than patches: these are derived facts, not choices. There is
/// nothing for the user to select - a column either is declared AGC or RSSI or
/// it is not - so deriving them is what keeps them from going stale the way the
/// diagnostic records could.
///
/// The engine reads them to find the signal strength of a gas's OWN analyser
/// for the conditional eddy covariance screen. Before they existed the only
/// statement of that was the metadata variable name, matched case-sensitively,
/// with the analyser inferred.
void BasicSettingsPage::syncSignalStrengthRecords()
{
    if (!ecProject_ || !dlProject_) { return; }

    const auto vars = dlProject_->variables();
    if (!vars) { return; }

    QVector<MeasurementRecord> records;
    for (int k = 0; k < vars->size(); ++k)
    {
        const auto& var = vars->at(k);
        const auto name = var.variable();
        if (name != VariableDesc::getVARIABLE_VAR_STRING_35()
            && name != VariableDesc::getVARIABLE_VAR_STRING_36())
        {
            continue;
        }
        //> A column the description ignores, or declares non-numeric, holds
        //> nothing the engine can read.
        if (var.ignore() == QLatin1String("yes")) { continue; }
        if (var.numeric() == QLatin1String("no")) { continue; }

        MeasurementRecord rec;
        //> Lower case, unlike the display name. The record is what the engine
        //> compares now, and comparing it case-insensitively is the point of
        //> having it: a metadata file from another tool saying `agc` used to
        //> go unscreened with nothing said.
        rec.slug = name.toLower();
        rec.rawColumn = k + 1;
        rec.instrumentId = canonicalInstrumentForColumn(k + 1);
        records.append(rec);
    }

    if (records != ecProject_->agcColumns())
    {
        ecProject_->setAgcColumns(records);
    }
}

/// A record whose instrument the user set by hand is left alone: `other` and
/// `none` are deliberate answers, not missing ones.
void BasicSettingsPage::syncNonGasRecordInstruments()
{
    if (!ecProject_ || !dlProject_) { return; }

    const auto sync = [this](QVector<MeasurementRecord>& records)
    {
        bool changed = false;
        for (auto& rec : records)
        {
            if (rec.rawColumn <= 0) { continue; }
            const auto resolved = canonicalInstrumentForColumn(rec.rawColumn);
            //> Empty means "cannot tell" - no metadata loaded, or the column
            //> is past its end - which is the one case a stored answer is
            //> worth more than a fresh one. Everything else is mirrored,
            //> including `none` and `other`: the raw file description is the
            //> authority here, and there is no competing answer to protect.
            if (resolved.isEmpty() || resolved == rec.instrumentId) { continue; }
            rec.instrumentId = resolved;
            changed = true;
        }
        return changed;
    };

    auto cells = ecProject_->cellColumns();
    if (sync(cells)) { ecProject_->setCellColumns(cells); }

    auto diags = ecProject_->diagColumns();
    if (sync(diags)) { ecProject_->setDiagColumns(diags); }
}

void BasicSettingsPage::onIdLabelClicked()
{
    idEdit->setFocus();
    idEdit->selectAll();
}

void BasicSettingsPage::updateFlag1Threshold(double n)
{
    ecProject_->setScreenFlag1Threshold(n);
}

void BasicSettingsPage::updateFlag2Threshold(double n)
{
    ecProject_->setScreenFlag2Threshold(n);
}

void BasicSettingsPage::updateFlag3Threshold(double n)
{
    ecProject_->setScreenFlag3Threshold(n);
}

void BasicSettingsPage::updateFlag4Threshold(double n)
{
    ecProject_->setScreenFlag4Threshold(n);
}

void BasicSettingsPage::updateFlag5Threshold(double n)
{
    ecProject_->setScreenFlag5Threshold(n);
}

void BasicSettingsPage::updateFlag6Threshold(double n)
{
    ecProject_->setScreenFlag6Threshold(n);
}

void BasicSettingsPage::updateFlag7Threshold(double n)
{
    ecProject_->setScreenFlag7Threshold(n);
}

void BasicSettingsPage::updateFlag8Threshold(double n)
{
    ecProject_->setScreenFlag8Threshold(n);
}

void BasicSettingsPage::updateFlag9Threshold(double n)
{
    ecProject_->setScreenFlag9Threshold(n);
}

void BasicSettingsPage::updateFlag10Threshold(double n)
{
    ecProject_->setScreenFlag10Threshold(n);
}

void BasicSettingsPage::updateFlag1Policy(int n)
{
    ecProject_->setScreenFlag1Policy(n);
}

void BasicSettingsPage::updateFlag2Policy(int n)
{
    ecProject_->setScreenFlag2Policy(n);
}

void BasicSettingsPage::updateFlag3Policy(int n)
{
    ecProject_->setScreenFlag3Policy(n);
}

void BasicSettingsPage::updateFlag4Policy(int n)
{
    ecProject_->setScreenFlag4Policy(n);
}

void BasicSettingsPage::updateFlag5Policy(int n)
{
    ecProject_->setScreenFlag5Policy(n);
}

void BasicSettingsPage::updateFlag6Policy(int n)
{
    ecProject_->setScreenFlag6Policy(n);
}

void BasicSettingsPage::updateFlag7Policy(int n)
{
    ecProject_->setScreenFlag7Policy(n);
}

void BasicSettingsPage::updateFlag8Policy(int n)
{
    ecProject_->setScreenFlag8Policy(n);
}

void BasicSettingsPage::updateFlag9Policy(int n)
{
    ecProject_->setScreenFlag9Policy(n);
}

void BasicSettingsPage::updateFlag10Policy(int n)
{
    ecProject_->setScreenFlag10Policy(n);
}

void BasicSettingsPage::onClickFlagLabel()
{
    QLabel* labelSender = qobject_cast<QLabel *>(sender());

    QString labelName = labelSender->objectName();
    QString flagName = labelName.left(labelName.indexOf(QLatin1String("Label")));
    QString flagComboObjectName = flagName + QStringLiteral("Combo");

    QComboBox *flagCombo = this->findChild<QComboBox *>(flagComboObjectName);

    flagCombo->setFocus();
    flagCombo->showPopup();
}

/// The two spin boxes describe the open gas slot, and now write its record.
///
/// They used to call setGeneralColGasMw / setGeneralColGasDiff, which set the
/// project-wide gas_mw and gas_diff. writeMeasurementRecords deletes both keys
/// before saving, so nothing the user typed here survived - and the engine
/// then fell back to its own default, which for any species outside CO2, H2O,
/// CH4 and N2O is nitrous oxide's 44.01 g/mol.
///
/// The table's Molecular weight and Molecular diffusivity cells write the same
/// records, so the two controls agree by construction.
void BasicSettingsPage::updateGasMw(double value)
{
    setGasMolecularWeight(openGasRecordIndex(), value);
}

void BasicSettingsPage::updateGasDiff(double value)
{
    setGasDiffusivity(openGasRecordIndex(), value);
}

/// Record index of the gas in the open slot, or -1 when there is none.
///
/// The open slot is the fourth table row - the one not pinned to CO2, H2O or
/// CH4 - and the two spin boxes have always described it.
/// The record the open-species controls edit.
///
/// This returned record three unconditionally - the fourth legacy slot - so
/// on a project with, say, COS at record three, NH3 at four and a second N2O
/// at five, the molecular-weight and diffusivity spin boxes edited COS while
/// appearing to describe whichever gas the user was looking at. It finds the
/// first open species now.
///
/// It still names only the FIRST, which is the honest limit of a single pair
/// of spin boxes standing in for a list. The per-row Molecular Weight and
/// Diffusivity cells address gasRecordIndex(row) correctly and are the path
/// to use for the others; these controls are a convenience for the common
/// single-open-gas case.
int BasicSettingsPage::openGasRecordIndex() const
{
    if (!ecProject_) { return -1; }
    const auto& gases = ecProject_->gasColumns();
    for (int i = 0; i < gases.size(); ++i)
    {
        if (gases.at(i).rawColumn <= 0) { continue; }
        const QString slug = gases.at(i).slug.toLower();
        if (slug == QLatin1String("co2") || slug == QLatin1String("h2o")
            || slug == QLatin1String("ch4"))
        {
            continue;
        }
        return i;
    }
    return -1;
}

// enforce (start date&time) <= (end date&time)
void BasicSettingsPage::forceEndDatePolicy()
{
    endDateEdit->setMinimumDate(startDateEdit->date());
}

// enforce (start date&time) <= (end date&time)
void BasicSettingsPage::forceEndTimePolicy()
{
    if (startDateEdit->date() == endDateEdit->date())
    {
        endTimeEdit->setMinimumTime(startTimeEdit->time());
    }
    else
    {
        endTimeEdit->clearMinimumTime();
    }
}

///
/// \brief BasicSettingsPage::updateProjectFilesFound
/// \param fileNumber
///
void BasicSettingsPage::updateProjectFilesFound(int fileNumber)
{
    ecProject_->setGeneralFilesFound(fileNumber);
}

void BasicSettingsPage::clearFilePrototype()
{
    updateFilePrototype(QString());
    clearFilesFound();
}

// called by programmatic changes on filePrototypeEdit
void BasicSettingsPage::updateFilePrototype(const QString& pattern)
{
    ecProject_->setGeneralFilePrototype(pattern);
}

// called by:
// 1. captureEmbeddedMetadata(), ok
// 2. clearFilesFound(), ok
// 3. updateFilesFound(), ok
void BasicSettingsPage::updateFilesFoundLabel(int fileNumber)
{
    if (fileNumber == 0)
    {
        filesFound->clear();
    }
    else if (fileNumber == 1)
    {
        filesFound->setText(tr("1 file found"));
    }
    else
    {
        filesFound->setText(tr("%1 files found").arg(fileNumber));
    }
}

// called by:
// 1. captureEmbeddedMetadata() in case of no files found, ok
// 2. reset(), ok
// 3. updateMetadaRead, ok
// 4. refresh(), ?
void BasicSettingsPage::clearFilesFound()
{
    updateFilesFoundLabel(0);
    updateProjectFilesFound(0);
}

// called by updateRecursion() when recursion checkbox is toggled
void BasicSettingsPage::runUpdateFilesFound()
{
    updateFilesFound(ecProject_->screenRecurse());
}

// transform prototype p to a regular expression pattern
QString BasicSettingsPage::prototypeToRegExp(const QString& p)
{
    auto pattern = p;

    pattern.replace(QLatin1String("."), QLatin1String("[.]"));  // dot
    pattern.replace(QLatin1String("?"), QLatin1String("."));    // single char
    pattern.replace(QLatin1String("yyyy"), QLatin1String("(19[89][0-9]|20[0-9][0-9]|2100)")); // year 4 digits
    pattern.replace(QLatin1String("yy"), QLatin1String("([0-9][0-9])"));         // year 2 digits
    pattern.replace(QLatin1String("mm"), QLatin1String("(0[1-9]|1[012])"));    // month 2 digits
    pattern.replace(QLatin1String("ddd"), QLatin1String("(00[1-9]|0[1-9][0-9]|[12][0-9][0-9]|3[0-5][0-9]|36[0-6])"));      // day of year 3 digits
    pattern.replace(QLatin1String("dd"), QLatin1String("(0[1-9]|[1-2][0-9]|3[01])")); // day 2 digits
    pattern.replace(QLatin1String("HH"), QLatin1String("([01][0-9]|2[0-4])")); // hours 2 digits
    pattern.replace(QLatin1String("MM"), QLatin1String("([0-5][0-9])"));       // minutes 2 digits

    return pattern;
}

QStringList BasicSettingsPage::filterRawDataWithPrototype(const QString& p)
{
    auto rePattern = prototypeToRegExp(p);
    QRegularExpression re;
    re.setPattern(rePattern);

    if (re.isValid())
    {
        //> Build the filtered list instead of removing from the list being
        //> iterated: removeAll() shifts the very elements the loop reference
        //> points into, so everything after the first removal was skipped and
        //> non-matching files survived the filter.
        QStringList filtered;

        for (const auto &filename : currentRawDataList_)
        {
            if (filename.contains(re))
            {
                filtered.append(filename);
            }
        }

        currentFilteredRawDataList_ = filtered;
    }

    return currentFilteredRawDataList_;
}

// called:
// 1. at the end of setPrototype() ok
// 2. from runUpdateFilesFound() ok
// 3. from updateMetadataRead() ok
void BasicSettingsPage::updateFilesFound(bool recursionToggled)
{
    if (datapathBrowse->path().isEmpty())
    {
        return;
    }

    auto fileCount = 0;

    findFileProgressWidget->startAnimation();

    // first pass, filter by extension on the file system
    if (filePrototypeEdit->text().isEmpty())
    {
        if (ecProject_->generalFileType() == Defs::RawFileType::GHG)
        {
            QString extension = QStringLiteral("*.") + Defs::GHG_NATIVE_DATA_FILE_EXT;
            currentRawDataList_ = FileUtils::getFiles(datapathBrowse->path(), extension, recursionToggled);
        }
    }
    else
    {
        int extensionIndex = ecProject_->generalFilePrototype().lastIndexOf(QLatin1String(".")) + 1;
        QString extension = QStringLiteral("*.") + ecProject_->generalFilePrototype().mid(extensionIndex);
        currentRawDataList_ = FileUtils::getFiles(datapathBrowse->path(), extension, recursionToggled);
    }

    // second pass, filter the file list with a regexp
    if (filePrototypeEdit->text().isEmpty())
    {
        currentFilteredRawDataList_ = currentRawDataList_;
    }
    else
    {
        currentFilteredRawDataList_ = filterRawDataWithPrototype(filePrototypeEdit->text());
    }

    findFileProgressWidget->stopAnimation();

    fileCount = currentFilteredRawDataList_.count();

    updateFilesFoundLabel(fileCount);
    updateProjectFilesFound(fileCount);
}

// called by:
// 1. RawFilenameDialog::updateFileFormatRequest() signal, ok
// 2. setPrototype(), ok
void BasicSettingsPage::updateFilePrototypeEdit(const QString& f)
{
    filePrototypeEdit->setText(f);
    updateFilesFound(ecProject_->screenRecurse());
}

void BasicSettingsPage::askRawFilenamePrototype()
{
    if (!rawFilenameDialog)
    {
        rawFilenameDialog = new RawFilenameDialog(this,
                                                  ecProject_,
                                                  &suffixList_,
                                                  &currentFilteredRawDataList_);
        rawFilenameDialog->setObjectName(QStringLiteral("RawFilenameDialog"));
        connect(rawFilenameDialog, &RawFilenameDialog::updateFileFormatRequest,
                this, &BasicSettingsPage::updateFilePrototypeEdit);
    }

    rawFilenameDialog->refresh();

    rawFilenameDialog->show();
    rawFilenameDialog->raise();
    rawFilenameDialog->activateWindow();
}

void BasicSettingsPage::fetchMagneticDeclination()
{
    magneticDeclinationFetchProgress->startAnimation();
    httpManager_ = new QNetworkAccessManager(this);

    auto noaaServiceUrl = QUrl(QStringLiteral("https://www.ngdc.noaa.gov/geomag-web/calculators/calculateDeclination"));

    auto decLat = dlProject_->siteLatitude();
    auto decLon = dlProject_->siteLongitude();

    QUrlQuery q;
    q.addQueryItem(QStringLiteral("lat1"), QString::number(decLat, 'd', 6));
    q.addQueryItem(QStringLiteral("lon1"), QString::number(decLon, 'd', 6));
    q.addQueryItem(QStringLiteral("model"), QStringLiteral("IGRF"));
    q.addQueryItem(QStringLiteral("startYear"), QString::number(declinationDateEdit->date().year()));
    q.addQueryItem(QStringLiteral("startMonth"), QString::number(declinationDateEdit->date().month()));
    q.addQueryItem(QStringLiteral("startDay"), QString::number(declinationDateEdit->date().day()));
    q.addQueryItem(QStringLiteral("resultFormat"), QStringLiteral("csv"));

    noaaServiceUrl.setQuery(q);

    qDebug() << "URL" << noaaServiceUrl.toString();

    QNetworkRequest getRequest;
    getRequest.setUrl(noaaServiceUrl);
    getRequest.setRawHeader("User-Agent", "MyOwnBrowser 1.0");

    httpReply_ = httpManager_->get(getRequest);

    connect(httpManager_, &QNetworkAccessManager::finished,
            this, &BasicSettingsPage::replyFinished);

    connect(httpReply_, &QNetworkReply::readyRead,
            this, &BasicSettingsPage::bufferHttpReply);
}

void BasicSettingsPage::replyFinished(QNetworkReply* reply)
{
    // if no error
    magneticDeclinationFetchProgress->stopAnimation();
    if (!reply->error()) { return; }

    // handle the error
    qDebug() << reply->error();
    qDebug() << reply->errorString();

    // show only if not aborted by the user
    if (reply->error() != QNetworkReply::OperationCanceledError)
    {
        noNoaaConnectionMsg();
    }
}

void BasicSettingsPage::bufferHttpReply()
{
    if (httpReply_->error() == QNetworkReply::NoError)
    {
        QByteArray data = httpReply_->readAll();

        httpBuffer_.append(data);
        QByteArray line(httpBuffer_);
        QByteArrayList lineList(line.split('\n'));

        // newlines found
        if (lineList.at(0) != httpBuffer_)
        {
            for (int i = 0; i < lineList.size(); ++i)
            {
                if (parseHttpReply(lineList.at(i)))
                {
                   continue;
                }
                else
                {
                    // manage NOAA server errors
                    noNoaaDownloadMsg();
                    magneticDeclinationFetchProgress->stopAnimation();
                    return;
                }
            }

            if (lineList.last().endsWith('\n'))
                httpBuffer_.resize(0);
            else
                httpBuffer_ = lineList.last();
        }
    }
    else
    {
        // handle the error
        qDebug() << httpReply_->errorString();
    }
}

bool BasicSettingsPage::parseHttpReply(const QByteArray& data)
{
    QByteArray cleanLine;

    cleanLine.append(data.simplified());

    if (cleanLine.startsWith("#") || cleanLine.startsWith("\n"))
    {
        // skip comments or empty row
        return true;
    }
    QByteArrayList columnList(cleanLine.split(','));

    // web-server error handling
    if (columnList.size() > 1)
    {
        // declination in decimal degrees
        declination_ = QVariant(columnList.at(4)).toString();

        QString decStr = strDeclination(declination_.toDouble());
        declinationEdit->setText(decStr);

        // variation of declination in decimal degrees / year
        double variation = QVariant(columnList.at(5)).toDouble();
        // variation of declination in decimal minutes / year
        QString variationDecValue = strDeclination(variation);

        QString variationStr = tr("Changing by %1 per year").arg(variationDecValue);

        decChangingLabel->setText(variationStr);

        magneticDeclinationFetchProgress->stopAnimation();
    }
    return true;
}

void BasicSettingsPage::northRadioClicked(int b)
{
    declinationLabel->setEnabled(b);
    declinationLabel->setToolTip(tr("<b>Magnetic Declination:</b> Based upon the latitude and longitudinal coordinates entered, EddyFlow determines the magnetic declination from the U.S. NOAA (National Oceanic and Atmospheric Organization) internet resources (U.S. National Geophysical Data Center)."));

    declinationEdit->setEnabled(b);
    declinationDateLabel->setEnabled(b);
    declinationDateEdit->setEnabled(b);
    declinationFetchButton->setEnabled(b);

    if (b)
    {
        declinationDateEdit->setDate(endDateEdit->date());
    }

    // block fetching
    if (b == 0 && httpReply_)
    {
        httpReply_->abort();
    }
}

void BasicSettingsPage::updateUseGeoNorth(int b)
{
    ecProject_->setScreenUseGeoNorth(b);
}

void BasicSettingsPage::updateMagDec(const QString& dec)
{
    QString dec_str = QString::number(numDeclination(dec), 'd', 6);
    ecProject_->setScreenMagDec(dec_str.toDouble());
}

// get declination in signed decimal degrees from degree, minutes (dddmm.sss) string
double BasicSettingsPage::numDeclination(const QString &text)
{
    double dec = 0.0;

    QString dec_pattern = tr("(?:(0\\d\\d)%1\\s([0-5]\\d)'\\s(E|W))|").arg(Defs::DEGREE);
            dec_pattern += tr("(?:(1[0-7]\\d)%1\\s([0-5]\\d)'\\s(E|W))|").arg(Defs::DEGREE);
            dec_pattern += tr("(?:(180)%1\\s(00)'\\s(E|W))").arg(Defs::DEGREE);
    QRegularExpression decRx(dec_pattern);
    auto match = decRx.match(text);
    if (match.hasMatch())
    {
        bool ok;
        if (!match.captured(1).isEmpty())
        {
            // first case: pattern from cap(1) to cap(3)
            dec = match.captured(1).toDouble(&ok);
            dec += match.captured(2).toDouble(&ok) / 60.0;
        }
        else if (!match.captured(4).isEmpty())
        {
            // second case: pattern from cap(4) to cap(6)
            dec = match.captured(4).toDouble(&ok);
            dec += match.captured(5).toDouble(&ok) / 60.0;
        }
        else
        {
            // third case: pattern from cap(7) to cap(9)
            dec = 180.0;
        }
        // negative coordinates case
        if ((match.captured(3) == QLatin1String("W"))
                || (match.captured(6) == QLatin1String("W"))
                || (match.captured(9) == QLatin1String("W")))
        {
            dec = 0.0 - dec;
        }
    }
    return dec;
}

void BasicSettingsPage::onClickDeclinationLabel()
{
    declinationEdit->setFocus();
    declinationEdit->selectAll();
}

// get declination in degree, minutes (dddmm.sss) string
// from signed decimal degrees
QString BasicSettingsPage::strDeclination(double dec)
{
    QString dms = QString();

    double degrees;
    modf(dec, &degrees);

    double rest = dec - degrees;
    double min_rest = rest * 60.0;

    double min_d;
    modf(min_rest, &min_d);

    int minutes = static_cast<int>(min_d);

    QString degrees_str;
    QTextStream d(&degrees_str);
    d << qSetFieldWidth(3) << qSetPadChar(QLatin1Char('0')) << Qt::right << qAbs( degrees );

    QString minutes_str;
    QTextStream m(&minutes_str);
    m << qSetFieldWidth(2) << qSetPadChar(QLatin1Char('0')) << Qt::right << qAbs( minutes );

    dms.append(degrees_str);
    dms.append(tr("%1 ").arg(Defs::DEGREE));
    dms.append(minutes_str);
    dms.append(QLatin1String("' "));

    if (dec >= 0)
    {
        dms.append(QLatin1String("E"));
    }
    else
    {
        dms.append(QLatin1String("W"));
    }
    return dms;
}

// get variation of declination in decimal minutes / year, i.e. in mmss.sss
// string from signed decimal degrees
// NOTE: never used
QString BasicSettingsPage::strVariation(double dec)
{
    QString dms = QString();

    double degrees;
    modf(dec, &degrees);

    double rest = dec - degrees;
    double min_rest = rest * 60.0;

    double min_d;
    modf(min_rest, &min_d);

    int minutes = static_cast<int>(min_d);

    QString degrees_str;
    QTextStream d(&degrees_str);
    d << qSetFieldWidth(3) << qSetPadChar(QLatin1Char('0')) << Qt::right << qAbs( degrees );

    QString minutes_str;
    QTextStream m(&minutes_str);
    m << qSetFieldWidth(2) << qSetPadChar(QLatin1Char('0')) << Qt::right << qAbs( minutes );

    dms.append(degrees_str);
    dms.append(tr("%1 ").arg(Defs::DEGREE));
    dms.append(minutes_str);
    dms.append(QLatin1String("' "));

    if (dec >= 0)
    {
        dms.append(QLatin1String("E"));
    }
    else
    {
        dms.append(QLatin1String("W"));
    }
    return dms;
}

void BasicSettingsPage::onDeclinationDateLabelClicked()
{
    declinationDateEdit->setFocus();
    WidgetUtils::showCalendarOf(declinationDateEdit);
}

void BasicSettingsPage::updateDeclinationDate(const QDate &d)
{
    ecProject_->setScreenDecDate(d.toString(Qt::ISODate));
}

void BasicSettingsPage::alignDeclinationDate(const QDate& d)
{
    if (ecProject_->generalSubset()
        && ecProject_->screenUseGeoNorth())
    {
        auto currentDeclinationDate = declinationDateEdit->date();
        if (currentDeclinationDate != d)
        {
            declinationDateEdit->setDate(d);
            if (ecProject_->screenMagDec() != 0.0)
            {
                fetchMagneticDeclination();
                emit saveSilentlyRequest();
            }
        }
    }
}

void BasicSettingsPage::clearDataSelection()
{
    int ret_code = acceptVariableReset();
    if (ret_code != QMessageBox::Ok) { return; }

    datapathBrowse->clear();
    clearFilesFound();
    subsetCheckBox->setChecked(false);
}

int BasicSettingsPage::handleVariableReset()
{
    //> "Has the user configured anything yet?" - asked of the gas list rather
    //> than of CO2 alone, so a site that measures only CH4 is not treated as
    //> a blank project whose variables can be reset without asking.
    const bool hasGas = !ecProject_->gasColumns().isEmpty()
                        || ecProject_->generalColCo2() != -1;

    // if not new project
    if (!ecProject_->screenDataPath().isEmpty()
        && !ecProject_->generalColMasterSonic().isEmpty()
        && hasGas)
    {
        return acceptVariableReset();
    }
    return QMessageBox::Yes;
}

int BasicSettingsPage::acceptVariableReset()
{
    // keep this value to go through in case of not showing dialog
    int res = QMessageBox::Ok;

    if (ecProject_->generalUseAltMdFile()) return res;

    bool showDialog = GlobalSettings::getAppPersistentSettings(
                            Defs::CONFGROUP_WINDOW,
                            Defs::CONF_WIN_BASIC_SETTINGS_CLEARING_MSG,
                            true).toBool();

    if (showDialog)
    {
        InfoMessage runDialog(QDialogButtonBox::Ok
                              | QDialogButtonBox::Cancel, this);
        runDialog.setTitle(tr("Variable and Flag Reset"));
        runDialog.setIcon(QPixmap(QStringLiteral(":/icons/msg-question")));
        runDialog.setType(InfoMessage::Type::SELECTION_CLEANING);
        runDialog.setDoNotShowAgainVisible(true);
        runDialog.setMessage(tr("When changing the raw data directory, "
                                "all current 'Variables' will be replaced "
                                "by new 'Variables' from the new metadata "
                                "inside the GHG files. "
                                "In addition all 'Flags' variables and their "
                                "settings under 'Select Items for Flux "
                                "Computation' will be reset to defaults. "
                                "\n\n"));
        runDialog.refresh();

        res = runDialog.exec();
    }

    if (res == QMessageBox::Ok)
    {
        clearSelectedItems();
        FileUtils::cleanSmfDirRecursively(configState_->general.env);
    }
    return res;
}

void BasicSettingsPage::clearSelectedItems()
{
    crossWindCheckBox->setEnabled(false);
    crossWindCheckBox->setChecked(true);
    clearVarsCombo();
    clearFlagVars();
    clearFlagUnits();
    clearFlagThresholdsAndPolicies();

    ecProject_->setGeneralColMasterSonic(QString());
    ecProject_->setGeneralColCo2(-1);
    ecProject_->setGeneralColH2o(-1);
    ecProject_->setGeneralColCh4(-1);
    ecProject_->setGeneralColGas4(-1);
    ecProject_->setGeneralColIntTc(-1);
    ecProject_->setGeneralColIntT1(-1);
    ecProject_->setGeneralColIntT2(-1);
    ecProject_->setGeneralColIntP(-1);
    ecProject_->setGeneralColAirT(-1);
    ecProject_->setGeneralColAirP(-1);

    ecProject_->setBiomParamColAirT(999);
    ecProject_->setBiomParamColAirP(999);
    ecProject_->setBiomParamColRh(-1);
    ecProject_->setBiomParamColRg(-1);
    ecProject_->setBiomParamColLwin(-1);
    ecProject_->setBiomParamColPpfd(-1);

    //> The project-wide gas_mw / gas_diff are retired: writeMeasurementRecords
    //> deletes both keys, so resetting them here did nothing. Molecular weight
    //> and diffusivity now live on the gas records, and clearing those is part
    //> of clearing the records themselves - which this routine still does not
    //> do for any record kind.
    ecProject_->setGeneralColTs(-1);
    ecProject_->setGeneralColDiag72(-1);
    ecProject_->setGeneralColDiag75(-1);
    ecProject_->setGeneralColDiag77(-1);

    ecProject_->setScreenFlag1Col(-1);
    ecProject_->setScreenFlag2Col(-1);
    ecProject_->setScreenFlag3Col(-1);
    ecProject_->setScreenFlag4Col(-1);
    ecProject_->setScreenFlag5Col(-1);
    ecProject_->setScreenFlag6Col(-1);
    ecProject_->setScreenFlag7Col(-1);
    ecProject_->setScreenFlag8Col(-1);
    ecProject_->setScreenFlag9Col(-1);
    ecProject_->setScreenFlag10Col(-1);
}

void BasicSettingsPage::dateRangeDetect()
{
    if (!currentRawDataList_.isEmpty())
    {
        findFileProgressWidget->startAnimation();

        FileUtils::DateRange dates;

        QFuture<FileUtils::DateRange> future = QtConcurrent::run(&FileUtils::getDateRangeFromFileList, currentRawDataList_, ecProject_->generalFilePrototype());
        while (!future.isFinished())
        {
            QCoreApplication::processEvents();
        }
        dates = future.result();

        findFileProgressWidget->stopAnimation();

        startDateEdit->setDate(dates.first.date());
        startTimeEdit->setTime(dates.first.time());

        // correct the start/end date accounting for file duration
        if (dlProject_->timestampEnd() == 0)
        {
            dates.second = dates.second.addSecs(dlProject_->fileDuration() * 60);
        }
        else
        {
            dates.first = dates.first.addSecs(-dlProject_->fileDuration() * 60);
        }

        endDateEdit->setDate(dates.second.date());
        endTimeEdit->setTime(dates.second.time());

        emit setDateRangeRequest(dates);
    }
}

void BasicSettingsPage::updateSmartfluxBar()
{
    smartfluxBar_->setVisible(configState_->project.smartfluxMode);
    setSmartfluxUI(configState_->project.smartfluxMode);
}

void BasicSettingsPage::setSmartfluxUI(bool on)
{
    QWidgetList widgets;
    widgets << avgIntervalLabel
         << avgIntervalSpin
         << outpathLabel
         << outpathBrowse
         << idLabel
         << idEdit
         << anemRefLabel
         << anemRefCombo
         << anemFlagLabel
         << anemFlagCombo
         << recursionCheckBox
         << subsetCheckBox
         << dateRangeDetectButton;

    for (auto w : widgets)
    {
        if (on)
        {
            //> Only the first entry records anything, or a second would save
            //> the disabled state this loop just imposed and call it original.
            if (!oldEnabled.contains(w)) { oldEnabled.insert(w, w->isEnabled()); }
            w->setDisabled(on);
        }
        else
        {
            //> Nothing recorded means the mode was never entered - which is
            //> the state the program starts in, since the persisted SmartFlux
            //> flag is written but never read back. Enabled is the right
            //> answer, and it is what the positional vector used to abort on.
            w->setEnabled(oldEnabled.value(w, true));
        }
    }
    if (!on) { oldEnabled.clear(); }

    if (on)
    {
        recursionCheckBox->setChecked(false);
        subsetCheckBox->setChecked(false);

        outpathBrowse->clear();

        // set the output id to a fixed string
        idEdit->setText(QStringLiteral("adv"));

        avgIntervalSpin->setValue(30);
    }
}

void BasicSettingsPage::noNoaaConnectionMsg()
{
    WidgetUtils::warning(this,
                         tr("NOAA Connection Problem"),
                         tr("<p>No connection available or connection "
                            "error updating the magnetic declination.</p>"));
}

void BasicSettingsPage::noNoaaDownloadMsg()
{
    bool showDialog
        = GlobalSettings::getAppPersistentSettings(Defs::CONFGROUP_WINDOW,
                                                   Defs::CONF_WIN_NOAA_WEBSITE_MSG,
                                                   true).toBool();
    if (!showDialog) { return; }

    // info message
    InfoMessage noaaDialog(QDialogButtonBox::Ok, nullptr);
    noaaDialog.setTitle(tr("NOAA Download Problem"));
    noaaDialog.setType(InfoMessage::Type::NOAA_WEBSITE);
    noaaDialog.setMessage(tr("<p>Server not responding or service not "
                             "available updating the magnetic "
                             "declination.</p>"));
    noaaDialog.refresh();
    noaaDialog.exec();
}

void BasicSettingsPage::createWindFilterArea()
{
    auto title = new QLabel(tr("Wind Direction Filter"));
    title->setProperty("groupLabel", true);

    windFilterApplyCheckbox = new QCheckBox(tr("Apply Wind Direction Filter"));

    setupWindFilterModel();
    setupWindFilterViews();

    auto buttonsLayout = new QVBoxLayout;
    buttonsLayout->addWidget(addButton);
    buttonsLayout->addSpacing(10);
    buttonsLayout->addWidget(removeButton);
    buttonsLayout->addStretch();
    buttonsLayout->setContentsMargins(0, 0, 0, 0);

    auto configLayout = new QGridLayout;
    configLayout->addWidget(windFilterTableView_, 0, 0, Qt::AlignCenter);
    configLayout->addLayout(buttonsLayout, 0, 1, Qt::AlignCenter);
    configLayout->addWidget(windFilterView_, 0, 2, Qt::AlignTop);
    configLayout->setVerticalSpacing(5);
    configLayout->setContentsMargins(11, 0, 0, 0);

    windFilterConfigFrame = new QWidget;
    windFilterConfigFrame->setLayout(configLayout);

    windFilterLayout = new QGridLayout;
    windFilterLayout->addWidget(title, 0, 0, Qt::AlignCenter);
    windFilterLayout->addWidget(windFilterApplyCheckbox, 1, 0, Qt::AlignCenter);
    windFilterLayout->addWidget(windFilterConfigFrame, 2, 0, Qt::AlignCenter);
    windFilterLayout->setRowStretch(3, 1);

    connect(windFilterApplyCheckbox, &QCheckBox::toggled, [=](bool checked) {
        ecProject_->setWindFilterApply(checked ? 1 : 0);
    });
}

void BasicSettingsPage::setupWindFilterModel()
{
    windFilterTableModel_ = new WindFilterTableModel(this, ecProject_->windFilterSectors());

    connect(windFilterTableModel_, &WindFilterTableModel::modified,
            this, &BasicSettingsPage::windFilterModelModified);
    connect(windFilterTableModel_, &WindFilterTableModel::modelReset,
            ecProject_, &EcProject::updateInfo);
}

void BasicSettingsPage::windFilterModelModified()
{
    ecProject_->setModified(true);
    windFilterView_->updateValidItems();
}

void BasicSettingsPage::updateWindFilterModel()
{
    windFilterTableModel_->flush();
    windFilterView_->updateValidItems();
}

void BasicSettingsPage::setupWindFilterViews()
{
    addButton = new QToolButton;
    addButton->setObjectName(QStringLiteral("plusButton"));
    addButton->setAutoRaise(true);
    { QIcon icon; icon.addPixmap(QPixmap(QStringLiteral(":/icons/plus")), QIcon::Normal, QIcon::Off); icon.addPixmap(QPixmap(QStringLiteral(":/icons/plus-hover")), QIcon::Active, QIcon::Off); icon.addPixmap(QPixmap(QStringLiteral(":/icons/plus-disabled")), QIcon::Disabled, QIcon::Off); addButton->setIcon(icon); addButton->setIconSize(QSize(18, 18)); }
    addButton->setToolTip(tr("<b>+</b> Add an angle."));

    removeButton = new QToolButton;
    removeButton->setObjectName(QStringLiteral("minusButton"));
    removeButton->setAutoRaise(true);
    { QIcon icon; icon.addPixmap(QPixmap(QStringLiteral(":/icons/minus")), QIcon::Normal, QIcon::Off); icon.addPixmap(QPixmap(QStringLiteral(":/icons/minus-hover")), QIcon::Active, QIcon::Off); icon.addPixmap(QPixmap(QStringLiteral(":/icons/minus-disabled")), QIcon::Disabled, QIcon::Off); removeButton->setIcon(icon); removeButton->setIconSize(QSize(18, 18)); }
    removeButton->setToolTip(tr("<b>-</b> Remove an angle."));

    connect(addButton, &QToolButton::clicked,
            this, &BasicSettingsPage::addWindFilterSector);
    connect(removeButton, &QToolButton::clicked,
            this, &BasicSettingsPage::removeWindFilterSector);

    windFilterTableView_ = new WindFilterTableView;
    windFilterTableView_->setModel(windFilterTableModel_);
    windFilterTableView_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    windFilterTableView_->setToolTip(tr("<b>Wind Direction Filter:</b> Visualization of the described wind sectors. Add or remove wind sectors using the <b>+</b> and <b>-</b> buttons."));

    windFilterView_ = new WindFilterView;
    windFilterView_->setModel(windFilterTableModel_);
    windFilterView_->setToolTip(tr("<b>Wind Direction Filter:</b> Polar chart of the filtered wind sectors."));
    windFilterView_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    windFilterView_->setContentsMargins(0, 0, 0, 0);

    windFilterSelectionModel_ = new QItemSelectionModel(windFilterTableModel_);
    windFilterTableView_->setSelectionModel(windFilterSelectionModel_);
    windFilterTableView_->setSelectionMode(QAbstractItemView::SingleSelection);
    windFilterView_->setSelectionModel(windFilterSelectionModel_);
    windFilterView_->setSelectionMode(QAbstractItemView::SingleSelection);

    auto hdr = windFilterTableView_->horizontalHeader();
    hdr->setModel(windFilterTableModel_);
    hdr->setStretchLastSection(true);
    hdr->setHighlightSections(false);
    hdr->setProperty("pieTableH", true);

    windFilterTableView_->verticalHeader()->setProperty("pieTableV", true);

    connect(windFilterView_, &WindFilterView::clicked,
            windFilterTableView_, qOverload<const QModelIndex &>(&QAbstractItemView::edit));
}

void BasicSettingsPage::insertAngleAt(int row)
{
    if (!windFilterTableModel_->insertRow(row))
        return;
    windFilterView_->setCurrentIndex(windFilterTableModel_->index(row - 1, 0));
    windFilterTableModel_->flush();
}

void BasicSettingsPage::removeAngleAt(int row)
{
    if (!windFilterTableModel_->removeRow(row))
        return;
    if (row > 0)
        windFilterView_->setCurrentIndex(windFilterTableModel_->index(row - 1, 0));
    windFilterTableModel_->flush();
}

void BasicSettingsPage::addWindFilterSector()
{
    int selected = windFilterSelectionModel_->currentIndex().row();
    int last = windFilterTableModel_->rowCount();
    insertAngleAt(selected < 0 ? last : selected + 1);
    resizeWindFilterRows();
}

void BasicSettingsPage::removeWindFilterSector()
{
    int selected = windFilterSelectionModel_->currentIndex().row();
    int last = windFilterTableModel_->rowCount();
    if (last > 0)
        removeAngleAt(selected < 0 ? last - 1 : selected);
    resizeWindFilterRows();
}

void BasicSettingsPage::resizeWindFilterRows()
{
    for (int i = 0; i < windFilterTableModel_->rowCount(); ++i)
        windFilterTableView_->resizeRowToContents(i);
}


