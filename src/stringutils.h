/***************************************************************************
  stringutils.h
  -------------------
  Copyright © 2013-2018, LI-COR Biosciences, Antonio Forgione
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

#ifndef STRINGUTILS_H
#define STRINGUTILS_H

class QString;
#include <QStringList>

namespace StringUtils
{
    const QString fromBool2YesNoString(bool b);

    const QString insertIndex(const QString& srcStr, int pos, const QString& str);

    const char* qString2Char(const QString& str);

    int daysToFromString(const QString& d_1, const QString& d_2);

    QString fromUnixTimeToISOString(double msec);

    bool isISODateTimeString(const QString &s);

    bool stringBelongsToList(const QString &str, const QStringList &list);

    int getVersionFromString(const QString& versionStr);

    bool isNewVersion(const QString &remoteVersion, const QString &localVersion);

    const QStringList subStringList(const QStringList &list, int begin, int end);

} // StringUtils

#endif // STRINGUTILS_H

