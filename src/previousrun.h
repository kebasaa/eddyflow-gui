/***************************************************************************
  previousrun.h
  -------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#ifndef PREVIOUSRUN_H
#define PREVIOUSRUN_H

#include <QCoreApplication>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <atomic>

/// What an earlier run left in the output folder, and what can be done with it.
///
/// An Advanced run is two passes: raw processing (eddyflow_rp), then flux
/// computation and correction (eddyflow_fcc). The second reads only [Project]
/// and the [FluxCorrection*] sections, and takes everything raw processing
/// computed from one intermediate FLUXNET file (ex_file) and the cospectra
/// folders. When those are still there and no setting raw processing reads has
/// changed since, the correction pass can be re-run on its own.
///
/// The record of a run is the processing_<tag>.eddyflow file in the output
/// folder: FCC copies the project there when it finishes, and its ex_file
/// names the raw-processing file it read. When a chained FCC fails or is
/// stopped the GUI writes one itself under raw processing's own tag, so FCC
/// can be retried; a record whose tag is its ex_file's tag is that kind.
///
/// Only reads. Deleting is the caller's, once someone has said so.
class PreviousRun
{
    Q_DECLARE_TR_FUNCTIONS(PreviousRun)

public:
    /// Files and their total size.
    struct Files
    {
        QStringList paths;
        qint64 bytes = 0;

        bool isEmpty() const { return paths.isEmpty(); }
    };

    struct Scan
    {
        /// Whether flux correction alone can be run, reusing raw processing.
        bool correctionOnly = false;
        /// Why it cannot, one readable sentence each; empty when it can.
        QStringList reasons;

        /// The record the correction-only run builds on.
        QString recordFile;
        /// The raw-processing FLUXNET file FCC would read.
        QString exFile;
        /// When that raw processing ran, from its file name.
        QDateTime rawProcessingTime;

        /// What a correction-only re-run makes obsolete: earlier correction
        /// passes over the same raw processing. The record above is one of
        /// them when it is FCC's own.
        Files correctionOutputs;
        /// What a full re-run makes obsolete: everything earlier runs of this
        /// project wrote, under the engine's own names.
        Files allOutputs;
    };

    /// How far a scan has got, read by another thread while it runs. Listing
    /// an output folder on a network share takes seconds: the cospectra
    /// folders hold one file per averaging period each.
    struct Progress
    {
        /// The folder being listed: -1 for the output folder itself, else an
        /// index into engineSubFolders().
        std::atomic<int> folder { -1 };
        /// Files looked at so far.
        std::atomic<qint64> files { 0 };
        /// Set by the reader to stop the scan; what it returns is then
        /// incomplete and must not be used.
        std::atomic<bool> cancelled { false };
    };

    /// Looks at the output folder named in `currentProject`, a saved copy of
    /// the project as it stands now. `fccNeeded` is whether the current
    /// settings call for the flux-correction pass at all
    /// (EcProject::isEngineStep2Needed). Safe to run off the GUI thread.
    static Scan scan(const QString& currentProject, bool fccNeeded,
                     Progress* progress = nullptr);

    /// The tag a file name carries, "2026-11-10T090707_adv", or empty.
    static QString runTag(const QString& fileName);

    /// The record FCC would have written for the raw-processing pass that
    /// wrote `exFile`: processing_<tag>.eddyflow beside it. Empty when the
    /// name carries no tag.
    static QString rawProcessingRecordFor(const QString& exFile);

    /// Whether `header`, the first line of an ex file, is one this engine's
    /// FCC reads. The same markers as JudgeExHeader in eddyflow-fcc_main.f90.
    static bool isCurrentExHeader(const QByteArray& header);

    /// The engine's sub-folders of the output folder.
    static const QStringList& engineSubFolders();
};

#endif // PREVIOUSRUN_H
