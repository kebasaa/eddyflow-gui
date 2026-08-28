/***************************************************************************
  advstatisticaloptions.h
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

#ifndef ADVSTATISTICALOPTIONS_H
#define ADVSTATISTICALOPTIONS_H

#include <QVector>
#include <QWidget>

#include "absolute_limit_units.h"

////////////////////////////////////////////////////////////////////////////////
/// \file src/advstatisticaloptions.h
/// \brief
/// \version
/// \date
/// \author      Antonio Forgione
/// \note
/// \sa
/// \bug
/// \deprecated
/// \test
/// \todo
////////////////////////////////////////////////////////////////////////////////

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGridLayout;
class QGroupBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QTabBar;
class QToolBox;

class ClickLabel;
class DetlimSettingsDialog;
class DlProject;
class EcProject;

/// \class AdvStatisticalOptions
/// \brief Class representing the 'Statisticals Tests' tab in the 'RawProcess' page
class AdvStatisticalOptions : public QWidget
{
    Q_OBJECT

public:
    //> Takes the raw file description as well as the project: the absolute
    //> limits are shown in the unit the column declares, which only the
    //> metadata knows.
    explicit AdvStatisticalOptions(QWidget* parent,
                                   DlProject* dlProject,
                                   EcProject* project);
    ~AdvStatisticalOptions();

public slots:
    void reset();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void refresh();

    /// Open the flux detection limit dialog, non-modally, as the CEC and PWB
    /// buttons open theirs.
    void showDetlimSettingsDialog();

    void on_spikeRemCheckBox_clicked(bool checked);
    void on_amplitudeResCheckBox_clicked(bool checked);
    void on_dropoutsCheckBox_clicked(bool checked);
    void on_absLimCheckBox_clicked(bool checked);
    void on_skewnessCheckBox_clicked(bool checked);
    void on_discontCheckBox_clicked(bool checked);
    void on_timeLagCheckBox_clicked(bool checked);
    void on_attackAngleCheckBox_clicked(bool checked);
    void on_nonSteadyCheckBox_clicked(bool checked);

    void on_defaultValuesButton_clicked();

    void selectAllTest(bool b);
    void updateSelectAllCheckbox();
    void updateRestoreDefault();

    void updateTestSr(bool b);
    void updateTestAr(bool b);
    void updateTestDo(bool b);
    void updateTestAl(bool b);
    void updateTestSk(bool b);
    void updateTestDs(bool b);
    void updateTestTl(bool b);
    void updateTestAa(bool b);
    void updateTestNs(bool b);
    void updateTestRf(bool b);

    void updateParamSrNumSpk(int n);
    void updateParamSrULim(double n);
    void updateParamSrWLim(double n);
    void updateParamSrHfLim(double n);
    void updateDespFilter(bool b);

    void updateParamArLim(double n);
    void updateParamArBins(int n);
    void updateParamArHfLim(int n);

    void updateParamDoExtLimDw(int n);
    void updateParamDoHf1Lim(double n);
    void updateParamDoHf2Lim(double n);

    void updateParamAlUMax(double n);
    void updateParamAlWMax(double n);
    void updateParamAlUMin(double n);
    void updateParamAlWMin(double n);
    void updateParamAlTsonMin(double n);
    void updateParamAlTsonMax(double n);
    void updateAbsLimFilter(bool b);

    void updateParamSkHfSkmin(double n);
    void updateParamSkHfSkmax(double n);
    void updateParamSkSfSkmin(double n);
    void updateParamSkSfSkmax(double n);
    void updateParamSkHfKumin(double n);
    void updateParamSkHfKumax(double n);
    void updateParamSkSfKumin(double n);
    void updateParamSkSfKumax(double n);

    void updateParamDsHfUV(double n);
    void updateParamDsHfW(double n);
    void updateParamDsHfT(double n);
    void updateParamDsHfVar(double n);
    void updateParamDsSfUV(double n);
    void updateParamDsSfW(double n);
    void updateParamDsSfT(double n);
    void updateParamDsSfVar(double n);

    void updateParamTlHfLim(double n);
    void updateParamTlSfLim(double n);

    void updateParamAaMin(double n);
    void updateParamAaMax(double n);
    void updateParamAaLim(double n);

    void updateParamNsHfLim(double n);

    void updateThumbnailGraphLabel(int i);

    void onlineHelpTrigger_1();
    void onlineHelpTrigger_2();
    void onlineHelpTrigger_3();
    void onlineHelpTrigger_4();
    void onlineHelpTrigger_5();
    void onlineHelpTrigger_6();
    void onlineHelpTrigger_7();
    void onlineHelpTrigger_8();
    void onlineHelpTrigger_9();
    void onlineHelpTrigger_10();
    void onlineHelpTrigger_11();

    void updateRandomErrorArea(bool b);
    //> ru_meth is written from the CEC settings dialog too, so this
    //> control has to be able to learn about it from somewhere other
    //> than a project load.
    void syncRandomErrorMethod();
    void setRandomErrorControlsEnabled(bool b);
    void onClickRandomMethodLabel();
    void updateRandomMethod(int n);
    void onClickItsDefinitionLabel();
    void updateItsDefinition(int n);
    void onTimelagMaxLabelCLicked();
    void updateTimelagMax(double d);
    void onSecurityCoeffLabelCLicked();
    void updateSecurityCoeff(double d);

    void onClickDespLabel_1();
    void onClickDespLabel_2();
    void onClickDespLabel_3();
    void onClickDespLabel_8();

    void onClickAmplResLabel_1();
    void onClickAmplResLabel_2();
    void onClickAmplResLabel_3();

    void onClickDropoutsLabel_1();
    void onClickDropoutsLabel_2();
    void onClickDropoutsLabel_3();

    void onClickAbsLimLabel_1();
    void onClickAbsLimLabel_2();
    void onClickAbsLimLabel_3();

    void onClickSkewnessLabel_1();
    void onClickSkewnessLabel_2();
    void onClickSkewnessLabel_5();
    void onClickSkewnessLabel_6();

    void onClickDiscontLabel_1();
    void onClickDiscontLabel_2();
    void onClickDiscontLabel_3();
    void onClickDiscontLabel_8();

    void onClickTimeLagLabel_1();
    void onClickTimeLagLabel_2();

    void onClickAttackAngleLabel_1();
    void onClickAttackAngleLabel_2();
    void onClickAttackAngleLabel_3();

    void onClickNonSteadyLabel_1();

    void updateTooltip(int i);

    void despikingRadioClicked(int b);
    void updateDespikingMethod(int b);

private:
    //> One generated row per configured gas, replacing the four fixed slots
    //> each of these tables used to carry. A site may measure the same species
    //> on several analysers, so the tables follow the project's gas records
    //> rather than a hard-wired CO2/H2O/CH4/4th-gas quartet.
    struct GasRow                   //< one value per gas
    {
        int gasIndex = -1;          //< index into EcProject::gasColumns()
        ClickLabel* label = nullptr;
        QDoubleSpinBox* spin = nullptr;
    };
    struct GasPairRow               //< two values per gas (min/max, hard/soft)
    {
        int gasIndex = -1;
        ClickLabel* label = nullptr;
        QDoubleSpinBox* first = nullptr;
        QDoubleSpinBox* second = nullptr;
    };

    //> Which per-gas setting a value belongs to. Used only to pick the
    //> built-in default for a gas that has none of its own.
    enum class GasParam { SrLim, StepLim, AlMin, AlMax, DsHf, DsSf, TlDef };

    void createTabWidget();
    void rebuildGasRows();
    QString gasSignature() const;
    QString gasRowLabel(int gasIndex) const;
    void configureAbsLimSpin(QDoubleSpinBox* spin, int gasIndex) const;
    AbsoluteLimitUnits::Scale absLimScale(int gasIndex) const;
    double gasParamFor(int gasIndex, GasParam param) const;
    double defaultGasParam(const QString& slug, GasParam param) const;
    void onGasParamChanged(int gasIndex, GasParam param, double value);
    void resetGasParamsToDefault();
    void setSpikeGasRowsEnabled(bool enabled);

    bool atLeastOneCheckedTest();
    bool areAllCheckedTests();
    int findClosestEnabledTest(int indexDisabled);
    void hideGraphLabels(bool hidden);
    void showThumbnailGraphLabel(bool visible);
    void createQuestionMark();
    bool requestTestSettingsReset();
    void setTestDefaultValues();

    QToolBox* testToolbox;
    QWidget* tab0;
    QRadioButton *vickersDespikingRadio;
    QRadioButton *mauderDespikingRadio;
    //> EddyUH's spi_method 1. Its parameters are absolute step limits, which
    //> share nothing with the sigma multipliers the other two use, so it
    //> carries its own four sonic spins and its own per-gas column.
    QRadioButton *stepDespikingRadio;
    ClickLabel* stepLabel_u;
    ClickLabel* stepLabel_v;
    ClickLabel* stepLabel_w;
    ClickLabel* stepLabel_ts;
    QDoubleSpinBox* stepSpin_u;
    QDoubleSpinBox* stepSpin_v;
    QDoubleSpinBox* stepSpin_w;
    QDoubleSpinBox* stepSpin_ts;
    QString stepGasTip_;
    QButtonGroup* despikingRadioGroup;
    QLabel* spikeGraphLabel;
    QSpinBox* despSpin_1;
    QDoubleSpinBox* despSpin_2;
    QDoubleSpinBox* despSpin_3;
    QDoubleSpinBox* despSpin_8;
    QCheckBox* despFilterCheckBox;

    QWidget* tab1;
    QLabel* amplResGraphLabel;
    QDoubleSpinBox* amplResSpin_1;
    QSpinBox* amplResSpin_2;
    QSpinBox* amplResSpin_3;

    QWidget* tab2;
    QLabel* dropoutsGraphLabel;
    QSpinBox* dropoutsSpin_1;
    QDoubleSpinBox* dropoutsSpin_2;
    QDoubleSpinBox* dropoutsSpin_3;

    QWidget* tab3;
    QLabel* absLimGraphLabel;
    QDoubleSpinBox* absLimSpin_1;
    QDoubleSpinBox* absLimSpin_2;
    QDoubleSpinBox* absLimSpin_3;
    QDoubleSpinBox* absLimSpin_4;
    QDoubleSpinBox* absLimSpin_13;
    QDoubleSpinBox* absLimSpin_14;
    QCheckBox* absLimFilterCheckBox;

    QWidget* tab4;
    QLabel* skewnessGraphLabel;
    QDoubleSpinBox* skewnessSpin_1;
    QDoubleSpinBox* skewnessSpin_2;
    QDoubleSpinBox* skewnessSpin_3;
    QDoubleSpinBox* skewnessSpin_4;
    QDoubleSpinBox* skewnessSpin_5;
    QDoubleSpinBox* skewnessSpin_6;
    QDoubleSpinBox* skewnessSpin_7;
    QDoubleSpinBox* skewnessSpin_8;

    QWidget* tab5;
    QLabel* discontGraphLabel;
    QDoubleSpinBox* discontSpin_1;
    QDoubleSpinBox* discontSpin_2;
    QDoubleSpinBox* discontSpin_3;
    QDoubleSpinBox* discontSpin_8;
    QDoubleSpinBox* discontSpin_9;
    QDoubleSpinBox* discontSpin_10;
    QDoubleSpinBox* discontSpin_11;
    QDoubleSpinBox* discontSpin_16;

    QWidget* tab6;
    QLabel* timelagGraphLabel;
    QDoubleSpinBox* timeLagSpin_1;
    QDoubleSpinBox* timeLagSpin_2;

    QWidget* tab7;
    QLabel* attackAngleGraphLabel;
    QDoubleSpinBox* attackAngleSpin_1;
    QDoubleSpinBox* attackAngleSpin_2;
    QDoubleSpinBox* attackAngleSpin_3;

    QWidget* tab8;
    QLabel* nonSteadyGraphLabel;
    QDoubleSpinBox* nonSteadySpin_1;

    QCheckBox* spikeRemCheckBox;
    QCheckBox* amplitudeResCheckBox;
    QCheckBox* dropoutsCheckBox;
    QCheckBox* absLimCheckBox;
    QCheckBox* skewnessCheckBox;
    QCheckBox* discontCheckBox;
    QCheckBox* timeLagCheckBox;
    QCheckBox* attackAngleCheckBox;
    QCheckBox* nonSteadyCheckBox;
    //> Not part of the Vickers & Mahrt (1997) nine above, and deliberately
    //> outside checkbox_list/areAllCheckedTests/selectAllTest: it has no
    //> parameter page of its own and "select all" restoring project
    //> defaults should not silently turn it on.
    QCheckBox* rfluxDiagCheckBox;
    //> Same reasoning as rfluxDiagCheckBox above: not one of the Vickers &
    //> Mahrt (1997) raw-data tests (it runs once, on the finished flux
    //> series), so it stays out of checkbox_list/areAllCheckedTests/
    //> selectAllTest too.
    QCheckBox* postFluxDespikeCheckBox;

    QCheckBox* selectAllCheckBox;
    QPushButton* defaultValuesButton;

    QLabel* thumbnailGraphLabel;

    ClickLabel* despLabel_1;
    ClickLabel* despLabel_2;
    ClickLabel* despLabel_3;
    ClickLabel* despLabel_8;
    ClickLabel* amplResLabel_1;
    ClickLabel* amplResLabel_2;
    ClickLabel* amplResLabel_3;
    ClickLabel* dropoutsLabel_1;
    ClickLabel* dropoutsLabel_2;
    ClickLabel* dropoutsLabel_3;
    ClickLabel* absLimLabel_1;
    ClickLabel* absLimLabel_2;
    ClickLabel* absLimLabel_3;
    ClickLabel* skewnessLabel_1;
    ClickLabel* skewnessLabel_2;
    ClickLabel* skewnessLabel_5;
    ClickLabel* skewnessLabel_6;
    ClickLabel* discontLabel_1;
    ClickLabel* discontLabel_2;
    ClickLabel* discontLabel_3;
    ClickLabel* discontLabel_8;
    ClickLabel* timeLagLabel_1;
    ClickLabel* timeLagLabel_2;
    ClickLabel* attackAngleLabel_1;
    ClickLabel* attackAngleLabel_2;
    ClickLabel* attackAngleLabel_3;
    ClickLabel* nonSteadyLabel_1;

    QGroupBox* acquisitionGroup;

    QPushButton* questionMark_1;
    QPushButton* questionMark_2;
    QPushButton* questionMark_3;
    QPushButton* questionMark_4;
    QPushButton* questionMark_5;
    QPushButton* questionMark_6;
    QPushButton* questionMark_7;
    QPushButton* questionMark_8;
    QPushButton* questionMark_9;
    QPushButton* questionMark_10;
    QPushButton* questionMark_11;

    QCheckBox* randomErrorCheckBox;
    ClickLabel* randomMethodLabel;
    QComboBox* randomMethodCombo;
    ClickLabel* itsDefinitionLabel;
    QComboBox* itsDefinitionCombo;
    ClickLabel* timelagMaxLabel;
    QDoubleSpinBox* timelagMaxSpin;
    ClickLabel* securityCoeffLabel;
    QDoubleSpinBox* securityCoeffSpin;

    //> Flux detection limit, Wienhold et al. (1994). A button rather than
    //> inline controls: the grid this sits in reserves column 0 for the
    //> random-error checkbox and pins its stretch at the row below the last
    //> live control, so the method plus its two windows go in a dialog.
    QPushButton* detlimSettingsButton;
    DetlimSettingsDialog* detlimDialog_{};

    //> The four grids that carry generated gas rows. Held because the rows,
    //> and everything the grid places below them, are positioned in
    //> rebuildGasRows() rather than at construction.
    QGridLayout* tab0Grid_ = nullptr;
    QGridLayout* tab3Grid_ = nullptr;
    QGridLayout* tab5Grid_ = nullptr;
    QGridLayout* tab6Grid_ = nullptr;

    //> Tooltips for the generated rows, taken from the fixed labels that head
    //> each table so the wording stays in one place.
    QString despGasTip_;
    QString absLimMinTip_;
    QString absLimMaxTip_;
    QString dsHardTip_;
    QString dsSoftTip_;

    QVector<GasRow> srRows_;        //< spike plausibility range
    QVector<GasRow> stepRows_;      //< consecutive-difference step limit
    QVector<GasPairRow> alRows_;    //< absolute limits, min and max
    QVector<GasPairRow> dsRows_;    //< discontinuities, hard and soft flag
    QVector<GasRow> tlRows_;        //< nominal time lag

    //> Where each grid's trailing stretch currently sits, so a shrinking gas
    //> count does not leave a stretched empty row behind.
    int srStretchRow_ = -1;
    int alStretchRow_ = -1;
    int dsStretchRow_ = -1;
    int tlStretchRow_ = -1;

    //> The record set the current rows were built from. Rebuilding is skipped
    //> when it has not changed, so a value the user is editing is not
    //> destroyed under them every time the page is shown.
    QString gasSignature_;

    DlProject* dlProject_;
    EcProject* ecProject_;
};

#endif // ADVSTATISTICALOPTIONS_H


