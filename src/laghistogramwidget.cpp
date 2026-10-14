/***************************************************************************
  laghistogramwidget.cpp
  ----------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#include "laghistogramwidget.h"

#include <QFontMetrics>
#include <QPainter>

#include <cmath>
#include <limits>

namespace {

//> A round step for an axis spanning `range` in about `ticks` steps.
double niceStep(double range, int ticks)
{
    if (range <= 0.0) { return 1.0; }
    const double raw = range / ticks;
    const double mag = std::pow(10.0, std::floor(std::log10(raw)));
    for (double m : { 1.0, 2.0, 5.0, 10.0 })
    {
        if (m * mag >= raw) { return m * mag; }
    }
    return 10.0 * mag;
}

}  // namespace

LagHistogramWidget::LagHistogramWidget(QWidget* parent) :
    QWidget(parent),
    nominal_(std::numeric_limits<double>::quiet_NaN()),
    minimum_(std::numeric_limits<double>::quiet_NaN()),
    maximum_(std::numeric_limits<double>::quiet_NaN())
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void LagHistogramWidget::setHistogram(const LagStats::Histogram& histogram)
{
    histogram_ = histogram;
    message_.clear();
    update();
}

void LagHistogramWidget::setMarkers(double nominal, double minimum, double maximum)
{
    nominal_ = nominal;
    minimum_ = minimum;
    maximum_ = maximum;
    update();
}

void LagHistogramWidget::setMessage(const QString& message)
{
    message_ = message;
    histogram_ = LagStats::Histogram();
    update();
}

void LagHistogramWidget::setTitle(const QString& title)
{
    title_ = title;
    update();
}

QImage LagHistogramWidget::renderImage(const QSize& size)
{
    const QPalette themed = palette();
    QPalette light = themed;
    light.setColor(QPalette::Base, Qt::white);
    light.setColor(QPalette::Text, QColor(0x20, 0x20, 0x20));
    light.setColor(QPalette::PlaceholderText, QColor(0x70, 0x70, 0x70));
    light.setColor(QPalette::Highlight, QColor(0x2a, 0x6f, 0xb5));
    light.setColor(QPalette::Mid, QColor(0xa8, 0xa8, 0xa8));
    setPalette(light);
    resize(size);

    QImage image(size, QImage::Format_ARGB32);
    image.fill(Qt::white);
    render(&image);
    setPalette(themed);
    return image;
}

QSize LagHistogramWidget::sizeHint() const
{
    return { 560, 260 };
}

QSize LagHistogramWidget::minimumSizeHint() const
{
    return { 320, 180 };
}

void LagHistogramWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QPalette pal = palette();
    p.fillRect(rect(), pal.color(QPalette::Base));

    const QFontMetrics fm(font());
    const QColor ink = pal.color(QPalette::Text);
    QColor faint = ink;
    faint.setAlphaF(0.18);

    if (histogram_.counts.isEmpty())
    {
        p.setPen(pal.color(QPalette::PlaceholderText));
        p.drawText(rect().adjusted(12, 12, -12, -12), Qt::AlignCenter | Qt::TextWordWrap,
                   message_.isEmpty() ? tr("Run the assessment to see where the time lags fall.")
                                      : message_);
        return;
    }

    const int nBins = histogram_.counts.size();
    const int top = std::max(1, histogram_.maxStack());
    const double x0 = histogram_.start;
    const double x1 = histogram_.start + nBins * histogram_.width;

    // Plot area, leaving room for the title and the axes' labels.
    int head = 8;
    if (!title_.isEmpty())
    {
        QFont bold = font();
        bold.setBold(true);
        p.setFont(bold);
        p.setPen(ink);
        p.drawText(QRectF(0, 4, width(), QFontMetrics(bold).height()), Qt::AlignHCenter, title_);
        p.setFont(font());
        head += QFontMetrics(bold).height() + 4;
    }
    const int left = fm.horizontalAdvance(QString::number(top)) + 14;
    const int bottom = fm.height() * 2 + 8;
    const QRectF plot(left, head, width() - left - 12, height() - bottom - head);
    if (plot.width() < 20 || plot.height() < 20) { return; }

    auto xPix = [&](double x) { return plot.left() + (x - x0) / (x1 - x0) * plot.width(); };
    auto yPix = [&](double y) { return plot.bottom() - y / top * plot.height(); };

    // Grid and y axis.
    const double yStep = std::max(1.0, std::round(niceStep(top, 4)));
    p.setPen(QPen(faint, 1.0));
    for (double y = 0.0; y <= top + 1e-9; y += yStep)
    {
        p.drawLine(QPointF(plot.left(), yPix(y)), QPointF(plot.right(), yPix(y)));
        p.setPen(ink);
        p.drawText(QRectF(0, yPix(y) - fm.height() / 2.0, left - 6, fm.height()),
                   Qt::AlignRight | Qt::AlignVCenter, QString::number(static_cast<int>(y)));
        p.setPen(QPen(faint, 1.0));
    }

    // Bars: off-edge lags in the accent colour, edge lags stacked above.
    const QColor bar = pal.color(QPalette::Highlight);
    QColor edge = pal.color(QPalette::Mid);
    const double gap = std::min(2.0, plot.width() / nBins * 0.15);
    for (int i = 0; i < nBins; ++i)
    {
        const double a = xPix(x0 + i * histogram_.width) + gap / 2.0;
        const double b = xPix(x0 + (i + 1) * histogram_.width) - gap / 2.0;
        const int c = histogram_.counts.at(i);
        const int e = histogram_.edgeCounts.at(i);
        if (c > 0) { p.fillRect(QRectF(QPointF(a, yPix(c)), QPointF(b, yPix(0))), bar); }
        if (e > 0) { p.fillRect(QRectF(QPointF(a, yPix(c + e)), QPointF(b, yPix(c))), edge); }
    }

    // x axis and its labels.
    p.setPen(ink);
    p.drawLine(plot.bottomLeft(), plot.bottomRight());
    const double xStep = niceStep(x1 - x0, std::max(2, static_cast<int>(plot.width() / 70)));
    for (double x = std::ceil(x0 / xStep) * xStep; x <= x1 + 1e-9; x += xStep)
    {
        const double px = xPix(x);
        p.drawLine(QPointF(px, plot.bottom()), QPointF(px, plot.bottom() + 4));
        const QString label = QString::number(x, 'g', 4);
        p.drawText(QRectF(px - 40, plot.bottom() + 4, 80, fm.height()), Qt::AlignHCenter, label);
    }
    p.drawText(QRectF(plot.left(), plot.bottom() + fm.height() + 6, plot.width(), fm.height()),
               Qt::AlignHCenter, tr("Detected time lag [s]"));

    // Suggested window and nominal lag.
    //> Each label on a line of its own, so three markers close together -
    //> the usual case, around a sharp peak - do not print over each other;
    //> beside its line, on whichever side has room.
    auto marker = [&](double x, Qt::PenStyle style, const QString& text, int line) {
        if (!std::isfinite(x) || x < x0 || x > x1) { return; }
        p.setPen(QPen(ink, 1.5, style));
        const double px = xPix(x);
        p.drawLine(QPointF(px, plot.top()), QPointF(px, plot.bottom()));
        const double w = fm.horizontalAdvance(text) + 2;
        const double y = plot.top() + line * fm.height();
        const bool right = px + 3 + w <= plot.right();
        p.drawText(QRectF(right ? px + 3 : px - 3 - w, y, w, fm.height()),
                   right ? Qt::AlignLeft : Qt::AlignRight, text);
    };
    marker(nominal_, Qt::SolidLine, tr("nominal"), 0);
    marker(minimum_, Qt::DashLine, tr("min"), 1);
    marker(maximum_, Qt::DashLine, tr("max"), 2);
}
