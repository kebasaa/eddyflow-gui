/***************************************************************************
  cumcovsettingsdialog.cpp
  ------------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#include "cumcovsettingsdialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include "ecproject.h"

namespace
{
//> Every control names where the method comes from.
QString credit()
{
    return QObject::tr("<br><br><i>Source:</i> innFLUX, Striednig et al. (2020), "
        "Atmos. Meas. Tech. 13, 1447-1465 (innFLUX_step2.m), who credit Park et al. "
        "(2013) with summing covariance functions.");
}

QString driftCredit()
{
    return QObject::tr("<br><br><i>Source:</i> the driftfix tool of innFLUX v1.2.0 "
        "(2023, University of Innsbruck, unpublished; a routine by M. Graus adapted "
        "by M. Striednig), not described in Striednig et al. (2020). Robust LOWESS "
        "after Cleveland (1979).");
}

QDoubleSpinBox *makeSpin(double lo, double hi, int decimals, double step)
{
    auto spin = new QDoubleSpinBox;
    spin->setRange(lo, hi);
    spin->setDecimals(decimals);
    spin->setSingleStep(step);
    return spin;
}
} // namespace

CumCovSettingsDialog::CumCovSettingsDialog(QWidget *parent, EcProject *ecProject)
    : QDialog(parent), ecProject_(ecProject)
{
    setWindowTitle(tr("Cumulated Covariance Time Lag Settings"));
    setWindowModality(Qt::WindowModal);

    auto intro = new QLabel(tr("<b>Time lag from cumulated cross-covariances</b> "
        "(after innFLUX, Striednig et al. 2020). For each gas, the absolute "
        "cross-covariance functions of every averaging period that passes the "
        "filters below are summed, per date segment, and the lag at the peak of the "
        "sum is used for every period in that segment. Meant for weak fluxes - most "
        "trace gases a PTR-MS measures - whose single-period peaks are noise. "
        "The search window of each gas is its time lag optimization window."));
    intro->setWordWrap(true);

    filterSpin = new QSpinBox;
    filterSpin->setRange(0, 999);
    filterSpin->setSpecialValueText(tr("Off"));
    filterSpin->setSuffix(tr(" [rows]", "Number of samples"));
    filterSpin->setToolTip(tr("<b>Peak smoothing:</b> length of the normalised "
        "Hamming window the summed function is smoothed with, run forwards and "
        "backwards, before its peak is taken. innFLUX: 15.") + credit());
    ustarSpin = makeSpin(0.0, 5.0, 2, 0.05);
    ustarSpin->setSuffix(tr(" [m/s]"));
    ustarSpin->setToolTip(tr("<b>Minimum u*:</b> periods with a friction velocity "
        "at or below this are not summed. innFLUX: 0.3 m/s.") + credit());
    complSonicSpin = makeSpin(0.0, 1.0, 2, 0.05);
    complSonicSpin->setToolTip(tr("<b>Minimum sonic completeness:</b> the share of "
        "the period's expected records with valid wind, at or below which the "
        "period is not summed. innFLUX: 0.95.") + credit());
    complGasSpin = makeSpin(0.0, 1.0, 2, 0.05);
    complGasSpin->setToolTip(tr("<b>Minimum gas completeness:</b> the share of the "
        "gas's expected samples, at its own rate, at or below which the period is "
        "not summed. innFLUX: 0.5.") + credit());
    sstSpin = makeSpin(0.0, 1000000.0, 2, 0.05);
    sstSpin->setToolTip(tr("<b>Maximum steady-state deviation:</b> the relative "
        "deviation of the mean of five sub-period covariances from the whole "
        "period's (Foken and Wichura 1996), at or above which the period is not "
        "summed. innFLUX: 0.3.") + credit());
    segmentsEdit = new QLineEdit;
    segmentsEdit->setPlaceholderText(tr("e.g. 2023-05-01T00:00, 2023-07-15T12:00"));
    segmentsEdit->setToolTip(tr("<b>Segment boundaries:</b> dates (yyyy-mm-dd, "
        "optionally THH:MM) at which a new lag starts, separated by commas - for "
        "instance where the inlet was changed. Empty: one lag over the whole run. "
        "A period belongs to the segment its start time falls in.") + credit());

    auto form = new QFormLayout;
    form->addRow(tr("Peak smoothing :"), filterSpin);
    form->addRow(tr("Minimum u* :"), ustarSpin);
    form->addRow(tr("Minimum sonic completeness :"), complSonicSpin);
    form->addRow(tr("Minimum gas completeness :"), complGasSpin);
    form->addRow(tr("Maximum steady-state deviation :"), sstSpin);
    form->addRow(tr("Segment boundaries :"), segmentsEdit);

    driftCheck = new QCheckBox(tr("Correct for clock drift"));
    driftCheck->setToolTip(tr("<b>Correct for clock drift:</b> when a logger's "
        "clock drifts against the analyser's, the time lag drifts, and jumps where "
        "the clock is reset. The per-period lags of the reference gases below are "
        "smoothed with a robust LOWESS, split where they jump, and the result is "
        "applied to every gas on the same analyser before and after summing. Off by "
        "default.") + driftCredit());
    spanSpin = makeSpin(0.1, 365.0, 1, 0.5);
    spanSpin->setSuffix(tr(" [days]"));
    spanSpin->setToolTip(tr("<b>Smoothing span:</b> the window of the robust LOWESS "
        "between jumps. innFLUX: 5 days.") + driftCredit());
    jumpSpin = makeSpin(-100.0, 0.0, 2, 0.1);
    jumpSpin->setSuffix(tr(" [s/day]"));
    jumpSpin->setToolTip(tr("<b>Jump threshold:</b> a jump is where the one-day "
        "smoothed lag rises faster than this. innFLUX: -0.8 s per day (the lag one "
        "day later exceeds today's by 0.8 s).") + driftCredit());
    maxDriftSpin = makeSpin(0.0, 600.0, 1, 1.0);
    maxDriftSpin->setSuffix(tr(" [s]"));
    maxDriftSpin->setToolTip(tr("<b>Largest drift:</b> how far from its median the "
        "drift may reach. The cross-covariances are kept this much wider, so a "
        "larger value costs memory and disk.") + driftCredit());
    refList = new QListWidget;
    refList->setToolTip(tr("<b>Reference gases:</b> gases whose lag is "
        "well-determined in most periods. Their lags are pooled per analyser to "
        "estimate the drift.") + driftCredit());

    auto driftForm = new QFormLayout;
    driftForm->addRow(driftCheck);
    driftForm->addRow(tr("Smoothing span :"), spanSpin);
    driftForm->addRow(tr("Jump threshold :"), jumpSpin);
    driftForm->addRow(tr("Largest drift :"), maxDriftSpin);
    driftForm->addRow(tr("Reference gases :"), refList);
    auto driftBox = new QGroupBox(tr("Clock drift (innFLUX driftfix)"));
    driftBox->setLayout(driftForm);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Close
                                        | QDialogButtonBox::RestoreDefaults);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked,
            this, &CumCovSettingsDialog::restoreDefaults);

    auto layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addLayout(form);
    layout->addWidget(driftBox);
    layout->addWidget(buttons);

    //> Stored on every edit the user makes; refresh() sets the controls with
    //> refreshing_ raised, so loading a project writes nothing back.
    for (auto spin : {ustarSpin, complSonicSpin, complGasSpin, sstSpin, spanSpin,
                      jumpSpin, maxDriftSpin})
    {
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &CumCovSettingsDialog::store);
    }
    connect(filterSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &CumCovSettingsDialog::store);
    connect(segmentsEdit, &QLineEdit::editingFinished, this, &CumCovSettingsDialog::store);
    connect(driftCheck, &QCheckBox::clicked, this, &CumCovSettingsDialog::store);
    connect(refList, &QListWidget::itemChanged, this, &CumCovSettingsDialog::storeReferences);
}

void CumCovSettingsDialog::refresh()
{
    refreshing_ = true;
    const auto &c = ecProject_->screenCumCov();
    filterSpin->setValue(c.filter_len);
    ustarSpin->setValue(c.min_ustar);
    complSonicSpin->setValue(c.min_compl_sonic);
    complGasSpin->setValue(c.min_compl_gas);
    sstSpin->setValue(c.max_sst);
    segmentsEdit->setText(c.segments);
    driftCheck->setChecked(c.drift_meth == 1);
    spanSpin->setValue(c.drift_span_d);
    jumpSpin->setValue(c.drift_jump_thr);
    maxDriftSpin->setValue(c.drift_max_s);

    {
        const QSignalBlocker blocker(refList);
        refList->clear();
        const auto &gases = ecProject_->gasColumns();
        for (int i = 0; i < gases.size(); ++i)
        {
            auto item = new QListWidgetItem(tr("Gas %1: %2 (%3)").arg(i + 1)
                .arg(gases.at(i).slug, gases.at(i).instrumentId));
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(gases.at(i).proc.tlagDriftRef == 1 ? Qt::Checked
                                                                   : Qt::Unchecked);
            refList->addItem(item);
        }
    }
    refreshing_ = false;
    updateEnabled();
}

void CumCovSettingsDialog::store()
{
    if (refreshing_) { return; }
    CumCovSettings c = ecProject_->screenCumCov();
    c.filter_len = filterSpin->value() < 2 ? 0 : filterSpin->value();
    c.min_ustar = ustarSpin->value();
    c.min_compl_sonic = complSonicSpin->value();
    c.min_compl_gas = complGasSpin->value();
    c.max_sst = sstSpin->value();
    c.segments = segmentsEdit->text().trimmed();
    c.drift_meth = driftCheck->isChecked() ? 1 : 0;
    c.drift_span_d = spanSpin->value();
    c.drift_jump_thr = jumpSpin->value();
    c.drift_max_s = maxDriftSpin->value();
    ecProject_->setScreenCumCov(c);
    updateEnabled();
}

void CumCovSettingsDialog::storeReferences()
{
    if (refreshing_) { return; }
    auto gases = ecProject_->gasColumns();
    for (int i = 0; i < gases.size() && i < refList->count(); ++i)
    {
        const bool on = refList->item(i)->checkState() == Qt::Checked;
        //> Unticked returns the gas to "not stated" rather than writing 0.
        gases[i].proc.tlagDriftRef = on ? 1 : -1;
    }
    ecProject_->setGasColumns(gases);
}

void CumCovSettingsDialog::restoreDefaults()
{
    const CumCovSettings d;
    CumCovSettings c = ecProject_->screenCumCov();
    //> The output settings in the same struct are not this dialog's, so
    //> they are left as they are.
    c.filter_len = d.filter_len;
    c.min_ustar = d.min_ustar;
    c.min_compl_sonic = d.min_compl_sonic;
    c.min_compl_gas = d.min_compl_gas;
    c.max_sst = d.max_sst;
    c.segments = d.segments;
    c.drift_meth = d.drift_meth;
    c.drift_span_d = d.drift_span_d;
    c.drift_jump_thr = d.drift_jump_thr;
    c.drift_max_s = d.drift_max_s;
    ecProject_->setScreenCumCov(c);
    refresh();
}

void CumCovSettingsDialog::updateEnabled()
{
    const bool drift = driftCheck->isChecked();
    spanSpin->setEnabled(drift);
    jumpSpin->setEnabled(drift);
    maxDriftSpin->setEnabled(drift);
    refList->setEnabled(drift);
}
