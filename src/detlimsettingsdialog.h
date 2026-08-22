/***************************************************************************
  detlimsettingsdialog.h
  ----------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#ifndef DETLIMSETTINGSDIALOG_H
#define DETLIMSETTINGSDIALOG_H

#include <QDialog>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class EcProject;

/// Settings for the flux detection limit after Wienhold et al. (1994).
///
/// A dialog rather than controls on the statistical options page: that page's
/// grid reserves its first column for one master checkbox and pins a stretch
/// at the row below the last live control, so a method plus two dependent
/// windows does not sit in it without rebuilding the layout.
class DetlimSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DetlimSettingsDialog(QWidget *parent, EcProject *ecProject);

public slots:
    void refresh();

private slots:
    void restoreDefaults();
    void updateWindowWarning();

private:
    QComboBox *methodCombo;
    QDoubleSpinBox *offsetSpin;
    QDoubleSpinBox *windowSpin;
    QLabel *methodLabel;
    QLabel *offsetLabel;
    QLabel *windowLabel;
    /// Lit when the half-width reaches the peak, which is the one way to
    /// configure this so that it measures the flux instead of the noise.
    QLabel *windowWarningLabel;

    EcProject *ecProject_;
};

#endif // DETLIMSETTINGSDIALOG_H
