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

#include <QElapsedTimer>
#include <QList>
#include <QPointF>
#include <QWidget>

class QTimer;

/// "Despike or Die": the Doom easter egg's mini-game. A w' time series
/// scrolls past, spike demons rise out of it, and the player clicks them
/// before they escape into the data.
class DespikeArena : public QWidget
{
    Q_OBJECT
public:
    explicit DespikeArena(QWidget* parent = nullptr);

    void start();
    void stop();

signals:
    void gameOver(int despiked, int escaped, int shots, bool died);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    enum class State { Idle, Countdown, Running, Over };

    struct Spike
    {
        qreal x;
        qreal height; // fraction of the plot height above the series
        qreal age;    // seconds
    };

    struct Pop
    {
        QPointF pos;
        qreal age;
    };

    void tick();
    void endRound(bool died);

    QRectF plotRect() const;
    qreal seriesY(qreal x) const;
    QPointF headPos(const Spike& spike) const;

    void paintSeries(QPainter& p) const;
    void paintSpike(QPainter& p, const Spike& spike) const;
    void paintHud(QPainter& p) const;

    State state_;
    QTimer* timer_;
    QElapsedTimer clock_;
    qreal roundTime_;
    qreal countdown_;
    qreal nextSpawn_;
    qreal flash_;
    qreal scrollCarry_;

    QList<qreal> series_;
    qreal walk_;
    QList<Spike> spikes_;
    QList<Pop> pops_;

    int health_;
    int ammo_;
    int despiked_;
    int escaped_;
    int shots_;
};

#endif // DESPIKEARENA_H
