/***************************************************************************
  detectdaterangedialog.h
  -----------------------
  Copyright © 2016-2018, LI-COR Biosciences, Antonio Forgione
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

#ifndef DETECTDATERANGEDIALOG_H
#define DETECTDATERANGEDIALOG_H

#include <QDialog>

#include "fileutils.h"

class QGridLayout;
class QLabel;

class EcProject;
class DlProject;

class DetectDateRangeDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DetectDateRangeDialog(QWidget* parent, EcProject *ecProject, DlProject *dlProject);
    ~DetectDateRangeDialog();

    enum class DateRangeType { RawData, Spectra, PlanarFit, TimeLag };
    enum class DateRangeOverlapping { Overlap, NoOverlap };

//    void setLabel(const QString& label);
    void showDateRange(DateRangeType type);
    void setCurrentRange(FileUtils::DateRange currentRange);

signals:
    void alignDeclinationRequest(const QDate& d);
    void refreshRdRequest();
    void refreshPfRequest();
    void refreshTlRequest();
    void refreshSpRequest();

public slots:

private slots:
    void setToAvailableDates();

private:
    void createDateSelectionWidget(DateRangeType type,
                                   FileUtils::DateRange subrange,
                                   DateRangeOverlapping overlap);
    bool dateRangesOverlap(FileUtils::DateRange availableDataset,
                           const QDate &start_date,
                           const QTime &start_time,
                           const QDate &end_date,
                           const QTime &end_time);
    void updateOverlap(QLabel *label,
                            const QDate &start_date,
                            const QTime &start_time,
                            const QDate &end_date,
                            const QTime &end_time);
    FileUtils::DateRange getBinnedCospectraDateRange();
    void updateSpectraOverlap(QLabel *label,
                            const QDate &start_date,
                            const QTime &start_time,
                            const QDate &end_date,
                            const QTime &end_time);
    void createCurrentRange();
    bool isSpectraSubsetPosssible();

    FileUtils::DateRange availableDataRange_;
    QPushButton *setAsCurrentRangeButton;
    QPushButton *okButton;
//    QLabel *msgLabel;
    QGridLayout *dialogLayout;

    EcProject *ecProject_;
    DlProject *dlProject_;
    bool isRangeOverlapping_;
};

#endif  // DETECTDATERANGEDIALOG_H
