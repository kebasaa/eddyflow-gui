/***************************************************************************
  laghistogramwidget.h
  --------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#ifndef LAGHISTOGRAMWIDGET_H
#define LAGHISTOGRAMWIDGET_H

#include <QImage>
#include <QWidget>

#include "lagstats.h"

//> Bar chart of the lags the time-lag preliminary assessment detected for one
//> gas: lags off the window edges in the accent colour, lags at an edge
//> stacked on top in a muted one, and the suggested nominal lag and window
//> drawn over them. Painted by hand - QtCharts is not linked, and it is
//> deprecated from Qt 6.11 on.
class LagHistogramWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LagHistogramWidget(QWidget* parent = nullptr);

    void setHistogram(const LagStats::Histogram& histogram);
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
    LagStats::Histogram histogram_;
    double nominal_;
    double minimum_;
    double maximum_;
    QString message_;
    QString title_;
};

#endif  // LAGHISTOGRAMWIDGET_H
