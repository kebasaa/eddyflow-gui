/***************************************************************************
  inputfilecheck.h
  ----------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#ifndef INPUTFILECHECK_H
#define INPUTFILECHECK_H

#include <QString>
#include <QStringList>

/// Finding a file the engine wrote, after the one a project names has moved.
///
/// The engine names its assessment files
/// <project_id><padding>_<YYYY-MM-DD>T<HHMMSS>_<adv|exp>.<ext>, so a later
/// run of the same project writes the same name with a different timestamp.
/// A project that names a file which is gone is therefore most often one run
/// behind: the file it wants is beside it, with another timestamp. No widgets
/// here, so the rules can be pinned on their own.
namespace InputFileCheck
{
    /// The padding each kind of file carries in its name.
    inline const char* PlanarFitPadding = "_planar_fit";
    inline const char* TimelagOptPadding = "_timelag_opt";
    inline const char* PwbTimelagPadding = "_pwb_timelag";
    inline const char* SpectralAssessmentPadding = "_spectral_assessment";

    /// The name filter for files like `missingName`: its timestamp made a
    /// wildcard. A name without one (a file named by hand) falls back to any
    /// file of the kind: `*<padding>_*.<ext>`.
    QString similarPattern(const QString& missingName, const QString& padding);

    /// Where to look, in order, existing folders only and each once: the
    /// missing file's own folder, the project's output folder, the folders
    /// beside the missing file's folder (a renamed run folder, or a step-2
    /// project pointing at step 1), and the project file's folder.
    QStringList searchDirs(const QString& missingPath,
                           const QString& outPath,
                           const QString& projectFile);

    /// The most recently modified file in `dirs` matching `pattern` whose
    /// name does not contain `exclude` (left empty to exclude nothing).
    /// Empty when there is none.
    QString findSimilar(const QString& pattern,
                        const QStringList& dirs,
                        const QString& exclude);

    /// Whether a path names something the run can read: a remote link, which
    /// the engine fetches itself, or a file that exists.
    bool usable(const QString& path);

    /// Whether two paths name the same file, whatever their separators.
    bool samePath(const QString& a, const QString& b);
}

#endif  // INPUTFILECHECK_H
