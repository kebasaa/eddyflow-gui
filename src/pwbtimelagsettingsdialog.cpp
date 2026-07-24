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

    co2MinLagSpin = createLagSpin();
    co2MaxLagSpin = createLagSpin();
    h2oMinLagSpin = createLagSpin();
    h2oMaxLagSpin = createLagSpin();
    ch4MinLagSpin = createLagSpin();
    ch4MaxLagSpin = createLagSpin();
    gas4MinLagSpin = createLagSpin();
    gas4MaxLagSpin = createLagSpin();

    auto windows = new QGridLayout;
    windows->addWidget(windowTitle, 0, 0);
    windows->addWidget(minTitle, 0, 1);
    windows->addWidget(maxTitle, 0, 2);
    windows->addWidget(new QLabel(tr("%1 :").arg(Defs::CO2_STRING)), 1, 0, Qt::AlignRight);
    windows->addWidget(co2MinLagSpin, 1, 1);
    windows->addWidget(co2MaxLagSpin, 1, 2);
    windows->addWidget(new QLabel(tr("%1 :").arg(Defs::H2O_STRING)), 2, 0, Qt::AlignRight);
    windows->addWidget(h2oMinLagSpin, 2, 1);
    windows->addWidget(h2oMaxLagSpin, 2, 2);
    windows->addWidget(new QLabel(tr("%1 :").arg(Defs::CH4_STRING)), 3, 0, Qt::AlignRight);
    windows->addWidget(ch4MinLagSpin, 3, 1);
    windows->addWidget(ch4MaxLagSpin, 3, 2);
    windows->addWidget(new QLabel(tr("%1 :").arg(Defs::GAS4_STRING)), 4, 0, Qt::AlignRight);
    windows->addWidget(gas4MinLagSpin, 4, 1);
    windows->addWidget(gas4MaxLagSpin, 4, 2);
    windows->setColumnStretch(3, 1);

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
    pwbOptionsLayout->addLayout(windows);
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

    connect(co2MinLagSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            ecProject_, &EcProject::setPwbCo2MinLag);
    connect(co2MaxLagSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            ecProject_, &EcProject::setPwbCo2MaxLag);
    connect(h2oMinLagSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            ecProject_, &EcProject::setPwbH2oMinLag);
    connect(h2oMaxLagSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            ecProject_, &EcProject::setPwbH2oMaxLag);
    connect(ch4MinLagSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            ecProject_, &EcProject::setPwbCh4MinLag);
    connect(ch4MaxLagSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            ecProject_, &EcProject::setPwbCh4MaxLag);
    connect(gas4MinLagSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            ecProject_, &EcProject::setPwbGas4MinLag);
    connect(gas4MaxLagSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            ecProject_, &EcProject::setPwbGas4MaxLag);
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

    co2MinLagSpin->setValue(ecProject_->pwbCo2MinLag());
    co2MaxLagSpin->setValue(ecProject_->pwbCo2MaxLag());
    h2oMinLagSpin->setValue(ecProject_->pwbH2oMinLag());
    h2oMaxLagSpin->setValue(ecProject_->pwbH2oMaxLag());
    ch4MinLagSpin->setValue(ecProject_->pwbCh4MinLag());
    ch4MaxLagSpin->setValue(ecProject_->pwbCh4MaxLag());
    gas4MinLagSpin->setValue(ecProject_->pwbGas4MinLag());
    gas4MaxLagSpin->setValue(ecProject_->pwbGas4MaxLag());
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
