/***************************************************************************
  binarysettingsdialog.h
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

#ifndef BINARYSETTINGSDIALOG_H
#define BINARYSETTINGSDIALOG_H

#include <QDialog>

////////////////////////////////////////////////////////////////////////////////
/// \file src/binarysettingsdialog.h
/// \brief
/// \date
/// \author      Antonio Forgione
/// \note
/// \sa
/// \bug
/// \deprecated
/// \test
/// \todo
////////////////////////////////////////////////////////////////////////////////

class QComboBox;
class QSpinBox;

class ClickLabel;
class EcProject;

/// \class BinarySettingsDialog
/// \brief Class representing the Binary settings dialog
class BinarySettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BinarySettingsDialog(QWidget* parent, EcProject *ecProject);
    ~BinarySettingsDialog();

public slots:
    void close();
    void refresh();

private slots:
    void onBinaryHLinesLabelClicked();
    void onBinaryNBytesLabelClicked();
    void onBinaryClickEolLabel();
    void onBinaryClickEndianessLabel();
    void updateBinaryHLine(int n);
    void updateBinaryEol(int n);
    void updateBinaryNBytes(int n);
    void updateBinaryEndianess(int n);

private:
    ClickLabel *binaryHLinesLabel;
    QSpinBox *binaryHLinesSpin;

    ClickLabel *binaryEolLabel;
    QComboBox *binaryEolCombo;

    ClickLabel *binaryNBytesLabel;
    QSpinBox *binaryNBytesSpin;

    ClickLabel *binaryEndianessLabel;
    QComboBox *binaryEndianessCombo;

    EcProject *ecProject_;

signals:
    void saveRequest();
};

#endif // BINARYSETTINGSDIALOG_H
