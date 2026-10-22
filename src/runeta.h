/***************************************************************************
  runeta.h
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

#ifndef RUNETA_H
#define RUNETA_H

#include <QHash>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

// The time left until a run is done: every pass still to come, RP's and
// FCC's, not only the one under way.
//
// The engine says, for the interface only, which passes a run will walk
// ("  Run plan: to pf rp (17520 periods)") and, every few seconds, how far the
// pass under way has got across all its processes
// ("  Progress [rp]: 1234 of 17520 periods done."). The pass under way is
// timed from those lines, over the last ten minutes of them: a split pass
// speeds up once its workers have started, and a mean since the pass began
// would not follow that. Passes still to come are predicted per period, from
// what the same pass of this project took last time, or else from how it
// compares with the pass being timed now.
//
// Times are the caller's milliseconds, from any clock that only moves on;
// nothing here reads a clock, so a test can drive it.
class RunEta
{
public:
    // `storeFile` empty: the application's settings, where the times of
    // earlier runs are kept between sessions.
    explicit RunEta(const QString& storeFile = QString());

    // A new run of the project at `projectFile`, split across processes or
    // not - the time a pass takes per period differs between the two.
    void start(const QString& projectFile, bool parallel, qint64 nowMs);
    // The engine's run plan: which passes, of RP or of FCC, lie ahead.
    void plan(const QStringList& stages, qint64 periods);
    // FCC will follow RP: its passes are part of the run from the start.
    void planFcc();
    // A progress line. `tag` is the engine's: to, pf, rp, pd, sa or fx.
    void progress(const QString& tag, qint64 done, qint64 total, qint64 nowMs);
    // How many processes share a split pass, from the engine's
    // "Splitting ... across N worker processes."
    void setProcesses(int n);
    // The parent began one of its own periods of the production pass: what a
    // period costs, for the flux round that follows the PWB detection round.
    void ownPeriod(qint64 nowMs);
    // An engine finished: its last pass is done, and what each pass took is
    // kept for the next run of this project.
    void engineFinished(qint64 nowMs);
    void pause(qint64 nowMs);
    void resume(qint64 nowMs);

    // Milliseconds left, or -1 while there is nothing to tell it from.
    qint64 remainingMs(qint64 nowMs) const;
    // What the run is doing now, for the label.
    QString currentStageName() const;
    bool running() const { return started_; }

    // "h:mm:ss" with as many hours as it takes - a QTime wraps at a day.
    static QString formatDuration(qint64 ms);
    static QString stageName(const QString& stage);

private:
    struct Stage
    {
        QString key;
        qint64 size = 0;          // periods (records, files) it walks
        bool begun = false;
        bool done = false;
        qint64 startMs = 0;       // on the run's own clock, pauses left out
        qint64 endMs = 0;
        QString round;            // pd, then rp, in a split pass with PWB
        double fraction = 0.0;
        qint64 lastMs = 0;        // when the last progress line came
        QList<QPair<qint64, double>> window;
    };

    qint64 clock(qint64 nowMs) const;
    int indexOf(const QString& key) const;
    int currentIndex() const;
    void insertPlanned(const QString& key, qint64 size);
    double measuredMsPerFraction(const Stage& s, qint64 minSpanMs = -1) const;
    double msPerUnit(const QString& key) const;
    double referenceMsPerUnit(QString* refKey) const;
    double storedMsPerUnit(const QString& key) const;
    double ratio(const QString& key) const;
    void store();
    QString projectGroup() const;
    QString ratioGroup() const;

    QString storeFile_;
    QString project_;
    bool parallel_ = false;
    bool started_ = false;
    qint64 periods_ = 0;
    int processes_ = 1;
    qint64 pausedTotal_ = 0;
    qint64 pausedAt_ = -1;
    QList<Stage> stages_;
    // The parent's own periods of the production pass, for the PWB round.
    qint64 ownCount_ = 0;
    qint64 ownFirstMs_ = -1;
    qint64 ownLastMs_ = -1;
    // Earlier runs' times, read once per run rather than every second.
    mutable bool storedLoaded_ = false;
    mutable QHash<QString, double> stored_;
    mutable QHash<QString, double> ratios_;
};

#endif // RUNETA_H
