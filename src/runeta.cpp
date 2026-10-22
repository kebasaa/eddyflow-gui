/***************************************************************************
  runeta.cpp
  -------------------
  Copyright © 2026, ETH Zurich, Jonathan Muller

  This file is part of EddyFlow®.

  EddyFlow (TM) is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version. You should have received a copy
  of the GNU General Public License along with EddyFlow (R). If not,
  see <http://www.gnu.org/licenses/>.

  EddyFlow® is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.
****************************************************************************/

#include "runeta.h"

#include <QCryptographicHash>
#include <QDir>
#include <QSettings>

#include <memory>

namespace {

// The passes in the order a run walks them: RP's, then FCC's.
const QStringList& order()
{
    static const QStringList keys = { QStringLiteral("to"), QStringLiteral("pf"),
                                      QStringLiteral("rp"), QStringLiteral("sa"),
                                      QStringLiteral("fx") };
    return keys;
}

bool isFcc(const QString& key)
{
    return key == QLatin1String("sa") || key == QLatin1String("fx");
}

// What a period of each pass costs against one of raw data processing, until
// runs on this machine have said better. From three days of CH-LAE 10 Hz data
// with PWB time lags and a planar fit, run serially and in four processes:
// time lags 0.41 / 0.44, planar fit 0.19 / 0.22, spectral assessment
// 0.009 / 0.011, flux computation 0.056 / 0.064.
double builtinRatio(const QString& key)
{
    if (key == QLatin1String("to")) return 0.42;
    if (key == QLatin1String("pf")) return 0.20;
    if (key == QLatin1String("sa")) return 0.01;
    if (key == QLatin1String("fx")) return 0.06;
    return 1.0;
}

// The window the pass under way is timed over, and the least of it that is
// worth a rate: the first seconds of a split pass are its workers starting.
constexpr qint64 WindowMs = 10 * 60 * 1000;
constexpr qint64 MinSpanMs = 30 * 1000;

std::unique_ptr<QSettings> openStore(const QString& file)
{
    if (file.isEmpty())
        return std::make_unique<QSettings>();
    return std::make_unique<QSettings>(file, QSettings::IniFormat);
}

} // namespace

RunEta::RunEta(const QString& storeFile) :
    storeFile_(storeFile)
{
}

void RunEta::start(const QString& projectFile, bool parallel, qint64 nowMs)
{
    Q_UNUSED(nowMs)
    project_ = QDir::cleanPath(projectFile).toLower();
    parallel_ = parallel;
    started_ = true;
    periods_ = 0;
    processes_ = 1;
    pausedTotal_ = 0;
    pausedAt_ = -1;
    stages_.clear();
    ownCount_ = 0;
    ownFirstMs_ = -1;
    ownLastMs_ = -1;
    storedLoaded_ = false;
    stored_.clear();
    ratios_.clear();
}

void RunEta::plan(const QStringList& stages, qint64 periods)
{
    if (!started_)
        return;
    if (periods > 0)
        periods_ = periods;
    bool fcc = false;
    for (const auto& key : stages)
        fcc = fcc || isFcc(key);

    // The engine knows best which of its passes run: what was guessed for
    // this engine and has not begun goes.
    for (int i = stages_.size() - 1; i >= 0; --i)
    {
        if (!stages_.at(i).begun && isFcc(stages_.at(i).key) == fcc)
            stages_.removeAt(i);
    }
    for (const auto& key : stages)
    {
        if (order().contains(key))
            insertPlanned(key, 0);
    }
}

void RunEta::planFcc()
{
    if (!started_)
        return;
    insertPlanned(QStringLiteral("sa"), 0);
    insertPlanned(QStringLiteral("fx"), 0);
}

void RunEta::progress(const QString& tag, qint64 done, qint64 total, qint64 nowMs)
{
    const QString key = (tag == QLatin1String("pd")) ? QStringLiteral("rp") : tag;
    if (!started_ || !order().contains(key) || total <= 0)
        return;
    const qint64 t = clock(nowMs);

    int idx = indexOf(key);
    if (idx < 0)
    {
        insertPlanned(key, total);
        idx = indexOf(key);
    }
    if (!stages_.at(idx).begun)
    {
        // The pass before it is over; and any pass planned before it that
        // never began will not now - a time-lag file that was reused.
        const int cur = currentIndex();
        if (cur >= 0)
        {
            stages_[cur].done = true;
            stages_[cur].endMs = t;
        }
        const int pos = order().indexOf(key);
        for (int i = stages_.size() - 1; i >= 0; --i)
        {
            if (!stages_.at(i).begun && order().indexOf(stages_.at(i).key) < pos)
                stages_.removeAt(i);
        }
        idx = indexOf(key);
        stages_[idx].begun = true;
        stages_[idx].startMs = t;
        stages_[idx].round = tag;
        if (key == QLatin1String("rp"))
        {
            ownCount_ = 0;
            ownFirstMs_ = -1;
            ownLastMs_ = -1;
        }
    }

    Stage& s = stages_[idx];
    if (s.done)
        return;
    if (s.round != tag)
    {
        s.round = tag;
        s.window.clear();
    }
    s.size = total;
    if (periods_ <= 0)
        periods_ = total;
    s.fraction = qBound(0.0, static_cast<double>(done) / total, 1.0);
    s.lastMs = t;
    s.window.append(qMakePair(t, s.fraction));
    // Keep the newest sample at least ten minutes old, so the window spans
    // the whole ten minutes once the pass has run that long.
    while (s.window.size() > 2 && t - s.window.at(1).first >= WindowMs)
        s.window.removeFirst();
}

void RunEta::setProcesses(int n)
{
    processes_ = qMax(1, n);
    // The parent's own periods are timed from the split on: the one before
    // it also wrote the workers' context and started them.
    const int cur = currentIndex();
    if (cur >= 0 && stages_.at(cur).key == QLatin1String("rp"))
    {
        ownCount_ = 0;
        ownFirstMs_ = -1;
        ownLastMs_ = -1;
    }
}

void RunEta::ownPeriod(qint64 nowMs)
{
    if (!started_)
        return;
    const qint64 t = clock(nowMs);
    if (ownFirstMs_ < 0)
        ownFirstMs_ = t;
    ownLastMs_ = t;
    ++ownCount_;
}

void RunEta::engineFinished(qint64 nowMs)
{
    if (!started_)
        return;
    const qint64 t = clock(nowMs);
    const int cur = currentIndex();
    bool fcc = false;
    if (cur >= 0)
    {
        fcc = isFcc(stages_.at(cur).key);
        stages_[cur].done = true;
        stages_[cur].endMs = t;
        stages_[cur].fraction = 1.0;
    }
    // Nothing more of this engine will run.
    for (int i = stages_.size() - 1; i >= 0; --i)
    {
        if (!stages_.at(i).begun && isFcc(stages_.at(i).key) == fcc)
            stages_.removeAt(i);
    }
    store();
}

void RunEta::pause(qint64 nowMs)
{
    if (pausedAt_ < 0)
        pausedAt_ = nowMs;
}

void RunEta::resume(qint64 nowMs)
{
    if (pausedAt_ >= 0)
    {
        pausedTotal_ += nowMs - pausedAt_;
        pausedAt_ = -1;
    }
}

qint64 RunEta::remainingMs(qint64 nowMs) const
{
    if (!started_ || stages_.isEmpty())
        return -1;
    const qint64 t = clock(nowMs);
    double total = 0.0;

    const int cur = currentIndex();
    if (cur >= 0)
    {
        const Stage& s = stages_.at(cur);
        const bool pd = s.round == QLatin1String("pd");
        double rem = -1.0;
        if (s.fraction >= 1.0)
        {
            rem = 0.0;
        }
        else
        {
            const double r = pd ? -1.0 : measuredMsPerFraction(s);
            if (r > 0.0)
            {
                // The last ten minutes, and - when this project ran the pass
                // before - what a period took over all of it then: a record
                // that speeds up or slows down along the way misleads the
                // first early on, the second as the pass goes on and the
                // range differs from last time's.
                double left = (1.0 - s.fraction) * r;
                const double stored = storedMsPerUnit(s.key);
                if (stored > 0.0)
                    left = s.fraction * left
                           + (1.0 - s.fraction) * (1.0 - s.fraction) * s.size * stored;
                rem = qMax(0.0, left - (t - s.lastMs));
            }
            else
            {
                // Not timed yet. A pass of this project timed before, less
                // what it has run; or the parent's own periods, shared out
                // over the processes - all a production pass has to go on
                // while its pieces are only read for PWB evidence.
                const double stored = storedMsPerUnit(s.key);
                double own = -1.0;
                if (s.key == QLatin1String("rp") && ownCount_ >= 2)
                    own = static_cast<double>(ownLastMs_ - ownFirstMs_)
                          / (ownCount_ - 1) / processes_;
                if (stored > 0.0)
                {
                    rem = qMax(0.0, stored * s.size - (t - s.startMs));
                }
                else if (own > 0.0)
                {
                    if (pd)
                    {
                        // What is left of the detection round, then the
                        // flux round over every period not the parent's.
                        const double rd = measuredMsPerFraction(s);
                        const double detect = rd > 0.0
                            ? qMax(0.0, (1.0 - s.fraction) * rd - (t - s.lastMs)) : 0.0;
                        rem = detect + qMax<double>(0.0, s.size - ownCount_) * own;
                    }
                    else
                    {
                        rem = qMax(0.0, (1.0 - s.fraction) * s.size * own - (t - s.lastMs));
                    }
                }
                else
                {
                    const double m = msPerUnit(s.key);
                    if (m <= 0.0)
                        return -1;
                    rem = qMax(0.0, m * s.size - (t - s.startMs));
                }
                // The model has run out and the pass has not: it is slower
                // than modelled - its workers took long to start, say. What
                // it has done so far, however briefly timed, says better
                // than a label standing still.
                if (rem <= 0.0 && !pd)
                {
                    const double early = measuredMsPerFraction(s, 0);
                    if (early > 0.0)
                        rem = qMax(0.0, (1.0 - s.fraction) * early - (t - s.lastMs));
                }
            }
        }
        total += rem;
    }

    for (const auto& s : stages_)
    {
        if (s.begun)
            continue;
        const qint64 size = s.size > 0 ? s.size : periods_;
        if (size <= 0)
            return -1;
        const double m = msPerUnit(s.key);
        if (m <= 0.0)
            return -1;
        total += m * size;
    }
    return static_cast<qint64>(total + 0.5);
}

QString RunEta::currentStageName() const
{
    const int cur = currentIndex();
    return cur >= 0 ? stageName(stages_.at(cur).key) : QString();
}

QString RunEta::formatDuration(qint64 ms)
{
    if (ms < 0)
        return QStringLiteral("--:--:--");
    const qint64 s = (ms + 500) / 1000;
    return QStringLiteral("%1:%2:%3")
        .arg(s / 3600, 2, 10, QLatin1Char('0'))
        .arg((s / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(s % 60, 2, 10, QLatin1Char('0'));
}

QString RunEta::stageName(const QString& stage)
{
    if (stage == QLatin1String("to")) return QStringLiteral("time-lag optimization");
    if (stage == QLatin1String("pf")) return QStringLiteral("planar fit");
    if (stage == QLatin1String("rp") || stage == QLatin1String("pd"))
        return QStringLiteral("raw data processing");
    if (stage == QLatin1String("sa")) return QStringLiteral("spectral assessment");
    if (stage == QLatin1String("fx")) return QStringLiteral("flux computation");
    return QString();
}

qint64 RunEta::clock(qint64 nowMs) const
{
    if (pausedAt_ >= 0)
        return pausedAt_ - pausedTotal_;
    return nowMs - pausedTotal_;
}

int RunEta::indexOf(const QString& key) const
{
    for (int i = 0; i < stages_.size(); ++i)
    {
        if (stages_.at(i).key == key)
            return i;
    }
    return -1;
}

int RunEta::currentIndex() const
{
    for (int i = 0; i < stages_.size(); ++i)
    {
        if (stages_.at(i).begun && !stages_.at(i).done)
            return i;
    }
    return -1;
}

void RunEta::insertPlanned(const QString& key, qint64 size)
{
    if (indexOf(key) >= 0)
        return;
    Stage s;
    s.key = key;
    s.size = size;
    const int pos = order().indexOf(key);
    int at = stages_.size();
    for (int i = 0; i < stages_.size(); ++i)
    {
        if (order().indexOf(stages_.at(i).key) > pos)
        {
            at = i;
            break;
        }
    }
    stages_.insert(at, s);
}

// Milliseconds per whole pass, from the window; -1 below `minSpanMs` of it,
// or below MinSpanMs when that is not given.
double RunEta::measuredMsPerFraction(const Stage& s, qint64 minSpanMs) const
{
    if (s.window.size() < 2)
        return -1.0;
    const auto& first = s.window.first();
    const auto& last = s.window.last();
    const double df = last.second - first.second;
    const qint64 dt = last.first - first.first;
    if (df <= 0.0 || dt <= 0 || dt < (minSpanMs < 0 ? MinSpanMs : minSpanMs))
        return -1.0;
    return dt / df;
}

// What a period of `key` takes: this project's own last time, or else the
// pass being timed now, scaled by how the two compare.
double RunEta::msPerUnit(const QString& key) const
{
    const double stored = storedMsPerUnit(key);
    if (stored > 0.0)
        return stored;
    QString refKey;
    const double ref = referenceMsPerUnit(&refKey);
    if (ref <= 0.0)
        return -1.0;
    return ref / ratio(refKey) * ratio(key);
}

// The pass this run has the best time of, per period: the one under way if it
// has been timed, else the last one done.
double RunEta::referenceMsPerUnit(QString* refKey) const
{
    const int cur = currentIndex();
    if (cur >= 0)
    {
        const Stage& s = stages_.at(cur);
        if (s.round == QLatin1String("pd"))
        {
            if (ownCount_ >= 2)
            {
                *refKey = s.key;
                return static_cast<double>(ownLastMs_ - ownFirstMs_)
                       / (ownCount_ - 1) / processes_;
            }
        }
        else
        {
            const double r = measuredMsPerFraction(s);
            if (r > 0.0 && s.size > 0)
            {
                *refKey = s.key;
                return r / s.size;
            }
        }
    }
    for (int i = stages_.size() - 1; i >= 0; --i)
    {
        const Stage& s = stages_.at(i);
        if (s.done && s.size > 0 && s.endMs > s.startMs)
        {
            *refKey = s.key;
            return static_cast<double>(s.endMs - s.startMs) / s.size;
        }
    }
    return -1.0;
}

double RunEta::storedMsPerUnit(const QString& key) const
{
    if (project_.isEmpty())
        return -1.0;
    if (!storedLoaded_)
    {
        const auto settings = openStore(storeFile_);
        for (const auto& k : order())
        {
            stored_[k] = settings->value(projectGroup() + QLatin1Char('/') + k, -1.0).toDouble();
            ratios_[k] = settings->value(ratioGroup() + QLatin1Char('/') + k, -1.0).toDouble();
        }
        storedLoaded_ = true;
    }
    return stored_.value(key, -1.0);
}

double RunEta::ratio(const QString& key) const
{
    storedMsPerUnit(key);
    const double learnt = ratios_.value(key, -1.0);
    return learnt > 0.0 ? learnt : builtinRatio(key);
}

// What each pass that ran took per period, for the next run of this project;
// and, when raw data processing ran, how each compares with it, for runs of
// other projects.
void RunEta::store()
{
    if (project_.isEmpty())
        return;
    const auto settings = openStore(storeFile_);
    double rp = -1.0;
    for (const auto& s : stages_)
    {
        if (s.key == QLatin1String("rp") && s.done && s.size > 0 && s.endMs > s.startMs)
            rp = static_cast<double>(s.endMs - s.startMs) / s.size;
    }
    for (const auto& s : stages_)
    {
        if (!s.done || s.size <= 0 || s.endMs <= s.startMs)
            continue;
        const double ms = static_cast<double>(s.endMs - s.startMs) / s.size;
        settings->setValue(projectGroup() + QLatin1Char('/') + s.key, ms);
        stored_[s.key] = ms;
        if (rp > 0.0 && s.key != QLatin1String("rp"))
        {
            settings->setValue(ratioGroup() + QLatin1Char('/') + s.key, ms / rp);
            ratios_[s.key] = ms / rp;
        }
    }
    settings->sync();
}

QString RunEta::projectGroup() const
{
    const QByteArray hash = QCryptographicHash::hash(project_.toUtf8(),
                                                     QCryptographicHash::Sha1).toHex().left(16);
    return QStringLiteral("runEta/") + QString::fromLatin1(hash)
           + (parallel_ ? QStringLiteral("/parallel") : QStringLiteral("/serial"));
}

QString RunEta::ratioGroup() const
{
    return parallel_ ? QStringLiteral("runEta/ratios/parallel")
                     : QStringLiteral("runEta/ratios/serial");
}
