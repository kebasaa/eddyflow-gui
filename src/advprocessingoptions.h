/***************************************************************************
  advprocessingoptions.h
  -------------------
  Copyright © 2007-2011, Eco2s team, Antonio Forgione
  Copyright © 2011-2018, LI-COR Biosciences
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

#ifndef ADVPROCESSINGOPTIONS_H
#define ADVPROCESSINGOPTIONS_H

#include <QWidget>

////////////////////////////////////////////////////////////////////////////////
/// \file src/advprocessingoptions.h
/// \brief
/// \version
/// \date
/// \author Antonio Forgione
/// \note
/// \sa PreProcessing
/// \bug
/// \deprecated
/// \test
/// \todo
////////////////////////////////////////////////////////////////////////////////

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QStackedWidget;
class QTabWidget;

class ClickLabel;
class CecSettingsDialog;
struct ConfigState;
class CustomResetLineEdit;
class DirBrowseWidget;
class DlProject;
class EcProject;
class PlanarFitSettingsDialog;
class PwbTimelagSettingsDialog;
class RichTextCheckBox;
class TimeLagSettingsDialog;

/// \class AdvlProcessingOptions
/// \brief Class representing the 'Settings' tab in the 'RawProcess' page
class AdvProcessingOptions : public QWidget
{
    Q_OBJECT

public:
    AdvProcessingOptions(QWidget* parent,
                         DlProject* dlProject,
                         EcProject* ecProject,
                         ConfigState* config);
    ~AdvProcessingOptions();

    PlanarFitSettingsDialog* getPlanarFitSettingsDialog() { return pfDialog_; }
    TimeLagSettingsDialog* getTimeLagSettingsDialog() { return tlDialog_; }

    //> Neutralise the options a SmartFlux module cannot run - Conditional Eddy
    //> Covariance and the pre-whitening block-bootstrap time lag, both of
    //> which are this program's own.
    void setSmartfluxUI();

public slots:
    void reset();

private slots:
    void updateUOffset(double d);
    void updateVOffset(double d);
    void updateWOffset(double d);
    void updateAoaMethod_1(bool b);
    void updateAoaMethod_2(int n);
    void updateRotMethod_1(bool b);
    void updateRotMethod_2(int n);
    void updatePfSettingsButton(bool b);
    void updateDetrendMeth(int l);
    void updateTimeConst(double l);
    void updateTlagMeth_1(bool b);
    void updateTlagMeth_2(int n);
    /// Grey the baseline-subtraction box unless the method maximises a
    /// covariance, which is the only thing it modifies.
    void updateCovmaxDebaselineAvailability();
    /// Grey the borrowing controls unless the flux detection limit is on.
    void updateTlagBorrowAvailability();
    void updateSonicHardwareAvailability();
    void onClickHeadCorrMethLabel();
    void onClickTiltSensorMethLabel();
    void updateTlSettingsButton(bool b);

    void onClickDetrendCombo(int detrendMethod);
    void onClickDetrendLabel();
    void onClickTimeConstantLabel();
    void updateWBoost(bool b);
    void onClickAoaMethLabel();
    void onClickRotMethLabel();
    void onClickTimeLagMethLabel();
    void onULabelClicked();
    void onVLabelClicked();
    void onWLabelClicked();

    void onClickQcMethodLabel();
    void updateQcMeth_1(bool b);
    void updateQcMeth_2(int n);
    void onClickFpMethodLabel();
    void updateFpMeth_1(bool b);
    void updateFpMeth_2(int n);

    void updateCecMeth_1(bool b);
    void updateCecAvailability();
    /// The partition reads the SIGN of each fluctuation, so it needs the
    /// density correction. Ticking it on switches WPL on; turning WPL back off
    /// warns and leaves the triangle beside it lit.
    void warnWplOffWithCec();
    void updateWplCecWarning();

    void updateWplMeth_1(bool b);
    void updateBurbaGroup(bool b);
    /// The engine refuses the instrument sensible heat terms outright when no
    /// LI-7500 family analyser is configured (override_settings.f90). Mirrored
    /// here so the interface does not offer a correction that will not run.
    void updateBurbaAvailability();
    void updateBurbaType_2(int n);
    void enableBurbaCorrectionArea(bool b);

    void on_setDefaultsButton_clicked();

    void refresh();

    void showPfSettingsDialog();
    void showTlSettingsDialog();
    void showCecSettingsDialog();

    void updateTooltip(int i);

    void onlineHelpTrigger_1();
    void onlineHelpTrigger_4();
    void onlineHelpTrigger_11();

private:
    enum class DetrendMethod {
        BlockAverage,
        LinearDetrending,
        RunningMean,
        ExponentialRunningMean
    };

    void createPfSettingsDialog();
    void createTlSettingsDialog();
    void createPwbTlSettingsDialog();
    void createCecSettingsDialog();
    void createBurbaParamItems();
    void createQuestionMark();
    bool requestBurbaSettingsReset();
    /// Whether the metadata describes an LI-7500 family analyser at all.
    bool hasLi7500FamilyIrga() const;
    void setBurbaDefaultValues();

    QLabel* windOffsetLabel;
    ClickLabel* uLabel;
    ClickLabel* vLabel;
    ClickLabel* wLabel;
    QDoubleSpinBox* uOffsetSpin;
    QDoubleSpinBox* vOffsetSpin;
    QDoubleSpinBox* wOffsetSpin;
    RichTextCheckBox* wBoostCheckBox;
    RichTextCheckBox* aoaCheckBox;
    ClickLabel* aoaMethLabel;
    QComboBox* aoaMethCombo;
    RichTextCheckBox* rotCheckBox;
    ClickLabel* rotMethLabel;
    QComboBox* rotMethCombo;
    QPushButton* pfSettingsButton;
    QLabel* detrendLabel;
    ClickLabel* detrendMethLabel;
    QComboBox* detrendCombo;
    RichTextCheckBox* timeLagCheckBox;
    ClickLabel* timeConstantLabel;
    QDoubleSpinBox* timeConstantSpin;
    ClickLabel* timeLagMethodLabel;
    QComboBox* timeLagMethodCombo;
    QPushButton* tlSettingsButton;
    RichTextCheckBox* qcCheckBox;
    ClickLabel* qcLabel;
    QComboBox* qcMethodCombo;
    RichTextCheckBox* fpCheckBox;
    ClickLabel* fpLabel;
    QComboBox* fpMethodCombo;

    RichTextCheckBox* cecCheckBox;
    QPushButton* cecSettingsButton;
    //> Kept because updateCecAvailability() swaps the tooltip for an
    //> "unavailable" one and has to be able to put this back. It used to
    //> restore cecCheckBox->toolTip(), which by then WAS the unavailable text.
    QString cecAvailableTooltip_;

    //> Same reason as cecAvailableTooltip_: updateBurbaAvailability() swaps in
    //> an "unavailable" tooltip and needs the original to put back.
    QString burbaAvailableTooltip_;

    //> Modifier on covariance maximisation. Greyed unless the time-lag method
    //> is one of the two that maximise a covariance.
    RichTextCheckBox* covmaxDebaselineCheckBox;
    RichTextCheckBox* parallelPrepassCheckBox;
    //> Conditional lag borrowing and its threshold. Greyed together with the
    //> flux detection limit, which they have nothing to test against without.
    RichTextCheckBox* tlagBorrowCheckBox;
    ClickLabel* tlagBorrowSnrLabel;
    QDoubleSpinBox* tlagBorrowSnrSpin;
    //> Which noise floor a covariance is judged against, and who a gas that
    //> fails borrows from. Both default to this program's own choice; the
    //> second entry on each is what EddyUH does.
    ClickLabel* tlagBorrowNoiseLabel;
    QComboBox* tlagBorrowNoiseCombo;
    ClickLabel* tlagBorrowDonorLabel;
    QComboBox* tlagBorrowDonorCombo;
    //> Two hardware corrections on the raw wind, ahead of any rotation.
    //> The Metek tables are that company's data and are not shipped, so the
    //> directory holding them is a setting rather than a fixed path.
    RichTextCheckBox* headCorrCheckBox;
    ClickLabel* headCorrMethLabel;
    QComboBox* headCorrMethCombo;
    ClickLabel* headCorrDirLabel;
    DirBrowseWidget* headCorrDirBrowse;
    //> The inclinometer's angles arrive as ordinary extra raw columns named
    //> theta, phi and psi, so nothing here says where they are - only how to
    //> read a voltage as an angle.
    RichTextCheckBox* tiltSensorCheckBox;
    ClickLabel* tiltSensorMethLabel;
    QComboBox* tiltSensorMethCombo;
    ClickLabel* tiltSensorVgLabel;
    QDoubleSpinBox* tiltSensorVgSpin;
    ClickLabel* tiltLpfLabel;
    QDoubleSpinBox* tiltLpfSpin;
    ClickLabel* tiltArmLabel;
    QLabel* tiltArmXLabel;
    QLabel* tiltArmYLabel;
    QLabel* tiltArmZLabel;
    QDoubleSpinBox* tiltArmXSpin;
    QDoubleSpinBox* tiltArmYSpin;
    QDoubleSpinBox* tiltArmZSpin;
    RichTextCheckBox* wplCheckBox;
    //> Closed-path spectroscopic correction, and its water-channel opt-in.
    //> The second is meaningless without the first and is greyed with it.
    RichTextCheckBox* spectroCheckBox;
    RichTextCheckBox* spectroWaterCheckBox;
    QLabel* wplWarningLabel;
    RichTextCheckBox* burbaCorrCheckBox;
    ClickLabel* burbaTypeLabel;
    QRadioButton* burbaSimpleRadio;
    QRadioButton* burbaMultiRadio;
    QPushButton* setDefaultsButton;
    QButtonGroup* burbaRadioGroup;
    QWidget* burbaSimpleDay;
    QWidget* burbaSimpleNight;
    QWidget* burbaMultiDay;
    QWidget* burbaMultiNight;
    QTabWidget* burbaSimpleTab;
    QTabWidget* burbaMultiTab;
    QStackedWidget* burbaParamWidget;

    CustomResetLineEdit* lDayBotGain;
    CustomResetLineEdit* lDayBotOffset;
    CustomResetLineEdit* lDayTopGain;
    CustomResetLineEdit* lDayTopOffset;
    CustomResetLineEdit* lDaySparGain;
    CustomResetLineEdit* lDaySparOffset;
    CustomResetLineEdit* lNightBotGain;
    CustomResetLineEdit* lNightBotOffset;
    CustomResetLineEdit* lNightTopGain;
    CustomResetLineEdit* lNightTopOffset;
    CustomResetLineEdit* lNightSparGain;
    CustomResetLineEdit* lNightSparOffset;

    CustomResetLineEdit* mDayBot1;
    CustomResetLineEdit* mDayBot2;
    CustomResetLineEdit* mDayBot3;
    CustomResetLineEdit* mDayBot4;
    CustomResetLineEdit* mDayTop1;
    CustomResetLineEdit* mDayTop2;
    CustomResetLineEdit* mDayTop3;
    CustomResetLineEdit* mDayTop4;
    CustomResetLineEdit* mDaySpar1;
    CustomResetLineEdit* mDaySpar2;
    CustomResetLineEdit* mDaySpar3;
    CustomResetLineEdit* mDaySpar4;
    CustomResetLineEdit* mNightBot1;
    CustomResetLineEdit* mNightBot2;
    CustomResetLineEdit* mNightBot3;
    CustomResetLineEdit* mNightBot4;
    CustomResetLineEdit* mNightTop1;
    CustomResetLineEdit* mNightTop2;
    CustomResetLineEdit* mNightTop3;
    CustomResetLineEdit* mNightTop4;
    CustomResetLineEdit* mNightSpar1;
    CustomResetLineEdit* mNightSpar2;
    CustomResetLineEdit* mNightSpar3;
    CustomResetLineEdit* mNightSpar4;

    QPushButton* questionMark_1;
    QPushButton* questionMark_4;
    QPushButton* questionMark_11;

    DlProject* dlProject_;
    EcProject* ecProject_;
    ConfigState* configState_;

    PlanarFitSettingsDialog* pfDialog_{};
    TimeLagSettingsDialog* tlDialog_{};
    PwbTimelagSettingsDialog* pwbTlDialog_{};
    CecSettingsDialog* cecDialog_{};

    DetrendMethod previousDetrendMethod_{DetrendMethod::BlockAverage};
};

#endif // ADVPROCESSINGOPTIONS_H
