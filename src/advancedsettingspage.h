/***************************************************************************
  advancedsettingspage.h
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

#ifndef ADVANCEDSETTINGSPAGE_H
#define ADVANCEDSETTINGSPAGE_H

#include <QWidget>

#include "configstate.h"

class QListWidget;

class AdvSettingsContainer;
class DlProject;
class EcProject;
class SmartFluxBar;

class AdvancedSettingsPage : public QWidget
{
    Q_OBJECT

public:
    AdvancedSettingsPage(QWidget* parent,
                         DlProject* dlProject,
                         EcProject* ecProject,
                         ConfigState* config);
    virtual ~AdvancedSettingsPage();

    AdvSettingsContainer* advancedSettingPages()
                            { return advancedSettingContainer; }
    void updateSmartfluxBar();

private slots:
    void changePage(int index);
    void resetButtonCLicked();

private:
    static const QString PAGE_TITLE_0;
    static const QString PAGE_TITLE_1;
    static const QString PAGE_TITLE_2;
    static const QString PAGE_TITLE_3;
    static const QString PAGE_TITLE_4;

    void createMenu();
    void createIcons();
    bool requestSettingsReset();

    DlProject* dlProject_;
    EcProject* ecProject_;
    ConfigState* configState_;

    SmartFluxBar* smartfluxBar {};
    QListWidget* menuWidget {};
    AdvSettingsContainer*  advancedSettingContainer {};
};

#endif // ADVANCEDSETTINGSPAGE_H
