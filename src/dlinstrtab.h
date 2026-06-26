/***************************************************************************
  dlinstrtab.h
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
 ***************************************************************************/

#ifndef DLINSTRTAB_H
#define DLINSTRTAB_H

#include <QWidget>

class QModelIndex;
class QToolButton;

class AnemDelegate;
class AnemModel;
class AnemView;
class DlProject;
class IrgaDelegate;
class IrgaModel;
class IrgaView;

class DlInstrTab : public QWidget {
    Q_OBJECT

public:
    DlInstrTab(QWidget *parent, DlProject *dlProject);
    virtual ~DlInstrTab();

    void reset();
    void refresh();

    AnemView *anemView_;
    IrgaView *irgaView_;

signals:
    void instrumentsModified();

private slots:
    void updateScrollBars();

private:
    AnemModel *anemModel_;
    AnemDelegate *anemDelegate_;

    IrgaModel *irgaModel_;
    IrgaDelegate *irgaDelegate_;

    DlProject *dlProject_;
};

#endif // DLINSTRTAB_H
