/***************************************************************************
  despikearena.h
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

#ifndef DESPIKEARENA_H
#define DESPIKEARENA_H

#include <QColor>
#include <QElapsedTimer>
#include <QList>
#include <QPointF>
#include <QWidget>

class QTimer;

/// "Despike or Die": the Doom easter egg's mini-game. A w' time series
/// scrolls past and spike demons rise out of it, ever faster; the player
/// shoots them before they escape into the data. Downward spikes carry
/// ammo, and stars sitting on the series are valid data that must be spared.
/// Rime-iced demons take two hits, drop-outs open gaps in the series that
/// have to be filled in, supplies drift along below the series, and the
/// BFG 42 charges as you despike and fires on a right-click.
class DespikeArena : public QWidget
{
    Q_OBJECT
public:
    explicit DespikeArena(QWidget* parent = nullptr);

    void start();
    void stop();

    /// what a valid data point costs when it is shot
    static constexpr int VALID_DATA_PENALTY = 2;

signals:
    void gameOver(int score, int despiked, int escaped, int validRemoved,
                  int shots, int gapsSealed, int gapsLost, bool died);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    enum class State { Idle, Countdown, Running, Over };

    struct Spike
    {
        enum class Kind { Plain, Rimed };

        Kind kind;
        qreal x;
        qreal height;   // fraction of the plot height above the series
        qreal age;      // seconds
        qreal lifetime; // shorter as the round goes on
        qreal rise;
        int hp;         // a rimed demon loses its ice first
    };

    // a downward spike with an ammo box at its tip
    struct AmmoSpike
    {
        qreal x;
        qreal depth;    // fraction of the plot height below the series
        qreal age;
    };

    // a valid data point, sitting on the series
    struct Star
    {
        qreal x;
        qreal age;
    };

    // a stretch of missing data, filled in from the left by clicking it
    struct Dropout
    {
        qreal x;        // left edge
        qreal width;
        qreal sealed;   // 0..1
        qreal age;
    };

    // a supply drifting along below the series, grabbed for free
    struct PowerUp
    {
        enum class Kind { Quad, Berserk, Downsample, GapFill, Invulnerable,
                          HardFlag, Rocket };

        Kind kind;
        qreal x;
        qreal y;        // fraction of the plot height
        qreal age;
    };

    struct Buff
    {
        PowerUp::Kind kind;
        qreal remaining;
        qreal total;
    };

    // purely cosmetic: the damage is dealt when the blast is set off
    struct Blast
    {
        QPointF pos;
        qreal age;
        qreal radius;
        QColor color;
    };

    struct Pop
    {
        QPointF pos;
        qreal age;
        QString text;
        QColor color;
    };

    void tick();
    void scroll(qreal dt);
    void spawn(qreal dt);
    void age(qreal dt);
    void endRound(bool died);
    qreal progress() const;
    void addPop(const QPointF& pos, const QString& text, const QColor& color);

    qreal timeScale() const;
    bool hasBuff(PowerUp::Kind kind) const;
    int damage() const;
    void hurt(int amount);
    void applyPowerUp(const PowerUp& powerUp);
    void killSpike(int index, bool charge, bool announce);
    void sealDropout(int index, bool charge, bool announce);
    void rewardKill(int points, qreal charge);
    int detonate(const QPointF& centre, qreal radius, const QColor& color,
                 bool hitsStars, bool charge);
    void fireRocket(const QPointF& click);
    void fireBfg(const QPointF& click);
    void shootStar(int index);

    static QColor kindColor(PowerUp::Kind kind);
    static QString kindChip(PowerUp::Kind kind);
    static QString kindName(PowerUp::Kind kind);
    static qreal kindDuration(PowerUp::Kind kind);

    QRectF plotRect() const;
    qreal seriesY(qreal x) const;
    QPointF headPos(const Spike& spike) const;
    QPointF boxPos(const AmmoSpike& ammo) const;
    QPointF powerUpPos(const PowerUp& powerUp) const;
    QRectF gapRect(const Dropout& dropout) const;
    bool inDropout(qreal x) const;

    void paintSeries(QPainter& p) const;
    void paintSpike(QPainter& p, const Spike& spike) const;
    void paintAmmoSpike(QPainter& p, const AmmoSpike& ammo) const;
    void paintStar(QPainter& p, const Star& star) const;
    void paintDropout(QPainter& p, const Dropout& dropout) const;
    void paintPowerUp(QPainter& p, const PowerUp& powerUp) const;
    void paintBlast(QPainter& p, const Blast& blast) const;
    void paintBuffChips(QPainter& p) const;
    void paintBfgGauge(QPainter& p) const;
    void paintHud(QPainter& p) const;

    State state_;
    QTimer* timer_;
    QElapsedTimer clock_;
    qreal roundTime_;
    qreal countdown_;
    qreal nextSpawn_;
    qreal nextStar_;
    qreal nextAmmo_;
    qreal nextDropout_;
    qreal nextPower_;
    qreal flash_;
    bool flashWhite_;
    qreal scrollCarry_;
    qreal bfgCharge_;
    qreal bfgNag_;      // seconds since the last "not charged" line

    QList<qreal> series_;
    qreal walk_;
    QList<Spike> spikes_;
    QList<AmmoSpike> ammoSpikes_;
    QList<Star> stars_;
    QList<Dropout> dropouts_;
    QList<PowerUp> powerUps_;
    QList<Buff> buffs_;
    QList<Blast> blasts_;
    QList<Pop> pops_;

    int health_;
    int ammo_;
    int rockets_;
    int points_;
    int despiked_;
    int escaped_;
    int validRemoved_;
    int shots_;
    int gapsSealed_;
    int gapsLost_;
};

#endif // DESPIKEARENA_H
