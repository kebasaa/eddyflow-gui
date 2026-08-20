/***************************************************************************
  cecsettingsdialog.h
  -------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#ifndef CECSETTINGSDIALOG_H
#define CECSETTINGSDIALOG_H

#include <QDialog>

class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableView;
class CecPairModel;
class EcProject;

class CecSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CecSettingsDialog(QWidget *parent, EcProject *ecProject);

public slots:
    void refresh();

private slots:
    void restoreDefaults();
    void addPair();
    void removeSelectedPair();
    void updatePairButtons();

private:
    QDoubleSpinBox *createPercentSpin();

    QDoubleSpinBox *hSpin;
    QDoubleSpinBox *singularBandSpin;
    QDoubleSpinBox *minO1O2Spin;
    QDoubleSpinBox *minOctantSpin;
    QDoubleSpinBox *minValidSpin;
    QDoubleSpinBox *signalStrengthSpin;
    QDoubleSpinBox *maxStationaritySpin;
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
};

#endif // CECSETTINGSDIALOG_H
