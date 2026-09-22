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

#include "eastereggwidgets.h"

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
const qreal STEP_PX = 3.0;          // horizontal distance between samples
const qreal COUNTDOWN_S = 3.0;
const qreal ROUND_S = 30.0;
const qreal POP_S = 0.7;
const qreal FLASH_S = 0.25;

// demons: they rise faster and escape sooner as the round goes on
const qreal HIT_RADIUS = 18.0;
const qreal HEAD_RADIUS = 9.0;
const int ESCAPE_DAMAGE = 10;

// ammo: a small magazine, refilled by grabbing downward spikes
const int START_AMMO = 15;
const int MAX_AMMO = 30;
const int LOW_AMMO = 5;
const int AMMO_PICKUP = 10;
const qreal AMMO_LIFETIME_S = 4.0;
const qreal AMMO_GRAB_RADIUS = 18.0;

// valid data: stars on the series that must not be shot
const qreal STAR_RADIUS = 6.0;
const qreal STAR_HIT_RADIUS = 11.0;
const qreal STAR_LIFETIME_S = 4.5;

const int START_HEALTH = 100;
const qreal PI = 3.14159265358979323846;

const QColor AMBER(232, 163, 23);
const QColor DANGER(230, 50, 40);
const QColor AMMO_GREEN(150, 210, 70);
const QColor STAR_BLUE(150, 220, 255);

// what a despiked demon says on the way out
const QStringList& hitPops()
{
    static const QStringList list {
        QStringLiteral("DESPIKED!"),
        QStringLiteral("FLAGGED!"),
        QStringLiteral("MEDIAN'D!"),
        QStringLiteral("OUTLIER DOWN!"),
        QStringLiteral("GOT IT!"),
        QStringLiteral("REMOVED!"),
        QStringLiteral("FILTERED!"),
        QStringLiteral("3.5 SIGMA!"),
        QStringLiteral("SPIKE-FREE!"),
        QStringLiteral("CLEAN!"),
        QStringLiteral("BOOM! DESPIKED!"),
        QStringLiteral("QC PASSED!"),
        QStringLiteral("GONE!"),
        QStringLiteral("NOISE REDUCED!"),
        QStringLiteral("BULLSEYE!"),
        QStringLiteral("OUT OF THE FOOTPRINT!"),
        QStringLiteral("DETRENDED!"),
        QStringLiteral("FLAG 2!"),
        QStringLiteral("HAMPEL'D!"),
        QStringLiteral("EXORCISED!"),
        QStringLiteral("VARIANCE DOWN!"),
        QStringLiteral("SPIKE ELIMINATED!"),
        QStringLiteral("NICE SHOT!"),
        QStringLiteral("REJECTED!"),
        QStringLiteral("SENT TO THE RAW ARCHIVE!"),
        QStringLiteral("NO MORE KURTOSIS!"),
        QStringLiteral("DELETED!"),
        QStringLiteral("CORRECTED!"),
        QStringLiteral("TOAST!"),
        QStringLiteral("HEADSHOT!")
    };
    return list;
}

// shooting a valid-data star; %1 is the penalty
const QStringList& starPops()
{
    static const QStringList list {
        QStringLiteral("VALID DATA! −%1"),
        QStringLiteral("THAT WAS REAL! −%1"),
        QStringLiteral("GOOD DATA! −%1"),
        QStringLiteral("OOPS, AN EDDY! −%1"),
        QStringLiteral("FRIENDLY FIRE! −%1"),
        QStringLiteral("REAL TURBULENCE! −%1"),
        QStringLiteral("NOT A SPIKE! −%1"),
        QStringLiteral("DON'T SHOOT THE STARS! −%1"),
        QStringLiteral("THAT ONE WAS FINE! −%1"),
        QStringLiteral("OVER-FILTERED! −%1"),
        QStringLiteral("TYPE I ERROR! −%1"),
        QStringLiteral("FALSE POSITIVE! −%1"),
        QStringLiteral("GENUINE FLUX! −%1"),
        QStringLiteral("INNOCENT DATA! −%1"),
        QStringLiteral("YOU SHOT A GOOD ONE! −%1"),
        QStringLiteral("REVIEWER 2 SAW THAT! −%1"),
        QStringLiteral("VALID EDDY! −%1"),
        QStringLiteral("HONEST DATA! −%1"),
        QStringLiteral("THAT WAS SIGNAL! −%1"),
        QStringLiteral("SIGNAL, NOT NOISE! −%1"),
        QStringLiteral("CAREFUL! −%1"),
        QStringLiteral("A PERFECTLY GOOD POINT! −%1"),
        QStringLiteral("STATIONARY AND INNOCENT! −%1"),
        QStringLiteral("OVERZEALOUS! −%1"),
        QStringLiteral("QC FAIL! −%1"),
        QStringLiteral("NOOO, NOT THAT ONE! −%1"),
        QStringLiteral("TRUE EDDY LOST! −%1"),
        QStringLiteral("GAP CREATED! −%1"),
        QStringLiteral("THE STAR WAS REAL! −%1"),
        QStringLiteral("DATA LOSS! −%1")
    };
    return list;
}

// grabbing an ammo crate; %1 is the ammo gained
const QStringList& ammoPops()
{
    static const QStringList list {
        QStringLiteral("+%1 AMMO"),
        QStringLiteral("+%1 AMMO: FRESH SPAN GAS!"),
        QStringLiteral("+%1 SHELLS: RELOADED!"),
        QStringLiteral("+%1 AMMO FROM THE HUT!"),
        QStringLiteral("+%1: SPARE BATTERIES!"),
        QStringLiteral("RELOAD! +%1"),
        QStringLiteral("+%1 CALIBRATION ROUNDS!"),
        QStringLiteral("+%1 AMMO, SHAKEN NOT STIRRED!"),
        QStringLiteral("+%1: THE TECHNICIAN DELIVERS!"),
        QStringLiteral("+%1 FROM THE LOGGER BUFFER!"),
        QStringLiteral("+%1: FIRMWARE UPDATED!"),
        QStringLiteral("+%1 MEDIAN FILTERS!"),
        QStringLiteral("+%1 SIGMA THRESHOLDS!"),
        QStringLiteral("+%1 AMMO (BACKORDERED SINCE MARCH)!"),
        QStringLiteral("+%1: FOUND IN THE CABLE BOX!"),
        QStringLiteral("+%1 ROUNDS OF DESPIKING!"),
        QStringLiteral("+%1: POWER RESTORED!"),
        QStringLiteral("+%1 AMMO, JUST IN TIME!"),
        QStringLiteral("+%1: SHIPMENT ARRIVED!"),
        QStringLiteral("+%1 AMMO FROM THE FIELD KIT!"),
        QStringLiteral("RESUPPLY! +%1"),
        QStringLiteral("+%1: THE GRANT CAME THROUGH!"),
        QStringLiteral("+%1 ZERO-GAS CARTRIDGES!"),
        QStringLiteral("+%1: DOWNWARD SPIKE CAUGHT!"),
        QStringLiteral("+%1 AMMO, NEGATIVE FLUX EDITION!"),
        QStringLiteral("+%1 SHELLS, SLIGHTLY DAMP!"),
        QStringLiteral("+%1: EMERGENCY STOCK!"),
        QStringLiteral("+%1 AMMO FROM THE GLOVEBOX!"),
        QStringLiteral("+%1: CAUGHT THE DOWNDRAFT!"),
        QStringLiteral("+%1 AND A BISCUIT!")
    };
    return list;
}

// firing with an empty gun
const QStringList& emptyPops()
{
    static const QStringList list {
        QStringLiteral("*click*"),
        QStringLiteral("*click click*"),
        QStringLiteral("*sad click*"),
        QStringLiteral("*empty*"),
        QStringLiteral("no ammo!"),
        QStringLiteral("*click* (grab a ▼)"),
        QStringLiteral("*dry fire*"),
        QStringLiteral("*nothing*"),
        QStringLiteral("out of shells!"),
        QStringLiteral("*clack*"),
        QStringLiteral("*tick*"),
        QStringLiteral("*click*…"),
        QStringLiteral("buffer empty!"),
        QStringLiteral("*whirr*… nothing"),
        QStringLiteral("error: no ammo"),
        QStringLiteral("*hollow click*"),
        QStringLiteral("reload first!"),
        QStringLiteral("*click* (sigh)"),
        QStringLiteral("*pfft*"),
        QStringLiteral("magazine empty!"),
        QStringLiteral("*click* (look down!)"),
        QStringLiteral("*klik*"),
        QStringLiteral("no rounds left!"),
        QStringLiteral("*click* (the ammo is ▼)"),
        QStringLiteral("*clunk*"),
        QStringLiteral("*click* (really?)"),
        QStringLiteral("*wheeze*"),
        QStringLiteral("ammo = NaN"),
        QStringLiteral("*click* (try a downward spike)"),
        QStringLiteral("*silence*")
    };
    return list;
}

qreal uniform(qreal lo, qreal hi)
{
    return lo + (hi - lo) * QRandomGenerator::global()->generateDouble();
}

qreal distance(const QPointF& a, const QPointF& b)
{
    return std::hypot(a.x() - b.x(), a.y() - b.y());
}

} // namespace

DespikeArena::DespikeArena(QWidget *parent) :
    QWidget(parent),
    state_(State::Idle),
    roundTime_(0.0),
    countdown_(0.0),
    nextSpawn_(0.0),
    nextStar_(0.0),
    nextAmmo_(0.0),
    flash_(0.0),
    scrollCarry_(0.0),
    walk_(0.0),
    health_(START_HEALTH),
    ammo_(START_AMMO),
    despiked_(0),
    escaped_(0),
    validRemoved_(0),
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
    validRemoved_ = 0;
    shots_ = 0;
    roundTime_ = 0.0;
    countdown_ = COUNTDOWN_S;
    nextSpawn_ = 0.4;
    nextStar_ = 1.0;
    nextAmmo_ = 0.8;
    flash_ = 0.0;
    scrollCarry_ = 0.0;
    walk_ = 0.0;
    series_.clear();
    spikes_.clear();
    ammoSpikes_.clear();
    stars_.clear();
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
    ammoSpikes_.clear();
    stars_.clear();
    pops_.clear();
    update();
}

qreal DespikeArena::progress() const
{
    return std::clamp(roundTime_ / ROUND_S, 0.0, 1.0);
}

void DespikeArena::tick()
{
    const qreal dt = std::min(clock_.restart() / 1000.0, 0.1);

    scroll(dt);

    if (state_ == State::Countdown)
    {
        countdown_ -= dt;
        if (countdown_ <= 0.0)
            state_ = State::Running;
        update();
        return;
    }

    if (state_ != State::Running)
        return;

    roundTime_ += dt;
    flash_ = std::max(flash_ - dt, 0.0);

    spawn(dt);
    age(dt);
    update();

    if (health_ <= 0)
        endRound(true);
    else if (roundTime_ >= ROUND_S)
        endRound(false);
}

void DespikeArena::scroll(qreal dt)
{
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

    // whole samples at a time, and everything on the series rides along;
    // the data speed up as the round goes on
    const qreal speed = 90.0 + 90.0 * progress();
    scrollCarry_ += speed * dt;
    while (scrollCarry_ >= STEP_PX)
    {
        scrollCarry_ -= STEP_PX;
        series_.removeFirst();
        series_.append(nextValue());
        for (auto& spike : spikes_)
            spike.x -= STEP_PX;
        for (auto& ammo : ammoSpikes_)
            ammo.x -= STEP_PX;
        for (auto& star : stars_)
            star.x -= STEP_PX;
    }
}

void DespikeArena::spawn(qreal dt)
{
    const qreal width = plotRect().width();
    const qreal p = progress();

    nextSpawn_ -= dt;
    if (nextSpawn_ <= 0.0)
    {
        spikes_.append({ uniform(0.3, 0.95) * width,
                         uniform(0.25, 0.45),
                         0.0,
                         1.8 - 1.0 * p,     // 1.8 s at the start, 0.8 s at the end
                         0.30 - 0.18 * p });
        nextSpawn_ = (1.0 - 0.65 * p) * uniform(0.7, 1.3);
    }

    nextStar_ -= dt;
    if (nextStar_ <= 0.0)
    {
        stars_.append({ uniform(0.4, 0.95) * width, 0.0 });
        nextStar_ = uniform(1.0, 2.2);
    }

    // a reload only shows up when the magazine runs low, like in the game
    if (ammo_ <= LOW_AMMO && ammoSpikes_.isEmpty())
    {
        if (ammo_ == 0)
            nextAmmo_ = std::min(nextAmmo_, 0.3);
        nextAmmo_ -= dt;
        if (nextAmmo_ <= 0.0)
        {
            ammoSpikes_.append({ uniform(0.45, 0.9) * width, uniform(0.22, 0.35), 0.0 });
            nextAmmo_ = 1.2;
        }
    }
}

void DespikeArena::age(qreal dt)
{
    for (int i = spikes_.size() - 1; i >= 0; --i)
    {
        auto& spike = spikes_[i];
        spike.age += dt;
        if (spike.age > spike.lifetime || spike.x < HEAD_RADIUS)
        {
            spikes_.removeAt(i);
            ++escaped_;
            health_ = std::max(health_ - ESCAPE_DAMAGE, 0);
            flash_ = FLASH_S;
        }
    }

    for (int i = ammoSpikes_.size() - 1; i >= 0; --i)
    {
        auto& ammo = ammoSpikes_[i];
        ammo.age += dt;
        if (ammo.age > AMMO_LIFETIME_S || ammo.x < 0)
            ammoSpikes_.removeAt(i);
    }

    for (int i = stars_.size() - 1; i >= 0; --i)
    {
        auto& star = stars_[i];
        star.age += dt;
        if (star.age > STAR_LIFETIME_S || star.x < 0)
            stars_.removeAt(i);
    }

    for (int i = pops_.size() - 1; i >= 0; --i)
    {
        pops_[i].age += dt;
        if (pops_[i].age > POP_S)
            pops_.removeAt(i);
    }
}

void DespikeArena::endRound(bool died)
{
    timer_->stop();
    state_ = State::Over;
    const int score = despiked_ - VALID_DATA_PENALTY * validRemoved_;
    emit gameOver(score, despiked_, escaped_, validRemoved_, shots_, died);
}

void DespikeArena::addPop(const QPointF &pos, const QString &text, const QColor &color)
{
    pops_.append({ pos, 0.0, text, color });
}

void DespikeArena::mousePressEvent(QMouseEvent *event)
{
    if (state_ != State::Running || event->button() != Qt::LeftButton)
    {
        QWidget::mousePressEvent(event);
        return;
    }

    const QPointF click = event->position();

    // ammo is grabbed, not shot: it costs nothing and works with an empty gun
    for (int i = 0; i < ammoSpikes_.size(); ++i)
    {
        const QPointF box = boxPos(ammoSpikes_.at(i));
        if (distance(box, click) <= AMMO_GRAB_RADIUS)
        {
            ammo_ = std::min(ammo_ + AMMO_PICKUP, MAX_AMMO);
            addPop(box, EggUi::pickFresh(ammoPops()).arg(AMMO_PICKUP), AMMO_GREEN);
            ammoSpikes_.removeAt(i);
            update();
            return;
        }
    }

    if (ammo_ <= 0)
    {
        addPop(click, EggUi::pickFresh(emptyPops()), QColor(150, 150, 150));
        update();
        return;
    }

    --ammo_;
    ++shots_;

    int spikeHit = -1;
    qreal spikeDistance = HIT_RADIUS;
    for (int i = 0; i < spikes_.size(); ++i)
    {
        const qreal d = distance(headPos(spikes_.at(i)), click);
        if (d <= spikeDistance)
        {
            spikeDistance = d;
            spikeHit = i;
        }
    }

    int starHit = -1;
    qreal starDistance = STAR_HIT_RADIUS;
    for (int i = 0; i < stars_.size(); ++i)
    {
        const auto& star = stars_.at(i);
        const qreal d = distance(QPointF(star.x, seriesY(star.x)), click);
        if (d <= starDistance)
        {
            starDistance = d;
            starHit = i;
        }
    }

    if (spikeHit >= 0 && (starHit < 0 || spikeDistance <= starDistance))
    {
        addPop(headPos(spikes_.at(spikeHit)), EggUi::pickFresh(hitPops()), AMBER);
        spikes_.removeAt(spikeHit);
        ++despiked_;
    }
    else if (starHit >= 0)
    {
        const auto& star = stars_.at(starHit);
        addPop(QPointF(star.x, seriesY(star.x)),
               EggUi::pickFresh(starPops()).arg(VALID_DATA_PENALTY), STAR_BLUE);
        stars_.removeAt(starHit);
        ++validRemoved_;
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
    const qreal grown = std::min(spike.age / spike.rise, 1.0);
    const qreal y = seriesY(spike.x) - grown * spike.height * plot.height();
    return QPointF(spike.x, std::max(y, plot.top() + HEAD_RADIUS + 12));
}

QPointF DespikeArena::boxPos(const AmmoSpike &ammo) const
{
    const auto plot = plotRect();
    const qreal grown = std::min(ammo.age / 0.3, 1.0);
    const qreal y = seriesY(ammo.x) + grown * ammo.depth * plot.height();
    return QPointF(ammo.x, std::min(y, plot.bottom() - 12));
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
    for (const auto& star : stars_)
        paintStar(p, star);
    for (const auto& ammo : ammoSpikes_)
        paintAmmoSpike(p, ammo);
    for (const auto& spike : spikes_)
        paintSpike(p, spike);

    auto popFont = font();
    popFont.setBold(true);
    p.setFont(popFont);
    for (const auto& pop : pops_)
    {
        QColor color = pop.color;
        color.setAlpha(static_cast<int>(255 * (1.0 - pop.age / POP_S)));
        p.setPen(color);
        const qreal half = p.fontMetrics().horizontalAdvance(pop.text) / 2.0;
        p.drawText(pop.pos + QPointF(-half, -14 - 30 * pop.age), pop.text);
    }

    if (state_ == State::Countdown)
    {
        auto big = font();
        big.setBold(true);
        big.setPointSize(48);
        p.setFont(big);
        p.setPen(AMBER);
        p.drawText(plot.adjusted(0, 0, 0, -80), Qt::AlignCenter,
                   QString::number(static_cast<int>(std::ceil(countdown_))));

        // how to play, while the player waits
        auto legend = font();
        legend.setBold(true);
        p.setFont(legend);
        const QRectF lines = plot.adjusted(0, plot.height() / 2 + 10, 0, 0);
        const qreal lineHeight = p.fontMetrics().height() + 4;
        p.setPen(QColor(230, 70, 60));
        p.drawText(lines, Qt::AlignHCenter | Qt::AlignTop, tr("Demons: shoot them"));
        p.setPen(STAR_BLUE);
        p.drawText(lines.adjusted(0, lineHeight, 0, 0), Qt::AlignHCenter | Qt::AlignTop,
                   tr("★ Stars: valid data, leave them alone"));
        p.setPen(AMMO_GREEN);
        p.drawText(lines.adjusted(0, 2 * lineHeight, 0, 0), Qt::AlignHCenter | Qt::AlignTop,
                   tr("▼ Downward spikes: grab them to reload"));
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
    if (spike.age > spike.lifetime * 0.6)
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

void DespikeArena::paintAmmoSpike(QPainter &p, const AmmoSpike &ammo) const
{
    // blinks when it is about to go
    if (ammo.age > AMMO_LIFETIME_S * 0.7
        && std::fmod(ammo.age * 8.0, 2.0) < 1.0)
        return;

    const QPointF base(ammo.x, seriesY(ammo.x));
    const QPointF box = boxPos(ammo);

    p.setPen(QPen(AMMO_GREEN, 2.5));
    p.drawLine(base, box);

    const QRectF crate(box.x() - 11, box.y() - 7, 22, 14);
    p.setPen(QPen(QColor(40, 50, 20), 1.2));
    p.setBrush(QColor(95, 110, 45));
    p.drawRect(crate);

    // three shells in the crate
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(230, 190, 60));
    for (int i = -1; i <= 1; ++i)
        p.drawRoundedRect(QRectF(box.x() + i * 6 - 2, box.y() - 4, 4, 8), 1.5, 1.5);
}

void DespikeArena::paintStar(QPainter &p, const Star &star) const
{
    const qreal fadeIn = std::min(star.age / 0.3, 1.0);
    const qreal fadeOut = std::min((STAR_LIFETIME_S - star.age) / 0.5, 1.0);
    const qreal alpha = std::clamp(std::min(fadeIn, fadeOut), 0.0, 1.0);
    const qreal r = STAR_RADIUS * (1.0 + 0.15 * std::sin(star.age * 6.0));
    const QPointF centre(star.x, seriesY(star.x));

    QColor glow = STAR_BLUE;
    glow.setAlphaF(0.25 * alpha);
    p.setPen(Qt::NoPen);
    p.setBrush(glow);
    p.drawEllipse(centre, r * 1.8, r * 1.8);

    QPainterPath shape;
    for (int i = 0; i < 10; ++i)
    {
        const qreal radius = (i % 2 == 0) ? r : r * 0.45;
        const qreal angle = -PI / 2 + i * PI / 5;
        const QPointF point = centre + QPointF(radius * std::cos(angle), radius * std::sin(angle));
        if (i == 0)
            shape.moveTo(point);
        else
            shape.lineTo(point);
    }
    shape.closeSubpath();

    QColor fill = STAR_BLUE;
    fill.setAlphaF(alpha);
    p.setBrush(fill);
    p.drawPath(shape);
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

    const QRectF text = hud.adjusted(10, 0, -10, 0);
    const int remaining = static_cast<int>(std::ceil(std::max(ROUND_S - roundTime_, 0.0)));

    p.setPen(health_ <= 30 ? DANGER : AMBER);
    p.drawText(text, Qt::AlignVCenter | Qt::AlignLeft, tr("HEALTH %1%").arg(health_));

    QString ammo;
    if (ammo_ == 0)
        ammo = tr("OUT OF AMMO: grab a ▼ spike");
    else if (ammo_ <= LOW_AMMO)
        ammo = tr("LOW AMMO %1").arg(ammo_);
    else
        ammo = tr("AMMO %1").arg(ammo_);
    p.setPen(ammo_ <= LOW_AMMO ? DANGER : AMBER);
    p.drawText(text, Qt::AlignCenter, tr("%1   DESPIKED %2").arg(ammo).arg(despiked_));

    p.setPen(AMBER);
    p.drawText(text, Qt::AlignVCenter | Qt::AlignRight,
               QStringLiteral("0:%1").arg(remaining, 2, 10, QLatin1Char('0')));
}
