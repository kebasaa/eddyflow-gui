/***************************************************************************
  advstatisticaloptions.cpp
  -------------------
  Copyright © 2007-2011,  Eco2s team, Antonio Forgione
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

#include "advstatisticaloptions.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QGridLayout>
#include <QIcon>
#include <QPixmap>
#include <QSize>
#include <QPushButton>
#include <QRadioButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSpinBox>
#include <QStringList>
#include <QTimer>
#include <QToolBox>
#include <QUrl>

#include "clicklabel.h"
#include "detlimsettingsdialog.h"
#include "dlproject.h"
#include "ecproject.h"
#include "measurement_record.h"
#include "variable_desc.h"
#include "splitter.h"
#include "widget_utils.h"

AdvStatisticalOptions::AdvStatisticalOptions(QWidget *parent,
                                             DlProject *dlProject,
                                             EcProject *ecProject) :
    QWidget(parent),
    dlProject_(dlProject),
    ecProject_(ecProject)
{
    selectAllCheckBox = new QCheckBox(tr("&Select all tests"));
    defaultValuesButton = new QPushButton(tr("Restore Default Values"));
    defaultValuesButton->setProperty("mdButton", true);
    defaultValuesButton->setMaximumWidth(defaultValuesButton->sizeHint().width());
    defaultValuesButton->setStyleSheet(QStringLiteral("QPushButton {margin-top: 15px;}"));
    defaultValuesButton->setToolTip(tr("<b>Restore Default Values</b>: Resets the Statistical Analysis settings to the default settings."));

    spikeRemCheckBox = new QCheckBox(tr("Spike count/removal"));
    spikeRemCheckBox->setToolTip(tr("<b>Spike count/removal:</b> Detects the number of spikes in each time series. Use the checkbox <b><i>Replace spikes with linear interpolation</i></b> to replace detected spikes with linear interpolation of neighboring values. Configure the definition of spikes and the flagging policy in the corresponding section of the right-side panel."));
    amplitudeResCheckBox = new QCheckBox(tr("Amplitude resolution"));
    amplitudeResCheckBox->setToolTip(tr("<b>Amplitude resolution:</b> Detects situations in which the signal variance is too low with respect to instrumental resolution. Configure the assessment procedure and the flagging policy in the corresponding section of the right-side panel."));
    dropoutsCheckBox = new QCheckBox(tr("Drop-outs"));
    dropoutsCheckBox->setToolTip(tr("<b>Drop-outs:</b> Detects relatively short periods in which time series stick to some value which is statistically different from the average value calculated over the whole period. Configure the assessment procedure and the flagging policy in the corresponding section of the right-side panel."));
    absLimCheckBox = new QCheckBox(tr("Absolute limits"));
    absLimCheckBox->setToolTip(tr("<b>Absolute limits:</b> Assesses whether each variable attains, at least once in the current time series, a value that is outside a user-defined plausible range. In this case, the variable is flagged. The test is performed after the despiking procedure. Thus, each outranged value found here is not a spike, it will remain in the time series and affect calculated statistics, including fluxes. Check <b><i>Filter outranged values</i></b> to eliminate such outliers."));
    skewnessCheckBox = new QCheckBox(tr("Skewness && kurtosis"));
    skewnessCheckBox->setToolTip(tr("<b>Skewness & Kurtosis:</b> Third and fourth order moments are calculated on the whole time series and variables are flagged if their values exceed certain thresholds that you can customize in the corresponding section of the right-side panel."));
    discontCheckBox = new QCheckBox(tr("Discontinuities"));
    discontCheckBox->setToolTip(tr("<b>Discontinuities:</b> Detect discontinuities that lead to semi-permanent changes, as opposed to sharp changes associated with smaller-scale fluctuations. Configure the assessment of discontinuities in the corresponding section of the right-side panel."));
    timeLagCheckBox = new QCheckBox(tr("Time lags"));
    timeLagCheckBox->setToolTip(tr("<b>Time lags:</b> This test flags scalar time series if the maximal <i>w</i>-covariances, determined via the covariance maximization procedure and evaluated over a predefined time-lag window, are too different from those calculated for the user-suggested time lags. Configure the expected time lags and the accepted discrepancies in the corresponding section of the right-side panel."));
    attackAngleCheckBox = new QCheckBox(tr("Angle of attack"));
    attackAngleCheckBox->setToolTip(tr("<b>Angle of attack:</b> Calculates sample-wise angle of attacks throughout the current flux averaging period, and flags it if the percentage of angles of attack exceeding a user-defined range is beyond a threshold that you can set on the right-side panel."));
    nonSteadyCheckBox = new QCheckBox(tr("Steadiness of horizontal wind"));
    nonSteadyCheckBox->setToolTip(tr("<b>Steadiness of horizontal wind:</b> Assesses whether the along-wind and cross-wind components of the wind vector undergo a systematic reduction/increase throughout the file. If the quadratic combination of such systematic variations is beyond the user-selected limit, the flux-averaging period is hard-flagged for non stationary horizontal wind."));
    rfluxDiagCheckBox = new QCheckBox(tr("Extra raw-signal diagnostics (RFlux)"));
    rfluxDiagCheckBox->setToolTip(tr("<b>Extra raw-signal diagnostics (adapted from RFlux):</b> Computes additional per-variable diagnostics beyond the tests above - lag-1 autocorrelation (AL1), the discrete/dominant-value test (DDI), spike counts on the raw series and its first differences scaled by a robust median/MAD estimator (HF5, HF10, HD5, HD10), and Hartigan's dip test p-value for multimodality (DIP, Hartigan & Hartigan 1985). These are informational only: nothing here hard-flags a period or feeds back into flux computation. Off by default; enabling it adds columns to the full and FLUXNET output."));

    postFluxDespikeCheckBox = new QCheckBox(tr("Post-flux despiking (STL)"));
    postFluxDespikeCheckBox->setToolTip(tr("<b>Post-flux despiking (adapted from RFlux):</b> After the run finishes, decomposes each of NEE, H and LE into seasonal, trend and remainder components (STL, Cleveland et al. 1990) and flags remainder-component outliers with a Laplace-distribution test. This looks for outliers in the <i>computed half-hourly flux series</i>, not in the raw high-frequency data, so it catches spikes the raw-data tests above cannot see. Needs at least 20 periods and 4 periods/day to run; shorter or coarser runs are skipped. Off by default; enabling it writes a separate <tt>..._flux_despiking...csv</tt> file and does not change any existing output."));

    auto hrLabel_1 = new QLabel;
    hrLabel_1->setObjectName(QStringLiteral("hrLabel"));

    auto hrLabel_2 = new QLabel;
    hrLabel_2->setObjectName(QStringLiteral("hrLabel"));

    thumbnailGraphLabel = new QLabel;

    auto testSelectionLayout = new QGridLayout;
    testSelectionLayout->addWidget(spikeRemCheckBox, 0, 0);
    testSelectionLayout->addWidget(amplitudeResCheckBox, 1, 0);
    testSelectionLayout->addWidget(dropoutsCheckBox, 2, 0);
    testSelectionLayout->addWidget(absLimCheckBox, 3, 0);
    testSelectionLayout->addWidget(skewnessCheckBox, 4, 0);
    testSelectionLayout->addWidget(discontCheckBox, 5, 0);
    testSelectionLayout->addWidget(timeLagCheckBox, 6, 0);
    testSelectionLayout->addWidget(attackAngleCheckBox, 7, 0);
    testSelectionLayout->addWidget(nonSteadyCheckBox, 8, 0);
    testSelectionLayout->addWidget(rfluxDiagCheckBox, 9, 0);
    testSelectionLayout->addWidget(postFluxDespikeCheckBox, 10, 0);
    testSelectionLayout->addWidget(hrLabel_1, 11, 0, 1, -1);
    testSelectionLayout->addWidget(selectAllCheckBox, 12, 0);
    testSelectionLayout->addWidget(defaultValuesButton, 13, 0);
    testSelectionLayout->addWidget(hrLabel_2, 14, 0, 1, -1);
    testSelectionLayout->addWidget(thumbnailGraphLabel, 15, 0);
    testSelectionLayout->setContentsMargins(10, 10, 10, 10);
    testSelectionLayout->setRowStretch(16, 1);
    testSelectionLayout->setColumnMinimumWidth(0, 185);

    createQuestionMark();

    createTabWidget();

    auto testSelectionAllLayout = new QGridLayout;
    testSelectionAllLayout->addLayout(testSelectionLayout, 0, 0);
    testSelectionAllLayout->addWidget(testToolbox, 0, 1, -1, 1);
    testSelectionAllLayout->setContentsMargins(0, 0, 0, 15);

    auto testSelectionGroupTitle = new QLabel(tr("Statistical tests for raw data screening, Vickers and Mahrt (1997)"));
    testSelectionGroupTitle->setProperty("groupTitle2", true);
    testSelectionGroupTitle->setToolTip(tr("<b>Statistical tests for raw data screening:</b> Select (on the left side) and configure (on the right side) up to 9 tests for assessing statistical quality of raw time series. Use the results of these tests to filter out results, for which flags are turned on. All tests are implemented after Vickers and Mahrt (1997). See the original publication for more details and how to interpret results."));

    auto qBox_1 = new QHBoxLayout;
    qBox_1->addWidget(testSelectionGroupTitle);
    qBox_1->addWidget(questionMark_1, 0, Qt::AlignLeft | Qt::AlignCenter);
    qBox_1->addStretch();
    qBox_1->setContentsMargins(0, 0, 0, 15);

    auto randomErrorGroupTitle = new QLabel(tr("Estimation of flux random uncertainty due to sampling error"));
    randomErrorGroupTitle->setProperty("groupTitle2", true);
    randomErrorGroupTitle->setToolTip(tr("Estimation of flux random uncertainty due to sampling error"));

    auto qBox_2 = new QHBoxLayout;
    qBox_2->addWidget(randomErrorGroupTitle);
    qBox_2->addWidget(questionMark_11, 0, Qt::AlignLeft | Qt::AlignVCenter);
    qBox_2->addStretch();
    qBox_2->setContentsMargins(0, 0, 0, 15);

//    auto upFrame = new QWidget;
//    upFrame->setLayout(testSelectionAllLayout);
//    upFrame->setProperty("scrollContainerWidget", true);

//    auto upScrollArea = new QScrollArea;
//    upScrollArea->setWidget(upFrame);
//    upScrollArea->setWidgetResizable(true);

    randomErrorCheckBox = new QCheckBox(tr("Random uncertainty estimation"));
    randomErrorCheckBox->setToolTip(tr("<b>Random uncertainty estimation:</b> Check this box to instruct EddyFlow to calculate flux random uncertainty due to sampling error. Select the preferred method and adjust settings to adapt to the specificity of your site/setup."));
    randomErrorCheckBox->setProperty("paddedCheckbox", true);

    randomMethodLabel = new ClickLabel(tr("Method :"));
    randomMethodLabel->setToolTip(tr("<b>Method:</b> Select the uncertainty computation method."));
    randomMethodCombo = new QComboBox;
    randomMethodCombo->addItem(tr("Finkelstein and Sims (2001)"));
    randomMethodCombo->addItem(tr("Mann and Lenschow (1994)"));
    randomMethodCombo->addItem(tr("Billesbach (2011)"));
    randomMethodCombo->addItem(tr("Lenschow et al. (2000) - instrument noise"));
    randomMethodCombo->setItemData(0, tr("<b>Finkelstein and Sims (2001):</b> Based on a mathematically rigorous expression for the variance of a covariance, which includes the auto- and cross-covariance terms for atmospheric fluxes. The uncertainty estimate is based on Eqs. 8-10 of the referenced paper."), Qt::ToolTipRole);
    randomMethodCombo->setItemData(1, tr("<b>Mann and Lenschow (1994):</b> Define the error variance of the central moment of the time series. The uncertainty estimate is based on, e.g. Eqs. 5 of Finkelstein and Sims (2001)."), Qt::ToolTipRole);
    randomMethodCombo->setItemData(2, tr("<b>Billesbach (2011):</b> The \"random shuffle\" method. Reordering a scalar at random destroys every real correlation with vertical wind, so whatever covariance survives the shuffle was produced by noise alone; the mean of twenty such covariances is a floor below which a flux cannot be told from zero. This is a <b>noise floor, not a sampling error</b>: it answers whether a flux is resolvable, not how uncertain it is, and it is systematically smaller than the two methods above. Most useful for weak-flux species such as carbonyl sulfide or nitrous oxide."), Qt::ToolTipRole);
    //> The stored value, not the row. These used to be read as index + 1,
    //> which worked while the menu held exactly the first two methods and
    //> stopped working the moment a third was added: ru_meth 3 is Mahrt
    //> (computed elsewhere and not offered here), so Billesbach is 4 and the
    //> row it sits on says nothing about it.
    randomMethodCombo->setItemData(3, tr("<b>Lenschow et al. (2000), as applied by Mauder et al. (2013):</b> White instrument noise is uncorrelated between samples, so it lands "
        "entirely in the autocovariance at zero lag and nowhere else. Fit a line through the first five lags, read it back at zero, and the "
        "gap between that line and the measured value is the noise variance."
        "<br><br>This estimates the <b>analyser's own noise</b>, and is the smallest of the four: it says nothing about whether the "
        "atmosphere was sampled long enough, which is what the two sampling errors above measure, and nothing about whether a flux is "
        "resolvable, which is what Billesbach measures. The three are not interchangeable."
        "<br><br>It <b>declines periods</b> rather than guessing: if either intercept comes out non-positive the assumption the method "
        "rests on does not hold for that period, and the estimate is reported as missing. On a low-noise analyser that can be most of "
        "them - which is itself the useful answer."
        "<br><br>The five-lag window is fixed in samples, as EddyUH has it, so it spans half as long at 20 Hz as at 10."), Qt::ToolTipRole);

    //> The stored value IS ru_meth. Five, not four, because four is
    //> already Billesbach - the row and the method number have not run
    //> in step since three became Mahrt.
    randomMethodCombo->setItemData(0, 1);
    randomMethodCombo->setItemData(1, 2);
    randomMethodCombo->setItemData(2, 4);
    randomMethodCombo->setItemData(3, 5);

    auto itsLabel = WidgetUtils::createBlueLabel(this, tr("Integral turbulence scale (ITS)"));
    itsLabel->setToolTip(tr(""));

    itsDefinitionLabel = new ClickLabel(tr("Definition of the ITS :"));
    itsDefinitionLabel->setToolTip(tr("<b>Definition of the ITS:</b> The Integral Turbulence (time-) Scale is defined by the integral of the cross-correlation function, between zero and infinite lag time. However, because the cross-correlation function, starting always at 1 for zero lag time, decreases more or less rapidly towards values close to zero (non-correlation for large lag times) the infinite integral can safely be approximated by a finite one. Here you can select the criterion to define the upper integration limit."));
    itsDefinitionCombo = new QComboBox;
    itsDefinitionCombo->addItem(tr("Cross-correlation first crossing 1/e"));
    itsDefinitionCombo->addItem(tr("Cross-correlation first crossing zero"));
    itsDefinitionCombo->addItem(tr("Integrate over the whole correlation period"));
    itsDefinitionCombo->setItemData(0, tr("<b>Cross-correlation first crossing 1/e:</b> The integration will be halted when the cross-correlation function for the first time attains a value of 1/e (note that the cross-correlation function always start at 1 for zero lag time)."), Qt::ToolTipRole);
    itsDefinitionCombo->setItemData(1, tr("<b>Cross-correlation first crossing 0:</b> The integration will be halted when the cross-correlation function crosses the <i>x</i>-axis for the first time. It may happen that the function never cross the <i>x</i>-axis in the selected lag time range. In this case, the integration will be performed over the whole correlation period defined below."), Qt::ToolTipRole);
    itsDefinitionCombo->setItemData(2, tr("<b>Integrate over the whole correlation period:</b> The integration will be performed from lag time equal to zero up to the maximum lag time defined by <b><i>Maximum correlation period</i></b>."), Qt::ToolTipRole);

    timelagMaxLabel = new ClickLabel(tr("Maximum correlation period :"));
    timelagMaxLabel->setToolTip(tr("<b>Maximum correlation period:</b> Define here the expected maximum cross-correlation period in seconds. Beyond the maximum correlation period, you expect variables (vertical wind component and gas concentrations) to be virtually uncorrelated."));
    timelagMaxSpin = new QDoubleSpinBox;
    timelagMaxSpin->setDecimals(1);
    timelagMaxSpin->setRange(0.0, 10000.0);
    timelagMaxSpin->setSingleStep(1.0);
    timelagMaxSpin->setAccelerated(true);
    timelagMaxSpin->setSuffix(tr("  [s]"));
    timelagMaxSpin->setToolTip(timelagMaxLabel->toolTip());

    // NOTE: temporarly disabled
    securityCoeffLabel = new ClickLabel(tr("Security coefficient :"));
    securityCoeffLabel->setToolTip(tr("<b>Security coefficient :</b>."));
    securityCoeffLabel->setVisible(false);
    securityCoeffSpin = new QDoubleSpinBox;
    securityCoeffSpin->setDecimals(1);
    securityCoeffSpin->setRange(1.0, 100.0);
    securityCoeffSpin->setSingleStep(1.0);
    securityCoeffSpin->setAccelerated(true);
    securityCoeffSpin->setToolTip(securityCoeffLabel->toolTip());
    securityCoeffSpin->setVisible(false);

    //> Flux detection limit. Independent of the random-error checkbox above
    //> it: the two answer different questions - how uncertain is this flux,
    //> against can this flux be told from zero - and either is useful without
    //> the other. So the button is always live, and the method lives in the
    //> dialog rather than a checkbox here.
    detlimSettingsButton = new QPushButton(tr("Flux Detection Limit..."));
    detlimSettingsButton->setToolTip(tr("<b>Flux detection limit:</b> "
        "Estimate the noise floor of each gas's covariance with vertical "
        "wind, after Wienhold et al. (1994), by measuring the scatter of the "
        "cross-covariance function away from its peak. A flux smaller than "
        "its detection limit cannot be distinguished from zero. Off by "
        "default; switching it on changes no flux, it only reports the limit "
        "alongside. Particularly worth having for weak-flux species such as "
        "carbonyl sulfide or nitrous oxide."));
    detlimSettingsButton->setProperty("mdButton", true);
    detlimSettingsButton->setMaximumWidth(detlimSettingsButton->sizeHint().width());

    auto downLayout = new QGridLayout;
    downLayout->addLayout(qBox_2, 0, 0, 1, 2);
    downLayout->addWidget(randomErrorCheckBox, 1, 0);
    downLayout->addWidget(randomMethodLabel, 1, 1, Qt::AlignRight);
    downLayout->addWidget(randomMethodCombo, 1, 2);
    downLayout->addWidget(itsLabel, 2, 1);
    downLayout->addWidget(itsDefinitionLabel, 3, 1, Qt::AlignRight);
    downLayout->addWidget(itsDefinitionCombo, 3, 2);
    downLayout->addWidget(timelagMaxLabel, 4, 1, Qt::AlignRight);
    downLayout->addWidget(timelagMaxSpin, 4, 2);
    downLayout->addWidget(securityCoeffLabel, 5, 1, Qt::AlignRight);
    downLayout->addWidget(securityCoeffSpin, 5, 2);
    //> Row 6, and the stretch moves down with it. The grid is hand-numbered,
    //> so anything added below has to push the stretch again.
    downLayout->addWidget(detlimSettingsButton, 6, 1, 1, 2, Qt::AlignLeft);
    downLayout->setRowStretch(7, 1);
    downLayout->setColumnStretch(3, 1);

    auto downFrame = new QWidget;
    downFrame->setLayout(downLayout);

    auto splitter = new Splitter(Qt::Vertical, this);
//    splitter->addWidget(upScrollArea);
    splitter->addWidget(WidgetUtils::getContainerScrollArea(this, testSelectionAllLayout));
    splitter->addWidget(downFrame);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);
    splitter->handle(1)->setToolTip(tr("Handle the separator."));

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(qBox_1);
    mainLayout->addWidget(splitter);
    mainLayout->setContentsMargins(15, 15, 15, 10);
    setLayout(mainLayout);

    connect(spikeRemCheckBox, &QCheckBox::toggled,
            this, &AdvStatisticalOptions::on_spikeRemCheckBox_clicked);
    connect(amplitudeResCheckBox, &QCheckBox::toggled,
            this, &AdvStatisticalOptions::on_amplitudeResCheckBox_clicked);
    connect(dropoutsCheckBox, &QCheckBox::toggled,
            this, &AdvStatisticalOptions::on_dropoutsCheckBox_clicked);
    connect(absLimCheckBox, &QCheckBox::toggled,
            this, &AdvStatisticalOptions::on_absLimCheckBox_clicked);
    connect(skewnessCheckBox, &QCheckBox::toggled,
            this, &AdvStatisticalOptions::on_skewnessCheckBox_clicked);
    connect(discontCheckBox, &QCheckBox::toggled,
            this, &AdvStatisticalOptions::on_discontCheckBox_clicked);
    connect(timeLagCheckBox, &QCheckBox::toggled,
            this, &AdvStatisticalOptions::on_timeLagCheckBox_clicked);
    connect(attackAngleCheckBox, &QCheckBox::toggled,
            this, &AdvStatisticalOptions::on_attackAngleCheckBox_clicked);
    connect(nonSteadyCheckBox, &QCheckBox::toggled,
            this, &AdvStatisticalOptions::on_nonSteadyCheckBox_clicked);

    auto checkbox_list = QWidgetList() << spikeRemCheckBox
                                       << amplitudeResCheckBox
                                       << dropoutsCheckBox
                                       << absLimCheckBox
                                       << skewnessCheckBox
                                       << discontCheckBox
                                       << timeLagCheckBox
                                       << attackAngleCheckBox
                                       << nonSteadyCheckBox;
    for (auto widget : checkbox_list)
    {
        auto checkbox = static_cast<QCheckBox *>(widget);
        connect(checkbox, &QCheckBox::toggled,
                this, &AdvStatisticalOptions::updateSelectAllCheckbox);
    }

    connect(selectAllCheckBox, &QCheckBox::toggled,
            defaultValuesButton, &QPushButton::setEnabled);
    connect(selectAllCheckBox, &QCheckBox::toggled,
            this, &AdvStatisticalOptions::selectAllTest);

    connect(defaultValuesButton, &QPushButton::clicked,
            this, &AdvStatisticalOptions::on_defaultValuesButton_clicked);

    connect(spikeRemCheckBox, &QCheckBox::toggled,
            this, &AdvStatisticalOptions::updateTestSr);
    connect(amplitudeResCheckBox, &QCheckBox::toggled,
            this, &AdvStatisticalOptions::updateTestAr);
    connect(dropoutsCheckBox, &QCheckBox::toggled,
            this, &AdvStatisticalOptions::updateTestDo);
    connect(absLimCheckBox, &QCheckBox::toggled,
            this, &AdvStatisticalOptions::updateTestAl);
    connect(skewnessCheckBox, &QCheckBox::toggled,
            this, &AdvStatisticalOptions::updateTestSk);
    connect(discontCheckBox, &QCheckBox::toggled,
            this, &AdvStatisticalOptions::updateTestDs);
    connect(timeLagCheckBox, &QCheckBox::toggled,
            this, &AdvStatisticalOptions::updateTestTl);
    connect(attackAngleCheckBox, &QCheckBox::toggled,
            this, &AdvStatisticalOptions::updateTestAa);
    connect(nonSteadyCheckBox, &QCheckBox::toggled,
            this, &AdvStatisticalOptions::updateTestNs);
    connect(rfluxDiagCheckBox, &QCheckBox::toggled,
            this, &AdvStatisticalOptions::updateTestRf);
    connect(postFluxDespikeCheckBox, &QCheckBox::toggled,
            this, [=](bool b) { ecProject_->setGeneralTestPfd(b ? 1 : 0); });

    connect(despikingRadioGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, &AdvStatisticalOptions::despikingRadioClicked);
    connect(stepSpin_u, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [=](double d) { ecProject_->setScreenParamSrStepU(d); });
    connect(stepSpin_v, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [=](double d) { ecProject_->setScreenParamSrStepV(d); });
    connect(stepSpin_w, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [=](double d) { ecProject_->setScreenParamSrStepW(d); });
    connect(stepSpin_ts, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [=](double d) { ecProject_->setScreenParamSrStepTs(d); });
    connect(despikingRadioGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, &AdvStatisticalOptions::updateDespikingMethod);
    connect(despSpin_1, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamSrNumSpk);
    connect(despSpin_8, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamSrHfLim);
    connect(despSpin_3, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamSrWLim);
    connect(despSpin_2, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamSrULim);
    connect(despFilterCheckBox, &QCheckBox::toggled,
            this, &AdvStatisticalOptions::updateDespFilter);

    connect(despLabel_1, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickDespLabel_1);
    connect(despLabel_2, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickDespLabel_2);
    connect(despLabel_3, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickDespLabel_3);
    connect(despLabel_8, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickDespLabel_8);

    connect(amplResLabel_1, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickAmplResLabel_1);
    connect(amplResLabel_2, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickAmplResLabel_2);
    connect(amplResLabel_3, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickAmplResLabel_3);

    connect(amplResSpin_1, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamArLim);
    connect(amplResSpin_2, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamArBins);
    connect(amplResSpin_3, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamArHfLim);

    connect(dropoutsLabel_1, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickDropoutsLabel_1);
    connect(dropoutsLabel_2, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickDropoutsLabel_2);
    connect(dropoutsLabel_3, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickDropoutsLabel_3);

    connect(dropoutsSpin_1, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamDoExtLimDw);
    connect(dropoutsSpin_2, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamDoHf1Lim);
    connect(dropoutsSpin_3, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamDoHf2Lim);

    connect(absLimLabel_1, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickAbsLimLabel_1);
    connect(absLimLabel_2, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickAbsLimLabel_2);
    connect(absLimLabel_3, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickAbsLimLabel_3);

    connect(absLimSpin_1, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamAlUMax);
    connect(absLimSpin_2, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamAlWMax);
    connect(absLimSpin_3, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamAlTsonMin);
    connect(absLimSpin_4, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamAlTsonMax);
    connect(absLimSpin_13, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamAlUMin);
    connect(absLimSpin_14, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamAlWMin);
    connect(absLimFilterCheckBox, &QCheckBox::toggled,
            this, &AdvStatisticalOptions::updateAbsLimFilter);

    connect(skewnessLabel_1, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickSkewnessLabel_1);
    connect(skewnessLabel_2, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickSkewnessLabel_2);
    connect(skewnessLabel_5, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickSkewnessLabel_5);
    connect(skewnessLabel_6, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickSkewnessLabel_6);

    connect(skewnessSpin_1, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamSkHfSkmin);
    connect(skewnessSpin_2, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamSkHfSkmax);
    connect(skewnessSpin_3, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamSkSfSkmin);
    connect(skewnessSpin_4, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamSkSfSkmax);
    connect(skewnessSpin_5, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamSkHfKumin);
    connect(skewnessSpin_6, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamSkHfKumax);
    connect(skewnessSpin_7, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamSkSfKumin);
    connect(skewnessSpin_8, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamSkSfKumax);

    connect(discontLabel_1, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickDiscontLabel_1);
    connect(discontLabel_2, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickDiscontLabel_2);
    connect(discontLabel_3, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickDiscontLabel_3);
    connect(discontLabel_8, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickDiscontLabel_8);

    connect(discontSpin_1, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamDsHfUV);
    connect(discontSpin_2, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamDsHfW);
    connect(discontSpin_3, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamDsHfT);
    connect(discontSpin_8, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamDsHfVar);
    connect(discontSpin_9, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamDsSfUV);
    connect(discontSpin_10, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamDsSfW);
    connect(discontSpin_11, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamDsSfT);
    connect(discontSpin_16, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamDsSfVar);

    connect(timeLagLabel_1, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickTimeLagLabel_1);
    connect(timeLagLabel_2, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickTimeLagLabel_2);

    connect(timeLagSpin_1, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamTlHfLim);
    connect(timeLagSpin_2, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamTlSfLim);

    connect(attackAngleLabel_1, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickAttackAngleLabel_1);
    connect(attackAngleLabel_2, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickAttackAngleLabel_2);
    connect(attackAngleLabel_3, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickAttackAngleLabel_3);

    connect(attackAngleSpin_1, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamAaMin);
    connect(attackAngleSpin_2, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamAaMax);
    connect(attackAngleSpin_3, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamAaLim);

    connect(nonSteadyLabel_1, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickNonSteadyLabel_1);
    connect(nonSteadySpin_1, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateParamNsHfLim);

    connect(detlimSettingsButton, &QPushButton::clicked,
            this, &AdvStatisticalOptions::showDetlimSettingsDialog);
    connect(randomErrorCheckBox, &QCheckBox::toggled,
            this, &AdvStatisticalOptions::updateRandomErrorArea);
    //> This control is not the only thing that writes ru_meth: the CEC
    //> significance test switches it on from its own dialog, because it cannot
    //> run without it. Without this the page would go on showing the old
    //> answer until the project was reloaded.
    connect(ecProject_, &EcProject::randomErrorMethodChanged,
            this, &AdvStatisticalOptions::syncRandomErrorMethod);

    connect(randomMethodLabel, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickRandomMethodLabel);
    connect(randomMethodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AdvStatisticalOptions::updateRandomMethod);

    connect(itsDefinitionLabel, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onClickItsDefinitionLabel);
    connect(itsDefinitionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AdvStatisticalOptions::updateItsDefinition);
    connect(timelagMaxLabel, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onTimelagMaxLabelCLicked);
    connect(timelagMaxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateTimelagMax);

    // NOTE: temporarly disabled
    connect(securityCoeffLabel, &ClickLabel::clicked,
            this, &AdvStatisticalOptions::onSecurityCoeffLabelCLicked);
    connect(securityCoeffSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AdvStatisticalOptions::updateSecurityCoeff);

    connect(ecProject_, &EcProject::ecProjectNew,
            this, &AdvStatisticalOptions::reset);
    connect(ecProject_, &EcProject::ecProjectChanged,
            this, &AdvStatisticalOptions::refresh);

    auto combo_list = QWidgetList() << randomMethodCombo
                                    << itsDefinitionCombo;
    for (auto widget : combo_list)
    {
        auto combo = static_cast<QComboBox *>(widget);
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &AdvStatisticalOptions::updateTooltip);
    }

    // init
    QTimer::singleShot(0, this, &AdvStatisticalOptions::reset);
}

AdvStatisticalOptions::~AdvStatisticalOptions()
{
}

void AdvStatisticalOptions::createTabWidget()
{
    tab0 = new QWidget;
    tab0->setObjectName(QStringLiteral("toolboxContent"));

    vickersDespikingRadio = new QRadioButton;
    vickersDespikingRadio->setText(tr("Vickers and Mahrt, 1997"));
    mauderDespikingRadio = new QRadioButton;
    mauderDespikingRadio->setText(tr("Mauder et al., 2013"));
    stepDespikingRadio = new QRadioButton;
    stepDespikingRadio->setText(tr("Consecutive difference (EddyUH)"));
    const auto stepTip = tr("<b>Consecutive difference:</b> Replaces a sample "
        "that steps further from the one before it than the limit you state, "
        "and counts it as a spike. A rate-of-change limit, <b>not</b> a "
        "statistical outlier test: nothing here is scaled by a standard "
        "deviation and nothing iterates, so the two methods above and this "
        "one do not take the same kind of number."
        "<br><br>The limits are <b>absolute, in each variable's own unit</b> "
        "- metres per second for the wind, kelvin for the sonic temperature, "
        "and the gas's own concentration unit for a gas. Leave one at zero "
        "and that column is not despiked at all; the run log names every "
        "column it passed over for that reason."
        "<br><br>This is EddyUH's <i>spi_method 1</i>. Its defaults here are "
        "the values the CH-LAE project used. Worth knowing before trusting "
        "them: a limit far larger than the signal's own sample-to-sample "
        "noise removes nothing, which is what those values do to a "
        "high-precision analyser's trace-gas channels.");
    stepDespikingRadio->setToolTip(stepTip);

    stepLabel_u = new ClickLabel(tr("u step limit : "));
    stepLabel_v = new ClickLabel(tr("v step limit : "));
    stepLabel_w = new ClickLabel(tr("w step limit : "));
    stepLabel_ts = new ClickLabel(tr("Sonic temperature step limit : "));
    const auto mkStep = [&](QDoubleSpinBox*& spin, const QString& suffix)
    {
        spin = new QDoubleSpinBox;
        spin->setDecimals(3);
        spin->setRange(0.0, 100000.0);
        spin->setSingleStep(0.5);
        spin->setAccelerated(true);
        spin->setSuffix(suffix);
        spin->setSpecialValueText(tr("not despiked"));
        spin->setToolTip(stepTip);
    };
    mkStep(stepSpin_u, tr("  [m+1s-1]"));
    mkStep(stepSpin_v, tr("  [m+1s-1]"));
    mkStep(stepSpin_w, tr("  [m+1s-1]"));
    mkStep(stepSpin_ts, tr("  [K]"));
    stepLabel_u->setToolTip(stepTip);
    stepLabel_v->setToolTip(stepTip);
    stepLabel_w->setToolTip(stepTip);
    stepLabel_ts->setToolTip(stepTip);
    stepGasTip_ = tr("<b>Step limit:</b> The largest step this gas may take "
        "from one sample to the next, in its own concentration unit. Zero "
        "leaves the column undespiked. Not a multiple of anything - see the "
        "method's own tooltip.");

    despikingRadioGroup = new QButtonGroup(this);
    despikingRadioGroup->addButton(vickersDespikingRadio, 0);
    despikingRadioGroup->addButton(mauderDespikingRadio, 1);
    //> Two, not two-in-sequence: the id IS the ini value, and the engine
    //> reads '2' for this method. Renumbering to keep the rows contiguous
    //> would change the method of every project that states one.
    despikingRadioGroup->addButton(stepDespikingRadio, 2);

    despLabel_1 = new ClickLabel(tr("Maximum number of consecutive outliers : "));
    despLabel_1->setToolTip(tr("<b>Maximum number of consecutive outliers:</b> Spikes are detected as outliers with respect to a certain plausibility range. However, if a series of consecutive outliers is found, it might be a sign of a physical trend. Specify <i>n</i>, the maximum number of consecutive outliers that define a spike. If more than <i>n</i> consecutive outliers are found, they are not flagged or removed. Note, however, that those values may be eliminated on the basis of a physical plausibility test (<b><i>Absolute limits</i></b> test)."));
    despSpin_1 = new QSpinBox;
    despSpin_1->setRange(1, 1000);
    despSpin_1->setSingleStep(10);
    despSpin_1->setAccelerated(true);
    despSpin_1->setToolTip(despLabel_1->toolTip());

    despLabel_8 = new ClickLabel(tr("Accepted spikes : "));
    despLabel_8->setToolTip(tr("<b>Accepted spikes:</b> Specify the percentage of spikes, after which each variable is flagged for too large number of spikes."));
    despSpin_8 = new QDoubleSpinBox;
    despSpin_8->setDecimals(1);
    despSpin_8->setRange(0.0, 50.0);
    despSpin_8->setSingleStep(1.0);
    despSpin_8->setAccelerated(true);
    despSpin_8->setSuffix(tr("  [%]", "Percentage"));
    despSpin_8->setToolTip(despLabel_8->toolTip());

    despFilterCheckBox = new QCheckBox;
    despFilterCheckBox->setToolTip(tr("<b>Replace spikes with linear interpolation:</b> Check this option to instruct EddyFlow to replace spikes with linear interpolation of neighboring data points."));
    despFilterCheckBox->setText(tr("Replace spikes with linear interpolation"));

    spikeGraphLabel = new QLabel;
    spikeGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/spike")));

    auto plausibilityLabel = WidgetUtils::createBlueLabel(this, tr("Plausibility ranges (<tt>%1%2n%3</tt>, where you set <tt>n</tt>)").arg(Defs::MICRO, Defs::PLUSMINUS, Defs::SIGMA));
    plausibilityLabel->setToolTip(tr("<b>Plausibility range:</b> A plausibility range is defined in a window of fixed length that moves throughout the time series, to detect outliers. The plausibility range is defined as the mean value in the window, %1 <i>n</i> times the standard deviation of the window. Specify here n. Note that default values differ for different variables. Note also that wind components, as well as fast temperature measurements, are included in <b><i>All other variables</i></b>.").arg(Defs::PLUSMINUS));

    despLabel_2 = new ClickLabel(tr("All other variables : "));
    despLabel_2->setToolTip(tr("<b>Plausibility range:</b> A plausibility range is defined in a window of fixed length that moves throughout the time series, to detect outliers. The plausibility range is defined as the mean value in the window, %1 <i>n</i> times the standard deviation of the window. Specify here n. Note that default values differ for different variables. Note also that wind components, as well as fast temperature measurements, are included in <b><i>All other variables</i></b>.").arg(Defs::PLUSMINUS));
    despSpin_2 = new QDoubleSpinBox;
    despSpin_2->setDecimals(1);
    despSpin_2->setRange(1.0, 20.0);
    despSpin_2->setSingleStep(0.1);
    despSpin_2->setAccelerated(true);
    despSpin_2->setSuffix(tr("  [%1]", "Sigma").arg(Defs::SIGMA));
    despSpin_2->setToolTip(despLabel_2->toolTip());

    despLabel_3 = new ClickLabel(tr("W : "));
    despLabel_3->setToolTip(tr("<b>Plausibility range:</b> A plausibility range is defined in a window of fixed length that moves throughout the time series, to detect outliers. The plausibility range is defined as the mean value in the window, %1 <i>n</i> times the standard deviation of the window. Specify here n. Note that default values differ for different variables. Note also that wind components, as well as fast temperature measurements, are included in <b><i>All other variables</i></b>.").arg(Defs::PLUSMINUS));
    despSpin_3 = new QDoubleSpinBox;
    despSpin_3->setDecimals(1);
    despSpin_3->setRange(1.0, 20.0);
    despSpin_3->setSingleStep(0.1);
    despSpin_3->setAccelerated(true);
    despSpin_3->setSuffix(tr("  [%1]", "Sigma").arg(Defs::SIGMA));
    despSpin_3->setToolTip(despLabel_3->toolTip());

    // The per-gas plausibility ranges are built in rebuildGasRows(), one row
    // per gas record; despLabel_2 below them moves with the gas count.
    despGasTip_ = despLabel_3->toolTip();

    auto tab0Grid = new QGridLayout;
    tab0Grid_ = tab0Grid;
    tab0Grid->addWidget(vickersDespikingRadio, 0, 0, 1, 2);
    tab0Grid->addWidget(despLabel_8, 0, 2, Qt::AlignRight);
    tab0Grid->addWidget(despSpin_8, 0, 3);
    tab0Grid->addWidget(mauderDespikingRadio, 1, 0, 1, 2);
    tab0Grid->addWidget(despFilterCheckBox, 1, 3);
    //> Its own column pair, because its numbers are absolute limits and the
    //> ones to the left are sigma multipliers. Side by side in the same
    //> column would invite reading one as the other.
    tab0Grid->addWidget(stepDespikingRadio, 0, 4, 1, 2);
    tab0Grid->addWidget(stepLabel_u, 1, 4, Qt::AlignRight);
    tab0Grid->addWidget(stepSpin_u, 1, 5);
    tab0Grid->addWidget(stepLabel_v, 2, 4, Qt::AlignRight);
    tab0Grid->addWidget(stepSpin_v, 2, 5);
    tab0Grid->addWidget(stepLabel_w, 3, 4, Qt::AlignRight);
    tab0Grid->addWidget(stepSpin_w, 3, 5);
    tab0Grid->addWidget(stepLabel_ts, 4, 4, Qt::AlignRight);
    tab0Grid->addWidget(stepSpin_ts, 4, 5);
    tab0Grid->addWidget(questionMark_2, 2, 0);
    tab0Grid->addWidget(spikeGraphLabel, 2, 1, -1, 1, Qt::AlignTop);
    tab0Grid->addWidget(spikeGraphLabel, 2, 1, -1, 1, Qt::AlignTop);
    tab0Grid->addWidget(despLabel_1, 2, 2, Qt::AlignRight);
    tab0Grid->addWidget(despSpin_1, 2, 3);
    tab0Grid->addWidget(plausibilityLabel, 3, 3, Qt::AlignRight);
    tab0Grid->addWidget(despLabel_3, 4, 2, Qt::AlignRight);
    tab0Grid->addWidget(despSpin_3, 4, 3);
    // gas rows from row 5, then despLabel_2 / despSpin_2 - see rebuildGasRows()
    tab0Grid->setColumnStretch(0, 0);
    tab0Grid->setColumnStretch(1, 0);
    tab0Grid->setColumnStretch(2, 1);
    tab0Grid->setColumnStretch(3, 0);
    tab0Grid->setColumnStretch(4, 1);
    tab0Grid->setColumnStretch(5, 0);
    tab0Grid->setContentsMargins(10, 10, 10, 10);
    tab0->setLayout(tab0Grid);
    tab0->setEnabled(false);

    tab1 = new QWidget;
    tab1->setObjectName(QStringLiteral("toolboxContent"));

    amplResLabel_1 = new ClickLabel(tr("Range of variation : "));
    amplResLabel_1->setToolTip(tr("<b>Range of variation:</b> The procedure requires the definition of the total range of variation for each variable along the time series. This range is defined as the largest among %1 <i>n</i>/2 times the standard deviation around the mean value of the variable, where <i>n</i> is the number you enter here, and the difference between the maximum and the minimum values attained by the variable along the time series.").arg(Defs::PLUSMINUS));
    amplResSpin_1 = new QDoubleSpinBox;
    amplResSpin_1->setDecimals(1);
    amplResSpin_1->setRange(1.0, 20.0);
    amplResSpin_1->setSingleStep(1.0);
    amplResSpin_1->setAccelerated(true);
    amplResSpin_1->setSuffix(tr("  [%1]", "Sigma").arg(Defs::SIGMA));
    amplResSpin_1->setToolTip(amplResLabel_1->toolTip());

    amplResLabel_2 = new ClickLabel(tr("Number of bins : "));
    amplResLabel_2->setToolTip(tr("<b>Number of bins:</b> The procedure requires the division of the variable variation range in <i>n</i> bins, and counting how many bins are empty, in order to detect if small enough variations are captured by the instrument. The higher the number of bins, the higher the expectation on the instrument ability to resolve tiny fluctuations."));
    amplResSpin_2 = new QSpinBox;
    amplResSpin_2->setRange(50, 150);
    amplResSpin_2->setSingleStep(10);
    amplResSpin_2->setAccelerated(true);
    amplResSpin_2->setToolTip(amplResLabel_2->toolTip());

    amplResLabel_3 = new ClickLabel(tr("Accepted empty bins : "));
    amplResLabel_3->setToolTip(tr("<b>Accepted empty bins:</b> Specify the maximum number of empty bins allowed for each variable. If more empty bins are found, the variable is flagged for poor amplitude resolution."));
    amplResSpin_3 = new QSpinBox;
    amplResSpin_3->setRange(1, 100);
    amplResSpin_3->setSingleStep(10);
    amplResSpin_3->setAccelerated(true);
    amplResSpin_3->setSuffix(tr("  [%]", "Percentage"));
    amplResSpin_3->setToolTip(amplResLabel_3->toolTip());

    amplResGraphLabel = new QLabel;
    amplResGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/amplitude")));

    auto tab1Grid = new QGridLayout;
    tab1Grid->addWidget(questionMark_3, 0, 0);
    tab1Grid->addWidget(amplResGraphLabel, 0, 1, -1, 1, Qt::AlignTop);
    tab1Grid->addWidget(amplResLabel_1, 0, 2, Qt::AlignRight);
    tab1Grid->addWidget(amplResSpin_1, 0, 3);
    tab1Grid->addWidget(amplResLabel_2, 1, 2, Qt::AlignRight);
    tab1Grid->addWidget(amplResSpin_2, 1, 3);
    tab1Grid->addWidget(amplResLabel_3, 2, 2, Qt::AlignRight);
    tab1Grid->addWidget(amplResSpin_3, 2, 3);
    tab1Grid->setRowStretch(3, 1);
    tab1Grid->setColumnStretch(0, 0);
    tab1Grid->setColumnStretch(1, 0);
    tab1Grid->setColumnStretch(2, 1);
    tab1Grid->setColumnStretch(3, 1);
    tab1Grid->setContentsMargins(10, 10, 10, 10);
    tab1->setLayout(tab1Grid);
    tab1->setEnabled(false);

    tab2 = new QWidget;
    tab2->setObjectName(QStringLiteral("toolboxContent"));

    dropoutsLabel_1 = new ClickLabel(tr("Percentile defining extreme bins : "));
    dropoutsLabel_1->setToolTip(tr("<b>Percentile defining extreme bins:</b> The procedure requires the division of the variable variation range in <i>n</i> bins, and the definition of <b><i>extreme</i></b> and <i><b>central</b></i> bins. Specify the percentile that sets this threshold."));
    dropoutsSpin_1 = new QSpinBox;
    dropoutsSpin_1->setRange(1, 100);
    dropoutsSpin_1->setSingleStep(5);
    dropoutsSpin_1->setAccelerated(true);
    dropoutsSpin_1->setToolTip(dropoutsLabel_1->toolTip());

    dropoutsLabel_2 = new ClickLabel(tr("Accepted central drop-outs : "));
    dropoutsLabel_2->setToolTip(tr("<b>Accepted central drop-outs:</b> Specify the percentage of consecutive data found to share the same bin as neighboring data. It is more likely to find this occurrence for <i><b>central</b></i> bins, which is why the default percentage here is higher. If a higher percentage is found, the variable is flagged for drop-outs."));
    dropoutsSpin_2 = new QDoubleSpinBox;
    dropoutsSpin_2->setDecimals(1);
    dropoutsSpin_2->setRange(1, 100);
    dropoutsSpin_2->setSingleStep(5);
    dropoutsSpin_2->setAccelerated(true);
    dropoutsSpin_2->setSuffix(tr("  [%]", "Percentage"));
    dropoutsSpin_2->setToolTip(dropoutsLabel_2->toolTip());

    dropoutsLabel_3 = new ClickLabel(tr("Accepted extreme drop-outs : "));
    dropoutsLabel_3->setToolTip(tr("<b>Accepted extreme drop-outs:</b> Specify the percentage of data found consecutive to share the same bin as neighboring data. It is less likely to find this occurrence for <i><b>extreme</b></i> bins, which is why the default percentage here is lower. If a higher percentage is found, the variable is flagged for drop-outs."));
    dropoutsSpin_3 = new QDoubleSpinBox;
    dropoutsSpin_3->setDecimals(1);
    dropoutsSpin_3->setRange(1, 100);
    dropoutsSpin_3->setSingleStep(1);
    dropoutsSpin_3->setAccelerated(true);
    dropoutsSpin_3->setSuffix(tr("  [%]", "Percentage"));
    dropoutsSpin_3->setToolTip(dropoutsLabel_3->toolTip());

    dropoutsGraphLabel = new QLabel;
    dropoutsGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/dropout")));

    auto tab2Grid = new QGridLayout;
    tab2Grid->addWidget(questionMark_4, 0, 0);
    tab2Grid->addWidget(dropoutsGraphLabel, 0, 1, -1, 1, Qt::AlignTop);
    tab2Grid->addWidget(dropoutsLabel_1, 0, 2, Qt::AlignRight);
    tab2Grid->addWidget(dropoutsSpin_1, 0, 3);
    tab2Grid->addWidget(dropoutsLabel_2, 1, 2, Qt::AlignRight);
    tab2Grid->addWidget(dropoutsSpin_2, 1, 3);
    tab2Grid->addWidget(dropoutsLabel_3, 2, 2, Qt::AlignRight);
    tab2Grid->addWidget(dropoutsSpin_3, 2, 3);
    tab2Grid->setRowStretch(3, 1);
    tab2Grid->setColumnStretch(0, 0);
    tab2Grid->setColumnStretch(1, 0);
    tab2Grid->setColumnStretch(2, 1);
    tab2Grid->setColumnStretch(3, 1);
    tab2Grid->setContentsMargins(10, 10, 10, 10);
    tab2->setLayout(tab2Grid);
    tab2->setEnabled(false);

    tab3 = new QWidget;
    tab3->setObjectName(QStringLiteral("toolboxContent"));

    auto minLabel = WidgetUtils::createBlueLabel(this, tr("Minimum"));
    minLabel->setToolTip(tr("<b>Minimum:</b> Set the minimum value physically plausible for each variable. If at least 1 value is found to exceed this limit, the variable is flagged."));

    auto maxLabel = WidgetUtils::createBlueLabel(this, tr("Maximum"));
    maxLabel->setToolTip(tr("<b>Maximum:</b> Set the maximum value physically plausible for each variable. If at least 1 value is found to exceed this limit, the variable is flagged."));

    absLimLabel_1 = new ClickLabel(tr("U : "));
    absLimSpin_1 = new QDoubleSpinBox;
    absLimSpin_1->setDecimals(1);
    absLimSpin_1->setRange(1.0, 50.0);
    absLimSpin_1->setSingleStep(5.0);
    absLimSpin_1->setAccelerated(true);
    absLimSpin_1->setSuffix(tr("  [m/s]", ""));
    absLimSpin_1->setToolTip(maxLabel->toolTip());

    absLimLabel_2 = new ClickLabel(tr("W : "));
    absLimSpin_2 = new QDoubleSpinBox;
    absLimSpin_2->setDecimals(1);
    absLimSpin_2->setRange(0.5, 10.0);
    absLimSpin_2->setSingleStep(0.5);
    absLimSpin_2->setAccelerated(true);
    absLimSpin_2->setSuffix(tr("  [m/s]", ""));
    absLimSpin_2->setToolTip(maxLabel->toolTip());

    absLimLabel_3 = new ClickLabel(tr("%1 : ").arg(Defs::TSON_STRING));
    absLimSpin_3 = new QDoubleSpinBox;
    absLimSpin_3->setDecimals(1);
    absLimSpin_3->setRange(-100.0, 20.0);
    absLimSpin_3->setSingleStep(10.0);
    absLimSpin_3->setAccelerated(true);
    absLimSpin_3->setSuffix(tr("  [%1]", "").arg(Defs::DEGREE_C));
    absLimSpin_3->setToolTip(minLabel->toolTip());

    absLimSpin_4 = new QDoubleSpinBox;
    absLimSpin_4->setDecimals(1);
    absLimSpin_4->setRange(-20.0, 100.0);
    absLimSpin_4->setSingleStep(10.0);
    absLimSpin_4->setAccelerated(true);
    absLimSpin_4->setSuffix(tr("  [%1]", "").arg(Defs::DEGREE_C));
    absLimSpin_4->setToolTip(maxLabel->toolTip());

    // The per-gas limits are built in rebuildGasRows(), one row per gas
    // record; absLimFilterCheckBox below them moves with the gas count.
    absLimMinTip_ = minLabel->toolTip();
    absLimMaxTip_ = maxLabel->toolTip();

    absLimSpin_13 = new QDoubleSpinBox;
    absLimSpin_13->setDecimals(1);
    absLimSpin_13->setRange(-50.0, -1.0);
    absLimSpin_13->setSingleStep(5.0);
    absLimSpin_13->setAccelerated(true);
    absLimSpin_13->setSuffix(tr("  [m/s]", ""));
    absLimSpin_13->setToolTip(minLabel->toolTip());

    absLimSpin_14 = new QDoubleSpinBox;
    absLimSpin_14->setDecimals(1);
    absLimSpin_14->setRange(-10.0, -0.5);
    absLimSpin_14->setSingleStep(0.5);
    absLimSpin_14->setAccelerated(true);
    absLimSpin_14->setSuffix(tr("  [m/s]", ""));
    absLimSpin_14->setToolTip(minLabel->toolTip());

    absLimFilterCheckBox = new QCheckBox;
    absLimFilterCheckBox->setText(tr("Filter outranged values"));
    absLimFilterCheckBox->setToolTip(tr("<b>Filter outranged values:</b> Check this option to instruct EddyFlow to eliminate values outside the plausibility range. When values are eliminated, all other variables are preserved and a lag is avoided by replacing the value with EddyFlow\'s error code."));

    absLimGraphLabel = new QLabel;
    absLimGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/range")));

    auto lockedIcon_1 = new QLabel;
    auto pixmap_1 = QPixmap(QStringLiteral(":/icons/link"));
#if defined(Q_OS_MACOS)
    pixmap_1.setDevicePixelRatio(2.0);
#endif
    lockedIcon_1->setPixmap(pixmap_1);

    auto lockedIcon_2 = new QLabel;
    auto pixmap_2 = QPixmap(QStringLiteral(":/icons/link"));
#if defined(Q_OS_MACOS)
    pixmap_2.setDevicePixelRatio(2.0);
#endif
    lockedIcon_2->setPixmap(pixmap_2);

    auto tab3Grid = new QGridLayout;
    tab3Grid_ = tab3Grid;
    tab3Grid->addWidget(questionMark_5, 0, 0);
    tab3Grid->addWidget(absLimGraphLabel, 0, 1, -1, 1, Qt::AlignTop);
    tab3Grid->addWidget(minLabel, 0, 4, 1, 1);
    tab3Grid->addWidget(maxLabel, 0, 6, 1, 1);
    tab3Grid->addWidget(absLimLabel_1, 1, 3, Qt::AlignRight);
    tab3Grid->addWidget(absLimSpin_13, 1, 4);
    tab3Grid->addWidget(lockedIcon_1, 1, 5);
    tab3Grid->addWidget(absLimSpin_1, 1, 6);
    tab3Grid->addWidget(absLimLabel_2, 2, 3, Qt::AlignRight);
    tab3Grid->addWidget(absLimSpin_14, 2, 4);
    tab3Grid->addWidget(lockedIcon_2, 2, 5);
    tab3Grid->addWidget(absLimSpin_2, 2, 6);
    tab3Grid->addWidget(absLimLabel_3, 3, 3, Qt::AlignRight);
    tab3Grid->addWidget(absLimSpin_3, 3, 4);
    tab3Grid->addWidget(absLimSpin_4, 3, 6);
    // gas rows from row 4, then absLimFilterCheckBox - see rebuildGasRows()
    tab3Grid->setColumnStretch(0, 0);
    tab3Grid->setColumnStretch(1, 0);
    tab3Grid->setColumnStretch(2, 1);
    tab3Grid->setColumnStretch(3, 0);
    tab3Grid->setColumnStretch(4, 1);
    tab3Grid->setColumnStretch(5, 0);
    tab3Grid->setColumnStretch(6, 1);
    tab3Grid->setColumnStretch(2, 1);
    tab3Grid->setColumnStretch(4, 1);

    tab3->setLayout(tab3Grid);
    tab3->setEnabled(false);

    tab4 = new QWidget;
    tab4->setObjectName(QStringLiteral("toolboxContent"));

    auto hardFlagLabel = WidgetUtils::createBlueLabel(this, tr("Hard-flag threshold"));
    hardFlagLabel->setToolTip(tr("<b>Hard-flag threshold:</b> Set the lower and upper limits to both skewness and kurtosis for hard-flagging the variable. If statistical moments are found to exceed these ranges, the variable is hard-flagged."));

    auto softFlagLabel = WidgetUtils::createBlueLabel(this, tr("Soft-flag threshold"));
    softFlagLabel->setToolTip(tr("<b>Soft-flag threshold:</b> Set the lower and upper limits to both skewness and kurtosis for soft-flagging the variable. If statistical moments are found to exceed these ranges, the variable is soft-flagged."));

    skewnessLabel_1 = new ClickLabel(tr("Skewness lower limit : "));
    skewnessSpin_1 = new QDoubleSpinBox;
    skewnessSpin_1->setDecimals(1);
    skewnessSpin_1->setRange(-3.0, -0.1);
    skewnessSpin_1->setSingleStep(0.1);
    skewnessSpin_1->setAccelerated(true);
    skewnessSpin_1->setToolTip(hardFlagLabel->toolTip());

    skewnessLabel_2 = new ClickLabel(tr("Skewness upper limit : "));
    skewnessSpin_2 = new QDoubleSpinBox;
    skewnessSpin_2->setDecimals(1);
    skewnessSpin_2->setRange(0.1, 3.0);
    skewnessSpin_2->setSingleStep(0.1);
    skewnessSpin_2->setAccelerated(true);
    skewnessSpin_2->setToolTip(hardFlagLabel->toolTip());

    skewnessSpin_3 = new QDoubleSpinBox;
    skewnessSpin_3->setDecimals(1);
    skewnessSpin_3->setRange(-3.0, -0.1);
    skewnessSpin_3->setSingleStep(0.1);
    skewnessSpin_3->setAccelerated(true);
    skewnessSpin_3->setToolTip(softFlagLabel->toolTip());

    skewnessSpin_4 = new QDoubleSpinBox;
    skewnessSpin_4->setDecimals(1);
    skewnessSpin_4->setRange(0.1, 3.0);
    skewnessSpin_4->setSingleStep(0.1);
    skewnessSpin_4->setAccelerated(true);
    skewnessSpin_4->setToolTip(softFlagLabel->toolTip());

    skewnessLabel_5 = new ClickLabel(tr("Kurtosis lower limit : "));
    skewnessSpin_5 = new QDoubleSpinBox;
    skewnessSpin_5->setDecimals(1);
    skewnessSpin_5->setRange(0.1, 3.0);
    skewnessSpin_5->setSingleStep(0.1);
    skewnessSpin_5->setAccelerated(true);
    skewnessSpin_5->setToolTip(hardFlagLabel->toolTip());

    skewnessLabel_6 = new ClickLabel(tr("Kurtosis upper limit : "));
    skewnessSpin_6 = new QDoubleSpinBox;
    skewnessSpin_6->setDecimals(1);
    skewnessSpin_6->setRange(3.0, 10.0);
    skewnessSpin_6->setSingleStep(1.0);
    skewnessSpin_6->setAccelerated(true);
    skewnessSpin_6->setToolTip(hardFlagLabel->toolTip());

    skewnessSpin_7 = new QDoubleSpinBox;
    skewnessSpin_7->setDecimals(1);
    skewnessSpin_7->setRange(0.1, 3.0);
    skewnessSpin_7->setSingleStep(0.1);
    skewnessSpin_7->setAccelerated(true);
    skewnessSpin_7->setToolTip(softFlagLabel->toolTip());

    skewnessSpin_8 = new QDoubleSpinBox;
    skewnessSpin_8->setDecimals(1);
    skewnessSpin_8->setRange(3.0, 10.0);
    skewnessSpin_8->setSingleStep(1.0);
    skewnessSpin_8->setAccelerated(true);
    skewnessSpin_8->setToolTip(softFlagLabel->toolTip());

    skewnessGraphLabel = new QLabel;
    skewnessGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/skewkurt")));

    auto tab4Grid = new QGridLayout;
    tab4Grid->addWidget(questionMark_6, 0, 0);
    tab4Grid->addWidget(skewnessGraphLabel, 0, 1, -1, 1, Qt::AlignTop);
    tab4Grid->addWidget(hardFlagLabel, 0, 3, 1, 1);
    tab4Grid->addWidget(softFlagLabel, 0, 4, 1, 1);
    tab4Grid->addWidget(skewnessLabel_1, 1, 2, Qt::AlignRight);
    tab4Grid->addWidget(skewnessSpin_1, 1, 3);
    tab4Grid->addWidget(skewnessSpin_3, 1, 4);
    tab4Grid->addWidget(skewnessLabel_2, 2, 2, Qt::AlignRight);
    tab4Grid->addWidget(skewnessSpin_2, 2, 3);
    tab4Grid->addWidget(skewnessSpin_4, 2, 4);
    tab4Grid->addWidget(skewnessLabel_5, 3, 2, Qt::AlignRight);
    tab4Grid->addWidget(skewnessSpin_5, 3, 3);
    tab4Grid->addWidget(skewnessSpin_7, 3, 4);
    tab4Grid->addWidget(skewnessLabel_6, 4, 2, Qt::AlignRight);
    tab4Grid->addWidget(skewnessSpin_6, 4, 3);
    tab4Grid->addWidget(skewnessSpin_8, 4, 4);
    tab4Grid->setRowStretch(5, 1);
    tab4Grid->setColumnStretch(2, 1);
    tab4Grid->setColumnStretch(3, 1);
    tab4Grid->setColumnStretch(4, 1);
    tab4Grid->setContentsMargins(10, 10, 10, 10);
    tab4->setLayout(tab4Grid);
    tab4->setEnabled(false);

    tab5 = new QWidget;
    tab5->setObjectName(QStringLiteral("toolboxContent"));

    auto hardFlagLabel_2 = WidgetUtils::createBlueLabel(this, tr("Hard-flag threshold"));
    hardFlagLabel_2->setToolTip(tr("<b>Hard-flag threshold:</b> Set the limits to the values attained by the Haar functions, beyond which the variable is hard-flagged for discontinuities. Refer to the original publication for the definition of the Haar functions."));

    auto softFlagLabel_2 = WidgetUtils::createBlueLabel(this, tr("Soft-flag threshold"));
    softFlagLabel_2->setToolTip(tr("<b>Soft-flag threshold:</b> Set the limits to the values attained by the Haar functions, beyond which the variable is soft-flagged for discontinuities. Refer to the original publication for the definition of the Haar functions."));

    discontLabel_1 = new ClickLabel(tr("U : "));
    discontSpin_1 = new QDoubleSpinBox;
    discontSpin_1->setDecimals(2);
    discontSpin_1->setRange(0.0, 50.0);
    discontSpin_1->setSingleStep(1.0);
    discontSpin_1->setAccelerated(true);
    discontSpin_1->setToolTip(hardFlagLabel_2->toolTip());

    discontLabel_2 = new ClickLabel(tr("W : "));
    discontSpin_2 = new QDoubleSpinBox;
    discontSpin_2->setDecimals(2);
    discontSpin_2->setRange(0.0, 50.0);
    discontSpin_2->setSingleStep(1.0);
    discontSpin_2->setAccelerated(true);
    discontSpin_2->setToolTip(hardFlagLabel_2->toolTip());

    discontLabel_3 = new ClickLabel(tr("%1 : ").arg(Defs::TSON_STRING));
    discontSpin_3 = new QDoubleSpinBox;
    discontSpin_3->setDecimals(2);
    discontSpin_3->setRange(0.0, 50.0);
    discontSpin_3->setSingleStep(1.0);
    discontSpin_3->setAccelerated(true);
    discontSpin_3->setToolTip(hardFlagLabel_2->toolTip());

    // The per-gas thresholds are built in rebuildGasRows(), one row per gas
    // record; the Variances row below them moves with the gas count.
    dsHardTip_ = hardFlagLabel_2->toolTip();
    dsSoftTip_ = softFlagLabel_2->toolTip();

    discontLabel_8 = new ClickLabel(tr("Variances : "));
    discontSpin_8 = new QDoubleSpinBox;
    discontSpin_8->setDecimals(2);
    discontSpin_8->setRange(0.0, 50.0);
    discontSpin_8->setSingleStep(1.0);
    discontSpin_8->setAccelerated(true);
    discontSpin_8->setToolTip(hardFlagLabel_2->toolTip());

    discontSpin_9 = new QDoubleSpinBox;
    discontSpin_9->setDecimals(2);
    discontSpin_9->setRange(0.0, 50.0);
    discontSpin_9->setSingleStep(1.0);
    discontSpin_9->setAccelerated(true);
    discontSpin_9->setToolTip(softFlagLabel_2->toolTip());

    discontSpin_10 = new QDoubleSpinBox;
    discontSpin_10->setDecimals(2);
    discontSpin_10->setRange(0.0, 50.0);
    discontSpin_10->setSingleStep(1.0);
    discontSpin_10->setAccelerated(true);
    discontSpin_10->setToolTip(softFlagLabel_2->toolTip());

    discontSpin_11 = new QDoubleSpinBox;
    discontSpin_11->setDecimals(2);
    discontSpin_11->setRange(0.0, 50.0);
    discontSpin_11->setSingleStep(1.0);
    discontSpin_11->setAccelerated(true);
    discontSpin_11->setToolTip(softFlagLabel_2->toolTip());

    discontSpin_16 = new QDoubleSpinBox;
    discontSpin_16->setDecimals(2);
    discontSpin_16->setRange(0.0, 50.0);
    discontSpin_16->setSingleStep(1.0);
    discontSpin_16->setAccelerated(true);
    discontSpin_16->setToolTip(softFlagLabel_2->toolTip());

    discontGraphLabel = new QLabel;
    discontGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/discont")));

    auto tab5Grid = new QGridLayout;
    tab5Grid_ = tab5Grid;
    tab5Grid->addWidget(questionMark_7, 0, 0);
    tab5Grid->addWidget(discontGraphLabel, 0, 1, -1, 1, Qt::AlignTop);
    tab5Grid->addWidget(hardFlagLabel_2, 0, 3, 1, 1);
    tab5Grid->addWidget(softFlagLabel_2, 0, 4, 1, 1);
    tab5Grid->addWidget(discontLabel_1, 1, 2, Qt::AlignRight);
    tab5Grid->addWidget(discontSpin_1, 1, 3);
    tab5Grid->addWidget(discontLabel_2, 2, 2, Qt::AlignRight);
    tab5Grid->addWidget(discontSpin_2, 2, 3);
    tab5Grid->addWidget(discontLabel_3, 3, 2, Qt::AlignRight);
    tab5Grid->addWidget(discontSpin_3, 3, 3);
    tab5Grid->addWidget(discontSpin_9, 1, 4);
    tab5Grid->addWidget(discontSpin_10, 2, 4);
    tab5Grid->addWidget(discontSpin_11, 3, 4);
    // gas rows from row 4, then the Variances row - see rebuildGasRows()
    tab5Grid->setColumnStretch(0, 0);
    tab5Grid->setColumnStretch(1, 0);
    tab5Grid->setColumnStretch(2, 1);
    tab5Grid->setColumnStretch(3, 1);
    tab5Grid->setColumnStretch(4, 1);
    tab5Grid->setContentsMargins(10, 10, 10, 10);
    tab5->setLayout(tab5Grid);
    tab5->setEnabled(false);

    tab6 = new QWidget;
    tab6->setObjectName(QStringLiteral("toolboxContent"));

    timeLagLabel_1 = new ClickLabel(tr("Accepted covariance difference (hard-flag) : "));
    timeLagLabel_1->setToolTip(tr("<b>Accepted covariance difference (hard-flag):</b> Set the percentage of deviation between the covariance calculated at the nominal time lag and the maximal covariance. If the actual difference exceeds this limit, the variable is hard-flagged."));
    timeLagSpin_1 = new QDoubleSpinBox;
    timeLagSpin_1->setDecimals(1);
    timeLagSpin_1->setRange(0.0, 100.0);
    timeLagSpin_1->setSingleStep(10.0);
    timeLagSpin_1->setAccelerated(true);
    timeLagSpin_1->setSuffix(tr("  [%]", "Percentage"));
    timeLagSpin_1->setToolTip(timeLagLabel_1->toolTip());

    timeLagLabel_2 = new ClickLabel(tr("Accepted covariance difference (soft-flag) : "));
    timeLagLabel_2->setToolTip(tr("<b>Accepted covariance difference (soft-flag):</b> Set the percentage of deviation between the covariance calculated at the nominal time lag and the maximal covariance. If the actual difference exceeds this limit, the variable is soft-flagged."));
    timeLagSpin_2 = new QDoubleSpinBox;
    timeLagSpin_2->setDecimals(1);
    timeLagSpin_2->setRange(0.0, 100.0);
    timeLagSpin_2->setSingleStep(10.0);
    timeLagSpin_2->setAccelerated(true);
    timeLagSpin_2->setSuffix(tr("  [%]", "Percentage"));
    timeLagSpin_2->setToolTip(timeLagLabel_2->toolTip());

    // The nominal time lags are built in rebuildGasRows(), one row per gas
    // record: a species measured on two analysers has two sampling lines and
    // so two nominal lags.

    timelagGraphLabel = new QLabel;
    timelagGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/timelag")));

    auto tab6Grid = new QGridLayout;
    tab6Grid_ = tab6Grid;
    tab6Grid->addWidget(questionMark_8, 0, 0);
    tab6Grid->addWidget(timelagGraphLabel, 0, 1, -1, 1, Qt::AlignTop);
    tab6Grid->addWidget(timeLagLabel_1, 0, 2, Qt::AlignRight);
    tab6Grid->addWidget(timeLagSpin_1, 0, 3);
    tab6Grid->addWidget(timeLagLabel_2, 1, 2, Qt::AlignRight);
    tab6Grid->addWidget(timeLagSpin_2, 1, 3);
    // gas rows from row 2 - see rebuildGasRows()
    tab6Grid->setColumnStretch(0, 0);
    tab6Grid->setColumnStretch(1, 0);
    tab6Grid->setColumnStretch(2, 1);
    tab6Grid->setColumnStretch(3, 1);
    tab6Grid->setContentsMargins(10, 10, 10, 10);
    tab6->setLayout(tab6Grid);
    tab6->setEnabled(false);

    tab7 = new QWidget;
    tab7->setObjectName(QStringLiteral("toolboxContent"));

    attackAngleLabel_1 = new ClickLabel(tr("Minimum angle of attack : "));
    attackAngleLabel_1->setToolTip(tr("<b>Minimum angle of attack:</b> Set the minimal angle of attack. Individual values below this one will be counted as outranged."));
    attackAngleSpin_1 = new QDoubleSpinBox;
    attackAngleSpin_1->setDecimals(1);
    attackAngleSpin_1->setRange(-90.0, 0.0);
    attackAngleSpin_1->setSingleStep(10.0);
    attackAngleSpin_1->setAccelerated(true);
    attackAngleSpin_1->setSuffix(tr("  [%1]", "Degrees").arg(Defs::DEGREE));
    attackAngleSpin_1->setToolTip(attackAngleLabel_1->toolTip());

    attackAngleLabel_2 = new ClickLabel(tr("Maximum angle of attack : "));
    attackAngleLabel_2->setToolTip(tr("<b>Maximum angle of attack:</b> Set the maximal angle of attack. Individual values above this one will be counted as outranged."));
    attackAngleSpin_2 = new QDoubleSpinBox;
    attackAngleSpin_2->setDecimals(1);
    attackAngleSpin_2->setRange(0.0, 90.0);
    attackAngleSpin_2->setSingleStep(10.0);
    attackAngleSpin_2->setAccelerated(true);
    attackAngleSpin_2->setSuffix(tr("  [%1]", "Degrees").arg(Defs::DEGREE));
    attackAngleSpin_2->setToolTip(attackAngleLabel_2->toolTip());

    attackAngleLabel_3 = new ClickLabel(tr("Accepted amount of outliers : "));
    attackAngleLabel_3->setToolTip(tr("<b>Accepted amount of outliers:</b> Set the accepted percentage of outranged attack angles. If the actual percentage is above your setting, the current flux averaging interval will be flagged correspondingly."));
    attackAngleSpin_3 = new QDoubleSpinBox;
    attackAngleSpin_3->setDecimals(1);
    attackAngleSpin_3->setRange(0.0, 100.0);
    attackAngleSpin_3->setSingleStep(10.0);
    attackAngleSpin_3->setAccelerated(true);
    attackAngleSpin_3->setSuffix(tr("  [%]", "Percentage"));
    attackAngleSpin_3->setToolTip(attackAngleLabel_3->toolTip());

    attackAngleGraphLabel = new QLabel;
    attackAngleGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/aoa")));

    auto tab7Grid = new QGridLayout;
    tab7Grid->addWidget(questionMark_9, 0, 0);
    tab7Grid->addWidget(attackAngleGraphLabel, 0, 1, -1, 1, Qt::AlignTop);
    tab7Grid->addWidget(attackAngleLabel_1, 0, 2, Qt::AlignRight);
    tab7Grid->addWidget(attackAngleSpin_1, 0, 3);
    tab7Grid->addWidget(attackAngleLabel_2, 1, 2, Qt::AlignRight);
    tab7Grid->addWidget(attackAngleSpin_2, 1, 3);
    tab7Grid->addWidget(attackAngleLabel_3, 2, 2, Qt::AlignRight);
    tab7Grid->addWidget(attackAngleSpin_3, 2, 3);
    tab7Grid->setRowStretch(3, 1);
    tab7Grid->setColumnStretch(0, 0);
    tab7Grid->setColumnStretch(1, 0);
    tab7Grid->setColumnStretch(2, 1);
    tab7Grid->setColumnStretch(3, 1);
    tab7Grid->setContentsMargins(10, 10, 10, 10);
    tab7->setLayout(tab7Grid);
    tab7->setEnabled(false);

    tab8 = new QWidget;
    tab8->setObjectName(QStringLiteral("toolboxContent"));

    nonSteadyLabel_1 = new ClickLabel(tr("Accepted wind relative instationarity : "));
    nonSteadyLabel_1->setToolTip(tr("<b>Accepted wind relative instationarity:</b> Set the maximum relative variation of horizontal wind between the beginning and the end of the file, assessed on the linear regression of horizontal wind time series. If the actual variation is beyond this limit, the flux averaging interval is flagged accordingly."));
    nonSteadySpin_1 = new QDoubleSpinBox;
    nonSteadySpin_1->setDecimals(1);
    nonSteadySpin_1->setRange(0.0, 50.0);
    nonSteadySpin_1->setSingleStep(1.0);
    nonSteadySpin_1->setAccelerated(true);
    nonSteadySpin_1->setToolTip(nonSteadyLabel_1->toolTip());

    nonSteadyGraphLabel = new QLabel;
    nonSteadyGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/instat")));

    auto tab8Grid = new QGridLayout;
    tab8Grid->addWidget(questionMark_10, 0, 0);
    tab8Grid->addWidget(nonSteadyGraphLabel, 0, 1, -1, 1, Qt::AlignTop);
    tab8Grid->addWidget(nonSteadyLabel_1, 0, 2, Qt::AlignRight);
    tab8Grid->addWidget(nonSteadySpin_1, 0, 3);
    tab8Grid->setRowStretch(1, 1);
    tab8Grid->setColumnStretch(0, 0);
    tab8Grid->setColumnStretch(1, 0);
    tab8Grid->setColumnStretch(2, 1);
    tab8Grid->setColumnStretch(3, 1);
    tab8Grid->setContentsMargins(10, 10, 10, 10);
    tab8->setLayout(tab8Grid);
    tab8->setEnabled(false);

    testToolbox = new QToolBox;
    testToolbox->addItem(tab0, tr("&Spike count/removal"));
    testToolbox->addItem(tab1, tr("&Amplitude resolution"));
    testToolbox->addItem(tab2, tr("D&rop-outs"));
    testToolbox->addItem(tab3, tr("A&bsolute limits"));
    testToolbox->addItem(tab4, tr("S&kewness && kurtosis"));
    testToolbox->addItem(tab5, tr("D&iscontinuities"));
    testToolbox->addItem(tab6, tr("&Time lags"));
    testToolbox->addItem(tab7, tr("Angle of atta&ck"));
    testToolbox->addItem(tab8, tr("S&teadiness of horizontal wind"));
    testToolbox->setMinimumWidth(300);
    testToolbox->setMaximumWidth(1000);

    connect(testToolbox, &QToolBox::currentChanged,
            this, &AdvStatisticalOptions::updateThumbnailGraphLabel);
}

int AdvStatisticalOptions::findClosestEnabledTest(int indexDisabled)
{
    bool found = false;
    int lastItem = testToolbox->count();
    int i;

    // backward search
    for (i = indexDisabled - 1; i >= 0; --i)
    {
        if (testToolbox->isItemEnabled(i))
        {
            found = true;
            break;
        }
    }
    if (found)
        return i;

    // forward search
    found = false;
    for (i = indexDisabled + 1; i < lastItem; ++i)
    {
        if (testToolbox->isItemEnabled(i))
        {
            found = true;
            break;
        }
    }

    if (found)
        return i;
    else
        return 0;
}

namespace {

/// Move a grid's trailing stretch to \a row, clearing where a previous
/// rebuild put it. Left behind, an old stretch row keeps taking space that
/// now belongs to a gas row.
void setTrailingStretch(QGridLayout* grid, int row, int& previous)
{
    if (previous >= 0 && previous != row) { grid->setRowStretch(previous, 0); }
    grid->setRowStretch(row, 1);
    previous = row;
}

/// Take a generated widget out of its grid at once, rather than leaving it
/// there overlapping its replacement until the event loop runs.
void dropWidget(QWidget* w)
{
    if (!w) { return; }
    w->setParent(nullptr);
    w->deleteLater();
}

} // namespace

/// The record set the current rows describe. Rebuilding is skipped while this
/// is unchanged, so showing the page does not destroy a value being edited.
QString AdvStatisticalOptions::gasSignature() const
{
    QStringList parts;
    const auto& gases = ecProject_->gasColumns();
    for (int i = 0; i < gases.size(); ++i)
    {
        const auto& gas = gases.at(i);
        //> The unit belongs in the signature as well as the column: the
        //> absolute-limit boxes are scaled from it, so editing a column's unit
        //> in the metadata editor has to rebuild the rows or they keep a scale
        //> that no longer matches what the label says.
        parts << gas.slug + QLatin1Char('|') + gas.instrumentId
                 + QLatin1Char('|') + QString::number(gas.rawColumn)
                 + QLatin1Char('|') + absLimScale(i).display;
    }
    return parts.join(QLatin1Char(';'));
}

/// Row label: the species, qualified by the analyser when there is one, so
/// two CO2 records on different analysers are told apart.
QString AdvStatisticalOptions::gasRowLabel(int gasIndex) const
{
    return MeasurementRecords::gasLabel(ecProject_->gasColumns(), gasIndex);
}

/// Range and unit of an absolute-limit spin box, by species. CO2 is reported
/// as a mole fraction in umol/mol over a much higher range, H2O in mmol/mol;
/// every other gas takes the range the fourth-gas slot used to have.
void AdvStatisticalOptions::configureAbsLimSpin(QDoubleSpinBox* spin,
                                                int gasIndex) const
{
    const auto& gases = ecProject_->gasColumns();
    const QString slug = gasIndex >= 0 && gasIndex < gases.size()
            ? gases.at(gasIndex).slug : QString();
    const auto scale = absLimScale(gasIndex);

    //> The range is the stored-unit one scaled into whatever the column
    //> reports, so a gas reported in ppb gets a ppb range rather than a
    //> umol/mol range it can only reach the bottom of.
    double lower = 0.0;
    double upper = 1000.0;
    double step = 10.0;
    if (slug == QLatin1String("co2"))
    {
        lower = 100.0;
        upper = 10000.0;
        step = 50.0;
    }

    //> Three decimals of the stored unit was the old resolution, and it is the
    //> floor here: a ppb column keeps three decimals *of ppb*, a thousand
    //> times finer, which is what makes an ambient COS limit expressible at
    //> all. Capped because a spin box with fifteen decimals is unreadable.
    int decimals = 3;
    for (double f = scale.factor; f > 1.5 && decimals < 9; f /= 10.0)
    {
        ++decimals;
    }

    spin->setDecimals(decimals);
    spin->setAccelerated(true);
    spin->setRange(lower * scale.factor, upper * scale.factor);
    spin->setSingleStep(step * scale.factor);
    spin->setSuffix(tr("  [%1]", "").arg(scale.display));
}

/// Built-in default for a gas that carries no value of its own, chosen by
/// species rather than by position: a second CO2 record deserves the CO2
/// default, not the fourth-gas one.
double AdvStatisticalOptions::defaultGasParam(const QString& slug,
                                              GasParam param) const
{
    const auto& d = ecProject_->defaultSettings.screenParam;
    const bool isCo2 = (slug == QLatin1String("co2"));
    const bool isH2o = (slug == QLatin1String("h2o"));
    const bool isCh4 = (slug == QLatin1String("ch4"));

    switch (param)
    {
        case GasParam::SrLim:
            if (isCo2) { return d.sr_lim_co2; }
            if (isH2o) { return d.sr_lim_h2o; }
            if (isCh4) { return d.sr_lim_ch4; }
            return d.sr_lim_other;
        case GasParam::StepLim:
            //> Zero, deliberately: an absolute step limit cannot be guessed
            //> from the species. A carbon dioxide series in ppm and one in
            //> mol/mol want numbers six orders apart, and the unit is a
            //> property of the column rather than of the gas. Zero means "not
            //> despiked", which is the only safe thing to assume, and the run
            //> log names every column that carries it.
            return 0.0;
        case GasParam::AlMin:
            if (isCo2) { return d.al_co2_min; }
            if (isH2o) { return d.al_h2o_min; }
            if (isCh4) { return d.al_ch4_min; }
            return d.al_other_min;
        case GasParam::AlMax:
            if (isCo2) { return d.al_co2_max; }
            if (isH2o) { return d.al_h2o_max; }
            if (isCh4) { return d.al_ch4_max; }
            return d.al_other_max;
        case GasParam::DsHf:
            if (isCo2) { return d.ds_hf_co2; }
            if (isH2o) { return d.ds_hf_h2o; }
            if (isCh4) { return d.ds_hf_ch4; }
            return d.ds_hf_other;
        case GasParam::DsSf:
            if (isCo2) { return d.ds_sf_co2; }
            if (isH2o) { return d.ds_sf_h2o; }
            if (isCh4) { return d.ds_sf_ch4; }
            return d.ds_sf_other;
        case GasParam::TlDef:
            if (isCo2) { return d.tl_def_co2; }
            if (isH2o) { return d.tl_def_h2o; }
            if (isCh4) { return d.tl_def_ch4; }
            return d.tl_def_other;
    }
    return 0.0;
}

/// Value to show for one gas: the record's own if it has one, else the
/// species default.
///
/// The flat keys are gone. A project written before records carried these
/// per gas slot; EcProject::migrateLegacyGasSettings() moves them onto the
/// records as the project is upgraded, so by the time this runs the record
/// is the only place the value can be.
/// How the absolute limits for \a gasIndex are shown.
///
/// The record names a raw column and the raw file description states that
/// column's unit, so the two meet here. A record whose column the description
/// does not cover - not read yet, or edited away - falls back to the stored
/// unit, which is what the box always showed.
AbsoluteLimitUnits::Scale
AdvStatisticalOptions::absLimScale(int gasIndex) const
{
    const auto& gases = ecProject_->gasColumns();
    if (gasIndex < 0 || gasIndex >= gases.size())
    {
        return AbsoluteLimitUnits::forColumn(QString(), false);
    }

    const auto& gas = gases.at(gasIndex);
    const bool isWater = gas.slug == QLatin1String("h2o");

    QString unitToken;
    if (dlProject_ && gas.rawColumn > 0)
    {
        const auto variables = dlProject_->variables();
        const int index = gas.rawColumn - 1;
        if (variables && index >= 0 && index < variables->size())
        {
            unitToken = dlProject_->canonicalMeasureUnit(
                variables->at(index).inputUnit());
        }
    }
    return AbsoluteLimitUnits::forColumn(unitToken, isWater);
}

double AdvStatisticalOptions::gasParamFor(int gasIndex, GasParam param) const
{
    const auto& gases = ecProject_->gasColumns();
    if (gasIndex < 0 || gasIndex >= gases.size()) { return 0.0; }
    const auto& proc = gases.at(gasIndex).proc;

    //> Stored in umol/mol (mmol/mol for water) whatever the column says, so
    //> the two limits - and only those two - are scaled on the way out.
    const auto scale = absLimScale(gasIndex);

    switch (param)
    {
        case GasParam::SrLim: if (proc.srLim >= 0.0) { return proc.srLim; } break;
        case GasParam::StepLim: if (proc.stepLim >= 0.0) { return proc.stepLim; } break;
        case GasParam::AlMin: if (proc.alMin >= 0.0) { return proc.alMin * scale.factor; } break;
        case GasParam::AlMax: if (proc.alMax >= 0.0) { return proc.alMax * scale.factor; } break;
        case GasParam::DsHf:  if (proc.dsHf  >= 0.0) { return proc.dsHf;  } break;
        case GasParam::DsSf:  if (proc.dsSf  >= 0.0) { return proc.dsSf;  } break;
        case GasParam::TlDef: if (proc.tlDef >= 0.0) { return proc.tlDef; } break;
    }

    const auto fallback = defaultGasParam(gases.at(gasIndex).slug, param);
    if (param == GasParam::AlMin || param == GasParam::AlMax)
    {
        //> The species defaults are stored-unit values too, so they take the
        //> same scaling as a value the record already holds.
        return fallback * scale.factor;
    }
    return fallback;
}

/// Store one per-gas value on its record.
///
/// The record is the only place it goes. The flat keys the first four slots
/// used to mirror are retired; an upgraded project has had them moved onto
/// its records by EcProject::migrateLegacyGasSettings().
void AdvStatisticalOptions::onGasParamChanged(int gasIndex, GasParam param,
                                              double value)
{
    if (!ecProject_) { return; }
    auto gases = ecProject_->gasColumns();
    if (gasIndex < 0 || gasIndex >= gases.size()) { return; }

    //> Back to the stored unit. The reciprocal of what gasParamFor applies, so
    //> a value read out and written back unchanged stays where it was - which
    //> is what keeps opening and saving an existing project a no-op.
    const auto scale = absLimScale(gasIndex);

    switch (param)
    {
        case GasParam::SrLim: gases[gasIndex].proc.srLim = value; break;
        case GasParam::StepLim: gases[gasIndex].proc.stepLim = value; break;
        case GasParam::AlMin: gases[gasIndex].proc.alMin = value / scale.factor; break;
        case GasParam::AlMax: gases[gasIndex].proc.alMax = value / scale.factor; break;
        case GasParam::DsHf:  gases[gasIndex].proc.dsHf  = value; break;
        case GasParam::DsSf:  gases[gasIndex].proc.dsSf  = value; break;
        case GasParam::TlDef: gases[gasIndex].proc.tlDef = value; break;
    }
    ecProject_->setGasColumns(gases);
}

/// Restore Default Values, for the per-gas tables.
///
/// Only the records are written now. The flat keys used to be reset here too,
/// because a project with no gas records still carried them; with those keys
/// retired there is nothing left to reset but the records themselves.
void AdvStatisticalOptions::resetGasParamsToDefault()
{
    if (!ecProject_) { return; }

    const auto& gases = ecProject_->gasColumns();
    const GasParam params[] = { GasParam::SrLim, GasParam::StepLim,
                                GasParam::AlMin, GasParam::AlMax,
                                GasParam::DsHf,  GasParam::DsSf,
                                GasParam::TlDef };
    for (int i = 0; i < gases.size(); ++i)
    {
        const auto slug = gases.at(i).slug;
        for (const auto param : params)
        {
            onGasParamChanged(i, param, defaultGasParam(slug, param));
        }
    }
    rebuildGasRows();
}

/// One row per configured gas in each of the four per-gas tables.
///
/// Records with no raw column are skipped: they are slots kept so that the
/// engine's record-to-slot mapping stays put, not measurements. Values are
/// set before the spin boxes are connected, so rebuilding never writes back
/// into the project.
void AdvStatisticalOptions::rebuildGasRows()
{
    if (!ecProject_ || !tab0Grid_ || !tab3Grid_ || !tab5Grid_ || !tab6Grid_)
    {
        return;
    }

    for (const auto& row : srRows_) { dropWidget(row.label); dropWidget(row.spin); }
    for (const auto& row : stepRows_) { dropWidget(row.label); dropWidget(row.spin); }
    for (const auto& row : tlRows_) { dropWidget(row.label); dropWidget(row.spin); }
    for (const auto& row : alRows_)
    {
        dropWidget(row.label); dropWidget(row.first); dropWidget(row.second);
    }
    for (const auto& row : dsRows_)
    {
        dropWidget(row.label); dropWidget(row.first); dropWidget(row.second);
    }
    srRows_.clear();
    stepRows_.clear();
    alRows_.clear();
    dsRows_.clear();
    tlRows_.clear();

    const auto& gases = ecProject_->gasColumns();

    // First free row of each grid, after the fixed rows laid out in
    // createTabWidget().
    int srRow = 5;
    //> The step column starts level with its own four sonic spins above it,
    //> not with the sigma column, so the two tables read as the separate
    //> settings they are.
    int stepRow = 5;
    int alRow = 4;
    int dsRow = 4;
    int tlRow = 2;

    // Alphabetical, so all four tables read the way the gases are named rather
    // than the way they were added. The index stays the record index - that is
    // what every spin writes back through.
    for (const int i : MeasurementRecords::gasDisplayOrder(gases))
    {
        const int idx = i;
        const auto name = gasRowLabel(i);
        const auto slug = gases.at(i).slug;

        // --- spike plausibility range ---------------------------------
        {
            GasRow row;
            row.gasIndex = i;
            row.label = new ClickLabel(tr("%1 : ").arg(name));
            row.label->setToolTip(despGasTip_);
            row.spin = new QDoubleSpinBox;
            row.spin->setDecimals(1);
            row.spin->setRange(1.0, 20.0);
            row.spin->setSingleStep(0.1);
            row.spin->setAccelerated(true);
            row.spin->setSuffix(tr("  [%1]", "Sigma").arg(Defs::SIGMA));
            row.spin->setToolTip(despGasTip_);
            row.spin->setValue(gasParamFor(i, GasParam::SrLim));

            tab0Grid_->addWidget(row.label, srRow, 2, Qt::AlignRight);
            tab0Grid_->addWidget(row.spin, srRow, 3);
            ++srRow;

            auto spin = row.spin;
            connect(row.label, &ClickLabel::clicked, this, [=]()
                    { spin->setFocus(Qt::ShortcutFocusReason); spin->selectAll(); });
            connect(row.spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, [=](double v)
                    { onGasParamChanged(idx, GasParam::SrLim, v); });
            srRows_.append(row);
        }

        // --- consecutive-difference step limit -------------------------
        {
            GasRow row;
            row.gasIndex = i;
            row.label = new ClickLabel(tr("%1 : ").arg(name));
            row.label->setToolTip(stepGasTip_);
            row.spin = new QDoubleSpinBox;
            //> Six decimals for the same reason the file carries six: a
            //> species reported in ppb has a plausible step down at 1e-3,
            //> and one decimal would round it to "not despiked".
            row.spin->setDecimals(6);
            row.spin->setRange(0.0, 1000000.0);
            row.spin->setSingleStep(0.1);
            row.spin->setAccelerated(true);
            row.spin->setSpecialValueText(tr("not despiked"));
            row.spin->setToolTip(stepGasTip_);
            row.spin->setValue(gasParamFor(i, GasParam::StepLim));

            tab0Grid_->addWidget(row.label, stepRow, 4, Qt::AlignRight);
            tab0Grid_->addWidget(row.spin, stepRow, 5);
            ++stepRow;

            auto spin = row.spin;
            connect(row.label, &ClickLabel::clicked, this, [=]()
                    { spin->setFocus(Qt::ShortcutFocusReason); spin->selectAll(); });
            connect(row.spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, [=](double v)
                    { onGasParamChanged(idx, GasParam::StepLim, v); });
            stepRows_.append(row);
        }

        // --- absolute limits ------------------------------------------
        {
            GasPairRow row;
            row.gasIndex = i;
            row.label = new ClickLabel(tr("%1 : ").arg(name));
            row.first = new QDoubleSpinBox;
            row.second = new QDoubleSpinBox;
            configureAbsLimSpin(row.first, i);
            configureAbsLimSpin(row.second, i);
            row.first->setToolTip(absLimMinTip_);
            row.second->setToolTip(absLimMaxTip_);
            row.first->setValue(gasParamFor(i, GasParam::AlMin));
            row.second->setValue(gasParamFor(i, GasParam::AlMax));

            tab3Grid_->addWidget(row.label, alRow, 3, Qt::AlignRight);
            tab3Grid_->addWidget(row.first, alRow, 4);
            tab3Grid_->addWidget(row.second, alRow, 6);
            ++alRow;

            auto minSpin = row.first;
            auto maxSpin = row.second;
            connect(row.label, &ClickLabel::clicked, this, [=]()
                    { minSpin->setFocus(Qt::ShortcutFocusReason); minSpin->selectAll(); });
            connect(minSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, [=](double v)
                    {
                        onGasParamChanged(idx, GasParam::AlMin, v);
                        // min/max constraint
                        if (v >= maxSpin->value()) { maxSpin->setValue(v + 0.001); }
                    });
            connect(maxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, [=](double v)
                    {
                        onGasParamChanged(idx, GasParam::AlMax, v);
                        // min/max constraint
                        if (v <= minSpin->value()) { minSpin->setValue(v - 0.001); }
                    });
            alRows_.append(row);
        }

        // --- discontinuities, hard and soft flag ----------------------
        {
            GasPairRow row;
            row.gasIndex = i;
            row.label = new ClickLabel(tr("%1 : ").arg(name));
            const auto makeSpin = [](const QString& tip)
            {
                auto spin = new QDoubleSpinBox;
                spin->setDecimals(2);
                spin->setRange(0.0, 50.0);
                spin->setSingleStep(1.0);
                spin->setAccelerated(true);
                spin->setToolTip(tip);
                return spin;
            };
            row.first = makeSpin(dsHardTip_);
            row.second = makeSpin(dsSoftTip_);
            row.first->setValue(gasParamFor(i, GasParam::DsHf));
            row.second->setValue(gasParamFor(i, GasParam::DsSf));

            tab5Grid_->addWidget(row.label, dsRow, 2, Qt::AlignRight);
            tab5Grid_->addWidget(row.first, dsRow, 3);
            tab5Grid_->addWidget(row.second, dsRow, 4);
            ++dsRow;

            auto hardSpin = row.first;
            auto softSpin = row.second;
            connect(row.label, &ClickLabel::clicked, this, [=]()
                    { hardSpin->setFocus(Qt::ShortcutFocusReason); hardSpin->selectAll(); });
            connect(hardSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, [=](double v)
                    {
                        onGasParamChanged(idx, GasParam::DsHf, v);
                        // min/max constraint
                        if (v < softSpin->value()) { softSpin->setValue(v); }
                    });
            connect(softSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, [=](double v)
                    {
                        onGasParamChanged(idx, GasParam::DsSf, v);
                        // min/max constraint
                        if (v > hardSpin->value()) { hardSpin->setValue(v); }
                    });
            dsRows_.append(row);
        }

        // --- nominal time lag -----------------------------------------
        {
            GasRow row;
            row.gasIndex = i;
            row.label = new ClickLabel(tr("Nominal %1 time lag : ").arg(name));
            row.label->setToolTip(
                tr("<b>Nominal %1 time lag:</b> Set the nominal (best guess) "
                   "time lag for %1. You may want to use the same value "
                   "entered in the <b><i>Metadata File Editor</i></b>, or "
                   "written inside your GHG files.").arg(name));
            row.spin = new QDoubleSpinBox;
            row.spin->setDecimals(2);
            row.spin->setRange(0.0, 100.0);
            row.spin->setSingleStep(1.0);
            row.spin->setAccelerated(true);
            row.spin->setSuffix(tr("  [s]", "Seconds"));
            row.spin->setToolTip(row.label->toolTip());
            row.spin->setValue(gasParamFor(i, GasParam::TlDef));

            tab6Grid_->addWidget(row.label, tlRow, 2, Qt::AlignRight);
            tab6Grid_->addWidget(row.spin, tlRow, 3);
            ++tlRow;

            auto spin = row.spin;
            connect(row.label, &ClickLabel::clicked, this, [=]()
                    { spin->setFocus(Qt::ShortcutFocusReason); spin->selectAll(); });
            connect(row.spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, [=](double v)
                    { onGasParamChanged(idx, GasParam::TlDef, v); });
            tlRows_.append(row);
        }
    }

    // The rows that sit below each table move with the gas count.
    tab0Grid_->addWidget(despLabel_2, srRow, 2, Qt::AlignRight);
    tab0Grid_->addWidget(despSpin_2, srRow, 3);
    setTrailingStretch(tab0Grid_, srRow + 1, srStretchRow_);

    tab3Grid_->addWidget(absLimFilterCheckBox, alRow, 3, 1, 2);
    setTrailingStretch(tab3Grid_, alRow + 1, alStretchRow_);

    tab5Grid_->addWidget(discontLabel_8, dsRow, 2, Qt::AlignRight);
    tab5Grid_->addWidget(discontSpin_8, dsRow, 3);
    tab5Grid_->addWidget(discontSpin_16, dsRow, 4);
    setTrailingStretch(tab5Grid_, dsRow + 1, dsStretchRow_);

    setTrailingStretch(tab6Grid_, tlRow, tlStretchRow_);

    setSpikeGasRowsEnabled(!ecProject_->screenParamDespikeVm());
    gasSignature_ = gasSignature();
}

/// The spike table's gas rows follow the despiking method, like the fixed
/// rows around them. The sigma column is Vickers and Mahrt's; the step column
/// beside it belongs to the consecutive difference, so the two are enabled by
/// different methods and never both at once.
void AdvStatisticalOptions::setSpikeGasRowsEnabled(bool enabled)
{
    for (const auto& row : srRows_)
    {
        if (row.label) { row.label->setEnabled(enabled); }
        if (row.spin) { row.spin->setEnabled(enabled); }
    }
    const bool stepSelected =
        ecProject_ && ecProject_->screenParamDespikeVm() == 2;
    for (const auto& row : stepRows_)
    {
        if (row.label) { row.label->setEnabled(stepSelected); }
        if (row.spin) { row.spin->setEnabled(stepSelected); }
    }
}

void AdvStatisticalOptions::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    // The gas records can change while another page is in front.
    if (gasSignature() != gasSignature_) { rebuildGasRows(); }
}

void AdvStatisticalOptions::on_spikeRemCheckBox_clicked(bool checked)
{
    if (checked)
    {
        testToolbox->setEnabled(checked);
    }
    testToolbox->setItemEnabled(0, checked);
    testToolbox->widget(0)->setEnabled(checked);

    if (checked)
    {
        testToolbox->setCurrentWidget(testToolbox->widget(0));
    }
    else
    {
        testToolbox->setCurrentWidget(testToolbox->widget(findClosestEnabledTest(0)));
    }
}

void AdvStatisticalOptions::on_amplitudeResCheckBox_clicked(bool checked)
{
    if (checked)
    {
        testToolbox->setEnabled(checked);
    }
    testToolbox->setItemEnabled(1, checked);
    testToolbox->widget(1)->setEnabled(checked);
    testToolbox->setCurrentWidget(testToolbox->widget(1));

    if (checked)
    {
        testToolbox->setCurrentWidget(testToolbox->widget(1));
    }
    else
    {
        testToolbox->setCurrentWidget(testToolbox->widget(findClosestEnabledTest(1)));
    }
}

void AdvStatisticalOptions::on_dropoutsCheckBox_clicked(bool checked)
{
    if (checked)
    {
        testToolbox->setEnabled(checked);
    }
    testToolbox->setItemEnabled(2, checked);
    testToolbox->widget(2)->setEnabled(checked);
    testToolbox->setCurrentWidget(testToolbox->widget(2));

    if (checked)
    {
        testToolbox->setCurrentWidget(testToolbox->widget(2));
    }
    else
    {
        testToolbox->setCurrentWidget(testToolbox->widget(findClosestEnabledTest(2)));
    }
}

void AdvStatisticalOptions::on_absLimCheckBox_clicked(bool checked)
{
    if (checked)
    {
        testToolbox->setEnabled(checked);
    }
    testToolbox->setItemEnabled(3, checked);
    testToolbox->widget(3)->setEnabled(checked);
    testToolbox->setCurrentWidget(testToolbox->widget(3));

    if (checked)
    {
        testToolbox->setCurrentWidget(testToolbox->widget(3));
    }
    else
    {
        testToolbox->setCurrentWidget(testToolbox->widget(findClosestEnabledTest(3)));
    }
}

void AdvStatisticalOptions::on_skewnessCheckBox_clicked(bool checked)
{
    if (checked)
    {
        testToolbox->setEnabled(checked);
    }
    testToolbox->setItemEnabled(4, checked);
    testToolbox->widget(4)->setEnabled(checked);
    testToolbox->setCurrentWidget(testToolbox->widget(4));

    if (checked)
    {
        testToolbox->setCurrentWidget(testToolbox->widget(4));
    }
    else
    {
        testToolbox->setCurrentWidget(testToolbox->widget(findClosestEnabledTest(4)));
    }
}

void AdvStatisticalOptions::on_discontCheckBox_clicked(bool checked)
{
    if (checked)
    {
        testToolbox->setEnabled(checked);
    }
    testToolbox->setItemEnabled(5, checked);
    testToolbox->widget(5)->setEnabled(checked);
    testToolbox->setCurrentWidget(testToolbox->widget(5));

    if (checked)
    {
        testToolbox->setCurrentWidget(testToolbox->widget(5));
    }
    else
    {
        testToolbox->setCurrentWidget(testToolbox->widget(findClosestEnabledTest(5)));
    }
}

void AdvStatisticalOptions::on_timeLagCheckBox_clicked(bool checked)
{
    if (checked)
    {
        testToolbox->setEnabled(checked);
    }
    testToolbox->setItemEnabled(6, checked);
    testToolbox->widget(6)->setEnabled(checked);
    testToolbox->setCurrentWidget(testToolbox->widget(6));

    if (checked)
    {
        testToolbox->setCurrentWidget(testToolbox->widget(6));
    }
    else
    {
        testToolbox->setCurrentWidget(testToolbox->widget(findClosestEnabledTest(6)));
    }
}

void AdvStatisticalOptions::on_attackAngleCheckBox_clicked(bool checked)
{
    if (checked)
    {
        testToolbox->setEnabled(checked);
    }
    testToolbox->setItemEnabled(7, checked);
    testToolbox->widget(7)->setEnabled(checked);
    testToolbox->setCurrentWidget(testToolbox->widget(7));

    if (checked)
    {
        testToolbox->setCurrentWidget(testToolbox->widget(7));
    }
    else
    {
        testToolbox->setCurrentWidget(testToolbox->widget(findClosestEnabledTest(7)));
    }
}


void AdvStatisticalOptions::on_nonSteadyCheckBox_clicked(bool checked)
{
    if (checked)
    {
        testToolbox->setEnabled(checked);
    }
    testToolbox->setItemEnabled(8, checked);
    testToolbox->widget(8)->setEnabled(checked);
    testToolbox->setCurrentWidget(testToolbox->widget(8));

    if (checked)
    {
        testToolbox->setCurrentWidget(testToolbox->widget(8));
    }
    else
    {
        testToolbox->setCurrentWidget(testToolbox->widget(findClosestEnabledTest(8)));
    }
}

void AdvStatisticalOptions::setTestDefaultValues()
{
    vickersDespikingRadio->setChecked(true);
    stepSpin_u->setValue(ecProject_->defaultSettings.screenParam.sr_step_u);
    stepSpin_v->setValue(ecProject_->defaultSettings.screenParam.sr_step_v);
    stepSpin_w->setValue(ecProject_->defaultSettings.screenParam.sr_step_w);
    stepSpin_ts->setValue(ecProject_->defaultSettings.screenParam.sr_step_ts);
    despikingRadioClicked(ecProject_->defaultSettings.screenParam.despike_vm);

    despSpin_1->setValue(ecProject_->defaultSettings.screenParam.sr_num_spk);
    despSpin_2->setValue(ecProject_->defaultSettings.screenParam.sr_lim_u);
    despSpin_3->setValue(ecProject_->defaultSettings.screenParam.sr_lim_w);
    despSpin_8->setValue(ecProject_->defaultSettings.screenParam.sr_lim_hf);
    despFilterCheckBox->setChecked(ecProject_->defaultSettings.screenSetting.filter_sr);
    updateParamSrNumSpk(ecProject_->defaultSettings.screenParam.sr_num_spk);
    updateParamSrULim(ecProject_->defaultSettings.screenParam.sr_lim_u);
    updateParamSrWLim(ecProject_->defaultSettings.screenParam.sr_lim_w);
    updateParamSrHfLim(ecProject_->defaultSettings.screenParam.sr_lim_hf);
    updateDespFilter(ecProject_->defaultSettings.screenSetting.filter_sr);
    amplResSpin_1->setValue(ecProject_->defaultSettings.screenParam.ar_lim);
    amplResSpin_2->setValue(ecProject_->defaultSettings.screenParam.ar_bins);
    amplResSpin_3->setValue(ecProject_->defaultSettings.screenParam.ar_hf_lim);
    updateParamArLim(ecProject_->defaultSettings.screenParam.ar_lim);
    updateParamArBins(ecProject_->defaultSettings.screenParam.ar_bins);
    updateParamArHfLim(ecProject_->defaultSettings.screenParam.ar_hf_lim);

    dropoutsSpin_1->setValue(ecProject_->defaultSettings.screenParam.do_extlim_dw);
    dropoutsSpin_2->setValue(ecProject_->defaultSettings.screenParam.do_hf1_lim);
    dropoutsSpin_3->setValue(ecProject_->defaultSettings.screenParam.do_hf2_lim);
    updateParamDoExtLimDw(ecProject_->defaultSettings.screenParam.do_extlim_dw);
    updateParamDoHf1Lim(ecProject_->defaultSettings.screenParam.do_hf1_lim);
    updateParamDoHf2Lim(ecProject_->defaultSettings.screenParam.do_hf2_lim);

    absLimSpin_1->setValue(ecProject_->defaultSettings.screenParam.al_u_max);
    absLimSpin_2->setValue(ecProject_->defaultSettings.screenParam.al_w_max);
    absLimSpin_3->setValue(ecProject_->defaultSettings.screenParam.al_tson_min);
    absLimSpin_4->setValue(ecProject_->defaultSettings.screenParam.al_tson_max);
    absLimFilterCheckBox->setChecked(ecProject_->defaultSettings.screenSetting.filter_al);
    updateParamAlUMax(ecProject_->defaultSettings.screenParam.al_u_max);
    updateParamAlWMax(ecProject_->defaultSettings.screenParam.al_w_max);
    updateParamAlTsonMin(ecProject_->defaultSettings.screenParam.al_tson_min);
    updateParamAlTsonMax(ecProject_->defaultSettings.screenParam.al_tson_max);
    updateAbsLimFilter(ecProject_->defaultSettings.screenSetting.filter_al);

    skewnessSpin_1->setValue(ecProject_->defaultSettings.screenParam.sk_hf_skmin);
    skewnessSpin_2->setValue(ecProject_->defaultSettings.screenParam.sk_hf_skmax);
    skewnessSpin_3->setValue(ecProject_->defaultSettings.screenParam.sk_sf_skmin);
    skewnessSpin_4->setValue(ecProject_->defaultSettings.screenParam.sk_sf_skmax);
    skewnessSpin_5->setValue(ecProject_->defaultSettings.screenParam.sk_hf_kumin);
    skewnessSpin_6->setValue(ecProject_->defaultSettings.screenParam.sk_hf_kumax);
    skewnessSpin_7->setValue(ecProject_->defaultSettings.screenParam.sk_sf_kumin);
    skewnessSpin_8->setValue(ecProject_->defaultSettings.screenParam.sk_sf_kumax);
    updateParamSkHfSkmin(ecProject_->defaultSettings.screenParam.sk_hf_skmin);
    updateParamSkHfSkmax(ecProject_->defaultSettings.screenParam.sk_hf_skmax);
    updateParamSkSfSkmin(ecProject_->defaultSettings.screenParam.sk_sf_skmin);
    updateParamSkSfSkmax(ecProject_->defaultSettings.screenParam.sk_sf_skmax);
    updateParamSkHfKumin(ecProject_->defaultSettings.screenParam.sk_hf_kumin);
    updateParamSkHfKumax(ecProject_->defaultSettings.screenParam.sk_hf_kumax);
    updateParamSkSfKumin(ecProject_->defaultSettings.screenParam.sk_sf_kumin);
    updateParamSkSfKumax(ecProject_->defaultSettings.screenParam.sk_sf_kumax);

    discontSpin_1->setValue(ecProject_->defaultSettings.screenParam.ds_hf_uv);
    discontSpin_2->setValue(ecProject_->defaultSettings.screenParam.ds_hf_w);
    discontSpin_3->setValue(ecProject_->defaultSettings.screenParam.ds_hf_t);
    discontSpin_8->setValue(ecProject_->defaultSettings.screenParam.ds_hf_var);
    discontSpin_9->setValue(ecProject_->defaultSettings.screenParam.ds_sf_uv);
    discontSpin_10->setValue(ecProject_->defaultSettings.screenParam.ds_sf_w);
    discontSpin_11->setValue(ecProject_->defaultSettings.screenParam.ds_sf_t);
    discontSpin_16->setValue(ecProject_->defaultSettings.screenParam.ds_sf_var);
    updateParamDsHfUV(ecProject_->defaultSettings.screenParam.ds_hf_uv);
    updateParamDsHfW(ecProject_->defaultSettings.screenParam.ds_hf_w);
    updateParamDsHfT(ecProject_->defaultSettings.screenParam.ds_hf_t);
    updateParamDsHfVar(ecProject_->defaultSettings.screenParam.ds_hf_var);
    updateParamDsSfUV(ecProject_->defaultSettings.screenParam.ds_sf_uv);
    updateParamDsSfW(ecProject_->defaultSettings.screenParam.ds_sf_w);
    updateParamDsSfT(ecProject_->defaultSettings.screenParam.ds_sf_t);
    updateParamDsSfVar(ecProject_->defaultSettings.screenParam.ds_sf_var);

    timeLagSpin_1->setValue(ecProject_->defaultSettings.screenParam.tl_hf_lim);
    timeLagSpin_2->setValue(ecProject_->defaultSettings.screenParam.tl_sf_lim);
    updateParamTlHfLim(ecProject_->defaultSettings.screenParam.tl_hf_lim);
    updateParamTlSfLim(ecProject_->defaultSettings.screenParam.tl_sf_lim);

    // Every per-gas value goes back to the built-in default for its species,
    // on its record.
    resetGasParamsToDefault();

    attackAngleSpin_1->setValue(ecProject_->defaultSettings.screenParam.aa_min);
    attackAngleSpin_2->setValue(ecProject_->defaultSettings.screenParam.aa_max);
    attackAngleSpin_3->setValue(ecProject_->defaultSettings.screenParam.aa_lim);
    updateParamAaMin(ecProject_->defaultSettings.screenParam.aa_min);
    updateParamAaMax(ecProject_->defaultSettings.screenParam.aa_max);
    updateParamAaLim(ecProject_->defaultSettings.screenParam.aa_lim);

    nonSteadySpin_1->setValue(ecProject_->defaultSettings.screenParam.ns_hf_lim);
    updateParamNsHfLim(ecProject_->defaultSettings.screenParam.ns_hf_lim);
}

void AdvStatisticalOptions::on_defaultValuesButton_clicked()
{
    if (requestTestSettingsReset())
    {
        setTestDefaultValues();
    }
}

void AdvStatisticalOptions::updateTestSr(bool b)
{
    ecProject_->setScreenTestSr(b);
}

void AdvStatisticalOptions::updateTestAr(bool b)
{
    ecProject_->setScreenTestAr(b);
}

void AdvStatisticalOptions::updateTestDo(bool b)
{
    ecProject_->setScreenTestDo(b);
}

void AdvStatisticalOptions::updateTestAl(bool b)
{
    ecProject_->setScreenTestAl(b);
}

void AdvStatisticalOptions::updateTestSk(bool b)
{
    ecProject_->setScreenTestSk(b);
}

void AdvStatisticalOptions::updateTestDs(bool b)
{
    ecProject_->setScreenTestDs(b);
}

void AdvStatisticalOptions::updateTestTl(bool b)
{
    ecProject_->setScreenTestTl(b);
}

void AdvStatisticalOptions::updateTestAa(bool b)
{
    ecProject_->setScreenTestAa(b);
}

void AdvStatisticalOptions::updateTestNs(bool b)
{
    ecProject_->setScreenTestNs(b);
}

void AdvStatisticalOptions::updateTestRf(bool b)
{
    ecProject_->setScreenTestRf(b);
}

void AdvStatisticalOptions::updateParamSrNumSpk(int n)
{
    ecProject_->setScreenParamSrNumSpk(n);
}

void AdvStatisticalOptions::updateParamSrULim(double n)
{
    ecProject_->setScreenParamSrULim(n);
}

void AdvStatisticalOptions::updateParamSrWLim(double n)
{
    ecProject_->setScreenParamSrWLim(n);
}

void AdvStatisticalOptions::updateParamSrHfLim(double n)
{
    ecProject_->setScreenParamSrHfLim(n);
}

void AdvStatisticalOptions::updateParamArLim(double n)
{
    ecProject_->setScreenParamArLim(n);
}

void AdvStatisticalOptions::updateParamArBins(int n)
{
    ecProject_->setScreenParamArBins(n);
}

void AdvStatisticalOptions::updateParamArHfLim(int n)
{
    ecProject_->setScreenParamArHfLim(n);
}

void AdvStatisticalOptions::updateParamDoExtLimDw(int n)
{
    ecProject_->setScreenParamDoExtLimDw(n);
}

void AdvStatisticalOptions::updateParamDoHf1Lim(double n)
{
    ecProject_->setScreenParamDoHf1Lim(n);
}

void AdvStatisticalOptions::updateParamDoHf2Lim(double n)
{
    ecProject_->setScreenParamDoHf2Lim(n);
}

void AdvStatisticalOptions::updateParamAlUMax(double n)
{
    ecProject_->setScreenParamAlUMax(n);
    absLimSpin_13->setValue(-n);
}

void AdvStatisticalOptions::updateParamAlWMax(double n)
{
    ecProject_->setScreenParamAlWMax(n);
    absLimSpin_14->setValue(-n);
}

void AdvStatisticalOptions::updateParamAlUMin(double n)
{
    absLimSpin_1->setValue(-n);
}

void AdvStatisticalOptions::updateParamAlWMin(double n)
{
    absLimSpin_2->setValue(-n);
}

void AdvStatisticalOptions::updateParamAlTsonMin(double n)
{
    ecProject_->setScreenParamAlTsonMin(n);

    // min/max constraint
    if (n >= absLimSpin_4->value())
    {
        absLimSpin_4->setValue(n + 0.1);
    }
}

void AdvStatisticalOptions::updateParamAlTsonMax(double n)
{
    ecProject_->setScreenParamAlTsonMax(n);

    // min/max constraint
    if (n <= absLimSpin_3->value())
    {
        absLimSpin_3->setValue(n - 0.1);
    }
}

void AdvStatisticalOptions::updateParamSkHfSkmin(double n)
{
    ecProject_->setScreenParamSkHfSkmin(n);

    // min/max constraint
    if (n > skewnessSpin_3->value())
    {
        skewnessSpin_3->setValue(n);
    }
}

void AdvStatisticalOptions::updateParamSkHfSkmax(double n)
{
    ecProject_->setScreenParamSkHfSkmax(n);

    // min/max constraint
    if (n < skewnessSpin_4->value())
    {
        skewnessSpin_4->setValue(n);
    }
}

void AdvStatisticalOptions::updateParamSkSfSkmin(double n)
{
    ecProject_->setScreenParamSkSfSkmin(n);

    // min/max constraint
    if (n < skewnessSpin_1->value())
    {
        skewnessSpin_1->setValue(n);
    }
}

void AdvStatisticalOptions::updateParamSkSfSkmax(double n)
{
    ecProject_->setScreenParamSkSfSkmax(n);

    // min/max constraint
    if (n > skewnessSpin_2->value())
    {
        skewnessSpin_2->setValue(n);
    }
}

void AdvStatisticalOptions::updateParamSkHfKumin(double n)
{
    ecProject_->setScreenParamSkHfKumin(n);

    // min/max constraint
    if (n > skewnessSpin_7->value())
    {
        skewnessSpin_7->setValue(n);
    }
}

void AdvStatisticalOptions::updateParamSkHfKumax(double n)
{
    ecProject_->setScreenParamSkHfKumax(n);

    // min/max constraint
    if (n < skewnessSpin_8->value())
    {
        skewnessSpin_8->setValue(n);
    }
}

void AdvStatisticalOptions::updateParamSkSfKumin(double n)
{
    ecProject_->setScreenParamSkSfKumin(n);

    // min/max constraint
    if (n < skewnessSpin_5->value())
    {
        skewnessSpin_5->setValue(n);
    }
}

void AdvStatisticalOptions::updateParamSkSfKumax(double n)
{
    ecProject_->setScreenParamSkSfKumax(n);

    // min/max constraint
    if (n > skewnessSpin_6->value())
    {
        skewnessSpin_6->setValue(n);
    }
}

void AdvStatisticalOptions::updateParamDsHfUV(double n)
{
    ecProject_->setScreenParamDsHfUV(n);

    // min/max constraint
    if (n < discontSpin_9->value())
    {
        discontSpin_9->setValue(n);
    }
}

void AdvStatisticalOptions::updateParamDsHfW(double n)
{
    ecProject_->setScreenParamDsHfW(n);

    // min/max constraint
    if (n < discontSpin_10->value())
    {
        discontSpin_10->setValue(n);
    }
}

void AdvStatisticalOptions::updateParamDsHfT(double n)
{
    ecProject_->setScreenParamDsHfT(n);

    // min/max constraint
    if (n < discontSpin_11->value())
    {
        discontSpin_11->setValue(n);
    }
}

void AdvStatisticalOptions::updateParamDsHfVar(double n)
{
    ecProject_->setScreenParamDsHfVar(n);

    // min/max constraint
    if (n < discontSpin_16->value())
    {
        discontSpin_16->setValue(n);
    }
}

void AdvStatisticalOptions::updateParamDsSfUV(double n)
{
    ecProject_->setScreenParamDsSfUV(n);

    // min/max constraint
    if (n > discontSpin_1->value())
    {
        discontSpin_1->setValue(n);
    }
}

void AdvStatisticalOptions::updateParamDsSfW(double n)
{
    ecProject_->setScreenParamDsSfW(n);

    // min/max constraint
    if (n > discontSpin_2->value())
    {
        discontSpin_2->setValue(n);
    }
}

void AdvStatisticalOptions::updateParamDsSfT(double n)
{
    ecProject_->setScreenParamDsSfT(n);

    // min/max constraint
    if (n > discontSpin_3->value())
    {
        discontSpin_3->setValue(n);
    }
}

void AdvStatisticalOptions::updateParamDsSfVar(double n)
{
    ecProject_->setScreenParamDsSfVar(n);

    // min/max constraint
    if (n > discontSpin_8->value())
    {
        discontSpin_8->setValue(n);
    }
}

void AdvStatisticalOptions::updateParamTlHfLim(double n)
{
    ecProject_->setScreenParamTlHfLim(n);

    // min/max constraint
    if (n < timeLagSpin_2->value())
    {
        timeLagSpin_2->setValue(n);
    }
}

void AdvStatisticalOptions::updateParamTlSfLim(double n)
{
    ecProject_->setScreenParamTlSfLim(n);

    // min/max constraint
    if (n > timeLagSpin_1->value())
    {
        timeLagSpin_1->setValue(n);
    }
}

void AdvStatisticalOptions::updateParamAaMin(double n)
{
    ecProject_->setScreenParamAaMin(n);
}

void AdvStatisticalOptions::updateParamAaMax(double n)
{
    ecProject_->setScreenParamAaMax(n);
}

void AdvStatisticalOptions::updateParamAaLim(double n)
{
    ecProject_->setScreenParamAaLim(n);
}

void AdvStatisticalOptions::updateParamNsHfLim(double n)
{
    ecProject_->setScreenParamNsHfLim(n);
}

void AdvStatisticalOptions::reset()
{
    // save the modified flag to prevent side effects of setting widgets
    bool oldmod = ecProject_->modified();
    ecProject_->blockSignals(true);

    setTestDefaultValues();

    spikeRemCheckBox->setChecked(ecProject_->defaultSettings.screenTest.test_sr);
    amplitudeResCheckBox->setChecked(ecProject_->defaultSettings.screenTest.test_ar);
    dropoutsCheckBox->setChecked(ecProject_->defaultSettings.screenTest.test_do);
    absLimCheckBox->setChecked(ecProject_->defaultSettings.screenTest.test_al);
    skewnessCheckBox->setChecked(ecProject_->defaultSettings.screenTest.test_sk);
    discontCheckBox->setChecked(ecProject_->defaultSettings.screenTest.test_ds);
    timeLagCheckBox->setChecked(ecProject_->defaultSettings.screenTest.test_tl);
    attackAngleCheckBox->setChecked(ecProject_->defaultSettings.screenTest.test_aa);
    nonSteadyCheckBox->setChecked(ecProject_->defaultSettings.screenTest.test_ns);
    rfluxDiagCheckBox->setChecked(ecProject_->defaultSettings.screenTest.test_rf);

    testToolbox->setCurrentIndex(0);
    //> Restore Default Values reaches every group on the page, so the
    //> detection limit has to be reset here too or it survives the reset.
    ecProject_->setScreenDetlimMethod(ecProject_->defaultSettings.screenSetting.detlim_meth);
    ecProject_->setScreenDetlimOffset(ecProject_->defaultSettings.screenSetting.detlim_offset_s);
    ecProject_->setScreenDetlimWindow(ecProject_->defaultSettings.screenSetting.detlim_window_s);
    if (detlimDialog_) { detlimDialog_->refresh(); }

    randomErrorCheckBox->setChecked(false);
    randomMethodLabel->setEnabled(false);
    randomMethodCombo->setEnabled(false);
    itsDefinitionLabel->setEnabled(false);
    itsDefinitionCombo->setEnabled(false);
    timelagMaxLabel->setEnabled(false);
    timelagMaxSpin->setEnabled(false);
    securityCoeffLabel->setEnabled(false);
    securityCoeffSpin->setEnabled(false);
    WidgetUtils::resetComboToItem(randomMethodCombo, 0);
    WidgetUtils::resetComboToItem(itsDefinitionCombo, 0);

    // NOTE: hack to prevent side effect setting from calling
    // WidgetUtils::resetComboToItem(randomMethodCombo, 0)
    ecProject_->setRandomErrorMethod(ecProject_->defaultSettings.randomError.ru_method);

    timelagMaxSpin->setValue(ecProject_->defaultSettings.projectGeneral.its_tlag_max);
    securityCoeffSpin->setValue(ecProject_->defaultSettings.randomError.its_sec_factor);

    // restore modified flag
    ecProject_->setModified(oldmod);
    ecProject_->blockSignals(false);
}

/// Created on first use rather than in the constructor.
///
/// The page is built before a project is necessarily loaded, and the dialog
/// reads the project the moment it refreshes. Constructing it lazily also
/// keeps it out of the way for the many sessions that never open it.
void AdvStatisticalOptions::showDetlimSettingsDialog()
{
    if (!detlimDialog_)
    {
        detlimDialog_ = new DetlimSettingsDialog(this, ecProject_);
    }
    detlimDialog_->refresh();
    detlimDialog_->show();
    detlimDialog_->raise();
    detlimDialog_->activateWindow();
}

void AdvStatisticalOptions::refresh()
{
    // save the modified flag to prevent side effects of setting widgets
    bool oldmod = ecProject_->modified();
    ecProject_->blockSignals(true);

    //> Only if it has been opened. A project loaded while the dialog is up
    //> would otherwise leave it showing the previous project's windows.
    if (detlimDialog_) { detlimDialog_->refresh(); }

    spikeRemCheckBox->setChecked(ecProject_->screenTestSr());
    amplitudeResCheckBox->setChecked(ecProject_->screenTestAr());
    dropoutsCheckBox->setChecked(ecProject_->screenTestDo());
    absLimCheckBox->setChecked(ecProject_->screenTestAl());
    skewnessCheckBox->setChecked(ecProject_->screenTestSk());
    discontCheckBox->setChecked(ecProject_->screenTestDs());
    timeLagCheckBox->setChecked(ecProject_->screenTestTl());
    attackAngleCheckBox->setChecked(ecProject_->screenTestAa());
    nonSteadyCheckBox->setChecked(ecProject_->screenTestNs());
    rfluxDiagCheckBox->setChecked(ecProject_->screenTestRf());
    postFluxDespikeCheckBox->setChecked(ecProject_->generalTestPfd());

    selectAllCheckBox->blockSignals(true);
    selectAllCheckBox->setChecked(areAllCheckedTests());
    selectAllCheckBox->blockSignals(false);

    despSpin_1->setValue(ecProject_->screenParamSrNumSpk());
    despSpin_8->setValue(ecProject_->screenParamSrHfLim());
    despSpin_3->setValue(ecProject_->screenParamSrWLim());
    despSpin_2->setValue(ecProject_->screenParamSrULim());
    despFilterCheckBox->setChecked(ecProject_->screenFilterSr());

    // Rebuilt here rather than only on show: a project load can change the
    // gas records under the rows, and the values below come from them.
    rebuildGasRows();

    //> The id IS the ini value, so an unknown one falls to Vickers - which is
    //> where the engine's own reader sends '0' and nothing else it knows.
    switch (ecProject_->screenParamDespikeVm())
    {
    case 1:
        mauderDespikingRadio->setChecked(true);
        break;
    case 2:
        stepDespikingRadio->setChecked(true);
        break;
    default:
        vickersDespikingRadio->setChecked(true);
        break;
    }
    stepSpin_u->setValue(ecProject_->screenParamSrStepU());
    stepSpin_v->setValue(ecProject_->screenParamSrStepV());
    stepSpin_w->setValue(ecProject_->screenParamSrStepW());
    stepSpin_ts->setValue(ecProject_->screenParamSrStepTs());
    despikingRadioClicked(ecProject_->screenParamDespikeVm());

    amplResSpin_1->setValue(ecProject_->screenParamArLim());
    amplResSpin_2->setValue(ecProject_->screenParamArBins());
    amplResSpin_3->setValue(ecProject_->screenParamArHfLim());

    dropoutsSpin_1->setValue(ecProject_->screenParamDoExtLimDw());
    dropoutsSpin_2->setValue(ecProject_->screenParamDoHf1Lim());
    dropoutsSpin_3->setValue(ecProject_->screenParamDoHf2Lim());

    absLimSpin_1->setValue(ecProject_->screenParamAlUMax());
    absLimSpin_2->setValue(ecProject_->screenParamAlWMax());
    absLimSpin_3->setValue(ecProject_->screenParamAlTsonMin());
    absLimSpin_4->setValue(ecProject_->screenParamAlTsonMax());
    absLimFilterCheckBox->setChecked(ecProject_->screenFilterAl());

    skewnessSpin_1->setValue(ecProject_->screenParamSkHfSkmin());
    skewnessSpin_2->setValue(ecProject_->screenParamSkHfSkmax());
    skewnessSpin_3->setValue(ecProject_->screenParamSkSfSkmin());
    skewnessSpin_4->setValue(ecProject_->screenParamSkSfSkmax());
    skewnessSpin_5->setValue(ecProject_->screenParamSkHfKumin());
    skewnessSpin_6->setValue(ecProject_->screenParamSkHfKumax());
    skewnessSpin_7->setValue(ecProject_->screenParamSkSfKumin());
    skewnessSpin_8->setValue(ecProject_->screenParamSkSfKumax());

    discontSpin_1->setValue(ecProject_->screenParamDsHfUV());
    discontSpin_2->setValue(ecProject_->screenParamDsHfW());
    discontSpin_3->setValue(ecProject_->screenParamDsHfT());
    discontSpin_8->setValue(ecProject_->screenParamDsHfVar());
    discontSpin_9->setValue(ecProject_->screenParamDsSfUV());
    discontSpin_10->setValue(ecProject_->screenParamDsSfW());
    discontSpin_11->setValue(ecProject_->screenParamDsSfT());
    discontSpin_16->setValue(ecProject_->screenParamDsSfVar());

    timeLagSpin_1->setValue(ecProject_->screenParamTlHfLim());
    timeLagSpin_2->setValue(ecProject_->screenParamTlSfLim());

    attackAngleSpin_1->setValue(ecProject_->screenParamAaMin());
    attackAngleSpin_2->setValue(ecProject_->screenParamAaMax());
    attackAngleSpin_3->setValue(ecProject_->screenParamAaLim());

    nonSteadySpin_1->setValue(ecProject_->screenParamNsHfLim());

    auto randomError = ecProject_->randErrorMethod();
    randomErrorCheckBox->setChecked(randomError);
    randomMethodLabel->setEnabled(randomErrorCheckBox->isChecked());
    randomMethodCombo->setEnabled(randomErrorCheckBox->isChecked());
    if (randomError)
    {
        //> By stored value, not by row: ru_meth 4 is Billesbach and sits on
        //> row 2, because 3 is Mahrt and this menu does not offer it. A
        //> project holding 3 finds no row and keeps the one it had.
        const int row = randomMethodCombo->findData(randomError);
        if (row >= 0) { randomMethodCombo->setCurrentIndex(row); }
    }
    else
    {
        randomMethodCombo->setCurrentIndex(0);
    }

    itsDefinitionLabel->setEnabled(randomErrorCheckBox->isChecked());
    itsDefinitionCombo->setEnabled(randomErrorCheckBox->isChecked());
    timelagMaxLabel->setEnabled(randomErrorCheckBox->isChecked());
    timelagMaxSpin->setEnabled(randomErrorCheckBox->isChecked());
    securityCoeffLabel->setEnabled(randomErrorCheckBox->isChecked());
    securityCoeffSpin->setEnabled(randomErrorCheckBox->isChecked());
    itsDefinitionCombo->setCurrentIndex(ecProject_->randErrorItsMehod());
    timelagMaxSpin->setValue(ecProject_->randErrorTlagMax());
    securityCoeffSpin->setValue(ecProject_->randErrorSecFactor());

    // restore modified flag
    ecProject_->setModified(oldmod);
    ecProject_->blockSignals(false);
}

bool AdvStatisticalOptions::atLeastOneCheckedTest()
{
    return (spikeRemCheckBox->isChecked()
            || amplitudeResCheckBox->isChecked()
            || dropoutsCheckBox->isChecked()
            || absLimCheckBox->isChecked()
            || skewnessCheckBox->isChecked()
            || discontCheckBox->isChecked()
            || timeLagCheckBox->isChecked()
            || attackAngleCheckBox->isChecked()
            || nonSteadyCheckBox->isChecked());
}

bool AdvStatisticalOptions::areAllCheckedTests()
{
    return (spikeRemCheckBox->isChecked()
            && amplitudeResCheckBox->isChecked()
            && dropoutsCheckBox->isChecked()
            && absLimCheckBox->isChecked()
            && skewnessCheckBox->isChecked()
            && discontCheckBox->isChecked()
            && timeLagCheckBox->isChecked()
            && attackAngleCheckBox->isChecked()
            && nonSteadyCheckBox->isChecked());
}

void AdvStatisticalOptions::selectAllTest(bool b)
{
    spikeRemCheckBox->setChecked(b);
    amplitudeResCheckBox->setChecked(b);
    dropoutsCheckBox->setChecked(b);
    absLimCheckBox->setChecked(b);
    skewnessCheckBox->setChecked(b);
    discontCheckBox->setChecked(b);
    timeLagCheckBox->setChecked(b);
    attackAngleCheckBox->setChecked(b);
    nonSteadyCheckBox->setChecked(b);
}

void AdvStatisticalOptions::updateSelectAllCheckbox()
{
    selectAllCheckBox->blockSignals(true);

    if (areAllCheckedTests())
        selectAllCheckBox->setChecked(true);
    else
        selectAllCheckBox->setChecked(false);

    selectAllCheckBox->blockSignals(false);

    updateRestoreDefault();
}

void AdvStatisticalOptions::updateRestoreDefault()
{
    if (atLeastOneCheckedTest())
        defaultValuesButton->setEnabled(true);
    else
        defaultValuesButton->setEnabled(false);
}

void AdvStatisticalOptions::updateDespFilter(bool b)
{
    ecProject_->setScreenFilterSr(b);
}

void AdvStatisticalOptions::updateAbsLimFilter(bool b)
{
    ecProject_->setScreenFilterAl(b);
}

void AdvStatisticalOptions::resizeEvent(QResizeEvent *event)
{
    QSize widgetSize = event->size();

    if (widgetSize.width() <= 608 && widgetSize.height() <= 650)
    {
        hideGraphLabels(true);
        showThumbnailGraphLabel(false);
    }
    else if (widgetSize.width() <= 608 && widgetSize.height() > 650)
    {
        hideGraphLabels(true);
        showThumbnailGraphLabel(true);
    }
    else if (widgetSize.width() > 608)
    {
        hideGraphLabels(false);
        showThumbnailGraphLabel(false);
    }

    QWidget::resizeEvent(event);
}

void AdvStatisticalOptions::hideGraphLabels(bool hidden)
{
    spikeGraphLabel->setHidden(hidden);
    amplResGraphLabel->setHidden(hidden);
    dropoutsGraphLabel->setHidden(hidden);
    absLimGraphLabel->setHidden(hidden);
    skewnessGraphLabel->setHidden(hidden);
    discontGraphLabel->setHidden(hidden);
    timelagGraphLabel->setHidden(hidden);
    attackAngleGraphLabel->setHidden(hidden);
    nonSteadyGraphLabel->setHidden(hidden);
}

void AdvStatisticalOptions::showThumbnailGraphLabel(bool visible)
{
    for (int i = 0; i < testToolbox->count(); ++i)
    {
        if (testToolbox->currentIndex() == i)
        {
            switch(i)
            {
            case 0:
                thumbnailGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/spike_small")));
                break;
            case 1:
                thumbnailGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/amplitude_small")));
                break;
            case 2:
                thumbnailGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/dropout_small")));
                break;
            case 3:
                thumbnailGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/range_small")));
                break;
            case 4:
                thumbnailGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/skewkurt_small")));
                break;
            case 5:
                thumbnailGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/discont_small")));
                break;
            case 6:
                thumbnailGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/timelag_small")));
                break;
            case 7:
                thumbnailGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/aoa_small")));
                break;
            case 8:
                thumbnailGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/instat_small")));
                break;
            default:
                break;
            }
            thumbnailGraphLabel->setVisible(visible);
            break;
        }
    }
}

void AdvStatisticalOptions::updateThumbnailGraphLabel(int i)
{
    if (thumbnailGraphLabel->isVisible())
    {
        switch(i)
        {
        case 0:
            thumbnailGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/spike_small")));
            break;
        case 1:
            thumbnailGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/amplitude_small")));
            break;
        case 2:
            thumbnailGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/dropout_small")));
            break;
        case 3:
            thumbnailGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/range_small")));
            break;
        case 4:
            thumbnailGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/skewkurt_small")));
            break;
        case 5:
            thumbnailGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/discont_small")));
            break;
        case 6:
            thumbnailGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/timelag_small")));
            break;
        case 7:
            thumbnailGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/aoa_small")));
            break;
        case 8:
            thumbnailGraphLabel->setPixmap(QPixmap(QStringLiteral(":/icons/instat_small")));
            break;
        default:
            break;
        }
    }
}

void AdvStatisticalOptions::createQuestionMark()
{
    questionMark_1 = new QPushButton;
    questionMark_1->setObjectName(QStringLiteral("questionMarkImg"));
    questionMark_1->setFlat(true);
    questionMark_1->setIcon(QIcon(QStringLiteral(":/icons/qm-enabled")));
    questionMark_1->setIconSize(QSize(12, 12));
    questionMark_2 = new QPushButton;
    questionMark_2->setObjectName(QStringLiteral("questionMarkImg"));
    questionMark_2->setFlat(true);
    questionMark_2->setIcon(QIcon(QStringLiteral(":/icons/qm-enabled")));
    questionMark_2->setIconSize(QSize(12, 12));
    questionMark_3 = new QPushButton;
    questionMark_3->setObjectName(QStringLiteral("questionMarkImg"));
    questionMark_3->setFlat(true);
    questionMark_3->setIcon(QIcon(QStringLiteral(":/icons/qm-enabled")));
    questionMark_3->setIconSize(QSize(12, 12));
    questionMark_4 = new QPushButton;
    questionMark_4->setObjectName(QStringLiteral("questionMarkImg"));
    questionMark_4->setFlat(true);
    questionMark_4->setIcon(QIcon(QStringLiteral(":/icons/qm-enabled")));
    questionMark_4->setIconSize(QSize(12, 12));
    questionMark_5 = new QPushButton;
    questionMark_5->setObjectName(QStringLiteral("questionMarkImg"));
    questionMark_5->setFlat(true);
    questionMark_5->setIcon(QIcon(QStringLiteral(":/icons/qm-enabled")));
    questionMark_5->setIconSize(QSize(12, 12));
    questionMark_6 = new QPushButton;
    questionMark_6->setObjectName(QStringLiteral("questionMarkImg"));
    questionMark_6->setFlat(true);
    questionMark_6->setIcon(QIcon(QStringLiteral(":/icons/qm-enabled")));
    questionMark_6->setIconSize(QSize(12, 12));
    questionMark_7 = new QPushButton;
    questionMark_7->setObjectName(QStringLiteral("questionMarkImg"));
    questionMark_7->setFlat(true);
    questionMark_7->setIcon(QIcon(QStringLiteral(":/icons/qm-enabled")));
    questionMark_7->setIconSize(QSize(12, 12));
    questionMark_8 = new QPushButton;
    questionMark_8->setObjectName(QStringLiteral("questionMarkImg"));
    questionMark_8->setFlat(true);
    questionMark_8->setIcon(QIcon(QStringLiteral(":/icons/qm-enabled")));
    questionMark_8->setIconSize(QSize(12, 12));
    questionMark_9 = new QPushButton;
    questionMark_9->setObjectName(QStringLiteral("questionMarkImg"));
    questionMark_9->setFlat(true);
    questionMark_9->setIcon(QIcon(QStringLiteral(":/icons/qm-enabled")));
    questionMark_9->setIconSize(QSize(12, 12));
    questionMark_10 = new QPushButton;
    questionMark_10->setObjectName(QStringLiteral("questionMarkImg"));
    questionMark_10->setFlat(true);
    questionMark_10->setIcon(QIcon(QStringLiteral(":/icons/qm-enabled")));
    questionMark_10->setIconSize(QSize(12, 12));
    questionMark_11 = new QPushButton;
    questionMark_11->setObjectName(QStringLiteral("questionMarkImg"));
    questionMark_11->setFlat(true);
    questionMark_11->setIcon(QIcon(QStringLiteral(":/icons/qm-enabled")));
    questionMark_11->setIconSize(QSize(12, 12));

    connect(questionMark_1, &QPushButton::clicked,
            this, &AdvStatisticalOptions::onlineHelpTrigger_1);
    connect(questionMark_2, &QPushButton::clicked,
            this, &AdvStatisticalOptions::onlineHelpTrigger_2);
    connect(questionMark_3, &QPushButton::clicked,
            this, &AdvStatisticalOptions::onlineHelpTrigger_3);
    connect(questionMark_4, &QPushButton::clicked,
            this, &AdvStatisticalOptions::onlineHelpTrigger_4);
    connect(questionMark_5, &QPushButton::clicked,
            this, &AdvStatisticalOptions::onlineHelpTrigger_5);
    connect(questionMark_6, &QPushButton::clicked,
            this, &AdvStatisticalOptions::onlineHelpTrigger_6);
    connect(questionMark_7, &QPushButton::clicked,
            this, &AdvStatisticalOptions::onlineHelpTrigger_7);
    connect(questionMark_8, &QPushButton::clicked,
            this, &AdvStatisticalOptions::onlineHelpTrigger_8);
    connect(questionMark_9, &QPushButton::clicked,
            this, &AdvStatisticalOptions::onlineHelpTrigger_9);
    connect(questionMark_10, &QPushButton::clicked,
            this, &AdvStatisticalOptions::onlineHelpTrigger_10);
    connect(questionMark_11, &QPushButton::clicked,
            this, &AdvStatisticalOptions::onlineHelpTrigger_11);
}

void AdvStatisticalOptions::onlineHelpTrigger_1()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Despiking_Raw_Stat_Screening.html")));
}

void AdvStatisticalOptions::onlineHelpTrigger_2()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Despiking_Raw_Stat_Screening.html")));
}

void AdvStatisticalOptions::onlineHelpTrigger_3()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Despiking_Raw_Stat_Screening.html")));
}

void AdvStatisticalOptions::onlineHelpTrigger_4()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Despiking_Raw_Stat_Screening.html")));
}

void AdvStatisticalOptions::onlineHelpTrigger_5()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Despiking_Raw_Stat_Screening.html")));
}

void AdvStatisticalOptions::onlineHelpTrigger_6()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Despiking_Raw_Stat_Screening.html")));
}

void AdvStatisticalOptions::onlineHelpTrigger_7()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Despiking_Raw_Stat_Screening.html")));
}

void AdvStatisticalOptions::onlineHelpTrigger_8()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Despiking_Raw_Stat_Screening.html")));
}

void AdvStatisticalOptions::onlineHelpTrigger_9()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Despiking_Raw_Stat_Screening.html")));
}

void AdvStatisticalOptions::onlineHelpTrigger_10()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Despiking_Raw_Stat_Screening.html")));
}

/// The estimator's own settings follow its switch, wherever the switch came
/// from.
void AdvStatisticalOptions::setRandomErrorControlsEnabled(bool b)
{
    randomMethodLabel->setEnabled(b);
    randomMethodCombo->setEnabled(b);
    itsDefinitionLabel->setEnabled(b);
    itsDefinitionCombo->setEnabled(b);
    timelagMaxLabel->setEnabled(b);
    timelagMaxSpin->setEnabled(b);
    securityCoeffLabel->setEnabled(b);
    securityCoeffSpin->setEnabled(b);
}

/// Re-read ru_meth after something outside this page wrote it.
///
/// Blocked, or the checkbox would write the value straight back and fight
/// whatever just set it - the CEC significance test asks for Finkelstein and
/// Sims, and an unblocked round trip through updateRandomErrorArea would
/// replace that with whatever the combo happens to be showing.
void AdvStatisticalOptions::syncRandomErrorMethod()
{
    const auto method = ecProject_->randErrorMethod();

    const QSignalBlocker checkBoxBlocker(randomErrorCheckBox);
    const QSignalBlocker comboBlocker(randomMethodCombo);
    randomErrorCheckBox->setChecked(method != 0);
    if (method != 0)
    {
        //> By stored value. A project may legitimately hold ru_meth = 3,
        //> Mahrt, which this menu does not offer because the engine computes
        //> it unconditionally; findData returns -1 there and the row is left
        //> alone rather than being pointed at an unrelated method.
        const int row = randomMethodCombo->findData(method);
        if (row >= 0) { randomMethodCombo->setCurrentIndex(row); }
    }

    setRandomErrorControlsEnabled(method != 0);
}

void AdvStatisticalOptions::onlineHelpTrigger_11()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Random_Uncertainty_Estimation.html")));
}

void AdvStatisticalOptions::updateRandomErrorArea(bool b)
{
    if (b)
    {
        ecProject_->setRandomErrorMethod(randomMethodCombo->currentData().toInt());
    }
    else
    {
        ecProject_->setRandomErrorMethod(0);
    }

    setRandomErrorControlsEnabled(b);
}

void AdvStatisticalOptions::onClickRandomMethodLabel()
{
    if (randomMethodCombo->isEnabled())
    {
        randomMethodCombo->showPopup();
    }
}

void AdvStatisticalOptions::updateRandomMethod(int n)
{
    ecProject_->setRandomErrorMethod(randomMethodCombo->itemData(n).toInt());
}

void AdvStatisticalOptions::onClickItsDefinitionLabel()
{
    if (itsDefinitionCombo->isEnabled())
    {
        itsDefinitionCombo->showPopup();
    }
}

void AdvStatisticalOptions::updateItsDefinition(int n)
{
    ecProject_->setRandomErrorItsMethod(n);
}

void AdvStatisticalOptions::onTimelagMaxLabelCLicked()
{
    timelagMaxSpin->setFocus(Qt::ShortcutFocusReason);
    timelagMaxSpin->selectAll();
}

void AdvStatisticalOptions::updateTimelagMax(double d)
{
    ecProject_->setRandomErrorItsTlagMax(d);
}

void AdvStatisticalOptions::onSecurityCoeffLabelCLicked()
{
    securityCoeffSpin->setFocus(Qt::ShortcutFocusReason);
    securityCoeffSpin->selectAll();
}

void AdvStatisticalOptions::updateSecurityCoeff(double d)
{
    ecProject_->setRandomErrorItsSecFactor(d);
}

void AdvStatisticalOptions::onClickDespLabel_1()
{
    despSpin_1->setFocus(Qt::ShortcutFocusReason);
    despSpin_1->selectAll();
}

void AdvStatisticalOptions::onClickDespLabel_2()
{
    despSpin_2->setFocus(Qt::ShortcutFocusReason);
    despSpin_2->selectAll();
}

void AdvStatisticalOptions::onClickDespLabel_3()
{
    despSpin_3->setFocus(Qt::ShortcutFocusReason);
    despSpin_3->selectAll();
}

void AdvStatisticalOptions::onClickDespLabel_8()
{
    despSpin_8->setFocus(Qt::ShortcutFocusReason);
    despSpin_8->selectAll();
}

void AdvStatisticalOptions::onClickAmplResLabel_1()
{
    amplResSpin_1->setFocus(Qt::ShortcutFocusReason);
    amplResSpin_1->selectAll();
}

void AdvStatisticalOptions::onClickAmplResLabel_2()
{
    amplResSpin_2->setFocus(Qt::ShortcutFocusReason);
    amplResSpin_2->selectAll();
}

void AdvStatisticalOptions::onClickAmplResLabel_3()
{
    amplResSpin_3->setFocus(Qt::ShortcutFocusReason);
    amplResSpin_3->selectAll();
}

void AdvStatisticalOptions::onClickDropoutsLabel_1()
{
    dropoutsSpin_1->setFocus(Qt::ShortcutFocusReason);
    dropoutsSpin_1->selectAll();
}

void AdvStatisticalOptions::onClickDropoutsLabel_2()
{
    dropoutsSpin_2->setFocus(Qt::ShortcutFocusReason);
    dropoutsSpin_2->selectAll();
}

void AdvStatisticalOptions::onClickDropoutsLabel_3()
{
    dropoutsSpin_3->setFocus(Qt::ShortcutFocusReason);
    dropoutsSpin_3->selectAll();
}

void AdvStatisticalOptions::onClickAbsLimLabel_1()
{
    absLimSpin_1->setFocus(Qt::ShortcutFocusReason);
    absLimSpin_1->selectAll();
}

void AdvStatisticalOptions::onClickAbsLimLabel_2()
{
    absLimSpin_2->setFocus(Qt::ShortcutFocusReason);
    absLimSpin_2->selectAll();
}

void AdvStatisticalOptions::onClickAbsLimLabel_3()
{
    absLimSpin_3->setFocus(Qt::ShortcutFocusReason);
    absLimSpin_3->selectAll();
}

void AdvStatisticalOptions::onClickSkewnessLabel_1()
{
    skewnessSpin_1->setFocus(Qt::ShortcutFocusReason);
    skewnessSpin_1->selectAll();
}

void AdvStatisticalOptions::onClickSkewnessLabel_2()
{
    skewnessSpin_2->setFocus(Qt::ShortcutFocusReason);
    skewnessSpin_2->selectAll();
}

void AdvStatisticalOptions::onClickSkewnessLabel_5()
{
    skewnessSpin_5->setFocus(Qt::ShortcutFocusReason);
    skewnessSpin_5->selectAll();
}

void AdvStatisticalOptions::onClickSkewnessLabel_6()
{
    skewnessSpin_6->setFocus(Qt::ShortcutFocusReason);
    skewnessSpin_6->selectAll();
}

void AdvStatisticalOptions::onClickDiscontLabel_1()
{
    discontSpin_1->setFocus(Qt::ShortcutFocusReason);
    discontSpin_1->selectAll();
}

void AdvStatisticalOptions::onClickDiscontLabel_2()
{
    discontSpin_2->setFocus(Qt::ShortcutFocusReason);
    discontSpin_2->selectAll();
}

void AdvStatisticalOptions::onClickDiscontLabel_3()
{
    discontSpin_3->setFocus(Qt::ShortcutFocusReason);
    discontSpin_3->selectAll();
}

void AdvStatisticalOptions::onClickDiscontLabel_8()
{
    discontSpin_8->setFocus(Qt::ShortcutFocusReason);
    discontSpin_8->selectAll();
}

void AdvStatisticalOptions::onClickTimeLagLabel_1()
{
    timeLagSpin_1->setFocus(Qt::ShortcutFocusReason);
    timeLagSpin_1->selectAll();
}

void AdvStatisticalOptions::onClickTimeLagLabel_2()
{
    timeLagSpin_2->setFocus(Qt::ShortcutFocusReason);
    timeLagSpin_2->selectAll();
}

void AdvStatisticalOptions::onClickAttackAngleLabel_1()
{
    attackAngleSpin_1->setFocus(Qt::ShortcutFocusReason);
    attackAngleSpin_1->selectAll();
}

void AdvStatisticalOptions::onClickAttackAngleLabel_2()
{
    attackAngleSpin_2->setFocus(Qt::ShortcutFocusReason);
    attackAngleSpin_2->selectAll();
}

void AdvStatisticalOptions::onClickAttackAngleLabel_3()
{
    attackAngleSpin_3->setFocus(Qt::ShortcutFocusReason);
    attackAngleSpin_3->selectAll();
}

void AdvStatisticalOptions::onClickNonSteadyLabel_1()
{
    nonSteadySpin_1->setFocus(Qt::ShortcutFocusReason);
    nonSteadySpin_1->selectAll();
}

void AdvStatisticalOptions::updateTooltip(int i)
{
    QComboBox* senderCombo = qobject_cast<QComboBox *>(sender());

    WidgetUtils::updateComboItemTooltip(senderCombo, i);
}

bool AdvStatisticalOptions::requestTestSettingsReset()
{
    return WidgetUtils::yesNoQuestion(this,
                tr("Reset Statistical Analysis Settings"),
                tr("<p>Do you want to reset the Statistical Analysis settings "
                   "to the default settings?</p>"),
                tr("<p>You cannot undo this action.</p>"));
}

// Enable/disable Vickers despiking settings
void AdvStatisticalOptions::despikingRadioClicked(int b)
{
    //> Three methods now, and each half of the page belongs to one of them.
    //> The sigma settings are Vickers's; the step limits are the consecutive
    //> difference's; Mauder takes neither.
    const bool vickersSelected = (b == 0);
    const bool stepSelected = (b == 2);

    despLabel_1->setEnabled(vickersSelected);
    despLabel_2->setEnabled(vickersSelected);
    despLabel_3->setEnabled(vickersSelected);
    despSpin_1->setEnabled(vickersSelected);
    despSpin_2->setEnabled(vickersSelected);
    despSpin_3->setEnabled(vickersSelected);

    stepLabel_u->setEnabled(stepSelected);
    stepLabel_v->setEnabled(stepSelected);
    stepLabel_w->setEnabled(stepSelected);
    stepLabel_ts->setEnabled(stepSelected);
    stepSpin_u->setEnabled(stepSelected);
    stepSpin_v->setEnabled(stepSelected);
    stepSpin_w->setEnabled(stepSelected);
    stepSpin_ts->setEnabled(stepSelected);

    setSpikeGasRowsEnabled(vickersSelected);
}

void AdvStatisticalOptions::updateDespikingMethod(int b)
{
    ecProject_->setScreenParamDespikeVm(b);
}

