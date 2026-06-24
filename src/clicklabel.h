/***************************************************************************
  clicklabel.h
  -------------------
  QLabel class with clicked signal
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

#ifndef CLICKLABEL_H
#define CLICKLABEL_H

#include <QElapsedTimer>
#include <QLabel>
#include <QMetaType>

////////////////////////////////////////////////////////////////////////////////
/// \file src/clicklabel.h
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

class QMouseEvent;

/// \class ClickLabel
/// \brief QLabel class with clicked signal, derived from QLabel
class ClickLabel : public QLabel
{
    Q_OBJECT

    // property system for question mark clickalabel in metadata editor tables
    Q_PROPERTY(HeaderData headerData READ headerData WRITE setHeaderData)
    Q_ENUMS(HeaderData)

public:
    explicit ClickLabel(QWidget* parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    explicit ClickLabel(const QString& text, QWidget* parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    explicit ClickLabel(const ClickLabel& clabel);
    ClickLabel& operator=(const ClickLabel& clabel);
    ~ClickLabel();

    enum HeaderData {
        NoHeader = 0,
        AnemSwVersion,
        AnemNAlign,
        AnemNOffset,
        AnemNSep,
        AnemESep,
        AnemVSep,
        IrgaModel = 10,
        IrgaLPathLength,
        IrgaTPathLength,
        VarIgnoreDesc = 20,
        VarNumericDesc,
        VarDesc,
        VarConv,
        VarMinValue,
        VarMaxValue,
        VarNomTLag,
        VarMinTLag,
        VarMaxTLag
    };

    HeaderData headerData() const { return headerData_; }
    void setHeaderData(HeaderData data);

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QElapsedTimer time;
    HeaderData headerData_;
};

Q_DECLARE_METATYPE(ClickLabel)

#endif // CLICKLABEL_H


