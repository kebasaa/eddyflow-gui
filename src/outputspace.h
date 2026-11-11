/***************************************************************************
  outputspace.h
  -------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#ifndef OUTPUTSPACE_H
#define OUTPUTSPACE_H

#include <QByteArray>
#include <QString>
#include <QtGlobal>

class EcProject;

/// Room on the disk a run writes to, checked before the run starts.
///
/// The engine watches the disk itself while it runs and stops when it runs
/// low (StopIfOutputDiskFull, batch_pool.f90), because a full network share
/// leaves every file written after that empty with no error to the program.
/// This is the same question asked earlier, before anything is processed.
namespace OutputSpace {

/// Below this the engine stops a run. The same number as the engine's
/// MinFreeOutputBytes; tests/test_output_space_static.py holds them together.
constexpr qint64 MinFreeBytes = Q_INT64_C(1073741824);

/// How many raw-dataset levels the project writes, 0 to 7.
int rawLevels(const EcProject* project);

/// What one averaging period's raw datasets take, over every level written,
/// at acquisition frequency `acqFreqHz`. 0 when none is written.
///
/// The engine writes each value list-directed, about 26 characters a value,
/// so a period's file is its records times the variables chosen, plus a
/// short header. An estimate: the engine measures the first period it
/// actually writes and stops the run if the rest will not fit
/// (CheckOutputWillFit). For scale, 30 min at 20 Hz came to 11.3 MB a level
/// on CH-LAE.
qint64 rawBytesPerPeriod(const EcProject* project, double acqFreqHz);

/// Bytes this user may still write to the disk holding `dir`, quota
/// included; -1 when that cannot be found out.
qint64 freeBytes(const QString& dir);

/// A byte count the way a person reads one.
QString sizeText(qint64 bytes);

/// Whether engine output says the run stopped for lack of room - either the
/// disk ran low while it ran, or the raw datasets were projected not to fit.
bool isDiskFullMessage(const QByteArray& engineOutput);

} // namespace OutputSpace

#endif // OUTPUTSPACE_H
