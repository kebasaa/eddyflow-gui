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

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

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
};

#endif  // LAGHISTOGRAMWIDGET_H
