/***************************************************************************
  cecsettingsdialog.h
  -------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#ifndef CECSETTINGSDIALOG_H
#define CECSETTINGSDIALOG_H

#include <QDialog>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableView;
class CecPairModel;
class DlProject;
class EcProject;

class CecSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CecSettingsDialog(QWidget *parent, EcProject *ecProject,
                               DlProject *dlProject);

public slots:
    void refresh();

private slots:
    void restoreDefaults();
    void addPair();
    void removeSelectedPair();
    void updatePairButtons();

private:
    QDoubleSpinBox *createPercentSpin();
    /// Grey the signal-strength cutoff, and flag it, when the metadata
    /// declares no diagnostic for the analysers CEC will read.
    void updateSignalStrengthAvailability();
    /// Switch random uncertainty estimation on, as the CEC checkbox switches
    /// on WPL: the significance test has nothing to compare a flux against
    /// until the random error is being estimated.
    void enableRandomUncertainty();
    /// The triangle beside the test: lit only while it is switched on and
    /// there is no random error for it to read.
    void updateMinFluxSigmaWarning();

    QDoubleSpinBox *hSpin;
    QDoubleSpinBox *singularBandSpin;
    QDoubleSpinBox *minO1O2Spin;
    QDoubleSpinBox *minOctantSpin;
    QDoubleSpinBox *minValidSpin;
    QDoubleSpinBox *signalStrengthSpin;
    QLabel *signalStrengthWarningLabel;
    QDoubleSpinBox *maxStationaritySpin;
    QCheckBox *ratioStationarityBox;
    QCheckBox *minFluxSigmaBox;
    QDoubleSpinBox *minFluxSigmaSpin;
    QLabel *minFluxSigmaWarningLabel;
    QSpinBox *maxGapFillSpin;

    QLabel *hLabel;
    QLabel *singularBandLabel;
    QLabel *minO1O2Label;
    QLabel *minOctantLabel;
    QLabel *minValidLabel;
    QLabel *signalStrengthLabel;
    QLabel *maxStationarityLabel;
    QLabel *maxGapFillLabel;

    QTableView *pairTable;
    CecPairModel *pairModel;
    QPushButton *addPairButton;
    QPushButton *removePairButton;
    QPushButton *resetPairsButton;

    EcProject *ecProject_;
    /// The Raw File Description. Only read, and only to ask whether a
    /// signal-strength column is declared for each analyser.
    DlProject *dlProject_;
};

#endif // CECSETTINGSDIALOG_H
