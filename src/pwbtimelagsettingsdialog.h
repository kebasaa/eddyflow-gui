/***************************************************************************
  pwbtimelagsettingsdialog.h
  --------------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#ifndef PWBTIMELAGSETTINGSDIALOG_H
#define PWBTIMELAGSETTINGSDIALOG_H

#include <QDialog>

class QCheckBox;
class QDoubleSpinBox;
class QButtonGroup;
class QRadioButton;
class QSpinBox;
class QWidget;
class EcProject;
class FileBrowseWidget;
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
    void setPwbControlsEnabled(bool enabled);
    void updateFile(const QString& fp);

private slots:
    void updateTlMode(int radioButton);
    void testSelectedFile(const QString& fp);

private:
    QRadioButton *existingRadio;
    QRadioButton *nonExistingRadio;
    QButtonGroup *radioGroup;
    FileBrowseWidget *fileBrowse;
    QWidget *pwbOptionsContainer;
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
    QCheckBox *detectOnRawCheckBox;
    QCheckBox *approxCcfCheckBox;
    QCheckBox *maxArOrderCheckBox;
    QSpinBox  *maxArOrderSpin;

    EcProject *ecProject_;
    ConfigState *configState_;
};

#endif // PWBTIMELAGSETTINGSDIALOG_H
