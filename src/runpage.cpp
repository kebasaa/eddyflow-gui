/***************************************************************************
  runpage.cpp
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

#include "runpage.h"

#include <QDebug>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QFile>
#include <QGridLayout>
#include <QProgressBar>
#include <QRegularExpression>
#include <QPushButton>
#include <QTextEdit>
#include <QTime>
#include <QTimer>

#include <cmath>

#include "QProgressIndicator.h"

#include "clicklabel.h"
#include "ecproject.h"
#include "engineprogress.h"
#include "mainwidget.h"
#include "smartfluxbar.h"
#include "widget_utils.h"

RunPage::RunPage(QWidget *parent, EcProject *ecProject, ConfigState* config)
    : QWidget(parent),
      runMode_(Defs::CurrRunStatus::Express),
      ecProject_(ecProject),
      configState_(config),
      progressValue_(0),
      errorEdit_(nullptr),
      pauseResumeDelayTimer_(nullptr),
      total_elapsed_update_timer_(nullptr)
{
    progressWidget_ = new QProgressIndicator;
    progressWidget_->setAnimationDelay(40);
    progressWidget_->setDisplayedWhenStopped(false);
    progressWidget_->setFixedSize(30, 30);
    progressWidget_->setColor(QColor(46, 98, 152));

    runModeIcon_ = new ClickLabel;

    runModeLabel_ = new QLabel;
    runModeLabel_->setObjectName(QStringLiteral("fileLabel"));

    progressLabel_ = new QLabel;
    progressLabel_->setObjectName(QStringLiteral("fileLabel"));

    avgPeriodLabel_ = new QLabel;
    avgPeriodLabel_->setObjectName(QStringLiteral("fileLabel"));

    fileListLabel_  = new QLabel;
    fileListLabel_->setObjectName(QStringLiteral("fileLabel"));

    fileProgressLabel_ = new QLabel;
    fileProgressLabel_->setObjectName(QStringLiteral("fileLabel"));

    timeEstimateLabels_ = new QLabel;
    timeEstimateLabels_->setText(QStringLiteral("Total elapsed time: 00:00:00 - Estimated time "
                                                "to completion: --:--:--"));
    timeEstimateLabels_->setObjectName(QStringLiteral("fileLabel"));

    pauseResumeLabel_ = new QLabel;
    pauseResumeLabel_->setObjectName(QStringLiteral("fileLabel"));

    main_progress_bar = new QProgressBar;
    main_progress_bar->setMinimum(0);
    main_progress_bar->setMaximum(10);
    main_progress_bar->setObjectName(QStringLiteral("mainProgress"));

    mini_progress_bar_ = new QProgressBar;
    mini_progress_bar_->setMinimum(0);
    mini_progress_bar_->setMaximum(25);
    mini_progress_bar_->setObjectName(QStringLiteral("miniProgress"));
    mini_progress_bar_->setTextVisible(false);

    pauseResumeDelayTimer_ = new QTimer(this);
    pauseResumeDelayTimer_->setInterval(1500);
    pauseResumeDelayTimer_->setSingleShot(true);

    total_elapsed_update_timer_ = new QTimer(this);
    total_elapsed_update_timer_->setInterval(1000);
    total_elapsed_update_timer_->setTimerType(Qt::PreciseTimer);
    eta_clock_.start();

    errorEdit_ = new QTextEdit;
    errorEdit_->setObjectName(QStringLiteral("ErrorEdit"));
    errorEdit_->setReadOnly(true);
    errorEdit_->setCursorWidth(0);
    errorEdit_->setLineWrapMode(QTextEdit::NoWrap);

    open_output_dir = new QPushButton;
    open_output_dir->setText(tr("Open output directory"));
    open_output_dir->setProperty("commonButton2", true);
    open_output_dir->setMaximumWidth(200);
    open_output_dir->setStyleSheet(QStringLiteral("QPushButton {margin-left: 0px; }"));

    auto clearErrorEditButton = new QPushButton(tr("Clear"));
    clearErrorEditButton->setProperty("mdButton", true);
    clearErrorEditButton->setMaximumWidth(clearErrorEditButton->sizeHint().width());


    auto progressLayout = new QGridLayout;
    progressLayout->addWidget(runModeIcon_, 0, 0, 1, 1);
    progressLayout->addWidget(runModeLabel_, 0, 1);
    progressLayout->addWidget(progressWidget_, 1, 0, 1, 1, Qt::AlignCenter);
    progressLayout->addWidget(progressLabel_, 1, 1);
    progressLayout->addWidget(timeEstimateLabels_, 2, 1);
    progressLayout->addWidget(main_progress_bar, 3, 1, 1, -1);
    progressLayout->addWidget(avgPeriodLabel_, 4, 1);
    progressLayout->addWidget(mini_progress_bar_, 5, 1, 1, 2);
    progressLayout->addWidget(fileListLabel_, 6, 1, Qt::AlignTop);
    progressLayout->addWidget(fileProgressLabel_, 7, 1, Qt::AlignTop);
    progressLayout->addWidget(errorEdit_, 8, 1, 1, 2);
    progressLayout->addWidget(pauseResumeLabel_, 9, 1);
    progressLayout->addWidget(clearErrorEditButton, 9, 2, Qt::AlignRight);
    progressLayout->addWidget(open_output_dir, 10, 1);
    progressLayout->setColumnStretch(2, 2);
    progressLayout->setRowStretch(8, 2);
    progressLayout->setRowStretch(12, 2);
    progressLayout->setRowMinimumHeight(0, 42);   // = runModeIcon_.width()
    progressLayout->setColumnMinimumWidth(0, 42); // > runModeIcon_.height()
    progressLayout->setHorizontalSpacing(6);
    progressLayout->setContentsMargins(20, 15, 60, 0);

    smartfluxBar_ = new SmartFluxBar(ecProject_, configState_);

    auto mainLayout = new QGridLayout(this);
    mainLayout->addWidget(smartfluxBar_, 0, 0);
    mainLayout->addLayout(progressLayout, 1, 0);
    mainLayout->setContentsMargins(20, 15, 30, 0);
    mainLayout->setRowStretch(1, 0);

    setLayout(mainLayout);

    connect(runModeIcon_, &ClickLabel::clicked,
            this, &RunPage::runModeIconClicked);

    auto mainWidget = static_cast<MainWidget*>(parent);
    connect(smartfluxBar_, &SmartFluxBar::showSmartfluxBarRequest,
            mainWidget, &MainWidget::showSmartfluxBarRequest);

    connect(smartfluxBar_, &SmartFluxBar::saveSilentlyRequest,
            mainWidget, &MainWidget::saveSilentlyRequest);

    connect(smartfluxBar_, &SmartFluxBar::saveRequest,
            mainWidget, &MainWidget::saveRequest);

    connect(total_elapsed_update_timer_, &QTimer::timeout,
            this, &RunPage::updateElapsedTime);

    connect(total_elapsed_update_timer_, &QTimer::timeout,
            this, &RunPage::updateMiniProgress);
    connect(open_output_dir, &QPushButton::clicked,
            this, &RunPage::openOutputDir);

    auto open_output_lambda = [=]() {
        open_output_dir->setDisabled(ecProject_->generalOutPath().isEmpty());
    };

    connect(ecProject_, &EcProject::ecProjectNew,
            this, open_output_lambda);

    connect(ecProject_, &EcProject::ecProjectModified,
            this, open_output_lambda);

    connect(ecProject_, &EcProject::ecProjectChanged,
            this, open_output_lambda);

    connect(clearErrorEditButton, &QPushButton::clicked,
            errorEdit_, &QTextEdit::clear);

    QList<WidgetUtils::PropertyList> progressBarProp;
    progressBarProp << WidgetUtils::PropertyList("expRun", false)
                    << WidgetUtils::PropertyList("advRun", false)
                    << WidgetUtils::PropertyList("retRun", false);
    WidgetUtils::updatePropertyListAndStyle(main_progress_bar, progressBarProp);
    WidgetUtils::updatePropertyListAndStyle(mini_progress_bar_, progressBarProp);
}

RunPage::~RunPage()
{
    if (pauseResumeDelayTimer_)
    {
        delete pauseResumeDelayTimer_;
    }
}

void RunPage::startRun(Defs::CurrRunStatus mode)
{
    QList<WidgetUtils::PropertyList> iconModeProp;
    QList<WidgetUtils::PropertyList> progressBarProp;
    auto progressText = QString();

    runMode_ = mode;

    // The estimate is of the whole run. FCC run straight after the RP run it
    // follows carries on with RP's; any other start is a new run.
    const bool continuing = mode == Defs::CurrRunStatus::Advanced_FCC
                            && fccFollows_ && rpSucceeded_ && eta_.running();
    if (!continuing)
    {
        eta_.start(ecProject_->generalFileName(),
                   configState_->general.parallelPrepass,
                   eta_clock_.elapsed());
        overall_progress_timer_.restart();
    }
    fccFollows_ = mode == Defs::CurrRunStatus::Advanced_RP
                  && ecProject_->isEngineStep2Needed();
    rpSucceeded_ = false;
    if (fccFollows_)
        eta_.planFcc();
    total_elapsed_update_timer_->start();
    updateTimeEstimateLabels();

    switch (runMode_)
    {
        case Defs::CurrRunStatus::Express:
        {
            iconModeProp << WidgetUtils::PropertyList("pauseExpButton", true)
                         << WidgetUtils::PropertyList("pauseAdvButton", false)
                         << WidgetUtils::PropertyList("pauseRetButton", false)
                         << WidgetUtils::PropertyList("resumeExpButton", false)
                         << WidgetUtils::PropertyList("resumeAdvButton", false)
                         << WidgetUtils::PropertyList("resumeRetButton", false);

            runModeLabel_->setText(tr("Running in Express mode"));

            progressBarProp << WidgetUtils::PropertyList("expRun", true)
                            << WidgetUtils::PropertyList("advRun", false)
                            << WidgetUtils::PropertyList("retRun", false);

            progressText = tr("Initializing raw data processing...");

            errorEdit_->clear();

            break;
        }
    case Defs::CurrRunStatus::Advanced_RP:
        {
            iconModeProp << WidgetUtils::PropertyList("pauseExpButton", false)
                         << WidgetUtils::PropertyList("pauseAdvButton", true)
                         << WidgetUtils::PropertyList("pauseRetButton", false)
                         << WidgetUtils::PropertyList("resumeExpButton", false)
                         << WidgetUtils::PropertyList("resumeAdvButton", false)
                         << WidgetUtils::PropertyList("resumeRetButton", false);

            runModeLabel_->setText(tr("Running in Advanced mode"));

            progressBarProp << WidgetUtils::PropertyList("expRun", false)
                            << WidgetUtils::PropertyList("advRun", true)
                            << WidgetUtils::PropertyList("retRun", false);

            progressText = tr("Initializing raw data processing...");

            errorEdit_->clear();

            break;
        }
    case Defs::CurrRunStatus::Advanced_FCC:
        {
            iconModeProp << WidgetUtils::PropertyList("pauseExpButton", false)
                         << WidgetUtils::PropertyList("pauseAdvButton", true)
                         << WidgetUtils::PropertyList("pauseRetButton", false)
                         << WidgetUtils::PropertyList("resumeExpButton", false)
                         << WidgetUtils::PropertyList("resumeAdvButton", false)
                         << WidgetUtils::PropertyList("resumeRetButton", false);

            runModeLabel_->setText(tr("Running in Advanced mode"));

            progressBarProp << WidgetUtils::PropertyList("expRun", false)
                            << WidgetUtils::PropertyList("advRun", true)
                            << WidgetUtils::PropertyList("retRun", false);

            progressText = tr("Initializing flux computation processing...");
            errorEdit_->append(QStringLiteral("\n"));
            break;
        }
    case Defs::CurrRunStatus::Retriever:
        {
            iconModeProp << WidgetUtils::PropertyList("pauseExpButton", false)
                         << WidgetUtils::PropertyList("pauseAdvButton", false)
                         << WidgetUtils::PropertyList("pauseRetButton", true)
                         << WidgetUtils::PropertyList("resumeExpButton", false)
                         << WidgetUtils::PropertyList("resumeAdvButton", false)
                         << WidgetUtils::PropertyList("resumeRetButton", false);

            runModeLabel_->setText(tr("Running Metadata Retriever"));

            progressBarProp << WidgetUtils::PropertyList("expRun", false)
                            << WidgetUtils::PropertyList("advRun", false)
                            << WidgetUtils::PropertyList("retRun", true);

            progressText = tr("Initializing metadata retriever...");

            errorEdit_->clear();

            break;
        }
//    default:
//        break;
    }

    WidgetUtils::updatePropertyListAndStyle(runModeIcon_, iconModeProp);
    WidgetUtils::updatePropertyListAndStyle(main_progress_bar, progressBarProp);
    WidgetUtils::updatePropertyListAndStyle(mini_progress_bar_, progressBarProp);

    runModeIcon_->setVisible(true);
    progressLabel_->setText(progressText);
    progressWidget_->startAnimation();
}

bool RunPage::pauseRun(Defs::CurrRunStatus mode)
{
    if (!pauseResumeDelayTimer_->isActive())
    {
        QList<WidgetUtils::PropertyList> iconModeProp;
        pauseResumeDelayTimer_->start();
        runMode_ = mode;
        switch (runMode_)
        {
            case Defs::CurrRunStatus::Express:
            {
                iconModeProp << WidgetUtils::PropertyList("pauseExpButton", false)
                             << WidgetUtils::PropertyList("pauseAdvButton", false)
                             << WidgetUtils::PropertyList("pauseRetButton", false)
                             << WidgetUtils::PropertyList("resumeExpButton", true)
                             << WidgetUtils::PropertyList("resumeAdvButton", false)
                             << WidgetUtils::PropertyList("resumeRetButton", false);
                break;
            }
        case Defs::CurrRunStatus::Advanced_RP:
        case Defs::CurrRunStatus::Advanced_FCC:
            {
                iconModeProp << WidgetUtils::PropertyList("pauseExpButton", false)
                             << WidgetUtils::PropertyList("pauseAdvButton", false)
                             << WidgetUtils::PropertyList("pauseRetButton", false)
                             << WidgetUtils::PropertyList("resumeExpButton", false)
                             << WidgetUtils::PropertyList("resumeAdvButton", true)
                             << WidgetUtils::PropertyList("resumeRetButton", false);
                break;
            }
        case Defs::CurrRunStatus::Retriever:
            {
                iconModeProp << WidgetUtils::PropertyList("pauseExpButton", false)
                             << WidgetUtils::PropertyList("pauseAdvButton", false)
                             << WidgetUtils::PropertyList("pauseRetButton", false)
                             << WidgetUtils::PropertyList("resumeExpButton", false)
                             << WidgetUtils::PropertyList("resumeAdvButton", false)
                             << WidgetUtils::PropertyList("resumeRetButton", true);
                break;
            }
//    default:
//        break;
        }

        WidgetUtils::updatePropertyListAndStyle(runModeIcon_, iconModeProp);
        runModeIcon_->setVisible(true);
        pauseResumeLabel_->setText(tr("Pausing computations..."));
        progressWidget_->setDisplayedWhenStopped(true);
        progressWidget_->stopAnimation();
        total_elapsed_update_timer_->stop();
        eta_.pause(eta_clock_.elapsed());
        QTimer::singleShot(1000, this, &RunPage::pauseLabel);
        return true;
    }
    else
    {
        return false;
    }
}

bool RunPage::resumeRun(Defs::CurrRunStatus mode)
{
    if (!pauseResumeDelayTimer_->isActive())
    {
        QList<WidgetUtils::PropertyList> iconModeProp;
        pauseResumeDelayTimer_->start();
        runMode_ = mode;
        switch (runMode_)
        {
            case Defs::CurrRunStatus::Express:
            {
                iconModeProp << WidgetUtils::PropertyList("pauseExpButton", true)
                             << WidgetUtils::PropertyList("pauseAdvButton", false)
                             << WidgetUtils::PropertyList("pauseRetButton", false)
                             << WidgetUtils::PropertyList("resumeExpButton", false)
                             << WidgetUtils::PropertyList("resumeAdvButton", false)
                             << WidgetUtils::PropertyList("resumeRetButton", false);
                break;
            }
        case Defs::CurrRunStatus::Advanced_RP:
        case Defs::CurrRunStatus::Advanced_FCC:
            {
                iconModeProp << WidgetUtils::PropertyList("pauseExpButton", false)
                             << WidgetUtils::PropertyList("pauseAdvButton", true)
                             << WidgetUtils::PropertyList("pauseRetButton", false)
                             << WidgetUtils::PropertyList("resumeExpButton", false)
                             << WidgetUtils::PropertyList("resumeAdvButton", false)
                             << WidgetUtils::PropertyList("resumeRetButton", false);
                break;
            }
        case Defs::CurrRunStatus::Retriever:
            {
                iconModeProp << WidgetUtils::PropertyList("pauseExpButton", false)
                             << WidgetUtils::PropertyList("pauseAdvButton", false)
                             << WidgetUtils::PropertyList("pauseRetButton", true)
                             << WidgetUtils::PropertyList("resumeExpButton", false)
                             << WidgetUtils::PropertyList("resumeAdvButton", false)
                             << WidgetUtils::PropertyList("resumeRetButton", false);
                break;
            }
//    default:
//        break;
        }

        WidgetUtils::updatePropertyListAndStyle(runModeIcon_, iconModeProp);
        runModeIcon_->setVisible(true);
        pauseResumeLabel_->setText(tr("Resuming computations..."));
        progressWidget_->setDisplayedWhenStopped(true);
        progressWidget_->startAnimation();
        total_elapsed_update_timer_->start();
        eta_.resume(eta_clock_.elapsed());
        QTimer::singleShot(1000, this, &RunPage::resumeLabel);
        return true;
    }
    else
    {
        return false;
    }
}

void RunPage::stopRun()
{
    progressWidget_->setDisplayedWhenStopped(false);
    progressWidget_->stopAnimation();
    resetBuffer();
    resetProgressHard();
    total_elapsed_update_timer_->stop();
}

// An engine process has ended. RP does not say "executed gracefully" when FCC
// follows it, so its end - and whether the FCC run that follows belongs to the
// same estimate - is known only from the process.
void RunPage::engineExited(bool success)
{
    if (success)
        eta_.engineFinished(eta_clock_.elapsed());
    rpSucceeded_ = success && fccFollows_;
    if (!success)
        fccFollows_ = false;
    updateTimeEstimateLabels();
}

void RunPage::pauseLabel()
{
    pauseResumeDelayTimer_->stop();
    pauseResumeLabel_->setText(tr("Computations paused"));
}

void RunPage::resumeLabel()
{
    pauseResumeDelayTimer_->stop();
    pauseResumeLabel_->clear();
}

void RunPage::resetBuffer()
{
    rxBuffer_.resize(0);
}

void RunPage::resetProgressSoft()
{
    progressValue_ = 0;
    main_progress_bar->setValue(0);
    main_progress_bar->setMaximum(10);
    main_progress_bar->reset();
}

void RunPage::resetProgressHard()
{
    runModeIcon_->setVisible(false);
    runModeLabel_->clear();
    pauseResumeLabel_->clear();
    doneLabel();
    resetLabels();
    resetFileLabels();
    resetTimeEstimateLabels();

    resetProgressSoft();
    mini_progress_bar_->setValue(0);
    mini_progress_bar_->reset();
}

void RunPage::doneLabel()
{
    main_progress_bar->setValue(main_progress_bar->maximum());
    progressLabel_->setText(tr("Done"));
}

void RunPage::resetFileLabels()
{
    avgPeriodLabel_->clear();
    fileListLabel_->clear();
    fileProgressLabel_->clear();
}

void RunPage::resetTimeEstimateLabels()
{
    // commented for debug only
//    timeEstimateLabels_->clear();
}

void RunPage::resetLabels()
{
    progressLabel_->clear();
    resetFileLabels();
    resetTimeEstimateLabels();
}

bool RunPage::filterData(const QByteArray& data)
{
    // The engine's lines for the estimate of the time left, every few
    // seconds: not for reading.
    if (data.startsWith(QByteArrayLiteral("  Progress ["))
        || data.startsWith(QByteArrayLiteral("  Run plan:")))
    {
        return true;
    }
    if (inPlanarFit_ or inTimeLag_)
    {
        return data.contains(QByteArrayLiteral("another small step"));
    }
    return false;
}

// Only complete lines are handled. The text after the last newline of a read
// is kept until the rest of its line arrives: handling it at once as well
// showed every line a pipe read happened to cut twice, first cut short.
void RunPage::bufferData(QByteArray &data)
{
    rxBuffer_.append(data);
    const auto lastNewline = rxBuffer_.lastIndexOf('\n');
    if (lastNewline < 0)
        return;

    const QByteArray complete = rxBuffer_.left(lastNewline);
    rxBuffer_ = rxBuffer_.mid(lastNewline + 1);

    const QByteArrayList lineList = complete.split('\n');
    for (const auto& rawLine : lineList)
    {
        for (const auto& line : splitGluedMessage(rawLine))
        {
            QByteArray tempData(line);
            data = cleanupEngineOutput(tempData);
            if (!data.isEmpty())
            {
                parseEngineOutput(data);
                if (!filterData(data))
                {
                    emit updateConsoleLineRequest(data);
                }
            }
        }
    }
}

// A run that ends without a final newline still shows its last line.
void RunPage::flushBuffer()
{
    if (!rxBuffer_.isEmpty())
    {
        QByteArray end("\n");
        bufferData(end);
    }
    resetBuffer();
}

// The engine opens a progress line ("   Absolute limits test..") and ends it
// once the step is done; a message raised in between is written onto it. The
// progress keywords are matched first and return, so the message never
// reached the warning panel and its first line was lost. Such a line is
// handed on as two: the progress text, and the message from its tag.
QByteArrayList RunPage::splitGluedMessage(const QByteArray& line)
{
    static const QRegularExpression tag(
        QStringLiteral("(?:Fatal error|Warning|Error|Alert)\\(\\d+\\)>"));
    const auto match = tag.match(QString::fromLatin1(line));
    if (match.hasMatch() && match.capturedStart() > 0)
    {
        const auto at = match.capturedStart();
        const QByteArray before = line.left(at);
        if (!before.trimmed().isEmpty())
            return { before, QByteArray(" ") + line.mid(at) };
    }
    return { line };
}

QByteArray RunPage::cleanupEngineOutput(QByteArray data)
{
    if (data.contains(QByteArrayLiteral("of file Z:")))
    {
        // cleanup file path in case of fortan runtime error
        data = data.mid(0, data.indexOf("Z:")) + data.mid(data.lastIndexOf("\\") + 1);
    }
    else if (data == QByteArrayLiteral(" ")
             || data.contains(QByteArrayLiteral("STOP"))
             || data.contains(QByteArrayLiteral("Note: The following floating-point exceptions")))
    {
        data = QByteArray();
    }

    return data;
}

void RunPage::parseEngineOutput(const QByteArray &data)
{
    QByteArray cleanLine;
    cleanLine.append(data);

    static int averagingPeriodIndex = 0;
    static int totalAveragingPeriods = 0;

    static QString fromStr;
    static QString toStr;

#ifdef QT_DEBUG
    QString elapsedTimeMSecStr;
#endif

    QStringList currentFileList;

    static QDate currentPlanarFitDate;
    static QDate currentTimeLagDate;

#ifdef QT_DEBUG
    QFile outfile(QStringLiteral("parse-engine-output.txt"));
    outfile.open(QIODevice::WriteOnly | QIODevice::Append);
    QTextStream out(&outfile);
#endif

    // start preamble
    if (cleanLine.contains(QByteArrayLiteral("Executing EddyFlow")))
    {
        total_elapsed_update_timer_->start();
        updateTimeEstimateLabels();

        // re-init numerical values
        averagingPeriodIndex = 0;
        totalAveragingPeriods = 0;
        // A split in the previous run is not one in this
        prodSplit_ = false;
        prodPwb_ = false;
        prodPhase_ = 0;
        fccFluxDays_ = false;

        resetProgressSoft();
        main_progress_bar->setValue(++progressValue_);

        fromStr.clear();
        toStr.clear();

        inPlanarFit_ = false;
        inTimeLag_ = false;
        currentFileList.clear();

#ifdef QT_DEBUG
        out << "file #, "
            << "procTimeMSec, " <<"meanProcTimeMSec, "
            << "elapsedMSec, " << "elapsedMSec_Str, "
            << "ETC, " << "ETC_Str";
        Qt::endl(out);
        out << "Executing EddyFlow";
        Qt::endl(out);
        elapsedTimeMSecStr = QTime(0, 0).addMSecs(static_cast<int>(overall_progress_timer_.elapsed()))
                                    .toString(QStringLiteral("hh:mm:ss.zzz"));
        out << averagingPeriodIndex << " "
            << "x "
            << "x "
            << overall_progress_timer_.elapsed() << " "
            << elapsedTimeMSecStr << " "
            << "x "
            << "x ";
        Qt::endl(out);
#endif
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Reading EddyFlow project file")))
    {
        main_progress_bar->setValue(++progressValue_);
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Retrieving file")))
    {
        main_progress_bar->setValue(++progressValue_);
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("names from directory")))
    {
        main_progress_bar->setValue(++progressValue_);
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Retrieving timestamps")))
    {
        main_progress_bar->setValue(++progressValue_);
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("from file names")))
    {
        main_progress_bar->setValue(++progressValue_);
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Arranging raw files")))
    {
        main_progress_bar->setValue(++progressValue_);
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("in chronological order")))
    {
        main_progress_bar->setValue(++progressValue_);
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Creating master time series")))
    {
        main_progress_bar->setValue(main_progress_bar->maximum());

#ifdef QT_DEBUG
        out << "Creating master time series";
        Qt::endl(out);
        elapsedTimeMSecStr = QTime(0, 0).addMSecs(static_cast<int>(overall_progress_timer_.elapsed()))
                                    .toString(QStringLiteral("hh:mm:ss.zzz"));
        out << averagingPeriodIndex << " "
            << "x "
            << "x "
            << overall_progress_timer_.elapsed() << " "
            << elapsedTimeMSecStr << " "
            << "x "
            << "x ";
        Qt::endl(out);
#endif
        return;
    }
    // end preamble

    // The passes the engine will walk, for the estimate of the time left:
    // "  Run plan: to pf rp (17520 periods)", or FCC's "  Run plan: sa fx".
    if (cleanLine.startsWith(QByteArrayLiteral("  Run plan:")))
    {
        QByteArray rest = cleanLine.mid(cleanLine.indexOf(':') + 1);
        qint64 periods = 0;
        const auto paren = rest.indexOf('(');
        if (paren >= 0)
        {
            periods = rest.mid(paren + 1).trimmed().split(' ').value(0).toLongLong();
            rest = rest.left(paren);
        }
        QStringList stages;
        const auto words = rest.simplified().split(' ');
        for (const auto& word : words)
        {
            if (!word.isEmpty())
                stages << QString::fromLatin1(word);
        }
        eta_.plan(stages, periods);
        updateTimeEstimateLabels();
        return;
    }
    // How far the pass under way has got, over every process of a split one:
    // "  Progress [rp]: 1234 of 17520 periods done."
    if (EngineProgress::isProgressLine(cleanLine))
    {
        QString tag;
        qint64 done = 0;
        qint64 total = 0;
        if (EngineProgress::parse(cleanLine, &tag, &done, &total))
        {
            eta_.progress(tag, done, total, eta_clock_.elapsed());
            if (prodSplit_ || tag == QLatin1String("sa"))
                setSplitProgress(tag, done, total);
            updateTimeEstimateLabels();
        }
        return;
    }

    // start planar fit
    if (cleanLine.contains(QByteArrayLiteral("Performing planar-fit assessment")))
    {
        inPlanarFit_ = true;
        progressLabel_->setText(tr("Performing planar-fit assessment..."));

        // re-init
        averagingPeriodIndex = 0;
        totalAveragingPeriods = 0;
        resetProgressSoft();
        main_progress_bar->setValue(++progressValue_);
        fromStr.clear();
        toStr.clear();
        prodSplit_ = false;

        errorEdit_->append(QStringLiteral("Performing planar-fit assessment"));

        return;
    }
    // valid for both planar fit and time lag
    if (cleanLine.contains(QByteArrayLiteral("Maximum number of flux averaging periods available")))
    {
        QString numStr = QLatin1String(cleanLine.trimmed().split(':').last().trimmed().constData());
        totalAveragingPeriods = numStr.toInt();
        main_progress_bar->setMaximum(totalAveragingPeriods);
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Importing wind data for")))
    {
        QString dateStr = QLatin1String(cleanLine.mid(25).constData());
        currentPlanarFitDate = QDate::fromString(dateStr.trimmed(),
                                                 QStringLiteral("dd MMMM yyyy"));
        fileProgressLabel_->setText(QStringLiteral("Importing wind data"));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("another small step to the planar-fit")))
    {
        auto timeStr = QLatin1String(cleanLine.trimmed().split(' ')
                                     .last().trimmed().constData());
        auto currentPlanarFitTime = QTime::fromString(timeStr, QStringLiteral("hh:mm"));
        QDateTime fromDate(currentPlanarFitDate, currentPlanarFitTime
                           .addSecs(-ecProject_->screenAvrgLen() * 60));
        fromStr = fromDate.toString(Qt::ISODate).replace(QLatin1String("T"), QLatin1String(" "));
        QDateTime toDate(currentPlanarFitDate, currentPlanarFitTime);
        toStr = toDate.toString(Qt::ISODate);
        avgPeriodLabel_->setText(tr("Averaging interval, From: %1, To: %2")
                            .arg(fromStr)
                            .arg(toStr));
        auto avgPeriod = avgPeriodLabel_->text();
        errorEdit_->append(avgPeriod.prepend(QStringLiteral("<font color=\"#A6D7F2\">"))
                                    .append(QStringLiteral("</font>")));

        ++averagingPeriodIndex;

        mini_progress_bar_->setValue(0);
        mini_progress_bar_->reset();
        if (!prodSplit_)
            main_progress_bar->setValue(++progressValue_);
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Sorting wind data by sector")))
    {
        fileProgressLabel_->setText(QStringLiteral("Sorting wind data by sector"));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Calculating planar fit rotation matrices")))
    {
        fileProgressLabel_->setText(QStringLiteral("Calculating planar fit rotation matrices"));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Planar Fit session terminated")))
    {
        inPlanarFit_ = false;
        main_progress_bar->setValue(main_progress_bar->maximum());
        return;
    }
    // end planar fit

    // start time lag
    if (cleanLine.contains(QByteArrayLiteral("Performing time-lag optimization")))
    {
        inTimeLag_ = true;
        progressLabel_->setText(tr("Performing time-lag optimization..."));

        // re-init
        averagingPeriodIndex = 0;
        totalAveragingPeriods = 0;
        resetProgressSoft();
        main_progress_bar->setValue(++progressValue_);
        fromStr.clear();
        toStr.clear();
        prodSplit_ = false;

        errorEdit_->append(QStringLiteral("Performing time-lag optimization"));

        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Importing data for")))
    {
        QString dateStr = QLatin1String(cleanLine.mid(21).constData());
        currentTimeLagDate = QDate::fromString(dateStr.trimmed(),
                                                 QStringLiteral("dd MMMM yyyy"));
        fileProgressLabel_->setText(QStringLiteral("Importing data"));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("another small step to the time-lag")))
    {
        auto timeStr = QLatin1String(cleanLine.trimmed().split(' ')
                                     .last().trimmed().constData());
        auto currentTimeLagTime = QTime::fromString(timeStr, QStringLiteral("hh:mm"));

        QDateTime fromDate(currentTimeLagDate, currentTimeLagTime
                           .addSecs(-ecProject_->screenAvrgLen() * 60));
        fromStr = fromDate.toString(Qt::ISODate).replace(QLatin1String("T"), QLatin1String(" "));
        QDateTime toDate(currentTimeLagDate, currentTimeLagTime);
        toStr = toDate.toString(Qt::ISODate);

        avgPeriodLabel_->setText(tr("Averaging interval, From: %1, To: %2")
                            .arg(fromStr)
                            .arg(toStr));
        auto avgPeriod = avgPeriodLabel_->text();
        errorEdit_->append(avgPeriod.prepend(QStringLiteral("<font color=\"#A6D7F2\">"))
                                    .append(QStringLiteral("</font>")));

        ++averagingPeriodIndex;

        mini_progress_bar_->setValue(0);
        mini_progress_bar_->reset();
        if (!prodSplit_)
            main_progress_bar->setValue(++progressValue_);
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("optimization session terminated")))
    {
        inTimeLag_ = false;
        main_progress_bar->setValue(main_progress_bar->maximum());
        return;
    }
    // end time lag

    // start raw data processing
    if (cleanLine.contains(QByteArrayLiteral("Start raw data processing")))
    {
        progressLabel_->setText(tr("Processing raw data..."));

        errorEdit_->append(QStringLiteral("Start raw data processing"));

        // re-init
        averagingPeriodIndex = 0;
        totalAveragingPeriods = 0;
        resetProgressSoft();
        main_progress_bar->setValue(++progressValue_);
        fromStr.clear();
        toStr.clear();
        prodSplit_ = false;
        prodPwb_ = false;
        prodPhase_ = 0;

#ifdef QT_DEBUG
        out << "Start raw data processing";
        Qt::endl(out);
        elapsedTimeMSecStr = QTime(0, 0).addMSecs(static_cast<int>(overall_progress_timer_.elapsed()))
                                    .toString(QStringLiteral("hh:mm:ss.zzz"));
        out << averagingPeriodIndex << " "
            << "x "
            << "x "
            << overall_progress_timer_.elapsed() << " "
            << elapsedTimeMSecStr << " "
            << "x "
            << "x ";
        Qt::endl(out);
#endif
        return;
    }

    // A split pass - a pre-pass, the production pass, FCC's binned import
    // or its flux computation. The engine hands most of the range to worker
    // processes; this process prints its own first piece only, so the bar
    // follows the engine's progress lines from here on, which count every
    // process's periods. With PWB time lags every piece of a production pass
    // is read twice - first for its evidence, then for its fluxes - and each
    // round reports its own pieces.
    if (cleanLine.contains(QByteArrayLiteral("Splitting the "))
        && cleanLine.contains(QByteArrayLiteral(" worker processes")))
    {
        // "  Splitting the production pass across 4 worker processes."
        const auto words = cleanLine.trimmed().split(' ');
        const auto across = words.indexOf(QByteArrayLiteral("across"));
        if (across >= 0)
            eta_.setProcesses(words.value(across + 1).toInt());
        prodSplit_ = true;
        prodPwb_ = false;
        prodPhase_ = 0;
        prodBaseValue_ = progressValue_;
        return;
    }
    if (prodSplit_ && cleanLine.contains(QByteArrayLiteral("PWB time lags: each piece is first read")))
    {
        prodPwb_ = true;
        return;
    }
    if (prodSplit_ && cleanLine.contains(QByteArrayLiteral("Waiting for the workers:")))
    {
        ++prodPhase_;
        return;
    }
    if (prodSplit_ && cleanLine.contains(QByteArrayLiteral(" pieces done.")))
    {
        // "   k of M pieces done."
        const auto words = cleanLine.trimmed().split(' ');
        const int done = words.value(0).toInt();
        const int total = words.value(2).toInt();
        if (done > 0 && total > 0)
        {
            QString stage = eta_.currentStageName();
            if (stage.isEmpty())
                stage = tr("Processing");
            stage[0] = stage.at(0).toUpper();
            progressLabel_->setText(tr("%1 in parallel: %2 of %3 pieces done")
                                    .arg(stage).arg(done).arg(total));
        }
        return;
    }

    if (cleanLine.contains(QByteArrayLiteral("From:")))
    {
        fromStr = QLatin1String(cleanLine.mid(7, 16).constData());
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("To:")))
    {
        toStr = QLatin1String(cleanLine.mid(7, 16).constData());
        avgPeriodLabel_->setText(tr("Averaging interval, From: %1, To: %2")
                            .arg(fromStr)
                            .arg(toStr));
        auto avgPeriod = avgPeriodLabel_->text();
        errorEdit_->append(avgPeriod.prepend(QStringLiteral("<font color=\"#A6D7F2\">"))
                                    .append(QStringLiteral("</font>")));

        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Total number of flux averaging periods")))
    {
        totalAveragingPeriods = cleanLine.trimmed().trimmed().split(':').last().trimmed().toInt();
        main_progress_bar->setMaximum(totalAveragingPeriods * 8 + 1);
        return;
    }
    // start processing cycle
    if (cleanLine.contains(QByteArrayLiteral("processing new flux averaging period")))
    {
        ++averagingPeriodIndex;
        eta_.ownPeriod(eta_clock_.elapsed());
        if (!prodSplit_)
            main_progress_bar->setValue(++progressValue_);

#ifdef QT_DEBUG
        out << ">> Processing new flux averaging period";
        Qt::endl(out);
        elapsedTimeMSecStr = QTime(0, 0).addMSecs(static_cast<int>(overall_progress_timer_.elapsed()))
                                    .toString(QStringLiteral("hh:mm:ss.zzz"));
        out << averagingPeriodIndex << " "
            << "x "
            << "x "
            << overall_progress_timer_.elapsed() << " "
            << elapsedTimeMSecStr << " "
            << "x "
            << "x ";
        Qt::endl(out);
#endif

        currentFileList.clear();

        return;
    }

    if (cleanLine.contains(QByteArrayLiteral("File(s): ..")))
    {
        mini_progress_bar_->setValue(1);
        avgPeriodLabel_->setText(tr("Averaging interval, From: %1, To: %2")
                            .arg(fromStr)
                            .arg(toStr));

        fileProgressLabel_->setText(tr("Parsing file"));

        currentFileList.append(QLatin1String(cleanLine.trimmed().split('\\')
                                             .last().trimmed().constData()));
        fileListLabel_->setText(QStringLiteral("File(s): %1")
                                .arg(currentFileList.join(QLatin1Char('\n'))));
        return;
    }

    if (cleanLine.contains((QByteArrayLiteral("Skipping to next averaging period"))))
    {
        main_progress_bar->setValue(++progressValue_);
        avgPeriodLabel_->setText(tr("Averaging interval, From: %1, To: %2")
                            .arg(fromStr)
                            .arg(toStr));

#ifdef QT_DEBUG
        out << "Skipping to next averaging period";
        Qt::endl(out);
#endif
        return;
    }

    if (cleanLine.contains(QByteArrayLiteral("..\\")))
    {
        currentFileList.append(QLatin1String(cleanLine.trimmed().split('\\')
                                             .last().trimmed().constData()));
        return;
    }

    if (cleanLine.contains(QByteArrayLiteral("Number of samples")))
    {
        mini_progress_bar_->setValue(2);
        fileListLabel_->setText(QStringLiteral("File(s): %1")
                                .arg(currentFileList.join(QLatin1Char('\n'))));
        return;
    }

    if (cleanLine.contains(QByteArrayLiteral("Calculating statistics..")))
    {
        mini_progress_bar_->setValue(3);
        fileProgressLabel_->setText(tr("Calculating statistics"));

        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Raw level statistical screening")))
    {
        mini_progress_bar_->setValue(4);
        fileProgressLabel_->setText(tr("Raw level statistical screening"));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Spike detection/removal test")))
    {
        mini_progress_bar_->setValue(5);
        fileProgressLabel_->setText(tr("Spike detection/removal"));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Spike detection/removal test")))
    {
        mini_progress_bar_->setValue(6);
        fileProgressLabel_->setText(tr("Spike detection/removal test"));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Absolute limits test")))
    {
        mini_progress_bar_->setValue(7);
        fileProgressLabel_->setText(tr("Absolute limits test"));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Skewness & kurtosis test")))
    {
        main_progress_bar->setValue(++progressValue_);
        mini_progress_bar_->setValue(8);
        fileProgressLabel_->setText(tr("Skewness & kurtosis test"));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Despiking user set")))
    {
        mini_progress_bar_->setValue(9);
        fileProgressLabel_->setText(tr("Despiking user set"));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Cross-wind correction")))
    {
        mini_progress_bar_->setValue(10);
        fileProgressLabel_->setText(tr("Cross-wind correction"));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Converting into mixing ratio")))
    {
        mini_progress_bar_->setValue(11);
        fileProgressLabel_->setText(tr("Converting into mixing ratio"));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Performing tilt correction")))
    {
        mini_progress_bar_->setValue(12);
        fileProgressLabel_->setText(tr("Performing tilt correction"));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Compensating time lags")))
    {
        mini_progress_bar_->setValue(13);
        fileProgressLabel_->setText(tr("Compensating time lags"));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Compensating user variables")))
    {
        mini_progress_bar_->setValue(14);
        fileProgressLabel_->setText(tr("Compensating user variables"));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Performing stationarity test")))
    {
        mini_progress_bar_->setValue(15);
        fileProgressLabel_->setText(tr("Performing stationarity test"));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Detrending")))
    {
        main_progress_bar->setValue(++progressValue_);
        mini_progress_bar_->setValue(16);
        fileProgressLabel_->setText(tr("Detrending"));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Calculating (co)spectra")))
    {
        mini_progress_bar_->setValue(17);
        fileProgressLabel_->setText(tr("Calculating (co)spectra"));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Tapering timeseries")))
    {
        mini_progress_bar_->setValue(18);
        fileProgressLabel_->setText(tr("Tapering timeseries"));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("FFT-ing")))
    {
        mini_progress_bar_->setValue(19);
        fileProgressLabel_->setText(tr("FFT-ing"));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Cospectral densities")))
    {
        mini_progress_bar_->setValue(20);
        fileProgressLabel_->setText(tr("Cospectral densities"));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Calculating fluxes Level 0")))
    {
        mini_progress_bar_->setValue(21);
        fileProgressLabel_->setText(tr("Calculating fluxes Level 0"));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Calculating fluxes Level 1")))
    {
        mini_progress_bar_->setValue(22);
        fileProgressLabel_->setText(tr("Calculating fluxes Level 1"));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Calculating fluxes Level 2")))
    {
        mini_progress_bar_->setValue(23);
        fileProgressLabel_->setText(tr("Calculating fluxes Level 2 and 3"));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Estimating footprint")))
    {
        mini_progress_bar_->setValue(24);
        fileProgressLabel_->setText(tr("Estimating footprint"));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Calculating quality flags")))
    {
        mini_progress_bar_->setValue(mini_progress_bar_->maximum());
        fileProgressLabel_->setText(tr("Calculating quality flags"));
        return;
    }

    // end processing cycle
    if (cleanLine.contains(QByteArrayLiteral("Flux averaging period processing time")))
    {
        if (!prodSplit_)
            main_progress_bar->setValue(++progressValue_);

        mini_progress_bar_->reset();
        mini_progress_bar_->setValue(0);
        resetFileLabels();
        return;
    }
    // end raw data processing
    if (cleanLine.contains(QByteArrayLiteral("Raw data processing terminated")))
    {
        errorEdit_->append(QStringLiteral("Raw data processing terminated"));

        return;
    }
    // get essential file path
    if (cleanLine.contains(QByteArrayLiteral("Essentials file path:")))
    {
        auto ex_file_path_ = QString();
        QByteArrayList path_components = cleanLine.trimmed().split(':');

        // one semicolon only (mac)
        if (path_components.size() < 3)
        {
            ex_file_path_ = QLatin1String(path_components.last().trimmed().constData());
        }
        // two semicolons (windows)
        else
        {
            // preserve the drive letter
            auto full_path_ = path_components.mid(1).join(':');
            ex_file_path_ = QLatin1String(full_path_.trimmed().constData());
        }
        ecProject_->setSpectraExFile(ex_file_path_);
        return;
    }

    // start flux computation
    if (cleanLine.contains(QByteArrayLiteral("Starting flux computation and correction")))
    {
        progressLabel_->setText(tr("Starting flux computation and correction..."));
        resetProgressSoft();
        main_progress_bar->setValue(++progressValue_);
        fccFluxDays_ = false;
        prodSplit_ = false;
        prodPwb_ = false;
        prodPhase_ = 0;
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Initializing retrieval of EddyFlow-RP results")))
    {
        main_progress_bar->setValue(++progressValue_);
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("File found, importing content")))
    {
        main_progress_bar->setValue(++progressValue_);
        return;
    }
    // end flux computation

    // start spectral corrections
    if (cleanLine.contains(QByteArrayLiteral("binned (co)spectra from files")))
    {
        // "  Importing, sorting and ensemble-averaging up to 24 binned
        //  (co)spectra from files.. "
        progressLabel_->setText(tr("Performing spectral assessment..."));
        const auto words = cleanLine.trimmed().split(' ');
        const auto upTo = words.indexOf(QByteArrayLiteral("to"));
        resetProgressSoft();
        main_progress_bar->setMaximum(qMax(1, words.value(upTo + 1).toInt()));
        prodBaseValue_ = 0;
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Importing binned (co)spectra for")))
    {
        avgPeriodLabel_->setText(QLatin1String(cleanLine.trimmed().constData()));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Fitting model")))
    {
        main_progress_bar->setValue(++progressValue_);
        avgPeriodLabel_->setText(QLatin1String(cleanLine.trimmed().constData()));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Sorting")))
    {
        main_progress_bar->setValue(++progressValue_);
        avgPeriodLabel_->setText(QLatin1String(cleanLine.trimmed().constData()));
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Spectral Assessment session terminated")))
    {
        main_progress_bar->setValue(main_progress_bar->maximum());
        return;
    }
    // FCC's flux loop says "  Calculating fluxes for 13 May 2019" once a
    // day. This matched "Calculating fluxes for:", which the engine never
    // writes, so the bar stood still for the whole flux computation. It counts
    // the days of the project's range when one is set; without one the length
    // is unknown here and the bar shows it is busy.
    if (cleanLine.contains(QByteArrayLiteral("Calculating fluxes for ")))
    {
        if (!fccFluxDays_)
        {
            fccFluxDays_ = true;
            resetProgressSoft();
            int days = 0;
            if (ecProject_->generalSubset())
            {
                const QDate dStart(QDate::fromString(ecProject_->generalStartDate(), Qt::ISODate));
                const QDate dEnd(QDate::fromString(ecProject_->generalEndDate(), Qt::ISODate));
                if (dStart.isValid() && dEnd.isValid() && dStart <= dEnd)
                    days = static_cast<int>(dStart.daysTo(dEnd)) + 1;
            }
            main_progress_bar->setMaximum(days);
        }
        if (main_progress_bar->maximum() > 0)
            main_progress_bar->setValue(qMin(++progressValue_, main_progress_bar->maximum()));
        avgPeriodLabel_->setText(QLatin1String(cleanLine.trimmed().constData()));
        return;
    }
    // start spectral corrections

    // start finalizing output files
    if (cleanLine.contains(QByteArrayLiteral("Raw data processing terminated. "
                                      "Creating continuous datasets if necessary")))
    {
//        resetTimeEstimateLabels();
        progressLabel_->setText(tr("Finalizing output files..."));
        resetProgressSoft();
        main_progress_bar->setValue(++progressValue_);

        int maxSteps = 0;

        if (ecProject_->generalRunMode() == Defs::CurrRunMode::Express)
        {
            maxSteps = 17;
        }
        else if (ecProject_->generalRunMode() == Defs::CurrRunMode::Advanced)
        {
            maxSteps = 2 + 2 * (ecProject_->screenOutSt1()
                                + ecProject_->screenOutSt2()
                                + ecProject_->screenOutSt3()
                                + ecProject_->screenOutSt4()
                                + ecProject_->screenOutSt5()
                                + ecProject_->screenOutSt6()
                                + ecProject_->screenOutSt7()
                                + ecProject_->screenOutRaw1()
                                + ecProject_->screenOutRaw2()
                                + ecProject_->screenOutRaw3()
                                + ecProject_->screenOutRaw4()
                                + ecProject_->screenOutRaw5()
                                + ecProject_->screenOutRaw6()
                                + ecProject_->screenOutRaw7()
                                )
                        + ecProject_->generalOutMd()
                        + ecProject_->screenOutDetails();
        }
        main_progress_bar->setMaximum(maxSteps);

#ifdef QT_DEBUG
        out << "Raw data processing terminated";
        Qt::endl(out);
        elapsedTimeMSecStr = QTime(0, 0).addMSecs(static_cast<int>(overall_progress_timer_.elapsed()))
                                    .toString(QStringLiteral("hh:mm:ss.zzz"));
        out << averagingPeriodIndex << " "
            << "x "
            << "x "
            << overall_progress_timer_.elapsed() << " "
            << elapsedTimeMSecStr << " "
            << "x "
            << "x ";
        Qt::endl(out);
#endif
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Creating Full Output dataset")))
    {
        main_progress_bar->setValue(++progressValue_);
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Creating GHG-EUROPE-style dataset")))
    {
        main_progress_bar->setValue(++progressValue_);
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Creating Metadata dataset")))
    {
        main_progress_bar->setValue(++progressValue_);
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Creating Level")))
    {
        main_progress_bar->setValue(++progressValue_);
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Creating Biomet dataset")))
    {
        main_progress_bar->setValue(++progressValue_);
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Closing COMMON output files")))
    {
        main_progress_bar->setValue(++progressValue_);
        return;
    }
    if (cleanLine.contains(QByteArrayLiteral("Closing RP output files")))
    {
        main_progress_bar->setValue(++progressValue_);

#ifdef QT_DEBUG
        out << "Closing RP output files";
        Qt::endl(out);
        elapsedTimeMSecStr = QTime(0, 0).addMSecs(static_cast<int>(overall_progress_timer_.elapsed()))
                                    .toString(QStringLiteral("hh:mm:ss.zzz"));
        out << averagingPeriodIndex << " "
            << "x "
            << "x "
            << overall_progress_timer_.elapsed() << " "
            << elapsedTimeMSecStr << " "
            << "x "
            << "x ";
        Qt::endl(out);
#endif
        return;
    }
    // end finalizing output files

    // engine run possible endings
    if (cleanLine.contains(QByteArrayLiteral("gracefully")))
    {
        main_progress_bar->setValue(main_progress_bar->maximum());
        progressWidget_->stopAnimation();
        averagingPeriodIndex = 0;
        eta_.engineFinished(eta_clock_.elapsed());
        updateTimeEstimateLabels();
        total_elapsed_update_timer_->stop();
        return;
    }

    if (cleanLine.contains(QByteArrayLiteral("At line"))
        || cleanLine.contains(QByteArrayLiteral("Fortran runtime error")))
    {
        cleanLine.prepend("Critical> ");
    }

    if (cleanLine.contains(QByteArrayLiteral("Alert"))
        || cleanLine.contains(QByteArrayLiteral("Warning"))
        || cleanLine.contains(QByteArrayLiteral("Error"))
        // runtime errors
        || cleanLine.contains(QByteArrayLiteral("Critical"))
        || cleanLine.contains(QByteArrayLiteral("Fatal error")))
    {
        // color the labels
        auto tag = cleanLine.mid(0, cleanLine.indexOf('>') + 1);

        auto htmlTag = tag;
        htmlTag.append(QByteArrayLiteral("</font>"));

        if (cleanLine.contains(QByteArrayLiteral("Alert"))
            && cleanLine.contains(QByteArrayLiteral("Error")))
        {
            // blue
            cleanLine.replace(tag, htmlTag.prepend(QByteArrayLiteral("<font color=\"#0066FF\">")));

            goto SKIP;
        }
        if (cleanLine.contains(QByteArrayLiteral("Alert")))
        {
            // blue
            cleanLine.replace(tag, htmlTag.prepend(QByteArrayLiteral("<font color=\"#0066FF\">")));
        }
        if (cleanLine.contains(QByteArrayLiteral("Warning")))
        {
            if (cleanLine.contains(QByteArrayLiteral("(80)"))
                || cleanLine.contains(QByteArrayLiteral("(81)"))
                || cleanLine.contains(QByteArrayLiteral("(82)"))
                || cleanLine.contains(QByteArrayLiteral("(83)"))
                || cleanLine.contains(QByteArrayLiteral("(84)"))
                || cleanLine.contains(QByteArrayLiteral("(85)")))
            {
                cleanLine.clear();
            }

            // yellow
            cleanLine.replace(tag, htmlTag.prepend(QByteArrayLiteral("<font color=\"#FFFF00\">")));
        }
        if (cleanLine.contains(QByteArrayLiteral("Error")))
        {
            // orange
            cleanLine.replace(tag, htmlTag.prepend(QByteArrayLiteral("<font color=\"#FF9900\">")));
        }
        if (cleanLine.contains(QByteArrayLiteral("Critical"))
            || cleanLine.contains(QByteArrayLiteral("Fatal error"))
            || cleanLine.contains(QByteArrayLiteral("At line"))
            || cleanLine.contains(QByteArrayLiteral("Fortran runtime error")))
        {
            // red
            cleanLine.replace(tag, htmlTag.prepend(QByteArrayLiteral("<font color=\"#FF3300\">")));
        }
SKIP:
        QString clearedStr = QLatin1String(cleanLine.trimmed().constData());
        if (!clearedStr.isEmpty())
        {
            errorEdit_->append(clearedStr);
        }

        if (cleanLine.contains(QByteArrayLiteral("Fatal"))
            || cleanLine.contains(QByteArrayLiteral("aborted")))
        {
            stopRun();
            averagingPeriodIndex = 0;
        }

        return;
    }
}

void RunPage::runModeIconClicked()
{
    emit pauseRequest(runMode_);
}

void RunPage::updateSmartfluxBar()
{
    smartfluxBar_->setVisible(configState_->project.smartfluxMode);
}

void RunPage::updateElapsedTime()
{
    updateTimeEstimateLabels();
}

// Elapsed time, and the time left until the whole run is done - counting
// down between the engine's progress lines - with the pass under way.
void RunPage::updateTimeEstimateLabels()
{
    const qint64 elapsed = overall_progress_timer_.isValid()
                           ? overall_progress_timer_.elapsed() : 0;
    const qint64 left = eta_.remainingMs(eta_clock_.elapsed());
    QString text = tr("Total elapsed time: %1 - Estimated time to completion: %2")
                       .arg(RunEta::formatDuration(elapsed), RunEta::formatDuration(left));
    const QString stage = eta_.currentStageName();
    if (!stage.isEmpty())
        text += QStringLiteral(" (%1)").arg(stage);
    timeEstimateLabels_->setText(text);
}

// The bar of a split pass, from the share of the pass done across every
// process; never back. With PWB time lags a split production pass reads its
// pieces twice - for evidence (pd), then for fluxes - and the first round,
// the cheaper, takes the first 40 % of the bar.
void RunPage::setSplitProgress(const QString& tag, qint64 done, qint64 total)
{
    if (total <= 0)
        return;
    // FCC's flux bar is busy when the run has no date range to size it by.
    if (main_progress_bar->maximum() <= 0)
    {
        main_progress_bar->setMaximum(static_cast<int>(qMin<qint64>(total, 1000000)));
        prodBaseValue_ = 0;
    }
    double fraction = static_cast<double>(done) / total;
    if (tag == QLatin1String("pd"))
        fraction *= 0.4;
    else if (prodPwb_ && tag == QLatin1String("rp"))
        fraction = 0.4 + 0.6 * fraction;
    const int span = main_progress_bar->maximum() - prodBaseValue_;
    const int value = qMin(main_progress_bar->maximum(),
                           prodBaseValue_ + static_cast<int>(fraction * span));
    if (value > progressValue_)
    {
        progressValue_ = value;
        main_progress_bar->setValue(progressValue_);
    }
}

// Update mini progress bar every second, scaling the speed of progress in respect of the
// standard 30 minutes. The progression is arbitrary.
void RunPage::updateMiniProgress()
{
    if (inPlanarFit_ or inTimeLag_)
    {
        // prevent division by zero in case of 'File as is' averaging period
        auto avrgPeriod = ecProject_->screenAvrgLen();
        if (avrgPeriod == 0) ++avrgPeriod;

        // factor to scale speed of progression in respect of the standard 30 minutes
        int progressFactor = static_cast<int>(lround(7.0 * 30.0 / avrgPeriod));

        WidgetUtils::setProgressValue(mini_progress_bar_,
                                      mini_progress_bar_->value() + progressFactor);
    }
}

void RunPage::openOutputDir()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(ecProject_->generalOutPath()));
}

void RunPage::updateRunPage(bool small)
{
    Q_UNUSED(small)
}

