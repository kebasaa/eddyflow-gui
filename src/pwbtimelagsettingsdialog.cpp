/***************************************************************************
  pwbtimelagsettingsdialog.cpp
  ----------------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#include "pwbtimelagsettingsdialog.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
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
    existingRadio->setToolTip(tr("<b>Time-lag file available:</b> Select either a per-period PWB cache or a standard Time-lag_optimisation_results file. A PWB cache reuses exact timestamp- and gas-specific lags; missing entries are detected and saved to a new output-side cache. An aggregate file uses its gas and H2O RH-class lags for the whole run and does not run PWB."));

    nonExistingRadio = new QRadioButton(tr("Time lag file not available :"));
    nonExistingRadio->setToolTip(tr("<b>Time lag file not available:</b> Choose this option and provide the following information if you need to detect time lags for your dataset with pre-whitening block-bootstrap."));

    fileBrowse = new FileBrowseWidget;
    fileBrowse->setToolTip(tr("<b>Load:</b> Load a PWB per-period cache or aggregate time-lag file"));
    fileBrowse->setDialogTitle(tr("Select a PWB Cache or Time-Lag Results File"));
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

    detectOnRawCheckBox = new QCheckBox(tr("Detect time lag on raw data (pre-processing step)"));
    detectOnRawCheckBox->setToolTip(tr(
        "<b>Detect time lag on raw data:</b><br>"
        "When enabled, PWB time lag detection runs as a pre-processing step directly on "
        "the raw high-frequency data, before any block averaging or flux computation. "
        "This can improve lag accuracy for instruments with variable or drift-prone lags "
        "by working with the full temporal resolution of the data.<br><br>"
        "<b>When to use:</b> Enable when closed-path instruments show systematic lag "
        "variability within an averaging period, or when block-averaged signals lack "
        "the resolution needed for reliable peak detection.<br><br>"
        "<b>Default:</b> Unchecked — detection runs on the averaged time series as normal."));

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

    minValidFracSpin = createFractionSpin();
    hdiThreshSpin = createSecondsSpin(0.0, 100.0);
    devThreshSpin = createSecondsSpin(0.0, 100.0);
    hdiPrefilterSpin = createSecondsSpin(0.0, 100.0);
    hdiPrefilterSpin->setSpecialValueText(tr("Disabled"));

    smoothingWidthSpin = new QSpinBox;
    smoothingWidthSpin->setRange(1, 999);
    smoothingWidthSpin->setSingleStep(2);
    smoothingWidthSpin->setAccelerated(true);

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
    detection->addWidget(new QLabel(tr("Random seed :")), 8, 0, Qt::AlignRight);
    detection->addWidget(randomSeedSpin, 8, 1);
    detection->setColumnStretch(2, 1);

    // Speed options group
    auto speedGroup = new QGroupBox(tr("Speed options"));
    auto warningLabel = new QLabel(
        tr("<i>&#9888; These options may slightly affect outputs — see tooltips for details.</i>"));
    warningLabel->setWordWrap(true);

    approxCcfCheckBox = new QCheckBox(tr("Use approximate CCF (faster)"));
    approxCcfCheckBox->setToolTip(tr(
        "<b>Use approximate CCF (faster):</b><br>"
        "Skips variance normalisation inside the bootstrap CCF loop. Only the "
        "cross-covariance (not the full correlation coefficient) is used to locate "
        "the lag peak.<br><br>"
        "<b>Speedup:</b> ~2–3× additional; ~4–5× total vs. the "
        "original two-pass implementation.<br><br>"
        "<b>Output impact:</b> The argmax of the normalised and unnormalised CCF is "
        "practically always identical when the lag window is small relative to N "
        "(e.g. standard 30-min periods with ±10 s windows). A shift of ±1 sample "
        "is possible in rare edge cases: very short periods, very wide lag windows, "
        "or low data availability after gap-filling.<br><br>"
        "<b>Guidance:</b> Suitable for routine production runs. Leave unchecked for "
        "validation runs, method comparisons, or non-standard setups."));

    maxArOrderCheckBox = new QCheckBox(tr("Cap AR model order"));
    maxArOrderCheckBox->setToolTip(tr(
        "<b>Cap AR model order:</b><br>"
        "By default, AIC-based AR model selection searches up to order ~455 for "
        "30-min 20 Hz data. Enabling this cap limits the search to the value shown, "
        "reducing AR fitting time by ~9× for a cap of 100, and proportionally more "
        "for smaller caps.<br><br>"
        "<b>Speedup:</b> The AR step is ~10–15%% of total PWB time, so the "
        "end-to-end gain is ~10–15%% additional.<br><br>"
        "<b>Output impact:</b> If the true optimal AR order exceeds the cap, "
        "pre-whitening is slightly less effective, potentially widening the HDI or "
        "shifting the selected lag. Most likely to occur for strongly autocorrelated "
        "gases (e.g. H₂O in humid conditions). In practice, EC turbulence signals "
        "are well-described by AR(1)–AR(10).<br><br>"
        "<b>Guidance:</b> Leave uncapped for final archival datasets. "
        "A cap of 100 is a conservative starting point. Reducing the cap further "
        "(e.g. to 50 or below) gives more speedup but increases the risk of "
        "under-fitting the AR model, which may widen the HDI or shift the selected lag."));

    maxArOrderSpin = new QSpinBox;
    maxArOrderSpin->setRange(1, 1000);
    maxArOrderSpin->setValue(100);
    maxArOrderSpin->setEnabled(false);
    maxArOrderSpin->setToolTip(maxArOrderCheckBox->toolTip());

    auto maxArRow = new QHBoxLayout;
    maxArRow->addWidget(maxArOrderCheckBox);
    maxArRow->addWidget(maxArOrderSpin);
    maxArRow->addStretch();

    auto speedLayout = new QVBoxLayout;
    speedLayout->addWidget(warningLabel);
    speedLayout->addWidget(approxCcfCheckBox);
    speedLayout->addLayout(maxArRow);
    speedGroup->setLayout(speedLayout);

    auto pwbOptionsLayout = new QVBoxLayout;
    pwbOptionsLayout->addWidget(detectOnRawCheckBox);
    // Gases on one analyser share a detected lag (PWB's S4_instrument_shared
    // rule), so a window set for one of them decides the others too. Without
    // saying so, narrowing CO2's window and watching H2O move looks like a bug.
    auto sharingNote = new QLabel(tr(
        "<i>Gases measured by the same instrument share a detected time lag: "
        "when one is detected natively, the others on that instrument adopt "
        "it. Narrowing one gas's window therefore affects every gas on the "
        "same analyser.</i>"));
    sharingNote->setWordWrap(true);

    pwbOptionsLayout->addLayout(windows);
    pwbOptionsLayout->addWidget(sharingNote);
    pwbOptionsLayout->addSpacing(10);
    pwbOptionsLayout->addLayout(detection);
    pwbOptionsLayout->addSpacing(10);
    pwbOptionsLayout->addWidget(speedGroup);
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
    connect(randomSeedSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            ecProject_, &EcProject::setPwbRandomSeed);
    connect(detectOnRawCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        ecProject_->setPwbDetectOnRaw(checked ? 1 : 0);
    });
    connect(approxCcfCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        ecProject_->setPwbApproxCcf(checked ? 1 : 0);
    });
    connect(maxArOrderCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        maxArOrderSpin->setEnabled(checked);
        ecProject_->setPwbMaxArOrder(checked ? maxArOrderSpin->value() : 0);
    });
    connect(maxArOrderSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int n) {
        if (maxArOrderCheckBox->isChecked())
            ecProject_->setPwbMaxArOrder(n);
    });

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
    randomSeedSpin->setValue(ecProject_->pwbRandomSeed());

    detectOnRawCheckBox->setChecked(ecProject_->pwbDetectOnRaw() != 0);
    approxCcfCheckBox->setChecked(ecProject_->pwbApproxCcf() != 0);

    const int maxAr = ecProject_->pwbMaxArOrder();
    const bool capEnabled = (maxAr > 0);
    maxArOrderCheckBox->setChecked(capEnabled);
    maxArOrderSpin->setEnabled(capEnabled);
    if (capEnabled)
        maxArOrderSpin->setValue(maxAr);

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
    if (enabled)
    {
        maxArOrderSpin->setEnabled(maxArOrderCheckBox->isChecked());
    }
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
/// The first four gas records also mirror the flat pwb_*_lag keys, so a
/// project written by an older version keeps its values and an older version
/// can still read what this writes. Gases past the fourth live only in the
/// record's own settings, which is where the engine reads them from.
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

    // Mirror the historical four, so the flat keys stay in step with the
    // records and an older version reads the same windows.
    switch (gasIndex)
    {
        case 0: if (isMin) { ecProject_->setPwbCo2MinLag(value); }
                else       { ecProject_->setPwbCo2MaxLag(value); } break;
        case 1: if (isMin) { ecProject_->setPwbH2oMinLag(value); }
                else       { ecProject_->setPwbH2oMaxLag(value); } break;
        case 2: if (isMin) { ecProject_->setPwbCh4MinLag(value); }
                else       { ecProject_->setPwbCh4MaxLag(value); } break;
        case 3: if (isMin) { ecProject_->setPwbGas4MinLag(value); }
                else       { ecProject_->setPwbGas4MaxLag(value); } break;
        default: break;
    }
}

/// Stored minimum for a gas: the record if it has one, else the flat key.
double PwbTimelagSettingsDialog::pwbMinLagFor(int gasIndex) const
{
    const auto &gases = ecProject_->gasColumns();
    if (gasIndex >= 0 && gasIndex < gases.size()
        && gases.at(gasIndex).proc.pwbMinLag > -9000.0
        && gases.at(gasIndex).proc.pwbMinLag != -1.0)
    {
        return gases.at(gasIndex).proc.pwbMinLag;
    }
    switch (gasIndex)
    {
        case 0: return ecProject_->pwbCo2MinLag();
        case 1: return ecProject_->pwbH2oMinLag();
        case 2: return ecProject_->pwbCh4MinLag();
        case 3: return ecProject_->pwbGas4MinLag();
        default: return 0.0;
    }
}

double PwbTimelagSettingsDialog::pwbMaxLagFor(int gasIndex) const
{
    const auto &gases = ecProject_->gasColumns();
    if (gasIndex >= 0 && gasIndex < gases.size()
        && gases.at(gasIndex).proc.pwbMaxLag > -9000.0
        && gases.at(gasIndex).proc.pwbMaxLag != -1.0)
    {
        return gases.at(gasIndex).proc.pwbMaxLag;
    }
    switch (gasIndex)
    {
        case 0: return ecProject_->pwbCo2MaxLag();
        case 1: return ecProject_->pwbH2oMaxLag();
        case 2: return ecProject_->pwbCh4MaxLag();
        case 3: return ecProject_->pwbGas4MaxLag();
        default: return 0.0;
    }
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
