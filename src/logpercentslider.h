/***************************************************************************
  logpercentslider.h
  ------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#ifndef LOGPERCENTSLIDER_H
#define LOGPERCENTSLIDER_H

#include <QSlider>

//> A horizontal slider for a share from 0.1 % to 100 %, on a log scale: each
//> tenfold step takes the same travel, so 1 % sits a third of the way along
//> and 10 % two thirds. The default is marked under the groove, and a double
//> click goes back to it.
class LogPercentSlider : public QSlider
{
    Q_OBJECT
public:
    explicit LogPercentSlider(QWidget* parent = nullptr);

    double percent() const;
    void setPercent(double percent);

    double defaultPercent() const { return default_; }
    void setDefaultPercent(double percent);

    static constexpr double MinPercent = 0.1;
    static constexpr double MaxPercent = 100.0;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    static int toPosition(double percent);
    static double toPercent(int position);

    double default_ = 10.0;
};

#endif  // LOGPERCENTSLIDER_H
