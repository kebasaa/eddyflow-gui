/***************************************************************************
  detlimsettingsdialog.cpp
  ------------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#include "detlimsettingsdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyle>
#include <QVBoxLayout>

#include "ecproject.h"

namespace
{
//> Wienhold's own geometry: two 50 s windows placed 100 s either side of the
//> gas's own lag. These are the values the engine defaults to as well, and
//> the pair has to stay in step - tests/test_detlim_engine_contract_static.py
//> compares the two files.
constexpr double DefaultOffset = 100.0;
constexpr double DefaultWindow = 50.0;
//> Off. The feature adds an output column and must not change what an
//> existing project computes.
constexpr int DefaultMethod = 0;
}

DetlimSettingsDialog::DetlimSettingsDialog(QWidget *parent, EcProject *ecProject) :
    QDialog(parent),
    ecProject_(ecProject)
{
    setWindowTitle(tr("Flux Detection Limit"));
    setWindowModality(Qt::WindowModal);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    methodLabel = new QLabel(tr("Method :"), this);
    methodCombo = new QComboBox(this);
    methodCombo->addItem(tr("None"));
    methodCombo->addItem(tr("Wienhold et al. (1994)"));
    methodCombo->addItem(tr("Langford et al. (2015)"));
    methodCombo->setItemData(0, tr("<b>None:</b> Do not estimate a detection "
        "limit. The output columns are written, carrying the error code."),
        Qt::ToolTipRole);
    methodCombo->setItemData(1, tr("<b>Wienhold et al. (1994):</b> The "
        "cross-covariance of vertical wind with a scalar carries the flux in "
        "a peak near the transport lag and nothing but noise far away from "
        "it. The scatter of the function out there is therefore a noise floor "
        "on the covariance: a flux smaller than it cannot be distinguished "
        "from zero. Two windows are placed symmetrically either side of the "
        "lag actually used for each gas, and their standard deviations "
        "averaged."), Qt::ToolTipRole);
    methodCombo->setItemData(2, tr("<b>Langford et al. (2015):</b> The same "
        "idea, measured as Langford et al. recommend: the root mean square of "
        "the cross-covariance over both far-lag windows together, "
        "RE<sub>RMSE</sub> = &radic;(&sigma;<sup>2</sup> + &mu;<sup>2</sup>) "
        "(their Eq. 9), which also counts an offset of the function from zero "
        "that the standard deviation misses. Its window and statistic are "
        "shared with the random uncertainty method of that name, under "
        "<b>Langford Settings...</b>."), Qt::ToolTipRole);
    methodCombo->setToolTip(tr("<b>Method:</b> Select how the flux detection "
        "limit is estimated. Off by default; switching it on does not change "
        "any flux, it only adds the limit alongside it."));
    methodLabel->setToolTip(methodCombo->toolTip());

    offsetLabel = new QLabel(tr("Window offset :"), this);
    offsetSpin = new QDoubleSpinBox(this);
    offsetSpin->setRange(1.0, 3600.0);
    offsetSpin->setDecimals(1);
    offsetSpin->setSingleStep(10.0);
    offsetSpin->setSuffix(tr("  [s]"));
    offsetSpin->setAccelerated(true);
    offsetSpin->setToolTip(tr("<b>Window offset:</b> How far from each gas's "
        "own time lag the two noise windows are centred, one before and one "
        "after. It must clear the cross-covariance peak: too small and the "
        "windows measure the flux rather than the noise under it, returning a "
        "detection limit that rises with the flux itself. Wienhold et al. use "
        "100 s."));
    offsetLabel->setToolTip(offsetSpin->toolTip());

    windowLabel = new QLabel(tr("Window width :"), this);
    windowSpin = new QDoubleSpinBox(this);
    windowSpin->setRange(1.0, 3600.0);
    windowSpin->setDecimals(1);
    windowSpin->setSingleStep(5.0);
    windowSpin->setSuffix(tr("  [s]"));
    windowSpin->setAccelerated(true);
    windowSpin->setToolTip(tr("<b>Window width:</b> How much of the "
        "cross-covariance function each noise window averages. Wider is a "
        "steadier estimate but reaches closer to the peak; narrower is "
        "noisier. Wienhold et al. use 50 s. The width is centred on the "
        "offset, and must stay below it: the engine refuses a window as wide "
        "as its own offset."));
    windowLabel->setToolTip(windowSpin->toolTip());

    windowWarningLabel = new QLabel(this);
    windowWarningLabel->setPixmap(
        style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(16, 16));
    windowWarningLabel->setToolTip(tr("The window is as wide as the offset or "
        "wider, which brings it close enough to the cross-covariance peak "
        "that it would measure the flux rather than the noise. The engine "
        "refuses this combination and falls back to 100 s and 50 s."));
    windowWarningLabel->setVisible(false);

    langfordButton = new QPushButton(tr("Langford Settings..."), this);
    langfordButton->setToolTip(tr("<b>Langford settings:</b> The lag window and "
        "statistic of the limit of detection after Langford et al. (2015). "
        "Shared with the random uncertainty method of that name."));

    auto grid = new QGridLayout;
    grid->addWidget(methodLabel, 0, 0, Qt::AlignRight);
    grid->addWidget(methodCombo, 0, 1);
    grid->addWidget(offsetLabel, 1, 0, Qt::AlignRight);
    grid->addWidget(offsetSpin, 1, 1);
    grid->addWidget(windowLabel, 2, 0, Qt::AlignRight);
    grid->addWidget(windowSpin, 2, 1);
    grid->addWidget(windowWarningLabel, 2, 2);
    grid->addWidget(langfordButton, 3, 1, Qt::AlignLeft);
    grid->setColumnStretch(3, 1);
    grid->setRowStretch(4, 1);

    auto note = new QLabel(tr("The limit is reported per gas in <b>covariance "
        "units</b>, as <i>&lt;gas&gt;_detlim</i> in the full output and "
        "<i>&lt;GAS&gt;_DETLIM</i> in the FLUXNET file. It is deliberately "
        "not scaled to a flux: what it qualifies is the covariance, and the "
        "flux has been through the spectral correction while the limit has "
        "not."), this);
    note->setWordWrap(true);

    auto resetButton = new QPushButton(tr("Restore Default Values"), this);
    resetButton->setToolTip(tr("Reset the detection limit settings to "
                               "Wienhold's values, with the method off."));
    auto closeButtons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    closeButtons->addButton(resetButton, QDialogButtonBox::ResetRole);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addLayout(grid);
    mainLayout->addWidget(note);
    mainLayout->addStretch();
    mainLayout->addWidget(closeButtons);
    setLayout(mainLayout);

    connect(methodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        //> The row is the stored value: 0 none, 1 Wienhold, 2 Langford.
        ecProject_->setScreenDetlimMethod(index);
        updateEnabled();
    });
    connect(langfordButton, &QPushButton::clicked,
            this, &DetlimSettingsDialog::langfordSettingsRequested);
    connect(offsetSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        ecProject_->setScreenDetlimOffset(v);
        updateWindowWarning();
    });
    connect(windowSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        ecProject_->setScreenDetlimWindow(v);
        updateWindowWarning();
    });
    connect(resetButton, &QPushButton::clicked,
            this, &DetlimSettingsDialog::restoreDefaults);
    connect(closeButtons, &QDialogButtonBox::rejected, this, &QDialog::hide);
}

/// Re-read the project into the controls.
///
/// Signals blocked throughout: these spins write straight back to EcProject,
/// and setting a value here is not the user changing it.
void DetlimSettingsDialog::refresh()
{
    const QSignalBlocker blockMethod(methodCombo);
    const QSignalBlocker blockOffset(offsetSpin);
    const QSignalBlocker blockWindow(windowSpin);

    //> A value from a later version lands on no row; show what the engine
    //> will do with it, which is nothing.
    const int method = ecProject_->screenDetlimMethod();
    methodCombo->setCurrentIndex((method >= 0 && method <= 2) ? method : 0);
    offsetSpin->setValue(ecProject_->screenDetlimOffset());
    windowSpin->setValue(ecProject_->screenDetlimWindow());
    updateEnabled();
}

/// Each method's own settings, and nothing else: Wienhold's windows mean
/// nothing to Langford, whose settings live in the shared dialog.
void DetlimSettingsDialog::updateEnabled()
{
    const int method = methodCombo->currentIndex();
    const bool wienhold = method == 1;
    offsetLabel->setEnabled(wienhold);
    offsetSpin->setEnabled(wienhold);
    windowLabel->setEnabled(wienhold);
    windowSpin->setEnabled(wienhold);
    langfordButton->setEnabled(method == 2);
    updateWindowWarning();
}

void DetlimSettingsDialog::restoreDefaults()
{
    methodCombo->setCurrentIndex(DefaultMethod);
    offsetSpin->setValue(DefaultOffset);
    windowSpin->setValue(DefaultWindow);
    ecProject_->setScreenDetlimMethod(DefaultMethod);
    ecProject_->setScreenDetlimOffset(DefaultOffset);
    ecProject_->setScreenDetlimWindow(DefaultWindow);
    refresh();
}

/// The one misconfiguration worth flagging.
///
/// The window spans half its width either side of the offset, so its inner
/// edge sits at offset - width/2. The engine does not wait for that to reach
/// zero: it refuses any width at or above the offset, which keeps the inner
/// edge at half the offset or further out. Mirrored here so the user sees it
/// while setting the numbers rather than afterwards in the log.
void DetlimSettingsDialog::updateWindowWarning()
{
    const bool on = methodCombo->currentIndex() == 1;
    const bool reachesPeak = windowSpin->value() >= offsetSpin->value();
    windowWarningLabel->setVisible(on && reachesPeak);
}
