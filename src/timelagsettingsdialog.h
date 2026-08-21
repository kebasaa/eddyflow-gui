/***************************************************************************
  timelagsettingsdialog.h
  -------------------
  Copyright © 2013-2018, LI-COR Biosciences, Antonio Forgione
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

#ifndef TIMELAGSETTINGSDIALOG_H
#define TIMELAGSETTINGSDIALOG_H

#include <QDateTime>
#include <QDialog>
#include <QVector>

#include "fileutils.h"

////////////////////////////////////////////////////////////////////////////////
/// \file src/timelagsettingsdialog.h
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

class QButtonGroup;
class QCheckBox;
class QDate;
class QDateEdit;
class QDoubleSpinBox;
class QGridLayout;
class QLabel;
class QRadioButton;
class QSpinBox;
class QTimeEdit;

class AncillaryFileTest;
class ClickLabel;
struct ConfigState;
class EcProject;
class FileBrowseWidget;

class TimeLagSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TimeLagSettingsDialog(QWidget *parent, EcProject *ecProject, ConfigState* config);
    ~TimeLagSettingsDialog();

    void setSmartfluxUI();
signals:
    void saveRequest();

public slots:
    void close();
    void reset();
    void refresh();
    void partialRefresh();
    void setDateRange(FileUtils::DateRange dates);

private slots:
    void updateTlMode(int radioButton);
    void radioClicked(int radioButton);
    void testSelectedFile(const QString &fp);

    void onStartDateLabelClicked();
    void onEndDateLabelClicked();
    void updateStartDate(const QDate &d);
    void updateEndDate(const QDate &d);
    void updateStartTime(const QTime &t);
    void updateEndTime(const QTime &t);

    void onRhClassClicked();
    void updateRhClass(int n);

    void onLeMinFluxClicked();
    void updateLeMinFlux(double d);
    void onPgRangeLabelClicked();
    void updatePgRange(double d);


    void updateSubsetSelection(bool b);

private:
    void updateFile(const QString& fp);
    void forceEndDatePolicy();
    void forceEndTimePolicy();

    QRadioButton* existingRadio;
    QRadioButton* nonExistingRadio;
    QButtonGroup* radioGroup;
    FileBrowseWidget * fileBrowse;
    QCheckBox* subsetCheckBox;
    ClickLabel* startDateLabel;
    QDateEdit* startDateEdit;
    QTimeEdit* startTimeEdit;
    QLabel* lockedIcon;
    ClickLabel* endDateLabel;
    QDateEdit* endDateEdit;
    QTimeEdit* endTimeEdit;
    ClickLabel* pgRangeLabel;
    QLabel* pgRangeLabel_2;
    QDoubleSpinBox* pgRangeSpin;

    QLabel* h2oTitleLabel;
    ClickLabel* rhClassLabel;
    QSpinBox* rhClassSpin;
    ClickLabel* leMinFluxLabel;
    QDoubleSpinBox* leMinFluxSpin;

    QLabel* gasTitleLabel;
    //> One minimum-flux row per gas, excluding H2O: its counterpart is
    //> leMinFluxSpin, a latent-heat threshold that lives in the H2O section
    //> above and is not a gas flux.
    struct MinFluxRow
    {
        int gasIndex = -1;
        ClickLabel* label = nullptr;
        QDoubleSpinBox* spin = nullptr;
    };
    QVector<MinFluxRow> minFluxRows_;

    QLabel* searchWindowLabel;
    QLabel* minLabel;
    QLabel* maxLabel;
    //> One time-lag search window per configured gas, replacing four fixed
    //> rows: a site may measure the same species on several analysers, each
    //> with its own tube and so its own plausible range.
    struct TlRow
    {
        int gasIndex = -1;              //< index into EcProject::gasColumns()
        ClickLabel* label = nullptr;
        QDoubleSpinBox* minSpin = nullptr;
        QDoubleSpinBox* maxSpin = nullptr;
    };
    QString minSpinTip_;
    QString maxSpinTip_;
    QVector<TlRow> tlRows_;
    QGridLayout* propertiesLayout_ = nullptr;
    //> First grid row of the generated tables; everything below moves
    //> with the number of gases.
    static const int kFirstGasGridRow = 8;

    void rebuildGasRows();
    void onMinFluxChanged(int gasIndex, double value);
    void onTlChanged(int gasIndex, bool isMin, double value);
    void setGasRowsEnabled(bool enabled);
    double minFluxFor(int gasIndex) const;
    double tlMinFor(int gasIndex) const;
    double tlMaxFor(int gasIndex) const;
    QDoubleSpinBox* createTlSpin(bool isMin);

    EcProject *ecProject_;
    ConfigState* configState_;
};

#endif // TIMELAGSETTINGSDIALOG_H
