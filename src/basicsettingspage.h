/***************************************************************************
  basicsettingspage.h
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

#ifndef BASICSETTINGSPAGE_H
#define BASICSETTINGSPAGE_H

#include <QDateTime>
#include <QString>
#include <QMap>
#include <QVector>

#include <functional>
#include <QWidget>

#include <vector>

#include "fileutils.h"

////////////////////////////////////////////////////////////////////////////////
/// \file src/basicsettingspage.h
/// \brief
/// \version
/// \date
/// \author Antonio Forgione
/// \note
/// \sa
/// \bug
/// \deprecated
/// \test
/// \todo
////////////////////////////////////////////////////////////////////////////////

class QCalendarWidget;
class QCheckBox;
class QComboBox;
class QDate;
class QDateEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QTime;
class QTimeEdit;
class QNetworkAccessManager;
class QNetworkReply;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QGridLayout;
class QToolButton;
class QItemSelectionModel;
class QAbstractTableModel;
class QTableView;

class QProgressIndicator;

class AnemDesc;
struct BiomItem;
class ClickLabel;
struct ConfigState;
class CustomClearLineEdit;
class DirBrowseWidget;
class DlProject;
class EcProject;
class FileFormatWidget;
class IrgaDesc;
class RawFilenameDialog;
class SmartFluxBar;
class VariableDesc;
class WindFilterView;
class WindFilterTableModel;
class WindFilterTableView;

/// \class BasicSettingsPage
/// \brief Class representing the 'General Options' tab in the 'RawProcess' page
/// Which measurement a variable-table row stands for.
enum class VariableTableRole
{
    Co2, H2o, Ch4, Gas4,
    IntTc, IntT1, IntT2, IntP,
    Diag7500, Diag7200, Diag7700,
    AmbientT, AmbientP, Rh, Rg, Lwin, Ppfd
};

/// One selectable variable for a variable-table role: the raw column and the
/// label shown for it.
struct VariableCandidateItem
{
    int rawColumn = 0;
    QString text;
};

class BasicSettingsPage : public QWidget
{
    Q_OBJECT

public:
    //> Moisture column support.
    //>
    //> hasMoistureCandidates is false when the project has no H2O at all, in
    //> which case the column stays blank rather than offering an empty list.
    //> moistureLabelForGas resolves through the same rule as the engine, so
    //> what the table shows is what the fluxes were computed with.
    //> Multi-select support: gases are held as records, so a site can select
    //> the same species more than once - two H2O columns, say, one per
    //> analyser, which is what the Moisture column needs to be useful.
    QString canonicalInstrumentForColumn(int rawColumn) const;
    //> Species of the open (non-pinned) record slot, read from the record.
    QString openGasSpecies() const;
    bool gasRecordExists(const QString& slug, int rawColumn) const;
    int gasRecordIndexFor(const QString& slug, int rawColumn) const;
    //> Why a gas cannot be added, or an empty string if it can.
    QString gasLimitBlockReason(int rawColumn) const;
    void addGasRecord(const QString& slug, int rawColumn);
    void removeGasRecord(const QString& slug, int rawColumn);
    int firstGasColumn(const QString& slug) const;

    bool hasMoistureCandidates() const;
    QString moistureLabelForGas(int gasRecordIndex) const;
    QVector<QPair<int, QString>> moistureChoices() const;
    int moistureRefForGas(int gasRecordIndex) const;
    void setMoistureRefForGas(int gasRecordIndex, int moistureRef);
    qreal gasMolecularWeight(int gasRecordIndex) const;
    qreal gasDiffusivity(int gasRecordIndex) const;
    void setGasMolecularWeight(int gasRecordIndex, qreal value);
    void setGasDiffusivity(int gasRecordIndex, qreal value);
    int openGasRecordIndex() const;

    enum EmbeddedFileFlag
    {
        rawEmbeddedFile    = 1,
        biometEmbeddedFile = 2
    };

    BasicSettingsPage(QWidget *parent, DlProject *dlProject, EcProject *ecProject, ConfigState* config);
    ~BasicSettingsPage() override;

    Q_DECLARE_FLAGS(EmbeddedFileFlags, EmbeddedFileFlag)

    void updateSmartfluxBar();

public slots:
    void refresh();
    void datapathSelected(const QString &dir_path);
    void outpathBrowseSelected(const QString& dir_path);
    void askRawFilenamePrototype();
    void partialRefresh();
    void updateMetadataRead(bool firstReading = false);
    void clearFilesFound();
    void updateProjectFilesFound(int fileNumber);
    void clearSelectedItems();
    void alignDeclinationDate(const QDate& d);
    void showSetPrototype();

private:
    void noNoaaConnectionMsg();
    void noNoaaDownloadMsg();

    static const QString FLAG_POLICY_STRING_0;
    static const QString FLAG_POLICY_STRING_1;

    std::vector<bool> oldEnabled {};

    QPushButton* questionMark_1;
    QPushButton* questionMark_2;
    QPushButton* questionMark_3;
    QPushButton* questionMark_4;

    ClickLabel *datapathLabel;
    DirBrowseWidget *datapathBrowse;
    QCheckBox *recursionCheckBox;
    QLabel* filesFound;
    QProgressIndicator* findFileProgressWidget;

    ClickLabel *idLabel;
    CustomClearLineEdit *idEdit;

    ClickLabel *outpathLabel;
    DirBrowseWidget *outpathBrowse;

    ClickLabel *avgIntervalLabel;
    QSpinBox *avgIntervalSpin;

    ClickLabel *maxLackLabel;
    QSpinBox *maxLackSpin;

    QRadioButton* useMagneticNRadio;
    QRadioButton* useGeographicNRadio;

    QCheckBox *subsetCheckBox;
    QPushButton* dateRangeDetectButton;
    ClickLabel *startDateLabel;
    ClickLabel *endDateLabel;
    QDateEdit *startDateEdit;
    QDateEdit *endDateEdit;
    QTimeEdit *startTimeEdit;
    QTimeEdit *endTimeEdit;
    QLabel* lockedIcon;

    QCheckBox *crossWindCheckBox;

    ClickLabel *filePrototypeLabel;
    FileFormatWidget *filePrototypeEdit;

    ClickLabel* anemRefLabel;
    QComboBox* anemRefCombo;
    ClickLabel* anemFlagLabel;
    QComboBox* anemFlagCombo;
    ClickLabel* tsRefLabel;
    QComboBox* tsRefCombo;
    QWidget *gasExtension;
    ClickLabel* gasMwLabel;
    QDoubleSpinBox *gasMw;
    ClickLabel* gasDiffLabel;
    QDoubleSpinBox *gasDiff;
    QPushButton* moreButton;
    QComboBox* airTRefCombo;
    QComboBox* airPRefCombo;
    QComboBox* rhCombo;
    QComboBox* rgCombo;
    QComboBox* lwinCombo;
    QComboBox* ppfdCombo;
    ClickLabel* flag1Label;
    QComboBox* flag1VarCombo;
    QLabel* flag1UnitLabel;
    QDoubleSpinBox* flag1ThresholdSpin;
    QComboBox* flag1PolicyCombo;
    ClickLabel* flag2Label;
    QComboBox* flag2VarCombo;
    QLabel* flag2UnitLabel;
    QDoubleSpinBox* flag2ThresholdSpin;
    QComboBox* flag2PolicyCombo;
    ClickLabel* flag3Label;
    QComboBox* flag3VarCombo;
    QLabel* flag3UnitLabel;
    QDoubleSpinBox* flag3ThresholdSpin;
    QComboBox* flag3PolicyCombo;
    ClickLabel* flag4Label;
    QComboBox* flag4VarCombo;
    QLabel* flag4UnitLabel;
    QDoubleSpinBox* flag4ThresholdSpin;
    QComboBox* flag4PolicyCombo;
    ClickLabel* flag5Label;
    QComboBox* flag5VarCombo;
    QLabel* flag5UnitLabel;
    QDoubleSpinBox* flag5ThresholdSpin;
    QComboBox* flag5PolicyCombo;
    ClickLabel* flag6Label;
    QComboBox* flag6VarCombo;
    QLabel* flag6UnitLabel;
    QDoubleSpinBox* flag6ThresholdSpin;
    QComboBox* flag6PolicyCombo;
    ClickLabel* flag7Label;
    QComboBox* flag7VarCombo;
    QLabel* flag7UnitLabel;
    QDoubleSpinBox* flag7ThresholdSpin;
    QComboBox* flag7PolicyCombo;
    ClickLabel* flag8Label;
    QComboBox* flag8VarCombo;
    QLabel* flag8UnitLabel;
    QDoubleSpinBox* flag8ThresholdSpin;
    QComboBox* flag8PolicyCombo;
    ClickLabel* flag9Label;
    QComboBox* flag9VarCombo;
    QLabel* flag9UnitLabel;
    QDoubleSpinBox* flag9ThresholdSpin;
    QComboBox* flag9PolicyCombo;
    ClickLabel* flag10Label;
    QComboBox* flag10VarCombo;
    QLabel* flag10UnitLabel;
    QDoubleSpinBox* flag10ThresholdSpin;
    QComboBox* flag10PolicyCombo;

    DlProject *dlProject_;
    EcProject *ecProject_;
    ConfigState* configState_;

    RawFilenameDialog* rawFilenameDialog;
    QStringList suffixList_;

    QLabel* northLabel;
    ClickLabel* declinationLabel;
    QLineEdit* declinationEdit;
    ClickLabel* declinationDateLabel;
    QDateEdit* declinationDateEdit;
    QLabel* decChangingLabel;
    QPushButton* declinationFetchButton;

    QNetworkAccessManager *httpManager_;
    QNetworkReply *httpReply_;
    QByteArray httpBuffer_;
    QString declination_;
    QProgressIndicator* magneticDeclinationFetchProgress;

    QStringList currentRawDataList_;
    QStringList currentFilteredRawDataList_;

    QList<BiomItem> biomList_;

    QAbstractTableModel* fluxVariablesModel_;
    QAbstractTableModel* ambientVariablesModel_;
    QTableView* fluxVariablesTable_;
    QTableView* ambientVariablesTable_;

    QString lastEmbeddedMdFileRead_;
    //> Species the absolute-limit floor was last applied for, per gas record,
    //> so a user's custom value survives re-selecting the same gas. Keyed by
    //> record because one string cannot say *which* record changed, and a
    //> project may carry more than one row whose species comes from the data.
    QHash<int, QString> lastAbsLimitSpecies_;
    /// Shown once per session, not once per keystroke: the check
    /// below runs whenever the gas selection changes.
    bool noHumidityWarned_ = false;

    SmartFluxBar* smartfluxBar_;

    void captureEmbeddedMetadata(BasicSettingsPage::EmbeddedFileFlags type);
    void addNoneStr_1();
    void addNoneStr_2();
    void clearVarsCombo();
    void clearBiometCombo();
    void clearFlagVars();
    void clearFlagUnits();
    void clearFlagThresholdsAndPolicies();
    void filterVariables();
    void preselectDensityVariables(QComboBox* combo);
    void preselect7700Variables(QComboBox* combo);

    void createQuestionMark();

    void readEmbeddedMetadata(const QString& mdFile);
    void readAlternativeMetadata(const QString &mdFile, bool firstReading = false);

    void readBiomEmbMetadata(const QString& mdFile);
    bool readBiomAltMetadata(const QString& mdFile);

    void reloadSelectedItems_1();
    void reloadSelectedItems_2();
    void refreshVariableTables();

    //> Fill the species and instrument a migrated project could not carry:
    //> a pre-record file names a raw column and nothing else.
    void resolveMigratedGasRecords();
    //> Auto-select plausible gases the first time a metadata file is read,
    //> replacing the combo preselection that used to fill col_* invisibly.
    void seedGasRecordsFromMetadata();
    //> The anemometer diagnostic is the one diagnostic with a visible combo
    //> rather than a table row; it is restored from its record.
    void restoreAnemFlagFromRecord();

public:
    //> Candidate list for a variable-table role, when that role does not keep
    //> one in a combo. Empty for every role today: the flux-table roles still
    //> store their candidates in their (never laid out) combos, because
    //> filterVariables() prunes them there by item text. Converting that
    //> pruning is what stands between here and deleting the widgets.
    QVector<VariableCandidateItem> candidatesForRole(int role) const;

private:
    //> Candidates for the flux-table roles. They used to live in eleven
    //> QComboBoxes that were never laid out - widgets used as containers,
    //> which also let them shadow the selection into col_*. The shadowing is
    //> gone and so are the widgets; this is what is left.
    QMap<int, QVector<VariableCandidateItem>> fluxCandidates_;
    void addCandidate(VariableTableRole role, int rawColumn, const QString& text);
    void clearCandidates();
    void pruneCandidates(VariableTableRole role,
                         const std::function<bool(const QString&)>& drop);


public:
    //> Cell temperature/pressure and diagnostic records, driven by the
    //> variable table exactly as the gas records are. Public because the
    //> table model calls them, like the gas equivalents beside them.
    static QString nonGasSlugForRole(int role);
    void addNonGasRecord(const QString& slug, int rawColumn);
    void removeNonGasRecord(const QString& slug, int rawColumn);
    bool nonGasRecordExists(const QString& slug, int rawColumn) const;

private:

    int getSuggestedFilesToMerge();

    void forceEndDatePolicy();
    void forceEndTimePolicy();

    void updateFilesFoundLabel(int fileNumber);

    QString getFlagUnit(const VariableDesc& varStr);

    void parseMetadataProject(bool isEmbedded);
    void parseBiomMetadata();

    void setSmartfluxUI(bool on);
    void setPrototype(bool showDialog = false);

    QStringList getAvailableGhgSuffixes();
    QStringList filterRawDataWithPrototype(const QString &p);

    QString prototypeToRegExp(const QString &p);

    QGridLayout* windFilterLayout;
    QWidget *windFilterConfigFrame;
    QCheckBox* windFilterApplyCheckbox;
    QToolButton *addButton;
    QToolButton *removeButton;
    WindFilterTableModel *windFilterTableModel_;
    WindFilterTableView *windFilterTableView_;
    WindFilterView *windFilterView_;
    QItemSelectionModel *windFilterSelectionModel_;
    void createWindFilterArea();
    void setupWindFilterModel();
    void setupWindFilterViews();
    void insertAngleAt(int row);
    void removeAngleAt(int row);
    void resizeWindFilterRows();

private slots:
    void updateDataPath(const QString& dp);
    void updateRecursion(bool b);
    void updateOutPath(const QString& dp);
    void updateAvrgLen(int n);
    void updateMaxLack(int n);
    void updateFilePrototype(const QString& pattern);
    void updateFilePrototypeEdit(const QString& f);
    void updateSubsetSelection(bool b);

    void onIdLabelClicked();
    void onAvgLenLabelClicked();
    void onMaxLackLabelClicked();
    void onStartDateLabelClicked();
    void onEndDateLabelClicked();
    void updateStartDate(const QDate &d);
    void updateEndDate(const QDate &d);
    void updateStartTime(const QTime &t);
    void updateEndTime(const QTime &t);

    void onClickAnemRefLabel();
    void onClickAnemFlagLabel();
    void onClickTsRefLabel();
    void updateAnemRefCombo(const QString& s);
    void updateAnemFlagCombo(int i);
    void updateGasMw(double value);
    void updateGasDiff(double value);
    void updateFourthGasSettings(const QString& s);
    //> Species questions, not slot questions: they apply to whichever gas
    //> row takes its species from the data, and say nothing about N2O.
    void applyGasAbsoluteLimitMin(int gasIndex, const QString& species);
    void showGasDiffusivityWarning(const QString& species);
    /// Mirrors the engine's warning 104. See the definition: the
    /// GUI and engine conditions differ deliberately.
    void showNoHumidityWarning();
    void updateAirTRefCombo(int i);
    void updateAirPRefCombo(int);
    void updateRhCombo(int);
    void updateRgCombo(int);
    void updateLwinCombo(int);
    void updatePpfdCombo(int);
    void updateTsRefCombo(int i);

    void updateFlag1Combo(int i);
    void updateFlag2Combo(int i);
    void updateFlag3Combo(int i);
    void updateFlag4Combo(int i);
    void updateFlag5Combo(int i);
    void updateFlag6Combo(int i);
    void updateFlag7Combo(int i);
    void updateFlag8Combo(int i);
    void updateFlag9Combo(int i);
    void updateFlag10Combo(int i);

    void updateFlagUnit(int i);

    void updateCrossWind(bool b);

    void onClickFlagLabel();

    void updateFlag1Threshold(double n);
    void updateFlag2Threshold(double n);
    void updateFlag3Threshold(double n);
    void updateFlag4Threshold(double n);
    void updateFlag5Threshold(double n);
    void updateFlag6Threshold(double n);
    void updateFlag7Threshold(double n);
    void updateFlag8Threshold(double n);
    void updateFlag9Threshold(double n);
    void updateFlag10Threshold(double n);

    void updateFlag1Policy(int n);
    void updateFlag2Policy(int n);
    void updateFlag3Policy(int n);
    void updateFlag4Policy(int n);
    void updateFlag5Policy(int n);
    void updateFlag6Policy(int n);
    void updateFlag7Policy(int n);
    void updateFlag8Policy(int n);
    void updateFlag9Policy(int n);
    void updateFlag10Policy(int n);

    void onlineHelpTrigger_2();
    void onlineHelpTrigger_3();
    void onlineHelpTrigger_4();
    void onlineHelpTrigger_5();

    void reset();

    void updateFilesFound(bool recursionToggled);
    void runUpdateFilesFound();

    void fetchMagneticDeclination();
    void replyFinished(QNetworkReply* reply);
    void bufferHttpReply();
    bool parseHttpReply(const QByteArray& data);

    void northRadioClicked(int b);
    void updateMagDec(const QString& dec);
    double numDeclination(const QString &text);
    QString strDeclination(double dec);
    QString strVariation(double dec);
    void onClickDeclinationLabel();
    void onDeclinationDateLabelClicked();
    void updateUseGeoNorth(int b);
    void updateDeclinationDate(const QDate &d);
    void clearDataSelection();
    int handleVariableReset();
    int acceptVariableReset();
    void dateRangeDetect();
    void clearFilePrototype();

    void addWindFilterSector();
    void removeWindFilterSector();
    void windFilterModelModified();
    void updateWindFilterModel();

    void init();

signals:
    void updateMetadataReadResult(bool b);
    void setDateRangeRequest(FileUtils::DateRange);
    void saveSilentlyRequest();
    void fastTemperatureSelected();
};

Q_DECLARE_OPERATORS_FOR_FLAGS(BasicSettingsPage::EmbeddedFileFlags)

#endif // BASICSETTINGSPAGE_H
