/***************************************************************************
  cumcovsettingsdialog.h
  ----------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#ifndef CUMCOVSETTINGSDIALOG_H
#define CUMCOVSETTINGSDIALOG_H

#include <QDialog>

class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;
class QListWidget;
class QSpinBox;
class EcProject;

/// Settings for the cumulated-covariance time lag (tlag_meth 6) and its
/// clock-drift correction, after innFLUX (Striednig et al. 2020; the drift
/// correction after innFLUX v1.2.0's driftfix tool, unpublished).
///
/// Every value is innFLUX's by default. The method sums each gas's
/// cross-covariance functions over every period that passes the filters, per
/// date segment, and takes one lag per segment; the drift correction lets
/// that lag follow a drifting clock.
class CumCovSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CumCovSettingsDialog(QWidget *parent, EcProject *ecProject);

public slots:
    void refresh();

private slots:
    void restoreDefaults();
    void store();
    void storeReferences();

private:
    void updateEnabled();

    QSpinBox *filterSpin;
    QDoubleSpinBox *ustarSpin;
    QDoubleSpinBox *complSonicSpin;
    QDoubleSpinBox *complGasSpin;
    QDoubleSpinBox *sstSpin;
    QLineEdit *segmentsEdit;
    QCheckBox *driftCheck;
    QDoubleSpinBox *spanSpin;
    QDoubleSpinBox *jumpSpin;
    QDoubleSpinBox *maxDriftSpin;
    QListWidget *refList;

    EcProject *ecProject_;
    bool refreshing_ = false;
};

#endif // CUMCOVSETTINGSDIALOG_H
