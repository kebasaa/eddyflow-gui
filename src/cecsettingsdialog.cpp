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

    minO1O2Spin = createPercentSpin();
    minOctantSpin = createPercentSpin();
    minValidSpin = createPercentSpin();
    signalStrengthSpin = createPercentSpin();

    maxGapFillSpin = new QSpinBox(this);
    maxGapFillSpin->setRange(0, 999);
    maxGapFillSpin->setSingleStep(1);
    maxGapFillSpin->setAccelerated(true);
    maxGapFillSpin->setSuffix(tr("  [samples]"));

    hLabel = new QLabel(tr("Hyperbolic threshold H :"), this);
    minO1O2Label = new QLabel(tr("Minimum O1 + O2 occupancy :"), this);
    minOctantLabel = new QLabel(tr("Minimum per-octant occupancy :"), this);
    minValidLabel = new QLabel(tr("Minimum valid data :"), this);
    signalStrengthLabel = new QLabel(tr("Signal-strength cutoff :"), this);
    maxGapFillLabel = new QLabel(tr("Maximum small-gap fill :"), this);

    auto setOptionTooltip = [](QLabel *label, QWidget *editor, const QString &tooltip)
    {
        label->setToolTip(tooltip);
        editor->setToolTip(tooltip);
    };

    setOptionTooltip(hLabel, hSpin,
                     tr("<b>Hyperbolic threshold H:</b> Earlier MREA work used H = 0.25, but Zahn et al. (2022) found the method was no longer sensitive to H after adding the partitioning constraint and used no hyperbolic threshold. Default: 0."));
    setOptionTooltip(minO1O2Label, minO1O2Spin,
                     tr("<b>Minimum O1 + O2 occupancy:</b> O1 and O2 are the two upward-motion octants used by the CEC/MREA partitioning constraint. Zahn et al. (2022) require at least 20% of instantaneous points in these two octants combined. Default: 20%."));
    setOptionTooltip(minOctantLabel, minOctantSpin,
                     tr("<b>Minimum per-octant occupancy:</b> If an individual required octant contains too few points, the partitioning constraint treats the corresponding ground/non-stomatal component as negligible. Default: 5%."));
    setOptionTooltip(minValidLabel, minValidSpin,
                     tr("<b>Minimum valid data:</b> Retain a period only when at least 90% of instantaneous data points remain available after QC and preprocessing. Default: 90%."));
    setOptionTooltip(signalStrengthLabel, signalStrengthSpin,
                     tr("<b>Signal-strength cutoff:</b> When signal strength is available, CO2/H2O measurements below 70% are removed because this can indicate analyzer windows that need cleaning. Default: 70%."));
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
    partitionLayout->setColumnStretch(2, 1);

    auto qcGroup = new QGroupBox(tr("QC/preprocessing limits"), this);
    qcGroup->setToolTip(tr("Paper-derived raw-data screening and gap-filling limits from Zahn et al. (2022)."));
    auto qcLayout = new QGridLayout(qcGroup);
    qcLayout->addWidget(minValidLabel, 0, 0, Qt::AlignRight);
    qcLayout->addWidget(minValidSpin, 0, 1);
    qcLayout->addWidget(signalStrengthLabel, 1, 0, Qt::AlignRight);
    qcLayout->addWidget(signalStrengthSpin, 1, 1);
    qcLayout->addWidget(maxGapFillLabel, 2, 0, Qt::AlignRight);
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
