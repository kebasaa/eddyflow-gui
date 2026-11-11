/***************************************************************************
  eddyproexport.h
  -------------------
  Write an EddyFlow project out as an EddyPro one
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

#ifndef EDDYPROEXPORT_H
#define EDDYPROEXPORT_H

#include <QCoreApplication>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

class DlProject;
class EcProject;

/// One setting the export could not carry across as it was.
///
/// \a from and \a to are what the user would recognise - the label on the
/// option, the instrument's name - rather than the ini spelling, and \a note
/// says why, when the reason is not obvious from the two values.
struct EddyProExportChange
{
    QString area;
    QString setting;
    QString from;
    QString to;
    QString note;
};

/// Which raw column fills each of EddyPro's four gas slots: CO2, H2O, CH4 and
/// the fourth gas. 0 leaves the slot empty; -1 keeps the project's own
/// first-match assignment for it.
struct EddyProGasSlots
{
    int column[4] = { -1, -1, -1, -1 };
};

///
/// \class EddyProExport
/// \brief Writes the open project as an EddyPro 7 project and metadata.
///
/// The mirror of the EddyPro import, and lossy where that is lossless: this
/// program measures more gases, knows more instruments and offers more
/// methods than EddyPro does. Nothing is refused. Whatever EddyPro cannot
/// express is replaced by what EddyPro would have used - its generic sonic or
/// analyser for an instrument it does not know, its default for a method it
/// does not have - and every replacement is recorded, so the user is told
/// exactly what the EddyPro run will do differently.
///
/// Two files, both beside the one the user chose:
///
///   - ``<stem>.eddypro``          the project, built by
///                                 EcProject::exportEddyProProject
///   - ``<stem>_EP.metadata``      the metadata, named apart from the
///                                 project's own so neither overwrites the
///                                 other
///
/// A project that reads the metadata embedded in its GHG files gets no
/// metadata file: LI-COR wrote those in EddyPro's vocabulary already.
///
/// The open documents are not touched. Both are written from copies, so an
/// unsaved project exports as it is shown and stays unsaved.
///
class EddyProExport
{
    Q_DECLARE_TR_FUNCTIONS(EddyProExport)

public:
    /// Writes \a eddyproFile and its metadata from \a ec and \a dl, putting
    /// the columns \a choice names in EddyPro's four gas slots. Returns false
    /// and sets \a error when nothing usable could be written.
    bool exportTo(EcProject* ec, const DlProject& dl,
                  const EddyProGasSlots& choice,
                  const QString& eddyproFile, QString* error = nullptr);

    /// The project's own assignment: for each slot, the column of the first
    /// gas record of that species, or 0. What the slot dialog proposes.
    static EddyProGasSlots defaultSlots(const EcProject& ec);

    /// The gas columns \a dl describes that may fill \a slot (0 CO2, 1 H2O,
    /// 2 CH4, 3 the fourth gas), as column number and a label naming the
    /// species and the instrument.
    static QVector<QPair<int, QString>> slotCandidates(const DlProject& dl, int slot);

    /// Where the metadata for \a eddyproFile goes: ``<stem>_EP.metadata``.
    static QString metadataFileFor(const QString& eddyproFile);
    /// Where the report for \a eddyproFile goes.
    static QString reportFileFor(const QString& eddyproFile);

    /// The metadata written, or empty when the project reads it from its
    /// GHG files.
    QString metadataFile() const { return metadataFile_; }

    /// Every setting that changed, in the order found.
    QVector<EddyProExportChange> changes() const { return changes_; }

    /// What EddyPro may refuse or misread that the export could not fix
    /// without inventing a value.
    QStringList warnings() const { return warnings_; }

    /// Notes on the export as a whole, such as where the metadata came from.
    QStringList notes() const { return notes_; }

    QString reportHtml() const;
    QString reportText() const;

private:
    void change(const QString& area, const QString& setting,
                const QString& from, const QString& to,
                const QString& note = QString());

    bool convertProjectOptions(const QString& nativeCopy,
                               const QString& eddyproFile,
                               const EcProject& ec, const DlProject& dl);
    bool convertMetadata(const DlProject& dl, QString* error);

    QString metadataFile_;
    QVector<EddyProExportChange> changes_;
    QStringList warnings_;
    QStringList notes_;

    //> Which raw column fills each of EddyPro's four gas slots, read back from
    //> the finished project. The metadata pass needs the fourth: its column is
    //> what EddyPro calls n2o, whatever was measured there.
    int slotColumn_[4] = { 0, 0, 0, 0 };
};

#endif  // EDDYPROEXPORT_H
