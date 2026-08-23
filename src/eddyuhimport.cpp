/***************************************************************************
  eddyuhimport.cpp
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

#include "eddyuhimport.h"

#include "anem_desc.h"
#include "dlproject.h"
#include "ecproject.h"
#include "irga_desc.h"
#include "matfile.h"
#include "variable_desc.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QRegularExpression>

#include <cmath>

namespace {

//> ---------------------------------------------------------------------
//> Vocabularies. Each table is one direction only and each miss is
//> reported, because a silent fallback here is a project that looks right
//> and computes something else.
//> ---------------------------------------------------------------------

struct NamePair
{
    const char* eddyuh;
    int eddyflow;
};

//> EddyUH's variable names, as they appear in set_sonic.Variables and
//> set_Gan.Variables, against VariableDesc's own list.
const NamePair kVariables[] = {
    {"u", 0}, {"v", 1}, {"w", 2},
    {"Ts", 3}, {"T_s", 3}, {"Tsonic", 3},
    {"CO2", 5}, {"H2O", 6}, {"CH4", 7}, {"N2O", 8}, {"COS", 31},
    {"CO", 19}, {"SO2", 20}, {"O3", 21}, {"NH3", 22}, {"NO", 23},
    {"NO2", 24}, {"N2", 32}, {"O2", 33}, {"Ar", 34},
    //> T_C and P_C are the analyser cell, not the ambient air. Reading them
    //> as ambient would feed the cell's pressure into the air density.
    {"T_C", 15}, {"P_C", 11},
    {"T_A", 12}, {"P_A", 13},
};

const NamePair kUnits[] = {
    {"m/s", 4}, {"cm/s", 3}, {"mm/s", 2},
    {"K", 5}, {"C", 7},
    {"ppm", 10}, {"umol/mol", 10},
    {"mmol/mol", 9}, {"ppt", 9},
    {"ppb", 11}, {"nmol/mol", 11},
    {"Pa", 14}, {"hPa", 15}, {"kPa", 16},
    {"mmol/m3", 12}, {"umol/m3", 13},
    {"g/m3", 19}, {"mg/m3", 20}, {"ug/m3", 21},
    {"mV", 0}, {"V", 1},
};

//> EddyUH's unittypes, against VariableDesc's measure type. Anything that is
//> not one of the three gas conventions leaves the field blank, which is what
//> EddyFlow does for wind and for cell diagnostics.
const NamePair kMeasureTypes[] = {
    {"Molar density", 0}, {"Mass density", 0}, {"Molar/Mass density", 0},
    {"Mole fraction", 1},
    {"Mixing ratio", 2},
};

//> EddyUH's set_sonic.Name against AnemDesc's model list. EddyUH's names
//> are free text typed into its setup dialog, so a miss here is expected and
//> is reported rather than guessed at.
struct ModelPair
{
    const char* eddyuh;
    int manufacturer;
    int model;
};

//> The numbers are the ACCESSOR numbers - getANEM_MODEL_STRING_1() and so
//> on - and not positions in any list, for the reason given further down.
const ModelPair kAnemometers[] = {
    {"GillHS", 1, 1},           // Gill, HS-50
    {"Gill HS", 1, 1},
    {"GillHS-50", 1, 1},
    {"GillHS-100", 1, 2},
    {"GillR2", 1, 3},
    {"GillR3", 1, 4},
    {"Gill R3-50", 1, 4},
    {"Gill R3-100", 1, 5},
    {"Windmaster", 1, 7},
    {"Windmaster Pro", 1, 8},
    {"CSAT3", 0, 0},            // Campbell Scientific, CSAT-3
    {"Campbell CSAT3", 0, 0},
    {"Metek USA-1", 2, 9},      // Metek, USA-1 Standard
    {"MetekUSA-1", 2, 9},
    {"Young81000", 3, 11},      // Young, 81000
};

//> And set_Gan.Name against IrgaDesc's.
const ModelPair kAnalysers[] = {
    {"Licor7200", 0, 4},        // LI-COR, LI-7200
    {"LI-7200", 0, 4},
    {"Licor7500", 0, 2},
    {"LI-7500", 0, 2},
    {"LI-7500A", 0, 3},
    {"LI-7700", 0, 5},
    {"Licor7000", 0, 1},
    {"LI-7000", 0, 1},
    {"Licor6262", 0, 0},
    {"LI-6262", 0, 0},
    //> EddyUH's name for an Aerodyne quantum cascade laser. It says nothing
    //> about which one, and EddyFlow's spectroscopic and multiplier
    //> corrections are chosen from the model, so this is reported as an
    //> assumption rather than left to look like a fact.
    {"Aerodyne cw-QCL", 4, 20},  // Aerodyne Research, TILDAS Analyzer
    {"Aerodyne QCL", 4, 20},
    {"AerodyneQCL", 4, 20},
};

//> Names that are matched but are a guess about which variant, so the user is
//> told rather than left to discover it from a correction that did not fire.
const char* const kApproximateAnalysers[] = {
    "Aerodyne cw-QCL", "Aerodyne QCL", "AerodyneQCL",
};

int lookupModel(const ModelPair* table, int n, const QString& key,
                int* manufacturer, int* model)
{
    for (int i = 0; i < n; ++i)
    {
        if (key.compare(QLatin1String(table[i].eddyuh), Qt::CaseInsensitive)
            == 0)
        {
            *manufacturer = table[i].manufacturer;
            *model = table[i].model;
            return i;
        }
    }
    return -1;
}

template <size_t N>
int lookupModel(const ModelPair (&table)[N], const QString& key,
                int* manufacturer, int* model)
{
    return lookupModel(table, static_cast<int>(N), key, manufacturer, model);
}

int lookup(const NamePair* table, int n, const QString& key, int fallback)
{
    for (int i = 0; i < n; ++i)
    {
        if (key.compare(QLatin1String(table[i].eddyuh), Qt::CaseInsensitive)
            == 0)
        {
            return table[i].eddyflow;
        }
    }
    return fallback;
}

template <size_t N>
int lookup(const NamePair (&table)[N], const QString& key, int fallback)
{
    return lookup(table, static_cast<int>(N), key, fallback);
}

//> ---------------------------------------------------------------------
//> Index to string.
//>
//> Every ...StringList() in this application is sorted alphabetically before
//> it is returned, so the position of a string in the list is NOT the number
//> of the accessor that produced it. Indexing the list turned raw column 1
//> into AGC and a Gill HS into an 81000RE, silently, in a file that looked
//> plausible. So the accessors are called directly.
//> ---------------------------------------------------------------------

QString variableString(int i)
{
    switch (i)
    {
    case 0:
        return VariableDesc::getVARIABLE_VAR_STRING_0();
    case 1:
        return VariableDesc::getVARIABLE_VAR_STRING_1();
    case 2:
        return VariableDesc::getVARIABLE_VAR_STRING_2();
    case 3:
        return VariableDesc::getVARIABLE_VAR_STRING_3();
    case 4:
        return VariableDesc::getVARIABLE_VAR_STRING_4();
    case 5:
        return VariableDesc::getVARIABLE_VAR_STRING_5();
    case 6:
        return VariableDesc::getVARIABLE_VAR_STRING_6();
    case 7:
        return VariableDesc::getVARIABLE_VAR_STRING_7();
    case 8:
        return VariableDesc::getVARIABLE_VAR_STRING_8();
    case 9:
        return VariableDesc::getVARIABLE_VAR_STRING_9();
    case 10:
        return VariableDesc::getVARIABLE_VAR_STRING_10();
    case 11:
        return VariableDesc::getVARIABLE_VAR_STRING_11();
    case 12:
        return VariableDesc::getVARIABLE_VAR_STRING_12();
    case 13:
        return VariableDesc::getVARIABLE_VAR_STRING_13();
    case 14:
        return VariableDesc::getVARIABLE_VAR_STRING_14();
    case 15:
        return VariableDesc::getVARIABLE_VAR_STRING_15();
    case 16:
        return VariableDesc::getVARIABLE_VAR_STRING_16();
    case 17:
        return VariableDesc::getVARIABLE_VAR_STRING_17();
    case 18:
        return VariableDesc::getVARIABLE_VAR_STRING_18();
    case 19:
        return VariableDesc::getVARIABLE_VAR_STRING_19();
    case 20:
        return VariableDesc::getVARIABLE_VAR_STRING_20();
    case 21:
        return VariableDesc::getVARIABLE_VAR_STRING_21();
    case 22:
        return VariableDesc::getVARIABLE_VAR_STRING_22();
    case 23:
        return VariableDesc::getVARIABLE_VAR_STRING_23();
    case 24:
        return VariableDesc::getVARIABLE_VAR_STRING_24();
    case 25:
        return VariableDesc::getVARIABLE_VAR_STRING_25();
    case 26:
        return VariableDesc::getVARIABLE_VAR_STRING_26();
    case 27:
        return VariableDesc::getVARIABLE_VAR_STRING_27();
    case 28:
        return VariableDesc::getVARIABLE_VAR_STRING_28();
    case 29:
        return VariableDesc::getVARIABLE_VAR_STRING_29();
    case 30:
        return VariableDesc::getVARIABLE_VAR_STRING_30();
    case 31:
        return VariableDesc::getVARIABLE_VAR_STRING_31();
    case 32:
        return VariableDesc::getVARIABLE_VAR_STRING_32();
    case 33:
        return VariableDesc::getVARIABLE_VAR_STRING_33();
    case 34:
        return VariableDesc::getVARIABLE_VAR_STRING_34();
    case 35:
        return VariableDesc::getVARIABLE_VAR_STRING_35();
    case 36:
        return VariableDesc::getVARIABLE_VAR_STRING_36();
    default:
        return QString();
    }
}

QString unitString(int i)
{
    switch (i)
    {
    case 0:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_0();
    case 1:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_1();
    case 2:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_2();
    case 3:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_3();
    case 4:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_4();
    case 5:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_5();
    case 6:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_6();
    case 7:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_7();
    case 8:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_8();
    case 9:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_9();
    case 10:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_10();
    case 11:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_11();
    case 12:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_12();
    case 13:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_13();
    case 14:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_14();
    case 15:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_15();
    case 16:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_16();
    case 17:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_17();
    case 18:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_18();
    case 19:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_19();
    case 20:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_20();
    case 21:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_21();
    case 22:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_22();
    case 23:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_23();
    case 24:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_24();
    case 25:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_25();
    case 26:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_26();
    case 27:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_27();
    case 28:
        return VariableDesc::getVARIABLE_MEASURE_UNIT_STRING_28();
    default:
        return QString();
    }
}

QString measureTypeString(int i)
{
    switch (i)
    {
    case 0:
        return VariableDesc::getVARIABLE_MEASURE_TYPE_STRING_0();
    case 1:
        return VariableDesc::getVARIABLE_MEASURE_TYPE_STRING_1();
    case 2:
        return VariableDesc::getVARIABLE_MEASURE_TYPE_STRING_2();
    case 3:
        return VariableDesc::getVARIABLE_MEASURE_TYPE_STRING_3();
    default:
        return QString();
    }
}

QString anemManufacturer(int i)
{
    switch (i)
    {
    case 0:
        return AnemDesc::getANEM_MANUFACTURER_STRING_0();
    case 1:
        return AnemDesc::getANEM_MANUFACTURER_STRING_1();
    case 2:
        return AnemDesc::getANEM_MANUFACTURER_STRING_2();
    case 3:
        return AnemDesc::getANEM_MANUFACTURER_STRING_3();
    case 4:
        return AnemDesc::getANEM_MANUFACTURER_STRING_4();
    default:
        return QString();
    }
}

QString anemModel(int i)
{
    switch (i)
    {
    case 0:
        return AnemDesc::getANEM_MODEL_STRING_0();
    case 1:
        return AnemDesc::getANEM_MODEL_STRING_1();
    case 2:
        return AnemDesc::getANEM_MODEL_STRING_2();
    case 3:
        return AnemDesc::getANEM_MODEL_STRING_3();
    case 4:
        return AnemDesc::getANEM_MODEL_STRING_4();
    case 5:
        return AnemDesc::getANEM_MODEL_STRING_5();
    case 6:
        return AnemDesc::getANEM_MODEL_STRING_6();
    case 7:
        return AnemDesc::getANEM_MODEL_STRING_7();
    case 8:
        return AnemDesc::getANEM_MODEL_STRING_8();
    case 9:
        return AnemDesc::getANEM_MODEL_STRING_9();
    case 10:
        return AnemDesc::getANEM_MODEL_STRING_10();
    case 11:
        return AnemDesc::getANEM_MODEL_STRING_11();
    case 12:
        return AnemDesc::getANEM_MODEL_STRING_12();
    case 13:
        return AnemDesc::getANEM_MODEL_STRING_13();
    case 14:
        return AnemDesc::getANEM_MODEL_STRING_14();
    case 15:
        return AnemDesc::getANEM_MODEL_STRING_15();
    case 16:
        return AnemDesc::getANEM_MODEL_STRING_16();
    case 17:
        return AnemDesc::getANEM_MODEL_STRING_17();
    case 18:
        return AnemDesc::getANEM_MODEL_STRING_18();
    case 19:
        return AnemDesc::getANEM_MODEL_STRING_19();
    case 20:
        return AnemDesc::getANEM_MODEL_STRING_20();
    case 21:
        return AnemDesc::getANEM_MODEL_STRING_21();
    default:
        return QString();
    }
}

QString irgaManufacturer(int i)
{
    switch (i)
    {
    case 0:
        return IrgaDesc::getIRGA_MANUFACTURER_STRING_0();
    case 1:
        return IrgaDesc::getIRGA_MANUFACTURER_STRING_1();
    case 2:
        return IrgaDesc::getIRGA_MANUFACTURER_STRING_2();
    case 3:
        return IrgaDesc::getIRGA_MANUFACTURER_STRING_3();
    case 4:
        return IrgaDesc::getIRGA_MANUFACTURER_STRING_4();
    default:
        return QString();
    }
}

QString irgaModel(int i)
{
    switch (i)
    {
    case 0:
        return IrgaDesc::getIRGA_MODEL_STRING_0();
    case 1:
        return IrgaDesc::getIRGA_MODEL_STRING_1();
    case 2:
        return IrgaDesc::getIRGA_MODEL_STRING_2();
    case 3:
        return IrgaDesc::getIRGA_MODEL_STRING_3();
    case 4:
        return IrgaDesc::getIRGA_MODEL_STRING_4();
    case 5:
        return IrgaDesc::getIRGA_MODEL_STRING_5();
    case 6:
        return IrgaDesc::getIRGA_MODEL_STRING_6();
    case 7:
        return IrgaDesc::getIRGA_MODEL_STRING_7();
    case 8:
        return IrgaDesc::getIRGA_MODEL_STRING_8();
    case 9:
        return IrgaDesc::getIRGA_MODEL_STRING_9();
    case 10:
        return IrgaDesc::getIRGA_MODEL_STRING_10();
    case 11:
        return IrgaDesc::getIRGA_MODEL_STRING_11();
    case 12:
        return IrgaDesc::getIRGA_MODEL_STRING_12();
    case 13:
        return IrgaDesc::getIRGA_MODEL_STRING_13();
    case 14:
        return IrgaDesc::getIRGA_MODEL_STRING_14();
    case 15:
        return IrgaDesc::getIRGA_MODEL_STRING_15();
    case 16:
        return IrgaDesc::getIRGA_MODEL_STRING_16();
    case 17:
        return IrgaDesc::getIRGA_MODEL_STRING_17();
    case 18:
        return IrgaDesc::getIRGA_MODEL_STRING_18();
    case 19:
        return IrgaDesc::getIRGA_MODEL_STRING_19();
    case 20:
        return IrgaDesc::getIRGA_MODEL_STRING_20();
    case 21:
        return IrgaDesc::getIRGA_MODEL_STRING_21();
    case 22:
        return IrgaDesc::getIRGA_MODEL_STRING_22();
    case 23:
        return IrgaDesc::getIRGA_MODEL_STRING_23();
    default:
        return QString();
    }
}

///
/// EddyUH's raw file name tokens against EddyFlow's.
///
/// The order matters twice over. MIN has to go before MM or the minute token
/// is eaten by the month one; DOY before DD for the same reason in reverse.
/// MIN is parked on a character that cannot occur in a file name until MM has
/// been dealt with, because EddyUH's minute token becomes EddyFlow's month
/// token spelt differently.
///
QString convertPrototype(const QString& in)
{
    const QChar parked(0x0001);
    QString out = in;
    out.replace(QLatin1String("YYYY"), QLatin1String("yyyy"));
    out.replace(QLatin1String("MIN"), QString(parked));
    out.replace(QLatin1String("MM"), QLatin1String("mm"));
    out.replace(QLatin1String("DOY"), QLatin1String("ddd"));
    out.replace(QLatin1String("DD"), QLatin1String("dd"));
    out.replace(QLatin1String("YY"), QLatin1String("yy"));
    out.replace(parked, QLatin1String("MM"));
    return out;
}

QString delimiterName(int code)
{
    //> EddyUH_ReadFileRaw.m:62. Anything else is a project this cannot read.
    switch (code)
    {
    case 2:
        return QStringLiteral("tab");
    case 3:
        return QStringLiteral("space");
    case 4:
        return QStringLiteral("comma");
    case 5:
        return QStringLiteral("semicolon");
    default:
        return QString();
    }
}

///
/// EddyUH's ConvFactor is [offset gain] per variable, but the sonic stores it
/// 2 x nvars and a gas analyser stores it nvars x 2 - in the same file. Read
/// by shape rather than by which instrument it came from.
///
void conversionFactors(const MatValue& conv, int nvars, int j,
                       double* gain, double* offset)
{
    *gain = 1.0;
    *offset = 0.0;
    const auto n = conv.numbers();
    if (n.isEmpty() || j < 0 || j >= nvars)
    {
        return;
    }
    //> Column-major throughout, so the two shapes index differently.
    if (conv.rows() == 2 && conv.columns() == nvars)
    {
        if (2 * j + 1 < n.size())
        {
            *offset = n.at(2 * j);
            *gain = n.at(2 * j + 1);
        }
    }
    else if (conv.rows() == nvars && conv.columns() == 2)
    {
        if (j + nvars < n.size())
        {
            *offset = n.at(j);
            *gain = n.at(j + nvars);
        }
    }
}

}  // namespace

bool EddyUhImport::looksLikeEddyUhProject(const QString& path)
{
    const QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile())
    {
        return false;
    }
    if (!fi.fileName().startsWith(QLatin1String("preproc_")))
    {
        return false;
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        return false;
    }
    const auto head = f.read(19);
    f.close();
    return head == QByteArrayLiteral("MATLAB 5.0 MAT-file");
}

QStringList EddyUhImport::siblingsOf(const QString& preprocPath)
{
    const QFileInfo fi(preprocPath);
    const QString stem = fi.fileName().mid(QStringLiteral("preproc_").size());
    const QDir dir = fi.dir();

    QStringList found;
    //> The siblings carry a run-count suffix - lag_<stem>.10cl, .1cl, .8cl -
    //> that changes every time EddyUH writes one, so they are matched on the
    //> stem and not on a full name.
    for (const auto& prefix : {QStringLiteral("lag_"),
                               QStringLiteral("planar_fit_"),
                               QStringLiteral("resptime_")})
    {
        const auto entries = dir.entryList(
            QStringList() << (prefix + stem + QStringLiteral("*")),
            QDir::Files);
        if (!entries.isEmpty())
        {
            found.append(dir.filePath(entries.first()));
        }
    }
    return found;
}

bool EddyUhImport::convert(const QString& path, EcProject* ec, DlProject* dl,
                           QString* error)
{
    notes_.clear();
    if (!ec || !dl)
    {
        return false;
    }

    MatFile pre;
    QString err;
    if (!pre.read(path, &err))
    {
        if (error)
        {
            *error = err;
        }
        return false;
    }

    //> A preproc file that carries none of these is not an EddyUH project,
    //> whatever it is called - better to say so than to produce a document
    //> full of defaults and call it an import.
    if (!pre.contains(QStringLiteral("set_sonic"))
        || !pre.contains(QStringLiteral("Columnorder")))
    {
        if (error)
        {
            *error = QObject::tr("%1 is a MATLAB file, but it does not hold an "
                                 "EddyUH project: there is no set_sonic and no "
                                 "Columnorder in it.").arg(path);
        }
        return false;
    }

    const QFileInfo fi(path);
    stem_ = fi.fileName().mid(QStringLiteral("preproc_").size());

    //> ---------------------------------------------------------------
    //> Site, station and timing.
    //> ---------------------------------------------------------------
    dl->setSiteName(pre.value(QStringLiteral("ECsite")).toString());
    dl->setSiteId(pre.value(QStringLiteral("ECsystem")).toString());
    dl->setSiteAltitude(pre.value(QStringLiteral("z_site")).toDouble());
    dl->setSiteLatitude(pre.value(QStringLiteral("lat")).toDouble());
    dl->setSiteCanopyHeight(pre.value(QStringLiteral("hc")).toDouble());
    dl->setSiteDisplacementHeight(pre.value(QStringLiteral("d")).toDouble());
    dl->setSiteRoughnessLength(pre.value(QStringLiteral("z_0")).toDouble());
    //> EddyUH holds no longitude at all. It matters for the potential
    //> radiation the daytime split uses, so it is worth saying out loud.
    note(QObject::tr("Longitude is not in an EddyUH project and was left at "
                     "zero. Set it on the Metadata page - the daytime/night "
                     "split uses it."));

    const MatValue sonic = pre.value(QStringLiteral("set_sonic"));
    const double fs = sonic.field(QStringLiteral("fs")).toDouble(10.0);
    dl->setAcquisitionFrequency(fs);
    dl->setFileDuration(pre.value(QStringLiteral("rawfile_len")).toInt(30));

    //> ---------------------------------------------------------------
    //> The raw files.
    //> ---------------------------------------------------------------
    const MatValue rawSetup = pre.value(QStringLiteral("rawdatafilesetup"));
    const QString rawPath = rawSetup.field(QStringLiteral("rawfilepath"))
                                .toString();
    if (!rawPath.isEmpty())
    {
        const QFileInfo rf(rawPath);
        dl->setDataPath(rf.path());
        ec->setScreenDataPath(rf.path());
        ec->setGeneralFilePrototype(convertPrototype(rf.fileName()));
    }
    ec->setGeneralFileType(Defs::RawFileType::ASCII);

    const int startRow = rawSetup.field(QStringLiteral("startrow")).toInt(1);
    dl->setHeaderRows(qMax(0, startRow - 1));
    const int delim = rawSetup.field(QStringLiteral("coldelimiter")).toInt(0);
    const QString sep = delimiterName(delim);
    if (sep.isEmpty())
    {
        note(QObject::tr("The raw file column delimiter (%1) is one EddyFlow "
                         "does not offer. Set it on the Metadata page.")
                 .arg(delim));
    }
    else
    {
        dl->setFieldSep(sep);
    }

    //> 0 beginning, 1 middle, 2 end of the averaging period. EddyFlow has
    //> only the two ends.
    const int mintype = rawSetup.field(QStringLiteral("mintype")).toInt(0);
    dl->setTimestampEnd(mintype == 2 ? 1 : 0);
    if (mintype == 1)
    {
        note(QObject::tr("The raw file timestamps are at the MIDDLE of the "
                         "averaging period, which EddyFlow does not offer. "
                         "They were imported as start-of-period, which shifts "
                         "every record by half an interval."));
    }
    if (rawSetup.field(QStringLiteral("datatype")).toInt(1) != 1)
    {
        note(QObject::tr("The raw files are not generic ASCII. Check the file "
                         "type on the Basic Settings page."));
    }

    //> ---------------------------------------------------------------
    //> Project identity and period.
    //> ---------------------------------------------------------------
    ec->setGeneralTitle(pre.value(QStringLiteral("projectname")).toString());
    auto asDate = [](const QString& yyyymmdd) -> QString {
        if (yyyymmdd.size() != 8)
        {
            return QString();
        }
        return yyyymmdd.left(4) + QLatin1Char('-') + yyyymmdd.mid(4, 2)
               + QLatin1Char('-') + yyyymmdd.mid(6, 2);
    };
    const QString start = asDate(pre.value(QStringLiteral("startdate"))
                                     .toString());
    const QString end = asDate(pre.value(QStringLiteral("enddate")).toString());
    if (!start.isEmpty())
    {
        ec->setGeneralStartDate(start);
    }
    if (!end.isEmpty())
    {
        ec->setGeneralEndDate(end);
    }

    //> ---------------------------------------------------------------
    //> Processing choices.
    //> ---------------------------------------------------------------
    ec->setScreenAvrgLen(pre.value(QStringLiteral("ave_t")).toInt(30));

    //> EddyUH_coordrot.m: 0 none, 1 one-dimensional, 2 two, 3 three, 4
    //> planar fit. EddyFlow: 0 off, 1 double, 2 triple, 3 planar fit.
    const int rot = pre.value(QStringLiteral("Dim_coordrot")).toInt(0);
    switch (rot)
    {
    case 0:
        ec->setScreenRotMethod(0);
        break;
    case 1:
        //> A yaw-only rotation. EddyFlow has no such option, and silently
        //> promoting it to a double rotation would also null the vertical
        //> wind, which is exactly what the project chose not to do.
        ec->setScreenRotMethod(0);
        note(QObject::tr("The project uses a ONE-dimensional rotation, which "
                         "EddyFlow does not offer. Rotations were switched "
                         "OFF rather than promoted to a double rotation, "
                         "which would additionally null the vertical wind."));
        break;
    case 2:
        ec->setScreenRotMethod(1);
        break;
    case 3:
        ec->setScreenRotMethod(2);
        break;
    case 4:
        ec->setScreenRotMethod(3);
        break;
    default:
        ec->setScreenRotMethod(1);
        note(QObject::tr("Unrecognised rotation setting %1; a double rotation "
                         "was assumed.").arg(rot));
        break;
    }

    //> COVAR.m:22 - 0 block averaging, 1 linear, 2 running mean. The same
    //> three, in the same order, as EddyFlow's first three.
    const int detrend = pre.value(QStringLiteral("DetrendType")).toInt(0);
    ec->setScreenDetrendMeth(detrend >= 0 && detrend <= 2 ? detrend : 0);
    //> Tc1 is the running-mean time constant and EddyUH ignores it for the
    //> other two: its linear detrending is over the whole block, which is
    //> what EddyFlow's zero means.
    const double tc = pre.value(QStringLiteral("Tc1")).toDouble(0.0);
    ec->setScreenTimeConst(detrend == 2 ? tc : 0.0);

    //> ---------------------------------------------------------------
    //> The instruments.
    //>
    //> EddyFlow numbers them in one list, sonics and analysers together, and
    //> a column names its instrument by that number. So the sonic is built
    //> first and the analysers follow in EddyUH's own order, which is the
    //> order set_Gan is stored in.
    //> ---------------------------------------------------------------
    dl->anems()->clear();
    dl->irgas()->clear();

    {
        AnemDesc anem;
        const QString name = sonic.field(QStringLiteral("Name")).toString();
        int man = -1;
        int mod = -1;
        if (lookupModel(kAnemometers, name, &man, &mod) >= 0)
        {
            anem.setManufacturer(anemManufacturer(man));
            anem.setModel(anemModel(mod));
        }
        else
        {
            note(QObject::tr("The anemometer is called \"%1\" in the EddyUH "
                             "project, which is not a name EddyFlow knows. "
                             "Its make and model were left unset - choose "
                             "them on the Metadata page, because the "
                             "angle-of-attack and w-boost corrections are "
                             "selected from the model.").arg(name));
        }
        anem.setId(name);
        //> EddyUH does not record which way the head is aligned. Left empty
        //> the metadata carries no wref at all, which is not a valid file for
        //> a Gill; "Axis" is the usual mounting and is what EddyFlow's own
        //> new-project default uses. Reported, because being wrong here
        //> rotates every wind direction by thirty degrees.
        anem.setNorthAlignment(AnemDesc::getANEM_NORTH_ALIGN_STRING_0());
        note(QObject::tr("EddyUH does not record the anemometer's north "
                         "alignment. It was set to Axis, the usual mounting. "
                         "If the head is spar-aligned, change it on the "
                         "Metadata page - it turns every wind direction."));
        anem.setHeight(pre.value(QStringLiteral("z")).toDouble());
        anem.setNorthOffset(pre.value(QStringLiteral("Boom_dir")).toDouble());
        //> EddyUH holds one path length; EddyFlow separates the horizontal
        //> and vertical ones, and metres become centimetres.
        const double pl = sonic.field(QStringLiteral("path_length"))
                              .toDouble(0.0) * 100.0;
        anem.setHPathLength(pl);
        anem.setVPathLength(pl);
        anem.setTau(sonic.field(QStringLiteral("time_constant")).toDouble(0.0));
        dl->anems()->append(anem);
    }

    const MatValue gans = pre.value(QStringLiteral("set_Gan"));
    const int nGan = gans.type() == MatValue::Type::Struct ? gans.count() : 0;
    for (int g = 0; g < nGan; ++g)
    {
        IrgaDesc irga;
        const QString name = gans.field(QStringLiteral("Name"), g).toString();
        int man = -1;
        int mod = -1;
        if (lookupModel(kAnalysers, name, &man, &mod) >= 0)
        {
            irga.setManufacturer(irgaManufacturer(man));
            irga.setModel(irgaModel(mod));
            for (const char* approx : kApproximateAnalysers)
            {
                if (name.compare(QLatin1String(approx), Qt::CaseInsensitive)
                    == 0)
                {
                    note(QObject::tr("\"%1\" names a manufacturer but not a "
                                     "model, so it was imported as %2. Check "
                                     "it on the Metadata page: the "
                                     "spectroscopic and multiplier "
                                     "corrections are chosen from the model.")
                             .arg(name, irgaModel(mod)));
                    break;
                }
            }
        }
        else
        {
            note(QObject::tr("The gas analyser called \"%1\" is not a name "
                             "EddyFlow knows. Its make and model were left "
                             "unset - choose them on the Metadata page, since "
                             "the spectroscopic and multiplier corrections "
                             "are selected from the model.").arg(name));
        }
        irga.setId(gans.field(QStringLiteral("ID"), g).toString());
        //> EddyUH_TF.m:151 - tube length and diameter are metres and the flow
        //> rate is litres per minute. EddyFlow wants centimetres, millimetres
        //> and litres per minute.
        irga.setTubeLength(gans.field(QStringLiteral("Tube_length"), g)
                               .toDouble(0.0) * 100.0);
        irga.setTubeDiameter(gans.field(QStringLiteral("Tube_diameter"), g)
                                 .toDouble(0.0) * 1000.0);
        irga.setTubeFlowRate(gans.field(QStringLiteral("Flowrate"), g)
                                 .toDouble(0.0));
        //> EddyUH has ONE horizontal separation and no sign; EddyFlow has a
        //> northward and an eastward one, and the sign says which side of the
        //> sonic the inlet is on. Put on the northward axis, because a signed
        //> pair cannot be recovered from an unsigned scalar and guessing an
        //> eastward component would be inventing data.
        const double sep = gans.field(QStringLiteral("Hsensor_separ"), g)
                               .toDouble(0.0) * 100.0;
        irga.setTubeNSeparation(sep);
        irga.setTubeESeparation(0.0);
        irga.setTubeVSeparation(0.0);
        if (sep != 0.0)
        {
            note(QObject::tr("The %1 inlet is %2 cm from the anemometer in "
                             "EddyUH, which records no direction. It was put "
                             "on the northward axis; correct it on the "
                             "Metadata page if the inlet is east of the "
                             "sonic, because the separation correction is "
                             "computed per wind direction.")
                     .arg(name).arg(sep, 0, 'f', 1));
        }
        irga.setTau(gans.field(QStringLiteral("time_constant"), g)
                        .toDouble(0.0));
        //> NOT imported: set_Gan.path_length. EddyUH prints it to its log and
        //> uses it in no calculation (only set_sonic.path_length reaches
        //> EddyUH_TF.m), so it is unchecked free text there - the supplied
        //> project holds 76 for one analyser and 0.125 for the other, an
        //> optical path against a physical one. EddyFlow's path averaging
        //> does use it, so importing it would turn a number that meant
        //> nothing into one that changes the answer.
        if (gans.hasField(QStringLiteral("path_length"), g))
        {
            note(QObject::tr("The path length of %1 was NOT imported. EddyUH "
                             "stores one but never uses it, so its value is "
                             "unchecked; EddyFlow's spectral correction does "
                             "use it. Set it on the Metadata page.").arg(name));
        }
        dl->irgas()->append(irga);
    }

    //> ---------------------------------------------------------------
    //> The columns.
    //>
    //> Columnorder lists the raw columns that carry something, and each
    //> instrument's ColumnsOrder says which of them are its. Everything not
    //> named by any instrument is a column EddyFlow must be told to ignore,
    //> or it will read the wrong field.
    //> ---------------------------------------------------------------
    const int nCols = rawSetup.field(QStringLiteral("colnum")).toInt(0);
    dl->variables()->clear();

    //> Which instrument owns which raw column, and under which name.
    struct Owned
    {
        int instrument = -1;   //> index into the combined instrument list
        int slot = -1;         //> index within that instrument's Variables
        bool isGas = false;
    };
    QMap<int, Owned> owner;

    auto claim = [&](const MatValue& cols, int instrument, bool isGas) {
        const auto ns = cols.numbers();
        for (int j = 0; j < ns.size(); ++j)
        {
            Owned o;
            o.instrument = instrument;
            o.slot = j;
            o.isGas = isGas;
            owner.insert(static_cast<int>(ns.at(j)), o);
        }
    };
    claim(sonic.field(QStringLiteral("ColumnsOrder")), 0, false);
    for (int g = 0; g < nGan; ++g)
    {
        claim(gans.field(QStringLiteral("ColumnsOrder"), g), 1 + g, true);
    }

    //> A column names its instrument by the string the metadata table shows,
    //> which is "Sonic 1: HS-50" or "Irga 2: LI-7200" - a type, a per-type
    //> number and the model. DlProject::toIniVariableInstrument splits it on
    //> the colon and then on the space, and takes element 1 of each without
    //> checking, so a bare model name does not produce a wrong file: it
    //> crashes the save. Anything unknown becomes "Other", which that
    //> function handles as a special case.
    auto instrumentLabel = [&](int i) -> QString {
        QString model;
        QString type;
        int number = 0;
        if (i == 0)
        {
            if (dl->anems()->isEmpty())
            {
                return QStringLiteral("Other");
            }
            model = dl->anems()->first().model();
            type = QObject::tr("Sonic");
            number = 1;
        }
        else
        {
            const int k = i - 1;
            if (k < 0 || k >= dl->irgas()->size())
            {
                return QStringLiteral("Other");
            }
            model = dl->irgas()->at(k).model();
            type = QObject::tr("Irga");
            number = k + 1;
        }
        if (model.isEmpty())
        {
            //> The model was not recognised above and has already been
            //> reported. "Other" keeps the file writable and leaves the
            //> column pointing at nothing in particular, which is honest.
            return QStringLiteral("Other");
        }
        return type + QLatin1Char(' ') + QString::number(number)
               + QStringLiteral(": ") + model;
    };

    for (int c = 1; c <= nCols; ++c)
    {
        VariableDesc var;
        if (!owner.contains(c))
        {
            //> Not claimed by any instrument. EddyUH simply never reads it;
            //> EddyFlow needs it declared and ignored so the column count
            //> still lines up with the file.
            var.setVariable(variableString(14));  //> Ignore
            dl->variables()->append(var);
            continue;
        }

        const Owned o = owner.value(c);
        const MatValue src = o.instrument == 0
                                 ? sonic
                                 : gans.at(o.instrument - 1);
        const int elem = 0;
        const MatValue names = src.field(QStringLiteral("Variables"), elem);
        const MatValue units = src.field(QStringLiteral("units"), elem);
        const MatValue types = src.field(QStringLiteral("unittypes"), elem);

        const QString uhName = names.at(o.slot).toString();
        const int vi = lookup(kVariables, uhName, -1);
        if (vi < 0)
        {
            note(QObject::tr("Raw column %1 is called \"%2\" in the EddyUH "
                             "project, which is not a variable EddyFlow "
                             "knows. It was set to Ignore.")
                     .arg(c).arg(uhName));
            var.setVariable(variableString(14));
            dl->variables()->append(var);
            continue;
        }
        var.setVariable(variableString(vi));
        var.setInstrument(instrumentLabel(o.instrument));

        const QString uhUnit = units.at(o.slot).toString();
        const int ui = lookup(kUnits, uhUnit, -1);
        if (ui < 0)
        {
            note(QObject::tr("Raw column %1 (%2) is in \"%3\", which is not "
                             "a unit EddyFlow offers. Set it on the Metadata "
                             "page.").arg(c).arg(uhName, uhUnit));
        }
        else
        {
            var.setInputUnit(unitString(ui));
        }

        const QString uhType = types.at(o.slot).toString();
        const int ti = lookup(kMeasureTypes, uhType, -1);
        if (ti >= 0)
        {
            var.setMeasureType(measureTypeString(ti));
        }

        double gain = 1.0;
        double offset = 0.0;
        conversionFactors(src.field(QStringLiteral("ConvFactor"), elem),
                          names.count(), o.slot, &gain, &offset);
        var.setAValue(gain);
        var.setBValue(offset);
        if (gain != 1.0 || offset != 0.0)
        {
            note(QObject::tr("Raw column %1 (%2) carries an EddyUH conversion "
                             "of gain %3 and offset %4. These were imported as "
                             "the a and b coefficients; check the conversion "
                             "type on the Metadata page, which EddyUH does "
                             "not record.")
                     .arg(c).arg(uhName).arg(gain).arg(offset));
        }

        //> The lag window. EddyUH_SC_Preproc.m:84 - lags and dlags are in
        //> SECONDS for every analyser, whatever class MATLAB stored them in.
        //> One project holds them as uint8 for one instrument and float64 for
        //> the next purely because 16 fits in a byte and 0.6 does not.
        if (o.isGas)
        {
            const auto lags = src.field(QStringLiteral("lags"), elem).numbers();
            const auto dlags = src.field(QStringLiteral("dlags"), elem)
                                   .numbers();
            if (o.slot < lags.size())
            {
                const double nominal = lags.at(o.slot);
                const double margin = o.slot < dlags.size()
                                          ? dlags.at(o.slot)
                                          : 0.0;
                var.setNomTimelag(nominal);
                var.setMinTimelag(nominal - margin);
                var.setMaxTimelag(nominal + margin);
            }
        }

        dl->variables()->append(var);
    }

    //> ---------------------------------------------------------------
    //> What is simply not in the files.
    //> ---------------------------------------------------------------
    note(QObject::tr("EddyUH collects its flux-time options afresh at every "
                     "run and writes them only to the text log beside the "
                     "fluxes, so they are NOT in the project files and could "
                     "not be imported: the spectral correction method, the "
                     "cospectral model, the peak-frequency parameterisation, "
                     "the time-lag method, the data screening and the "
                     "footprint model. All of these took EddyFlow's defaults "
                     "and should be set deliberately."));

    dl->setModified(true);
    ec->setModified(true);
    return true;
}
