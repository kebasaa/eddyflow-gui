/***************************************************************************
  fluxrunner.h
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

#ifndef FLUXRUNNER_H
#define FLUXRUNNER_H

#include <QColor>
#include <QElapsedTimer>
#include <QList>
#include <QPointF>
#include <QWidget>

class FakeLoading;
class QLabel;
class QProgressBar;
class QPushButton;
class QTimer;

/// "Super Flux Run": a side-scroller across a week of raw eddy covariance
/// data. The 10 Hz CO2 trace is the ground - high under the stable night,
/// drawn down by the day's uptake, ragged with turbulence - missing data
/// drop to -9999 and leave a pit, and spikes stand up as needles; the
/// boundary layer above it gusts, swirls and rains on the same diurnal
/// clock. The flux tower at the end of each level is the goal, and every
/// level is a new site, season and week, harder than the last.
class FluxRunner : public QWidget
{
    Q_OBJECT
public:
    struct Result
    {
        // this level
        int coins;
        int totalCoins;
        int defeated;
        int totalFoes;
        int gapsBridged;
        qreal seconds;
        int livesLost;
        int firstDoy;
        int level;
        QString site;
        QString siteCode;
        QString season;
        bool cleared;
        bool flawless;      // the level cleared without a life lost, or every half-hour taken
        // the run so far
        int score;
        int rating;         // 0..100 for the whole run, independent of the levels drawn
        int levelsCleared;
        int lives;
        int runCoins;
        int runTotalCoins;
        int runDefeated;
        int runGaps;
        int runLivesLost;
        qreal runSeconds;
        QString firstSite;
        bool timedOut;      // the last life went to the deadline
    };

    explicit FluxRunner(QWidget* parent = nullptr);

    /// a new run: three lives, no score, one of the two figures, level 1
    void startRun(int figure);
    /// the next, harder level of the same run
    void nextLevel();
    void stop();

    QSize sizeHint() const override;

signals:
    void levelCleared(const FluxRunner::Result& result);
    void runOver(const FluxRunner::Result& result);

protected:
    bool event(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    bool focusNextPrevChild(bool next) override;
    void paintEvent(QPaintEvent* event) override;

private:
    enum class State { Idle, Card, Running, Paused, Dying, Piping, Cleared, Over };
    enum KeyBit : unsigned { KeyLeft = 1, KeyRight = 2, KeyJump = 4, KeyDown = 8 };
    enum class Power { Small, Hat, Filter };
    enum class Item { Coin, Hat, Filter, Coffee };
    // the shared foes first, then each site's own
    enum class FoeKind { Reviewer, Bubble, Puff, Cow, Tractor, DustDevil, Mosquito, Faller };

    // everything that makes one level harder than another, from its number;
    // each knob is clamped to a cap the jump can always manage
    struct LevelSpec
    {
        int level;
        int minSlots;
        int maxSlots;
        qreal pitsPer100;
        qreal wallsPer100;
        int earlyPitMin;
        int earlyPitMax;
        int latePitMin;
        int latePitMax;
        int maxPit;
        int maxZonePit;
        qreal maxLandingRise;
        qreal earlyRiseMin;
        qreal earlyRiseMax;
        qreal lateRiseMin;
        qreal lateRiseMax;
        qreal doubleWallChance;
        int pad;
        int movingMin;
        int movingMax;
        qreal movingAmplitude;
        qreal movingPeriod;
        int tubesMin;
        int tubesMax;
        int showersMin;
        int showersMax;
        qreal zoneShare;
        qreal zoneStrengthMin;
        qreal zoneStrengthMax;
        qreal gustMax;
        qreal eddyKick;
        qreal eddyIntervalMin;
        qreal eddyIntervalMax;
        int maxEddies;
        int reviewerGapMin;
        int reviewerGapMax;
        qreal reviewerSpeed;
        qreal puffsPer100;
        qreal puffSpeed;
        qreal bubbleChance;
        qreal bubblePauseMin;
        qreal bubblePauseMax;
        qreal dangersPer100;    // the site's own danger
        int hats;
        int filters;
        int coffees;
        int maxBlocks;
        qreal coinRowChance;
        qreal deadlinePerSlot;
        qreal pointsFactor;
    };

    // a half-hour of the level: what it holds, and its mean level
    struct Bar
    {
        qreal top;      // world y of the half-hour's mean, FLOOR - height
        qreal flux;     // the flux behind it, umol m-2 s-1
        bool gap;       // missing data: no ground at all
        bool spike;
        bool tube;      // one of the two half-hours of a sampling tube
        int moving;     // index into moving_, or -1
        int patch;      // index into patches_, or -1
    };

    // one point of the raw trace: the ground the game is played on
    struct Sample
    {
        qreal top;      // what is stood on, a needle included
        qreal base;     // the trace without its needle
        int moving;
        bool gap;
        bool spike;
        bool tube;
    };

    struct Body
    {
        QPointF feet;   // bottom centre
        QPointF vel;
        qreal halfW;
        qreal height;
        bool grounded;
    };

    struct Foe
    {
        FoeKind kind;
        Body body;
        Body spawn;
        qreal timer;    // bubbles: until the next leap
        qreal squash;   // seconds of the flattened sheet left
        int facing;
        bool alive;
        bool active;
        bool defeated;
    };

    struct Block
    {
        int column;
        qreal bottom;
        Item item;
        int coinsLeft;
        qreal bump;
        bool used;
    };

    struct Coin
    {
        QPointF pos;
        bool taken;
    };

    struct Pickup
    {
        Item kind;
        Body body;
        qreal rise;     // px risen out of the block so far
        int facing;
    };

    struct Shot
    {
        Body body;
        qreal age;
    };

    struct Pop
    {
        QPointF pos;    // world coordinates
        qreal age;
        QString text;
        QColor color;
    };

    struct Zone
    {
        int start;
        int end;
        qreal strength; // 0..1
        qreal phase;
        qreal noise;    // smoothed random walk, -1..1
        int direction;  // the mean wind, +1 with the run
        int thermal;    // first column of the updraft, or -1
        bool announced;
    };

    struct Eddy
    {
        QPointF pos;
        qreal radius;
        int spin;       // +1 anticlockwise on screen
        qreal age;
        qreal cooldown;
        int zone;
    };

    struct Tube
    {
        int column;
        int exitColumn;
        qreal lag;      // seconds
    };

    struct MovingRun
    {
        int start;
        int end;
        qreal amplitude;
        qreal period;
        qreal phase;
    };

    struct Shower
    {
        int start;
        int end;
        bool announced;
    };

    // ice on the tundra, a grass fire in the savanna
    struct Patch
    {
        int start;
        int end;
        bool fire;
        bool announced;
    };

    struct Move
    {
        bool hitWall;
        bool landed;
        int bumpedBlock;
    };

    // the level
    static LevelSpec specFor(int level);
    void startLevel(int level);
    void drawSetting();
    bool generateLevel(bool hazards);
    bool verifyLevel() const;
    void buildSamples();
    void placeThings();
    int columnCount() const { return static_cast<int>(bars_.size()); }
    int columnOf(qreal x) const;        // the half-hour under x
    int sampleOf(qreal x) const;        // the trace sample under x
    qreal surfaceAt(int sample) const;
    qreal groundTop(qreal x) const;
    qreal standTop(qreal x, qreal halfW) const;
    qreal supportTop(qreal left, qreal right, qreal feet) const;
    bool nearGap(int column, int reach) const;
    qreal hourOfSlot(int t) const;      // t counts half-hours from the first of the data
    qreal sunElevation(qreal hour) const;
    qreal hourAt(qreal x) const;
    qreal nightAt(qreal x) const;
    int doyAt(qreal x) const;
    int zoneAt(qreal x) const;
    bool inThermal(qreal x) const;
    bool raining(qreal x) const;
    int patchAt(qreal x) const;         // the patch under x, or -1
    qreal gust(int zone) const;
    qreal rhythmDensity(int t) const;
    qreal rhythmLateness(int t) const;
    int points(int base) const;

    // the run
    void tick();
    void step(qreal dt);
    Move moveBody(Body& body, qreal dt, qreal stepUp);
    void updatePlayer(qreal dt);
    void updateAtmosphere(qreal dt);
    void updateFoes(qreal dt);
    void updatePickups(qreal dt);
    void updateShots(qreal dt);
    void collectCoins();
    void touchFoes(qreal previousFeet);
    void bumpBlock(int index);
    void defeat(Foe& foe, int value, const QString& text);
    void gainCoin(const QPointF& where);
    void fire();
    void hurt(const QString& why = QString());
    void die(bool pit);
    void respawn();
    void enterTube(int index);
    void reachTower();
    void finishLevel(bool cleared);
    void togglePause();
    void addPop(const QPointF& world, const QString& text, const QColor& color);
    void announce(const QString& text, const QColor& color);
    void noticeDanger();
    qreal levelQuality() const;
    Result result() const;

    // drawing
    void paintSky(QPainter& p) const;
    void paintParallax(QPainter& p) const;
    void paintFarScenery(QPainter& p, qreal night) const;
    void paintNearScenery(QPainter& p, qreal night) const;
    void paintAxes(QPainter& p) const;
    void paintAtmosphereBack(QPainter& p) const;
    void paintGround(QPainter& p) const;
    void paintThings(QPainter& p) const;
    void paintFoes(QPainter& p) const;
    void paintEddies(QPainter& p) const;
    void paintPlayer(QPainter& p) const;
    void paintFigure(QPainter& p, const QPointF& feet, int frame, bool left,
                     qreal opacity) const;
    void paintWeather(QPainter& p) const;
    void paintHud(QPainter& p) const;
    void paintOverlay(QPainter& p) const;
    int playerFrame() const;

    State state_;
    QTimer* timer_;
    QElapsedTimer clock_;
    qreal accumulator_;
    qreal stateTime_;
    qreal clockTime_;   // seconds of play, for animations

    unsigned held_;
    qreal jumpBuffer_;
    qreal coyote_;
    qreal invulnerable_;
    qreal coffee_;
    qreal fireCooldown_;
    qreal runDistance_;
    bool facingLeft_;
    bool crouching_;
    Power power_;
    Body player_;
    qreal cameraX_;
    int stompChain_;
    int figure_;        // which of the two figures this run is played with

    QList<Bar> bars_;
    QList<Sample> samples_;
    QList<Foe> foes_;
    QList<Block> blocks_;
    QList<Coin> coins_;
    QList<Pickup> pickups_;
    QList<Shot> shots_;
    QList<Pop> pops_;
    QList<Zone> zones_;
    QList<Eddy> eddies_;
    QList<Tube> tubes_;
    QList<MovingRun> moving_;
    QList<Shower> showers_;
    QList<Patch> patches_;
    QList<QPointF> stars_;      // the night sky, in screen fractions
    QList<int> gapEnds_;        // first column after each pit, in order
    QList<int> clusters_;       // where a clustered rhythm puts its hazards
    int blockCoins_;            // the half-hours waiting inside blocks

    // the level's setting: a site, a date, a time of day, the weather
    LevelSpec spec_;
    int level_;
    int site_;
    QString siteName_;
    QString siteCode_;
    QString season_;
    int seasonIndex_;
    QString weatherName_;
    QStringList modifierNames_;
    unsigned modifiers_;
    qreal seasonFactor_;    // the growing season's share of full uptake
    qreal sunrise_;
    qreal sunset_;
    int startSlot_;         // the time of day of the first half-hour, in half-hours
    int dataSlots_;
    int weather_;           // 0 clear, 1 mixed, 2 overcast
    int frontDay_;          // a grey day with a shower tied to it, or -1
    int rhythm_;
    qreal rhythmPhase_;
    qreal rhythmPeriod_;
    int cloudCount_;
    bool snowy_;
    qreal cardDuration_;

    int checkpointColumn_;
    int towerColumn_;
    int hutColumn_;
    int respawnColumn_;
    bool checkpointTaken_;
    int nextGap_;
    int firstDoy_;
    int tubeIndex_;
    int clearStage_;
    qreal flagY_;
    qreal nextEddy_;
    bool diedInPit_;
    bool diedOfDeadline_;
    bool nightAnnounced_;
    bool dangerAnnounced_;
    QColor sky_;
    QString banner_;
    QColor bannerColor_;
    qreal bannerAge_;
    QString cardLine_;
    QString pauseLine_;

    // this level
    int lives_;
    int livesLost_;
    int coinCount_;
    int defeated_;
    int gapsBridged_;
    qreal deadline_;
    qreal levelDeadline_;
    qreal runTime_;
    bool cleared_;
    bool timedOut_;

    // the run
    int score_;
    int levelsCleared_;
    int runCoins_;
    int runTotalCoins_;
    int runDefeated_;
    int runGaps_;
    int runLivesLost_;
    qreal runSeconds_;
    qreal qualitySum_;
    qreal qualityWeight_;
    QString firstSite_;
};

/// The panel around the game: the intro and its Start button, a short
/// loading sequence, the game, a summary between levels, and the rank at
/// the end of the run.
class FluxRunnerPanel : public QWidget
{
    Q_OBJECT
public:
    explicit FluxRunnerPanel(QWidget* parent = nullptr);

    /// back to the intro; stops anything that is running
    void reset();

signals:
    void finished();

private:
    void launch();
    void play();
    void continueRun();
    void showLevelSummary(const FluxRunner::Result& result);
    void showResult(const FluxRunner::Result& result);

    FakeLoading* loading_;
    QLabel* intro_;
    QLabel* controls_;
    QPushButton* startButton_;
    QProgressBar* bar_;
    QLabel* status_;
    FluxRunner* game_;
    QWidget* result_;
    QLabel* rank_;
    QLabel* stats_;
    QLabel* tag_;
    QPushButton* again_;
    QPushButton* next_;
};

#endif // FLUXRUNNER_H
