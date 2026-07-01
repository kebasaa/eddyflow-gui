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
class QSpinBox;
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

private:
    QDoubleSpinBox *createPercentSpin();

    QDoubleSpinBox *hSpin;
    QDoubleSpinBox *minO1O2Spin;
    QDoubleSpinBox *minOctantSpin;
    QDoubleSpinBox *minValidSpin;
    QDoubleSpinBox *signalStrengthSpin;
    QSpinBox *maxGapFillSpin;

    QLabel *hLabel;
    QLabel *minO1O2Label;
    QLabel *minOctantLabel;
    QLabel *minValidLabel;
    QLabel *signalStrengthLabel;
    QLabel *maxGapFillLabel;

    EcProject *ecProject_;
};

#endif // CECSETTINGSDIALOG_H
