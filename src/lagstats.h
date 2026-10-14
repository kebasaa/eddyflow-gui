/***************************************************************************
  lagstats.h
  ----------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#ifndef LAGSTATS_H
#define LAGSTATS_H

#include <QString>
#include <QVector>

//> What the time-lag preliminary assessment does with the engine's table of
//> detected lags (<id>_tlag_preassess_<ts>.csv): read it, bin it, and suggest
//> a nominal lag and a window from it. Free of widgets, so it can be checked
//> on its own.
namespace LagStats {

//> One period and gas of the table.
struct Row
{
    QString date;
    QString time;
    QString gas;            //< the engine's label for the gas, e.g. co2, cos
    int record = 0;         //< 1-based gas record, as gas_<N>_* in the project
    QString method;         //< maxcov or pwb
    double lag = 0.0;       //< the detected lag [s]
    double windowMin = 0.0;
    double windowMax = 0.0;
    bool atEdge = false;    //< at either end of the search window
    bool hasFlux = false;
    double flux = 0.0;
    bool fluxOk = true;     //< cleared the optimiser's flux threshold
};

//> The table, or an empty list and `error` set.
QVector<Row> readTable(const QString& path, QString* error = nullptr);

double median(QVector<double> values);
//> Median absolute deviation from the median, unscaled.
double mad(const QVector<double>& values);

struct Suggestion
{
    bool ok = false;        //< enough usable lags to say anything
    int used = 0;
    double nominal = 0.0;
    double minimum = 0.0;
    double maximum = 0.0;
};

//> Fewest usable lags a suggestion is made from.
constexpr int MinLagsForSuggestion = 5;

//> The time-lag optimiser's own rule for a passive gas (optimize_timelags.f90):
//> the median, and a window of pgRange robust standard deviations either side
//> of it, the deviation being MAD / 0.6745 with the MAD floored at 0.1 s. A
//> closed path cannot lead the anemometer, so its minimum stops at zero.
Suggestion suggest(const QVector<double>& lags, double pgRange, bool closedPath);

struct Histogram
{
    double start = 0.0;     //< left edge of the first bin [s]
    double width = 0.1;     //< bin width [s]
    QVector<int> counts;    //< lags off the window edges
    QVector<int> edgeCounts;//< lags at a window edge
    int maxStack() const;
};

//> Bins aligned to multiples of `width`, spanning every lag given.
Histogram bin(const QVector<double>& lags, const QVector<bool>& atEdge, double width);

//> The share of the half-hours to assess, in percent: 10 % for a month of
//> data or less, 1 % for a year or more, and in between on a log scale.
double defaultSamplePercent(qint64 periods);

//> How many half-hours a share of `periods` comes to - never fewer than
//> `minimum`, never more than there are.
qint64 sampledCount(qint64 periods, double percent, qint64 minimum = 10);

//> Default search window for a gas with no stated one: +/-5 s on an open
//> path; on a closed path from 0 s to three tube transit times plus 10 s,
//> the transit time from the tube's metadata (length [cm], inner diameter
//> [mm], flow [l/min]) as set_timelags.f90 forms it, or 0 to 30 s if the
//> tube is not described.
void defaultSearchWindow(bool closedPath, double tubeLengthCm, double tubeDiameterMm,
                         double flowLpm, double* min, double* max);

}  // namespace LagStats

#endif  // LAGSTATS_H
