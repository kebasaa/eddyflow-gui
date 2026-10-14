/***************************************************************************
  engineprogress.h
  ----------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#ifndef ENGINEPROGRESS_H
#define ENGINEPROGRESS_H

#include <QByteArray>
#include <QString>

//> The engine's progress line, said for the interface only (-c gui):
//>     "  Progress [rp]: 1234 of 17520 periods done."
//> Read by the run page and by the time-lag preliminary assessment, which runs
//> an engine of its own; one reading of it, so the two cannot drift apart.
namespace EngineProgress {

inline bool isProgressLine(const QByteArray& line)
{
    return line.startsWith(QByteArrayLiteral("  Progress ["));
}

//> True, with the pass's tag and its counts, if `line` is a progress line.
inline bool parse(const QByteArray& line, QString* tag, qint64* done, qint64* total)
{
    if (!isProgressLine(line)) { return false; }
    const auto open = line.indexOf('[');
    const auto close = line.indexOf(']');
    if (close <= open) { return false; }
    const auto words = line.mid(close + 2).trimmed().split(' ');
    if (tag) { *tag = QString::fromLatin1(line.mid(open + 1, close - open - 1)); }
    if (done) { *done = words.value(0).toLongLong(); }
    if (total) { *total = words.value(2).toLongLong(); }
    return true;
}

}  // namespace EngineProgress

#endif  // ENGINEPROGRESS_H
