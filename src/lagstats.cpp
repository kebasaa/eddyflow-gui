/***************************************************************************
  lagstats.cpp
  ------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#include "lagstats.h"

#include <QFile>
#include <QHash>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <cmath>

namespace LagStats {

QVector<Row> readTable(const QString& path, QString* error)
{
    QVector<Row> rows;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        if (error) { *error = file.errorString(); }
        return rows;
    }

    QTextStream in(&file);
    const QStringList header = in.readLine().trimmed().split(QLatin1Char(','));
    QHash<QString, int> col;
    for (int i = 0; i < header.size(); ++i) { col.insert(header.at(i).trimmed(), i); }
    for (const auto& need : { "gas", "gas_record", "detected_lag_s", "at_edge" })
    {
        if (!col.contains(QLatin1String(need)))
        {
            if (error) { *error = QStringLiteral("no column %1 in %2").arg(QLatin1String(need), path); }
            return rows;
        }
    }
    auto field = [&col](const QStringList& f, const char* name) {
        return f.value(col.value(QLatin1String(name), -1)).trimmed();
    };

    while (!in.atEnd())
    {
        const QString line = in.readLine();
        if (line.trimmed().isEmpty()) { continue; }
        const QStringList f = line.split(QLatin1Char(','));
        bool ok = false;
        Row r;
        r.lag = field(f, "detected_lag_s").toDouble(&ok);
        if (!ok) { continue; }
        r.date = field(f, "date");
        r.time = field(f, "time");
        r.gas = field(f, "gas");
        r.record = field(f, "gas_record").toInt();
        r.method = field(f, "method");
        r.windowMin = field(f, "window_min_s").toDouble();
        r.windowMax = field(f, "window_max_s").toDouble();
        r.atEdge = field(f, "at_edge") == QLatin1String("1");
        const QString flux = field(f, "flux");
        r.flux = flux.toDouble(&r.hasFlux);
        r.fluxOk = field(f, "flux_ok") != QLatin1String("0");
        rows.append(r);
    }
    if (error) { error->clear(); }
    return rows;
}

double median(QVector<double> values)
{
    if (values.isEmpty()) { return 0.0; }
    std::sort(values.begin(), values.end());
    const auto n = values.size();
    return (n % 2) ? values.at(n / 2) : 0.5 * (values.at(n / 2 - 1) + values.at(n / 2));
}

double mad(const QVector<double>& values)
{
    const double m = median(values);
    QVector<double> dev;
    dev.reserve(values.size());
    for (double v : values) { dev.append(std::abs(v - m)); }
    return median(dev);
}

Suggestion suggest(const QVector<double>& lags, double pgRange, bool closedPath)
{
    Suggestion s;
    s.used = lags.size();
    if (lags.size() < MinLagsForSuggestion) { return s; }
    const double half = pgRange * std::max(mad(lags), 0.1) / 0.6745;
    s.nominal = median(lags);
    s.minimum = s.nominal - half;
    s.maximum = s.nominal + half;
    if (closedPath) { s.minimum = std::max(0.0, s.minimum); }
    s.ok = true;
    return s;
}

PeakSuggestion suggestFromPeak(const QVector<double>& lags, double pgRange, bool closedPath,
                               double windowMin, double windowMax)
{
    PeakSuggestion p;
    p.total = lags.size();
    if (lags.size() < MinLagsForSuggestion) { return p; }

    constexpr double bandwidth = 0.3;   // s
    constexpr double gridStep = 0.05;   // s
    constexpr double seedHalfWidth = 2.0;
    constexpr double minHalfWidth = 0.5;
    constexpr double clearFactor = 3.0;

    const auto [lo, hi] = std::minmax_element(lags.begin(), lags.end());
    double from = *lo, to = *hi;
    if (windowMax > windowMin)
    {
        from = std::min(from, windowMin);
        to = std::max(to, windowMax);
    }

    // The mode: highest point of a Gaussian kernel density. Of equal
    // heights, the one nearest the median of all lags.
    const double allMedian = median(lags);
    double bestDensity = -1.0;
    for (double x = from; x <= to + 1e-9; x += gridStep)
    {
        double density = 0.0;
        for (double l : lags)
        {
            const double z = (x - l) / bandwidth;
            if (std::abs(z) < 5.0) { density += std::exp(-0.5 * z * z); }
        }
        if (density > bestDensity + 1e-12
            || (std::abs(density - bestDensity) <= 1e-12
                && std::abs(x - allMedian) < std::abs(p.peak - allMedian)))
        {
            bestDensity = density;
            p.peak = x;
        }
    }

    // The lags around it, narrowed until the set settles.
    auto within = [&lags](double centre, double halfWidth) {
        QVector<double> out;
        for (double l : lags)
        {
            if (std::abs(l - centre) <= halfWidth) { out.append(l); }
        }
        return out;
    };
    QVector<double> inPeak = within(p.peak, seedHalfWidth);
    for (int round = 0; round < 10 && inPeak.size() >= MinLagsForSuggestion; ++round)
    {
        const double halfWidth = std::max(minHalfWidth, 3.0 * mad(inPeak) / 0.6745);
        QVector<double> next = within(median(inPeak), halfWidth);
        if (next == inPeak) { break; }
        inPeak = next;
    }

    static_cast<Suggestion&>(p) = suggest(inPeak, pgRange, closedPath);
    p.total = lags.size();
    p.share = lags.isEmpty() ? 0.0 : static_cast<double>(inPeak.size()) / lags.size();
    if (!p.ok) { return p; }

    // Clear when the peak holds well more than an even spread would put in
    // its width.
    const double span = std::max(to - from, gridStep);
    const double width = std::max(p.maximum - p.minimum, 2.0 * minHalfWidth);
    const double evenSpread = lags.size() * std::min(1.0, width / span);
    p.clear = p.used >= MinLagsForSuggestion && p.used >= clearFactor * evenSpread;
    return p;
}

int Histogram::maxStack() const
{
    int m = 0;
    for (int i = 0; i < counts.size(); ++i) { m = std::max(m, counts.at(i) + edgeCounts.at(i)); }
    return m;
}

Histogram bin(const QVector<double>& lags, const QVector<bool>& atEdge, double width)
{
    Histogram h;
    h.width = width > 0.0 ? width : 0.1;
    if (lags.isEmpty()) { return h; }
    const auto [lo, hi] = std::minmax_element(lags.begin(), lags.end());
    //> Half a bin of slack, so a lag that is a whole number of samples does
    //> not fall on a bin edge and get split by rounding.
    h.start = std::floor(*lo / h.width) * h.width - 0.5 * h.width;
    const int n = static_cast<int>(std::floor((*hi - h.start) / h.width)) + 1;
    h.counts.fill(0, n);
    h.edgeCounts.fill(0, n);
    for (int i = 0; i < lags.size(); ++i)
    {
        const int k = std::clamp(static_cast<int>(std::floor((lags.at(i) - h.start) / h.width)), 0, n - 1);
        if (atEdge.value(i)) { ++h.edgeCounts[k]; } else { ++h.counts[k]; }
    }
    return h;
}

double defaultSamplePercent(qint64 periods)
{
    //> 10 % of a month, falling as a power of the size, so that the share
    //> shrinks while the number of half-hours assessed still grows slowly:
    //> about 29 for three days, 144 for a month, 290 for a year. Capped at
    //> 20 % for a small dataset, floored at 1 % from two years on.
    constexpr double month = 1440.0;
    constexpr double twoYears = 35040.0;
    constexpr double MaxDefault = 20.0;
    constexpr double MinDefault = 1.0;
    if (periods <= 0) { return MaxDefault; }
    const double exponent = std::log(10.0) / std::log(twoYears / month);
    const double pct = 10.0 * std::pow(month / static_cast<double>(periods), exponent);
    return std::round(std::clamp(pct, MinDefault, MaxDefault) * 10.0) / 10.0;
}

qint64 sampledCount(qint64 periods, double percent, qint64 minimum)
{
    if (periods <= 0) { return 0; }
    const auto n = static_cast<qint64>(std::llround(periods * percent / 100.0));
    return std::min(periods, std::max(minimum, n));
}

void defaultSearchWindow(bool closedPath, double tubeLengthCm, double tubeDiameterMm,
                         double flowLpm, double* min, double* max)
{
    if (!closedPath)
    {
        *min = -5.0;
        *max = 5.0;
        return;
    }
    *min = 0.0;
    *max = 30.0;
    if (tubeLengthCm <= 0.0 || tubeDiameterMm <= 0.0 || flowLpm <= 0.0) { return; }
    const double radius = 0.5 * tubeDiameterMm * 1.0e-3;          // m
    constexpr double pi = 3.14159265358979323846;
    const double volume = pi * radius * radius * tubeLengthCm * 1.0e-2;  // m3
    const double transit = volume / (flowLpm * 1.0e-3 / 60.0);  // s
    *max = std::ceil(3.0 * transit + 10.0);
}

}  // namespace LagStats
