/***************************************************************************
  tlagdonordialog.h
  -----------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#ifndef TLAGDONORDIALOG_H
#define TLAGDONORDIALOG_H

#include <QDialog>

class QGridLayout;
class QLabel;
class EcProject;

/// Which tube-mate each gas borrows its time lag from, when lag borrowing is
/// chosen per gas.
///
/// A dialog rather than rows on the processing page, because the number of
/// rows is the number of gases. Each gas is offered only the gases on its own
/// analyser - the shared tube is the whole justification for borrowing - and
/// water is offered to nobody and never borrows.
class TlagDonorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TlagDonorDialog(QWidget *parent, EcProject *ecProject);

public slots:
    /// Rebuild the rows from the gas records, which change on another page.
    void refresh();

private:
    void setDonor(int gasIndex, int donor);

    QGridLayout *grid_;
    QLabel *emptyLabel_;
    EcProject *ecProject_;
};

#endif // TLAGDONORDIALOG_H
