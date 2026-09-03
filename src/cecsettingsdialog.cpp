/***************************************************************************
  cecsettingsdialog.cpp
  ---------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#include "cecsettingsdialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSet>
#include <QSpinBox>
#include <QStyle>
#include <QTableView>
#include <QVBoxLayout>

#include "cecpairmodel.h"
#include "dlproject.h"
#include "ecproject.h"
#include "variable_desc.h"

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
//> What the box offers when it is switched on. Two sigma, the usual line
//> between a flux and its own noise. The stored default is 0, meaning off.
constexpr double DefaultMinFluxSigma = 2.0;
}

CecSettingsDialog::CecSettingsDialog(QWidget *parent, EcProject *ecProject,
                                     DlProject *dlProject) :
    QDialog(parent),
    ecProject_(ecProject),
    dlProject_(dlProject)
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
    //> Lit when a channel CEC will read has no diagnostic to screen it by.
    signalStrengthWarningLabel = new QLabel(this);
    signalStrengthWarningLabel->setPixmap(QApplication::style()
                                          ->standardIcon(QStyle::SP_MessageBoxWarning)
                                          .pixmap(16, 16));
    signalStrengthWarningLabel->hide();
    maxStationaritySpin = createPercentSpin();
    ratioStationarityBox = new QCheckBox(tr("Judge the partition, not the flux"), this);
    ratioStationarityBox->setToolTip(tr(
        "<b>Which stationarity criterion gates the partition.</b>"
        "<p><b>Unchecked</b> is Zahn et al. (2022): the period is dropped when "
        "Foken's non-stationarity statistic for w'c' or w'q' exceeds the limit "
        "beside this box. That statistic divides by the very covariance it is "
        "testing, so it runs away as that covariance approaches zero \xe2\x80\x93 which "
        "for carbon at night it does. It then rejects periods whose octants are "
        "perfectly well sampled, and those are the night-time periods the paper "
        "reports the method handling best.</p>"
        "<p><b>Checked</b> asks instead whether the <i>split</i> between the two "
        "octants holds steady: the same six sub-intervals, each re-centred on "
        "its own mean, compared against the whole period. Both are measured as "
        "a fraction of the partition's own size, which cannot vanish while the "
        "octants hold anything, so the statistic stays bounded and a drift that "
        "scales both octants together cancels.</p>"
        "<p>The statistic is written out either way, as "
        "cec_ns_&lt;species&gt;, so the two can be compared on one run. Same "
        "limit for both; 0 disables the gate altogether. Leave this unchecked "
        "to reproduce the published method.</p>"));
    maxStationaritySpin->setRange(0.0, 1000.0);

    //> Is there a flux here at all?
    //>
    //> Nothing in the published method asks this, and on a well-coupled
    //> daytime record nothing needs to. It is the weak-turbulence case this
    //> guards: the octants only mean something if the sign of c' carries a
    //> surface signature, and when it does not the partition still returns two
    //> numbers that sum to the total.
    minFluxSigmaBox = new QCheckBox(tr("Only partition a resolvable flux"), this);
    minFluxSigmaSpin = new QDoubleSpinBox(this);
    minFluxSigmaSpin->setRange(0.5, 10.0);
    minFluxSigmaSpin->setDecimals(1);
    minFluxSigmaSpin->setSingleStep(0.5);
    minFluxSigmaSpin->setAccelerated(true);
    minFluxSigmaSpin->setSuffix(tr("  x random error"));
    minFluxSigmaSpin->setEnabled(false);
    //> Lit only in the inconsistent state - the test on, the estimate it reads
    //> switched off. A warning icon that is usually on is one nobody reads.
    minFluxSigmaWarningLabel = new QLabel(this);
    minFluxSigmaWarningLabel->setPixmap(QApplication::style()
                                        ->standardIcon(QStyle::SP_MessageBoxWarning)
                                        .pixmap(16, 16));
    minFluxSigmaWarningLabel->setToolTip(tr(
        "<b>This test is on and random uncertainty estimation is off.</b> "
        "There is no random error for it to compare a flux against, so it is "
        "being skipped and no period is refused by it. Switch it on under "
        "<i>Statistical Analysis &gt; Random uncertainty estimation</i>, with "
        "Finkelstein and Sims (2001). The run will say so as well, as "
        "Warning(115)."));
    minFluxSigmaWarningLabel->hide();

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
                     tr("<b>Singularity band:</b> When a species' two components nearly cancel, their ratio approaches \xe2\x88\x92" "1 and the partition divides by something near zero on a total that is itself near zero. Periods whose ratio falls within this distance of \xe2\x88\x92" "1 are reported as singular and their components withheld. Zahn et al. (2022) reject \xe2\x88\x92" "1.2 to \xe2\x88\x92" "0.8 and note the width is dataset-dependent. Only species whose components have opposite signs can reach it, so water is never affected. Set to 0 to disable. Default: 0.20."));
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
    //> Not through setOptionTooltip: the label half of this row is the
    //> checkbox itself, and that helper wants a QLabel.
    const auto minFluxSigmaTooltip = tr("<b>Only partition a resolvable flux:</b> Refuses a pairing whose water or carbon flux is smaller than this many times its own random error, reporting it as insignificant instead of splitting it."
       "<p>The octants mean something only if the sign of c&prime; carries a surface signature. Where the scalar is barely coupled to the vertical wind that sign is noise, the moist ejections split near evenly between O1 and O2, and the method returns an E/T and a Reco/P split of a signal that is not there. Nothing else in CEC notices: the occupancy gates see two well-filled octants, and the components still sum to the total.</p>"
       "<p>Finkelstein and Sims (2001) give the random error of a covariance from the period's own integral timescale, and |F| / RE is about |r| &times; &radic;(N<sub>indep</sub> / 2) &ndash; so this is the significance of that correlation, with the number of independent samples measured rather than assumed. It therefore needs <b>random uncertainty estimation</b>, and ticking this box switches it on.</p>"
       "<p><b>Higher values reject more periods</b>, night-time ones first. The paper suggests no value at all, because it applies no such test; 2 is the usual line between a flux and its own noise, and it is what this box offers. Refused periods are flagged 6 in qc_cec_&lt;species&gt;, distinguishable from every other reason a period is dropped. Unticked reproduces the published method. Default: off.</p>");
    minFluxSigmaBox->setToolTip(minFluxSigmaTooltip);
    minFluxSigmaSpin->setToolTip(minFluxSigmaTooltip);

    setOptionTooltip(maxGapFillLabel, maxGapFillSpin,
                     tr("<b>Maximum small-gap fill:</b> Runs of up to this many consecutive samples are repaired by linear interpolation, after the signal-strength screen has deleted what the analyser condemned â the order Zahn et al. (2022) state. It is what keeps a period with brief dropouts above the minimum valid data gate. Set it to 0 to never reconstruct a condemned sample, at the cost of losing more periods. Default: 4 samples."));

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
    qcLayout->addWidget(signalStrengthWarningLabel, 1, 2, Qt::AlignLeft);
    qcLayout->addWidget(maxStationarityLabel, 2, 0, Qt::AlignRight);
    qcLayout->addWidget(maxStationaritySpin, 2, 1);
    qcLayout->addWidget(ratioStationarityBox, 2, 2, Qt::AlignLeft);
    qcLayout->addWidget(maxGapFillLabel, 3, 0, Qt::AlignRight);
    qcLayout->addWidget(maxGapFillSpin, 3, 1);
    qcLayout->addWidget(minFluxSigmaBox, 4, 0, Qt::AlignRight);
    qcLayout->addWidget(minFluxSigmaSpin, 4, 1);
    qcLayout->addWidget(minFluxSigmaWarningLabel, 4, 2, Qt::AlignLeft);
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
    connect(ratioStationarityBox, &QCheckBox::toggled, this, [=](bool on)
            { ecProject_->setGeneralCecStationarityMode(on ? 1 : 0); });
    connect(maxStationaritySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            ecProject_, &EcProject::setGeneralCecMaxStationarity);
    //> Off is stored as 0 rather than as a flag, so the spin box keeps its
    //> value while unticked and the project says plainly that no test runs.
    connect(minFluxSigmaBox, &QCheckBox::toggled, this, [=](bool on)
            {
                minFluxSigmaSpin->setEnabled(on);
                ecProject_->setGeneralCecMinFluxSigma(
                            on ? minFluxSigmaSpin->value() : 0.0);
                updateMinFluxSigmaWarning();
            });
    //> The side effect hangs off clicked, not toggled, so that opening a
    //> project with the test on does not silently switch anything on for the
    //> user. Same rule as the CEC checkbox and WPL.
    connect(minFluxSigmaBox, &QCheckBox::clicked, this, [=]()
            {
                if (minFluxSigmaBox->isChecked()) { enableRandomUncertainty(); }
                updateMinFluxSigmaWarning();
            });
    connect(minFluxSigmaSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [=](double d)
            {
                if (minFluxSigmaBox->isChecked())
                { ecProject_->setGeneralCecMinFluxSigma(d); }
            });
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
    const QSignalBlocker ratioStationarityBlocker(ratioStationarityBox);
    const QSignalBlocker minFluxSigmaBoxBlocker(minFluxSigmaBox);
    const QSignalBlocker minFluxSigmaSpinBlocker(minFluxSigmaSpin);
    const QSignalBlocker maxGapFillBlocker(maxGapFillSpin);
    const QSignalBlocker singularBandBlocker(singularBandSpin);

    hSpin->setValue(ecProject_->generalCecH());
    minO1O2Spin->setValue(ecProject_->generalCecMinO1O2());
    minOctantSpin->setValue(ecProject_->generalCecMinOctant());
    minValidSpin->setValue(ecProject_->generalCecMinValid());
    signalStrengthSpin->setValue(ecProject_->generalCecSignalStrength());
    maxStationaritySpin->setValue(ecProject_->generalCecMaxStationarity());
    ratioStationarityBox->setChecked(ecProject_->generalCecStationarityMode() == 1);
    //> 0 is off. The box remembers the offered value rather than 0, so
    //> unticking and reticking does not silently arrive at a different test.
    const auto minFluxSigma = ecProject_->generalCecMinFluxSigma();
    minFluxSigmaBox->setChecked(minFluxSigma > 0.0);
    minFluxSigmaSpin->setValue(minFluxSigma > 0.0 ? minFluxSigma
                                                  : DefaultMinFluxSigma);
    minFluxSigmaSpin->setEnabled(minFluxSigma > 0.0);
    updateMinFluxSigmaWarning();
    maxGapFillSpin->setValue(ecProject_->generalCecMaxGapFill());
    singularBandSpin->setValue(ecProject_->generalCecSingularBand());

    //> The pairings name gas records, so a record added or removed elsewhere
    //> can invalidate one. Re-read rather than trust what the table holds.
    pairModel->reload();
    updatePairButtons();
    updateSignalStrengthAvailability();
}

/// The screen needs a diagnostic, and most analysers do not report one.
///
/// The engine looks for a column named exactly AGC or RSSI **on the gas's own
/// analyser** and, finding none, skips that channel's screen and partitions
/// anyway. That is the right behaviour and it is silent, so the cutoff sat
/// here promising a screen that in many projects never ran - on the site this
/// was written for, no analyser declared one and the setting did nothing at
/// all.
///
/// Asked of the metadata, which is where the column is declared, and matched
/// the way the engine matches it: same two names, same instrument, same case.
void CecSettingsDialog::updateSignalStrengthAvailability()
{
    QSet<QString> withDiagnostic;
    if (dlProject_ && dlProject_->variables())
    {
        for (const auto &var : *dlProject_->variables())
        {
            if (var.variable() != VariableDesc::getVARIABLE_VAR_STRING_35()
                && var.variable() != VariableDesc::getVARIABLE_VAR_STRING_36())
            {
                continue;
            }
            //> The canonical id, never var.instrument(): that holds the
            //> translated label the table shows - "Irga 1: LI-7200" - while
            //> the gas records store "li7200_1". Comparing the two matches
            //> nothing, and the failure is silent: every analyser would look
            //> unscreened.
            withDiagnostic.insert(
                dlProject_->canonicalInstrumentId(var.instrument()));
        }
    }

    QStringList unscreened;
    for (const auto &gas : ecProject_->gasColumns())
    {
        if (gas.rawColumn <= 0) { continue; }
        if (withDiagnostic.contains(gas.instrumentId)) { continue; }
        if (unscreened.contains(gas.instrumentId)) { continue; }
        unscreened.append(gas.instrumentId);
    }

    const bool anyAtAll = !withDiagnostic.isEmpty();
    signalStrengthLabel->setEnabled(anyAtAll);
    signalStrengthSpin->setEnabled(anyAtAll);
    signalStrengthWarningLabel->setVisible(!anyAtAll || !unscreened.isEmpty());

    if (!anyAtAll)
    {
        signalStrengthWarningLabel->setToolTip(
            tr("<b>No signal strength is declared, so this screen cannot run.</b>"
               "<p>It needs a column named <b>AGC</b> or <b>RSSI</b> on the same "
               "analyser as the gas. Add one in the Raw File Description if your "
               "instrument reports it.</p>"
               "<p>The partition itself is unaffected and still runs \xe2\x80\x93 only "
               "this one screening step is skipped.</p>"));
    }
    else
    {
        //> Enabled, not greyed: the screen genuinely runs for the analysers
        //> that do declare one, and greying the box would say otherwise.
        signalStrengthWarningLabel->setToolTip(
            tr("<b>Some channels cannot be screened.</b>"
               "<p>These analysers declare no AGC or RSSI column, so their gases "
               "are read unscreened while the others are screened normally: "
               "<b>%1</b>.</p>"
               "<p>The partition itself is unaffected.</p>").arg(unscreened.join(
                   QStringLiteral(", "))));
    }
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

/// Switch random uncertainty estimation on, as the CEC checkbox switches on
/// WPL, and for the same kind of reason: the test has nothing to compare a
/// flux against until the random error is being estimated.
///
/// Finkelstein and Sims because that is the estimator whose value makes
/// |F| / RE the significance of r - it takes the number of independent samples
/// from the period's own integral timescale rather than assuming one. Mann and
/// Lenschow is left alone if it is already selected; it also produces a random
/// error, and the choice is the user's.
///
/// Billesbach (2011) is left alone too, but it is worth knowing what that
/// does to the threshold. It estimates a noise floor rather than a sampling
/// error and is systematically the smaller of the two, so cec_min_flux_sigma
/// then asks whether the flux is resolvable at all rather than whether it is
/// significant, and the same number admits more periods than it would under
/// Finkelstein and Sims. That is a defensible test and not the one the
/// setting was written for; select it deliberately or not at all.
void CecSettingsDialog::enableRandomUncertainty()
{
    if (ecProject_->randErrorMethod() != 0) { return; }
    ecProject_->setRandomErrorMethod(1);
}

void CecSettingsDialog::updateMinFluxSigmaWarning()
{
    minFluxSigmaWarningLabel->setVisible(minFluxSigmaBox->isChecked()
                                         && ecProject_->randErrorMethod() == 0);
}

void CecSettingsDialog::restoreDefaults()
{
    hSpin->setValue(DefaultH);
    minO1O2Spin->setValue(DefaultMinO1O2);
    minOctantSpin->setValue(DefaultMinOctant);
    minValidSpin->setValue(DefaultMinValid);
    signalStrengthSpin->setValue(DefaultSignalStrength);
    maxStationaritySpin->setValue(DefaultMaxStationarity);
    ratioStationarityBox->setChecked(false);
    //> Unticking writes the 0 and greys the spin, so the value it is left
    //> showing is only what the box will offer next time.
    minFluxSigmaBox->setChecked(false);
    minFluxSigmaSpin->setValue(DefaultMinFluxSigma);
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
