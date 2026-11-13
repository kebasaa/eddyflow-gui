/***************************************************************************
  spectralassessmentfile.cpp
  --------------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#include "spectralassessmentfile.h"

#include <QFile>
#include <QObject>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

#include <algorithm>

#include "measurement_record.h"

namespace SpectralAssessmentFile {

namespace {

constexpr int kMonthRows = 12;

/// Records naming \a species, in record order.
QVector<int> recordsOfSpecies(const QVector<GasRecord>& gases,
                              const QString& species)
{
    QVector<int> found;
    for (int i = 0; i < gases.size(); ++i)
    {
        if (gases.at(i).slug.compare(species, Qt::CaseInsensitive) == 0) { found << i; }
    }
    return found;
}

/// The value of a `key=value` token in a header, or an empty string.
QString stampValue(const QStringList& words, const QString& key)
{
    const auto prefix = key + QLatin1Char('=');
    for (const auto& word : words)
    {
        if (word.startsWith(prefix)) { return word.mid(prefix.size()); }
    }
    return QString();
}

/// The record a block header belongs to, or -1.
///
/// By its `var=`/`instr=` stamp when exactly one record matches it - the name
/// is an ordinal over repeats of a species and changes when the records are
/// reordered, the stamp does not. Otherwise by the name, the way the engine
/// falls back: a bare species is its first record, NAME_k its k-th.
int recordForHeader(const QVector<GasRecord>& gases, const QStringList& words)
{
    const auto var = stampValue(words, QStringLiteral("var"));
    const auto instr = stampValue(words, QStringLiteral("instr"));
    if (!var.isEmpty() && !instr.isEmpty())
    {
        int match = -1;
        int count = 0;
        for (int i = 0; i < gases.size(); ++i)
        {
            if (gases.at(i).slug.compare(var, Qt::CaseInsensitive) != 0) { continue; }
            if (gases.at(i).instrumentId.compare(instr, Qt::CaseInsensitive) != 0) { continue; }
            match = i;
            ++count;
        }
        if (count == 1) { return match; }
    }

    QStringList nameWords;
    for (const auto& word : words)
    {
        if (word == QLatin1String("TFP")) { break; }
        nameWords << word;
    }
    const auto name = nameWords.join(QLatin1Char(' '));
    if (name.isEmpty()) { return -1; }
    const auto whole = recordsOfSpecies(gases, name);
    if (!whole.isEmpty()) { return whole.first(); }

    static const QRegularExpression numbered(QStringLiteral("^(.+)_(\\d+)$"));
    const auto m = numbered.match(name);
    if (!m.hasMatch()) { return -1; }
    const auto same = recordsOfSpecies(gases, m.captured(1));
    const auto k = m.captured(2).toInt();
    return (k >= 1 && k <= same.size()) ? same.at(k - 1) : -1;
}

/// Fn and fc from a month row - "January = 0.83927 1.24696". A multi-rate
/// file carries one pair per rate; the first is taken.
Fit monthRow(const QStringList& words)
{
    Fit fit;
    const auto eq = words.indexOf(QStringLiteral("="));
    if (eq < 0 || eq + 2 >= words.size()) { return fit; }
    bool okFn = false;
    bool okFc = false;
    const auto fn = words.at(eq + 1).toDouble(&okFn);
    const auto fc = words.at(eq + 2).toDouble(&okFc);
    if (okFn) { fit.fn = fn; }
    if (okFc) { fit.fc = fc; }
    return fit;
}

/// Whether \a i may be measured against \a gasIndex: itself, when it could
/// lend to anyone, or a tube-mate that may lend to it. The engine's
/// SpectralTubeMates counts a gas as its own tube-mate on the same terms.
bool onSameAnalyser(const QVector<GasRecord>& gases, int gasIndex, int i)
{
    if (i != gasIndex) { return MeasurementRecords::isSaSourceFor(gases, gasIndex, i); }
    const auto& gas = gases.at(gasIndex);
    return gas.slug != QLatin1String("h2o") && gas.rawColumn > 0
        && MeasurementRecords::isRealInstrument(gas.instrumentId);
}

QString number(double x)
{
    return QString::number(x, 'g', 3);
}

} // namespace

Fits readFits(const QVector<GasRecord>& gases, const QString& path)
{
    Fits fits;
    QFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) { return fits; }

    QList<QStringList> lines;
    QTextStream in(&file);
    while (!in.atEnd())
    {
        lines << in.readLine().split(QRegularExpression(QStringLiteral("\\s+")),
                                     Qt::SkipEmptyParts);
    }

    for (int i = 0; i < lines.size(); ++i)
    {
        const auto& header = lines.at(i);
        if (!header.contains(QStringLiteral("TFP"))) { continue; }
        //> The preamble names TFP too, in prose, on a line that is one word.
        if (header.size() < 3) { continue; }
        //> An RH-class block is water's: nine rows, never a month.
        if (header.contains(QStringLiteral("numerosity"))) { continue; }
        const int record = recordForHeader(gases, header);
        if (record < 0 || i + kMonthRows >= lines.size()) { continue; }
        QVector<Fit> months;
        for (int m = 1; m <= kMonthRows; ++m) { months << monthRow(lines.at(i + m)); }
        fits.insert(record, months);
        i += kMonthRows;
    }
    return fits;
}

bool fitUsable(const Fit& fit)
{
    return fit.fc != kError && fit.fc > 0.0
        && fit.fn != kError && fit.fn >= kFnMin;
}

int suggestedSource(const QVector<GasRecord>& gases, const Fits& fits,
                    int gasIndex, int month)
{
    if (gasIndex < 0 || gasIndex >= gases.size()) { return -1; }
    if (month < 0 || month >= kMonthRows) { return -1; }

    //> The best-resolved usable fit on the analyser, the gas itself included;
    //> ties go to the lower record, as in the engine.
    int ref = -1;
    double bestFn = -1.0;
    for (int i = 0; i < gases.size(); ++i)
    {
        if (!onSameAnalyser(gases, gasIndex, i)) { continue; }
        if (!fits.contains(i)) { continue; }
        const Fit fit = fits.value(i).at(month);
        if (!fitUsable(fit)) { continue; }
        if (fit.fn > bestFn)
        {
            bestFn = fit.fn;
            ref = i;
        }
    }
    if (ref < 0 || ref == gasIndex) { return -1; }

    if (!fits.contains(gasIndex)) { return ref; }
    const auto own = fits.value(gasIndex).at(month);
    if (!fitUsable(own)) { return ref; }
    const auto ratio = own.fc / fits.value(ref).at(month).fc;
    if (ratio < kFcRatioMin || ratio > 1.0 / kFcRatioMin) { return ref; }
    return -1;
}

int suggestedSourceOverall(const QVector<GasRecord>& gases, const Fits& fits,
                           int gasIndex)
{
    QHash<int, int> counts;
    for (int m = 0; m < kMonthRows; ++m)
    {
        const auto s = suggestedSource(gases, fits, gasIndex, m);
        if (s >= 0) { counts[s] += 1; }
    }
    int best = -1;
    int most = 0;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it)
    {
        if (it.value() > most || (it.value() == most && it.key() < best))
        {
            most = it.value();
            best = it.key();
        }
    }
    return best;
}

QString fitSummary(const QVector<Fit>& months)
{
    QVector<double> fn;
    QVector<double> fc;
    for (const auto& fit : months)
    {
        if (fit.fc == kError) { continue; }
        fc << fit.fc;
        if (fit.fn != kError) { fn << fit.fn; }
    }
    if (fc.isEmpty()) { return QObject::tr("not fitted"); }

    const auto range = [](const QVector<double>& v) -> QString
    {
        const auto [lo, hi] = std::minmax_element(v.cbegin(), v.cend());
        if (*lo == *hi) { return number(*lo); }
        return QString(number(*lo) + QStringLiteral("\u2013") + number(*hi));
    };
    QString text;
    if (!fn.isEmpty()) { text = QStringLiteral("Fn ") + range(fn) + QStringLiteral(", "); }
    return text + QStringLiteral("fc ") + range(fc) + QStringLiteral(" Hz");
}

} // namespace SpectralAssessmentFile
