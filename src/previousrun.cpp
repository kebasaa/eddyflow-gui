/***************************************************************************
  previousrun.cpp
  ---------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#include "previousrun.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QRegularExpression>
#include <QSet>

#include "defs.h"

namespace {

using IniSection = QMap<QString, QString>;
using Ini = QMap<QString, IniSection>;

const QString ProjectGroup = QStringLiteral("Project");
const QString SpectraGroup = QStringLiteral("FluxCorrection_SpectralAnalysis_General");

//> Raw processing reads [Project] and the sections whose name starts with
//> this (ReadIniRP('RawProcess') in eddyflow-rp_main.f90, a prefix match in
//> StoreIniTags). Nothing else in the file reaches it.
const QString RawProcessPrefix = QStringLiteral("RawProcess");

//> [Project] keys that may differ between the record and the project now.
//> Everything else in [Project] must match: a key is safe only once it is
//> known that raw processing writes nothing from it into the files FCC reads.
//>
//> Bookkeeping: written by the GUI or the engine, describing the file
//> rather than the processing.
const QSet<QString>& bookkeepingKeys()
{
    static const QSet<QString> keys {
        QStringLiteral("creation_date"),
        QStringLiteral("last_change_date"),
        QStringLiteral("file_name"),
        QStringLiteral("run_mode"),
        QStringLiteral("run_fcc"),
        QStringLiteral("sw_version"),
        QStringLiteral("ini_version"),
        QStringLiteral("files_found"),
        QStringLiteral("bin_sp_avail"),
        QStringLiteral("full_sp_avail"),
    };
    return keys;
}

//> Read by raw processing, but used there only for what it computes when no
//> FCC follows - and a record with an ex file is from a run FCC followed:
//> - lf_meth, cosp_model, corr_iter_*, foot_meth: the spectral corrections
//>   and footprint RP skips when FCC follows (eddyflow-rp_main.f90:3107-3207);
//> - out_rich, out_metadata: forced off in RP when FCC follows (:371-372);
//> - qc_meth: in RP only whether qc_details is written, which FCC never reads;
//> - out_mean_cosp, out_mean_spec: make RP write binned spectra, which the
//>   spectra check below asks for whenever the current settings need them;
//> - test_pfd, sonic_output_rate, ru_spec_corr, hf_correct_ghg_*: not used by
//>   RP at all; fix_out_format: read by neither engine.
//> hf_meth is NOT here: RP writes it into the ex file as
//> SPECTRAL_CORRECTION_METHOD and FCC copies that field to its output as it
//> is (read_ex_record.f90), so a changed method would be labelled wrongly.
const QSet<QString>& correctionOnlyKeys()
{
    static const QSet<QString> keys {
        QStringLiteral("lf_meth"),
        QStringLiteral("cosp_model"),
        QStringLiteral("corr_iter_meth"),
        QStringLiteral("corr_iter_max"),
        QStringLiteral("corr_iter_tol"),
        QStringLiteral("foot_meth"),
        QStringLiteral("qc_meth"),
        QStringLiteral("out_rich"),
        QStringLiteral("out_metadata"),
        QStringLiteral("out_mean_cosp"),
        QStringLiteral("out_mean_spec"),
        QStringLiteral("test_pfd"),
        QStringLiteral("sonic_output_rate"),
        QStringLiteral("ru_spec_corr"),
        QStringLiteral("hf_correct_ghg_ba"),
        QStringLiteral("hf_correct_ghg_zoh"),
        QStringLiteral("fix_out_format"),
    };
    return keys;
}

//> The tag the engine stamps on every file of a run: start time and run mode
//> (init_env.f90, tag_run_mode.f90), followed by the extension.
const QRegularExpression& tagPattern()
{
    static const QRegularExpression re(
        QStringLiteral("_(\\d{4}-\\d{2}-\\d{2}T\\d{6}_(?:adv|exp|mdr|nul))(?=\\.)"));
    return re;
}

Ini readIni(const QString& path)
{
    Ini ini;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) { return ini; }

    QString section;
    while (!file.atEnd())
    {
        const auto line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char(';'))
            || line.startsWith(QLatin1Char('#')))
        {
            continue;
        }
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']')))
        {
            section = line.mid(1, line.size() - 2).trimmed();
            ini[section];
            continue;
        }
        const auto eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0) { continue; }
        ini[section].insert(line.left(eq).trimmed(), line.mid(eq + 1).trimmed());
    }
    return ini;
}

QString value(const Ini& ini, const QString& section, const QString& key)
{
    return ini.value(section).value(key);
}

int intValue(const Ini& ini, const QString& section, const QString& key)
{
    return value(ini, section, key).toInt();
}

bool samePath(const QString& a, const QString& b)
{
    const auto ca = QDir::cleanPath(QDir::fromNativeSeparators(a));
    const auto cb = QDir::cleanPath(QDir::fromNativeSeparators(b));
#if defined(Q_OS_WIN)
    return ca.compare(cb, Qt::CaseInsensitive) == 0;
#else
    return ca == cb;
#endif
}

//> A readable name for a setting raw processing depends on. Keys that come
//> in families (one per gas, per column) share one name, so a list of what
//> changed stays short.
QString settingLabel(const QString& section, const QString& key)
{
    if (section == ProjectGroup)
    {
        static const QHash<QString, QString> exact {
            { QStringLiteral("ru_meth"), PreviousRun::tr("random uncertainty method (ru_meth)") },
            { QStringLiteral("ru_its_meth"), PreviousRun::tr("random uncertainty integral time scale (ru_its_meth)") },
            { QStringLiteral("ru_tlag_max"), PreviousRun::tr("random uncertainty maximum time lag (ru_tlag_max)") },
            { QStringLiteral("lod_meth"), PreviousRun::tr("limit of detection (lod_meth)") },
            { QStringLiteral("lod_alpha"), PreviousRun::tr("limit of detection confidence (lod_alpha)") },
            { QStringLiteral("wpl_meth"), PreviousRun::tr("WPL terms (wpl_meth)") },
            { QStringLiteral("hf_meth"), PreviousRun::tr("spectral correction method (hf_meth)") },
            { QStringLiteral("err_label"), PreviousRun::tr("missing-value label (err_label)") },
            { QStringLiteral("make_dataset"), PreviousRun::tr("continuous dataset (make_dataset)") },
            { QStringLiteral("out_biomet"), PreviousRun::tr("biomet output (out_biomet)") },
            { QStringLiteral("out_path"), PreviousRun::tr("output folder (out_path)") },
            { QStringLiteral("project_id"), PreviousRun::tr("project ID (project_id)") },
            { QStringLiteral("project_title"), PreviousRun::tr("project name (project_title)") },
            { QStringLiteral("file_type"), PreviousRun::tr("raw file type (file_type)") },
            { QStringLiteral("file_prototype"), PreviousRun::tr("raw file name format (file_prototype)") },
            { QStringLiteral("use_pfile"), PreviousRun::tr("metadata file (use_pfile)") },
            { QStringLiteral("proj_file"), PreviousRun::tr("metadata file (proj_file)") },
            { QStringLiteral("use_dyn_md_file"), PreviousRun::tr("dynamic metadata file (use_dyn_md_file)") },
            { QStringLiteral("dyn_metadata_file"), PreviousRun::tr("dynamic metadata file (dyn_metadata_file)") },
            { QStringLiteral("master_sonic"), PreviousRun::tr("master anemometer (master_sonic)") },
            { QStringLiteral("use_biom"), PreviousRun::tr("biomet data (use_biom)") },
            { QStringLiteral("tob1_format"), PreviousRun::tr("TOB1 format (tob1_format)") },
        };
        const auto it = exact.constFind(key);
        if (it != exact.constEnd()) { return it.value(); }

        static const QList<QPair<QString, QString>> families {
            { QStringLiteral("pr_"), PreviousRun::tr("processing period (Basic Settings)") },
            { QStringLiteral("gas_"), PreviousRun::tr("gas records (Basic Settings)") },
            { QStringLiteral("cell_"), PreviousRun::tr("cell temperature and pressure records (Basic Settings)") },
            { QStringLiteral("diag_"), PreviousRun::tr("diagnostic records (Basic Settings)") },
            { QStringLiteral("agc_"), PreviousRun::tr("signal strength records (Basic Settings)") },
            { QStringLiteral("col_"), PreviousRun::tr("column assignments (Basic Settings)") },
            { QStringLiteral("biom_"), PreviousRun::tr("biomet data (Project Creation)") },
            { QStringLiteral("binary_"), PreviousRun::tr("binary file settings (Project Creation)") },
            { QStringLiteral("cec_"), PreviousRun::tr("CEC partitioning settings") },
            { QStringLiteral("fluxnet_"), PreviousRun::tr("FLUXNET output options") },
            { QStringLiteral("ru_"), PreviousRun::tr("random uncertainty settings") },
            { QStringLiteral("lod_"), PreviousRun::tr("limit of detection settings") },
        };
        for (const auto& family : families)
        {
            if (key.startsWith(family.first)) { return family.second; }
        }
        return key;
    }

    static const QHash<QString, QString> sections {
        { QStringLiteral("RawProcess_General"), PreviousRun::tr("Basic Settings") },
        { QStringLiteral("RawProcess_Settings"), PreviousRun::tr("raw processing options") },
        { QStringLiteral("RawProcess_Tests"), PreviousRun::tr("statistical tests") },
        { QStringLiteral("RawProcess_ParameterSettings"), PreviousRun::tr("statistical test parameters") },
        { QStringLiteral("RawProcess_TiltCorrection_Settings"), PreviousRun::tr("planar fit settings") },
        { QStringLiteral("RawProcess_TimelagOptimization_Settings"), PreviousRun::tr("time lag optimization settings") },
        { QStringLiteral("RawProcess_PWBTimelag_Settings"), PreviousRun::tr("PWB time lag settings") },
        { QStringLiteral("RawProcess_BiometMeasurements"), PreviousRun::tr("biomet measurements") },
        { QStringLiteral("RawProcess_WindDirectionFilter"), PreviousRun::tr("wind direction filter") },
    };
    const auto name = sections.value(section, section);
    return QStringLiteral("%1 (%2)").arg(name, key);
}

//> What changed between `record` and `current` in what raw processing reads.
QStringList rawProcessingChanges(const Ini& record, const Ini& current)
{
    QStringList labels;
    auto note = [&labels](const QString& label)
    {
        if (!labels.contains(label)) { labels << label; }
    };

    QSet<QString> sections;
    for (auto it = record.constBegin(); it != record.constEnd(); ++it) { sections << it.key(); }
    for (auto it = current.constBegin(); it != current.constEnd(); ++it) { sections << it.key(); }

    QStringList ordered(sections.cbegin(), sections.cend());
    ordered.sort();
    //> [Project] first: what people change most is there.
    if (ordered.removeOne(ProjectGroup)) { ordered.prepend(ProjectGroup); }

    for (const auto& section : std::as_const(ordered))
    {
        const bool project = section == ProjectGroup;
        if (!project && !section.startsWith(RawProcessPrefix)) { continue; }

        const auto a = record.value(section);
        const auto b = current.value(section);
        QStringList keys = a.keys();
        for (const auto& k : b.keys())
        {
            if (!a.contains(k)) { keys << k; }
        }
        for (const auto& key : std::as_const(keys))
        {
            if (project && (bookkeepingKeys().contains(key)
                            || correctionOnlyKeys().contains(key)))
            {
                continue;
            }
            //> A key on one side only counts as a change: the refusal is the
            //> safe answer when it is not known what an absent key meant.
            if (a.contains(key) != b.contains(key) || a.value(key) != b.value(key))
            {
                note(settingLabel(section, key));
            }
        }
    }
    return labels;
}

struct Entry
{
    QString path;
    QString name;
    QString tag;
    qint64 size = 0;
    //> -1 for the output folder itself, else its index in engineSubFolders().
    int folder = -1;
};

int subFolderIndex(const QString& name)
{
    return int(PreviousRun::engineSubFolders().indexOf(name));
}

//> Every file in the output folder and the engine's sub-folders that carries
//> a run tag. One listing, read many times below. File by file rather than
//> a whole folder at once, so that progress can be shown and the scan
//> stopped: one cospectra folder alone holds a file per averaging period.
QList<Entry> listTagged(const QString& outPath, PreviousRun::Progress* progress)
{
    QList<Entry> entries;
    const auto& subs = PreviousRun::engineSubFolders();
    for (int i = -1; i < subs.size(); ++i)
    {
        if (progress)
        {
            if (progress->cancelled) { break; }
            progress->folder = i;
        }
        const auto d = i < 0 ? outPath : outPath + QLatin1Char('/') + subs.at(i);
        QDirIterator it(d, QDir::Files | QDir::Hidden | QDir::NoSymLinks);
        while (it.hasNext())
        {
            it.next();
            if (progress)
            {
                ++progress->files;
                if (progress->cancelled) { break; }
            }
            const auto info = it.fileInfo();
            const auto tag = PreviousRun::runTag(info.fileName());
            if (tag.isEmpty()) { continue; }
            entries << Entry { info.absoluteFilePath(), info.fileName(), tag, info.size(), i };
        }
    }
    return entries;
}

//> Each file is listed once, so no check for one already added: a contains()
//> here made the scan quadratic in the tens of thousands of cospectra files.
void add(PreviousRun::Files& files, const Entry& e)
{
    files.paths << e.path;
    files.bytes += e.size;
}

struct Record
{
    QString path;
    QString tag;
    QString exFile;
    QString projectId;
    bool rawProcessing = false;
};

//> Spectra a correction-only run reads, judged for the current settings.
//> Empty when they are there; otherwise why not. Spectra in the output
//> folder are judged from `entries`, the listing already made; a folder the
//> project points to elsewhere only needs to hold a file.
QString spectraProblem(const QString& what,
                       const QString& dir,
                       bool userProvided,
                       const QString& rpTag,
                       const QList<Entry>& entries,
                       int folder)
{
    if (userProvided)
    {
        QDirIterator it(dir, QDir::Files);
        if (!it.hasNext())
        {
            return PreviousRun::tr("The %1 the project points to are not there (%2).")
                       .arg(what, QDir::toNativeSeparators(dir));
        }
        return QString();
    }

    bool ours = false;
    bool others = false;
    for (const auto& e : entries)
    {
        if (e.folder != folder) { continue; }
        if (e.tag == rpTag) { ours = true; }
        else { others = true; }
    }
    if (!ours)
    {
        return PreviousRun::tr("The raw processing did not write the %1 the current "
                               "settings need.").arg(what);
    }
    //> FCC reads every file in the folder and goes by the half-hour in the
    //> name, so another run's files would be read as more of the same.
    if (others)
    {
        return PreviousRun::tr("The %1 folder also holds files from another run, "
                               "which the flux correction would read as well. A "
                               "full re-run with \"Delete and run\" clears them.")
                   .arg(what);
    }
    return QString();
}

} // namespace

const QStringList& PreviousRun::engineSubFolders()
{
    //> The engine's names: m_common_global_var.f90 (binned and full
    //> cospectra), m_rp_global_var.f90 (stats, raw datasets, user stats,
    //> ogives), m_fx_global_var_mod.f90 (spectral analysis).
    static const QStringList folders {
        QStringLiteral("eddyflow_binned_cospectra"),
        QStringLiteral("eddyflow_full_cospectra"),
        QStringLiteral("eddyflow_binned_ogives"),
        QStringLiteral("eddyflow_spectral_analysis"),
        QStringLiteral("eddyflow_stats"),
        QStringLiteral("eddyflow_user_stats"),
        QStringLiteral("eddyflow_raw_datasets"),
    };
    return folders;
}

QString PreviousRun::runTag(const QString& fileName)
{
    QString tag;
    auto it = tagPattern().globalMatch(fileName);
    while (it.hasNext()) { tag = it.next().captured(1); }
    return tag;
}

QString PreviousRun::rawProcessingRecordFor(const QString& exFile)
{
    const auto tag = runTag(QFileInfo(exFile).fileName());
    if (tag.isEmpty()) { return QString(); }
    return QFileInfo(exFile).absolutePath()
           + QStringLiteral("/processing_") + tag + QStringLiteral(".")
           + Defs::PROJECT_FILE_EXT;
}

bool PreviousRun::isCurrentExHeader(const QByteArray& header)
{
    //> JudgeExHeader, eddyflow-fcc_main.f90: two markers every file must
    //> carry, and two that a file must carry once it has the block they
    //> belong to.
    if (!header.contains("NUM_WATER_FLUX")
        || !header.contains("H2O_BIOMET_MOLE_FRACTION"))
    {
        return false;
    }
    if (header.contains("CEC_METH") && !header.contains("CEC_NS_")) { return false; }
    if (header.contains("_MOIST_SLOT") && !header.contains("_MOIST_REF")) { return false; }
    return true;
}

PreviousRun::Scan PreviousRun::scan(const QString& currentProject, bool fccNeeded,
                                    Progress* progress)
{
    Scan result;

    const auto current = readIni(currentProject);
    const auto outPath = QDir::cleanPath(QDir::fromNativeSeparators(
                             value(current, ProjectGroup, QStringLiteral("out_path"))));
    const bool keepOn = value(current, SpectraGroup,
                              QStringLiteral("keep_parent_fluxnet_file")) != QStringLiteral("0");
    const auto keepHint = tr("Turn on Advanced Settings > Output Files > \"Keep "
                             "raw-processing results for a faster correction-only "
                             "re-run\": after the next full run, a change to "
                             "flux-correction settings alone can then re-run only "
                             "the flux correction.");

    if (outPath.isEmpty() || !QDir(outPath).exists())
    {
        result.reasons << tr("No previous run was found in the output folder.");
        return result;
    }

    const auto projectId = value(current, ProjectGroup, QStringLiteral("project_id"));
    const auto prefix = (projectId.startsWith(QLatin1Char('_'))
                            ? QStringLiteral("eddyflow") : QStringLiteral("eddyflow_"))
                        + projectId + QLatin1Char('_');

    //> The records of this project's runs.
    QList<Record> records;
    const auto recordInfos = QDir(outPath).entryInfoList(
        { QStringLiteral("processing_*.") + Defs::PROJECT_FILE_EXT }, QDir::Files);
    for (const auto& info : recordInfos)
    {
        Record r;
        r.path = info.absoluteFilePath();
        r.tag = runTag(info.fileName());
        if (r.tag.isEmpty()) { continue; }
        const auto ini = readIni(r.path);
        r.projectId = value(ini, ProjectGroup, QStringLiteral("project_id"));
        if (r.projectId != projectId) { continue; }
        r.exFile = value(ini, SpectraGroup, QStringLiteral("ex_file"));
        r.rawProcessing = !r.exFile.isEmpty()
                          && runTag(QFileInfo(r.exFile).fileName()) == r.tag;
        records << r;
    }

    //> Everything earlier runs of this project wrote: the tags on its own
    //> files and records, then every file under those tags, the engine's
    //> sub-folders included.
    const auto entries = listTagged(outPath, progress);
    if (progress && progress->cancelled) { return result; }
    QSet<QString> projectTags;
    for (const auto& e : entries)
    {
        if (e.folder < 0 && e.name.startsWith(prefix)) { projectTags << e.tag; }
    }
    for (const auto& r : std::as_const(records)) { projectTags << r.tag; }
    //> The cospectra folders whatever the tag: FCC reads them whole, so a
    //> file left there by any other run would be read with the new ones.
    const int binnedFolder = subFolderIndex(Defs::OUT_BINNED_COSPECTRA_DIR.mid(1));
    const int fullFolder = subFolderIndex(Defs::OUT_FULL_COSPECTRA_DIR.mid(1));
    for (const auto& e : entries)
    {
        if (projectTags.contains(e.tag) || e.folder == binnedFolder || e.folder == fullFolder)
        {
            add(result.allOutputs, e);
        }
    }

    if (result.allOutputs.isEmpty() && records.isEmpty())
    {
        result.reasons << tr("No previous run was found in the output folder.");
        return result;
    }

    //> The newest Advanced record that names a raw-processing file. An older
    //> one is not a fallback: its cospectra would sit beside the newer run's.
    const Record* record = nullptr;
    for (const auto& r : std::as_const(records))
    {
        if (!r.tag.endsWith(QStringLiteral("_adv")) || r.exFile.isEmpty()) { continue; }
        if (!record || r.tag > record->tag) { record = &r; }
    }

    if (!fccNeeded)
    {
        result.reasons << tr("The current settings do not need the flux-correction "
                             "step: no in-situ spectral correction (Horst, Ibrom, "
                             "Fratini) and no ensemble-averaged spectra or cospectra "
                             "output.");
    }

    const auto keepNow = tr("\"Keep raw-processing results\" is on now, so the "
                            "next full run keeps them.");

    if (!record)
    {
        result.reasons << tr("No earlier Advanced run in the output folder kept a "
                             "record of its raw processing.");
        result.reasons << (keepOn ? keepNow : keepHint);
        return result;
    }

    result.recordFile = record->path;
    result.exFile = QDir::fromNativeSeparators(record->exFile);
    const auto rpTag = runTag(QFileInfo(result.exFile).fileName());
    result.rawProcessingTime = QDateTime::fromString(rpTag.left(17),
                                                     QStringLiteral("yyyy-MM-ddTHHmmss"));

    bool exUsable = QFileInfo::exists(result.exFile) && !rpTag.isEmpty();
    if (!exUsable)
    {
        result.reasons << tr("The previous run did not keep its raw-processing "
                             "results (%1).").arg(QFileInfo(result.exFile).fileName());
    }
    else
    {
        QFile ex(result.exFile);
        QByteArray header;
        if (ex.open(QIODevice::ReadOnly)) { header = ex.readLine(); }
        if (!isCurrentExHeader(header))
        {
            exUsable = false;
            result.reasons << tr("The raw-processing results were written by an "
                                 "earlier EddyFlow version, which this version's "
                                 "flux correction cannot read.");
        }
    }
    //> The comparison runs whether or not the file is there: it is the other
    //> half of the answer, and both halves are worth knowing at once.
    const auto changes = rawProcessingChanges(readIni(record->path), current);
    if (!changes.isEmpty())
    {
        constexpr int shown = 8;
        auto list = QStringList(changes.mid(0, shown)).join(QStringLiteral("; "));
        if (changes.size() > shown)
        {
            list += tr("; and %n more", nullptr, int(changes.size() - shown));
        }
        result.reasons << tr("Settings that raw processing depends on have changed "
                             "since it ran: %1. Changing them needs a full re-run.")
                              .arg(list);
    }

    if (exUsable)
    {
        const int hf = intValue(current, ProjectGroup, QStringLiteral("hf_meth"));
        const bool inSitu = hf == 2 || hf == 3 || hf == 4;
        const int saMode = intValue(current, SpectraGroup, QStringLiteral("sa_mode"));
        const bool meanSpectra = intValue(current, ProjectGroup, QStringLiteral("out_mean_cosp")) == 1
                                 || intValue(current, ProjectGroup, QStringLiteral("out_mean_spec")) == 1;

        //> The same tests as read_ini_fcc.f90: binned spectra for an in-situ
        //> method that assesses them now or for the ensemble outputs, full
        //> w/Ts cospectra for Fratini, and the assessment file otherwise.
        QStringList problems;
        if ((inSitu && saMode != 0) || meanSpectra)
        {
            const bool user = intValue(current, ProjectGroup, QStringLiteral("bin_sp_avail")) == 1;
            const auto dir = user ? value(current, SpectraGroup, QStringLiteral("sa_bin_spectra"))
                                  : outPath + Defs::OUT_BINNED_COSPECTRA_DIR;
            problems << spectraProblem(tr("binned cospectra"), dir, user, rpTag,
                                       entries, binnedFolder);
        }
        if (hf == 4)
        {
            const bool user = intValue(current, ProjectGroup, QStringLiteral("full_sp_avail")) == 1;
            const auto dir = user ? value(current, SpectraGroup, QStringLiteral("sa_full_spectra"))
                                  : outPath + Defs::OUT_FULL_COSPECTRA_DIR;
            problems << spectraProblem(tr("full cospectra"), dir, user, rpTag,
                                       entries, fullFolder);
        }
        if (inSitu && saMode == 0)
        {
            const auto saFile = value(current, SpectraGroup, QStringLiteral("sa_file"));
            if (saFile.isEmpty() || !QFileInfo::exists(saFile))
            {
                problems << tr("The spectral assessment file the project points to "
                               "is not there.");
            }
        }
        problems.removeAll(QString());
        result.reasons << problems;
    }

    result.correctionOnly = exUsable && fccNeeded && result.reasons.isEmpty();

    //> How to get there next time. Not a reason of its own: with the file
    //> still there, the setting being off now does not stop this run.
    if (!result.correctionOnly)
    {
        if (!exUsable) { result.reasons << (keepOn ? keepNow : keepHint); }
        else if (!keepOn) { result.reasons << keepHint; }
    }

    //> Earlier correction passes over the same raw processing. Never the ex
    //> file itself, which carries raw processing's tag anyway.
    QSet<QString> correctionTags;
    for (const auto& r : std::as_const(records))
    {
        if (!r.rawProcessing && !r.exFile.isEmpty() && samePath(r.exFile, result.exFile))
        {
            correctionTags << r.tag;
        }
    }
    for (const auto& e : entries)
    {
        if (correctionTags.contains(e.tag) && !samePath(e.path, result.exFile))
        {
            add(result.correctionOutputs, e);
        }
    }

    return result;
}
