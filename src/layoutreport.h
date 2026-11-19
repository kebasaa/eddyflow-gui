/***************************************************************************
  layoutreport.h
  --------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#ifndef LAYOUTREPORT_H
#define LAYOUTREPORT_H

#include <QString>

class AdvSettingsContainer;
class QWidget;

/// Which Advanced Settings pages would make the user scroll sideways, and why.
///
/// A developer's tool, run with the environment variable
/// EDDYFLOW_LAYOUT_REPORT=<file> (and optionally EDDYFLOW_LAYOUT_WIDTH, the
/// screen's logical width, default 1536: 1920 px at 125 %). With a project
/// given on the command line, MainWindow shows Advanced Settings at that
/// width, writes this report and quits; QT_QPA_PLATFORM=offscreen runs it
/// without a window. Each page - and each item of a page's tool box, which
/// only lays out the one that is open - gets whether it overflows, by how
/// much, and the widgets and layouts that are widest, so a fix aims at what
/// actually sets the width. Nothing happens unless the variable is set.
namespace LayoutReport
{
    /// Measure every page of `pages`, shown inside `window`, and write the
    /// report to `path`. Returns false if the file could not be written.
    bool write(const QString& path, QWidget* window, AdvSettingsContainer* pages);
}

#endif  // LAYOUTREPORT_H
