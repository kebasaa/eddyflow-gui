/***************************************************************************
  timelagpreassessdialog.h
  ------------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#ifndef TIMELAGPREASSESSDIALOG_H
#define TIMELAGPREASSESSDIALOG_H

#include <QDateTime>
#include <QDialog>
#include <QElapsedTimer>
#include <QHash>
#include <QPair>
#include <QVector>

#include <memory>

#include "lagstats.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QTableWidget;
class QTemporaryDir;
class DlProject;
class EcProject;
class LagHistogramWidget;
class LogPercentSlider;
class Process;

//> "Timelag preliminary assessment": detect the time lags of a random sample
//> of the half-hours - or of all of them - with every pre-set lag window set
//> aside for a wide one, show where they fall, and offer a nominal lag and a
//> window per gas to write to the metadata.
//>
//> The engine runs on a throw-away copy of the project in a temporary
//> folder, with a home folder of its own, so neither the user's project nor
//> a run of the main window is touched. Its outputs go when the dialog closes
//> unless "Keep pre-assessment outputs" is ticked; a run over the entire
//> dataset leaves its time-lag optimisation file in the output folder either
//> way, as a normal pre-pass does.
class TimelagPreassessDialog : public QDialog
{
    Q_OBJECT
public:
    TimelagPreassessDialog(QWidget* parent, DlProject* dlProject, EcProject* ecProject);
    ~TimelagPreassessDialog() override;

    //> Why the assessment cannot run on this project's raw data, or an empty
    //> string if it can: there has to be a raw data folder holding raw files.
    static QString rawDataProblem(const EcProject* ecProject);

public slots:
    void refresh();
    void reject() override;

private slots:
    void runOrCancel();
    void readEngineOutput();
    void engineFinished();
    void updateSampleLabel();
    void wholeToggled(bool whole);
    void showGas(int comboIndex);
    void redrawCurrent();
    void selectGasColumn(int column);
    void setInMetadata();
    void useFileInProject();
    void keepToggled(bool keep);

private:
    //> One gas: a column of both tables.
    struct GasRow
    {
        int record = 0;                 //< 1-based gas record
        QString name;
        bool closedPath = true;
        double transit = -1.0;          //< tube transit time [s], <0 unknown
        bool suggestion = false;        //< the results column holds one
        QString note;                   //< shown as the column's tooltip
        bool noteIsWarning = false;
        QString instrumentId;           //< the analyser, for borrowing
        bool isWater = false;
        LagStats::PeakSuggestion peak;  //< its own, from its own lags
        int borrowedFrom = 0;           //< record lent from, 0 for its own
    };

    //> Rows of the two tables.
    enum WindowRow { WindowFrom = 0, WindowTo = 1 };
    enum ChartView { ViewHistogram = 0, ViewTimeSeries = 1 };
    enum ResultRow { ResultUsed = 0, ResultNominal = 1, ResultMinimum = 2, ResultMaximum = 3,
                     ResultBorrow = 4 };

    void buildGasRows();
    void fitTableHeight(QTableWidget* table) const;
    double cellValue(QTableWidget* table, int row, int column) const;
    void setValueCell(QTableWidget* table, int row, int column, double value, bool editable);
    void setTextCell(QTableWidget* table, int row, int column, const QString& text);
    void applyNote(int column);
    qint64 estimatePeriods() const;
    bool writeRunProject(const QString& dir, int seed, QString* projectPath, QString* error);
    void startEngine(const QString& projectPath);
    void setRunning(bool running);
    void loadResults();
    QVector<const LagStats::Row*> usableRows(int record, bool forSuggestion) const;
    void updateSuggestions();
    //> The gas on the same analyser whose own peak is clearest, which `column`
    //> may borrow from; -1 for none. Never water vapour, either way round.
    int donorFor(int column) const;
    //> The column's remark and its icon, from its peak, any borrowing and its
    //> tube-mates.
    void applyRemark(int column);
    QString nameOfRecord(int record) const;
    static QDateTime periodEnd(const LagStats::Row& row);
    void showSuggestion(int column, double nominal, double minimum, double maximum);
    void updateBorrowCell(int column);
    void toggleBorrow(int column);
    QString outputBaseDir() const;
    void copyOptimisationFiles();
    void keepOutputs();
    void saveFigures(const QString& dir);
    void discardOutputs();

    DlProject* dlProject_;
    EcProject* ecProject_;

    LogPercentSlider* slider_;
    QCheckBox* wholeCheck_;
    QLabel* sampleLabel_;
    QTableWidget* windowTable_;
    QCheckBox* keepCheck_;
    QPushButton* runButton_;
    QProgressBar* progressBar_;
    QLabel* statusLabel_;
    QPlainTextEdit* engineLog_;
    QPushButton* logToggle_;

    QComboBox* gasCombo_;
    QDoubleSpinBox* binSpin_;
    QComboBox* viewCombo_;
    QCheckBox* edgeCheck_;
    QCheckBox* fluxCheck_;
    LagHistogramWidget* histogram_;
    QLabel* histogramNote_;
    QTableWidget* resultTable_;
    QPushButton* setButton_;
    QPushButton* useFileButton_;

    QVector<GasRow> rows_;
    QVector<LagStats::Row> table_;
    qint64 periodsEstimate_ = -1;
    double percentBeforeWhole_ = 10.0;

    //> What the last run was asked: the whole dataset or not, which method,
    //> and each gas's search window - the table may have been edited since.
    bool runWhole_ = false;
    int runMethod_ = 0;
    QHash<int, QPair<double, double>> runWindows_;
    //> The optimisation file the last whole-dataset run left in the output
    //> folder, which "Use in project" points the project at.
    QString optFileForProject_;

    Process* engine_ = nullptr;
    std::unique_ptr<QTemporaryDir> workDir_;
    QString keptDir_;
    QByteArray pendingOutput_;
    QElapsedTimer runClock_;
    bool cancelled_ = false;
};

#endif  // TIMELAGPREASSESSDIALOG_H
