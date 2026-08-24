/***************************************************************************
  advspectraloptions.cpp
  -------------------
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

#include "advspectraloptions.h"

#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QAbstractTableModel>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QPixmap>
#include <QSize>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QHeaderView>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTimer>
#include <QTimeEdit>
#include <QUrl>
#include <QVector>

#include <cmath>

#include "ancillaryfiletest.h"
#include "clicklabel.h"
#include "customcombomodel.h"
#include "customclearlineedit.h"
#include "dirbrowsewidget.h"
#include "dlproject.h"
#include "ecproject.h"
#include "fileutils.h"
#include "filebrowsewidget.h"
#include "globalsettings.h"
#include "measurement_record.h"
#include "variable_desc.h"
#include "widget_utils.h"

namespace {

class AdaptivePrecisionDoubleSpinBox final : public QDoubleSpinBox
{
public:
    explicit AdaptivePrecisionDoubleSpinBox(QWidget* parent = nullptr)
        : QDoubleSpinBox(parent)
    {}

protected:
    QString textFromValue(double value) const override
    {
        const double roundedToFour = std::round(value * 10000.0) / 10000.0;
        const bool needsExtraPrecision = value != 0.0
                && (std::abs(value) < 0.0001
                    || std::abs(value - roundedToFour) > 0.0000005);

        QString text = locale().toString(value, 'f', needsExtraPrecision ? 6 : 4);
        if (needsExtraPrecision)
        {
            const QString decimalPoint = locale().decimalPoint();
            while (text.contains(decimalPoint) && text.endsWith(QLatin1Char('0')))
            {
                text.chop(1);
            }
            if (text.endsWith(decimalPoint))
            {
                text.chop(1);
            }
        }
        return text;
    }
};

struct SpectralQaQcRow
{
    QString label;
    QDoubleSpinBox* noiseFrequency = nullptr;
    QDoubleSpinBox* minUnstable = nullptr;
    QDoubleSpinBox* minStable = nullptr;
    QDoubleSpinBox* maximum = nullptr;
    QDoubleSpinBox* lowestFrequency = nullptr;
    QDoubleSpinBox* highestFrequency = nullptr;
};

class SpectralQaQcTableModel final : public QAbstractTableModel
{
public:
    enum Column
    {
        NoiseFrequency = 0,
        MinUnstable,
        MinStable,
        Maximum,
        LowestFrequency,
        HighestFrequency,
        ColumnCount
    };

    SpectralQaQcTableModel(QObject* parent,
                           const QVector<SpectralQaQcRow>& rows,
                           const QStringList& tooltips)
        : QAbstractTableModel(parent),
          rows_(rows),
          tooltips_(tooltips)
    {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : rows_.size();
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : ColumnCount;
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
    {
        if (orientation == Qt::Vertical)
        {
            return role == Qt::DisplayRole && section >= 0 && section < rows_.size()
                    ? rows_.at(section).label
                    : QVariant();
        }

        if (role == Qt::ToolTipRole && section >= 0 && section < tooltips_.size())
        {
            return tooltips_.at(section);
        }
        if (role != Qt::DisplayRole) { return QVariant(); }

        switch (section)
        {
            case NoiseFrequency: return tr("Lowest noise frequency");
            case MinUnstable: return tr("Minimum unstable");
            case MinStable: return tr("Minimum stable");
            case Maximum: return tr("Maximum");
            case LowestFrequency: return tr("Lowest frequency");
            case HighestFrequency: return tr("Highest frequency");
            default: return QVariant();
        }
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || !spinAt(index))
        {
            return QVariant();
        }

        if (role == Qt::TextAlignmentRole)
        {
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        }
        if (role == Qt::ToolTipRole && index.column() >= 0 && index.column() < tooltips_.size())
        {
            return tooltips_.at(index.column());
        }
        if (role == Qt::EditRole)
        {
            return spinAt(index)->value();
        }
        if (role == Qt::DisplayRole)
        {
            return spinAt(index)->text();
        }
        return QVariant();
    }

    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override
    {
        auto spin = spinAt(index);
        if (role != Qt::EditRole || !spin)
        {
            return false;
        }

        spin->setValue(value.toDouble());
        emit dataChanged(index, index, { Qt::DisplayRole, Qt::EditRole });
        return true;
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        if (!index.isValid() || !spinAt(index))
        {
            return Qt::NoItemFlags;
        }
        const auto spin = spinAt(index);
        Qt::ItemFlags itemFlags = Qt::ItemIsSelectable;
        if (spin->isEnabled())
        {
            itemFlags |= Qt::ItemIsEnabled | Qt::ItemIsEditable;
        }
        return itemFlags;
    }

    QDoubleSpinBox* spinAt(const QModelIndex& index) const
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size())
        {
            return nullptr;
        }
        const auto& row = rows_.at(index.row());
        switch (index.column())
        {
            case NoiseFrequency: return row.noiseFrequency;
            case MinUnstable: return row.minUnstable;
            case MinStable: return row.minStable;
            case Maximum: return row.maximum;
            case LowestFrequency: return row.lowestFrequency;
            case HighestFrequency: return row.highestFrequency;
            default: return nullptr;
        }
    }

    QModelIndex firstEditableIndex(int column) const
    {
        for (int row = 0; row < rows_.size(); ++row)
        {
            const QModelIndex candidate = index(row, column);
            if (flags(candidate).testFlag(Qt::ItemIsEditable))
            {
                return candidate;
            }
        }
        return QModelIndex();
    }

    void refreshAll()
    {
        if (rows_.isEmpty()) { return; }
        emit dataChanged(index(0, 0),
                         index(rows_.size() - 1, ColumnCount - 1),
                         { Qt::DisplayRole, Qt::EditRole });
    }

    void setRows(const QVector<SpectralQaQcRow>& rows)
    {
        beginResetModel();
        rows_ = rows;
        endResetModel();
    }

private:
    QVector<SpectralQaQcRow> rows_;
    QStringList tooltips_;
};

class SpectralQaQcDelegate final : public QStyledItemDelegate
{
public:
    explicit SpectralQaQcDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
    {}

    QWidget* createEditor(QWidget* parent,
                          const QStyleOptionViewItem& option,
                          const QModelIndex& index) const override
    {
        Q_UNUSED(option)
        const auto model = dynamic_cast<const SpectralQaQcTableModel*>(index.model());
        if (!model) { return nullptr; }
        const auto sourceSpin = model->spinAt(index);
        if (!sourceSpin) { return nullptr; }

        QDoubleSpinBox* editor = (index.column() == SpectralQaQcTableModel::MinUnstable
                                  || index.column() == SpectralQaQcTableModel::MinStable)
                ? static_cast<QDoubleSpinBox*>(new AdaptivePrecisionDoubleSpinBox(parent))
                : new QDoubleSpinBox(parent);
        editor->setRange(sourceSpin->minimum(), sourceSpin->maximum());
        editor->setDecimals(sourceSpin->decimals());
        editor->setSingleStep(sourceSpin->singleStep());
        editor->setSuffix(sourceSpin->suffix());
        editor->setSpecialValueText(sourceSpin->specialValueText());
        editor->setAccelerated(sourceSpin->isAccelerated());
        editor->setToolTip(sourceSpin->toolTip());
        return editor;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override
    {
        if (auto spin = qobject_cast<QDoubleSpinBox*>(editor))
        {
            spin->setValue(index.data(Qt::EditRole).toDouble());
        }
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override
    {
        if (auto spin = qobject_cast<QDoubleSpinBox*>(editor))
        {
            model->setData(index, spin->value(), Qt::EditRole);
        }
    }
};

void refreshSpectralQaQcTableView(QAbstractTableModel* model, QTableView* table)
{
    if (model)
    {
        static_cast<SpectralQaQcTableModel*>(model)->refreshAll();
    }
    if (table)
    {
        table->viewport()->update();
    }
}

} // namespace

AdvSpectralOptions::AdvSpectralOptions(QWidget *parent,
                                       DlProject *dlProject,
                                       EcProject *ecProject,
                                       ConfigState *config) :
    QWidget(parent),
    dlProject_(dlProject),
    ecProject_(ecProject),
    configState_(config)
{
    settingsGroupTitle_1 = new QLabel(tr("Spectra and Cospectra Calculation"));
    settingsGroupTitle_1->setProperty("groupTitle2", true);
    settingsGroupTitle_1->setStyleSheet(
            QStringLiteral("QLabel { margin: 2px 0px 3px -2px; padding: 0px; }"));

    // spectra and cospectra section
    binnedSpectraNonExistingRadio = new QRadioButton(tr("Binned (co)spectra files not available"));
    binnedSpectraNonExistingRadio->setToolTip(tr("<b>Binned (co)spectra files not available:</b> Select this option if you did not yet obtain <i>Binned spectra and cospectra files</i> for the current dataset in a previous run of EddyFlow. Note that such binned (co)spectra files do not need to correspond exactly to the current dataset, rather they need to be representative of it. Binned (co)spectra files are used by certain spectral corrections procedures to quantify spectral attenuations, thus they must have been collected in conditions comparable to those of the current dataset (e.g., same EC system and similar canopy heights, measurement height, instrument spatial separations, etc.). At least one month of spectra files is needed for a robust spectral attenuation assessment. If you select this option, the option <i>All binned spectra and cospectra</i> in the Output Files page will be automatically selected."));

#if defined(Q_OS_MACOS)
    binnedSpectraNonExistingRadio->setStyleSheet(QStringLiteral("QRadioButton { margin-left: 5px; }"));
#elif defined(Q_OS_WIN)
    binnedSpectraNonExistingRadio->setStyleSheet(QStringLiteral("QRadioButton { margin-left: 1px; }"));
#endif

    binnedSpectraExistingRadio = new QRadioButton(tr("Binned (co)spectra files available for this dataset :"));
    binnedSpectraExistingRadio->setToolTip(tr("<b>Binned (co)spectra files available:</b> Select this option if you already obtained <i>Binned spectra and cospectra files</i> for the current dataset in a previous run of EddyFlow. Note that the binned (co)spectra files do not need to correspond exactly to the current dataset, rather they need to be representative of it. Binned (co)spectra are used here for quantification of spectral attenuations, thus they must have been collected in conditions comparable to those of the current dataset (e.g., the same EC system and similar canopy heights, measurement height, instrument spatial separations). At least one month of spectra files is needed for a robust spectral attenuation assessment. If you select this option, the option <i>All binned spectra and cospectra</i> in the Output Files page will be automatically deselected and activated."));
    binnedSpectraExistingRadio->setStyleSheet(QStringLiteral("QRadioButton { margin-right: 0px; }"));

    binnedSpectraDirBrowse = new DirBrowseWidget;
    binnedSpectraDirBrowse->setToolTip(tr("<b>Browse:</b> Specify the folder that contains the binned (co)spectra files."));
    binnedSpectraDirBrowse->setDialogWorkingDir(WidgetUtils::getDialogPathHint(QStringLiteral("binned_cospectra_dir")));
    binnedSpectraDirBrowse->setDialogTitle(tr("Select the Binned (Co)Spectra Files Directory"));

    binnedSpectraRadioGroup = new QButtonGroup(this);
    binnedSpectraRadioGroup->addButton(binnedSpectraNonExistingRadio, 0);
    binnedSpectraRadioGroup->addButton(binnedSpectraExistingRadio, 1);

    subsetCheckBox = new QCheckBox;
    subsetCheckBox->setText(tr("Select a different period"));
    subsetCheckBox->setToolTip(tr("<b>Select a different period:</b> Select the starting and ending date of the period you want the use (co)spectra from. If you selected the option <i>Binned (co)spectra files not available</i>, then this subperiod must overlap, at least partially, with that covered by available raw data or with the subperiod selected in the Basic Settings page, if one was selected. If you selected the option Binned (co)spectra files available for this dataset, then this subperiod must overlap, at least partially, with that covered by available (co)spectra files."));
    subsetCheckBox->setStyleSheet(QStringLiteral("QCheckBox {margin-left: 40px;}"));

    lockedIcon = new QLabel;
    auto pixmap_2x = QPixmap(QStringLiteral(":/icons/vlink-locked"));
#if defined(Q_OS_MACOS)
    pixmap_2x.setDevicePixelRatio(2.0);
#endif
    lockedIcon->setPixmap(pixmap_2x);

    startDateLabel = new ClickLabel;
    startDateLabel->setText(tr("Start :"));
    startDateLabel->setToolTip(tr("<b>Start:</b> Beginning of the period to use (co)spectra from. If (co)spectra will be used in spectral corrections, we recommend using a time period that is as long as possible. However, make sure that the instrument setup (sampling line, instrument separations) did not undergo any major change during the selected time period."));
    startDateEdit = new QDateEdit;
    startDateEdit->setToolTip(startDateLabel->toolTip());
    startDateEdit->setCalendarPopup(true);
    startDateEdit->setDisplayFormat(WidgetUtils::eddyDateFormat());
    WidgetUtils::customizeCalendar(startDateEdit->calendarWidget());

    startTimeEdit = new QTimeEdit;
    startTimeEdit->setDisplayFormat(QStringLiteral("hh:mm"));
    startTimeEdit->setAccelerated(true);

    endDateLabel = new ClickLabel;
    endDateLabel->setText(tr("End :"));
    endDateLabel->setToolTip(tr("<b>End:</b> End of the period to use (co)spectra from. If (co)spectra will be used in spectral corrections, we recommend using a time period that is as long as possible. However, make sure that the instrument setup (sampling line, instrument separations) did not undergo any major change during the selected time period."));
    endDateEdit = new QDateEdit;
    endDateEdit->setToolTip(endDateLabel->toolTip());
    endDateEdit->setCalendarPopup(true);
    endDateEdit->setDisplayFormat(WidgetUtils::eddyDateFormat());
    WidgetUtils::customizeCalendar(endDateEdit->calendarWidget());

    endTimeEdit = new QTimeEdit;
    endTimeEdit->setDisplayFormat(QStringLiteral("hh:mm"));
    endTimeEdit->setAccelerated(true);

    auto dateTimeContainer = new QGridLayout;
    dateTimeContainer->addWidget(startDateEdit, 0, 1);
    dateTimeContainer->addWidget(startTimeEdit, 0, 2);
    dateTimeContainer->addWidget(lockedIcon, 0, 0, 2, 1);
    dateTimeContainer->addWidget(endDateEdit, 1, 1);
    dateTimeContainer->addWidget(endTimeEdit, 1, 2);
    dateTimeContainer->setColumnStretch(1, 1);
    dateTimeContainer->setColumnStretch(2, 1);
    dateTimeContainer->setColumnStretch(3, 2);
    dateTimeContainer->setContentsMargins(0, 0, 0, 0);

    // FFT section
    filterLabel = new ClickLabel(tr("Tapering window :"));
    filterLabel->setToolTip(tr("<b>Tapering window:</b> Select the shape of the window used to taper the time series before the Fast Fourier Transform. The tapering procedure is a sample-wise multiplication in the time domain between the time series and the window, performed to reduce the discontinuities of the time series at the boundaries and avoid spectral energy overestimation. Kaimal & Kristensen (1991) suggested the Hamming window."));
    filterCombo = new QComboBox;
    filterCombo->setToolTip(filterLabel->toolTip());
    filterCombo->addItem(tr("Squared (no window)"));
    filterCombo->addItem(tr("Bartlett"));
    filterCombo->addItem(tr("Welch"));
    filterCombo->addItem(tr("Hamming"));
    filterCombo->addItem(tr("Hann"));

    nBinsLabel = new ClickLabel(tr("Frequency bins for (co)spectra reduction :"));
    nBinsLabel->setToolTip(tr("<b>Frequency bins for spectra and cospectra reduction:</b> Select the number of exponentially-spaced frequency bins to reduce spectra and cospectra. All spectral samples falling in a given bin are averaged, so that smoother curves result, greatly reduced in length. In EddyFlow binned (co)spectra are used for in-situ spectral assessments and for calculation of ensemble averaged (co)spectra."));
    nBinsSpin = new QSpinBox;
    nBinsSpin->setToolTip(nBinsLabel->toolTip());
    nBinsSpin->setRange(10, 3000);
    nBinsSpin->setSingleStep(10);
    nBinsSpin->setAccelerated(true);
    nBinsSpin->setValue(50);

    fftCheckBox = new QCheckBox(tr("Use power-of-two samples to speed up the FFT"));
    fftCheckBox->setToolTip(tr("<b>Use power-of-two samples to speed up the FFT: </b>Check this box to instruct EddyFlow to use a number of samples equal to the power-of-two closest to the currently available samples, for calculating spectra. This option greatly speeds up the FFT procedure and is therefore recommended."));
    fftCheckBox->setStyleSheet(QStringLiteral("QCheckBox { margin-left: 40px; }"));

    // The per-gas noise-frequency spins are generated in
    // rebuildGasSpectralSpins(), one set per gas record.
    noiseFrequencyTip_ = tr("<b>Lowest noise frequency:</b> High-frequency noise (blue noise) can compromise the spectral assessment by modifying the shape of spectra. EddyFlow has an option to eliminate such noise. Set the minimum frequency at which you expect the noise to start being relevant. EddyFlow will linearly (in a log-log sense) interpolate the high frequency portion of the spectra and subtract it from the spectra before calculating transfer functions. Set 0 Hz to instruct EddyFlow to not perform noise elimination. In this case the string <i>Do not remove noise</i> will appear in this field.");
    const QString noiseFrequencyTooltip = noiseFrequencyTip_;

    // QA/QC section
    minUnstableTip_ = tr("<b>Minimum, unstable flux:</b> when fluxes are below these minima, corresponding cospectra are excluded from calculation of ensemble averaged cospectra in unstable stratifications, and corresponding spectra are excluded from calculation ensemble averaged spectra. For more details, click on the question mark at the right side of the title of this section <i>Spectra and cospectra QA/QC</i>.");
    minStableTip_ = tr("<b>Minimum, stable flux:</b> when fluxes are below these minima, corresponding cospectra are excluded from calculation of ensemble averaged cospectra in stable stratifications. For more details, click on the question mark at the right side of the title of this section <i>Spectra and cospectra QA/QC</i>.");
    maxTip_ = tr("<b>Maximum :</b> when fluxes are above these maxima, corresponding (co)spectra are excluded from any ensemble averaging procedure. Maxima are meant to exclude spikes or periods characterized by abnormal fluxes. For more details, click on the question mark at the right side of the title of this section <i>Spectra and cospectra QA/QC</i>.");
    const QString minUnstableTooltip = minUnstableTip_;
    const QString minStableTooltip = minStableTip_;
    const QString maxTooltip = maxTip_;

    qcMinUnstableUstarSpin = new AdaptivePrecisionDoubleSpinBox;
    qcMinUnstableUstarSpin->setRange(0.0, 5.0);
    qcMinUnstableUstarSpin->setSingleStep(0.05);
    qcMinUnstableUstarSpin->setDecimals(6);
    qcMinUnstableUstarSpin->setSuffix(QStringLiteral(" [m/s]"));
    qcMinUnstableUstarSpin->setAccelerated(true);

    qcMinUnstableHSpin = new AdaptivePrecisionDoubleSpinBox;
    qcMinUnstableHSpin->setRange(0.0, 10000.0);
    qcMinUnstableHSpin->setSingleStep(10.0);
    qcMinUnstableHSpin->setDecimals(6);
    qcMinUnstableHSpin->setSuffix(tr(" [%1]").arg(Defs::W_M2_STRING));
    qcMinUnstableHSpin->setAccelerated(true);

    qcMinUnstableLESpin = new AdaptivePrecisionDoubleSpinBox;
    qcMinUnstableLESpin->setRange(0.0, 10000.0);
    qcMinUnstableLESpin->setSingleStep(10.0);
    qcMinUnstableLESpin->setDecimals(6);
    qcMinUnstableLESpin->setSuffix(tr(" [%1]").arg(Defs::W_M2_STRING));
    qcMinUnstableLESpin->setAccelerated(true);

    qcMinStableUstarSpin = new AdaptivePrecisionDoubleSpinBox;
    qcMinStableUstarSpin->setRange(0.0, 5.0);
    qcMinStableUstarSpin->setSingleStep(0.05);
    qcMinStableUstarSpin->setDecimals(6);
    qcMinStableUstarSpin->setSuffix(QStringLiteral(" [m/s]"));
    qcMinStableUstarSpin->setAccelerated(true);

    qcMinStableHSpin = new AdaptivePrecisionDoubleSpinBox;
    qcMinStableHSpin->setRange(0.0, 10000.0);
    qcMinStableHSpin->setSingleStep(10.0);
    qcMinStableHSpin->setDecimals(6);
    qcMinStableHSpin->setSuffix(tr(" [%1]").arg(Defs::W_M2_STRING));
    qcMinStableHSpin->setAccelerated(true);

    qcMinStableLESpin = new AdaptivePrecisionDoubleSpinBox;
    qcMinStableLESpin->setRange(0.0, 10000.0);
    qcMinStableLESpin->setSingleStep(10.0);
    qcMinStableLESpin->setDecimals(6);
    qcMinStableLESpin->setSuffix(tr(" [%1]").arg(Defs::W_M2_STRING));
    qcMinStableLESpin->setAccelerated(true);

    qcMaxUstarSpin = new AdaptivePrecisionDoubleSpinBox;
    qcMaxUstarSpin->setRange(0.0, 5.0);
    qcMaxUstarSpin->setSingleStep(0.1);
    qcMaxUstarSpin->setDecimals(6);
    qcMaxUstarSpin->setSuffix(QStringLiteral(" [m/s]"));
    qcMaxUstarSpin->setAccelerated(true);

    qcMaxHSpin = new AdaptivePrecisionDoubleSpinBox;
    qcMaxHSpin->setRange(0.0, 10000.0);
    qcMaxHSpin->setSingleStep(100.0);
    qcMaxHSpin->setDecimals(6);
    qcMaxHSpin->setSuffix(tr(" [%1]").arg(Defs::W_M2_STRING));
    qcMaxHSpin->setAccelerated(true);

    qcMaxLESpin = new AdaptivePrecisionDoubleSpinBox;
    qcMaxLESpin->setRange(0.0, 10000.0);
    qcMaxLESpin->setSingleStep(100.0);
    qcMaxLESpin->setDecimals(6);
    qcMaxLESpin->setSuffix(tr(" [%1]").arg(Defs::W_M2_STRING));
    qcMaxLESpin->setAccelerated(true);

    vmFlagsCheckBox = new QCheckBox(tr("Filter (co)spectra according "
                                       "to Vickers and Mahrt (1997) "
                                       "test results  "));
    vmFlagsCheckBox->setToolTip(tr("<b>Filter (co)spectra according"
                                   "to Vickers and Mahrt (1997) "
                                   "test results:</b> check this option to exclude from ensemble averages (co)spectra for periods, during which the corresponding time series were flagged by the statistical tests found in the Statistical Analysis page. For more details, click on the question mark at the right side of the title of this section <i>Spectra and cospectra QA/QC</i>."));
    vmFlagsCheckBox->setStyleSheet(QStringLiteral("QCheckBox { margin-left: 40px; }"));

    // filter cospectra mauder section
    auto filterCospectraMauderTitle
        = new QLabel(tr("Filter (co)spectra "
                        "according to micrometeorological quality test results "
                        "(Mauder and Foken, 2004)"));
    filterCospectraMauderTitle->setProperty("groupLabel", true);

    lowQualityCheckBox = new QCheckBox(tr("Low data quality (flag value = 2)"));
    lowQualityCheckBox->setToolTip(tr("<b>Low data quality:</b> "
                                      "check this option to exclude from ensemble averages (co)spectra for periods, during which the corresponding fluxes where flagged for low quality by Foken’s micrometeorological tests. For more details, click on the question mark at the right side of the title of this section <i>Spectra and cospectra QA/QC</i>."));
    lowQualityCheckBox->setStyleSheet(QStringLiteral("QCheckBox { margin-left: 40px; }"));

    moderateQualityCheckBox = new QCheckBox(tr("Moderate data quality "
                                               "(flag value = 1)"));
    moderateQualityCheckBox->setToolTip(tr("<b>Moderate data quality:</b> "
                                           "check this option to exclude from ensemble averages (co)spectra for periods, during which the corresponding fluxes where flagged for moderate quality by Foken’s micrometeorological tests. Note that choosing these options, forces the pair option ‘low data quality’ to be automatically selected. For more details, click on the question mark at the right side of the title of this section <i>Spectra and cospectra QA/QC</i>."));
    moderateQualityCheckBox->setStyleSheet(QStringLiteral("QCheckBox { margin-left: 40px; }"));

    // Spectral corrections title
    auto spectralCorrectionTitle = new QLabel(tr("Spectral Correction Options"));
    spectralCorrectionTitle->setProperty("groupTitle2", true);

    // low freq section
    lowFreqTitle = new QLabel(tr("Low frequency range"));
    lowFreqTitle->setProperty("groupLabel", true);

    lfMethodCheck = new QCheckBox(tr("Analytic correction of high-pass filtering effects (Moncrieff et al. 2004)   "));
    lfMethodCheck->setToolTip(tr("<b>Analytic correction of high-pass filtering effects:</b> Check this option to apply a low frequency spectral correction, to compensate flux losses due to finite averaging length and detrending. The method is adapted to the selected fluctuation computation procedure and its time constant as applicable."));
    lfMethodCheck->setStyleSheet(QStringLiteral("QCheckBox { margin-left: 40px; }"));

    // high freq section
    highFreqTitle = new QLabel(tr("High frequency range"));
    highFreqTitle->setProperty("groupLabel", true);

    hfMethodCheck = new QCheckBox(tr("Correction of low-pass filtering effects"));
    hfMethodCheck->setToolTip(tr("<b>Correction of low-pass filtering effects:</b> Check this option to apply a high frequency spectral correction, to compensate flux losses due to finite sensors separation, signal attenuation, path averaging, time response, etc. Select the most appropriate method according to your EC setup."));
    hfMethodCheck->setStyleSheet(QStringLiteral("QCheckBox { margin-left: 40px; }"));

    hfMethLabel = new ClickLabel(tr("Method :"));
    hfMethCombo = new QComboBox;
    hfMethCombo->setModel(new CustomComboModel(hfMethCombo));
    hfMethCombo->addItem(tr("Moncrieff et al. (1997) - Fully analytic"));
    hfMethCombo->addItem(tr("Massmann (2000, 2001) - Fully analytic"));
    hfMethCombo->addItem(tr("Horst (1997) - Analytic with in situ parameterization"));
    hfMethCombo->addItem(tr("Ibrom et al. (2007) - In situ/analytic"));
    hfMethCombo->addItem(tr("Fratini et al. (2012) - In situ/analytic"));
    hfMethCombo->setItemData(0, tr("<b>Moncrieff et al. (1997):</b> This method models all major sources of flux attenuation by means of a mathematical formulation. The use of this method is suggested for open path EC systems or for closed path systems if the sampling line is short and heated. This method may seriously underestimate the attenuation (and hence the correction) - notably for water vapor - when the sampling line is long and/or not heated, because of the dependency of attenuation of H<sub>2</sub>O on relative humidity."), Qt::ToolTipRole);
    hfMethCombo->setItemData(1, tr("<b>Massmann (2000, 2001):</b> This method provides a simple analytical expression for the spectral correction factors. The use of this method is suggested for open path EC systems or for closed path systems if the sampling line is short and heated. This method may seriously underestimate the attenuation (and hence the correction) for water vapor, when the sampling line is long and/or not heated, because of the dependency of attenuation of %2 on relative humidity. For closed path systems, this method is only applicable for %1, %2, %3, %4 and %5 fluxes.").arg(Defs::CO2_STRING, Defs::H2O_STRING, Defs::CH4_STRING, Defs::N2O_STRING, Defs::O3_STRING), Qt::ToolTipRole);
    hfMethCombo->setItemData(3, tr("<b>Horst (1997):</b> Correction method based on an analytical formulation of the spectral correction factor that requires an in-situ assessment of the system's cut-off frequency. Provide the information below to specify how to perform such assessment."), Qt::ToolTipRole);
    hfMethCombo->setItemData(4, tr("<b>Ibrom et al. (2007):</b> Correction method based on an analytical formulation of the spectra correction factors, that requires an in-situ assessment of the system's cut-off frequencies, separately for each instrument and gas, and as a function of relative humidity for water vapor. Provide the settings in the <i>Assessment of high-frequency attenuation</i> to specify how to perform the assessment. This method is recommended in most cases, notably for closed-path systems placed high over rough canopies."), Qt::ToolTipRole);
    hfMethCombo->setItemData(5, tr("<b>Fratini et al. (2012):</b> Correction method based on the combination of a direct approach (similar to Hollinger et al., 2009) and the analytical formulation of Ibrom et al., 2007. It requires an in-situ assessment of the system's cut-off frequencies, separately for each instrument and gas, and as a function of relative humidity for water vapor. It also requires full length cospectra of measured sensible heat. This method is recommendable in most cases, notably for closed-path systems placed low over smooth surfaces."), Qt::ToolTipRole);

    horstCheck = new QCheckBox(tr("Correction for instruments separation"));
    horstCheck->setToolTip(tr("<b>Correction for instrument separation:</b> Check this option and select the corresponding method to add an extra correction term to that calculated with the method by Ibrom et al. (2007). This accounts for any separation between the inlet of the sampling line (closed path instruments) or the center of the open path instrument and the center of the anemometer."));
    horstCheck->setStyleSheet(QStringLiteral("QCheckBox { margin-left: 40px; }"));
    horstMethodLabel = new ClickLabel(tr("Method :"));
    horstCombo = new QComboBox;
    horstCombo->addItem(tr("Horst and Lenschow (2009), along-wind, crosswind and vertical"));
    horstCombo->addItem(tr("Horst and Lenschow (2009), only crosswind and vertical"));
    horstCombo->setItemData(0, tr("<b>Horst and Lenschow (2009), along-wind, crosswind and vertical:</b> Select this option to account for sensor separations in any direction. Note that correcting for along-wind separations may result in overcorrection, if any time lag compensation method was also selected."), Qt::ToolTipRole);
    horstCombo->setItemData(1, tr("<b>Horst and Lenschow (2009), only crosswind and vertical:</b> Select this option to account for sensor separations only in the crosswind and vertical directions. Recommended when a time lag compensation method is selected."), Qt::ToolTipRole);

    //> The analytic cospectral SHAPE, not a correction method. Every method
    //> above weights a transfer function by it, so it is a modifier on all of
    //> them rather than a sixth entry in their list.
    //> Iterative correction. Under the cospectral model because it is the
    //> same circle: the model is evaluated at z/L, and z/L is what the
    //> corrected heat flux produces.
    corrIterCheckBox = new QCheckBox(tr("Iterate the correction"));
    corrIterCheckBox->setStyleSheet(QStringLiteral("QCheckBox { margin-left: 40px; }"));
    const QString corrIterTip = tr("<b>Iterate the correction:</b> The "
        "analytic cospectrum is evaluated at z/L; z/L comes from the "
        "corrected sensible heat flux; and that flux is what the spectral "
        "correction produces. A single pass leaves the three disagreeing - "
        "the correction was computed at a stability the run then revised."
        "<br><br>Repeating closes the circle. Each pass re-corrects the same "
        "raw covariances at the stability the pass before it produced, so "
        "nothing compounds; what changes is only which z/L the cospectrum "
        "was evaluated at."
        "<br><br>Largest effect in strongly non-neutral conditions, where "
        "the correction factor is most sensitive to stability. On a "
        "near-neutral forest day it moves the fluxes by hundredths of a "
        "percent, and the <i>corr_iter_dev</i> column in the full output "
        "says by how much for every period."
        "<br><br>This is EddyUH's behaviour (EddyUH.m:722-903), which "
        "iterates unconditionally. Off here, because a single pass is what "
        "this program has always done.");
    corrIterCheckBox->setToolTip(corrIterTip);

    corrIterMaxLabel = new ClickLabel(tr("Passes :"));
    corrIterMaxSpin = new QSpinBox;
    corrIterMaxSpin->setRange(1, 20);
    corrIterMaxSpin->setSingleStep(1);
    corrIterMaxSpin->setAccelerated(true);
    corrIterMaxSpin->setToolTip(tr("<b>Passes:</b> How many times to repeat "
        "the correction. Four is EddyUH's, which runs exactly that many and "
        "tests nothing. One is the same as switching this off."));
    corrIterMaxLabel->setToolTip(corrIterMaxSpin->toolTip());

    corrIterTolLabel = new ClickLabel(tr("Stop below :"));
    corrIterTolSpin = new QDoubleSpinBox;
    corrIterTolSpin->setDecimals(2);
    corrIterTolSpin->setRange(0.0, 100.0);
    corrIterTolSpin->setSingleStep(0.1);
    corrIterTolSpin->setAccelerated(true);
    corrIterTolSpin->setSuffix(tr("  [%]"));
    corrIterTolSpin->setSpecialValueText(tr("run every pass"));
    corrIterTolSpin->setToolTip(tr("<b>Stop below:</b> Stop early once every "
        "gas flux moves by less than this between passes."
        "<br><br>Zero means run every pass, and is the default because it is "
        "what EddyUH does - its loop has no early exit at all. Setting a "
        "tolerance is this program's own addition: it saves passes, and it "
        "changes the answer slightly, because stopping at pass two is not "
        "the same as stopping at pass four."));
    corrIterTolLabel->setToolTip(corrIterTolSpin->toolTip());

    cospModelLabel = new ClickLabel(tr("Cospectral model :"));
    cospModelCombo = new QComboBox;
    cospModelCombo->addItem(tr("Moncrieff et al. (1997) - the default"));
    cospModelCombo->addItem(tr("Kaimal et al. (1972)"));
    cospModelCombo->addItem(tr("Sakai et al. (2001) - rough surfaces"));
    cospModelCombo->addItem(tr("Su et al. (2003) - forest, non-flat terrain"));
    cospModelCombo->addItem(tr("Moraes et al. (2008)"));
    cospModelCombo->addItem(tr("Kristensen et al. (1997)"));
    const QString cospShared = tr("<br><br>Only the SHAPE matters: the correction "
        "is a ratio of two integrals of this same curve, so any constant "
        "scaling it divides out. The Reynolds stress keeps Moncrieff's "
        "momentum cospectrum whichever option is chosen - the four "
        "single-form models below are scalar cospectra and have no momentum "
        "counterpart.");
    const QString cospNeutral = tr("<br><br><b>No stability dependence.</b> This "
        "is a single form applied at any z/L. Under stable stratification the "
        "cospectral peak moves to higher frequency, so a neutral-form model "
        "puts too little flux there and will understate the high-frequency "
        "loss.");
    cospModelCombo->setItemData(0, QString(tr("<b>Moncrieff et al. (1997):</b> The "
        "curve this program has always integrated against, with separate "
        "stable and unstable branches. Identical, term for term, to the "
        "cospectrum Moore (1986) gives and that EddyUH calls CMoore.")
        + cospShared),
        Qt::ToolTipRole);
    cospModelCombo->setItemData(1, QString(tr("<b>Kaimal et al. (1972):</b> The Kansas "
        "cospectrum, with stable and unstable branches of its own. A "
        "genuinely different curve from Moncrieff's, though the two agree "
        "closely - Moncrieff's is a fit to this data.") + cospShared),
        Qt::ToolTipRole);
    cospModelCombo->setItemData(2, QString(tr("<b>Sakai et al. (2001):</b> Fitted over "
        "rough surfaces, where more of the flux sits at low frequency than "
        "Kaimal's curve allows.") + cospNeutral + cospShared),
        Qt::ToolTipRole);
    cospModelCombo->setItemData(3, QString(tr("<b>Su et al. (2003):</b> Fitted over "
        "two mixed hardwood forests in non-flat terrain.") + cospNeutral
        + cospShared),
        Qt::ToolTipRole);
    cospModelCombo->setItemData(4, QString(tr("<b>Moraes et al. (2008):</b> Fitted "
        "across differing surface boundary conditions.") + cospNeutral
        + cospShared),
        Qt::ToolTipRole);
    cospModelCombo->setItemData(5, QString(tr("<b>Kristensen et al. (1997):</b> A "
        "broader curve than the others, with a long low-frequency tail. On "
        "the test dataset it gives the largest correction of the six.")
        + cospNeutral + cospShared),
        Qt::ToolTipRole);
    const QString cospTip = tr("<b>Cospectral model:</b> The analytic cospectrum "
        "that every low-pass correction is integrated against. It decides how "
        "much of the flux is assumed to sit at the frequencies the instrument "
        "attenuates, and so how large the correction comes out - on the test "
        "dataset the six models spread the gas correction factors over about "
        "four percent. It is a modifier on whichever method is selected "
        "above, not a method of its own, and it has no effect where the "
        "correction is taken from measured cospectra instead."
        "<br><br>The library is EddyUH's, from its diagnostic plots; EddyUH "
        "corrects against measured and fitted cospectra rather than these. "
        "<b>Moncrieff et al. (1997) is the default</b> and is what this "
        "program has always used.");
    cospModelLabel->setToolTip(cospTip);
    cospModelCombo->setToolTip(cospTip);

    ////////////////////////////////////////////////////////////////////////////////
    // NOTE: explicitely disabled
    ghgSystemCorrectionTitle = new QLabel(tr("Data acquisition system correction (only GHG files collected with LI-7550 software 7.6.0 or earlier)"));
    ghgSystemCorrectionTitle->setProperty("groupLabel", true);
    ghgSystemCorrectionTitle->setVisible(false);

    hfCorrectGhgBaCheck = new QCheckBox(tr("Digital block averaging"));
    hfCorrectGhgBaCheck->setToolTip(tr("<b>Digital block averaging:</b> ..."));
    hfCorrectGhgBaCheck->setStyleSheet(QStringLiteral("QCheckBox { margin-left: 40px; }"));
    hfCorrectGhgBaCheck->setVisible(false);

    hfCorrectGhgZohCheck = new QCheckBox(tr("DAC zero-order hold"));
    hfCorrectGhgZohCheck->setToolTip(tr("<b>DAC zero-order hold:</b> ..."));
    hfCorrectGhgZohCheck->setStyleSheet(QStringLiteral("QCheckBox { margin-left: 40px; }"));
    hfCorrectGhgZohCheck->setVisible(false);

    sonicFrequencyLabel = new ClickLabel;
    sonicFrequencyLabel->setText(QStringLiteral("Sonic frequency:"));
    sonicFrequencyLabel->setVisible(false);

    sonicFrequency = new QSpinBox;
    sonicFrequency->setRange(4, 100);
    sonicFrequency->setSpecialValueText(QStringLiteral("Default"));
    sonicFrequency->setAccelerated(true);
    sonicFrequency->setSingleStep(1);
    sonicFrequency->setSuffix(QStringLiteral(" [Hz]"));
    sonicFrequency->setToolTip(tr("<b>Sonic frequency: </b>..."));
    sonicFrequency->setVisible(false);
////////////////////////////////////////////////////////////////////////////////

    spectraExistingRadio = new QRadioButton(tr("Spectral assessment file available for this dataset :"));
    spectraExistingRadio->setToolTip(tr("<b>Spectral assessment file available:</b> If you have a spectral assessment file from a previous run, and it applies to the current dataset, you can use the same file to by providing the path to the file named \"EddyFlow_spectral_assessment_ID.txt\". This file includes the results of the assessment. It can be used to shorten program execution time and assure full comparability between previous and current results."));

    spectraNonExistingRadio = new QRadioButton(tr("Spectral assessment file not available"));
    spectraNonExistingRadio->setToolTip(tr("<b>Spectral assessment file not available:</b> Choose this option and provide the following information if you need to calculate cut-off frequencies for your system. The assessment will be performed as an intermediate step, after all binned (co)spectra for the current dataset are calculated and before calculating and correcting fluxes."));

    automaticSpectraConfigCheck = new QCheckBox(tr("Automatically configure spectral assessment after this run"));
    automaticSpectraConfigCheck->setToolTip(tr("<b>Automatically configure spectral assessment:</b> When an on-the-fly assessment identifies eligible spectra excluded by flux limits, save data-driven flux-limit recommendations to the output processing project. The current run is not changed; rerun using the generated processing project."));
    automaticSpectraConfigCheck->setStyleSheet(QStringLiteral("QCheckBox { margin-left: 40px; }"));

    spectraFileBrowse = new FileBrowseWidget;
    spectraFileBrowse->setToolTip(tr("<b>Load:</b> Load an existing spectral assessment file"));
    spectraFileBrowse->setDialogTitle(tr("Select the Spectral Assessment File"));
    spectraFileBrowse->setDialogWorkingDir(WidgetUtils::getDialogPathHint(QStringLiteral("spectral_assessment_file")));
    spectraFileBrowse->setDialogFilter(tr("All Files (*.*)"));

    spectraRadioGroup = new QButtonGroup(this);
    spectraRadioGroup->addButton(spectraExistingRadio, 0);
    spectraRadioGroup->addButton(spectraNonExistingRadio, 1);

    // The per-gas transfer-function windows are generated in
    // rebuildGasSpectralSpins(), one pair per gas record.
    lowestFrequencyTip_ = tr("<b>Lowest frequency:</b> The assessment of the system transfer function implies the frequency-wise ratio of gas concentration to temperature spectra (temperature considered as proxy for un-attenuated atmospheric scalar spectra). This ratio must be taken in the frequency range where the system filtering is expected to occur. At lower frequencies, slow-paced atmospheric and source/sink dynamics may imply a breakdown of the similarity assumption. Default values can be good in most occasions, but the lower frequency should be adapted based mostly on the averaging interval.");
    highestFrequencyTip_ = tr("<b>Highest frequency:</b> The assessment of the system transfer function implies the frequency-wise ratio of gas concentration to temperature spectra (temperature being considered as a proxy for un-attenuated atmospheric scalar spectra). This ratio must be taken in the frequency range where the system filtering is expected to occur. At higher frequencies, noise and aliasing may corrupt the procedure. Default values can be good in most occasions, but the higher frequency should be adapted based on acquisition frequency and instrument performance.");
    const QString lowestFrequencyTooltip = lowestFrequencyTip_;
    const QString highestFrequencyTooltip = highestFrequencyTip_;

    minSmplLabel = new ClickLabel(tr("Minimum number of (co)spectra for valid averages :"));
    minSmplLabel->setToolTip(tr("<b>Minimum number of spectra for valid averages:</b> Select the minimum number of spectra that should be found in each class, for the corresponding ensemble average to be valid. Currently classes are defined only for H<sub>2</sub>O with respect to ambient relative humidity: 9 classes are defined between RH = 5% and RH = 95%. We expect to add classes also for passive gases, related to time periods. Entering a number that is too high may imply that, for certain classes, average spectra cannot be calculated. A number that is too small may result in poor characterization of average spectra. The higher this number, the longer the time period needed."));
    minSmplSpin = new QSpinBox;
    minSmplSpin->setRange(1, 1000);
    minSmplSpin->setSingleStep(1);
    minSmplSpin->setAccelerated(true);
    minSmplSpin->setToolTip(minSmplLabel->toolTip());

    // fratini section
    fratiniTitle = new QLabel(tr("Fratini et al. (2012) method settings"));
    fratiniTitle->setProperty("groupLabel", true);

    fullSpectraExistingRadio = new QRadioButton(tr("Full w/Ts cospectra files available for this dataset :"));
    fullSpectraExistingRadio->setToolTip(tr("<b>Full w/Ts cospectra files available:</b> Select this option if you already obtained <i>Full cospectra of w/T<sub>s</sub></i> for the current dataset (from a previous run of EddyFlow). Note that the cospectra files need to correspond exactly to the current dataset. Full cospectra of w/T<sub>s</sub> (sensible heat) are used for definition of the spectral correction factor for each flux with the method by Fratini et al. (2012). If you select this option, the option <i>Full length cospectra w/T<sub>s</sub></i> in the Output Files page will be automatically deselected and activated."));

    fullSpectraNonExistingRadio = new QRadioButton(tr("Full w/Ts cospectra files not available"));
    fullSpectraNonExistingRadio->setToolTip(tr("<b>Full w/T<sub>s</sub> cospectra files not available:</b> Select this option if you do not have <i>Full cospectra of w/T<sub>s</sub></i> for the current dataset (from a previous run of EddyFlow). Note that existing cospectra files need to correspond exactly to the current dataset. Full cospectra of w/T<sub>s</sub> (sensible heat) are used for definition of the spectral correction factor for each flux with the method by Fratini et al. (2012). If you select this option, the option <i>Full length cospectra w/T<sub>s</sub></i> in the Output Files page will be automatically selected and deactivated."));

    fullSpectraDirBrowse = new DirBrowseWidget;
    fullSpectraDirBrowse->setToolTip(tr("<b>Browse:</b> Specify the folder that contains the full w/T<sub>s</sub> cospectra files."));
    fullSpectraDirBrowse->setDialogTitle(tr("Select the Full Spectra Files Directory"));
    fullSpectraDirBrowse->setDialogWorkingDir(WidgetUtils::getDialogPathHint(QStringLiteral("full_cospectra_dir")));

    fullSpectraRadioGroup = new QButtonGroup(this);
    fullSpectraRadioGroup->addButton(fullSpectraNonExistingRadio, 0);
    fullSpectraRadioGroup->addButton(fullSpectraExistingRadio, 1);

    addSonicCheck = new QCheckBox(tr("Include anemometer losses for path averaging and time response"));
    addSonicCheck->setToolTip(tr("<b>Include anemometer losses for path averaging and time response:</b> Select this option to instruct EddyFlow to correct sensible heat cospectra for those losses, before using them as a model to calculate correction factors according to Fratini et al. (2012)."));
    addSonicCheck->setStyleSheet(QStringLiteral("QCheckBox { margin-left: 40px; }"));

    WidgetUtils::setCompactSpinBoxWidth(nBinsSpin, 76);
    WidgetUtils::setCompactSpinBoxWidth(minSmplSpin, 76);
    WidgetUtils::setCompactSpinBoxWidth(sonicFrequency, 76);

    for (auto spin : { qcMinUnstableUstarSpin, qcMinUnstableHSpin, qcMinUnstableLESpin })
    {
        spin->setToolTip(minUnstableTooltip);
    }
    for (auto spin : { qcMinStableUstarSpin, qcMinStableHSpin, qcMinStableLESpin })
    {
        spin->setToolTip(minStableTooltip);
    }
    for (auto spin : { qcMaxUstarSpin, qcMaxHSpin, qcMaxLESpin })
    {
        spin->setToolTip(maxTooltip);
    }

    WidgetUtils::setCompactSpinBoxWidth(qcMinUnstableUstarSpin, 96);
    WidgetUtils::setCompactSpinBoxWidth(qcMinUnstableHSpin, 96);
    WidgetUtils::setCompactSpinBoxWidth(qcMinUnstableLESpin, 96);
    WidgetUtils::setCompactSpinBoxWidth(qcMinStableUstarSpin, 96);
    WidgetUtils::setCompactSpinBoxWidth(qcMinStableHSpin, 96);
    WidgetUtils::setCompactSpinBoxWidth(qcMinStableLESpin, 96);
    WidgetUtils::setCompactSpinBoxWidth(qcMaxUstarSpin, 96);
    WidgetUtils::setCompactSpinBoxWidth(qcMaxHSpin, 96);
    WidgetUtils::setCompactSpinBoxWidth(qcMaxLESpin, 96);

    // horizontal rules
    auto hrLabel_0 = new QLabel;
    hrLabel_0->setObjectName(QStringLiteral("hrLabel"));

    // question marks
    createQuestionMarks();
    auto settingsGroup1Label = new QHBoxLayout;
    settingsGroup1Label->addWidget(settingsGroupTitle_1);
    settingsGroup1Label->addWidget(questionMark_11, 0, Qt::AlignRight | Qt::AlignVCenter);
    settingsGroup1Label->addStretch();
    auto lowFreqLabel = new QHBoxLayout;
    lowFreqLabel->addWidget(lowFreqTitle);
    lowFreqLabel->addWidget(questionMark_22, 0, Qt::AlignLeft | Qt::AlignVCenter);
    lowFreqLabel->addStretch();
    auto highFreqLabel = new QHBoxLayout;
    highFreqLabel->addWidget(highFreqTitle);
    highFreqLabel->addWidget(questionMark_33, 0, Qt::AlignLeft | Qt::AlignVCenter);
    highFreqLabel->addStretch();

    const auto spectralTooltips = QStringList{
        noiseFrequencyTooltip,
        minUnstableTooltip,
        minStableTooltip,
        maxTooltip,
        lowestFrequencyTooltip,
        highestFrequencyTooltip
    };
    spectralQaQcModel = new SpectralQaQcTableModel(this, QVector<SpectralQaQcRow>{}, spectralTooltips);

    //> The fixed rows only. The per-gas spins are parented, hidden and
    //> connected in rebuildGasSpectralSpins(), which runs next.
    const QList<QDoubleSpinBox*> spectralTableSpins = {
        qcMinUnstableUstarSpin, qcMinUnstableHSpin, qcMinUnstableLESpin,
        qcMinStableUstarSpin, qcMinStableHSpin, qcMinStableLESpin,
        qcMaxUstarSpin, qcMaxHSpin, qcMaxLESpin
    };
    for (auto spin : spectralTableSpins)
    {
        spin->setParent(this);
        spin->hide();
        connect(spin,
                qOverload<double>(&QDoubleSpinBox::valueChanged),
                spectralQaQcModel,
                [model = spectralQaQcModel](){ static_cast<SpectralQaQcTableModel*>(model)->refreshAll(); });
    }

    rebuildSpectralQaQcRows();

    spectralQaQcTable = new QTableView;
    spectralQaQcTable->setModel(spectralQaQcModel);
    spectralQaQcTable->setItemDelegate(new SpectralQaQcDelegate(spectralQaQcTable));
    spectralQaQcTable->setAlternatingRowColors(true);
    spectralQaQcTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    spectralQaQcTable->setSelectionMode(QAbstractItemView::SingleSelection);
    spectralQaQcTable->setEditTriggers(QAbstractItemView::DoubleClicked
                                       | QAbstractItemView::SelectedClicked
                                       | QAbstractItemView::EditKeyPressed);
    spectralQaQcTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    spectralQaQcTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    spectralQaQcTable->verticalHeader()->setMinimumSectionSize(24);
    spectralQaQcTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    spectralQaQcTable->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    spectralQaQcTable->setMinimumHeight(260);
    spectralQaQcTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    connect(spectralQaQcTable->horizontalHeader(),
            &QHeaderView::sectionClicked,
            this,
            [this](int column){ focusSpectralTableColumn(column); });

    auto spectralTableTitle = new QWidget;
    auto spectralTableTitleLayout = new QHBoxLayout(spectralTableTitle);
    spectralTableTitleLayout->setContentsMargins(0, 0, 0, 0);
    spectralTableTitleLayout->setSpacing(4);
    auto spectralTableTitleLabel = new QLabel(tr("<b>Spectra, cospectra QA/QC, and attenuation assessment</b>"));
    spectralTableTitleLabel->setTextFormat(Qt::RichText);
    spectralTableTitleLayout->addWidget(spectralTableTitleLabel);
    spectralTableTitleLayout->addWidget(questionMark_1, 0, Qt::AlignVCenter);
    spectralTableTitleLayout->addWidget(questionMark_44, 0, Qt::AlignVCenter);
    spectralTableTitleLayout->addStretch();

    auto spectralTableContainer = new QWidget;
    auto spectralTableLayout = new QVBoxLayout(spectralTableContainer);
    spectralTableLayout->setContentsMargins(0, 0, 0, 0);
    spectralTableLayout->setSpacing(4);
    spectralTableLayout->addWidget(spectralTableTitle);
    spectralTableLayout->addWidget(spectralQaQcTable);

    auto settingsLayout = new QGridLayout;
    settingsLayout->addLayout(settingsGroup1Label, 0, 0, 1, -1);
    settingsLayout->addWidget(binnedSpectraNonExistingRadio, 1, 0);
    settingsLayout->addWidget(binnedSpectraExistingRadio, 2, 0, Qt::AlignRight);
    settingsLayout->addWidget(binnedSpectraDirBrowse, 2, 1, 1, 4);

    settingsLayout->addWidget(subsetCheckBox, 3, 0);
    settingsLayout->addWidget(startDateLabel, 3, 0, Qt::AlignRight);
    settingsLayout->addWidget(endDateLabel, 4, 0, Qt::AlignRight);
    settingsLayout->addLayout(dateTimeContainer, 3, 1, 2, 2);

    settingsLayout->addWidget(filterLabel, 6, 0, Qt::AlignRight);
    settingsLayout->addWidget(filterCombo, 6, 1);
    settingsLayout->addWidget(nBinsLabel, 7, 0, Qt::AlignRight);
    settingsLayout->addWidget(nBinsSpin, 7, 1);
    settingsLayout->addWidget(fftCheckBox, 8, 0, 1, 2);

    settingsLayout->addWidget(spectralTableContainer, 9, 0, 1, -1);
    settingsLayout->addWidget(minSmplLabel, 10, 0, Qt::AlignRight);
    settingsLayout->addWidget(minSmplSpin, 10, 1);
    settingsLayout->addWidget(vmFlagsCheckBox, 11, 0, 1, 2);

    settingsLayout->addWidget(filterCospectraMauderTitle, 12, 0, 1, -1);
    settingsLayout->addWidget(lowQualityCheckBox, 13, 0, 1, -1);
    settingsLayout->addWidget(moderateQualityCheckBox, 14, 0, 1, 2);
    settingsLayout->addWidget(hrLabel_0, 15, 0, 1, -1);

    settingsLayout->addWidget(spectralCorrectionTitle, 16, 0);
    settingsLayout->addWidget(automaticSpectraConfigCheck, 17, 0, 1, -1);
    settingsLayout->addLayout(lowFreqLabel, 18, 0);
    settingsLayout->addWidget(lfMethodCheck, 19, 0, 1, 2);

    settingsLayout->addLayout(highFreqLabel, 20, 0);
    settingsLayout->addWidget(hfMethodCheck, 21, 0, 1, 2);
    settingsLayout->addWidget(hfMethLabel, 21, 1, Qt::AlignRight);
    settingsLayout->addWidget(hfMethCombo, 21, 2, 1, 3);
    settingsLayout->addWidget(horstCheck, 22, 0, 1, 2);
    settingsLayout->addWidget(horstMethodLabel, 22, 1, Qt::AlignRight);
    settingsLayout->addWidget(horstCombo, 22, 2, 1, 3);

    settingsLayout->addWidget(cospModelLabel, 23, 1, Qt::AlignRight);
    settingsLayout->addWidget(cospModelCombo, 23, 2, 1, 3);
    settingsLayout->addWidget(corrIterCheckBox, 24, 0, 1, 2);
    settingsLayout->addWidget(corrIterMaxLabel, 24, 1, Qt::AlignRight);
    settingsLayout->addWidget(corrIterMaxSpin, 24, 2);
    settingsLayout->addWidget(corrIterTolLabel, 24, 3, Qt::AlignRight);
    settingsLayout->addWidget(corrIterTolSpin, 24, 4);

    settingsLayout->addWidget(ghgSystemCorrectionTitle, 25, 0, 1, -1);
    settingsLayout->addWidget(hfCorrectGhgBaCheck, 26, 0, 1, 2);
    settingsLayout->addWidget(hfCorrectGhgZohCheck, 27, 0, 1, 2);
    settingsLayout->addWidget(sonicFrequencyLabel, 27, 1, Qt::AlignRight);
    settingsLayout->addWidget(sonicFrequency, 27, 2, 1, 1);

    settingsLayout->addWidget(spectraExistingRadio, 28, 0, 1, 2);
    settingsLayout->addWidget(spectraFileBrowse, 28, 1, 1, 4);
    settingsLayout->addWidget(spectraNonExistingRadio, 29, 0, 1, 2);

    settingsLayout->addWidget(fratiniTitle, 30, 0, 1, -1);
    settingsLayout->addWidget(fullSpectraNonExistingRadio, 31, 0, 1, 2);
    settingsLayout->addWidget(fullSpectraExistingRadio, 32, 0, 1, 2);
    settingsLayout->addWidget(fullSpectraDirBrowse, 32, 1, 1, 4);
    settingsLayout->addWidget(addSonicCheck, 33, 0, 1, -1);
    settingsLayout->setColumnStretch(7, 1);

    auto settingsGroupLayout = new QHBoxLayout;
    settingsGroupLayout
            ->addWidget(WidgetUtils::getContainerScrollArea(this,
                                                            settingsLayout));

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(settingsGroupLayout);
    mainLayout->setContentsMargins(15, 15, 15, 10);
    setLayout(mainLayout);

    connect(binnedSpectraRadioGroup,
            &QButtonGroup::idClicked,
            [=](int radioButton){ ecProject_->setGeneralBinSpectraAvail(radioButton); });
    connect(binnedSpectraRadioGroup, &QButtonGroup::idClicked,
            this, &AdvSpectralOptions::binnedSpectraRadioClicked);
    connect(binnedSpectraDirBrowse, &DirBrowseWidget::pathChanged,
            this, &AdvSpectralOptions::updateBinnedSpectraFile);
    connect(binnedSpectraDirBrowse, &DirBrowseWidget::pathSelected,
            this, &AdvSpectralOptions::binnedSpectraDirSelected);
    connect(subsetCheckBox, &QCheckBox::toggled, [=](bool toggle)
            { ecProject_->setSpectraSubset(toggle); } );
    connect(subsetCheckBox, &QCheckBox::toggled,
            this, &AdvSpectralOptions::onSubsetCheckboxToggled);
    connect(startDateLabel, &ClickLabel::clicked,
            this, &AdvSpectralOptions::onStartDateLabelClicked);
    connect(startDateEdit, &QDateEdit::dateChanged,
            this, &AdvSpectralOptions::updateStartDate);
    connect(startTimeEdit, &QTimeEdit::timeChanged,
            this, &AdvSpectralOptions::updateStartTime);
    connect(endDateLabel, &ClickLabel::clicked,
            this, &AdvSpectralOptions::onEndDateLabelClicked);
    connect(endDateEdit, &QDateEdit::dateChanged,
            this, &AdvSpectralOptions::updateEndDate);
    connect(endTimeEdit, &QTimeEdit::timeChanged,
            this, &AdvSpectralOptions::updateEndTime);

    connect(filterLabel, &ClickLabel::clicked, [=]()
            { if (filterCombo->isEnabled()) filterCombo->showPopup(); });
    connect(filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AdvSpectralOptions::updateFilter);
    connect(nBinsLabel, &ClickLabel::clicked, [=]()
            { nBinsSpin->setFocus(); nBinsSpin->selectAll(); });
    connect(nBinsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AdvSpectralOptions::updateNBins);
    connect(fftCheckBox, &QCheckBox::toggled, [=](bool checked)
            { ecProject_->setScreenlPowerOfTwo(checked); });

    connect(qcMinUnstableUstarSpin,
            static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            [=](double d){ ecProject_->setSpectraMinUnstableUstar(d); });
    connect(qcMinUnstableHSpin,
            static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            [=](double d){ ecProject_->setSpectraMinUnstableH(d); });
    connect(qcMinUnstableLESpin,
            static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            [=](double d){ ecProject_->setSpectraMinUnstableLE(d); });
    connect(qcMinStableUstarSpin,
            static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            [=](double d){ ecProject_->setSpectraMinStableUstar(d); });
    connect(qcMinStableHSpin,
            static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            [=](double d){ ecProject_->setSpectraMinStableH(d); });
    connect(qcMinStableLESpin,
            static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            [=](double d){ ecProject_->setSpectraMinStableLE(d); });
    connect(qcMaxUstarSpin,
            static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            [=](double d){ ecProject_->setSpectraMaxUstar(d); });
    connect(qcMaxHSpin,
            static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            [=](double d){ ecProject_->setSpectraMaxH(d); });
    connect(qcMaxLESpin,
            static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
            [=](double d){ ecProject_->setSpectraMaxLE(d); });
    connect(vmFlagsCheckBox, &QCheckBox::toggled, [=](bool checked)
            { ecProject_->setSpectraUseVmFlags(checked); });
    connect(lowQualityCheckBox, &QCheckBox::toggled, [=](bool checked)
            { ecProject_->setSpectraUseFokenLow(checked); });
    connect(moderateQualityCheckBox, &QCheckBox::toggled, [=](bool checked)
            { ecProject_->setSpectraUseFokenMid(checked); });
    connect(automaticSpectraConfigCheck, &QCheckBox::toggled, [=](bool checked)
            { ecProject_->setSpectraAutomaticConfig(checked); });

    connect(lfMethodCheck, &QCheckBox::toggled, [=](bool checked)
            { ecProject_->setGeneralLfMethod(checked); });

    connect(hfMethodCheck, &QCheckBox::toggled,
            this, &AdvSpectralOptions::updateHfMethod_1);
    connect(hfMethodCheck, &QCheckBox::toggled,
            hfMethLabel, &ClickLabel::setEnabled);
    connect(hfMethodCheck, &QCheckBox::toggled,
            hfMethCombo, &QComboBox::setEnabled);
    connect(hfMethLabel, &ClickLabel::clicked,
            this, &AdvSpectralOptions::onClickHfMethLabel);
    connect(hfMethCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AdvSpectralOptions::updateHfMethod_2);
    connect(horstCheck, &QCheckBox::toggled,
            horstMethodLabel, &ClickLabel::setEnabled);
    connect(horstCheck, &QCheckBox::toggled,
            horstCombo, &QComboBox::setEnabled);
    connect(horstMethodLabel, &ClickLabel::clicked,
            this, &AdvSpectralOptions::onClickHorstLabel);
    connect(horstCheck, &QCheckBox::toggled,
            this, &AdvSpectralOptions::updateHorst_1);
    connect(cospModelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=](int n){ ecProject_->setGeneralCospModel(n); });
    //> On the click, not the state change: refresh() blocks the project's
    //> signals, not the widgets', so a toggled connection would switch the
    //> loop on in a file the user only opened.
    connect(corrIterCheckBox, &QCheckBox::clicked,
            this, [=]()
            {
                ecProject_->setGeneralCorrIterMethod(
                    corrIterCheckBox->isChecked() ? 1 : 0);
                updateCorrIterAvailability();
            });
    connect(corrIterMaxSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [=](int n) { ecProject_->setGeneralCorrIterMax(n); });
    connect(corrIterTolSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [=](double d) { ecProject_->setGeneralCorrIterTol(d); });
    connect(corrIterMaxLabel, &ClickLabel::clicked,
            this, [=]() { corrIterMaxSpin->setFocus(Qt::ShortcutFocusReason); });
    connect(corrIterTolLabel, &ClickLabel::clicked,
            this, [=]() { corrIterTolSpin->setFocus(Qt::ShortcutFocusReason); });

    connect(cospModelLabel, &ClickLabel::clicked,
            this, [=](){ cospModelCombo->showPopup(); });
    connect(horstCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AdvSpectralOptions::updateHorst_2);

    connect(hfCorrectGhgBaCheck, &QCheckBox::toggled, [=](bool checked)
            { ecProject_->setGeneralHfCorrectGhgBa(checked); });
    connect(hfCorrectGhgZohCheck, &QCheckBox::toggled, [=](bool checked)
            { ecProject_->setGeneralHfCorrectGhgZoh(checked);
              sonicFrequencyLabel->setEnabled(checked);
              sonicFrequency->setEnabled(checked); });
    connect(sonicFrequencyLabel, &ClickLabel::clicked, this, [=]()
            { sonicFrequency->setFocus(); sonicFrequency->selectAll(); });
    connect(sonicFrequency,
            static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            [=](int value) { ecProject_->setGeneralSonicOutputRate(value); });

    connect(spectraRadioGroup,
            &QButtonGroup::idClicked,
            [=](int radioButton){ ecProject_->setSpectraMode(radioButton); });
    connect(spectraRadioGroup,
            &QButtonGroup::idClicked,
            [=](int radioButton){ spectraRadioClicked(radioButton); });
    connect(spectraFileBrowse, &FileBrowseWidget::pathChanged,
            this, &AdvSpectralOptions::updateSpectraFile);
    connect(spectraFileBrowse, &FileBrowseWidget::pathSelected,
            this, &AdvSpectralOptions::testSelectedSpectraFile);
    connect(minSmplLabel, &ClickLabel::clicked,
            this, &AdvSpectralOptions::onMinSmplLabelClicked);
    connect(minSmplSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AdvSpectralOptions::updateMinSmpl);
    connect(fullSpectraRadioGroup, &QButtonGroup::idClicked,
            this, &AdvSpectralOptions::fullSpectraRadioClicked);
    connect(fullSpectraDirBrowse, &DirBrowseWidget::pathChanged,
            this, &AdvSpectralOptions::updateFullSpectraFile);
    connect(fullSpectraDirBrowse, &DirBrowseWidget::pathSelected,
            this, &AdvSpectralOptions::fullSpectraDirSelected);
    connect(addSonicCheck, &QCheckBox::toggled, [=](bool checked)
            { ecProject_->setSpectraAddSonic(checked); });

    connect(ecProject_, &EcProject::ecProjectNew,
            this, &AdvSpectralOptions::reset);
    connect(ecProject_, &EcProject::ecProjectChanged,
            this, &AdvSpectralOptions::refresh);
    connect(ecProject_, &EcProject::updateInfo,
            this, &AdvSpectralOptions::refreshSpectralAssessmentCreationMode);

    auto combo_list = QWidgetList() << hfMethCombo
                                    << horstCombo;
    for (auto widget : combo_list)
    {
        auto combo = static_cast<QComboBox *>(widget);
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &AdvSpectralOptions::updateTooltip);
    }

    QTimer::singleShot(0, this, &AdvSpectralOptions::reset);
}

AdvSpectralOptions::~AdvSpectralOptions()
{
}

void AdvSpectralOptions::setSmartfluxUI()
{
    bool on = configState_->project.smartfluxMode;

    if (on)
    {
        spectraNonExistingRadioOldEnabled = spectraNonExistingRadio->isEnabled();
        spectraNonExistingRadio->setDisabled(on);
        hfMethCombo->setItemData(4, QStringLiteral("disabled"), Qt::UserRole);
    }
    else
    {
        spectraNonExistingRadio->setEnabled(spectraNonExistingRadioOldEnabled);
        hfMethCombo->setItemData(4, QStringLiteral("enabled"), Qt::UserRole);
    }

    // block project modified() signal
//    bool oldmod;
//    if (!on)
//    {
//        // save the modified flag to prevent side effects of setting widgets
//        oldmod = ecProject_->modified();
//        ecProject_->blockSignals(true);
//    }

    if (on)
    {
        spectraRadioGroup->button(0)->click();
    }

//    // restore project modified() signal
//    if (!on)
//    {
//        // restore modified flag
//        ecProject_->setModified(oldmod);
//        ecProject_->blockSignals(false);
//    }
}

void AdvSpectralOptions::reset()
{
    // save the modified flag to prevent side effects of setting widgets
    bool oldmod = ecProject_->modified();
    ecProject_->blockSignals(true);

    vmFlagsCheckBox->setChecked(ecProject_->defaultSettings.spectraSettings.use_vm_flags);
    lowQualityCheckBox->setChecked(ecProject_->defaultSettings.spectraSettings.use_foken_low);
    moderateQualityCheckBox->setChecked(ecProject_->defaultSettings.spectraSettings.use_foken_mid);
    automaticSpectraConfigCheck->setChecked(ecProject_->defaultSettings.spectraSettings.automatic_spectra_config);

    filterCombo->setCurrentIndex(ecProject_->defaultSettings.screenSetting.tap_win);
    filterCombo->setEnabled(true);
    nBinsSpin->setValue(ecProject_->defaultSettings.screenSetting.nbins);
    nBinsSpin->setEnabled(true);
    fftCheckBox->setChecked(ecProject_->defaultSettings.screenSetting.power_of_two);
    fftCheckBox->setEnabled(true);

    spectraExistingRadio->setEnabled(false);
    spectraExistingRadio->setChecked(!ecProject_->defaultSettings.spectraSettings.sa_mode);
    spectraFileBrowse->setEnabled(false);
    spectraFileBrowse->clear();
    spectraNonExistingRadio->setEnabled(false);
    spectraNonExistingRadio->setChecked(ecProject_->defaultSettings.spectraSettings.sa_mode);

    binnedSpectraExistingRadio->setChecked(ecProject_->defaultSettings.projectGeneral.bin_sp_avail);
    binnedSpectraDirBrowse->setEnabled(false);
    binnedSpectraNonExistingRadio->setChecked(!ecProject_->defaultSettings.projectGeneral.bin_sp_avail);
    binnedSpectraDirBrowse->clear();

    lfMethodCheck->setChecked(ecProject_->defaultSettings.projectGeneral.lf_meth);
    hfMethodCheck->setChecked(ecProject_->defaultSettings.projectGeneral.hf_meth);
    WidgetUtils::resetComboToItem(hfMethCombo, 0);
    WidgetUtils::resetComboToItem(cospModelCombo, ecProject_->defaultSettings.projectGeneral.cosp_model);
    corrIterCheckBox->setChecked(ecProject_->defaultSettings.projectGeneral.corr_iter_meth);
    corrIterMaxSpin->setValue(ecProject_->defaultSettings.projectGeneral.corr_iter_max);
    corrIterTolSpin->setValue(ecProject_->defaultSettings.projectGeneral.corr_iter_tol);
    updateCorrIterAvailability();
    horstMethodLabel->setEnabled(false);
    horstCheck->setEnabled(false);
    horstCheck->setChecked(false);
    WidgetUtils::resetComboToItem(horstCombo, 1);
    horstCombo->setEnabled(false);

    hfCorrectGhgBaCheck->setChecked(ecProject_->defaultSettings.projectGeneral.hf_correct_ghg_ba);
    hfCorrectGhgZohCheck->setChecked(ecProject_->defaultSettings.projectGeneral.hf_correct_ghg_zoh);
    hfCorrectGhgBaCheck->setEnabled(hfMethodCheck->isEnabled());
    hfCorrectGhgZohCheck->setEnabled(hfMethodCheck->isEnabled());

    sonicFrequencyLabel->setEnabled(hfMethodCheck->isEnabled());
    sonicFrequency->setEnabled(hfMethodCheck->isEnabled());
    sonicFrequency->setValue(ecProject_->defaultSettings.projectGeneral.sonic_output_rate);

    subsetCheckBox->setChecked(ecProject_->defaultSettings.spectraSettings.subset);
    startDateLabel->setEnabled(false);
    startDateEdit->setEnabled(false);
    startTimeEdit->setEnabled(false);
    lockedIcon->setEnabled(false);
    endDateLabel->setEnabled(false);
    endDateEdit->setEnabled(false);
    endTimeEdit->setEnabled(false);

    startDateEdit->setDate(QDate::fromString(ecProject_->generalStartDate(), Qt::ISODate));
    startTimeEdit->setTime(QTime::fromString(ecProject_->generalStartTime(), QStringLiteral("hh:mm")));
    endDateEdit->setDate(QDate::fromString(ecProject_->generalEndDate(), Qt::ISODate));
    endTimeEdit->setTime(QTime::fromString(ecProject_->generalEndTime(), QStringLiteral("hh:mm")));
    forceEndDatePolicy();
    forceEndTimePolicy();

    qcMinUnstableUstarSpin->setValue(ecProject_->defaultSettings.spectraSettings.sa_min_un_ustar);
    qcMinUnstableHSpin->setValue(ecProject_->defaultSettings.spectraSettings.sa_min_un_h);
    qcMinUnstableLESpin->setValue(ecProject_->defaultSettings.spectraSettings.sa_min_un_le);
    qcMinStableUstarSpin->setValue(ecProject_->defaultSettings.spectraSettings.sa_min_st_ustar);
    qcMinStableHSpin->setValue(ecProject_->defaultSettings.spectraSettings.sa_min_st_h);
    qcMinStableLESpin->setValue(ecProject_->defaultSettings.spectraSettings.sa_min_st_le);
    qcMaxUstarSpin->setValue(ecProject_->defaultSettings.spectraSettings.sa_max_ustar);
    qcMaxHSpin->setValue(ecProject_->defaultSettings.spectraSettings.sa_max_h);
    qcMaxLESpin->setValue(ecProject_->defaultSettings.spectraSettings.sa_max_le);

    minSmplSpin->setValue(ecProject_->defaultSettings.spectraSettings.sa_min_smpl);

    setSpectralAssessmentFrequencyCellsEnabled(false);

    // Every per-gas value back to the built-in default for its species, on
    // its record.
    resetGasSpectralToDefault();

    fullSpectraExistingRadio->setEnabled(false);
    fullSpectraExistingRadio->setChecked(ecProject_->defaultSettings.projectGeneral.full_sp_avail);
    fullSpectraNonExistingRadio->setEnabled(false);
    fullSpectraNonExistingRadio->setChecked(!ecProject_->defaultSettings.projectGeneral.full_sp_avail);
    fullSpectraDirBrowse->setEnabled(false);
    fullSpectraDirBrowse->clear();

    addSonicCheck->setChecked(ecProject_->defaultSettings.spectraSettings.add_sonic_lptf);
    addSonicCheck->setEnabled(false);

    // restore modified flag
    ecProject_->setModified(oldmod);
    ecProject_->blockSignals(false);

    refreshSpectralAssessmentCreationMode();
    refreshSpectralQaQcTableState();
    emit updateOutputsRequest(0);
}

void AdvSpectralOptions::partialRefresh()
{
    // save the modified flag to prevent side effects of setting widgets
    bool oldmod = ecProject_->modified();
    ecProject_->blockSignals(true);

    subsetCheckBox->setChecked(ecProject_->spectraSubset());
    if (ecProject_->spectraSubset())
    {
        startDateEdit->setDate(QDate::fromString(ecProject_->spectraStartDate(), Qt::ISODate));
        startTimeEdit->setTime(QTime::fromString(ecProject_->spectraStartTime(), QStringLiteral("hh:mm")));
        endDateEdit->setDate(QDate::fromString(ecProject_->spectraEndDate(), Qt::ISODate));
        endTimeEdit->setTime(QTime::fromString(ecProject_->spectraEndTime(), QStringLiteral("hh:mm")));
    }
    else
    {
        startDateEdit->setDate(QDate::fromString(ecProject_->generalStartDate(), Qt::ISODate));
        startTimeEdit->setTime(QTime::fromString(ecProject_->generalStartTime(), QStringLiteral("hh:mm")));
        endDateEdit->setDate(QDate::fromString(ecProject_->generalEndDate(), Qt::ISODate));
        endTimeEdit->setTime(QTime::fromString(ecProject_->generalEndTime(), QStringLiteral("hh:mm")));
    }

    // restore modified flag
    ecProject_->setModified(oldmod);
    ecProject_->blockSignals(false);
}

void AdvSpectralOptions::refresh()
{
    // save the modified flag to prevent side effects of setting widgets
    bool oldmod = ecProject_->modified();
    ecProject_->blockSignals(true);

    vmFlagsCheckBox->setChecked(ecProject_->spectraUseVmFlags());
    lowQualityCheckBox->setChecked(ecProject_->spectraUseFokenLow());
    moderateQualityCheckBox->setChecked(ecProject_->spectraUseFokenMid());
    automaticSpectraConfigCheck->setChecked(ecProject_->spectraAutomaticConfig());

    lfMethodCheck->setChecked(ecProject_->generalLfMethod());
    hfMethodCheck->setChecked(ecProject_->generalHfMethod());
    cospModelCombo->setCurrentIndex(ecProject_->generalCospModel());
    corrIterCheckBox->setChecked(ecProject_->generalCorrIterMethod());
    corrIterMaxSpin->setValue(ecProject_->generalCorrIterMax());
    corrIterTolSpin->setValue(ecProject_->generalCorrIterTol());
    updateCorrIterAvailability();

    int hfMethod = ecProject_->generalHfMethod();
    switch(hfMethod)
    {
    case 0:
    case 1: // moncrieff
        hfMethCombo->setCurrentIndex(0);
        break;
    case 2: // horst
        hfMethCombo->setCurrentIndex(2);
        break;
    case 3: // ibrom
        hfMethCombo->setCurrentIndex(3);
        break;
    case 4: // fratini
        hfMethCombo->setCurrentIndex(4);
        break;
    case 5: // massmann
        hfMethCombo->setCurrentIndex(1);
        break;
    }

    sonicFrequencyLabel->setEnabled(hfMethodCheck->isChecked());
    sonicFrequency->setEnabled(hfMethodCheck->isChecked());
    sonicFrequency->setValue(ecProject_->generalSonicOutputRate());

    spectraExistingRadio->setEnabled(hfMethodCheck->isChecked()
                                     && isHorstIbromFratini());
    spectraNonExistingRadio->setEnabled(hfMethodCheck->isChecked()
                                        && isHorstIbromFratini());
    spectraExistingRadio->setChecked(!ecProject_->spectraMode());
    spectraNonExistingRadio->setChecked(ecProject_->spectraMode());

    spectraFileBrowse->setPath(QDir::toNativeSeparators(ecProject_->spectraFile()));
    spectraFileBrowse->setEnabled(spectraExistingRadio->isEnabled()
                                  && spectraExistingRadio->isChecked());

    subsetCheckBox->setChecked(ecProject_->spectraSubset());

    binnedSpectraNonExistingRadio->setChecked(!ecProject_->generalBinSpectraAvail());
    binnedSpectraExistingRadio->setChecked(ecProject_->generalBinSpectraAvail());
    binnedSpectraDirBrowse->setPath(ecProject_->spectraBinSpectra());
    binnedSpectraDirBrowse->setEnabled(binnedSpectraExistingRadio->isChecked());

    startDateLabel->setEnabled(subsetCheckBox->isEnabled() && subsetCheckBox->isChecked());
    startDateEdit->setEnabled(startDateLabel->isEnabled());
    startTimeEdit->setEnabled(startDateLabel->isEnabled());
    lockedIcon->setEnabled(startDateLabel->isEnabled());
    endDateLabel->setEnabled(startDateLabel->isEnabled());
    endDateEdit->setEnabled(startDateLabel->isEnabled());
    endTimeEdit->setEnabled(startDateLabel->isEnabled());

    startDateEdit->setDate(QDate::fromString(ecProject_->spectraStartDate(), Qt::ISODate));
    startTimeEdit->setTime(QTime::fromString(ecProject_->spectraStartTime(), QStringLiteral("hh:mm")));
    endDateEdit->setDate(QDate::fromString(ecProject_->spectraEndDate(), Qt::ISODate));
    endTimeEdit->setTime(QTime::fromString(ecProject_->spectraEndTime(), QStringLiteral("hh:mm")));

    filterCombo->setCurrentIndex(ecProject_->screenTapWin());
    filterCombo->setEnabled(!ecProject_->generalBinSpectraAvail());
    nBinsSpin->setValue(ecProject_->screenNBins());
    nBinsSpin->setEnabled(!ecProject_->generalBinSpectraAvail());
    fftCheckBox->setChecked(ecProject_->screenPowerOfTwo());
    fftCheckBox->setEnabled(!ecProject_->generalBinSpectraAvail());

    horstCheck->setEnabled(isIbrom() || isFratini());
    if (ecProject_->spectraHorst() > 0)
    {
        horstCombo->setCurrentIndex(ecProject_->spectraHorst() - 1);
    }
    else
    {
        horstCombo->setCurrentIndex(1);
    }
    horstCheck->setChecked(ecProject_->spectraHorst());
    horstMethodLabel->setEnabled(horstCheck->isEnabled() && horstCheck->isChecked());
    horstCombo->setEnabled(horstCheck->isEnabled() && horstCheck->isChecked());

    hfCorrectGhgBaCheck->setEnabled(hfMethodCheck->isChecked());
    hfCorrectGhgZohCheck->setEnabled(hfMethodCheck->isChecked());
    hfCorrectGhgBaCheck->setChecked(ecProject_->generalHfCorrectGhgBa());
    hfCorrectGhgZohCheck->setChecked(ecProject_->generalHfCorrectGhgZoh());

    sonicFrequency->setValue(ecProject_->generalSonicOutputRate());

    auto toEnable = isHorstIbromFratini()
                    && spectraNonExistingRadio->isEnabled()
                    && spectraNonExistingRadio->isChecked();

    minSmplSpin->setValue(ecProject_->spectraMinSmpl());

    qcMinUnstableUstarSpin->setValue(ecProject_->spectraMinUnstableUstar());
    qcMinUnstableHSpin->setValue(ecProject_->spectraMinUnstableH());
    qcMinUnstableLESpin->setValue(ecProject_->spectraMinUnstableLE());
    qcMinStableUstarSpin->setValue(ecProject_->spectraMinStableUstar());
    qcMinStableHSpin->setValue(ecProject_->spectraMinStableH());
    qcMinStableLESpin->setValue(ecProject_->spectraMinStableLE());
    qcMaxUstarSpin->setValue(ecProject_->spectraMaxUstar());
    qcMaxHSpin->setValue(ecProject_->spectraMaxH());
    qcMaxLESpin->setValue(ecProject_->spectraMaxLE());

    // Rebuilt here rather than only on show: a project load can change the
    // gas records under the rows, and their values come from them.
    rebuildGasSpectralSpins();
    setSpectralAssessmentFrequencyCellsEnabled(toEnable);

    auto toEnableFratini = isFratini();

    fullSpectraExistingRadio->setEnabled(toEnableFratini);
    fullSpectraExistingRadio->setChecked(ecProject_->generalFullSpectraAvail());
    fullSpectraNonExistingRadio->setEnabled(toEnableFratini);
    fullSpectraNonExistingRadio->setChecked(!ecProject_->generalFullSpectraAvail());

    fullSpectraDirBrowse->setPath(ecProject_->spectraFullSpectra());
    fullSpectraDirBrowse->setEnabled(toEnableFratini
                                     && fullSpectraExistingRadio->isChecked());

    addSonicCheck->setChecked(ecProject_->spectraAddSonic());
    addSonicCheck->setEnabled(toEnableFratini);

    // restore modified flag
    ecProject_->setModified(oldmod);
    ecProject_->blockSignals(false);

    refreshSpectralAssessmentCreationMode();
    refreshSpectralQaQcTableState();
    emit updateOutputsRequest(hfMethCombo->currentIndex());
}

void AdvSpectralOptions::refreshSpectralAssessmentCreationMode()
{
    const auto createAssessment = ecProject_->spectraFluxRunMode() == 1;
    const auto productionRun = ecProject_->spectraFluxRunMode() == 2;
    if (createAssessment && configState_->project.smartfluxMode)
    {
        ecProject_->setSpectraFluxRunMode(0);
        return;
    }
    if (productionRun && configState_->project.smartfluxMode)
    {
        ecProject_->setSpectraFluxRunMode(0);
        return;
    }

    hfMethCombo->setItemData(0,
                             createAssessment
                             ? QStringLiteral("disabled")
                             : QStringLiteral("enabled"),
                             Qt::UserRole);
    hfMethCombo->setItemData(1,
                             createAssessment
                             ? QStringLiteral("disabled")
                             : QStringLiteral("enabled"),
                             Qt::UserRole);
    hfMethCombo->setItemData(2, QStringLiteral("enabled"), Qt::UserRole);
    hfMethCombo->setItemData(3, QStringLiteral("enabled"), Qt::UserRole);
    hfMethCombo->setItemData(4, QStringLiteral("enabled"), Qt::UserRole);

    if (createAssessment)
    {
        if (ecProject_->generalHfMethod() < 2 || ecProject_->generalHfMethod() > 4)
        {
            ecProject_->setGeneralHfMethod(4);
        }
        if (ecProject_->spectraMode() != 1)
        {
            ecProject_->setSpectraMode(1);
        }
        if (ecProject_->generalBinSpectraAvail())
        {
            ecProject_->setGeneralBinSpectraAvail(0);
        }
        if (ecProject_->generalFullSpectraAvail())
        {
            ecProject_->setGeneralFullSpectraAvail(0);
        }
        if (ecProject_->spectraUseVmFlags())
        {
            ecProject_->setSpectraUseVmFlags(0);
        }
        if (ecProject_->spectraUseFokenMid())
        {
            ecProject_->setSpectraUseFokenMid(0);
        }

        QSignalBlocker hfMethodBlocker(hfMethodCheck);
        QSignalBlocker hfComboBlocker(hfMethCombo);
        QSignalBlocker spectraExistingBlocker(spectraExistingRadio);
        QSignalBlocker spectraNonExistingBlocker(spectraNonExistingRadio);
        QSignalBlocker binnedExistingBlocker(binnedSpectraExistingRadio);
        QSignalBlocker binnedNonExistingBlocker(binnedSpectraNonExistingRadio);
        QSignalBlocker fullExistingBlocker(fullSpectraExistingRadio);
        QSignalBlocker fullNonExistingBlocker(fullSpectraNonExistingRadio);
        QSignalBlocker vmBlocker(vmFlagsCheckBox);
        QSignalBlocker moderateBlocker(moderateQualityCheckBox);

        hfMethodCheck->setChecked(true);
        hfMethCombo->setCurrentIndex(hfComboIndexFromProjectMethod());
        spectraExistingRadio->setChecked(false);
        spectraNonExistingRadio->setChecked(true);
        binnedSpectraExistingRadio->setChecked(false);
        binnedSpectraNonExistingRadio->setChecked(true);
        fullSpectraExistingRadio->setChecked(false);
        fullSpectraNonExistingRadio->setChecked(true);
        vmFlagsCheckBox->setChecked(false);
        moderateQualityCheckBox->setChecked(false);

        hfMethodCheck->setEnabled(false);
        hfMethLabel->setEnabled(true);
        hfMethCombo->setEnabled(true);
        spectraExistingRadio->setEnabled(false);
        spectraNonExistingRadio->setEnabled(false);
        spectraFileBrowse->setEnabled(false);
        binnedSpectraExistingRadio->setEnabled(true);
        binnedSpectraNonExistingRadio->setEnabled(true);
        binnedSpectraDirBrowse->setEnabled(false);
        fullSpectraExistingRadio->setEnabled(isFratini());
        fullSpectraNonExistingRadio->setEnabled(isFratini());
        fullSpectraDirBrowse->setEnabled(false);
        vmFlagsCheckBox->setEnabled(false);
        moderateQualityCheckBox->setEnabled(false);
        lowQualityCheckBox->setEnabled(true);

        const auto toEnable = isHorstIbromFratini();
        setSpectralAssessmentFrequencyCellsEnabled(toEnable);
    }
    else if (productionRun)
    {
        if (ecProject_->generalHfMethod() < 2 || ecProject_->generalHfMethod() > 4)
        {
            ecProject_->setGeneralHfMethod(4);
        }
        ecProject_->setSpectraMode(0);
        ecProject_->setGeneralBinSpectraAvail(1);
        ecProject_->setGeneralFullSpectraAvail(1);
        ecProject_->setSpectraUseVmFlags(1);
        ecProject_->setSpectraUseFokenMid(1);
        ecProject_->setSpectraUseFokenLow(1);

        QSignalBlocker hfMethodBlocker(hfMethodCheck);
        QSignalBlocker hfComboBlocker(hfMethCombo);
        QSignalBlocker spectraExistingBlocker(spectraExistingRadio);
        QSignalBlocker spectraNonExistingBlocker(spectraNonExistingRadio);
        QSignalBlocker binnedExistingBlocker(binnedSpectraExistingRadio);
        QSignalBlocker binnedNonExistingBlocker(binnedSpectraNonExistingRadio);
        QSignalBlocker fullExistingBlocker(fullSpectraExistingRadio);
        QSignalBlocker fullNonExistingBlocker(fullSpectraNonExistingRadio);
        QSignalBlocker vmBlocker(vmFlagsCheckBox);
        QSignalBlocker lowBlocker(lowQualityCheckBox);
        QSignalBlocker moderateBlocker(moderateQualityCheckBox);

        hfMethodCheck->setChecked(true);
        hfMethCombo->setCurrentIndex(hfComboIndexFromProjectMethod());
        spectraExistingRadio->setChecked(true);
        spectraNonExistingRadio->setChecked(false);
        binnedSpectraExistingRadio->setChecked(true);
        binnedSpectraNonExistingRadio->setChecked(false);
        fullSpectraExistingRadio->setChecked(true);
        fullSpectraNonExistingRadio->setChecked(false);
        vmFlagsCheckBox->setChecked(true);
        lowQualityCheckBox->setChecked(true);
        moderateQualityCheckBox->setChecked(true);

        hfMethodCheck->setEnabled(true);
        hfMethLabel->setEnabled(hfMethodCheck->isChecked());
        hfMethCombo->setEnabled(hfMethodCheck->isChecked());

        spectraExistingRadio->setEnabled(hfMethodCheck->isChecked()
                                         && isHorstIbromFratini());
        spectraNonExistingRadio->setEnabled(hfMethodCheck->isChecked()
                                            && isHorstIbromFratini()
                                            && !configState_->project.smartfluxMode);
        spectraFileBrowse->setEnabled(spectraExistingRadio->isEnabled());
        binnedSpectraExistingRadio->setEnabled(true);
        binnedSpectraNonExistingRadio->setEnabled(true);
        binnedSpectraDirBrowse->setEnabled(true);
        fullSpectraExistingRadio->setEnabled(isFratini());
        fullSpectraNonExistingRadio->setEnabled(isFratini());
        fullSpectraDirBrowse->setEnabled(isFratini());
        vmFlagsCheckBox->setEnabled(true);
        moderateQualityCheckBox->setEnabled(true);
        lowQualityCheckBox->setEnabled(false);
    }
    else
    {
        vmFlagsCheckBox->setEnabled(true);
        lowQualityCheckBox->setEnabled(true);
        moderateQualityCheckBox->setEnabled(true);
        hfMethodCheck->setEnabled(true);
        hfMethLabel->setEnabled(hfMethodCheck->isChecked());
        hfMethCombo->setEnabled(hfMethodCheck->isChecked());

        const auto smartfluxOn = configState_->project.smartfluxMode;
        if (smartfluxOn)
        {
            hfMethCombo->setItemData(4, QStringLiteral("disabled"), Qt::UserRole);
        }

        spectraExistingRadio->setEnabled(hfMethodCheck->isChecked()
                                         && isHorstIbromFratini());
        spectraNonExistingRadio->setEnabled(hfMethodCheck->isChecked()
                                            && isHorstIbromFratini()
                                            && !smartfluxOn);
        spectraFileBrowse->setEnabled(spectraExistingRadio->isEnabled()
                                      && spectraExistingRadio->isChecked());
    }

    refreshSpectralQaQcTableState();
    emit updateOutputsRequest(hfMethCombo->currentIndex());
}

void AdvSpectralOptions::updateSpectraFile(const QString &fp)
{
    ecProject_->setSpectraFile(QDir::cleanPath(fp));
}

void AdvSpectralOptions::updateBinnedSpectraFile(const QString &fp)
{
    ecProject_->setSpectraBinSpectra(QDir::cleanPath(fp));
}

void AdvSpectralOptions::updateFullSpectraFile(const QString &fp)
{
    ecProject_->setSpectraFullSpectra(QDir::cleanPath(fp));
}

void AdvSpectralOptions::testSelectedSpectraFile(const QString& fp)
{
    QFileInfo paramFilePath(fp);
    QString canonicalParamFile = paramFilePath.canonicalFilePath();

    AncillaryFileTest test_dialog(AncillaryFileTest::FileType::Spectra, ecProject_, this);
    test_dialog.refresh(canonicalParamFile);

    auto test_result = test_dialog.makeTest();
    auto dialog_result = true;

    // blocking behavior if test fails
    if (!test_result)
    {
        dialog_result = test_dialog.exec();
    }

    if (dialog_result)
    {
        spectraFileBrowse->setPath(fp);
        WidgetUtils::rememberDialogPath(QStringLiteral("spectral_assessment_file"), fp, true);
    }
    else
    {
        spectraFileBrowse->clear();
    }
}

void AdvSpectralOptions::binnedSpectraDirSelected(const QString& dir_path)
{
    binnedSpectraDirBrowse->setPath(dir_path);

    WidgetUtils::rememberDialogPath(QStringLiteral("binned_cospectra_dir"), dir_path, false);
}

void AdvSpectralOptions::fullSpectraDirSelected(const QString& dir_path)
{
    fullSpectraDirBrowse->setPath(dir_path);

    WidgetUtils::rememberDialogPath(QStringLiteral("full_cospectra_dir"), dir_path, false);
}

void AdvSpectralOptions::spectraRadioClicked(int radioButton)
{
    // existing spectral assessment file
    if (radioButton == 0)
    {
        spectraFileBrowse->setEnabled(true);
        setSpectralAssessmentFrequencyCellsEnabled(false);
    }
    // non existing spectral assessment file
    else
    {
        spectraFileBrowse->setEnabled(false);

        auto toEnable = hfMethodCheck->isChecked() && isHorstIbromFratini();

        setSpectralAssessmentFrequencyCellsEnabled(toEnable);
    }
    refreshSpectralQaQcTableState();
    emit updateOutputsRequest(hfMethCombo->currentIndex());
}

void AdvSpectralOptions::binnedSpectraRadioClicked(int radioButton)
{
    if (radioButton == 0)
    {
        binnedSpectraDirBrowse->setEnabled(false);
        filterCombo->setEnabled(true);
        nBinsSpin->setEnabled(true);
        fftCheckBox->setEnabled(true);
    }
    else
    {
        binnedSpectraDirBrowse->setEnabled(true);
        filterCombo->setEnabled(false);
        nBinsSpin->setEnabled(false);
        fftCheckBox->setEnabled(false);
    }
    emit updateOutputsRequest(hfMethCombo->currentIndex());
}

void AdvSpectralOptions::fullSpectraRadioClicked(int radioButton)
{
    if (radioButton == 0)
    {
        ecProject_->setGeneralFullSpectraAvail(0);
        fullSpectraDirBrowse->setEnabled(false);
    }
    else
    {
        ecProject_->setGeneralFullSpectraAvail(1);
        fullSpectraDirBrowse->setEnabled(true);
    }
    emit updateOutputsRequest(hfMethCombo->currentIndex());
}

void AdvSpectralOptions::onStartDateLabelClicked()
{
    startDateEdit->setFocus();
    WidgetUtils::showCalendarOf(startDateEdit);
}

void AdvSpectralOptions::onEndDateLabelClicked()
{
    endDateEdit->setFocus();
    WidgetUtils::showCalendarOf(endDateEdit);
}

void AdvSpectralOptions::updateStartDate(const QDate &d)
{
    ecProject_->setSpectraStartDate(d.toString(Qt::ISODate));
    forceEndDatePolicy();
}

void AdvSpectralOptions::updateStartTime(const QTime& t)
{
    ecProject_->setSpectraStartTime(t.toString(QStringLiteral("hh:mm")));
    forceEndTimePolicy();
}

void AdvSpectralOptions::updateEndDate(const QDate &d)
{
    ecProject_->setSpectraEndDate(d.toString(Qt::ISODate));
}

void AdvSpectralOptions::updateEndTime(const QTime& t)
{
    ecProject_->setSpectraEndTime(t.toString(QStringLiteral("hh:mm")));
}

void AdvSpectralOptions::onClickHfMethLabel()
{
    if (hfMethCombo->isEnabled())
    {
        hfMethCombo->showPopup();
    }
}

void AdvSpectralOptions::setHfMethod(int hfMethComboIndex)
{
    switch (hfMethComboIndex)
    {
    case 0: // moncrieff
        ecProject_->setGeneralHfMethod(1);
        break;
    case 1: // massmann
        ecProject_->setGeneralHfMethod(5);
        break;
    case 2: // horst
        ecProject_->setGeneralHfMethod(2);
        break;
    case 3: // ibrom
        ecProject_->setGeneralHfMethod(3);
        break;
    case 4: // fratini
        ecProject_->setGeneralHfMethod(4);
        break;
    }
}

int AdvSpectralOptions::hfComboIndexFromProjectMethod() const
{
    switch (ecProject_->generalHfMethod())
    {
    case 2:
        return 2;
    case 3:
        return 3;
    case 4:
        return 4;
    case 5:
        return 1;
    case 0:
    case 1:
    default:
        return 0;
    }
}

void AdvSpectralOptions::updateHfMethod_1(bool b)
{
    if (ecProject_->spectraFluxRunMode() == 1 && !b)
    {
        refreshSpectralAssessmentCreationMode();
        return;
    }

    bool smartfluxOn = configState_->project.smartfluxMode;

    if (b)
    {
        setHfMethod(hfMethCombo->currentIndex());

        spectraExistingRadio->setEnabled(isHorstIbromFratini());
        spectraNonExistingRadio->setEnabled(isHorstIbromFratini()
                                            && !smartfluxOn);
        spectraFileBrowse->setEnabled(isHorstIbromFratini()
                                      && spectraExistingRadio->isChecked());
        auto toEnable = isHorstIbromFratini() && spectraNonExistingRadio->isChecked();

        horstCheck->setEnabled(isIbrom() || isFratini());
        horstMethodLabel->setEnabled(horstCheck->isEnabled() && horstCheck->isChecked());
        horstCombo->setEnabled(horstMethodLabel->isEnabled());

        hfCorrectGhgBaCheck->setEnabled(true);
        hfCorrectGhgZohCheck->setEnabled(true);
        sonicFrequencyLabel->setEnabled(hfCorrectGhgZohCheck->isChecked());
        sonicFrequency->setEnabled(hfCorrectGhgZohCheck->isChecked());

        setSpectralAssessmentFrequencyCellsEnabled(toEnable);

        auto toEnableFratini = isFratini();
        fullSpectraNonExistingRadio->setEnabled(toEnableFratini);
        fullSpectraExistingRadio->setEnabled(toEnableFratini);
        fullSpectraNonExistingRadio->setEnabled(toEnableFratini);
        fullSpectraDirBrowse->setEnabled(toEnableFratini
                                         && fullSpectraExistingRadio->isChecked());
        addSonicCheck->setEnabled(toEnableFratini);

        emit updateOutputsRequest(hfMethCombo->currentIndex());
        maybeWarnMassmanFallback();
    }
    else
    {
        ecProject_->setGeneralHfMethod(0);
        massmanFallbackWarningShown_ = false;

        horstMethodLabel->setEnabled(false);
        horstCheck->setEnabled(false);
        horstCombo->setEnabled(false);

        hfCorrectGhgBaCheck->setEnabled(false);
        hfCorrectGhgZohCheck->setEnabled(false);
        sonicFrequencyLabel->setEnabled(false);
        sonicFrequency->setEnabled(false);

        spectraExistingRadio->setEnabled(false);
        spectraNonExistingRadio->setEnabled(false);
        spectraFileBrowse->setEnabled(false);
        setSpectralAssessmentFrequencyCellsEnabled(false);

        fullSpectraExistingRadio->setEnabled(false);
        fullSpectraNonExistingRadio->setEnabled(false);
        fullSpectraDirBrowse->setEnabled(false);
        addSonicCheck->setEnabled(false);

        emit updateOutputsRequest(0);
    }
}

// update project properties and fluxes rotation choices
void AdvSpectralOptions::updateHfMethod_2(int n)
{
    if (ecProject_->spectraFluxRunMode() == 1 && n < 2)
    {
        refreshSpectralAssessmentCreationMode();
        return;
    }

    bool smartfluxOn = configState_->project.smartfluxMode;

    setHfMethod(n);

    spectraExistingRadio->setEnabled(n > 1);
    spectraNonExistingRadio->setEnabled((n > 1) && !smartfluxOn);
    spectraFileBrowse->setEnabled(n > 1 && spectraExistingRadio->isChecked());

    auto toEnable = isHorstIbromFratini() && spectraNonExistingRadio->isChecked();

    horstCheck->setEnabled(isIbrom() || isFratini());
    horstMethodLabel->setEnabled(horstCheck->isEnabled() && horstCheck->isChecked());
    horstCombo->setEnabled(horstMethodLabel->isEnabled());

    setSpectralAssessmentFrequencyCellsEnabled(toEnable);

    // fratini only
    auto toEnableFratini = (n == 4);

    fullSpectraExistingRadio->setEnabled(toEnableFratini);
    fullSpectraNonExistingRadio->setEnabled(toEnableFratini);
    fullSpectraDirBrowse->setEnabled(toEnableFratini
                                     && fullSpectraExistingRadio->isChecked());
    addSonicCheck->setEnabled(toEnableFratini);

    emit updateOutputsRequest(n);
    maybeWarnMassmanFallback();
}

bool AdvSpectralOptions::isHorstIbromFratini()
{
    return (ecProject_->generalHfMethod() == 2)
            || (ecProject_->generalHfMethod() == 3)
            || (ecProject_->generalHfMethod() == 4);
}

bool AdvSpectralOptions::isIbrom()
{
    return (ecProject_->generalHfMethod() == 3);
}

bool AdvSpectralOptions::isFratini()
{
    return (ecProject_->generalHfMethod() == 4);
}

bool AdvSpectralOptions::hasLi7500FamilyIrga() const
{
    return IrgaDesc::hasLi7500Family(dlProject_ ? dlProject_->irgas() : nullptr);
}

void AdvSpectralOptions::maybeWarnMassmanFallback()
{
    const bool massmanSelected = hfMethodCheck->isChecked()
                                 && ecProject_->generalHfMethod() == 5;
    if (!massmanSelected)
    {
        massmanFallbackWarningShown_ = false;
        return;
    }
    if (massmanFallbackWarningShown_ || hasLi7500FamilyIrga())
    {
        return;
    }

    massmanFallbackWarningShown_ = true;
    WidgetUtils::warning(
        this,
        tr("Massman spectral correction"),
        tr("For non-LI-7500-family gas analyzers, the analyzer bandwidth "
           "constant currently falls back to a near-zero value. Another "
           "low-pass correction method is recommended unless you have "
           "verified the required metadata."));
}

void AdvSpectralOptions::onMinSmplLabelClicked()
{
    minSmplSpin->setFocus();
    minSmplSpin->selectAll();
}

void AdvSpectralOptions::updateMinSmpl(int n)
{
    ecProject_->setSpectraMinSmpl(n);
}

/// The two numbers describe a loop that is not running otherwise.
void AdvSpectralOptions::updateCorrIterAvailability()
{
    const bool on = corrIterCheckBox->isChecked();
    corrIterMaxLabel->setEnabled(on);
    corrIterMaxSpin->setEnabled(on);
    corrIterTolLabel->setEnabled(on);
    corrIterTolSpin->setEnabled(on);
}

void AdvSpectralOptions::onClickHorstLabel()
{
    if (horstCombo->isEnabled())
    {
        horstCombo->showPopup();
    }
}

void AdvSpectralOptions::updateHorst_1(bool b)
{
    if (b)
    {
        ecProject_->setSpectraHorst(horstCombo->currentIndex() + 1);
    }
    else
    {
        ecProject_->setSpectraHorst(0);
    }
}

void AdvSpectralOptions::updateHorst_2(int n)
{
    ecProject_->setSpectraHorst(n + 1);
}

void AdvSpectralOptions::focusSpectralTableColumn(int column)
{
    if (!spectralQaQcModel || !spectralQaQcTable)
    {
        return;
    }

    const auto model = static_cast<SpectralQaQcTableModel*>(spectralQaQcModel);
    const QModelIndex firstEditable = model->firstEditableIndex(column);
    if (!firstEditable.isValid())
    {
        return;
    }

    spectralQaQcTable->setFocus();
    spectralQaQcTable->setCurrentIndex(firstEditable);
    spectralQaQcTable->edit(firstEditable);
}

void AdvSpectralOptions::setSpectralAssessmentFrequencyCellsEnabled(bool enabled)
{
    // Remembered so a rebuild can reapply it: the table's flags() reads the
    // enabled state off the spin, so a fresh spin would otherwise become
    // editable in a mode where the frequencies do not apply.
    frequencyCellsEnabled_ = enabled;
    for (const auto& row : gasSpectralRows_)
    {
        if (row.noiseFrequency) { row.noiseFrequency->setEnabled(enabled); }
        if (row.lowestFrequency) { row.lowestFrequency->setEnabled(enabled); }
        if (row.highestFrequency) { row.highestFrequency->setEnabled(enabled); }
    }
    refreshSpectralQaQcTableState();
}

const VariableDesc* AdvSpectralOptions::rawVariableAtColumn(int column) const
{
    if (!dlProject_ || column <= 0)
    {
        return nullptr;
    }

    const auto variables = dlProject_->variables();
    const int index = column - 1;
    if (!variables || index < 0 || index >= variables->size())
    {
        return nullptr;
    }
    return &variables->at(index);
}

bool AdvSpectralOptions::selectedColumnIsVariable(int column, const QString& variableName) const
{
    const auto variable = rawVariableAtColumn(column);
    return variable && variable->variable() == variableName;
}

/// The record set the current spins were built from.
QString AdvSpectralOptions::gasSignature() const
{
    QStringList parts;
    for (const auto& gas : ecProject_->gasColumns())
    {
        parts << gas.slug + QLatin1Char('|') + gas.instrumentId
                 + QLatin1Char('|') + QString::number(gas.rawColumn);
    }
    return parts.join(QLatin1Char(';'));
}

/// Row label: the species, qualified by the analyser when there is one, so
/// two CO2 records on different analysers are told apart.
QString AdvSpectralOptions::gasRowLabel(int gasIndex) const
{
    return MeasurementRecords::gasLabel(ecProject_->gasColumns(), gasIndex);
}

/// Built-in default for a gas that carries no value of its own, chosen by
/// species rather than by position: a second CO2 record deserves the CO2
/// default, not the fourth-gas one.
double AdvSpectralOptions::defaultGasSpectral(const QString& slug,
                                              SpectralParam param) const
{
    const auto& d = ecProject_->defaultSettings.spectraSettings;
    const bool isCo2 = (slug == QLatin1String("co2"));
    const bool isH2o = (slug == QLatin1String("h2o"));
    const bool isCh4 = (slug == QLatin1String("ch4"));

    switch (param)
    {
        case SpectralParam::HfnFmin:
            if (isCo2) { return d.sa_hfn_co2_fmin; }
            if (isH2o) { return d.sa_hfn_h2o_fmin; }
            if (isCh4) { return d.sa_hfn_ch4_fmin; }
            return d.sa_hfn_other_fmin;
        case SpectralParam::Fmin:
            if (isCo2) { return d.sa_fmin_co2; }
            if (isH2o) { return d.sa_fmin_h2o; }
            if (isCh4) { return d.sa_fmin_ch4; }
            return d.sa_fmin_other;
        case SpectralParam::Fmax:
            if (isCo2) { return d.sa_fmax_co2; }
            if (isH2o) { return d.sa_fmax_h2o; }
            if (isCh4) { return d.sa_fmax_ch4; }
            return d.sa_fmax_other;
        case SpectralParam::MinUnstable:
            if (isCo2) { return d.sa_min_un_co2; }
            if (isCh4) { return d.sa_min_un_ch4; }
            return d.sa_min_un_other;
        case SpectralParam::MinStable:
            if (isCo2) { return d.sa_min_st_co2; }
            if (isCh4) { return d.sa_min_st_ch4; }
            return d.sa_min_st_other;
        case SpectralParam::Maximum:
            if (isCo2) { return d.sa_max_co2; }
            if (isCh4) { return d.sa_max_ch4; }
            return d.sa_max_other;
    }
    return 0.0;
}

/// Value to show for one gas: the record's own if it has one, else the
/// species default.
///
/// The flat keys are retired; an upgraded project has had them moved onto
/// its records by EcProject::migrateLegacyGasSettings().
double AdvSpectralOptions::gasSpectralFor(int gasIndex,
                                          SpectralParam param) const
{
    const auto& gases = ecProject_->gasColumns();
    if (gasIndex < 0 || gasIndex >= gases.size()) { return 0.0; }
    const auto& proc = gases.at(gasIndex).proc;

    switch (param)
    {
        case SpectralParam::HfnFmin:
            if (proc.saHfnFmin >= 0.0) { return proc.saHfnFmin; } break;
        case SpectralParam::Fmin:
            if (proc.saFmin >= 0.0) { return proc.saFmin; } break;
        case SpectralParam::Fmax:
            if (proc.saFmax >= 0.0) { return proc.saFmax; } break;
        case SpectralParam::MinUnstable:
            if (proc.saMinUn >= 0.0) { return proc.saMinUn; } break;
        case SpectralParam::MinStable:
            if (proc.saMinSt >= 0.0) { return proc.saMinSt; } break;
        case SpectralParam::Maximum:
            if (proc.saMax >= 0.0) { return proc.saMax; } break;
    }

    return defaultGasSpectral(gases.at(gasIndex).slug, param);
}

/// Store one per-gas value on its record. The record is the only place it
/// goes; the flat keys the first four slots used to mirror are retired.
void AdvSpectralOptions::onGasSpectralChanged(int gasIndex,
                                              SpectralParam param,
                                              double value)
{
    if (!ecProject_) { return; }
    auto gases = ecProject_->gasColumns();
    if (gasIndex < 0 || gasIndex >= gases.size()) { return; }

    switch (param)
    {
        case SpectralParam::HfnFmin: gases[gasIndex].proc.saHfnFmin = value; break;
        case SpectralParam::Fmin: gases[gasIndex].proc.saFmin = value; break;
        case SpectralParam::Fmax: gases[gasIndex].proc.saFmax = value; break;
        case SpectralParam::MinUnstable: gases[gasIndex].proc.saMinUn = value; break;
        case SpectralParam::MinStable: gases[gasIndex].proc.saMinSt = value; break;
        case SpectralParam::Maximum: gases[gasIndex].proc.saMax = value; break;
    }
    ecProject_->setGasColumns(gases);

}

/// Restore Default Values, for the per-gas spectral settings. Only the
/// records are written now that the flat keys are retired.
void AdvSpectralOptions::resetGasSpectralToDefault()
{
    if (!ecProject_) { return; }

    const auto& gases = ecProject_->gasColumns();
    const SpectralParam params[] = {
        SpectralParam::HfnFmin, SpectralParam::Fmin, SpectralParam::Fmax,
        SpectralParam::MinUnstable, SpectralParam::MinStable,
        SpectralParam::Maximum
    };
    for (int i = 0; i < gases.size(); ++i)
    {
        const auto slug = gases.at(i).slug;
        for (const auto param : params)
        {
            onGasSpectralChanged(i, param, defaultGasSpectral(slug, param));
        }
    }
    rebuildGasSpectralSpins();
}

/// One spin box, configured for its setting and species.
///
/// The value is set before the connection is made, so building the row never
/// writes back into the project.
QDoubleSpinBox* AdvSpectralOptions::makeGasSpectralSpin(int gasIndex,
                                                        SpectralParam param)
{
    const bool isFrequency = (param == SpectralParam::HfnFmin
                              || param == SpectralParam::Fmin
                              || param == SpectralParam::Fmax);

    QDoubleSpinBox* spin = isFrequency ? new QDoubleSpinBox
                                       : new AdaptivePrecisionDoubleSpinBox;
    if (isFrequency)
    {
        spin->setRange(0.0, 50.0);
        spin->setSingleStep(0.1);
        spin->setDecimals(4);
        spin->setSuffix(QStringLiteral(" [Hz]"));
        if (param == SpectralParam::HfnFmin)
        {
            spin->setSpecialValueText(tr("Do not remove noise"));
        }
    }
    else
    {
        spin->setRange(0.0, 5000.0);
        // CO2 fluxes are an order of magnitude larger than the other gases',
        // which is why its minima stepped coarser than theirs.
        const auto slug = ecProject_->gasColumns().at(gasIndex).slug;
        const bool isCo2 = (slug == QLatin1String("co2"));
        if (param == SpectralParam::Maximum) { spin->setSingleStep(10.0); }
        else { spin->setSingleStep(isCo2 ? 1.0 : 0.1); }
        spin->setDecimals(6);
        spin->setSuffix(tr(" [%1]").arg(Defs::UMOL_M2S_STRING));
    }
    spin->setAccelerated(true);

    switch (param)
    {
        case SpectralParam::HfnFmin: spin->setToolTip(noiseFrequencyTip_); break;
        case SpectralParam::Fmin: spin->setToolTip(lowestFrequencyTip_); break;
        case SpectralParam::Fmax: spin->setToolTip(highestFrequencyTip_); break;
        case SpectralParam::MinUnstable: spin->setToolTip(minUnstableTip_); break;
        case SpectralParam::MinStable: spin->setToolTip(minStableTip_); break;
        case SpectralParam::Maximum: spin->setToolTip(maxTip_); break;
    }

    WidgetUtils::setCompactSpinBoxWidth(spin, isFrequency ? 86 : 96);
    spin->setParent(this);
    spin->hide();
    if (isFrequency) { spin->setEnabled(frequencyCellsEnabled_); }

    spin->setValue(gasSpectralFor(gasIndex, param));

    connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            spectralQaQcModel,
            [model = spectralQaQcModel]()
            { static_cast<SpectralQaQcTableModel*>(model)->refreshAll(); });
    return spin;
}

/// One set of spin boxes per configured gas.
///
/// Records with no raw column are skipped: they are slots kept so the
/// engine's record-to-slot mapping stays put, not measurements.
void AdvSpectralOptions::rebuildGasSpectralSpins()
{
    if (!ecProject_ || !spectralQaQcModel) { return; }

    // Cleared from the model first: it holds raw pointers into these rows,
    // and would paint through them while the widgets are being destroyed.
    static_cast<SpectralQaQcTableModel*>(spectralQaQcModel)
        ->setRows(QVector<SpectralQaQcRow>{});

    const auto dropSpin = [](QDoubleSpinBox* spin)
    {
        if (!spin) { return; }
        spin->setParent(nullptr);
        spin->deleteLater();
    };
    for (const auto& row : gasSpectralRows_)
    {
        dropSpin(row.noiseFrequency);
        dropSpin(row.lowestFrequency);
        dropSpin(row.highestFrequency);
        dropSpin(row.minUnstable);
        dropSpin(row.minStable);
        dropSpin(row.maximum);
    }
    gasSpectralRows_.clear();

    // Alphabetical, so the table reads the way the gases are named rather than
    // the way they were added. The index stays the record index throughout -
    // that is what every spin writes back through.
    const auto& gases = ecProject_->gasColumns();
    for (const int i : MeasurementRecords::gasDisplayOrder(gases))
    {
        GasSpectralRow row;
        row.gasIndex = i;
        row.noiseFrequency = makeGasSpectralSpin(i, SpectralParam::HfnFmin);
        row.lowestFrequency = makeGasSpectralSpin(i, SpectralParam::Fmin);
        row.highestFrequency = makeGasSpectralSpin(i, SpectralParam::Fmax);

        // Water's thresholds are the latent-heat triple, one per project and
        // not a gas flux, so an H2O record has no QA/QC spins of its own.
        if (gases.at(i).slug != QLatin1String("h2o"))
        {
            row.minUnstable = makeGasSpectralSpin(i, SpectralParam::MinUnstable);
            row.minStable = makeGasSpectralSpin(i, SpectralParam::MinStable);
            row.maximum = makeGasSpectralSpin(i, SpectralParam::Maximum);
        }

        const int idx = i;
        auto fmin = row.lowestFrequency;
        auto fmax = row.highestFrequency;
        connect(row.noiseFrequency, qOverload<double>(&QDoubleSpinBox::valueChanged),
                this, [=](double d)
                { onGasSpectralChanged(idx, SpectralParam::HfnFmin, d); });
        connect(fmin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                this, [=](double d)
                {
                    onGasSpectralChanged(idx, SpectralParam::Fmin, d);
                    // min/max constraint
                    if (d >= fmax->value()) { fmax->setValue(d + 0.0001); }
                });
        connect(fmax, qOverload<double>(&QDoubleSpinBox::valueChanged),
                this, [=](double d)
                {
                    onGasSpectralChanged(idx, SpectralParam::Fmax, d);
                    // min/max constraint
                    if (d <= fmin->value()) { fmin->setValue(d - 0.0001); }
                });
        if (row.minUnstable)
        {
            connect(row.minUnstable, qOverload<double>(&QDoubleSpinBox::valueChanged),
                    this, [=](double d)
                    { onGasSpectralChanged(idx, SpectralParam::MinUnstable, d); });
            connect(row.minStable, qOverload<double>(&QDoubleSpinBox::valueChanged),
                    this, [=](double d)
                    { onGasSpectralChanged(idx, SpectralParam::MinStable, d); });
            connect(row.maximum, qOverload<double>(&QDoubleSpinBox::valueChanged),
                    this, [=](double d)
                    { onGasSpectralChanged(idx, SpectralParam::Maximum, d); });
        }

        gasSpectralRows_.append(row);
    }

    gasSignature_ = gasSignature();
    rebuildSpectralQaQcRows();
}

void AdvSpectralOptions::rebuildSpectralQaQcRows()
{
    if (!spectralQaQcModel)
    {
        return;
    }

    // The gas records can change while another page is in front:
    // setGasColumns emits ecProjectModified, which nothing here listens to.
    if (gasSignature() != gasSignature_)
    {
        rebuildGasSpectralSpins();
        return;                 // that call comes back through here
    }

    QVector<SpectralQaQcRow> rows;
    rows.append({ tr("Friction velocity"), nullptr, qcMinUnstableUstarSpin, qcMinStableUstarSpin, qcMaxUstarSpin, nullptr, nullptr });

    // The latent-heat row exists whenever the project measures water, wherever
    // that record sits in the list.
    const bool hasH2o = !ecProject_->gasRecordsFor(QStringLiteral("h2o")).isEmpty();
    if (hasH2o)
    {
        rows.append({ tr("Latent heat flux"), nullptr, qcMinUnstableLESpin, qcMinStableLESpin, qcMaxLESpin, nullptr, nullptr });
    }

    rows.append({ tr("Sensible heat flux"), nullptr, qcMinUnstableHSpin, qcMinStableHSpin, qcMaxHSpin, nullptr, nullptr });

    for (const auto& row : gasSpectralRows_)
    {
        rows.append({ tr("%1 flux").arg(gasRowLabel(row.gasIndex)),
                      row.noiseFrequency,
                      row.minUnstable, row.minStable, row.maximum,
                      row.lowestFrequency, row.highestFrequency });
    }

    static_cast<SpectralQaQcTableModel*>(spectralQaQcModel)->setRows(rows);
}

void AdvSpectralOptions::refreshSpectralQaQcTableState()
{
    rebuildSpectralQaQcRows();
    refreshSpectralQaQcTableView(spectralQaQcModel, spectralQaQcTable);
}

// enforce (start date&time) <= (end date&time)
void AdvSpectralOptions::forceEndDatePolicy()
{
    endDateEdit->setMinimumDate(startDateEdit->date());
}

// enforce (start date&time) <= (end date&time)
void AdvSpectralOptions::forceEndTimePolicy()
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

void AdvSpectralOptions::createQuestionMarks()
{
    questionMark_1 = new QPushButton;
    questionMark_1->setObjectName(QStringLiteral("questionMarkImg"));
    questionMark_1->setFlat(true);
    questionMark_1->setIcon(QIcon(QStringLiteral(":/icons/qm-enabled")));
    questionMark_1->setIconSize(QSize(12, 12));
    questionMark_11 = new QPushButton;
    questionMark_11->setObjectName(QStringLiteral("questionMarkImg"));
    questionMark_11->setFlat(true);
    questionMark_11->setIcon(QIcon(QStringLiteral(":/icons/qm-enabled")));
    questionMark_11->setIconSize(QSize(12, 12));
    questionMark_22 = new QPushButton;
    questionMark_22->setObjectName(QStringLiteral("questionMarkImg"));
    questionMark_22->setFlat(true);
    questionMark_22->setIcon(QIcon(QStringLiteral(":/icons/qm-enabled")));
    questionMark_22->setIconSize(QSize(12, 12));
    questionMark_33 = new QPushButton;
    questionMark_33->setObjectName(QStringLiteral("questionMarkImg"));
    questionMark_33->setFlat(true);
    questionMark_33->setIcon(QIcon(QStringLiteral(":/icons/qm-enabled")));
    questionMark_33->setIconSize(QSize(12, 12));
    questionMark_44 = new QPushButton;
    questionMark_44->setObjectName(QStringLiteral("questionMarkImg"));
    questionMark_44->setFlat(true);
    questionMark_44->setIcon(QIcon(QStringLiteral(":/icons/qm-enabled")));
    questionMark_44->setIconSize(QSize(12, 12));
    questionMark_55 = new QPushButton;
    questionMark_55->setObjectName(QStringLiteral("questionMarkImg"));
    questionMark_55->setFlat(true);
    questionMark_55->setIcon(QIcon(QStringLiteral(":/icons/qm-enabled")));
    questionMark_55->setIconSize(QSize(12, 12));

    connect(questionMark_1, &QPushButton::clicked,
            this, &AdvSpectralOptions::onlineHelpTrigger_11);
    connect(questionMark_11, &QPushButton::clicked,
            this, &AdvSpectralOptions::onlineHelpTrigger_1);
    connect(questionMark_22, &QPushButton::clicked,
            this, &AdvSpectralOptions::onlineHelpTrigger_2);
    connect(questionMark_33, &QPushButton::clicked,
            this, &AdvSpectralOptions::onlineHelpTrigger_3);
    connect(questionMark_44, &QPushButton::clicked,
            this, &AdvSpectralOptions::onlineHelpTrigger_4);
    connect(questionMark_55, &QPushButton::clicked,
            this, &AdvSpectralOptions::onlineHelpTrigger_5);
}

void AdvSpectralOptions::onlineHelpTrigger_11()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Ensemble_Averages.html")));
}

void AdvSpectralOptions::onlineHelpTrigger_1()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Calculating_Spectral_Correction_Factors.html")));
}

void AdvSpectralOptions::onlineHelpTrigger_2()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/High-pass_Filtering.html")));
}

void AdvSpectralOptions::onlineHelpTrigger_3()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Low-pass_Filtering.html")));
}

void AdvSpectralOptions::onlineHelpTrigger_4()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Spectral_Corrections.html")));
}

void AdvSpectralOptions::onlineHelpTrigger_5()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Calculating_Spectra_Cospectra_and_Ogives.html")));
}

void AdvSpectralOptions::updateTooltip(int i)
{
    QComboBox* senderCombo = qobject_cast<QComboBox *>(sender());

    WidgetUtils::updateComboItemTooltip(senderCombo, i);
}

void AdvSpectralOptions::onSubsetCheckboxToggled(bool b)
{
    auto widget_list = QWidgetList() << startDateLabel
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
}

void AdvSpectralOptions::updateFilter(int n)
{
    ecProject_->setScreenTapWin(n);
}

void AdvSpectralOptions::updateNBins(int n)
{
    ecProject_->setScreenNBins(n);
}

