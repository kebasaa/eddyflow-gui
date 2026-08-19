/***************************************************************************
  pwbtimelagsettingsdialog.h
  --------------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#ifndef PWBTIMELAGSETTINGSDIALOG_H
#define PWBTIMELAGSETTINGSDIALOG_H

#include <QDialog>
#include <QVector>

class QDoubleSpinBox;
class QGridLayout;
class QLabel;
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
    //> One search-window row per configured gas, rather than four fixed rows.
    //>
    //> A site may measure the same species on several analysers, each with
    //> its own tube and so its own plausible lag range; four fixed rows could
    //> only describe one of them.
    struct LagRow
    {
        int gasIndex = -1;              //< index into EcProject::gasColumns()
        QLabel *label = nullptr;
        QDoubleSpinBox *minSpin = nullptr;
        QDoubleSpinBox *maxSpin = nullptr;
    };
    QVector<LagRow> lagRows_;
    QGridLayout *lagGrid_ = nullptr;

    void rebuildLagRows();
    void onLagChanged(int gasIndex, bool isMin, double value);
    double pwbMinLagFor(int gasIndex) const;
    double pwbMaxLagFor(int gasIndex) const;
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
