/***************************************************************************
  tlagdonordialog.cpp
  -------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#include "tlagdonordialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "ecproject.h"
#include "measurement_record.h"

TlagDonorDialog::TlagDonorDialog(QWidget *parent, EcProject *ecProject) :
    QDialog(parent),
    ecProject_(ecProject)
{
    setWindowTitle(tr("Time Lag Donors"));
    setWindowModality(Qt::WindowModal);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    grid_ = new QGridLayout;
    grid_->setColumnStretch(1, 1);

    emptyLabel_ = new QLabel(tr("No gas shares an analyser with another gas "
        "it could borrow from. Water is never borrowed for or from, and the "
        "analyser has to be named in the metadata."), this);
    emptyLabel_->setWordWrap(true);

    auto note = new QLabel(tr("<i>Automatic</i> takes the best-resolved gas on "
        "the analyser. A gas that names a donor borrows from that one only: "
        "if the donor did not clear the threshold itself in a period, nothing "
        "is borrowed for that period. Open-path gases never borrow, whatever "
        "is chosen here."), this);
    note->setWordWrap(true);

    auto resetButton = new QPushButton(tr("All Automatic"), this);
    resetButton->setToolTip(tr("Set every gas back to borrowing from the "
                               "best-resolved gas on its analyser."));
    auto closeButtons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    closeButtons->addButton(resetButton, QDialogButtonBox::ResetRole);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addLayout(grid_);
    mainLayout->addWidget(emptyLabel_);
    mainLayout->addWidget(note);
    mainLayout->addStretch();
    mainLayout->addWidget(closeButtons);
    setLayout(mainLayout);

    connect(resetButton, &QPushButton::clicked, this, [this] {
        auto gases = ecProject_->gasColumns();
        bool changed = false;
        for (auto& gas : gases)
        {
            if (gas.tlagDonor == 0) { continue; }
            gas.tlagDonor = 0;
            changed = true;
        }
        if (changed) { ecProject_->setGasColumns(gases); }
        refresh();
    });
    connect(closeButtons, &QDialogButtonBox::rejected, this, &QDialog::hide);
}

void TlagDonorDialog::refresh()
{
    while (auto item = grid_->takeAt(0))
    {
        if (auto widget = item->widget()) { widget->deleteLater(); }
        delete item;
    }

    const auto& gases = ecProject_->gasColumns();
    const auto order = MeasurementRecords::gasDisplayOrder(gases);
    int row = 0;
    for (const int i : order)
    {
        //> The donors this gas may name, in the same reading order as the
        //> rows. A gas with none gets no row: a combo holding only
        //> "Automatic" would offer a choice that is not one.
        QVector<int> donors;
        for (const int j : order)
        {
            if (MeasurementRecords::isTlagDonorFor(gases, i, j)) { donors << j; }
        }
        if (donors.isEmpty()) { continue; }

        auto label = new QLabel(MeasurementRecords::gasLabel(gases, i)
                                + QStringLiteral(" :"), this);
        auto combo = new QComboBox(this);
        //> Item data is the stored value: 0 automatic, else the 1-based
        //> record index, the same convention as moistureRef.
        combo->addItem(tr("Automatic (best-resolved)"), 0);
        for (const int j : donors)
        {
            combo->addItem(MeasurementRecords::gasLabel(gases, j), j + 1);
        }
        const int at = combo->findData(gases.at(i).tlagDonor);
        combo->setCurrentIndex(at >= 0 ? at : 0);

        //> Connected after the current index is set, so filling the row is
        //> not the user changing it.
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, combo, i](int) {
            setDonor(i, combo->currentData().toInt());
        });

        grid_->addWidget(label, row, 0, Qt::AlignRight);
        grid_->addWidget(combo, row, 1);
        ++row;
    }
    emptyLabel_->setVisible(row == 0);
}

void TlagDonorDialog::setDonor(int gasIndex, int donor)
{
    auto gases = ecProject_->gasColumns();
    if (gasIndex < 0 || gasIndex >= gases.size()) { return; }
    if (gases.at(gasIndex).tlagDonor == donor) { return; }
    gases[gasIndex].tlagDonor = donor;
    ecProject_->setGasColumns(gases);
}
