/***************************************************************************
  windfilter_view.cpp
  -------------------
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
#include "windfilter_view.h"

#include <cmath>

#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QFont>
#include <QHelpEvent>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPoint>
#include <QRadialGradient>
#include <QScrollBar>
#include <QToolTip>
#include <QVariant>

// ---------------------------------------------------------------------------
// Coordinate convention helpers
//
// Qt's arc angles: 0° = east, positive = counter-clockwise (CCW).
// Wind compass: 0° = north, positive = clockwise (CW).
//
// To convert compass angle C to Qt arc angle Q:
//   Q = 90° - C
// ---------------------------------------------------------------------------

static double compassToQtAngle(double compassDeg)
{
    return 90.0 - compassDeg;
}

WindFilterView::WindFilterView(QWidget *parent)
    : QAbstractItemView(parent)
    , margin(28)
    , totalSize(470)
    , pieSize(414)
    , fontSize(14)
    , validItems(0)
    , origin(0, 0)
{
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setMinimumSize(totalSize, totalSize);
}

// ---------------------------------------------------------------------------
// Required QAbstractItemView overrides
// ---------------------------------------------------------------------------

QRect WindFilterView::visualRect(const QModelIndex &index) const
{
    return itemRect(index);
}

void WindFilterView::scrollTo(const QModelIndex & /*index*/,
                               ScrollHint /*hint*/)
{
    // The view has no scrolling; this is a no-op.
}

QModelIndex WindFilterView::indexAt(const QPoint &point) const
{
    if (!model())
        return QModelIndex();

    // Translate viewport coordinates to view coordinates
    const QPoint translated = point - origin;

    for (int row = 0; row < model()->rowCount(rootIndex()); ++row) {
        const QModelIndex idx = model()->index(row, 0, rootIndex());
        if (itemRegion(idx).contains(translated))
            return idx;
    }
    return QModelIndex();
}

QSize WindFilterView::sizeHint() const
{
    return QSize(totalSize, totalSize);
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void WindFilterView::updateValidItems()
{
    if (!model())
        return;
    validItems = model()->rowCount(rootIndex());
    viewport()->update();
}

void WindFilterView::dataChanged(const QModelIndex &topLeft,
                                  const QModelIndex &bottomRight,
                                  const QList<int> &roles)
{
    QAbstractItemView::dataChanged(topLeft, bottomRight, roles);
    validItems = model() ? model()->rowCount(rootIndex()) : 0;
    viewport()->update();
}

void WindFilterView::rowsInserted(const QModelIndex &parent, int start, int end)
{
    validItems += end - start + 1;
    QAbstractItemView::rowsInserted(parent, start, end);
}

void WindFilterView::rowsAboutToBeRemoved(const QModelIndex &parent,
                                           int start, int end)
{
    validItems -= end - start + 1;
    QAbstractItemView::rowsAboutToBeRemoved(parent, start, end);
}

void WindFilterView::updateGeometries()
{
    // No scroll bars to update
}

// ---------------------------------------------------------------------------
// Protected event handlers
// ---------------------------------------------------------------------------

bool WindFilterView::edit(const QModelIndex & /*index*/,
                           EditTrigger /*trigger*/,
                           QEvent * /*event*/)
{
    return false;
}

QModelIndex WindFilterView::moveCursor(
    QAbstractItemView::CursorAction action,
    Qt::KeyboardModifiers /*modifiers*/)
{
    const QModelIndex current = currentIndex();
    if (!model() || !current.isValid())
        return QModelIndex();

    const int nRows = model()->rowCount(rootIndex());
    if (nRows == 0)
        return QModelIndex();

    int row = current.row();

    switch (action) {
        case MoveUp:
        case MoveLeft:
            row = (row > 0) ? row - 1 : nRows - 1;
            break;
        case MoveDown:
        case MoveRight:
            row = (row < nRows - 1) ? row + 1 : 0;
            break;
        default:
            break;
    }

    return model()->index(row, current.column(), rootIndex());
}

int WindFilterView::horizontalOffset() const { return 0; }
int WindFilterView::verticalOffset() const   { return 0; }

bool WindFilterView::isIndexHidden(const QModelIndex & /*index*/) const
{
    return false;
}

void WindFilterView::setSelection(const QRect &rect,
                                   QItemSelectionModel::SelectionFlags flags)
{
    if (!model())
        return;

    QItemSelection selection;
    for (int row = 0; row < model()->rowCount(rootIndex()); ++row) {
        const QModelIndex idx = model()->index(row, 0, rootIndex());
        if (itemRegion(idx).intersects(rect.translated(-origin)))
            selection.select(idx, idx);
    }
    selectionModel()->select(selection, flags);
}

void WindFilterView::mousePressEvent(QMouseEvent *event)
{
    QAbstractItemView::mousePressEvent(event);
    const QModelIndex idx = indexAt(event->pos());
    if (idx.isValid())
        setCurrentIndex(idx);
}

void WindFilterView::mouseMoveEvent(QMouseEvent *event)
{
    QAbstractItemView::mouseMoveEvent(event);
}

void WindFilterView::mouseReleaseEvent(QMouseEvent *event)
{
    QAbstractItemView::mouseReleaseEvent(event);
    viewport()->update();
}

void WindFilterView::paintEvent(QPaintEvent * /*event*/)
{
    if (!model() || validItems == 0) {
        QPainter p(viewport());
        p.fillRect(viewport()->rect(), palette().window());
        return;
    }

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(viewport()->rect(), palette().window());

    // Pie chart geometry
    const int half = pieSize / 2;
    const QPoint centre(totalSize / 2, totalSize / 2);
    const QRectF pieRect(centre.x() - half, centre.y() - half,
                          pieSize, pieSize);

    // Draw background circle
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(240, 240, 240));
    painter.drawEllipse(pieRect);

    // Draw each sector
    for (int row = 0; row < model()->rowCount(rootIndex()); ++row) {
        const QModelIndex idx = model()->index(row, 0, rootIndex());

        const double startCompass = model()->data(idx, Qt::DisplayRole).toDouble();
        const QModelIndex endIdx  = model()->index(row, 1, rootIndex());
        const double endCompass   = model()->data(endIdx, Qt::DisplayRole).toDouble();
        const double spanDeg      = endCompass - startCompass;
        if (spanDeg <= 0.0)
            continue;

        QColor sliceColor = model()->data(idx, Qt::DecorationRole).value<QColor>();
        if (!sliceColor.isValid())
            sliceColor = QColor(170, 230, 255);

        const bool selected = selectionModel() &&
                              selectionModel()->isSelected(idx);

        // Build a radial gradient for the slice
        QRadialGradient grad(centre, half);
        grad.setColorAt(0.0, selected ? sliceColor.lighter(130) : sliceColor.lighter(115));
        grad.setColorAt(1.0, selected ? sliceColor.darker(120)  : sliceColor);

        // Convert compass angles to Qt arc angles
        const double qtStart = compassToQtAngle(startCompass);
        const double qtSpan  = -spanDeg; // negative = CW in Qt

        QPainterPath path;
        path.moveTo(centre);
        path.arcTo(pieRect, qtStart, qtSpan);
        path.closeSubpath();

        painter.setBrush(grad);
        painter.setPen(selected ? QPen(Qt::darkBlue, 2) : QPen(Qt::white, 1));
        painter.drawPath(path);
    }

    // Cardinal direction labels (N, E, S, W)
    QFont labelFont = painter.font();
    labelFont.setPointSize(fontSize);
    labelFont.setBold(true);
    painter.setFont(labelFont);
    painter.setPen(Qt::black);

    struct { const char *label; int dx; int dy; } cardinals[] = {
        {"N",  0, -1},
        {"E",  1,  0},
        {"S",  0,  1},
        {"W", -1,  0},
    };

    const int labelOffset = half + margin / 2;
    const QFontMetrics fm(labelFont);

    for (const auto &c : cardinals) {
        const QString txt = QLatin1String(c.label);
        const QRect textBound = fm.boundingRect(txt);
        const int x = centre.x() + c.dx * labelOffset - textBound.width() / 2;
        const int y = centre.y() + c.dy * labelOffset + textBound.height() / 4;
        painter.drawText(x, y, txt);
    }

    // Outer ring
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(Qt::gray, 1));
    painter.drawEllipse(pieRect);
}

void WindFilterView::resizeEvent(QResizeEvent *event)
{
    QAbstractItemView::resizeEvent(event);
    updateGeometries();
}

void WindFilterView::scrollContentsBy(int dx, int dy)
{
    viewport()->scroll(dx, dy);
}

QRegion WindFilterView::visualRegionForSelection(
    const QItemSelection &selection) const
{
    QRegion region;
    for (const QItemSelectionRange &range : selection) {
        for (const QModelIndex &idx : range.indexes())
            region += itemRegion(idx);
    }
    return region;
}

bool WindFilterView::viewportEvent(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        auto *helpEvent = static_cast<QHelpEvent *>(event);
        const QModelIndex idx = indexAt(helpEvent->pos());
        if (idx.isValid()) {
            const double start = model()->data(
                model()->index(idx.row(), 0, rootIndex())).toDouble();
            const double end   = model()->data(
                model()->index(idx.row(), 1, rootIndex())).toDouble();
            QToolTip::showText(helpEvent->globalPos(),
                               QStringLiteral("%1° – %2°").arg(start).arg(end));
        } else {
            QToolTip::hideText();
        }
        return true;
    }
    return QAbstractItemView::viewportEvent(event);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

QRect WindFilterView::itemRect(const QModelIndex &index) const
{
    if (!index.isValid())
        return QRect();
    return viewport()->rect();
}

QRegion WindFilterView::itemRegion(const QModelIndex &index) const
{
    if (!model() || !index.isValid())
        return QRegion();

    const double startCompass = model()->data(
        model()->index(index.row(), 0, rootIndex())).toDouble();
    const QModelIndex endIdx  = model()->index(index.row(), 1, rootIndex());
    const double endCompass   = model()->data(endIdx).toDouble();
    const double spanDeg      = endCompass - startCompass;
    if (spanDeg <= 0.0)
        return QRegion();

    const int half = pieSize / 2;
    const QPoint centre(totalSize / 2, totalSize / 2);
    const QRectF pieRect(centre.x() - half, centre.y() - half,
                          pieSize, pieSize);

    const double qtStart = compassToQtAngle(startCompass);
    const double qtSpan  = -spanDeg;

    QPainterPath path;
    path.moveTo(centre);
    path.arcTo(pieRect, qtStart, qtSpan);
    path.closeSubpath();

    return QRegion(path.toFillPolygon().toPolygon());
}

int WindFilterView::rows(const QModelIndex &index) const
{
    if (!model())
        return 0;
    return model()->rowCount(index);
}
