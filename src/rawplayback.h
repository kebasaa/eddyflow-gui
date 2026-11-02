/***************************************************************************
  rawplayback.h
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

#ifndef RAWPLAYBACK_H
#define RAWPLAYBACK_H

#include <QDateTime>
#include <QList>
#include <QString>
#include <QVector>

/// Reads one raw file into memory, for playing it back as it was measured.
/// Nothing here touches a project: the caller hands in what it needs, so
/// every reader can run off the GUI thread.
namespace RawPlayback {

enum class Role { U, V, W, Scalar };

/// One variable of the file; a gap (error value, empty or unreadable cell)
/// is NaN, so every channel has a value slot for every row of the file.
struct Channel
{
    QString key;      // the engine's name: u, v, w, ts, co2, ...
    QString label;    // for the legend: CO₂, Ts, ...
    Role role = Role::Scalar;
    QVector<float> values;
    int decimals = 0;   // as logged, so a glyph can follow the last digit
};

struct Recording
{
    QString label;            // the file name
    double frequency = 0.0;   // Hz
    QDateTime start;          // invalid when the name does not say
    int rows = 0;
    QList<Channel> channels;
    bool isSample = false;    // the bundled one, not the project's
    QString error;            // why nothing could be read; empty on success

    bool isValid() const { return error.isEmpty() && rows > 0 && frequency > 0.0; }
    const Channel* channel(Role role) const;
    QList<const Channel*> scalars() const;
};

/// How an ASCII raw file is laid out, taken from the open project
struct AsciiFormat
{
    QString separator;          // comma, semicolon, space, tab or the character
    int headerRows = 0;
    QString dataLabel;          // rows not starting with it are skipped; empty: none
    double frequency = 0.0;
    QString filenamePrototype;  // for the start time
    QStringList columns;        // the engine's variable name per column, in order
};

/// A LI-COR .ghg archive: the data member and its own metadata, read
/// straight from the zip. Nothing is extracted to disk.
Recording readGhg(const QString& path);

/// An ASCII raw file described by format
Recording readAscii(const QString& path, const AsciiFormat& format);

/// The sample bundled for when the project's own data cannot be read
Recording readSample();

/// The channel role and legend label of an engine variable name, or false
/// when it is not one worth playing (diagnostics, pressures, ignored ...)
bool describeVariable(const QString& key, Role* role, QString* label);

} // namespace RawPlayback

#endif // RAWPLAYBACK_H
