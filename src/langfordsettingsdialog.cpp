/***************************************************************************
  langfordsettingsdialog.cpp
  --------------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#include "langfordsettingsdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "ecproject.h"
#include "widget_utils.h"

namespace
{
//> The engine's defaults, which the two sides must agree on -
//> tests/test_langford_lod_static.py compares them. +/-150-180 s is
//> Langford's own region, RMSE the statistic they recommend, 1.96 the 95 %
//> coverage factor; ten integral scales is EddyFlow's choice for the
//> automatic window, not a published value.
constexpr int DefaultWindow = 0;
constexpr int DefaultStat = 0;
constexpr double DefaultItsMult = 10.0;
constexpr double DefaultAlpha = 1.96;
//> The window row that measures the integral turbulence scale.
constexpr int AutomaticWindow = 2;
}

LangfordSettingsDialog::LangfordSettingsDialog(QWidget *parent, EcProject *ecProject) :
    QDialog(parent),
    ecProject_(ecProject)
{
    setWindowTitle(tr("Limit of detection (Langford et al. 2015)"));
    setWindowModality(Qt::WindowModal);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    auto intro = new QLabel(tr("Far from the transport lag, vertical wind and "
        "the scalar are no longer correlated, so the cross-covariance function "
        "out there is noise. Its scatter is the random flux error RE, and the "
        "limit of detection is LoD = α × RE (Langford et al. 2015, "
        "Sect. 2.2). These settings apply wherever the method is chosen: as the "
        "random uncertainty method and as the flux detection limit."), this);
    intro->setWordWrap(true);

    //> Combo rows are the ini values (lod_window, lod_stat), so no mapping.
    auto windowLabel = new QLabel(tr("Lag window :"), this);
    windowCombo = new QComboBox(this);
    windowCombo->addItem(tr("±150–180 s (Langford et al. 2015)"));
    windowCombo->addItem(tr("±160–180 s (Maier et al. 2022)"));
    windowCombo->addItem(tr("Automatic (multiple of the integral turbulence scale, after Spirig et al. 2005)"));
    windowCombo->setItemData(0, tr("<b>±150–180 s (Langford et al. 2015):</b> "
        "The far-lag region of Langford et al. (2015), Sect. 2.2: from 150 to "
        "180 s either side of the lag."), Qt::ToolTipRole);
    windowCombo->setItemData(1, tr("<b>±160–180 s (Maier et al. 2022):</b> "
        "The region Maier et al. (2022) used for N<sub>2</sub>O and CH<sub>4</sub>, "
        "taken from Spirig et al. (2005)."), Qt::ToolTipRole);
    windowCombo->setItemData(2, tr("<b>Automatic:</b> Spirig et al. (2005) placed "
        "their window several integral time scales away from the lag, where vertical "
        "wind and the scalar no longer correlate. Neither they nor Langford et al. "
        "(2015) give a rule for it, so <b>the rule here is EddyFlow's own</b>: the "
        "window starts at the multiple below of the integral turbulence scale, or "
        "10 s past the gas's time-lag search half-width, whichever is later, and is "
        "30 s wide. The scale is measured per period and variable with the "
        "<i>Definition of the ITS</i> and <i>Maximum correlation period</i> of the "
        "random uncertainty settings. Where it cannot be measured, ±150–180 s is "
        "used."), Qt::ToolTipRole);
    windowCombo->setToolTip(tr("<b>Lag window:</b> Where the cross-covariance "
        "is read for noise, in seconds either side of the lag the flux uses."));
    windowLabel->setToolTip(windowCombo->toolTip());

    itsMultLabel = new QLabel(tr("Integral turbulence scales :"), this);
    itsMultSpin = new QDoubleSpinBox(this);
    itsMultSpin->setRange(1.0, 100.0);
    itsMultSpin->setDecimals(1);
    itsMultSpin->setSingleStep(1.0);
    itsMultSpin->setPrefix(tr("× "));
    itsMultSpin->setToolTip(tr("<b>Integral turbulence scales:</b> How many "
        "integral turbulence scales from the lag the automatic window starts. "
        "EddyFlow's choice, default 10; not a published value. Only used with "
        "the automatic window."));
    itsMultLabel->setToolTip(itsMultSpin->toolTip());

    auto statLabel = new QLabel(tr("Statistic :"), this);
    statCombo = new QComboBox(this);
    statCombo->addItem(tr("RMSE (recommended)"));
    statCombo->addItem(tr("Standard deviation"));
    statCombo->setItemData(0, tr("<b>RMSE:</b> RE<sub>RMSE</sub> = "
        "&radic;(&sigma;<sup>2</sup> + &mu;<sup>2</sup>), the root mean square of "
        "the cross-covariance in the window (Langford et al. 2015, Eq. 9). It also "
        "counts an offset of the function from zero, which a trend or a "
        "persistent structure produces, and which the standard deviation misses. "
        "The method Langford et al. use."), Qt::ToolTipRole);
    statCombo->setItemData(1, tr("<b>Standard deviation:</b> RE<sub>SD</sub> = "
        "&sigma;, the scatter of the cross-covariance about its own mean in the "
        "window. Smaller than the RMSE wherever the function sits off zero."),
        Qt::ToolTipRole);
    statCombo->setToolTip(tr("<b>Statistic:</b> How the noise in the window is "
        "summarised into the random error."));
    statLabel->setToolTip(statCombo->toolTip());

    auto alphaLabel = new QLabel(tr("Coverage factor α :"), this);
    alphaSpin = new QDoubleSpinBox(this);
    alphaSpin->setRange(1.0, 5.0);
    alphaSpin->setDecimals(2);
    alphaSpin->setSingleStep(0.01);
    alphaSpin->setToolTip(tr("<b>Coverage factor &alpha;:</b> LoD = &alpha; "
        "&times; RE. 1.96 for the 95 % level, as Langford et al. (2015) and "
        "Maier et al. (2022) use; 3 for 99 %. Only scales the <i>lod_*</i> and "
        "<i>_LOD</i> columns; the random error itself is written unscaled."));
    alphaLabel->setToolTip(alphaSpin->toolTip());

    auto grid = new QGridLayout;
    grid->addWidget(windowLabel, 0, 0, Qt::AlignRight);
    grid->addWidget(windowCombo, 0, 1);
    grid->addWidget(itsMultLabel, 1, 0, Qt::AlignRight);
    grid->addWidget(itsMultSpin, 1, 1, Qt::AlignLeft);
    grid->addWidget(statLabel, 2, 0, Qt::AlignRight);
    grid->addWidget(statCombo, 2, 1, Qt::AlignLeft);
    grid->addWidget(alphaLabel, 3, 0, Qt::AlignRight);
    grid->addWidget(alphaSpin, 3, 1, Qt::AlignLeft);
    grid->setColumnStretch(2, 1);

    auto note = new QLabel(tr("As the random uncertainty method, RE is written "
        "in flux units as <i>rand_err_*</i> and the limit of detection as "
        "<i>lod_*</i> (full output) and <i>*_LOD</i> (FLUXNET file), measured "
        "on the detrended series the flux comes from. As the flux detection "
        "limit, RE is written per gas in covariance units as <i>&lt;gas&gt;_detlim</i>, "
        "measured on the raw series about each gas's own lag. RE and LoD are "
        "multiplied by the flux's spectral correction factor while <b>Spectrally "
        "correct the random uncertainty</b> is checked (off by default); the "
        "detection limit is not."), this);
    note->setWordWrap(true);

    auto resetButton = new QPushButton(tr("Restore Default Values"), this);
    resetButton->setToolTip(tr("±150–180 s, RMSE, 10 integral turbulence "
                               "scales and α = 1.96."));
    auto closeButtons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    closeButtons->addButton(resetButton, QDialogButtonBox::ResetRole);
    closeButtons->addButton(WidgetUtils::createQuestionMarkButton(this,
        QStringLiteral("https://kebasaa.github.io/eddyflow-documentation/topics/limit-of-detection-settings/")),
        QDialogButtonBox::HelpRole);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(intro);
    mainLayout->addLayout(grid);
    mainLayout->addWidget(note);
    mainLayout->addStretch();
    mainLayout->addWidget(closeButtons);
    setLayout(mainLayout);

    connect(windowCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        ecProject_->setScreenLodWindow(index);
        updateEnabled();
    });
    connect(statCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        ecProject_->setScreenLodStat(index);
    });
    connect(itsMultSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        ecProject_->setScreenLodItsMult(v);
    });
    connect(alphaSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        ecProject_->setRandomErrorLodAlpha(v);
    });
    connect(resetButton, &QPushButton::clicked,
            this, &LangfordSettingsDialog::restoreDefaults);
    connect(closeButtons, &QDialogButtonBox::rejected, this, &QDialog::hide);
}

/// Re-read the project into the controls, signals blocked: setting a value
/// here is not the user changing it.
void LangfordSettingsDialog::refresh()
{
    const QSignalBlocker blockWindow(windowCombo);
    const QSignalBlocker blockStat(statCombo);
    const QSignalBlocker blockMult(itsMultSpin);
    const QSignalBlocker blockAlpha(alphaSpin);

    //> Sanitised on load already; clamped again so a value from a later
    //> version never lands on no row.
    const int window = ecProject_->screenLodWindow();
    windowCombo->setCurrentIndex((window >= 0 && window <= 2) ? window : DefaultWindow);
    statCombo->setCurrentIndex(ecProject_->screenLodStat() == 1 ? 1 : DefaultStat);
    itsMultSpin->setValue(ecProject_->screenLodItsMult());
    alphaSpin->setValue(ecProject_->randErrorLodAlpha());
    updateEnabled();
}

void LangfordSettingsDialog::restoreDefaults()
{
    ecProject_->setScreenLodWindow(DefaultWindow);
    ecProject_->setScreenLodStat(DefaultStat);
    ecProject_->setScreenLodItsMult(DefaultItsMult);
    ecProject_->setRandomErrorLodAlpha(DefaultAlpha);
    refresh();
}

void LangfordSettingsDialog::updateEnabled()
{
    const bool automatic = windowCombo->currentIndex() == AutomaticWindow;
    itsMultLabel->setEnabled(automatic);
    itsMultSpin->setEnabled(automatic);
}
