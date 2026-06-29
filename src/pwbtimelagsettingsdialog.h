/***************************************************************************
  pwbtimelagsettingsdialog.h
  --------------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#ifndef PWBTIMELAGSETTINGSDIALOG_H
#define PWBTIMELAGSETTINGSDIALOG_H

#include <QDialog>

class QDoubleSpinBox;
class QSpinBox;
class EcProject;
struct ConfigState;

class PwbTimelagSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PwbTimelagSettingsDialog(QWidget *parent, EcProject *ecProject, ConfigState *config);

public slots:
    void refresh();

private:
    QDoubleSpinBox *createLagSpin();
    QDoubleSpinBox *createSecondsSpin(double min, double max);
    QDoubleSpinBox *createFractionSpin();

    QDoubleSpinBox *co2MinLagSpin;
    QDoubleSpinBox *co2MaxLagSpin;
    QDoubleSpinBox *h2oMinLagSpin;
    QDoubleSpinBox *h2oMaxLagSpin;
    QDoubleSpinBox *ch4MinLagSpin;
    QDoubleSpinBox *ch4MaxLagSpin;
    QDoubleSpinBox *gas4MinLagSpin;
    QDoubleSpinBox *gas4MaxLagSpin;
    QSpinBox *nBootstrapSpin;
    QDoubleSpinBox *blockLengthSpin;
    QDoubleSpinBox *minValidFracSpin;
    QDoubleSpinBox *hdiThreshSpin;
    QDoubleSpinBox *devThreshSpin;
    QDoubleSpinBox *hdiPrefilterSpin;
    QSpinBox *smoothingWidthSpin;
    QSpinBox *randomSeedSpin;

    EcProject *ecProject_;
    ConfigState *configState_;
};

#endif // PWBTIMELAGSETTINGSDIALOG_H
