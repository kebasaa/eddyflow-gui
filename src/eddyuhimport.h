/***************************************************************************
  eddyuhimport.h
  -------------------
  Convert an EddyUH project into an EddyFlow one
  -------------------
  Copyright © 2026,      ETH Zurich, Jonathan Muller

  This file is part of EddyFlow®.

  EddyFlow (TM) is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version. You should have received a copy
  of the GNU General Public License along with EddyFlow (R). If not,
  see <http://www.gnu.org/licenses/>.

  EddyFlow® contains additional Open Source Components. The licenses
  and/or notices these Components can be found in the file LIBRARIES.txt.

  EddyFlow® is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.
****************************************************************************/

#ifndef EDDYUHIMPORT_H
#define EDDYUHIMPORT_H

#include <QString>
#include <QStringList>

class DlProject;
class EcProject;

///
/// \class EddyUhImport
/// \brief Reads an EddyUH project and fills an EddyFlow project and metadata.
///
/// An EddyUH project is four files sharing a stem:
///
///   - ``preproc_<stem>``            the settings proper, a MAT file
///   - ``lag_<stem>.<n>cl``          per-gas lag windows, a MAT file
///   - ``planar_fit_<stem>.<n>cl``   planar fit coefficients, a MAT file
///   - ``resptime_<stem>.<n>cl``     per-gas response times, plain text
///
/// The user picks the first; the rest are found beside it by stem and each is
/// optional. Nothing is written: the two documents are filled in memory and
/// the caller saves them, so that opening a project to look at it does not
/// rewrite anything.
///
/// **A large part of an EddyUH run cannot be imported at all.** Its
/// flux-time options - spectral correction method, cospectral model, peak
/// frequency parameterisation, lag method, data screening, footprint - are
/// collected interactively at every run and saved only to a text log beside
/// the fluxes. They are not in the project files. Those take EddyFlow's
/// defaults and are listed in \a notes so the user knows to set them.
///
class EddyUhImport
{
public:
    /// True when \a path looks like the ``preproc_`` file of an EddyUH
    /// project. Cheap: the name and the MAT header, not a full parse.
    static bool looksLikeEddyUhProject(const QString& path);

    /// The sibling files this project has, for the confirmation the caller
    /// shows before it does anything.
    static QStringList siblingsOf(const QString& preprocPath);

    /// Fills \a ec and \a dl from the project whose preproc file is \a path.
    /// Returns false and sets \a error when the project cannot be read at
    /// all; a project that is merely incomplete succeeds and says what was
    /// missing in \a notes.
    bool convert(const QString& path, EcProject* ec, DlProject* dl,
                 QString* error = nullptr);

    /// What could not be recovered, and what was assumed. Always worth
    /// showing: it is never empty, because the flux-time options are never
    /// in the files.
    QStringList notes() const { return notes_; }

    /// The stem the four files share, for naming the converted project.
    QString stem() const { return stem_; }

private:
    void note(const QString& text) { notes_.append(text); }

    QStringList notes_;
    QString stem_;
};

#endif  // EDDYUHIMPORT_H
