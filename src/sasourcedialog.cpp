/***************************************************************************
  sasourcedialog.cpp
  ------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#include "sasourcedialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "ecproject.h"
#include "measurement_record.h"

SaSourceDialog::SaSourceDialog(QWidget *parent, EcProject *ecProject) :
    QDialog(parent),
    ecProject_(ecProject)
{
    setWindowTitle(tr("Spectral Assessment Sources"));
    setWindowModality(Qt::WindowModal);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    grid_ = new QGridLayout;
    grid_->setColumnStretch(2, 1);

    emptyLabel_ = new QLabel(tr("No gas shares an analyser with another gas "
        "it could take a transfer function from. Water never takes part, and "
        "the analyser has to be named in the metadata."), this);
    emptyLabel_->setWordWrap(true);

    auto note = new QLabel(tr("A transfer function fitted in situ measures the "
        "analyser's tube, cell and response, so a gas whose own fit follows "
        "noise can take that of a gas on the same analyser. <i>Automatic</i> "
        "keeps the gas's own fit, unless automatic selection is on and that "
        "fit looks implausible: Fn below %1, or a cut-off more than %2 times "
        "off the best-resolved gas on the analyser. <i>Own assessment</i> "
        "keeps it whatever it looks like. A gas whose source has no fit for a "
        "month is corrected analytically for that month. The assessment file "
        "always keeps each gas's own fit.")
        .arg(SpectralAssessmentFile::kFnMin)
        .arg(1.0 / SpectralAssessmentFile::kFcRatioMin), this);
    note->setWordWrap(true);

    auto resetButton = new QPushButton(tr("All Automatic"), this);
    resetButton->setToolTip(tr("Set every gas back to automatic."));
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
            if (gas.saSource == 0) { continue; }
            gas.saSource = 0;
            changed = true;
        }
        if (changed) { ecProject_->setGasColumns(gases); }
        refresh();
    });
    connect(closeButtons, &QDialogButtonBox::rejected, this, &QDialog::hide);
}

void SaSourceDialog::setFits(const SpectralAssessmentFile::Fits& fits,
                             bool fromFile)
{
    fits_ = fits;
    fromFile_ = fromFile;
    refresh();
}

void SaSourceDialog::refresh()
{
    while (auto item = grid_->takeAt(0))
    {
        if (auto widget = item->widget()) { widget->deleteLater(); }
        delete item;
    }

    const auto& gases = ecProject_->gasColumns();
    const auto order = MeasurementRecords::gasDisplayOrder(gases);
    const bool autoOn = ecProject_->spectraAutoSource() == 1;
    int row = 0;
    for (const int i : order)
    {
        //> The sources this gas may name, in the same reading order as the
        //> rows. A gas with none gets no row: a combo offering only its own
        //> fit would offer a choice that is not one.
        QVector<int> sources;
        for (const int j : order)
        {
            if (MeasurementRecords::isSaSourceFor(gases, i, j)) { sources << j; }
        }
        if (sources.isEmpty()) { continue; }

        const int suggested
            = SpectralAssessmentFile::suggestedSourceOverall(gases, fits_, i);
        const auto suggestedLabel = MeasurementRecords::gasLabel(gases, suggested);

        auto label = new QLabel(MeasurementRecords::gasLabel(gases, i)
                                + QStringLiteral(" :"), this);
        auto combo = new QComboBox(this);
        //> Item data is the stored value: 0 automatic, kSaSourceOwn the gas's
        //> own fit, else the 1-based record index.
        QString automatic;
        if (autoOn && suggested >= 0)
        {
            automatic = tr("Automatic %1 %2").arg(QString(QChar(0x2192)), suggestedLabel);
        }
        else if (autoOn)
        {
            automatic = tr("Automatic (own fit)");
        }
        else
        {
            automatic = tr("Automatic (off: own fit)");
        }
        combo->addItem(automatic, 0);
        if (suggested >= 0)
        {
            combo->setItemData(0, autoOn
                ? tr("The own fit looks implausible, so %1's is used.").arg(suggestedLabel)
                : tr("Suggested: %1. Turn on automatic selection, or choose it "
                     "here, to use it.").arg(suggestedLabel),
                Qt::ToolTipRole);
        }
        combo->addItem(tr("Own assessment"), kSaSourceOwn);
        for (const int j : sources)
        {
            combo->addItem(MeasurementRecords::gasLabel(gases, j), j + 1);
        }
        const int at = combo->findData(gases.at(i).saSource);
        combo->setCurrentIndex(at >= 0 ? at : 0);

        //> Connected after the current index is set, so filling the row is
        //> not the user changing it.
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, combo, i](int) {
            setSource(i, combo->currentData().toInt());
        });

        //> The gas's own fit, from the selected file.
        QString own;
        if (!fromFile_)
        {
            own = ecProject_->spectraMode() == 1
                ? tr("fitted in this run")
                : QStringLiteral("\u2014");
        }
        else if (!fits_.contains(i))
        {
            own = tr("not in the file");
        }
        else
        {
            own = SpectralAssessmentFile::fitSummary(fits_.value(i));
            if (suggested >= 0)
            {
                own = tr("%1 <b><font color=\"#c0392b\">implausible</font></b>").arg(own);
            }
        }
        auto ownLabel = new QLabel(own, this);
        ownLabel->setTextFormat(Qt::RichText);
        ownLabel->setToolTip(tr("This gas's own fit in the spectral assessment "
                                "file, over the months it covers."));

        grid_->addWidget(label, row, 0, Qt::AlignRight);
        grid_->addWidget(combo, row, 1);
        grid_->addWidget(ownLabel, row, 2);
        ++row;
    }
    emptyLabel_->setVisible(row == 0);
}

void SaSourceDialog::setSource(int gasIndex, int source)
{
    auto gases = ecProject_->gasColumns();
    if (gasIndex < 0 || gasIndex >= gases.size()) { return; }
    if (gases.at(gasIndex).saSource == source) { return; }
    gases[gasIndex].saSource = source;
    ecProject_->setGasColumns(gases);
}
