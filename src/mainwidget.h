/***************************************************************************
  mainwidget.h
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

#ifndef MAINDIALOG_H
#define MAINDIALOG_H

#include <QPointer>
#include <QWidget>

#include "defs.h"
#include "faderwidget.h"

////////////////////////////////////////////////////////////////////////////////
/// \file src/mainwidget.h
/// \brief
/// \version
/// \date
/// \author      Antonio Forgione
/// \note
/// \sa
/// \bug
/// \deprecated
/// \test
/// \todo
////////////////////////////////////////////////////////////////////////////////

class QStackedLayout;

class AdvSpectralOptions;
class BasicSettingsPage;
class DlProject;
class EcProject;
class FullProjectPage;
class PlanarFitSettingsDialog;
class ProjectPage;
class RunPage;
class TimeLagSettingsDialog;
class WelcomePage;
struct ConfigState;

#include "advancedsettingspage.h"
#include "advsettingscontainer.h"
#include "advprocessingoptions.h"

/// \class StartDialog
/// \brief Widget representing the page container of the application
class MainWidget : public QWidget
{
    Q_OBJECT

public:
    MainWidget(QWidget *parent, DlProject *dlProject, EcProject *ecProject, ConfigState* configState);
    ~MainWidget();

    inline WelcomePage* welcomePage() { return welcomePage_; }
    inline ProjectPage* projectPage() { return projectPage_; }
    inline BasicSettingsPage* basicPage() { return basicSettingsPage_; }
    inline AdvancedSettingsPage* advancedPage() { return advancedSettingsPage_; }
    inline RunPage* runPage() { return runPage_; }
    inline PlanarFitSettingsDialog* pfDialog() { return advancedSettingsPage_
                                                            ->advancedSettingPages()
                                                            ->processingOptions()
                                                            ->getPlanarFitSettingsDialog(); }
    inline TimeLagSettingsDialog* tlDialog() { return advancedSettingsPage_
                                                            ->advancedSettingPages()
                                                            ->processingOptions()
                                                            ->getTimeLagSettingsDialog(); }
    inline AdvSpectralOptions* spectralOptions() { return advancedSettingsPage_
                                                            ->advancedSettingPages()
                                                            ->spectralOptions(); }

    Defs::CurrPage currentPage();

    bool smartFluxCloseRequest();

public slots:
    void setCurrentPage(Defs::CurrPage page);
    void updateSmartfluxBarStatus();
//    void routeSmartfluxBarRequests();

private:
    DlProject* dlProject_;
    EcProject* ecProject_;
    ConfigState* configState_;

    QStackedLayout* mainWidgetLayout;

    WelcomePage* welcomePage_;
    ProjectPage* projectPage_;
    BasicSettingsPage* basicSettingsPage_;
    AdvancedSettingsPage* advancedSettingsPage_;
    RunPage* runPage_;

    QPointer<FaderWidget> faderWidget;
    bool fadingOn;

private slots:
    void fadeInWidget(int);

signals:
    void openProjectRequest(QString);
    void newProjectRequest();
    void updateMetadataReadResult(bool);
    void recentUpdated();
    void showSmartfluxBarRequest(bool on);
    void saveSilentlyRequest();
    void saveRequest();
    void fastTemperatureSelected();
    void mdCleanupRequest();
    void showSetPrototypeRequest();
};

#endif // MAINDIALOG_H
