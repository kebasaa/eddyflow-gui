/***************************************************************************
  eddyprogasslotdialog.h
  ----------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#ifndef EDDYPROGASSLOTDIALOG_H
#define EDDYPROGASSLOTDIALOG_H

#include <QDialog>

#include "eddyproexport.h"

class QComboBox;
class DlProject;

/// Which metadata column goes in each of EddyPro's four gas slots.
///
/// EddyPro has exactly one CO2, one H2O, one CH4 and one fourth gas, where
/// this program has as many of each as the site measures. Rather than guess
/// which analyser the user wants carried across, the export asks. Each slot
/// offers only the columns of its own species - the fourth takes any other
/// gas - and None, so a slot can be left empty on purpose.
///
/// Opens on the project's own assignment: the first gas of each species, which
/// is what an export without this dialog would write.
class EddyProGasSlotDialog : public QDialog
{
    Q_OBJECT

public:
    EddyProGasSlotDialog(QWidget* parent, const DlProject& dl,
                         const EddyProGasSlots& proposed);

    /// The columns chosen, 0 for a slot left empty.
    EddyProGasSlots choice() const;

    /// True when \a dl describes any gas column at all; without one there is
    /// nothing to choose and the dialog need not be shown.
    static bool hasGasColumns(const DlProject& dl);

private:
    QComboBox* combo_[4];
};

#endif // EDDYPROGASSLOTDIALOG_H
