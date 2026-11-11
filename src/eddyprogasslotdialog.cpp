/***************************************************************************
  eddyprogasslotdialog.cpp
  ------------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#include "eddyprogasslotdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "dlproject.h"

EddyProGasSlotDialog::EddyProGasSlotDialog(QWidget* parent, const DlProject& dl,
                                           const EddyProGasSlots& proposed) :
    QDialog(parent)
{
    setWindowTitle(tr("Export to EddyPro: Gas Columns"));
    setWindowModality(Qt::WindowModal);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    auto intro = new QLabel(tr("EddyPro processes one CO<sub>2</sub>, one "
        "H<sub>2</sub>O, one CH<sub>4</sub> and one other gas. Choose the "
        "metadata column for each. A gas not chosen here is not exported, "
        "and the report lists it."));
    intro->setWordWrap(true);

    const QString slotLabel[4] = {
        tr("CO<sub>2</sub> :"),
        tr("H<sub>2</sub>O :"),
        tr("CH<sub>4</sub> :"),
        tr("Fourth gas :"),
    };

    auto form = new QFormLayout;
    for (int s = 0; s < 4; ++s)
    {
        combo_[s] = new QComboBox;
        combo_[s]->addItem(tr("None"), 0);
        for (const auto& [column, label] : EddyProExport::slotCandidates(dl, s))
        {
            combo_[s]->addItem(label, column);
        }
        const int at = combo_[s]->findData(proposed.column[s]);
        combo_[s]->setCurrentIndex(at < 0 ? 0 : at);
        form->addRow(new QLabel(slotLabel[s]), combo_[s]);
    }

    auto note = new QLabel(tr("A fourth gas other than N<sub>2</sub>O is "
        "written as EddyPro's <i>n2o</i>, with its own molecular weight and "
        "diffusivity."));
    note->setWordWrap(true);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok
                                        | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto layout = new QVBoxLayout;
    layout->addWidget(intro);
    layout->addLayout(form);
    layout->addWidget(note);
    layout->addStretch();
    layout->addWidget(buttons);
    setLayout(layout);
}

EddyProGasSlots EddyProGasSlotDialog::choice() const
{
    EddyProGasSlots c;
    for (int s = 0; s < 4; ++s)
    {
        c.column[s] = combo_[s]->currentData().toInt();
    }
    return c;
}

bool EddyProGasSlotDialog::hasGasColumns(const DlProject& dl)
{
    for (int s = 0; s < 4; ++s)
    {
        if (!EddyProExport::slotCandidates(dl, s).isEmpty()) { return true; }
    }
    return false;
}
