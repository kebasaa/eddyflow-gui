/***************************************************************************
  ini_file.cpp
  ------------
  Copyright © 2026-    , ETH Zurich, Jonathan Muller

  This file is part of EddyFlow®.

  EddyFlow (TM) is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version. You should have received a copy
  of the GNU General Public License along with EddyFlow (R). If not,
  see <http://www.gnu.org/licenses/>.
***************************************************************************/

#include "ini_file.h"

IniScalar::IniScalar(QVariant value, QVariant fallback) :
    value_(std::move(value)),
    fallback_(std::move(fallback))
{
}

QString IniScalar::text() const
{
    if (!value_.isValid()) { return QString(); }
    return value_.toString().trimmed();
}

int IniScalar::toInt() const
{
    const auto number = text();
    if (number.isEmpty()) { return fallback_.toInt(); }

    bool ok = false;
    const int parsed = number.toInt(&ok);
    return ok ? parsed : fallback_.toInt();
}

double IniScalar::toDouble() const
{
    const auto number = text();
    if (number.isEmpty()) { return fallback_.toDouble(); }

    bool ok = false;
    const double parsed = number.toDouble(&ok);
    return ok ? parsed : fallback_.toDouble();
}

qreal IniScalar::toReal() const
{
    return toDouble();
}

int IniScalar::toInt(bool* ok) const
{
    const auto number = text();
    if (!number.isEmpty())
    {
        bool parsed = false;
        const int value = number.toInt(&parsed);
        if (parsed)
        {
            if (ok) { *ok = true; }
            return value;
        }
    }

    bool fallbackOk = false;
    const int value = fallback_.toInt(&fallbackOk);
    if (ok) { *ok = fallbackOk; }
    return value;
}

double IniScalar::toDouble(bool* ok) const
{
    const auto number = text();
    if (!number.isEmpty())
    {
        bool parsed = false;
        const double value = number.toDouble(&parsed);
        if (parsed)
        {
            if (ok) { *ok = true; }
            return value;
        }
    }

    bool fallbackOk = false;
    const double value = fallback_.toDouble(&fallbackOk);
    if (ok) { *ok = fallbackOk; }
    return value;
}

bool IniScalar::toBool() const
{
    //> A boolean key is written as 0 or 1 here, so an empty one is as
    //> unreadable as an empty number and takes the same fallback.
    if (text().isEmpty()) { return fallback_.toBool(); }
    return value_.toBool();
}

QString IniScalar::toString() const
{
    return value_.isValid() ? value_.toString() : fallback_.toString();
}

QStringList IniScalar::toStringList() const
{
    return value_.isValid() ? value_.toStringList() : fallback_.toStringList();
}

QVariant IniScalar::toVariant() const
{
    return value_.isValid() ? value_ : fallback_;
}

IniScalar IniFile::value(const QString& key, const QVariant& fallback) const
{
    return IniScalar(QSettings::value(key), fallback);
}
