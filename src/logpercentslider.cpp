/***************************************************************************
  logpercentslider.cpp
  --------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#include "logpercentslider.h"

#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyleOptionSlider>

#include <algorithm>
#include <cmath>

namespace {
//> Positions per tenfold step: 0.01 in log10 terms, fine enough that every
//> tenth of a percent below 10 % has a position of its own.
constexpr int StepsPerDecade = 100;
}  // namespace

LogPercentSlider::LogPercentSlider(QWidget* parent) :
    QSlider(Qt::Horizontal, parent)
{
    setRange(0, 3 * StepsPerDecade);
    setSingleStep(1);
    setPageStep(StepsPerDecade / 4);
    setTickPosition(QSlider::NoTicks);
    // Room below the groove for the default's marker.
    setMinimumHeight(sizeHint().height() + 8);
}

int LogPercentSlider::toPosition(double percent)
{
    const double p = std::clamp(percent, MinPercent, MaxPercent);
    return static_cast<int>(std::lround((std::log10(p) + 1.0) * StepsPerDecade));
}

double LogPercentSlider::toPercent(int position)
{
    return std::pow(10.0, static_cast<double>(position) / StepsPerDecade - 1.0);
}

double LogPercentSlider::percent() const
{
    return toPercent(value());
}

void LogPercentSlider::setPercent(double percent)
{
    setValue(toPosition(percent));
}

void LogPercentSlider::setDefaultPercent(double percent)
{
    default_ = std::clamp(percent, MinPercent, MaxPercent);
    setToolTip(tr("<b>Share of the half-hours to assess</b>, on a log scale from "
                   "0.1 % to 100 %. The mark below the slider is the default for "
                   "this dataset, %1 %; double-click to go back to it.")
                   .arg(QString::number(default_, 'g', 3)));
    update();
}

void LogPercentSlider::paintEvent(QPaintEvent* event)
{
    QSlider::paintEvent(event);

    QStyleOptionSlider opt;
    initStyleOption(&opt);
    const QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
    const QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);
    // Where the handle's centre sits at the default value.
    const int span = groove.width() - handle.width();
    const int x = groove.left() + handle.width() / 2
                  + QStyle::sliderPositionFromValue(minimum(), maximum(), toPosition(default_), span,
                                                    opt.upsideDown);
    const int y = std::max(groove.bottom(), handle.bottom()) + 2;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath tri;
    tri.moveTo(x, y);
    tri.lineTo(x - 4, y + 6);
    tri.lineTo(x + 4, y + 6);
    tri.closeSubpath();
    p.fillPath(tri, palette().color(isEnabled() ? QPalette::WindowText : QPalette::Mid));
}

void LogPercentSlider::mouseDoubleClickEvent(QMouseEvent* event)
{
    Q_UNUSED(event)
    if (isEnabled()) { setPercent(default_); }
}
