/***************************************************************************
  outputspace.cpp
  ---------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#include "outputspace.h"

#include <QDir>
#include <QStorageInfo>

#include "ecproject.h"

namespace OutputSpace {

namespace {

//> Characters a value takes in a raw dataset: gfortran's list-directed form
//> of a double and the blanks around it. Rounded up, so the estimate errs
//> large.
constexpr qint64 BytesPerValue = 26;
//> The legend and column names at the top of each file.
constexpr qint64 HeaderBytes = 1024;

} // namespace

int rawLevels(const EcProject* project)
{
    if (!project) { return 0; }
    return (project->screenOutRaw1() > 0) + (project->screenOutRaw2() > 0)
         + (project->screenOutRaw3() > 0) + (project->screenOutRaw4() > 0)
         + (project->screenOutRaw5() > 0) + (project->screenOutRaw6() > 0)
         + (project->screenOutRaw7() > 0);
}

qint64 rawBytesPerPeriod(const EcProject* project, double acqFreqHz)
{
    const int levels = rawLevels(project);
    if (levels == 0 || acqFreqHz <= 0.0) { return 0; }

    //> The variables the page offers, counted as it shows them: a gas is
    //> written only where its own record says so (AdvOutputOptions::gasRawFor).
    int variables = (project->screenOutRawU() > 0) + (project->screenOutRawV() > 0)
                  + (project->screenOutRawW() > 0) + (project->screenOutRawTs() > 0)
                  + (project->screenOutRawTair() > 0) + (project->screenOutRawPair() > 0);
    for (const auto& gas : project->gasColumns())
    {
        if (gas.rawColumn > 0 && gas.proc.outRaw > 0) { ++variables; }
    }
    if (variables == 0) { return 0; }

    const auto records = static_cast<qint64>(acqFreqHz * project->screenAvrgLen() * 60.0);
    const qint64 perLevel = records * (variables * BytesPerValue + 1) + HeaderBytes;
    return perLevel * levels;
}

qint64 freeBytes(const QString& dir)
{
    if (dir.isEmpty()) { return -1; }
    //> The folder may not exist yet - the engine creates it - so ask about
    //> the nearest one that does; it is on the same disk.
    QDir existing(dir);
    while (!existing.exists())
    {
        if (!existing.cdUp()) { return -1; }
    }
    QStorageInfo storage(existing.absolutePath());
    if (!storage.isValid() || !storage.isReady()) { return -1; }
    return storage.bytesAvailable();
}

QString sizeText(qint64 bytes)
{
    const double b = static_cast<double>(bytes);
    if (bytes < Q_INT64_C(1073741824))
    {
        return QStringLiteral("%1 MB").arg(b / 1048576.0, 0, 'f', 1);
    }
    if (bytes < Q_INT64_C(1099511627776))
    {
        return QStringLiteral("%1 GB").arg(b / 1073741824.0, 0, 'f', 1);
    }
    return QStringLiteral("%1 TB").arg(b / 1099511627776.0, 0, 'f', 2);
}

bool isDiskFullMessage(const QByteArray& engineOutput)
{
    //> The engine's own wording, from StopIfDirLow and CheckOutputWillFit in
    //> batch_pool.f90: the first line of each explanation, which a worker's
    //> failure also carries into the parent's output, and the stop codes.
    return engineOutput.contains("The disk this run writes to has only")
        || engineOutput.contains("will not fit on the")
        || engineOutput.contains("The output disk is full.")
        || engineOutput.contains("Not enough room on the output disk");
}

} // namespace OutputSpace
