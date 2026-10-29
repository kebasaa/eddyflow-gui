/***************************************************************************
  rawplayback.cpp
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

#include "rawplayback.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <limits>

#include "fileutils.h"
#include "quazip.h"
#include "quazipfile.h"

namespace RawPlayback {

namespace {

const float GAP = std::numeric_limits<float>::quiet_NaN();

// the scalars worth playing, with their legend labels; anything else that
// is not wind is a diagnostic, a pressure or a housekeeping column
const QHash<QString, QString>& scalarLabels()
{
    static const QHash<QString, QString> labels {
        { QStringLiteral("ts"), QStringLiteral("Ts") },
        { QStringLiteral("sos"), QStringLiteral("SoS") },
        { QStringLiteral("co2"), QStringLiteral("CO₂") },
        { QStringLiteral("h2o"), QStringLiteral("H₂O") },
        { QStringLiteral("ch4"), QStringLiteral("CH₄") },
        { QStringLiteral("n2o"), QStringLiteral("N₂O") },
        { QStringLiteral("co"), QStringLiteral("CO") },
        { QStringLiteral("so2"), QStringLiteral("SO₂") },
        { QStringLiteral("o3"), QStringLiteral("O₃") },
        { QStringLiteral("nh3"), QStringLiteral("NH₃") },
        { QStringLiteral("no"), QStringLiteral("NO") },
        { QStringLiteral("no2"), QStringLiteral("NO₂") },
        { QStringLiteral("cos"), QStringLiteral("COS") },
        { QStringLiteral("n2"), QStringLiteral("N₂") },
        { QStringLiteral("o2"), QStringLiteral("O₂") },
        { QStringLiteral("ar"), QStringLiteral("Ar") }
    };
    return labels;
}

QString separatorString(const QString& separator)
{
    const QString s = separator.trimmed().toLower();
    if (s == QLatin1String("tab"))
        return QStringLiteral("\t");
    if (s == QLatin1String("comma"))
        return QStringLiteral(",");
    if (s == QLatin1String("semicolon"))
        return QStringLiteral(";");
    if (s == QLatin1String("space") || separator.isEmpty())
        return QStringLiteral(" ");
    return separator;
}

// the start time a LI-COR name states: 2021-08-21T013000_AIU-0737.ghg
QDateTime ghgStart(const QString& fileName)
{
    static const QRegularExpression re(
        QStringLiteral("(\\d{4})-(\\d{2})-(\\d{2})T(\\d{2})(\\d{2})(\\d{2})"));
    const auto m = re.match(fileName);
    if (!m.hasMatch())
        return {};
    return QDateTime(QDate(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt()),
                     QTime(m.captured(4).toInt(), m.captured(5).toInt(), m.captured(6).toInt()));
}

float parseValue(QStringView cell)
{
    bool ok = false;
    const float x = cell.trimmed().toFloat(&ok);
    // the engine's error code, and anything a logger writes for nothing
    if (!ok || !std::isfinite(x) || x == -9999.0f || std::fabs(x) > 1e30f)
        return GAP;
    return x;
}

// the decimals a cell was logged with: 453.125 has 3, 1.2e-3 has 4
int decimalsOf(QStringView cell)
{
    cell = cell.trimmed();
    int exponent = 0;
    const auto e = cell.indexOf(QLatin1Char('e'), 0, Qt::CaseInsensitive);
    if (e >= 0)
    {
        exponent = cell.mid(e + 1).toInt();
        cell = cell.left(e);
    }
    const auto dot = cell.indexOf(QLatin1Char('.'));
    const int digits = dot < 0 ? 0 : int(cell.size() - dot - 1);
    return std::clamp(digits - exponent, 0, 9);
}

float variance(const QVector<float>& values)
{
    double sum = 0.0;
    double sum2 = 0.0;
    int n = 0;
    for (float x : values)
    {
        if (std::isnan(x))
            continue;
        sum += x;
        sum2 += double(x) * x;
        ++n;
    }
    if (n < 2)
        return -1.0f;
    const double mean = sum / n;
    return float(sum2 / n - mean * mean);
}

// Splits the text into rows and fills one channel per playable column. A
// variable named twice (a LI-7500 logs the sonic both on its aux inputs and
// directly) keeps the column that actually moves.
Recording parseTable(const QByteArray& text, const QString& separator, int headerRows,
                     const QString& dataLabel, const QStringList& columns)
{
    Recording rec;

    struct Wanted { int column; Channel channel; };
    QList<Wanted> wanted;
    for (int c = 0; c < columns.size(); ++c)
    {
        Role role;
        QString label;
        if (!describeVariable(columns.at(c), &role, &label))
            continue;
        Channel ch;
        ch.key = columns.at(c);
        ch.label = label;
        ch.role = role;
        wanted.append({ c, ch });
    }
    if (wanted.isEmpty())
    {
        rec.error = QStringLiteral("no wind, temperature or gas column");
        return rec;
    }

    const QString sep = separatorString(separator);
    const bool space = (sep == QLatin1String(" "));
    const QString text16 = QString::fromLatin1(text);
    const auto lines = QStringView(text16).split(QLatin1Char('\n'));

    int row = 0;
    for (int l = 0; l < lines.size(); ++l)
    {
        if (l < headerRows)
            continue;
        QStringView line = lines.at(l);
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        if (line.trimmed().isEmpty())
            continue;

        QList<QStringView> cells = space ? line.split(QLatin1Char(' '), Qt::SkipEmptyParts)
                                         : line.split(sep);
        if (!dataLabel.isEmpty())
        {
            if (cells.isEmpty() || cells.first().trimmed() != dataLabel)
                continue;
            cells.removeFirst();
        }

        for (auto& w : wanted)
        {
            if (w.column >= cells.size())
            {
                w.channel.values.append(GAP);
                continue;
            }
            const float x = parseValue(cells.at(w.column));
            w.channel.values.append(x);
            // the resolution the logger wrote, from the first rows
            if (row < 500 && !std::isnan(x))
                w.channel.decimals = std::max(w.channel.decimals, decimalsOf(cells.at(w.column)));
        }
        ++row;
    }

    QHash<QString, int> best;
    for (int i = 0; i < wanted.size(); ++i)
    {
        const QString& key = wanted.at(i).channel.key;
        if (!best.contains(key)
            || variance(wanted.at(i).channel.values) > variance(wanted.at(best.value(key)).channel.values))
            best.insert(key, i);
    }
    for (int i = 0; i < wanted.size(); ++i)
    {
        if (best.value(wanted.at(i).channel.key) == i && variance(wanted.at(i).channel.values) >= 0.0f)
            rec.channels.append(wanted.at(i).channel);
    }

    rec.rows = row;
    if (rec.rows == 0 || rec.channels.isEmpty())
        rec.error = QStringLiteral("no data rows");
    return rec;
}

QHash<QString, QString> parseKeyValues(const QByteArray& text)
{
    QHash<QString, QString> values;
    const auto lines = QString::fromLatin1(text).split(QLatin1Char('\n'));
    for (QString line : lines)
    {
        line = line.trimmed();
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0 || line.startsWith(QLatin1Char('[')) || line.startsWith(QLatin1Char(';')))
            continue;
        values.insert(line.left(eq).trimmed(), line.mid(eq + 1).trimmed());
    }
    return values;
}

} // namespace

const Channel* Recording::channel(Role role) const
{
    for (const auto& ch : channels)
        if (ch.role == role)
            return &ch;
    return nullptr;
}

QList<const Channel*> Recording::scalars() const
{
    QList<const Channel*> list;
    for (const auto& ch : channels)
        if (ch.role == Role::Scalar)
            list.append(&ch);
    return list;
}

bool describeVariable(const QString& key, Role* role, QString* label)
{
    if (key == QLatin1String("u") || key == QLatin1String("v") || key == QLatin1String("w"))
    {
        *role = key == QLatin1String("u") ? Role::U : key == QLatin1String("v") ? Role::V : Role::W;
        *label = key;
        return true;
    }
    // a gas the project names itself, passed through as gas:<label>
    if (key.startsWith(QLatin1String("gas:")))
    {
        *role = Role::Scalar;
        *label = key.mid(4);
        return !label->isEmpty();
    }
    const auto it = scalarLabels().constFind(key.toLower());
    if (it == scalarLabels().constEnd())
        return false;
    *role = Role::Scalar;
    *label = it.value();
    return true;
}

Recording readGhg(const QString& path)
{
    Recording rec;
    rec.label = QFileInfo(path).fileName();

    QuaZip zip(path);
    if (!zip.open(QuaZip::mdUnzip))
    {
        rec.error = QStringLiteral("cannot open the archive");
        return rec;
    }

    QString dataName;
    QString metaName;
    for (const auto& name : zip.getFileNameList())
    {
        // the biomet table is a separate, slow file in the same archive
        if (name.contains(QLatin1String("-biomet")) || name.contains(QLatin1Char('/')))
            continue;
        if (name.endsWith(QLatin1String(".data")))
            dataName = name;
        else if (name.endsWith(QLatin1String(".metadata")))
            metaName = name;
    }
    if (dataName.isEmpty() || metaName.isEmpty())
    {
        rec.error = QStringLiteral("no data or metadata in the archive");
        return rec;
    }

    auto readMember = [&zip](const QString& name, QByteArray* out)
    {
        if (!zip.setCurrentFile(name))
            return false;
        QuaZipFile file(&zip);
        if (!file.open(QIODevice::ReadOnly))
            return false;
        *out = file.readAll();
        file.close();
        return true;
    };

    QByteArray meta;
    QByteArray data;
    if (!readMember(metaName, &meta) || !readMember(dataName, &data))
    {
        rec.error = QStringLiteral("cannot read the archive");
        return rec;
    }
    zip.close();

    const auto md = parseKeyValues(meta);
    QStringList columns;
    for (int c = 1; md.contains(QStringLiteral("col_%1_variable").arg(c)); ++c)
        columns.append(md.value(QStringLiteral("col_%1_variable").arg(c)).toLower());

    const Recording table = parseTable(data,
                                       md.value(QStringLiteral("separator"), QStringLiteral("tab")),
                                       md.value(QStringLiteral("header_rows")).toInt(),
                                       md.value(QStringLiteral("data_label")),
                                       columns);
    rec.channels = table.channels;
    rec.rows = table.rows;
    rec.error = table.error;
    rec.frequency = md.value(QStringLiteral("acquisition_frequency")).toDouble();
    rec.start = ghgStart(rec.label);
    if (rec.error.isEmpty() && rec.frequency <= 0.0)
        rec.error = QStringLiteral("no acquisition frequency");
    return rec;
}

Recording readAscii(const QString& path, const AsciiFormat& format)
{
    Recording rec;
    rec.label = QFileInfo(path).fileName();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        rec.error = QStringLiteral("cannot open the file");
        return rec;
    }
    const Recording table = parseTable(file.readAll(), format.separator,
                                       std::max(format.headerRows, 0),
                                       format.dataLabel, format.columns);
    rec.channels = table.channels;
    rec.rows = table.rows;
    rec.error = table.error;
    rec.frequency = format.frequency;
    if (!format.filenamePrototype.isEmpty())
        rec.start = FileUtils::getDateTimeFromFilename(rec.label, format.filenamePrototype);
    if (rec.error.isEmpty() && rec.frequency <= 0.0)
        rec.error = QStringLiteral("no acquisition frequency");
    return rec;
}

// The sample is five minutes of the engine's regression archive
// tests/regression/data_ghg/2021-08-21T013000_AIU-0737.ghg: the sonic's own
// u, v, w and Ts, CO₂ and H₂O mole fractions and CH₄, at 10 Hz.
Recording readSample()
{
    Recording rec;
    QFile file(QStringLiteral(":/icons/egg-matrix-sample"));
    if (!file.open(QIODevice::ReadOnly))
    {
        rec.error = QStringLiteral("no sample");
        return rec;
    }
    const QByteArray text = file.readAll();
    const int firstBreak = text.indexOf('\n');
    const int secondBreak = text.indexOf('\n', firstBreak + 1);
    // line 1: "# 10 Hz, 2021-08-21T01:30:00", line 2: the column names
    const QString info = QString::fromLatin1(text.left(firstBreak)).mid(1).trimmed();
    const QStringList columns = QString::fromLatin1(text.mid(firstBreak + 1, secondBreak - firstBreak - 1))
                                    .trimmed().split(QLatin1Char(','));
    const Recording table = parseTable(text.mid(secondBreak + 1), QStringLiteral("comma"), 0,
                                       QString(), columns);
    rec.channels = table.channels;
    rec.rows = table.rows;
    rec.error = table.error;
    rec.frequency = info.section(QLatin1Char(' '), 0, 0).toDouble();
    rec.start = QDateTime::fromString(info.section(QLatin1String(", "), 1), Qt::ISODate);
    rec.label = QStringLiteral("2021-08-21T013000_AIU-0737.ghg");
    rec.isSample = true;
    return rec;
}

} // namespace RawPlayback
