/***************************************************************************
  ecproject.cpp
  -------------------
  Copyright © 2007-2011, Eco2s team, Antonio Forgione
  Copyright © 2011-2018, LI-COR Biosciences, Antonio Forgione
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

#include "ecproject.h"

#include <algorithm>
#include <cmath>

#include <QDebug>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QTemporaryFile>

#include "dlproject.h"
#include "ecinidefs.h"
#include "fileutils.h"
#include "gas_metadata.h"
#include "mainwindow.h"
#include "stringutils.h"
#include "widget_utils.h"

namespace {

//> The keys EddyPro labels for nitrous oxide, and what this program calls
//> them. The fourth slot takes whatever species the site measured, so the
//> keys are named for the slot rather than for one gas.
//>
//> Only these nine. Every other fourth-slot key - sa_*_gas4, out_raw_gas4,
//> to_gas4_* - is already spelled gas4 on both sides; a package written by
//> EddyPro 7.0.9 is the authority for which is which. One list, read forwards
//> on import and backwards on export, so the two directions cannot drift.
const QVector<QPair<QString, QString>>& fourthGasKeyRenames()
{
    static const QVector<QPair<QString, QString>> renames = {
        { QStringLiteral("col_n2o"),             QStringLiteral("col_gas4") },
        { QStringLiteral("out_full_sp_n2o"),     QStringLiteral("out_full_sp_gas4") },
        { QStringLiteral("out_full_cosp_w_n2o"), QStringLiteral("out_full_cosp_w_gas4") },
        { QStringLiteral("sr_lim_n2o"),          QStringLiteral("sr_lim_gas4") },
        { QStringLiteral("ds_hf_n2o"),           QStringLiteral("ds_hf_gas4") },
        { QStringLiteral("ds_sf_n2o"),           QStringLiteral("ds_sf_gas4") },
        { QStringLiteral("al_n2o_min"),          QStringLiteral("al_gas4_min") },
        { QStringLiteral("al_n2o_max"),          QStringLiteral("al_gas4_max") },
        { QStringLiteral("tl_def_n2o"),          QStringLiteral("tl_def_gas4") },
    };
    return renames;
}

//> How many gas slots EddyPro provides for. The four record positions this
//> program pins to CO2, H2O, CH4 and the open slot are exactly these, which is
//> what makes the export a slot-for-slot mapping rather than a search.
const int kEddyProGasSlots = 4;

//> Remove every key in the current group matching \a pattern.
void removeMatchingKeys(QSettings& ini, const QString& pattern)
{
    const auto doomed = ini.childKeys().filter(QRegularExpression(pattern));
    for (const auto& key : doomed) { ini.remove(key); }
}

QString formatSpectraQcMinimum(double value)
{
    const double roundedToFour = std::round(value * 10000.0) / 10000.0;
    const bool needsExtraPrecision = value != 0.0
            && (std::abs(value) < 0.0001
                || std::abs(value - roundedToFour) > 0.0000005);

    QString text = QString::number(value, 'f', needsExtraPrecision ? 6 : 4);
    if (needsExtraPrecision)
    {
        while (text.contains(QLatin1Char('.')) && text.endsWith(QLatin1Char('0')))
        {
            text.chop(1);
        }
        if (text.endsWith(QLatin1Char('.')))
        {
            text.chop(1);
        }
    }
    return text;
}

} // namespace

EcProject::EcProject(QObject *parent, const ProjConfigState& project_config) :
    QObject(parent),
    defaultSettings(EcProjectState()),
    modified_(false),
    ec_project_state_(EcProjectState()),
    project_config_state_(project_config)
{
    Defs::qt_registerCustomTypes();
}

EcProject::EcProject(const EcProject& project) :
    QObject(nullptr),
    defaultSettings(EcProjectState()),
    modified_(project.modified_),
    ec_project_state_(project.ec_project_state_),
    project_config_state_(project.project_config_state_)
{
}

EcProject& EcProject::operator=(const EcProject &project)
{
    if (this != &project)
    {
        defaultSettings = project.defaultSettings;
        modified_ = project.modified_;
        ec_project_state_ = project.ec_project_state_;
        project_config_state_ = project.project_config_state_;
    }
    return *this;
}

// destructor
EcProject::~EcProject()
{ ; }

bool EcProject::previousSettingsCompare(bool current, bool previous)
{
    if (previous)
    {
        return true;
    }
    else
    {
        if (current)
            return false;
        else
            return true;
    }
}

/// Whether two gas record sets describe the same measurements.
///
/// **Order-insensitive**, which is the point: the records are a set, and a
/// site that adds a fifth gas has not changed what it says about the first
/// four. Comparing them positionally would invalidate previously processed
/// data for every gas whenever one was added or removed.
///
/// Replaces the flat col_co2/col_h2o/col_ch4 conjunction and the fourth-gas
/// special case that went with it.
bool EcProject::gasRecordsCompare(const QVector<GasRecord>& current,
                                  const QVector<GasRecord>& previous)
{
    //> A record with no column is a slot kept for the engine's
    //> record-to-slot mapping, not a measurement, and does not count.
    const auto measured = [](const QVector<GasRecord>& recs)
    {
        QStringList keys;
        for (const auto& rec : recs)
        {
            if (rec.rawColumn <= 0) { continue; }
            //> Molecular weight and diffusivity are part of the identity:
            //> reprocessing with a different diffusivity gives different
            //> fluxes for the same column.
            keys << QStringLiteral("%1|%2|%3|%4|%5")
                        .arg(rec.slug, rec.instrumentId)
                        .arg(rec.rawColumn)
                        .arg(rec.mw, 0, 'f', 4)
                        .arg(rec.diff, 0, 'f', 5);
        }
        keys.sort();
        return keys;
    };

    return measured(current) == measured(previous);
}

/// The same, for cell and diagnostic records, which carry no overrides.
///
/// This also retires a copy/paste bug the flat comparison carried: it tested
/// col_diag_75 against the previous project's col_diag_77 and never compared
/// col_diag_77 against itself, so a changed diagnostic column could go
/// undetected and stale results be reused.
bool EcProject::plainRecordsCompare(const QVector<MeasurementRecord>& current,
                                    const QVector<MeasurementRecord>& previous)
{
    const auto measured = [](const QVector<MeasurementRecord>& recs)
    {
        QStringList keys;
        for (const auto& rec : recs)
        {
            if (rec.rawColumn <= 0) { continue; }
            keys << QStringLiteral("%1|%2|%3")
                        .arg(rec.slug, rec.instrumentId).arg(rec.rawColumn);
        }
        keys.sort();
        return keys;
    };

    return measured(current) == measured(previous);
}

bool EcProject::previousFileNameCompare(const QString& currentPath, const QString& previousPath)
{
    QFileInfo currentFilename(currentPath);
    QFileInfo previousFilename(previousPath);
    return (currentFilename.fileName() == previousFilename.fileName());
}

// true if current range is a subrange of the previous one
bool EcProject::compareDates(const QString& currStartDate, const QString& prevStartDate,
                             const QString& currStartTime, const QString& prevStartTime,
                             const QString& currEndDate, const QString& prevEndDate,
                             const QString& currEndTime, const QString& prevEndTime)
{
    QDate cStartDate = QDate::fromString(currStartDate, Qt::ISODate);
    QDate pStartDate = QDate::fromString(prevStartDate, Qt::ISODate);
    QTime cStartTime = QTime::fromString(currStartTime, QStringLiteral("hh:mm"));
    QTime pStartTime = QTime::fromString(prevStartTime, QStringLiteral("hh:mm"));
    QDate cEndDate = QDate::fromString(currEndDate, Qt::ISODate);
    QDate pEndDate = QDate::fromString(prevEndDate, Qt::ISODate);
    QTime cEndTime = QTime::fromString(currEndTime, QStringLiteral("hh:mm"));
    QTime pEndTime = QTime::fromString(prevEndTime, QStringLiteral("hh:mm"));

    bool test_1 = (cStartDate > pStartDate);
    bool test_2 = (cStartDate == pStartDate);
    bool test_3 = true;
    if (test_2)
        test_3 = (cStartTime >= pStartTime);
    bool startTest = (test_1 || test_2) && test_3;

    bool test_4 = (cEndDate < pEndDate);
    bool test_5 = (cEndDate == pEndDate);
    bool test_6 = true;
    if (test_5)
        test_6 = (cEndTime <= pEndTime);
    bool endTest = (test_4 || test_5) && test_6;

    return (startTest && endTest);
}

bool EcProject::fuzzyCompare(const EcProject& previousProject)
{
    bool dataSetTest = (ec_project_state_.projectGeneral.file_type == previousProject.ec_project_state_.projectGeneral.file_type)
        && (ec_project_state_.projectGeneral.file_prototype == previousProject.ec_project_state_.projectGeneral.file_prototype)
        && (ec_project_state_.projectGeneral.col_ts == previousProject.ec_project_state_.projectGeneral.col_ts)
        //> Gases, cell measurements and diagnostics compare as record sets.
        //> The flat col_* conjunction they replace could only describe one
        //> column per role, and compared the fourth gas through a special
        //> case that no longer has a fourth gas to be special about.
        && gasRecordsCompare(ec_project_state_.projectGeneral.gasColumns,
                             previousProject.ec_project_state_.projectGeneral.gasColumns)
        && plainRecordsCompare(ec_project_state_.projectGeneral.cellColumns,
                               previousProject.ec_project_state_.projectGeneral.cellColumns)
        && plainRecordsCompare(ec_project_state_.projectGeneral.diagColumns,
                               previousProject.ec_project_state_.projectGeneral.diagColumns)
        //> Signal strength too: it screens samples out of the conditional
        //> eddy covariance partition, so a project that gained or moved one
        //> does not produce what the previous run produced.
        && plainRecordsCompare(ec_project_state_.projectGeneral.agcColumns,
                               previousProject.ec_project_state_.projectGeneral.agcColumns)
        && (ec_project_state_.projectGeneral.col_air_t == previousProject.ec_project_state_.projectGeneral.col_air_t)
        && (ec_project_state_.projectGeneral.col_air_p == previousProject.ec_project_state_.projectGeneral.col_air_p);

    if (ec_project_state_.projectGeneral.subset)
    {
        dataSetTest = dataSetTest
            && compareDates(ec_project_state_.projectGeneral.start_date, previousProject.ec_project_state_.projectGeneral.start_date,
                ec_project_state_.projectGeneral.start_time, previousProject.ec_project_state_.projectGeneral.start_time,
                ec_project_state_.projectGeneral.end_date, previousProject.ec_project_state_.projectGeneral.end_date,
                ec_project_state_.projectGeneral.end_time, previousProject.ec_project_state_.projectGeneral.end_time);
    }
    else
    {
        dataSetTest = dataSetTest
            && (ec_project_state_.projectGeneral.files_found == previousProject.ec_project_state_.projectGeneral.files_found);
    }

    dataSetTest = dataSetTest
        && previousSettingsCompare(ec_project_state_.screenGeneral.recurse, previousProject.ec_project_state_.screenGeneral.recurse);
    dataSetTest = dataSetTest && (ec_project_state_.screenGeneral.flag1_col == previousProject.ec_project_state_.screenGeneral.flag1_col);
    dataSetTest = dataSetTest && qFuzzyCompare(ec_project_state_.screenGeneral.flag1_threshold, previousProject.ec_project_state_.screenGeneral.flag1_threshold);
    dataSetTest = dataSetTest && (ec_project_state_.screenGeneral.flag1_policy == previousProject.ec_project_state_.screenGeneral.flag1_policy);
    dataSetTest = dataSetTest && (ec_project_state_.screenGeneral.flag2_col == previousProject.ec_project_state_.screenGeneral.flag2_col);
    dataSetTest = dataSetTest && qFuzzyCompare(ec_project_state_.screenGeneral.flag2_threshold, previousProject.ec_project_state_.screenGeneral.flag2_threshold);
    dataSetTest = dataSetTest && (ec_project_state_.screenGeneral.flag2_policy == previousProject.ec_project_state_.screenGeneral.flag2_policy);
    dataSetTest = dataSetTest && (ec_project_state_.screenGeneral.flag3_col == previousProject.ec_project_state_.screenGeneral.flag3_col);
    dataSetTest = dataSetTest && qFuzzyCompare(ec_project_state_.screenGeneral.flag3_threshold, previousProject.ec_project_state_.screenGeneral.flag3_threshold);
    dataSetTest = dataSetTest && (ec_project_state_.screenGeneral.flag3_policy == previousProject.ec_project_state_.screenGeneral.flag3_policy);
    dataSetTest = dataSetTest && (ec_project_state_.screenGeneral.flag4_col == previousProject.ec_project_state_.screenGeneral.flag4_col);
    dataSetTest = dataSetTest && qFuzzyCompare(ec_project_state_.screenGeneral.flag4_threshold, previousProject.ec_project_state_.screenGeneral.flag4_threshold);
    dataSetTest = dataSetTest && (ec_project_state_.screenGeneral.flag4_policy == previousProject.ec_project_state_.screenGeneral.flag4_policy);
    dataSetTest = dataSetTest && (ec_project_state_.screenGeneral.flag5_col == previousProject.ec_project_state_.screenGeneral.flag5_col);
    dataSetTest = dataSetTest && qFuzzyCompare(ec_project_state_.screenGeneral.flag5_threshold, previousProject.ec_project_state_.screenGeneral.flag5_threshold);
    dataSetTest = dataSetTest && (ec_project_state_.screenGeneral.flag5_policy == previousProject.ec_project_state_.screenGeneral.flag5_policy);
    dataSetTest = dataSetTest && (ec_project_state_.screenGeneral.flag6_col == previousProject.ec_project_state_.screenGeneral.flag6_col);
    dataSetTest = dataSetTest && qFuzzyCompare(ec_project_state_.screenGeneral.flag6_threshold, previousProject.ec_project_state_.screenGeneral.flag6_threshold);
    dataSetTest = dataSetTest && (ec_project_state_.screenGeneral.flag6_policy == previousProject.ec_project_state_.screenGeneral.flag6_policy);
    dataSetTest = dataSetTest && (ec_project_state_.screenGeneral.flag7_col == previousProject.ec_project_state_.screenGeneral.flag7_col);
    dataSetTest = dataSetTest && qFuzzyCompare(ec_project_state_.screenGeneral.flag7_threshold, previousProject.ec_project_state_.screenGeneral.flag7_threshold);
    dataSetTest = dataSetTest && (ec_project_state_.screenGeneral.flag7_policy == previousProject.ec_project_state_.screenGeneral.flag7_policy);
    dataSetTest = dataSetTest && (ec_project_state_.screenGeneral.flag8_col == previousProject.ec_project_state_.screenGeneral.flag8_col);
    dataSetTest = dataSetTest && qFuzzyCompare(ec_project_state_.screenGeneral.flag8_threshold, previousProject.ec_project_state_.screenGeneral.flag8_threshold);
    dataSetTest = dataSetTest && (ec_project_state_.screenGeneral.flag8_policy == previousProject.ec_project_state_.screenGeneral.flag8_policy);
    dataSetTest = dataSetTest && (ec_project_state_.screenGeneral.flag9_col == previousProject.ec_project_state_.screenGeneral.flag9_col);
    dataSetTest = dataSetTest && qFuzzyCompare(ec_project_state_.screenGeneral.flag9_threshold, previousProject.ec_project_state_.screenGeneral.flag9_threshold);
    dataSetTest = dataSetTest && (ec_project_state_.screenGeneral.flag9_policy == previousProject.ec_project_state_.screenGeneral.flag9_policy);
    dataSetTest = dataSetTest && (ec_project_state_.screenGeneral.flag10_col == previousProject.ec_project_state_.screenGeneral.flag10_col);
    dataSetTest = dataSetTest && qFuzzyCompare(ec_project_state_.screenGeneral.flag10_threshold, previousProject.ec_project_state_.screenGeneral.flag10_threshold);
    dataSetTest = dataSetTest && (ec_project_state_.screenGeneral.flag10_policy == previousProject.ec_project_state_.screenGeneral.flag10_policy);
    dataSetTest = dataSetTest && (ec_project_state_.screenSetting.max_lack == previousProject.ec_project_state_.screenSetting.max_lack);
    dataSetTest = dataSetTest && (ec_project_state_.screenSetting.instr_max_lack == previousProject.ec_project_state_.screenSetting.instr_max_lack);
    dataSetTest = dataSetTest && qFuzzyCompare(ec_project_state_.screenSetting.u_offset, previousProject.ec_project_state_.screenSetting.u_offset);
    dataSetTest = dataSetTest && qFuzzyCompare(ec_project_state_.screenSetting.v_offset, previousProject.ec_project_state_.screenSetting.v_offset);
    dataSetTest = dataSetTest && qFuzzyCompare(ec_project_state_.screenSetting.w_offset, previousProject.ec_project_state_.screenSetting.w_offset);
    dataSetTest = dataSetTest && (ec_project_state_.screenSetting.avrg_len == previousProject.ec_project_state_.screenSetting.avrg_len);

    bool advSettingsTest = true;
    bool subTest = (ec_project_state_.screenSetting.detrend_meth == previousProject.ec_project_state_.screenSetting.detrend_meth);

    if (subTest && ec_project_state_.screenSetting.detrend_meth)
    {
        advSettingsTest = advSettingsTest && qFuzzyCompare(ec_project_state_.screenSetting.timeconst, previousProject.ec_project_state_.screenSetting.timeconst);
    }

    if ((ec_project_state_.projectGeneral.hf_meth > 1
         && ec_project_state_.projectGeneral.hf_meth < 5
         && ec_project_state_.spectraSettings.sa_mode)
        || ec_project_state_.screenSetting.out_bin_sp == 1
        || ec_project_state_.screenSetting.out_bin_og == 1
        || ec_project_state_.screenSetting.out_full_sp_u == 1
        || ec_project_state_.screenSetting.out_full_sp_v == 1
        || ec_project_state_.screenSetting.out_full_sp_w == 1
        || ec_project_state_.screenSetting.out_full_sp_ts == 1
        || ec_project_state_.screenSetting.out_full_sp_ch4 == 1
        || ec_project_state_.screenSetting.out_full_sp_co2 == 1
        || ec_project_state_.screenSetting.out_full_sp_h2o == 1
        || ec_project_state_.screenSetting.out_full_sp_gas4 == 1
        || ec_project_state_.screenSetting.out_full_cosp_ch4 == 1
        || ec_project_state_.screenSetting.out_full_cosp_co2 == 1
        || ec_project_state_.screenSetting.out_full_cosp_h2o == 1
        || ec_project_state_.screenSetting.out_full_cosp_gas4 == 1
        || ec_project_state_.screenSetting.out_full_cosp_ts == 1
        || ec_project_state_.screenSetting.out_full_cosp_u == 1
        || ec_project_state_.screenSetting.out_full_cosp_v == 1
        || ec_project_state_.projectGeneral.out_mean_spectra == 1
        || ec_project_state_.projectGeneral.out_mean_cosp == 1)
    {
        advSettingsTest = advSettingsTest
               && (ec_project_state_.screenSetting.tap_win == previousProject.ec_project_state_.screenSetting.tap_win)
               && (ec_project_state_.screenSetting.nbins == previousProject.ec_project_state_.screenSetting.nbins);
    }

    advSettingsTest = advSettingsTest
        && qFuzzyCompare(ec_project_state_.screenSetting.timeconst, previousProject.ec_project_state_.screenSetting.timeconst);

    advSettingsTest = advSettingsTest
        && (ec_project_state_.screenSetting.tlag_meth == previousProject.ec_project_state_.screenSetting.tlag_meth);

    advSettingsTest = advSettingsTest
        && previousSettingsCompare(ec_project_state_.screenSetting.out_bin_sp, previousProject.ec_project_state_.screenSetting.out_bin_sp);

    advSettingsTest = advSettingsTest
        && previousSettingsCompare(ec_project_state_.screenSetting.out_bin_og, previousProject.ec_project_state_.screenSetting.out_bin_og);

    advSettingsTest = advSettingsTest
        && previousSettingsCompare(ec_project_state_.projectGeneral.out_mean_spectra, previousProject.ec_project_state_.projectGeneral.out_mean_spectra);

    advSettingsTest = advSettingsTest
        && previousSettingsCompare(ec_project_state_.projectGeneral.out_mean_cosp, previousProject.ec_project_state_.projectGeneral.out_mean_cosp);

    advSettingsTest = advSettingsTest
        && previousSettingsCompare(ec_project_state_.screenSetting.out_full_sp_u, previousProject.ec_project_state_.screenSetting.out_full_sp_u);

    advSettingsTest = advSettingsTest
        && previousSettingsCompare(ec_project_state_.screenSetting.out_full_sp_v, previousProject.ec_project_state_.screenSetting.out_full_sp_v);

    advSettingsTest = advSettingsTest
        && previousSettingsCompare(ec_project_state_.screenSetting.out_full_sp_w, previousProject.ec_project_state_.screenSetting.out_full_sp_w);

    advSettingsTest = advSettingsTest
        && previousSettingsCompare(ec_project_state_.screenSetting.out_full_sp_ts, previousProject.ec_project_state_.screenSetting.out_full_sp_ts);

    advSettingsTest = advSettingsTest
        && previousSettingsCompare(ec_project_state_.screenSetting.out_full_sp_co2, previousProject.ec_project_state_.screenSetting.out_full_sp_co2);

    advSettingsTest = advSettingsTest
        && previousSettingsCompare(ec_project_state_.screenSetting.out_full_sp_h2o, previousProject.ec_project_state_.screenSetting.out_full_sp_h2o);

    advSettingsTest = advSettingsTest
        && previousSettingsCompare(ec_project_state_.screenSetting.out_full_sp_ch4, previousProject.ec_project_state_.screenSetting.out_full_sp_ch4);

    advSettingsTest = advSettingsTest
        && previousSettingsCompare(ec_project_state_.screenSetting.out_full_sp_gas4, previousProject.ec_project_state_.screenSetting.out_full_sp_gas4);

    advSettingsTest = advSettingsTest
        && previousSettingsCompare(ec_project_state_.screenSetting.out_full_cosp_u, previousProject.ec_project_state_.screenSetting.out_full_cosp_u);

    advSettingsTest = advSettingsTest
        && previousSettingsCompare(ec_project_state_.screenSetting.out_full_cosp_v, previousProject.ec_project_state_.screenSetting.out_full_cosp_v);

    advSettingsTest = advSettingsTest
        && previousSettingsCompare(ec_project_state_.screenSetting.out_full_cosp_ts, previousProject.ec_project_state_.screenSetting.out_full_cosp_ts);

    advSettingsTest = advSettingsTest
        && previousSettingsCompare(ec_project_state_.screenSetting.out_full_cosp_co2, previousProject.ec_project_state_.screenSetting.out_full_cosp_co2);

    advSettingsTest = advSettingsTest
        && previousSettingsCompare(ec_project_state_.screenSetting.out_full_cosp_h2o, previousProject.ec_project_state_.screenSetting.out_full_cosp_h2o);

    advSettingsTest = advSettingsTest
        && previousSettingsCompare(ec_project_state_.screenSetting.out_full_cosp_ch4, previousProject.ec_project_state_.screenSetting.out_full_cosp_ch4);

    advSettingsTest = advSettingsTest
        && previousSettingsCompare(ec_project_state_.screenSetting.out_full_cosp_gas4, previousProject.ec_project_state_.screenSetting.out_full_cosp_gas4);

    advSettingsTest = advSettingsTest
        && previousSettingsCompare(ec_project_state_.screenSetting.out_st_1, previousProject.ec_project_state_.screenSetting.out_st_1);

    advSettingsTest = advSettingsTest
        && previousSettingsCompare(ec_project_state_.screenSetting.out_st_2, previousProject.ec_project_state_.screenSetting.out_st_2)
        && previousSettingsCompare(ec_project_state_.screenSetting.out_st_3, previousProject.ec_project_state_.screenSetting.out_st_3)
        && previousSettingsCompare(ec_project_state_.screenSetting.out_st_4, previousProject.ec_project_state_.screenSetting.out_st_4)
        && previousSettingsCompare(ec_project_state_.screenSetting.out_st_5, previousProject.ec_project_state_.screenSetting.out_st_5)
        && previousSettingsCompare(ec_project_state_.screenSetting.out_st_6, previousProject.ec_project_state_.screenSetting.out_st_6)
        && previousSettingsCompare(ec_project_state_.screenSetting.out_st_7, previousProject.ec_project_state_.screenSetting.out_st_7)
        && previousSettingsCompare(ec_project_state_.screenSetting.out_details, previousProject.ec_project_state_.screenSetting.out_details)
        && previousSettingsCompare(ec_project_state_.screenSetting.out_raw_1, previousProject.ec_project_state_.screenSetting.out_raw_1)
        && previousSettingsCompare(ec_project_state_.screenSetting.out_raw_2, previousProject.ec_project_state_.screenSetting.out_raw_2)
        && previousSettingsCompare(ec_project_state_.screenSetting.out_raw_3, previousProject.ec_project_state_.screenSetting.out_raw_3)
        && previousSettingsCompare(ec_project_state_.screenSetting.out_raw_4, previousProject.ec_project_state_.screenSetting.out_raw_4)
        && previousSettingsCompare(ec_project_state_.screenSetting.out_raw_5, previousProject.ec_project_state_.screenSetting.out_raw_5)
        && previousSettingsCompare(ec_project_state_.screenSetting.out_raw_6, previousProject.ec_project_state_.screenSetting.out_raw_6)
        && previousSettingsCompare(ec_project_state_.screenSetting.out_raw_7, previousProject.ec_project_state_.screenSetting.out_raw_7)
        && previousSettingsCompare(ec_project_state_.screenSetting.out_raw_u, previousProject.ec_project_state_.screenSetting.out_raw_u)
        && previousSettingsCompare(ec_project_state_.screenSetting.out_raw_v, previousProject.ec_project_state_.screenSetting.out_raw_v)
        && previousSettingsCompare(ec_project_state_.screenSetting.out_raw_w, previousProject.ec_project_state_.screenSetting.out_raw_w)
        && previousSettingsCompare(ec_project_state_.screenSetting.out_raw_ts, previousProject.ec_project_state_.screenSetting.out_raw_ts)
        && previousSettingsCompare(ec_project_state_.screenSetting.out_raw_co2, previousProject.ec_project_state_.screenSetting.out_raw_co2)
        && previousSettingsCompare(ec_project_state_.screenSetting.out_raw_h2o, previousProject.ec_project_state_.screenSetting.out_raw_h2o)
        && previousSettingsCompare(ec_project_state_.screenSetting.out_raw_ch4, previousProject.ec_project_state_.screenSetting.out_raw_ch4)
        && previousSettingsCompare(ec_project_state_.screenSetting.out_raw_gas4, previousProject.ec_project_state_.screenSetting.out_raw_gas4)
        && previousSettingsCompare(ec_project_state_.screenSetting.out_raw_tair, previousProject.ec_project_state_.screenSetting.out_raw_tair)
        && previousSettingsCompare(ec_project_state_.screenSetting.out_raw_pair, previousProject.ec_project_state_.screenSetting.out_raw_pair);

    advSettingsTest = advSettingsTest
        && previousSettingsCompare(ec_project_state_.screenTest.test_sr, previousProject.ec_project_state_.screenTest.test_sr);

    advSettingsTest = advSettingsTest
        && previousSettingsCompare(ec_project_state_.screenTest.test_ar, previousProject.ec_project_state_.screenTest.test_ar)
        && previousSettingsCompare(ec_project_state_.screenTest.test_do, previousProject.ec_project_state_.screenTest.test_do)
        && previousSettingsCompare(ec_project_state_.screenTest.test_al, previousProject.ec_project_state_.screenTest.test_al)
        && previousSettingsCompare(ec_project_state_.screenTest.test_sk, previousProject.ec_project_state_.screenTest.test_sk)
        && previousSettingsCompare(ec_project_state_.screenTest.test_ds, previousProject.ec_project_state_.screenTest.test_ds)
        && previousSettingsCompare(ec_project_state_.screenTest.test_tl, previousProject.ec_project_state_.screenTest.test_tl)
        && previousSettingsCompare(ec_project_state_.screenTest.test_aa, previousProject.ec_project_state_.screenTest.test_aa)
        && previousSettingsCompare(ec_project_state_.screenTest.test_ns, previousProject.ec_project_state_.screenTest.test_ns);

    subTest = (ec_project_state_.screenTest.test_sr && previousProject.ec_project_state_.screenTest.test_sr);
    if (subTest)
    {
        advSettingsTest = advSettingsTest
               && (ec_project_state_.screenParam.sr_num_spk == previousProject.ec_project_state_.screenParam.sr_num_spk)
               && qFuzzyCompare(ec_project_state_.screenParam.sr_lim_hf, previousProject.ec_project_state_.screenParam.sr_lim_hf)
               && (ec_project_state_.screenSetting.filter_sr == previousProject.ec_project_state_.screenSetting.filter_sr)
               && qFuzzyCompare(ec_project_state_.screenParam.sr_lim_u, previousProject.ec_project_state_.screenParam.sr_lim_u)
               && qFuzzyCompare(ec_project_state_.screenParam.sr_lim_w, previousProject.ec_project_state_.screenParam.sr_lim_w)
               && qFuzzyCompare(ec_project_state_.screenParam.sr_lim_co2, previousProject.ec_project_state_.screenParam.sr_lim_co2)
               && qFuzzyCompare(ec_project_state_.screenParam.sr_lim_h2o, previousProject.ec_project_state_.screenParam.sr_lim_h2o)
               && qFuzzyCompare(ec_project_state_.screenParam.sr_lim_ch4, previousProject.ec_project_state_.screenParam.sr_lim_ch4)
               && qFuzzyCompare(ec_project_state_.screenParam.sr_lim_other, previousProject.ec_project_state_.screenParam.sr_lim_other);
    }

    subTest = (ec_project_state_.screenTest.test_ar && previousProject.ec_project_state_.screenTest.test_ar);
    if (subTest)
    {
        advSettingsTest = advSettingsTest
               && qFuzzyCompare(ec_project_state_.screenParam.ar_lim, previousProject.ec_project_state_.screenParam.ar_lim)
               && (ec_project_state_.screenParam.ar_bins == previousProject.ec_project_state_.screenParam.ar_bins)
               && (ec_project_state_.screenParam.ar_hf_lim == previousProject.ec_project_state_.screenParam.ar_hf_lim);
    }

    subTest = (ec_project_state_.screenTest.test_do && previousProject.ec_project_state_.screenTest.test_do);
    if (subTest)
    {
        advSettingsTest = advSettingsTest
               && (ec_project_state_.screenParam.do_extlim_dw == previousProject.ec_project_state_.screenParam.do_extlim_dw)
               && qFuzzyCompare(ec_project_state_.screenParam.do_hf1_lim, previousProject.ec_project_state_.screenParam.do_hf1_lim)
               && qFuzzyCompare(ec_project_state_.screenParam.do_hf2_lim, previousProject.ec_project_state_.screenParam.do_hf2_lim);
    }

    subTest = (ec_project_state_.screenTest.test_al && previousProject.ec_project_state_.screenTest.test_al);
    if (subTest)
    {
        advSettingsTest = advSettingsTest
               && qFuzzyCompare(ec_project_state_.screenParam.al_u_max, previousProject.ec_project_state_.screenParam.al_u_max)
               && qFuzzyCompare(ec_project_state_.screenParam.al_w_max, previousProject.ec_project_state_.screenParam.al_w_max)
               && qFuzzyCompare(ec_project_state_.screenParam.al_tson_min, previousProject.ec_project_state_.screenParam.al_tson_min)
               && qFuzzyCompare(ec_project_state_.screenParam.al_tson_max, previousProject.ec_project_state_.screenParam.al_tson_max)
               && qFuzzyCompare(ec_project_state_.screenParam.al_co2_min, previousProject.ec_project_state_.screenParam.al_co2_min)
               && qFuzzyCompare(ec_project_state_.screenParam.al_co2_max, previousProject.ec_project_state_.screenParam.al_co2_max)
               && qFuzzyCompare(ec_project_state_.screenParam.al_h2o_min, previousProject.ec_project_state_.screenParam.al_h2o_min)
               && qFuzzyCompare(ec_project_state_.screenParam.al_h2o_max, previousProject.ec_project_state_.screenParam.al_h2o_max)
               && qFuzzyCompare(ec_project_state_.screenParam.al_ch4_min, previousProject.ec_project_state_.screenParam.al_ch4_min)
               && qFuzzyCompare(ec_project_state_.screenParam.al_ch4_max, previousProject.ec_project_state_.screenParam.al_ch4_max)
               && qFuzzyCompare(ec_project_state_.screenParam.al_other_min, previousProject.ec_project_state_.screenParam.al_other_min)
               && qFuzzyCompare(ec_project_state_.screenParam.al_other_max, previousProject.ec_project_state_.screenParam.al_other_max)
               && (ec_project_state_.screenSetting.filter_al == previousProject.ec_project_state_.screenSetting.filter_al);
    }

    subTest = (ec_project_state_.screenTest.test_sk && previousProject.ec_project_state_.screenTest.test_sk);
    if (subTest)
    {
        advSettingsTest = advSettingsTest
               && qFuzzyCompare(ec_project_state_.screenParam.sk_hf_kumax, previousProject.ec_project_state_.screenParam.sk_hf_kumax)
               && qFuzzyCompare(ec_project_state_.screenParam.sk_hf_kumin, previousProject.ec_project_state_.screenParam.sk_hf_kumin)
               && qFuzzyCompare(ec_project_state_.screenParam.sk_hf_skmax, previousProject.ec_project_state_.screenParam.sk_hf_skmax)
               && qFuzzyCompare(ec_project_state_.screenParam.sk_hf_skmin, previousProject.ec_project_state_.screenParam.sk_hf_skmin)
               && qFuzzyCompare(ec_project_state_.screenParam.sk_sf_kumax, previousProject.ec_project_state_.screenParam.sk_sf_kumax)
               && qFuzzyCompare(ec_project_state_.screenParam.sk_sf_kumin, previousProject.ec_project_state_.screenParam.sk_sf_kumin)
               && qFuzzyCompare(ec_project_state_.screenParam.sk_sf_skmax, previousProject.ec_project_state_.screenParam.sk_sf_skmax)
               && qFuzzyCompare(ec_project_state_.screenParam.sk_sf_skmin, previousProject.ec_project_state_.screenParam.sk_sf_skmin);
    }

    subTest = (ec_project_state_.screenTest.test_ds && previousProject.ec_project_state_.screenTest.test_ds);
    if (subTest)
    {
        advSettingsTest = advSettingsTest
               && qFuzzyCompare(ec_project_state_.screenParam.ds_hf_ch4, previousProject.ec_project_state_.screenParam.ds_hf_ch4)
               && qFuzzyCompare(ec_project_state_.screenParam.ds_hf_co2, previousProject.ec_project_state_.screenParam.ds_hf_co2)
               && qFuzzyCompare(ec_project_state_.screenParam.ds_hf_h2o, previousProject.ec_project_state_.screenParam.ds_hf_h2o)
               && qFuzzyCompare(ec_project_state_.screenParam.ds_hf_other, previousProject.ec_project_state_.screenParam.ds_hf_other)
               && qFuzzyCompare(ec_project_state_.screenParam.ds_hf_t, previousProject.ec_project_state_.screenParam.ds_hf_t)
               && qFuzzyCompare(ec_project_state_.screenParam.ds_hf_uv, previousProject.ec_project_state_.screenParam.ds_hf_uv)
               && qFuzzyCompare(ec_project_state_.screenParam.ds_hf_var, previousProject.ec_project_state_.screenParam.ds_hf_var)
               && qFuzzyCompare(ec_project_state_.screenParam.ds_hf_w, previousProject.ec_project_state_.screenParam.ds_hf_w)
               && qFuzzyCompare(ec_project_state_.screenParam.ds_sf_ch4, previousProject.ec_project_state_.screenParam.ds_sf_ch4)
               && qFuzzyCompare(ec_project_state_.screenParam.ds_sf_co2, previousProject.ec_project_state_.screenParam.ds_sf_co2)
               && qFuzzyCompare(ec_project_state_.screenParam.ds_sf_h2o, previousProject.ec_project_state_.screenParam.ds_sf_h2o)
               && qFuzzyCompare(ec_project_state_.screenParam.ds_sf_other, previousProject.ec_project_state_.screenParam.ds_sf_other)
               && qFuzzyCompare(ec_project_state_.screenParam.ds_sf_t, previousProject.ec_project_state_.screenParam.ds_sf_t)
               && qFuzzyCompare(ec_project_state_.screenParam.ds_sf_uv, previousProject.ec_project_state_.screenParam.ds_sf_uv)
               && qFuzzyCompare(ec_project_state_.screenParam.ds_sf_var, previousProject.ec_project_state_.screenParam.ds_sf_var)
               && qFuzzyCompare(ec_project_state_.screenParam.ds_sf_w, previousProject.ec_project_state_.screenParam.ds_sf_w)
               && ec_project_state_.screenParam.despike_vm == previousProject.ec_project_state_.screenParam.despike_vm;
    }

    subTest = (ec_project_state_.screenTest.test_tl && previousProject.ec_project_state_.screenTest.test_tl);
    if (subTest)
    {
        advSettingsTest = advSettingsTest
               && qFuzzyCompare(ec_project_state_.screenParam.tl_def_ch4, previousProject.ec_project_state_.screenParam.tl_def_ch4)
               && qFuzzyCompare(ec_project_state_.screenParam.tl_def_co2, previousProject.ec_project_state_.screenParam.tl_def_co2)
               && qFuzzyCompare(ec_project_state_.screenParam.tl_def_h2o, previousProject.ec_project_state_.screenParam.tl_def_h2o)
               && qFuzzyCompare(ec_project_state_.screenParam.tl_def_other, previousProject.ec_project_state_.screenParam.tl_def_other)
               && qFuzzyCompare(ec_project_state_.screenParam.tl_hf_lim, previousProject.ec_project_state_.screenParam.tl_hf_lim)
               && qFuzzyCompare(ec_project_state_.screenParam.tl_sf_lim, previousProject.ec_project_state_.screenParam.tl_sf_lim);
    }

    subTest = (ec_project_state_.screenTest.test_aa && previousProject.ec_project_state_.screenTest.test_aa);
    if (subTest)
    {
        advSettingsTest = advSettingsTest
               && qFuzzyCompare(ec_project_state_.screenParam.aa_lim, previousProject.ec_project_state_.screenParam.aa_lim)
               && qFuzzyCompare(ec_project_state_.screenParam.aa_max, previousProject.ec_project_state_.screenParam.aa_max)
               && qFuzzyCompare(ec_project_state_.screenParam.aa_min, previousProject.ec_project_state_.screenParam.aa_min);
    }

    subTest = (ec_project_state_.screenTest.test_ns && previousProject.ec_project_state_.screenTest.test_ns);
    if (subTest)
    {
        advSettingsTest = advSettingsTest
               && qFuzzyCompare(ec_project_state_.screenParam.ns_hf_lim, previousProject.ec_project_state_.screenParam.ns_hf_lim);
    }

    subTest = (ec_project_state_.projectGeneral.use_biomet == previousProject.ec_project_state_.projectGeneral.use_biomet);
    dataSetTest = dataSetTest && subTest;

    if (subTest)
    {
        advSettingsTest = advSettingsTest
               && (ec_project_state_.biomParam.col_lwin == previousProject.ec_project_state_.biomParam.col_lwin)
               && (ec_project_state_.biomParam.col_pa == previousProject.ec_project_state_.biomParam.col_pa)
               && (ec_project_state_.biomParam.col_ppfd == previousProject.ec_project_state_.biomParam.col_ppfd)
               && (ec_project_state_.biomParam.col_rg == previousProject.ec_project_state_.biomParam.col_rg)
               && (ec_project_state_.biomParam.col_rh == previousProject.ec_project_state_.biomParam.col_rh)
               && (ec_project_state_.biomParam.col_ta == previousProject.ec_project_state_.biomParam.col_ta);
    }

    subTest = (ec_project_state_.projectGeneral.use_alt_md_file == previousProject.ec_project_state_.projectGeneral.use_alt_md_file);
    dataSetTest = dataSetTest && subTest;

    if (subTest && ec_project_state_.projectGeneral.use_alt_md_file)
        subTest = subTest && previousFileNameCompare(ec_project_state_.projectGeneral.md_file, previousProject.ec_project_state_.projectGeneral.md_file);
    dataSetTest = dataSetTest && subTest;

    subTest = (ec_project_state_.projectGeneral.use_tlfile == previousProject.ec_project_state_.projectGeneral.use_tlfile);
    dataSetTest = dataSetTest && subTest;

    if (subTest && ec_project_state_.projectGeneral.use_tlfile)
        subTest = subTest && previousFileNameCompare(ec_project_state_.projectGeneral.timeline_file, previousProject.ec_project_state_.projectGeneral.timeline_file);
    dataSetTest = dataSetTest && subTest;

    subTest = (ec_project_state_.projectGeneral.wpl_meth == previousProject.ec_project_state_.projectGeneral.wpl_meth);
    advSettingsTest = advSettingsTest && subTest;

    if (subTest && ec_project_state_.projectGeneral.wpl_meth)
    {
        subTest = (ec_project_state_.screenSetting.bu_corr == previousProject.ec_project_state_.screenSetting.bu_corr);
        advSettingsTest = advSettingsTest && subTest;

        if (subTest && ec_project_state_.screenSetting.bu_multi == previousProject.ec_project_state_.screenSetting.bu_multi)
        {
            advSettingsTest = advSettingsTest
                && qFuzzyCompare(ec_project_state_.screenSetting.l_day_bot_gain, previousProject.ec_project_state_.screenSetting.l_day_bot_gain)
                && qFuzzyCompare(ec_project_state_.screenSetting.l_day_bot_offset, previousProject.ec_project_state_.screenSetting.l_day_bot_offset)
                && qFuzzyCompare(ec_project_state_.screenSetting.l_day_spar_gain, previousProject.ec_project_state_.screenSetting.l_day_spar_gain)
                && qFuzzyCompare(ec_project_state_.screenSetting.l_day_spar_offset, previousProject.ec_project_state_.screenSetting.l_day_spar_offset)
                && qFuzzyCompare(ec_project_state_.screenSetting.l_day_top_gain, previousProject.ec_project_state_.screenSetting.l_day_top_gain)
                && qFuzzyCompare(ec_project_state_.screenSetting.l_day_top_offset, previousProject.ec_project_state_.screenSetting.l_day_top_offset)
                && qFuzzyCompare(ec_project_state_.screenSetting.l_night_bot_gain, previousProject.ec_project_state_.screenSetting.l_night_bot_gain)
                && qFuzzyCompare(ec_project_state_.screenSetting.l_night_bot_offset, previousProject.ec_project_state_.screenSetting.l_night_bot_offset)
                && qFuzzyCompare(ec_project_state_.screenSetting.l_night_spar_gain, previousProject.ec_project_state_.screenSetting.l_night_spar_gain)
                && qFuzzyCompare(ec_project_state_.screenSetting.l_night_spar_offset, previousProject.ec_project_state_.screenSetting.l_night_spar_offset)
                && qFuzzyCompare(ec_project_state_.screenSetting.l_night_top_gain, previousProject.ec_project_state_.screenSetting.l_night_top_gain)
                && qFuzzyCompare(ec_project_state_.screenSetting.l_night_top_offset, previousProject.ec_project_state_.screenSetting.l_night_top_offset)
                && qFuzzyCompare(ec_project_state_.screenSetting.m_day_bot1, previousProject.ec_project_state_.screenSetting.m_day_bot1)
                && qFuzzyCompare(ec_project_state_.screenSetting.m_day_bot2, previousProject.ec_project_state_.screenSetting.m_day_bot2)
                && qFuzzyCompare(ec_project_state_.screenSetting.m_day_bot3, previousProject.ec_project_state_.screenSetting.m_day_bot3)
                && qFuzzyCompare(ec_project_state_.screenSetting.m_day_bot4, previousProject.ec_project_state_.screenSetting.m_day_bot4)
                && qFuzzyCompare(ec_project_state_.screenSetting.m_day_spar1, previousProject.ec_project_state_.screenSetting.m_day_spar1)
                && qFuzzyCompare(ec_project_state_.screenSetting.m_day_spar2, previousProject.ec_project_state_.screenSetting.m_day_spar2)
                && qFuzzyCompare(ec_project_state_.screenSetting.m_day_spar3, previousProject.ec_project_state_.screenSetting.m_day_spar3)
                && qFuzzyCompare(ec_project_state_.screenSetting.m_day_spar4, previousProject.ec_project_state_.screenSetting.m_day_spar4)
                && qFuzzyCompare(ec_project_state_.screenSetting.m_day_top1, previousProject.ec_project_state_.screenSetting.m_day_top1)
                && qFuzzyCompare(ec_project_state_.screenSetting.m_day_top2, previousProject.ec_project_state_.screenSetting.m_day_top2)
                && qFuzzyCompare(ec_project_state_.screenSetting.m_day_top3, previousProject.ec_project_state_.screenSetting.m_day_top3)
                && qFuzzyCompare(ec_project_state_.screenSetting.m_day_top4, previousProject.ec_project_state_.screenSetting.m_day_top4)
                && qFuzzyCompare(ec_project_state_.screenSetting.m_night_bot1, previousProject.ec_project_state_.screenSetting.m_night_bot1)
                && qFuzzyCompare(ec_project_state_.screenSetting.m_night_bot2, previousProject.ec_project_state_.screenSetting.m_night_bot2)
                && qFuzzyCompare(ec_project_state_.screenSetting.m_night_bot3, previousProject.ec_project_state_.screenSetting.m_night_bot3)
                && qFuzzyCompare(ec_project_state_.screenSetting.m_night_bot4, previousProject.ec_project_state_.screenSetting.m_night_bot4)
                && qFuzzyCompare(ec_project_state_.screenSetting.m_night_spar1, previousProject.ec_project_state_.screenSetting.m_night_spar1)
                && qFuzzyCompare(ec_project_state_.screenSetting.m_night_spar2, previousProject.ec_project_state_.screenSetting.m_night_spar2)
                && qFuzzyCompare(ec_project_state_.screenSetting.m_night_spar3, previousProject.ec_project_state_.screenSetting.m_night_spar3)
                && qFuzzyCompare(ec_project_state_.screenSetting.m_night_spar4, previousProject.ec_project_state_.screenSetting.m_night_spar4)
                && qFuzzyCompare(ec_project_state_.screenSetting.m_night_top1, previousProject.ec_project_state_.screenSetting.m_night_top1)
                && qFuzzyCompare(ec_project_state_.screenSetting.m_night_top2, previousProject.ec_project_state_.screenSetting.m_night_top2)
                && qFuzzyCompare(ec_project_state_.screenSetting.m_night_top3, previousProject.ec_project_state_.screenSetting.m_night_top3)
                && qFuzzyCompare(ec_project_state_.screenSetting.m_night_top4, previousProject.ec_project_state_.screenSetting.m_night_top4);
        }
    }

    subTest = (ec_project_state_.projectGeneral.cec_meth == previousProject.ec_project_state_.projectGeneral.cec_meth);
    advSettingsTest = advSettingsTest && subTest;

    if (subTest && ec_project_state_.projectGeneral.cec_meth)
    {
        advSettingsTest = advSettingsTest
            && qFuzzyCompare(ec_project_state_.projectGeneral.cec_h, previousProject.ec_project_state_.projectGeneral.cec_h)
            && qFuzzyCompare(ec_project_state_.projectGeneral.cec_min_o1_o2, previousProject.ec_project_state_.projectGeneral.cec_min_o1_o2)
            && qFuzzyCompare(ec_project_state_.projectGeneral.cec_min_octant, previousProject.ec_project_state_.projectGeneral.cec_min_octant)
            && qFuzzyCompare(ec_project_state_.projectGeneral.cec_min_valid, previousProject.ec_project_state_.projectGeneral.cec_min_valid)
            && qFuzzyCompare(ec_project_state_.projectGeneral.cec_signal_strength, previousProject.ec_project_state_.projectGeneral.cec_signal_strength)
            && (ec_project_state_.projectGeneral.cec_max_gap_fill == previousProject.ec_project_state_.projectGeneral.cec_max_gap_fill)
            && qFuzzyCompare(ec_project_state_.projectGeneral.cec_max_stationarity, previousProject.ec_project_state_.projectGeneral.cec_max_stationarity)
            && qFuzzyCompare(ec_project_state_.projectGeneral.cec_singular_band, previousProject.ec_project_state_.projectGeneral.cec_singular_band)
            && (ec_project_state_.projectGeneral.cec_stationarity_mode == previousProject.ec_project_state_.projectGeneral.cec_stationarity_mode)
            && (ec_project_state_.projectGeneral.cecPairs == previousProject.ec_project_state_.projectGeneral.cecPairs);
    }

    subTest = (ec_project_state_.projectGeneral.master_sonic == previousProject.ec_project_state_.projectGeneral.master_sonic);
    dataSetTest = dataSetTest && subTest;
    if (subTest && !ec_project_state_.projectGeneral.master_sonic.contains(QStringLiteral("csat")))
    {
        dataSetTest = dataSetTest && (ec_project_state_.screenSetting.cross_wind == previousProject.ec_project_state_.screenSetting.cross_wind);
    }

    if (subTest && (ec_project_state_.projectGeneral.master_sonic.contains(QStringLiteral("hs"))
                    || ec_project_state_.projectGeneral.master_sonic.contains(QStringLiteral("wm"))
                    || ec_project_state_.projectGeneral.master_sonic.contains(QStringLiteral("r2"))
                    || ec_project_state_.projectGeneral.master_sonic.contains(QStringLiteral("r3"))))
    {
        dataSetTest = dataSetTest && (ec_project_state_.screenSetting.flow_distortion == previousProject.ec_project_state_.screenSetting.flow_distortion);
    }

    subTest = (ec_project_state_.screenSetting.rot_meth == previousProject.ec_project_state_.screenSetting.rot_meth);
    advSettingsTest = advSettingsTest && subTest;

    if (subTest && (ec_project_state_.screenSetting.rot_meth == 3
                    || ec_project_state_.screenSetting.rot_meth == 4))
    {
        subTest = (ec_project_state_.screenTilt.mode == previousProject.ec_project_state_.screenTilt.mode);
        advSettingsTest = advSettingsTest && subTest;

        if (subTest && ec_project_state_.screenTilt.mode == 1)
        {
            advSettingsTest = advSettingsTest
                && qFuzzyCompare(ec_project_state_.screenTilt.north_offset, previousProject.ec_project_state_.screenTilt.north_offset)
                && (ec_project_state_.screenTilt.min_num_per_sec == previousProject.ec_project_state_.screenTilt.min_num_per_sec)
                && qFuzzyCompare(ec_project_state_.screenTilt.w_max, previousProject.ec_project_state_.screenTilt.w_max)
                && qFuzzyCompare(ec_project_state_.screenTilt.u_min, previousProject.ec_project_state_.screenTilt.u_min)
                && (ec_project_state_.screenTilt.fix_policy == previousProject.ec_project_state_.screenTilt.fix_policy);

            subTest = (ec_project_state_.screenTilt.subset == previousProject.ec_project_state_.screenTilt.subset);
            if (subTest)
            {
                if (ec_project_state_.screenTilt.subset)
                {
                    advSettingsTest = advSettingsTest
                                      && (ec_project_state_.screenTilt.start_date == previousProject.ec_project_state_.screenTilt.start_date)
                                      && (ec_project_state_.screenTilt.end_date == previousProject.ec_project_state_.screenTilt.end_date);
                }
                else
                {
                    advSettingsTest = advSettingsTest
                                      && (ec_project_state_.projectGeneral.files_found == previousProject.ec_project_state_.projectGeneral.files_found);
                }
            }
            else
            {
                advSettingsTest = false;
            }
        }
        else if (subTest && ec_project_state_.screenTilt.mode == 0)
        {
            advSettingsTest = advSettingsTest
                              && previousFileNameCompare(ec_project_state_.screenTilt.file, previousProject.ec_project_state_.screenTilt.file);
        }
        else
        {
            advSettingsTest = false;
        }
    }

    subTest = (ec_project_state_.screenSetting.tlag_meth == previousProject.ec_project_state_.screenSetting.tlag_meth);
    advSettingsTest = advSettingsTest && subTest;

    if (subTest && (ec_project_state_.screenSetting.tlag_meth == 4))
    {
        subTest = (ec_project_state_.timelagOpt.mode == previousProject.ec_project_state_.timelagOpt.mode);
        advSettingsTest = advSettingsTest && subTest;

        if (subTest && ec_project_state_.timelagOpt.mode == 1)
        {
            advSettingsTest = advSettingsTest
                && qFuzzyCompare(ec_project_state_.timelagOpt.ch4_max_lag, previousProject.ec_project_state_.timelagOpt.ch4_max_lag)
                && qFuzzyCompare(ec_project_state_.timelagOpt.ch4_min_flux, previousProject.ec_project_state_.timelagOpt.ch4_min_flux)
                && qFuzzyCompare(ec_project_state_.timelagOpt.ch4_min_lag, previousProject.ec_project_state_.timelagOpt.ch4_min_lag)
                && qFuzzyCompare(ec_project_state_.timelagOpt.co2_max_lag, previousProject.ec_project_state_.timelagOpt.co2_max_lag)
                && qFuzzyCompare(ec_project_state_.timelagOpt.co2_min_flux, previousProject.ec_project_state_.timelagOpt.co2_min_flux)
                && qFuzzyCompare(ec_project_state_.timelagOpt.co2_min_lag, previousProject.ec_project_state_.timelagOpt.co2_min_lag)
                && qFuzzyCompare(ec_project_state_.timelagOpt.gas4_max_lag, previousProject.ec_project_state_.timelagOpt.gas4_max_lag)
                && qFuzzyCompare(ec_project_state_.timelagOpt.gas4_min_flux, previousProject.ec_project_state_.timelagOpt.gas4_min_flux)
                && qFuzzyCompare(ec_project_state_.timelagOpt.gas4_min_lag, previousProject.ec_project_state_.timelagOpt.gas4_min_lag)
                && qFuzzyCompare(ec_project_state_.timelagOpt.h2o_max_lag, previousProject.ec_project_state_.timelagOpt.h2o_max_lag)
                && qFuzzyCompare(ec_project_state_.timelagOpt.h2o_min_lag, previousProject.ec_project_state_.timelagOpt.h2o_min_lag)
                && qFuzzyCompare(ec_project_state_.timelagOpt.le_min_flux, previousProject.ec_project_state_.timelagOpt.le_min_flux)
                && qFuzzyCompare(ec_project_state_.timelagOpt.pg_range, previousProject.ec_project_state_.timelagOpt.pg_range)
                && (ec_project_state_.timelagOpt.to_h2o_nclass == previousProject.ec_project_state_.timelagOpt.to_h2o_nclass);

            subTest = (ec_project_state_.timelagOpt.subset == previousProject.ec_project_state_.timelagOpt.subset);
            if (subTest)
            {
                if (ec_project_state_.timelagOpt.subset)
                {
                    advSettingsTest = advSettingsTest
                                      && (ec_project_state_.timelagOpt.start_date == previousProject.ec_project_state_.timelagOpt.start_date)
                                      && (ec_project_state_.timelagOpt.end_date == previousProject.ec_project_state_.timelagOpt.end_date);
                }
                else
                {
                    advSettingsTest = advSettingsTest
                                      && (ec_project_state_.projectGeneral.files_found == previousProject.ec_project_state_.projectGeneral.files_found);
                }
            }
            else
            {
                advSettingsTest = false;
            }
        }
        else if (subTest && ec_project_state_.timelagOpt.mode == 0)
        {
            advSettingsTest = advSettingsTest
                              && previousFileNameCompare(ec_project_state_.timelagOpt.file, previousProject.ec_project_state_.timelagOpt.file);
        }
        else
        {
            advSettingsTest = false;
        }
    }
    else if (subTest && (ec_project_state_.screenSetting.tlag_meth == 5))
    {
        advSettingsTest = advSettingsTest
            && qFuzzyCompare(ec_project_state_.pwbTimelag.co2_min_lag, previousProject.ec_project_state_.pwbTimelag.co2_min_lag)
            && qFuzzyCompare(ec_project_state_.pwbTimelag.co2_max_lag, previousProject.ec_project_state_.pwbTimelag.co2_max_lag)
            && qFuzzyCompare(ec_project_state_.pwbTimelag.h2o_min_lag, previousProject.ec_project_state_.pwbTimelag.h2o_min_lag)
            && qFuzzyCompare(ec_project_state_.pwbTimelag.h2o_max_lag, previousProject.ec_project_state_.pwbTimelag.h2o_max_lag)
            && qFuzzyCompare(ec_project_state_.pwbTimelag.ch4_min_lag, previousProject.ec_project_state_.pwbTimelag.ch4_min_lag)
            && qFuzzyCompare(ec_project_state_.pwbTimelag.ch4_max_lag, previousProject.ec_project_state_.pwbTimelag.ch4_max_lag)
            && qFuzzyCompare(ec_project_state_.pwbTimelag.gas4_min_lag, previousProject.ec_project_state_.pwbTimelag.gas4_min_lag)
            && qFuzzyCompare(ec_project_state_.pwbTimelag.gas4_max_lag, previousProject.ec_project_state_.pwbTimelag.gas4_max_lag)
            && (ec_project_state_.pwbTimelag.n_bootstrap == previousProject.ec_project_state_.pwbTimelag.n_bootstrap)
            && qFuzzyCompare(ec_project_state_.pwbTimelag.block_length_s, previousProject.ec_project_state_.pwbTimelag.block_length_s)
            && qFuzzyCompare(ec_project_state_.pwbTimelag.min_valid_frac, previousProject.ec_project_state_.pwbTimelag.min_valid_frac)
            && qFuzzyCompare(ec_project_state_.pwbTimelag.hdi_thresh_s, previousProject.ec_project_state_.pwbTimelag.hdi_thresh_s)
            && qFuzzyCompare(ec_project_state_.pwbTimelag.dev_thresh_s, previousProject.ec_project_state_.pwbTimelag.dev_thresh_s)
            && qFuzzyCompare(ec_project_state_.pwbTimelag.hdi_prefilter_s, previousProject.ec_project_state_.pwbTimelag.hdi_prefilter_s)
            && (ec_project_state_.pwbTimelag.smoothing_width == previousProject.ec_project_state_.pwbTimelag.smoothing_width)
            && (ec_project_state_.pwbTimelag.random_seed == previousProject.ec_project_state_.pwbTimelag.random_seed);
    }

    subTest = ec_project_state_.randomError.ru_method == previousProject.ec_project_state_.randomError.ru_method;
    advSettingsTest = advSettingsTest && subTest;

    if (subTest && (ec_project_state_.randomError.ru_method == 1 || ec_project_state_.randomError.ru_method == 2))
    {
        advSettingsTest = advSettingsTest
            && ec_project_state_.randomError.its_method == previousProject.ec_project_state_.randomError.its_method
            && qFuzzyCompare(ec_project_state_.randomError.its_tlag_max, previousProject.ec_project_state_.randomError.its_tlag_max);
    }

    switch (ec_project_state_.projectGeneral.run_mode)
    {
        case Defs::CurrRunMode::Express:
            return dataSetTest;
        case Defs::CurrRunMode::Advanced:
            return (dataSetTest && advSettingsTest);
        case Defs::CurrRunMode::Retriever:
            return false;
    }
    Q_ASSERT(false);
    return false;
}

// New project
void EcProject::newEcProject(const ProjConfigState& project_config)
{
    // data e ora in formato ISO
    auto now = QDateTime::currentDateTime();
    auto now_str = now.toString(Qt::ISODate);

    // update project configuration
    project_config_state_ = project_config;

    EcProjectState defaultEcProjectState;
    ec_project_state_.projectGeneral.sw_version = defaultEcProjectState.projectGeneral.sw_version;
    ec_project_state_.projectGeneral.ini_version = defaultEcProjectState.projectGeneral.ini_version;
    ec_project_state_.projectGeneral.creation_date = now_str;
    ec_project_state_.projectGeneral.last_change_date.clear();
    ec_project_state_.projectGeneral.run_mode = defaultEcProjectState.projectGeneral.run_mode;
    ec_project_state_.projectGeneral.run_fcc = defaultEcProjectState.projectGeneral.run_fcc;
    ec_project_state_.projectGeneral.file_name.clear();
    ec_project_state_.projectGeneral.project_title.clear();
    ec_project_state_.projectGeneral.project_id.clear();
    ec_project_state_.projectGeneral.file_type = defaultEcProjectState.projectGeneral.file_type;
    ec_project_state_.projectGeneral.file_prototype.clear();
    ec_project_state_.projectGeneral.use_alt_md_file = defaultEcProjectState.projectGeneral.use_alt_md_file;
    ec_project_state_.projectGeneral.md_file.clear();
    ec_project_state_.projectGeneral.use_tlfile = defaultEcProjectState.projectGeneral.use_tlfile;
    ec_project_state_.projectGeneral.timeline_file.clear();
    ec_project_state_.projectGeneral.binary_hnlines = defaultEcProjectState.projectGeneral.binary_hnlines;
    ec_project_state_.projectGeneral.binary_eol = defaultEcProjectState.projectGeneral.binary_eol;
    ec_project_state_.projectGeneral.binary_nbytes = defaultEcProjectState.projectGeneral.binary_nbytes;
    ec_project_state_.projectGeneral.binary_little_end = defaultEcProjectState.projectGeneral.binary_little_end;
    ec_project_state_.projectGeneral.master_sonic.clear();
    ec_project_state_.projectGeneral.col_co2 = defaultEcProjectState.projectGeneral.col_co2;
    ec_project_state_.projectGeneral.col_h2o = defaultEcProjectState.projectGeneral.col_h2o;
    ec_project_state_.projectGeneral.col_ch4 = defaultEcProjectState.projectGeneral.col_ch4;
    ec_project_state_.projectGeneral.col_gas4 = defaultEcProjectState.projectGeneral.col_gas4;
    ec_project_state_.projectGeneral.col_int_t_c = defaultEcProjectState.projectGeneral.col_int_t_c;
    ec_project_state_.projectGeneral.col_int_t_1 = defaultEcProjectState.projectGeneral.col_int_t_1;
    ec_project_state_.projectGeneral.col_int_t_2 = defaultEcProjectState.projectGeneral.col_int_t_2;
    ec_project_state_.projectGeneral.col_int_p = defaultEcProjectState.projectGeneral.col_int_p;
    ec_project_state_.projectGeneral.col_air_t = defaultEcProjectState.projectGeneral.col_air_t;
    ec_project_state_.projectGeneral.col_air_p = defaultEcProjectState.projectGeneral.col_air_p;
    ec_project_state_.projectGeneral.col_diag_75 = defaultEcProjectState.projectGeneral.col_diag_75;
    ec_project_state_.projectGeneral.col_diag_72 = defaultEcProjectState.projectGeneral.col_diag_72;
    ec_project_state_.projectGeneral.col_diag_77 = defaultEcProjectState.projectGeneral.col_diag_77;
    ec_project_state_.projectGeneral.col_diag_anem = defaultEcProjectState.projectGeneral.col_diag_anem;
    ec_project_state_.projectGeneral.col_ts = defaultEcProjectState.projectGeneral.col_ts;
    ec_project_state_.projectGeneral.gas_mw = defaultEcProjectState.projectGeneral.gas_mw;
    ec_project_state_.projectGeneral.gas_diff = defaultEcProjectState.projectGeneral.gas_diff;
    ec_project_state_.projectGeneral.out_rich = defaultEcProjectState.projectGeneral.out_rich;
    ec_project_state_.projectGeneral.fluxnet_standardize_biomet = defaultEcProjectState.projectGeneral.fluxnet_standardize_biomet;
    ec_project_state_.projectGeneral.fluxnet_err_label = defaultEcProjectState.projectGeneral.fluxnet_err_label;
    ec_project_state_.projectGeneral.out_md = defaultEcProjectState.projectGeneral.out_md;
    ec_project_state_.projectGeneral.out_biomet = defaultEcProjectState.projectGeneral.out_biomet;
    ec_project_state_.projectGeneral.make_dataset = defaultEcProjectState.projectGeneral.make_dataset;
    ec_project_state_.projectGeneral.subset = defaultEcProjectState.projectGeneral.subset;
    ec_project_state_.projectGeneral.start_date = QDate(2000, 1, 1).toString(Qt::ISODate);
    ec_project_state_.projectGeneral.end_date = QDate::currentDate().toString(Qt::ISODate);
    ec_project_state_.projectGeneral.start_time = QTime(0, 0).toString(QStringLiteral("hh:mm"));
    ec_project_state_.projectGeneral.end_time = QTime(23, 59).toString(QStringLiteral("hh:mm"));
    ec_project_state_.projectGeneral.hf_meth = defaultEcProjectState.projectGeneral.hf_meth;
    ec_project_state_.projectGeneral.lf_meth = defaultEcProjectState.projectGeneral.lf_meth;
    ec_project_state_.projectGeneral.wpl_meth = defaultEcProjectState.projectGeneral.wpl_meth;
    ec_project_state_.projectGeneral.foot_meth = defaultEcProjectState.projectGeneral.foot_meth;
    ec_project_state_.projectGeneral.cec_meth = defaultEcProjectState.projectGeneral.cec_meth;
    ec_project_state_.projectGeneral.cec_h = defaultEcProjectState.projectGeneral.cec_h;
    ec_project_state_.projectGeneral.cec_min_o1_o2 = defaultEcProjectState.projectGeneral.cec_min_o1_o2;
    ec_project_state_.projectGeneral.cec_min_octant = defaultEcProjectState.projectGeneral.cec_min_octant;
    ec_project_state_.projectGeneral.cec_min_valid = defaultEcProjectState.projectGeneral.cec_min_valid;
    ec_project_state_.projectGeneral.cec_signal_strength = defaultEcProjectState.projectGeneral.cec_signal_strength;
    ec_project_state_.projectGeneral.cec_max_gap_fill = defaultEcProjectState.projectGeneral.cec_max_gap_fill;
    ec_project_state_.projectGeneral.cec_max_stationarity = defaultEcProjectState.projectGeneral.cec_max_stationarity;
    ec_project_state_.projectGeneral.cec_singular_band = defaultEcProjectState.projectGeneral.cec_singular_band;
    ec_project_state_.projectGeneral.cec_stationarity_mode = defaultEcProjectState.projectGeneral.cec_stationarity_mode;
    ec_project_state_.projectGeneral.cecPairs.clear();
    ec_project_state_.projectGeneral.tob1_format = defaultEcProjectState.projectGeneral.tob1_format;
    ec_project_state_.projectGeneral.out_path.clear();
    ec_project_state_.projectGeneral.fix_out_format = defaultEcProjectState.projectGeneral.fix_out_format;
    ec_project_state_.projectGeneral.err_label = defaultEcProjectState.projectGeneral.err_label;
    ec_project_state_.projectGeneral.qcflag_meth = defaultEcProjectState.projectGeneral.qcflag_meth;
    ec_project_state_.projectGeneral.use_biomet = defaultEcProjectState.projectGeneral.use_biomet;
    ec_project_state_.projectGeneral.biom_file.clear();
    ec_project_state_.projectGeneral.biom_dir.clear();
    ec_project_state_.projectGeneral.biom_recurse = defaultEcProjectState.projectGeneral.biom_recurse;
    ec_project_state_.projectGeneral.biom_ext = defaultEcProjectState.projectGeneral.biom_ext;
    ec_project_state_.projectGeneral.out_mean_spectra = defaultEcProjectState.projectGeneral.out_mean_spectra;
    ec_project_state_.projectGeneral.out_mean_cosp = defaultEcProjectState.projectGeneral.out_mean_cosp;
    ec_project_state_.projectGeneral.bin_sp_avail = defaultEcProjectState.projectGeneral.bin_sp_avail;
    ec_project_state_.projectGeneral.full_sp_avail = defaultEcProjectState.projectGeneral.full_sp_avail;
    ec_project_state_.projectGeneral.files_found = defaultEcProjectState.projectGeneral.files_found;
    ec_project_state_.projectGeneral.hf_correct_ghg_ba = defaultEcProjectState.projectGeneral.hf_correct_ghg_ba;
    ec_project_state_.projectGeneral.hf_correct_ghg_zoh = defaultEcProjectState.projectGeneral.hf_correct_ghg_zoh;
    ec_project_state_.projectGeneral.sonic_output_rate = defaultEcProjectState.projectGeneral.sonic_output_rate;

    // preproc general section
    ec_project_state_.screenGeneral.start_run.clear();
    ec_project_state_.screenGeneral.end_run.clear();
    ec_project_state_.screenGeneral.data_path.clear();
    ec_project_state_.screenGeneral.use_geo_north = defaultEcProjectState.screenGeneral.use_geo_north;
    ec_project_state_.screenGeneral.mag_dec = defaultEcProjectState.screenGeneral.mag_dec;
    ec_project_state_.screenGeneral.dec_date = ec_project_state_.projectGeneral.end_date;
    ec_project_state_.screenGeneral.recurse = defaultEcProjectState.screenGeneral.recurse;
    ec_project_state_.screenGeneral.flag1_col = defaultEcProjectState.screenGeneral.flag1_col;
    ec_project_state_.screenGeneral.flag1_threshold = defaultEcProjectState.screenGeneral.flag1_threshold;
    ec_project_state_.screenGeneral.flag1_policy = defaultEcProjectState.screenGeneral.flag1_policy;
    ec_project_state_.screenGeneral.flag2_col = defaultEcProjectState.screenGeneral.flag2_col;
    ec_project_state_.screenGeneral.flag2_threshold = defaultEcProjectState.screenGeneral.flag2_threshold;
    ec_project_state_.screenGeneral.flag2_policy = defaultEcProjectState.screenGeneral.flag2_policy;
    ec_project_state_.screenGeneral.flag3_col = defaultEcProjectState.screenGeneral.flag3_col;
    ec_project_state_.screenGeneral.flag3_threshold = defaultEcProjectState.screenGeneral.flag3_threshold;
    ec_project_state_.screenGeneral.flag3_policy = defaultEcProjectState.screenGeneral.flag3_policy;
    ec_project_state_.screenGeneral.flag4_col = defaultEcProjectState.screenGeneral.flag4_col;
    ec_project_state_.screenGeneral.flag4_threshold = defaultEcProjectState.screenGeneral.flag4_threshold;
    ec_project_state_.screenGeneral.flag4_policy = defaultEcProjectState.screenGeneral.flag4_policy;
    ec_project_state_.screenGeneral.flag5_col = defaultEcProjectState.screenGeneral.flag5_col;
    ec_project_state_.screenGeneral.flag5_threshold = defaultEcProjectState.screenGeneral.flag5_threshold;
    ec_project_state_.screenGeneral.flag5_policy = defaultEcProjectState.screenGeneral.flag5_policy;
    ec_project_state_.screenGeneral.flag6_col = defaultEcProjectState.screenGeneral.flag6_col;
    ec_project_state_.screenGeneral.flag6_threshold = defaultEcProjectState.screenGeneral.flag6_threshold;
    ec_project_state_.screenGeneral.flag6_policy = defaultEcProjectState.screenGeneral.flag6_policy;
    ec_project_state_.screenGeneral.flag7_col = defaultEcProjectState.screenGeneral.flag7_col;
    ec_project_state_.screenGeneral.flag7_threshold = defaultEcProjectState.screenGeneral.flag7_threshold;
    ec_project_state_.screenGeneral.flag7_policy = defaultEcProjectState.screenGeneral.flag7_policy;
    ec_project_state_.screenGeneral.flag8_col = defaultEcProjectState.screenGeneral.flag8_col;
    ec_project_state_.screenGeneral.flag8_threshold = defaultEcProjectState.screenGeneral.flag8_threshold;
    ec_project_state_.screenGeneral.flag8_policy = defaultEcProjectState.screenGeneral.flag8_policy;
    ec_project_state_.screenGeneral.flag9_col = defaultEcProjectState.screenGeneral.flag9_col;
    ec_project_state_.screenGeneral.flag9_threshold = defaultEcProjectState.screenGeneral.flag9_threshold;
    ec_project_state_.screenGeneral.flag9_policy = defaultEcProjectState.screenGeneral.flag9_policy;
    ec_project_state_.screenGeneral.flag10_col = defaultEcProjectState.screenGeneral.flag10_col;
    ec_project_state_.screenGeneral.flag10_threshold = defaultEcProjectState.screenGeneral.flag10_threshold;
    ec_project_state_.screenGeneral.flag10_policy = defaultEcProjectState.screenGeneral.flag10_policy;

    // preproc setting section
    ec_project_state_.screenSetting.avrg_len = defaultEcProjectState.screenSetting.avrg_len;
    ec_project_state_.screenSetting.max_lack = defaultEcProjectState.screenSetting.max_lack;
    ec_project_state_.screenSetting.instr_max_lack = defaultEcProjectState.screenSetting.instr_max_lack;
    ec_project_state_.screenSetting.u_offset = defaultEcProjectState.screenSetting.u_offset;
    ec_project_state_.screenSetting.v_offset = defaultEcProjectState.screenSetting.v_offset;
    ec_project_state_.screenSetting.w_offset = defaultEcProjectState.screenSetting.w_offset;
    ec_project_state_.screenSetting.cross_wind = defaultEcProjectState.screenSetting.cross_wind;
    ec_project_state_.screenSetting.gill_wm_wboost = defaultEcProjectState.screenSetting.gill_wm_wboost;
    ec_project_state_.screenSetting.flow_distortion = defaultEcProjectState.screenSetting.flow_distortion;
    ec_project_state_.screenSetting.rot_meth = defaultEcProjectState.screenSetting.rot_meth;
    ec_project_state_.screenSetting.detrend_meth = defaultEcProjectState.screenSetting.detrend_meth;
    ec_project_state_.screenSetting.timeconst = defaultEcProjectState.screenSetting.timeconst;
    ec_project_state_.screenSetting.tlag_meth = defaultEcProjectState.screenSetting.tlag_meth;
    ec_project_state_.screenSetting.tap_win = defaultEcProjectState.screenSetting.tap_win;
    ec_project_state_.screenSetting.nbins = defaultEcProjectState.screenSetting.nbins;
    ec_project_state_.screenSetting.out_bin_sp = defaultEcProjectState.screenSetting.out_bin_sp;
    ec_project_state_.screenSetting.out_bin_og = defaultEcProjectState.screenSetting.out_bin_og;
    ec_project_state_.screenSetting.out_full_sp_u = defaultEcProjectState.screenSetting.out_full_sp_u;
    ec_project_state_.screenSetting.out_full_sp_v = defaultEcProjectState.screenSetting.out_full_sp_v;
    ec_project_state_.screenSetting.out_full_sp_w = defaultEcProjectState.screenSetting.out_full_sp_w;
    ec_project_state_.screenSetting.out_full_sp_ts = defaultEcProjectState.screenSetting.out_full_sp_ts;
    ec_project_state_.screenSetting.out_full_sp_co2 = defaultEcProjectState.screenSetting.out_full_sp_co2;
    ec_project_state_.screenSetting.out_full_sp_h2o = defaultEcProjectState.screenSetting.out_full_sp_h2o;
    ec_project_state_.screenSetting.out_full_sp_ch4 = defaultEcProjectState.screenSetting.out_full_sp_ch4;
    ec_project_state_.screenSetting.out_full_sp_gas4 = defaultEcProjectState.screenSetting.out_full_sp_gas4;
    ec_project_state_.screenSetting.out_st_1 = defaultEcProjectState.screenSetting.out_st_1;
    ec_project_state_.screenSetting.out_st_2 = defaultEcProjectState.screenSetting.out_st_2;
    ec_project_state_.screenSetting.out_st_3 = defaultEcProjectState.screenSetting.out_st_3;
    ec_project_state_.screenSetting.out_st_4 = defaultEcProjectState.screenSetting.out_st_4;
    ec_project_state_.screenSetting.out_st_5 = defaultEcProjectState.screenSetting.out_st_5;
    ec_project_state_.screenSetting.out_st_6 = defaultEcProjectState.screenSetting.out_st_6;
    ec_project_state_.screenSetting.out_st_7 = defaultEcProjectState.screenSetting.out_st_7;
    ec_project_state_.screenSetting.out_raw_1 = defaultEcProjectState.screenSetting.out_raw_1;
    ec_project_state_.screenSetting.out_raw_2 = defaultEcProjectState.screenSetting.out_raw_2;
    ec_project_state_.screenSetting.out_raw_3 = defaultEcProjectState.screenSetting.out_raw_3;
    ec_project_state_.screenSetting.out_raw_4 = defaultEcProjectState.screenSetting.out_raw_4;
    ec_project_state_.screenSetting.out_raw_5 = defaultEcProjectState.screenSetting.out_raw_5;
    ec_project_state_.screenSetting.out_raw_6 = defaultEcProjectState.screenSetting.out_raw_6;
    ec_project_state_.screenSetting.out_raw_7 = defaultEcProjectState.screenSetting.out_raw_7;
    ec_project_state_.screenSetting.out_raw_u = defaultEcProjectState.screenSetting.out_raw_u;
    ec_project_state_.screenSetting.out_raw_v = defaultEcProjectState.screenSetting.out_raw_v;
    ec_project_state_.screenSetting.out_raw_w = defaultEcProjectState.screenSetting.out_raw_w;
    ec_project_state_.screenSetting.out_raw_ts = defaultEcProjectState.screenSetting.out_raw_ts;
    ec_project_state_.screenSetting.out_raw_co2 = defaultEcProjectState.screenSetting.out_raw_co2;
    ec_project_state_.screenSetting.out_raw_h2o = defaultEcProjectState.screenSetting.out_raw_h2o;
    ec_project_state_.screenSetting.out_raw_ch4 = defaultEcProjectState.screenSetting.out_raw_ch4;
    ec_project_state_.screenSetting.out_raw_gas4 = defaultEcProjectState.screenSetting.out_raw_gas4;
    ec_project_state_.screenSetting.out_raw_tair = defaultEcProjectState.screenSetting.out_raw_tair;
    ec_project_state_.screenSetting.out_raw_pair = defaultEcProjectState.screenSetting.out_raw_pair;
    ec_project_state_.screenSetting.out_full_cosp_u = defaultEcProjectState.screenSetting.out_full_cosp_u;
    ec_project_state_.screenSetting.out_full_cosp_v = defaultEcProjectState.screenSetting.out_full_cosp_v;
    ec_project_state_.screenSetting.out_full_cosp_ts = defaultEcProjectState.screenSetting.out_full_cosp_ts;
    ec_project_state_.screenSetting.out_full_cosp_co2 = defaultEcProjectState.screenSetting.out_full_cosp_co2;
    ec_project_state_.screenSetting.out_full_cosp_h2o = defaultEcProjectState.screenSetting.out_full_cosp_h2o;
    ec_project_state_.screenSetting.out_full_cosp_ch4 = defaultEcProjectState.screenSetting.out_full_cosp_ch4;
    ec_project_state_.screenSetting.out_full_cosp_gas4 = defaultEcProjectState.screenSetting.out_full_cosp_gas4;
    ec_project_state_.screenSetting.filter_sr = defaultEcProjectState.screenSetting.filter_sr;
    ec_project_state_.screenSetting.filter_al = defaultEcProjectState.screenSetting.filter_al;
    ec_project_state_.screenSetting.bu_corr = defaultEcProjectState.screenSetting.bu_corr;
    ec_project_state_.screenSetting.bu_multi = defaultEcProjectState.screenSetting.bu_multi;
    ec_project_state_.screenSetting.l_day_bot_gain = defaultEcProjectState.screenSetting.l_day_bot_gain;
    ec_project_state_.screenSetting.l_day_bot_offset = defaultEcProjectState.screenSetting.l_day_bot_offset;
    ec_project_state_.screenSetting.l_day_top_gain = defaultEcProjectState.screenSetting.l_day_top_gain;
    ec_project_state_.screenSetting.l_day_top_offset = defaultEcProjectState.screenSetting.l_day_top_offset;
    ec_project_state_.screenSetting.l_day_spar_gain = defaultEcProjectState.screenSetting.l_day_spar_gain;
    ec_project_state_.screenSetting.l_day_spar_offset = defaultEcProjectState.screenSetting.l_day_spar_offset;
    ec_project_state_.screenSetting.l_night_bot_gain = defaultEcProjectState.screenSetting.l_night_bot_gain;
    ec_project_state_.screenSetting.l_night_bot_offset = defaultEcProjectState.screenSetting.l_night_bot_offset;
    ec_project_state_.screenSetting.l_night_top_gain = defaultEcProjectState.screenSetting.l_night_top_gain;
    ec_project_state_.screenSetting.l_night_top_offset = defaultEcProjectState.screenSetting.l_night_top_offset;
    ec_project_state_.screenSetting.l_night_spar_gain = defaultEcProjectState.screenSetting.l_night_spar_gain;
    ec_project_state_.screenSetting.l_night_spar_offset = defaultEcProjectState.screenSetting.l_night_spar_offset;
    ec_project_state_.screenSetting.m_day_bot1 = defaultEcProjectState.screenSetting.m_day_bot1;
    ec_project_state_.screenSetting.m_day_bot2 = defaultEcProjectState.screenSetting.m_day_bot2;
    ec_project_state_.screenSetting.m_day_bot3 = defaultEcProjectState.screenSetting.m_day_bot3;
    ec_project_state_.screenSetting.m_day_bot4 = defaultEcProjectState.screenSetting.m_day_bot4;
    ec_project_state_.screenSetting.m_day_top1 = defaultEcProjectState.screenSetting.m_day_top1;
    ec_project_state_.screenSetting.m_day_top2 = defaultEcProjectState.screenSetting.m_day_top2;
    ec_project_state_.screenSetting.m_day_top3 = defaultEcProjectState.screenSetting.m_day_top3;
    ec_project_state_.screenSetting.m_day_top4 = defaultEcProjectState.screenSetting.m_day_top4;
    ec_project_state_.screenSetting.m_day_spar1 = defaultEcProjectState.screenSetting.m_day_spar1;
    ec_project_state_.screenSetting.m_day_spar2 = defaultEcProjectState.screenSetting.m_day_spar2;
    ec_project_state_.screenSetting.m_day_spar3 = defaultEcProjectState.screenSetting.m_day_spar3;
    ec_project_state_.screenSetting.m_day_spar4 = defaultEcProjectState.screenSetting.m_day_spar4;
    ec_project_state_.screenSetting.m_night_bot1 = defaultEcProjectState.screenSetting.m_night_bot1;
    ec_project_state_.screenSetting.m_night_bot2 = defaultEcProjectState.screenSetting.m_night_bot2;
    ec_project_state_.screenSetting.m_night_bot3 = defaultEcProjectState.screenSetting.m_night_bot3;
    ec_project_state_.screenSetting.m_night_bot4 = defaultEcProjectState.screenSetting.m_night_bot4;
    ec_project_state_.screenSetting.m_night_top1 = defaultEcProjectState.screenSetting.m_night_top1;
    ec_project_state_.screenSetting.m_night_top2 = defaultEcProjectState.screenSetting.m_night_top2;
    ec_project_state_.screenSetting.m_night_top3 = defaultEcProjectState.screenSetting.m_night_top3;
    ec_project_state_.screenSetting.m_night_top4 = defaultEcProjectState.screenSetting.m_night_top4;
    ec_project_state_.screenSetting.m_night_spar1 = defaultEcProjectState.screenSetting.m_night_spar1;
    ec_project_state_.screenSetting.m_night_spar2 = defaultEcProjectState.screenSetting.m_night_spar2;
    ec_project_state_.screenSetting.m_night_spar3 = defaultEcProjectState.screenSetting.m_night_spar3;
    ec_project_state_.screenSetting.m_night_spar4 = defaultEcProjectState.screenSetting.m_night_spar4;
    ec_project_state_.screenSetting.out_details = defaultEcProjectState.screenSetting.out_details;
    ec_project_state_.screenSetting.power_of_two = defaultEcProjectState.screenSetting.power_of_two;

    // preproc test section
    ec_project_state_.screenTest.test_sr = defaultEcProjectState.screenTest.test_sr;
    ec_project_state_.screenTest.test_ar = defaultEcProjectState.screenTest.test_ar;
    ec_project_state_.screenTest.test_do = defaultEcProjectState.screenTest.test_do;
    ec_project_state_.screenTest.test_al = defaultEcProjectState.screenTest.test_al;
    ec_project_state_.screenTest.test_sk = defaultEcProjectState.screenTest.test_sk;
    ec_project_state_.screenTest.test_ds = defaultEcProjectState.screenTest.test_ds;
    ec_project_state_.screenTest.test_tl = defaultEcProjectState.screenTest.test_tl;
    ec_project_state_.screenTest.test_aa = defaultEcProjectState.screenTest.test_aa;
    ec_project_state_.screenTest.test_ns = defaultEcProjectState.screenTest.test_ns;

    // preproc parameters section
    ec_project_state_.screenParam.aa_lim = defaultEcProjectState.screenParam.aa_lim;
    ec_project_state_.screenParam.aa_max = defaultEcProjectState.screenParam.aa_max;
    ec_project_state_.screenParam.aa_min = defaultEcProjectState.screenParam.aa_min;
    ec_project_state_.screenParam.al_co2_min = defaultEcProjectState.screenParam.al_co2_min;
    ec_project_state_.screenParam.al_co2_max = defaultEcProjectState.screenParam.al_co2_max;
    ec_project_state_.screenParam.al_h2o_min = defaultEcProjectState.screenParam.al_h2o_min;
    ec_project_state_.screenParam.al_h2o_max = defaultEcProjectState.screenParam.al_h2o_max;
    ec_project_state_.screenParam.al_ch4_min = defaultEcProjectState.screenParam.al_ch4_min;
    ec_project_state_.screenParam.al_ch4_max = defaultEcProjectState.screenParam.al_ch4_max;
    ec_project_state_.screenParam.al_other_min = defaultEcProjectState.screenParam.al_other_min;
    ec_project_state_.screenParam.al_other_max = defaultEcProjectState.screenParam.al_other_max;
    ec_project_state_.screenParam.al_tson_min = defaultEcProjectState.screenParam.al_tson_min;
    ec_project_state_.screenParam.al_tson_max = defaultEcProjectState.screenParam.al_tson_max;
    ec_project_state_.screenParam.al_u_max = defaultEcProjectState.screenParam.al_u_max;
    ec_project_state_.screenParam.al_w_max = defaultEcProjectState.screenParam.al_w_max;
    ec_project_state_.screenParam.ar_bins = defaultEcProjectState.screenParam.ar_bins;
    ec_project_state_.screenParam.ar_hf_lim = defaultEcProjectState.screenParam.ar_hf_lim;
    ec_project_state_.screenParam.ar_lim = defaultEcProjectState.screenParam.ar_lim;
    ec_project_state_.screenParam.ds_hf_uv = defaultEcProjectState.screenParam.ds_hf_uv;
    ec_project_state_.screenParam.ds_hf_w = defaultEcProjectState.screenParam.ds_hf_w;
    ec_project_state_.screenParam.ds_hf_t = defaultEcProjectState.screenParam.ds_hf_t;
    ec_project_state_.screenParam.ds_hf_co2 = defaultEcProjectState.screenParam.ds_hf_co2;
    ec_project_state_.screenParam.ds_hf_h2o = defaultEcProjectState.screenParam.ds_hf_h2o;
    ec_project_state_.screenParam.ds_hf_ch4 = defaultEcProjectState.screenParam.ds_hf_ch4;
    ec_project_state_.screenParam.ds_hf_other = defaultEcProjectState.screenParam.ds_hf_other;
    ec_project_state_.screenParam.ds_hf_var = defaultEcProjectState.screenParam.ds_hf_var;
    ec_project_state_.screenParam.ds_sf_uv = defaultEcProjectState.screenParam.ds_sf_uv;
    ec_project_state_.screenParam.ds_sf_w = defaultEcProjectState.screenParam.ds_sf_w;
    ec_project_state_.screenParam.ds_sf_t = defaultEcProjectState.screenParam.ds_sf_t;
    ec_project_state_.screenParam.ds_sf_co2 = defaultEcProjectState.screenParam.ds_sf_co2;
    ec_project_state_.screenParam.ds_sf_h2o = defaultEcProjectState.screenParam.ds_sf_h2o;
    ec_project_state_.screenParam.ds_sf_ch4 = defaultEcProjectState.screenParam.ds_sf_ch4;
    ec_project_state_.screenParam.ds_sf_other = defaultEcProjectState.screenParam.ds_sf_other;
    ec_project_state_.screenParam.ds_sf_var = defaultEcProjectState.screenParam.ds_sf_var;
    ec_project_state_.screenParam.despike_vm = defaultEcProjectState.screenParam.despike_vm;
    ec_project_state_.screenParam.do_extlim_dw = defaultEcProjectState.screenParam.do_extlim_dw;
    ec_project_state_.screenParam.do_hf1_lim = defaultEcProjectState.screenParam.do_hf1_lim;
    ec_project_state_.screenParam.do_hf2_lim = defaultEcProjectState.screenParam.do_hf2_lim;
    ec_project_state_.screenParam.ns_hf_lim = defaultEcProjectState.screenParam.ns_hf_lim;
    ec_project_state_.screenParam.sk_hf_kumax = defaultEcProjectState.screenParam.sk_hf_kumax;
    ec_project_state_.screenParam.sk_hf_kumin = defaultEcProjectState.screenParam.sk_hf_kumin;
    ec_project_state_.screenParam.sk_hf_skmax = defaultEcProjectState.screenParam.sk_hf_skmax;
    ec_project_state_.screenParam.sk_hf_skmin = defaultEcProjectState.screenParam.sk_hf_skmin;
    ec_project_state_.screenParam.sk_sf_kumax = defaultEcProjectState.screenParam.sk_sf_kumax;
    ec_project_state_.screenParam.sk_sf_kumin = defaultEcProjectState.screenParam.sk_sf_kumin;
    ec_project_state_.screenParam.sk_sf_skmax = defaultEcProjectState.screenParam.sk_sf_skmax;
    ec_project_state_.screenParam.sk_sf_skmin = defaultEcProjectState.screenParam.sk_sf_skmin;
    ec_project_state_.screenParam.sr_num_spk = defaultEcProjectState.screenParam.sr_num_spk;
    ec_project_state_.screenParam.sr_lim_u = defaultEcProjectState.screenParam.sr_lim_u;
    ec_project_state_.screenParam.sr_lim_w = defaultEcProjectState.screenParam.sr_lim_w;
    ec_project_state_.screenParam.sr_lim_co2 = defaultEcProjectState.screenParam.sr_lim_co2;
    ec_project_state_.screenParam.sr_lim_h2o = defaultEcProjectState.screenParam.sr_lim_h2o;
    ec_project_state_.screenParam.sr_lim_ch4 = defaultEcProjectState.screenParam.sr_lim_ch4;
    ec_project_state_.screenParam.sr_lim_other = defaultEcProjectState.screenParam.sr_lim_other;
    ec_project_state_.screenParam.sr_lim_hf = defaultEcProjectState.screenParam.sr_lim_hf;
    ec_project_state_.screenParam.tl_hf_lim = defaultEcProjectState.screenParam.tl_hf_lim;
    ec_project_state_.screenParam.tl_def_co2 = defaultEcProjectState.screenParam.tl_def_co2;
    ec_project_state_.screenParam.tl_def_h2o = defaultEcProjectState.screenParam.tl_def_h2o;
    ec_project_state_.screenParam.tl_def_ch4 = defaultEcProjectState.screenParam.tl_def_ch4;
    ec_project_state_.screenParam.tl_def_other = defaultEcProjectState.screenParam.tl_def_other;
    ec_project_state_.screenParam.tl_sf_lim = defaultEcProjectState.screenParam.tl_sf_lim;

    ec_project_state_.spectraSettings.start_sa_date = QDate(2000, 1, 1).toString(Qt::ISODate);
    ec_project_state_.spectraSettings.end_sa_date = QDate::currentDate().toString(Qt::ISODate);
    ec_project_state_.spectraSettings.start_sa_time = QTime(0, 0).toString(QStringLiteral("hh:mm"));
    ec_project_state_.spectraSettings.end_sa_time = QTime(23, 59).toString(QStringLiteral("hh:mm"));
    ec_project_state_.spectraSettings.sa_mode = defaultEcProjectState.spectraSettings.sa_mode;
    ec_project_state_.spectraSettings.sa_file.clear();
    ec_project_state_.spectraSettings.horst_lens = defaultEcProjectState.spectraSettings.horst_lens;
    ec_project_state_.spectraSettings.sa_min_smpl = defaultEcProjectState.spectraSettings.sa_min_smpl;
    ec_project_state_.spectraSettings.sa_fmin_co2 = defaultEcProjectState.spectraSettings.sa_fmin_co2;
    ec_project_state_.spectraSettings.sa_fmin_h2o = defaultEcProjectState.spectraSettings.sa_fmin_h2o;
    ec_project_state_.spectraSettings.sa_fmin_ch4 = defaultEcProjectState.spectraSettings.sa_fmin_ch4;
    ec_project_state_.spectraSettings.sa_fmin_other = defaultEcProjectState.spectraSettings.sa_fmin_other;
    ec_project_state_.spectraSettings.sa_fmax_co2 = defaultEcProjectState.spectraSettings.sa_fmax_co2;
    ec_project_state_.spectraSettings.sa_fmax_h2o = defaultEcProjectState.spectraSettings.sa_fmax_h2o;
    ec_project_state_.spectraSettings.sa_fmax_ch4 = defaultEcProjectState.spectraSettings.sa_fmax_ch4;
    ec_project_state_.spectraSettings.sa_fmax_other = defaultEcProjectState.spectraSettings.sa_fmax_other;
    ec_project_state_.spectraSettings.sa_hfn_co2_fmin = defaultEcProjectState.spectraSettings.sa_hfn_co2_fmin;
    ec_project_state_.spectraSettings.sa_hfn_h2o_fmin = defaultEcProjectState.spectraSettings.sa_hfn_h2o_fmin;
    ec_project_state_.spectraSettings.sa_hfn_ch4_fmin = defaultEcProjectState.spectraSettings.sa_hfn_ch4_fmin;
    ec_project_state_.spectraSettings.sa_hfn_other_fmin = defaultEcProjectState.spectraSettings.sa_hfn_other_fmin;
    ec_project_state_.spectraSettings.add_sonic_lptf = defaultEcProjectState.spectraSettings.add_sonic_lptf;
    ec_project_state_.spectraSettings.sa_min_un_ustar = defaultEcProjectState.spectraSettings.sa_min_un_ustar;
    ec_project_state_.spectraSettings.sa_min_un_h = defaultEcProjectState.spectraSettings.sa_min_un_h;
    ec_project_state_.spectraSettings.sa_min_un_le = defaultEcProjectState.spectraSettings.sa_min_un_le;
    ec_project_state_.spectraSettings.sa_min_un_co2 = defaultEcProjectState.spectraSettings.sa_min_un_co2;
    ec_project_state_.spectraSettings.sa_min_un_ch4 = defaultEcProjectState.spectraSettings.sa_min_un_ch4;
    ec_project_state_.spectraSettings.sa_min_un_other = defaultEcProjectState.spectraSettings.sa_min_un_other;
    ec_project_state_.spectraSettings.sa_min_st_ustar = defaultEcProjectState.spectraSettings.sa_min_st_ustar;
    ec_project_state_.spectraSettings.sa_min_st_h = defaultEcProjectState.spectraSettings.sa_min_st_h;
    ec_project_state_.spectraSettings.sa_min_st_le = defaultEcProjectState.spectraSettings.sa_min_st_le;
    ec_project_state_.spectraSettings.sa_min_st_co2 = defaultEcProjectState.spectraSettings.sa_min_st_co2;
    ec_project_state_.spectraSettings.sa_min_st_ch4 = defaultEcProjectState.spectraSettings.sa_min_st_ch4;
    ec_project_state_.spectraSettings.sa_min_st_other = defaultEcProjectState.spectraSettings.sa_min_st_other;
    ec_project_state_.spectraSettings.sa_max_ustar = defaultEcProjectState.spectraSettings.sa_max_ustar;
    ec_project_state_.spectraSettings.sa_max_h = defaultEcProjectState.spectraSettings.sa_max_h;
    ec_project_state_.spectraSettings.sa_max_le = defaultEcProjectState.spectraSettings.sa_max_le;
    ec_project_state_.spectraSettings.sa_max_co2 = defaultEcProjectState.spectraSettings.sa_max_co2;
    ec_project_state_.spectraSettings.sa_max_ch4 = defaultEcProjectState.spectraSettings.sa_max_ch4;
    ec_project_state_.spectraSettings.sa_max_other = defaultEcProjectState.spectraSettings.sa_max_other;
    ec_project_state_.spectraSettings.ex_file.clear();
    ec_project_state_.spectraSettings.sa_bin_spectra.clear();
    ec_project_state_.spectraSettings.sa_full_spectra.clear();
    ec_project_state_.spectraSettings.ex_dir.clear();
    ec_project_state_.spectraSettings.subset = defaultEcProjectState.spectraSettings.subset;
    ec_project_state_.spectraSettings.use_vm_flags = defaultEcProjectState.spectraSettings.use_vm_flags;
    ec_project_state_.spectraSettings.use_foken_low = defaultEcProjectState.spectraSettings.use_foken_low;
    ec_project_state_.spectraSettings.use_foken_mid = defaultEcProjectState.spectraSettings.use_foken_mid;
    ec_project_state_.spectraSettings.flux_run_mode = defaultEcProjectState.spectraSettings.flux_run_mode;
    ec_project_state_.spectraSettings.automatic_spectra_config = defaultEcProjectState.spectraSettings.automatic_spectra_config;

    ec_project_state_.screenTilt.start_date = QDate(2000, 1, 1).toString(Qt::ISODate);
    ec_project_state_.screenTilt.end_date = QDate::currentDate().toString(Qt::ISODate);
    ec_project_state_.screenTilt.start_time = QTime(0, 0).toString(QStringLiteral("hh:mm"));
    ec_project_state_.screenTilt.end_time = QTime(23, 59).toString(QStringLiteral("hh:mm"));
    ec_project_state_.screenTilt.mode = defaultEcProjectState.screenTilt.mode;
    ec_project_state_.screenTilt.north_offset = defaultEcProjectState.screenTilt.north_offset;
    ec_project_state_.screenTilt.min_num_per_sec = defaultEcProjectState.screenTilt.min_num_per_sec;
    ec_project_state_.screenTilt.w_max = defaultEcProjectState.screenTilt.w_max;
    ec_project_state_.screenTilt.u_min = defaultEcProjectState.screenTilt.u_min;
    ec_project_state_.screenTilt.file.clear();
    ec_project_state_.screenTilt.fix_policy = defaultEcProjectState.screenTilt.fix_policy;
    ec_project_state_.screenTilt.angles.clear();
    ec_project_state_.screenTilt.subset = defaultEcProjectState.screenTilt.subset;
    ec_project_state_.screenTilt.assessment_only = defaultEcProjectState.screenTilt.assessment_only;

    ec_project_state_.timelagOpt.start_date = QDate(2000, 1, 1).toString(Qt::ISODate);
    ec_project_state_.timelagOpt.end_date = QDate::currentDate().toString(Qt::ISODate);
    ec_project_state_.timelagOpt.start_time = QTime(0, 0).toString(QStringLiteral("hh:mm"));
    ec_project_state_.timelagOpt.end_time = QTime(23, 59).toString(QStringLiteral("hh:mm"));
    ec_project_state_.timelagOpt.mode = defaultEcProjectState.timelagOpt.mode;
    ec_project_state_.timelagOpt.file.clear();
    ec_project_state_.timelagOpt.pg_range = defaultEcProjectState.timelagOpt.pg_range;
    ec_project_state_.timelagOpt.le_min_flux = defaultEcProjectState.timelagOpt.le_min_flux;
    ec_project_state_.timelagOpt.to_h2o_nclass = defaultEcProjectState.timelagOpt.to_h2o_nclass;
    ec_project_state_.timelagOpt.co2_min_flux = defaultEcProjectState.timelagOpt.co2_min_flux;
    ec_project_state_.timelagOpt.ch4_min_flux = defaultEcProjectState.timelagOpt.ch4_min_flux;
    ec_project_state_.timelagOpt.gas4_min_flux = defaultEcProjectState.timelagOpt.gas4_min_flux;
    ec_project_state_.timelagOpt.co2_min_lag = defaultEcProjectState.timelagOpt.co2_min_lag;
    ec_project_state_.timelagOpt.co2_max_lag = defaultEcProjectState.timelagOpt.co2_max_lag;
    ec_project_state_.timelagOpt.h2o_min_lag = defaultEcProjectState.timelagOpt.h2o_min_lag;
    ec_project_state_.timelagOpt.h2o_max_lag = defaultEcProjectState.timelagOpt.h2o_max_lag;
    ec_project_state_.timelagOpt.ch4_min_lag = defaultEcProjectState.timelagOpt.ch4_min_lag;
    ec_project_state_.timelagOpt.ch4_max_lag = defaultEcProjectState.timelagOpt.ch4_max_lag;
    ec_project_state_.timelagOpt.gas4_min_lag = defaultEcProjectState.timelagOpt.gas4_min_lag;
    ec_project_state_.timelagOpt.gas4_max_lag = defaultEcProjectState.timelagOpt.gas4_max_lag;
    ec_project_state_.timelagOpt.subset  = defaultEcProjectState.timelagOpt.subset;
    ec_project_state_.timelagOpt.assessment_only = defaultEcProjectState.timelagOpt.assessment_only;

    ec_project_state_.randomError.ru_method = defaultEcProjectState.randomError.ru_method;
    ec_project_state_.randomError.its_method = defaultEcProjectState.randomError.its_method;
    ec_project_state_.randomError.its_tlag_max = defaultEcProjectState.randomError.its_tlag_max;
    ec_project_state_.randomError.its_sec_factor = defaultEcProjectState.randomError.its_sec_factor;

    ec_project_state_.biomParam.native_header = defaultEcProjectState.biomParam.native_header;
    ec_project_state_.biomParam.hlines = defaultEcProjectState.biomParam.hlines;
    ec_project_state_.biomParam.separator = defaultEcProjectState.biomParam.separator;
    ec_project_state_.biomParam.tstamp_ref = defaultEcProjectState.biomParam.tstamp_ref;
    ec_project_state_.biomParam.col_ta = defaultEcProjectState.biomParam.col_ta;
    ec_project_state_.biomParam.col_pa = defaultEcProjectState.biomParam.col_pa;
    ec_project_state_.biomParam.col_rh = defaultEcProjectState.biomParam.col_rh;
    ec_project_state_.biomParam.col_ppfd = defaultEcProjectState.biomParam.col_ppfd;
    ec_project_state_.biomParam.col_rg = defaultEcProjectState.biomParam.col_rg;
    ec_project_state_.biomParam.col_lwin = defaultEcProjectState.biomParam.col_lwin;

    setModified(false); // new documents are not in a modified state
    emit ecProjectNew();
}

// Save an ec project
/// Write the gas, cell and diagnostic records into the open [Project] group.
///
/// Only the records that exist are written: a project with four gases carries
/// gas_1_* .. gas_4_* and nothing else, so the file does not describe columns
/// the site does not have.
///
/// Every gas_*, cell_* and diag_* key is removed first. Shrinking a list
/// otherwise leaves orphaned keys behind for the reader to pick back up -
/// which is exactly what pf_sect_* and wdf_sect_* do today.
/// Build records from the legacy col_* fields, for a project written before
/// records existed.
///
/// Instrument ids are left empty and the moisture reference left on auto:
/// neither can be recovered from the project file alone, since col_* names a
/// raw column number and nothing else. BasicSettingsPage fills the instrument
/// in once the metadata is parsed, and auto resolves to the single H2O that
/// such a project necessarily has - which is the behaviour it had before.
void EcProject::migrateLegacyColumnsToRecords()
{
    auto& g = ec_project_state_.projectGeneral;
    if (!g.gasColumns.isEmpty()) { return; }

    const auto addGas = [&](const QString& slug, int col)
    {
        // Only gases the legacy file actually named. The four slots used to be
        // appended whether or not the project had them, so that record i stayed
        // the engine's slot firstGas+i-1 - but nothing reads species from a
        // position any more, on either side, and an absent gas cost a column of
        // error codes in every output. migrateLegacyGasSettings() finds the
        // flat thresholds by species now rather than by index.
        if (col <= 0) { return; }
        GasRecord rec;
        rec.slug = slug;
        rec.rawColumn = col;
        g.gasColumns.append(rec);
    };
    addGas(QStringLiteral("co2"), g.col_co2);
    addGas(QStringLiteral("h2o"), g.col_h2o);
    addGas(QStringLiteral("ch4"), g.col_ch4);
    // The fourth slot holds whichever gas the site measured; its species is
    // only recoverable from the metadata, so it is resolved later and left
    // blank rather than guessed at here.
    addGas(QString(), g.col_gas4);
    if (g.gas_mw >= 0.0) { g.gasColumns.last().mw = g.gas_mw; }
    if (g.gas_diff >= 0.0) { g.gasColumns.last().diff = g.gas_diff; }

    const auto addPlain = [](QVector<MeasurementRecord>& recs,
                             const QString& slug, int col)
    {
        if (col <= 0) { return; }
        MeasurementRecord rec;
        rec.slug = slug;
        rec.rawColumn = col;
        recs.append(rec);
    };
    addPlain(g.cellColumns, QStringLiteral("cell_t"), g.col_int_t_c);
    addPlain(g.cellColumns, QStringLiteral("int_t_1"), g.col_int_t_1);
    addPlain(g.cellColumns, QStringLiteral("int_t_2"), g.col_int_t_2);
    addPlain(g.cellColumns, QStringLiteral("int_p"), g.col_int_p);
    addPlain(g.diagColumns, QStringLiteral("diag_75"), g.col_diag_75);
    addPlain(g.diagColumns, QStringLiteral("diag_72"), g.col_diag_72);
    addPlain(g.diagColumns, QStringLiteral("diag_77"), g.col_diag_77);
    addPlain(g.diagColumns, QStringLiteral("diag_anem"), g.col_diag_anem);
}

/// The Conditional Eddy Covariance pairings, beside the gas records they
/// index into.
///
/// An empty list writes nothing at all, not `cec_num=0`: those two say
/// different things to the engine. Absent means "you decide", and it derives
/// one pairing per carbon channel from the analyser layout; zero means "none",
/// and it runs no partition. The interface writes the list as soon as the
/// table is touched, so what the file states is what the user chose.
void EcProject::writeCecPairs(QSettings& project_ini)
{
    const auto& pairs = ec_project_state_.projectGeneral.cecPairs;

    const auto stale = project_ini.childKeys().filter(
        QRegularExpression(QStringLiteral("^cec_\\d+_")));
    for (const auto& key : stale) { project_ini.remove(key); }
    project_ini.remove(EcIni::INI_PROJECT_CEC_NUM);
    if (pairs.isEmpty()) { return; }

    project_ini.setValue(EcIni::INI_PROJECT_CEC_NUM, pairs.size());
    for (int i = 0; i < pairs.size(); ++i)
    {
        const auto p = QStringLiteral("cec_%1_").arg(i + 1);
        const auto& pair = pairs.at(i);
        project_ini.setValue(p + QStringLiteral("meth"), pair.meth);
        project_ini.setValue(p + QStringLiteral("co2"), pair.carbonIndex);
        project_ini.setValue(p + QStringLiteral("h2o"), pair.waterIndex);
        QStringList extras;
        for (int idx : pair.extraIndices)
        {
            if (idx > 0) { extras << QString::number(idx); }
        }
        project_ini.setValue(p + QStringLiteral("extra"),
                             extras.join(QLatin1Char(',')));
    }
}

void EcProject::readCecPairs(QSettings& project_ini)
{
    auto& pairs = ec_project_state_.projectGeneral.cecPairs;
    pairs.clear();

    const int num = project_ini.value(EcIni::INI_PROJECT_CEC_NUM, 0).toInt();
    for (int i = 1; i <= num; ++i)
    {
        const auto p = QStringLiteral("cec_%1_").arg(i);
        CecPairRecord pair;
        pair.meth = project_ini.value(p + QStringLiteral("meth"), 1).toInt();
        pair.carbonIndex = project_ini.value(p + QStringLiteral("co2"), 0).toInt();
        pair.waterIndex = project_ini.value(p + QStringLiteral("h2o"), 0).toInt();
        const auto extras = project_ini.value(p + QStringLiteral("extra"))
                                .toString()
                                .split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const auto& token : extras)
        {
            bool ok = false;
            const int idx = token.trimmed().toInt(&ok);
            if (ok && idx > 0) { pair.extraIndices.append(idx); }
        }
        pairs.append(pair);
    }
}

void EcProject::writeMeasurementRecords(QSettings& project_ini)
{
    const auto& g = ec_project_state_.projectGeneral;

    const auto stale = project_ini.childKeys().filter(
        QRegularExpression(QStringLiteral("^(gas|cell|diag|agc)_")));
    for (const auto& key : stale) { project_ini.remove(key); }

    //> The retired legacy keys go too, not just the record keys. QSettings
    //> preserves whatever it is not asked to overwrite, so an upgraded
    //> project would otherwise keep col_co2 = 7 beside its records for ever
    //> - the same orphan problem the record keys are cleared to avoid, and
    //> confusing to anyone reading the file.
    //>
    //> col_air_t, col_air_p and col_ts are deliberately absent from this
    //> list: they are still live keys.
    for (const auto& retired : { EcIni::INI_PROJECT_18, EcIni::INI_PROJECT_19,
                                 EcIni::INI_PROJECT_20, EcIni::INI_PROJECT_21,
                                 EcIni::INI_PROJECT_22, EcIni::INI_PROJECT_23,
                                 EcIni::INI_PROJECT_24, EcIni::INI_PROJECT_27,
                                 EcIni::INI_PROJECT_28, EcIni::INI_PROJECT_29,
                                 EcIni::INI_PROJECT_30, EcIni::INI_PROJECT_69,
                                 EcIni::INI_PROJECT_31, EcIni::INI_PROJECT_32 })
    {
        project_ini.remove(retired);
    }

    if (g.gasColumns.isEmpty() && g.cellColumns.isEmpty()
        && g.diagColumns.isEmpty() && g.agcColumns.isEmpty())
    {
        return;
    }

    //> Every record is written, because by this point every record is a real
    //> measurement: the list is compacted when it is read and no longer grows
    //> holes, so there is nothing here to filter. Filtering *here* would be
    //> the wrong place anyway - the five per-gas key blocks are written from
    //> their own loops over the same list, and a skip in one of them would
    //> shift gas_<N>_col away from gas_<N>_sr_lim.
    project_ini.setValue(QStringLiteral("gas_num"), g.gasColumns.size());
    for (int i = 0; i < g.gasColumns.size(); ++i)
    {
        const auto& rec = g.gasColumns.at(i);
        const auto p = QStringLiteral("gas_%1_").arg(i + 1);
        project_ini.setValue(p + QStringLiteral("var"), rec.slug);
        project_ini.setValue(p + QStringLiteral("instr"), rec.instrumentId);
        project_ini.setValue(p + QStringLiteral("col"), rec.rawColumn);
        project_ini.setValue(p + QStringLiteral("moist"), rec.moistureRef);
        project_ini.setValue(p + QStringLiteral("cell"), rec.cellRef);
        // Written empty rather than omitted when there is no override, so the
        // record keeps a uniform shape for the engine's stride reader.
        project_ini.setValue(p + QStringLiteral("mw"),
            rec.mw >= 0.0 ? QString::number(rec.mw, 'f', 4) : QString());
        project_ini.setValue(p + QStringLiteral("diff"),
            rec.diff >= 0.0 ? QString::number(rec.diff, 'f', 5) : QString());
    }

    const auto writePlain = [&](const QString& prefix,
                                const QVector<MeasurementRecord>& recs)
    {
        project_ini.setValue(prefix + QStringLiteral("_num"), recs.size());
        for (int i = 0; i < recs.size(); ++i)
        {
            const auto p = QStringLiteral("%1_%2_").arg(prefix).arg(i + 1);
            project_ini.setValue(p + QStringLiteral("var"), recs.at(i).slug);
            project_ini.setValue(p + QStringLiteral("instr"),
                                 recs.at(i).instrumentId);
            project_ini.setValue(p + QStringLiteral("col"),
                                 recs.at(i).rawColumn);
        }
    };
    writePlain(QStringLiteral("cell"), g.cellColumns);
    writePlain(QStringLiteral("diag"), g.diagColumns);
    //> Always stated, even as agc_num=0. Absent means "this file predates the
    //> records", and the engine then falls back to matching a column named
    //> AGC or RSSI by name; zero means "this site declares no signal
    //> strength", which is a different thing and the file should be able to
    //> say it.
    writePlain(QStringLiteral("agc"), g.agcColumns);
}

/// Read the records back. Absent gas_num means a project written before
/// records existed; the caller migrates it from the legacy col_* fields.
bool EcProject::readMeasurementRecords(QSettings& project_ini)
{
    auto& g = ec_project_state_.projectGeneral;
    g.gasColumns.clear();
    g.cellColumns.clear();
    g.diagColumns.clear();
    g.agcColumns.clear();

    const int gasNum = project_ini.value(QStringLiteral("gas_num"), 0).toInt();
    if (gasNum <= 0) { return false; }

    for (int i = 1; i <= gasNum; ++i)
    {
        const auto p = QStringLiteral("gas_%1_").arg(i);
        GasRecord rec;
        rec.slug = project_ini.value(p + QStringLiteral("var")).toString();
        rec.instrumentId =
            project_ini.value(p + QStringLiteral("instr")).toString();
        rec.rawColumn =
            project_ini.value(p + QStringLiteral("col"), -1).toInt();
        rec.moistureRef =
            project_ini.value(p + QStringLiteral("moist"), 0).toInt();
        rec.cellRef = project_ini.value(p + QStringLiteral("cell"), 0).toInt();
        const auto mw = project_ini.value(p + QStringLiteral("mw")).toString();
        const auto diff =
            project_ini.value(p + QStringLiteral("diff")).toString();
        rec.mw = mw.isEmpty() ? -1.0 : mw.toDouble();
        rec.diff = diff.isEmpty() ? -1.0 : diff.toDouble();
        g.gasColumns.append(rec);
    }

    const auto readPlain = [&](const QString& prefix,
                               QVector<MeasurementRecord>& recs)
    {
        const int n =
            project_ini.value(prefix + QStringLiteral("_num"), 0).toInt();
        for (int i = 1; i <= n; ++i)
        {
            const auto p = QStringLiteral("%1_%2_").arg(prefix).arg(i);
            MeasurementRecord rec;
            rec.slug = project_ini.value(p + QStringLiteral("var")).toString();
            rec.instrumentId =
                project_ini.value(p + QStringLiteral("instr")).toString();
            rec.rawColumn =
                project_ini.value(p + QStringLiteral("col"), -1).toInt();
            recs.append(rec);
        }
    };
    readPlain(QStringLiteral("cell"), g.cellColumns);
    readPlain(QStringLiteral("diag"), g.diagColumns);
    readPlain(QStringLiteral("agc"), g.agcColumns);

    MeasurementRecords::validateReferences(g.gasColumns, g.cellColumns);
    return true;
}

bool EcProject::saveEcProject(const QString &filename)
{
    // try to open file just for checking
    QFile datafile(filename);
    if (!datafile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        // error opening file
        qWarning() << "Error: Cannot open file" << filename;
        WidgetUtils::warning(nullptr,
                             tr("Write Project Error"),
                             tr("Cannot write file %1:\n%2")
                             .arg(filename)
                             .arg(datafile.errorString()));
        datafile.close();
        return false;
    }
    datafile.close();

    QDateTime now = QDateTime::currentDateTime();
    QString now_str = now.toString(Qt::ISODate);
    QFileInfo fileinfo = QFileInfo(filename);

    QSettings project_ini(filename, QSettings::IniFormat);

    // general section
    project_ini.beginGroup(EcIni::INIGROUP_PROJECT);
        project_ini.setValue(EcIni::INI_PROJECT_0, ec_project_state_.projectGeneral.creation_date);
        project_ini.setValue(EcIni::INI_PROJECT_1, now_str);
        project_ini.setValue(EcIni::INI_PROJECT_2, fileinfo.absoluteFilePath());
        project_ini.setValue(EcIni::INI_PROJECT_33, QVariant::fromValue(ec_project_state_.projectGeneral.run_mode).toInt());
        project_ini.setValue(EcIni::INI_PROJECT_40, QVariant(ec_project_state_.projectGeneral.run_fcc).toInt());
        project_ini.setValue(EcIni::INI_PROJECT_3, QDir::fromNativeSeparators(ec_project_state_.projectGeneral.project_title));

        // update sw version if empty or old
        if (ec_project_state_.projectGeneral.sw_version.isEmpty()
            || ec_project_state_.projectGeneral.sw_version != Defs::APP_VERSION_STR)
        {
            project_ini.setValue(EcIni::INI_PROJECT_4, Defs::APP_VERSION_STR);
        }
        else
        {
            project_ini.setValue(EcIni::INI_PROJECT_4, ec_project_state_.projectGeneral.sw_version);
        }

        // update ini version if empty or old
        if (ec_project_state_.projectGeneral.ini_version.isEmpty()
            || ec_project_state_.projectGeneral.ini_version != Defs::PROJECT_FILE_VERSION_STR)
        {
            project_ini.setValue(EcIni::INI_PROJECT_5, Defs::PROJECT_FILE_VERSION_STR);
        }
        else
        {
            project_ini.setValue(EcIni::INI_PROJECT_5, ec_project_state_.projectGeneral.ini_version);
        }
        project_ini.setValue(EcIni::INI_PROJECT_6, ec_project_state_.projectGeneral.project_id);
        project_ini.setValue(EcIni::INI_PROJECT_7, QVariant::fromValue(ec_project_state_.projectGeneral.file_type).toInt());
        project_ini.setValue(EcIni::INI_PROJECT_8, ec_project_state_.projectGeneral.file_prototype);
        project_ini.setValue(EcIni::INI_PROJECT_9, QVariant(ec_project_state_.projectGeneral.use_alt_md_file).toInt());
        project_ini.setValue(EcIni::INI_PROJECT_10, QDir::fromNativeSeparators(ec_project_state_.projectGeneral.md_file));
        project_ini.setValue(EcIni::INI_PROJECT_11, QVariant(ec_project_state_.projectGeneral.use_tlfile).toInt());
        project_ini.setValue(EcIni::INI_PROJECT_12, QDir::fromNativeSeparators(ec_project_state_.projectGeneral.timeline_file));
        project_ini.setValue(EcIni::INI_PROJECT_13, ec_project_state_.projectGeneral.binary_hnlines);
        project_ini.setValue(EcIni::INI_PROJECT_14, ec_project_state_.projectGeneral.binary_eol);
        project_ini.setValue(EcIni::INI_PROJECT_15, ec_project_state_.projectGeneral.binary_nbytes);
        project_ini.setValue(EcIni::INI_PROJECT_16, ec_project_state_.projectGeneral.binary_little_end);
        project_ini.setValue(EcIni::INI_PROJECT_17, ec_project_state_.projectGeneral.master_sonic);
        //> col_co2 .. col_diag_anem, gas_mw and gas_diff are **not written any
        //> more** - the records below carry all of it, and carry what those
        //> keys never could: which analyser each column came from, and more
        //> than one measurement of the same species.
        //>
        //> They are still *read*: migrateLegacyColumnsToRecords() needs them
        //> to upgrade a file written before records existed. Reading without
        //> writing is the whole shape of the retirement.
        //>
        //> col_air_t, col_air_p and col_ts stay - ambient temperature and
        //> pressure and the sonic temperature are one per project, not one
        //> per instrument, and are out of scope.
        project_ini.setValue(EcIni::INI_PROJECT_25, ec_project_state_.projectGeneral.col_air_t);
        project_ini.setValue(EcIni::INI_PROJECT_26, ec_project_state_.projectGeneral.col_air_p);
        project_ini.setValue(EcIni::INI_PROJECT_36, ec_project_state_.projectGeneral.col_ts);
        writeMeasurementRecords(project_ini);
        project_ini.setValue(EcIni::INI_PROJECT_39, ec_project_state_.projectGeneral.out_rich);
        project_ini.setValue(EcIni::INI_PROJECT_70, ec_project_state_.projectGeneral.fluxnet_standardize_biomet);
        project_ini.setValue(EcIni::INI_PROJECT_71, ec_project_state_.projectGeneral.fluxnet_err_label);
        project_ini.setValue(EcIni::INI_PROJECT_56, ec_project_state_.projectGeneral.out_md);
        project_ini.setValue(EcIni::INI_PROJECT_41, QVariant(ec_project_state_.projectGeneral.make_dataset).toInt());
        project_ini.setValue(EcIni::INI_PROJECT_54, ec_project_state_.projectGeneral.subset);
        project_ini.setValue(EcIni::INI_PROJECT_42, ec_project_state_.projectGeneral.start_date);
        project_ini.setValue(EcIni::INI_PROJECT_44, ec_project_state_.projectGeneral.start_time);
        project_ini.setValue(EcIni::INI_PROJECT_43, ec_project_state_.projectGeneral.end_date);
        project_ini.setValue(EcIni::INI_PROJECT_45, ec_project_state_.projectGeneral.end_time);
        project_ini.setValue(EcIni::INI_PROJECT_46, ec_project_state_.projectGeneral.hf_meth);
        project_ini.setValue(EcIni::INI_PROJECT_47, ec_project_state_.projectGeneral.lf_meth);
        project_ini.setValue(EcIni::INI_PROJECT_48, ec_project_state_.projectGeneral.wpl_meth);
        project_ini.setValue(EcIni::INI_PROJECT_49, ec_project_state_.projectGeneral.foot_meth);
        project_ini.setValue(EcIni::INI_PROJECT_72, ec_project_state_.projectGeneral.cec_meth);
        project_ini.setValue(EcIni::INI_PROJECT_73, QString::number(ec_project_state_.projectGeneral.cec_h, 'f', 3));
        project_ini.setValue(EcIni::INI_PROJECT_74, QString::number(ec_project_state_.projectGeneral.cec_min_o1_o2, 'f', 1));
        project_ini.setValue(EcIni::INI_PROJECT_75, QString::number(ec_project_state_.projectGeneral.cec_min_octant, 'f', 1));
        project_ini.setValue(EcIni::INI_PROJECT_76, QString::number(ec_project_state_.projectGeneral.cec_min_valid, 'f', 1));
        project_ini.setValue(EcIni::INI_PROJECT_77, QString::number(ec_project_state_.projectGeneral.cec_signal_strength, 'f', 1));
        project_ini.setValue(EcIni::INI_PROJECT_78, ec_project_state_.projectGeneral.cec_max_gap_fill);
        project_ini.setValue(EcIni::INI_PROJECT_79, QString::number(ec_project_state_.projectGeneral.cec_max_stationarity, 'f', 1));
        project_ini.setValue(EcIni::INI_PROJECT_80, QString::number(ec_project_state_.projectGeneral.cec_singular_band, 'f', 3));
        project_ini.setValue(EcIni::INI_PROJECT_81, ec_project_state_.projectGeneral.cec_stationarity_mode);
        writeCecPairs(project_ini);
        project_ini.setValue(EcIni::INI_PROJECT_50, ec_project_state_.projectGeneral.tob1_format);
        project_ini.setValue(EcIni::INI_PROJECT_51, QDir::fromNativeSeparators(ec_project_state_.projectGeneral.out_path));
        project_ini.setValue(EcIni::INI_PROJECT_52, ec_project_state_.projectGeneral.fix_out_format);
        project_ini.setValue(EcIni::INI_PROJECT_53, ec_project_state_.projectGeneral.err_label);
        project_ini.setValue(EcIni::INI_PROJECT_55, ec_project_state_.projectGeneral.qcflag_meth);
        project_ini.setValue(EcIni::INI_PROJECT_34, QVariant(ec_project_state_.projectGeneral.use_biomet).toInt());
        project_ini.setValue(EcIni::INI_PROJECT_35, QDir::fromNativeSeparators(ec_project_state_.projectGeneral.biom_file));
        project_ini.setValue(EcIni::INI_PROJECT_57, QDir::fromNativeSeparators(ec_project_state_.projectGeneral.biom_dir));
        project_ini.setValue(EcIni::INI_PROJECT_58, ec_project_state_.projectGeneral.biom_recurse);
        project_ini.setValue(EcIni::INI_PROJECT_59, QVariant(QString(QLatin1Char('.')) + ec_project_state_.projectGeneral.biom_ext));
        project_ini.setValue(EcIni::INI_PROJECT_60, ec_project_state_.projectGeneral.out_mean_cosp);
        project_ini.setValue(EcIni::INI_PROJECT_61, ec_project_state_.projectGeneral.out_biomet);
        project_ini.setValue(EcIni::INI_PROJECT_62, ec_project_state_.projectGeneral.bin_sp_avail);
        project_ini.setValue(EcIni::INI_PROJECT_63, ec_project_state_.projectGeneral.full_sp_avail);
        project_ini.setValue(EcIni::INI_PROJECT_64, ec_project_state_.projectGeneral.files_found);
        project_ini.setValue(EcIni::INI_PROJECT_65, ec_project_state_.projectGeneral.out_mean_spectra);
        project_ini.setValue(EcIni::INI_PROJECT_66, ec_project_state_.projectGeneral.hf_correct_ghg_ba);
        project_ini.setValue(EcIni::INI_PROJECT_67, ec_project_state_.projectGeneral.hf_correct_ghg_zoh);
        project_ini.setValue(EcIni::INI_PROJECT_68, ec_project_state_.projectGeneral.sonic_output_rate);
    project_ini.endGroup();

    // spec settings section
    project_ini.beginGroup(EcIni::INIGROUP_SPEC_SETTINGS);
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_0, ec_project_state_.spectraSettings.start_sa_date);
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_1, ec_project_state_.spectraSettings.end_sa_date);
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_50, ec_project_state_.spectraSettings.start_sa_time);
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_51, ec_project_state_.spectraSettings.end_sa_time);
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_2, ec_project_state_.spectraSettings.sa_mode);
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_3, QDir::fromNativeSeparators(ec_project_state_.spectraSettings.sa_file));
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_4, ec_project_state_.spectraSettings.sa_min_smpl);
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_20, formatSpectraQcMinimum(ec_project_state_.spectraSettings.sa_min_st_le));
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_21, formatSpectraQcMinimum(ec_project_state_.spectraSettings.sa_min_st_h));
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_22, ec_project_state_.spectraSettings.add_sonic_lptf);
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_28, ec_project_state_.spectraSettings.horst_lens);
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_29, QDir::fromNativeSeparators(ec_project_state_.spectraSettings.ex_file));
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_30, QDir::fromNativeSeparators(ec_project_state_.spectraSettings.sa_bin_spectra));
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_31, QDir::fromNativeSeparators(ec_project_state_.spectraSettings.sa_full_spectra));
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_32, QDir::fromNativeSeparators(ec_project_state_.spectraSettings.ex_dir));
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_33, ec_project_state_.spectraSettings.subset);
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_34, ec_project_state_.spectraSettings.use_vm_flags);
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_35, formatSpectraQcMinimum(ec_project_state_.spectraSettings.sa_min_st_ustar));
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_36, formatSpectraQcMinimum(ec_project_state_.spectraSettings.sa_min_un_ustar));
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_37, formatSpectraQcMinimum(ec_project_state_.spectraSettings.sa_min_un_h));
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_38, formatSpectraQcMinimum(ec_project_state_.spectraSettings.sa_min_un_le));
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_42, formatSpectraQcMinimum(ec_project_state_.spectraSettings.sa_max_ustar));
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_43, formatSpectraQcMinimum(ec_project_state_.spectraSettings.sa_max_h));
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_44, formatSpectraQcMinimum(ec_project_state_.spectraSettings.sa_max_le));
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_48, ec_project_state_.spectraSettings.use_foken_low);
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_49, ec_project_state_.spectraSettings.use_foken_mid);
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_52, ec_project_state_.spectraSettings.flux_run_mode);
        project_ini.setValue(EcIni::INI_SPEC_SETTINGS_53, ec_project_state_.spectraSettings.automatic_spectra_config);

        //> The three SA-group tables that used to be written here as
        //> placeholders - sa_co2_g1_start/stop and the ch4 and gas4 pairs,
        //> always 1 and 12 - are retired. A gas states its own months as
        //> gas_<i>_sa_months below, which reaches every gas rather than the
        //> three the tables were labelled for.

        //> Per-gas spectral settings, for the gases the flat keys above
        //> cannot reach. These are FCC SNTags and ReadIniFCC only sweeps
        //> sections named FluxCorrection*, so they belong here rather than in
        //> [Project] - the FCC counterpart of the RawProcess* rule.
        //>
        //> Written only where the record holds a real value: the engine
        //> applies a record override whenever the tag is *present*, so
        //> emitting a sentinel would replace the legacy setting with one.
        {
            const auto staleGas = project_ini.childKeys().filter(
                QRegularExpression(QStringLiteral("^gas_\\d+_sa_")));
            for (const auto& key : staleGas) { project_ini.remove(key); }

            //> The three retired SA-group tables, 72 keys, swept for the same
            //> reason the retired [Project] keys are: QSettings keeps what it
            //> is not asked to overwrite, so an upgraded project would carry
            //> sa_co2_g1_start beside gas_1_sa_months for ever - and the
            //> engine has blanked those tags, so they would describe nothing.
            const auto staleGroups = project_ini.childKeys().filter(
                QRegularExpression(
                    QStringLiteral("^sa_(co2|ch4|gas4)_g\\d+_(start|stop)$")));
            for (const auto& key : staleGroups) { project_ini.remove(key); }

            const auto& gases = ec_project_state_.projectGeneral.gasColumns;
            for (int i = 0; i < gases.size(); ++i)
            {
                const auto p = QStringLiteral("gas_%1_sa_").arg(i + 1);
                const auto& proc = gases.at(i).proc;
                if (proc.saFmin >= 0.0)
                {
                    project_ini.setValue(p + QStringLiteral("fmin"),
                                         QString::number(proc.saFmin, 'f', 4));
                }
                if (proc.saFmax >= 0.0)
                {
                    project_ini.setValue(p + QStringLiteral("fmax"),
                                         QString::number(proc.saFmax, 'f', 4));
                }
                if (proc.saHfnFmin >= 0.0)
                {
                    project_ini.setValue(p + QStringLiteral("hfn_fmin"),
                                         QString::number(proc.saHfnFmin, 'f', 4));
                }
                if (proc.saMinSt >= 0.0)
                {
                    project_ini.setValue(p + QStringLiteral("min_st"),
                                         QString::number(proc.saMinSt, 'f', 6));
                }
                if (proc.saMinUn >= 0.0)
                {
                    project_ini.setValue(p + QStringLiteral("min_un"),
                                         QString::number(proc.saMinUn, 'f', 6));
                }
                if (proc.saMax >= 0.0)
                {
                    project_ini.setValue(p + QStringLiteral("max"),
                                         QString::number(proc.saMax, 'f', 6));
                }
                //> Written verbatim: the engine parses the group list itself,
                //> and an empty string is a record that states nothing rather
                //> than one that states "no months".
                if (!proc.saMonths.isEmpty())
                {
                    project_ini.setValue(p + QStringLiteral("months"),
                                         proc.saMonths);
                }
            }
        }
    project_ini.endGroup();

    // screen general section
    project_ini.beginGroup(EcIni::INIGROUP_SCREEN_GENERAL);
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_4, QDir::fromNativeSeparators(ec_project_state_.screenGeneral.data_path));
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_6, ec_project_state_.screenGeneral.recurse);
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_40, QVariant(ec_project_state_.screenGeneral.use_geo_north).toInt());
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_38, ec_project_state_.screenGeneral.mag_dec);
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_39, ec_project_state_.screenGeneral.dec_date);
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_8, ec_project_state_.screenGeneral.flag1_col);
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_9, QString::number(ec_project_state_.screenGeneral.flag1_threshold, 'f', 10));
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_10, ec_project_state_.screenGeneral.flag1_policy);
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_11, ec_project_state_.screenGeneral.flag2_col);
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_12, QString::number(ec_project_state_.screenGeneral.flag2_threshold, 'f', 10));
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_13, ec_project_state_.screenGeneral.flag2_policy);
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_14, ec_project_state_.screenGeneral.flag3_col);
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_15, QString::number(ec_project_state_.screenGeneral.flag3_threshold, 'f', 10));
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_16, ec_project_state_.screenGeneral.flag3_policy);
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_17, ec_project_state_.screenGeneral.flag4_col);
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_18, QString::number(ec_project_state_.screenGeneral.flag4_threshold, 'f', 10));
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_19, ec_project_state_.screenGeneral.flag4_policy);
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_20, ec_project_state_.screenGeneral.flag5_col);
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_21, QString::number(ec_project_state_.screenGeneral.flag5_threshold, 'f', 10));
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_22, ec_project_state_.screenGeneral.flag5_policy);
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_23, ec_project_state_.screenGeneral.flag6_col);
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_24, QString::number(ec_project_state_.screenGeneral.flag6_threshold, 'f', 10));
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_25, ec_project_state_.screenGeneral.flag6_policy);
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_26, ec_project_state_.screenGeneral.flag7_col);
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_27, QString::number(ec_project_state_.screenGeneral.flag7_threshold, 'f', 10));
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_28, ec_project_state_.screenGeneral.flag7_policy);
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_29, ec_project_state_.screenGeneral.flag8_col);
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_30, QString::number(ec_project_state_.screenGeneral.flag8_threshold, 'f', 10));
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_31, ec_project_state_.screenGeneral.flag8_policy);
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_32, ec_project_state_.screenGeneral.flag9_col);
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_33, QString::number(ec_project_state_.screenGeneral.flag9_threshold, 'f', 10));
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_34, ec_project_state_.screenGeneral.flag9_policy);
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_35, ec_project_state_.screenGeneral.flag10_col);
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_36, QString::number(ec_project_state_.screenGeneral.flag10_threshold, 'f', 10));
        project_ini.setValue(EcIni::INI_SCREEN_GENERAL_37, ec_project_state_.screenGeneral.flag10_policy);
    project_ini.endGroup();

    // screen settings section
    project_ini.beginGroup(EcIni::INIGROUP_SCREEN_SETTINGS);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_1, ec_project_state_.screenSetting.max_lack);
        //> Written only for the instruments that state one. Writing every slot
        //> would freeze the global's current value onto instruments nobody has
        //> touched, and they would stop following it.
        for (auto it = ec_project_state_.screenSetting.instr_max_lack.constBegin();
             it != ec_project_state_.screenSetting.instr_max_lack.constEnd(); ++it)
        {
            project_ini.setValue(EcIni::iniScreenSettingsInstrMaxLack(it.key()),
                                 it.value());
        }
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_12, ec_project_state_.screenSetting.u_offset);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_13, ec_project_state_.screenSetting.v_offset);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_14, ec_project_state_.screenSetting.w_offset);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_101, ec_project_state_.screenSetting.gill_wm_wboost);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_2, ec_project_state_.screenSetting.cross_wind);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_3, ec_project_state_.screenSetting.flow_distortion);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_4, ec_project_state_.screenSetting.rot_meth);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_5, ec_project_state_.screenSetting.detrend_meth);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_6, QString::number(ec_project_state_.screenSetting.timeconst, 'f', 1));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_7, ec_project_state_.screenSetting.tlag_meth);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_8, ec_project_state_.screenSetting.tap_win);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_9, ec_project_state_.screenSetting.nbins);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_10, ec_project_state_.screenSetting.avrg_len);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_15, ec_project_state_.screenSetting.out_bin_sp);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_38, ec_project_state_.screenSetting.out_bin_og);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_16, ec_project_state_.screenSetting.out_full_sp_u);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_17, ec_project_state_.screenSetting.out_full_sp_v);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_18, ec_project_state_.screenSetting.out_full_sp_w);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_19, ec_project_state_.screenSetting.out_full_sp_ts);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_24, ec_project_state_.screenSetting.out_st_1);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_25, ec_project_state_.screenSetting.out_st_2);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_26, ec_project_state_.screenSetting.out_st_3);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_27, ec_project_state_.screenSetting.out_st_4);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_28, ec_project_state_.screenSetting.out_st_5);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_29, ec_project_state_.screenSetting.out_st_6);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_30, ec_project_state_.screenSetting.out_st_7);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_82, ec_project_state_.screenSetting.out_raw_1);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_83, ec_project_state_.screenSetting.out_raw_2);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_84, ec_project_state_.screenSetting.out_raw_3);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_85, ec_project_state_.screenSetting.out_raw_4);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_86, ec_project_state_.screenSetting.out_raw_5);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_87, ec_project_state_.screenSetting.out_raw_6);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_88, ec_project_state_.screenSetting.out_raw_7);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_89, ec_project_state_.screenSetting.out_raw_u);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_90, ec_project_state_.screenSetting.out_raw_v);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_91, ec_project_state_.screenSetting.out_raw_w);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_92, ec_project_state_.screenSetting.out_raw_ts);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_97, ec_project_state_.screenSetting.out_raw_tair);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_98, ec_project_state_.screenSetting.out_raw_pair);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_31, ec_project_state_.screenSetting.out_full_cosp_u);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_32, ec_project_state_.screenSetting.out_full_cosp_v);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_33, ec_project_state_.screenSetting.out_full_cosp_ts);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_40, ec_project_state_.screenSetting.filter_sr);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_41, ec_project_state_.screenSetting.filter_al);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_44, ec_project_state_.screenSetting.bu_corr);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_45, ec_project_state_.screenSetting.bu_multi);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_46, QString::number(ec_project_state_.screenSetting.l_day_bot_gain, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_47, QString::number(ec_project_state_.screenSetting.l_day_bot_offset, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_48, QString::number(ec_project_state_.screenSetting.l_day_top_gain, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_49, QString::number(ec_project_state_.screenSetting.l_day_top_offset, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_50, QString::number(ec_project_state_.screenSetting.l_day_spar_gain, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_51, QString::number(ec_project_state_.screenSetting.l_day_spar_offset, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_52, QString::number(ec_project_state_.screenSetting.l_night_bot_gain, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_53, QString::number(ec_project_state_.screenSetting.l_night_bot_offset, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_54, QString::number(ec_project_state_.screenSetting.l_night_top_gain, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_55, QString::number(ec_project_state_.screenSetting.l_night_top_offset, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_56, QString::number(ec_project_state_.screenSetting.l_night_spar_gain, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_57, QString::number(ec_project_state_.screenSetting.l_night_spar_offset, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_58, QString::number(ec_project_state_.screenSetting.m_day_bot1, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_59, QString::number(ec_project_state_.screenSetting.m_day_bot2, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_60, QString::number(ec_project_state_.screenSetting.m_day_bot3, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_61, QString::number(ec_project_state_.screenSetting.m_day_bot4, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_62, QString::number(ec_project_state_.screenSetting.m_day_top1, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_63, QString::number(ec_project_state_.screenSetting.m_day_top2, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_64, QString::number(ec_project_state_.screenSetting.m_day_top3, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_65, QString::number(ec_project_state_.screenSetting.m_day_top4, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_66, QString::number(ec_project_state_.screenSetting.m_day_spar1, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_67, QString::number(ec_project_state_.screenSetting.m_day_spar2, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_68, QString::number(ec_project_state_.screenSetting.m_day_spar3, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_69, QString::number(ec_project_state_.screenSetting.m_day_spar4, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_70, QString::number(ec_project_state_.screenSetting.m_night_bot1, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_71, QString::number(ec_project_state_.screenSetting.m_night_bot2, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_72, QString::number(ec_project_state_.screenSetting.m_night_bot3, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_73, QString::number(ec_project_state_.screenSetting.m_night_bot4, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_74, QString::number(ec_project_state_.screenSetting.m_night_top1, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_75, QString::number(ec_project_state_.screenSetting.m_night_top2, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_76, QString::number(ec_project_state_.screenSetting.m_night_top3, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_77, QString::number(ec_project_state_.screenSetting.m_night_top4, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_78, QString::number(ec_project_state_.screenSetting.m_night_spar1, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_79, QString::number(ec_project_state_.screenSetting.m_night_spar2, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_80, QString::number(ec_project_state_.screenSetting.m_night_spar3, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_81, QString::number(ec_project_state_.screenSetting.m_night_spar4, 'f', 8));
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_99, ec_project_state_.screenSetting.out_details);
        project_ini.setValue(EcIni::INI_SCREEN_SETTINGS_100, ec_project_state_.screenSetting.power_of_two);

        //> Per-gas output selections, for the gases the flat keys above
        //> cannot reach. These are SCTags, read by ReadIniRP, which only
        //> sweeps sections named RawProcess* - so they belong here.
        //>
        //> Written only where the record carries a decision: the engine
        //> applies a record override whenever the tag is *present*, so an
        //> unset flag must be absent rather than written as 0.
        {
            const auto staleGas = project_ini.childKeys().filter(
                QRegularExpression(QStringLiteral("^gas_\\d+_out_")));
            for (const auto& key : staleGas) { project_ini.remove(key); }

            const auto& gases = ec_project_state_.projectGeneral.gasColumns;
            for (int i = 0; i < gases.size(); ++i)
            {
                const auto p = QStringLiteral("gas_%1_out_").arg(i + 1);
                const auto& proc = gases.at(i).proc;
                if (proc.outFullSp >= 0)
                {
                    project_ini.setValue(p + QStringLiteral("full_sp"),
                                         proc.outFullSp);
                }
                if (proc.outFullCospW >= 0)
                {
                    project_ini.setValue(p + QStringLiteral("full_cosp_w"),
                                         proc.outFullCospW);
                }
                if (proc.outRaw >= 0)
                {
                    project_ini.setValue(p + QStringLiteral("raw"), proc.outRaw);
                }
            }
        }
    project_ini.endGroup();

    // screen test section
    project_ini.beginGroup(EcIni::INIGROUP_SCREEN_TESTS);
        project_ini.setValue(EcIni::INI_SCREEN_TESTS_0, ec_project_state_.screenTest.test_sr);
        project_ini.setValue(EcIni::INI_SCREEN_TESTS_1, ec_project_state_.screenTest.test_ar);
        project_ini.setValue(EcIni::INI_SCREEN_TESTS_2, ec_project_state_.screenTest.test_do);
        project_ini.setValue(EcIni::INI_SCREEN_TESTS_3, ec_project_state_.screenTest.test_al);
        project_ini.setValue(EcIni::INI_SCREEN_TESTS_4, ec_project_state_.screenTest.test_sk);
        project_ini.setValue(EcIni::INI_SCREEN_TESTS_5, ec_project_state_.screenTest.test_ds);
        project_ini.setValue(EcIni::INI_SCREEN_TESTS_6, ec_project_state_.screenTest.test_tl);
        project_ini.setValue(EcIni::INI_SCREEN_TESTS_7, ec_project_state_.screenTest.test_aa);
        project_ini.setValue(EcIni::INI_SCREEN_TESTS_8, ec_project_state_.screenTest.test_ns);
    project_ini.endGroup();

    // screen param section
    project_ini.beginGroup(EcIni::INIGROUP_SCREEN_PARAM);
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_0 , ec_project_state_.screenParam.sr_num_spk);
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_1 , QString::number(ec_project_state_.screenParam.sr_lim_u, 'f', 1));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_45 , QString::number(ec_project_state_.screenParam.sr_lim_w, 'f', 1));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_2 , QString::number(ec_project_state_.screenParam.sr_lim_hf, 'f', 1));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_3 , QString::number(ec_project_state_.screenParam.ar_lim, 'f', 1));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_4 , ec_project_state_.screenParam.ar_bins);
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_5 , ec_project_state_.screenParam.ar_hf_lim);
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_6 , ec_project_state_.screenParam.do_extlim_dw);
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_7 , QString::number(ec_project_state_.screenParam.do_hf1_lim, 'f', 1));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_8 , QString::number(ec_project_state_.screenParam.do_hf2_lim, 'f', 1));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_9 , QString::number(ec_project_state_.screenParam.al_u_max, 'f', 1));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_10, QString::number(ec_project_state_.screenParam.al_w_max, 'f', 1));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_11, QString::number(ec_project_state_.screenParam.al_tson_min, 'f', 1));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_12, QString::number(ec_project_state_.screenParam.al_tson_max, 'f', 1));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_17, QString::number(ec_project_state_.screenParam.sk_hf_skmin, 'f', 1));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_18, QString::number(ec_project_state_.screenParam.sk_hf_skmax, 'f', 1));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_19, QString::number(ec_project_state_.screenParam.sk_sf_skmin, 'f', 1));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_20, QString::number(ec_project_state_.screenParam.sk_sf_skmax, 'f', 1));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_21, QString::number(ec_project_state_.screenParam.sk_hf_kumin, 'f', 1));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_22, QString::number(ec_project_state_.screenParam.sk_hf_kumax, 'f', 1));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_23, QString::number(ec_project_state_.screenParam.sk_sf_kumin, 'f', 1));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_24, QString::number(ec_project_state_.screenParam.sk_sf_kumax, 'f', 1));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_25, QString::number(ec_project_state_.screenParam.ds_hf_uv, 'f', 2));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_26, QString::number(ec_project_state_.screenParam.ds_hf_w, 'f', 2));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_27, QString::number(ec_project_state_.screenParam.ds_hf_t, 'f', 2));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_30, QString::number(ec_project_state_.screenParam.ds_hf_var, 'f', 2));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_31, QString::number(ec_project_state_.screenParam.ds_sf_uv, 'f', 2));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_32, QString::number(ec_project_state_.screenParam.ds_sf_w, 'f', 2));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_33, QString::number(ec_project_state_.screenParam.ds_sf_t, 'f', 2));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_36, QString::number(ec_project_state_.screenParam.ds_sf_var, 'f', 2));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_60, ec_project_state_.screenParam.despike_vm);
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_37, QString::number(ec_project_state_.screenParam.tl_hf_lim, 'f', 1));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_38, QString::number(ec_project_state_.screenParam.tl_sf_lim, 'f', 1));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_41, QString::number(ec_project_state_.screenParam.aa_min, 'f', 1));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_42, QString::number(ec_project_state_.screenParam.aa_max, 'f', 1));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_43, QString::number(ec_project_state_.screenParam.aa_lim, 'f', 1));
        project_ini.setValue(EcIni::INI_SCREEN_PARAM_44, QString::number(ec_project_state_.screenParam.ns_hf_lim, 'f', 1));
        //> Per-gas screening thresholds, for the gases the flat keys above
        //> cannot reach. These are SNTags and ReadIniRP only sweeps sections
        //> named RawProcess*, so they belong here rather than in [Project].
        //>
        //> Written only where the record holds a real value: the engine
        //> applies a record override whenever the tag is *present*, so
        //> emitting a sentinel would replace the legacy threshold with one.
        {
            const auto staleGas = project_ini.childKeys().filter(
                QRegularExpression(QStringLiteral("^gas_\\d+_")));
            for (const auto& key : staleGas) { project_ini.remove(key); }

            const auto& gases = ec_project_state_.projectGeneral.gasColumns;
            for (int i = 0; i < gases.size(); ++i)
            {
                const auto p = QStringLiteral("gas_%1_").arg(i + 1);
                const auto& proc = gases.at(i).proc;
                if (proc.srLim >= 0.0)
                {
                    project_ini.setValue(p + QStringLiteral("sr_lim"),
                                         QString::number(proc.srLim, 'f', 1));
                }
                //> Six decimals, not three. These are umol/mol (mmol/mol for
                //> water) whatever unit the column reports, so a gas measured
                //> in ppb has an ambient-scale limit down at 1e-4 and three
                //> decimals rounded it to zero - which the engine reads as
                //> "no limit stated" and, for a minimum, as rejecting nothing
                //> or everything. The engine parses these with a
                //> list-directed read, so the extra digits cost nothing.
                if (proc.alMin >= 0.0)
                {
                    project_ini.setValue(p + QStringLiteral("al_min"),
                                         QString::number(proc.alMin, 'f', 6));
                }
                if (proc.alMax >= 0.0)
                {
                    project_ini.setValue(p + QStringLiteral("al_max"),
                                         QString::number(proc.alMax, 'f', 6));
                }
                if (proc.dsHf >= 0.0)
                {
                    project_ini.setValue(p + QStringLiteral("ds_hf"),
                                         QString::number(proc.dsHf, 'f', 2));
                }
                if (proc.dsSf >= 0.0)
                {
                    project_ini.setValue(p + QStringLiteral("ds_sf"),
                                         QString::number(proc.dsSf, 'f', 2));
                }
                if (proc.tlDef >= 0.0)
                {
                    project_ini.setValue(p + QStringLiteral("tl_def"),
                                         QString::number(proc.tlDef, 'f', 1));
                }
            }
        }
    project_ini.endGroup();

    // planar fit section
    project_ini.beginGroup(EcIni::INIGROUP_SCREEN_TILT);
        // firstly remove all previous keys because they are variable in number
        project_ini.remove(QString());

        project_ini.setValue(EcIni::INI_SCREEN_TILT_0, ec_project_state_.screenTilt.start_date);
        project_ini.setValue(EcIni::INI_SCREEN_TILT_1, ec_project_state_.screenTilt.end_date);
        project_ini.setValue(EcIni::INI_SCREEN_TILT_12, ec_project_state_.screenTilt.start_time);
        project_ini.setValue(EcIni::INI_SCREEN_TILT_13, ec_project_state_.screenTilt.end_time);
        project_ini.setValue(EcIni::INI_SCREEN_TILT_2, ec_project_state_.screenTilt.mode);
        project_ini.setValue(EcIni::INI_SCREEN_TILT_3, ec_project_state_.screenTilt.north_offset);
        project_ini.setValue(EcIni::INI_SCREEN_TILT_4, ec_project_state_.screenTilt.min_num_per_sec);
        project_ini.setValue(EcIni::INI_SCREEN_TILT_5, QString::number(ec_project_state_.screenTilt.w_max, 'f', 3));
        project_ini.setValue(EcIni::INI_SCREEN_TILT_6, QString::number(ec_project_state_.screenTilt.u_min, 'f', 3));
        project_ini.setValue(EcIni::INI_SCREEN_TILT_7, QDir::fromNativeSeparators(ec_project_state_.screenTilt.file));
        project_ini.setValue(EcIni::INI_SCREEN_TILT_8, ec_project_state_.screenTilt.fix_policy);
        project_ini.setValue(EcIni::INI_SCREEN_TILT_11, ec_project_state_.screenTilt.subset);
        project_ini.setValue(EcIni::INI_SCREEN_TILT_14, ec_project_state_.screenTilt.assessment_only);

        // iterate through angle list
        int k = 0;
        for (const auto &angle : ec_project_state_.screenTilt.angles)
        {
            QString index = QStringLiteral("_") + QString::number(k + 1);
            QString prefix = StringUtils::insertIndex(EcIni::INI_SCREEN_TILT_PREFIX, 7, index);

            int excluded = angle.included_ ? 0 : 1;

            project_ini.setValue(prefix + EcIni::INI_SCREEN_TILT_9, angle.angle_);
            project_ini.setValue(prefix + EcIni::INI_SCREEN_TILT_10, excluded);

            ++k;
        }
    project_ini.endGroup();

    // timelag opt section
    project_ini.beginGroup(EcIni::INIGROUP_TIMELAG_OPT);
        project_ini.setValue(EcIni::INI_TIMELAG_OPT_0, ec_project_state_.timelagOpt.start_date);
        project_ini.setValue(EcIni::INI_TIMELAG_OPT_1, ec_project_state_.timelagOpt.end_date);
        project_ini.setValue(EcIni::INI_TIMELAG_OPT_19, ec_project_state_.timelagOpt.start_time);
        project_ini.setValue(EcIni::INI_TIMELAG_OPT_20, ec_project_state_.timelagOpt.end_time);
        project_ini.setValue(EcIni::INI_TIMELAG_OPT_2, ec_project_state_.timelagOpt.mode);
        project_ini.setValue(EcIni::INI_TIMELAG_OPT_3, QDir::fromNativeSeparators(ec_project_state_.timelagOpt.file));
        project_ini.setValue(EcIni::INI_TIMELAG_OPT_17, ec_project_state_.timelagOpt.to_h2o_nclass);
        project_ini.setValue(EcIni::INI_TIMELAG_OPT_7, QString::number(ec_project_state_.timelagOpt.le_min_flux, 'f', 1));
        project_ini.setValue(EcIni::INI_TIMELAG_OPT_8, QString::number(ec_project_state_.timelagOpt.pg_range, 'f', 1));
        //> Per-gas search windows, for gases the four flat keys cannot reach.
        //> These are SNTags, and ReadIniRP only sweeps RawProcess* sections,
        //> so they belong here rather than in [Project].
        //>
        //> Written only where the record holds a real value: the engine
        //> applies an override whenever the tag is present, so a sentinel
        //> would replace the legacy window with nonsense.
        {
            const auto staleTo = project_ini.childKeys().filter(
                QRegularExpression(QStringLiteral("^gas_\\d+_to_")));
            for (const auto& key : staleTo) { project_ini.remove(key); }

            const auto& gases = ec_project_state_.projectGeneral.gasColumns;
            for (int i = 0; i < gases.size(); ++i)
            {
                const auto pfx = QStringLiteral("gas_%1_to_").arg(i + 1);
                const auto& proc = gases.at(i).proc;
                if (proc.toMinLag > -9000.0 && proc.toMinLag != -1.0)
                {
                    project_ini.setValue(pfx + QStringLiteral("min_lag"),
                                         QString::number(proc.toMinLag, 'f', 1));
                }
                if (proc.toMaxLag > -9000.0 && proc.toMaxLag != -1.0)
                {
                    project_ini.setValue(pfx + QStringLiteral("max_lag"),
                                         QString::number(proc.toMaxLag, 'f', 1));
                }
                if (proc.toMinFlux >= 0.0)
                {
                    project_ini.setValue(pfx + QStringLiteral("min_flux"),
                                         QString::number(proc.toMinFlux, 'f', 3));
                }
            }
        }
        project_ini.setValue(EcIni::INI_TIMELAG_OPT_18, ec_project_state_.timelagOpt.subset);
        project_ini.setValue(EcIni::INI_TIMELAG_OPT_21, ec_project_state_.timelagOpt.assessment_only);
    project_ini.endGroup();

    // PWB timelag section
    project_ini.beginGroup(EcIni::INIGROUP_PWB_TIMELAG);
        //> Per-gas search windows, for gases the four flat keys above cannot
        //> reach. These are SNTags read by ReadIniRP, which only sweeps
        //> sections named RawProcess*, so they belong here and not in
        //> [Project] beside gas_N_col.
        //>
        //> Only written where the record carries a real value: the engine
        //> applies an override whenever the tag is *present*, so emitting a
        //> sentinel would override the legacy window with nonsense.
        {
            const auto stalePwb = project_ini.childKeys().filter(
                QRegularExpression(QStringLiteral("^gas_\\d+_pwb_")));
            for (const auto& key : stalePwb) { project_ini.remove(key); }

            const auto& gases = ec_project_state_.projectGeneral.gasColumns;
            for (int i = 0; i < gases.size(); ++i)
            {
                const auto p = QStringLiteral("gas_%1_pwb_").arg(i + 1);
                if (gases.at(i).proc.pwbMinLag > -9000.0)
                {
                    project_ini.setValue(p + QStringLiteral("min_lag"),
                        QString::number(gases.at(i).proc.pwbMinLag, 'f', 1));
                }
                if (gases.at(i).proc.pwbMaxLag > -9000.0)
                {
                    project_ini.setValue(p + QStringLiteral("max_lag"),
                        QString::number(gases.at(i).proc.pwbMaxLag, 'f', 1));
                }
            }
        }
        project_ini.setValue(EcIni::INI_PWB_TIMELAG_8, ec_project_state_.pwbTimelag.n_bootstrap);
        project_ini.setValue(EcIni::INI_PWB_TIMELAG_9, QString::number(ec_project_state_.pwbTimelag.block_length_s, 'f', 1));
        project_ini.setValue(EcIni::INI_PWB_TIMELAG_10, QString::number(ec_project_state_.pwbTimelag.min_valid_frac, 'f', 3));
        project_ini.setValue(EcIni::INI_PWB_TIMELAG_11, QString::number(ec_project_state_.pwbTimelag.hdi_thresh_s, 'f', 2));
        project_ini.setValue(EcIni::INI_PWB_TIMELAG_12, QString::number(ec_project_state_.pwbTimelag.dev_thresh_s, 'f', 2));
        project_ini.setValue(EcIni::INI_PWB_TIMELAG_13, QString::number(ec_project_state_.pwbTimelag.hdi_prefilter_s, 'f', 2));
        project_ini.setValue(EcIni::INI_PWB_TIMELAG_14, ec_project_state_.pwbTimelag.smoothing_width);
        project_ini.setValue(EcIni::INI_PWB_TIMELAG_15, ec_project_state_.pwbTimelag.random_seed);
        project_ini.setValue(EcIni::INI_PWB_TIMELAG_19,
            QString::number(ec_project_state_.pwbTimelag.max_carry_h, 'f', 2));
        //> Three retired settings, removed rather than left behind: they
        //> are inert now, and a key nothing reads is a key someone will
        //> eventually edit expecting it to matter.
        project_ini.remove(EcIni::INI_PWB_TIMELAG_16_RETIRED);
        project_ini.remove(EcIni::INI_PWB_TIMELAG_17_RETIRED);
        project_ini.remove(EcIni::INI_PWB_TIMELAG_18_RETIRED);
        project_ini.remove(EcIni::INI_PWB_TIMELAG_18_LEGACY);
    project_ini.endGroup();

    // random error section
    //
    // Also clear the legacy RawProcess copies, or QSettings keeps them for ever
    // beside the live ones and a later reader cannot tell which is current.
    project_ini.beginGroup(EcIni::INIGROUP_RAND_ERROR_LEGACY);
        project_ini.remove(EcIni::INI_RAND_ERROR_0);
        project_ini.remove(EcIni::INI_RAND_ERROR_1);
        project_ini.remove(EcIni::INI_RAND_ERROR_2);
        project_ini.remove(EcIni::INI_RAND_ERROR_3);
    project_ini.endGroup();

    project_ini.beginGroup(EcIni::INIGROUP_RAND_ERROR);
        project_ini.setValue(EcIni::INI_RAND_ERROR_0,
                             ec_project_state_.randomError.ru_method);
        project_ini.setValue(EcIni::INI_RAND_ERROR_1,
                             ec_project_state_.randomError.its_method);
        project_ini.setValue(EcIni::INI_RAND_ERROR_2,
                             QString::number(ec_project_state_.randomError.its_tlag_max, 'f', 1));

        // NOTE: temporarly disabled
//        project_ini.setValue(EcIni::INI_RAND_ERROR_3,
//                             QString::number(ec_project_state_.randomError.its_sec_factor, 'f', 1));
    project_ini.endGroup();

    // biomet section
    project_ini.beginGroup(EcIni::INIGROUP_BIOMET);
        project_ini.setValue(EcIni::INI_BIOMET_0, ec_project_state_.biomParam.native_header);
        project_ini.setValue(EcIni::INI_BIOMET_1, ec_project_state_.biomParam.hlines);
        project_ini.setValue(EcIni::INI_BIOMET_2, ec_project_state_.biomParam.separator);
        project_ini.setValue(EcIni::INI_BIOMET_3, ec_project_state_.biomParam.tstamp_ref);
        project_ini.setValue(EcIni::INI_BIOMET_4, ec_project_state_.biomParam.col_ta - 1000);
        project_ini.setValue(EcIni::INI_BIOMET_5, ec_project_state_.biomParam.col_pa - 1000);
        project_ini.setValue(EcIni::INI_BIOMET_6, ec_project_state_.biomParam.col_rh);
        //> Written for this interface alone. Every other key in this group is
        //> read by the engine, and a reader will assume this one is too - it
        //> is not. What the engine acts on is each gas's own moisture
        //> reference, which ticking the box sets to the biomet; this only
        //> remembers that the box was ticked.
        project_ini.setValue(EcIni::INI_BIOMET_RH_OVERRIDE,
            ec_project_state_.biomParam.rh_override ? 1 : 0);
        project_ini.setValue(EcIni::INI_BIOMET_7, ec_project_state_.biomParam.col_rg);
        project_ini.setValue(EcIni::INI_BIOMET_8, ec_project_state_.biomParam.col_lwin);
        project_ini.setValue(EcIni::INI_BIOMET_9, ec_project_state_.biomParam.col_ppfd);
    project_ini.endGroup();

    // wind direction filter section
    project_ini.beginGroup(EcIni::INIGROUP_WIND_FILTER);
        project_ini.setValue(EcIni::INI_WIND_FILTER_APPLY, ec_project_state_.windFilter.apply);
        const auto &sectors = ec_project_state_.windFilter.sectors;
        for (int i = 0; i < sectors.size(); ++i) {
            const QString prefix = EcIni::INI_WIND_FILTER_PREFIX + QString::number(i) + QLatin1Char('_');
            project_ini.setValue(prefix + EcIni::INI_WIND_FILTER_START_SUFFIX, sectors.at(i).startAngle_);
            project_ini.setValue(prefix + EcIni::INI_WIND_FILTER_END_SUFFIX,   sectors.at(i).endAngle_);
        }
    project_ini.endGroup();
    project_ini.sync();

    bool result = tagProject(filename);
    if (!result)
    {
        WidgetUtils::warning(nullptr,
                             tr("Write Project Error"),
                             tr("Unable to tag project file!"));
    }

    // project is saved, so set flags accordingly
    setModified(false);
    return true;
}

// Load a project. Assumes file has been checked with nativeFormat()
//> A project file whose values carry native Windows separators cannot be read
//> with QSettings as it stands, and the damage is silent.
//>
//> QSettings(IniFormat) unescapes values on read, and drops any escape it does
//> not recognise together with the character after it. Every separator in a
//> Windows path introduces one: "C:\\Users\\jonmuell\\Documents\\x.csv" comes back
//> as "C:sersonmuellocuments.csv" - measured, not supposed. Saving then writes
//> that back, so one open-and-save destroys the path permanently, and the
//> engine is left opening a file that cannot exist.
//>
//> Nothing can recover it after the fact: by the time QSettings hands the value
//> over, the characters are gone. So the separators are converted BEFORE
//> QSettings sees the file. A file this interface wrote is already
//> forward-slashed and is passed through untouched; only a hand-edited or
//> third-party file takes the copy.
//>
//> Returns the path to read from - the original, or a repaired temporary - and
//> reports through `repaired` whether anything had to be changed.
QString normalisedProjectPath(const QString& filename, QTemporaryFile& scratch,
                              bool* repaired)
{
    *repaired = false;

    QFile in(filename);
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text)) { return filename; }
    const QString text = QString::fromUtf8(in.readAll());
    in.close();

    QStringList out;
    out.reserve(1024);
    bool changed = false;
    const auto lines = text.split(QLatin1Char('\n'));
    for (const QString& line : lines)
    {
        const int eq = line.indexOf(QLatin1Char('='));
        //> Section headers, comments and blank lines have no value to repair.
        const QString trimmed = line.trimmed();
        if (eq <= 0 || trimmed.startsWith(QLatin1Char('['))
            || trimmed.startsWith(QLatin1Char(';')))
        {
            out << line;
            continue;
        }
        const QString key = line.left(eq + 1);
        QString value = line.mid(eq + 1);
        if (value.contains(QLatin1Char('\\')))
        {
            value.replace(QLatin1Char('\\'), QLatin1Char('/'));
            changed = true;
        }
        out << key + value;
    }

    if (!changed) { return filename; }

    if (!scratch.open()) { return filename; }
    scratch.write(out.join(QLatin1Char('\n')).toUtf8());
    scratch.flush();
    scratch.close();
    *repaired = true;
    return scratch.fileName();
}

bool EcProject::loadEcProject(const QString &filename, bool checkVersion, bool *modified)
{
    auto parent = static_cast<MainWindow*>(this->parent());
    if (parent == nullptr) { return false; }

    //> Cleared per load. It was set once and never reset, so opening a
    //> modern project after a legacy one in the same session still read as
    //> upgraded - harmless while the only consequence was a dialog, and a
    //> spurious backup-and-save now that it drives one.
    wasUpgradedOnLoad_ = false;

    bool isVersionCompatible = true;
    QVariant v; // container for conversions

    // open file
    QFile datafile(filename);
    if (!datafile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        // error opening file
        qWarning() << "Error: Cannot open [loadEcProject()]" << filename;
        WidgetUtils::warning(nullptr,
                             tr("Load Project Error"),
                             tr("Cannot read file<br /><p>%1:</p>\n<b>%2</b>")
                             .arg(filename, datafile.errorString()));
        return false;
    }

    //> Repair native separators before QSettings can eat them; see the note
    //> on normalisedProjectPath above. The temporary lives until the end of
    //> this function, which outlasts every read from project_ini.
    QTemporaryFile normalisedCopy;
    bool pathsRepaired = false;
    const QString readFrom =
        normalisedProjectPath(filename, normalisedCopy, &pathsRepaired);

    QSettings project_ini(readFrom, QSettings::IniFormat);

    if (pathsRepaired)
    {
        //> Marked modified so that saving persists the repair, and said out
        //> loud because the file on disk still holds separators that any
        //> other Qt reader would destroy.
        if (modified != nullptr) { *modified = true; }
        WidgetUtils::information(
            nullptr,
            tr("Project paths adjusted"),
            tr("This project file uses Windows path separators."),
            tr("Paths have been converted to forward slashes so they survive "
               "being read. Save the project to keep the correction."));
    }

    // in case of old non existing file name, use the current existing
    QString projectFilename = project_ini.value(EcIni::INI_PROJECT_2, QString()).toString();
    if (!FileUtils::existsPath(projectFilename))
    {
        projectFilename = filename;
    }

    EcProjectState defaultEcProjectState;
    // general section
    project_ini.beginGroup(EcIni::INIGROUP_PROJECT);
        ec_project_state_.projectGeneral.project_title
            = project_ini.value(EcIni::INI_PROJECT_3,
                                defaultEcProjectState.projectGeneral.project_title).toString();
        ec_project_state_.projectGeneral.project_id
            = project_ini.value(EcIni::INI_PROJECT_6,
                                defaultEcProjectState.projectGeneral.project_id).toString();
        ec_project_state_.projectGeneral.creation_date
            = project_ini.value(EcIni::INI_PROJECT_0,
                                defaultEcProjectState.projectGeneral.creation_date).toString();
        ec_project_state_.projectGeneral.last_change_date
            = project_ini.value(EcIni::INI_PROJECT_1,
                                defaultEcProjectState.projectGeneral.last_change_date).toString();

        v = project_ini.value(EcIni::INI_PROJECT_7,
                              QVariant::fromValue(defaultEcProjectState.projectGeneral.file_type).toInt());
        ec_project_state_.projectGeneral.file_type = static_cast<Defs::RawFileType>(v.toInt());

        ec_project_state_.projectGeneral.use_alt_md_file
                = project_ini.value(EcIni::INI_PROJECT_9,
                                    defaultEcProjectState.projectGeneral.use_alt_md_file).toBool();

        // NOTE: backward compatibility for 'is_express' key
        if (project_ini.contains(EcIni::INI_PROJECT_33_OLD))
        {
            if (checkVersion)
            {
                if (!parent->queryEcProjectImport(filename))
                {
                    return false;
                }

                v = project_ini.value(EcIni::INI_PROJECT_33_OLD,
                         QVariant::fromValue(defaultEcProjectState.projectGeneral.run_mode).toInt());
                ec_project_state_.projectGeneral.run_mode
                    = static_cast<Defs::CurrRunMode>(v.toInt());

                project_ini.remove(EcIni::INI_PROJECT_33_OLD);
                isVersionCompatible = false;
            }
            else
            {
                // abort the file loading to skip it during previous data comparison
                return false;
            }
        }
        else
        {
            v = project_ini.value(EcIni::INI_PROJECT_33,
                            QVariant::fromValue(defaultEcProjectState.projectGeneral.run_mode));

            if (v.canConvert<Defs::CurrRunMode>())
            {
                ec_project_state_.projectGeneral.run_mode
                    = v.value<Defs::CurrRunMode>();
            }

            ec_project_state_.projectGeneral.run_fcc
                    = project_ini.value(EcIni::INI_PROJECT_40,
                                        defaultEcProjectState.projectGeneral.run_fcc).toBool();
        }

        ec_project_state_.projectGeneral.file_name = projectFilename;
        ec_project_state_.projectGeneral.sw_version
                = project_ini.value(EcIni::INI_PROJECT_4,
                                    defaultEcProjectState.projectGeneral.sw_version).toString();
        ec_project_state_.projectGeneral.ini_version
                = project_ini.value(EcIni::INI_PROJECT_5,
                                    defaultEcProjectState.projectGeneral.ini_version).toString();
        ec_project_state_.projectGeneral.file_prototype
                = project_ini.value(EcIni::INI_PROJECT_8,
                                    defaultEcProjectState.projectGeneral.file_prototype).toString();
        ec_project_state_.projectGeneral.md_file
                = project_ini.value(EcIni::INI_PROJECT_10,
                                    defaultEcProjectState.projectGeneral.md_file).toString();
        ec_project_state_.projectGeneral.use_tlfile
                = project_ini.value(EcIni::INI_PROJECT_11,
                                    defaultEcProjectState.projectGeneral.use_tlfile).toBool();
        ec_project_state_.projectGeneral.timeline_file
                = project_ini.value(EcIni::INI_PROJECT_12,
                                    defaultEcProjectState.projectGeneral.timeline_file).toString();
        ec_project_state_.projectGeneral.binary_hnlines
                = project_ini.value(EcIni::INI_PROJECT_13,
                                    defaultEcProjectState.projectGeneral.binary_hnlines).toInt();
        ec_project_state_.projectGeneral.binary_eol
                = project_ini.value(EcIni::INI_PROJECT_14,
                                    defaultEcProjectState.projectGeneral.binary_eol).toInt();
        ec_project_state_.projectGeneral.binary_nbytes
                = project_ini.value(EcIni::INI_PROJECT_15,
                                    defaultEcProjectState.projectGeneral.binary_nbytes).toInt();
        ec_project_state_.projectGeneral.binary_little_end
                = project_ini.value(EcIni::INI_PROJECT_16,
                                    defaultEcProjectState.projectGeneral.binary_little_end).toInt();
        ec_project_state_.projectGeneral.master_sonic
                = project_ini.value(EcIni::INI_PROJECT_17,
                                    defaultEcProjectState.projectGeneral.master_sonic).toString();

        //> The one model key the project file carries of its own, and the only
        //> one that never went through the metadata converters that normalise
        //> the rest. Left alone, a retired name matches nothing in the master
        //> anemometer list and the selection silently falls back to None - the
        //> project opens, and the sonic it named is gone.
        //>
        //> The value is the model key plus a 1-based index, so only the key
        //> part is rewritten; the index is what pairs it with an instrument.
        {
            const auto sonic = ec_project_state_.projectGeneral.master_sonic;
            static const QRegularExpression indexSuffix(QStringLiteral("_\\d*$"));
            const auto match = indexSuffix.match(sonic);
            if (match.hasMatch())
            {
                const auto key = sonic.left(match.capturedStart());
                const auto canonical = DlProject::canonicalModelKey(key);
                if (canonical != key)
                {
                    ec_project_state_.projectGeneral.master_sonic
                            = canonical + match.captured();
                    // Rewritten in memory only: the file still names the sonic
                    // the old way until it is saved.
                    isVersionCompatible = false;
                }
            }
        }
        ec_project_state_.projectGeneral.col_co2
                = project_ini.value(EcIni::INI_PROJECT_18,
                                    defaultEcProjectState.projectGeneral.col_co2).toInt();
        ec_project_state_.projectGeneral.col_h2o
                = project_ini.value(EcIni::INI_PROJECT_19,
                                    defaultEcProjectState.projectGeneral.col_h2o).toInt();
        ec_project_state_.projectGeneral.col_ch4
                = project_ini.value(EcIni::INI_PROJECT_20,
                                    defaultEcProjectState.projectGeneral.col_ch4).toInt();
        ec_project_state_.projectGeneral.col_gas4
                = project_ini.value(EcIni::INI_PROJECT_21,
                                    project_ini.value(QStringLiteral("col_n2o"),
                                    defaultEcProjectState.projectGeneral.col_gas4)).toInt();
        ec_project_state_.projectGeneral.col_int_t_1
                = project_ini.value(EcIni::INI_PROJECT_22,
                                    defaultEcProjectState.projectGeneral.col_int_t_1).toInt();
        ec_project_state_.projectGeneral.col_int_t_2
                = project_ini.value(EcIni::INI_PROJECT_23,
                                    defaultEcProjectState.projectGeneral.col_int_t_2).toInt();
        ec_project_state_.projectGeneral.col_int_p
                = project_ini.value(EcIni::INI_PROJECT_24,
                                    defaultEcProjectState.projectGeneral.col_int_p).toInt();
        ec_project_state_.projectGeneral.col_air_t
                = project_ini.value(EcIni::INI_PROJECT_25,
                                    defaultEcProjectState.projectGeneral.col_air_t).toInt();
        ec_project_state_.projectGeneral.col_air_p
                = project_ini.value(EcIni::INI_PROJECT_26,
                                    defaultEcProjectState.projectGeneral.col_air_p).toInt();
        ec_project_state_.projectGeneral.col_int_t_c
                = project_ini.value(EcIni::INI_PROJECT_27,
                                    defaultEcProjectState.projectGeneral.col_int_t_c).toInt();
        ec_project_state_.projectGeneral.col_diag_75
                = project_ini.value(EcIni::INI_PROJECT_28,
                                    defaultEcProjectState.projectGeneral.col_diag_75).toInt();
        ec_project_state_.projectGeneral.col_diag_72
                = project_ini.value(EcIni::INI_PROJECT_29,
                                    defaultEcProjectState.projectGeneral.col_diag_72).toInt();
        ec_project_state_.projectGeneral.col_diag_77
                = project_ini.value(EcIni::INI_PROJECT_30,
                                    defaultEcProjectState.projectGeneral.col_diag_77).toInt();
        ec_project_state_.projectGeneral.col_diag_anem
                = project_ini.value(EcIni::INI_PROJECT_69,
                                    defaultEcProjectState.projectGeneral.col_diag_anem).toInt();
        ec_project_state_.projectGeneral.col_ts
                = project_ini.value(EcIni::INI_PROJECT_36,
                                    defaultEcProjectState.projectGeneral.col_ts).toInt();

        // Records if the file has them, otherwise built from the col_* fields
        // read just above. Reading them here, after those fields, is what lets
        // the migration work off values rather than re-reading the file.
        //> A file with no gas_num predates the record format. Build the
        //> records from the legacy col_* ints and mark the project as
        //> upgraded: it is saved back in the new format, and from then on
        //> nothing but this branch needs to understand the old shape.
        if (!readMeasurementRecords(project_ini))
        {
            migrateLegacyColumnsToRecords();
            wasUpgradedOnLoad_ = true;
        }
        ec_project_state_.projectGeneral.gas_mw
                = project_ini.value(EcIni::INI_PROJECT_31,
                                    defaultEcProjectState.projectGeneral.gas_mw).toReal();
        ec_project_state_.projectGeneral.gas_diff
                = project_ini.value(EcIni::INI_PROJECT_32,
                                    defaultEcProjectState.projectGeneral.gas_diff).toReal();
        ec_project_state_.projectGeneral.fluxnet_standardize_biomet
                = project_ini.value(EcIni::INI_PROJECT_70,
                                    defaultEcProjectState.projectGeneral.fluxnet_standardize_biomet).toInt();
        ec_project_state_.projectGeneral.fluxnet_err_label
                = project_ini.value(EcIni::INI_PROJECT_71,
                                    defaultEcProjectState.projectGeneral.fluxnet_err_label).toInt();

        // NOTE: backward compatibility change for 'out_rich' key
        if (project_ini.value(EcIni::INI_PROJECT_5,
                              defaultEcProjectState.projectGeneral.ini_version).toString() == QLatin1String("1.0"))
        {
            if (checkVersion)
            {
                if (!parent->queryEcProjectImport(filename))
                {
                    return false;
                }
                ec_project_state_.projectGeneral.out_rich = 1;
                isVersionCompatible = false;
            }
            else
            {
                // abort the file loading to skip it during previous data comparison
                return false;
            }
        }
        else
        {
            ec_project_state_.projectGeneral.out_rich
                    = project_ini.value(EcIni::INI_PROJECT_39,
                                        defaultEcProjectState.projectGeneral.out_rich).toInt();
        }

        ec_project_state_.projectGeneral.out_md
                = project_ini.value(EcIni::INI_PROJECT_56,
                                    defaultEcProjectState.projectGeneral.out_md).toInt();
        ec_project_state_.projectGeneral.make_dataset
                = project_ini.value(EcIni::INI_PROJECT_41,
                                    defaultEcProjectState.projectGeneral.make_dataset).toBool();

        ec_project_state_.projectGeneral.subset
                = project_ini.value(EcIni::INI_PROJECT_54,
                                    defaultEcProjectState.projectGeneral.subset).toInt();
        ec_project_state_.projectGeneral.start_date
                = project_ini.value(EcIni::INI_PROJECT_42,
                                    QDate(2000, 1, 1).toString(Qt::ISODate)).toString();
        ec_project_state_.projectGeneral.start_time
                = project_ini.value(EcIni::INI_PROJECT_44,
                                    QTime(0, 0).toString(QStringLiteral("hh:mm"))).toString();
        ec_project_state_.projectGeneral.end_date
                = project_ini.value(EcIni::INI_PROJECT_43,
                                    QDate::currentDate().toString(Qt::ISODate)).toString();
        ec_project_state_.projectGeneral.end_time
                = project_ini.value(EcIni::INI_PROJECT_45,
                                    QTime(23, 59).toString(QStringLiteral("hh:mm"))).toString();

        ec_project_state_.projectGeneral.hf_meth
                = project_ini.value(EcIni::INI_PROJECT_46,
                                    defaultEcProjectState.projectGeneral.hf_meth).toInt();
        ec_project_state_.projectGeneral.lf_meth
                = project_ini.value(EcIni::INI_PROJECT_47,
                                    defaultEcProjectState.projectGeneral.lf_meth).toInt();
        ec_project_state_.projectGeneral.wpl_meth
                = project_ini.value(EcIni::INI_PROJECT_48,
                                    defaultEcProjectState.projectGeneral.wpl_meth).toInt();
        ec_project_state_.projectGeneral.foot_meth
                = project_ini.value(EcIni::INI_PROJECT_49,
                                    defaultEcProjectState.projectGeneral.foot_meth).toInt();
        ec_project_state_.projectGeneral.cec_meth
                = project_ini.value(EcIni::INI_PROJECT_72,
                                    defaultEcProjectState.projectGeneral.cec_meth).toInt();
        ec_project_state_.projectGeneral.cec_h
                = project_ini.value(EcIni::INI_PROJECT_73,
                                    defaultEcProjectState.projectGeneral.cec_h).toDouble();
        ec_project_state_.projectGeneral.cec_min_o1_o2
                = project_ini.value(EcIni::INI_PROJECT_74,
                                    defaultEcProjectState.projectGeneral.cec_min_o1_o2).toDouble();
        ec_project_state_.projectGeneral.cec_min_octant
                = project_ini.value(EcIni::INI_PROJECT_75,
                                    defaultEcProjectState.projectGeneral.cec_min_octant).toDouble();
        ec_project_state_.projectGeneral.cec_min_valid
                = project_ini.value(EcIni::INI_PROJECT_76,
                                    defaultEcProjectState.projectGeneral.cec_min_valid).toDouble();
        ec_project_state_.projectGeneral.cec_signal_strength
                = project_ini.value(EcIni::INI_PROJECT_77,
                                    defaultEcProjectState.projectGeneral.cec_signal_strength).toDouble();
        ec_project_state_.projectGeneral.cec_max_gap_fill
                = project_ini.value(EcIni::INI_PROJECT_78,
                                    defaultEcProjectState.projectGeneral.cec_max_gap_fill).toInt();
        bool cecMaxStationarityOk = false;
        const double cecMaxStationarity
                = project_ini.value(EcIni::INI_PROJECT_79,
                                    defaultEcProjectState.projectGeneral.cec_max_stationarity).toDouble(&cecMaxStationarityOk);
        ec_project_state_.projectGeneral.cec_max_stationarity
                = (cecMaxStationarityOk && cecMaxStationarity >= 0.0)
                    ? cecMaxStationarity
                    : defaultEcProjectState.projectGeneral.cec_max_stationarity;
        bool cecSingularBandOk = false;
        const double cecSingularBand
                = project_ini.value(EcIni::INI_PROJECT_80,
                                    defaultEcProjectState.projectGeneral.cec_singular_band).toDouble(&cecSingularBandOk);
        ec_project_state_.projectGeneral.cec_singular_band
                = (cecSingularBandOk && cecSingularBand >= 0.0 && cecSingularBand <= 1.0)
                    ? cecSingularBand
                    : defaultEcProjectState.projectGeneral.cec_singular_band;
        //> Anything but 1 is the paper's criterion, a value from some later
        //> version this build does not understand included. Falling back to
        //> the published one is the safe direction to be wrong in, and it is
        //> what the engine does with the same key.
        ec_project_state_.projectGeneral.cec_stationarity_mode
                = (project_ini.value(EcIni::INI_PROJECT_81,
                                     defaultEcProjectState.projectGeneral.cec_stationarity_mode)
                       .toInt() == 1) ? 1 : 0;
        readCecPairs(project_ini);
        ec_project_state_.projectGeneral.tob1_format
                = project_ini.value(EcIni::INI_PROJECT_50,
                                    defaultEcProjectState.projectGeneral.tob1_format).toInt();
        ec_project_state_.projectGeneral.out_path
                = project_ini.value(EcIni::INI_PROJECT_51,
                                    defaultEcProjectState.projectGeneral.out_path).toString();
        ec_project_state_.projectGeneral.fix_out_format
                = project_ini.value(EcIni::INI_PROJECT_52,
                                    defaultEcProjectState.projectGeneral.fix_out_format).toInt();
        ec_project_state_.projectGeneral.err_label
                = project_ini.value(EcIni::INI_PROJECT_53,
                                    defaultEcProjectState.projectGeneral.err_label).toString();
        ec_project_state_.projectGeneral.qcflag_meth
                = project_ini.value(EcIni::INI_PROJECT_55,
                                    defaultEcProjectState.projectGeneral.qcflag_meth).toInt();
        ec_project_state_.projectGeneral.use_biomet
                = project_ini.value(EcIni::INI_PROJECT_34,
                                    defaultEcProjectState.projectGeneral.use_biomet).toInt();
        ec_project_state_.projectGeneral.biom_file
                = project_ini.value(EcIni::INI_PROJECT_35,
                                    defaultEcProjectState.projectGeneral.biom_file).toString();
        ec_project_state_.projectGeneral.biom_dir
                = project_ini.value(EcIni::INI_PROJECT_57,
                                    defaultEcProjectState.projectGeneral.biom_dir).toString();
        ec_project_state_.projectGeneral.biom_recurse
                = project_ini.value(EcIni::INI_PROJECT_58,
                                    defaultEcProjectState.projectGeneral.biom_recurse).toInt();
        ec_project_state_.projectGeneral.biom_ext
                = project_ini.value(EcIni::INI_PROJECT_59,
                                    defaultEcProjectState.projectGeneral.biom_ext).toString().remove(QLatin1Char('.'));
        ec_project_state_.projectGeneral.out_mean_cosp
                = project_ini.value(EcIni::INI_PROJECT_60,
                                    defaultEcProjectState.projectGeneral.out_mean_cosp).toInt();
        ec_project_state_.projectGeneral.out_biomet
                = project_ini.value(EcIni::INI_PROJECT_61,
                                    defaultEcProjectState.projectGeneral.out_biomet).toInt();
        ec_project_state_.projectGeneral.bin_sp_avail
                = project_ini.value(EcIni::INI_PROJECT_62,
                                    defaultEcProjectState.projectGeneral.bin_sp_avail).toInt();
        ec_project_state_.projectGeneral.full_sp_avail
                = project_ini.value(EcIni::INI_PROJECT_63,
                                    defaultEcProjectState.projectGeneral.full_sp_avail).toInt();
        ec_project_state_.projectGeneral.files_found
                = project_ini.value(EcIni::INI_PROJECT_64,
                                    defaultEcProjectState.projectGeneral.files_found).toInt();
        ec_project_state_.projectGeneral.out_mean_spectra
                = project_ini.value(EcIni::INI_PROJECT_65,
                                    defaultEcProjectState.projectGeneral.out_mean_spectra).toInt();
        ec_project_state_.projectGeneral.hf_correct_ghg_ba
                = project_ini.value(EcIni::INI_PROJECT_66,
                                    defaultEcProjectState.projectGeneral.hf_correct_ghg_ba).toInt();
        ec_project_state_.projectGeneral.hf_correct_ghg_zoh
                = project_ini.value(EcIni::INI_PROJECT_67,
                                    defaultEcProjectState.projectGeneral.hf_correct_ghg_zoh).toInt();
        ec_project_state_.projectGeneral.sonic_output_rate
                = project_ini.value(EcIni::INI_PROJECT_68,
                                    defaultEcProjectState.projectGeneral.sonic_output_rate).toInt();
    project_ini.endGroup();

    // spec settings section
    project_ini.beginGroup(EcIni::INIGROUP_SPEC_SETTINGS);
        ec_project_state_.spectraSettings.start_sa_date
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_0,
                                    QDate(2000, 1, 1).toString(Qt::ISODate)).toString();
        ec_project_state_.spectraSettings.end_sa_date
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_1,
                                    QDate::currentDate().toString(Qt::ISODate)).toString();
        ec_project_state_.spectraSettings.start_sa_time
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_50,
                                    QTime(0, 0).toString(QStringLiteral("hh:mm"))).toString();
        ec_project_state_.spectraSettings.end_sa_time
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_51,
                                    QTime(23, 59).toString(QStringLiteral("hh:mm"))).toString();
        ec_project_state_.spectraSettings.sa_mode
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_2,
                                    defaultEcProjectState.spectraSettings.sa_mode).toInt();
        ec_project_state_.spectraSettings.sa_file
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_3,
                                    defaultEcProjectState.spectraSettings.sa_file).toString();
        ec_project_state_.spectraSettings.sa_min_smpl
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_4,
                                    defaultEcProjectState.spectraSettings.sa_min_smpl).toInt();
        //> Falls back to sa_fmin_co2, not sa_fmax_co2.
        //>
        //> The key read has always been right; the default named the wrong
        //> field, so a project without the flat key got CO2's *upper* bound as
        //> its lower one - fmin == fmax, an empty frequency range. Latent for
        //> as long as the writer still emitted sa_fmin_co2; the record format
        //> stopped emitting it, so from then on every round-tripped project
        //> took the bad default and carried it onto its CO2 records.
        ec_project_state_.spectraSettings.sa_fmin_co2
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_5,
                                    defaultEcProjectState.spectraSettings.sa_fmin_co2).toDouble();
        ec_project_state_.spectraSettings.sa_fmin_h2o
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_6,
                                    defaultEcProjectState.spectraSettings.sa_fmin_h2o).toDouble();
        ec_project_state_.spectraSettings.sa_fmin_ch4
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_7,
                                    defaultEcProjectState.spectraSettings.sa_fmin_ch4).toDouble();
        ec_project_state_.spectraSettings.sa_fmin_other
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_8,
                                    defaultEcProjectState.spectraSettings.sa_fmin_other).toDouble();
        ec_project_state_.spectraSettings.sa_fmax_co2
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_9,
                                    defaultEcProjectState.spectraSettings.sa_fmax_co2).toDouble();
        ec_project_state_.spectraSettings.sa_fmax_h2o
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_10,
                                    defaultEcProjectState.spectraSettings.sa_fmax_h2o).toDouble();
        ec_project_state_.spectraSettings.sa_fmax_ch4
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_11,
                                    defaultEcProjectState.spectraSettings.sa_fmax_ch4).toDouble();
        ec_project_state_.spectraSettings.sa_fmax_other
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_12,
                                    defaultEcProjectState.spectraSettings.sa_fmax_other).toDouble();
        ec_project_state_.spectraSettings.sa_hfn_co2_fmin
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_13,
                                    defaultEcProjectState.spectraSettings.sa_hfn_co2_fmin).toDouble();
        ec_project_state_.spectraSettings.sa_hfn_h2o_fmin
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_14,
                                    defaultEcProjectState.spectraSettings.sa_hfn_h2o_fmin).toDouble();
        ec_project_state_.spectraSettings.sa_hfn_ch4_fmin
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_15,
                                    defaultEcProjectState.spectraSettings.sa_hfn_ch4_fmin).toDouble();
        ec_project_state_.spectraSettings.sa_hfn_other_fmin
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_16,
                                    defaultEcProjectState.spectraSettings.sa_hfn_other_fmin).toDouble();
        ec_project_state_.spectraSettings.sa_min_un_ustar
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_36,
                                    defaultEcProjectState.spectraSettings.sa_min_un_ustar).toDouble();
        ec_project_state_.spectraSettings.sa_min_un_h
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_37,
                                    defaultEcProjectState.spectraSettings.sa_min_un_h).toDouble();
        ec_project_state_.spectraSettings.sa_min_un_le
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_38,
                                    defaultEcProjectState.spectraSettings.sa_min_un_le).toDouble();
        ec_project_state_.spectraSettings.sa_min_un_co2
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_39,
                                    defaultEcProjectState.spectraSettings.sa_min_un_co2).toDouble();
        ec_project_state_.spectraSettings.sa_min_un_ch4
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_40,
                                    defaultEcProjectState.spectraSettings.sa_min_un_ch4).toDouble();
        ec_project_state_.spectraSettings.sa_min_un_other
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_41,
                                    defaultEcProjectState.spectraSettings.sa_min_un_other).toDouble();
        ec_project_state_.spectraSettings.sa_min_st_ustar
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_35,
                                    defaultEcProjectState.spectraSettings.sa_min_st_ustar).toDouble();
        ec_project_state_.spectraSettings.sa_min_st_h
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_21,
                                    defaultEcProjectState.spectraSettings.sa_min_st_h).toDouble();
        ec_project_state_.spectraSettings.sa_min_st_le
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_20,
                                    defaultEcProjectState.spectraSettings.sa_min_st_le).toDouble();
        ec_project_state_.spectraSettings.sa_min_st_co2
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_17,
                                    defaultEcProjectState.spectraSettings.sa_min_st_co2).toDouble();
        ec_project_state_.spectraSettings.sa_min_st_ch4
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_18,
                                    defaultEcProjectState.spectraSettings.sa_min_st_ch4).toDouble();
        ec_project_state_.spectraSettings.sa_min_st_other
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_19,
                                    defaultEcProjectState.spectraSettings.sa_min_st_other).toDouble();
        ec_project_state_.spectraSettings.sa_max_ustar
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_42,
                                    defaultEcProjectState.spectraSettings.sa_max_ustar).toDouble();
        ec_project_state_.spectraSettings.sa_max_h
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_43,
                                    defaultEcProjectState.spectraSettings.sa_max_h).toDouble();
        ec_project_state_.spectraSettings.sa_max_le
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_44,
                                    defaultEcProjectState.spectraSettings.sa_max_le).toDouble();
        ec_project_state_.spectraSettings.sa_max_co2
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_45,
                                    defaultEcProjectState.spectraSettings.sa_max_co2).toDouble();
        ec_project_state_.spectraSettings.sa_max_ch4
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_46,
                                    defaultEcProjectState.spectraSettings.sa_max_ch4).toDouble();
        ec_project_state_.spectraSettings.sa_max_other
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_47,
                                    defaultEcProjectState.spectraSettings.sa_max_other).toDouble();
        ec_project_state_.spectraSettings.add_sonic_lptf
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_22,
                                    defaultEcProjectState.spectraSettings.add_sonic_lptf).toInt();
        ec_project_state_.spectraSettings.horst_lens
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_28,
                                    defaultEcProjectState.spectraSettings.horst_lens).toInt();
        ec_project_state_.spectraSettings.ex_file
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_29,
                                    defaultEcProjectState.spectraSettings.ex_file).toString();
        ec_project_state_.spectraSettings.sa_bin_spectra
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_30,
                                    defaultEcProjectState.spectraSettings.sa_bin_spectra).toString();
        ec_project_state_.spectraSettings.sa_full_spectra
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_31,
                                    defaultEcProjectState.spectraSettings.sa_full_spectra).toString();
        ec_project_state_.spectraSettings.ex_dir
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_32,
                                    defaultEcProjectState.spectraSettings.ex_dir).toString();
        ec_project_state_.spectraSettings.subset
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_33,
                                    defaultEcProjectState.spectraSettings.subset).toInt();
        ec_project_state_.spectraSettings.use_vm_flags
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_34,
                                    defaultEcProjectState.spectraSettings.use_vm_flags).toInt();
        ec_project_state_.spectraSettings.use_foken_low
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_48,
                                    defaultEcProjectState.spectraSettings.use_foken_low).toInt();
        ec_project_state_.spectraSettings.use_foken_mid
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_49,
                                    defaultEcProjectState.spectraSettings.use_foken_mid).toInt();
        ec_project_state_.spectraSettings.flux_run_mode
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_52,
                                    defaultEcProjectState.spectraSettings.flux_run_mode).toInt();
        ec_project_state_.spectraSettings.automatic_spectra_config
                = project_ini.value(EcIni::INI_SPEC_SETTINGS_53,
                                    defaultEcProjectState.spectraSettings.automatic_spectra_config).toInt();

        //> Per-gas spectral settings. Read after the gas records exist, so the
        //> loop can address them; an absent key leaves the record's sentinel
        //> in place and the page falls back to the flat value.
        for (int i = 0; i < ec_project_state_.projectGeneral.gasColumns.size(); ++i)
        {
            const auto p = QStringLiteral("gas_%1_sa_").arg(i + 1);
            auto& proc = ec_project_state_.projectGeneral.gasColumns[i].proc;

            const auto readInto = [&](const QString& key, qreal& target)
            {
                const auto value = project_ini.value(p + key).toString();
                if (!value.isEmpty()) { target = value.toDouble(); }
            };
            readInto(QStringLiteral("fmin"), proc.saFmin);
            readInto(QStringLiteral("fmax"), proc.saFmax);
            readInto(QStringLiteral("hfn_fmin"), proc.saHfnFmin);
            readInto(QStringLiteral("min_st"), proc.saMinSt);
            readInto(QStringLiteral("min_un"), proc.saMinUn);
            readInto(QStringLiteral("max"), proc.saMax);

            //> The month grouping is a string, so it is read as one rather
            //> than through readInto. Absent leaves it empty, which is the
            //> record stating nothing.
            proc.saMonths
                = project_ini.value(p + QStringLiteral("months")).toString();
        }

        //> An upgraded project's month grouping, folded out of the three flat
        //> tables into the records that held those slots.
        //>
        //> This is the only place it can happen: the tables are twelve
        //> start/stop pairs each and live in this section, so unlike the other
        //> nineteen settings there is no flat state member to migrate from
        //> later - the interface never read them into one.
        //>
        //> A single group spanning the whole calendar is NOT migrated. That is
        //> what the interface wrote unconditionally as a placeholder, for
        //> every project it ever saved, so it records no decision - and it is
        //> the engine's own default for a record that says nothing. Writing it
        //> out would turn a placeholder into a declaration.
        if (wasUpgradedOnLoad_)
        {
            const auto legacyGrouping = [&](const QString& slot) -> QString
            {
                QStringList groups;
                for (int k = 1; k <= 12; ++k)
                {
                    const auto stem = QStringLiteral("sa_%1_g%2_").arg(slot).arg(k);
                    const auto start
                        = project_ini.value(stem + QStringLiteral("start")).toInt();
                    const auto stop
                        = project_ini.value(stem + QStringLiteral("stop")).toInt();
                    if (start < 1 || stop < start || stop > 12) { continue; }
                    groups << QStringLiteral("%1-%2").arg(start).arg(stop);
                }
                if (groups.size() == 1
                    && groups.first() == QLatin1String("1-12"))
                {
                    return QString();
                }
                return groups.join(QLatin1Char(','));
            };

            //> The three tables were labelled co2, ch4 and gas4; water is
            //> classed by relative humidity and never had one. Matched by
            //> species rather than by position, because migration no longer
            //> pads the absent gases and index two is CH4 only on a site that
            //> measures all three.
            const auto tableFor = [](const QString& slug) -> QString
            {
                if (slug == QLatin1String("co2")) { return QStringLiteral("co2"); }
                if (slug == QLatin1String("ch4")) { return QStringLiteral("ch4"); }
                if (slug == QLatin1String("h2o")) { return QString(); }
                return QStringLiteral("gas4");
            };

            //> One table per label, so a second CO2 analyser takes nothing:
            //> the legacy keys can express one grouping per slot.
            QStringList claimed;
            auto& gases = ec_project_state_.projectGeneral.gasColumns;
            for (int i = 0; i < gases.size(); ++i)
            {
                const auto table = tableFor(gases.at(i).slug);
                if (table.isEmpty()) { continue; }
                if (claimed.contains(table)) { continue; }
                claimed << table;
                if (!gases.at(i).proc.saMonths.isEmpty()) { continue; }
                gases[i].proc.saMonths = legacyGrouping(table);
            }
        }
    project_ini.endGroup();

    // preproc general section
    project_ini.beginGroup(EcIni::INIGROUP_SCREEN_GENERAL);
        ec_project_state_.screenGeneral.data_path
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_4,
                                    defaultEcProjectState.screenGeneral.data_path).toString();
        ec_project_state_.screenGeneral.recurse
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_6,
                                    defaultEcProjectState.screenGeneral.recurse).toInt();
        ec_project_state_.screenGeneral.use_geo_north
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_40,
                                    defaultEcProjectState.screenGeneral.use_geo_north).toBool();
        ec_project_state_.screenGeneral.mag_dec
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_38,
                                    defaultEcProjectState.screenGeneral.mag_dec).toReal();
        ec_project_state_.screenGeneral.dec_date
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_39,
                                    defaultEcProjectState.projectGeneral.end_date).toString();
        ec_project_state_.screenGeneral.flag1_col
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_8,
                                    defaultEcProjectState.screenGeneral.flag1_col).toInt();
        ec_project_state_.screenGeneral.flag1_threshold
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_9,
                                    defaultEcProjectState.screenGeneral.flag1_threshold).toReal();
        ec_project_state_.screenGeneral.flag1_policy
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_10,
                                    defaultEcProjectState.screenGeneral.flag1_policy).toInt();
        ec_project_state_.screenGeneral.flag2_col
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_11,
                                    defaultEcProjectState.screenGeneral.flag2_col).toInt();
        ec_project_state_.screenGeneral.flag2_threshold
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_12,
                                    defaultEcProjectState.screenGeneral.flag2_threshold).toReal();
        ec_project_state_.screenGeneral.flag2_policy
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_13,
                                    defaultEcProjectState.screenGeneral.flag2_policy).toInt();
        ec_project_state_.screenGeneral.flag3_col
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_14,
                                    defaultEcProjectState.screenGeneral.flag3_col).toInt();
        ec_project_state_.screenGeneral.flag3_threshold
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_15,
                                    defaultEcProjectState.screenGeneral.flag3_threshold).toReal();
        ec_project_state_.screenGeneral.flag3_policy
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_16,
                                    defaultEcProjectState.screenGeneral.flag3_policy).toInt();
        ec_project_state_.screenGeneral.flag4_col
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_17,
                                    defaultEcProjectState.screenGeneral.flag4_col).toInt();
        ec_project_state_.screenGeneral.flag4_threshold
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_18,
                                    defaultEcProjectState.screenGeneral.flag4_threshold).toReal();
        ec_project_state_.screenGeneral.flag4_policy
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_19,
                                    defaultEcProjectState.screenGeneral.flag4_policy).toInt();
        ec_project_state_.screenGeneral.flag5_col
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_20,
                                    defaultEcProjectState.screenGeneral.flag5_col).toInt();
        ec_project_state_.screenGeneral.flag5_threshold
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_21,
                                    defaultEcProjectState.screenGeneral.flag5_threshold).toReal();
        ec_project_state_.screenGeneral.flag5_policy
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_22,
                                    defaultEcProjectState.screenGeneral.flag5_policy).toInt();
        ec_project_state_.screenGeneral.flag6_col
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_23,
                                    defaultEcProjectState.screenGeneral.flag6_col).toInt();
        ec_project_state_.screenGeneral.flag6_threshold
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_24,
                                    defaultEcProjectState.screenGeneral.flag6_threshold).toReal();
        ec_project_state_.screenGeneral.flag6_policy
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_25,
                                    defaultEcProjectState.screenGeneral.flag6_policy).toInt();
        ec_project_state_.screenGeneral.flag7_col
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_26,
                                    defaultEcProjectState.screenGeneral.flag7_col).toInt();
        ec_project_state_.screenGeneral.flag7_threshold
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_27,
                                    defaultEcProjectState.screenGeneral.flag7_threshold).toReal();
        ec_project_state_.screenGeneral.flag7_policy
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_28,
                                    defaultEcProjectState.screenGeneral.flag7_policy).toInt();
        ec_project_state_.screenGeneral.flag8_col
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_29,
                                    defaultEcProjectState.screenGeneral.flag8_col).toInt();
        ec_project_state_.screenGeneral.flag8_threshold
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_30,
                                    defaultEcProjectState.screenGeneral.flag8_threshold).toReal();
        ec_project_state_.screenGeneral.flag8_policy
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_31,
                                    defaultEcProjectState.screenGeneral.flag8_policy).toInt();
        ec_project_state_.screenGeneral.flag9_col
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_32,
                                    defaultEcProjectState.screenGeneral.flag9_col).toInt();
        ec_project_state_.screenGeneral.flag9_threshold
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_33,
                                    defaultEcProjectState.screenGeneral.flag9_threshold).toReal();
        ec_project_state_.screenGeneral.flag9_policy
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_34,
                                    defaultEcProjectState.screenGeneral.flag9_policy).toInt();
        ec_project_state_.screenGeneral.flag10_col
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_35,
                                    defaultEcProjectState.screenGeneral.flag10_col).toInt();
        ec_project_state_.screenGeneral.flag10_threshold
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_36,
                                    defaultEcProjectState.screenGeneral.flag10_threshold).toReal();
        ec_project_state_.screenGeneral.flag10_policy
                = project_ini.value(EcIni::INI_SCREEN_GENERAL_37,
                                    defaultEcProjectState.screenGeneral.flag10_policy).toInt();
    project_ini.endGroup();

    // preproc setup section
    project_ini.beginGroup(EcIni::INIGROUP_SCREEN_SETTINGS);
        ec_project_state_.screenSetting.max_lack
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_1,
                                    defaultEcProjectState.screenSetting.max_lack).toInt();
        //> Only the slots the file actually carries, so an absent key stays
        //> absent and goes on resolving to max_lack rather than being pinned
        //> to whatever max_lack happened to be when the project was opened.
        ec_project_state_.screenSetting.instr_max_lack.clear();
        for (int slot = 1; slot <= Defs::MAX_INSTRUMENTS; ++slot)
        {
            const auto key = EcIni::iniScreenSettingsInstrMaxLack(slot);
            if (!project_ini.contains(key)) { continue; }
            ec_project_state_.screenSetting.instr_max_lack.insert(
                slot, project_ini.value(key).toInt());
        }
        ec_project_state_.screenSetting.u_offset
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_12,
                                    defaultEcProjectState.screenSetting.u_offset).toDouble();
        ec_project_state_.screenSetting.v_offset
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_13,
                                    defaultEcProjectState.screenSetting.v_offset).toDouble();
        ec_project_state_.screenSetting.w_offset
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_14,
                                    defaultEcProjectState.screenSetting.w_offset).toDouble();
        ec_project_state_.screenSetting.cross_wind
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_2,
                                    defaultEcProjectState.screenSetting.cross_wind).toInt();
        ec_project_state_.screenSetting.gill_wm_wboost
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_101,
                                    defaultEcProjectState.screenSetting.gill_wm_wboost).toInt();
        ec_project_state_.screenSetting.flow_distortion
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_3,
                                    defaultEcProjectState.screenSetting.flow_distortion).toInt();
        ec_project_state_.screenSetting.rot_meth
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_4,
                                    defaultEcProjectState.screenSetting.rot_meth).toInt();
        ec_project_state_.screenSetting.detrend_meth
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_5,
                                    defaultEcProjectState.screenSetting.detrend_meth).toInt();
        ec_project_state_.screenSetting.timeconst
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_6,
                                    defaultEcProjectState.screenSetting.timeconst).toDouble();
        ec_project_state_.screenSetting.tlag_meth
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_7,
                                    defaultEcProjectState.screenSetting.tlag_meth).toInt();
        ec_project_state_.screenSetting.tap_win
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_8,
                                    defaultEcProjectState.screenSetting.tap_win).toInt();
        ec_project_state_.screenSetting.nbins
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_9,
                                    defaultEcProjectState.screenSetting.nbins).toInt();
        ec_project_state_.screenSetting.avrg_len
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_10,
                                    defaultEcProjectState.screenSetting.avrg_len).toInt();
        ec_project_state_.screenSetting.out_bin_sp
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_15,
                                    defaultEcProjectState.screenSetting.out_bin_sp).toInt();
        ec_project_state_.screenSetting.out_bin_og
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_38,
                                    defaultEcProjectState.screenSetting.out_bin_og).toInt();
        ec_project_state_.screenSetting.out_full_sp_u
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_16,
                                    defaultEcProjectState.screenSetting.out_full_sp_u).toInt();
        ec_project_state_.screenSetting.out_full_sp_v
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_17,
                                    defaultEcProjectState.screenSetting.out_full_sp_v).toInt();
        ec_project_state_.screenSetting.out_full_sp_w
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_18,
                                    defaultEcProjectState.screenSetting.out_full_sp_w).toInt();
        ec_project_state_.screenSetting.out_full_sp_ts
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_19,
                                    defaultEcProjectState.screenSetting.out_full_sp_ts).toInt();
        ec_project_state_.screenSetting.out_full_sp_co2
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_20,
                                    defaultEcProjectState.screenSetting.out_full_sp_co2).toInt();
        ec_project_state_.screenSetting.out_full_sp_h2o
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_21,
                                    defaultEcProjectState.screenSetting.out_full_sp_h2o).toInt();
        ec_project_state_.screenSetting.out_full_sp_ch4
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_22,
                                    defaultEcProjectState.screenSetting.out_full_sp_ch4).toInt();
        ec_project_state_.screenSetting.out_full_sp_gas4
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_23,
                                    project_ini.value(QStringLiteral("out_full_sp_n2o"),
                                    defaultEcProjectState.screenSetting.out_full_sp_gas4)).toInt();
        ec_project_state_.screenSetting.out_full_cosp_u
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_31,
                                    defaultEcProjectState.screenSetting.out_full_cosp_u).toInt();
        ec_project_state_.screenSetting.out_full_cosp_v
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_32,
                                    defaultEcProjectState.screenSetting.out_full_cosp_v).toInt();
        ec_project_state_.screenSetting.out_full_cosp_ts
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_33,
                                    defaultEcProjectState.screenSetting.out_full_cosp_ts).toInt();
        ec_project_state_.screenSetting.out_full_cosp_co2
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_34,
                                    defaultEcProjectState.screenSetting.out_full_cosp_co2).toInt();
        ec_project_state_.screenSetting.out_full_cosp_h2o
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_35,
                                    defaultEcProjectState.screenSetting.out_full_cosp_h2o).toInt();
        ec_project_state_.screenSetting.out_full_cosp_ch4
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_36,
                                    defaultEcProjectState.screenSetting.out_full_cosp_ch4).toInt();
        ec_project_state_.screenSetting.out_full_cosp_gas4
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_37,
                                    project_ini.value(QStringLiteral("out_full_cosp_w_n2o"),
                                    defaultEcProjectState.screenSetting.out_full_cosp_gas4)).toInt();
        ec_project_state_.screenSetting.out_st_1
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_24,
                                    defaultEcProjectState.screenSetting.out_st_1).toInt();
        ec_project_state_.screenSetting.out_st_2
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_25,
                                    defaultEcProjectState.screenSetting.out_st_2).toInt();
        ec_project_state_.screenSetting.out_st_3
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_26,
                                    defaultEcProjectState.screenSetting.out_st_3).toInt();
        ec_project_state_.screenSetting.out_st_4
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_27,
                                    defaultEcProjectState.screenSetting.out_st_4).toInt();
        ec_project_state_.screenSetting.out_st_5
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_28,
                                    defaultEcProjectState.screenSetting.out_st_5).toInt();
        ec_project_state_.screenSetting.out_st_6
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_29,
                                    defaultEcProjectState.screenSetting.out_st_6).toInt();
        ec_project_state_.screenSetting.out_st_7
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_30,
                                    defaultEcProjectState.screenSetting.out_st_7).toInt();
        ec_project_state_.screenSetting.out_raw_1
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_82,
                                    defaultEcProjectState.screenSetting.out_raw_1).toInt();
        ec_project_state_.screenSetting.out_raw_2
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_83,
                                    defaultEcProjectState.screenSetting.out_raw_2).toInt();
        ec_project_state_.screenSetting.out_raw_3
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_84,
                                    defaultEcProjectState.screenSetting.out_raw_3).toInt();
        ec_project_state_.screenSetting.out_raw_4
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_85,
                                    defaultEcProjectState.screenSetting.out_raw_4).toInt();
        ec_project_state_.screenSetting.out_raw_5
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_86,
                                    defaultEcProjectState.screenSetting.out_raw_5).toInt();
        ec_project_state_.screenSetting.out_raw_6
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_87,
                                    defaultEcProjectState.screenSetting.out_raw_6).toInt();
        ec_project_state_.screenSetting.out_raw_7
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_88,
                                    defaultEcProjectState.screenSetting.out_raw_7).toInt();
        ec_project_state_.screenSetting.out_raw_u
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_89,
                                    defaultEcProjectState.screenSetting.out_raw_u).toInt();
        ec_project_state_.screenSetting.out_raw_v
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_90,
                                    defaultEcProjectState.screenSetting.out_raw_v).toInt();
        ec_project_state_.screenSetting.out_raw_w
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_91,
                                    defaultEcProjectState.screenSetting.out_raw_w).toInt();
        ec_project_state_.screenSetting.out_raw_ts
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_92,
                                    defaultEcProjectState.screenSetting.out_raw_ts).toInt();
        ec_project_state_.screenSetting.out_raw_co2
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_93,
                                    defaultEcProjectState.screenSetting.out_raw_co2).toInt();
        ec_project_state_.screenSetting.out_raw_h2o
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_94,
                                    defaultEcProjectState.screenSetting.out_raw_h2o).toInt();
        ec_project_state_.screenSetting.out_raw_ch4
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_95,
                                    defaultEcProjectState.screenSetting.out_raw_ch4).toInt();
        ec_project_state_.screenSetting.out_raw_gas4
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_96,
                                    defaultEcProjectState.screenSetting.out_raw_gas4).toInt();
        ec_project_state_.screenSetting.out_raw_tair
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_97,
                                    defaultEcProjectState.screenSetting.out_raw_tair).toInt();
        ec_project_state_.screenSetting.out_raw_pair
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_98,
                                    defaultEcProjectState.screenSetting.out_raw_pair).toInt();
        ec_project_state_.screenSetting.filter_sr
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_40,
                                    defaultEcProjectState.screenSetting.filter_sr).toInt();
        ec_project_state_.screenSetting.filter_al
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_41,
                                    defaultEcProjectState.screenSetting.filter_al).toInt();
        ec_project_state_.screenSetting.bu_corr
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_44,
                                    defaultEcProjectState.screenSetting.bu_corr).toInt();
        ec_project_state_.screenSetting.bu_multi
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_45,
                                    defaultEcProjectState.screenSetting.bu_multi).toInt();
        ec_project_state_.screenSetting.l_day_bot_gain
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_46,
                                    defaultEcProjectState.screenSetting.l_day_bot_gain).toDouble();
        ec_project_state_.screenSetting.l_day_bot_offset
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_47,
                                    defaultEcProjectState.screenSetting.l_day_bot_offset).toDouble();
        ec_project_state_.screenSetting.l_day_top_gain
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_48,
                                    defaultEcProjectState.screenSetting.l_day_top_gain).toDouble();
        ec_project_state_.screenSetting.l_day_top_offset
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_49,
                                    defaultEcProjectState.screenSetting.l_day_top_offset).toDouble();
        ec_project_state_.screenSetting.l_day_spar_gain
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_50,
                                    defaultEcProjectState.screenSetting.l_day_spar_gain).toDouble();
        ec_project_state_.screenSetting.l_day_spar_offset
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_51,
                                    defaultEcProjectState.screenSetting.l_day_spar_offset).toDouble();
        ec_project_state_.screenSetting.l_night_bot_gain
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_52,
                                    defaultEcProjectState.screenSetting.l_night_bot_gain).toDouble();
        ec_project_state_.screenSetting.l_night_bot_offset
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_53,
                                    defaultEcProjectState.screenSetting.l_night_bot_offset).toDouble();
        ec_project_state_.screenSetting.l_night_top_gain
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_54,
                                    defaultEcProjectState.screenSetting.l_night_top_gain).toDouble();
        ec_project_state_.screenSetting.l_night_top_offset
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_55,
                                    defaultEcProjectState.screenSetting.l_night_top_offset).toDouble();
        ec_project_state_.screenSetting.l_night_spar_gain
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_56,
                                    defaultEcProjectState.screenSetting.l_night_spar_gain).toDouble();
        ec_project_state_.screenSetting.l_night_spar_offset
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_57,
                                    defaultEcProjectState.screenSetting.l_night_spar_offset).toDouble();
        ec_project_state_.screenSetting.m_day_bot1
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_58,
                                    defaultEcProjectState.screenSetting.m_day_bot1).toDouble();
        ec_project_state_.screenSetting.m_day_bot2
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_59,
                                    defaultEcProjectState.screenSetting.m_day_bot2).toDouble();
        ec_project_state_.screenSetting.m_day_bot3
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_60,
                                    defaultEcProjectState.screenSetting.m_day_bot3).toDouble();
        ec_project_state_.screenSetting.m_day_bot4
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_61,
                                    defaultEcProjectState.screenSetting.m_day_bot4).toDouble();
        ec_project_state_.screenSetting.m_day_top1
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_62,
                                    defaultEcProjectState.screenSetting.m_day_top1).toDouble();
        ec_project_state_.screenSetting.m_day_top2
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_63,
                                    defaultEcProjectState.screenSetting.m_day_top2).toDouble();
        ec_project_state_.screenSetting.m_day_top3
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_64,
                                    defaultEcProjectState.screenSetting.m_day_top3).toDouble();
        ec_project_state_.screenSetting.m_day_top4
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_65,
                                    defaultEcProjectState.screenSetting.m_day_top4).toDouble();
        ec_project_state_.screenSetting.m_day_spar1
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_66,
                                    defaultEcProjectState.screenSetting.m_day_spar1).toDouble();
        ec_project_state_.screenSetting.m_day_spar2
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_67,
                                    defaultEcProjectState.screenSetting.m_day_spar2).toDouble();
        ec_project_state_.screenSetting.m_day_spar3
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_68,
                                    defaultEcProjectState.screenSetting.m_day_spar3).toDouble();
        ec_project_state_.screenSetting.m_day_spar4
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_69,
                                    defaultEcProjectState.screenSetting.m_day_spar4).toDouble();
        ec_project_state_.screenSetting.m_night_bot1
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_70,
                                    defaultEcProjectState.screenSetting.m_night_bot1).toDouble();
        ec_project_state_.screenSetting.m_night_bot2
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_71,
                                    defaultEcProjectState.screenSetting.m_night_bot2).toDouble();
        ec_project_state_.screenSetting.m_night_bot3
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_72,
                                    defaultEcProjectState.screenSetting.m_night_bot3).toDouble();
        ec_project_state_.screenSetting.m_night_bot4
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_73,
                                    defaultEcProjectState.screenSetting.m_night_bot4).toDouble();
        ec_project_state_.screenSetting.m_night_top1
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_74,
                                    defaultEcProjectState.screenSetting.m_night_top1).toDouble();
        ec_project_state_.screenSetting.m_night_top2
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_75,
                                    defaultEcProjectState.screenSetting.m_night_top2).toDouble();
        ec_project_state_.screenSetting.m_night_top3
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_76,
                                    defaultEcProjectState.screenSetting.m_night_top3).toDouble();
        ec_project_state_.screenSetting.m_night_top4
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_77,
                                    defaultEcProjectState.screenSetting.m_night_top4).toDouble();
        ec_project_state_.screenSetting.m_night_spar1
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_78,
                                    defaultEcProjectState.screenSetting.m_night_spar1).toDouble();
        ec_project_state_.screenSetting.m_night_spar2
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_79,
                                    defaultEcProjectState.screenSetting.m_night_spar2).toDouble();
        ec_project_state_.screenSetting.m_night_spar3
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_80,
                                    defaultEcProjectState.screenSetting.m_night_spar3).toDouble();
        ec_project_state_.screenSetting.m_night_spar4
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_81,
                                    defaultEcProjectState.screenSetting.m_night_spar4).toDouble();
        ec_project_state_.screenSetting.out_details =
                project_ini.value(EcIni::INI_SCREEN_SETTINGS_99,
                                  defaultEcProjectState.screenSetting.out_details).toInt();
        ec_project_state_.screenSetting.power_of_two
                = project_ini.value(EcIni::INI_SCREEN_SETTINGS_100,
                                    defaultEcProjectState.screenSetting.power_of_two).toInt();

        //> Per-gas output selections. Read after the gas records exist, so the
        //> loop can address them; an absent key leaves the record's -1 in
        //> place and the page falls back to the flat value.
        for (int i = 0; i < ec_project_state_.projectGeneral.gasColumns.size(); ++i)
        {
            const auto p = QStringLiteral("gas_%1_out_").arg(i + 1);
            auto& proc = ec_project_state_.projectGeneral.gasColumns[i].proc;

            const auto readInto = [&](const QString& key, int& target)
            {
                const auto value = project_ini.value(p + key).toString();
                if (!value.isEmpty()) { target = value.toInt(); }
            };
            readInto(QStringLiteral("full_sp"), proc.outFullSp);
            readInto(QStringLiteral("full_cosp_w"), proc.outFullCospW);
            readInto(QStringLiteral("raw"), proc.outRaw);
        }
    project_ini.endGroup();

    // preproc test section
    project_ini.beginGroup(EcIni::INIGROUP_SCREEN_TESTS);
        ec_project_state_.screenTest.test_sr
                = project_ini.value(EcIni::INI_SCREEN_TESTS_0,
                                    defaultEcProjectState.screenTest.test_sr).toInt();
        ec_project_state_.screenTest.test_ar
                = project_ini.value(EcIni::INI_SCREEN_TESTS_1,
                                    defaultEcProjectState.screenTest.test_ar).toInt();
        ec_project_state_.screenTest.test_do
                = project_ini.value(EcIni::INI_SCREEN_TESTS_2,
                                    defaultEcProjectState.screenTest.test_do).toInt();
        ec_project_state_.screenTest.test_al
                = project_ini.value(EcIni::INI_SCREEN_TESTS_3,
                                    defaultEcProjectState.screenTest.test_al).toInt();
        ec_project_state_.screenTest.test_sk
                = project_ini.value(EcIni::INI_SCREEN_TESTS_4,
                                    defaultEcProjectState.screenTest.test_sk).toInt();
        ec_project_state_.screenTest.test_ds
                = project_ini.value(EcIni::INI_SCREEN_TESTS_5,
                                    defaultEcProjectState.screenTest.test_ds).toInt();
        ec_project_state_.screenTest.test_tl
                = project_ini.value(EcIni::INI_SCREEN_TESTS_6,
                                    defaultEcProjectState.screenTest.test_tl).toInt();
        ec_project_state_.screenTest.test_aa
                = project_ini.value(EcIni::INI_SCREEN_TESTS_7,
                                    defaultEcProjectState.screenTest.test_aa).toInt();
        ec_project_state_.screenTest.test_ns
                = project_ini.value(EcIni::INI_SCREEN_TESTS_8,
                                    defaultEcProjectState.screenTest.test_ns).toInt();
    project_ini.endGroup();

    // preproc test section
    project_ini.beginGroup(EcIni::INIGROUP_SCREEN_PARAM);
        ec_project_state_.screenParam.sr_num_spk
                = project_ini.value(EcIni::INI_SCREEN_PARAM_0,
                                    defaultEcProjectState.screenParam.sr_num_spk).toInt();
        ec_project_state_.screenParam.sr_lim_u
                = project_ini.value(EcIni::INI_SCREEN_PARAM_1,
                                    defaultEcProjectState.screenParam.sr_lim_u).toDouble();
        ec_project_state_.screenParam.sr_lim_w
                = project_ini.value(EcIni::INI_SCREEN_PARAM_45,
                                    defaultEcProjectState.screenParam.sr_lim_w).toDouble();
        ec_project_state_.screenParam.sr_lim_co2
                = project_ini.value(EcIni::INI_SCREEN_PARAM_46,
                                    defaultEcProjectState.screenParam.sr_lim_co2).toDouble();
        ec_project_state_.screenParam.sr_lim_h2o
                = project_ini.value(EcIni::INI_SCREEN_PARAM_47,
                                    defaultEcProjectState.screenParam.sr_lim_h2o).toDouble();
        ec_project_state_.screenParam.sr_lim_ch4
                = project_ini.value(EcIni::INI_SCREEN_PARAM_48,
                                    defaultEcProjectState.screenParam.sr_lim_ch4).toDouble();
        ec_project_state_.screenParam.sr_lim_other
                = project_ini.value(EcIni::INI_SCREEN_PARAM_49,
                                    project_ini.value(QStringLiteral("sr_lim_n2o"),
                                    defaultEcProjectState.screenParam.sr_lim_other)).toDouble();
        ec_project_state_.screenParam.sr_lim_hf
                = project_ini.value(EcIni::INI_SCREEN_PARAM_2,
                                    defaultEcProjectState.screenParam.sr_lim_hf).toDouble();
        ec_project_state_.screenParam.ar_lim
                = project_ini.value(EcIni::INI_SCREEN_PARAM_3,
                                    defaultEcProjectState.screenParam.ar_lim).toDouble();
        ec_project_state_.screenParam.ar_bins
                = project_ini.value(EcIni::INI_SCREEN_PARAM_4,
                                    defaultEcProjectState.screenParam.ar_bins).toInt();
        ec_project_state_.screenParam.ar_hf_lim
                = project_ini.value(EcIni::INI_SCREEN_PARAM_5,
                                    defaultEcProjectState.screenParam.ar_hf_lim).toInt();
        ec_project_state_.screenParam.do_extlim_dw
                = project_ini.value(EcIni::INI_SCREEN_PARAM_6,
                                    defaultEcProjectState.screenParam.do_extlim_dw).toInt();
        ec_project_state_.screenParam.do_hf1_lim
                = project_ini.value(EcIni::INI_SCREEN_PARAM_7,
                                    defaultEcProjectState.screenParam.do_hf1_lim).toDouble();
        ec_project_state_.screenParam.do_hf2_lim
                = project_ini.value(EcIni::INI_SCREEN_PARAM_8,
                                    defaultEcProjectState.screenParam.do_hf2_lim).toDouble();
        ec_project_state_.screenParam.al_u_max
                = project_ini.value(EcIni::INI_SCREEN_PARAM_9,
                                    defaultEcProjectState.screenParam.al_u_max).toDouble();
        ec_project_state_.screenParam.al_w_max
                = project_ini.value(EcIni::INI_SCREEN_PARAM_10,
                                    defaultEcProjectState.screenParam.al_w_max).toDouble();
        ec_project_state_.screenParam.al_tson_min
                = project_ini.value(EcIni::INI_SCREEN_PARAM_11,
                                    defaultEcProjectState.screenParam.al_tson_min).toDouble();
        ec_project_state_.screenParam.al_tson_max
                = project_ini.value(EcIni::INI_SCREEN_PARAM_12,
                                    defaultEcProjectState.screenParam.al_tson_max).toDouble();
        ec_project_state_.screenParam.al_co2_min
                = project_ini.value(EcIni::INI_SCREEN_PARAM_13,
                                    defaultEcProjectState.screenParam.al_co2_min).toDouble();
        ec_project_state_.screenParam.al_co2_max
                = project_ini.value(EcIni::INI_SCREEN_PARAM_14,
                                    defaultEcProjectState.screenParam.al_co2_max).toDouble();
        ec_project_state_.screenParam.al_h2o_min
                = project_ini.value(EcIni::INI_SCREEN_PARAM_15,
                                    defaultEcProjectState.screenParam.al_h2o_min).toDouble();
        ec_project_state_.screenParam.al_h2o_max
                = project_ini.value(EcIni::INI_SCREEN_PARAM_16,
                                    defaultEcProjectState.screenParam.al_h2o_max).toDouble();
        ec_project_state_.screenParam.al_ch4_min
                = project_ini.value(EcIni::INI_SCREEN_PARAM_54,
                                    defaultEcProjectState.screenParam.al_ch4_min).toDouble();
        ec_project_state_.screenParam.al_ch4_max
                = project_ini.value(EcIni::INI_SCREEN_PARAM_55,
                                    defaultEcProjectState.screenParam.al_ch4_max).toDouble();
        ec_project_state_.screenParam.al_other_min
                = project_ini.value(EcIni::INI_SCREEN_PARAM_56,
                                    project_ini.value(QStringLiteral("al_n2o_min"),
                                    defaultEcProjectState.screenParam.al_other_min)).toDouble();
        ec_project_state_.screenParam.al_other_max
                = project_ini.value(EcIni::INI_SCREEN_PARAM_57,
                                    project_ini.value(QStringLiteral("al_n2o_max"),
                                    defaultEcProjectState.screenParam.al_other_max)).toDouble();
        ec_project_state_.screenParam.sk_hf_skmin
                = project_ini.value(EcIni::INI_SCREEN_PARAM_17,
                                    defaultEcProjectState.screenParam.sk_hf_skmin).toDouble();
        ec_project_state_.screenParam.sk_hf_skmax
                = project_ini.value(EcIni::INI_SCREEN_PARAM_18,
                                    defaultEcProjectState.screenParam.sk_hf_skmax).toDouble();
        ec_project_state_.screenParam.sk_sf_skmin
                = project_ini.value(EcIni::INI_SCREEN_PARAM_19,
                                    defaultEcProjectState.screenParam.sk_sf_skmin).toDouble();
        ec_project_state_.screenParam.sk_sf_skmax
                = project_ini.value(EcIni::INI_SCREEN_PARAM_20,
                                    defaultEcProjectState.screenParam.sk_sf_skmax).toDouble();
        ec_project_state_.screenParam.sk_hf_kumin
                = project_ini.value(EcIni::INI_SCREEN_PARAM_21,
                                    defaultEcProjectState.screenParam.sk_hf_kumin).toDouble();
        ec_project_state_.screenParam.sk_hf_kumax
                = project_ini.value(EcIni::INI_SCREEN_PARAM_22,
                                    defaultEcProjectState.screenParam.sk_hf_kumax).toDouble();
        ec_project_state_.screenParam.sk_sf_kumin
                = project_ini.value(EcIni::INI_SCREEN_PARAM_23,
                                    defaultEcProjectState.screenParam.sk_sf_kumin).toDouble();
        ec_project_state_.screenParam.sk_sf_kumax
                = project_ini.value(EcIni::INI_SCREEN_PARAM_24,
                                    defaultEcProjectState.screenParam.sk_sf_kumax).toDouble();
        ec_project_state_.screenParam.ds_hf_uv
                = project_ini.value(EcIni::INI_SCREEN_PARAM_25,
                                    defaultEcProjectState.screenParam.ds_hf_uv).toDouble();
        ec_project_state_.screenParam.ds_hf_w
                = project_ini.value(EcIni::INI_SCREEN_PARAM_26,
                                    defaultEcProjectState.screenParam.ds_hf_w).toDouble();
        ec_project_state_.screenParam.ds_hf_t
                = project_ini.value(EcIni::INI_SCREEN_PARAM_27,
                                    defaultEcProjectState.screenParam.ds_hf_t).toDouble();
        ec_project_state_.screenParam.ds_hf_co2
                = project_ini.value(EcIni::INI_SCREEN_PARAM_28,
                                    defaultEcProjectState.screenParam.ds_hf_co2).toDouble();
        ec_project_state_.screenParam.ds_hf_h2o
                = project_ini.value(EcIni::INI_SCREEN_PARAM_29,
                                    defaultEcProjectState.screenParam.ds_hf_h2o).toDouble();
        ec_project_state_.screenParam.ds_hf_ch4
                = project_ini.value(EcIni::INI_SCREEN_PARAM_50,
                                    defaultEcProjectState.screenParam.ds_hf_ch4).toDouble();
        ec_project_state_.screenParam.ds_hf_other
                = project_ini.value(EcIni::INI_SCREEN_PARAM_51,
                                    project_ini.value(QStringLiteral("ds_hf_n2o"),
                                    defaultEcProjectState.screenParam.ds_hf_other)).toDouble();
        ec_project_state_.screenParam.ds_hf_var
                = project_ini.value(EcIni::INI_SCREEN_PARAM_30,
                                    defaultEcProjectState.screenParam.ds_hf_var).toDouble();
        ec_project_state_.screenParam.ds_sf_uv
                = project_ini.value(EcIni::INI_SCREEN_PARAM_31,
                                    defaultEcProjectState.screenParam.ds_sf_uv).toDouble();
        ec_project_state_.screenParam.ds_sf_w
                = project_ini.value(EcIni::INI_SCREEN_PARAM_32,
                                    defaultEcProjectState.screenParam.ds_sf_w).toDouble();
        ec_project_state_.screenParam.ds_sf_t
                = project_ini.value(EcIni::INI_SCREEN_PARAM_33,
                                    defaultEcProjectState.screenParam.ds_sf_t).toDouble();
        ec_project_state_.screenParam.ds_sf_co2
                = project_ini.value(EcIni::INI_SCREEN_PARAM_34,
                                    defaultEcProjectState.screenParam.ds_sf_co2).toDouble();
        ec_project_state_.screenParam.ds_sf_h2o
                = project_ini.value(EcIni::INI_SCREEN_PARAM_35,
                                    defaultEcProjectState.screenParam.ds_sf_h2o).toDouble();
        ec_project_state_.screenParam.ds_sf_ch4
                = project_ini.value(EcIni::INI_SCREEN_PARAM_52,
                                    defaultEcProjectState.screenParam.ds_sf_ch4).toDouble();
        ec_project_state_.screenParam.ds_sf_other
                = project_ini.value(EcIni::INI_SCREEN_PARAM_53,
                                    project_ini.value(QStringLiteral("ds_sf_n2o"),
                                    defaultEcProjectState.screenParam.ds_sf_other)).toDouble();
        ec_project_state_.screenParam.ds_sf_var
                = project_ini.value(EcIni::INI_SCREEN_PARAM_36,
                                    defaultEcProjectState.screenParam.ds_sf_var).toDouble();
        ec_project_state_.screenParam.despike_vm
                = project_ini.value(EcIni::INI_SCREEN_PARAM_60,
                                    defaultEcProjectState.screenParam.despike_vm).toInt();
        ec_project_state_.screenParam.tl_hf_lim
                = project_ini.value(EcIni::INI_SCREEN_PARAM_37,
                                    defaultEcProjectState.screenParam.tl_hf_lim).toDouble();
        ec_project_state_.screenParam.tl_sf_lim
                = project_ini.value(EcIni::INI_SCREEN_PARAM_38,
                                    defaultEcProjectState.screenParam.tl_sf_lim).toDouble();
        ec_project_state_.screenParam.tl_def_co2
                = project_ini.value(EcIni::INI_SCREEN_PARAM_39,
                                    defaultEcProjectState.screenParam.tl_def_co2).toDouble();
        ec_project_state_.screenParam.tl_def_h2o
                = project_ini.value(EcIni::INI_SCREEN_PARAM_40,
                                    defaultEcProjectState.screenParam.tl_def_h2o).toDouble();
        ec_project_state_.screenParam.tl_def_ch4
                = project_ini.value(EcIni::INI_SCREEN_PARAM_58,
                                    defaultEcProjectState.screenParam.tl_def_ch4).toDouble();
        ec_project_state_.screenParam.tl_def_other
                = project_ini.value(EcIni::INI_SCREEN_PARAM_59,
                                    project_ini.value(QStringLiteral("tl_def_n2o"),
                                    defaultEcProjectState.screenParam.tl_def_other)).toDouble();
        ec_project_state_.screenParam.aa_min
                = project_ini.value(EcIni::INI_SCREEN_PARAM_41,
                                    defaultEcProjectState.screenParam.aa_min).toDouble();
        ec_project_state_.screenParam.aa_max
                = project_ini.value(EcIni::INI_SCREEN_PARAM_42,
                                    defaultEcProjectState.screenParam.aa_max).toDouble();
        ec_project_state_.screenParam.aa_lim
                = project_ini.value(EcIni::INI_SCREEN_PARAM_43,
                                    defaultEcProjectState.screenParam.aa_lim).toDouble();
        ec_project_state_.screenParam.ns_hf_lim
                = project_ini.value(EcIni::INI_SCREEN_PARAM_44,
                                    defaultEcProjectState.screenParam.ns_hf_lim).toDouble();

        //> Per-gas screening thresholds. Read after the gas records exist, so
        //> the loop can address them; an absent key leaves the record's
        //> sentinel in place and the page falls back to the flat value.
        for (int i = 0; i < ec_project_state_.projectGeneral.gasColumns.size(); ++i)
        {
            const auto p = QStringLiteral("gas_%1_").arg(i + 1);
            auto& proc = ec_project_state_.projectGeneral.gasColumns[i].proc;

            const auto readInto = [&](const QString& key, qreal& target)
            {
                const auto value = project_ini.value(p + key).toString();
                if (!value.isEmpty()) { target = value.toDouble(); }
            };
            readInto(QStringLiteral("sr_lim"), proc.srLim);
            readInto(QStringLiteral("al_min"), proc.alMin);
            readInto(QStringLiteral("al_max"), proc.alMax);
            readInto(QStringLiteral("ds_hf"), proc.dsHf);
            readInto(QStringLiteral("ds_sf"), proc.dsSf);
            readInto(QStringLiteral("tl_def"), proc.tlDef);
        }
    project_ini.endGroup();

    // planar fit section
    project_ini.beginGroup(EcIni::INIGROUP_SCREEN_TILT);
        ec_project_state_.screenTilt.start_date
                = project_ini.value(EcIni::INI_SCREEN_TILT_0,
                                    QDate(2000, 1, 1).toString(Qt::ISODate)).toString();
        ec_project_state_.screenTilt.end_date
                = project_ini.value(EcIni::INI_SCREEN_TILT_1,
                                    QDate::currentDate().toString(Qt::ISODate)).toString();
        ec_project_state_.screenTilt.start_time
                = project_ini.value(EcIni::INI_SCREEN_TILT_12,
                                    QTime(0, 0).toString(QStringLiteral("hh:mm"))).toString();
        ec_project_state_.screenTilt.end_time
                = project_ini.value(EcIni::INI_SCREEN_TILT_13,
                                    QTime(23, 59).toString(QStringLiteral("hh:mm"))).toString();
        ec_project_state_.screenTilt.mode
                = project_ini.value(EcIni::INI_SCREEN_TILT_2,
                                    defaultEcProjectState.screenTilt.mode).toInt();
        ec_project_state_.screenTilt.north_offset
                = project_ini.value(EcIni::INI_SCREEN_TILT_3,
                                    defaultEcProjectState.screenTilt.north_offset).toDouble();
        ec_project_state_.screenTilt.min_num_per_sec
                = project_ini.value(EcIni::INI_SCREEN_TILT_4,
                                    defaultEcProjectState.screenTilt.min_num_per_sec).toInt();
        ec_project_state_.screenTilt.w_max
                = project_ini.value(EcIni::INI_SCREEN_TILT_5,
                                    defaultEcProjectState.screenTilt.w_max).toDouble();
        ec_project_state_.screenTilt.u_min
                = project_ini.value(EcIni::INI_SCREEN_TILT_6,
                                    defaultEcProjectState.screenTilt.u_min).toDouble();
        ec_project_state_.screenTilt.file
                = project_ini.value(EcIni::INI_SCREEN_TILT_7,
                                    defaultEcProjectState.screenTilt.file).toString();
        ec_project_state_.screenTilt.fix_policy
                = project_ini.value(EcIni::INI_SCREEN_TILT_8,
                                    defaultEcProjectState.screenTilt.fix_policy).toInt();
        ec_project_state_.screenTilt.subset
                = project_ini.value(EcIni::INI_SCREEN_TILT_11,
                                    defaultEcProjectState.screenTilt.subset).toInt();
        ec_project_state_.screenTilt.assessment_only
                = project_ini.value(EcIni::INI_SCREEN_TILT_14,
                                    defaultEcProjectState.screenTilt.assessment_only).toInt();

        ec_project_state_.screenTilt.angles.clear();
        int numAngles = countPlanarFitAngles(project_ini.allKeys());
        // iterate through angle list
        for (int k = 0; k < numAngles; ++k)
        {
            QString prefix = EcIni::INI_SCREEN_TILT_PREFIX + QString::number(k + 1) + QStringLiteral("_");

            int exclude = project_ini.value(prefix + EcIni::INI_SCREEN_TILT_10).toInt();
            int include = exclude ? 0 : (exclude + 2);
            Qt::CheckState included = static_cast<Qt::CheckState>(include);

            AngleItem item;
            item.angle_ = project_ini.value(prefix + EcIni::INI_SCREEN_TILT_9).toDouble();
            item.included_ = included;
            item.color_ = WidgetUtils::getColor(k);
            addPlanarFitAngle(item);
        }

    project_ini.endGroup();

    // time lag opt section
    project_ini.beginGroup(EcIni::INIGROUP_TIMELAG_OPT);
        ec_project_state_.timelagOpt.start_date
                = project_ini.value(EcIni::INI_TIMELAG_OPT_0,
                                    QDate(2000, 1, 1).toString(Qt::ISODate)).toString();
        ec_project_state_.timelagOpt.end_date
                = project_ini.value(EcIni::INI_TIMELAG_OPT_1,
                                    QDate::currentDate().toString(Qt::ISODate)).toString();
        ec_project_state_.timelagOpt.start_time
                = project_ini.value(EcIni::INI_TIMELAG_OPT_19,
                                    QTime(0, 0).toString(QStringLiteral("hh:mm"))).toString();
        ec_project_state_.timelagOpt.end_time
                = project_ini.value(EcIni::INI_TIMELAG_OPT_20,
                                    QTime(23, 59).toString(QStringLiteral("hh:mm"))).toString();
        ec_project_state_.timelagOpt.mode
                = project_ini.value(EcIni::INI_TIMELAG_OPT_2,
                                    defaultEcProjectState.timelagOpt.mode).toInt();
        ec_project_state_.timelagOpt.file
                = project_ini.value(EcIni::INI_TIMELAG_OPT_3,
                                    defaultEcProjectState.timelagOpt.file).toString();
        ec_project_state_.timelagOpt.to_h2o_nclass
                = project_ini.value(EcIni::INI_TIMELAG_OPT_17,
                                    defaultEcProjectState.timelagOpt.to_h2o_nclass).toInt();
        ec_project_state_.timelagOpt.co2_min_flux
                = project_ini.value(EcIni::INI_TIMELAG_OPT_4,
                                    defaultEcProjectState.timelagOpt.co2_min_flux).toDouble();
        ec_project_state_.timelagOpt.ch4_min_flux
                = project_ini.value(EcIni::INI_TIMELAG_OPT_5,
                                    defaultEcProjectState.timelagOpt.ch4_min_flux).toDouble();
        ec_project_state_.timelagOpt.gas4_min_flux
                = project_ini.value(EcIni::INI_TIMELAG_OPT_6,
                                    defaultEcProjectState.timelagOpt.gas4_min_flux).toDouble();
        ec_project_state_.timelagOpt.le_min_flux
                = project_ini.value(EcIni::INI_TIMELAG_OPT_7,
                                    defaultEcProjectState.timelagOpt.le_min_flux).toDouble();
        ec_project_state_.timelagOpt.pg_range
                = project_ini.value(EcIni::INI_TIMELAG_OPT_8,
                                    defaultEcProjectState.timelagOpt.pg_range).toDouble();
        ec_project_state_.timelagOpt.co2_min_lag
                = project_ini.value(EcIni::INI_TIMELAG_OPT_9,
                                    defaultEcProjectState.timelagOpt.co2_min_lag).toDouble();
        ec_project_state_.timelagOpt.co2_max_lag
                = project_ini.value(EcIni::INI_TIMELAG_OPT_10,
                                    defaultEcProjectState.timelagOpt.co2_max_lag).toDouble();
        ec_project_state_.timelagOpt.h2o_min_lag
                = project_ini.value(EcIni::INI_TIMELAG_OPT_11,
                                    defaultEcProjectState.timelagOpt.h2o_min_lag).toDouble();
        ec_project_state_.timelagOpt.h2o_max_lag
                = project_ini.value(EcIni::INI_TIMELAG_OPT_12,
                                    defaultEcProjectState.timelagOpt.h2o_max_lag).toDouble();
        ec_project_state_.timelagOpt.ch4_min_lag
                = project_ini.value(EcIni::INI_TIMELAG_OPT_13,
                                    defaultEcProjectState.timelagOpt.ch4_min_lag).toDouble();
        ec_project_state_.timelagOpt.ch4_max_lag
                = project_ini.value(EcIni::INI_TIMELAG_OPT_14,
                                    defaultEcProjectState.timelagOpt.ch4_max_lag).toDouble();
        ec_project_state_.timelagOpt.gas4_min_lag
                = project_ini.value(EcIni::INI_TIMELAG_OPT_15,
                                    defaultEcProjectState.timelagOpt.gas4_min_lag).toDouble();
        ec_project_state_.timelagOpt.gas4_max_lag
                = project_ini.value(EcIni::INI_TIMELAG_OPT_16,
                                    defaultEcProjectState.timelagOpt.gas4_max_lag).toDouble();
        ec_project_state_.timelagOpt.subset
                = project_ini.value(EcIni::INI_TIMELAG_OPT_18,
                                    defaultEcProjectState.timelagOpt.subset).toInt();
        ec_project_state_.timelagOpt.assessment_only
                = project_ini.value(EcIni::INI_TIMELAG_OPT_21,
                                    defaultEcProjectState.timelagOpt.assessment_only).toInt();

        //> Per-gas search windows; absent keys leave the record's sentinel in
        //> place and the dialog falls back to the flat value.
        for (int i = 0; i < ec_project_state_.projectGeneral.gasColumns.size(); ++i)
        {
            const auto pfx = QStringLiteral("gas_%1_to_").arg(i + 1);
            const auto lo = project_ini.value(pfx + QStringLiteral("min_lag")).toString();
            const auto hi = project_ini.value(pfx + QStringLiteral("max_lag")).toString();
            if (!lo.isEmpty())
            {
                ec_project_state_.projectGeneral.gasColumns[i].proc.toMinLag = lo.toDouble();
            }
            if (!hi.isEmpty())
            {
                ec_project_state_.projectGeneral.gasColumns[i].proc.toMaxLag = hi.toDouble();
            }
            const auto mf = project_ini.value(pfx + QStringLiteral("min_flux")).toString();
            if (!mf.isEmpty())
            {
                ec_project_state_.projectGeneral.gasColumns[i].proc.toMinFlux = mf.toDouble();
            }
        }
    project_ini.endGroup();

    // PWB time lag section
    project_ini.beginGroup(EcIni::INIGROUP_PWB_TIMELAG);
        ec_project_state_.pwbTimelag.co2_min_lag
                = project_ini.value(EcIni::INI_PWB_TIMELAG_0,
                                    defaultEcProjectState.pwbTimelag.co2_min_lag).toDouble();
        ec_project_state_.pwbTimelag.co2_max_lag
                = project_ini.value(EcIni::INI_PWB_TIMELAG_1,
                                    defaultEcProjectState.pwbTimelag.co2_max_lag).toDouble();
        ec_project_state_.pwbTimelag.h2o_min_lag
                = project_ini.value(EcIni::INI_PWB_TIMELAG_2,
                                    defaultEcProjectState.pwbTimelag.h2o_min_lag).toDouble();
        ec_project_state_.pwbTimelag.h2o_max_lag
                = project_ini.value(EcIni::INI_PWB_TIMELAG_3,
                                    defaultEcProjectState.pwbTimelag.h2o_max_lag).toDouble();
        ec_project_state_.pwbTimelag.ch4_min_lag
                = project_ini.value(EcIni::INI_PWB_TIMELAG_4,
                                    defaultEcProjectState.pwbTimelag.ch4_min_lag).toDouble();
        ec_project_state_.pwbTimelag.ch4_max_lag
                = project_ini.value(EcIni::INI_PWB_TIMELAG_5,
                                    defaultEcProjectState.pwbTimelag.ch4_max_lag).toDouble();
        ec_project_state_.pwbTimelag.gas4_min_lag
                = project_ini.value(EcIni::INI_PWB_TIMELAG_6,
                                    defaultEcProjectState.pwbTimelag.gas4_min_lag).toDouble();
        ec_project_state_.pwbTimelag.gas4_max_lag
                = project_ini.value(EcIni::INI_PWB_TIMELAG_7,
                                    defaultEcProjectState.pwbTimelag.gas4_max_lag).toDouble();
        ec_project_state_.pwbTimelag.n_bootstrap
                = project_ini.value(EcIni::INI_PWB_TIMELAG_8,
                                    defaultEcProjectState.pwbTimelag.n_bootstrap).toInt();
        ec_project_state_.pwbTimelag.block_length_s
                = project_ini.value(EcIni::INI_PWB_TIMELAG_9,
                                    defaultEcProjectState.pwbTimelag.block_length_s).toDouble();
        ec_project_state_.pwbTimelag.min_valid_frac
                = project_ini.value(EcIni::INI_PWB_TIMELAG_10,
                                    defaultEcProjectState.pwbTimelag.min_valid_frac).toDouble();
        ec_project_state_.pwbTimelag.hdi_thresh_s
                = project_ini.value(EcIni::INI_PWB_TIMELAG_11,
                                    defaultEcProjectState.pwbTimelag.hdi_thresh_s).toDouble();
        ec_project_state_.pwbTimelag.dev_thresh_s
                = project_ini.value(EcIni::INI_PWB_TIMELAG_12,
                                    defaultEcProjectState.pwbTimelag.dev_thresh_s).toDouble();
        ec_project_state_.pwbTimelag.hdi_prefilter_s
                = project_ini.value(EcIni::INI_PWB_TIMELAG_13,
                                    defaultEcProjectState.pwbTimelag.hdi_prefilter_s).toDouble();
        ec_project_state_.pwbTimelag.smoothing_width
                = project_ini.value(EcIni::INI_PWB_TIMELAG_14,
                                    defaultEcProjectState.pwbTimelag.smoothing_width).toInt();
        ec_project_state_.pwbTimelag.random_seed
                = project_ini.value(EcIni::INI_PWB_TIMELAG_15,
                                    defaultEcProjectState.pwbTimelag.random_seed).toInt();
        ec_project_state_.pwbTimelag.max_carry_h
                = project_ini.value(EcIni::INI_PWB_TIMELAG_19,
                                    defaultEcProjectState.pwbTimelag.max_carry_h).toDouble();

        //> Per-gas search windows. Read after the gas records exist, so the
        //> loop can address them; absent keys leave the record's sentinel in
        //> place and the dialog falls back to the flat value.
        for (int i = 0; i < ec_project_state_.projectGeneral.gasColumns.size(); ++i)
        {
            const auto p = QStringLiteral("gas_%1_pwb_").arg(i + 1);
            const auto lo = project_ini.value(p + QStringLiteral("min_lag")).toString();
            const auto hi = project_ini.value(p + QStringLiteral("max_lag")).toString();
            if (!lo.isEmpty())
            {
                ec_project_state_.projectGeneral.gasColumns[i].proc.pwbMinLag =
                    lo.toDouble();
            }
            if (!hi.isEmpty())
            {
                ec_project_state_.projectGeneral.gasColumns[i].proc.pwbMaxLag =
                    hi.toDouble();
            }
        }
    project_ini.endGroup();

    // random error section
    //
    // Read from the legacy RawProcess group first, then let [Project] override.
    // A file written before the move carries them only in the legacy group; one
    // written since carries them only in [Project]. Reading both in this order
    // means either kind opens with the user's settings intact, and the next
    // save writes them to the place the engine actually reads.
    project_ini.beginGroup(EcIni::INIGROUP_RAND_ERROR_LEGACY);
        ec_project_state_.randomError.ru_method
                = project_ini.value(EcIni::INI_RAND_ERROR_0,
                                    defaultEcProjectState.randomError.ru_method).toInt();
        ec_project_state_.randomError.its_method
                = project_ini.value(EcIni::INI_RAND_ERROR_1,
                                    defaultEcProjectState.randomError.its_method).toInt();
        ec_project_state_.randomError.its_tlag_max
                = project_ini.value(EcIni::INI_RAND_ERROR_2,
                                    defaultEcProjectState.randomError.its_tlag_max).toDouble();
    project_ini.endGroup();

    project_ini.beginGroup(EcIni::INIGROUP_RAND_ERROR);
        ec_project_state_.randomError.ru_method
                = project_ini.value(EcIni::INI_RAND_ERROR_0,
                                    ec_project_state_.randomError.ru_method).toInt();
        ec_project_state_.randomError.its_method
                = project_ini.value(EcIni::INI_RAND_ERROR_1,
                                    ec_project_state_.randomError.its_method).toInt();
        ec_project_state_.randomError.its_tlag_max
                = project_ini.value(EcIni::INI_RAND_ERROR_2,
                                    ec_project_state_.randomError.its_tlag_max).toDouble();

        // NOTE: temporarly disabled
//        ec_project_state_.randomError.its_sec_factor
//                = project_ini.value(EcIni::INI_RAND_ERROR_3,
//                                    defaultEcProjectState.randomError.its_sec_factor).toDouble();
    project_ini.endGroup();

    // biomet section
    project_ini.beginGroup(EcIni::INIGROUP_BIOMET);
        ec_project_state_.biomParam.col_ta
                = project_ini.value(EcIni::INI_BIOMET_4,
                                    defaultEcProjectState.biomParam.col_ta).toInt() + 1000;
        ec_project_state_.biomParam.col_pa
                = project_ini.value(EcIni::INI_BIOMET_5,
                                    defaultEcProjectState.biomParam.col_pa).toInt() + 1000;
        ec_project_state_.biomParam.col_rh
                = project_ini.value(EcIni::INI_BIOMET_6,
                                    defaultEcProjectState.biomParam.col_rh).toInt();
        ec_project_state_.biomParam.rh_override =
            project_ini.value(EcIni::INI_BIOMET_RH_OVERRIDE, 0).toInt() == 1;
        ec_project_state_.biomParam.col_rg
                = project_ini.value(EcIni::INI_BIOMET_7,
                                    defaultEcProjectState.biomParam.col_rg).toInt();
        ec_project_state_.biomParam.col_lwin
                = project_ini.value(EcIni::INI_BIOMET_8,
                                    defaultEcProjectState.biomParam.col_lwin).toInt();
        ec_project_state_.biomParam.col_ppfd
                = project_ini.value(EcIni::INI_BIOMET_9,
                                    defaultEcProjectState.biomParam.col_ppfd).toInt();
    project_ini.endGroup();

    // wind direction filter section
    project_ini.beginGroup(EcIni::INIGROUP_WIND_FILTER);
        ec_project_state_.windFilter.apply
                = project_ini.value(EcIni::INI_WIND_FILTER_APPLY,
                                    defaultEcProjectState.windFilter.apply).toInt();

        ec_project_state_.windFilter.sectors.clear();
        const QStringList allKeys = project_ini.allKeys();
        QSet<int> sectorIndices;
        for (const QString &key : allKeys) {
            if (key.startsWith(EcIni::INI_WIND_FILTER_PREFIX)) {
                const QString remainder = key.mid(EcIni::INI_WIND_FILTER_PREFIX.length());
                const int sep = remainder.indexOf(QLatin1Char('_'));
                if (sep > 0)
                    sectorIndices.insert(remainder.left(sep).toInt());
            }
        }
        QList<int> orderedIndices = sectorIndices.values();
        std::sort(orderedIndices.begin(), orderedIndices.end());
        for (int idx : orderedIndices) {
            const QString prefix = EcIni::INI_WIND_FILTER_PREFIX + QString::number(idx) + QLatin1Char('_');
            SectorItem sector;
            sector.startAngle_ = project_ini.value(prefix + EcIni::INI_WIND_FILTER_START_SUFFIX, 0.0).toDouble();
            sector.endAngle_   = project_ini.value(prefix + EcIni::INI_WIND_FILTER_END_SUFFIX,   10.0).toDouble();
            ec_project_state_.windFilter.sectors.append(sector);
        }
    project_ini.endGroup();

    datafile.close();

    //> Drop the gases the project names without measuring, now that every
    //> per-gas section has been read - they are keyed by the position the
    //> record had in the file, so this cannot happen any earlier. Before the
    //> signal below, so no page ever sees a placeholder.
    compactGasRecords();

    // just loaded projects are not modified
    setModified(false);
    emit ecProjectChanged();

    //> Guarded, because the parameter defaults to nullptr. That was latent
    //> while every caller happened to pass a pointer; it stopped being latent
    //> when the silent upgrade made this the signal a save depends on.
    if (!isVersionCompatible && modified != nullptr)
        *modified = true;

    //> An upgraded project must be saved back, or the next run still reads
    //> the legacy shape - which nothing writes any more.
    if (wasUpgradedOnLoad_)
    {
        //> Only possible here: the per-gas settings live in five sections
        //> that are read long after the records are built.
        migrateLegacyGasSettings();
        if (modified != nullptr) { *modified = true; }
    }

    //> Unconditional, and after the legacy migration so that a flat value the
    //> old file stated is still preferred over the species default. A
    //> record-format project can carry a gas with no processing block at all -
    //> see repairMissingGasProcessing - and nothing else would ever fill it.
    repairMissingGasProcessing();

    return true;
}

/// Rewrite every moistureRef through \a remap, a map from old 1-based position
/// to new, with 0 meaning "gone".
///
/// Any rearrangement of the gas list has to do this, and there is now more than
/// one: compaction drops records, and choosing a primary instrument reorders
/// them. `moistureRef` is a 1-based index into the very list being rearranged,
/// so a rearrangement that forgets it repoints one gas's humidity at another
/// gas — invisible until a flux is corrected against the wrong water. One
/// implementation, so the second caller cannot be the one that forgets.
static void remapMoistureRefs(QVector<GasRecord>& gases,
                              const QVector<int>& remap)
{
    for (auto& gas : gases)
    {
        //> A reference to a record that went away becomes automatic rather
        //> than some other gas: auto re-resolves on every read, so it repairs
        //> itself instead of silently naming an unrelated species.
        gas.moistureRef = gas.moistureRef > 0 && gas.moistureRef < remap.size()
                ? remap.at(gas.moistureRef) : 0;
    }
}

/// Drop every gas record that names no column, and renumber what pointed past
/// one.
///
/// A record without a column is not a measurement. It used to be kept so that
/// the first four positions stayed pinned to CO2, H2O, CH4 and the open slot,
/// but nothing depends on that any more: the engine derives every
/// species-dependent decision from the record's own `var`, and this interface
/// resolves its table rows the same way. What the placeholder still cost was
/// real - the engine reserves a slot for every record it counts, so an absent
/// gas reached the output as a column of error codes and took the species
/// label with it, renaming a genuine second CH4 to `ch4_2`.
void EcProject::compactGasRecords()
{
    auto& gases = ec_project_state_.projectGeneral.gasColumns;

    //> Old 1-based position to new, or 0 for a record that goes. Built before
    //> anything is removed, because moistureRef is a 1-based index into this
    //> same list and would otherwise point one gas too far for every record
    //> after the hole.
    QVector<int> remap(gases.size() + 1, 0);
    QVector<GasRecord> kept;
    kept.reserve(gases.size());
    for (int i = 0; i < gases.size(); ++i)
    {
        if (gases.at(i).rawColumn <= 0) { continue; }
        kept.append(gases.at(i));
        remap[i + 1] = kept.size();
    }

    if (kept.size() == gases.size()) { return; }

    remapMoistureRefs(kept, remap);
    gases = kept;
}

/// Point every gas at the biomet, or return them all to automatic.
///
/// The flag is only remembered so the tickbox comes back ticked; what the
/// engine acts on is the per-gas references this sets. Which is why unticking
/// restores *automatic* rather than what each gas named before: nothing kept
/// those, and inventing a previous state would be worse than saying plainly
/// that the box does not undo.
bool EcProject::setBiometRhOverride(bool on)
{
    auto& gases = ec_project_state_.projectGeneral.gasColumns;
    const int want = on ? MeasurementRecords::biometMoistureRef : 0;

    bool changed = ec_project_state_.biomParam.rh_override != on;
    ec_project_state_.biomParam.rh_override = on;
    for (auto& gas : gases)
    {
        if (gas.moistureRef == want) { continue; }
        gas.moistureRef = want;
        changed = true;
    }
    if (changed) { setModified(true); }
    return changed;
}

QString EcProject::primaryGasInstrument() const
{
    for (const auto& rec : ec_project_state_.projectGeneral.gasColumns)
    {
        if (MeasurementRecords::isRealInstrument(rec.instrumentId))
        {
            return rec.instrumentId;
        }
    }
    return QString();
}

bool EcProject::setPrimaryGasInstrument(const QString& instrumentId)
{
    auto& gases = ec_project_state_.projectGeneral.gasColumns;
    if (gases.isEmpty()) { return false; }

    //> Which analyser leads is expressed as record order, and as nothing else.
    //>
    //> The engine reads gas records in file order: the full output emits its
    //> column families in that order and numbers a repeated species by it, and
    //> DesignatedGasSlot — which decides the bare FLUXNET species names, the
    //> hygrometer behind the unsuffixed H/LE/ET, and the CEC pair — takes the
    //> first record of each species. Putting the chosen analyser's records
    //> first therefore gives it co2_1, h2o_1 and the unsuffixed FLUXNET
    //> columns with nothing else to set: no new project key, and no engine
    //> change to keep in step with this one.
    //>
    //> Stable within each group, so the order the user arranged among an
    //> analyser's own gases survives; only the two groups move relative to
    //> each other.
    QVector<GasRecord> reordered;
    reordered.reserve(gases.size());
    QVector<int> remap(gases.size() + 1, 0);

    const auto take = [&](bool wantPrimary)
    {
        for (int i = 0; i < gases.size(); ++i)
        {
            const bool isPrimary = !instrumentId.isEmpty()
                    && gases.at(i).instrumentId == instrumentId;
            if (isPrimary != wantPrimary) { continue; }
            reordered.append(gases.at(i));
            remap[i + 1] = reordered.size();
        }
    };
    take(true);
    take(false);

    bool moved = false;
    for (int i = 0; i < gases.size(); ++i)
    {
        if (remap.at(i + 1) != i + 1) { moved = true; break; }
    }
    if (!moved) { return false; }

    //> Built before anything moved, applied to the new list — see
    //> remapMoistureRefs.
    remapMoistureRefs(reordered, remap);
    gases = reordered;
    setModified(true);
    return true;
}

/// Carry the legacy per-gas processing settings onto the records.
///
/// migrateLegacyColumnsToRecords() runs while the [Project] group is being
/// read and can only see the columns; the 19 per-gas settings live in five
/// later sections, so this has to run once the whole file is in. It is called
/// at the end of loadEcProject for an upgraded project.
///
/// **Without this, retiring the flat keys silently resets every upgraded
/// project's thresholds to the built-in defaults** - the record would carry
/// its -1 sentinel and the pages, having lost the flat fallback, would fall
/// through to the species default. The numbers would look plausible and be
/// wrong.
///
/// Records 0..3 are the historical CO2/H2O/CH4/4th-gas slots, which is what
/// makes the mapping possible at all.

//> Which legacy slot a species' thresholds are labelled with.
//>
//> The flat keys are co2/h2o/ch4/other, and they used to be read straight off
//> record 0..3 because migration pinned those four positions. Records are
//> compacted now, so a project without CO2 has water at position zero - and
//> reading positionally would hand it CO2's thresholds. A blank slug is the
//> open slot, whose species the metadata has not resolved yet.
static int legacySlotOfSpecies(const QString& slug)
{
    if (slug == QLatin1String("co2")) { return 0; }
    if (slug == QLatin1String("h2o")) { return 1; }
    if (slug == QLatin1String("ch4")) { return 2; }
    return 3;
}

GasProcessingSettings EcProject::defaultGasProcessing(const QString& rawSlug) const
{
    GasProcessingSettings proc;

    //> Records hold a lowercase slug, but the open slot's dropdown hands over
    //> a display formula - `N2O`, `COS`. Both have to reach the same species,
    //> or a gas selected by name gets the open slot's thresholds while the
    //> same gas selected by column gets its own.
    const QString slug = rawSlug.toLower();

    const auto& sp = ec_project_state_.screenParam;
    const auto& sa = ec_project_state_.spectraSettings;
    const auto& to = ec_project_state_.timelagOpt;
    const auto& pw = ec_project_state_.pwbTimelag;
    const auto& os = ec_project_state_.screenSetting;

    const int slot = legacySlotOfSpecies(slug);

    const qreal srLim[4]  = { sp.sr_lim_co2, sp.sr_lim_h2o, sp.sr_lim_ch4, sp.sr_lim_other };
    const qreal alMin[4]  = { sp.al_co2_min, sp.al_h2o_min, sp.al_ch4_min, sp.al_other_min };
    const qreal alMax[4]  = { sp.al_co2_max, sp.al_h2o_max, sp.al_ch4_max, sp.al_other_max };
    const qreal dsHf[4]   = { sp.ds_hf_co2, sp.ds_hf_h2o, sp.ds_hf_ch4, sp.ds_hf_other };
    const qreal dsSf[4]   = { sp.ds_sf_co2, sp.ds_sf_h2o, sp.ds_sf_ch4, sp.ds_sf_other };
    const qreal tlDef[4]  = { sp.tl_def_co2, sp.tl_def_h2o, sp.tl_def_ch4, sp.tl_def_other };

    //> The noise frequency is spelled sa_hfn_<gas>_fmin.
    const qreal saFmin[4] = { sa.sa_fmin_co2, sa.sa_fmin_h2o, sa.sa_fmin_ch4, sa.sa_fmin_other };
    const qreal saFmax[4] = { sa.sa_fmax_co2, sa.sa_fmax_h2o, sa.sa_fmax_ch4, sa.sa_fmax_other };
    const qreal saHfn[4]  = { sa.sa_hfn_co2_fmin, sa.sa_hfn_h2o_fmin,
                              sa.sa_hfn_ch4_fmin, sa.sa_hfn_other_fmin };

    const qreal toMinLag[4] = { to.co2_min_lag, to.h2o_min_lag, to.ch4_min_lag, to.gas4_min_lag };
    const qreal toMaxLag[4] = { to.co2_max_lag, to.h2o_max_lag, to.ch4_max_lag, to.gas4_max_lag };
    const qreal pwMinLag[4] = { pw.co2_min_lag, pw.h2o_min_lag, pw.ch4_min_lag, pw.gas4_min_lag };
    const qreal pwMaxLag[4] = { pw.co2_max_lag, pw.h2o_max_lag, pw.ch4_max_lag, pw.gas4_max_lag };

    const int outSp[4]   = { os.out_full_sp_co2, os.out_full_sp_h2o,
                             os.out_full_sp_ch4, os.out_full_sp_gas4 };
    const int outCosp[4] = { os.out_full_cosp_co2, os.out_full_cosp_h2o,
                             os.out_full_cosp_ch4, os.out_full_cosp_gas4 };
    const int outRaw[4]  = { os.out_raw_co2, os.out_raw_h2o,
                             os.out_raw_ch4, os.out_raw_gas4 };

    proc.srLim     = srLim[slot];
    proc.alMin     = alMin[slot];
    proc.alMax     = alMax[slot];
    proc.dsHf      = dsHf[slot];
    proc.dsSf      = dsSf[slot];
    proc.tlDef     = tlDef[slot];
    proc.saFmin    = saFmin[slot];
    proc.saFmax    = saFmax[slot];
    proc.saHfnFmin = saHfn[slot];
    proc.toMinLag  = toMinLag[slot];
    proc.toMaxLag  = toMaxLag[slot];
    proc.pwbMinLag = pwMinLag[slot];
    proc.pwbMaxLag = pwMaxLag[slot];
    proc.outFullSp    = outSp[slot];
    proc.outFullCospW = outCosp[slot];
    proc.outRaw       = outRaw[slot];

    //> The species' own plausibility window wins over the slot's.
    //>
    //> The floor: only N2O has a published value; every other species answers
    //> 0, which is the same "no floor" the open slot already used.
    //>
    //> The ceiling: a real number for every species, because the slot's was
    //> shared by everything past methane - N2O and COS were seeded from one
    //> pair though they differ by three orders of magnitude. A species that
    //> states none takes the generic, so this never resolves to 0 and the
    //> engine never sees the max <= min it reads as "limits absent".
    const qreal floor = GasMetadata::defaultAbsoluteLimitMin(slug);
    if (floor > 0.0) { proc.alMin = floor; }
    proc.alMax = GasMetadata::defaultAbsoluteLimitMax(slug);

    //> H2O is the exception in four places, and getting it wrong would move a
    //> latent-heat threshold onto a gas. Its minimum-flux counterpart is
    //> le_min_flux and its spectral QA/QC thresholds are the LE triple -
    //> none is a per-gas quantity, so the water record takes none of them and
    //> they stay at the sentinel, which the writer omits.
    if (slug != QLatin1String("h2o"))
    {
        const qreal toMinFlux[4] = { to.co2_min_flux, -1.0, to.ch4_min_flux, to.gas4_min_flux };
        const qreal saMinUn[4]   = { sa.sa_min_un_co2, -1.0, sa.sa_min_un_ch4, sa.sa_min_un_other };
        const qreal saMinSt[4]   = { sa.sa_min_st_co2, -1.0, sa.sa_min_st_ch4, sa.sa_min_st_other };
        const qreal saMax[4]     = { sa.sa_max_co2, -1.0, sa.sa_max_ch4, sa.sa_max_other };

        proc.toMinFlux = toMinFlux[slot];
        proc.saMinUn   = saMinUn[slot];
        proc.saMinSt   = saMinSt[slot];
        proc.saMax     = saMax[slot];
    }

    return proc;
}

/// Fill one record's unset processing fields from this species' defaults.
///
/// Gap-filling only: a value the file already stated wins. There is no way to
/// state "no threshold" through the interface, so a sentinel here always means
/// the key was absent, never that somebody wanted the test declined.
static void seedGasProcessingGaps(GasProcessingSettings& proc,
                                  const GasProcessingSettings& d)
{
    const auto put = [](qreal& target, qreal value)
    {
        if (target < 0.0) { target = value; }
    };
    const auto putLag = [](qreal& target, qreal value)
    {
        if (target < -9000.0) { target = value; }
    };
    const auto putFlag = [](int& target, int value)
    {
        if (target < 0) { target = value; }
    };

    //> An empty or inverted spectral frequency range counts as unset.
    //>
    //> No configuration can want fmin >= fmax, and the interface cannot
    //> produce one, so a record holding it was written from the loader's
    //> sa_fmin_co2 default when that default still named sa_fmax_co2. That
    //> value is *stated*, not a sentinel, so `put` below would keep it and the
    //> fix to the loader would never reach a project already saved with it.
    if (proc.saFmin >= 0.0 && proc.saFmax >= 0.0 && proc.saFmin >= proc.saFmax)
    {
        proc.saFmin = -1.0;
    }

    put(proc.srLim, d.srLim);
    put(proc.alMin, d.alMin);
    put(proc.alMax, d.alMax);
    put(proc.dsHf, d.dsHf);
    put(proc.dsSf, d.dsSf);
    put(proc.tlDef, d.tlDef);
    put(proc.saFmin, d.saFmin);
    put(proc.saFmax, d.saFmax);
    put(proc.saHfnFmin, d.saHfnFmin);
    put(proc.toMinLag, d.toMinLag);
    put(proc.toMaxLag, d.toMaxLag);
    //> Not put(): that reads any negative value as unset, and a negative
    //> minimum is a legitimate PWB window bound. Only the sentinel is unset.
    putLag(proc.pwbMinLag, d.pwbMinLag);
    putLag(proc.pwbMaxLag, d.pwbMaxLag);
    putFlag(proc.outFullSp, d.outFullSp);
    putFlag(proc.outFullCospW, d.outFullCospW);
    putFlag(proc.outRaw, d.outRaw);
    //> The LE triple and the minimum flux. defaultGasProcessing leaves
    //> these at the sentinel for water, which is the carve-out that used
    //> to be a second loop with its own tables and its own taken[] array.
    put(proc.toMinFlux, d.toMinFlux);
    put(proc.saMinUn, d.saMinUn);
    put(proc.saMinSt, d.saMinSt);
    put(proc.saMax, d.saMax);
}

void EcProject::migrateLegacyGasSettings()
{
    auto& gases = ec_project_state_.projectGeneral.gasColumns;
    if (gases.isEmpty()) { return; }

    //> Only the first record of each species takes the flat value: the keys
    //> can express one threshold per slot, so a site with two CO2 analysers
    //> has nothing legacy to say about the second.
    bool taken[4] = { false, false, false, false };

    for (int i = 0; i < gases.size(); ++i)
    {
        const int slot = legacySlotOfSpecies(gases.at(i).slug);
        if (taken[slot]) { continue; }
        taken[slot] = true;

        //> Same species-to-threshold mapping a record created from scratch
        //> gets.
        seedGasProcessingGaps(gases[i].proc,
                              defaultGasProcessing(gases.at(i).slug));
    }
}

void EcProject::repairMissingGasProcessing()
{
    auto& gases = ec_project_state_.projectGeneral.gasColumns;

    //> Every record, not the first of each species, and on every load rather
    //> than only on a legacy upgrade.
    //>
    //> A record-format project could carry a gas with no processing block at
    //> all, and stay that way: seedGasRecordsFromMetadata built the records the
    //> metadata preselects without seeding proc, an unset field is written as
    //> no key, an absent key reads back as unset, and migrateLegacyGasSettings
    //> only ever ran on files with no gas_num. The engine read the result as
    //> "not configured" and declined the absolute-limits, spike, discontinuity
    //> and time-lag-window tests for exactly the site's main gases, silently.
    for (int i = 0; i < gases.size(); ++i)
    {
        seedGasProcessingGaps(gases[i].proc,
                              defaultGasProcessing(gases.at(i).slug));
    }
}

/// Whether the project just loaded was written in the pre-record format and
/// had to be migrated. The interface uses this to tell the user once, and to
/// resolve the fourth slot's species from the metadata before the first save.
bool EcProject::wasUpgradedOnLoad() const
{
    return wasUpgradedOnLoad_;
}

void EcProject::clearUpgradedOnLoad()
{
    wasUpgradedOnLoad_ = false;
}

bool EcProject::nativeFormat(const QString &filename)
{
    QFile datafile(filename);
    if (!datafile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        // error opening file
        qWarning() << "Error: Cannot open file: doesn't exists (check the path) "
                   << filename;
        WidgetUtils::warning(nullptr,
                             tr("Load Project Error"),
                             tr("Cannot read file <p>%1:</p>\n<b>%2</b>")
                             .arg(filename, datafile.errorString()));
        return false;
    }

    // test if the first row of the file start with the correct tag
    // case sensitive by default
    QTextStream in(&datafile);
    QString firstLine;
    in >> firstLine;
    datafile.close();

    // filter out metadata files
    if (firstLine.startsWith(Defs::APP_MD_INI_TAG)
        || firstLine.startsWith(QLatin1String(";ECO2S_METADATA"))
        || firstLine.startsWith(QLatin1String(";ECO2S_DATALOGGING"))
        || firstLine.startsWith(QLatin1String(";ECO2catch"))
        || firstLine.startsWith(QLatin1String(";ECCOCatch")))
    {
        WidgetUtils::warning(nullptr,
                             tr("Load Project Error"),
                             tr("Cannot read file <p>%1:</p>\n"
                                "<b>not in %2 native format.</b>").arg(filename, Defs::APP_NAME));
        return false;
    }

    // filter out other generic files
    if (!firstLine.startsWith(Defs::APP_PD_INI_TAG)
        && !firstLine.startsWith(QLatin1String(";ECO2S_PROCESSING"))
        && !firstLine.startsWith(QLatin1String(";ECO2S_DATAPROCESSING")))
    {
        WidgetUtils::warning(nullptr,
                             tr("Load Error"),
                             tr("Cannot read file <p>%1:</p>\n"
                                "<b>not in %2 native format.</b>")
                             .arg(filename, Defs::APP_NAME));
        return false;
    }

    return true;
}

// prepend a known tag to the project file
bool EcProject::tagProject(const QString &filename)
{
    QString app_tag(Defs::APP_PD_INI_TAG);
    app_tag += QLatin1String("\n");

    return FileUtils::prependToFile(app_tag, filename);
}

bool EcProject::eddyProNativeFormat(const QString &filename)
{
    QFile datafile(filename);
    if (!datafile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        WidgetUtils::warning(nullptr,
                             tr("Import Error"),
                             tr("Cannot read file <p>%1:</p>\n<b>%2</b>")
                             .arg(filename, datafile.errorString()));
        return false;
    }
    QTextStream in(&datafile);
    QString firstLine;
    in >> firstLine;
    datafile.close();

    if (!firstLine.startsWith(QLatin1String(";EDDYPRO_PROCESSING")))
    {
        WidgetUtils::warning(nullptr,
                             tr("Import Error"),
                             tr("Cannot import file <p>%1:</p>\n"
                                "<b>not in EddyPro native format.</b>").arg(filename));
        return false;
    }
    return true;
}

bool EcProject::importEddyProProject(const QString &filename, bool updateMode, bool *modified)
{
    //> Pre-process: rename the project keys EddyPro labelled for N2O. The
    //> fourth gas slot takes whatever species the site measured, so the keys
    //> are named for the slot rather than for one gas.
    //>
    //> Instrument model keys are deliberately *not* rewritten here. They do
    //> not exist in this file - they live in the .metadata, under
    //> instr_<n>_model - so a pattern for them can never match. This table
    //> used to carry fifteen such rows that had never once fired. The model
    //> keys are handled where they are actually read: canonicalModelKey() on
    //> the metadata path, master_sonic in loadEcProject, and the engine's own
    //> reader for the copy embedded in a GHG archive.
    QFile srcFile(filename);
    if (!srcFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QString content = QTextStream(&srcFile).readAll();
    srcFile.close();

    bool anyReplaced = false;
    for (const auto& [eddyProKey, ownKey] : fourthGasKeyRenames())
    {
        const QRegularExpression re(
            QStringLiteral("(^|\\n)%1=").arg(eddyProKey));
        QString updated = content;
        updated.replace(re, QStringLiteral("\\1%1=").arg(ownKey));
        if (updated != content) { content = updated; anyReplaced = true; }
    }

    if (!anyReplaced)
        return loadEcProject(filename, updateMode, modified);

    QTemporaryFile tmpFile;
    tmpFile.setAutoRemove(true);
    if (!tmpFile.open())
        return false;
    QTextStream(&tmpFile) << content;
    tmpFile.flush();
    return loadEcProject(tmpFile.fileName(), updateMode, modified);
}

/// Turn a freshly copied native project into an EddyPro one, in place.
///
/// Three jobs: rebuild the flat per-slot keys the record format replaced, drop
/// the keys this fork added, and rename the fourth slot where EddyPro spells it
/// for nitrous oxide. What stays is as important as what goes -
/// fluxnet_standardize_biomet, fluxnet_err_label, wdf_apply and the ru_* trio
/// in [Project] all look like this fork's work and are not: EddyPro 7.0.9
/// writes every one of them, as the reference package shows.
void EcProject::writeEddyProCompatibleKeys(QSettings& ini) const
{
    const auto& g = ec_project_state_.projectGeneral;
    const auto& gases = g.gasColumns;

    //> EddyPro's four pinned slots, rebuilt from this project's records.
    //>
    //> The two lists mean different things. This program's holds only the
    //> gases the site measures, in the order they were selected. EddyPro's is
    //> a fixed layout of four: CO2, H2O, CH4, and a fourth slot for whatever
    //> else the site measured, which its keys spell `n2o` in some sections and
    //> `gas4` in others. A site without methane leaves the third empty and its
    //> COS - or N2O, or anything else - goes in the fourth.
    //>
    //> The pinning is re-established here and nowhere else. It used to be the
    //> record list's own shape, which is why this code could once walk it
    //> positionally; that padding is gone, and walking the compacted list
    //> would file COS under `ch4` and hand the engine a methane flux.
    //>
    //> Only one gas per slot: EddyPro's keys cannot express a second CO2
    //> analyser, and smartfluxBlockReason() refuses such a project before this
    //> runs, so first match is the whole rule.
    int slotOf[kEddyProGasSlots] = { -1, -1, -1, -1 };
    {
        const char* pinned[3] = { "co2", "h2o", "ch4" };
        for (int i = 0; i < gases.size(); ++i)
        {
            const auto& slug = gases.at(i).slug;
            int slot = 3;
            for (int s = 0; s < 3; ++s)
            {
                if (slug == QLatin1String(pinned[s])) { slot = s; break; }
            }
            if (slotOf[slot] < 0) { slotOf[slot] = i; }
        }
    }
    const int n = kEddyProGasSlots;

    //> A record carrying no decision falls back to the same species default
    //> the interface displays for it, so the package reproduces what the user
    //> was shown rather than whatever EddyPro would assume for a missing key.
    const auto pick = [](qreal own, qreal fallback)
    { return own >= 0.0 ? own : fallback; };
    const auto pickLag = [](qreal own, qreal fallback)
    { return (own > -9000.0 && own != -1.0) ? own : fallback; };
    const auto pickFlag = [](int own, int fallback)
    { return own >= 0 ? own : fallback; };

    //> Settings for a slot no record fills. Every field is the "no decision"
    //> sentinel, so each pick() below falls through to the species default -
    //> which is what the reference package holds for a slot the project does
    //> not use, rather than the key being absent.
    static const GasProcessingSettings kUnfilledSlot;
    const auto procFor = [&](int slot) -> const GasProcessingSettings&
    {
        return slotOf[slot] >= 0 ? gases.at(slotOf[slot]).proc : kUnfilledSlot;
    };

    const auto& dsp = defaultSettings.screenParam;
    const auto& dsa = defaultSettings.spectraSettings;
    const auto& dto = defaultSettings.timelagOpt;
    const auto& dos = defaultSettings.screenSetting;

    //> Slot one is water by construction now, since slotRecord finds it by
    //> species. Kept as a table so the exclusions below read the same way they
    //> did, and so an empty slot answers false rather than indexing nothing.
    bool isWater[kEddyProGasSlots];
    for (int s = 0; s < kEddyProGasSlots; ++s)
    {
        isWater[s] = slotOf[s] >= 0
                && gases.at(slotOf[s]).slug == QLatin1String("h2o");
    }

    // ---- [Project] -------------------------------------------------------
    ini.beginGroup(EcIni::INIGROUP_PROJECT);
        removeMatchingKeys(ini, QStringLiteral("^cec_"));
        removeMatchingKeys(ini, QStringLiteral("^(gas|cell|diag)_(num$|\\d+_)"));

        const char* colKey[4] = { "col_co2", "col_h2o", "col_ch4", "col_n2o" };
        for (int i = 0; i < kEddyProGasSlots; ++i)
        {
            const int col = slotOf[i] >= 0 ? gases.at(slotOf[i]).rawColumn : 0;
            ini.setValue(QLatin1String(colKey[i]), col);
        }

        //> Cell and diagnostic columns are found by slug, not by position:
        //> their record lists are ordered by when the user selected them.
        const auto columnForSlug = [](const QVector<MeasurementRecord>& recs,
                                      const char* slug)
        {
            for (const auto& rec : recs)
            {
                if (rec.slug == QLatin1String(slug) && rec.rawColumn > 0)
                {
                    return rec.rawColumn;
                }
            }
            return 0;
        };
        ini.setValue(QStringLiteral("col_int_t_1"), columnForSlug(g.cellColumns, "int_t_1"));
        ini.setValue(QStringLiteral("col_int_t_2"), columnForSlug(g.cellColumns, "int_t_2"));
        ini.setValue(QStringLiteral("col_int_p"),   columnForSlug(g.cellColumns, "int_p"));
        ini.setValue(QStringLiteral("col_cell_t"),  columnForSlug(g.cellColumns, "cell_t"));
        ini.setValue(QStringLiteral("col_diag_75"), columnForSlug(g.diagColumns, "diag_75"));
        ini.setValue(QStringLiteral("col_diag_72"), columnForSlug(g.diagColumns, "diag_72"));
        ini.setValue(QStringLiteral("col_diag_77"), columnForSlug(g.diagColumns, "diag_77"));
        ini.setValue(QStringLiteral("col_diag_anem"), columnForSlug(g.diagColumns, "diag_anem"));

        //> EddyPro carries one molecular weight and diffusivity, for the open
        //> slot; -1 is its "use the built-in constant" sentinel, which is what
        //> the reference package contains.
        const qreal openMw = slotOf[3] >= 0 ? gases.at(slotOf[3]).mw : -1.0;
        const qreal openDiff = slotOf[3] >= 0 ? gases.at(slotOf[3]).diff : -1.0;
        ini.setValue(QStringLiteral("gas_mw"),
                     QString::number(openMw >= 0.0 ? openMw : -1.0, 'f', 4));
        ini.setValue(QStringLiteral("gas_diff"),
                     QString::number(openDiff >= 0.0 ? openDiff : -1.0, 'f', 5));

        //> The file says it was written by the program that can read it. A
        //> module checking either of these against its own build must not be
        //> handed a version pair no EddyPro ever produced.
        ini.setValue(EcIni::INI_PROJECT_4, Defs::SMARTFLUX_SW_VERSION_STR);
        ini.setValue(EcIni::INI_PROJECT_5, Defs::SMARTFLUX_INI_VERSION_STR);
    ini.endGroup();

    // ---- [RawProcess_ParameterSettings] ----------------------------------
    const char* paramSlot[4] = { "co2", "h2o", "ch4", "n2o" };
    ini.beginGroup(EcIni::INIGROUP_SCREEN_PARAM);
        removeMatchingKeys(ini, QStringLiteral("^gas_\\d+_"));
        for (int i = 0; i < n; ++i)
        {
            const auto& p = procFor(i);
            const auto s = QLatin1String(paramSlot[i]);
            const qreal dSr[4]    = { dsp.sr_lim_co2, dsp.sr_lim_h2o, dsp.sr_lim_ch4, dsp.sr_lim_other };
            const qreal dAlMin[4] = { dsp.al_co2_min, dsp.al_h2o_min, dsp.al_ch4_min, dsp.al_other_min };
            const qreal dAlMax[4] = { dsp.al_co2_max, dsp.al_h2o_max, dsp.al_ch4_max, dsp.al_other_max };
            const qreal dHf[4]    = { dsp.ds_hf_co2, dsp.ds_hf_h2o, dsp.ds_hf_ch4, dsp.ds_hf_other };
            const qreal dSf[4]    = { dsp.ds_sf_co2, dsp.ds_sf_h2o, dsp.ds_sf_ch4, dsp.ds_sf_other };
            const qreal dTl[4]    = { dsp.tl_def_co2, dsp.tl_def_h2o, dsp.tl_def_ch4, dsp.tl_def_other };

            ini.setValue(QStringLiteral("sr_lim_%1").arg(s),
                         QString::number(pick(p.srLim, dSr[i]), 'f', 1));
            ini.setValue(QStringLiteral("al_%1_min").arg(s),
                         QString::number(pick(p.alMin, dAlMin[i]), 'f', 3));
            ini.setValue(QStringLiteral("al_%1_max").arg(s),
                         QString::number(pick(p.alMax, dAlMax[i]), 'f', 3));
            ini.setValue(QStringLiteral("ds_hf_%1").arg(s),
                         QString::number(pick(p.dsHf, dHf[i]), 'f', 2));
            ini.setValue(QStringLiteral("ds_sf_%1").arg(s),
                         QString::number(pick(p.dsSf, dSf[i]), 'f', 2));
            ini.setValue(QStringLiteral("tl_def_%1").arg(s),
                         QString::number(pick(p.tlDef, dTl[i]), 'f', 1));
        }
    ini.endGroup();

    // ---- [FluxCorrection_SpectralAnalysis_General] -----------------------
    const char* specSlot[4] = { "co2", "h2o", "ch4", "gas4" };
    ini.beginGroup(EcIni::INIGROUP_SPEC_SETTINGS);
        removeMatchingKeys(ini, QStringLiteral("^gas_\\d+_sa_"));
        ini.remove(EcIni::INI_SPEC_SETTINGS_52);   // flux_run_mode
        ini.remove(EcIni::INI_SPEC_SETTINGS_53);   // automatic_spectra_config
        for (int i = 0; i < n; ++i)
        {
            const auto& p = procFor(i);
            const auto s = QLatin1String(specSlot[i]);
            const qreal dFmin[4] = { dsa.sa_fmin_co2, dsa.sa_fmin_h2o, dsa.sa_fmin_ch4, dsa.sa_fmin_other };
            const qreal dFmax[4] = { dsa.sa_fmax_co2, dsa.sa_fmax_h2o, dsa.sa_fmax_ch4, dsa.sa_fmax_other };
            const qreal dHfn[4]  = { dsa.sa_hfn_co2_fmin, dsa.sa_hfn_h2o_fmin,
                                     dsa.sa_hfn_ch4_fmin, dsa.sa_hfn_other_fmin };

            ini.setValue(QStringLiteral("sa_fmin_%1").arg(s),
                         QString::number(pick(p.saFmin, dFmin[i]), 'f', 4));
            ini.setValue(QStringLiteral("sa_fmax_%1").arg(s),
                         QString::number(pick(p.saFmax, dFmax[i]), 'f', 4));
            ini.setValue(QStringLiteral("sa_hfn_%1_fmin").arg(s),
                         QString::number(pick(p.saHfnFmin, dHfn[i]), 'f', 4));

            //> Water takes none of the QA/QC triple. Those are the latent-heat
            //> thresholds - sa_min_st_le and its pair - and belong to the
            //> project, not to a gas. Skipped by slug rather than by position,
            //> the way the migration that put them on records does it, because
            //> a project without water has some other gas at index one.
            if (isWater[i]) { continue; }
            const qreal dMinSt[4] = { dsa.sa_min_st_co2, -1.0, dsa.sa_min_st_ch4, dsa.sa_min_st_other };
            const qreal dMinUn[4] = { dsa.sa_min_un_co2, -1.0, dsa.sa_min_un_ch4, dsa.sa_min_un_other };
            const qreal dMax[4]   = { dsa.sa_max_co2,    -1.0, dsa.sa_max_ch4,    dsa.sa_max_other };
            ini.setValue(QStringLiteral("sa_min_st_%1").arg(s),
                         QString::number(pick(p.saMinSt, dMinSt[i]), 'f', 4));
            ini.setValue(QStringLiteral("sa_min_un_%1").arg(s),
                         QString::number(pick(p.saMinUn, dMinUn[i]), 'f', 4));
            ini.setValue(QStringLiteral("sa_max_%1").arg(s),
                         QString::number(pick(p.saMax, dMax[i]), 'f', 4));
        }

        //> Month grouping, back out into the twelve start/stop pairs per slot
        //> it was folded up from. An empty grouping records no decision, and
        //> the placeholder for that is one group spanning the calendar - which
        //> is what the reference package carries for all three slots.
        removeMatchingKeys(ini,
            QStringLiteral("^sa_(co2|ch4|gas4)_g\\d+_(start|stop)$"));
        for (int i = 0; i < n; ++i)
        {
            if (isWater[i]) { continue; }
            const auto s = QLatin1String(specSlot[i]);
            auto groups = procFor(i).saMonths.split(QLatin1Char(','),
                                                          Qt::SkipEmptyParts);
            if (groups.isEmpty()) { groups << QStringLiteral("1-12"); }
            for (int k = 0; k < groups.size() && k < 12; ++k)
            {
                const auto bounds = groups.at(k).split(QLatin1Char('-'));
                if (bounds.size() != 2) { continue; }
                const auto stem = QStringLiteral("sa_%1_g%2_").arg(s).arg(k + 1);
                ini.setValue(stem + QStringLiteral("start"), bounds.at(0).toInt());
                ini.setValue(stem + QStringLiteral("stop"), bounds.at(1).toInt());
            }
        }
    ini.endGroup();

    // ---- [RawProcess_Settings] -------------------------------------------
    const char* outSpSlot[4] = { "co2", "h2o", "ch4", "n2o" };
    const char* outRawSlot[4] = { "co2", "h2o", "ch4", "gas4" };
    ini.beginGroup(EcIni::INIGROUP_SCREEN_SETTINGS);
        removeMatchingKeys(ini, QStringLiteral("^gas_\\d+_out_"));
        for (int i = 0; i < n; ++i)
        {
            const auto& p = procFor(i);
            const int dSp[4]   = { dos.out_full_sp_co2, dos.out_full_sp_h2o,
                                   dos.out_full_sp_ch4, dos.out_full_sp_gas4 };
            const int dCosp[4] = { dos.out_full_cosp_co2, dos.out_full_cosp_h2o,
                                   dos.out_full_cosp_ch4, dos.out_full_cosp_gas4 };
            const int dRaw[4]  = { dos.out_raw_co2, dos.out_raw_h2o,
                                   dos.out_raw_ch4, dos.out_raw_gas4 };
            ini.setValue(QStringLiteral("out_full_sp_%1").arg(QLatin1String(outSpSlot[i])),
                         pickFlag(p.outFullSp, dSp[i]));
            ini.setValue(QStringLiteral("out_full_cosp_w_%1").arg(QLatin1String(outSpSlot[i])),
                         pickFlag(p.outFullCospW, dCosp[i]));
            ini.setValue(QStringLiteral("out_raw_%1").arg(QLatin1String(outRawSlot[i])),
                         pickFlag(p.outRaw, dRaw[i]));
        }
    ini.endGroup();

    // ---- [RawProcess_TiltCorrection_Settings] ----------------------------
    ini.beginGroup(EcIni::INIGROUP_SCREEN_TILT);
        ini.remove(EcIni::INI_SCREEN_TILT_14);     // rot_pf_assessment_only
    ini.endGroup();

    // ---- [RawProcess_TimelagOptimization_Settings] -----------------------
    const char* toSlot[4] = { "co2", "h2o", "ch4", "gas4" };
    ini.beginGroup(EcIni::INIGROUP_TIMELAG_OPT);
        removeMatchingKeys(ini, QStringLiteral("^gas_\\d+_to_"));
        ini.remove(EcIni::INI_TIMELAG_OPT_21);     // tlag_assessment_only
        for (int i = 0; i < n; ++i)
        {
            const auto& p = procFor(i);
            const auto s = QLatin1String(toSlot[i]);
            const qreal dMin[4] = { dto.co2_min_lag, dto.h2o_min_lag, dto.ch4_min_lag, dto.gas4_min_lag };
            const qreal dMax[4] = { dto.co2_max_lag, dto.h2o_max_lag, dto.ch4_max_lag, dto.gas4_max_lag };
            ini.setValue(QStringLiteral("to_%1_min_lag").arg(s),
                         QString::number(pickLag(p.toMinLag, dMin[i]), 'f', 1));
            ini.setValue(QStringLiteral("to_%1_max_lag").arg(s),
                         QString::number(pickLag(p.toMaxLag, dMax[i]), 'f', 1));

            //> Water's counterpart is to_le_min_flux, a project-wide key that
            //> is already in the file - the same exception the QA/QC triple
            //> takes above, and skipped by slug for the same reason.
            if (isWater[i]) { continue; }
            const qreal dFlux[4] = { dto.co2_min_flux, -1.0, dto.ch4_min_flux, dto.gas4_min_flux };
            ini.setValue(QStringLiteral("to_%1_min_flux").arg(s),
                         QString::number(pick(p.toMinFlux, dFlux[i]), 'f', 3));
        }
    ini.endGroup();

    // ---- the whole pre-whitening group -----------------------------------
    ini.remove(EcIni::INIGROUP_PWB_TIMELAG);

    //> Random error, written where EddyPro keeps it and nowhere else.
    //>
    //> These three carry EddyPro's own names and it reads them from [Project],
    //> which is where this program writes them too - so ordinarily there is
    //> nothing to do. The group below is where they lived for years, and a
    //> project file that has not been saved since still has them there. The
    //> export must not inherit that: it takes the values from the project
    //> rather than from whatever layout the file it was handed happens to
    //> have, so the result is the same whatever the input's history.
    ini.beginGroup(EcIni::INIGROUP_RAND_ERROR);
        ini.setValue(EcIni::INI_RAND_ERROR_0,
                     ec_project_state_.randomError.ru_method);
        ini.setValue(EcIni::INI_RAND_ERROR_1,
                     ec_project_state_.randomError.its_method);
        ini.setValue(EcIni::INI_RAND_ERROR_2,
                     ec_project_state_.randomError.its_tlag_max);
    ini.endGroup();
    ini.remove(EcIni::INIGROUP_RAND_ERROR_LEGACY);

    //> Drop this program's spelling of the nine fourth-slot keys wherever it
    //> survives. Nothing writes them any more, but QSettings keeps whatever it
    //> is not asked to overwrite, so a project carried forward from before the
    //> records can still hold sr_lim_gas4 beside the sr_lim_n2o just written -
    //> and the reader that finds both is not this program's.
    //>
    //> Read backwards from the same list the import reads forwards; removing a
    //> key that is not there costs nothing, so each group is simply offered
    //> all nine rather than being told which of them it could hold.
    for (const auto& group : { EcIni::INIGROUP_PROJECT,
                               EcIni::INIGROUP_SCREEN_PARAM,
                               EcIni::INIGROUP_SCREEN_SETTINGS })
    {
        ini.beginGroup(group);
        for (const auto& [eddyProKey, ownKey] : fourthGasKeyRenames())
        {
            Q_UNUSED(eddyProKey)
            ini.remove(ownKey);
        }
        ini.endGroup();
    }
}

/// Why this project cannot be written as an EddyPro one, or empty when it can.
QString EcProject::smartfluxBlockReason() const
{
    const auto& g = ec_project_state_.projectGeneral;

    int configured = 0;
    for (int i = 0; i < g.gasColumns.size(); ++i)
    {
        if (g.gasColumns.at(i).rawColumn <= 0) { continue; }
        ++configured;
        if (i >= kEddyProGasSlots)
        {
            return tr("This project measures a gas beyond the four EddyPro "
                      "provides for, so a SmartFlux package could not describe "
                      "it. Deselect the extra gases in Basic Settings.");
        }
    }
    if (configured > kEddyProGasSlots)
    {
        return tr("This project measures %1 gases and EddyPro provides for "
                  "four. Deselect the extra gases in Basic Settings.")
                .arg(configured);
    }

    //> One cell block, not one record: EddyPro has a single set of cell slots,
    //> so two analysers each reporting their own cell temperature cannot both
    //> be described.
    const auto instrumentsOf = [](const QVector<MeasurementRecord>& recs)
    {
        QStringList found;
        for (const auto& rec : recs)
        {
            if (rec.rawColumn <= 0) { continue; }
            if (!MeasurementRecords::isRealInstrument(rec.instrumentId)) { continue; }
            if (!found.contains(rec.instrumentId)) { found << rec.instrumentId; }
        }
        return found;
    };
    if (instrumentsOf(g.cellColumns).size() > 1)
    {
        return tr("This project reads cell measurements from more than one "
                  "analyser, and EddyPro has one set of cell slots. A SmartFlux "
                  "package can describe only one of them.");
    }
    if (instrumentsOf(g.diagColumns).size() > 2)
    {
        return tr("This project reads diagnostics from more than two "
                  "instruments, which a SmartFlux package cannot describe.");
    }

    //> Both are this fork's own, and the Processing page makes them
    //> unreachable in SmartFlux mode - but a project saved with them set and
    //> packaged without visiting that page would otherwise slip through.
    if (ec_project_state_.screenSetting.tlag_meth == 5)
    {
        return tr("The pre-whitening block-bootstrap time lag method is not "
                  "available on a SmartFlux module. Choose another method in "
                  "Advanced Settings > Processing Options.");
    }
    if (ec_project_state_.projectGeneral.cec_meth != 0)
    {
        return tr("Conditional Eddy Covariance is not available on a SmartFlux "
                  "module. Switch it off in Advanced Settings > Processing "
                  "Options.");
    }

    return QString();
}

bool EcProject::exportEddyProProject(const QString& sourceFile,
                                     const QString& targetFile) const
{
    //> The tag is a line, not a key, so it cannot be rewritten through
    //> QSettings. Dropped here and put back at the end, which also means the
    //> settings object never sees a stray comment line.
    QFile srcFile(sourceFile);
    if (!srcFile.open(QIODevice::ReadOnly | QIODevice::Text)) { return false; }
    QString content = QTextStream(&srcFile).readAll();
    srcFile.close();

    if (content.startsWith(QLatin1Char(';')))
    {
        const int firstBreak = content.indexOf(QLatin1Char('\n'));
        content = firstBreak < 0 ? QString() : content.mid(firstBreak + 1);
    }

    if (QFile::exists(targetFile) && !QFile::remove(targetFile)) { return false; }
    {
        QFile out(targetFile);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) { return false; }
        QTextStream(&out) << content;
    }

    {
        QSettings ini(targetFile, QSettings::IniFormat);
        writeEddyProCompatibleKeys(ini);
        ini.sync();
        if (ini.status() != QSettings::NoError) { return false; }
    }

    QString tag(Defs::EDDYPRO_PD_INI_TAG);
    tag += QLatin1String("\n");
    return FileUtils::prependToFile(tag, targetFile);
}

void EcProject::setModified(bool mod)
{
    modified_ = mod;
    if (mod)
    {
        emit ecProjectModified();
    }
}

bool EcProject::modified() const
{
    return modified_;
}

void EcProject::setGeneralSubset(int n)
{
    ec_project_state_.projectGeneral.subset = n;
    setModified(true);
}

void EcProject::setSpectraMinSmpl(int n)
{
    ec_project_state_.spectraSettings.sa_min_smpl = n;
    setModified(true);
}

void EcProject::setSpectraFminCo2(double d)
{
    ec_project_state_.spectraSettings.sa_fmin_co2 = d;
    setModified(true);
}

void EcProject::setSpectraFminH2o(double d)
{
    ec_project_state_.spectraSettings.sa_fmin_h2o = d;
    setModified(true);
}

void EcProject::setSpectraFminCh4(double d)
{
    ec_project_state_.spectraSettings.sa_fmin_ch4 = d;
    setModified(true);
}

void EcProject::setSpectraFminGas4(double d)
{
    ec_project_state_.spectraSettings.sa_fmin_other = d;
    setModified(true);
}

void EcProject::setSpectraFmaxCo2(double d)
{
    ec_project_state_.spectraSettings.sa_fmax_co2 = d;
    setModified(true);
}

void EcProject::setSpectraFmaxH2o(double d)
{
    ec_project_state_.spectraSettings.sa_fmax_h2o = d;
    setModified(true);
}

void EcProject::setSpectraFmaxCh4(double d)
{
    ec_project_state_.spectraSettings.sa_fmax_ch4 = d;
    setModified(true);
}

void EcProject::setSpectraFmaxGas4(double d)
{
    ec_project_state_.spectraSettings.sa_fmax_other = d;
    setModified(true);
}

void EcProject::setSpectraHfnCo2(double d)
{
    ec_project_state_.spectraSettings.sa_hfn_co2_fmin = d;
    setModified(true);
}

void EcProject::setSpectraHfnH2o(double d)
{
    ec_project_state_.spectraSettings.sa_hfn_h2o_fmin = d;
    setModified(true);
}

void EcProject::setSpectraHfnCh4(double d)
{
    ec_project_state_.spectraSettings.sa_hfn_ch4_fmin= d;
    setModified(true);
}

void EcProject::setSpectraHfnGas4(double d)
{
    ec_project_state_.spectraSettings.sa_hfn_other_fmin = d;
    setModified(true);
}

void EcProject::setSpectraMinUnstableUstar(double d)
{
    ec_project_state_.spectraSettings.sa_min_un_ustar = d;
    setModified(true);
}

void EcProject::setSpectraMinUnstableH(double d)
{
    ec_project_state_.spectraSettings.sa_min_un_h = d;
    setModified(true);
}

void EcProject::setSpectraMinUnstableLE(double d)
{
    ec_project_state_.spectraSettings.sa_min_un_le = d;
    setModified(true);
}

void EcProject::setSpectraMinUnstableCo2(double d)
{
    ec_project_state_.spectraSettings.sa_min_un_co2 = d;
    setModified(true);
}

void EcProject::setSpectraMinUnstableCh4(double d)
{
    ec_project_state_.spectraSettings.sa_min_un_ch4 = d;
    setModified(true);
}

void EcProject::setSpectraMinUnstableGas4(double d)
{
    ec_project_state_.spectraSettings.sa_min_un_other = d;
    setModified(true);
}

void EcProject::setSpectraMinStableUstar(double d)
{
    ec_project_state_.spectraSettings.sa_min_st_ustar = d;
    setModified(true);
}

void EcProject::setSpectraMinStableH(double d)
{
    ec_project_state_.spectraSettings.sa_min_st_h = d;
    setModified(true);
}

void EcProject::setSpectraMinStableLE(double d)
{
    ec_project_state_.spectraSettings.sa_min_st_le = d;
    setModified(true);
}

void EcProject::setSpectraMinStableCo2(double d)
{
    ec_project_state_.spectraSettings.sa_min_st_co2 = d;
    setModified(true);
}

void EcProject::setSpectraMinStableCh4(double d)
{
    ec_project_state_.spectraSettings.sa_min_st_ch4 = d;
    setModified(true);
}

void EcProject::setSpectraMinStableGas4(double d)
{
    ec_project_state_.spectraSettings.sa_min_st_other = d;
    setModified(true);
}

void EcProject::setSpectraMaxUstar(double d)
{
    ec_project_state_.spectraSettings.sa_max_ustar = d;
    setModified(true);
}

void EcProject::setSpectraMaxH(double d)
{
    ec_project_state_.spectraSettings.sa_max_h = d;
    setModified(true);
}

void EcProject::setSpectraMaxLE(double d)
{
    ec_project_state_.spectraSettings.sa_max_le = d;
    setModified(true);
}

void EcProject::setSpectraMaxCo2(double d)
{
    ec_project_state_.spectraSettings.sa_max_co2 = d;
    setModified(true);
}

void EcProject::setSpectraMaxCh4(double d)
{
    ec_project_state_.spectraSettings.sa_max_ch4 = d;
    setModified(true);
}

void EcProject::setSpectraMaxGas4(double d)
{
    ec_project_state_.spectraSettings.sa_max_other = d;
    setModified(true);
}

void EcProject::setSpectraHorst(int n)
{
    ec_project_state_.spectraSettings.horst_lens = n;
    setModified(true);
}

void EcProject::setSpectraExFile(const QString &p)
{
    ec_project_state_.spectraSettings.ex_file = p;
}

void EcProject::setSpectraBinSpectra(const QString &p)
{
    ec_project_state_.spectraSettings.sa_bin_spectra = p;
    setModified(true);
    emit updateInfo();
}

void EcProject::setSpectraFullSpectra(const QString &p)
{
    ec_project_state_.spectraSettings.sa_full_spectra = p;
    setModified(true);
    emit updateInfo();
}

void EcProject::setSpectraExDir(const QString &p)
{
    ec_project_state_.spectraSettings.ex_dir = p;
    setModified(true);
}

void EcProject::setSpectraSubset(int n)
{
    ec_project_state_.spectraSettings.subset = n;
    setModified(true);
}

void EcProject::setSpectraAddSonic(int n)
{
    ec_project_state_.spectraSettings.add_sonic_lptf = n;
    setModified(true);
}

void EcProject::setScreenDataPath(const QString &p)
{
    ec_project_state_.screenGeneral.data_path = p;
    setModified(true);
    emit updateInfo();
}

void EcProject::setScreenRecurse(int n)
{
    ec_project_state_.screenGeneral.recurse = n;
    setModified(true);
}

void EcProject::setScreenUseGeoNorth(bool b)
{
    ec_project_state_.screenGeneral.use_geo_north = b;
    setModified(true);
    emit updateInfo();
}

void EcProject::setScreenMagDec(double n)
{
    ec_project_state_.screenGeneral.mag_dec = n;
    setModified(true);
}

void EcProject::setScreenDecDate(const QString &d)
{
    ec_project_state_.screenGeneral.dec_date = d;
    setModified(true);
}

void EcProject::setScreenFlag1Col(int n)
{
    ec_project_state_.screenGeneral.flag1_col = n;
    setModified(true);
}

void EcProject::setScreenFlag1Threshold(double n)
{
    ec_project_state_.screenGeneral.flag1_threshold = n;
    setModified(true);
}

void EcProject::setScreenFlag1Policy(int n)
{
    ec_project_state_.screenGeneral.flag1_policy= n;
    setModified(true);
}

void EcProject::setScreenFlag2Col(int n)
{
    ec_project_state_.screenGeneral.flag2_col = n;
    setModified(true);
}

void EcProject::setScreenFlag2Threshold(double n)
{
    ec_project_state_.screenGeneral.flag2_threshold = n;
    setModified(true);
}

void EcProject::setScreenFlag2Policy(int n)
{
    ec_project_state_.screenGeneral.flag2_policy= n;
    setModified(true);
}

void EcProject::setScreenFlag3Col(int n)
{
    ec_project_state_.screenGeneral.flag3_col = n;
    setModified(true);
}

void EcProject::setScreenFlag3Threshold(double n)
{
    ec_project_state_.screenGeneral.flag3_threshold = n;
    setModified(true);
}

void EcProject::setScreenFlag3Policy(int n)
{
    ec_project_state_.screenGeneral.flag3_policy= n;
    setModified(true);
}

void EcProject::setScreenFlag4Col(int n)
{
    ec_project_state_.screenGeneral.flag4_col = n;
    setModified(true);
}

void EcProject::setScreenFlag4Threshold(double n)
{
    ec_project_state_.screenGeneral.flag4_threshold = n;
    setModified(true);
}

void EcProject::setScreenFlag4Policy(int n)
{
    ec_project_state_.screenGeneral.flag4_policy= n;
    setModified(true);
}

void EcProject::setScreenFlag5Col(int n)
{
    ec_project_state_.screenGeneral.flag5_col = n;
    setModified(true);
}

void EcProject::setScreenFlag5Threshold(double n)
{
    ec_project_state_.screenGeneral.flag5_threshold = n;
    setModified(true);
}

void EcProject::setScreenFlag5Policy(int n)
{
    ec_project_state_.screenGeneral.flag5_policy= n;
    setModified(true);
}

void EcProject::setScreenFlag6Col(int n)
{
    ec_project_state_.screenGeneral.flag6_col = n;
    setModified(true);
}

void EcProject::setScreenFlag6Threshold(double n)
{
    ec_project_state_.screenGeneral.flag6_threshold = n;
    setModified(true);
}

void EcProject::setScreenFlag6Policy(int n)
{
    ec_project_state_.screenGeneral.flag6_policy= n;
    setModified(true);
}

void EcProject::setScreenFlag7Col(int n)
{
    ec_project_state_.screenGeneral.flag7_col = n;
    setModified(true);
}

void EcProject::setScreenFlag7Threshold(double n)
{
    ec_project_state_.screenGeneral.flag7_threshold = n;
    setModified(true);
}

void EcProject::setScreenFlag7Policy(int n)
{
    ec_project_state_.screenGeneral.flag7_policy= n;
    setModified(true);
}

void EcProject::setScreenFlag8Col(int n)
{
    ec_project_state_.screenGeneral.flag8_col = n;
    setModified(true);
}

void EcProject::setScreenFlag8Threshold(double n)
{
    ec_project_state_.screenGeneral.flag8_threshold = n;
    setModified(true);
}

void EcProject::setScreenFlag8Policy(int n)
{
    ec_project_state_.screenGeneral.flag8_policy= n;
    setModified(true);
}

void EcProject::setScreenFlag9Col(int n)
{
    ec_project_state_.screenGeneral.flag9_col = n;
    setModified(true);
}

void EcProject::setScreenFlag9Threshold(double n)
{
    ec_project_state_.screenGeneral.flag9_threshold = n;
    setModified(true);
}

void EcProject::setScreenFlag9Policy(int n)
{
    ec_project_state_.screenGeneral.flag9_policy= n;
    setModified(true);
}

void EcProject::setScreenFlag10Col(int n)
{
    ec_project_state_.screenGeneral.flag10_col = n;
    setModified(true);
}

void EcProject::setScreenFlag10Threshold(double n)
{
    ec_project_state_.screenGeneral.flag10_threshold = n;
    setModified(true);
}

void EcProject::setScreenFlag10Policy(int n)
{
    ec_project_state_.screenGeneral.flag10_policy= n;
    setModified(true);
}

void EcProject::setGeneralOutPath(const QString &p)
{
    ec_project_state_.projectGeneral.out_path = p;
    setModified(true);
    emit updateInfo();
}

void EcProject::setGeneralFixedOutFormat(int n)
{
    ec_project_state_.projectGeneral.fix_out_format = n;
    setModified(true);
}

void EcProject::setGeneralErrorLabel(const QString &p)
{
    ec_project_state_.projectGeneral.err_label = p;
    setModified(true);
}

void EcProject::setGeneralOutMeanSpectra(int n)
{
    ec_project_state_.projectGeneral.out_mean_spectra = n;
    setModified(true);
}

void EcProject::setGeneralOutMeanCosp(int n)
{
    ec_project_state_.projectGeneral.out_mean_cosp = n;
    setModified(true);
}

void EcProject::setGeneralBinSpectraAvail(int n)
{
    ec_project_state_.projectGeneral.bin_sp_avail = n;
    setModified(true);
    emit updateInfo();
}

void EcProject::setGeneralFullSpectraAvail(int n)
{
    ec_project_state_.projectGeneral.full_sp_avail = n;
    setModified(true);
    emit updateInfo();
}

void EcProject::setGeneralFilesFound(int n)
{
    ec_project_state_.projectGeneral.files_found = n;

    // NOTE: create side effects when loading a project and then a refresh.
    // in fact, the corresponding recursion checkbox is enough to inform
    // about a possible interactive change
//    setModified(true);
}

void EcProject::setScreenInstrMaxLack(int slot, int n)
{
    auto& lacks = ec_project_state_.screenSetting.instr_max_lack;
    if (n < 0)
    {
        //> Removed rather than stored as a sentinel: the key's absence IS the
        //> "follows the global" state, in the file and in the engine alike.
        if (!lacks.remove(slot)) { return; }
    }
    else
    {
        if (lacks.value(slot, -1) == n) { return; }
        lacks.insert(slot, n);
    }
    setModified(true);
}

void EcProject::setGeneralHfCorrectGhgBa(int n)
{
    ec_project_state_.projectGeneral.hf_correct_ghg_ba = n;
    setModified(true);
    emit updateInfo();
}

void EcProject::setGeneralHfCorrectGhgZoh(int n)
{
    ec_project_state_.projectGeneral.hf_correct_ghg_zoh = n;
    setModified(true);
    emit updateInfo();
}

void EcProject::setGeneralSonicOutputRate(int n)
{
    ec_project_state_.projectGeneral.sonic_output_rate = n;
    setModified(true);
    emit updateInfo();
}

void EcProject::setScreenMaxLack(int n)
{
    ec_project_state_.screenSetting.max_lack = n;
    setModified(true);
}

void EcProject::setScreenUOffset(double d)
{
    ec_project_state_.screenSetting.u_offset = d;
    setModified(true);
}

void EcProject::setScreenVOffset(double d)
{
    ec_project_state_.screenSetting.v_offset = d;
    setModified(true);
}

void EcProject::setScreenWOffset(double d)
{
    ec_project_state_.screenSetting.w_offset = d;
    setModified(true);
}

void EcProject::setScreenWBoost(int n)
{
    ec_project_state_.screenSetting.gill_wm_wboost = n;
    setModified(true);
}

void EcProject::setScreenFlowDistortion(int n)
{
    ec_project_state_.screenSetting.flow_distortion = n;
    setModified(true);
}

void EcProject::setScreenFilterSr(int n)
{
    ec_project_state_.screenSetting.filter_sr = n;
    setModified(true);
}

void EcProject::setScreenFilterAl(int n)
{
    ec_project_state_.screenSetting.filter_al = n;
    setModified(true);
}

void EcProject::setScreenCrossWind(int n)
{
    ec_project_state_.screenSetting.cross_wind = n;
    setModified(true);
}

void EcProject::setScreenRotMethod(int n)
{
    ec_project_state_.screenSetting.rot_meth = n;
    setModified(true);
    emit updateInfo();
}

void EcProject::setScreenDetrendMeth(int n)
{
    ec_project_state_.screenSetting.detrend_meth = n;
    setModified(true);
}

void EcProject::setScreenTimeConst(double n)
{
    ec_project_state_.screenSetting.timeconst = n;
    setModified(true);
}

void EcProject::setScreenTlagMeth(int n)
{
    ec_project_state_.screenSetting.tlag_meth = n;
    setModified(true);
    emit updateInfo();
}

void EcProject::setScreenTapWin(int n)
{
    ec_project_state_.screenSetting.tap_win = n;
    setModified(true);
}

void EcProject::setScreenNBins(int n)
{
    ec_project_state_.screenSetting.nbins = n;
    setModified(true);
}

void EcProject::setScreenOutBinSpectra(int n)
{
    ec_project_state_.screenSetting.out_bin_sp = n;
    setModified(true);
}

void EcProject::setScreenOutBinOgives(int n)
{
    ec_project_state_.screenSetting.out_bin_og = n;
    setModified(true);
}
void EcProject::setScreenOutFullSpectraU(int n)
{
    ec_project_state_.screenSetting.out_full_sp_u = n;
    setModified(true);
}

void EcProject::setScreenOutFullSpectraV(int n)
{
    ec_project_state_.screenSetting.out_full_sp_v = n;
    setModified(true);
}

void EcProject::setScreenOutFullSpectraW(int n)
{
    ec_project_state_.screenSetting.out_full_sp_w = n;
    setModified(true);
}

void EcProject::setScreenOutFullSpectraTs(int n)
{
    ec_project_state_.screenSetting.out_full_sp_ts = n;
    setModified(true);
}

void EcProject::setScreenOutFullSpectraCo2(int n)
{
    ec_project_state_.screenSetting.out_full_sp_co2 = n;
    setModified(true);
}

void EcProject::setScreenOutFullSpectraH2o(int n)
{
    ec_project_state_.screenSetting.out_full_sp_h2o = n;
    setModified(true);
}

void EcProject::setScreenOutFullSpectraCh4(int n)
{
    ec_project_state_.screenSetting.out_full_sp_ch4 = n;
    setModified(true);
}

void EcProject::setScreenOutFullSpectralGas4(int n)
{
    ec_project_state_.screenSetting.out_full_sp_gas4 = n;
    setModified(true);
}

void EcProject::setScreenOutSt1(int n)
{
    ec_project_state_.screenSetting.out_st_1 = n;
    setModified(true);
}

void EcProject::setScreenOutSt2(int n)
{
    ec_project_state_.screenSetting.out_st_2 = n;
    setModified(true);
}

void EcProject::setScreenOutSt3(int n)
{
    ec_project_state_.screenSetting.out_st_3 = n;
    setModified(true);
}

void EcProject::setScreenOutSt4(int n)
{
    ec_project_state_.screenSetting.out_st_4 = n;
    setModified(true);
}

void EcProject::setScreenOutSt5(int n)
{
    ec_project_state_.screenSetting.out_st_5 = n;
    setModified(true);
}

void EcProject::setScreenOutSt6(int n)
{
    ec_project_state_.screenSetting.out_st_6 = n;
    setModified(true);
}

void EcProject::setScreenOutSt7(int n)
{
    ec_project_state_.screenSetting.out_st_7 = n;
    setModified(true);
}
void EcProject::setScreenOutRaw1(int n)
{
    ec_project_state_.screenSetting.out_raw_1 = n;
    setModified(true);
}

void EcProject::setScreenOutRaw2(int n)
{
    ec_project_state_.screenSetting.out_raw_2 = n;
    setModified(true);
}

void EcProject::setScreenOutRaw3(int n)
{
    ec_project_state_.screenSetting.out_raw_3 = n;
    setModified(true);
}

void EcProject::setScreenOutRaw4(int n)
{
    ec_project_state_.screenSetting.out_raw_4 = n;
    setModified(true);
}

void EcProject::setScreenOutRaw5(int n)
{
    ec_project_state_.screenSetting.out_raw_5 = n;
    setModified(true);
}

void EcProject::setScreenOutRaw6(int n)
{
    ec_project_state_.screenSetting.out_raw_6 = n;
    setModified(true);
}

void EcProject::setScreenOutRaw7(int n)
{
    ec_project_state_.screenSetting.out_raw_7 = n;
    setModified(true);
}
void EcProject::setScreenOutRawU(int n)
{
    ec_project_state_.screenSetting.out_raw_u = n;
    setModified(true);
}

void EcProject::setScreenOutRawV(int n)
{
    ec_project_state_.screenSetting.out_raw_v = n;
    setModified(true);
}

void EcProject::setScreenOutRawW(int n)
{
    ec_project_state_.screenSetting.out_raw_w = n;
    setModified(true);
}

void EcProject::setScreenOutRawTs(int n)
{
    ec_project_state_.screenSetting.out_raw_ts = n;
    setModified(true);
}

void EcProject::setScreenOutRawCo2(int n)
{
    ec_project_state_.screenSetting.out_raw_co2 = n;
    setModified(true);
}

void EcProject::setScreenOutRawH2o(int n)
{
    ec_project_state_.screenSetting.out_raw_h2o = n;
    setModified(true);
}

void EcProject::setScreenOutRawCh4(int n)
{
    ec_project_state_.screenSetting.out_raw_ch4 = n;
    setModified(true);
}

void EcProject::setScreenOutRawGas4(int n)
{
    ec_project_state_.screenSetting.out_raw_gas4 = n;
    setModified(true);
}

void EcProject::setScreenOutRawTair(int n)
{
    ec_project_state_.screenSetting.out_raw_tair = n;
    setModified(true);
}

void EcProject::setScreenOutRawPair(int n)
{
    ec_project_state_.screenSetting.out_raw_pair = n;
    setModified(true);
}

void EcProject::setScreenOutFullCospectraU(int n)
{
    ec_project_state_.screenSetting.out_full_cosp_u = n;
    setModified(true);
}

void EcProject::setScreenOutFullCospectraV(int n)
{
    ec_project_state_.screenSetting.out_full_cosp_v = n;
    setModified(true);
}

void EcProject::setScreenOutFullCospectraTs(int n)
{
    ec_project_state_.screenSetting.out_full_cosp_ts = n;
    setModified(true);
}

void EcProject::setScreenOutFullCospectraCo2(int n)
{
    ec_project_state_.screenSetting.out_full_cosp_co2 = n;
    setModified(true);
}

void EcProject::setScreenOutFullCospectraH2o(int n)
{
    ec_project_state_.screenSetting.out_full_cosp_h2o = n;
    setModified(true);
}

void EcProject::setScreenOutFullCospectraCh4(int n)
{
    ec_project_state_.screenSetting.out_full_cosp_ch4 = n;
    setModified(true);
}

void EcProject::setScreenOutFullCospectralGas4(int n)
{
    ec_project_state_.screenSetting.out_full_cosp_gas4 = n;
    setModified(true);
}

void EcProject::setGeneralFpMeth(int n)
{
    ec_project_state_.projectGeneral.foot_meth = n;
    setModified(true);
}

void EcProject::setGeneralCecMeth(int n)
{
    ec_project_state_.projectGeneral.cec_meth = n;
    setModified(true);
}

void EcProject::setGeneralCecH(double d)
{
    ec_project_state_.projectGeneral.cec_h = d;
    setModified(true);
}

void EcProject::setGeneralCecMinO1O2(double d)
{
    ec_project_state_.projectGeneral.cec_min_o1_o2 = d;
    setModified(true);
}

void EcProject::setGeneralCecMinOctant(double d)
{
    ec_project_state_.projectGeneral.cec_min_octant = d;
    setModified(true);
}

void EcProject::setGeneralCecMinValid(double d)
{
    ec_project_state_.projectGeneral.cec_min_valid = d;
    setModified(true);
}

void EcProject::setGeneralCecSignalStrength(double d)
{
    ec_project_state_.projectGeneral.cec_signal_strength = d;
    setModified(true);
}

void EcProject::setGeneralCecMaxGapFill(int n)
{
    ec_project_state_.projectGeneral.cec_max_gap_fill = n;
    setModified(true);
}

void EcProject::setGeneralCecMaxStationarity(double d)
{
    ec_project_state_.projectGeneral.cec_max_stationarity = d;
    setModified(true);
}

void EcProject::setGeneralCecSingularBand(double d)
{
    ec_project_state_.projectGeneral.cec_singular_band = d;
    setModified(true);
}

void EcProject::setGeneralCecStationarityMode(int n)
{
    if (ec_project_state_.projectGeneral.cec_stationarity_mode == n) { return; }
    ec_project_state_.projectGeneral.cec_stationarity_mode = n;
    setModified(true);
}

void EcProject::setCecPairs(const QVector<CecPairRecord>& pairs)
{
    if (ec_project_state_.projectGeneral.cecPairs == pairs) { return; }
    ec_project_state_.projectGeneral.cecPairs = pairs;
    setModified(true);
}

void EcProject::setGeneralTob1Format(int n)
{
    ec_project_state_.projectGeneral.tob1_format = n;
    setModified(true);
}

void EcProject::setGeneralOutRich(int n)
{
    ec_project_state_.projectGeneral.out_rich = n;
    setModified(true);
}

void EcProject::setFluxnetStandardizeBiomet(int n)
{
    ec_project_state_.projectGeneral.fluxnet_standardize_biomet = n;
    setModified(true);
}

void EcProject::setFluxnetErrLabel(int n)
{
    ec_project_state_.projectGeneral.fluxnet_err_label = n;
    setModified(true);
}

void EcProject::setWindFilter(const WindFilterState &state)
{
    ec_project_state_.windFilter = state;
    setModified(true);
}

void EcProject::setWindFilterApply(int n)
{
    ec_project_state_.windFilter.apply = n;
    setModified(true);
}

void EcProject::setGeneralOutMd(int n)
{
    ec_project_state_.projectGeneral.out_md = n;
    setModified(true);
}

void EcProject::setGeneralOutBiomet(int n)
{
    ec_project_state_.projectGeneral.out_biomet = n;
    setModified(true);
}

void EcProject::setScreenTestSr(int l)
{
    ec_project_state_.screenTest.test_sr = l;
    setModified(true);
}

void EcProject::setScreenTestAr(int l)
{
    ec_project_state_.screenTest.test_ar = l;
    setModified(true);
}

void EcProject::setScreenTestDo(int l)
{
    ec_project_state_.screenTest.test_do = l;
    setModified(true);
}

void EcProject::setScreenTestAl(int l)
{
    ec_project_state_.screenTest.test_al = l;
    setModified(true);
}

void EcProject::setScreenTestSk(int l)
{
    ec_project_state_.screenTest.test_sk = l;
    setModified(true);
}

void EcProject::setScreenTestDs(int l)
{
    ec_project_state_.screenTest.test_ds = l;
    setModified(true);
}

void EcProject::setScreenTestTl(int l)
{
    ec_project_state_.screenTest.test_tl = l;
    setModified(true);
}

void EcProject::setScreenTestAa(int l)
{
    ec_project_state_.screenTest.test_aa = l;
    setModified(true);
}

void EcProject::setScreenTestNs(int l)
{
    ec_project_state_.screenTest.test_ns = l;
    setModified(true);
}

void EcProject::setScreenParamSrNumSpk(int n)
{
    ec_project_state_.screenParam.sr_num_spk = n;
    setModified(true);
}

void EcProject::setScreenParamSrHfLim(double n)
{
    ec_project_state_.screenParam.sr_lim_hf = n;
    setModified(true);
}

void EcProject::setScreenParamSrULim(double n)
{
    ec_project_state_.screenParam.sr_lim_u = n;
    setModified(true);
}

void EcProject::setScreenParamSrWLim(double n)
{
    ec_project_state_.screenParam.sr_lim_w = n;
    setModified(true);
}

void EcProject::setScreenParamSrCo2Lim(double n)
{
    ec_project_state_.screenParam.sr_lim_co2 = n;
    setModified(true);
}

void EcProject::setScreenParamSrH2oLim(double n)
{
    ec_project_state_.screenParam.sr_lim_h2o = n;
    setModified(true);
}

void EcProject::setScreenParamSrCh4Lim(double n)
{
    ec_project_state_.screenParam.sr_lim_ch4 = n;
    setModified(true);
}

void EcProject::setScreenParamSrGas4Lim(double n)
{
    ec_project_state_.screenParam.sr_lim_other = n;
    setModified(true);
}

void EcProject::setScreenParamArLim(double n)
{
    ec_project_state_.screenParam.ar_lim = n;
    setModified(true);
}

void EcProject::setScreenParamArBins(int n)
{
    ec_project_state_.screenParam.ar_bins = n;
    setModified(true);
}

void EcProject::setScreenParamArHfLim(int n)
{
    ec_project_state_.screenParam.ar_hf_lim = n;
    setModified(true);
}

void EcProject::setScreenParamDoExtLimDw(int n)
{
    ec_project_state_.screenParam.do_extlim_dw = n;
    setModified(true);
}

void EcProject::setScreenParamDoHf1Lim(double n)
{
    ec_project_state_.screenParam.do_hf1_lim = n;
    setModified(true);
}

void EcProject::setScreenParamDoHf2Lim(double n)
{
    ec_project_state_.screenParam.do_hf2_lim = n;
    setModified(true);
}

void EcProject::setScreenParamAlUMax(double n)
{
    ec_project_state_.screenParam.al_u_max = n;
    setModified(true);
}

void EcProject::setScreenParamAlWMax(double n)
{
    ec_project_state_.screenParam.al_w_max = n;
    setModified(true);
}

void EcProject::setScreenParamAlTsonMin(double n)
{
    ec_project_state_.screenParam.al_tson_min = n;
    setModified(true);
}

void EcProject::setScreenParamAlTsonMax(double n)
{
    ec_project_state_.screenParam.al_tson_max = n;
    setModified(true);
}

void EcProject::setScreenParamAlCo2Min(double n)
{
    ec_project_state_.screenParam.al_co2_min = n;
    setModified(true);
}

void EcProject::setScreenParamAlCo2Max(double n)
{
    ec_project_state_.screenParam.al_co2_max = n;
    setModified(true);
}

void EcProject::setScreenParamAlH2oMin(double n)
{
    ec_project_state_.screenParam.al_h2o_min = n;
    setModified(true);
}

void EcProject::setScreenParamAlH2oMax(double n)
{
    ec_project_state_.screenParam.al_h2o_max = n;
    setModified(true);
}

void EcProject::setScreenParamAlCh4Min(double n)
{
    ec_project_state_.screenParam.al_ch4_min = n;
    setModified(true);
}

void EcProject::setScreenParamAlCh4Max(double n)
{
    ec_project_state_.screenParam.al_ch4_max = n;
    setModified(true);
}

void EcProject::setScreenParamAlGas4Min(double n)
{
    ec_project_state_.screenParam.al_other_min = n;
    setModified(true);
}

void EcProject::setScreenParamAlGas4Max(double n)
{
    ec_project_state_.screenParam.al_other_max = n;
    setModified(true);
}

void EcProject::setScreenParamSkHfSkmin(double n)
{
    ec_project_state_.screenParam.sk_hf_skmin = n;
    setModified(true);
}

void EcProject::setScreenParamSkHfSkmax(double n)
{
    ec_project_state_.screenParam.sk_hf_skmax = n;
    setModified(true);
}

void EcProject::setScreenParamSkSfSkmin(double n)
{
    ec_project_state_.screenParam.sk_sf_skmin = n;
    setModified(true);
}

void EcProject::setScreenParamSkSfSkmax(double n)
{
    ec_project_state_.screenParam.sk_sf_skmax = n;
    setModified(true);
}

void EcProject::setScreenParamSkHfKumin(double n)
{
    ec_project_state_.screenParam.sk_hf_kumin = n;
    setModified(true);
}

void EcProject::setScreenParamSkHfKumax(double n)
{
    ec_project_state_.screenParam.sk_hf_kumax = n;
    setModified(true);
}

void EcProject::setScreenParamSkSfKumin(double n)
{
    ec_project_state_.screenParam.sk_sf_kumin = n;
    setModified(true);
}

void EcProject::setScreenParamSkSfKumax(double n)
{
    ec_project_state_.screenParam.sk_sf_kumax = n;
    setModified(true);
}

void EcProject::setScreenParamDsHfUV(double n)
{
    ec_project_state_.screenParam.ds_hf_uv = n;
    setModified(true);
}

void EcProject::setScreenParamDsHfW(double n)
{
    ec_project_state_.screenParam.ds_hf_w = n;
    setModified(true);
}

void EcProject::setScreenParamDsHfT(double n)
{
    ec_project_state_.screenParam.ds_hf_t = n;
    setModified(true);
}

void EcProject::setScreenParamDsHfCo2(double n)
{
    ec_project_state_.screenParam.ds_hf_co2 = n;
    setModified(true);
}

void EcProject::setScreenParamDsHfH2o(double n)
{
    ec_project_state_.screenParam.ds_hf_h2o = n;
    setModified(true);
}

void EcProject::setScreenParamDsHfCh4(double n)
{
    ec_project_state_.screenParam.ds_hf_ch4 = n;
    setModified(true);
}

void EcProject::setScreenParamDsHfGas4(double n)
{
    ec_project_state_.screenParam.ds_hf_other = n;
    setModified(true);
}

void EcProject::setScreenParamDsHfVar(double n)
{
    ec_project_state_.screenParam.ds_hf_var = n;
    setModified(true);
}

void EcProject::setScreenParamDsSfUV(double n)
{
    ec_project_state_.screenParam.ds_sf_uv = n;
    setModified(true);
}

void EcProject::setScreenParamDsSfW(double n)
{
    ec_project_state_.screenParam.ds_sf_w = n;
    setModified(true);
}

void EcProject::setScreenParamDespikeVm(int n)
{
    ec_project_state_.screenParam.despike_vm = n;
    setModified(true);
}

void EcProject::setScreenParamDsSfT(double n)
{
    ec_project_state_.screenParam.ds_sf_t = n;
    setModified(true);
}

void EcProject::setScreenParamDsSfCo2(double n)
{
    ec_project_state_.screenParam.ds_sf_co2 = n;
    setModified(true);
}

void EcProject::setScreenParamDsSfH2o(double n)
{
    ec_project_state_.screenParam.ds_sf_h2o = n;
    setModified(true);
}

void EcProject::setScreenParamDsSfCh4(double n)
{
    ec_project_state_.screenParam.ds_sf_ch4 = n;
    setModified(true);
}

void EcProject::setScreenParamDsSfGas4(double n)
{
    ec_project_state_.screenParam.ds_sf_other = n;
    setModified(true);
}

void EcProject::setScreenParamDsSfVar(double n)
{
    ec_project_state_.screenParam.ds_sf_var = n;
    setModified(true);
}

void EcProject::setScreenParamTlHfLim(double n)
{
    ec_project_state_.screenParam.tl_hf_lim = n;
    setModified(true);
}

void EcProject::setScreenParamTlSfLim(double n)
{
    ec_project_state_.screenParam.tl_sf_lim = n;
    setModified(true);
}

void EcProject::setScreenParamTlDefCo2(double n)
{
    ec_project_state_.screenParam.tl_def_co2 = n;
    setModified(true);
}

void EcProject::setScreenParamTlDefH2o(double n)
{
    ec_project_state_.screenParam.tl_def_h2o = n;
    setModified(true);
}

void EcProject::setScreenParamTlDefCh4(double n)
{
    ec_project_state_.screenParam.tl_def_ch4 = n;
    setModified(true);
}

void EcProject::setScreenParamTlDefGas4(double n)
{
    ec_project_state_.screenParam.tl_def_other = n;
    setModified(true);
}

void EcProject::setScreenParamAaMin(double n)
{
    ec_project_state_.screenParam.aa_min = n;
    setModified(true);
}

void EcProject::setScreenParamAaMax(double n)
{
    ec_project_state_.screenParam.aa_max = n;
    setModified(true);
}

void EcProject::setScreenParamAaLim(double n)
{
    ec_project_state_.screenParam.aa_lim = n;
    setModified(true);
}

void EcProject::setScreenParamNsHfLim(double n)
{
    ec_project_state_.screenParam.ns_hf_lim = n;
    setModified(true);
}

void EcProject::setGeneralStartDate(const QString &d)
{
    ec_project_state_.projectGeneral.start_date = d;
    setModified(true);
}

void EcProject::setGeneralEndDate(const QString &d)
{
    ec_project_state_.projectGeneral.end_date = d;
    setModified(true);
}

void EcProject::setGeneralStartTime(const QString &t)
{
    ec_project_state_.projectGeneral.start_time = t;
    setModified(true);
}

void EcProject::setGeneralEndTime(const QString &t)
{
    ec_project_state_.projectGeneral.end_time = t;
    setModified(true);
}

void EcProject::setGeneralHfMethod(int n)
{
    ec_project_state_.projectGeneral.hf_meth = n;
    setModified(true);
    emit updateInfo();
}

void EcProject::setGeneralLfMethod(int n)
{
    ec_project_state_.projectGeneral.lf_meth = n;
    setModified(true);
}

void EcProject::setScreenBuCorr(int n)
{
    ec_project_state_.screenSetting.bu_corr = n;
    setModified(true);
}

void EcProject::setScreenBuMulti(int n)
{
    ec_project_state_.screenSetting.bu_multi = n;
    setModified(true);
}

void EcProject::setScreenLDayBotGain(double d)
{
    ec_project_state_.screenSetting.l_day_bot_gain = d;
    setModified(true);
}

void EcProject::setScreenLDayBotOffset(double d)
{
    ec_project_state_.screenSetting.l_day_bot_offset = d;
    setModified(true);
}

void EcProject::setScreenLDayTopGain(double d)
{
    ec_project_state_.screenSetting.l_day_top_gain = d;
    setModified(true);
}

void EcProject::setScreenLDayTopOffset(double d)
{
    ec_project_state_.screenSetting.l_day_top_offset = d;
    setModified(true);
}

void EcProject::setScreenLDaySparGain(double d)
{
    ec_project_state_.screenSetting.l_day_spar_gain = d;
    setModified(true);
}

void EcProject::setScreenLDaySparOffset(double d)
{
    ec_project_state_.screenSetting.l_day_spar_offset = d;
    setModified(true);
}

void EcProject::setScreenLNightBotGain(double d)
{
    ec_project_state_.screenSetting.l_night_bot_gain = d;
    setModified(true);
}

void EcProject::setScreenLNightBotOffset(double d)
{
    ec_project_state_.screenSetting.l_night_bot_offset = d;
    setModified(true);
}

void EcProject::setScreenLNightTopGain(double d)
{
    ec_project_state_.screenSetting.l_night_top_gain = d;
    setModified(true);
}

void EcProject::setScreenLNightTopOffset(double d)
{
    ec_project_state_.screenSetting.l_night_top_offset = d;
    setModified(true);
}

void EcProject::setScreenLNightSparGain(double d)
{
    ec_project_state_.screenSetting.l_night_spar_gain = d;
    setModified(true);
}

void EcProject::setScreenLNightSparOffset(double d)
{
    ec_project_state_.screenSetting.l_night_spar_offset = d;
    setModified(true);
}

void EcProject::setScreenMDayBot1(double d)
{
    ec_project_state_.screenSetting.m_day_bot1 = d;
    setModified(true);
}

void EcProject::setScreenMDayBot2(double d)
{
    ec_project_state_.screenSetting.m_day_bot2 = d;
    setModified(true);
}

void EcProject::setScreenMDayBot3(double d)
{
    ec_project_state_.screenSetting.m_day_bot3 = d;
    setModified(true);
}

void EcProject::setScreenMDayBot4(double d)
{
    ec_project_state_.screenSetting.m_day_bot4 = d;
    setModified(true);
}

void EcProject::setScreenMDayTop1(double d)
{
    ec_project_state_.screenSetting.m_day_top1 = d;
    setModified(true);
}

void EcProject::setScreenMDayTop2(double d)
{
    ec_project_state_.screenSetting.m_day_top2 = d;
    setModified(true);
}

void EcProject::setScreenMDayTop3(double d)
{
    ec_project_state_.screenSetting.m_day_top3 = d;
    setModified(true);
}

void EcProject::setScreenMDayTop4(double d)
{
    ec_project_state_.screenSetting.m_day_top4 = d;
    setModified(true);
}

void EcProject::setScreenMDaySpar1(double d)
{
    ec_project_state_.screenSetting.m_day_spar1 = d;
    setModified(true);
}

void EcProject::setScreenMDaySpar2(double d)
{
    ec_project_state_.screenSetting.m_day_spar2 = d;
    setModified(true);
}

void EcProject::setScreenMDaySpar3(double d)
{
    ec_project_state_.screenSetting.m_day_spar3 = d;
    setModified(true);
}

void EcProject::setScreenMDaySpar4(double d)
{
    ec_project_state_.screenSetting.m_day_spar4 = d;
    setModified(true);
}

void EcProject::setScreenMNightBot1(double d)
{
    ec_project_state_.screenSetting.m_night_bot1 = d;
    setModified(true);
}

void EcProject::setScreenMNightBot2(double d)
{
    ec_project_state_.screenSetting.m_night_bot2 = d;
    setModified(true);
}

void EcProject::setScreenMNightBot3(double d)
{
    ec_project_state_.screenSetting.m_night_bot3 = d;
    setModified(true);
}

void EcProject::setScreenMNightBot4(double d)
{
    ec_project_state_.screenSetting.m_night_bot4 = d;
    setModified(true);
}

void EcProject::setScreenMNightTop1(double d)
{
    ec_project_state_.screenSetting.m_night_top1 = d;
    setModified(true);
}

void EcProject::setScreenMNightTop2(double d)
{
    ec_project_state_.screenSetting.m_night_top2 = d;
    setModified(true);
}

void EcProject::setScreenMNightTop3(double d)
{
    ec_project_state_.screenSetting.m_night_top3 = d;
    setModified(true);
}

void EcProject::setScreenMNightTop4(double d)
{
    ec_project_state_.screenSetting.m_night_top4 = d;
    setModified(true);
}

void EcProject::setScreenMNightSpar1(double d)
{
    ec_project_state_.screenSetting.m_night_spar1 = d;
    setModified(true);
}

void EcProject::setScreenMNightSpar2(double d)
{
    ec_project_state_.screenSetting.m_night_spar2 = d;
    setModified(true);
}

void EcProject::setScreenMNightSpar3(double d)
{
    ec_project_state_.screenSetting.m_night_spar3 = d;
    setModified(true);
}

void EcProject::setScreenMNightSpar4(double d)
{
    ec_project_state_.screenSetting.m_night_spar4 = d;
    setModified(true);
}

void EcProject::setScreenlOutDetails(int n)
{
    ec_project_state_.screenSetting.out_details = n;
    setModified(true);
}

void EcProject::setScreenlPowerOfTwo(int n)
{
    ec_project_state_.screenSetting.power_of_two = n;
    setModified(true);
}

void EcProject::setGeneralRunMode(Defs::CurrRunMode mode)
{
    ec_project_state_.projectGeneral.run_mode = mode;
    setModified(true);
}

void EcProject::setGeneralRunFcc(bool yes)
{
    ec_project_state_.projectGeneral.run_fcc = yes;
    setModified(true);
}

void EcProject::setGeneralUseAltMdFile(bool b)
{
    ec_project_state_.projectGeneral.use_alt_md_file = b;
    setModified(true);
    emit updateInfo();
}

void EcProject::setGeneralMdFilepath(const QString &p)
{
    ec_project_state_.projectGeneral.md_file = p;
    setModified(true);
    emit updateInfo();
}

void EcProject::setGeneralUseTimelineFile(bool b)
{
    ec_project_state_.projectGeneral.use_tlfile = b;
    setModified(true);
    emit updateInfo();
}

void EcProject::setGeneralTimelineFilepath(const QString &p)
{
    ec_project_state_.projectGeneral.timeline_file = p;
    setModified(true);
    emit updateInfo();
}

void EcProject::setGeneralFileType(Defs::RawFileType type)
{
    ec_project_state_.projectGeneral.file_type = type;
    setModified(true);
    emit updateInfo();
}

void EcProject::setGeneralBinaryHNLines(int n)
{
    ec_project_state_.projectGeneral.binary_hnlines = n;
    setModified(true);
    emit updateInfo();
}

void EcProject::setGeneralBinaryEol(int n)
{
    ec_project_state_.projectGeneral.binary_eol = n;
    setModified(true);
    emit updateInfo();
}

void EcProject::setGeneralBinaryNBytes(int n)
{
    ec_project_state_.projectGeneral.binary_nbytes = n;
    setModified(true);
    emit updateInfo();
}

void EcProject::setGeneralBinaryLittleEnd(int n)
{
    ec_project_state_.projectGeneral.binary_little_end = n;
    setModified(true);
    emit updateInfo();
}

void EcProject::setGeneralTitle(const QString &s)
{
    ec_project_state_.projectGeneral.project_title = s;
    setModified(true);
}

void EcProject::setGeneralFileName(const QString &n)
{
    ec_project_state_.projectGeneral.file_name = n;
    setModified(true);
}

void EcProject::setGeneralId(const QString &id)
{
    ec_project_state_.projectGeneral.project_id = id;
    setModified(true);
    emit updateInfo();
}

void EcProject::setGeneralFilePrototype(const QString &f)
{
    ec_project_state_.projectGeneral.file_prototype = f;
    setModified(true);
    emit updateInfo();
}

void EcProject::setGeneralWplMeth(int n)
{
    ec_project_state_.projectGeneral.wpl_meth = n;
    setModified(true);
}

void EcProject::setGeneralColMasterSonic(const QString &s)
{
    ec_project_state_.projectGeneral.master_sonic = s;
    setModified(true);
    emit updateInfo();
}

void EcProject::setGeneralColCo2(int n)
{
    ec_project_state_.projectGeneral.col_co2 = n;
    setModified(true);
}

void EcProject::setGeneralColH2o(int n)
{
    ec_project_state_.projectGeneral.col_h2o = n;
    setModified(true);
}

void EcProject::setGeneralColCh4(int n)
{
    ec_project_state_.projectGeneral.col_ch4 = n;
    setModified(true);
}

void EcProject::setGeneralColGas4(int n)
{
    ec_project_state_.projectGeneral.col_gas4 = n;
    setModified(true);
}

void EcProject::setGeneralColIntTc(int n)
{
    ec_project_state_.projectGeneral.col_int_t_c = n;
    setModified(true);
}

void EcProject::setGeneralColIntT1(int n)
{
    ec_project_state_.projectGeneral.col_int_t_1 = n;
    setModified(true);
}

void EcProject::setGeneralColIntT2(int n)
{
    ec_project_state_.projectGeneral.col_int_t_2 = n;
    setModified(true);
}

void EcProject::setGeneralColIntP(int n)
{
    ec_project_state_.projectGeneral.col_int_p = n;
    setModified(true);
}

void EcProject::setGeneralColAirT(int n)
{
    ec_project_state_.projectGeneral.col_air_t = n;
    setModified(true);
}

void EcProject::setGeneralColAirP(int n)
{
    ec_project_state_.projectGeneral.col_air_p = n;
    setModified(true);
}

void EcProject::setGeneralColDiag75(int n)
{
    ec_project_state_.projectGeneral.col_diag_75 = n;
    setModified(true);
}

void EcProject::setGeneralColDiag72(int n)
{
    ec_project_state_.projectGeneral.col_diag_72 = n;
    setModified(true);
}

void EcProject::setGeneralColDiag77(int n)
{
    ec_project_state_.projectGeneral.col_diag_77 = n;
    setModified(true);
}

void EcProject::setGeneralColDiagAnem(int n)
{
    ec_project_state_.projectGeneral.col_diag_anem = n;
    setModified(true);
}

void EcProject::setGeneralColGasMw(double n)
{
    ec_project_state_.projectGeneral.gas_mw = n;
    setModified(true);
}

void EcProject::setGeneralColTs(int n)
{
    ec_project_state_.projectGeneral.col_ts = n;
    setModified(true);
    emit updateInfo();
}

void EcProject::setGeneralColGasDiff(double n)
{
    ec_project_state_.projectGeneral.gas_diff = n;
    setModified(true);
}

void EcProject::setGeneralQcMeth(int n)
{
    ec_project_state_.projectGeneral.qcflag_meth = n;
    setModified(true);
}

void EcProject::setGeneralUseBiomet(int n)
{
    ec_project_state_.projectGeneral.use_biomet = n;
    setModified(true);
    emit updateInfo();
}

void EcProject::setGeneralBiomFile(const QString &p)
{
    ec_project_state_.projectGeneral.biom_file = p;
    setModified(true);
    emit updateInfo();
}

void EcProject::setGeneralBiomDir(const QString &p)
{
    ec_project_state_.projectGeneral.biom_dir = p;
    setModified(true);
    emit updateInfo();
}

void EcProject::setGeneralBiomRecurse(int n)
{
    ec_project_state_.projectGeneral.biom_recurse = n;
    setModified(true);
}

void EcProject::setGeneralBiomExt(const QString &p)
{
    ec_project_state_.projectGeneral.biom_ext = p;
    setModified(true);
}

void EcProject::setScreenAvrgLen(int n)
{
    ec_project_state_.screenSetting.avrg_len = n;
    setModified(true);
}

void EcProject::setGeneralMakeDataset(bool b)
{
    ec_project_state_.projectGeneral.make_dataset = b;
    setModified(true);
}

void EcProject::setPlanarFitStartDate(const QString& date)
{
    ec_project_state_.screenTilt.start_date = date;
    setModified(true);
}

void EcProject::setPlanarFitEndDate(const QString& date)
{
    ec_project_state_.screenTilt.end_date = date;
    setModified(true);
}

void EcProject::setPlanarFitStartTime(const QString& time)
{
    ec_project_state_.screenTilt.start_time = time;
    setModified(true);
}

void EcProject::setPlanarFitEndTime(const QString& time)
{
    ec_project_state_.screenTilt.end_time = time;
    setModified(true);
}

void EcProject::setPlanarFitMode(int i)
{
    ec_project_state_.screenTilt.mode = i;
    setModified(true);
    emit updateInfo();
}

void EcProject::setPlanarFitNorthOffset(double d)
{
    ec_project_state_.screenTilt.north_offset = d;
    setModified(true);
    emit updateInfo();
}

void EcProject::setPlanarFitItemPerSector(int i)
{
    ec_project_state_.screenTilt.min_num_per_sec = i;
    setModified(true);
    emit updateInfo();
}

void EcProject::setPlanarFitWmax(double d)
{
    ec_project_state_.screenTilt.w_max = d;
    setModified(true);
    emit updateInfo();
}

void EcProject::setPlanarFitUmin(double d)
{
    ec_project_state_.screenTilt.u_min = d;
    setModified(true);
    emit updateInfo();
}

void EcProject::setPlanarFitFile(const QString &p)
{
    ec_project_state_.screenTilt.file = p;
    setModified(true);
    emit updateInfo();
}

void EcProject::setPlanarFitPolicy(int i)
{
    ec_project_state_.screenTilt.fix_policy = i;
    setModified(true);
}

void EcProject::setPlanarFitSubset(int n)
{
    ec_project_state_.screenTilt.subset = n;
    setModified(true);
}

void EcProject::setPlanarFitAssessmentOnly(int n)
{
    ec_project_state_.screenTilt.assessment_only = n;
    setModified(true);
    emit updateInfo();
}

void EcProject::setSpectraStartDate(const QString& date)
{
    ec_project_state_.spectraSettings.start_sa_date = date;
    setModified(true);
}

void EcProject::setSpectraEndDate(const QString& date)
{
    ec_project_state_.spectraSettings.end_sa_date = date;
    setModified(true);
}

void EcProject::setSpectraStartTime(const QString& time)
{
    ec_project_state_.spectraSettings.start_sa_time = time;
    setModified(true);
}

void EcProject::setSpectraEndTime(const QString& time)
{
    ec_project_state_.spectraSettings.end_sa_time = time;
    setModified(true);
}

void EcProject::setSpectraMode(int i)
{
    ec_project_state_.spectraSettings.sa_mode = i;
    setModified(true);
    emit updateInfo();
}

void EcProject::setSpectraFile(const QString &p)
{
    ec_project_state_.spectraSettings.sa_file = p;
    setModified(true);
    emit updateInfo();
}

void EcProject::setSpectraFluxRunMode(int n)
{
    ec_project_state_.spectraSettings.flux_run_mode = n;
    setModified(true);
    emit updateInfo();
}

void EcProject::setSpectraAutomaticConfig(int n)
{
    ec_project_state_.spectraSettings.automatic_spectra_config = n;
    setModified(true);
}

void EcProject::setSpectraUseVmFlags(int n)
{
    ec_project_state_.spectraSettings.use_vm_flags = n;
    setModified(true);
}

void EcProject::setSpectraUseFokenLow(int n)
{
    ec_project_state_.spectraSettings.use_foken_low = n;
    setModified(true);
}


void EcProject::setSpectraUseFokenMid(int n)
{
    ec_project_state_.spectraSettings.use_foken_mid = n;
    setModified(true);
}

void EcProject::setTimelagOptStartDate(const QString& date)
{
    ec_project_state_.timelagOpt.start_date = date;
    setModified(true);
}

void EcProject::setTimelagOptEndDate(const QString& date)
{
    ec_project_state_.timelagOpt.end_date = date;
    setModified(true);
}

void EcProject::setTimelagOptStartTime(const QString& time)
{
    ec_project_state_.timelagOpt.start_time = time;
    setModified(true);
}

void EcProject::setTimelagOptEndTime(const QString& time)
{
    ec_project_state_.timelagOpt.end_time = time;
    setModified(true);
}

void EcProject::setTimelagOptMode(int i)
{
    ec_project_state_.timelagOpt.mode = i;
    setModified(true);
}

void EcProject::setTimelagOptSubset(int i)
{
    ec_project_state_.timelagOpt.subset = i;
    setModified(true);
}

void EcProject::setTimelagOptFile(const QString &p)
{
    ec_project_state_.timelagOpt.file = p;
    setModified(true);
    emit updateInfo();
}

void EcProject::setTimelagOptH2oNClass(int n)
{
    ec_project_state_.timelagOpt.to_h2o_nclass = n;
    setModified(true);
}

void EcProject::setTimelagOptCo2MinFlux(double d)
{
    ec_project_state_.timelagOpt.co2_min_flux = d;
    setModified(true);
}

void EcProject::setTimelagOptCh4MinFlux(double d)
{
    ec_project_state_.timelagOpt.ch4_min_flux = d;
    setModified(true);
}

void EcProject::setTimelagOptGas4MinFlux(double d)
{
    ec_project_state_.timelagOpt.gas4_min_flux = d;
    setModified(true);
}

void EcProject::setTimelagOptLeMinFlux(double d)
{
    ec_project_state_.timelagOpt.le_min_flux = d;
    setModified(true);
}

void EcProject::setTimelagOptPgRange(double d)
{
    ec_project_state_.timelagOpt.pg_range = d;
    setModified(true);
}

void EcProject::setTimelagOptCo2MinLag(double d)
{
    ec_project_state_.timelagOpt.co2_min_lag = d;
    setModified(true);
}

void EcProject::setTimelagOptCo2MaxLag(double d)
{
    ec_project_state_.timelagOpt.co2_max_lag = d;
    setModified(true);
}

void EcProject::setTimelagOptH2oMinLag(double d)
{
    ec_project_state_.timelagOpt.h2o_min_lag = d;
    setModified(true);
}

void EcProject::setTimelagOptH2oMaxLag(double d)
{
    ec_project_state_.timelagOpt.h2o_max_lag = d;
    setModified(true);
}

void EcProject::setTimelagOptCh4MinLag(double d)
{
    ec_project_state_.timelagOpt.ch4_min_lag = d;
    setModified(true);
}

void EcProject::setTimelagOptCh4MaxLag(double d)
{
    ec_project_state_.timelagOpt.ch4_max_lag = d;
    setModified(true);
}

void EcProject::setTimelagOptGas4MinLag(double d)
{
    ec_project_state_.timelagOpt.gas4_min_lag = d;
    setModified(true);
}

void EcProject::setTimelagOptGas4MaxLag(double d)
{
    ec_project_state_.timelagOpt.gas4_max_lag = d;
    setModified(true);
}

void EcProject::setTimelagAssessmentOnly(int n)
{
    ec_project_state_.timelagOpt.assessment_only = n;
    setModified(true);
    emit updateInfo();
}

void EcProject::setPwbCo2MinLag(double d)
{
    ec_project_state_.pwbTimelag.co2_min_lag = d;
    setModified(true);
}

void EcProject::setPwbCo2MaxLag(double d)
{
    ec_project_state_.pwbTimelag.co2_max_lag = d;
    setModified(true);
}

void EcProject::setPwbH2oMinLag(double d)
{
    ec_project_state_.pwbTimelag.h2o_min_lag = d;
    setModified(true);
}

void EcProject::setPwbH2oMaxLag(double d)
{
    ec_project_state_.pwbTimelag.h2o_max_lag = d;
    setModified(true);
}

void EcProject::setPwbCh4MinLag(double d)
{
    ec_project_state_.pwbTimelag.ch4_min_lag = d;
    setModified(true);
}

void EcProject::setPwbCh4MaxLag(double d)
{
    ec_project_state_.pwbTimelag.ch4_max_lag = d;
    setModified(true);
}

void EcProject::setPwbGas4MinLag(double d)
{
    ec_project_state_.pwbTimelag.gas4_min_lag = d;
    setModified(true);
}

void EcProject::setPwbGas4MaxLag(double d)
{
    ec_project_state_.pwbTimelag.gas4_max_lag = d;
    setModified(true);
}

void EcProject::setPwbNBootstrap(int n)
{
    ec_project_state_.pwbTimelag.n_bootstrap = n;
    setModified(true);
}

void EcProject::setPwbBlockLength(double d)
{
    ec_project_state_.pwbTimelag.block_length_s = d;
    setModified(true);
}

void EcProject::setPwbMinValidFrac(double d)
{
    ec_project_state_.pwbTimelag.min_valid_frac = d;
    setModified(true);
}

void EcProject::setPwbHdiThresh(double d)
{
    ec_project_state_.pwbTimelag.hdi_thresh_s = d;
    setModified(true);
}

void EcProject::setPwbDevThresh(double d)
{
    ec_project_state_.pwbTimelag.dev_thresh_s = d;
    setModified(true);
}

void EcProject::setPwbHdiPrefilter(double d)
{
    ec_project_state_.pwbTimelag.hdi_prefilter_s = d;
    setModified(true);
}

void EcProject::setPwbSmoothingWidth(int n)
{
    ec_project_state_.pwbTimelag.smoothing_width = n;
    setModified(true);
}

void EcProject::setPwbRandomSeed(int n)
{
    ec_project_state_.pwbTimelag.random_seed = n;
    setModified(true);
}

void EcProject::setPwbMaxCarryH(double n)
{
    ec_project_state_.pwbTimelag.max_carry_h = n;
    setModified(true);
}

void EcProject::setRandomErrorMethod(int n)
{
    ec_project_state_.randomError.ru_method = n;
    setModified(true);
}

void EcProject::setRandomErrorItsMethod(int n)
{
    ec_project_state_.randomError.its_method = n;
    setModified(true);
}

void EcProject::setRandomErrorItsTlagMax(double d)
{
    ec_project_state_.randomError.its_tlag_max = d;
    setModified(true);
}

void EcProject::setRandomErrorItsSecFactor(double d)
{
    ec_project_state_.randomError.its_sec_factor = d;
    setModified(true);
}

QList<AngleItem>* EcProject::planarFitAngles()
{
    return &ec_project_state_.screenTilt.angles;
}

bool EcProject::hasPlanarFitFullAngle()
{
    QList<AngleItem>* angles = planarFitAngles();

    double angleSum = 0.0;
    for (int n = 0; n < angles->count(); ++n)
    {
        angleSum += angles->at(n).angle_;
    }

    return (angleSum == 360.0);
}

void EcProject::addPlanarFitAngle(const AngleItem& angle)
{
    ec_project_state_.screenTilt.angles.append(angle);
}

int EcProject::countPlanarFitAngles(const QStringList& list)
{
    int i = 0;
    for (const auto &s : list)
    {
        if (s.contains(QStringLiteral("width")))
        {
            ++i;
        }
    }
    return i;
}

void EcProject::setBiomParamColAirT(int n)
{
    ec_project_state_.biomParam.col_ta = n;
    setModified(true);
}

void EcProject::setBiomParamColAirP(int n)
{
    ec_project_state_.biomParam.col_pa = n;
    setModified(true);
}

void EcProject::setBiomParamColRh(int n)
{
    ec_project_state_.biomParam.col_rh = n;
    setModified(true);
}

void EcProject::setBiomParamColRg(int n)
{
    ec_project_state_.biomParam.col_rg = n;
    setModified(true);
}

void EcProject::setBiomParamColLwin(int n)
{
    ec_project_state_.biomParam.col_lwin = n;
    setModified(true);
}

void EcProject::setBiomParamColPpfd(int n)
{
    ec_project_state_.biomParam.col_ppfd = n;
    setModified(true);
}

bool EcProject::isEngineStep2Needed()
{
    if (timelagAssessmentOnly() || planarFitAssessmentOnly())
    {
        return false;
    }

    bool test = false;

    switch (generalHfMethod())
    {
        // case no HF spectral corrections or Moncrieff or Masssmann
        case 0:
        case 1:
        case 5:
            break;
        // case Horst, Ibrom, Fratini
        case 2:
        case 3:
        case 4:
            test = true;
            break;
        default:
            break;
    }

    // in smartflux mode, this output is always disabled
    if (generalOutMeanCosp())
        test = true;

    return test;
}

bool EcProject::isGoodRawFilePrototype(const QString& s)
{
    bool test = !s.isEmpty()
                && s.contains(QStringLiteral("yy"))
                && s.contains(QStringLiteral("dd"))
                && s.contains(QStringLiteral("HH"))
                && s.contains(QStringLiteral("MM"))
                && s.contains(QStringLiteral("."));
    return test;
}
