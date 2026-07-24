/***************************************************************************
  advoutputoptions.h
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

#ifndef ADVOUTPUTOPTIONS_H
#define ADVOUTPUTOPTIONS_H

#include <QWidget>

#include <vector>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QRadioButton;

class ClickLabel;
struct ConfigState;
class EcProject;
class RichTextCheckBox;

class AdvOutputOptions : public QWidget
{
    Q_OBJECT

public:
    explicit AdvOutputOptions(QWidget* parent,
                              EcProject* ecProject,
                              ConfigState* config);

    void setSmartfluxUI();

    static bool requiresBinnedSpectraOutput(int methodIndex,
                                            bool spectraMode,
                                            bool binnedSpectraAvailable);
    static bool requiresFullTsCospectraOutput(int methodIndex,
                                              bool fullSpectraAvailable);

public slots:
    void updateOutputs(int n);
    void checkMetadataOutput();
    void reset();
    void setOutputBiomet();

private slots:
    void refresh();

    void updateOutBinSpectra(bool b);
    void updateBinSpectra(bool b);

    void updateFixedOuputFormat(int n);
    void updateErrorLabel(const QString& s);
    void updateFluxnetErrLabelMode(bool checked);
    void updateSpectralAssessmentCreationMode(bool checked);
    void updateProductionRunMode(bool checked);
    void updateDefaultRunMode(bool checked);
    void updateTimelagAssessmentOnly(bool checked);
    void updatePlanarFitAssessmentOnly(bool checked);

    void checkFullSpectraAll(bool b);
    void checkFullCospectraAll(bool b);

    void updateOutputRaw1(bool b);
    void updateOutputRaw2(bool b);
    void updateOutputRaw3(bool b);
    void updateOutputRaw4(bool b);
    void updateOutputRaw5(bool b);
    void updateOutputRaw6(bool b);
    void updateOutputRaw7(bool b);
    void updateOutputRawU(bool b);
    void updateOutputRawV(bool b);
    void updateOutputRawW(bool b);
    void updateOutputRawTs(bool b);
    void updateOutputRawCo2(bool b);
    void updateOutputRawH2o(bool b);
    void updateOutputRawCh4(bool b);
    void updateOutputRawGas4(bool b);
    void updateOutputRawTair(bool b);
    void updateOutputRawPair(bool b);
    void checkStAll(bool b);
    void checkTimeSeriesAll(bool b);
    void checkVarsAll(bool b);
    void updateSelectAllCheckbox();

    void selectMin();
    void selectTypical();
    void selectFull();

    void onlineHelpTrigger_1();
    void onlineHelpTrigger_2();
    void onlineHelpTrigger_4();
    void onlineHelpTrigger_5();
    void onlineHelpTrigger_6();
    void onlineHelpTrigger_7();
    void onlineHelpTrigger_8();
    void onlineHelpTrigger_9();

    void onClickerrorFormatLabel();

private:
    bool areAllCheckedVars();
    void setVarsAvailable(bool ok);
    bool isCo2OutputAvailable() const;
    bool isH2oOutputAvailable() const;
    bool isCh4OutputAvailable() const;
    bool isGas4OutputAvailable() const;
    bool areTimeSeriesChecked();
    void updateVarsAvailable();
    void restoreOutputs();
    bool canCreateTimelagAssessmentOnly() const;
    bool canCreatePlanarFitAssessmentOnly() const;
    bool canCreateSpectralAssessment() const;
    bool validateSpectralAssessmentCreationRequest();
    int estimatedSpectralAssessmentIntervals() const;
    int currentSpectralMethodIndex() const;
    bool isSpectralAssessmentCreationMode() const;
    bool isProductionRunMode() const;
    void applySpectralAssessmentCreationRequirements();
    void applyProductionRunRequirements();
    void clearRunModeRadios();
    void updateSpectralAssessmentCreationAvailability();
    void updatePreprocessingAssessmentAvailability();
    void updateGasOutputAvailability();
    void setRequiredSpectralOutputState(int methodIndex);
    void setRequiredGasFullCospectraOutputState(int methodIndex);
    void setRequiredIcon(QLabel* label, bool visible);

    void createQuestionMark();

    std::vector<bool> oldEnabled{};
    std::vector<bool> oldVisible{};

    QPushButton* fullSelectionButton;
    QLabel* fullSelectionDesc;
    QPushButton* minSelectionButton;
    QLabel* minSelectionDesc;
    QPushButton* typicalSelectionButton;
    QLabel* typicalSelectionDesc;

    QRadioButton* spectralAssessmentCreationRadioButton;
    QRadioButton* productionRunRadioButton;
    QRadioButton* defaultRunRadioButton;
    QButtonGroup* assessmentRunModeRadioGroup;
    QLabel* spectralAssessmentCreationWarningIcon;
    QCheckBox* timelagAssessmentOnlyCheckBox;
    QCheckBox* planarFitAssessmentOnlyCheckBox;

    QCheckBox* outBinSpectraCheckBox;
    QLabel* outBinSpectraRequiredIcon;
    QCheckBox* outBinOgivesCheckBox;
    QCheckBox* outMeanSpectraCheckBox;
    QCheckBox* outMeanCospCheckBox;

    RichTextCheckBox* outFullSpectraCheckBoxU;
    RichTextCheckBox* outFullSpectraCheckBoxV;
    RichTextCheckBox* outFullSpectraCheckBoxW;
    RichTextCheckBox* outFullSpectraCheckBoxTs;
    RichTextCheckBox* outFullSpectraCheckBoxCo2;
    RichTextCheckBox* outFullSpectraCheckBoxH2o;
    RichTextCheckBox* outFullSpectraCheckBoxCh4;
    RichTextCheckBox* outFullSpectralCheckBoxGas4;

    RichTextCheckBox* outFullCospectraCheckBoxU;
    RichTextCheckBox* outFullCospectraCheckBoxV;
    RichTextCheckBox* outFullCospectraCheckBoxTs;
    QLabel* outFullCospectraTsRequiredIcon;
    RichTextCheckBox* outFullCospectraCheckBoxCo2;
    RichTextCheckBox* outFullCospectraCheckBoxH2o;
    RichTextCheckBox* outFullCospectraCheckBoxCh4;
    RichTextCheckBox* outFullCospectralCheckBoxGas4;

    QCheckBox* outFullCospectraAll;
    QGroupBox* fluxnetGroupBox;
    QCheckBox* fluxnetBiometCheckBox;
    QCheckBox* fluxnetErrLabelCheckBox;
    QCheckBox* outDetailsCheckBox;
    QCheckBox* outMdCheckBox;
    QCheckBox* outBiometCheckBox;
    QCheckBox* createDatasetCheckBox;
    QCheckBox* outFullCheckBox;
    ClickLabel* fullOutformatLabel;
    QRadioButton* fixedVarsOutputRadio;
    QRadioButton* variableVarsOutputRadio;
    QButtonGroup* outputFormatRadioGroup;
    ClickLabel* errorFormatLabel;
    QComboBox* errorFormatCombo;
    ClickLabel* level1Label;
    ClickLabel* level2Label;
    ClickLabel* level3Label;
    ClickLabel* level4Label;
    ClickLabel* level5Label;
    ClickLabel* level6Label;
    ClickLabel* level7Label;
    QCheckBox* outSt1CheckBox;
    QCheckBox* outSt2CheckBox;
    QCheckBox* outSt3CheckBox;
    QCheckBox* outSt4CheckBox;
    QCheckBox* outSt5CheckBox;
    QCheckBox* outSt6CheckBox;
    QCheckBox* outSt7CheckBox;
    QCheckBox* outRaw1CheckBox;
    QCheckBox* outRaw2CheckBox;
    QCheckBox* outRaw3CheckBox;
    QCheckBox* outRaw4CheckBox;
    QCheckBox* outRaw5CheckBox;
    QCheckBox* outRaw6CheckBox;
    QCheckBox* outRaw7CheckBox;
    RichTextCheckBox* outRawUCheckBox;
    RichTextCheckBox* outRawVCheckBox;
    RichTextCheckBox* outRawWCheckBox;
    RichTextCheckBox* outRawTsCheckBox;
    RichTextCheckBox* outRawCo2CheckBox;
    RichTextCheckBox* outRawH2oCheckBox;
    RichTextCheckBox* outRawCh4CheckBox;
    RichTextCheckBox* outRawGas4CheckBox;
    RichTextCheckBox* outRawTairCheckBox;
    RichTextCheckBox* outRawPairCheckBox;
    QCheckBox* outVarsAllCheckBox;

    QPushButton* questionMark_1;
    QPushButton* questionMark_2;
    QPushButton* questionMark_3;
    QPushButton* questionMark_4;
    QPushButton* questionMark_5;
    QPushButton* questionMark_6;
    QPushButton* questionMark_7;
    QPushButton* questionMark_8;
    QPushButton* questionMark_9;

    QLabel* hrLabel_1;
    QLabel* spectralOutputsRequiredIcon;
    QLabel* spectralOutputsRequiredLabel;

    EcProject* ecProject_;
    ConfigState* configState_;
};

#endif // ADVOUTPUTOPTIONS_H
