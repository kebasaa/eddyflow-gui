/***************************************************************************
  advsettingscontainer.h
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

#ifndef ADVSETTINGSCONTAINER_H
#define ADVSETTINGSCONTAINER_H

#include <QWidget>

#include "configstate.h"

class QStackedLayout;

class AdvOutputOptions;
class AdvProcessingOptions;
class AdvSpectralOptions;
class AdvStatisticalOptions;
class DlProject;
class EcProject;

class AdvSettingsContainer : public QWidget
{
    Q_OBJECT

public:
    explicit AdvSettingsContainer(QWidget* parent,
                                  DlProject* dlProject,
                                  EcProject* ecProject,
                                  ConfigState* config);
    ~AdvSettingsContainer();

    AdvProcessingOptions *processingOptions() { return processingOptions_; }
    AdvSpectralOptions *spectralOptions() { return spectralOptions_; }
    AdvStatisticalOptions *statisticalOptions() { return statisticalOptions_; }
    AdvOutputOptions *outputOptions() { return outputOptions_; }

signals:
    void checkMetadataOutputRequest();

public slots:
    void setCurrentPage(int page);

private:
    AdvProcessingOptions* processingOptions_ {};
    AdvStatisticalOptions* statisticalOptions_ {};
    AdvOutputOptions* outputOptions_ {};
    AdvSpectralOptions* spectralOptions_ {};
    QStackedLayout* mainLayout_ {};

    DlProject* dlProject_;
    EcProject* ecProject_;
    ConfigState* configState_;
};

#endif // ADVSETTINGSCONTAINER_H
