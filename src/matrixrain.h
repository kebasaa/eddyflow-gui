/***************************************************************************
  matrixrain.h
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

#ifndef MATRIXRAIN_H
#define MATRIXRAIN_H

#include <QElapsedTimer>
#include <QList>
#include <QRawFont>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <memory>

#include "rawplayback.h"

class QTimer;

class DlProject;
class EcProject;

/// Running statistics of one recording, row by row: the Reynolds
/// decomposition the engine does per averaging block, done here over a
/// sliding 30 s window, with the wind rotated into its mean direction.
/// Pure arithmetic, so it can be checked without painting anything.
class MatrixStats
{
public:
    static constexpr double WINDOW_S = 30.0;

    void reset(int scalarCount, double frequency);

    /// one row; NaN marks a gap in that variable
    void push(float u, float v, float w, const QVector<float>& scalars);

    bool hasWind() const { return windSeen_; }
    bool windGap() const { return windGap_; }
    /// mean wind direction in the sonic frame, atan2(v̄, ū), radians
    double direction() const;
    double meanSpeed() const;
    double meanU() const { return u_.mean; }
    double meanW() const { return w_.mean; }
    /// the |v| that makes the largest or smallest glyphs: |mean v| plus
    /// two standard deviations over the last 30 s, so the sizes use the
    /// whole range whatever the site's crosswind
    double vScale() const;

    /// the last row's fluctuations over their standard deviations; 0 in a gap
    double uz() const { return uz_; }
    double vz() const { return vz_; }
    double wz() const { return wz_; }
    /// NaN in a gap of that scalar
    double scalarZ(int i) const { return scalarZ_.value(i); }
    /// running w′s′, and w′s′ / (σw σs)
    double covariance(int i) const;
    double correlation(int i) const;

    /// The raw wind vector on screen, in cells per second, each component
    /// centred on zero: negative w falls, positive w rises, u moves sideways
    /// by its sign, and a component of exactly zero does not move at all.
    /// v is depth: glyphs grow coming towards you and shrink going away.
    static double fallRate(double w);
    static double sideRate(double u);
    static int sizeOf(double v, double scale);
    static constexpr int SIZES = 7;
    /// -1 (carried down) .. +1 (carried up): the instantaneous flux term
    static double brightness(double wz, double sz);
    /// the glyph of a raw value as it was logged: its digits at the
    /// logged resolution, modulo count; part n takes the next base-count
    /// digit up, for the cells a fast drop crosses; -1 in a gap
    static int glyphFromValue(double value, int decimals, int count, int part = 0);

private:
    struct Moment
    {
        double mean = 0.0;
        double var = 0.0;
        long long n = 0;
    };
    double alpha(long long n) const;
    static double zOf(double fluctuation, double var);

    double frequency_ = 0.0;
    Moment u_, v_, w_;
    double varU_ = 0.0, varV_ = 0.0;
    long long windN_ = 0;
    QVector<Moment> scalars_;
    QVector<double> cov_;
    QVector<double> scalarZ_;
    double uz_ = 0.0, vz_ = 0.0, wz_ = 0.0;
    bool windSeen_ = false;
    bool windGap_ = true;
};

/// Raw data played as it was measured, one row per 1/frequency of wall
/// clock time, as the falling code of a certain film. Wind moves the rain,
/// temperature and gases choose the glyphs, their flux lights them.
class MatrixRain : public QWidget
{
    Q_OBJECT
public:
    explicit MatrixRain(QWidget* parent = nullptr);

    /// "Wake up, Neo…"; the rain follows as soon as there is data
    void playIntro();
    void setRecording(std::shared_ptr<const RawPlayback::Recording> recording);
    void stop();

    /// rows of the current recording played so far
    int consumedSamples() const { return consumed_; }
    bool isRaining() const { return state_ == State::Rain; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    static const QStringList& introLines();

signals:
    /// the recording has played to its end, or N was pressed
    void nextFileRequested();
    void leaveRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    enum class State { Idle, Intro, Waiting, Rain };

    struct Cell
    {
        qint16 glyph = -1;
        quint8 scalar = 0;
        qint8 bright = 0;   // -2 .. 2
        qint8 warm = 0;     // -2 (colder) .. 2 (warmer)
        quint8 size = 3;    // 0 (going away) .. 6 (coming towards you)
        quint8 trail = 28;  // samples it stays lit: its stream's length
        long long stamp = 0;
    };
    struct Drop
    {
        double x = 0.0;
        double y = 0.0;
        int scalar = 0;
        int wait = 0;
        double pace = 1.0;  // streams fall at their own speeds, as in the film
        int trail = 28;
        // a stream released as part of an air parcel
        double spread = 0.0;        // cells/s sideways, away from the others
        double pu = 0.0, pw = 0.0;  // the raw u and w that released it
        int age = 0;
        int life = 0;
    };
    // what one row looks like, kept for a while so a drop further
    // downstream can be shown an older row (Taylor's frozen turbulence)
    struct Frame
    {
        float u = 0.0f, v = 0.0f, w = 0.0f;   // as logged
        float wz = 0.0f;
        bool wind = false;
        QVector<float> z;       // per scalar, NaN in a gap
        QVector<float> bright;  // per scalar, -1 .. 1
        QVector<float> raw;     // per scalar, the value as logged
        float warm = 0.0f;      // Ts fluctuation, smoothed
    };

    void tick();
    void flicker(double seconds);
    void skipIntro();
    void startRain();
    void step(int row, bool draw = true);
    void releaseParcel(const Frame& frame);
    void moveParcels(const Frame& newest);
    void lay(const Drop& drop, double x0, double y0, const Frame& seen, const Frame& now, bool parcel);
    void layoutGrid();
    void spawn(Drop& drop, bool anywhere);
    void wake();
    double playedMs() const;
    void setPaused(bool paused);

    void chooseFont();
    QColor cellColour(const Cell& cell, long long age) const;
    void paintIntro(QPainter& p);
    void paintRain(QPainter& p);
    /// the row a drop at column x is shown: older the further downwind
    const Frame* frameAt(double x) const;
    void paintHud(QPainter& p);

    State state_;
    QTimer* timer_;
    QElapsedTimer clock_;
    double pausedMs_;
    bool paused_;
    QElapsedTimer introClock_;
    qint64 introShiftMs_;
    QElapsedTimer hudClock_;
    QElapsedTimer frameClock_;
    bool legend_;
    bool endSignalled_;     // the next file has been asked for
    bool swapNow_;          // N: change file as soon as it is in

    std::shared_ptr<const RawPlayback::Recording> recording_;
    std::shared_ptr<const RawPlayback::Recording> pending_;
    const RawPlayback::Channel* u_;
    const RawPlayback::Channel* v_;
    const RawPlayback::Channel* w_;
    QList<const RawPlayback::Channel*> scalars_;
    QStringList scalarLabels_;
    QVector<int> decimals_;
    QVector<double> scalarHue_;
    int tsScalar_;
    double warm_;

    MatrixStats stats_;
    int consumed_;
    long long steps_;
    QVector<Frame> history_;
    long long historyBase_;
    QVector<float> rowScalars_;

    QRawFont rawFont_[MatrixStats::SIZES];
    QVector<quint32> glyphIndexes_[MatrixStats::SIZES];
    bool katakana_;
    double cellW_;
    double cellH_;
    int cols_;
    int rows_;
    QVector<Cell> grid_;
    QVector<Drop> drops_;
    QVector<Drop> parcels_;
    int parcelCooldown_;
};

/// The Matrix variant of the easter egg: finds the open project's raw
/// files, reads them off the GUI thread and feeds them to the rain one
/// after another, falling back to a bundled sample when they cannot be read.
class MatrixPanel : public QWidget
{
    Q_OBJECT
public:
    MatrixPanel(DlProject* dlProject, EcProject* ecProject, QWidget* parent = nullptr);

    /// stops the rain and frees the data; the next show starts afresh
    void reset();

signals:
    void finished();

protected:
    void showEvent(QShowEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void start();
    void loadNext();
    void load(const QString& file);
    void loadSample();

    DlProject* dlProject_;
    EcProject* ecProject_;
    MatrixRain* rain_;
    QStringList files_;
    int fileIndex_;
    int generation_;
    bool started_;
    bool projectFailed_;
};

#endif // MATRIXRAIN_H
