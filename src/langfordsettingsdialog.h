/***************************************************************************
  langfordsettingsdialog.h
  ------------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#ifndef LANGFORDSETTINGSDIALOG_H
#define LANGFORDSETTINGSDIALOG_H

#include <QDialog>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class EcProject;

/// Settings for the limit of detection after Langford et al. (2015).
///
/// One set of settings, shared by the two places the method is offered: the
/// random uncertainty method of that name (ru_meth 6) and the flux detection
/// limit (detlim_meth 2). Both buttons open this same dialog, so the two can
/// never be configured to disagree.
class LangfordSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LangfordSettingsDialog(QWidget *parent, EcProject *ecProject);

public slots:
    void refresh();

private slots:
    void restoreDefaults();

private:
    void updateEnabled();

    QComboBox *windowCombo;
    QComboBox *statCombo;
    QDoubleSpinBox *itsMultSpin;
    QDoubleSpinBox *alphaSpin;
    QLabel *itsMultLabel;

    EcProject *ecProject_;
};

#endif // LANGFORDSETTINGSDIALOG_H
