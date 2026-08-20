/***************************************************************************
  cecsettingsdialog.cpp
  ---------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#include "cecsettingsdialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableView>
#include <QVBoxLayout>

#include "cecpairmodel.h"
#include "ecproject.h"

namespace
{
constexpr double DefaultH = 0.0;
constexpr double DefaultSingularBand = 0.20;
constexpr double DefaultMinO1O2 = 20.0;
constexpr double DefaultMinOctant = 5.0;
constexpr double DefaultMinValid = 90.0;
constexpr double DefaultSignalStrength = 70.0;
constexpr double DefaultMaxStationarity = 25.0;
constexpr int DefaultMaxGapFill = 4;
}

CecSettingsDialog::CecSettingsDialog(QWidget *parent, EcProject *ecProject) :
    QDialog(parent),
    ecProject_(ecProject)
{
    setWindowTitle(tr("CEC Settings"));
    setWindowModality(Qt::WindowModal);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    hSpin = new QDoubleSpinBox(this);
    //> Dimensionless now, scaled by sigma_w*sigma_s, so the useful range is a
    //> few multiples of one rather than the thousand an absolute product
    //> needed.
    hSpin->setRange(0.0, 10.0);
    hSpin->setDecimals(3);
    hSpin->setSingleStep(0.05);
    hSpin->setAccelerated(true);

    singularBandSpin = new QDoubleSpinBox(this);
    singularBandSpin->setRange(0.0, 1.0);
    singularBandSpin->setDecimals(2);
    singularBandSpin->setSingleStep(0.05);
    singularBandSpin->setAccelerated(true);

    minO1O2Spin = createPercentSpin();
    minOctantSpin = createPercentSpin();
    minValidSpin = createPercentSpin();
    signalStrengthSpin = createPercentSpin();
    maxStationaritySpin = createPercentSpin();
    maxStationaritySpin->setRange(0.0, 1000.0);

    maxGapFillSpin = new QSpinBox(this);
    maxGapFillSpin->setRange(0, 999);
    maxGapFillSpin->setSingleStep(1);
    maxGapFillSpin->setAccelerated(true);
    maxGapFillSpin->setSuffix(tr("  [samples]"));

    hLabel = new QLabel(tr("Hyperbolic threshold H :"), this);
    singularBandLabel = new QLabel(tr("Singularity band :"), this);
    minO1O2Label = new QLabel(tr("Minimum O1 + O2 occupancy :"), this);
    minOctantLabel = new QLabel(tr("Minimum per-octant occupancy :"), this);
    minValidLabel = new QLabel(tr("Minimum valid data :"), this);
    signalStrengthLabel = new QLabel(tr("Signal-strength cutoff :"), this);
    maxStationarityLabel = new QLabel(tr("Maximum stationarity :"), this);
    maxGapFillLabel = new QLabel(tr("Maximum small-gap fill :"), this);

    auto setOptionTooltip = [](QLabel *label, QWidget *editor, const QString &tooltip)
    {
        label->setToolTip(tooltip);
        editor->setToolTip(tooltip);
    };

    setOptionTooltip(hLabel, hSpin,
                     tr("<b>Hyperbolic threshold H:</b> Leaves out the events nearest the origin, whose sign is instrument noise rather than a surface signature. A point counts toward an octant only when |w's'| is at least H\xc2\xb7\xcf\x83<sub>w</sub>\xc2\xb7\xcf\x83<sub>s</sub>, so one value of H means the same thing at every site and in any unit. This is what makes the method usable when turbulence is weak: without it, noise around c' = 0 fills both octants about equally and the CO\xe2\x82\x82 ratio walks into the singularity below. Raising H also thins the octants, so the occupancy limits below may need to come down with it. Earlier MREA work used H = 0.25; Zahn et al. (2022) found their results insensitive to it and used none. Default: 0 (off)."));
    setOptionTooltip(singularBandLabel, singularBandSpin,
                     tr("<b>Singularity band:</b> When a species' two components nearly cancel, their ratio approaches \xe2\x88\x921 and the partition divides by something near zero on a total that is itself near zero. Periods whose ratio falls within this distance of \xe2\x88\x921 are reported as singular and their components withheld. Zahn et al. (2022) reject \xe2\x88\x921.2 to \xe2\x88\x920.8 and note the width is dataset-dependent. Only species whose components have opposite signs can reach it, so water is never affected. Set to 0 to disable. Default: 0.20."));
    setOptionTooltip(minO1O2Label, minO1O2Spin,
                     tr("<b>Minimum O1 + O2 occupancy:</b> O1 and O2 are the two upward-motion octants used by the CEC/MREA partitioning constraint. Zahn et al. (2022) require at least 20% of instantaneous points in these two octants combined. Default: 20%."));
    setOptionTooltip(minOctantLabel, minOctantSpin,
                     tr("<b>Minimum per-octant occupancy:</b> If an individual required octant contains too few points, the partitioning constraint treats the corresponding ground/non-stomatal component as negligible. Default: 5%."));
    setOptionTooltip(minValidLabel, minValidSpin,
                     tr("<b>Minimum valid data:</b> Retain a period only when at least 90% of instantaneous data points remain available after QC and preprocessing. Default: 90%."));
    setOptionTooltip(signalStrengthLabel, signalStrengthSpin,
                     tr("<b>Signal-strength cutoff:</b> When signal strength is available, CO2/H2O measurements below 70% are removed because this can indicate analyzer windows that need cleaning. Default: 70%."));
    setOptionTooltip(maxStationarityLabel, maxStationaritySpin,
                     tr("<b>Maximum stationarity:</b> Zahn et al. (2022) filtered out strongly non-stationary periods using the Foken (2017) stationarity test, removing periods when the non-stationarity statistic for w'c' or w'q' exceeded 25%. Lower positive values are stricter and may discard more averaging periods; higher values are more permissive. Allowed range: 0-1000%. Set to 0 to disable this gate for sensitivity/testing runs. Invalid INI values fall back to 25%. Default: 25%."));
    setOptionTooltip(maxGapFillLabel, maxGapFillSpin,
                     tr("<b>Maximum small-gap fill:</b> Small gaps up to 4 consecutive samples are filled by linear interpolation. Default: 4 samples."));

    auto partitionGroup = new QGroupBox(tr("Partitioning constraints"), this);
    partitionGroup->setToolTip(tr("Paper-derived CEC/MREA octant constraints from Zahn et al. (2022)."));
    auto partitionLayout = new QGridLayout(partitionGroup);
    partitionLayout->addWidget(hLabel, 0, 0, Qt::AlignRight);
    partitionLayout->addWidget(hSpin, 0, 1);
    partitionLayout->addWidget(minO1O2Label, 1, 0, Qt::AlignRight);
    partitionLayout->addWidget(minO1O2Spin, 1, 1);
    partitionLayout->addWidget(minOctantLabel, 2, 0, Qt::AlignRight);
    partitionLayout->addWidget(minOctantSpin, 2, 1);
    partitionLayout->addWidget(singularBandLabel, 3, 0, Qt::AlignRight);
    partitionLayout->addWidget(singularBandSpin, 3, 1);
    partitionLayout->setColumnStretch(2, 1);

    auto qcGroup = new QGroupBox(tr("QC/preprocessing limits"), this);
    qcGroup->setToolTip(tr("Paper-derived raw-data screening and gap-filling limits from Zahn et al. (2022)."));
    auto qcLayout = new QGridLayout(qcGroup);
    qcLayout->addWidget(minValidLabel, 0, 0, Qt::AlignRight);
    qcLayout->addWidget(minValidSpin, 0, 1);
    qcLayout->addWidget(signalStrengthLabel, 1, 0, Qt::AlignRight);
    qcLayout->addWidget(signalStrengthSpin, 1, 1);
    qcLayout->addWidget(maxStationarityLabel, 2, 0, Qt::AlignRight);
    qcLayout->addWidget(maxStationaritySpin, 2, 1);
    qcLayout->addWidget(maxGapFillLabel, 3, 0, Qt::AlignRight);
    qcLayout->addWidget(maxGapFillSpin, 3, 1);
    qcLayout->setColumnStretch(2, 1);

    //> Which channels go together, and what else rides along with them.
    //>
    //> This used to be implicit: the first record of each species, whatever
    //> analyser each happened to sit on. A site running two analysers had no
    //> way to say that each should be partitioned against its own water, and
    //> no way to partition anything but its CO2 and its H2O.
    auto pairGroup = new QGroupBox(tr("Channel pairing"), this);
    pairGroup->setToolTip(tr("Which CO\xe2\x82\x82 channel is partitioned against which H\xe2\x82\x82O channel, and which further species ride along in the same octants. Defaults to one pairing per CO\xe2\x82\x82 channel, each with the water on its own analyser."));

    pairModel = new CecPairModel(ecProject_, this);
    pairTable = new QTableView(pairGroup);
    pairTable->setModel(pairModel);
    pairTable->setItemDelegate(new CecPairDelegate(pairModel, pairTable));
    pairTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    pairTable->setSelectionMode(QAbstractItemView::SingleSelection);
    pairTable->setEditTriggers(QAbstractItemView::AllEditTriggers);
    pairTable->setAlternatingRowColors(true);
    pairTable->verticalHeader()->setVisible(false);
    pairTable->horizontalHeader()->setStretchLastSection(false);
    pairTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    pairTable->horizontalHeader()->setSectionResizeMode(CecPairModel::Extra,
                                                        QHeaderView::Stretch);
    pairTable->setMinimumHeight(120);

    addPairButton = new QPushButton(tr("Add"), pairGroup);
    addPairButton->setProperty("mdButton", true);
    addPairButton->setToolTip(tr("Add a pairing. Two pairings may share a CO\xe2\x82\x82 channel \xe2\x80\x93 a site with two hygrometers on one analyser \xe2\x80\x93 and their columns are told apart by an occurrence number after the instrument name."));
    removePairButton = new QPushButton(tr("Remove"), pairGroup);
    removePairButton->setProperty("mdButton", true);
    resetPairsButton = new QPushButton(tr("Same-analyser default"), pairGroup);
    resetPairsButton->setProperty("mdButton", true);
    resetPairsButton->setToolTip(tr("Discard the table and derive it again: one pairing per CO\xe2\x82\x82 channel, each with the H\xe2\x82\x82O on the same analyser."));

    auto pairButtons = new QHBoxLayout;
    pairButtons->addWidget(addPairButton);
    pairButtons->addWidget(removePairButton);
    pairButtons->addWidget(resetPairsButton);
    pairButtons->addStretch();

    auto pairLayout = new QVBoxLayout(pairGroup);
    pairLayout->addWidget(pairTable);
    pairLayout->addLayout(pairButtons);

    auto restoreButton = new QPushButton(tr("Restore Default Values"), this);
    restoreButton->setProperty("mdButton", true);
    restoreButton->setToolTip(tr("<b>Restore Default Values:</b> Resets CEC limits to the defaults used by Zahn et al. (2022)."));

    auto closeButtons = new QDialogButtonBox(QDialogButtonBox::Close, this);

    auto buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(restoreButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButtons);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(pairGroup);
    mainLayout->addWidget(partitionGroup);
    mainLayout->addWidget(qcGroup);
    mainLayout->addLayout(buttonLayout);
    setLayout(mainLayout);

    connect(hSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            ecProject_, &EcProject::setGeneralCecH);
    connect(minO1O2Spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            ecProject_, &EcProject::setGeneralCecMinO1O2);
    connect(minOctantSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            ecProject_, &EcProject::setGeneralCecMinOctant);
    connect(minValidSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            ecProject_, &EcProject::setGeneralCecMinValid);
    connect(signalStrengthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            ecProject_, &EcProject::setGeneralCecSignalStrength);
    connect(maxStationaritySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            ecProject_, &EcProject::setGeneralCecMaxStationarity);
    connect(maxGapFillSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            ecProject_, &EcProject::setGeneralCecMaxGapFill);
    connect(singularBandSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            ecProject_, &EcProject::setGeneralCecSingularBand);
    connect(addPairButton, &QPushButton::clicked,
            this, &CecSettingsDialog::addPair);
    connect(removePairButton, &QPushButton::clicked,
            this, &CecSettingsDialog::removeSelectedPair);
    connect(resetPairsButton, &QPushButton::clicked,
            pairModel, &CecPairModel::restoreDefaults);
    connect(pairTable->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &CecSettingsDialog::updatePairButtons);
    connect(pairModel, &QAbstractItemModel::modelReset,
            this, &CecSettingsDialog::updatePairButtons);
    connect(restoreButton, &QPushButton::clicked,
            this, &CecSettingsDialog::restoreDefaults);
    connect(closeButtons, &QDialogButtonBox::rejected,
            this, &CecSettingsDialog::hide);

    refresh();
}

void CecSettingsDialog::refresh()
{
    const QSignalBlocker hBlocker(hSpin);
    const QSignalBlocker minO1O2Blocker(minO1O2Spin);
    const QSignalBlocker minOctantBlocker(minOctantSpin);
    const QSignalBlocker minValidBlocker(minValidSpin);
    const QSignalBlocker signalStrengthBlocker(signalStrengthSpin);
    const QSignalBlocker maxStationarityBlocker(maxStationaritySpin);
    const QSignalBlocker maxGapFillBlocker(maxGapFillSpin);
    const QSignalBlocker singularBandBlocker(singularBandSpin);

    hSpin->setValue(ecProject_->generalCecH());
    minO1O2Spin->setValue(ecProject_->generalCecMinO1O2());
    minOctantSpin->setValue(ecProject_->generalCecMinOctant());
    minValidSpin->setValue(ecProject_->generalCecMinValid());
    signalStrengthSpin->setValue(ecProject_->generalCecSignalStrength());
    maxStationaritySpin->setValue(ecProject_->generalCecMaxStationarity());
    maxGapFillSpin->setValue(ecProject_->generalCecMaxGapFill());
    singularBandSpin->setValue(ecProject_->generalCecSingularBand());

    //> The pairings name gas records, so a record added or removed elsewhere
    //> can invalidate one. Re-read rather than trust what the table holds.
    pairModel->reload();
    updatePairButtons();
}

void CecSettingsDialog::addPair()
{
    pairModel->addPair();
    pairTable->selectRow(pairModel->rowCount() - 1);
}

void CecSettingsDialog::removeSelectedPair()
{
    const auto rows = pairTable->selectionModel()->selectedRows();
    if (rows.isEmpty()) { return; }
    pairModel->removePair(rows.first().row());
}

void CecSettingsDialog::updatePairButtons()
{
    removePairButton->setEnabled(
        !pairTable->selectionModel()->selectedRows().isEmpty());
}

void CecSettingsDialog::restoreDefaults()
{
    hSpin->setValue(DefaultH);
    minO1O2Spin->setValue(DefaultMinO1O2);
    minOctantSpin->setValue(DefaultMinOctant);
    minValidSpin->setValue(DefaultMinValid);
    signalStrengthSpin->setValue(DefaultSignalStrength);
    maxStationaritySpin->setValue(DefaultMaxStationarity);
    maxGapFillSpin->setValue(DefaultMaxGapFill);
    singularBandSpin->setValue(DefaultSingularBand);
    pairModel->restoreDefaults();
}

QDoubleSpinBox *CecSettingsDialog::createPercentSpin()
{
    auto spin = new QDoubleSpinBox(this);
    spin->setRange(0.0, 100.0);
    spin->setDecimals(1);
    spin->setSingleStep(1.0);
    spin->setAccelerated(true);
    spin->setSuffix(tr("  [%]"));
    return spin;
}
