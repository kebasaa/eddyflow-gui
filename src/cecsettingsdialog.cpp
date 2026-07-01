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
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include "ecproject.h"

namespace
{
constexpr double DefaultH = 0.0;
constexpr double DefaultMinO1O2 = 20.0;
constexpr double DefaultMinOctant = 5.0;
constexpr double DefaultMinValid = 90.0;
constexpr double DefaultSignalStrength = 70.0;
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
    hSpin->setRange(0.0, 1000.0);
    hSpin->setDecimals(3);
    hSpin->setSingleStep(0.05);
    hSpin->setAccelerated(true);
    hSpin->setToolTip(tr("<b>Hyperbolic threshold H:</b> Threshold used by the octant partitioning constraints. Zahn et al. (2022) use no hyperbolic threshold, H = 0."));

    minO1O2Spin = createPercentSpin();
    minO1O2Spin->setToolTip(tr("<b>Minimum O1 + O2 occupancy:</b> Minimum percentage of instantaneous data points required in the first and second octants combined."));

    minOctantSpin = createPercentSpin();
    minOctantSpin->setToolTip(tr("<b>Minimum per-octant occupancy:</b> Minimum percentage of instantaneous data points required in each of the two octants."));

    minValidSpin = createPercentSpin();
    minValidSpin->setToolTip(tr("<b>Minimum valid data:</b> Minimum percentage of valid instantaneous data points required after QC and preprocessing."));

    signalStrengthSpin = createPercentSpin();
    signalStrengthSpin->setToolTip(tr("<b>Signal-strength cutoff:</b> Instantaneous data below this signal-strength threshold are removed before partitioning."));

    maxGapFillSpin = new QSpinBox(this);
    maxGapFillSpin->setRange(0, 999);
    maxGapFillSpin->setSingleStep(1);
    maxGapFillSpin->setAccelerated(true);
    maxGapFillSpin->setSuffix(tr("  [samples]"));
    maxGapFillSpin->setToolTip(tr("<b>Maximum small-gap fill:</b> Maximum length of consecutive missing samples that may be filled during preprocessing."));

    auto partitionGroup = new QGroupBox(tr("Partitioning constraints"), this);
    auto partitionLayout = new QGridLayout(partitionGroup);
    partitionLayout->addWidget(new QLabel(tr("Hyperbolic threshold H :"), this), 0, 0, Qt::AlignRight);
    partitionLayout->addWidget(hSpin, 0, 1);
    partitionLayout->addWidget(new QLabel(tr("Minimum O1 + O2 occupancy :"), this), 1, 0, Qt::AlignRight);
    partitionLayout->addWidget(minO1O2Spin, 1, 1);
    partitionLayout->addWidget(new QLabel(tr("Minimum per-octant occupancy :"), this), 2, 0, Qt::AlignRight);
    partitionLayout->addWidget(minOctantSpin, 2, 1);
    partitionLayout->setColumnStretch(2, 1);

    auto qcGroup = new QGroupBox(tr("QC/preprocessing limits"), this);
    auto qcLayout = new QGridLayout(qcGroup);
    qcLayout->addWidget(new QLabel(tr("Minimum valid data :"), this), 0, 0, Qt::AlignRight);
    qcLayout->addWidget(minValidSpin, 0, 1);
    qcLayout->addWidget(new QLabel(tr("Signal-strength cutoff :"), this), 1, 0, Qt::AlignRight);
    qcLayout->addWidget(signalStrengthSpin, 1, 1);
    qcLayout->addWidget(new QLabel(tr("Maximum small-gap fill :"), this), 2, 0, Qt::AlignRight);
    qcLayout->addWidget(maxGapFillSpin, 2, 1);
    qcLayout->setColumnStretch(2, 1);

    auto restoreButton = new QPushButton(tr("Restore Default Values"), this);
    restoreButton->setProperty("mdButton", true);
    restoreButton->setToolTip(tr("<b>Restore Default Values:</b> Resets CEC limits to the defaults used by Zahn et al. (2022)."));

    auto closeButtons = new QDialogButtonBox(QDialogButtonBox::Close, this);

    auto buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(restoreButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButtons);

    auto mainLayout = new QVBoxLayout(this);
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
    connect(maxGapFillSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            ecProject_, &EcProject::setGeneralCecMaxGapFill);
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
    const QSignalBlocker maxGapFillBlocker(maxGapFillSpin);

    hSpin->setValue(ecProject_->generalCecH());
    minO1O2Spin->setValue(ecProject_->generalCecMinO1O2());
    minOctantSpin->setValue(ecProject_->generalCecMinOctant());
    minValidSpin->setValue(ecProject_->generalCecMinValid());
    signalStrengthSpin->setValue(ecProject_->generalCecSignalStrength());
    maxGapFillSpin->setValue(ecProject_->generalCecMaxGapFill());
}

void CecSettingsDialog::restoreDefaults()
{
    hSpin->setValue(DefaultH);
    minO1O2Spin->setValue(DefaultMinO1O2);
    minOctantSpin->setValue(DefaultMinOctant);
    minValidSpin->setValue(DefaultMinValid);
    signalStrengthSpin->setValue(DefaultSignalStrength);
    maxGapFillSpin->setValue(DefaultMaxGapFill);
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
