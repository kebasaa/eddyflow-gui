/***************************************************************************
  logging.cpp
  -----------
  Copyright © 2026-    , ETH Zurich, Jonathan Muller

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
#include "logging.h"

#include <iostream>

#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QIODevice>
#include <QtCore/QStandardPaths>
#include <QtCore/QString>
#include <QtCore/QTextStream>

#include "defs.h"
#include "fileutils.h"

void Logging::messageHandler(QtMsgType type,
                             const QMessageLogContext& context,
                             const QString& message)
{
    // Build severity tag
    QString tag;
    switch (type) {
        case QtDebugMsg:    tag = QStringLiteral("[Deb]"); break;
        case QtInfoMsg:     tag = QStringLiteral("[Inf]"); break;
        case QtWarningMsg:  tag = QStringLiteral("[War]"); break;
        case QtCriticalMsg: tag = QStringLiteral("[Cri]"); break;
        case QtFatalMsg:    tag = QStringLiteral("[Fat]"); break;
    }

    // Compose log entry: timestamp, tag, source location, message
    const QString timestamp = QDateTime::currentDateTime()
                                  .toString(QStringLiteral("yyyyMMdd h:mm:ss.zzz t"));
    const QString srcFile = QString::fromLatin1(context.file ? context.file : "");
    const int srcLine = context.line;

    QString entry;
    if (!srcFile.isEmpty()) {
        entry = QStringLiteral("[%1] %2 %3:%4 - %5")
                    .arg(timestamp, tag, srcFile)
                    .arg(srcLine)
                    .arg(message);
    } else {
        entry = QStringLiteral("[%1] %2 %3").arg(timestamp, tag, message);
    }

    if (entry.isEmpty())
        goto done;

    {
        // Ensure log directory exists
        const QString configDir =
            QStandardPaths::standardLocations(QStandardPaths::AppConfigLocation).first();
        FileUtils::createDir(configDir);

        const QString logPath = configDir
                                + QLatin1Char('/')
                                + Defs::APP_NAME_LCASE
                                + QLatin1Char('.')
                                + Defs::LOG_FILE_EXT;

        // Rotate log when it grows too large
        QFile logFile(logPath);
        if (logFile.size() > Defs::LOG_FILE_MAX_SIZE)
            logFile.remove();

        if (logFile.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&logFile);
            out << entry << '\n';
        }

        // Mirror to stderr
        std::cerr << entry.toStdString() << '\n';
    }

done:
    if (type == QtFatalMsg)
        abort();
}
