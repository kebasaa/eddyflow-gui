/***************************************************************************
  ini_file.h
  ----------
  Copyright © 2026-    , ETH Zurich, Jonathan Muller

  This file is part of EddyFlow®.

  EddyFlow (TM) is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version. You should have received a copy
  of the GNU General Public License along with EddyFlow (R). If not,
  see <http://www.gnu.org/licenses/>.
***************************************************************************/

#ifndef INI_FILE_H
#define INI_FILE_H

#include <QSettings>
#include <QVariant>

/// \file src/ini_file.h
/// \brief An ini file where a key that says nothing is a key that says
/// nothing, whatever its type.
///
/// QSettings::value(key, fallback) hands back the fallback only when the key
/// is ABSENT. A line written as `error_value=` is a key that EXISTS holding an
/// empty string, so the fallback is skipped, and QVariant::toReal() answers
/// 0.0 - it has no "did it convert?" flag, unlike QString::toDouble(). A
/// metadata file written elsewhere with an empty error_value therefore
/// declared 0 to be its missing-data fill, and every genuine zero in that
/// column was read as missing. An empty acquisition_frequency read as 0 Hz,
/// an empty path length as 0 m, an empty u* threshold as 0.
///
/// For a number, "the key is not there" and "the key is there and says
/// nothing" mean the same thing, and so does text that is not a number at
/// all. This reads them that way. Strings keep QSettings' meaning, where an
/// empty value is a legitimate answer and not a missing one.

/// One value read from an IniFile, which remembers the fallback so the
/// conversion can fall back to it.
class IniScalar
{
public:
    IniScalar(QVariant value, QVariant fallback);

    int toInt() const;
    double toDouble() const;
    qreal toReal() const;
    bool toBool() const;

    //> The QVariant forms that report the conversion. \a ok answers whether
    //> the number handed back is usable, so a caller that range-checks the
    //> result and falls back itself - several do - keeps working unchanged.
    //> It is false only when the value cannot be read AND no fallback was
    //> given to read instead.
    int toInt(bool* ok) const;
    double toDouble(bool* ok) const;

    //> Strings keep QSettings' reading: an empty value is an empty string,
    //> not a missing one. Only an absent key gives the fallback.
    QString toString() const;
    QStringList toStringList() const;

    QVariant toVariant() const;
    operator QVariant() const { return toVariant(); }

private:
    //> The value as trimmed text, empty when the key is absent, present but
    //> blank, or whitespace - the three cases a number cannot be read from.
    QString text() const;

    QVariant value_;
    QVariant fallback_;
};

/// A QSettings whose value() answers an IniScalar.
///
/// value() hides the base class's rather than overriding it - QSettings has no
/// virtuals - which is why the readers declare their file as an IniFile. Call
/// it through a QSettings reference and the old reading is back.
class IniFile : public QSettings
{
public:
    using QSettings::QSettings;

    IniScalar value(const QString& key,
                    const QVariant& fallback = QVariant()) const;
};

#endif // INI_FILE_H
