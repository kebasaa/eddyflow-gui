/***************************************************************************
  eddyproexport.cpp
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

#include "eddyproexport.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextStream>

#include <algorithm>
#include <functional>

#include "anem_desc.h"
#include "defs.h"
#include "dlinidefs.h"
#include "dlproject.h"
#include "ecinidefs.h"
#include "ecproject.h"
#include "fileutils.h"
#include "gas_metadata.h"
#include "irga_desc.h"
#include "variable_desc.h"

namespace {

// ---- EddyPro 7's vocabulary ------------------------------------------------
//
//> Copied from LI-COR's own GUI (eddypro-gui, src/dlproject.cpp), which is the
//> only authority on what EddyPro accepts. Anything outside these lists is
//> this program's, and the export has to translate it.

const QSet<QString>& eddyProSonicModels()
{
    static const QSet<QString> s {
        QStringLiteral("csat3"), QStringLiteral("hs_50"), QStringLiteral("hs_100"),
        QStringLiteral("r2"), QStringLiteral("r3_50"), QStringLiteral("r3_100"),
        QStringLiteral("r3a_100"), QStringLiteral("wm"), QStringLiteral("wmpro"),
        QStringLiteral("usa1_standard"), QStringLiteral("usa1_fast"),
        QStringLiteral("81000"), QStringLiteral("generic_sonic"),
        QStringLiteral("csat3b"), QStringLiteral("usoni3_classa_mp"),
        QStringLiteral("usoni3_cage_mp"), QStringLiteral("81000v"),
        QStringLiteral("81000re"), QStringLiteral("81000vre"),
    };
    return s;
}

const QSet<QString>& eddyProIrgaModels()
{
    static const QSet<QString> s {
        QStringLiteral("li6262"), QStringLiteral("li7000"), QStringLiteral("li7500"),
        QStringLiteral("li7500a"), QStringLiteral("li7200"), QStringLiteral("li7700"),
        QStringLiteral("generic_open_path"), QStringLiteral("generic_closed_path"),
        QStringLiteral("open_path_krypton"), QStringLiteral("open_path_lyman"),
        QStringLiteral("closed_path_krypton"), QStringLiteral("closed_path_lyman"),
        QStringLiteral("li7500rs"), QStringLiteral("li7200rs"), QStringLiteral("li7500ds"),
    };
    return s;
}

const QSet<QString>& eddyProVariables()
{
    static const QSet<QString> s {
        QStringLiteral("u"), QStringLiteral("v"), QStringLiteral("w"),
        QStringLiteral("ts"), QStringLiteral("sos"), QStringLiteral("co2"),
        QStringLiteral("h2o"), QStringLiteral("ch4"), QStringLiteral("n2o"),
        QStringLiteral("int_t_1"), QStringLiteral("int_t_2"), QStringLiteral("int_p"),
        QStringLiteral("air_t"), QStringLiteral("air_p"), QStringLiteral("ignore"),
        QStringLiteral("cell_t"), QStringLiteral("not_numeric"), QStringLiteral("co"),
        QStringLiteral("so2"), QStringLiteral("o3"), QStringLiteral("nh3"),
        QStringLiteral("no"), QStringLiteral("no2"), QStringLiteral("diag_75"),
        QStringLiteral("diag_72"), QStringLiteral("diag_77"), QStringLiteral("fast_t"),
        QStringLiteral("flowrate"), QStringLiteral("anemometer_diagnostic"),
    };
    return s;
}

//> The species EddyPro's interface offers for its fourth gas. Any other is
//> written as n2o - which is what EddyPro's engine renames the fourth column to
//> anyway (define_used_variables.f90) - with its real constants carried in
//> gas_mw and gas_diff.
const QSet<QString>& eddyProFourthGases()
{
    static const QSet<QString> s {
        QStringLiteral("n2o"), QStringLiteral("co"), QStringLiteral("so2"),
        QStringLiteral("o3"), QStringLiteral("nh3"), QStringLiteral("no"),
        QStringLiteral("no2"),
    };
    return s;
}

const auto kGenericSonic = QStringLiteral("generic_sonic");
const auto kGenericOpenPath = QStringLiteral("generic_open_path");
const auto kGenericClosedPath = QStringLiteral("generic_closed_path");
const auto kOtherSonic = QStringLiteral("other_sonic");
const auto kOtherIrga = QStringLiteral("other_irga");

//> Instruments EddyPro knows under another name. Spelling only, so not
//> reported: the user has not lost anything.
const QHash<QString, QString>& renamedSonics()
{
    static const QHash<QString, QString> s {
        { QStringLiteral("csi_csat3"),  QStringLiteral("csat3") },
        { QStringLiteral("csi_csat3b"), QStringLiteral("csat3b") },
    };
    return s;
}

//> This program's analysers, by EddyPro's generic that stands in for each -
//> the same pairs the extended .ghg format uses, tested against EddyPro 7.0.9.
//> An analyser added later and missing here falls back on its tube: one with a
//> sampling line is closed-path.
const QSet<QString>& openPathIrgas()
{
    static const QSet<QString> s {
        QStringLiteral("csi_ec150"), QStringLiteral("csi_irgason_irga"),
    };
    return s;
}

const QSet<QString>& closedPathIrgas()
{
    static const QSet<QString> s {
        QStringLiteral("csi_ec155"), QStringLiteral("csi_tga200a"),
        QStringLiteral("miro_mga1_5"), QStringLiteral("miro_mga4_6"),
        QStringLiteral("miro_mga9_10"), QStringLiteral("miro_mgai_n2o"),
        QStringLiteral("aerodyne_tildas"),
    };
    return s;
}

/// Split an instrument id `<model>_<n>` into its model and instance number.
bool splitInstrumentId(const QString& id, QString* base, QString* n)
{
    const int i = id.lastIndexOf(QLatin1Char('_'));
    if (i <= 0) { return false; }
    bool ok = false;
    id.mid(i + 1).toInt(&ok);
    if (!ok) { return false; }
    *base = id.left(i);
    *n = id.mid(i + 1);
    return true;
}

QString sonicStandIn(const QString& base)
{
    if (eddyProSonicModels().contains(base)) { return base; }
    const auto renamed = renamedSonics().constFind(base);
    if (renamed != renamedSonics().constEnd()) { return renamed.value(); }
    return kGenericSonic;
}

QString irgaStandIn(const QString& base, bool hasTube)
{
    if (eddyProIrgaModels().contains(base)) { return base; }
    if (openPathIrgas().contains(base)) { return kGenericOpenPath; }
    if (closedPathIrgas().contains(base)) { return kGenericClosedPath; }
    return hasTube ? kGenericClosedPath : kGenericOpenPath;
}

QString standInLabel(const QString& model)
{
    if (model == kGenericSonic) { return EddyProExport::tr("EddyPro generic anemometer"); }
    if (model == kGenericOpenPath) { return EddyProExport::tr("EddyPro generic open-path analyser"); }
    return EddyProExport::tr("EddyPro generic closed-path analyser");
}

//> Rewrite an ini file whose first line is a tag. QSettings treats the tag as
//> a comment and drops it on writing, so it is taken off first and put back
//> after - the same dance EcProject::exportEddyProProject does. \a tag, when
//> given, replaces whatever tag the file had.
bool editTaggedIni(const QString& path, const QString& tag,
                   const std::function<void(QSettings&)>& edit)
{
    QFile in(path);
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text)) { return false; }
    QString content = QTextStream(&in).readAll();
    in.close();

    QString firstLine = tag;
    if (content.startsWith(QLatin1Char(';')))
    {
        const int br = content.indexOf(QLatin1Char('\n'));
        if (firstLine.isEmpty())
        {
            firstLine = content.left(br < 0 ? content.size() : br).trimmed();
        }
        content = br < 0 ? QString() : content.mid(br + 1);
    }

    {
        QFile out(path);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        {
            return false;
        }
        QTextStream(&out) << content;
    }

    {
        QSettings ini(path, QSettings::IniFormat);
        edit(ini);
        ini.sync();
        if (ini.status() != QSettings::NoError) { return false; }
    }

    if (firstLine.isEmpty()) { return true; }
    return FileUtils::prependToFile(firstLine + QLatin1Char('\n'), path);
}

// ---- processing options ----------------------------------------------------

//> One option EddyPro either reads differently or does not have.
//>
//> A *replace* rule names the values EddyPro lacks: EddyPro would read any of
//> them as something else - usually "none", and despike_vm 2 as Mauder 2013 -
//> so each is set to EddyPro's own default for the key.
//>
//> A *remove* rule is a key EddyPro does not have at all. Removing it gives
//> EddyPro's behaviour, which is what \a inert means here, so the option is
//> reported only when it was set to something else: an option that was
//> already off has not changed.
struct OptionRule
{
    QString group;
    QString key;
    const char* label;
    QVector<QPair<int, const char*>> values;
    QSet<int> unsupported;  // replace rules only
    int target;             // replace: EddyPro's value for it; remove: the inert value
    bool remove;
    //> Why, when the replacement is not simply EddyPro's default.
    const char* note = nullptr;
};

const QVector<OptionRule>& optionRules()
{
    static const QVector<OptionRule> rules {
        // ---- values EddyPro lacks -----------------------------------------
        { EcIni::INIGROUP_SCREEN_SETTINGS, EcIni::INI_SCREEN_SETTINGS_7,
          QT_TRANSLATE_NOOP("EddyProExport", "Time lag compensation"),
          { { 0, QT_TRANSLATE_NOOP("EddyProExport", "None") },
            { 1, QT_TRANSLATE_NOOP("EddyProExport", "Constant") },
            { 2, QT_TRANSLATE_NOOP("EddyProExport", "Covariance maximization with default") },
            { 3, QT_TRANSLATE_NOOP("EddyProExport", "Covariance maximization") },
            { 4, QT_TRANSLATE_NOOP("EddyProExport", "Automatic time lag optimization") },
            { 5, QT_TRANSLATE_NOOP("EddyProExport", "Pre-whitening block-bootstrap (Vitale et al. 2024)") } },
          //> Not EddyPro's default (covariance maximization with default):
          //> PWB finds a lag per period within a window it optimises, and the
          //> automatic optimization is the EddyPro method closest to that.
          { 5 }, 4, false,
          QT_TRANSLATE_NOOP("EddyProExport", "Not available in EddyPro; its automatic "
                            "time lag optimization is the closest method. Check the "
                            "optimization settings in EddyPro before running.") },
        { EcIni::INIGROUP_PROJECT, EcIni::INI_PROJECT_46,
          QT_TRANSLATE_NOOP("EddyProExport", "High-frequency spectral correction"),
          { { 0, QT_TRANSLATE_NOOP("EddyProExport", "None") },
            { 1, QT_TRANSLATE_NOOP("EddyProExport", "Moncrieff et al. (1997)") },
            { 2, QT_TRANSLATE_NOOP("EddyProExport", "Horst (1997)") },
            { 3, QT_TRANSLATE_NOOP("EddyProExport", "Ibrom et al. (2007)") },
            { 4, QT_TRANSLATE_NOOP("EddyProExport", "Fratini et al. (2012)") },
            { 5, QT_TRANSLATE_NOOP("EddyProExport", "Massman (2000, 2001)") },
            { 7, QT_TRANSLATE_NOOP("EddyProExport", "Eugster and Senn (1995)") } },
          { 7 }, 1, false },
        { EcIni::INIGROUP_PROJECT, EcIni::INI_PROJECT_55,
          QT_TRANSLATE_NOOP("EddyProExport", "Quality check flagging policy"),
          { { 0, QT_TRANSLATE_NOOP("EddyProExport", "None") },
            { 1, QT_TRANSLATE_NOOP("EddyProExport", "Mauder and Foken (2004)") },
            { 2, QT_TRANSLATE_NOOP("EddyProExport", "Foken (2003)") },
            { 3, QT_TRANSLATE_NOOP("EddyProExport", "Goeckede et al. (2004)") },
            { 4, QT_TRANSLATE_NOOP("EddyProExport", "Vitale et al. (2020)") } },
          { 4 }, 1, false },
        { EcIni::INIGROUP_RAND_ERROR, EcIni::INI_RAND_ERROR_0,
          QT_TRANSLATE_NOOP("EddyProExport", "Random uncertainty method"),
          { { 0, QT_TRANSLATE_NOOP("EddyProExport", "None") },
            { 1, QT_TRANSLATE_NOOP("EddyProExport", "Finkelstein and Sims (2001)") },
            { 2, QT_TRANSLATE_NOOP("EddyProExport", "Mann and Lenschow (1994)") },
            { 3, QT_TRANSLATE_NOOP("EddyProExport", "Mahrt (1998)") },
            { 4, QT_TRANSLATE_NOOP("EddyProExport", "Billesbach (2011)") },
            { 5, QT_TRANSLATE_NOOP("EddyProExport", "Lenschow et al. (2000)") } },
          { 4, 5 }, 0, false },
        { EcIni::INIGROUP_SCREEN_PARAM, EcIni::INI_SCREEN_PARAM_60,
          QT_TRANSLATE_NOOP("EddyProExport", "Despiking method"),
          { { 0, QT_TRANSLATE_NOOP("EddyProExport", "Vickers and Mahrt (1997)") },
            { 1, QT_TRANSLATE_NOOP("EddyProExport", "Mauder et al. (2013)") },
            { 2, QT_TRANSLATE_NOOP("EddyProExport", "Consecutive difference (EddyUH)") } },
          { 2 }, 0, false },

        // ---- options EddyPro does not have ----------------------------------
        { EcIni::INIGROUP_PROJECT, EcIni::INI_PROJECT_83,
          QT_TRANSLATE_NOOP("EddyProExport", "Cospectral model"),
          { { 0, QT_TRANSLATE_NOOP("EddyProExport", "Moncrieff et al. (1997)") },
            { 1, QT_TRANSLATE_NOOP("EddyProExport", "Kaimal et al. (1972)") },
            { 2, QT_TRANSLATE_NOOP("EddyProExport", "Sakai et al. (2001)") },
            { 3, QT_TRANSLATE_NOOP("EddyProExport", "Su et al. (2004)") },
            { 4, QT_TRANSLATE_NOOP("EddyProExport", "Moraes et al. (2008)") },
            { 5, QT_TRANSLATE_NOOP("EddyProExport", "Kristensen et al. (1997)") } },
          {}, 0, true },
        { EcIni::INIGROUP_PROJECT, EcIni::INI_PROJECT_84,
          QT_TRANSLATE_NOOP("EddyProExport", "Iterative spectral correction"), {}, {}, 0, true },
        { EcIni::INIGROUP_PROJECT, EcIni::INI_PROJECT_87,
          QT_TRANSLATE_NOOP("EddyProExport", "Post-flux despiking (Vitale et al. 2020)"), {}, {}, 0, true },
        { EcIni::INIGROUP_RAND_ERROR, EcIni::INI_RAND_ERROR_6,
          QT_TRANSLATE_NOOP("EddyProExport", "Limit of detection"),
          { { 0, QT_TRANSLATE_NOOP("EddyProExport", "None") },
            { 1, QT_TRANSLATE_NOOP("EddyProExport", "Langford et al. (2015)") } },
          {}, 0, true },
        { EcIni::INIGROUP_RAND_ERROR, EcIni::INI_RAND_ERROR_5,
          QT_TRANSLATE_NOOP("EddyProExport", "Random uncertainty scaled by the spectral correction"),
          {}, {}, 0, true },
        { EcIni::INIGROUP_SCREEN_SETTINGS, EcIni::INI_SCREEN_SETTINGS_102,
          QT_TRANSLATE_NOOP("EddyProExport", "Flux detection limit"),
          { { 0, QT_TRANSLATE_NOOP("EddyProExport", "None") },
            { 1, QT_TRANSLATE_NOOP("EddyProExport", "Wienhold et al. (1994)") },
            { 2, QT_TRANSLATE_NOOP("EddyProExport", "Langford et al. (2015)") } },
          {}, 0, true },
        { EcIni::INIGROUP_SCREEN_SETTINGS, EcIni::INI_SCREEN_SETTINGS_105,
          QT_TRANSLATE_NOOP("EddyProExport", "Spectroscopic correction (Peltola et al. 2014)"),
          {}, {}, 0, true },
        { EcIni::INIGROUP_SCREEN_SETTINGS, EcIni::INI_SCREEN_SETTINGS_107,
          QT_TRANSLATE_NOOP("EddyProExport", "Covariance maximization against the window's chord"),
          {}, {}, 0, true },
        { EcIni::INIGROUP_SCREEN_SETTINGS, EcIni::INI_SCREEN_SETTINGS_108,
          QT_TRANSLATE_NOOP("EddyProExport", "Conditional time lag borrowing (Nemitz et al. 2018)"),
          {}, {}, 0, true },
        { EcIni::INIGROUP_SCREEN_SETTINGS, EcIni::INI_SCREEN_SETTINGS_112,
          QT_TRANSLATE_NOOP("EddyProExport", "Inclinometer tilt correction"),
          { { 0, QT_TRANSLATE_NOOP("EddyProExport", "Off") },
            { 1, QT_TRANSLATE_NOOP("EddyProExport", "Position") },
            { 2, QT_TRANSLATE_NOOP("EddyProExport", "Position and swinging") } },
          {}, 0, true },
        { EcIni::INIGROUP_SCREEN_SETTINGS, EcIni::INI_SCREEN_SETTINGS_118,
          QT_TRANSLATE_NOOP("EddyProExport", "Metek head correction"),
          { { 0, QT_TRANSLATE_NOOP("EddyProExport", "Off") },
            { 1, QT_TRANSLATE_NOOP("EddyProExport", "On the raw data") },
            { 2, QT_TRANSLATE_NOOP("EddyProExport", "Undo the online 2-D correction") } },
          {}, 0, true },
        { EcIni::INIGROUP_SCREEN_SETTINGS, EcIni::INI_SCREEN_SETTINGS_120,
          QT_TRANSLATE_NOOP("EddyProExport", "Storage flux cleaning (Vitale et al. 2020)"),
          {}, {}, 0, true },
        { EcIni::INIGROUP_SCREEN_TESTS, EcIni::INI_SCREEN_TESTS_9,
          QT_TRANSLATE_NOOP("EddyProExport", "Instrument malfunction diagnostics (RFlux)"),
          {}, {}, 0, true },
        { EcIni::INIGROUP_SCREEN_TILT, EcIni::INI_SCREEN_TILT_14,
          QT_TRANSLATE_NOOP("EddyProExport", "Planar fit: assessment only"), {}, {}, 0, true },
        { EcIni::INIGROUP_TIMELAG_OPT, EcIni::INI_TIMELAG_OPT_21,
          QT_TRANSLATE_NOOP("EddyProExport", "Time lag optimization: assessment only"), {}, {}, 0, true },
    };
    return rules;
}

//> Keys EddyPro does not have whose meaning is carried by a rule above - the
//> settings of an option, not an option - or that only this program's
//> interface reads. Removed without a word: reporting them would repeat the
//> option they belong to.
const QVector<QPair<QString, QString>>& silentRemovals()
{
    static const QVector<QPair<QString, QString>> keys {
        { EcIni::INIGROUP_PROJECT, EcIni::INI_PROJECT_85 },
        { EcIni::INIGROUP_PROJECT, EcIni::INI_PROJECT_86 },
        { EcIni::INIGROUP_RAND_ERROR, EcIni::INI_RAND_ERROR_4 },
        { EcIni::INIGROUP_SCREEN_SETTINGS, EcIni::INI_SCREEN_SETTINGS_103 },
        { EcIni::INIGROUP_SCREEN_SETTINGS, EcIni::INI_SCREEN_SETTINGS_104 },
        { EcIni::INIGROUP_SCREEN_SETTINGS, EcIni::INI_SCREEN_SETTINGS_106 },
        { EcIni::INIGROUP_SCREEN_SETTINGS, EcIni::INI_SCREEN_SETTINGS_109 },
        { EcIni::INIGROUP_SCREEN_SETTINGS, EcIni::INI_SCREEN_SETTINGS_110 },
        { EcIni::INIGROUP_SCREEN_SETTINGS, EcIni::INI_SCREEN_SETTINGS_111 },
        { EcIni::INIGROUP_SCREEN_SETTINGS, EcIni::INI_SCREEN_SETTINGS_124 },
        { EcIni::INIGROUP_SCREEN_SETTINGS, EcIni::INI_SCREEN_SETTINGS_113 },
        { EcIni::INIGROUP_SCREEN_SETTINGS, EcIni::INI_SCREEN_SETTINGS_114 },
        { EcIni::INIGROUP_SCREEN_SETTINGS, EcIni::INI_SCREEN_SETTINGS_115 },
        { EcIni::INIGROUP_SCREEN_SETTINGS, EcIni::INI_SCREEN_SETTINGS_116 },
        { EcIni::INIGROUP_SCREEN_SETTINGS, EcIni::INI_SCREEN_SETTINGS_117 },
        { EcIni::INIGROUP_SCREEN_SETTINGS, EcIni::INI_SCREEN_SETTINGS_119 },
        { EcIni::INIGROUP_SCREEN_SETTINGS, EcIni::INI_SCREEN_SETTINGS_121 },
        { EcIni::INIGROUP_SCREEN_SETTINGS, EcIni::INI_SCREEN_SETTINGS_122 },
        { EcIni::INIGROUP_SCREEN_SETTINGS, EcIni::INI_SCREEN_SETTINGS_123 },
        { EcIni::INIGROUP_SCREEN_PARAM, EcIni::INI_SCREEN_PARAM_61 },
        { EcIni::INIGROUP_SCREEN_PARAM, EcIni::INI_SCREEN_PARAM_62 },
        { EcIni::INIGROUP_SCREEN_PARAM, EcIni::INI_SCREEN_PARAM_63 },
        { EcIni::INIGROUP_SCREEN_PARAM, EcIni::INI_SCREEN_PARAM_64 },
        { EcIni::INIGROUP_BIOMET, EcIni::INI_BIOMET_RH_OVERRIDE },
    };
    return keys;
}

QString valueLabel(const OptionRule& rule, int value)
{
    for (const auto& v : rule.values)
    {
        if (v.first == value)
        {
            return QCoreApplication::translate("EddyProExport", v.second);
        }
    }
    if (rule.values.isEmpty())
    {
        return value == 0 ? EddyProExport::tr("Off") : EddyProExport::tr("On");
    }
    return QString::number(value);
}

QString path(const QString& group, const QString& key)
{
    return group + QLatin1Char('/') + key;
}

QString formatNumber(double value)
{
    return QString::number(value, 'f', 6);
}

} // namespace

// ---- public ----------------------------------------------------------------

QString EddyProExport::metadataFileFor(const QString& eddyproFile)
{
    const QFileInfo fi(eddyproFile);
    return fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName()
           + QStringLiteral("_EP.") + Defs::METADATA_FILE_EXT;
}

QString EddyProExport::reportFileFor(const QString& eddyproFile)
{
    const QFileInfo fi(eddyproFile);
    return fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName()
           + QStringLiteral("_EP_export_report.txt");
}

EddyProGasSlots EddyProExport::defaultSlots(const EcProject& ec)
{
    //> The rule EcProject::writeEddyProCompatibleKeys applies when it is given
    //> no choice: the first record of each species, any other gas in the
    //> fourth slot.
    EddyProGasSlots choice;
    for (int& c : choice.column) { c = 0; }
    const char* pinned[3] = { "co2", "h2o", "ch4" };
    for (const auto& rec : ec.gasColumns())
    {
        if (rec.rawColumn <= 0) { continue; }
        int slot = 3;
        for (int s = 0; s < 3; ++s)
        {
            if (rec.slug == QLatin1String(pinned[s])) { slot = s; break; }
        }
        if (choice.column[slot] == 0) { choice.column[slot] = rec.rawColumn; }
    }
    return choice;
}

QVector<QPair<int, QString>> EddyProExport::slotCandidates(const DlProject& dl, int slot)
{
    const char* pinned[3] = { "co2", "h2o", "ch4" };
    DlProject copy(dl);
    QVector<QPair<int, QString>> found;
    const auto* vars = copy.variables();
    for (int i = 0; i < vars->size(); ++i)
    {
        const auto& var = vars->at(i);
        if (var.ignore() == QLatin1String("yes")
            || var.numeric() == QLatin1String("no"))
        {
            continue;
        }
        const auto* entry = GasMetadata::findSpecies(var.variable());
        if (!entry) { continue; }
        const auto species = GasMetadata::normaliseFormula(entry->displayFormula);

        bool fits = false;
        if (slot < 3)
        {
            fits = species == QLatin1String(pinned[slot]);
        }
        else
        {
            fits = species != QLatin1String(pinned[0])
                   && species != QLatin1String(pinned[1])
                   && species != QLatin1String(pinned[2]);
        }
        if (!fits) { continue; }

        auto label = tr("Column %1: %2").arg(i + 1).arg(entry->displayFormula);
        if (!var.instrument().isEmpty())
        {
            label += QStringLiteral(" (%1)").arg(var.instrument());
        }
        found.append({ i + 1, label });
    }
    return found;
}

bool EddyProExport::exportTo(EcProject* ec, const DlProject& dl,
                             const EddyProGasSlots& choice,
                             const QString& eddyproFile, QString* error)
{
    changes_.clear();
    warnings_.clear();
    notes_.clear();
    metadataFile_.clear();

    const auto fail = [error](const QString& why)
    {
        if (error) { *error = why; }
        return false;
    };

    //> The project is written from a copy of what is on screen, not from the
    //> file on disk: an unsaved project exports as the user sees it, and the
    //> open document - its file, its modified flag - is not touched.
    QTemporaryDir tmp;
    if (!tmp.isValid())
    {
        return fail(tr("Could not create a temporary folder."));
    }
    const auto nativeCopy = tmp.filePath(QStringLiteral("export.") + Defs::PROJECT_FILE_EXT);
    if (!ec->saveEcProjectCopy(nativeCopy))
    {
        return fail(tr("Could not write a copy of the project."));
    }

    if (ec->generalUseAltMdFile())
    {
        metadataFile_ = metadataFileFor(eddyproFile);
    }
    else
    {
        notes_ << tr("The project reads the metadata embedded in its GHG files, "
                     "so no metadata file was written: EddyPro reads the same "
                     "embedded metadata.");
    }

    if (!ec->exportEddyProProject(nativeCopy, eddyproFile, &choice, &changes_))
    {
        return fail(tr("Could not write %1.").arg(QDir::toNativeSeparators(eddyproFile)));
    }

    if (!convertProjectOptions(nativeCopy, eddyproFile, *ec, dl))
    {
        return fail(tr("Could not convert the options in %1.")
                        .arg(QDir::toNativeSeparators(eddyproFile)));
    }

    if (!convertMetadata(dl, error)) { return false; }

    //> Saved beside the export, so the record of what changed travels with
    //> the files it describes.
    QFile report(reportFileFor(eddyproFile));
    if (report.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    {
        QTextStream(&report) << reportText();
    }
    else
    {
        warnings_ << tr("The report could not be saved to %1.")
                         .arg(QDir::toNativeSeparators(report.fileName()));
    }
    return true;
}

void EddyProExport::change(const QString& area, const QString& setting,
                           const QString& from, const QString& to,
                           const QString& note)
{
    changes_.append({ area, setting, from, to, note });
}

// ---- the project file --------------------------------------------------------

bool EddyProExport::convertProjectOptions(const QString& nativeCopy,
                                          const QString& eddyproFile,
                                          const EcProject& ec,
                                          const DlProject& dl)
{
    //> Values are read from the native copy, which still holds everything the
    //> structural rewrite removed, and acted on in the finished file.
    QSettings pre(nativeCopy, QSettings::IniFormat);
    const auto options = tr("Processing options");

    if (ec.generalCecMeth() != 0)
    {
        change(options, tr("Conditional eddy covariance partitioning"),
               tr("On"), tr("Off"), tr("Not available in EddyPro."));
    }

    return editTaggedIni(eddyproFile, QString(), [&](QSettings& out)
    {
        for (const auto& rule : optionRules())
        {
            const auto key = path(rule.group, rule.key);
            if (!pre.contains(key)) { continue; }
            const int value = pre.value(key).toInt();
            if (rule.remove)
            {
                if (value != rule.target)
                {
                    change(options, tr(rule.label), valueLabel(rule, value),
                           valueLabel(rule, rule.target), tr("Not available in EddyPro."));
                }
                out.remove(key);
            }
            else if (rule.unsupported.contains(value))
            {
                change(options, tr(rule.label), valueLabel(rule, value),
                       valueLabel(rule, rule.target),
                       rule.note ? tr(rule.note)
                                 : tr("Not available in EddyPro; EddyPro's default is used."));
                out.setValue(key, rule.target);
            }
        }

        for (const auto& [group, key] : silentRemovals())
        {
            out.remove(path(group, key));
        }

        //> Per-instrument gap limits. EddyPro has the one project-wide limit,
        //> which is what each instrument falls back to here too.
        {
            const int global = pre.value(path(EcIni::INIGROUP_SCREEN_SETTINGS,
                                              EcIni::INI_SCREEN_SETTINGS_1), 10).toInt();
            const QRegularExpression re(QStringLiteral("^instr_(\\d+)_max_lack$"));
            pre.beginGroup(EcIni::INIGROUP_SCREEN_SETTINGS);
            const auto keys = pre.childKeys();
            QMap<int, int> perInstrument;
            for (const auto& k : keys)
            {
                const auto m = re.match(k);
                if (m.hasMatch()) { perInstrument.insert(m.captured(1).toInt(), pre.value(k).toInt()); }
            }
            pre.endGroup();
            for (auto it = perInstrument.cbegin(); it != perInstrument.cend(); ++it)
            {
                if (it.value() != global)
                {
                    change(options,
                           tr("Maximum missing samples, instrument %1").arg(it.key()),
                           tr("%1%").arg(it.value()), tr("%1%").arg(global),
                           tr("EddyPro applies one limit to every instrument."));
                }
                out.remove(path(EcIni::INIGROUP_SCREEN_SETTINGS,
                                EcIni::iniScreenSettingsInstrMaxLack(it.key())));
            }
        }

        // ---- [Project] -------------------------------------------------------
        out.beginGroup(EcIni::INIGROUP_PROJECT);

        //> Signal-strength records have no place in EddyPro.
        const int agc = pre.value(path(EcIni::INIGROUP_PROJECT,
                                       QStringLiteral("agc_num")), 0).toInt();
        if (agc > 0)
        {
            change(tr("Gases"), tr("Signal strength (AGC/RSSI) records"),
                   tr("%n column(s)", nullptr, agc), tr("none"),
                   tr("EddyPro reads signal strength only through LI-COR's diagnostics."));
        }
        for (const auto& k : out.childKeys())
        {
            if (k.startsWith(QLatin1String("agc_"))) { out.remove(k); }
        }

        const char* colKey[4] = { "col_co2", "col_h2o", "col_ch4", "col_n2o" };
        for (int s = 0; s < 4; ++s)
        {
            slotColumn_[s] = out.value(QLatin1String(colKey[s]), 0).toInt();
        }

        if (!metadataFile_.isEmpty())
        {
            out.setValue(EcIni::INI_PROJECT_9, 1);
            out.setValue(EcIni::INI_PROJECT_10, QDir::fromNativeSeparators(metadataFile_));
        }

        //> The master anemometer is named the way the metadata names it, so it
        //> follows the same stand-in.
        const auto master = out.value(EcIni::INI_PROJECT_17).toString();
        QString base;
        QString n;
        if (splitInstrumentId(master, &base, &n))
        {
            const auto standIn = sonicStandIn(base);
            if (standIn != base)
            {
                out.setValue(EcIni::INI_PROJECT_17, QString(standIn + QLatin1Char('_') + n));
            }
        }

        //> The fourth gas's constants. EddyPro reads gas_mw <= 0 as "use
        //> N2O's", so a project that left this program's species default in
        //> place would have its COS computed as nitrous oxide. The real values
        //> are written instead.
        if (slotColumn_[3] > 0)
        {
            DlProject copy(dl);
            const auto* vars = copy.variables();
            const int i = slotColumn_[3] - 1;
            const auto* entry = (i < vars->size())
                    ? GasMetadata::findSpecies(vars->at(i).variable()) : nullptr;
            const auto species = entry
                    ? GasMetadata::normaliseFormula(entry->displayFormula) : QString();
            if (entry && species != QLatin1String("n2o"))
            {
                double mw = out.value(QStringLiteral("gas_mw"), -1.0).toDouble();
                double diff = out.value(QStringLiteral("gas_diff"), -1.0).toDouble();
                if (mw <= 0.0)
                {
                    mw = entry->molecularWeight;
                    out.setValue(QStringLiteral("gas_mw"), QString::number(mw, 'f', 4));
                }
                if (diff <= 0.0 && entry->diffusivity > 0.0)
                {
                    diff = entry->diffusivity;
                    out.setValue(QStringLiteral("gas_diff"), QString::number(diff, 'f', 5));
                }
                if (diff <= 0.0)
                {
                    warnings_ << tr("No diffusivity is known for %1, the fourth gas, "
                                    "so EddyPro will use N₂O's. Set it in Basic "
                                    "Settings before exporting if spectral corrections "
                                    "matter for this gas.").arg(entry->displayFormula);
                }
                warnings_ << tr("The fourth gas is %1. Its molecular weight (%2 g/mol) "
                                "and diffusivity are written into the project, but "
                                "EddyPro's interface resets both to N₂O's values if "
                                "the project is opened there. Run it from the command "
                                "line, or re-enter them in EddyPro.")
                                 .arg(entry->displayFormula)
                                 .arg(QString::number(mw, 'f', 3));
            }
        }
        out.endGroup();
    });
}

// ---- the metadata file -------------------------------------------------------

bool EddyProExport::convertMetadata(const DlProject& dl, QString* error)
{
    if (metadataFile_.isEmpty()) { return true; }

    //> A fresh file. DlProject::saveProject merges into whatever a file holds
    //> outside two of its groups, so an earlier export's leftovers would
    //> otherwise survive.
    if (QFile::exists(metadataFile_) && !QFile::remove(metadataFile_))
    {
        if (error)
        {
            *error = tr("Could not replace %1.").arg(QDir::toNativeSeparators(metadataFile_));
        }
        return false;
    }

    //> saveCopy, the route for writing a metadata file that is not the user's
    //> open one - and on a copy besides, so not even its flags are touched.
    DlProject copy(dl);
    if (!copy.saveCopy(metadataFile_))
    {
        if (error)
        {
            *error = tr("Could not write %1.").arg(QDir::toNativeSeparators(metadataFile_));
        }
        return false;
    }

    //> The instrument names the user knows, by block: the metadata writes the
    //> anemometers first and the analysers after, one block each.
    QStringList display;
    for (const auto& a : *copy.anems()) { display << a.model(); }
    for (const auto& g : *copy.irgas()) { display << g.model(); }
    const auto nameOf = [&display](int k)
    {
        return (k >= 1 && k <= display.size() && !display.at(k - 1).isEmpty())
                ? display.at(k - 1)
                : tr("instrument %1").arg(k);
    };

    const auto instrumentsArea = tr("Instruments");
    const auto variablesArea = tr("Variables");

    const bool ok = editTaggedIni(metadataFile_, Defs::GHG_MD_INI_TAG,
                                  [&](QSettings& ini)
    {
        ini.setValue(path(DlIni::INIGROUP_PROJECT, DlIni::INI_GENE_SW_VERSION),
                     Defs::SMARTFLUX_SW_VERSION_STR);
        ini.remove(path(DlIni::INIGROUP_SITE, DlIni::INI_SITE_UTC_OFFSET));

        // ---- [Instruments] ---------------------------------------------------
        QMap<int, QMap<QString, QVariant>> blocks;
        ini.beginGroup(DlIni::INIGROUP_INSTRUMENTS);
        {
            const QRegularExpression re(QStringLiteral("^instr_(\\d+)_(.+)$"));
            for (const auto& key : ini.childKeys())
            {
                const auto m = re.match(key);
                if (m.hasMatch())
                {
                    blocks[m.captured(1).toInt()][m.captured(2)] = ini.value(key);
                }
            }
        }
        ini.endGroup();

        QHash<QString, QString> renamed;
        for (auto it = blocks.begin(); it != blocks.end(); ++it)
        {
            const int k = it.key();
            auto& f = it.value();
            const auto model = f.value(DlIni::INI_ANEM_2).toString();
            const bool isSonic = f.contains(DlIni::INI_ANEM_6);

            QString base;
            QString n;
            if (splitInstrumentId(model, &base, &n))
            {
                QString standIn;
                QString firm = f.value(DlIni::INI_ANEM_1).toString();
                if (isSonic)
                {
                    standIn = sonicStandIn(base);
                    if (standIn == kGenericSonic && base != kGenericSonic)
                    {
                        firm = kOtherSonic;
                    }
                    else if (renamedSonics().contains(base))
                    {
                        firm = QStringLiteral("csi");
                    }
                }
                else
                {
                    standIn = irgaStandIn(base,
                                          f.value(DlIni::INI_IRGA_5).toDouble() > 0.0);
                    if (firm != QLatin1String("licor") && firm != kOtherIrga)
                    {
                        firm = standIn.startsWith(QLatin1String("li"))
                               ? QStringLiteral("licor") : kOtherIrga;
                    }
                }

                if (standIn != base)
                {
                    const QString newId = standIn + QLatin1Char('_') + n;
                    renamed.insert(model, newId);
                    f[DlIni::INI_ANEM_2] = newId;
                    if (standIn.startsWith(QLatin1String("generic_")))
                    {
                        change(instrumentsArea, tr("Instrument %1").arg(k),
                               nameOf(k), standInLabel(standIn),
                               tr("EddyPro does not know this instrument. Its measured "
                                  "geometry is kept, which the stand-in needs to give "
                                  "the same fluxes."));
                    }
                }
                f[DlIni::INI_ANEM_1] = firm;

                //> EddyPro refuses a generic instrument it cannot model. The
                //> missing value is the user's to supply; inventing one would
                //> give fluxes that only look right.
                if (standIn.startsWith(QLatin1String("generic_")))
                {
                    const double geometry = f.value(DlIni::INI_ANEM_13).toDouble()
                                            * f.value(DlIni::INI_ANEM_14).toDouble()
                                            * f.value(DlIni::INI_ANEM_15).toDouble();
                    if (geometry == 0.0)
                    {
                        warnings_ << tr("Instrument %1 (%2): EddyPro rejects a generic "
                                        "instrument whose path lengths or time response "
                                        "are zero. Set them in the metadata before "
                                        "exporting.").arg(k).arg(nameOf(k));
                    }
                    if (standIn == kGenericClosedPath
                        && (f.value(DlIni::INI_IRGA_5).toDouble() <= 0.0
                            || f.value(DlIni::INI_IRGA_6).toDouble() <= 0.0
                            || f.value(DlIni::INI_IRGA_7).toDouble() <= 0.0))
                    {
                        warnings_ << tr("Instrument %1 (%2): EddyPro rejects a generic "
                                        "closed-path analyser without a tube length, "
                                        "diameter and flow rate. Set them in the "
                                        "metadata before exporting.").arg(k).arg(nameOf(k));
                    }
                }
            }

            const double acFreq = f.value(DlIni::INI_ANEM_17).toDouble();
            if (acFreq != 0.0)
            {
                change(instrumentsArea, tr("Instrument %1 (%2): sampling rate").arg(k).arg(nameOf(k)),
                       tr("%1 Hz").arg(acFreq), tr("the station's rate"),
                       tr("EddyPro samples every instrument at the station's rate."));
            }
            if (f.value(DlIni::INI_ANEM_18).toInt() == 1)
            {
                change(instrumentsArea, tr("Instrument %1 (%2): sampling").arg(k).arg(nameOf(k)),
                       tr("Averaged over the interval"), tr("Instantaneous"),
                       tr("EddyPro treats every reading as instantaneous."));
            }
            f.remove(DlIni::INI_ANEM_17);
            f.remove(DlIni::INI_ANEM_18);
            f.remove(DlIni::INI_INSTR_EF_MODEL);
        }

        // ---- [FileDescription] -----------------------------------------------
        QSet<QString> referenced;
        ini.beginGroup(DlIni::INIGROUP_VARDESC);
        {
            const QRegularExpression re(QStringLiteral("^col_(\\d+)_variable$"));
            QList<int> columns;
            for (const auto& key : ini.childKeys())
            {
                const auto m = re.match(key);
                if (m.hasMatch()) { columns << m.captured(1).toInt(); }
            }
            std::sort(columns.begin(), columns.end());

            for (const int c : columns)
            {
                const auto col = QStringLiteral("col_%1_").arg(c);
                const auto var = ini.value(col + DlIni::INI_VARDESC_VAR).toString();
                const auto norm = GasMetadata::normaliseFormula(var);

                auto instrument = ini.value(col + DlIni::INI_VARDESC_INSTRUMENT).toString();
                if (renamed.contains(instrument))
                {
                    instrument = renamed.value(instrument);
                    ini.setValue(col + DlIni::INI_VARDESC_INSTRUMENT, instrument);
                }
                referenced.insert(instrument);

                // the variable name
                QString newVar = var;
                QString why;
                if (c == slotColumn_[3])
                {
                    if (!eddyProFourthGases().contains(norm))
                    {
                        newVar = QStringLiteral("n2o");
                        why = tr("EddyPro's fourth gas. The project carries this "
                                 "gas's molecular weight and diffusivity.");
                    }
                    else
                    {
                        newVar = norm;
                    }
                }
                else if (eddyProVariables().contains(var))
                {
                    // as it is
                }
                else if (eddyProVariables().contains(norm))
                {
                    newVar = norm;
                }
                else if (var == QLatin1String("AGC") || var == QLatin1String("RSSI")
                         || GasMetadata::findGas(var))
                {
                    newVar = QStringLiteral("ignore");
                    why = tr("EddyPro does not know this variable.");
                }
                if (newVar != var)
                {
                    ini.setValue(col + DlIni::INI_VARDESC_VAR, newVar);
                    //> Spelling only - case, or subscripts folded to ASCII -
                    //> is not a change the user needs to hear about.
                    if (newVar != norm)
                    {
                        change(variablesArea, tr("Column %1").arg(c), var, newVar, why);
                    }
                }

                // pmol/mol, which EddyPro has no unit for
                const auto pmol = QStringLiteral("pmol_mol");
                const auto ppb = QStringLiteral("ppb");
                auto conversion = ini.value(col + DlIni::INI_VARDESC_CONVERSION).toString();
                const auto unitIn = ini.value(col + DlIni::INI_VARDESC_UNIT_IN).toString();
                const auto unitOut = ini.value(col + DlIni::INI_VARDESC_UNIT_OUT).toString();
                const bool converted = conversion == QLatin1String("gain_offset")
                                       || conversion == QLatin1String("zero_fullscale");
                if (unitOut == pmol || (!converted && unitIn == pmol))
                {
                    double a = ini.value(col + DlIni::INI_VARDESC_A_VALUE, 1.0).toDouble();
                    double b = ini.value(col + DlIni::INI_VARDESC_B_VALUE, 0.0).toDouble();
                    if (!converted)
                    {
                        conversion = QStringLiteral("gain_offset");
                        a = 0.001;
                        b = 0.0;
                    }
                    else
                    {
                        a *= 0.001;
                        b *= 0.001;
                    }
                    ini.setValue(col + DlIni::INI_VARDESC_CONVERSION, conversion);
                    ini.setValue(col + DlIni::INI_VARDESC_A_VALUE, formatNumber(a));
                    ini.setValue(col + DlIni::INI_VARDESC_B_VALUE, formatNumber(b));
                    ini.setValue(col + DlIni::INI_VARDESC_UNIT_OUT, ppb);
                    if (unitIn == pmol)
                    {
                        ini.setValue(col + DlIni::INI_VARDESC_UNIT_IN, ppb);
                    }
                    change(variablesArea, tr("Column %1 (%2): unit").arg(c).arg(var),
                           tr("pmol/mol"), tr("ppb (nmol/mol)"),
                           tr("EddyPro has no pmol/mol. The values are scaled by 0.001 "
                              "through the column's gain and offset."));
                }
                else if (unitIn == pmol)
                {
                    ini.setValue(col + DlIni::INI_VARDESC_UNIT_IN, ppb);
                }

                // keys EddyPro lacks, and the pair it needs
                const QString errorKey = col + DlIni::INI_VARDESC_ERROR_VALUE;
                if (ini.contains(errorKey))
                {
                    const double ev = ini.value(errorKey).toDouble();
                    if (ev != -9999.0)
                    {
                        change(variablesArea, tr("Column %1 (%2): missing value").arg(c).arg(var),
                               QString::number(ev), tr("none"),
                               tr("EddyPro recognises only its own missing-value label."));
                    }
                    ini.remove(errorKey);
                }
                ini.remove(col + DlIni::INI_VARDESC_SPECTRO_A);
                ini.remove(col + DlIni::INI_VARDESC_SPECTRO_B);
                if (!ini.contains(col + DlIni::INI_VARDESC_MIN_VALUE))
                {
                    ini.setValue(col + DlIni::INI_VARDESC_MIN_VALUE, formatNumber(0.0));
                }
                if (!ini.contains(col + DlIni::INI_VARDESC_MAX_VALUE))
                {
                    ini.setValue(col + DlIni::INI_VARDESC_MAX_VALUE, formatNumber(0.0));
                }
            }
        }
        ini.endGroup();

        //> EddyPro describes five instruments. Beyond that, drop the last ones
        //> no exported column reads; renumbering the blocks is safe because
        //> columns name an instrument by its model id, not by its block.
        constexpr int kEddyProMaxInstruments = 5;
        QList<int> order = blocks.keys();
        while (order.size() > kEddyProMaxInstruments)
        {
            int victim = -1;
            for (int i = order.size() - 1; i >= 0; --i)
            {
                const auto id = blocks.value(order.at(i)).value(DlIni::INI_ANEM_2).toString();
                if (!referenced.contains(id)) { victim = i; break; }
            }
            if (victim < 0)
            {
                warnings_ << tr("The metadata describes %1 instruments and EddyPro "
                                "reads at most %2. Every one is used by a column, so "
                                "none was dropped.")
                                 .arg(order.size()).arg(kEddyProMaxInstruments);
                break;
            }
            change(instrumentsArea, tr("Instrument %1").arg(order.at(victim)),
                   nameOf(order.at(victim)), tr("not exported"),
                   tr("EddyPro describes at most five instruments, and no column "
                      "reads this one."));
            order.removeAt(victim);
        }

        ini.beginGroup(DlIni::INIGROUP_INSTRUMENTS);
        ini.remove(QString());
        int k = 1;
        for (const int old : order)
        {
            const auto prefix = QStringLiteral("instr_%1_").arg(k++);
            const auto& f = blocks.value(old);
            for (auto it = f.cbegin(); it != f.cend(); ++it)
            {
                ini.setValue(prefix + it.key(), it.value());
            }
        }
        ini.endGroup();
    });

    if (!ok && error)
    {
        *error = tr("Could not convert %1.").arg(QDir::toNativeSeparators(metadataFile_));
    }

    //> Far more columns than EddyPro reads leaves the ones past its limit
    //> invisible to it. Said, not fixed: renumbering columns would mean
    //> rewriting the raw files.
    for (int s = 0; s < 4; ++s)
    {
        if (slotColumn_[s] > 100)
        {
            warnings_ << tr("Column %1 is a gas column, and EddyPro reads at most "
                            "100 columns.").arg(slotColumn_[s]);
        }
    }
    return ok;
}

// ---- the report --------------------------------------------------------------

namespace {

QStringList areaOrder()
{
    return { EddyProExport::tr("Gases"), EddyProExport::tr("Instruments"),
             EddyProExport::tr("Variables"), EddyProExport::tr("Processing options") };
}

} // namespace

QString EddyProExport::reportHtml() const
{
    QString html;
    if (changes_.isEmpty())
    {
        html += QStringLiteral("<p>") + tr("Exported without changes.") + QStringLiteral("</p>");
    }

    QStringList areas = areaOrder();
    for (const auto& c : changes_)
    {
        if (!areas.contains(c.area)) { areas << c.area; }
    }
    for (const auto& area : areas)
    {
        QString items;
        for (const auto& c : changes_)
        {
            if (c.area != area) { continue; }
            items += QStringLiteral("<li><b>%1</b>: %2 &rarr; %3")
                         .arg(c.setting.toHtmlEscaped(), c.from.toHtmlEscaped(),
                              c.to.toHtmlEscaped());
            if (!c.note.isEmpty())
            {
                items += QStringLiteral("<br><i>%1</i>").arg(c.note.toHtmlEscaped());
            }
            items += QStringLiteral("</li>");
        }
        if (!items.isEmpty())
        {
            html += QStringLiteral("<b>%1</b><ul>%2</ul>").arg(area.toHtmlEscaped(), items);
        }
    }

    const auto list = [](const QString& title, const QStringList& lines)
    {
        if (lines.isEmpty()) { return QString(); }
        QString out = QStringLiteral("<b>%1</b><ul>").arg(title.toHtmlEscaped());
        for (const auto& l : lines)
        {
            out += QStringLiteral("<li>%1</li>").arg(l.toHtmlEscaped());
        }
        out += QStringLiteral("</ul>");
        return out;
    };
    html += list(tr("Warnings"), warnings_);
    html += list(tr("Notes"), notes_);
    return html;
}

QString EddyProExport::reportText() const
{
    QString text;
    QTextStream out(&text);
    out << tr("EddyPro export report") << '\n'
        << QStringLiteral("=").repeated(21) << "\n\n";

    if (changes_.isEmpty())
    {
        out << tr("Exported without changes.") << "\n\n";
    }

    QStringList areas = areaOrder();
    for (const auto& c : changes_)
    {
        if (!areas.contains(c.area)) { areas << c.area; }
    }
    for (const auto& area : areas)
    {
        bool any = false;
        for (const auto& c : changes_)
        {
            if (c.area != area) { continue; }
            if (!any) { out << area << '\n'; any = true; }
            out << QStringLiteral("  - %1: %2 -> %3").arg(c.setting, c.from, c.to) << '\n';
            if (!c.note.isEmpty()) { out << QStringLiteral("      %1").arg(c.note) << '\n'; }
        }
        if (any) { out << '\n'; }
    }

    const auto list = [&out](const QString& title, const QStringList& lines)
    {
        if (lines.isEmpty()) { return; }
        out << title << '\n';
        for (const auto& l : lines) { out << QStringLiteral("  - %1").arg(l) << '\n'; }
        out << '\n';
    };
    list(tr("Warnings"), warnings_);
    list(tr("Notes"), notes_);
    out.flush();
    return text;
}
