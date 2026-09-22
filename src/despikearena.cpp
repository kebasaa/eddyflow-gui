/***************************************************************************
  despikearena.cpp
  -------------------
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

#include "despikearena.h"

#include <QFontDatabase>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace {

const int TICK_MS = 30;
const int HUD_HEIGHT = 32;
const qreal STEP_PX = 3.0;         // horizontal distance between samples
const qreal SCROLL_PX_PER_S = 100.0;
const qreal COUNTDOWN_S = 3.0;
const qreal ROUND_S = 20.0;
const qreal LIFETIME_S = 1.6;      // a spike not shot by then escapes
const qreal RISE_S = 0.25;
const qreal POP_S = 0.6;
const qreal FLASH_S = 0.25;
const qreal HIT_RADIUS = 18.0;
const qreal HEAD_RADIUS = 9.0;
const int START_HEALTH = 100;
const int START_AMMO = 50;
const int ESCAPE_DAMAGE = 10;

qreal uniform(qreal lo, qreal hi)
{
    return lo + (hi - lo) * QRandomGenerator::global()->generateDouble();
}

// spawn interval shrinks from ~1 s to ~0.4 s over the round
qreal spawnInterval(qreal roundTime)
{
    const qreal progress = std::min(roundTime / ROUND_S, 1.0);
    return (1.0 - 0.6 * progress) * uniform(0.7, 1.3);
}

} // namespace

DespikeArena::DespikeArena(QWidget *parent) :
    QWidget(parent),
    state_(State::Idle),
    roundTime_(0.0),
    countdown_(0.0),
    nextSpawn_(0.0),
    flash_(0.0),
    scrollCarry_(0.0),
    walk_(0.0),
    health_(START_HEALTH),
    ammo_(START_AMMO),
    despiked_(0),
    escaped_(0),
    shots_(0)
{
    setMinimumSize(560, 300);
    setCursor(Qt::CrossCursor);
    setMouseTracking(false);

    timer_ = new QTimer(this);
    timer_->setInterval(TICK_MS);
    timer_->setTimerType(Qt::PreciseTimer);
    connect(timer_, &QTimer::timeout, this, &DespikeArena::tick);
}

void DespikeArena::start()
{
    health_ = START_HEALTH;
    ammo_ = START_AMMO;
    despiked_ = 0;
    escaped_ = 0;
    shots_ = 0;
    roundTime_ = 0.0;
    countdown_ = COUNTDOWN_S;
    nextSpawn_ = 0.0;
    flash_ = 0.0;
    scrollCarry_ = 0.0;
    walk_ = 0.0;
    series_.clear();
    spikes_.clear();
    pops_.clear();

    state_ = State::Countdown;
    clock_.start();
    timer_->start();
    update();
}

void DespikeArena::stop()
{
    timer_->stop();
    state_ = State::Idle;
    spikes_.clear();
    pops_.clear();
    update();
}

void DespikeArena::tick()
{
    const qreal dt = std::min(clock_.restart() / 1000.0, 0.1);

    // keep one sample per STEP_PX across the current width
    const auto needed = static_cast<int>(std::ceil(plotRect().width() / STEP_PX)) + 2;
    auto nextValue = [this]()
    {
        walk_ = 0.9 * walk_ + 0.35 * uniform(-1.0, 1.0);
        return walk_ + 0.15 * uniform(-1.0, 1.0);
    };
    while (series_.size() < needed)
        series_.append(nextValue());
    while (series_.size() > needed)
        series_.removeFirst();

    // scroll: whole samples at a time, and the spikes ride along
    scrollCarry_ += SCROLL_PX_PER_S * dt;
    while (scrollCarry_ >= STEP_PX)
    {
        scrollCarry_ -= STEP_PX;
        series_.removeFirst();
        series_.append(nextValue());
        for (auto& spike : spikes_)
            spike.x -= STEP_PX;
    }

    if (state_ == State::Countdown)
    {
        countdown_ -= dt;
        if (countdown_ <= 0.0)
        {
            state_ = State::Running;
            nextSpawn_ = 0.4;
        }
        update();
        return;
    }

    if (state_ != State::Running)
        return;

    roundTime_ += dt;
    flash_ = std::max(flash_ - dt, 0.0);

    nextSpawn_ -= dt;
    if (nextSpawn_ <= 0.0)
    {
        const auto width = plotRect().width();
        spikes_.append({ uniform(0.3, 0.95) * width, uniform(0.25, 0.45), 0.0 });
        nextSpawn_ = spawnInterval(roundTime_);
    }

    for (int i = spikes_.size() - 1; i >= 0; --i)
    {
        auto& spike = spikes_[i];
        spike.age += dt;
        if (spike.age > LIFETIME_S || spike.x < HEAD_RADIUS)
        {
            spikes_.removeAt(i);
            ++escaped_;
            health_ = std::max(health_ - ESCAPE_DAMAGE, 0);
            flash_ = FLASH_S;
        }
    }

    for (int i = pops_.size() - 1; i >= 0; --i)
    {
        pops_[i].age += dt;
        if (pops_[i].age > POP_S)
            pops_.removeAt(i);
    }

    update();

    if (health_ <= 0)
        endRound(true);
    else if (roundTime_ >= ROUND_S)
        endRound(false);
}

void DespikeArena::endRound(bool died)
{
    timer_->stop();
    state_ = State::Over;
    emit gameOver(despiked_, escaped_, shots_, died);
}

void DespikeArena::mousePressEvent(QMouseEvent *event)
{
    if (state_ != State::Running || event->button() != Qt::LeftButton || ammo_ <= 0)
    {
        QWidget::mousePressEvent(event);
        return;
    }

    --ammo_;
    ++shots_;

    const QPointF click = event->position();
    int hit = -1;
    qreal best = HIT_RADIUS;
    for (int i = 0; i < spikes_.size(); ++i)
    {
        const QPointF d = headPos(spikes_.at(i)) - click;
        const qreal distance = std::hypot(d.x(), d.y());
        if (distance <= best)
        {
            best = distance;
            hit = i;
        }
    }

    if (hit >= 0)
    {
        pops_.append({ headPos(spikes_.at(hit)), 0.0 });
        spikes_.removeAt(hit);
        ++despiked_;
    }
    update();
}

QRectF DespikeArena::plotRect() const
{
    return QRectF(0, 0, width(), height() - HUD_HEIGHT);
}

qreal DespikeArena::seriesY(qreal x) const
{
    const auto plot = plotRect();
    if (series_.isEmpty())
        return plot.center().y();
    const auto index = std::clamp(static_cast<int>(x / STEP_PX), 0,
                                  static_cast<int>(series_.size()) - 1);
    return plot.center().y() + 0.18 * plot.height() * series_.at(index);
}

QPointF DespikeArena::headPos(const Spike &spike) const
{
    const auto plot = plotRect();
    const qreal grown = std::min(spike.age / RISE_S, 1.0);
    const qreal y = seriesY(spike.x) - grown * spike.height * plot.height();
    return QPointF(spike.x, std::max(y, plot.top() + HEAD_RADIUS + 6));
}

void DespikeArena::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const auto plot = plotRect();
    p.fillRect(plot, QColor(16, 16, 16));

    p.setPen(QPen(QColor(34, 34, 34), 1));
    for (qreal x = 0; x < plot.width(); x += 40)
        p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
    for (qreal y = plot.top(); y < plot.bottom(); y += 40)
        p.drawLine(QPointF(0, y), QPointF(plot.width(), y));

    if (flash_ > 0.0)
        p.fillRect(plot, QColor(200, 0, 0, static_cast<int>(90 * flash_ / FLASH_S)));

    p.setPen(QColor(120, 120, 120));
    p.drawText(plot.adjusted(8, 6, -8, -6), Qt::AlignLeft | Qt::AlignTop,
               QStringLiteral("w′ (m s⁻¹)"));

    paintSeries(p);
    for (const auto& spike : spikes_)
        paintSpike(p, spike);

    auto popFont = font();
    popFont.setBold(true);
    p.setFont(popFont);
    for (const auto& pop : pops_)
    {
        const int alpha = static_cast<int>(255 * (1.0 - pop.age / POP_S));
        p.setPen(QColor(232, 163, 23, alpha));
        p.drawText(pop.pos + QPointF(-32, -14 - 30 * pop.age), tr("DESPIKED!"));
    }

    if (state_ == State::Countdown)
    {
        auto big = font();
        big.setBold(true);
        big.setPointSize(48);
        p.setFont(big);
        p.setPen(QColor(232, 163, 23));
        p.drawText(plot, Qt::AlignCenter,
                   QString::number(static_cast<int>(std::ceil(countdown_))));
    }

    paintHud(p);
}

void DespikeArena::paintSeries(QPainter &p) const
{
    if (series_.size() < 2)
        return;

    QPainterPath path;
    for (int i = 0; i < series_.size(); ++i)
    {
        const qreal x = i * STEP_PX;
        const QPointF point(x, seriesY(x));
        if (i == 0)
            path.moveTo(point);
        else
            path.lineTo(point);
    }
    p.setPen(QPen(QColor(80, 220, 80), 1.6));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
}

void DespikeArena::paintSpike(QPainter &p, const Spike &spike) const
{
    const QPointF base(spike.x, seriesY(spike.x));
    const QPointF head = headPos(spike);

    p.setPen(QPen(QColor(220, 40, 40), 2.5));
    p.drawLine(base, head);

    // heads throb as they get close to escaping
    qreal r = HEAD_RADIUS;
    if (spike.age > LIFETIME_S * 0.6)
        r *= 1.0 + 0.18 * std::sin(spike.age * 25.0);

    QPainterPath horns;
    for (int side : { -1, 1 })
    {
        horns.moveTo(head + QPointF(side * r * 0.45, -r * 0.7));
        horns.lineTo(head + QPointF(side * r * 1.15, -r * 1.8));
        horns.lineTo(head + QPointF(side * r * 0.85, -r * 0.35));
        horns.closeSubpath();
    }
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(235, 220, 195));
    p.drawPath(horns);

    p.setBrush(QColor(190, 20, 20));
    p.drawEllipse(head, r, r);

    p.setBrush(QColor(255, 220, 0));
    p.drawEllipse(head + QPointF(-r * 0.38, -r * 0.1), r * 0.2, r * 0.2);
    p.drawEllipse(head + QPointF(r * 0.38, -r * 0.1), r * 0.2, r * 0.2);
}

void DespikeArena::paintHud(QPainter &p) const
{
    const QRectF hud(0, height() - HUD_HEIGHT, width(), HUD_HEIGHT);
    p.fillRect(hud, QColor(45, 40, 35));
    p.setPen(QPen(QColor(90, 80, 70), 1));
    p.drawLine(hud.topLeft(), hud.topRight());

    auto hudFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    hudFont.setBold(true);
    hudFont.setPointSize(11);
    p.setFont(hudFont);

    const int remaining = static_cast<int>(std::ceil(std::max(ROUND_S - roundTime_, 0.0)));
    const QString ammo = ammo_ > 0 ? tr("AMMO %1").arg(ammo_)
                                   : tr("OUT OF AMMO: try a median filter");

    p.setPen(health_ <= 30 ? QColor(230, 50, 40) : QColor(232, 163, 23));
    p.drawText(hud.adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft,
               tr("HEALTH %1%").arg(health_));

    p.setPen(QColor(232, 163, 23));
    p.drawText(hud.adjusted(10, 0, -10, 0), Qt::AlignCenter,
               tr("%1   DESPIKED %2").arg(ammo).arg(despiked_));
    p.drawText(hud.adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignRight,
               QStringLiteral("0:%1").arg(remaining, 2, 10, QLatin1Char('0')));
}
