/***************************************************************************
  advoutputoptions.cpp
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

#include "advoutputoptions.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QLineEdit>
#include <QIcon>
#include <QPixmap>
#include <QSize>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QStringList>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QTimer>
#include <QUrl>
#include <QDesktopServices>

#include "clicklabel.h"
#include "configstate.h"
#include "customcheckbox.h"
#include "ecproject.h"
#include "measurement_record.h"
#include "richtextcheckbox.h"
#include "widget_utils.h"

AdvOutputOptions::AdvOutputOptions(QWidget* parent,
                                   EcProject* ecProject,
                                   ConfigState* config) :
    QWidget(parent),
    ecProject_(ecProject),
    configState_(config)
{
    QString tooltipStr;

    minSelectionButton = new QPushButton;
    minSelectionButton->setText(tr("Set Minimal"));
    minSelectionButton->setProperty("mdButton", true);
    tooltipStr =
        tr("<b>Set minimal:</b> Click this button to pre-select a minimal "
           "set of output files, providing you with the essential results "
           "while speeding up program execution. Suggested for processing "
           "long datasets without a need for an in-depth analysis and "
           "thorough validation of computations. Note that you can customize "
           "this pre-selection by adding or removing items.");
    minSelectionButton->setToolTip(tooltipStr);

    minSelectionDesc = new QLabel;
    minSelectionDesc->setText(tr("Maximize computational speed"));
    minSelectionDesc->setStyleSheet(QStringLiteral("QLabel {margin-left: 30px}"));
    minSelectionDesc->setToolTip(minSelectionButton->toolTip());

    auto minSelectionLayout = new QHBoxLayout;
    minSelectionLayout->addWidget(minSelectionButton);
    minSelectionLayout->addWidget(minSelectionDesc);

    typicalSelectionButton = new QPushButton;
    typicalSelectionButton->setText(tr("Set Typical"));
    typicalSelectionButton->setProperty("mdButton", true);
    tooltipStr =
        tr("<b>Set typical:</b> Click this button to pre-select a balanced "
           "set of output files, providing you with the essential results as "
           "well as diagnostic information. The computation time increases "
           "with respect to the minimal output configuration. Note that you "
           "can customize this pre-selection by adding or removing items.");
    typicalSelectionButton->setToolTip(tooltipStr);

    typicalSelectionDesc = new QLabel;
    typicalSelectionDesc->setText(tr("Typical output selection"));
    typicalSelectionDesc->setStyleSheet(QStringLiteral("QLabel {margin-left: 30px;}"));
    typicalSelectionDesc->setToolTip(typicalSelectionButton->toolTip());

    auto typicalSelectionLayout = new QHBoxLayout;
    typicalSelectionLayout->addWidget(typicalSelectionButton);
    typicalSelectionLayout->addWidget(typicalSelectionDesc);

    fullSelectionButton = new QPushButton;
    fullSelectionButton->setText(tr("Set Thorough"));
    fullSelectionButton->setProperty("mdButton", true);
    tooltipStr =
        tr("<b>Set thorough:</b> Click this button to pre-select a thorough "
           "set of output files. While program execution increases (even "
           "dramatically), you are provided with full results and diagnostic "
           "information. Note that you can customize this pre-selection by "
           "adding or removing items.");
    fullSelectionButton->setToolTip(tooltipStr);

    fullSelectionDesc = new QLabel;
    fullSelectionDesc->setText(tr("Complete results and diagnostic information"));
    fullSelectionDesc->setStyleSheet(QStringLiteral("QLabel {margin-left: 30px;}"));
    fullSelectionDesc->setToolTip(fullSelectionButton->toolTip());

    auto fullSelectionLayout = new QHBoxLayout;
    fullSelectionLayout->addWidget(fullSelectionButton);
    fullSelectionLayout->addWidget(fullSelectionDesc);

    minSelectionButton->setMaximumWidth(fullSelectionButton->sizeHint().width());
    typicalSelectionButton->setMaximumWidth(fullSelectionButton->sizeHint().width());
    fullSelectionButton->setMaximumWidth(fullSelectionButton->sizeHint().width());

    hrLabel_1 = new QLabel;
    hrLabel_1->setObjectName(QStringLiteral("hrLabel"));
    auto hrLabel_2 = new QLabel;
    hrLabel_2->setObjectName(QStringLiteral("hrLabel"));
    auto hrLabel_3 = new QLabel;
    hrLabel_3->setObjectName(QStringLiteral("hrLabel"));
    auto hrLabel_4 = new QLabel;
    hrLabel_4->setObjectName(QStringLiteral("hrLabel"));
    auto hrLabel_5 = new QLabel;
    hrLabel_5->setObjectName(QStringLiteral("hrLabel"));

    auto vrLabel_1 = new QLabel;
    vrLabel_1->setObjectName(QStringLiteral("vrLabel"));
    auto vrLabel_2 = new QLabel;
    vrLabel_2->setObjectName(QStringLiteral("vrLabel"));
    auto hrLabelPreprocessing = new QLabel;
    hrLabelPreprocessing->setObjectName(QStringLiteral("hrLabel"));

    spectralAssessmentCreationRadioButton = new QRadioButton;
    spectralAssessmentCreationRadioButton->setText(tr("Pre-processing run (Create Spectral Assessment File if possible)"));
    tooltipStr =
        tr("<b>Pre-processing run:</b> "
           "Prepare a run that creates a new spectral assessment file if "
           "enough data are available. This selects the required spectral "
           "assessment and output options and locks settings that must stay "
           "fixed during the assessment-file run.");
    spectralAssessmentCreationRadioButton->setToolTip(tooltipStr);
    spectralAssessmentCreationWarningIcon = new QLabel;
    setRequiredIcon(spectralAssessmentCreationWarningIcon, true);
    spectralAssessmentCreationWarningIcon->setToolTip(spectralAssessmentCreationRadioButton->toolTip());

    productionRunRadioButton = new QRadioButton;
    productionRunRadioButton->setText(tr("Production run"));
    productionRunRadioButton->setToolTip(
        tr("<b>Production run:</b> Prepare a normal processing run that uses "
           "existing spectral assessment inputs and production QA/QC defaults. "
           "Normal output choices remain available for editing."));
    defaultRunRadioButton = new QRadioButton;
    defaultRunRadioButton->setText(tr("Default, all options available"));
    defaultRunRadioButton->setToolTip(
        tr("<b>Default, all options available:</b> Apply no assessment-output "
           "preset. All options remain controlled manually by the settings "
           "shown in this page and in <i>Spectral Analysis and Corrections</i>."));
    assessmentRunModeRadioGroup = new QButtonGroup(this);
    assessmentRunModeRadioGroup->addButton(spectralAssessmentCreationRadioButton, 0);
    assessmentRunModeRadioGroup->addButton(productionRunRadioButton, 1);
    assessmentRunModeRadioGroup->addButton(defaultRunRadioButton, 2);

    timelagAssessmentOnlyCheckBox = new QCheckBox;
    timelagAssessmentOnlyCheckBox->setText(tr("Create timelag file only"));
    tooltipStr =
        tr("<b>Create timelag file only:</b> Run only the assessment-file "
           "step that creates a time lag assessment file. To activate this "
           "option, enable <i>Time lags compensation</i> and choose either "
           "<i>Automatic time lag optimization</i> or "
           "<i>Pre-whitening block-bootstrap</i>.");
    timelagAssessmentOnlyCheckBox->setToolTip(tooltipStr);

    planarFitAssessmentOnlyCheckBox = new QCheckBox;
    planarFitAssessmentOnlyCheckBox->setText(tr("Create planar fit file only"));
    tooltipStr =
        tr("<b>Create planar fit file only:</b> Run only the assessment-file "
           "step that creates a planar fit assessment file. To activate this "
           "option, enable a planar fit rotation method and choose "
           "<i>Planar fit file not available</i> in the Planar Fit Settings.");
    planarFitAssessmentOnlyCheckBox->setToolTip(tooltipStr);

    outBinSpectraCheckBox = new QCheckBox;
    outBinSpectraCheckBox->setText(tr("All binned spectra and cospectra"));
    tooltipStr =
        tr("<b>All binned spectra and cospectra:</b> Binned spectra and "
           "cospectra are derived from \"full\" ones, by aggregating "
           "frequencies into clusters (bins) of exponentially increasing "
           "spectral width. All frequency components falling in each bin "
           "are averaged together to provide a much shorter file in which "
           "only the main spectral slopes are evidenced. Select this option "
           "to output binned spectra and cospectra for all available "
           "variables for each flux averaging interval. Results files are "
           "stored in a separate sub-folder \"\\EddyFlow_binned_cospectra\" "
           "inside the selected output folder.");
    outBinSpectraCheckBox->setToolTip(tooltipStr);
    outBinSpectraRequiredIcon = new QLabel;
    setRequiredIcon(outBinSpectraRequiredIcon, false);

    outBinOgivesCheckBox = new QCheckBox;
    outBinOgivesCheckBox->setText(tr("All binned ogives"));
    tooltipStr =
            tr("<b>All binned ogives:</b> Binned ogives are calculated by "
               "binning \"full\" ogives. Full ogives are calculated by partial "
               "integration of cospectra: the ogive at a given frequency is "
               "the integration of the corresponding (co)spectrum from the "
               "highest frequency to the given one. Ogives are normalized to "
               "attain the value of 1 at the lower frequency. Select this "
               "option to output binned ogives for all available variables, "
               "for each flux averaging interval. Results files are stored "
               "in a separate sub-folder \"\\EddyFlow_binned_ogives\" inside "
               "the selected output folder.");
    outBinOgivesCheckBox->setToolTip(tooltipStr);

    outMeanSpectraCheckBox = new QCheckBox;
    outMeanSpectraCheckBox->setText(tr("Ensemble averaged spectra"));
    tooltipStr =
        tr("<b>Ensemble averaged spectra:</b> Check this box to "
           "instruct EddyFlow to calculate ensemble-averaged spectra. "
           "EddyFlow will present ensemble gas spectra with and without noise "
           "elimination (if this option is selected in the Spectral Analysis "
           "page) along with ensemble temperature spectra for reference. Use "
           "ensemble spectra to analyze the turbulence structure and the "
           "instruments performance at your site. Note "
           "that a fair amount of spectral data are needed (e.g., more than "
           "2-3 weeks) to obtain a robust assessment.");
    outMeanSpectraCheckBox->setToolTip(tooltipStr);

    outMeanCospCheckBox = new QCheckBox;
    outMeanCospCheckBox->setText(tr("Ensemble averaged cospectra and models"));
    tooltipStr =
        tr("<b>Ensemble averaged cospectra and models:</b> Check this box to "
           "instruct EddyFlow to calculate ensemble-averaged cospectra, fit "
           "model cospectra and present \"ideal\" cospectra along with the "
           "former, to analyzer the turbulence structure at your site. Note "
           "that a fair amount of cospectral data are needed (e.g., more than "
           "2-3 weeks) to obtain a robust assessment.");
    outMeanCospCheckBox->setToolTip(tooltipStr);

    auto fullSpectraDescription = new QLabel;
    fullSpectraDescription->setText(tr("Obtaining full spectra and cospectra "
                            "will significantly increase processing time "
                            "and disk usage.<br />"
                            "For long datasets, we recommend limiting the "
                            "number of these outputs "
                            "to no more than two."));
    fullSpectraDescription->setObjectName(QStringLiteral("alertLabel"));

    outFullSpectraCheckBoxU = new RichTextCheckBox;
    outFullSpectraCheckBoxU->setText(tr("U (longitudinal wind component)"));
    outFullSpectraCheckBoxV = new RichTextCheckBox;
    outFullSpectraCheckBoxV->setText(tr("V (transversal wind component)"));
    outFullSpectraCheckBoxW = new RichTextCheckBox;
    outFullSpectraCheckBoxW->setText(tr("W (vertical wind component)"));
    outFullSpectraCheckBoxTs = new RichTextCheckBox;
    outFullSpectraCheckBoxTs->setText(tr("%1 (sonic or fast ambient temperature)")
                                      .arg(Defs::TSON_STRING));
    // The per-gas spectra boxes are built in rebuildGasRows(), one per record.

    outFullCospectraCheckBoxU = new RichTextCheckBox;
    outFullCospectraCheckBoxU->setText(QStringLiteral("W/U"));
    outFullCospectraCheckBoxV = new RichTextCheckBox;
    outFullCospectraCheckBoxV->setText(QStringLiteral("W/V"));
    outFullCospectraCheckBoxTs = new RichTextCheckBox;
    outFullCospectraCheckBoxTs->setText(QStringLiteral("W/%1").arg(Defs::TSON_STRING));
    outFullCospectraTsRequiredIcon = new QLabel;
    setRequiredIcon(outFullCospectraTsRequiredIcon, false);
    // The per-gas cospectra boxes are built in rebuildGasRows() too.

    fluxnetBiometCheckBox = new QCheckBox;
    fluxnetBiometCheckBox->setText(tr("Use Fluxnet standard for biomet labels and units"));

    fluxnetErrLabelCheckBox = new QCheckBox;
    fluxnetErrLabelCheckBox->setText(tr("Set error label in Fluxnet mode (-9999)"));

    fluxnetGroupBox = new QGroupBox(tr("Fluxnet output settings"));
    auto fluxnetLayout = new QVBoxLayout;
    fluxnetLayout->addWidget(fluxnetBiometCheckBox);
    fluxnetLayout->addWidget(fluxnetErrLabelCheckBox);
    fluxnetGroupBox->setLayout(fluxnetLayout);

    outFullCheckBox = new QCheckBox;
    outFullCheckBox->setText(tr("Full output (fluxes, quality flags, "
                                "turbulence, statistics...)"));
    tooltipStr =
        tr("<b>Full Output:</b> This is the main EddyFlow results file. It "
           "contains fluxes, quality flags, micrometeorological variables, "
           "gas concentrations and densities, footprint estimations and "
           "diagnostic information along with ancillary variables such as "
           "uncorrected fluxes, main statistics, etc.");
    outFullCheckBox->setToolTip(tooltipStr);

    outDetailsCheckBox = new QCheckBox;
    outDetailsCheckBox->setText(tr("Details of steady state and developed "
                                   "turbulence tests (Foken et al. 2004)"));
    tooltipStr =
        tr("<b>Details of steady state and developed turbulence tests:</b> "
           "Partial results obtained from the steady state and the developed "
           "turbulence tests. It reports the percentage of deviation from "
           "expectations, as well as individual test flags.");
    outDetailsCheckBox->setToolTip(tooltipStr);

    outMdCheckBox = new QCheckBox;
    outMdCheckBox->setText(tr("Metadata"));
    tooltipStr =
        tr("<b>Metadata:</b> Summarizes metadata used for the processed "
           "datasets. If an <b><i>Alternative metadata file</i></b> is used, "
           "without any <b><i>Dynamic metadata file</i></b>, the content of "
           "this result file will be identical for all lines and can be "
           "avoided. If you are processing GHG files and/or are using "
           "a <b><i>Dynamic metadata file</i></b>, this result file will "
           "tell you which metadata was actually used during processing.");
    outMdCheckBox->setToolTip(tooltipStr);

    outBiometCheckBox = new QCheckBox;
    outBiometCheckBox->setText(tr("Biomet measurements (averaged over the "
                                  "flux averaging period)"));
    tooltipStr =
        tr("<b>Biomet measurements:</b> Average values of all available "
           "biomet measurements, calculated over the same time period "
           "selected for fluxes. Biomet measurements that are recognized "
           "by EddyFlow (i.e., marked by recognized labels) are screened "
           "for physical plausibility before calculating the average value "
           "and they are converted to units that coincide with other "
           "EddyFlow results. All other variables are solely averaged "
           "and provided on output.");
    outBiometCheckBox->setToolTip(tooltipStr);

    createDatasetCheckBox = new QCheckBox;
    createDatasetCheckBox->setText(tr("Build continuous dataset "
                                      "(This is not gap-filling!\n"
                                      "Missing flux averaging period "
                                      "are filled with error codes)"));

    fullOutformatLabel = new ClickLabel;
    fullOutformatLabel->setText(tr("Output format :"));

    variableVarsOutputRadio = new QRadioButton;
    variableVarsOutputRadio->setText(tr("Output only available results"));

    fixedVarsOutputRadio = new QRadioButton;
    fixedVarsOutputRadio->setText(tr("Use standard output format"));

    outputFormatRadioGroup = new QButtonGroup(this);
    outputFormatRadioGroup->addButton(variableVarsOutputRadio, 0);
    outputFormatRadioGroup->addButton(fixedVarsOutputRadio, 1);

    errorFormatLabel = new ClickLabel;
    errorFormatLabel->setText(tr("Error label :"));

    errorFormatCombo = new QComboBox;
    errorFormatCombo->addItem(QStringLiteral("-9999.0"));
    errorFormatCombo->addItem(QStringLiteral("-6999.0"));
    errorFormatCombo->addItem(QStringLiteral("NaN"));
    errorFormatCombo->addItem(QStringLiteral("Error"));
    errorFormatCombo->addItem(QStringLiteral("N/A"));
    errorFormatCombo->addItem(QStringLiteral("NOOP"));
    errorFormatCombo->setEditable(true);
    QLineEdit* errorLinedit = errorFormatCombo->lineEdit();
    errorLinedit->setMaxLength(32);

    auto statLabel = WidgetUtils::createBlueLabel(this, tr("Statistics"));
    tooltipStr =
        tr("<b>Statistics:</b> Main statistics (mean values, standard "
           "deviations, variances and covariances, skewness and kurtosis) "
           "for all variables contained in the raw files. Result files "
           "concerning variables selected for flux computation are stored "
           "in a separate sub-folder \"\\EddyFlow_stats\" inside the "
           "selected output folder. Result files concerning variables "
           "not selected for flux computation but available in the raw "
           "files are stored in a separate sub-folder \"\\EddyFlow_user_stats\" "
           "inside the selected output folder.");
    statLabel->setToolTip(tooltipStr);

    auto timeSeriesLabel = WidgetUtils::createBlueLabel(this, tr("Time series"));
    tooltipStr =
            tr("<b>Time series:</b> Actual time series for each variable "
               "selected in the list on the right. Result files are stored "
               "in a separate sub-folder \"\\EddyFlow_raw_datasets\" inside "
               "the selected output folder.");
    timeSeriesLabel->setToolTip(tooltipStr);

    auto varLabel = WidgetUtils::createBlueLabel(this, tr("Variables"));

    level1Label = new ClickLabel(tr("Level 1 (unprocessed) :"));
    level2Label = new ClickLabel(tr("Level 2 (after despiking) :"));
    level3Label = new ClickLabel(tr("Level 3 (after cross-wind correction) :"));
    level4Label = new ClickLabel(tr("Level 4 (after angle-of-attack correction) :"));
    level5Label = new ClickLabel(tr("Level 5 (after tilt correction) :"));
    level6Label = new ClickLabel(tr("Level 6 (after time lag compensation) :"));
    level7Label = new ClickLabel(tr("Level 7 (after detrending) :"));

    outSt1CheckBox = new QCheckBox;
    outSt2CheckBox = new QCheckBox;
    outSt3CheckBox = new QCheckBox;
    outSt4CheckBox = new QCheckBox;
    outSt5CheckBox = new QCheckBox;
    outSt6CheckBox = new QCheckBox;
    outSt7CheckBox = new QCheckBox;

    outRaw1CheckBox = new QCheckBox;
    outRaw2CheckBox = new QCheckBox;
    outRaw3CheckBox = new QCheckBox;
    outRaw4CheckBox = new QCheckBox;
    outRaw5CheckBox = new QCheckBox;
    outRaw6CheckBox = new QCheckBox;
    outRaw7CheckBox = new QCheckBox;

    outRawUCheckBox = new RichTextCheckBox;
    outRawUCheckBox->setText(QStringLiteral("U"));
    outRawVCheckBox = new RichTextCheckBox;
    outRawVCheckBox->setText(QStringLiteral("V"));
    outRawWCheckBox = new RichTextCheckBox;
    outRawWCheckBox->setText(QStringLiteral("W"));
    outRawTsCheckBox = new RichTextCheckBox;
    outRawTsCheckBox->setText(QStringLiteral("%1").arg(Defs::TSON_STRING));
    // The per-gas raw-output boxes are built in rebuildGasRows().
    outRawTairCheckBox = new RichTextCheckBox;
    outRawTairCheckBox->setText(tr("T<sub>air</sub>"));
    outRawPairCheckBox = new RichTextCheckBox;
    outRawPairCheckBox->setText(tr("P<sub>air</sub>"));

    outVarsAllCheckBox = new QCheckBox;
    outVarsAllCheckBox->setText(tr("Select all variables"));
    outVarsAllCheckBox->setStyleSheet(QStringLiteral("QCheckBox {margin-left: 40px;}"));

    auto titlePreprocessing = new QLabel;
    titlePreprocessing->setText(tr("Assessment File Outputs"));
    titlePreprocessing->setProperty("groupLabel", true);

    auto title_1 = new QLabel;
    title_1->setText(tr("Results files"));
    title_1->setProperty("groupLabel", true);

    auto title_6 = new QLabel;
    title_6->setText(tr("Spectra and cospectra outputs"));
    title_6->setProperty("groupLabel", true);

    spectralOutputsRequiredIcon = new QLabel;
    setRequiredIcon(spectralOutputsRequiredIcon, false);
    spectralOutputsRequiredLabel = new QLabel;
    spectralOutputsRequiredLabel->setVisible(false);
    spectralOutputsRequiredLabel->setTextFormat(Qt::RichText);

    auto title_2 = WidgetUtils::createBlueLabel(this, tr("Reduced spectra and ogives"));

    auto title_3 = WidgetUtils::createBlueLabel(this, tr("Full length spectra"));
    tooltipStr =
        tr("<b>Full length spectra:</b> Spectra calculated for each variable, "
           "for each flux averaging interval. Results files are stored in "
           "a separate sub-folder \"\\EddyFlow_full_cospectra\" inside the "
           "selected output folder.");
    title_3->setToolTip(tooltipStr);

    auto title_4 = WidgetUtils::createBlueLabel(this, tr("Full length cospectra"));
    tooltipStr =
        tr("<b>Full length cospectra:</b> Cospectra with the vertical wind "
           "component, calculated for each variable, for each flux averaging "
           "interval. Result files are stored in a separate sub-folder "
           "\"\\EddyFlow_full_cospectra\" inside the selected output folder.");
    title_4->setToolTip(tooltipStr);

    auto title_5 = new QLabel;
    title_5->setText(tr("Processed raw data"));
    title_5->setProperty("groupLabel", true);

    createQuestionMark();

    auto qBox_1 = new QHBoxLayout;
    qBox_1->addWidget(title_1);
    qBox_1->addWidget(questionMark_1);
    qBox_1->addStretch();

    auto qBox_2 = new QHBoxLayout;
    qBox_2->addStretch();
    qBox_2->addWidget(fullOutformatLabel);
    qBox_2->addWidget(questionMark_2);

    auto qBox_3 = new QHBoxLayout;
    qBox_3->addStretch();
    qBox_3->addWidget(errorFormatLabel);
    qBox_3->addSpacerItem(new QSpacerItem(18, 12));

    auto qBox_4 = new QHBoxLayout;
    qBox_4->addWidget(createDatasetCheckBox);
    qBox_4->addWidget(questionMark_4);
    qBox_4->addStretch();

    auto qBox_10 = new QHBoxLayout;
    qBox_10->addWidget(title_6);
    qBox_10->addWidget(questionMark_5);
    qBox_10->addStretch();

    auto qBox_11 = new QHBoxLayout;
    qBox_11->addWidget(spectralOutputsRequiredIcon);
    qBox_11->addWidget(spectralOutputsRequiredLabel);
    qBox_11->addStretch();
    qBox_11->setContentsMargins(11, 0, 0, 0);

    auto qBox_5 = new QHBoxLayout;
    qBox_5->addWidget(title_2);
    qBox_5->addSpacerItem(new QSpacerItem(18, 12));
    qBox_5->addStretch();
    qBox_5->setContentsMargins(11, 0, 0, 0);

    auto qBox_6 = new QHBoxLayout;
    qBox_6->addWidget(title_3);
    qBox_5->addSpacerItem(new QSpacerItem(18, 12));
    qBox_6->addStretch();
    qBox_6->setContentsMargins(11, 0, 0, 0);

    auto qBox_7 = new QHBoxLayout;
    qBox_7->addWidget(title_4);
    qBox_5->addSpacerItem(new QSpacerItem(18, 12));
    qBox_7->addStretch();
    qBox_7->setContentsMargins(11, 0, 0, 0);

    auto qBox_12 = new QHBoxLayout;
    qBox_12->addWidget(outBinSpectraCheckBox);
    qBox_12->addWidget(outBinSpectraRequiredIcon);
    qBox_12->addStretch();

    auto qBox_13 = new QHBoxLayout;
    qBox_13->addWidget(outFullCospectraCheckBoxTs);
    qBox_13->addWidget(outFullCospectraTsRequiredIcon);
    qBox_13->addStretch();
    qBox_13->setContentsMargins(0, 0, 0, 0);

    auto qBox_14 = new QHBoxLayout;
    qBox_14->addWidget(spectralAssessmentCreationRadioButton);
    qBox_14->addWidget(spectralAssessmentCreationWarningIcon);
    qBox_14->addStretch();
    qBox_14->setContentsMargins(0, 0, 0, 0);

    auto qBox_17 = new QHBoxLayout;
    qBox_17->addWidget(productionRunRadioButton);
    qBox_17->addStretch();
    qBox_17->setContentsMargins(0, 0, 0, 0);

    auto qBox_18 = new QHBoxLayout;
    qBox_18->addWidget(defaultRunRadioButton);
    qBox_18->addStretch();
    qBox_18->setContentsMargins(0, 0, 0, 0);

    auto qBox_15 = new QHBoxLayout;
    qBox_15->addWidget(timelagAssessmentOnlyCheckBox);
    qBox_15->addStretch();
    qBox_15->setContentsMargins(0, 0, 0, 0);
    auto timelagAssessmentOnlyContainer = new QWidget;
    timelagAssessmentOnlyContainer->setLayout(qBox_15);
    timelagAssessmentOnlyContainer->setToolTip(timelagAssessmentOnlyCheckBox->toolTip());

    auto qBox_16 = new QHBoxLayout;
    qBox_16->addWidget(planarFitAssessmentOnlyCheckBox);
    qBox_16->addStretch();
    qBox_16->setContentsMargins(0, 0, 0, 0);
    auto planarFitAssessmentOnlyContainer = new QWidget;
    planarFitAssessmentOnlyContainer->setLayout(qBox_16);
    planarFitAssessmentOnlyContainer->setToolTip(planarFitAssessmentOnlyCheckBox->toolTip());

    auto spectralAssessmentCreationRow = new QWidget;
    spectralAssessmentCreationRow->setLayout(qBox_14);
    auto productionRunRow = new QWidget;
    productionRunRow->setLayout(qBox_17);
    auto defaultRunRow = new QWidget;
    defaultRunRow->setLayout(qBox_18);

    auto assessmentFileOutputsLayout = new QVBoxLayout;
    assessmentFileOutputsLayout->addWidget(timelagAssessmentOnlyContainer);
    assessmentFileOutputsLayout->addWidget(planarFitAssessmentOnlyContainer);
    assessmentFileOutputsLayout->addWidget(defaultRunRow);
    assessmentFileOutputsLayout->addWidget(spectralAssessmentCreationRow);
    assessmentFileOutputsLayout->addWidget(productionRunRow);
    assessmentFileOutputsLayout->setContentsMargins(0, 0, 0, 0);
    assessmentFileOutputsLayout->setSpacing(4);

    auto qBox_8 = new QHBoxLayout;
    qBox_8->addWidget(statLabel);
    qBox_8->addWidget(questionMark_8);
    qBox_8->addStretch();

    auto qBox_9 = new QHBoxLayout;
    qBox_9->addWidget(timeSeriesLabel);
    qBox_9->addWidget(questionMark_9);
    qBox_9->addStretch();

//
    auto outputLayout = new QGridLayout;
    outputLayout->addLayout(minSelectionLayout, 0, 0, 1, -1);
    outputLayout->addLayout(typicalSelectionLayout, 1, 0, 1, -1);
    outputLayout->addLayout(fullSelectionLayout, 2, 0, 1, -1);
    outputLayout->addWidget(hrLabel_1, 3, 0, 1, -1);
    outputLayout->addWidget(titlePreprocessing, 4, 0);
    outputLayout->addLayout(assessmentFileOutputsLayout, 5, 0, 1, 4);
    outputLayout->addWidget(hrLabelPreprocessing, 6, 0, 1, -1);
    outputLayout->addLayout(qBox_1, 7, 0);
    outputLayout->addWidget(outFullCheckBox, 8, 0, 1, 4);
    outputLayout->addLayout(qBox_2, 8, 2, Qt::AlignRight);
    outputLayout->addWidget(variableVarsOutputRadio, 8, 3, 1, 2, Qt::AlignLeft);
    outputLayout->addWidget(fixedVarsOutputRadio, 9, 3, 1, 2, Qt::AlignLeft);
    outputLayout->addWidget(vrLabel_1, 8, 5, 7, 1);
    outputLayout->addLayout(qBox_4, 8, 6, 6, 2, Qt::AlignLeft);
    outputLayout->addLayout(qBox_3, 10, 2, Qt::AlignRight);
    outputLayout->addWidget(errorFormatCombo, 10, 3);
    outputLayout->addWidget(fluxnetGroupBox, 11, 0, 1, 4);
    outputLayout->addWidget(outBiometCheckBox, 12, 0, 1, 4);
    outputLayout->addWidget(outDetailsCheckBox, 13, 0, 1, 4);
    outputLayout->addWidget(outMdCheckBox, 14, 0, 1, 4);
    outputLayout->addWidget(hrLabel_2, 15, 0, 1, -1);
    outputLayout->addLayout(qBox_10, 16, 0);
    outputLayout->addLayout(qBox_11, 17, 0, 1, -1);
    outputLayout->addLayout(qBox_5, 18, 0);
    outputLayout->addLayout(qBox_12, 19, 0);
    outputLayout->addWidget(outBinOgivesCheckBox, 20, 0);
    outputLayout->addWidget(outMeanSpectraCheckBox, 21, 0);
    outputLayout->addWidget(outMeanCospCheckBox, 22, 0);
    //> Each variable group gets its own grid, so a fifth gas grows the group
    //> in place instead of pushing everything below it - which here means the
    //> spectra description, the whole processing-level block and the raw
    //> variables, three of which are interleaved in adjacent columns of the
    //> same rows.
    spectraGrid_ = new QGridLayout;
    spectraGrid_->setContentsMargins(0, 0, 0, 0);
    spectraGrid_->addLayout(qBox_6, 0, 0);
    spectraGrid_->addWidget(outFullSpectraCheckBoxU, 1, 0);
    spectraGrid_->addWidget(outFullSpectraCheckBoxV, 2, 0);
    spectraGrid_->addWidget(outFullSpectraCheckBoxW, 3, 0);
    spectraGrid_->addWidget(outFullSpectraCheckBoxTs, 4, 0);
    outputLayout->addLayout(spectraGrid_, 18, 1, 9, 2);

    cospectraGrid_ = new QGridLayout;
    cospectraGrid_->setContentsMargins(0, 0, 0, 0);
    cospectraGrid_->addLayout(qBox_7, 0, 0);
    cospectraGrid_->addWidget(outFullCospectraCheckBoxU, 1, 0);
    cospectraGrid_->addWidget(outFullCospectraCheckBoxV, 2, 0);
    cospectraGrid_->addLayout(qBox_13, 3, 0);
    outputLayout->addLayout(cospectraGrid_, 18, 3, 9, 3);

    outputLayout->addWidget(fullSpectraDescription, 27, 1, 1, 7);
    outputLayout->addWidget(hrLabel_3, 28, 0, 1, -1);
    outputLayout->addWidget(title_5, 29, 0);
    outputLayout->addLayout(qBox_8, 30, 1, Qt::AlignRight);
    outputLayout->addLayout(qBox_9, 30, 2, Qt::AlignRight);
    outputLayout->addWidget(varLabel, 30, 3, 1, 2, Qt::AlignCenter);
    outputLayout->addWidget(hrLabel_4, 31, 1, 1, 2);
    outputLayout->addWidget(hrLabel_5, 31, 3, 1, 2);
    outputLayout->addWidget(level1Label, 32, 0, Qt::AlignRight);
    outputLayout->addWidget(level2Label, 33, 0, Qt::AlignRight);
    outputLayout->addWidget(level3Label, 34, 0, Qt::AlignRight);
    outputLayout->addWidget(level4Label, 35, 0, Qt::AlignRight);
    outputLayout->addWidget(level5Label, 36, 0, Qt::AlignRight);
    outputLayout->addWidget(level6Label, 37, 0, Qt::AlignRight);
    outputLayout->addWidget(level7Label, 38, 0, Qt::AlignRight);
    outputLayout->addWidget(outSt1CheckBox, 32, 1, Qt::AlignCenter);
    outputLayout->addWidget(outSt2CheckBox, 33, 1, Qt::AlignCenter);
    outputLayout->addWidget(outSt3CheckBox, 34, 1, Qt::AlignCenter);
    outputLayout->addWidget(outSt4CheckBox, 35, 1, Qt::AlignCenter);
    outputLayout->addWidget(outSt5CheckBox, 36, 1, Qt::AlignCenter);
    outputLayout->addWidget(outSt6CheckBox, 37, 1, Qt::AlignCenter);
    outputLayout->addWidget(outSt7CheckBox, 38, 1, Qt::AlignCenter);
    outputLayout->addWidget(outRaw1CheckBox, 32, 2, Qt::AlignCenter);
    outputLayout->addWidget(outRaw2CheckBox, 33, 2, Qt::AlignCenter);
    outputLayout->addWidget(outRaw3CheckBox, 34, 2, Qt::AlignCenter);
    outputLayout->addWidget(outRaw4CheckBox, 35, 2, Qt::AlignCenter);
    outputLayout->addWidget(outRaw5CheckBox, 36, 2, Qt::AlignCenter);
    outputLayout->addWidget(outRaw6CheckBox, 37, 2, Qt::AlignCenter);
    outputLayout->addWidget(outRaw7CheckBox, 38, 2, Qt::AlignCenter);
    outputLayout->addWidget(vrLabel_2, 32, 3, 7, 1, Qt::AlignLeft);
    //> The raw variables are laid out two columns wide, filled column by
    //> column, so the ten of a four-gas project land exactly where they
    //> always did and a fifth simply lengthens the columns.
    rawVarsGrid_ = new QGridLayout;
    rawVarsGrid_->setContentsMargins(0, 0, 0, 0);
    outputLayout->addLayout(rawVarsGrid_, 32, 3, 6, 3);
    outputLayout->addWidget(outVarsAllCheckBox, 38, 3, 1, 3);
    outputLayout->setRowStretch(39, 1);
    outputLayout->setColumnStretch(8, 1);
    outputLayout->setColumnMinimumWidth(4, 60);

//    auto outputFrame = new QWidget;
//    outputFrame->setProperty("scrollContainerWidget", true);
//    outputFrame->setLayout(outputLayout);

//    auto scrollArea = new QScrollArea;
//    scrollArea->setWidget(outputFrame);
//    scrollArea->setWidgetResizable(true);

//    auto settingsGroupLayout = new QHBoxLayout;
//    settingsGroupLayout->addWidget(scrollArea);

    auto settingsGroupLayout = new QHBoxLayout;
    settingsGroupLayout
            ->addWidget(WidgetUtils::getContainerScrollArea(this,
                                                            outputLayout));

    auto settingsGroupTitle = new QLabel;
    settingsGroupTitle->setText(tr("Output File Options"));
    settingsGroupTitle->setProperty("groupTitle", true);
//    settingsGroupTitle->setStyleSheet(
//        QStringLiteral("QLabel { margin: 5px 0px 10px 0px; padding: 0px; }"));

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(settingsGroupTitle);
    mainLayout->addLayout(settingsGroupLayout);
    mainLayout->setContentsMargins(15, 0, 15, 10);
    setLayout(mainLayout);

    connect(outBinSpectraCheckBox, &QCheckBox::toggled,
            this, &AdvOutputOptions::updateOutBinSpectra);
    connect(outBinOgivesCheckBox, &QCheckBox::toggled, [=](bool checked)
            { ecProject_->setScreenOutBinOgives(checked); });
    connect(outMeanSpectraCheckBox, &QCheckBox::toggled, [=](bool checked)
            { ecProject_->setGeneralOutMeanSpectra(checked); });
    connect(outMeanCospCheckBox, &QCheckBox::toggled, [=](bool checked)
            { ecProject_->setGeneralOutMeanCosp(checked); });
    connect(outMeanCospCheckBox, &QCheckBox::toggled,
            this, &AdvOutputOptions::updateBinSpectra);
    connect(fluxnetBiometCheckBox, &QCheckBox::toggled, [=](bool checked)
            { ecProject_->setFluxnetStandardizeBiomet(checked); });
    connect(outDetailsCheckBox, &QCheckBox::toggled, [=](bool checked)
            { ecProject_->setScreenlOutDetails(checked); });
    connect(outMdCheckBox, &QCheckBox::toggled, [=](bool checked)
            { ecProject_->setGeneralOutMd(checked); });
    connect(outBiometCheckBox, &QCheckBox::toggled, [=](bool checked)
            { ecProject_->setGeneralOutBiomet(checked); });
    connect(createDatasetCheckBox, &QCheckBox::toggled, [=](bool checked)
            { ecProject_->setGeneralMakeDataset(checked); });
    connect(fluxnetErrLabelCheckBox, &QCheckBox::toggled, [=](bool checked)
            { ecProject_->setFluxnetErrLabel(checked); });
    connect(fluxnetErrLabelCheckBox, &QCheckBox::toggled,
            this, &AdvOutputOptions::updateFluxnetErrLabelMode);
    connect(outFullCheckBox, &QCheckBox::toggled, [=](bool checked)
            { ecProject_->setGeneralOutRich(checked); });
    connect(spectralAssessmentCreationRadioButton, &QRadioButton::toggled,
            this, &AdvOutputOptions::updateSpectralAssessmentCreationMode);
    connect(productionRunRadioButton, &QRadioButton::toggled,
            this, &AdvOutputOptions::updateProductionRunMode);
    connect(defaultRunRadioButton, &QRadioButton::toggled,
            this, &AdvOutputOptions::updateDefaultRunMode);
    connect(timelagAssessmentOnlyCheckBox, &QCheckBox::toggled,
            this, &AdvOutputOptions::updateTimelagAssessmentOnly);
    connect(planarFitAssessmentOnlyCheckBox, &QCheckBox::toggled,
            this, &AdvOutputOptions::updatePlanarFitAssessmentOnly);

    connect(outputFormatRadioGroup, &QButtonGroup::idClicked,
            this, &AdvOutputOptions::updateFixedOuputFormat);

    connect(errorFormatLabel, &ClickLabel::clicked,
            this, &AdvOutputOptions::onClickerrorFormatLabel);

    connect(errorFormatCombo, &QComboBox::currentTextChanged,
            this, &AdvOutputOptions::updateErrorLabel);
    connect(errorFormatCombo, &QComboBox::editTextChanged,
            this, &AdvOutputOptions::updateErrorLabel);

    connect(outFullSpectraCheckBoxU, &RichTextCheckBox::toggled, [=](bool checked)
            { ecProject_->setScreenOutFullSpectraU(checked); });
    connect(outFullSpectraCheckBoxV, &RichTextCheckBox::toggled, [=](bool checked)
            { ecProject_->setScreenOutFullSpectraV(checked); });
    connect(outFullSpectraCheckBoxW, &RichTextCheckBox::toggled, [=](bool checked)
            { ecProject_->setScreenOutFullSpectraW(checked); });
    connect(outFullSpectraCheckBoxTs, &RichTextCheckBox::toggled, [=](bool checked)
            { ecProject_->setScreenOutFullSpectraTs(checked); });

    connect(outFullCospectraCheckBoxU, &RichTextCheckBox::toggled, [=](bool checked)
            { ecProject_->setScreenOutFullCospectraU(checked); });
    connect(outFullCospectraCheckBoxV, &RichTextCheckBox::toggled, [=](bool checked)
            { ecProject_->setScreenOutFullCospectraV(checked); });
    connect(outFullCospectraCheckBoxTs, &RichTextCheckBox::toggled, [=](bool checked)
            { ecProject_->setScreenOutFullCospectraTs(checked); });

    connect(outSt1CheckBox, &QCheckBox::toggled, [=](bool checked)
            { ecProject_->setScreenOutSt1(checked); });
    connect(outSt2CheckBox, &QCheckBox::toggled, [=](bool checked)
            { ecProject_->setScreenOutSt2(checked); });
    connect(outSt3CheckBox, &QCheckBox::toggled, [=](bool checked)
            { ecProject_->setScreenOutSt3(checked); });
    connect(outSt4CheckBox, &QCheckBox::toggled, [=](bool checked)
            { ecProject_->setScreenOutSt4(checked); });
    connect(outSt5CheckBox, &QCheckBox::toggled, [=](bool checked)
            { ecProject_->setScreenOutSt5(checked); });
    connect(outSt6CheckBox, &QCheckBox::toggled, [=](bool checked)
            { ecProject_->setScreenOutSt6(checked); });
    connect(outSt7CheckBox, &QCheckBox::toggled, [=](bool checked)
            { ecProject_->setScreenOutSt7(checked); });

    connect(outRaw1CheckBox, &QCheckBox::toggled,
            this, &AdvOutputOptions::updateOutputRaw1);
    connect(outRaw2CheckBox, &QCheckBox::toggled,
            this, &AdvOutputOptions::updateOutputRaw2);
    connect(outRaw3CheckBox, &QCheckBox::toggled,
            this, &AdvOutputOptions::updateOutputRaw3);
    connect(outRaw4CheckBox, &QCheckBox::toggled,
            this, &AdvOutputOptions::updateOutputRaw4);
    connect(outRaw5CheckBox, &QCheckBox::toggled,
            this, &AdvOutputOptions::updateOutputRaw5);
    connect(outRaw6CheckBox, &QCheckBox::toggled,
            this, &AdvOutputOptions::updateOutputRaw6);
    connect(outRaw7CheckBox, &QCheckBox::toggled,
            this, &AdvOutputOptions::updateOutputRaw7);

    connect(outRawUCheckBox, &RichTextCheckBox::toggled,
            this, &AdvOutputOptions::updateOutputRawU);
    connect(outRawVCheckBox, &RichTextCheckBox::toggled,
            this, &AdvOutputOptions::updateOutputRawV);
    connect(outRawWCheckBox, &RichTextCheckBox::toggled,
            this, &AdvOutputOptions::updateOutputRawW);
    connect(outRawTsCheckBox, &RichTextCheckBox::toggled,
            this, &AdvOutputOptions::updateOutputRawTs);
    connect(outRawTairCheckBox, &RichTextCheckBox::toggled,
            this, &AdvOutputOptions::updateOutputRawTair);
    connect(outRawPairCheckBox, &RichTextCheckBox::toggled,
            this, &AdvOutputOptions::updateOutputRawPair);
    connect(outVarsAllCheckBox, &QCheckBox::toggled,
            this, &AdvOutputOptions::checkVarsAll);

    connect(minSelectionButton, &QPushButton::clicked,
            this, &AdvOutputOptions::selectMin);
    connect(typicalSelectionButton, &QPushButton::clicked,
            this, &AdvOutputOptions::selectTypical);
    connect(fullSelectionButton, &QPushButton::clicked,
            this, &AdvOutputOptions::selectFull);

    connect(ecProject_, &EcProject::ecProjectNew,
            this, &AdvOutputOptions::reset);
    connect(ecProject_, &EcProject::ecProjectChanged,
            this, &AdvOutputOptions::refresh);
    connect(ecProject_, &EcProject::updateInfo,
            this, &AdvOutputOptions::refresh);

    // init
    //> Populates the three generated groups for the first time. The raw
    //> variables in particular are only ever placed here, so without this
    //> even the fixed wind and temperature boxes would have no home.
    rebuildGasRows();
    QTimer::singleShot(0, this, &AdvOutputOptions::reset);
}

void AdvOutputOptions::setSmartfluxUI()
{
    bool on = configState_->project.smartfluxMode;

    QWidgetList enableableWidgets;
    enableableWidgets << spectralAssessmentCreationRadioButton
                      << productionRunRadioButton
                      << defaultRunRadioButton
                      << timelagAssessmentOnlyCheckBox
                      << planarFitAssessmentOnlyCheckBox
                      << outFullCheckBox
                      << fullOutformatLabel
                      << fixedVarsOutputRadio
                      << variableVarsOutputRadio
                      << errorFormatLabel
                      << errorFormatCombo
                      << createDatasetCheckBox
                      << outMeanSpectraCheckBox
                      << outMeanCospCheckBox
                      << fluxnetGroupBox
                      << fluxnetErrLabelCheckBox
                      << fluxnetBiometCheckBox
                      << outBiometCheckBox
                      << outMdCheckBox
                      << outFullSpectraCheckBoxU
                      << outFullSpectraCheckBoxV
                      << outFullSpectraCheckBoxW
                      << outFullSpectraCheckBoxTs
                      << outFullCospectraCheckBoxU
                      << outFullCospectraCheckBoxV
                      << outFullCospectraCheckBoxTs
                      << outSt1CheckBox
                      << outSt2CheckBox
                      << outSt3CheckBox
                      << outSt4CheckBox
                      << outSt5CheckBox
                      << outSt6CheckBox
                      << outSt7CheckBox
                      << outRaw1CheckBox
                      << outRaw2CheckBox
                      << outRaw3CheckBox
                      << outRaw4CheckBox
                      << outRaw5CheckBox
                      << outRaw6CheckBox
                      << outRaw7CheckBox
                      << outRawUCheckBox
                      << outRawVCheckBox
                      << outRawWCheckBox
                      << outRawTsCheckBox
                      << outRawTairCheckBox
                      << outRawPairCheckBox
                      << outVarsAllCheckBox;
    //> The generated per-gas boxes belong here too, or they stay live while
    //> the rest of the page is disabled.
    enableableWidgets << gasOutputCheckBoxes();

    for (auto w : enableableWidgets)
    {
        if (on)
        {
            oldEnabled.push_back(w->isEnabled());
            w->setDisabled(on);
        }
        else
        {
            w->setEnabled(oldEnabled.at(static_cast<unsigned long>(enableableWidgets.indexOf(w))));
        }
    }

    QWidgetList visibleWidgets;
    visibleWidgets << fullSelectionButton
            << fullSelectionDesc
            << minSelectionButton
            << minSelectionDesc
            << typicalSelectionButton
            << typicalSelectionDesc
            << hrLabel_1;

    for (auto w : visibleWidgets)
    {
        if (on)
        {
            oldVisible.push_back(w->isEnabled());
            w->setHidden(on);
        }
        else
        {
            w->setVisible(oldVisible.at(static_cast<unsigned long>(visibleWidgets.indexOf(w))));
        }
    }

    // block project modified() signal
    bool oldmod = false;
    if (!on)
    {
        // save the modified flag to prevent side effects of setting widgets
        oldmod = ecProject_->modified();
        ecProject_->blockSignals(true);
    }

    QList<QAbstractButton *> checkableWidgets;
    checkableWidgets << outFullCheckBox
                     << outBiometCheckBox
                     << fixedVarsOutputRadio;
    for (auto w : checkableWidgets)
    {
        if (on)
        {
            w->setChecked(on);
        }
    }

    if (on)
    {
        updateFixedOuputFormat(1);
    }

    QList<QCheckBox *> uncheckableCheckbox;
    uncheckableCheckbox << fluxnetErrLabelCheckBox
                       << fluxnetBiometCheckBox
                       << createDatasetCheckBox
                       << outMeanSpectraCheckBox
                       << outMeanCospCheckBox
                       << outDetailsCheckBox
                       << outMdCheckBox
                       << outBinOgivesCheckBox
                       << outSt1CheckBox
                       << outSt2CheckBox
                       << outSt3CheckBox
                       << outSt4CheckBox
                       << outSt5CheckBox
                       << outSt6CheckBox
                       << outSt7CheckBox
                       << outRaw1CheckBox
                       << outRaw2CheckBox
                       << outRaw3CheckBox
                       << outRaw4CheckBox
                       << outRaw5CheckBox
                       << outRaw6CheckBox
                       << outRaw7CheckBox
                       << outVarsAllCheckBox;
    for (auto w : uncheckableCheckbox)
    {
        if (on)
        {
            w->setChecked(!on);
        }
    }

    QList<RichTextCheckBox *> uncheckableRichTextCheckbox;
    uncheckableRichTextCheckbox << outFullSpectraCheckBoxU
                       << outFullSpectraCheckBoxV
                       << outFullSpectraCheckBoxW
                       << outFullSpectraCheckBoxTs
                       << outFullCospectraCheckBoxU
                       << outFullCospectraCheckBoxV
                       << outFullCospectraCheckBoxTs
                       << outRawUCheckBox
                       << outRawVCheckBox
                       << outRawWCheckBox
                       << outRawTsCheckBox
                       << outRawTairCheckBox
                       << outRawPairCheckBox;
    for (const auto& row : gasRows_)
    {
        if (row.spectra) { uncheckableRichTextCheckbox << row.spectra; }
        if (row.cospectra) { uncheckableRichTextCheckbox << row.cospectra; }
        if (row.raw) { uncheckableRichTextCheckbox << row.raw; }
    }
    for (auto w : uncheckableRichTextCheckbox)
    {
        if (on)
        {
            w->setChecked(!on);
        }
    }

    if (on)
    {
        errorFormatCombo->setCurrentIndex(2);
    }

    // restore project modified() signal
    if (!on)
    {
        // restore modified flag
        ecProject_->setModified(oldmod);
        ecProject_->blockSignals(false);
    }

    updateGasOutputAvailability();
    setRequiredSpectralOutputState(currentSpectralMethodIndex());
}

void AdvOutputOptions::reset()
{
    // save the modified flag to prevent side effects of setting widgets
    bool oldmod = ecProject_->modified();
    ecProject_->blockSignals(true);

    selectMin();
    setVarsAvailable(false);
    outVarsAllCheckBox->setEnabled(false);
    outVarsAllCheckBox->setChecked(false);
    variableVarsOutputRadio->setChecked(true);
    errorFormatCombo->setCurrentIndex(0);
    {
        QSignalBlocker preprocessingBlocker(spectralAssessmentCreationRadioButton);
        QSignalBlocker productionBlocker(productionRunRadioButton);
        QSignalBlocker defaultBlocker(defaultRunRadioButton);
        defaultRunRadioButton->setChecked(true);
    }
    ecProject_->setSpectraFluxRunMode(0);
    timelagAssessmentOnlyCheckBox->setChecked(false);
    planarFitAssessmentOnlyCheckBox->setChecked(false);

    if (ecProject_->generalUseBiomet())
    {
        outBiometCheckBox->setChecked(true);
    }

    // restore modified flag
    ecProject_->setModified(oldmod);
    ecProject_->blockSignals(false);
    updateGasOutputAvailability();
    updateSpectralAssessmentCreationAvailability();
    setRequiredSpectralOutputState(currentSpectralMethodIndex());
}

void AdvOutputOptions::refresh()
{
    // save the modified flag to prevent side effects of setting widgets
    bool oldmod = ecProject_->modified();
    ecProject_->blockSignals(true);

    outBinSpectraCheckBox->setChecked(ecProject_->screenOutBinSpectra());
    outBinOgivesCheckBox->setChecked(ecProject_->screenOutBinOgives());
    outMeanSpectraCheckBox->setChecked(ecProject_->generalOutMeanSpectra());
    outMeanCospCheckBox->setChecked(ecProject_->generalOutMeanCosp());

    outFullSpectraCheckBoxU->setChecked(ecProject_->screenOutFullSpectraU());
    outFullSpectraCheckBoxV->setChecked(ecProject_->screenOutFullSpectraV());
    outFullSpectraCheckBoxW->setChecked(ecProject_->screenOutFullSpectraW());
    outFullSpectraCheckBoxTs->setChecked(ecProject_->screenOutFullSpectraTs());
    //> Rebuilt here rather than only on show: a project load can change the
    //> gas records under the rows, and their checked state comes from them.
    rebuildGasRows();

    outFullCospectraCheckBoxU->setChecked(ecProject_->screenOutFullCospectraU());
    outFullCospectraCheckBoxV->setChecked(ecProject_->screenOutFullCospectraV());
    outFullCospectraCheckBoxTs->setChecked(ecProject_->screenOutFullCospectraTs());

    fluxnetBiometCheckBox->setChecked(ecProject_->fluxnetStandardizeBiomet());
    fluxnetErrLabelCheckBox->setChecked(ecProject_->fluxnetErrLabel());
    updateFluxnetErrLabelMode(ecProject_->fluxnetErrLabel());
    outFullCheckBox->setChecked(ecProject_->generalOutRich());
    {
        QSignalBlocker preprocessingBlocker(spectralAssessmentCreationRadioButton);
        QSignalBlocker productionBlocker(productionRunRadioButton);
        QSignalBlocker defaultBlocker(defaultRunRadioButton);
        if (isSpectralAssessmentCreationMode())
        {
            spectralAssessmentCreationRadioButton->setChecked(true);
        }
        else if (isProductionRunMode())
        {
            productionRunRadioButton->setChecked(true);
        }
        else
        {
            defaultRunRadioButton->setChecked(true);
        }
    }
    timelagAssessmentOnlyCheckBox->setChecked(ecProject_->timelagAssessmentOnly());
    planarFitAssessmentOnlyCheckBox->setChecked(ecProject_->planarFitAssessmentOnly());
    if (ecProject_->timelagAssessmentOnly() || ecProject_->planarFitAssessmentOnly())
    {
        clearRunModeRadios();
    }
    outDetailsCheckBox->setChecked(ecProject_->screenOutDetails());
    outMdCheckBox->setChecked(ecProject_->generalOutMd());
    outBiometCheckBox->setChecked(ecProject_->generalOutBiomet());
    createDatasetCheckBox->setChecked(ecProject_->generalMakeDataset());

    outputFormatRadioGroup->buttons().at(ecProject_->generalFixedOutFormat())->setChecked(true);

    QString s(ecProject_->generalErroLabel());
    if (!s.isEmpty())
    {
        int n = errorFormatCombo->findText(s);
        if (n < 0)
        {
            errorFormatCombo->addItem(s);
            errorFormatCombo->setCurrentIndex(errorFormatCombo->findText(s));
        }
        else
            errorFormatCombo->setCurrentIndex(n);
    }
    else
    {
        errorFormatCombo->setCurrentIndex(0);
    }

    outSt1CheckBox->setChecked(ecProject_->screenOutSt1());
    outSt2CheckBox->setChecked(ecProject_->screenOutSt2());
    outSt3CheckBox->setChecked(ecProject_->screenOutSt3());
    outSt4CheckBox->setChecked(ecProject_->screenOutSt4());
    outSt5CheckBox->setChecked(ecProject_->screenOutSt5());
    outSt6CheckBox->setChecked(ecProject_->screenOutSt6());
    outSt7CheckBox->setChecked(ecProject_->screenOutSt7());
    outRaw1CheckBox->setChecked(ecProject_->screenOutRaw1());
    outRaw2CheckBox->setChecked(ecProject_->screenOutRaw2());
    outRaw3CheckBox->setChecked(ecProject_->screenOutRaw3());
    outRaw4CheckBox->setChecked(ecProject_->screenOutRaw4());
    outRaw5CheckBox->setChecked(ecProject_->screenOutRaw5());
    outRaw6CheckBox->setChecked(ecProject_->screenOutRaw6());
    outRaw7CheckBox->setChecked(ecProject_->screenOutRaw7());
    outRawUCheckBox->setChecked(ecProject_->screenOutRawU());
    outRawVCheckBox->setChecked(ecProject_->screenOutRawV());
    outRawWCheckBox->setChecked(ecProject_->screenOutRawW());
    outRawTsCheckBox->setChecked(ecProject_->screenOutRawTs());
    outRawTairCheckBox->setChecked(ecProject_->screenOutRawTair());
    outRawPairCheckBox->setChecked(ecProject_->screenOutRawPair());
    setVarsAvailable(areTimeSeriesChecked());

    outVarsAllCheckBox->setChecked(areAllCheckedVars());
    outVarsAllCheckBox->setEnabled(areTimeSeriesChecked());

    // restore modified flag
    ecProject_->setModified(oldmod);
    ecProject_->blockSignals(false);
    updateGasOutputAvailability();
    updateSpectralAssessmentCreationAvailability();
    setRequiredSpectralOutputState(currentSpectralMethodIndex());
}

void AdvOutputOptions::updateOutBinSpectra(bool b)
{
    ecProject_->setScreenOutBinSpectra(b);
}

void AdvOutputOptions::updateSpectralAssessmentCreationMode(bool checked)
{
    if (!checked)
    {
        updateSpectralAssessmentCreationAvailability();
        setRequiredSpectralOutputState(currentSpectralMethodIndex());
        return;
    }

    if (checked)
    {
        if (!validateSpectralAssessmentCreationRequest())
        {
            ecProject_->setSpectraFluxRunMode(0);
            defaultRunRadioButton->setChecked(true);
            return;
        }

        ecProject_->setSpectraFluxRunMode(1);
        timelagAssessmentOnlyCheckBox->setChecked(false);
        planarFitAssessmentOnlyCheckBox->setChecked(false);
        applySpectralAssessmentCreationRequirements();
    }

    updateSpectralAssessmentCreationAvailability();
    setRequiredSpectralOutputState(currentSpectralMethodIndex());
}

void AdvOutputOptions::updateProductionRunMode(bool checked)
{
    if (!checked)
    {
        return;
    }

    ecProject_->setSpectraFluxRunMode(2);
    timelagAssessmentOnlyCheckBox->setChecked(false);
    planarFitAssessmentOnlyCheckBox->setChecked(false);
    applyProductionRunRequirements();
    updateSpectralAssessmentCreationAvailability();
    setRequiredSpectralOutputState(currentSpectralMethodIndex());
}

void AdvOutputOptions::updateDefaultRunMode(bool checked)
{
    if (!checked)
    {
        return;
    }

    ecProject_->setSpectraFluxRunMode(0);
    timelagAssessmentOnlyCheckBox->setChecked(false);
    planarFitAssessmentOnlyCheckBox->setChecked(false);
    updateSpectralAssessmentCreationAvailability();
    setRequiredSpectralOutputState(currentSpectralMethodIndex());
}

void AdvOutputOptions::updateTimelagAssessmentOnly(bool checked)
{
    if (checked)
    {
        ecProject_->setSpectraFluxRunMode(0);
        clearRunModeRadios();
    }
    ecProject_->setTimelagAssessmentOnly(checked);
}

void AdvOutputOptions::updatePlanarFitAssessmentOnly(bool checked)
{
    if (checked)
    {
        ecProject_->setSpectraFluxRunMode(0);
        clearRunModeRadios();
    }
    ecProject_->setPlanarFitAssessmentOnly(checked);
}

bool AdvOutputOptions::canCreateTimelagAssessmentOnly() const
{
    return ecProject_->screenTlagMeth() == 4 || ecProject_->screenTlagMeth() == 5;
}

bool AdvOutputOptions::canCreatePlanarFitAssessmentOnly() const
{
    return (ecProject_->screenRotMethod() == 3 || ecProject_->screenRotMethod() == 4)
            && ecProject_->planarFitMode() == 1;
}

bool AdvOutputOptions::canCreateSpectralAssessment() const
{
    return !configState_->project.smartfluxMode
            && ecProject_->spectraMode() != 0;
}

bool AdvOutputOptions::validateSpectralAssessmentCreationRequest()
{
    if (!QFileInfo(ecProject_->screenDataPath()).isDir()
        || ecProject_->generalFilesFound() <= 0)
    {
        WidgetUtils::warning(this,
                             tr("Create Spectral Assessment File"),
                             tr("Raw data are not available. Please select a valid raw data directory first."));
        return false;
    }

    auto intervals = estimatedSpectralAssessmentIntervals();
    if (intervals < 0)
    {
        WidgetUtils::warning(this,
                             tr("Create Spectral Assessment File"),
                             tr("The selected assessment period is invalid. Please check the selected timestamps."));
        return false;
    }

    if (intervals < ecProject_->spectraMinSmpl())
    {
        WidgetUtils::warning(this,
                             tr("Create Spectral Assessment File"),
                             tr("Not enough samples; please select more timestamps to process."));
        return false;
    }

    if (ecProject_->generalBinSpectraAvail()
        && !QFileInfo(ecProject_->spectraBinSpectra()).isDir())
    {
        WidgetUtils::warning(this,
                             tr("Create Spectral Assessment File"),
                             tr("The selected binned (co)spectra directory is not valid."));
        return false;
    }

    if (ecProject_->generalFullSpectraAvail()
        && !QFileInfo(ecProject_->spectraFullSpectra()).isDir())
    {
        WidgetUtils::warning(this,
                             tr("Create Spectral Assessment File"),
                             tr("The selected full cospectra directory is not valid."));
        return false;
    }

    return true;
}

int AdvOutputOptions::estimatedSpectralAssessmentIntervals() const
{
    const auto startDate = ecProject_->spectraSubset()
            ? ecProject_->spectraStartDate()
            : ecProject_->generalStartDate();
    const auto startTime = ecProject_->spectraSubset()
            ? ecProject_->spectraStartTime()
            : ecProject_->generalStartTime();
    const auto endDate = ecProject_->spectraSubset()
            ? ecProject_->spectraEndDate()
            : ecProject_->generalEndDate();
    const auto endTime = ecProject_->spectraSubset()
            ? ecProject_->spectraEndTime()
            : ecProject_->generalEndTime();

    const auto start = QDateTime(QDate::fromString(startDate, Qt::ISODate),
                                 QTime::fromString(startTime, QStringLiteral("hh:mm")));
    const auto end = QDateTime(QDate::fromString(endDate, Qt::ISODate),
                               QTime::fromString(endTime, QStringLiteral("hh:mm")));
    const auto avgLenSeconds = ecProject_->screenAvrgLen() * 60;

    if (!start.isValid() || !end.isValid() || avgLenSeconds <= 0 || end <= start)
    {
        return -1;
    }

    return static_cast<int>(start.secsTo(end) / avgLenSeconds);
}

int AdvOutputOptions::currentSpectralMethodIndex() const
{
    return ecProject_->generalHfMethod() == 4 ? 4
         : ecProject_->generalHfMethod() == 3 ? 3
         : ecProject_->generalHfMethod() == 2 ? 2
         : 0;
}

bool AdvOutputOptions::isSpectralAssessmentCreationMode() const
{
    return ecProject_->spectraFluxRunMode() == 1;
}

bool AdvOutputOptions::isProductionRunMode() const
{
    return ecProject_->spectraFluxRunMode() == 2;
}

void AdvOutputOptions::applySpectralAssessmentCreationRequirements()
{
    if (ecProject_->generalHfMethod() < 2 || ecProject_->generalHfMethod() > 4)
    {
        ecProject_->setGeneralHfMethod(4);
    }
    if (ecProject_->spectraMode() != 1)
    {
        ecProject_->setSpectraMode(1);
    }
    if (!ecProject_->screenOutBinSpectra())
    {
        ecProject_->setScreenOutBinSpectra(1);
    }
    if (ecProject_->generalBinSpectraAvail())
    {
        ecProject_->setGeneralBinSpectraAvail(0);
    }
    if (ecProject_->generalFullSpectraAvail())
    {
        ecProject_->setGeneralFullSpectraAvail(0);
    }
    if (!ecProject_->screenOutFullCospectraTs())
    {
        ecProject_->setScreenOutFullCospectraTs(1);
    }
    if (ecProject_->spectraUseVmFlags())
    {
        ecProject_->setSpectraUseVmFlags(0);
    }
    if (ecProject_->spectraUseFokenMid())
    {
        ecProject_->setSpectraUseFokenMid(0);
    }
    if (ecProject_->timelagAssessmentOnly())
    {
        ecProject_->setTimelagAssessmentOnly(0);
    }
    if (ecProject_->planarFitAssessmentOnly())
    {
        ecProject_->setPlanarFitAssessmentOnly(0);
    }
}

void AdvOutputOptions::applyProductionRunRequirements()
{
    if (ecProject_->generalHfMethod() < 2 || ecProject_->generalHfMethod() > 4)
    {
        ecProject_->setGeneralHfMethod(4);
    }
    ecProject_->setGeneralBinSpectraAvail(1);
    ecProject_->setGeneralFullSpectraAvail(1);
    ecProject_->setSpectraMode(0);
    ecProject_->setSpectraUseVmFlags(1);
    ecProject_->setSpectraUseFokenMid(1);
    ecProject_->setSpectraUseFokenLow(1);

    outMeanSpectraCheckBox->setChecked(false);
    outMeanCospCheckBox->setChecked(false);
    checkFullSpectraAll(false);
    checkFullCospectraAll(false);
}

void AdvOutputOptions::clearRunModeRadios()
{
    QSignalBlocker preprocessingBlocker(spectralAssessmentCreationRadioButton);
    QSignalBlocker productionBlocker(productionRunRadioButton);
    QSignalBlocker defaultBlocker(defaultRunRadioButton);
    const auto wasExclusive = assessmentRunModeRadioGroup->exclusive();
    assessmentRunModeRadioGroup->setExclusive(false);
    spectralAssessmentCreationRadioButton->setChecked(false);
    productionRunRadioButton->setChecked(false);
    defaultRunRadioButton->setChecked(false);
    assessmentRunModeRadioGroup->setExclusive(wasExclusive);
}

void AdvOutputOptions::updateSpectralAssessmentCreationAvailability()
{
    auto enabled = canCreateSpectralAssessment();
    spectralAssessmentCreationRadioButton->setEnabled(enabled);
    if (!enabled && spectralAssessmentCreationRadioButton->isChecked())
    {
        ecProject_->setSpectraFluxRunMode(0);
        defaultRunRadioButton->setChecked(true);
    }
    productionRunRadioButton->setEnabled(!configState_->project.smartfluxMode);
    if (!productionRunRadioButton->isEnabled() && productionRunRadioButton->isChecked())
    {
        ecProject_->setSpectraFluxRunMode(0);
        defaultRunRadioButton->setChecked(true);
    }
    defaultRunRadioButton->setEnabled(!configState_->project.smartfluxMode);

    if (isSpectralAssessmentCreationMode())
    {
        applySpectralAssessmentCreationRequirements();
    }

    updatePreprocessingAssessmentAvailability();
}

void AdvOutputOptions::updatePreprocessingAssessmentAvailability()
{
    const auto spectralAssessmentMode = isSpectralAssessmentCreationMode()
                                        || isProductionRunMode();

    auto timelagEnabled = canCreateTimelagAssessmentOnly();
    if (spectralAssessmentMode)
    {
        timelagEnabled = false;
    }
    timelagAssessmentOnlyCheckBox->setEnabled(timelagEnabled);
    if (!timelagEnabled && timelagAssessmentOnlyCheckBox->isChecked())
    {
        timelagAssessmentOnlyCheckBox->setChecked(false);
    }

    auto planarFitEnabled = canCreatePlanarFitAssessmentOnly();
    if (spectralAssessmentMode)
    {
        planarFitEnabled = false;
    }
    planarFitAssessmentOnlyCheckBox->setEnabled(planarFitEnabled);
    if (!planarFitEnabled && planarFitAssessmentOnlyCheckBox->isChecked())
    {
        planarFitAssessmentOnlyCheckBox->setChecked(false);
    }
}

void AdvOutputOptions::updateBinSpectra(bool b)
{
    updateOutBinSpectra(b);
    outBinSpectraCheckBox->setDisabled(b);
}

void AdvOutputOptions::updateOutputRaw1(bool b)
{
    ecProject_->setScreenOutRaw1(b);
    updateVarsAvailable();
}

void AdvOutputOptions::updateOutputRaw2(bool b)
{
    ecProject_->setScreenOutRaw2(b);
    updateVarsAvailable();
}

void AdvOutputOptions::updateOutputRaw3(bool b)
{
    ecProject_->setScreenOutRaw3(b);
    updateVarsAvailable();
}

void AdvOutputOptions::updateOutputRaw4(bool b)
{
    ecProject_->setScreenOutRaw4(b);
    updateVarsAvailable();
}

void AdvOutputOptions::updateOutputRaw5(bool b)
{
    ecProject_->setScreenOutRaw5(b);
    updateVarsAvailable();
}

void AdvOutputOptions::updateOutputRaw6(bool b)
{
    ecProject_->setScreenOutRaw6(b);
    updateVarsAvailable();
}

void AdvOutputOptions::updateOutputRaw7(bool b)
{
    ecProject_->setScreenOutRaw7(b);
    updateVarsAvailable();
}
void AdvOutputOptions::updateOutputRawU(bool b)
{
    ecProject_->setScreenOutRawU(b);
    updateSelectAllCheckbox();
}

void AdvOutputOptions::updateOutputRawV(bool b)
{
    ecProject_->setScreenOutRawV(b);
    updateVarsAvailable();
}

void AdvOutputOptions::updateOutputRawW(bool b)
{
    ecProject_->setScreenOutRawW(b);
    updateVarsAvailable();
}

void AdvOutputOptions::updateOutputRawTs(bool b)
{
    ecProject_->setScreenOutRawTs(b);
    updateVarsAvailable();
}

void AdvOutputOptions::updateOutputRawTair(bool b)
{
    ecProject_->setScreenOutRawTair(b);
    updateVarsAvailable();
}

void AdvOutputOptions::updateOutputRawPair(bool b)
{
    ecProject_->setScreenOutRawPair(b);
    updateVarsAvailable();
}

void AdvOutputOptions::checkFullSpectraAll(bool b)
{
    outFullSpectraCheckBoxU->setChecked(b);
    outFullSpectraCheckBoxV->setChecked(b);
    outFullSpectraCheckBoxW->setChecked(b);
    outFullSpectraCheckBoxTs->setChecked(b);
    for (const auto& row : gasRows_)
    {
        if (row.spectra)
        {
            row.spectra->setChecked(b && isOutputAvailable(row.gasIndex));
        }
    }
}

bool AdvOutputOptions::areAllCheckedVars()
{
    return (outRawUCheckBox->isChecked()
            && outRawVCheckBox->isChecked()
            && outRawWCheckBox->isChecked()
            && outRawTsCheckBox->isChecked()
            && allGasRawChecked()
            && outRawTairCheckBox->isChecked()
            && outRawPairCheckBox->isChecked());
}

void AdvOutputOptions::checkFullCospectraAll(bool b)
{
    outFullCospectraCheckBoxU->setChecked(b);
    outFullCospectraCheckBoxV->setChecked(b);
    outFullCospectraCheckBoxTs->setChecked(b);
    for (const auto& row : gasRows_)
    {
        if (row.cospectra)
        {
            row.cospectra->setChecked(b && isOutputAvailable(row.gasIndex));
        }
    }
}

void AdvOutputOptions::checkStAll(bool b)
{
    outSt1CheckBox->setChecked(b);
    outSt2CheckBox->setChecked(b);
    outSt3CheckBox->setChecked(b);
    outSt4CheckBox->setChecked(b);
    outSt5CheckBox->setChecked(b);
    outSt6CheckBox->setChecked(b);
    outSt7CheckBox->setChecked(b);
}

void AdvOutputOptions::checkTimeSeriesAll(bool b)
{
    outRaw1CheckBox->setChecked(b);
    outRaw2CheckBox->setChecked(b);
    outRaw3CheckBox->setChecked(b);
    outRaw4CheckBox->setChecked(b);
    outRaw5CheckBox->setChecked(b);
    outRaw6CheckBox->setChecked(b);
    outRaw7CheckBox->setChecked(b);
}

void AdvOutputOptions::checkVarsAll(bool b)
{
    outRawUCheckBox->setChecked(b);
    outRawVCheckBox->setChecked(b);
    outRawWCheckBox->setChecked(b);
    outRawTsCheckBox->setChecked(b);
    for (const auto& row : gasRows_)
    {
        if (row.raw) { row.raw->setChecked(b && isOutputAvailable(row.gasIndex)); }
    }
    outRawTairCheckBox->setChecked(b);
    outRawPairCheckBox->setChecked(b);
}

void AdvOutputOptions::selectMin()
{
    outFullCheckBox->setChecked(true);
    fluxnetErrLabelCheckBox->setChecked(false);
    fluxnetBiometCheckBox->setChecked(false);
    outDetailsCheckBox->setChecked(false);
    outMdCheckBox->setChecked(true);
    outBiometCheckBox->setChecked(false);
    createDatasetCheckBox->setChecked(true);
    outBinSpectraCheckBox->setChecked(true);
    outBinOgivesCheckBox->setChecked(false);
    outMeanSpectraCheckBox->setChecked(false);
    outMeanCospCheckBox->setChecked(false);
    checkFullSpectraAll(false);
    checkFullCospectraAll(false);
    outFullCospectraCheckBoxTs->setChecked(true);
    checkStAll(false);
    outSt1CheckBox->setChecked(true);
    checkTimeSeriesAll(false);
    checkVarsAll(false);
    updateGasOutputAvailability();
    setRequiredSpectralOutputState(currentSpectralMethodIndex());
}

void AdvOutputOptions::selectTypical()
{
    outFullCheckBox->setChecked(true);
    fluxnetErrLabelCheckBox->setChecked(false);
    fluxnetBiometCheckBox->setChecked(false);
    outDetailsCheckBox->setChecked(false);
    outMdCheckBox->setChecked(true);
    outBiometCheckBox->setChecked(true);
    createDatasetCheckBox->setChecked(true);
    outBinSpectraCheckBox->setChecked(true);
    outBinOgivesCheckBox->setChecked(true);
    outMeanSpectraCheckBox->setChecked(true);
    outMeanCospCheckBox->setChecked(true);
    checkFullSpectraAll(false);
    checkFullCospectraAll(false);
    outFullCospectraCheckBoxTs->setChecked(true);
    checkStAll(true);
    checkTimeSeriesAll(false);
    checkVarsAll(false);
    updateGasOutputAvailability();
    setRequiredSpectralOutputState(currentSpectralMethodIndex());
}

void AdvOutputOptions::selectFull()
{
    outFullCheckBox->setChecked(true);
    fluxnetErrLabelCheckBox->setChecked(true);
    fluxnetBiometCheckBox->setChecked(true);
    outDetailsCheckBox->setChecked(true);
    outMdCheckBox->setChecked(true);
    outBiometCheckBox->setChecked(true);
    createDatasetCheckBox->setChecked(true);
    outBinSpectraCheckBox->setChecked(true);
    outBinOgivesCheckBox->setChecked(true);
    outMeanSpectraCheckBox->setChecked(true);
    outMeanCospCheckBox->setChecked(true);
    checkFullSpectraAll(false);
    checkFullCospectraAll(false);
    outFullCospectraCheckBoxTs->setChecked(true);
    checkStAll(true);
    checkTimeSeriesAll(false);
    checkVarsAll(false);
    updateGasOutputAvailability();
    setRequiredSpectralOutputState(currentSpectralMethodIndex());
}

void AdvOutputOptions::setVarsAvailable(bool ok)
{
    outRawUCheckBox->setEnabled(ok);
    outRawVCheckBox->setEnabled(ok);
    outRawWCheckBox->setEnabled(ok);
    outRawTsCheckBox->setEnabled(ok);
    for (const auto& row : gasRows_)
    {
        if (row.raw) { row.raw->setEnabled(ok && isOutputAvailable(row.gasIndex)); }
    }
    outRawTairCheckBox->setEnabled(ok);
    outRawPairCheckBox->setEnabled(ok);
    outVarsAllCheckBox->setEnabled(ok);
}

/// The record set the current rows were built from.
QString AdvOutputOptions::gasSignature() const
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
QString AdvOutputOptions::gasRowLabel(int gasIndex) const
{
    return MeasurementRecords::gasLabel(ecProject_->gasColumns(), gasIndex);
}

bool AdvOutputOptions::gasSpectraFor(int gasIndex) const
{
    const auto& gases = ecProject_->gasColumns();
    if (gasIndex < 0 || gasIndex >= gases.size()) { return false; }
    if (gases.at(gasIndex).proc.outFullSp >= 0)
    {
        return gases.at(gasIndex).proc.outFullSp > 0;
    }
    return false;
}

bool AdvOutputOptions::gasCospectraFor(int gasIndex) const
{
    const auto& gases = ecProject_->gasColumns();
    if (gasIndex < 0 || gasIndex >= gases.size()) { return false; }
    if (gases.at(gasIndex).proc.outFullCospW >= 0)
    {
        return gases.at(gasIndex).proc.outFullCospW > 0;
    }
    return false;
}

bool AdvOutputOptions::gasRawFor(int gasIndex) const
{
    const auto& gases = ecProject_->gasColumns();
    if (gasIndex < 0 || gasIndex >= gases.size()) { return false; }
    if (gases.at(gasIndex).proc.outRaw >= 0)
    {
        return gases.at(gasIndex).proc.outRaw > 0;
    }
    return false;
}

void AdvOutputOptions::onGasSpectraToggled(int gasIndex, bool checked)
{
    auto gases = ecProject_->gasColumns();
    if (gasIndex < 0 || gasIndex >= gases.size()) { return; }
    gases[gasIndex].proc.outFullSp = checked ? 1 : 0;
    ecProject_->setGasColumns(gases);

}

void AdvOutputOptions::onGasCospectraToggled(int gasIndex, bool checked)
{
    auto gases = ecProject_->gasColumns();
    if (gasIndex < 0 || gasIndex >= gases.size()) { return; }
    gases[gasIndex].proc.outFullCospW = checked ? 1 : 0;
    ecProject_->setGasColumns(gases);

}

void AdvOutputOptions::onGasRawToggled(int gasIndex, bool checked)
{
    auto gases = ecProject_->gasColumns();
    if (gasIndex < 0 || gasIndex >= gases.size()) { return; }
    gases[gasIndex].proc.outRaw = checked ? 1 : 0;
    ecProject_->setGasColumns(gases);

    updateVarsAvailable();
}

/// The generated boxes, for the two collections that enable or disable the
/// whole page.
QWidgetList AdvOutputOptions::gasOutputCheckBoxes() const
{
    QWidgetList list;
    for (const auto& row : gasRows_)
    {
        if (row.spectra) { list << row.spectra; }
        if (row.cospectra) { list << row.cospectra; }
        if (row.raw) { list << row.raw; }
    }
    return list;
}

/// One checkbox per configured gas in each of the three output groups.
///
/// Records with no raw column are skipped: they are slots kept so the
/// engine's record-to-slot mapping stays put, not measurements. Checked
/// states are set before the boxes are connected, so a rebuild never writes
/// back into the project.
void AdvOutputOptions::rebuildGasRows()
{
    if (!ecProject_ || !spectraGrid_ || !cospectraGrid_ || !rawVarsGrid_)
    {
        return;
    }

    const auto dropBox = [](RichTextCheckBox* box)
    {
        if (!box) { return; }
        box->setParent(nullptr);
        box->deleteLater();
    };
    for (const auto& row : gasRows_)
    {
        dropBox(row.spectra);
        dropBox(row.cospectra);
        dropBox(row.raw);
    }
    gasRows_.clear();

    // The raw variables are one two-column block: the fixed wind and
    // temperature boxes, then the gases, then air temperature and pressure.
    // Taking them all out and re-adding them keeps that order whatever the
    // gas count.
    for (auto* box : { outRawUCheckBox, outRawVCheckBox, outRawWCheckBox,
                       outRawTsCheckBox, outRawTairCheckBox, outRawPairCheckBox })
    {
        rawVarsGrid_->removeWidget(box);
    }

    const auto& gases = ecProject_->gasColumns();
    QList<RichTextCheckBox*> rawOrder = { outRawUCheckBox, outRawVCheckBox,
                                          outRawWCheckBox, outRawTsCheckBox };

    int spectraRow = 5;      // after qBox_6 and the four fixed boxes
    int cospectraRow = 4;    // after qBox_7 and the three fixed entries
    // Alphabetical, so all three groups read the way the gases are named
    // rather than the way they were added. The index stays the record index -
    // that is what the three toggles write back through.
    for (const int i : MeasurementRecords::gasDisplayOrder(gases))
    {
        const int idx = i;
        const auto name = gasRowLabel(i);

        GasOutputRow row;
        row.gasIndex = i;

        row.spectra = new RichTextCheckBox;
        row.spectra->setText(tr("%1 (concentration or density)").arg(name));
        row.spectra->setChecked(gasSpectraFor(i));
        spectraGrid_->addWidget(row.spectra, spectraRow++, 0);

        row.cospectra = new RichTextCheckBox;
        row.cospectra->setText(QStringLiteral("W/%1").arg(name));
        row.cospectra->setChecked(gasCospectraFor(i));
        cospectraGrid_->addWidget(row.cospectra, cospectraRow++, 0);

        row.raw = new RichTextCheckBox;
        row.raw->setText(name);
        row.raw->setChecked(gasRawFor(i));
        rawOrder << row.raw;

        connect(row.spectra, &RichTextCheckBox::toggled, this,
                [=](bool checked) { onGasSpectraToggled(idx, checked); });
        connect(row.cospectra, &RichTextCheckBox::toggled, this,
                [=](bool checked) { onGasCospectraToggled(idx, checked); });
        connect(row.raw, &RichTextCheckBox::toggled, this,
                [=](bool checked) { onGasRawToggled(idx, checked); });

        gasRows_.append(row);
    }

    rawOrder << outRawTairCheckBox << outRawPairCheckBox;
    // Two columns, filled column by column, so a ten-variable project lands
    // exactly where it always did.
    const int perColumn = (rawOrder.size() + 1) / 2;
    for (int i = 0; i < rawOrder.size(); ++i)
    {
        rawVarsGrid_->addWidget(rawOrder.at(i), i % perColumn, i / perColumn);
    }

    gasSignature_ = gasSignature();
    updateGasOutputAvailability();
}

void AdvOutputOptions::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    // The gas records can change while another page is in front:
    // setGasColumns emits ecProjectModified, which nothing here listens to.
    if (gasSignature() != gasSignature_) { rebuildGasRows(); }
}

/// Whether the gas record at \a gasIndex names a column, and so has anything
/// to output. Replaces the four fixed isCo2/H2o/Ch4/Gas4 predicates, which
/// could only ever answer for the historical slots.
bool AdvOutputOptions::isOutputAvailable(int gasIndex) const
{
    const auto& gases = ecProject_->gasColumns();
    return gasIndex >= 0 && gasIndex < gases.size()
           && gases.at(gasIndex).rawColumn > 0;
}

/// Whether every gas that has raw output to offer has it selected. Part of
/// the "all variables" checkbox state.
bool AdvOutputOptions::allGasRawChecked() const
{
    for (const auto& row : gasRows_)
    {
        if (!isOutputAvailable(row.gasIndex)) { continue; }
        if (!row.raw || !row.raw->isChecked()) { return false; }
    }
    return true;
}

bool AdvOutputOptions::areTimeSeriesChecked()
{
    return (outRaw1CheckBox->isChecked()
            || outRaw2CheckBox->isChecked()
            || outRaw3CheckBox->isChecked()
            || outRaw4CheckBox->isChecked()
            || outRaw5CheckBox->isChecked()
            || outRaw6CheckBox->isChecked()
            || outRaw7CheckBox->isChecked());
}

void AdvOutputOptions::updateVarsAvailable()
{
    setVarsAvailable(areTimeSeriesChecked());
    outVarsAllCheckBox->setEnabled(areTimeSeriesChecked());
    updateGasOutputAvailability();
    updateSelectAllCheckbox();
}

void AdvOutputOptions::updateGasOutputAvailability()
{
    const bool smartfluxOn = configState_->project.smartfluxMode;
    const bool timeSeriesAvailable = areTimeSeriesChecked();

    for (const auto& row : gasRows_)
    {
        const bool available = isOutputAvailable(row.gasIndex);
        if (row.spectra) { row.spectra->setEnabled(available && !smartfluxOn); }
        if (row.cospectra) { row.cospectra->setEnabled(available && !smartfluxOn); }
        if (row.raw)
        {
            row.raw->setEnabled(available && timeSeriesAvailable && !smartfluxOn);
        }

        //> A gas whose column has gone leaves nothing to output, so its
        //> selections are cleared rather than left set on a dead record.
        if (!available)
        {
            if (row.spectra) { row.spectra->setChecked(false); }
            if (row.cospectra) { row.cospectra->setChecked(false); }
            if (row.raw) { row.raw->setChecked(false); }
        }
    }

    updateSelectAllCheckbox();
}

void AdvOutputOptions::updateOutputs(int n)
{
    setRequiredSpectralOutputState(n);
}

bool AdvOutputOptions::requiresBinnedSpectraOutput(int methodIndex,
                                                   bool spectraMode,
                                                   bool binnedSpectraAvailable)
{
    return methodIndex >= 2
           && methodIndex <= 4
           && spectraMode
           && !binnedSpectraAvailable;
}

bool AdvOutputOptions::requiresFullTsCospectraOutput(int methodIndex,
                                                     bool fullSpectraAvailable)
{
    return methodIndex == 4 && !fullSpectraAvailable;
}

void AdvOutputOptions::setRequiredIcon(QLabel* label, bool visible)
{
    auto pixmap = QPixmap(QStringLiteral(":/icons/msg-warning"));
#if defined(Q_OS_MACOS)
    pixmap.setDevicePixelRatio(2.0);
#endif
    label->setPixmap(pixmap.scaled(QSize(12, 12),
                                   Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation));
    label->setToolTip(tr("Required by selected spectral correction"));
    label->setVisible(visible);
}

void AdvOutputOptions::setRequiredSpectralOutputState(int methodIndex)
{
    const bool spectralAssessmentMode = isSpectralAssessmentCreationMode();
    const bool needsBinnedSpectra = requiresBinnedSpectraOutput(
        methodIndex,
        ecProject_->spectraMode(),
        ecProject_->generalBinSpectraAvail());
    const bool needsFullTsCospectra = requiresFullTsCospectraOutput(
        methodIndex,
        ecProject_->generalFullSpectraAvail());

    if (needsBinnedSpectra || spectralAssessmentMode)
    {
        outBinSpectraCheckBox->setChecked(true);
    }
    outBinSpectraCheckBox->setEnabled(!needsBinnedSpectra && !spectralAssessmentMode);
    setRequiredIcon(outBinSpectraRequiredIcon, needsBinnedSpectra || spectralAssessmentMode);

    if (needsFullTsCospectra || spectralAssessmentMode)
    {
        outFullCospectraCheckBoxTs->setChecked(true);
    }
    outFullCospectraCheckBoxTs->setEnabled(!needsFullTsCospectra && !spectralAssessmentMode);
    setRequiredIcon(outFullCospectraTsRequiredIcon, needsFullTsCospectra || spectralAssessmentMode);

    updateGasOutputAvailability();
    setRequiredGasFullCospectraOutputState(methodIndex);

    QString methodName;
    switch (methodIndex)
    {
    case 2:
        methodName = tr("Horst (1997)");
        break;
    case 3:
        methodName = tr("Ibrom et al. (2007)");
        break;
    case 4:
        methodName = tr("Fratini et al. (2012)");
        break;
    default:
        break;
    }

    const bool hasRequiredOutput = needsBinnedSpectra || needsFullTsCospectra || spectralAssessmentMode;
    spectralOutputsRequiredIcon->setVisible(hasRequiredOutput);
    spectralOutputsRequiredLabel->setVisible(hasRequiredOutput);
    if (hasRequiredOutput)
    {
        spectralOutputsRequiredLabel->setText(
            tr("<i>Outputs required by selected spectral correction %1</i>")
                .arg(methodName));
    }
}

void AdvOutputOptions::setRequiredGasFullCospectraOutputState(int methodIndex)
{
    const bool forceGasCospectra = isSpectralAssessmentCreationMode()
                                   && methodIndex == 4;
    if (!forceGasCospectra)
    {
        const bool smartfluxOn = configState_->project.smartfluxMode;
        for (const auto& row : gasRows_)
        {
            if (row.cospectra)
            {
                row.cospectra->setEnabled(isOutputAvailable(row.gasIndex)
                                          && !smartfluxOn);
            }
        }
        return;
    }

    for (const auto& row : gasRows_)
    {
        if (row.cospectra)
        {
            row.cospectra->setChecked(isOutputAvailable(row.gasIndex));
        }
    }

    for (const auto& row : gasRows_)
    {
        if (row.cospectra) { row.cospectra->setEnabled(false); }
    }
}

void AdvOutputOptions::updateFixedOuputFormat(int n)
{
    ecProject_->setGeneralFixedOutFormat(n);
}

void AdvOutputOptions::updateErrorLabel(const QString& s)
{
    if (s.isEmpty() || s.toUpper() == QLatin1String("NONE"))
    {
        WidgetUtils::warning(this,
                             tr("Error Label"),
                             tr("Enter a label other than "
                                "\"none\" (case insensitive)."));

        errorFormatCombo->setCurrentIndex(0);
        return;
    }
    ecProject_->setGeneralErrorLabel(s);
}

void AdvOutputOptions::updateFluxnetErrLabelMode(bool checked)
{
    errorFormatLabel->setEnabled(!checked);
    errorFormatCombo->setEnabled(!checked);
}

void AdvOutputOptions::createQuestionMark()
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

    connect(questionMark_1, &QPushButton::clicked,
            this, &AdvOutputOptions::onlineHelpTrigger_1);
    connect(questionMark_2, &QPushButton::clicked,
            this, &AdvOutputOptions::onlineHelpTrigger_2);
    connect(questionMark_4, &QPushButton::clicked,
            this, &AdvOutputOptions::onlineHelpTrigger_4);
    connect(questionMark_5, &QPushButton::clicked,
            this, &AdvOutputOptions::onlineHelpTrigger_5);
    connect(questionMark_8, &QPushButton::clicked,
            this, &AdvOutputOptions::onlineHelpTrigger_8);
    connect(questionMark_9, &QPushButton::clicked,
            this, &AdvOutputOptions::onlineHelpTrigger_9);
}

void AdvOutputOptions::onlineHelpTrigger_1()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Output_Files_Overview.html")));
}

void AdvOutputOptions::onlineHelpTrigger_2()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Output_Files.html")));
}

void AdvOutputOptions::onlineHelpTrigger_4()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Output_Files_Full_Output.html")));
}

void AdvOutputOptions::onlineHelpTrigger_5()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Calculating_Spectra_Cospectra_and_Ogives.html")));
}

void AdvOutputOptions::onlineHelpTrigger_6()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("http://")));
}

void AdvOutputOptions::onlineHelpTrigger_7()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("http://")));
}

void AdvOutputOptions::onlineHelpTrigger_8()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Output_Files_The_Stats_Folder.html")));
}

void AdvOutputOptions::onlineHelpTrigger_9()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/Output_Files.html")));
}

void AdvOutputOptions::onClickerrorFormatLabel()
{
    errorFormatCombo->setFocus();
    errorFormatCombo->showPopup();
}

void AdvOutputOptions::checkMetadataOutput()
{
    if (!outMdCheckBox->isChecked())
    {
        outMdCheckBox->setChecked(true);
    }
}

void AdvOutputOptions::updateSelectAllCheckbox()
{
    outVarsAllCheckBox->blockSignals(true);

    if (areAllCheckedVars())
        outVarsAllCheckBox->setChecked(true);
    else
        outVarsAllCheckBox->setChecked(false);

    outVarsAllCheckBox->blockSignals(false);
}

void AdvOutputOptions::setOutputBiomet()
{
    // save the modified flag to prevent side effects of setting widgets
    bool oldmod = ecProject_->modified();
    ecProject_->blockSignals(true);

    outBiometCheckBox->setChecked(true);

    // restore modified flag
    ecProject_->setModified(oldmod);
    ecProject_->blockSignals(false);
}


