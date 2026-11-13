/***************************************************************************
  sasourcedialog.h
  ----------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#ifndef SASOURCEDIALOG_H
#define SASOURCEDIALOG_H

#include <QDialog>

#include "spectralassessmentfile.h"

class QGridLayout;
class QLabel;
class EcProject;

/// Whose fitted spectral transfer function each gas is corrected with.
///
/// For a gas too noisy for its own fit to mean anything - COS or N2O beside
/// a laser analyser's CO2 - the transfer function of a gas on the same
/// analyser that can be fitted. A dialog rather than rows on the spectral
/// page, because the number of rows is the number of gases. Each gas is
/// offered only gases on its own analyser, since tube, cell and response are
/// what the fit measures; water is offered to nobody and never borrows.
///
/// When a spectral assessment file is selected, each row shows the gas's own
/// fit from it and whether the engine would call that fit implausible.
class SaSourceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SaSourceDialog(QWidget *parent, EcProject *ecProject);

    /// The fits of the selected assessment file, and whether there is a file
    /// to have read them from. Rebuilds the rows.
    void setFits(const SpectralAssessmentFile::Fits& fits, bool fromFile);

public slots:
    /// Rebuild the rows from the gas records, which change on another page.
    void refresh();

private:
    void setSource(int gasIndex, int source);

    QGridLayout *grid_;
    QLabel *emptyLabel_;
    EcProject *ecProject_;
    SpectralAssessmentFile::Fits fits_;
    bool fromFile_ = false;
};

#endif // SASOURCEDIALOG_H
