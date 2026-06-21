/***************************************************************************
  anem_view.cpp
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

#include "anem_view.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QMenu>

#include "anem_model.h"

// Constructor
AnemView::AnemView(QWidget *parent) :
    AnemTableView(parent),
    addAction_(nullptr),
    removeAction_(nullptr),
    clearAction_(nullptr)
{
    // create context menu actions
    addAction_ = new QAction(tr("&Add Anemometer"), this);
    addAction_->setShortcut(QKeySequence(QKeySequence::ZoomIn));
    addAction_->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    removeAction_ = new QAction(tr("&Remove Anemometer"), this);
    removeAction_->setShortcut(QKeySequence(QKeySequence::ZoomOut));
    removeAction_->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    clearAction_ = new QAction(tr("&Clear Selection"), this);
    clearAction_->setShortcut(QKeySequence(tr("Escape")));
    clearAction_->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    connect(addAction_, &QAction::triggered, this, &AnemView::addAnem);
    connect(removeAction_, &QAction::triggered, this, &AnemView::removeAnem);
    connect(clearAction_, &QAction::triggered, this, &AnemView::clearSelection);

    addAction(addAction_);
    addAction(removeAction_);
    addAction(clearAction_);
}

AnemView::~AnemView()
{
}

// Create and show context menu
void AnemView::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    menu.addAction(addAction_);
    menu.addAction(removeAction_);
    menu.addSeparator();
    menu.addAction(clearAction_);
    menu.exec(event->globalPos());
}

// Add a new blank anemometer after the selected or after the last if there is
// no selection
void AnemView::addAnem()
{
    int currCol = currentIndex().column();
    int lastCol = anemCount();

    if (currCol == -1)
        currCol = lastCol;
    else
        ++currCol;

    // cast the model(), but it's not stricly necessary because
    // model() already returns the setModel() assigned to the view instance
    AnemModel *concreteModel = static_cast<AnemModel *>(model());

    concreteModel->insertColumns(currCol, 1, QModelIndex());
    updateGeometries();
    concreteModel->flush();
    setCurrentIndex(concreteModel->index(AnemModel::MANUFACTURER, currCol));
}

// Remove the selected anemometer or the last if there is no selection
void AnemView::removeAnem()
{
    int currCol = currentIndex().column();
    int lastCol = anemCount() - 1;

    if (currCol == -1)
        currCol = lastCol;

    static_cast<AnemModel *>(model())->removeColumns(currCol, 1);
    updateGeometries();
    clearSelection();
}

int AnemView::anemCount()
{
    return static_cast<AnemModel *>(model())->columnCount(QModelIndex());
}
