/***************************************************************************
  laghistogramwidget.h
  --------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#ifndef LAGHISTOGRAMWIDGET_H
#define LAGHISTOGRAMWIDGET_H

#include <QDateTime>
#include <QImage>
#include <QVector>
#include <QWidget>

#include "lagstats.h"

class QPainter;

//> The lags the time-lag preliminary assessment detected for one gas, as
//> either of two charts:
//> - a histogram: lags off the window edges in the accent colour, lags at an
//>   edge stacked on top in a muted one, and the suggested nominal lag and
//>   window as vertical lines;
//> - a time series: one dot per half-hour at its detected lag, edge lags
//>   muted, and the suggestion as horizontal lines - which shows whether the
//>   lag drifts, jumps or depends on the time of day.
//> Painted by hand - QtCharts is not linked, and it is deprecated from Qt 6.11
//> on.
class LagHistogramWidget : public QWidget
{
    Q_OBJECT
public:
    enum class Mode { Histogram, TimeSeries };

    explicit LagHistogramWidget(QWidget* parent = nullptr);

    void setHistogram(const LagStats::Histogram& histogram);
    //> One entry per half-hour; the window the lags were searched in sets
    //> the lag axis.
    void setTimeSeries(const QVector<QDateTime>& times, const QVector<double>& lags,
                       const QVector<bool>& atEdge, double windowMin, double windowMax);
    Mode mode() const { return mode_; }
    //> The suggestion to mark; NaN leaves a marker out.
    void setMarkers(double nominal, double minimum, double maximum);
    void setMessage(const QString& message);
    //> A heading above the plot; empty for none.
    void setTitle(const QString& title);

    //> The chart as a picture of `size`, in light colours whatever the
    //> application's theme, so a saved figure looks the same everywhere.
    QImage renderImage(const QSize& size);

    //> Back to the whole range: on a new gas, a new view or a new run.
    void resetZoom();
    bool isZoomed() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    //> Zoom: drag across the plot to zoom to that stretch (the histogram
    //> along the lag axis, the time series along both axes when a box is
    //> dragged); the wheel zooms around the cursor, Shift+wheel the time
    //> series' lag axis; a double click shows everything again.
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void paintHistogram(QPainter& p, int head);
    void paintTimeSeries(QPainter& p, int head);

    Mode mode_ = Mode::Histogram;
    LagStats::Histogram histogram_;
    QVector<QDateTime> times_;
    QVector<double> lags_;
    QVector<bool> atEdge_;
    double windowMin_ = 0.0;
    double windowMax_ = 0.0;
    double nominal_;
    double minimum_;
    double maximum_;
    QString message_;
    QString title_;

    //> The range shown, in data units (seconds of lag; for the time
    //> series' x axis, seconds since the epoch); NaN for the whole range.
    double zoomX0_;
    double zoomX1_;
    double zoomY0_;
    double zoomY1_;
    //> What the last paint drew, to turn mouse positions into data.
    QRectF plot_;
    double shownX0_ = 0.0, shownX1_ = 1.0, shownY0_ = 0.0, shownY1_ = 1.0;
    double fullX0_ = 0.0, fullX1_ = 1.0, fullY0_ = 0.0, fullY1_ = 1.0;
    //> A drag under way, in widget pixels.
    bool dragging_ = false;
    QPointF dragFrom_;
    QPointF dragTo_;

    double xData(double px) const;
    double yData(double py) const;
    void zoomX(double from, double to);
    void zoomY(double from, double to);
    void paintZoomOverlay(QPainter& p);
};

#endif  // LAGHISTOGRAMWIDGET_H
