/***************************************************************************
  windfilter_view.h
  -----------------
  Copyright © 2026-    , ETH Zurich, Jonathan Muller

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
#ifndef WINDFILTER_VIEW_H
#define WINDFILTER_VIEW_H

#include <QAbstractItemView>
#include <QPoint>
#include <QRect>
#include <QRegion>
#include <QSize>

class WindFilterView : public QAbstractItemView
{
    Q_OBJECT

public:
    explicit WindFilterView(QWidget *parent = nullptr);

    QRect       visualRect(const QModelIndex &index) const override;
    void        scrollTo(const QModelIndex &index,
                         ScrollHint hint = EnsureVisible) override;
    QModelIndex indexAt(const QPoint &point) const override;
    QSize       sizeHint() const override;

public slots:
    void updateValidItems();

protected slots:
    void dataChanged(const QModelIndex &topLeft,
                     const QModelIndex &bottomRight,
                     const QList<int> &roles = QList<int>()) override;
    void rowsInserted(const QModelIndex &parent, int start, int end) override;
    void rowsAboutToBeRemoved(const QModelIndex &parent,
                               int start, int end) override;
    void updateGeometries() override;

protected:
    bool        edit(const QModelIndex &index, EditTrigger trigger,
                     QEvent *event) override;
    QModelIndex moveCursor(QAbstractItemView::CursorAction cursorAction,
                           Qt::KeyboardModifiers modifiers) override;

    int  horizontalOffset() const override;
    int  verticalOffset() const override;
    bool isIndexHidden(const QModelIndex &index) const override;

    void setSelection(const QRect &rect,
                      QItemSelectionModel::SelectionFlags flags) override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;

    QRegion visualRegionForSelection(const QItemSelection &selection) const override;
    bool    viewportEvent(QEvent *event) override;

private:
    QRect   itemRect(const QModelIndex &index) const;
    QRegion itemRegion(const QModelIndex &index) const;
    int     rows(const QModelIndex &index = QModelIndex()) const;

    int    margin;
    int    totalSize;
    int    pieSize;
    int    fontSize;
    int    validItems;
    QPoint origin;
};

#endif // WINDFILTER_VIEW_H
