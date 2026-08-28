/***************************************************************************
  biometmetadatareader.cpp
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

#include "biommetadatareader.h"

#include <QDebug>
#include <QFile>
#include <QSettings>

#include <QSet>

#include "bminidefs.h"

/// A biomet label with its positional qualifier removed - SW_IN_1_1_1 becomes
/// SW_IN, TA_1_3_1 becomes TA, and a label carrying no qualifier is handed back
/// as it stands.
///
/// The rule is the engine's, from biometBaseNameAndPositionalQualifierFromLabel
/// in biomet_subs.f90: take trailing _<integer> groups off the right one at a
/// time, and stop at the first segment that is not a number.
///
/// Counting underscores cannot do it, which is what the code this replaces
/// tried. The name itself may contain them - P_RAIN_1_1_1, T_DP_1_1_1 - and a
/// name with underscores and no qualifier at all, which SW_IN is, has to
/// survive untouched. Splitting on "_" and keeping the first field turned
/// SW_IN into SW; assuming three trailing fields turned a bare SW_IN into SW
/// as well.
QString BiomMetadataReader::baseName(const QString& label)
{
    auto s = label.trimmed();

    while (true)
    {
        const auto underscore = s.lastIndexOf(QLatin1Char('_'));
        if (underscore < 0) { break; }

        auto isNumber = false;
        s.mid(underscore + 1).toInt(&isNumber);
        if (!isNumber) { break; }

        s.truncate(underscore);
    }

    return s;
}

/// Which measurement a label names.
///
/// Matched whole, against the alias sets the engine keeps in
/// biomet_enrich_vars_description.f90, so the two agree on what a name means.
/// The synonyms are the engine's: global radiation IS incoming shortwave, and
/// it files RG, R_G, RGLOBAL, R_GLOBAL, SWIN and SW_IN together as SW_IN.
///
/// Whole rather than by substring, because substrings were the bug. The
/// FLUXNET names never matched anything - "SW_IN_1_1_1".contains("RG") is
/// false, and "LW_IN_1_1_1".contains("LWIN") is false for the underscore -
/// while the short literals matched names they should not: PPFD_OUT contains
/// PPFD but is outgoing PAR, which the engine files separately as PPFD_R.
BiomMetadataReader::VarType BiomMetadataReader::varType(const QString& label)
{
    const auto base = baseName(label).toUpper();
    if (base.isEmpty()) { return VarType::Unknown; }

    static const QSet<QString> airTemperature {
        QStringLiteral("TA"), QStringLiteral("T_A"),
        QStringLiteral("T_AIR"), QStringLiteral("TAIR") };
    static const QSet<QString> airPressure {
        QStringLiteral("PA"), QStringLiteral("P_A"),
        QStringLiteral("PAIR"), QStringLiteral("P_AIR") };
    static const QSet<QString> relativeHumidity {
        QStringLiteral("RH") };
    static const QSet<QString> globalRadiation {
        QStringLiteral("RG"), QStringLiteral("R_G"),
        QStringLiteral("RGLOBAL"), QStringLiteral("R_GLOBAL"),
        QStringLiteral("SWIN"), QStringLiteral("SW_IN") };
    static const QSet<QString> longwaveIncoming {
        QStringLiteral("LWIN"), QStringLiteral("LW_IN") };
    static const QSet<QString> par {
        QStringLiteral("PPFD"), QStringLiteral("PPFD_IN") };

    if (airTemperature.contains(base))   { return VarType::AirTemperature; }
    if (airPressure.contains(base))      { return VarType::AirPressure; }
    if (relativeHumidity.contains(base)) { return VarType::RelativeHumidity; }
    if (globalRadiation.contains(base))  { return VarType::GlobalRadiation; }
    if (longwaveIncoming.contains(base)) { return VarType::LongwaveIncoming; }
    if (par.contains(base))              { return VarType::Par; }

    return VarType::Unknown;
}

BiomMetadataReader::BiomMetadataReader(QList<BiomItem> *biomMetadata)
    : biomMetadata_(biomMetadata)
{
}

bool BiomMetadataReader::readEmbMetadata(const QString& fileName)
{
    // open file
    QFile dataFile(fileName);
    if (!dataFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        // error opening file
        qDebug() << "Error: Cannot open" << fileName << "file";
        return false;
    }

    // read file
    QSettings settings(fileName, QSettings::IniFormat);

    // try old format first
    settings.beginGroup(BmIni::INIGROUP_VARS_OLD);

    // iterate through instrument list
    auto numVars = countEmbVariables(settings.allKeys());

    // if no variables are found, try new format
    if (numVars == 0)
    {
        settings.endGroup();

        settings.beginGroup(BmIni::INIGROUP_VARS);
        numVars = countEmbVariables(settings.allKeys());
    }

    for (auto k = 0; k < numVars; ++k)
    {
        auto prefix = BmIni::INI_PREFIX;
        prefix += QString::number(k + 1);
        prefix += QStringLiteral("_");

        auto var = settings.value(prefix + BmIni::INI_VARS_0, QString()).toString();

        // NOTE: not really needed for now
        auto id = settings.value(prefix + BmIni::INI_VARS_1, QString()).toString();

        // skip entries with no type ('variable' field in the biomet metadata
        // file) defined
        if (var.isEmpty())
        {
            continue;
        }

        //> Only the measurements the interface has a row for, decided on the
        //> whole base name.
        //>
        //> The test this replaces was inverted: allowedVarIDs.filter(extracted)
        //> keeps an entry when an ALLOWED id contains the extracted type, not
        //> the other way round. So single letters got in - "P" through "PA",
        //> "G" through "RG" - while SW_IN and LW_IN, which no allowed id
        //> contains, were turned away before they could ever be offered.
        if (varType(var) == VarType::Unknown)
        {
            continue;
        }

        // add allowed biogeo variables
        biomMetadata_->append(BiomItem(var, id, k + 1));
    }
    settings.endGroup();
    dataFile.close();

    return true;
}

int BiomMetadataReader::countEmbVariables(const QStringList& list)
{
    auto i = 0;
    for (const auto &s : list)
    {
        if (s.contains(BmIni::INI_VARS_0))
        {
            ++i;
        }
    }
    return i;
}

bool BiomMetadataReader::readAltMetadata(const QString& fileName)
{
    // open file
    QFile dataFile(fileName);
    if (!dataFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        // error opening file
        qDebug() << "Error: Cannot open" << fileName << "file";
        return false;
    }

    // read file
    auto baLine = dataFile.readLine();
    auto lineContent = baLine.constData();
    auto line = QString::fromUtf8(lineContent);

    if (!line.isEmpty())
    {
        // skip first 2 rows
        if (line.contains(QStringLiteral("Station Name")))
        {
            // read a line
            baLine = dataFile.readLine();
            lineContent = baLine.constData();
            line = QString::fromUtf8(lineContent);

            if (line.contains(QStringLiteral("UC4")))
            {
                // read a line
                baLine = dataFile.readLine();
                lineContent = baLine.constData();
                line = QString::fromUtf8(lineContent);
            }
        }

        auto strings = line.split(QLatin1Char(','));

        // iterate on the variable list
        for (auto k = 0; k < strings.count(); ++k)
        {
            //> The whole header field, not its first underscore-separated
            //> piece. Splitting on "_" and keeping the front of it reduced
            //> SW_IN_1_1_1 to SW and LW_IN_1_1_1 to LW, neither of which names
            //> anything - and it collapsed TA_1_1_1 and TA_1_3_1 to the same
            //> "TA", so two different sensors were offered under one label
            //> with no way to tell them apart.
            const auto label = strings.at(k).trimmed();

            if (varType(label) != VarType::Unknown)
            {
                biomMetadata_->append(BiomItem(label, label, k + 1));
            }
        }
    }
    dataFile.close();

    return true;
}
