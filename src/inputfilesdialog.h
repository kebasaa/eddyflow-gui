/***************************************************************************
  inputfilesdialog.h
  ------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#ifndef INPUTFILESDIALOG_H
#define INPUTFILESDIALOG_H

#include <QDialog>

class FileBrowseWidget;
class QDialogButtonBox;
class QLabel;

/// The assessment files an Advanced run reads, checked before it starts.
///
/// A project names its planar fit, time lag and spectral assessment files by
/// path, and nothing noticed when one of them was moved or renamed: the
/// engine started, could not open it, and stopped. This window opens when a
/// file the run will use is gone. It shows one line per file, with the same
/// Browse and Remote drive buttons as the settings pages, the line pre-filled
/// with a similar file when one was found nearby, and lines that do not apply
/// greyed out. A line whose path is not the one the project saved is marked,
/// so a suggestion is never taken without being seen. Nothing is written to
/// the project here; the caller reads the result.
class InputFilesDialog : public QDialog
{
    Q_OBJECT

public:
    enum Kind { PlanarFit = 0, Timelag = 1, SpectralAssessment = 2, KindCount = 3 };

    struct Line
    {
        bool applies = false;     //!< the run will read this file
        QString saved;            //!< the path the project has
        QString suggestion;       //!< a similar file, when the saved one is gone
    };

    InputFilesDialog(QWidget* parent, const Line (&lines)[KindCount]);

    /// The path the line holds now: the saved one, a suggestion, or a pick.
    QString path(Kind kind) const;
    /// Whether that path is not the one the project saved.
    bool changed(Kind kind) const;

private:
    void updateLine(Kind kind);
    void updateContinue();

    Line lines_[KindCount];
    FileBrowseWidget* browse_[KindCount];
    QLabel* status_[KindCount];
    QDialogButtonBox* buttons_;
};

#endif  // INPUTFILESDIALOG_H
