/***************************************************************************
  advspectraloptions.h
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

#ifndef ADVSPECTRALOPTIONS_H
#define ADVSPECTRALOPTIONS_H

#include <QVector>
#include <QWidget>

#include "configstate.h"

class QButtonGroup;
class QAbstractTableModel;
class QCheckBox;
class QComboBox;
class QDate;
class QDateEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QTableView;
class QTimeEdit;
class VariableDesc;

class AncillaryFileTest;
class ClickLabel;
class CustomClearLineEdit;
class DirBrowseWidget;
class DlProject;
class EcProject;
class FileBrowseWidget;

class AdvSpectralOptions : public QWidget
{
    Q_OBJECT
public:
    explicit AdvSpectralOptions(QWidget* parent,
                                DlProject* dlProject,
                                EcProject* ecProject,
                                ConfigState* config);
    ~AdvSpectralOptions();

    void setSmartfluxUI();

signals:
    void updateOutputsRequest(int n);

public slots:
    void reset();
    void partialRefresh();
    void refreshSpectralAssessmentCreationMode();

private slots:
    /// Grey the two iteration numbers unless the loop is switched on.
    void updateCorrIterAvailability();
    void refresh();
    void updateSpectraFile(const QString& fp);
    void updateBinnedSpectraFile(const QString& fp);
    void updateFullSpectraFile(const QString& fp);
    void testSelectedSpectraFile(const QString &fp);
    void binnedSpectraDirSelected(const QString &dir_path);
    void fullSpectraDirSelected(const QString &dir_path);
    void spectraRadioClicked(int radioButton);
    void binnedSpectraRadioClicked(int radioButton);
    void fullSpectraRadioClicked(int radioButton);

    void onClickHfMethLabel();
    void updateHfMethod_1(bool b);
    void updateHfMethod_2(int n);

    void onClickHorstLabel();
    void updateHorst_1(bool b);
    void updateHorst_2(int n);

    void onStartDateLabelClicked();
    void onEndDateLabelClicked();
    void updateStartDate(const QDate& d);
    void updateStartTime(const QTime& t);
    void updateEndDate(const QDate& d);
    void updateEndTime(const QTime& t);

    void onMinSmplLabelClicked();
    void updateMinSmpl(int n);

    void onlineHelpTrigger_11();
    void onlineHelpTrigger_1();
    void onlineHelpTrigger_2();
    void onlineHelpTrigger_3();
    void onlineHelpTrigger_4();
    void onlineHelpTrigger_5();

    void updateTooltip(int i);

    void onSubsetCheckboxToggled(bool b);

    void updateFilter(int n);
    void updateNBins(int n);

private:
    //> The six per-gas spin boxes of one QA/QC table row, generated per gas
    //> record. They are value holders, not laid-out widgets: the table model
    //> reads and writes them through the pointers in SpectralQaQcRow.
    //>
    //> An H2O record carries only the three frequency spins. Its minimum and
    //> maximum thresholds are the latent-heat triple, which is one per project
    //> rather than one per record and stays fixed - the same carve-out as
    //> leMinFluxSpin in the time-lag dialog.
    struct GasSpectralRow
    {
        int gasIndex = -1;                        //< index into gasColumns()
        QDoubleSpinBox* noiseFrequency = nullptr; //< sa_hfn_fmin
        QDoubleSpinBox* lowestFrequency = nullptr;//< sa_fmin
        QDoubleSpinBox* highestFrequency = nullptr;//< sa_fmax
        QDoubleSpinBox* minUnstable = nullptr;    //< sa_min_un
        QDoubleSpinBox* minStable = nullptr;      //< sa_min_st
        QDoubleSpinBox* maximum = nullptr;        //< sa_max
    };

    //> Which per-gas spectral setting a value belongs to.
    enum class SpectralParam { HfnFmin, Fmin, Fmax, MinUnstable, MinStable, Maximum };

    bool isHorstIbromFratini();
    bool isIbrom();
    bool isFratini();
    bool hasLi7500FamilyIrga() const;
    void maybeWarnMassmanFallback();
    void forceEndDatePolicy();
    void forceEndTimePolicy();

    void createQuestionMarks();
    void setHfMethod(int hfMethComboIndex);
    int hfComboIndexFromProjectMethod() const;
    void focusSpectralTableColumn(int column);
    void setSpectralAssessmentFrequencyCellsEnabled(bool enabled);
    void rebuildSpectralQaQcRows();
    void refreshSpectralQaQcTableState();
    const VariableDesc* rawVariableAtColumn(int column) const;
    bool selectedColumnIsVariable(int column, const QString& variableName) const;

    void rebuildGasSpectralSpins();
    QString gasSignature() const;
    QString gasRowLabel(int gasIndex) const;
    QDoubleSpinBox* makeGasSpectralSpin(int gasIndex, SpectralParam param);
    double gasSpectralFor(int gasIndex, SpectralParam param) const;
    double defaultGasSpectral(const QString& slug, SpectralParam param) const;
    void onGasSpectralChanged(int gasIndex, SpectralParam param, double value);
    void resetGasSpectralToDefault();

    QCheckBox* vmFlagsCheckBox;
    QCheckBox* lowQualityCheckBox;
    QCheckBox* moderateQualityCheckBox;

    ClickLabel* filterLabel;
    QComboBox* filterCombo;
    ClickLabel* nBinsLabel;
    QSpinBox* nBinsSpin;
    QCheckBox* fftCheckBox;

    QRadioButton* spectraExistingRadio;
    QRadioButton* spectraNonExistingRadio;
    QRadioButton* binnedSpectraExistingRadio;
    QRadioButton* binnedSpectraNonExistingRadio;
    QCheckBox* subsetCheckBox;
    ClickLabel* startDateLabel;
    QDateEdit* startDateEdit;
    QTimeEdit* startTimeEdit;
    QLabel* lockedIcon;
    ClickLabel* endDateLabel;
    QDateEdit* endDateEdit;
    QTimeEdit* endTimeEdit;
    FileBrowseWidget* spectraFileBrowse;
    QButtonGroup* spectraRadioGroup;
    DirBrowseWidget* binnedSpectraDirBrowse;
    QButtonGroup* binnedSpectraRadioGroup;
    QCheckBox* lfMethodCheck;
    QCheckBox* hfMethodCheck;
    ClickLabel* hfMethLabel;
    QComboBox* hfMethCombo;
    QCheckBox* horstCheck;
    ClickLabel* horstMethodLabel;
    QComboBox* horstCombo;
    //> The analytic cospectral shape every low-pass correction is
    //> integrated against - a modifier on all the methods above, not a
    //> method of its own, so it has no enabling checkbox of its own.
    //> Iterative correction: repeat the spectral correction and the two
    //> flux levels until the stability they assume and the stability they
    //> produce agree. Its two numbers are greyed with the checkbox.
    QCheckBox* corrIterCheckBox;
    ClickLabel* corrIterMaxLabel;
    QSpinBox* corrIterMaxSpin;
    ClickLabel* corrIterTolLabel;
    QDoubleSpinBox* corrIterTolSpin;
    ClickLabel* cospModelLabel;
    QComboBox* cospModelCombo;
    QCheckBox* hfCorrectGhgBaCheck;
    QCheckBox* hfCorrectGhgZohCheck;
    ClickLabel* sonicFrequencyLabel;
    QSpinBox* sonicFrequency;
    ClickLabel* minSmplLabel;
    QSpinBox* minSmplSpin;
    QDoubleSpinBox* qcMinUnstableUstarSpin;
    QDoubleSpinBox* qcMinUnstableHSpin;
    QDoubleSpinBox* qcMinUnstableLESpin;
    QDoubleSpinBox* qcMinStableUstarSpin;
    QDoubleSpinBox* qcMinStableHSpin;
    QDoubleSpinBox* qcMinStableLESpin;
    QDoubleSpinBox* qcMaxUstarSpin;
    QDoubleSpinBox* qcMaxHSpin;
    QDoubleSpinBox* qcMaxLESpin;

    //> One set of six spin boxes per configured gas, replacing the twelve
    //> frequency spins and twelve QA/QC spins that used to be fixed at four
    //> gases. A site may measure the same species on several analysers, each
    //> with its own filtering and so its own transfer-function window.
    QVector<GasSpectralRow> gasSpectralRows_;

    //> Tooltips shared by the generated spins, kept next to the table headers
    //> they belong to so the wording lives in one place.
    QString noiseFrequencyTip_;
    QString lowestFrequencyTip_;
    QString highestFrequencyTip_;
    QString minUnstableTip_;
    QString minStableTip_;
    QString maxTip_;

    //> The record set the current spins were built from, and the enabled state
    //> to reapply after a rebuild - the table's flags() reads it off the spin,
    //> so a fresh spin must not silently become editable.
    QString gasSignature_;
    bool frequencyCellsEnabled_ = false;

    QRadioButton* fullSpectraExistingRadio;
    QRadioButton* fullSpectraNonExistingRadio;
    DirBrowseWidget* fullSpectraDirBrowse;
    QButtonGroup* fullSpectraRadioGroup;
    QLabel* fratiniTitle;
    QCheckBox* addSonicCheck;
    QCheckBox* automaticSpectraConfigCheck;
    QAbstractTableModel* spectralQaQcModel;
    QTableView* spectralQaQcTable;

    QLabel* settingsGroupTitle_1;
    QLabel* lowFreqTitle;
    QLabel* highFreqTitle;
    QLabel* ghgSystemCorrectionTitle;
    QPushButton* questionMark_1;
    QPushButton* questionMark_11;
    QPushButton* questionMark_22;
    QPushButton* questionMark_33;
    QPushButton* questionMark_44;
    QPushButton* questionMark_55;

    DlProject* dlProject_;
    EcProject* ecProject_;
    ConfigState* configState_;

    bool spectraNonExistingRadioOldEnabled = false;
    bool massmanFallbackWarningShown_ = false;
};

#endif // ADVSPECTRALOPTIONS_H
