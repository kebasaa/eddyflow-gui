/***************************************************************************
  qt_helpers.cpp
  -------------------
  Copyright © 2014, LI-COR Biosciences, Antonio Forgione
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

#include "qt_helpers.h"

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QStringList>

// NOTE: fix for Qt5
// from http://blog.inventic.eu/2014/08/qt5-application-crashed-with-error-0xc0000005/
// see https://bugreports.qt-project.org/browse/QTBUG-40738
void QtHelper::prependApplicationPathToLibraryPaths(const QString& app_name)
{
    // get application path
    QString executablePath;
#if defined(Q_OS_WIN)
    executablePath = app_name.mid(0, app_name.lastIndexOf(QStringLiteral("\\")));
#elif defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    executablePath = app_name.mid(0, app_name.lastIndexOf(QLatin1String("/")));
#endif

    // get library paths
    QString installPathPlugins = QLibraryInfo::path(QLibraryInfo::PluginsPath);

    // set paths prepending the application path
    QCoreApplication::setLibraryPaths(QStringList() << executablePath << installPathPlugins);
}
