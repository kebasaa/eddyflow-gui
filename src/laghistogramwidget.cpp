/***************************************************************************
  laghistogramwidget.cpp
  ----------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#include "laghistogramwidget.h"

#include <QFontMetrics>
#include <QPainter>
#include <QTimeZone>

#include <algorithm>
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

//> A round step for a time axis spanning `seconds`, about `ticks` of them.
qint64 niceTimeStep(qint64 seconds, int ticks)
{
    static const qint64 steps[] = { 1800, 3600, 2 * 3600, 3 * 3600, 6 * 3600, 12 * 3600,
                                    86400, 2 * 86400, 7 * 86400, 14 * 86400, 28 * 86400,
                                    91 * 86400, 182 * 86400, 365 * 86400 };
    const qint64 raw = seconds / std::max(1, ticks);
    for (qint64 s : steps)
    {
        if (s >= raw) { return s; }
    }
    return 365 * 86400;
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
    mode_ = Mode::Histogram;
    message_.clear();
    update();
}

void LagHistogramWidget::setTimeSeries(const QVector<QDateTime>& times, const QVector<double>& lags,
                                       const QVector<bool>& atEdge, double windowMin, double windowMax)
{
    times_ = times;
    lags_ = lags;
    atEdge_ = atEdge;
    windowMin_ = windowMin;
    windowMax_ = windowMax;
    mode_ = Mode::TimeSeries;
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
    times_.clear();
    lags_.clear();
    atEdge_.clear();
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

    const bool empty = mode_ == Mode::Histogram ? histogram_.counts.isEmpty() : lags_.isEmpty();
    if (empty)
    {
        p.setPen(pal.color(QPalette::PlaceholderText));
        p.drawText(rect().adjusted(12, 12, -12, -12), Qt::AlignCenter | Qt::TextWordWrap,
                   message_.isEmpty() ? tr("Run the assessment to see where the time lags fall.")
                                      : message_);
        return;
    }

    // Room for the title above the plot.
    int head = 8;
    if (!title_.isEmpty())
    {
        QFont bold = font();
        bold.setBold(true);
        p.setFont(bold);
        p.setPen(pal.color(QPalette::Text));
        p.drawText(QRectF(0, 4, width(), QFontMetrics(bold).height()), Qt::AlignHCenter, title_);
        p.setFont(font());
        head += QFontMetrics(bold).height() + 4;
    }

    if (mode_ == Mode::Histogram)
    {
        paintHistogram(p, head);
    }
    else
    {
        paintTimeSeries(p, head);
    }
}

void LagHistogramWidget::paintHistogram(QPainter& p, int head)
{
    const QPalette pal = palette();
    const QFontMetrics fm(font());
    const QColor ink = pal.color(QPalette::Text);
    QColor faint = ink;
    faint.setAlphaF(0.18);

    const int nBins = histogram_.counts.size();
    const int top = std::max(1, histogram_.maxStack());
    const double x0 = histogram_.start;
    const double x1 = histogram_.start + nBins * histogram_.width;

    // Plot area, leaving room for the axes' labels.
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
    const QColor edge = pal.color(QPalette::Mid);
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

void LagHistogramWidget::paintTimeSeries(QPainter& p, int head)
{
    const QPalette pal = palette();
    const QFontMetrics fm(font());
    const QColor ink = pal.color(QPalette::Text);
    QColor faint = ink;
    faint.setAlphaF(0.18);

    // The lag axis spans the window searched, so a lag's height says where
    // in the window it fell; the lags themselves, if no window is known.
    const auto [lo, hi] = std::minmax_element(lags_.cbegin(), lags_.cend());
    double y0 = *lo;
    double y1 = *hi;
    if (std::isfinite(windowMin_) && std::isfinite(windowMax_) && windowMax_ > windowMin_)
    {
        y0 = std::min(y0, windowMin_);
        y1 = std::max(y1, windowMax_);
    }
    if (y1 - y0 < 1.0) { y0 -= 0.5; y1 += 0.5; }

    qint64 t0 = std::numeric_limits<qint64>::max();
    qint64 t1 = std::numeric_limits<qint64>::min();
    for (const auto& t : times_)
    {
        if (!t.isValid()) { continue; }
        t0 = std::min(t0, t.toSecsSinceEpoch());
        t1 = std::max(t1, t.toSecsSinceEpoch());
    }
    if (t0 > t1) { return; }
    // Half an hour either side, so the first and last points sit inside.
    t0 -= 1800;
    t1 += 1800;

    const double yStep = niceStep(y1 - y0, 5);
    const double yFirst = std::ceil(y0 / yStep) * yStep;
    int labelWidth = 0;
    for (double y = yFirst; y <= y1 + 1e-9; y += yStep)
    {
        labelWidth = std::max(labelWidth, fm.horizontalAdvance(QString::number(y, 'g', 4)));
    }
    // Room on the left for the lag axis's own label, written upwards.
    const int left = labelWidth + 14 + fm.height();
    const int bottom = fm.height() * 2 + 8;
    const QRectF plot(left, head, width() - left - 12, height() - bottom - head);
    if (plot.width() < 20 || plot.height() < 20) { return; }

    p.save();
    p.setPen(ink);
    p.translate(2, plot.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-plot.height() / 2.0, 0, plot.height(), fm.height()), Qt::AlignHCenter,
               tr("Detected time lag [s]"));
    p.restore();

    auto xPix = [&](qint64 t) { return plot.left() + double(t - t0) / double(t1 - t0) * plot.width(); };
    auto yPix = [&](double y) { return plot.bottom() - (y - y0) / (y1 - y0) * plot.height(); };

    // Grid and lag axis.
    for (double y = yFirst; y <= y1 + 1e-9; y += yStep)
    {
        p.setPen(QPen(faint, 1.0));
        p.drawLine(QPointF(plot.left(), yPix(y)), QPointF(plot.right(), yPix(y)));
        p.setPen(ink);
        p.drawText(QRectF(fm.height(), yPix(y) - fm.height() / 2.0, left - 6 - fm.height(), fm.height()),
                   Qt::AlignRight | Qt::AlignVCenter, QString::number(y, 'g', 4));
    }

    // Suggested window and nominal lag, across the whole time span. The
    // labels sit side by side at the right end - the three lines are often
    // a fraction of a second apart - each just above its own line.
    const int slot = fm.horizontalAdvance(tr("nominal")) + 10;
    auto marker = [&](double y, Qt::PenStyle style, const QString& text, int position) {
        if (!std::isfinite(y) || y < y0 || y > y1) { return; }
        p.setPen(QPen(ink, 1.5, style));
        p.drawLine(QPointF(plot.left(), yPix(y)), QPointF(plot.right(), yPix(y)));
        const double right = plot.right() - 4 - position * slot;
        p.drawText(QRectF(right - slot + 6, yPix(y) - fm.height(), slot - 6, fm.height()),
                   Qt::AlignRight, text);
    };
    marker(nominal_, Qt::SolidLine, tr("nominal"), 0);
    marker(maximum_, Qt::DashLine, tr("max"), 1);
    marker(minimum_, Qt::DashLine, tr("min"), 2);

    // The lags: one dot per half-hour, edge lags grey like the bars.
    const QColor dot = pal.color(QPalette::Highlight);
    const QColor edge = pal.color(QPalette::Mid);
    const double r = std::clamp(plot.width() / std::max<qsizetype>(1, lags_.size()) / 2.0, 1.5, 3.0);
    p.setPen(Qt::NoPen);
    for (int i = 0; i < lags_.size(); ++i)
    {
        const auto& t = times_.value(i);
        if (!t.isValid()) { continue; }
        p.setBrush(atEdge_.value(i) ? edge : dot);
        p.drawEllipse(QPointF(xPix(t.toSecsSinceEpoch()), yPix(lags_.at(i))), r, r);
    }
    p.setBrush(Qt::NoBrush);

    // Time axis: dates, with the time of day only when the span is short.
    p.setPen(ink);
    p.drawLine(plot.bottomLeft(), plot.bottomRight());
    const qint64 step = niceTimeStep(t1 - t0, std::max(2, static_cast<int>(plot.width() / 110)));
    const bool showTime = step < 86400;
    const QString format = showTime ? QStringLiteral("MM-dd HH:mm") : QStringLiteral("yyyy-MM-dd");
    for (qint64 t = (t0 / step + 1) * step; t < t1; t += step)
    {
        const double px = xPix(t);
        p.drawLine(QPointF(px, plot.bottom()), QPointF(px, plot.bottom() + 4));
        const QString label = QDateTime::fromSecsSinceEpoch(t, QTimeZone::UTC).toString(format);
        //> Left out rather than cut off at either end.
        const double half = fm.horizontalAdvance(label) / 2.0 + 1;
        if (px - half < 0 || px + half > width()) { continue; }
        p.drawText(QRectF(px - half, plot.bottom() + 4, 2 * half, fm.height()), Qt::AlignHCenter, label);
    }
    p.drawText(QRectF(plot.left(), plot.bottom() + fm.height() + 6, plot.width(), fm.height()),
               Qt::AlignHCenter, tr("End of averaging period"));
}
