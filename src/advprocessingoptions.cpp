/***************************************************************************
  advprocessingoptions.cpp
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

#include "advprocessingoptions.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QIcon>
#include <QPixmap>
#include <QSize>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QDesktopServices>

#include "cecsettingsdialog.h"
#include "clicklabel.h"
#include "configstate.h"
#include "customcombomodel.h"
#include "customresetlineedit.h"
#include "dlproject.h"
#include "ecproject.h"
#include "fileutils.h"
#include "infomessage.h"
#include "irga_desc.h"
#include "dirbrowsewidget.h"
#include "planarfitsettingsdialog.h"
#include "pwbtimelagsettingsdialog.h"
#include "richtextcheckbox.h"
#include "timelagsettingsdialog.h"
#include "stringutils.h"
#include "widget_utils.h"

AdvProcessingOptions::AdvProcessingOptions(QWidget *parent,
                                           DlProject *dlProject,
                                           EcProject *ecProject,
                                           ConfigState *config) :
    QWidget(parent),
    dlProject_(dlProject),
    ecProject_(ecProject),
    configState_(config)
{
    createQuestionMark();

    auto rawProcessingTitle = new QLabel(tr("Raw data processing"));
    rawProcessingTitle->setProperty("groupLabel", true);

    windOffsetLabel = new QLabel(tr("Wind speed measurement offsets "));
    windOffsetLabel->setMinimumWidth(windOffsetLabel->sizeHint().width());
    windOffsetLabel->setMargin(11);
    windOffsetLabel->setToolTip(tr("<b>Wind speed measurement offsets:</b> Wind measurements by a sonic anemometer may be biased by systematic deviations, which need to be eliminated (e.g., for a proper assessment of tilt angles). You may get these offsets from the calibration certificate of your anemometer, but you could also assess it easily, by recording the 3 wind components from the anemometer enclosed in a box with still air (zero-wind test). Any long-term systematic deviation from zero of a wind component is a good estimation of this bias."));
    uLabel = new ClickLabel(tr("U :"));
    uLabel->setToolTip(windOffsetLabel->toolTip());
    uOffsetSpin = new QDoubleSpinBox;
    uOffsetSpin->setRange(-10.0, 10.0);
    uOffsetSpin->setSingleStep(0.1);
    uOffsetSpin->setDecimals(3);
    uOffsetSpin->setValue(0.0);
    uOffsetSpin->setAccelerated(true);
    uOffsetSpin->setSuffix(tr("  [m/s]", "Velocity"));
#if defined(Q_OS_WIN)
    uOffsetSpin->setMinimumWidth(uOffsetSpin->sizeHint().width() * 1.3);
#elif defined (Q_OS_MACOS)
    uOffsetSpin->setMinimumWidth(102);
#endif
    uOffsetSpin->setToolTip(windOffsetLabel->toolTip());

    vLabel = new ClickLabel(tr("V :"));
    vLabel->setToolTip(windOffsetLabel->toolTip());
    vOffsetSpin = new QDoubleSpinBox;
    vOffsetSpin->setRange(-10.0, 10.0);
    vOffsetSpin->setSingleStep(0.1);
    vOffsetSpin->setDecimals(3);
    vOffsetSpin->setValue(0.0);
    vOffsetSpin->setAccelerated(true);
    vOffsetSpin->setSuffix(tr("  [m/s]", "Velocity"));
#if defined(Q_OS_WIN)
    vOffsetSpin->setMinimumWidth(vOffsetSpin->sizeHint().width() * 1.3);
#elif defined(Q_OS_MACOS)
    vOffsetSpin->setMinimumWidth(102);
#endif
    vOffsetSpin->setToolTip(windOffsetLabel->toolTip());

    wLabel = new ClickLabel(tr("W :"));
    wLabel->setToolTip(windOffsetLabel->toolTip());
    wOffsetSpin = new QDoubleSpinBox;
    wOffsetSpin->setRange(-10.0, 10.0);
    wOffsetSpin->setSingleStep(0.1);
    wOffsetSpin->setDecimals(3);
    wOffsetSpin->setValue(0.0);
    wOffsetSpin->setAccelerated(true);
    wOffsetSpin->setSuffix(tr("  [m/s]", "Velocity"));
#if defined(Q_OS_WIN)
    wOffsetSpin->setMinimumWidth(wOffsetSpin->sizeHint().width() * 1.3);
#elif defined(Q_OS_MACOS)
    wOffsetSpin->setMinimumWidth(102);
#endif
    wOffsetSpin->setToolTip(windOffsetLabel->toolTip());

    auto windComponentLayout = new QHBoxLayout;
    windComponentLayout->addWidget(uLabel, 0, Qt::AlignRight);
    windComponentLayout->addWidget(uOffsetSpin, 1);
    windComponentLayout->addStretch(1);
    windComponentLayout->addWidget(vLabel, 0, Qt::AlignRight);
    windComponentLayout->addWidget(vOffsetSpin, 1);
    windComponentLayout->addStretch(1);
    windComponentLayout->addWidget(wLabel, 0, Qt::AlignRight);
    windComponentLayout->addWidget(wOffsetSpin, 1);
    windComponentLayout->addStretch(1);

    wBoostCheckBox = new RichTextCheckBox;
    wBoostCheckBox->setText(tr("Fix 'w boost' bug (Gill WindMaster and WindMaster Pro only)"));
    wBoostCheckBox->setToolTip(tr("<b>Fix 'w boost' bug:</b> Gill WindMaster and WindMaster Pro produced between 2006 and 2015 and identified by a firmware version of the form 2329.x.y with x &lt; 700, are affected by a bug such that the vertical wind speed is underestimated. Check this option to have EddyFlow fix the bug. For more details, please visit <a href=\"http://gillinstruments.com/data/manuals/KN1509_WindMaster_WBug_info.pdf\">Gill's Technical Key Note</a>"));
    wBoostCheckBox->setQuestionMark(QStringLiteral("https://www.licor.com/env/help/EddyFlow/topics_EddyFlow/w-boost_bug_correction.html"));

    aoaCheckBox = new RichTextCheckBox;
    aoaCheckBox->setToolTip(tr("<b>Angle-of-attack correction:</b> Applies only to vertical mount Gill sonic anemometers with the same geometry of the R3 (e.g., R2, WindMaster, WindMaster Pro). This correction is meant to compensate the effects of flow distortion induced by the anemometer frame on the turbulent flow field. We recommend applying this correction whenever an R3-shaped anemometer was used."));
    aoaCheckBox->setText(tr("Angle-of-attack correction for wind components (Gill's only)"));
    aoaCheckBox->setQuestionMark(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Angle_of_Attack_Correction.html"));

    aoaMethLabel = new ClickLabel(tr("Method :"));
    aoaMethLabel->setEnabled(false);
    aoaMethCombo = new QComboBox;
    aoaMethCombo->addItem(tr("Select automatically"), -1);
    aoaMethCombo->addItem(tr("Field calibration (Nakai and Shimoyama, 2012)"), 1);
    aoaMethCombo->addItem(tr("Wind tunnel calibration (Nakai et al., 2006)"), 2);
    aoaMethCombo->setItemData(0, tr("<b>Select automatically:</b> Select this option to allow EddyFlow to choose the most appropriate angle of attack correction method based on the anemometer model and - in the case of the WindMaster<sup>%1</sup> or WindMaster Pro - its firmware version.").arg(Defs::TRADEMARK_SYMBOL), Qt::ToolTipRole);
    aoaMethCombo->setItemData(1, tr("<b>Field calibration:</b> Select this option to apply the angle-of-attack correction according to the method described in the referenced paper, which makes use of a field calibration instead of the wind tunnel calibration."), Qt::ToolTipRole);
    aoaMethCombo->setItemData(2, tr("<b>Wind tunnel calibration:</b> Select this option to apply the angle-of-attack correction according to the method described in the referenced paper, which makes use of a wind tunnel calibration instead of the field calibration."), Qt::ToolTipRole);
    aoaMethCombo->setEnabled(false);

    //> Two hardware corrections that run on the raw wind before any
    //> rotation, so they sit above it here as well as in the engine.
    headCorrCheckBox = new RichTextCheckBox;
    headCorrCheckBox->setText(tr("Metek USA-1 head correction (three-dimensional flow distortion)"));
    headCorrCheckBox->setToolTip(tr("<b>Metek USA-1 head correction:</b> The "
        "transducers and their supports deflect the flow before the sonic "
        "measures it, by an amount that depends on where the wind comes from. "
        "Metek measured that in a wind tunnel and published three tables of "
        "Fourier coefficients over elevation angle - one each for the wind "
        "speed, the azimuth and the elevation - evaluated at three, six and "
        "nine times the azimuth. Applied sample by sample to the raw wind, "
        "before the inclinometer correction and any axis rotation."
        "<br><br><b>The tables are not shipped with EddyFlow.</b> They are "
        "Metek GmbH's measurements, which EddyUH redistributes under the "
        "University of Helsinki's own agreement; this program cannot. Point "
        "the directory below at your own copy of <i>phicorr.dat</i>, "
        "<i>ucorr.dat</i> and <i>alphacorr.dat</i>. Without all three the "
        "correction declines and says so in the run log, and the fluxes come "
        "out as if it had never been switched on."
        "<br><br>Applies to one-inner-bar USA-1 models only, which is what "
        "the tables were measured on. Nothing checks that, because nothing in "
        "the metadata distinguishes the variants."));

    headCorrMethLabel = new ClickLabel(tr("Applies to :"));
    headCorrMethCombo = new QComboBox;
    headCorrMethCombo->addItem(tr("Raw, uncorrected data"));
    headCorrMethCombo->addItem(tr("Data already carrying Metek's online 2-D correction"));
    headCorrMethCombo->setItemData(0, tr("<b>Raw, uncorrected data:</b> The "
        "logger applied nothing, so the three-dimensional correction is "
        "applied to the wind as recorded."), Qt::ToolTipRole);
    headCorrMethCombo->setItemData(1, tr("<b>Data already carrying Metek's "
        "online 2-D correction:</b> The sonic's own two-dimensional "
        "correction is first undone, with the closed form Metek publishes for "
        "it, and the full three-dimensional correction applied to what is "
        "left."
        "<br><br>Applying the three-dimensional correction on top of the "
        "two-dimensional one without removing it would count the horizontal "
        "part twice. If you are unsure which your logger wrote, the sonic's "
        "configuration says so - guessing costs a percent or so of the "
        "horizontal wind."), Qt::ToolTipRole);
    headCorrMethLabel->setToolTip(headCorrMethCombo->itemData(0, Qt::ToolTipRole).toString());

    headCorrDirLabel = new ClickLabel(tr("Table directory :"));
    headCorrDirBrowse = new DirBrowseWidget;
    headCorrDirBrowse->setDialogTitle(tr("Select the Metek Head Correction Table Directory"));
    headCorrDirBrowse->setToolTip(tr("<b>Table directory:</b> The folder "
        "holding <i>phicorr.dat</i>, <i>ucorr.dat</i> and "
        "<i>alphacorr.dat</i>, each twenty comma-separated rows of elevation "
        "from -50 to +45 degrees in steps of five, carrying the elevation and "
        "then C0, C3, S3, C6, S6, C9 and S9."
        "<br><br>Read once per run rather than once per averaging period. A "
        "directory missing any of the three, or holding a file with fewer "
        "than twenty rows, declines the correction for the whole run and says "
        "so in the log rather than correcting some periods and not others."));
    headCorrDirLabel->setToolTip(headCorrDirBrowse->toolTip());

    tiltSensorCheckBox = new RichTextCheckBox;
    tiltSensorCheckBox->setText(tr("Inclinometer tilt correction (fast inclination channels)"));
    tiltSensorCheckBox->setToolTip(tr("<b>Inclinometer tilt correction:</b> A "
        "mast that leans, or sways, tilts the sonic with it. A planar fit or "
        "a double rotation removes the <i>mean</i> tilt over an averaging "
        "period; neither can remove a tilt that changes <i>within</i> one. An "
        "inclinometer logged at the same rate as the wind can, sample by "
        "sample, and that is what this does."
        "<br><br><b>Where the angles come from.</b> Ordinary extra raw "
        "columns named <i>theta</i>, <i>phi</i> and <i>psi</i>, declared in "
        "the <b>Raw File Description</b> like any other channel. There is "
        "nothing to configure here about which column is which - the name is "
        "the whole of it, because there is only one sonic. A channel that is "
        "absent contributes a zero angle, which leaves that axis alone; if "
        "none of the three is found the correction is skipped and says so."
        "<br><br>The channels hold the inclinometer's <i>output voltage</i>, "
        "not an angle. The angle is -asin(V / sensitivity)."
        "<br><br><b>psi is always zero</b>, even when a psi column exists. "
        "EddyUH reads it and then overwrites it with zeros, commenting "
        "&quot;not measured&quot;; the rotation matrix and the swinging term "
        "are both built "
        "on that assumption. So this is a two-angle correction with three "
        "channels declared."
        "<br><br>Runs on the raw wind, before any axis rotation, which "
        "expects a series already in the sonic's true frame."));

    tiltSensorMethLabel = new ClickLabel(tr("Correct for :"));
    tiltSensorMethCombo = new QComboBox;
    tiltSensorMethCombo->addItem(tr("Position"));
    tiltSensorMethCombo->addItem(tr("Position and swinging"));
    tiltSensorMethCombo->setItemData(0, tr("<b>Position:</b> Rotates the "
        "measured wind vector by the inclination of the moment. This is the "
        "part of the correction that is unambiguously right, and the one to "
        "use unless you have a reason not to."), Qt::ToolTipRole);
    tiltSensorMethCombo->setItemData(1, tr("<b>Position and swinging:</b> "
        "Adds a term for the motion of the sonic head itself as the mast "
        "swings, built from the lever arm below and the time derivatives of "
        "the angles."
        "<br><br><b>That term is a single number, added to u, v and w "
        "alike.</b> EddyUH writes it as a dot product, where the velocity of "
        "a point on a rotating body is a cross product and would give three "
        "different components (EddyUH_tiltangle.m:104). The units survive - "
        "radians per second times metres is metres per second - which is why "
        "it is easy to miss. EddyFlow reproduces it as written, because this "
        "option exists to reproduce EddyUH's numbers and a silently corrected "
        "version would reproduce nothing."
        "<br><br>If you want the physical correction rather than EddyUH's, "
        "use <i>Position</i> and treat this mode as unavailable. Adjusting "
        "the lever arm will not help - no arm turns a scalar into a "
        "vector."), Qt::ToolTipRole);
    tiltSensorMethLabel->setToolTip(tiltSensorMethCombo->itemData(1, Qt::ToolTipRole).toString());

    tiltSensorVgLabel = new ClickLabel(tr("Sensitivity :"));
    tiltSensorVgSpin = new QDoubleSpinBox;
    tiltSensorVgSpin->setDecimals(4);
    tiltSensorVgSpin->setRange(0.0001, 1000.0);
    tiltSensorVgSpin->setSingleStep(0.1);
    tiltSensorVgSpin->setAccelerated(true);
    tiltSensorVgSpin->setSuffix(tr("  [V/g]"));
    tiltSensorVgSpin->setToolTip(tr("<b>Sensitivity:</b> Volts per g of the "
        "inclinometer, which is what turns the logged voltage into an angle: "
        "-asin(V / sensitivity). Four is the value EddyUH uses, as a literal "
        "rather than a setting; check your inclinometer's data sheet before "
        "trusting it."
        "<br><br>A reading past full scale is clamped to plus or minus a "
        "right angle rather than allowed to produce a value the arc sine "
        "cannot take, which would otherwise spread through every wind "
        "component of that sample."));
    tiltSensorVgLabel->setToolTip(tiltSensorVgSpin->toolTip());

    tiltLpfLabel = new ClickLabel(tr("Smoothing :"));
    tiltLpfSpin = new QDoubleSpinBox;
    tiltLpfSpin->setDecimals(2);
    tiltLpfSpin->setRange(0.0, 600.0);
    tiltLpfSpin->setSingleStep(0.5);
    tiltLpfSpin->setAccelerated(true);
    tiltLpfSpin->setSuffix(tr("  [s]"));
    tiltLpfSpin->setSpecialValueText(tr("no smoothing"));
    tiltLpfSpin->setToolTip(tr("<b>Smoothing:</b> A centred running mean over "
        "the angle series, in seconds, to keep the inclinometer's own noise "
        "out of the correction. Zero, the default, applies none."
        "<br><br>Smoothing the angle is not the same as smoothing the wind: "
        "it removes noise from what the mast is <i>believed</i> to be doing, "
        "not from what the sonic measured. Too long a window also removes the "
        "genuine sway this correction exists to catch, so keep it short "
        "against the swinging period."));
    tiltLpfLabel->setToolTip(tiltLpfSpin->toolTip());

    const auto armTip = tr("<b>Lever arm:</b> The vector from the point the "
        "mast pivots about to the sonic head, in metres, in the sonic's own "
        "axes. Used only by <i>Position and swinging</i>; <i>Position</i> "
        "ignores it entirely."
        "<br><br>-1.5 on each axis is EddyUH's own literal, which is a "
        "starting point and not a measurement of your mast. A wrong arm adds "
        "a velocity that is not there.");
    tiltArmLabel = new ClickLabel(tr("Lever arm :"));
    tiltArmLabel->setToolTip(armTip);
    tiltArmXLabel = new QLabel(tr("x"));
    tiltArmYLabel = new QLabel(tr("y"));
    tiltArmZLabel = new QLabel(tr("z"));
    tiltArmXSpin = new QDoubleSpinBox;
    tiltArmYSpin = new QDoubleSpinBox;
    tiltArmZSpin = new QDoubleSpinBox;
    for (auto* spin : {tiltArmXSpin, tiltArmYSpin, tiltArmZSpin})
    {
        spin->setDecimals(3);
        spin->setRange(-100.0, 100.0);
        spin->setSingleStep(0.1);
        spin->setAccelerated(true);
        spin->setSuffix(tr("  [m]"));
        spin->setToolTip(armTip);
    }

    rotCheckBox = new RichTextCheckBox;
    rotCheckBox->setToolTip(tr("<b>Axis rotation for tilt correction:</b> Select the appropriate method for compensating anemometer tilt with respect to local streamlines. Uncheck the box to <i>not perform</i> any rotation (not recommnended). If your site has a complex or sloping topography, a planar-fit method is advisable. Click on the <b><i>Planar Fit Settings...</i></b> to configure the procedure."));
    rotCheckBox->setText(tr("Axis rotations for tilt correction"));
    rotCheckBox->setQuestionMark(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Anemometer_Tilt_Correction.html"));

    rotMethLabel = new ClickLabel(tr("Rotation method :"));
    rotMethCombo = new QComboBox;
    rotMethCombo->addItem(tr("Double rotation"));
    rotMethCombo->addItem(tr("Triple rotation"));
    rotMethCombo->addItem(tr("Planar fit (Wilczak et al. 2001)"));
    rotMethCombo->addItem(tr("Planar fit with no velocity bias (van Dijk et al. 2004)"));
    rotMethCombo->setItemData(0, tr("<b>Double rotation:</b> Aligns the x-axis of the anemometer to the current mean streamlines, nullifying the vertical and cross-wind components. This is suggested in cases of flat topography and homogeneous canopies and in all conditions in which it is safe to assume that local wind streamlines are horizontal, parallel to the underlying surface."), Qt::ToolTipRole);
    rotMethCombo->setItemData(1, tr("<b>Triple rotation:</b> Double rotations plus a third rotation that nullifies the cross-stream stress. Not suitable in situations where the cross-stream stress is not expected to vanish, e.g., over water surfaces. Not recommended in general. Provided primarily for backwards compatibility."), Qt::ToolTipRole);
    rotMethCombo->setItemData(2, tr("<b>Planar fit:</b> Aligns the anemometer coordinate system to local streamlines assessed on a long time period (e.g., 2 weeks or more). Can be performed sector-wise, meaning that different rotation angles are calculated for different wind sectors. Suggested for complex topographies and canopy structures, if canopy heights don\'t change too quickly. Click on the <b><i>Planar Fit Settings...</i></b> to configure the procedure."), Qt::ToolTipRole);
    rotMethCombo->setItemData(3, tr("<b>Planar fit with no velocity bias:</b> Aligns the anemometer coordinate system to local streamlines assessed on a long time period (e.g., 2 weeks or more), but unlike the classic <i>Planar fit</i>, it assumes that any bias in the measurement of vertical wind is preliminarily compensated, and forces the fitting plane to pass through the origin (that is, such that if average <i>u</i> and <i>v</i> are zero, also average <i>w</i> is zero), thus its coefficient <i>b0</i> is zero. Can be performed sector-wise, meaning that different rotation angles are calculated for different wind sectors. It is recommended for complex topographies and canopy structures, if canopy heights don\'t change too quickly. Click on the <b><i>Planar Fit Settings...</i></b> to configure the procedure."), Qt::ToolTipRole);
    rotMethCombo->setEnabled(false);

    pfSettingsButton = new QPushButton(tr("Planar Fit Settings..."));
    pfSettingsButton->setProperty("mdButton", true);
    pfSettingsButton->setMaximumWidth(pfSettingsButton->sizeHint().width());

    detrendLabel = new QLabel(tr("Turbulent fluctuations"));
    detrendLabel->setMargin(11);
    detrendLabel->setToolTip(tr("<b>Turbulent fluctuations:</b> Select the method to extract turbulence fluctuations out of the time series."));
    detrendMethLabel = new ClickLabel(tr("Detrend method :"));
    detrendCombo = new QComboBox;
    detrendCombo->addItem(tr("Block average"));
    detrendCombo->addItem(tr("Linear detrending"));
    detrendCombo->addItem(tr("Running mean"));
    detrendCombo->addItem(tr("Exponential running mean"));
    detrendCombo->setItemData(0, tr("<b>Block average:</b> Simply removes the mean value from the time series, calculated over the flux averaging interval. Obeys Reynolds decomposition rule (the mean value of fluctuations is identically zero). Among all methods available, block average retains the largest portion of low frequency content, including genuine turbulent fluctuations and possible non-turbulence related trends."), Qt::ToolTipRole);
    detrendCombo->setItemData(1, tr("<b>Linear detrending:</b> Calculates fluctuations as the deviations from a linear trend. The linear trend can be evaluated on a time basis different from the flux averaging interval. Specify this time basis using the <b><i>Time constant</i></b> entry. For classic linear detrending, with the trend evaluated on the whole flux averaging interval, set <b><i>Time constant = 0</i></b>, which will be automatically converted into the text <i>Same as Flux averaging interval</i>."), Qt::ToolTipRole);
    detrendCombo->setItemData(2, tr("<b>Running mean:</b> High-pass, finite impulse response filter. The current mean is determined by the previous <i>N</i> data points, where <i>N</i> depends on the <i>time constant</i>. The smaller the time constant, the more low-frequency content is eliminated from the time series."), Qt::ToolTipRole);
    detrendCombo->setItemData(3, tr("<b>Exponential running mean:</b> High-pass, infinite impulse response filter. Similar to the simple running mean, but weighted in such a way that distant samples have an exponentially decreasing weight in the current mean, never reaching zero. The smaller the time constant, the more low-frequency content is eliminated from the time series."), Qt::ToolTipRole);

    timeConstantLabel = new ClickLabel(tr("Time constant :"));
    timeConstantLabel->setEnabled(false);
    timeConstantLabel->setToolTip(tr("<b>Time constant:</b> Applies to the linear detrending, running mean and exponential running mean methods. In general, the higher the time constant, the more low-frequency content is retained in the turbulent fluctuations. Note that for the linear detrending the unit is minutes, while for the running means it is seconds."));
    timeConstantSpin = new QDoubleSpinBox(this);
    timeConstantSpin->setToolTip(timeConstantLabel->toolTip());
    timeConstantSpin->setRange(0.0, 5000.0);
    timeConstantSpin->setValue(0.0);
    timeConstantSpin->setDecimals(1);
    timeConstantSpin->setAccelerated(true);
    timeConstantSpin->setSingleStep(10.0);
    timeConstantSpin->setSuffix(tr("  [s]", "Second"));
    timeConstantSpin->setEnabled(false);
    timeConstantSpin->setSpecialValueText(tr("Same as Flux averaging interval"));
    timeConstantSpin->setMaximumWidth(timeConstantSpin->sizeHint().width());

    timeLagCheckBox = new RichTextCheckBox;
    timeLagCheckBox->setToolTip(tr("<b>Time lags compensation:</b> Select the method to compensate time lags between anemometric measurements and any other high frequency measurements included in the raw files. Time lags arise due mainly to sensors physical distances and to the passage of air into sampling lines. Uncheck this box to instruct EddyFlow not to compensate time lags (not recommended)."));
    timeLagCheckBox->setText(tr("Time lags compensation"));
    timeLagCheckBox->setQuestionMark(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Time_Lag_Detect_Correct.html"));

    timeLagMethodLabel = new ClickLabel(tr("Time lag detection method :"));
    timeLagMethodCombo = new QComboBox;
    //> Set before the items, because it replaces the model the combo builds
    //> its own. It is what makes a "disabled" marker on an item actually
    //> unselectable, which SmartFlux mode needs for the pre-whitening entry.
    timeLagMethodCombo->setModel(new CustomComboModel(timeLagMethodCombo));
    timeLagMethodCombo->addItem(tr("Constant"));
    timeLagMethodCombo->addItem(tr("Covariance maximization with default"));
    timeLagMethodCombo->addItem(tr("Covariance maximization"));
    timeLagMethodCombo->addItem(tr("Automatic time lag optimization"));
    timeLagMethodCombo->addItem(tr("Pre-whitening block-bootstrap (Vitale et al. 2024)"));
    timeLagMethodCombo->setItemData(0, tr("<b>Constant:</b> EddyFlow will apply constant time lags for all flux averaging intervals, using the <b><i>Nominal time lag</i></b> stored inside the GHG files or in the <b><i>Alternative metadata file</i></b> (for files other than GHG). While it can speed up the computation, this method is not recommended for physically displaced sensors or closed/enclosed path gas analysers. It can be used for closed/enclosed analysers if flow rate in the sampling line is strictly controlled and the sampling tube is actively heated to keep relative humidity low and constant."), Qt::ToolTipRole);
    timeLagMethodCombo->setItemData(1, tr("<b>Covariance maximization with default:</b> Similar to the <i>Covariance maximization</i>, this calculates the most likely time lag based on the circular correlation procedure. However, if a maximum of the covariance is not attained within the window (but at one of its ends), the time lag is set to the <b><i>Nominal time lag</i></b> value stored inside the GHG files or in the <b><i>Alternative metadata file</i></b> (for files other than GHG), for each variable. Recommended in most situations."), Qt::ToolTipRole);
    timeLagMethodCombo->setItemData(2, tr("<b>Covariance maximization:</b> Calculates the most likely time lag within a plausible window, based on the circular correlation procedure. The window is defined by the <i>Minimum time lags</i> and <i>Maximum time lags</i> stored inside the GHG files or in the <i>Alternative metadata file</i> (for files other than GHG), for each variable."), Qt::ToolTipRole);
    timeLagMethodCombo->setItemData(3, tr("<b>Automatic time lag optimization:</b> Select this option and configure it clicking on the <b><i>Time Lag Optimization Settings...</i></b> to instruct EddyFlow to perform a statistical optimization of time lags. It will calculate nominal time lags and plausibility windows and apply them in the raw data processing step. For water vapor, the assessment is performed as a function of relative humidity."), Qt::ToolTipRole);
    timeLagMethodCombo->setItemData(4, tr("<b>Pre-whitening block-bootstrap:</b> Detects one time lag per averaging period using the Vitale et al. (2024) pre-whitening and block-bootstrap procedure. It is intended for weak trace-gas signals and variable inlet delays, and reports bootstrap uncertainty in a diagnostics file."), Qt::ToolTipRole);
    timeLagMethodCombo->setEnabled(false);

    tlSettingsButton = new QPushButton(tr("Time Lag Optimization Settings..."));
    tlSettingsButton->setProperty("mdButton", true);
    tlSettingsButton->setMaximumWidth(tlSettingsButton->sizeHint().width());

    qcCheckBox = new RichTextCheckBox;
    qcCheckBox->setToolTip(tr("<b>Quality check:</b> Select the quality flagging policy. Flux quality flags are obtained from the combination of two partial flags that result from the application of the steady-state and the developed turbulence tests. Select the flag combination policy."));
    qcCheckBox->setText(tr("Quality check"));
    qcCheckBox->setQuestionMark(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Flux_Quality_Flags.html"));

    qcLabel = new ClickLabel(tr("Flagging policy :"));
    qcMethodCombo = new QComboBox;
    qcMethodCombo->setToolTip(tr("<b>Mauder and Foken 2004:</b> Policy described in the documentation of the TK2 Eddy Covariance software that also constituted the standard of the CarboEurope IP project and is widely adopted. \"0\" means high quality fluxes, \"1\" means fluxes are ok for budget analysis, \"2\" fluxes should be discarded from the result dataset."));
    qcMethodCombo->addItem(tr("Mauder and Foken (2004) (0-1-2 system)"));
    qcMethodCombo->addItem(tr("Foken (2003) (1 to 9 system)"));
    qcMethodCombo->addItem(tr("Goeckede et al. (2004) (1 to 5 system)"));
    qcMethodCombo->addItem(tr("Vitale et al. (2020) (0-1-2 severity system)"));
    qcMethodCombo->setItemData(0, tr("<b>Mauder and Foken 2004:</b> Policy described in the documentation of the TK2 Eddy Covariance software that also constituted the standard of the CarboEurope IP project and is widely adopted. \"0\" means high quality fluxes, \"1\" means fluxes are ok for budget analysis, \"2\" fluxes should be discarded from the result dataset."), Qt::ToolTipRole);
    qcMethodCombo->setItemData(1, tr("<b>Foken 2003:</b> A system based on 9 quality grades. \"1\" is best, \"9\" is worst. The system of Mauder and Foken (2004) and of Goeckede et al. (2006) are based on a rearrangement of these system."), Qt::ToolTipRole);
    qcMethodCombo->setItemData(2, tr("<b>Goeckede et al., 2004:</b> A system based on 5 quality grades. \"1\" is best, \"5\" is worst."), Qt::ToolTipRole);
    qcMethodCombo->setItemData(3, tr("<b>Vitale et al. (2020), Biogeosciences:</b> \"0\" (ok), \"1\" (moderate) or \"2\" (severe), from a wider net of tests than the three systems above: the steadiness and developed-turbulence tests they share, plus Mahrt's (1998) nonstationarity ratio and the ITC deviation - both already computed regardless of which system is selected - and, when <i>Extra raw-signal diagnostics (RFlux)</i> is also enabled, its AL1/DDI/HF/HD/DIP tests and the always-on KID test. \"2\" (severe) wins over \"1\" (moderate) wherever a period trips both. Adapted from RFlux's cleanFlux(): its own low-signal-resolution and physical-range tests have no EddyFlow equivalent yet and are not part of this grade, and its wind-sector exclusion is folded in only once EddyFlow supports time-varying sectors."), Qt::ToolTipRole);

    fpCheckBox = new RichTextCheckBox;
    fpCheckBox->setToolTip(tr("<b>Footprint estimation:</b> Select whether to calculate flux footprint estimations and which method should be used. Flux crosswind-integrated footprints are provided as distances from the tower contributing for 10%, 30%, 50%, 70% and 90% to measured fluxes. Also, the location of the peak contribution is given."));
    fpCheckBox->setText(tr("Footprint estimation"));
    fpCheckBox->setQuestionMark(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Estimating_Flux_Footprint.html"));

    fpLabel = new ClickLabel(tr("Footprint method :"));
    fpMethodCombo = new QComboBox;
    fpMethodCombo->setToolTip(tr("<b>Kljun et al. (2004):</b> A cross-wind integrated parameterization of footprint estimations obtained with a 3D Lagrangian model by means of a scaling procedure."));
    fpMethodCombo->addItem(tr("Kljun et al. (2004)"));
    fpMethodCombo->addItem(tr("Kormann and Meixner (2001)"));
    fpMethodCombo->addItem(tr("Hsieh et al. (2000)"));
    fpMethodCombo->setItemData(0, tr("<b>Kljun et al. (2004):</b> A cross-wind integrated parameterization of footprint estimations obtained with a 3D Lagrangian model by means of a scaling procedure."), Qt::ToolTipRole);
    fpMethodCombo->setItemData(1, tr("<b>Kormann and Meixner (2001):</b> A cross-wind integrated model based on the solution of the two dimensional advection-diffusion equation given by van Ulden (1978) and others for power-law profiles in wind velocity and eddy diffusivity."), Qt::ToolTipRole);
    fpMethodCombo->setItemData(2, tr("<b>Hsien et al. (2000):</b> A cross-wind integrated model based based on the former model of Gash (1986) and on simulations with a Lagrangian stochastic model."), Qt::ToolTipRole);

    cecCheckBox = new RichTextCheckBox;
    cecAvailableTooltip_ = tr("<b>Conditional Eddy Covariance:</b> Partitions fluxes into stomatal and non-stomatal components using the octant-based conditional statistics method of Zahn et al. (2022). Each CO\xe2\x82\x82/H\xe2\x82\x82O pairing adds evaporation and transpiration (<i>E_cec</i>, <i>Tr_cec</i>), ecosystem respiration and net photosynthesis (<i>Reco_cec</i>, <i>P_cec</i>), the ratios and quality flags behind them, and the octant counts they were computed from. Any further species the pairing carries \xe2\x80\x93 carbonyl sulfide, for instance \xe2\x80\x93 is partitioned in the same octants. Use <i>CEC Settings</i> to choose which channels pair with which."
                              "<p>Ticking this also switches on <b>compensation of density fluctuations (WPL)</b>, because the method sorts each air parcel by the SIGN of its water and carbon dioxide fluctuation. In an uncorrected molar density from an open-path analyser, part of that fluctuation is the air expanding rather than the gas arriving, and it is large enough to reverse the sign \xe2\x80\x93 which puts the parcel in the wrong octant.</p>");
    cecCheckBox->setToolTip(cecAvailableTooltip_);
    cecCheckBox->setText(tr("Conditional Eddy Covariance"));
    cecCheckBox->setQuestionMark(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Conditional_Eddy_Covariance.html"));

    cecSettingsButton = new QPushButton(tr("CEC Settings"));
    cecSettingsButton->setProperty("mdButton", true);
    cecSettingsButton->setMaximumWidth(cecSettingsButton->sizeHint().width());
    cecSettingsButton->setEnabled(false);
    cecSettingsButton->setToolTip(tr("<b>CEC Settings:</b> Configure Conditional Eddy Covariance partitioning constraints and QC/preprocessing limits."));

    wplCheckBox = new RichTextCheckBox;
    wplCheckBox->setToolTip(tr("<b>Compensate density fluctuations:</b> This is the so-called WPL correction (Webb et al., 1980). Choose whether to apply the compensation of density fluctuations to raw gas concentrations available as molar densities or mole fractions (moles gas per mole of wet air). The correction does not apply if raw concentrations are available as mixing ratios (mole gas per mole dry air)."));
    wplCheckBox->setText(tr("Compensate density fluctuations (WPL terms)"));
    wplCheckBox->setQuestionMark(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Converting_to_Mixing_Ratio.html"));

    covmaxDebaselineCheckBox = new RichTextCheckBox;
    covmaxDebaselineCheckBox->setText(tr("Subtract the cross-covariance baseline"));
    covmaxDebaselineCheckBox->setToolTip(tr("<b>Subtract the cross-covariance "
        "baseline:</b> Choose the time lag by the largest departure of the "
        "cross-covariance from the straight line joining the two ends of the search "
        "window, rather than by its largest absolute value. A weak flux often sits on a "
        "sloping cross-covariance - from a trend, or from a neighbouring stronger "
        "correlation - and the plain maximum then lands on whichever end the slope is "
        "highest at instead of on the peak."
        "<br><br>This changes <b>which lag is selected</b>, and nothing about the "
        "covariance reported there: the flux at the chosen lag is computed exactly as "
        "before. Off by default."
        "<br><br>Note that with the baseline removed the two ends of the window score "
        "zero by construction, so the maximum can never land on an end - which means "
        "<i>Covariance maximization with default</i> stops falling back to the nominal "
        "lag. For a weak flux that safety net is worth replacing rather than simply "
        "losing."));

    tlagBorrowCheckBox = new RichTextCheckBox;
    tlagBorrowCheckBox->setText(tr("Borrow a tube-mate's lag below the detection limit"));
    tlagBorrowCheckBox->setToolTip(tr("<b>Borrow a tube-mate's lag below the "
        "detection limit:</b> Gases drawn down one tube share a transport delay. A "
        "species whose cross-covariance peak cannot be told from noise has nothing of "
        "its own to detect, so it takes the lag of a gas on the same analyser that "
        "resolved its peak - the best-resolved one, not the first in the list "
        "(Nemitz et al., 2018)."
        "<br><br>It also fires when the maximum lands on an end of the search window, "
        "where a maximisation goes when there is no interior peak to find."
        "<br><br><b>Requires the flux detection limit</b>, under Statistical Analysis: "
        "without it there is nothing to compare a covariance against, and this control "
        "stays greyed. Water is never borrowed for or from - its lag is the one every "
        "other gas's water covariance is taken at. A borrowed lag is flagged in "
        "<i>&lt;gas&gt;_def_timelag</i> and the donor is named in the run log. "
        "Off by default."));

    tlagBorrowSnrLabel = new ClickLabel(tr("Detection limits to clear :"));
    tlagBorrowSnrLabel->setToolTip(tr("<b>Detection limits to clear:</b> How far above "
        "its detection limit a gas's covariance must stand to keep its own time lag. "
        "Three is the value Nemitz et al. use. Lower means fewer gases borrow; higher "
        "means more."));
    tlagBorrowSnrSpin = new QDoubleSpinBox;
    tlagBorrowSnrSpin->setDecimals(1);
    tlagBorrowSnrSpin->setRange(0.1, 100.0);
    tlagBorrowSnrSpin->setSingleStep(0.5);
    tlagBorrowSnrSpin->setAccelerated(true);
    tlagBorrowSnrSpin->setToolTip(tlagBorrowSnrLabel->toolTip());

    //> Which noise floor, and which donor. Both default to this program's
    //> own choice; the second entry on each is what EddyUH does, and both
    //> say so, because "EddyUH's" is the reason anyone would pick it.
    tlagBorrowNoiseLabel = new ClickLabel(tr("Judged against :"));
    tlagBorrowNoiseCombo = new QComboBox;
    tlagBorrowNoiseCombo->addItem(tr("The flux detection limit"));
    tlagBorrowNoiseCombo->addItem(tr("Lenschow instrument noise (EddyUH)"));
    tlagBorrowNoiseCombo->setItemData(0, tr("<b>The flux detection limit:</b> "
        "The scatter of the cross-covariance far from its peak, where there "
        "is no flux - so it measures what the covariance itself does with "
        "nothing in it. Requires the detection limit to be switched on, under "
        "<i>Statistical Analysis</i>."), Qt::ToolTipRole);
    tlagBorrowNoiseCombo->setItemData(1, tr("<b>Lenschow instrument noise:</b> "
        "The step in the autocovariance at zero lag, which is the analyser's "
        "own white noise. This is what EddyUH actually divides by "
        "(EddyUH_SC_Flux2.m:325), although its own comment there calls it a "
        "detection limit."
        "<br><br>Measured from the series in hand, so it needs nothing else "
        "switched on - and being a smaller floor than the detection limit, it "
        "lets more gases keep their own lag."), Qt::ToolTipRole);
    tlagBorrowNoiseLabel->setToolTip(tlagBorrowNoiseCombo->itemData(0, Qt::ToolTipRole).toString());

    tlagBorrowDonorLabel = new ClickLabel(tr("Borrow from :"));
    tlagBorrowDonorCombo = new QComboBox;
    tlagBorrowDonorCombo->addItem(tr("The best-resolved gas on the analyser"));
    tlagBorrowDonorCombo->addItem(tr("The analyser's carbon dioxide (EddyUH)"));
    tlagBorrowDonorCombo->setItemData(0, tr("<b>The best-resolved gas on the "
        "analyser:</b> Ranks the eligible tube-mates by how far each stands "
        "above the noise and takes the strongest. Taking the first in metadata "
        "order instead produces pairings that read backwards - carbon dioxide, "
        "the strongest flux on the analyser, taking its lag from nitrous "
        "oxide."), Qt::ToolTipRole);
    tlagBorrowDonorCombo->setItemData(1, tr("<b>The analyser's carbon "
        "dioxide:</b> What EddyUH does, hard-coded by variable name with no "
        "user switch. Usually the best-resolved channel on a trace-gas "
        "analyser anyway, with the merit of being the same donor in every "
        "period - a lag population that does not change donor halfway through "
        "the day is easier to defend."
        "<br><br>Carbon dioxide can then never borrow, since it would be "
        "borrowing from itself. If the analyser measures no carbon dioxide, or "
        "its carbon dioxide did not clear the threshold either, nothing is "
        "borrowed - another instrument's gas shares no tube with this one and "
        "is not a substitute."), Qt::ToolTipRole);
    tlagBorrowDonorLabel->setToolTip(tlagBorrowDonorCombo->itemData(1, Qt::ToolTipRole).toString());

    spectroCheckBox = new RichTextCheckBox;
    spectroCheckBox->setText(tr("Remove the spectroscopic effect of water vapour"));
    spectroCheckBox->setToolTip(tr("<b>Remove the spectroscopic effect of water vapour:</b> "
        "Water vapour broadens the absorption lines a laser analyser measures, so the mixing "
        "ratio it reports depends on humidity beyond simple dilution. Each affected column is "
        "divided, sample by sample, by 1 + <i>a</i>&#183;&#967;<sub>q</sub> + "
        "<i>b</i>&#183;&#967;<sub>q</sub>&#178;, using the water its own analyser read at the "
        "same instant (Peltola et al., 2014; applied point by point after Chen et al., 2010). "
        "<br><br>Enter <i>a</i> and <i>b</i> per column in the <b>Raw File Description</b>; "
        "a column that leaves them at zero is not touched, and neither is an open-path "
        "analyser. This is independent of the density compensation above - the bias is in "
        "what the instrument reported, whether or not WPL is applied. Off by default."
        "<br><br><b>Coefficients here are spectroscopic only.</b> EddyUH and the Rella "
        "(2010) tables it ships use a convention in which the dilution is folded into the "
        "same polynomial, so that <i>a</i> = &minus;1, <i>b</i> = 0 means pure dilution and "
        "no spectroscopy. EddyFlow corrects the density separately, so the identity here is "
        "<i>a</i> = <i>b</i> = 0. To carry a published value across, add one to <i>a</i>: an "
        "EddyUH <i>a</i> of &minus;1.39 becomes &minus;0.39 here. Typing the EddyUH value "
        "unchanged would count the dilution twice."));

    spectroWaterCheckBox = new RichTextCheckBox;
    spectroWaterCheckBox->setText(tr("Also correct the water channel (EddyUH form, unpublished)"));
    spectroWaterCheckBox->setToolTip(tr("<b>Also correct the water channel:</b> Applies the same "
        "division to each hygrometer against <i>its own</i> reading, which is self-broadening. "
        "<br><br><b>This is not part of the published Peltola et al. (2014) result</b>, which "
        "derives the effect of water on <i>another</i> gas's absorption lines. EddyUH corrects "
        "the water channel too, using a coefficient whose derivation its own source comment "
        "states is not published anywhere; this offers the same idea in the point-by-point form "
        "used for every other column. Select it deliberately, and report that you did. "
        "Off by default, and it does nothing unless the correction above is on and the "
        "hygrometer's own coefficients are non-zero."));

    //> Lit only in the inconsistent state - the partition on, the correction it
    //> depends on off. A warning icon that is usually on is one nobody reads.
    wplWarningLabel = new QLabel;
    wplWarningLabel->setPixmap(QApplication::style()
                               ->standardIcon(QStyle::SP_MessageBoxWarning)
                               .pixmap(16, 16));
    wplWarningLabel->setToolTip(tr("<b>Conditional Eddy Covariance is on and this is off.</b> "
                                   "The partition sorts each air parcel by the SIGN of its water "
                                   "and carbon dioxide fluctuation, and without this correction an "
                                   "open-path molar density carries an expansion term big enough to "
                                   "reverse that sign. Zahn et al. (2022) require the fluctuations "
                                   "themselves to be density-corrected, separately from the totals. "
                                   "The run will say so as well."));
    wplWarningLabel->hide();

    // burba correction
    burbaCorrCheckBox = new RichTextCheckBox;
    burbaAvailableTooltip_ = tr("<b>Add instrument sensible heat components, only for LI-7500:</b> Only applies to the LI-7500. It takes into account air density fluctuations due to temperature fluctuations induced by heat exchange processes at the instrument surfaces, as from Burba et al. (2008).");
    burbaCorrCheckBox->setToolTip(burbaAvailableTooltip_);
    burbaCorrCheckBox->setText(tr("Add instrument sensible heat components, only for LI-7500 "));
    burbaCorrCheckBox->setQuestionMark(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Calculating_Off-season_Uptake_Correction.html"));

    burbaTypeLabel = new ClickLabel;
    burbaTypeLabel->setText(tr("Surface temperature estimation :"));
    burbaSimpleRadio = new QRadioButton(tr("Simple linear regressions"), this);
    burbaSimpleRadio->setToolTip(tr("<b>Simple linear regressions:</b> Instrument surface temperatures are estimated based on air temperature, using linear regressions as from Burba et al. 2008, eqs. 3-8. Default regression parameters are from Table 3 in the same paper. If you have experimental data for your LI-7500 unit, you may customize those values. Otherwise we suggest using default values."));
    burbaMultiRadio = new QRadioButton(tr("Multiple regressions"), this);
    burbaMultiRadio->setToolTip(tr("<b>Multiple regressions:</b> Instrument surface temperatures are estimated based on air temperature, global radiation, long-wave radiation and wind speed, as from Burba et al. 2008, Table 2. Default regression parameters are from the same table. If you have experimental data for your LI-7500 unit, you may customize those values. Otherwise we suggest using default values."));

    burbaRadioGroup = new QButtonGroup(this);
    burbaRadioGroup->addButton(burbaSimpleRadio, 0);
    burbaRadioGroup->addButton(burbaMultiRadio, 1);

    setDefaultsButton = new QPushButton(tr("Restore Default Values"));
    setDefaultsButton->setProperty("mdButton", true);
    setDefaultsButton->setMaximumWidth(setDefaultsButton->sizeHint().width());
    setDefaultsButton->setToolTip(tr("<b>Restore Default Values</b>: Resets the surface heating correction to the default values of Burba et al. (2008)."));

    auto defaultLabel = new QLabel(tr("Default values as from Burba et al. (2008)"));
    defaultLabel->setObjectName(QStringLiteral("citeLabel"));

    auto defaultContainerLayout = new QHBoxLayout;
    defaultContainerLayout->addWidget(setDefaultsButton);
    defaultContainerLayout->addWidget(defaultLabel);
    defaultContainerLayout->addStretch();

    auto defaultContainer = new QWidget;
    defaultContainer->setLayout(defaultContainerLayout);

    burbaSimpleDay = new QWidget;
    burbaSimpleNight = new QWidget;
    burbaMultiDay = new QWidget;
    burbaMultiNight = new QWidget;

    createBurbaParamItems();

    burbaSimpleTab = new QTabWidget;
    burbaSimpleTab->addTab(burbaSimpleDay, tr("Day time"));
    burbaSimpleTab->addTab(burbaSimpleNight, tr("Night time"));

    burbaMultiTab = new QTabWidget;
    burbaMultiTab->addTab(burbaMultiDay, tr("Day time"));
    burbaMultiTab->addTab(burbaMultiNight, tr("Night time"));

    burbaParamWidget = new QStackedWidget;
    burbaParamWidget->addWidget(burbaSimpleTab);
    burbaParamWidget->addWidget(burbaMultiTab);
    burbaParamWidget->setCurrentIndex(0);

    auto wplTitle = new QLabel(tr("Compensation of density fluctuations"));
    wplTitle->setProperty("groupLabel", true);

    auto qcTitle = new QLabel(tr("Other options"));
    qcTitle->setProperty("groupLabel", true);

    parallelPrepassCheckBox = new RichTextCheckBox;
    parallelPrepassCheckBox->setText(tr("Parallelise the planar fit and time lag pre-passes"));
    parallelPrepassCheckBox->setToolTip(tr("<b>Parallelise the planar fit and "
        "time lag pre-passes:</b> Before it computes any flux, EddyFlow may walk "
        "every averaging period once to fit the planar fit planes, or to optimise "
        "the time lags. On a long dataset that walk dominates the run. Each period "
        "in it is independent of the others, so the range is split across the "
        "processor cores and the pieces joined back together in order."
        "<br><br>This does <b>not change the results</b>: the planar fit "
        "coefficients and the optimised time lags come out identical to a run "
        "without it. It has no effect on a project that runs no pre-pass, nor on "
        "the flux computation itself, which is not split."
        "<br><br>This is a setting for <i>this computer</i>, not for the project: "
        "it is remembered between sessions but is not saved into the project file, "
        "so a colleague opening the same project decides it for themselves."));

    auto hrLabel = new QLabel;
    hrLabel->setObjectName(QStringLiteral("hrLabel"));
    auto hrLabel_2 = new QLabel;
    hrLabel_2->setObjectName(QStringLiteral("hrLabel"));
    auto qBox_1 = new QHBoxLayout;
    qBox_1->addWidget(windOffsetLabel);
    qBox_1->addWidget(questionMark_1);
    qBox_1->addStretch();

    auto qBox_2 = new QHBoxLayout;
    qBox_2->addWidget(detrendLabel);
    qBox_2->addWidget(questionMark_4);
    qBox_2->addStretch();

//
    auto tiltArmLayout = new QHBoxLayout;
    tiltArmLayout->addWidget(tiltArmXLabel, 0, Qt::AlignRight);
    tiltArmLayout->addWidget(tiltArmXSpin, 1);
    tiltArmLayout->addStretch(1);
    tiltArmLayout->addWidget(tiltArmYLabel, 0, Qt::AlignRight);
    tiltArmLayout->addWidget(tiltArmYSpin, 1);
    tiltArmLayout->addStretch(1);
    tiltArmLayout->addWidget(tiltArmZLabel, 0, Qt::AlignRight);
    tiltArmLayout->addWidget(tiltArmZSpin, 1);
    tiltArmLayout->addStretch(1);

    auto settingsLayout = new QGridLayout;
    settingsLayout->addWidget(rawProcessingTitle, 0, 0);
    settingsLayout->addLayout(qBox_1, 1, 0, 1, 2);
    settingsLayout->addLayout(windComponentLayout, 1, 2, 1, 1);
    settingsLayout->addWidget(wBoostCheckBox, 2, 0);
    settingsLayout->addWidget(aoaCheckBox, 3, 0);
    settingsLayout->addWidget(aoaMethLabel, 3, 1, Qt::AlignRight);
    settingsLayout->addWidget(aoaMethCombo, 3, 2);
    settingsLayout->addWidget(headCorrCheckBox, 4, 0);
    settingsLayout->addWidget(headCorrMethLabel, 4, 1, Qt::AlignRight);
    settingsLayout->addWidget(headCorrMethCombo, 4, 2);
    settingsLayout->addWidget(headCorrDirLabel, 5, 1, Qt::AlignRight);
    settingsLayout->addWidget(headCorrDirBrowse, 5, 2, 1, 2);
    settingsLayout->addWidget(tiltSensorCheckBox, 6, 0);
    settingsLayout->addWidget(tiltSensorMethLabel, 6, 1, Qt::AlignRight);
    settingsLayout->addWidget(tiltSensorMethCombo, 6, 2);
    settingsLayout->addWidget(tiltSensorVgLabel, 7, 1, Qt::AlignRight);
    settingsLayout->addWidget(tiltSensorVgSpin, 7, 2);
    settingsLayout->addWidget(tiltLpfLabel, 8, 1, Qt::AlignRight);
    settingsLayout->addWidget(tiltLpfSpin, 8, 2);
    settingsLayout->addWidget(tiltArmLabel, 9, 1, Qt::AlignRight);
    settingsLayout->addLayout(tiltArmLayout, 9, 2, 1, 2);
    settingsLayout->addWidget(rotCheckBox, 10, 0);
    settingsLayout->addWidget(rotMethLabel, 10, 1, Qt::AlignRight);
    settingsLayout->addWidget(rotMethCombo, 10, 2);
    settingsLayout->addWidget(pfSettingsButton, 10, 3);
    settingsLayout->addLayout(qBox_2, 11, 0);
    settingsLayout->addWidget(detrendMethLabel, 11, 1, Qt::AlignRight);
    settingsLayout->addWidget(detrendCombo, 11, 2);
    settingsLayout->addWidget(timeConstantLabel, 12, 1, Qt::AlignRight);
    settingsLayout->addWidget(timeConstantSpin, 12, 2);
    settingsLayout->addWidget(timeLagCheckBox, 13, 0);
    settingsLayout->addWidget(timeLagMethodLabel, 13, 1, Qt::AlignRight);
    settingsLayout->addWidget(timeLagMethodCombo, 13, 2);
    settingsLayout->addWidget(tlSettingsButton, 13, 3);
    //> Indented under the method it modifies, in the combo's own column.
    settingsLayout->addWidget(covmaxDebaselineCheckBox, 14, 2, 1, 2);
    settingsLayout->addWidget(tlagBorrowCheckBox, 15, 2, 1, 2);
    settingsLayout->addWidget(tlagBorrowSnrLabel, 16, 1, Qt::AlignRight);
    settingsLayout->addWidget(tlagBorrowSnrSpin, 16, 2);
    settingsLayout->addWidget(tlagBorrowNoiseLabel, 17, 1, Qt::AlignRight);
    settingsLayout->addWidget(tlagBorrowNoiseCombo, 17, 2, 1, 2);
    settingsLayout->addWidget(tlagBorrowDonorLabel, 18, 1, Qt::AlignRight);
    settingsLayout->addWidget(tlagBorrowDonorCombo, 18, 2, 1, 2);
    settingsLayout->addWidget(hrLabel, 19, 0, 1, 4);
    settingsLayout->addWidget(wplTitle, 20, 0);
    //> One cell, not two: column 0 is as wide as its widest widget, so the
    //> icon in a neighbouring cell would sit far off to the right of the text
    //> it belongs to. Same shape as qBox_2 above.
    auto wplBox = new QHBoxLayout;
    wplBox->addWidget(wplCheckBox);
    wplBox->addWidget(wplWarningLabel);
    wplBox->addStretch();
    settingsLayout->addLayout(wplBox, 21, 0);
    //> Beside WPL because both are about what the analyser really saw, but
    //> deliberately not gated on it: WPL is a density correction and this is
    //> an optical one, and the bias is there whether or not densities are
    //> being compensated.
    settingsLayout->addWidget(spectroCheckBox, 22, 0);
    settingsLayout->addWidget(spectroWaterCheckBox, 23, 0);
    settingsLayout->addWidget(burbaCorrCheckBox, 24, 0);
    settingsLayout->addWidget(burbaTypeLabel, 25, 0, 1, 1, Qt::AlignRight);
    settingsLayout->addWidget(burbaSimpleRadio, 25, 1);
    settingsLayout->addWidget(burbaMultiRadio, 26, 1);
    settingsLayout->addWidget(burbaParamWidget, 27, 0, 1, 4);
    settingsLayout->addWidget(defaultContainer, 28, 0, 1, 4);
    //> Between the corrections block and the quality-control one. It sat at
    //> row 16, with slack rows between it and the quality-control block
    //> below; the controls inserted above have taken that slack up, and a
    //> rule drawn across an occupied row overlays the widget there.
    settingsLayout->addWidget(hrLabel_2, 29, 0, 1, 4);
    settingsLayout->addWidget(qcTitle, 30, 0);
    settingsLayout->addWidget(qcCheckBox, 31, 0);
    settingsLayout->addWidget(qcLabel, 31, 1, Qt::AlignRight);
    settingsLayout->addWidget(qcMethodCombo, 31, 2);
    settingsLayout->addWidget(fpCheckBox, 32, 0);
    settingsLayout->addWidget(fpLabel, 32, 1, Qt::AlignRight);
    settingsLayout->addWidget(fpMethodCombo, 32, 2);
    //> On the checkbox's own row, beside its settings button, the way every
    //> other option carrying one sits. It used to be added at row 27, which the
    //> Burba parameter stack already spans: two widgets in one cell overlap,
    //> and the one added later is painted over what is underneath it - here,
    //> the tab bar, which is why the day/night tabs could not be clicked.
    settingsLayout->addWidget(cecCheckBox, 33, 0);
    settingsLayout->addWidget(cecSettingsButton, 33, 3);
    //> Last in the group: it is about how the run is computed rather than
    //> about what is computed, so it sits below the options that change the
    //> numbers rather than among them.
    settingsLayout->addWidget(parallelPrepassCheckBox, 34, 0, 1, 4);
    //> On an empty trailing row: row 27 carries the Burba stack, and giving the
    //> stretch to an occupied row lets that block absorb the slack instead.
    settingsLayout->setRowStretch(35, 1);
    settingsLayout->setColumnStretch(4, 1);

//    auto overallFrame = new QWidget;
//    overallFrame->setProperty("scrollContainerWidget", true);
//    overallFrame->setLayout(settingsLayout);

//    auto scrollArea = new QScrollArea;
//    scrollArea->setWidget(overallFrame);
//    scrollArea->setWidgetResizable(true);

//    auto settingsGroupLayout = new QHBoxLayout;
//    settingsGroupLayout->addWidget(scrollArea);

    auto settingsGroupLayout = new QHBoxLayout;
    settingsGroupLayout->addWidget(WidgetUtils::getContainerScrollArea(this, settingsLayout));

    auto settingsGroupTitle = new QLabel(tr("Raw Processing Options"));
    settingsGroupTitle->setProperty("groupTitle2", true);

    auto qBox_11 = new QHBoxLayout;
    qBox_11->addWidget(settingsGroupTitle, 0, Qt::AlignRight | Qt::AlignBottom);
    qBox_11->addWidget(questionMark_11);
    qBox_11->addStretch();

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(qBox_11);
    mainLayout->addLayout(settingsGroupLayout);
    mainLayout->setContentsMargins(15, 15, 15, 10);
    setLayout(mainLayout);

    connect(uLabel, &ClickLabel::clicked,
            this, &AdvProcessingOptions::onULabelClicked);
    connect(vLabel, &ClickLabel::clicked,
            this, &AdvProcessingOptions::onVLabelClicked);
    connect(wLabel, &ClickLabel::clicked,
            this, &AdvProcessingOptions::onWLabelClicked);

    connect(uOffsetSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvProcessingOptions::updateUOffset);
    connect(vOffsetSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvProcessingOptions::updateVOffset);
    connect(wOffsetSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvProcessingOptions::updateWOffset);

    connect(wBoostCheckBox, &RichTextCheckBox::toggled,
            this, &AdvProcessingOptions::updateWBoost);
    connect(aoaCheckBox, &RichTextCheckBox::toggled,
            aoaMethLabel, &ClickLabel::setEnabled);
    connect(aoaCheckBox, &RichTextCheckBox::toggled,
            aoaMethCombo, &QComboBox::setEnabled);
    connect(aoaMethLabel, &ClickLabel::clicked,
            this, &AdvProcessingOptions::onClickAoaMethLabel);
    connect(aoaCheckBox, &RichTextCheckBox::toggled,
            this, &AdvProcessingOptions::updateAoaMethod_1);
    connect(aoaMethCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AdvProcessingOptions::updateAoaMethod_2);

    connect(rotCheckBox, &RichTextCheckBox::toggled,
            rotMethLabel, &ClickLabel::setEnabled);
    connect(rotCheckBox, &RichTextCheckBox::toggled,
            rotMethCombo, &QComboBox::setEnabled);
    connect(rotMethLabel, &ClickLabel::clicked,
            this, &AdvProcessingOptions::onClickRotMethLabel);
    connect(rotCheckBox, &RichTextCheckBox::toggled,
            this, &AdvProcessingOptions::updateRotMethod_1);
    connect(rotMethCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AdvProcessingOptions::updateRotMethod_2);
    connect(rotCheckBox, &RichTextCheckBox::toggled,
            this, &AdvProcessingOptions::updatePfSettingsButton);

    connect(pfSettingsButton, &QPushButton::clicked,
            this, &AdvProcessingOptions::showPfSettingsDialog);
    connect(tlSettingsButton, &QPushButton::clicked,
            this, &AdvProcessingOptions::showTlSettingsDialog);
    connect(detrendMethLabel, &ClickLabel::clicked,
            this, &AdvProcessingOptions::onClickDetrendLabel);
    connect(timeConstantLabel, &ClickLabel::clicked,
            this, &AdvProcessingOptions::onClickTimeConstantLabel);
    connect(detrendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AdvProcessingOptions::onClickDetrendCombo);
    connect(detrendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AdvProcessingOptions::updateDetrendMeth);

    connect(timeConstantSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvProcessingOptions::updateTimeConst);

    connect(timeLagCheckBox, &RichTextCheckBox::toggled,
            timeLagMethodLabel, &ClickLabel::setEnabled);
    connect(timeLagCheckBox, &RichTextCheckBox::toggled,
            this, &AdvProcessingOptions::updateTlSettingsButton);
    connect(timeLagCheckBox, &RichTextCheckBox::toggled,
            timeLagMethodCombo, &QComboBox::setEnabled);
    connect(timeLagMethodLabel, &ClickLabel::clicked,
            this, &AdvProcessingOptions::onClickTimeLagMethLabel);
    connect(timeLagCheckBox, &RichTextCheckBox::toggled,
            this, &AdvProcessingOptions::updateTlagMeth_1);
    connect(timeLagMethodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AdvProcessingOptions::updateTlagMeth_2);

    connect(qcCheckBox, &RichTextCheckBox::toggled,
            qcLabel, &ClickLabel::setEnabled);
    connect(qcCheckBox, &RichTextCheckBox::toggled,
            qcMethodCombo, &QComboBox::setEnabled);
    connect(qcLabel, &ClickLabel::clicked,
            this, &AdvProcessingOptions::onClickQcMethodLabel);
    connect(qcCheckBox, &RichTextCheckBox::toggled,
            this, &AdvProcessingOptions::updateQcMeth_1);
    connect(qcMethodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AdvProcessingOptions::updateQcMeth_2);

    connect(fpCheckBox, &RichTextCheckBox::toggled,
            fpLabel, &ClickLabel::setEnabled);
    connect(fpCheckBox, &RichTextCheckBox::toggled,
            fpMethodCombo, &QComboBox::setEnabled);
    connect(fpLabel, &ClickLabel::clicked,
            this, &AdvProcessingOptions::onClickFpMethodLabel);
    connect(fpCheckBox, &RichTextCheckBox::toggled,
            this, &AdvProcessingOptions::updateFpMeth_1);
    connect(fpMethodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AdvProcessingOptions::updateFpMeth_2);

    connect(cecCheckBox, &RichTextCheckBox::toggled,
            cecSettingsButton, &QPushButton::setEnabled);
    connect(cecCheckBox, &RichTextCheckBox::toggled,
            this, &AdvProcessingOptions::updateCecMeth_1);
    connect(cecSettingsButton, &QPushButton::clicked,
            this, &AdvProcessingOptions::showCecSettingsDialog);
    //> clicked, not toggled: refresh() blocks the PROJECT's signals, not the
    //> widgets', so a toggled connection would fire while a project is being
    //> loaded and quietly switch WPL on in a file the user only opened.
    //> clicked comes from a real interaction and from nothing else.
    connect(cecCheckBox, &RichTextCheckBox::clicked, this, [=]()
            {
                if (cecCheckBox->isChecked() && !wplCheckBox->isChecked())
                {
                    wplCheckBox->setChecked(true);
                }
            });

    //> Burba availability is the only thing on this page that depends on the
    //> METADATA rather than on the project, and no other Advanced page listens
    //> to DlProject at all - so adding an analyser in the Metadata File Editor
    //> would otherwise leave the box greyed until the project was reopened.
    if (dlProject_)
    {
        connect(dlProject_, &DlProject::projectChanged,
                this, &AdvProcessingOptions::updateBurbaAvailability);
        connect(dlProject_, &DlProject::projectModified,
                this, &AdvProcessingOptions::updateBurbaAvailability);
    }

    connect(wplCheckBox, &RichTextCheckBox::clicked,
            this, &AdvProcessingOptions::warnWplOffWithCec);
    connect(wplCheckBox, &RichTextCheckBox::toggled,
            this, &AdvProcessingOptions::updateWplCecWarning);
    connect(wplCheckBox, &RichTextCheckBox::toggled,
            this, &AdvProcessingOptions::updateWplMeth_1);
    connect(wplCheckBox, &RichTextCheckBox::toggled,
            this, &AdvProcessingOptions::updateBurbaGroup);
    connect(wplCheckBox, &RichTextCheckBox::toggled,
            [=](bool b){ burbaCorrCheckBox->setEnabled(b); });
    //> On the click, not the state change - the same rule the WPL and CEC
    //> boxes above follow. refresh() blocks the project's signals, not the
    //> widgets', so a state-change connection fires while a project is being
    //> loaded and would switch the correction on in a file the user only
    //> opened.
    connect(covmaxDebaselineCheckBox, &RichTextCheckBox::clicked,
            this, [=]()
            {
                ecProject_->setScreenCovmaxDebaseline(
                    covmaxDebaselineCheckBox->isChecked() ? 1 : 0);
            });
    //> Straight into the application preferences. Unlike every other control
    //> on this page this one is not part of the project, so it neither dirties
    //> the project nor travels with it.
    connect(parallelPrepassCheckBox, &RichTextCheckBox::clicked,
            this, [=]()
            {
                configState_->general.parallelPrepass =
                    parallelPrepassCheckBox->isChecked();
            });
    connect(tlagBorrowCheckBox, &RichTextCheckBox::clicked,
            this, [=]()
            {
                ecProject_->setScreenTlagBorrowMethod(
                    tlagBorrowCheckBox->isChecked() ? 1 : 0);
                updateTlagBorrowAvailability();
            });
    connect(tlagBorrowSnrSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [=](double d) { ecProject_->setScreenTlagBorrowSnr(d); });
    connect(tlagBorrowNoiseCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=](int n)
            {
                ecProject_->setScreenTlagBorrowNoise(n);
                updateTlagBorrowAvailability();
            });
    connect(tlagBorrowDonorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=](int n) { ecProject_->setScreenTlagBorrowDonor(n); });
    //> clicked, not toggled: refresh() blocks the project's signals and
    //> not the widgets', so a toggled connection writes the file back while
    //> the page is only being redrawn.
    connect(headCorrCheckBox, &RichTextCheckBox::clicked,
            this, [=]()
            {
                ecProject_->setScreenHeadCorrMeth(
                    headCorrCheckBox->isChecked()
                        ? headCorrMethCombo->currentIndex() + 1
                        : 0);
                updateSonicHardwareAvailability();
            });
    connect(headCorrMethCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=](int n)
            {
                if (headCorrCheckBox->isChecked())
                {
                    ecProject_->setScreenHeadCorrMeth(n + 1);
                }
            });
    connect(headCorrMethLabel, &ClickLabel::clicked,
            this, &AdvProcessingOptions::onClickHeadCorrMethLabel);
    connect(headCorrDirBrowse, &DirBrowseWidget::pathChanged,
            this, [=](const QString& path)
            { ecProject_->setScreenHeadCorrDir(path); });
    connect(headCorrDirBrowse, &DirBrowseWidget::pathSelected,
            this, [=](const QString& path)
            { ecProject_->setScreenHeadCorrDir(path); });

    connect(tiltSensorCheckBox, &RichTextCheckBox::clicked,
            this, [=]()
            {
                ecProject_->setScreenTiltSensorMeth(
                    tiltSensorCheckBox->isChecked()
                        ? tiltSensorMethCombo->currentIndex() + 1
                        : 0);
                updateSonicHardwareAvailability();
            });
    connect(tiltSensorMethCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [=](int n)
            {
                if (tiltSensorCheckBox->isChecked())
                {
                    ecProject_->setScreenTiltSensorMeth(n + 1);
                }
                //> The arm is only read by the swinging mode, so it greys
                //> with the choice rather than with the checkbox.
                updateSonicHardwareAvailability();
            });
    connect(tiltSensorMethLabel, &ClickLabel::clicked,
            this, &AdvProcessingOptions::onClickTiltSensorMethLabel);
    connect(tiltSensorVgSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [=](double d) { ecProject_->setScreenTiltSensorVg(d); });
    connect(tiltLpfSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [=](double d) { ecProject_->setScreenTiltLpfS(d); });
    connect(tiltArmXSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [=](double d) { ecProject_->setScreenTiltArmX(d); });
    connect(tiltArmYSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [=](double d) { ecProject_->setScreenTiltArmY(d); });
    connect(tiltArmZSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [=](double d) { ecProject_->setScreenTiltArmZ(d); });

    connect(spectroCheckBox, &RichTextCheckBox::clicked,
            this, [=]()
            {
                const bool on = spectroCheckBox->isChecked();
                ecProject_->setScreenSpectroMethod(on ? 1 : 0);
                spectroWaterCheckBox->setEnabled(on);
            });
    connect(spectroWaterCheckBox, &RichTextCheckBox::clicked,
            this, [=]()
            {
                ecProject_->setScreenSpectroWater(
                    spectroWaterCheckBox->isChecked() ? 1 : 0);
            });
    connect(burbaCorrCheckBox, &RichTextCheckBox::toggled, [=](bool checked)
            { ecProject_->setScreenBuCorr(checked); });
    connect(burbaCorrCheckBox, &RichTextCheckBox::toggled,
            this, &AdvProcessingOptions::enableBurbaCorrectionArea);
    connect(burbaRadioGroup, &QButtonGroup::idClicked,
            this, &AdvProcessingOptions::updateBurbaType_2);
    connect(setDefaultsButton, &QPushButton::clicked,
            this, &AdvProcessingOptions::on_setDefaultsButton_clicked);

    connect(ecProject_, &EcProject::ecProjectNew,
            this, &AdvProcessingOptions::reset);
    connect(ecProject_, &EcProject::ecProjectChanged,
            this, &AdvProcessingOptions::refresh);

    auto combo_list = QWidgetList() << aoaMethCombo
                                    << rotMethCombo
                                    << detrendCombo
                                    << timeLagMethodCombo
                                    << qcMethodCombo
                                    << fpMethodCombo;
    for (auto widget : combo_list)
    {
        auto combo = static_cast<QComboBox *>(widget);
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &AdvProcessingOptions::updateTooltip);
    }

    createPfSettingsDialog();
    createTlSettingsDialog();
    createPwbTlSettingsDialog();
    createCecSettingsDialog();
    QTimer::singleShot(0, this, &AdvProcessingOptions::reset);
}

AdvProcessingOptions::~AdvProcessingOptions()
{
    if (pfDialog_)
        delete pfDialog_;

    if (tlDialog_)
        delete tlDialog_;

    if (pwbTlDialog_)
        delete pwbTlDialog_;

    if (cecDialog_)
        delete cecDialog_;
}

void AdvProcessingOptions::updateUOffset(double d)
{
    ecProject_->setScreenUOffset(d);
}

void AdvProcessingOptions::updateVOffset(double d)
{
    ecProject_->setScreenVOffset(d);
}

void AdvProcessingOptions::updateWOffset(double d)
{
    ecProject_->setScreenWOffset(d);
}

// update project properties
void AdvProcessingOptions::updateAoaMethod_1(bool b)
{
    if (b)
    {
        auto value = aoaMethCombo->itemData(aoaMethCombo->currentIndex());
        ecProject_->setScreenFlowDistortion(value.toInt());
    }
    else
    {
        ecProject_->setScreenFlowDistortion(0);
    }
}

// update project properties
void AdvProcessingOptions::updateAoaMethod_2(int n)
{
    auto value = aoaMethCombo->itemData(n);
    ecProject_->setScreenFlowDistortion(value.toInt());
}

// update project properties
void AdvProcessingOptions::updateRotMethod_1(bool b)
{
    if (b)
    {
        ecProject_->setScreenRotMethod(rotMethCombo->currentIndex() + 1);
    }
    else
    {
        ecProject_->setScreenRotMethod(0);
    }
}

// update project properties and fluxes rotation choices
void AdvProcessingOptions::updateRotMethod_2(int n)
{
    ecProject_->setScreenRotMethod(n + 1);

    // planar fit
    pfSettingsButton->setEnabled((n == 2) || (n == 3));
}

void AdvProcessingOptions::updatePfSettingsButton(bool b)
{
    int n = rotMethCombo->currentIndex();
    if (b)
        pfSettingsButton->setEnabled((n == 2) || (n == 3));
    else
        pfSettingsButton->setEnabled(false);
}

void AdvProcessingOptions::updateTlSettingsButton(bool b)
{
    int n = timeLagMethodCombo->currentIndex();
    if (b)
        tlSettingsButton->setEnabled(n == 3 || n == 4);
    else
        tlSettingsButton->setEnabled(false);
}

void AdvProcessingOptions::updateDetrendMeth(int l)
{
    ecProject_->setScreenDetrendMeth(l);
    previousDetrendMethod_ = static_cast<DetrendMethod>(l);
}

void AdvProcessingOptions::updateTimeConst(double l)
{
    // write [min] on the GUI but [sec] in the file
    if (detrendCombo->currentIndex() == 1)
    {
        Q_ASSERT_X(detrendCombo->currentIndex() == 1,
                   "detrending",
                   "save linear time constant in sec");
        ecProject_->setScreenTimeConst(l * 60.0);
    }
    // write in [sec]
    else
    {
        ecProject_->setScreenTimeConst(l);
    }
}

void AdvProcessingOptions::updateTlagMeth_1(bool b)
{
    if (b)
    {
        ecProject_->setScreenTlagMeth(timeLagMethodCombo->currentIndex() + 1);
    }
    else
    {
        ecProject_->setScreenTlagMeth(0);
    }
    updateCovmaxDebaselineAvailability();
}

void AdvProcessingOptions::updateTlagMeth_2(int n)
{
    ecProject_->setScreenTlagMeth(n + 1);

    // timelag optimization button
    tlSettingsButton->setEnabled(n == 3 || n == 4);
    updateCovmaxDebaselineAvailability();
}

/// The borrowing test divides a covariance by a detection limit, so without
/// one there is nothing to decide. The engine refuses the same combination;
/// this is the interface saying so before the run rather than after it.
///
/// detlim_meth lives on the Statistical Analysis page, so this control can be
/// greyed by something changed on a different tab - which is why the tooltip
/// names where to switch it on.
///
/// Both corrections carry parameters that only one of their modes reads, so
/// the greying is finer than a single checkbox: the lever arm is meaningless
/// under plain position correction and says so by being unavailable, rather
/// than sitting there inviting a number nothing will use.
void AdvProcessingOptions::updateSonicHardwareAvailability()
{
    const bool head = headCorrCheckBox->isChecked();
    headCorrMethLabel->setEnabled(head);
    headCorrMethCombo->setEnabled(head);
    headCorrDirLabel->setEnabled(head);
    headCorrDirBrowse->setEnabled(head);

    const bool tilt = tiltSensorCheckBox->isChecked();
    tiltSensorMethLabel->setEnabled(tilt);
    tiltSensorMethCombo->setEnabled(tilt);
    tiltSensorVgLabel->setEnabled(tilt);
    tiltSensorVgSpin->setEnabled(tilt);
    tiltLpfLabel->setEnabled(tilt);
    tiltLpfSpin->setEnabled(tilt);

    const bool swinging = tilt && tiltSensorMethCombo->currentIndex() == 1;
    tiltArmLabel->setEnabled(swinging);
    tiltArmXLabel->setEnabled(swinging);
    tiltArmYLabel->setEnabled(swinging);
    tiltArmZLabel->setEnabled(swinging);
    tiltArmXSpin->setEnabled(swinging);
    tiltArmYSpin->setEnabled(swinging);
    tiltArmZSpin->setEnabled(swinging);
}

void AdvProcessingOptions::onClickHeadCorrMethLabel()
{
    if (headCorrMethCombo->isEnabled())
    {
        headCorrMethCombo->showPopup();
    }
}

void AdvProcessingOptions::onClickTiltSensorMethLabel()
{
    if (tiltSensorMethCombo->isEnabled())
    {
        tiltSensorMethCombo->showPopup();
    }
}

void AdvProcessingOptions::updateTlagBorrowAvailability()
{
    //> Only ONE of the two floors needs something else switched on. The
    //> detection limit is computed elsewhere and read back, so asking for it
    //> without it would divide by a number nothing produced; the Lenschow
    //> noise is measured from the series in hand. The engine draws the same
    //> distinction, which is what lets EddyUH's combination stand alone.
    const bool needsLimit = ecProject_->screenTlagBorrowNoise() == 0;
    const bool haveLimit = ecProject_->screenDetlimMethod() > 0;
    const bool usable = haveLimit || !needsLimit;

    tlagBorrowCheckBox->setEnabled(usable);
    const bool on = usable && tlagBorrowCheckBox->isChecked();
    tlagBorrowSnrLabel->setEnabled(on);
    tlagBorrowSnrSpin->setEnabled(on);
    tlagBorrowDonorLabel->setEnabled(on);
    tlagBorrowDonorCombo->setEnabled(on);

    //> The floor itself stays reachable whenever borrowing is ticked, even
    //> when the current choice is the unavailable one - otherwise a user
    //> whose detection limit is off would find the control that fixes it
    //> greyed out along with everything else.
    const bool ticked = tlagBorrowCheckBox->isChecked();
    tlagBorrowNoiseLabel->setEnabled(ticked);
    tlagBorrowNoiseCombo->setEnabled(ticked);
}

/// The baseline subtraction only means anything to a method that maximises a
/// covariance: tlag_meth 2 and 3, which are combo rows 1 and 2. Constant does
/// not search, and the optimiser and the block-bootstrap choose their lags by
/// other machinery entirely.
void AdvProcessingOptions::updateCovmaxDebaselineAvailability()
{
    const auto meth = ecProject_->screenTlagMeth();
    covmaxDebaselineCheckBox->setEnabled(meth == 2 || meth == 3);
}

void AdvProcessingOptions::onClickDetrendCombo(int newDetrendMethod)
{
    DetrendMethod currDetrendMethod = static_cast<DetrendMethod>(newDetrendMethod);

    if (previousDetrendMethod_ == DetrendMethod::LinearDetrending)
    {
        if (timeConstantSpin->value() == 0.0)
            timeConstantSpin->setValue(250.0);
        else
            timeConstantSpin->setValue(timeConstantSpin->value() * 60.0);
    }
    else if (currDetrendMethod == DetrendMethod::LinearDetrending)
    {
        timeConstantSpin->setValue(timeConstantSpin->value() / 60.0);
    }

    if (currDetrendMethod == DetrendMethod::BlockAverage)
    {
        timeConstantSpin->setSingleStep(10.0);
        timeConstantSpin->setSuffix(tr("  [s]", "Second"));
        timeConstantLabel->setEnabled(false);
        timeConstantSpin->setEnabled(false);
    }
    else if (currDetrendMethod == DetrendMethod::LinearDetrending)
    {
        if (qFuzzyCompare(timeConstantSpin->value(), 4.2))
        {
            timeConstantSpin->setValue(0.0);
        }
        timeConstantSpin->setSingleStep(1.0);
        timeConstantSpin->setSuffix(tr("  [min]", "Minute"));
        timeConstantLabel->setEnabled(true);
        timeConstantSpin->setEnabled(true);
        timeConstantSpin->setFocus();
        timeConstantSpin->selectAll();
    }
    else
    {
        timeConstantSpin->setSingleStep(10.0);
        timeConstantSpin->setSuffix(tr("  [s]", "Second"));
        timeConstantLabel->setEnabled(true);
        timeConstantSpin->setEnabled(true);
        timeConstantSpin->setFocus();
        timeConstantSpin->selectAll();
    }

    // possibly write the new correct value in minutes or seconds
    updateTimeConst(timeConstantSpin->value());
}

void AdvProcessingOptions::onClickDetrendLabel()
{
    detrendCombo->setFocus();
    detrendCombo->showPopup();
}

void AdvProcessingOptions::onClickTimeConstantLabel()
{
    if (timeConstantSpin->isEnabled())
    {
        timeConstantSpin->setFocus();
        timeConstantSpin->selectAll();
    }
}

void AdvProcessingOptions::updateWBoost(bool b)
{
    ecProject_->setScreenWBoost(b);
}

void AdvProcessingOptions::onClickAoaMethLabel()
{
    if (aoaMethCombo->isEnabled())
    {
        aoaMethCombo->showPopup();
    }
}

void AdvProcessingOptions::onClickRotMethLabel()
{
    if (rotMethCombo->isEnabled())
    {
        rotMethCombo->showPopup();
    }
}

void AdvProcessingOptions::onClickTimeLagMethLabel()
{
    if (timeLagMethodCombo->isEnabled())
    {
        timeLagMethodCombo->showPopup();
    }
}

void AdvProcessingOptions::onULabelClicked()
{
    uOffsetSpin->setFocus();
    uOffsetSpin->selectAll();
}

void AdvProcessingOptions::onVLabelClicked()
{
    vOffsetSpin->setFocus();
    vOffsetSpin->selectAll();
}

void AdvProcessingOptions::onWLabelClicked()
{
    wOffsetSpin->setFocus();
    wOffsetSpin->selectAll();
}

void AdvProcessingOptions::reset()
{
    // save the modified flag to prevent side effects of setting widgets
    bool oldmod = ecProject_->modified();
    ecProject_->blockSignals(true);

    uOffsetSpin->setValue(ecProject_->defaultSettings.screenSetting.u_offset);
    vOffsetSpin->setValue(ecProject_->defaultSettings.screenSetting.v_offset);
    wOffsetSpin->setValue(ecProject_->defaultSettings.screenSetting.w_offset);

    wBoostCheckBox->setChecked(ecProject_->defaultSettings.screenSetting.gill_wm_wboost);
    aoaCheckBox->setChecked(ecProject_->defaultSettings.screenSetting.flow_distortion);
    aoaMethCombo->setCurrentIndex(0);

    rotCheckBox->setChecked(true);
    rotMethCombo->setCurrentIndex(0);
    pfSettingsButton->setEnabled(false);

    detrendCombo->setCurrentIndex(ecProject_->defaultSettings.screenSetting.detrend_meth);

    timeConstantSpin->setValue(ecProject_->defaultSettings.screenSetting.timeconst);

    timeLagCheckBox->setChecked(true);
    timeLagMethodCombo->setCurrentIndex(1);
    tlSettingsButton->setEnabled(false);

    pfDialog_->reset();
    tlDialog_->reset();

    qcLabel->setEnabled(true);
    qcCheckBox->setChecked(true);
    qcMethodCombo->setCurrentIndex(0);

    fpLabel->setEnabled(true);
    fpCheckBox->setChecked(true);
    fpMethodCombo->setCurrentIndex(0);

    cecCheckBox->setChecked(false);
    cecSettingsButton->setEnabled(false);

    wplCheckBox->setChecked(ecProject_->defaultSettings.projectGeneral.wpl_meth);
    tlagBorrowCheckBox->setChecked(ecProject_->defaultSettings.screenSetting.tlag_borrow_meth);
    tlagBorrowSnrSpin->setValue(ecProject_->defaultSettings.screenSetting.tlag_borrow_snr);
    WidgetUtils::resetComboToItem(tlagBorrowNoiseCombo,
        ecProject_->defaultSettings.screenSetting.tlag_borrow_noise);
    WidgetUtils::resetComboToItem(tlagBorrowDonorCombo,
        ecProject_->defaultSettings.screenSetting.tlag_borrow_donor);
    updateTlagBorrowAvailability();
    headCorrCheckBox->setChecked(ecProject_->defaultSettings.screenSetting.head_corr_meth > 0);
    WidgetUtils::resetComboToItem(headCorrMethCombo,
        qMax(0, ecProject_->defaultSettings.screenSetting.head_corr_meth - 1));
    headCorrDirBrowse->setPath(ecProject_->defaultSettings.screenSetting.head_corr_dir);
    tiltSensorCheckBox->setChecked(ecProject_->defaultSettings.screenSetting.tilt_sensor_meth > 0);
    WidgetUtils::resetComboToItem(tiltSensorMethCombo,
        qMax(0, ecProject_->defaultSettings.screenSetting.tilt_sensor_meth - 1));
    tiltSensorVgSpin->setValue(ecProject_->defaultSettings.screenSetting.tilt_sensor_v_g);
    tiltLpfSpin->setValue(ecProject_->defaultSettings.screenSetting.tilt_lpf_s);
    tiltArmXSpin->setValue(ecProject_->defaultSettings.screenSetting.tilt_arm_x);
    tiltArmYSpin->setValue(ecProject_->defaultSettings.screenSetting.tilt_arm_y);
    tiltArmZSpin->setValue(ecProject_->defaultSettings.screenSetting.tilt_arm_z);
    updateSonicHardwareAvailability();
    spectroCheckBox->setChecked(ecProject_->defaultSettings.screenSetting.spectro_meth);
    spectroWaterCheckBox->setChecked(ecProject_->defaultSettings.screenSetting.spectro_water);
    spectroWaterCheckBox->setEnabled(ecProject_->defaultSettings.screenSetting.spectro_meth);

    setBurbaDefaultValues();

    burbaCorrCheckBox->setEnabled(true);
    burbaCorrCheckBox->setChecked(false);

    enableBurbaCorrectionArea(false);
    //> setEnabled(true) above is the default for a project that CAN have the
    //> correction. Whether this one can depends on the metadata, which a
    //> reset does not touch - so without this, resetting a project with no
    //> LI-7500 handed back a clickable box for a correction the engine drops.
    //> Every other path that can change the answer already ends here.
    updateBurbaAvailability();

    burbaSimpleRadio->setChecked(true);
    burbaParamWidget->setCurrentIndex(0);
    burbaSimpleTab->setCurrentIndex(0);
    burbaMultiTab->setCurrentIndex(0);

    WidgetUtils::updateComboItemTooltip(aoaMethCombo, 0);
    WidgetUtils::updateComboItemTooltip(rotMethCombo, 0);
    WidgetUtils::updateComboItemTooltip(detrendCombo, 0);
    WidgetUtils::updateComboItemTooltip(timeLagMethodCombo, 1);
    WidgetUtils::updateComboItemTooltip(qcMethodCombo, 0);
    WidgetUtils::updateComboItemTooltip(fpMethodCombo, 0);

    // restore modified flag
    ecProject_->setModified(oldmod);
    ecProject_->blockSignals(false);
}

void AdvProcessingOptions::refresh()
{
    // save the modified flag to prevent side effects of setting widgets
    bool oldmod = ecProject_->modified();
    ecProject_->blockSignals(true);

    uOffsetSpin->setValue(ecProject_->screenUOffset());
    vOffsetSpin->setValue(ecProject_->screenVOffset());
    wOffsetSpin->setValue(ecProject_->screenWOffset());

    wBoostCheckBox->setChecked(ecProject_->screenWBoost());

    auto aoaCorrection = ecProject_->screenFlowDistortion();
    aoaCheckBox->setChecked(aoaCorrection);
    if (aoaCorrection)
    {
        switch (aoaCorrection)
        {
        // nakai 2012
        case 1:
            aoaMethCombo->setCurrentIndex(1);
            break;
        // nakai 2006
        case 2:
            aoaMethCombo->setCurrentIndex(2);
            break;
        // automatic
        case -1:
            aoaMethCombo->setCurrentIndex(0);
            break;
        }
    }
    else
    {
        aoaMethCombo->setCurrentIndex(0);
    }

    rotCheckBox->setChecked(ecProject_->screenRotMethod());
    if (ecProject_->screenRotMethod())
    {
        rotMethCombo->setCurrentIndex(ecProject_->screenRotMethod() - 1);
    }
    else
    {
        rotMethCombo->setCurrentIndex(0);
    }

    pfSettingsButton->setEnabled((ecProject_->screenRotMethod() == 3)
                                 || (ecProject_->screenRotMethod() == 4));

    timeConstantSpin->setValue(ecProject_->screenTimeConst());
    detrendCombo->setCurrentIndex(ecProject_->screenDetrendMeth());

    timeLagCheckBox->setChecked(ecProject_->screenTlagMeth());
    if (ecProject_->screenTlagMeth())
    {
        timeLagMethodCombo->setCurrentIndex(ecProject_->screenTlagMeth() - 1);
    }
    else
    {
        timeLagMethodCombo->setCurrentIndex(0);
    }
    tlSettingsButton->setEnabled(ecProject_->screenTlagMeth() == 4 || ecProject_->screenTlagMeth() == 5);
    covmaxDebaselineCheckBox->setChecked(ecProject_->screenCovmaxDebaseline());
    parallelPrepassCheckBox->setChecked(configState_->general.parallelPrepass);
    tlagBorrowCheckBox->setChecked(ecProject_->screenTlagBorrowMethod());
    tlagBorrowSnrSpin->setValue(ecProject_->screenTlagBorrowSnr());
    tlagBorrowNoiseCombo->setCurrentIndex(ecProject_->screenTlagBorrowNoise());
    tlagBorrowDonorCombo->setCurrentIndex(ecProject_->screenTlagBorrowDonor());
    updateTlagBorrowAvailability();
    updateCovmaxDebaselineAvailability();

    //> Zero is off, so the combo carries the mode minus one and is left
    //> where it was when the correction is off - a project that has never
    //> used it opens on the first mode rather than on nothing.
    headCorrCheckBox->setChecked(ecProject_->screenHeadCorrMeth() > 0);
    headCorrMethCombo->setCurrentIndex(qMax(0, ecProject_->screenHeadCorrMeth() - 1));
    headCorrDirBrowse->setPath(ecProject_->screenHeadCorrDir());
    tiltSensorCheckBox->setChecked(ecProject_->screenTiltSensorMeth() > 0);
    tiltSensorMethCombo->setCurrentIndex(qMax(0, ecProject_->screenTiltSensorMeth() - 1));
    tiltSensorVgSpin->setValue(ecProject_->screenTiltSensorVg());
    tiltLpfSpin->setValue(ecProject_->screenTiltLpfS());
    tiltArmXSpin->setValue(ecProject_->screenTiltArmX());
    tiltArmYSpin->setValue(ecProject_->screenTiltArmY());
    tiltArmZSpin->setValue(ecProject_->screenTiltArmZ());
    updateSonicHardwareAvailability();

    qcCheckBox->setChecked(ecProject_->generalQcfMeth());
    if (ecProject_->generalQcfMeth())
    {
        qcMethodCombo->setCurrentIndex(ecProject_->generalQcfMeth() - 1);
    }
    else
    {
        qcMethodCombo->setCurrentIndex(0);
    }

    fpCheckBox->setChecked(ecProject_->generalFpMeth());
    if (ecProject_->generalFpMeth())
    {
        fpMethodCombo->setCurrentIndex(ecProject_->generalFpMeth() - 1);
    }
    else
    {
        fpMethodCombo->setCurrentIndex(0);
    }

    cecCheckBox->setChecked(ecProject_->generalCecMeth() > 0);
    cecSettingsButton->setEnabled(ecProject_->generalCecMeth() > 0);
    updateCecAvailability();

    wplCheckBox->setChecked(ecProject_->generalWplMeth());
    spectroCheckBox->setChecked(ecProject_->screenSpectroMethod());
    spectroWaterCheckBox->setChecked(ecProject_->screenSpectroWater());
    //> The water switch is meaningless on its own.
    spectroWaterCheckBox->setEnabled(ecProject_->screenSpectroMethod());

    burbaCorrCheckBox->setChecked(ecProject_->screenBuCorr());
    burbaCorrCheckBox->setEnabled(ecProject_->generalWplMeth());

    burbaRadioGroup->buttons().at(ecProject_->screenBuMulti())->setChecked(true);

    //> The stack page follows the project, because which regression is in force
    //> IS a project setting. Which of that page's day/night tabs is on top is
    //> not: it is where the user last was, and forcing it back to Day here sent
    //> them there again on every refresh.
    burbaParamWidget->setCurrentIndex(ecProject_->screenBuMulti());

    enableBurbaCorrectionArea(wplCheckBox->isChecked()
                              && burbaCorrCheckBox->isChecked());
    //> Last, so it can override the two lines above: without an LI-7500 the box
    //> goes off and stays off, whatever the project said.
    updateBurbaAvailability();

    lDayBotGain->setText(QString::number(ecProject_->screenLDayBotGain(), 'f', 3));
    lDayBotOffset->setText(QString::number(ecProject_->screenLDayBotOffset(), 'f', 2));
    lDayTopGain->setText(QString::number(ecProject_->screenLDayTopGain(), 'f', 3));
    lDayTopOffset->setText(QString::number(ecProject_->screenLDayTopOffset(), 'f', 2));
    lDaySparGain->setText(QString::number(ecProject_->screenLDaySparGain(), 'f', 3));
    lDaySparOffset->setText(QString::number(ecProject_->screenLDaySparOffset(), 'f', 2));
    lNightBotGain->setText(QString::number(ecProject_->screenLNightBotGain(), 'f', 3));
    lNightBotOffset->setText(QString::number(ecProject_->screenLNightBotOffset(), 'f', 2));
    lNightTopGain->setText(QString::number(ecProject_->screenLNightTopGain(), 'f', 3));
    lNightTopOffset->setText(QString::number(ecProject_->screenLNightTopOffset(), 'f', 2));
    lNightSparGain->setText(QString::number(ecProject_->screenLNightSparGain(), 'f', 3));
    lNightSparOffset->setText(QString::number(ecProject_->screenLNightSparOffset(), 'f', 2));
    mDayBot1->setText(QString::number(ecProject_->screenMDayBot1(), 'f', 1));
    mDayBot2->setText(QString::number(ecProject_->screenMDayBot2(), 'f', 4));
    mDayBot3->setText(QString::number(ecProject_->screenMDayBot3(), 'f', 4));
    mDayBot4->setText(QString::number(ecProject_->screenMDayBot4(), 'f', 3));
    mDayTop1->setText(QString::number(ecProject_->screenMDayTop1(), 'f', 1));
    mDayTop2->setText(QString::number(ecProject_->screenMDayTop2(), 'f', 4));
    mDayTop3->setText(QString::number(ecProject_->screenMDayTop3(), 'f', 4));
    mDayTop4->setText(QString::number(ecProject_->screenMDayTop4(), 'f', 3));
    mDaySpar1->setText(QString::number(ecProject_->screenMDaySpar1(), 'f', 1));
    mDaySpar2->setText(QString::number(ecProject_->screenMDaySpar2(), 'f', 4));
    mDaySpar3->setText(QString::number(ecProject_->screenMDaySpar3(), 'f', 4));
    mDaySpar4->setText(QString::number(ecProject_->screenMDaySpar4(), 'f', 3));
    mNightBot1->setText(QString::number(ecProject_->screenMNightBot1(), 'f', 1));
    mNightBot2->setText(QString::number(ecProject_->screenMNightBot2(), 'f', 4));
    mNightBot3->setText(QString::number(ecProject_->screenMNightBot3(), 'f', 4));
    mNightBot4->setText(QString::number(ecProject_->screenMNightBot4(), 'f', 3));
    mNightTop1->setText(QString::number(ecProject_->screenMNightTop1(), 'f', 1));
    mNightTop2->setText(QString::number(ecProject_->screenMNightTop2(), 'f', 4));
    mNightTop3->setText(QString::number(ecProject_->screenMNightTop3(), 'f', 4));
    mNightTop4->setText(QString::number(ecProject_->screenMNightTop4(), 'f', 3));
    mNightSpar1->setText(QString::number(ecProject_->screenMNightSpar1(), 'f', 1));
    mNightSpar2->setText(QString::number(ecProject_->screenMNightSpar2(), 'f', 4));
    mNightSpar3->setText(QString::number(ecProject_->screenMNightSpar3(), 'f', 4));
    mNightSpar4->setText(QString::number(ecProject_->screenMNightSpar4(), 'f', 3));

    // restore modified flag
    ecProject_->setModified(oldmod);

    updateWplCecWarning();
    ecProject_->blockSignals(false);
}

void AdvProcessingOptions::createPfSettingsDialog()
{
    if (!pfDialog_)
    {
        pfDialog_ = new PlanarFitSettingsDialog(this, ecProject_, configState_);
    }
}

void AdvProcessingOptions::showPfSettingsDialog()
{
    pfDialog_->refresh();
    pfDialog_->show();
    pfDialog_->raise();
    pfDialog_->activateWindow();
}

void AdvProcessingOptions::createTlSettingsDialog()
{
    if (!tlDialog_)
    {
        tlDialog_ = new TimeLagSettingsDialog(this, ecProject_, configState_);
    }
}

void AdvProcessingOptions::createPwbTlSettingsDialog()
{
    if (!pwbTlDialog_)
    {
        pwbTlDialog_ = new PwbTimelagSettingsDialog(this, ecProject_, configState_);
    }
}

void AdvProcessingOptions::createCecSettingsDialog()
{
    if (!cecDialog_)
    {
        cecDialog_ = new CecSettingsDialog(this, ecProject_, dlProject_);
    }
}

void AdvProcessingOptions::showTlSettingsDialog()
{
    if (ecProject_->screenTlagMeth() == 5)
    {
        pwbTlDialog_->refresh();
        pwbTlDialog_->show();
        pwbTlDialog_->raise();
        pwbTlDialog_->activateWindow();
    }
    else
    {
        tlDialog_->refresh();
        tlDialog_->show();
        tlDialog_->raise();
        tlDialog_->activateWindow();
    }
}

void AdvProcessingOptions::showCecSettingsDialog()
{
    cecDialog_->refresh();
    cecDialog_->show();
    cecDialog_->raise();
    cecDialog_->activateWindow();
}

void AdvProcessingOptions::onClickQcMethodLabel()
{
    if (qcMethodCombo->isEnabled())
    {
        qcMethodCombo->showPopup();
    }
}

void AdvProcessingOptions::updateQcMeth_1(bool b)
{
    if (b)
    {
        ecProject_->setGeneralQcMeth(qcMethodCombo->currentIndex() + 1);

    }
    else
    {
        ecProject_->setGeneralQcMeth(0);
    }
}

void AdvProcessingOptions::updateQcMeth_2(int n)
{
    ecProject_->setGeneralQcMeth(n + 1);
}

void AdvProcessingOptions::onClickFpMethodLabel()
{
    if (fpMethodCombo->isEnabled())
    {
        fpMethodCombo->showPopup();
    }
}

void AdvProcessingOptions::updateFpMeth_1(bool b)
{
    if (b)
    {
        ecProject_->setGeneralFpMeth(fpMethodCombo->currentIndex() + 1);

    }
    else
    {
        ecProject_->setGeneralFpMeth(0);
    }
}

void AdvProcessingOptions::updateFpMeth_2(int n)
{
    ecProject_->setGeneralFpMeth(n + 1);
}

/// The master switch, and only that.
///
/// It used to carry a three-way choice of which flux to partition, which said
/// nothing useful once a site could have a pairing per analyser - and which
/// was the direct cause of the interface offering "H2O flux only" to a project
/// with no CO2 at all, where the engine cannot build an octant and every
/// column came out empty. Which fluxes a pairing partitions is now the
/// pairing's own setting, in the CEC Settings table beside the channels it
/// pairs.
void AdvProcessingOptions::updateCecMeth_1(bool b)
{
    ecProject_->setGeneralCecMeth(b ? 1 : 0);
    updateCecAvailability();
    //> After updateCecAvailability, which can force the box back off under a
    //> QSignalBlocker - so asking the widget here is asking the settled state.
    updateWplCecWarning();
}

/// The triangle beside the WPL checkbox: on when the partition is on and the
/// correction it depends on is not.
///
/// Driven from `toggled` rather than `clicked`, unlike the dialog. This one is
/// passive and it SHOULD light up on load: a project saved with the partition
/// on and the correction off is exactly the case worth flagging, and it is the
/// case that opens without a dialog.
void AdvProcessingOptions::updateWplCecWarning()
{
    wplWarningLabel->setVisible(cecCheckBox->isChecked()
                                && !wplCheckBox->isChecked());
}

/// Turning the density correction off while the partition is on.
///
/// Allowed - some sites read mixing ratios straight off a closed-path analyser
/// and the correction is a no-op for them - but said out loud, because for an
/// open-path site it silently moves parcels into the wrong octant. The engine
/// repeats this as Warning(114) at run time.
void AdvProcessingOptions::warnWplOffWithCec()
{
    if (wplCheckBox->isChecked()) { return; }
    if (!cecCheckBox->isChecked()) { return; }

    WidgetUtils::warning(this,
        tr("Conditional Eddy Covariance Needs the Density Correction"),
        tr("<b>Conditional Eddy Covariance is on, and you have just switched off "
           "the compensation of density fluctuations.</b>"),
        tr("The partition sorts each air parcel into an octant by the SIGN of its "
           "water and carbon dioxide fluctuation. In a molar density from an "
           "open-path analyser, part of that fluctuation is the air expanding and "
           "contracting rather than the gas arriving, and that part is large "
           "enough to reverse the sign - so parcels land in the wrong octant and "
           "the ratio is drawn from the wrong points."
           "<p>Zahn et al. (2022) require the fluctuations themselves to carry "
           "the density correction, separately from the totals. If your analyser "
           "already reports mixing ratios, this does not affect you.</p>"
           "<p>The setting has been left off. The run will report this as "
           "Warning(114).</p>"));
}

void AdvProcessingOptions::updateCecAvailability()
{
    //> Conditional Eddy Covariance is this program's own; a SmartFlux module
    //> runs LI-COR's EddyPro and has never heard of it. Handled here rather
    //> than only in setSmartfluxUI so that the two callers that re-derive
    //> availability - refresh() and updateCecMeth_1() - cannot switch it back
    //> on while the mode is active.
    if (configState_->project.smartfluxMode)
    {
        QSignalBlocker blocker(cecCheckBox);
        cecCheckBox->setChecked(false);
        cecCheckBox->setEnabled(false);
        cecSettingsButton->setEnabled(false);
        return;
    }

    //> Asked of the records rather than the two legacy columns, so a site
    //> that measures CO2 on a second analyser still counts.
    //>
    //> BOTH species, not either. The octants are the signs of w', q' and c'
    //> together, so the method needs a water channel and a carbon channel
    //> whichever of the two fluxes it is asked to report - "CO2 only" still
    //> reads the water. This used to accept either and force the choice to
    //> whichever was present, which the engine then refused, and the project
    //> got a full set of columns containing nothing but the error code.
    const bool hasCo2 = !ecProject_->gasRecordsFor(QStringLiteral("co2")).isEmpty()
                        || ecProject_->generalColCo2() != -1;
    const bool hasH2o = !ecProject_->gasRecordsFor(QStringLiteral("h2o")).isEmpty()
                        || ecProject_->generalColH2o() != -1;
    const bool hasBoth = hasCo2 && hasH2o;

    cecCheckBox->setEnabled(hasBoth);
    cecCheckBox->setToolTip(hasBoth
        ? cecAvailableTooltip_
        : tr("<b>Conditional Eddy Covariance:</b> Unavailable. The method sorts "
             "air parcels by the signs of the vertical wind, the water and the "
             "carbon dioxide together, so it needs both a CO\xe2\x82\x82 channel "
             "and an H\xe2\x82\x82O channel \xe2\x80\x93 whichever of the two "
             "fluxes you want partitioned."));

    if (!hasBoth)
    {
        QSignalBlocker blocker(cecCheckBox);
        cecCheckBox->setChecked(false);
        cecSettingsButton->setEnabled(false);
        //> Only when it actually says otherwise. EcProject::loadEcProject()
        //> calls setModified(false) BEFORE it emits ecProjectChanged(), and
        //> refresh() - which lands on this - is what that signal drives. So
        //> writing the 0 that is already there marks a project modified purely
        //> for having been opened, and a site that measures only one of the two
        //> species hits this on every single load.
        if (ecProject_->generalCecMeth() != 0)
        {
            ecProject_->setGeneralCecMeth(0);
        }
        return;
    }

    cecSettingsButton->setEnabled(cecCheckBox->isChecked());
}

/// Neutralise the two options a SmartFlux module cannot run.
///
/// Conditional Eddy Covariance and the pre-whitening block-bootstrap time lag
/// are both this program's own, and the package is an EddyPro project - so a
/// user who configured either would get a module that silently did something
/// else. The page had no SmartFlux handling at all before; only its two child
/// dialogs were reached.
void AdvProcessingOptions::setSmartfluxUI()
{
    const bool on = configState_->project.smartfluxMode;

    auto oldmod = false;
    if (!on)
    {
        oldmod = ecProject_->modified();
        ecProject_->blockSignals(true);
    }

    //> Deliberately no saved-enabled stack. The pages that keep one push onto
    //> a vector they never clear and restore by position, so from the second
    //> toggle they hand back the first cycle's values; updateCecAvailability
    //> derives the right state from the records every time, so there is
    //> nothing worth remembering.
    //> Same guard, same reason: entering the mode with the partition already
    //> off is not a change to the project.
    if (on && ecProject_->generalCecMeth() != 0)
    {
        ecProject_->setGeneralCecMeth(0);
    }
    updateCecAvailability();

    timeLagMethodCombo->setItemData(4,
        on ? QStringLiteral("disabled") : QStringLiteral("enabled"),
        Qt::UserRole);

    //> An unselectable item is not enough on its own: the project may already
    //> have been saved with the method set, in which case the combo is sitting
    //> on it and nothing would move it off.
    if (on && timeLagMethodCombo->currentIndex() == 4)
    {
        timeLagMethodCombo->setCurrentIndex(1);
        updateTlagMeth_2(1);
    }

    if (!on)
    {
        ecProject_->setModified(oldmod);
        ecProject_->blockSignals(false);
    }
}

void AdvProcessingOptions::createBurbaParamItems()
{
    auto simpleDayGrid = new QGridLayout;
    auto simpleNightGrid = new QGridLayout;
    auto multiDayGrid = new QGridLayout;
    auto multiNightGrid = new QGridLayout;

    simpleDayGrid->addWidget(new QLabel(tr("Bottom :"), this), 0, 0, 1, 1, Qt::AlignRight);
    simpleDayGrid->addWidget(new QLabel(tr("T<sub>bot</sub> = "), this), 0, 1, 1, 1);
    simpleDayGrid->addWidget(new QLabel(tr(" * T<sub>a</sub> + "), this), 0, 3, 1, 1);
    simpleDayGrid->addWidget(new QLabel(tr("Top :"), this), 1, 0, 1, 1, Qt::AlignRight);
    simpleDayGrid->addWidget(new QLabel(tr("T<sub>top</sub> = "), this), 1, 1, 1, 1);
    simpleDayGrid->addWidget(new QLabel(tr(" * T<sub>a</sub> + "), this), 1, 3, 1, 1);
    simpleDayGrid->addWidget(new QLabel(tr("Spar :"), this), 2, 0, 1, 1, Qt::AlignRight);
    simpleDayGrid->addWidget(new QLabel(tr("T<sub>spar</sub> = "), this), 2, 1, 1, 1);
    simpleDayGrid->addWidget(new QLabel(tr(" * T<sub>a</sub> + "), this), 2, 3, 1, 1);

    simpleNightGrid->addWidget(new QLabel(tr("Bottom :"), this), 0, 0, 1, 1, Qt::AlignRight);
    simpleNightGrid->addWidget(new QLabel(tr("T<sub>bot</sub> = "), this), 0, 1, 1, 1);
    simpleNightGrid->addWidget(new QLabel(tr(" * T<sub>a</sub> + "), this), 0, 3, 1, 1);
    simpleNightGrid->addWidget(new QLabel(tr("Top :"), this), 1, 0, 1, 1, Qt::AlignRight);
    simpleNightGrid->addWidget(new QLabel(tr("T<sub>top</sub> = "), this), 1, 1, 1, 1);
    simpleNightGrid->addWidget(new QLabel(tr(" * T<sub>a</sub> + "), this), 1, 3, 1, 1);
    simpleNightGrid->addWidget(new QLabel(tr("Spar :"), this), 2, 0, 1, 1, Qt::AlignRight);
    simpleNightGrid->addWidget(new QLabel(tr("T<sub>spar</sub> = "), this), 2, 1, 1, 1);
    simpleNightGrid->addWidget(new QLabel(tr(" * T<sub>a</sub> + "), this), 2, 3, 1, 1);

    // matching floating point number with no exponents
    QString floatingPointRegexp = QStringLiteral("[-+]?[0-9]*\\.?[0-9]+");
    // TODO: use a QDoubleValidator with also range specs

    lDayBotGain = new CustomResetLineEdit;
    lDayBotGain->setMaxLength(10);
    lDayBotGain->setRegExp(floatingPointRegexp);
    lDayBotOffset = new CustomResetLineEdit;
    lDayBotOffset->setMaxLength(10);
    lDayBotOffset->setRegExp(floatingPointRegexp);
    lDayTopGain = new CustomResetLineEdit;
    lDayTopGain->setMaxLength(10);
    lDayTopGain->setRegExp(floatingPointRegexp);
    lDayTopOffset = new CustomResetLineEdit;
    lDayTopOffset->setMaxLength(10);
    lDayTopOffset->setRegExp(floatingPointRegexp);
    lDaySparGain = new CustomResetLineEdit;
    lDaySparGain->setMaxLength(10);
    lDaySparGain->setRegExp(floatingPointRegexp);
    lDaySparOffset = new CustomResetLineEdit;
    lDaySparOffset->setMaxLength(10);
    lDaySparOffset->setRegExp(floatingPointRegexp);

    simpleDayGrid->addWidget(lDayBotGain, 0, 2, 1, 1);
    simpleDayGrid->addWidget(lDayBotOffset, 0, 4, 1, 1);
    simpleDayGrid->addWidget(lDayTopGain, 1, 2, 1, 1);
    simpleDayGrid->addWidget(lDayTopOffset, 1, 4, 1, 1);
    simpleDayGrid->addWidget(lDaySparGain, 2, 2, 1, 1);
    simpleDayGrid->addWidget(lDaySparOffset, 2, 4, 1, 1);
    simpleDayGrid->setColumnStretch(5, 1);

    lNightBotGain = new CustomResetLineEdit;
    lNightBotGain->setMaxLength(10);
    lNightBotGain->setRegExp(floatingPointRegexp);
    lNightBotOffset = new CustomResetLineEdit;
    lNightBotOffset->setMaxLength(10);
    lNightBotOffset->setRegExp(floatingPointRegexp);
    lNightTopGain = new CustomResetLineEdit;
    lNightTopGain->setMaxLength(10);
    lNightTopGain->setRegExp(floatingPointRegexp);
    lNightTopOffset = new CustomResetLineEdit;
    lNightTopOffset->setMaxLength(10);
    lNightTopOffset->setRegExp(floatingPointRegexp);
    lNightSparGain = new CustomResetLineEdit;
    lNightSparGain->setMaxLength(10);
    lNightSparGain->setRegExp(floatingPointRegexp);
    lNightSparOffset = new CustomResetLineEdit;
    lNightSparOffset->setMaxLength(10);
    lNightSparOffset->setRegExp(floatingPointRegexp);

    simpleNightGrid->addWidget(lNightBotGain, 0, 2, 1, 1);
    simpleNightGrid->addWidget(lNightBotOffset, 0, 4, 1, 1);
    simpleNightGrid->addWidget(lNightTopGain, 1, 2, 1, 1);
    simpleNightGrid->addWidget(lNightTopOffset, 1, 4, 1, 1);
    simpleNightGrid->addWidget(lNightSparGain, 2, 2, 1, 1);
    simpleNightGrid->addWidget(lNightSparOffset, 2, 4, 1, 1);
    simpleNightGrid->setColumnStretch(5, 1);

    multiDayGrid->addWidget(new QLabel(tr("Bottom :"), this), 0, 0, 1, 1, Qt::AlignRight);
    multiDayGrid->addWidget(new QLabel(tr("T<sub>bot</sub> - T<sub>a</sub> = "), this), 0, 1, 1, 1);
    multiDayGrid->addWidget(new QLabel(tr(" * T<sub>a</sub> + "), this), 0, 3, 1, 1);
    multiDayGrid->addWidget(new QLabel(tr(" * R<sub>g</sub> + "), this), 0, 5, 1, 1);
    multiDayGrid->addWidget(new QLabel(tr(" * U + "), this), 0, 7, 1, 1);
    multiDayGrid->addWidget(new QLabel(tr("Top :"), this), 1, 0, 1, 1, Qt::AlignRight);
    multiDayGrid->addWidget(new QLabel(tr("T<sub>top</sub> - T<sub>a</sub> = "), this), 1, 1, 1, 1);
    multiDayGrid->addWidget(new QLabel(tr(" * T<sub>a</sub> + "), this), 1, 3, 1, 1);
    multiDayGrid->addWidget(new QLabel(tr(" * R<sub>g</sub> + "), this), 1, 5, 1, 1);
    multiDayGrid->addWidget(new QLabel(tr(" * U + "), this), 1, 7, 1, 1);
    multiDayGrid->addWidget(new QLabel(tr("Spar :"), this), 2, 0, 1, 1, Qt::AlignRight);
    multiDayGrid->addWidget(new QLabel(tr("T<sub>spar</sub> - T<sub>a</sub> = "), this), 2, 1, 1, 1);
    multiDayGrid->addWidget(new QLabel(tr(" * T<sub>a</sub> + "), this), 2, 3, 1, 1);
    multiDayGrid->addWidget(new QLabel(tr(" * R<sub>g</sub> + "), this), 2, 5, 1, 1);
    multiDayGrid->addWidget(new QLabel(tr(" * U + "), this), 2, 7, 1, 1);

    mDayBot1 = new CustomResetLineEdit;
    mDayBot1->setMaxLength(10);
    mDayBot1->setRegExp(floatingPointRegexp);
    mDayBot2 = new CustomResetLineEdit;
    mDayBot2->setMaxLength(10);
    mDayBot2->setRegExp(floatingPointRegexp);
    mDayBot3 = new CustomResetLineEdit;
    mDayBot3->setMaxLength(10);
    mDayBot3->setRegExp(floatingPointRegexp);
    mDayBot4 = new CustomResetLineEdit;
    mDayBot4->setMaxLength(10);
    mDayBot4->setRegExp(floatingPointRegexp);
    mDayTop1 = new CustomResetLineEdit;
    mDayTop1->setMaxLength(10);
    mDayTop1->setRegExp(floatingPointRegexp);
    mDayTop2 = new CustomResetLineEdit;
    mDayTop2->setMaxLength(10);
    mDayTop2->setRegExp(floatingPointRegexp);
    mDayTop3 = new CustomResetLineEdit;
    mDayTop3->setMaxLength(10);
    mDayTop3->setRegExp(floatingPointRegexp);
    mDayTop4 = new CustomResetLineEdit;
    mDayTop4->setMaxLength(10);
    mDayTop4->setRegExp(floatingPointRegexp);
    mDaySpar1 = new CustomResetLineEdit;
    mDaySpar1->setMaxLength(10);
    mDaySpar1->setRegExp(floatingPointRegexp);
    mDaySpar2 = new CustomResetLineEdit;
    mDaySpar2->setMaxLength(10);
    mDaySpar2->setRegExp(floatingPointRegexp);
    mDaySpar3 = new CustomResetLineEdit;
    mDaySpar3->setMaxLength(10);
    mDaySpar3->setRegExp(floatingPointRegexp);
    mDaySpar4 = new CustomResetLineEdit;
    mDaySpar4->setMaxLength(10);
    mDaySpar4->setRegExp(floatingPointRegexp);

    multiDayGrid->addWidget(mDayBot1, 0, 2, 1, 1);
    multiDayGrid->addWidget(mDayBot2, 0, 4, 1, 1);
    multiDayGrid->addWidget(mDayBot3, 0, 6, 1, 1);
    multiDayGrid->addWidget(mDayBot4, 0, 8, 1, 1);
    multiDayGrid->addWidget(mDayTop1, 1, 2, 1, 1);
    multiDayGrid->addWidget(mDayTop2, 1, 4, 1, 1);
    multiDayGrid->addWidget(mDayTop3, 1, 6, 1, 1);
    multiDayGrid->addWidget(mDayTop4, 1, 8, 1, 1);
    multiDayGrid->addWidget(mDaySpar1, 2, 2, 1, 1);
    multiDayGrid->addWidget(mDaySpar2, 2, 4, 1, 1);
    multiDayGrid->addWidget(mDaySpar3, 2, 6, 1, 1);
    multiDayGrid->addWidget(mDaySpar4, 2, 8, 1, 1);
    multiDayGrid->setColumnMinimumWidth(2, 75);
    multiDayGrid->setColumnMinimumWidth(4, 75);
    multiDayGrid->setColumnMinimumWidth(6, 75);
    multiDayGrid->setColumnMinimumWidth(8, 75);
    multiDayGrid->setColumnStretch(9, 1);

    multiNightGrid->addWidget(new QLabel(tr("Bottom :"), this), 0, 0, 1, 1, Qt::AlignRight);
    multiNightGrid->addWidget(new QLabel(tr("T<sub>bot</sub> - T<sub>a</sub> = "), this), 0, 1, 1, 1);
    multiNightGrid->addWidget(new QLabel(tr(" * T<sub>a</sub> + "), this), 0, 3, 1, 1);
    multiNightGrid->addWidget(new QLabel(tr(" * LWin + "), this), 0, 5, 1, 1);
    multiNightGrid->addWidget(new QLabel(tr(" * U + "), this), 0, 7, 1, 1);
    multiNightGrid->addWidget(new QLabel(tr("Top :"), this), 1, 0, 1, 1, Qt::AlignRight);
    multiNightGrid->addWidget(new QLabel(tr("T<sub>top</sub> - T<sub>a</sub> = "), this), 1, 1, 1, 1);
    multiNightGrid->addWidget(new QLabel(tr(" * T<sub>a</sub> + "), this), 1, 3, 1, 1);
    multiNightGrid->addWidget(new QLabel(tr(" * LWin + "), this), 1, 5, 1, 1);
    multiNightGrid->addWidget(new QLabel(tr(" * U + "), this), 1, 7, 1, 1);
    multiNightGrid->addWidget(new QLabel(tr("Spar :"), this), 2, 0, 1, 1, Qt::AlignRight);
    multiNightGrid->addWidget(new QLabel(tr("T<sub>spar</sub> - T<sub>a</sub> = "), this), 2, 1, 1, 1);
    multiNightGrid->addWidget(new QLabel(tr(" * T<sub>a</sub> + "), this), 2, 3, 1, 1);
    multiNightGrid->addWidget(new QLabel(tr(" * LWin + "), this), 2, 5, 1, 1);
    multiNightGrid->addWidget(new QLabel(tr(" * U + "), this), 2, 7, 1, 1);

    mNightBot1 = new CustomResetLineEdit;
    mNightBot1->setMaxLength(10);
    mNightBot1->setRegExp(floatingPointRegexp);
    mNightBot2 = new CustomResetLineEdit;
    mNightBot2->setMaxLength(10);
    mNightBot2->setRegExp(floatingPointRegexp);
    mNightBot3 = new CustomResetLineEdit;
    mNightBot3->setMaxLength(10);
    mNightBot3->setRegExp(floatingPointRegexp);
    mNightBot4 = new CustomResetLineEdit;
    mNightBot4->setMaxLength(10);
    mNightBot4->setRegExp(floatingPointRegexp);
    mNightTop1 = new CustomResetLineEdit;
    mNightTop1->setMaxLength(10);
    mNightTop1->setRegExp(floatingPointRegexp);
    mNightTop2 = new CustomResetLineEdit;
    mNightTop2->setMaxLength(10);
    mNightTop2->setRegExp(floatingPointRegexp);
    mNightTop3 = new CustomResetLineEdit;
    mNightTop3->setMaxLength(10);
    mNightTop3->setRegExp(floatingPointRegexp);
    mNightTop4 = new CustomResetLineEdit;
    mNightTop4->setMaxLength(10);
    mNightTop4->setRegExp(floatingPointRegexp);
    mNightSpar1 = new CustomResetLineEdit;
    mNightSpar1->setMaxLength(10);
    mNightSpar1->setRegExp(floatingPointRegexp);
    mNightSpar2 = new CustomResetLineEdit;
    mNightSpar2->setMaxLength(10);
    mNightSpar2->setRegExp(floatingPointRegexp);
    mNightSpar3 = new CustomResetLineEdit;
    mNightSpar3->setMaxLength(10);
    mNightSpar3->setRegExp(floatingPointRegexp);
    mNightSpar4 = new CustomResetLineEdit;
    mNightSpar4->setMaxLength(10);
    mNightSpar4->setRegExp(floatingPointRegexp);

    multiNightGrid->addWidget(mNightBot1, 0, 2, 1, 1);
    multiNightGrid->addWidget(mNightBot2, 0, 4, 1, 1);
    multiNightGrid->addWidget(mNightBot3, 0, 6, 1, 1);
    multiNightGrid->addWidget(mNightBot4, 0, 8, 1, 1);
    multiNightGrid->addWidget(mNightTop1, 1, 2, 1, 1);
    multiNightGrid->addWidget(mNightTop2, 1, 4, 1, 1);
    multiNightGrid->addWidget(mNightTop3, 1, 6, 1, 1);
    multiNightGrid->addWidget(mNightTop4, 1, 8, 1, 1);
    multiNightGrid->addWidget(mNightSpar1, 2, 2, 1, 1);
    multiNightGrid->addWidget(mNightSpar2, 2, 4, 1, 1);
    multiNightGrid->addWidget(mNightSpar3, 2, 6, 1, 1);
    multiNightGrid->addWidget(mNightSpar4, 2, 8, 1, 1);
    multiNightGrid->setColumnMinimumWidth(2, 75);
    multiNightGrid->setColumnMinimumWidth(4, 75);
    multiNightGrid->setColumnMinimumWidth(6, 75);
    multiNightGrid->setColumnMinimumWidth(8, 75);
    multiNightGrid->setColumnStretch(9, 1);

    burbaSimpleDay->setLayout(simpleDayGrid);
    burbaSimpleNight->setLayout(simpleNightGrid);
    burbaMultiDay->setLayout(multiDayGrid);
    burbaMultiNight->setLayout(multiNightGrid);

    connect(lDayBotGain, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenLDayBotGain(s.toDouble()); });
    connect(lDayBotOffset, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenLDayBotOffset(s.toDouble()); });
    connect(lDayTopGain, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenLDayTopGain(s.toDouble()); });
    connect(lDayTopOffset, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenLDayTopOffset(s.toDouble()); });
    connect(lDaySparGain, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenLDaySparGain(s.toDouble()); });
    connect(lDaySparOffset, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenLDaySparOffset(s.toDouble()); });

    connect(lNightBotGain, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenLNightBotGain(s.toDouble()); });
    connect(lNightBotOffset, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenLNightBotOffset(s.toDouble()); });
    connect(lNightTopGain, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenLNightTopGain(s.toDouble()); });
    connect(lNightTopOffset, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenLNightTopOffset(s.toDouble()); });
    connect(lNightSparGain, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenLNightSparGain(s.toDouble()); });
    connect(lNightSparOffset, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenLNightSparOffset(s.toDouble()); });

    connect(mDayBot1, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenMDayBot1(s.toDouble()); });
    connect(mDayBot2, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenMDayBot2(s.toDouble()); });
    connect(mDayBot3, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenMDayBot3(s.toDouble()); });
    connect(mDayBot4, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenMDayBot4(s.toDouble()); });

    connect(mDayTop1, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenMDayTop1(s.toDouble()); });
    connect(mDayTop2, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenMDayTop2(s.toDouble()); });
    connect(mDayTop3, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenMDayTop3(s.toDouble()); });
    connect(mDayTop4, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenMDayTop4(s.toDouble()); });

    connect(mDaySpar1, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenMDaySpar1(s.toDouble()); });
    connect(mDaySpar2, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenMDaySpar2(s.toDouble()); });
    connect(mDaySpar3, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenMDaySpar3(s.toDouble()); });
    connect(mDaySpar4, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenMDaySpar4(s.toDouble()); });

    connect(mNightBot1, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenMNightBot1(s.toDouble()); });
    connect(mNightBot2, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenMNightBot2(s.toDouble()); });
    connect(mNightBot3, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenMNightBot3(s.toDouble()); });
    connect(mNightBot4, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenMNightBot4(s.toDouble()); });

    connect(mNightTop1, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenMNightTop1(s.toDouble()); });
    connect(mNightTop2, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenMNightTop2(s.toDouble()); });
    connect(mNightTop3, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenMNightTop3(s.toDouble()); });
    connect(mNightTop4, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenMNightTop4(s.toDouble()); });

    connect(mNightSpar1, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenMNightSpar1(s.toDouble()); });
    connect(mNightSpar2, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenMNightSpar2(s.toDouble()); });
    connect(mNightSpar3, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenMNightSpar3(s.toDouble()); });
    connect(mNightSpar4, &CustomResetLineEdit::textChanged, [=](const QString &s)
            { ecProject_->setScreenMNightSpar4(s.toDouble()); });
}

void AdvProcessingOptions::setBurbaDefaultValues()
{
    // init
    lDayBotGain->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.l_day_bot_gain, 'f', 3));
    lDayBotOffset->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.l_day_bot_offset, 'f', 2));
    lDayTopGain->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.l_day_top_gain, 'f', 3));
    lDayTopOffset->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.l_day_top_offset, 'f', 2));
    lDaySparGain->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.l_day_spar_gain, 'f', 3));
    lDaySparOffset->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.l_day_spar_offset, 'f', 2));

    lNightBotGain->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.l_night_bot_gain, 'f', 3));
    lNightBotOffset->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.l_night_bot_offset, 'f', 2));
    lNightTopGain->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.l_night_top_gain, 'f', 3));
    lNightTopOffset->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.l_night_top_offset, 'f', 2));
    lNightSparGain->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.l_night_spar_gain, 'f', 3));
    lNightSparOffset->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.l_night_spar_offset, 'f', 2));

    mDayBot1->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.m_day_bot1, 'f', 1));
    mDayBot2->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.m_day_bot2, 'f', 4));
    mDayBot3->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.m_day_bot3, 'f', 4));
    mDayBot4->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.m_day_bot4, 'f', 3));
    mDayTop1->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.m_day_top1, 'f', 1));
    mDayTop2->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.m_day_top2, 'f', 4));
    mDayTop3->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.m_day_top3, 'f', 4));
    mDayTop4->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.m_day_top4, 'f', 3));
    mDaySpar1->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.m_day_spar1, 'f', 1));
    mDaySpar2->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.m_day_spar2, 'f', 4));
    mDaySpar3->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.m_day_spar3, 'f', 4));
    mDaySpar4->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.m_day_spar4, 'f', 3));

    mNightBot1->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.m_night_bot1, 'f', 1));
    mNightBot2->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.m_night_bot2, 'f', 4));
    mNightBot3->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.m_night_bot3, 'f', 4));
    mNightBot4->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.m_night_bot4, 'f', 3));
    mNightTop1->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.m_night_top1, 'f', 1));
    mNightTop2->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.m_night_top2, 'f', 4));
    mNightTop3->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.m_night_top3, 'f', 4));
    mNightTop4->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.m_night_top4, 'f', 3));
    mNightSpar1->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.m_night_spar1, 'f', 1));
    mNightSpar2->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.m_night_spar2, 'f', 4));
    mNightSpar3->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.m_night_spar3, 'f', 4));
    mNightSpar4->setDefaultText(QString::number(ecProject_->defaultSettings.screenSetting.m_night_spar4, 'f', 3));
}

void AdvProcessingOptions::on_setDefaultsButton_clicked()
{
    if (requestBurbaSettingsReset())
    {
        setBurbaDefaultValues();
    }
}

void AdvProcessingOptions::updateWplMeth_1(bool b)
{
    ecProject_->setGeneralWplMeth(b);
}

void AdvProcessingOptions::enableBurbaCorrectionArea(bool b)
{
    burbaTypeLabel->setEnabled(b);
    burbaSimpleRadio->setEnabled(b);
    burbaMultiRadio->setEnabled(b);
    //> The stack, not the tab widgets inside it: disabling a parent already
    //> disables its children, and naming only one of the two tab widgets left
    //> the other relying on that cascade anyway.
    burbaParamWidget->setEnabled(b);
    setDefaultsButton->setEnabled(b);
}

void AdvProcessingOptions::updateBurbaType_2(int n)
{
    ecProject_->setScreenBuMulti(n);
    burbaParamWidget->setCurrentIndex(n);
}

// update when click "Compensate density fluctuations" checkbox
void AdvProcessingOptions::updateBurbaGroup(bool b)
{
    burbaCorrCheckBox->setEnabled(b);
    enableBurbaCorrectionArea(b && burbaCorrCheckBox->isChecked());
    //> After the WPL gate, not before: this can only ever take the box further
    //> off, and it must have the last word on whether it is enabled.
    updateBurbaAvailability();
}

/// Burba terms need an LI-7500 family analyser, and the engine says so itself:
/// OverrideSettings() forces bu_corr to 'none' when no gas column names one, so
/// a project that ticks this box without one is asking for a correction that is
/// then silently dropped. Greyed here instead, and the setting cleared with it,
/// so the interface and the run agree on what is going to happen.
///
/// Deliberately no SmartFlux veto, unlike updateCecAvailability() above. The
/// engine clears bu_corr in exactly three places - configure_for_express.f90,
/// configure_for_md_retrieval.f90 and the LI-7500 test in
/// override_settings.f90 - and none of them is the embedded run environment.
/// SmartFlux is also the case where the correction matters most, being paired
/// with the open-path head it describes.
void AdvProcessingOptions::updateBurbaAvailability()
{
    const auto hasLi7500 = hasLi7500FamilyIrga();

    //> The density correction still gates it: an unavailable Burba stays
    //> unavailable, but an available one is only offered while WPL is on.
    const auto enabled = hasLi7500 && wplCheckBox->isChecked();

    burbaCorrCheckBox->setEnabled(enabled);
    burbaCorrCheckBox->setToolTip(hasLi7500
        ? burbaAvailableTooltip_
        : tr("<b>Add instrument sensible heat components, only for LI-7500:</b> "
             "Unavailable. The correction describes the heat exchange at the "
             "surfaces of an LI-7500 open-path head, so it applies only "
             "to that family of analyzers - and none is configured in the "
             "metadata. Add one in the Metadata File Editor to enable it."));

    if (!hasLi7500)
    {
        QSignalBlocker blocker(burbaCorrCheckBox);
        burbaCorrCheckBox->setChecked(false);
        //> Only when it actually says otherwise. The setter marks the project
        //> modified unconditionally, and this runs on every metadata read - so
        //> writing the 0 that is already there would mark a project dirty for
        //> no reason other than having been opened.
        if (ecProject_->screenBuCorr() != 0)
        {
            ecProject_->setScreenBuCorr(0);
        }
        enableBurbaCorrectionArea(false);
        return;
    }

    enableBurbaCorrectionArea(enabled && burbaCorrCheckBox->isChecked());
}

/// Asked of every analyser the metadata describes, which is the same set the
/// engine walks. The walk itself lives in IrgaDesc, so this page and the
/// spectral page cannot answer it differently.
bool AdvProcessingOptions::hasLi7500FamilyIrga() const
{
    if (!dlProject_) { return false; }

    return IrgaDesc::hasLi7500Family(dlProject_->irgas());
}

void AdvProcessingOptions::createQuestionMark()
{
    questionMark_1 = new QPushButton;
    questionMark_1->setObjectName(QStringLiteral("questionMarkImg"));
    questionMark_1->setFlat(true);
    questionMark_1->setIcon(QIcon(QStringLiteral(":/icons/qm-enabled")));
    questionMark_1->setIconSize(QSize(12, 12));
    questionMark_4 = new QPushButton;
    questionMark_4->setObjectName(QStringLiteral("questionMarkImg"));
    questionMark_4->setFlat(true);
    questionMark_4->setIcon(QIcon(QStringLiteral(":/icons/qm-enabled")));
    questionMark_4->setIconSize(QSize(12, 12));
    questionMark_11 = new QPushButton;
    questionMark_11->setObjectName(QStringLiteral("questionMarkImg"));
    questionMark_11->setFlat(true);
    questionMark_11->setIcon(QIcon(QStringLiteral(":/icons/qm-enabled")));
    questionMark_11->setIconSize(QSize(12, 12));

    connect(questionMark_1, &QPushButton::clicked,
            this, &AdvProcessingOptions::onlineHelpTrigger_1);
    connect(questionMark_4, &QPushButton::clicked,
            this, &AdvProcessingOptions::onlineHelpTrigger_4);
    connect(questionMark_11, &QPushButton::clicked,
            this, &AdvProcessingOptions::onlineHelpTrigger_11);
}

void AdvProcessingOptions::onlineHelpTrigger_1()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Wind_Speed_Offsets.html")));
}

void AdvProcessingOptions::onlineHelpTrigger_4()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Calculate_Turbulent_Flux.html")));
}

void AdvProcessingOptions::onlineHelpTrigger_11()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Raw_Processing_Options.html")));
}

void AdvProcessingOptions::updateTooltip(int i)
{
    auto senderCombo = qobject_cast<QComboBox *>(sender());
    WidgetUtils::updateComboItemTooltip(senderCombo, i);
}

bool AdvProcessingOptions::requestBurbaSettingsReset()
{
    return WidgetUtils::yesNoQuestion(this,
                tr("Reset Surface Heating Correction"),
                tr("<p>Do you want to reset the surface heating correction "
                   "to the default values of Burba et al. (2008)?</p>"),
                tr("<p>You cannot undo this action.</p>"));
}


