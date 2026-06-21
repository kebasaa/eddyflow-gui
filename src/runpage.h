/***************************************************************************
  runpage.h
  -------------------
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

#ifndef RUNPAGE_H
#define RUNPAGE_H

#include <QElapsedTimer>
#include <QWidget>

#include "configstate.h"
#include "defs.h"

class QLabel;
class QProgressBar;
class QProgressIndicator;
class QPushButton;
class QTextEdit;
class QTimer;

class ClickLabel;
class EcProject;
class SmartFluxBar;

class RunPage : public QWidget
{
    Q_OBJECT

public:
    explicit RunPage(QWidget *parent, EcProject *ecProject, ConfigState *config);
    ~RunPage();

    void startRun(Defs::CurrRunStatus mode);
    bool pauseRun(Defs::CurrRunStatus mode);
    bool resumeRun(Defs::CurrRunStatus mode);
    void stopRun();
    void updateSmartfluxBar();
    void updateRunPage(bool small);

public slots:
    void resetBuffer();
    void bufferData(QByteArray &data);

signals:
    void updateConsoleLineRequest(QByteArray &data);
    void updateConsoleCharRequest(QByteArray &data);
    void pauseRequest(Defs::CurrRunStatus mode);

private slots:
    void pauseLabel();
    void resumeLabel();
    void runModeIconClicked();
    void updateElapsedTime();
    void updateMiniProgress();
    void openOutputDir();
    void openToviHomepage();

private:
    bool filterData(const QByteArray &data);
    QByteArray cleanupEngineOutput(QByteArray data);
    void parseEngineOutput(const QByteArray& data);
    void resetProgressSoft();
    void resetProgressHard();
    void doneLabel();
    void resetLabels();
    void resetFileLabels();
    void resetTimeEstimateLabels();
    int updateETC(int *mean_processing_time,
                  const int current_processing_time,
                  const int index,
                  const int num_steps);

    Defs::CurrRunStatus runMode_;
    QProgressIndicator* progressWidget_;
    QProgressBar* main_progress_bar;
    QProgressBar* mini_progress_bar_;
    ClickLabel* runModeIcon_;
    QLabel* runModeLabel_;
    QLabel* progressLabel_;
    QLabel* avgPeriodLabel_;
    QLabel* fileListLabel_;
    QLabel* fileProgressLabel_;
    QLabel* timeEstimateLabels_;
    QLabel* pauseResumeLabel_;
    QPushButton* open_output_dir;
    ClickLabel *toviLabel;

    EcProject* ecProject_;
    ConfigState* configState_;
    int progressValue_;
    QByteArray rxBuffer_;
    QTextEdit* errorEdit_;

    QTimer* pauseResumeDelayTimer_;        // delay and control the pause/resume operations
    QTimer* total_elapsed_update_timer_;   // update the overall elapsed time shown during a run
    QElapsedTimer overall_progress_timer_; // measure the total run time
    QElapsedTimer main_progress_timer_;    // measure the main steps run time

    SmartFluxBar* smartfluxBar_;

    bool inPlanarFit_ = false;
    bool inTimeLag_ = false;
};

#endif // RUNPAGE_H
