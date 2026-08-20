/***************************************************************************
  pwbtimelagsettingsdialog.cpp
  ----------------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#include "pwbtimelagsettingsdialog.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

#include "ancillaryfiletest.h"
#include "defs.h"
#include "ecproject.h"
#include "measurement_record.h"
#include "filebrowsewidget.h"
#include "widget_utils.h"

namespace {
//> The window read_ini_rp.f90 applies when a project states none.
constexpr double kDefaultMinLag = -10.0;
constexpr double kDefaultMaxLag =  10.0;
}  // namespace

PwbTimelagSettingsDialog::PwbTimelagSettingsDialog(QWidget *parent,
                                                   EcProject *ecProject,
                                                   ConfigState *config) :
    QDialog(parent),
    ecProject_(ecProject),
    configState_(config)
{
    Q_UNUSED(configState_)

    setWindowModality(Qt::WindowModal);
    setWindowTitle(tr("PWB Time Lag Optimization Settings"));
    WidgetUtils::removeContextHelpButton(this);

    auto title = new QLabel(tr("Pre-whitening block-bootstrap time lag detection"));
    title->setProperty("groupLabel", true);

    existingRadio = new QRadioButton(tr("Time-lag file available : "));
    existingRadio->setToolTip(tr("<b>Time-lag file available:</b> Select either a PWB half-hourly time-lag table (*_pwb_timelag_*.csv, written by a previous run) or a standard Time-lag_optimisation_results file. The half-hourly table reuses the exact lag recorded for each timestamp and gas; any period missing from it is detected and the table rewritten. An aggregate file uses its gas and H2O RH-class lags for the whole run and does not run PWB."));

    nonExistingRadio = new QRadioButton(tr("Time lag file not available :"));
    nonExistingRadio->setToolTip(tr("<b>Time lag file not available:</b> Choose this option and provide the following information if you need to detect time lags for your dataset with pre-whitening block-bootstrap."));

    fileBrowse = new FileBrowseWidget;
    fileBrowse->setToolTip(tr("<b>Load:</b> Load a PWB half-hourly time-lag table or an aggregate time-lag file"));
    fileBrowse->setDialogTitle(tr("Select a PWB Time-Lag Table or Time-Lag Results File"));
    fileBrowse->setDialogWorkingDir(WidgetUtils::getDialogPathHint(QStringLiteral("timelag_file")));
    fileBrowse->setDialogFilter(tr("All Files (*.*)"));

    auto existingFileLayout = new QHBoxLayout;
    existingFileLayout->addWidget(existingRadio);
    existingFileLayout->addWidget(fileBrowse);
    existingFileLayout->setStretch(1, 1);
    existingFileLayout->setContentsMargins(0, 0, 0, 0);
    existingFileLayout->setSpacing(0);

    radioGroup = new QButtonGroup(this);
    radioGroup->addButton(existingRadio, 0);
    radioGroup->addButton(nonExistingRadio, 1);

    auto windowTitle = WidgetUtils::createBlueLabel(this, tr("Time lag search windows"));
    auto minTitle = WidgetUtils::createBlueLabel(this, tr("Minimum"));
    auto maxTitle = WidgetUtils::createBlueLabel(this, tr("Maximum"));

    // Rows are built from the project's gases in rebuildLagRows(), so the
    // dialog follows whatever the site selected rather than a fixed four.
    auto windows = new QGridLayout;
    windows->addWidget(windowTitle, 0, 0);
    windows->addWidget(minTitle, 0, 1);
    windows->addWidget(maxTitle, 0, 2);
    windows->setColumnStretch(3, 1);
    lagGrid_ = windows;

    nBootstrapSpin = new QSpinBox;
    nBootstrapSpin->setRange(1, 9999);
    nBootstrapSpin->setAccelerated(true);

    blockLengthSpin = createSecondsSpin(0.0, 1000.0);
    blockLengthSpin->setSpecialValueText(tr("Auto (2 x search window)"));
    blockLengthSpin->setToolTip(tr(
        "<b>Block length:</b> length of the resampling blocks the bootstrap "
        "draws, which preserve local autocorrelation.<br><br>"
        "This is a <b>floor</b>, applied per gas. A block shorter than the "
        "lag range cannot contain the lag structure the bootstrap exists to "
        "preserve, so each gas actually uses "
        "<i>max(this, 2 x its widest search bound)</i> - a gas searching to "
        "25 s resamples in 50 s blocks whatever is set here."));

    minValidFracSpin = createFractionSpin();
    hdiThreshSpin = createSecondsSpin(0.0, 100.0);
    devThreshSpin = createSecondsSpin(0.0, 100.0);
    hdiPrefilterSpin = createSecondsSpin(0.0, 100.0);
    hdiPrefilterSpin->setSpecialValueText(tr("Disabled"));
    hdiPrefilterSpin->setToolTip(tr(
        "<b>HDI prefilter:</b><br>"
        "Detections whose 95% HDI is wider than this are discarded before the "
        "S1/S2 classification runs, so temporal continuity cannot accept a "
        "vague detection merely because it happens to land near the previous "
        "period's lag.<br><br>"
        "Stricter than the reliable-HDI threshold above, which only decides "
        "whether a detection is accepted outright.<br><br>"
        "<b>Default:</b> 1.00 s. Set to zero to disable."));

    smoothingWidthSpin = new QSpinBox;
    smoothingWidthSpin->setRange(1, 999);
    smoothingWidthSpin->setSingleStep(2);
    smoothingWidthSpin->setAccelerated(true);
    smoothingWidthSpin->setToolTip(tr(
        "<b>Smoothing width:</b> width of the centred rolling mean applied to "
        "each bootstrap cross-correlation before its peak is located, in "
        "records.<br><br>"
        "Either parity is allowed. An odd window is symmetric; an even one "
        "puts its extra sample after the centre, following the same "
        "convention R does.<br><br>"
        "<b>Default:</b> 5, which is RFlux's. The paper specifies "
        "<i>hz/2 + 1</i> instead - 6 at 10 Hz, 11 at 20 Hz - and the choice "
        "is not cosmetic: on the reference implementation's test data, "
        "widening from 5 to 11 at 20 Hz widened the 95% interval from "
        "0.00/0.05 s to 0.30/0.20 s, against the 0.5 s threshold that decides "
        "whether a detection is accepted."));

    maxCarrySpin = new QDoubleSpinBox;
    maxCarrySpin->setDecimals(1);
    maxCarrySpin->setRange(0.0, 8760.0);
    maxCarrySpin->setSingleStep(1.0);
    maxCarrySpin->setAccelerated(true);
    maxCarrySpin->setSuffix(tr("  [h]"));
    maxCarrySpin->setSpecialValueText(tr("Unlimited"));
    maxCarrySpin->setToolTip(tr(
        "<b>Max carry:</b><br>"
        "How far a detected time lag may travel to a period that detected "
        "none. It bounds all three ways a gas reaches its own lag - "
        "interpolation between reliable neighbours, carrying the last one "
        "forward, and filling backward from the next - because bounding only "
        "one of them would achieve nothing: with detections either side of a "
        "long unusable stretch, an unbounded backward fill covers exactly the "
        "span the forward carry was forbidden to cross.<br><br>"
        "Past this distance the period takes the lag of another gas on the "
        "same analyser instead, and failing that the gas's median.<br><br>"
        "Measured in <b>elapsed hours</b>, not in averaging periods: the "
        "table has a row only where a period was processed, so counting "
        "periods would reach straight across a gap in the raw files.<br><br>"
        "<b>Default:</b> 24 h. Set to zero for the published rule, under "
        "which one reliable half hour can supply days."));

    randomSeedSpin = new QSpinBox;
    randomSeedSpin->setRange(1, 2147483647);
    randomSeedSpin->setAccelerated(true);

    auto detectionTitle = WidgetUtils::createBlueLabel(this, tr("Bootstrap and reliability"));
    auto detection = new QGridLayout;
    detection->addWidget(detectionTitle, 0, 0, 1, 2);
    detection->addWidget(new QLabel(tr("Bootstrap replicates :")), 1, 0, Qt::AlignRight);
    detection->addWidget(nBootstrapSpin, 1, 1);
    detection->addWidget(new QLabel(tr("Block length :")), 2, 0, Qt::AlignRight);
    detection->addWidget(blockLengthSpin, 2, 1);
    detection->addWidget(new QLabel(tr("Minimum valid fraction :")), 3, 0, Qt::AlignRight);
    detection->addWidget(minValidFracSpin, 3, 1);
    detection->addWidget(new QLabel(tr("Reliable HDI threshold :")), 4, 0, Qt::AlignRight);
    detection->addWidget(hdiThreshSpin, 4, 1);
    detection->addWidget(new QLabel(tr("Deviation threshold :")), 5, 0, Qt::AlignRight);
    detection->addWidget(devThreshSpin, 5, 1);
    detection->addWidget(new QLabel(tr("HDI prefilter :")), 6, 0, Qt::AlignRight);
    detection->addWidget(hdiPrefilterSpin, 6, 1);
    detection->addWidget(new QLabel(tr("Smoothing width :")), 7, 0, Qt::AlignRight);
    detection->addWidget(smoothingWidthSpin, 7, 1);
    detection->addWidget(new QLabel(tr("Max carry :")), 8, 0, Qt::AlignRight);
    detection->addWidget(maxCarrySpin, 8, 1);
    detection->addWidget(new QLabel(tr("Random seed :")), 9, 0, Qt::AlignRight);
    detection->addWidget(randomSeedSpin, 9, 1);
    detection->setColumnStretch(2, 1);

    //> Detection runs on rotated, pre-WPL high-frequency data, and there is
    //> no longer a control for that. A checkbox here offered a choice between
    //> two stages that both ran on rotated 20 Hz data, described in its
    //> tooltip as a choice between raw and block-averaged data - which it
    //> never was. The conversion it actually straddled runs before time-lag
    //> compensation, so detecting after it puts cell temperature and water
    //> into the gas series at the wrong relative lag.
    auto stageNote = new QLabel(tr(
        "<i>Time lags are detected on rotated high-frequency data, before the "
        "mixing-ratio conversion, as a pre-processing pass over the whole "
        "run.</i>"));
    stageNote->setWordWrap(true);

    auto pwbOptionsLayout = new QVBoxLayout;
    pwbOptionsLayout->addWidget(stageNote);
    // Gases on one analyser share a detected lag (PWB's S4_instrument_shared
    // rule), so a window set for one of them decides the others too. Without
    // saying so, narrowing CO2's window and watching H2O move looks like a bug.
    auto sharingNote = new QLabel(tr(
        "<i>Each averaging period gets its own time lag per gas. Where a "
        "period has no reliable detection, the gas's <b>own</b> lag is used "
        "first in all three of its forms - interpolated between the reliable "
        "lags either side, carried forward, or filled backward - each no "
        "further than Max carry. Only past that is the lag of another gas on "
        "the <b>same analyser</b> borrowed, and then the gas's median. Two "
        "gases down one tube still have measurably different delays, so a "
        "borrowed lag trades a stale number for a biased one.<br>"
        "A lag is never taken from a different instrument, and never from "
        "water, whose delay depends on humidity as the trace gases' does "
        "not. A gas whose record names no instrument neither donates nor "
        "borrows, since nothing then proves it shares a tube.</i>"));
    sharingNote->setWordWrap(true);

    pwbOptionsLayout->addLayout(windows);
    pwbOptionsLayout->addWidget(sharingNote);
    pwbOptionsLayout->addSpacing(10);
    pwbOptionsLayout->addLayout(detection);
    pwbOptionsLayout->setContentsMargins(0, 0, 0, 0);

    pwbOptionsContainer = new QWidget;
    pwbOptionsContainer->setLayout(pwbOptionsLayout);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &PwbTimelagSettingsDialog::hide);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(title);
    mainLayout->addLayout(existingFileLayout);
    mainLayout->addWidget(nonExistingRadio);
    mainLayout->addWidget(pwbOptionsContainer);
    mainLayout->addWidget(buttons);

    connect(radioGroup, &QButtonGroup::idClicked,
            this, &PwbTimelagSettingsDialog::updateTlMode);
    connect(fileBrowse, &FileBrowseWidget::pathChanged,
            this, &PwbTimelagSettingsDialog::updateFile);
    connect(fileBrowse, &FileBrowseWidget::pathSelected,
            this, &PwbTimelagSettingsDialog::testSelectedFile);

    connect(nBootstrapSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            ecProject_, &EcProject::setPwbNBootstrap);
    connect(blockLengthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            ecProject_, &EcProject::setPwbBlockLength);
    connect(minValidFracSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            ecProject_, &EcProject::setPwbMinValidFrac);
    connect(hdiThreshSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            ecProject_, &EcProject::setPwbHdiThresh);
    connect(devThreshSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            ecProject_, &EcProject::setPwbDevThresh);
    connect(hdiPrefilterSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            ecProject_, &EcProject::setPwbHdiPrefilter);
    connect(smoothingWidthSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            ecProject_, &EcProject::setPwbSmoothingWidth);
    connect(maxCarrySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            ecProject_, &EcProject::setPwbMaxCarryH);
    connect(randomSeedSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            ecProject_, &EcProject::setPwbRandomSeed);

    refresh();
}

void PwbTimelagSettingsDialog::refresh()
{
    bool oldmod = ecProject_->modified();
    ecProject_->blockSignals(true);

    existingRadio->setChecked(!ecProject_->timelagOptMode());
    nonExistingRadio->setChecked(ecProject_->timelagOptMode());
    fileBrowse->setPath(ecProject_->timelagOptFile());

    rebuildLagRows();

    nBootstrapSpin->setValue(ecProject_->pwbNBootstrap());
    blockLengthSpin->setValue(ecProject_->pwbBlockLength());
    minValidFracSpin->setValue(ecProject_->pwbMinValidFrac());
    hdiThreshSpin->setValue(ecProject_->pwbHdiThresh());
    devThreshSpin->setValue(ecProject_->pwbDevThresh());
    hdiPrefilterSpin->setValue(ecProject_->pwbHdiPrefilter());
    smoothingWidthSpin->setValue(ecProject_->pwbSmoothingWidth());
    maxCarrySpin->setValue(ecProject_->pwbMaxCarryH());
    randomSeedSpin->setValue(ecProject_->pwbRandomSeed());

    setPwbControlsEnabled(ecProject_->timelagOptMode() != 0);

    ecProject_->setModified(oldmod);
    ecProject_->blockSignals(false);
}

void PwbTimelagSettingsDialog::updateTlMode(int radioButton)
{
    ecProject_->setTimelagOptMode(radioButton);
    setPwbControlsEnabled(radioButton != 0);
}

void PwbTimelagSettingsDialog::setPwbControlsEnabled(bool enabled)
{
    fileBrowse->setEnabled(!enabled);
    pwbOptionsContainer->setEnabled(enabled);
}

void PwbTimelagSettingsDialog::updateFile(const QString& fp)
{
    ecProject_->setTimelagOptFile(QDir::cleanPath(fp));
}

void PwbTimelagSettingsDialog::testSelectedFile(const QString& fp)
{
    if (fp.isEmpty()) { return; }

    QFileInfo paramFilePath(fp);
    QString canonicalParamFile = paramFilePath.canonicalFilePath();
    if (canonicalParamFile.isEmpty())
    {
        fileBrowse->clear();
        return;
    }

    // PWB caches are validated by the engine. Standard aggregate time-lag
    // assessment files continue through the existing ancillary-file validator.
    QFile pwbCache(canonicalParamFile);
    if (pwbCache.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        const auto firstLine = QString::fromUtf8(pwbCache.readLine()).trimmed();
        if (firstLine == QLatin1String("PWB_TIMELAG_CACHE_VERSION=1") ||
            firstLine == QLatin1String("PWB_TIMELAG_CACHE_VERSION=2"))
        {
            fileBrowse->setPath(canonicalParamFile);
            WidgetUtils::rememberDialogPath(QStringLiteral("timelag_file"), canonicalParamFile, true);
            WidgetUtils::rememberDialogPath(QStringLiteral("timelag_import_file"), canonicalParamFile, true);
            return;
        }
    }

    AncillaryFileTest test_dialog(AncillaryFileTest::FileType::TimeLag, ecProject_, this);
    test_dialog.refresh(canonicalParamFile);

    auto test_result = test_dialog.makeTest();
    auto dialog_result = true;
    if (!test_result)
    {
        dialog_result = test_dialog.exec();
    }

    if (dialog_result)
    {
        fileBrowse->setPath(canonicalParamFile);
        WidgetUtils::rememberDialogPath(QStringLiteral("timelag_file"), canonicalParamFile, true);
        WidgetUtils::rememberDialogPath(QStringLiteral("timelag_import_file"), canonicalParamFile, true);
    }
    else
    {
        fileBrowse->clear();
    }
}

/// Rebuild one search-window row per configured gas.
///
/// Every gas keeps its window on its own record, which is where the engine
/// reads it from. The flat pwb_*_lag keys the first four used to mirror are
/// retired; an upgraded project has had them moved onto its records.
void PwbTimelagSettingsDialog::rebuildLagRows()
{
    if (!lagGrid_ || !ecProject_) { return; }

    for (const auto &row : lagRows_)
    {
        if (row.label) { row.label->deleteLater(); }
        if (row.minSpin) { row.minSpin->deleteLater(); }
        if (row.maxSpin) { row.maxSpin->deleteLater(); }
    }
    lagRows_.clear();

    const auto &gases = ecProject_->gasColumns();
    int gridRow = 1;
    for (int i = 0; i < gases.size(); ++i)
    {
        // A record with no column is a slot the project keeps for ordering,
        // not a measurement; it gets no row.
        if (gases.at(i).rawColumn <= 0) { continue; }

        LagRow row;
        row.gasIndex = i;
        row.minSpin = createLagSpin();
        row.maxSpin = createLagSpin();

        auto text = gases.at(i).slug.toUpper();
        if (MeasurementRecords::isRealInstrument(gases.at(i).instrumentId))
        {
            text += QStringLiteral(" (") + gases.at(i).instrumentId
                    + QStringLiteral(")");
        }
        row.label = new QLabel(tr("%1 :").arg(text));

        lagGrid_->addWidget(row.label, gridRow, 0, Qt::AlignRight);
        lagGrid_->addWidget(row.minSpin, gridRow, 1);
        lagGrid_->addWidget(row.maxSpin, gridRow, 2);

        row.minSpin->setValue(pwbMinLagFor(i));
        row.maxSpin->setValue(pwbMaxLagFor(i));

        const int idx = i;
        connect(row.minSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [=](double v) { onLagChanged(idx, true, v); });
        connect(row.maxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [=](double v) { onLagChanged(idx, false, v); });

        lagRows_.append(row);
        ++gridRow;
    }
}

void PwbTimelagSettingsDialog::onLagChanged(int gasIndex, bool isMin, double value)
{
    if (!ecProject_) { return; }
    auto gases = ecProject_->gasColumns();
    if (gasIndex < 0 || gasIndex >= gases.size()) { return; }

    if (isMin) { gases[gasIndex].proc.pwbMinLag = value; }
    else       { gases[gasIndex].proc.pwbMaxLag = value; }
    ecProject_->setGasColumns(gases);

}

/// Stored minimum for a gas, or the window the engine searches without one.
///
/// Both of these answered 0.0 when the record carried no window, while the
/// engine's own default is [-10, +10] - so the dialog showed a window nobody
/// was using, and a user who wanted a minimum of 0 could not say so: setting a
/// spin to the value it already displays emits no signal, the record kept its
/// sentinel, and the engine went on searching from -10.
double PwbTimelagSettingsDialog::pwbMinLagFor(int gasIndex) const
{
    const auto &gases = ecProject_->gasColumns();
    if (gasIndex >= 0 && gasIndex < gases.size()
        && gases.at(gasIndex).proc.pwbMinLag > -9000.0)
    {
        return gases.at(gasIndex).proc.pwbMinLag;
    }
    return kDefaultMinLag;
}

double PwbTimelagSettingsDialog::pwbMaxLagFor(int gasIndex) const
{
    const auto &gases = ecProject_->gasColumns();
    if (gasIndex >= 0 && gasIndex < gases.size()
        && gases.at(gasIndex).proc.pwbMaxLag > -9000.0)
    {
        return gases.at(gasIndex).proc.pwbMaxLag;
    }
    return kDefaultMaxLag;
}

QDoubleSpinBox *PwbTimelagSettingsDialog::createLagSpin()
{
    auto spin = new QDoubleSpinBox;
    spin->setDecimals(1);
    spin->setRange(-1000.0, 1000.0);
    spin->setSingleStep(0.1);
    spin->setAccelerated(true);
    spin->setSuffix(tr("  [s]"));
    return spin;
}

QDoubleSpinBox *PwbTimelagSettingsDialog::createSecondsSpin(double min, double max)
{
    auto spin = new QDoubleSpinBox;
    spin->setDecimals(2);
    spin->setRange(min, max);
    spin->setSingleStep(0.1);
    spin->setAccelerated(true);
    spin->setSuffix(tr("  [s]"));
    return spin;
}

QDoubleSpinBox *PwbTimelagSettingsDialog::createFractionSpin()
{
    auto spin = new QDoubleSpinBox;
    spin->setDecimals(3);
    spin->setRange(0.0, 1.0);
    spin->setSingleStep(0.05);
    spin->setAccelerated(true);
    return spin;
}
