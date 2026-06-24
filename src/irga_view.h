/***************************************************************************
  irga_view.h
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

#ifndef IRGA_VIEW_H
#define IRGA_VIEW_H

#include "irga_tableview.h"

class QAction;
class QContextMenuEvent;

class IrgaView : public IrgaTableView {
    Q_OBJECT
public:
    explicit IrgaView(QWidget *parent);
    ~IrgaView();

    int irgaCount();

public slots:
    void addIrga();
    void removeIrga();

private:
    void contextMenuEvent(QContextMenuEvent *event);

private:
    QAction *addAction_;
    QAction *removeAction_;
    QAction *clearAction_;
};

#endif // IRGA_VIEW_H
