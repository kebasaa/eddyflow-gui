/***************************************************************************
  fluxrunner.cpp
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

#include "fluxrunner.h"

#include "eastereggwidgets.h"

#include <QFocusEvent>
#include <QFontDatabase>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QTimer>
#include <QTransform>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

using namespace EggUi;

namespace {

////////////////////////////////////////////////////////////////////////////////
// numbers

// world and screen; y points down, the floor is the bottom of the playfield
const int PLAY_HEIGHT = 388;
const int HUD_HEIGHT = 32;
const qreal FLOOR = 388.0;
const qreal BAR_W = 24.0;           // one half-hour of the level
const qreal SAMPLE_W = 4.0;         // one point of the raw trace, and of the ground
const int SAMPLES_PER_SLOT = 6;
const int TICK_MS = 16;
const qreal STEP_S = 1.0 / 120.0;   // fixed physics step: jumps come out the same at any frame rate
const qreal INF = std::numeric_limits<qreal>::infinity();
const qreal EPS = 0.001;
const qreal PI = 3.14159265358979323846;

// level layout, in columns: a flat lead-in, the level's half-hours, the tower
const int LEAD = 12;
const int TAIL = 24;
const int TOWER_OFFSET = 4;
const int HUT_OFFSET = 12;

// feel
constexpr qreal MAX_RUN = 240.0;
const qreal GROUND_ACCEL = 1400.0;
const qreal GROUND_FRICTION = 1800.0;
const qreal TURN_DECEL = 2600.0;
const qreal AIR_ACCEL = 900.0;
const qreal AIR_DRAG = 300.0;
constexpr qreal JUMP_V = 760.0;
constexpr qreal G_RISE = 1900.0;    // while rising with jump held
constexpr qreal G_FALL = 2600.0;
const qreal JUMP_CUT_V = 380.0;     // releasing early cuts the jump
const qreal FALL_MAX = 900.0;
const qreal COYOTE_S = 0.10;
const qreal JUMP_BUFFER_S = 0.12;
const qreal STOMP_V = 480.0;
const qreal STOMP_HELD_V = 700.0;
const qreal STEP_UP = 10.0;
const qreal AIR_STEP_UP = 3.0;
const qreal SNAP_DOWN = 10.0;
const qreal PLAYER_HALF_W = 11.0;
const qreal PLAYER_HEIGHT = 56.0;
const qreal CROUCH_HEIGHT = 36.0;
const qreal DEATH_DEPTH = 70.0;

// what the generator keeps to whatever the level: a run-up before every
// jump, and ground no steeper than can be walked (the caps are with the
// levels, further down)
const int RUN_UP = 3;
const qreal SLOPE = 8.0;

// the raw trace: turbulence on top of the half-hourly level, never more
// of a step between two points than can be walked
// (small: the figure stands on the highest point under its feet, and the
// drawn trace adds a finer wiggle of its own)
const qreal TURBULENCE_DAY = 2.0;
const qreal TURBULENCE_NIGHT = 0.8;
const qreal TURBULENCE_BURST = 2.4;  // intermittent turbulence on a stable night
const qreal TURBULENCE_MAX = 5.0;
const qreal SAMPLE_STEP = 5.0;
// the y-axis: CO2 mole fraction at a height above the floor
const qreal CO2_AT_FLOOR = 360.0;
const qreal CO2_PER_PX = 0.4;

// the boundary layer
const qreal THERMAL_LIFT = 600.0;
const qreal THERMAL_MAX_RISE = 420.0;
const int THERMAL_WIDTH = 3;
const qreal RAIN_GRIP = 0.35;
const qreal ICE_GRIP = 0.2;         // still stops within the room kept around a pit
const qreal TUBE_HEIGHT = 40.0;

// things
const qreal BUBBLE_G = 1400.0;
const qreal SHOT_SPEED = 400.0;
const qreal SHOT_G = 2200.0;
const qreal HAT_SPEED = 80.0;
const qreal COW_SPEED = 25.0;
const qreal FALL_WARNING_S = 0.8;       // the shadow before a cone or a rock lets go
const qreal FALL_WAKE = 14.0 * BAR_W;   // lets go so as to land as a runner arrives
const qreal COFFEE_SPEED = 100.0;
const qreal COFFEE_S = 8.0;
const qreal HURT_INVULNERABLE_S = 1.5;
const qreal BLOCK = 24.0;
const qreal BLOCK_HEIGHT = 104.0;   // a block's bottom above the ground
const int START_LIVES = 3;
const qreal CARD_S = 1.6;
const qreal NEW_LEVEL_CARD_S = 2.4;
const qreal OVER_S = 1.2;
const qreal POP_S = 0.9;
const qreal BANNER_S = 2.4;
const int COIN_POINTS = 10;
const int KILL_POINTS = 100;
const int POWER_POINTS = 1000;

const QColor UPTAKE(118, 189, 29);
const QColor RELEASE(200, 110, 50);
const QColor SPIKE_RED(210, 60, 50);
const QColor AMBER(232, 163, 23);
const QColor DANGER(230, 50, 40);
const QColor COIN_GREEN(150, 220, 90);
const QColor ICE(170, 210, 255);
const QColor HAT_YELLOW(245, 200, 40);
const QColor FILTER_ORANGE(250, 140, 40);
const QColor COFFEE_BROWN(150, 95, 55);
const QColor HUD_BG(45, 40, 35);
const QColor NIGHT_SKY(12, 16, 42);
const QColor DAY_SKY(120, 180, 240);
const QColor DAWN_SKY(240, 150, 90);

qreal uniform(qreal lo, qreal hi)
{
    return lo + (hi - lo) * QRandomGenerator::global()->generateDouble();
}

// inclusive at both ends
int between(int lo, int hi)
{
    return lo + QRandomGenerator::global()->bounded(hi - lo + 1);
}

bool chance(qreal probability)
{
    return uniform(0.0, 1.0) < probability;
}

QColor mix(const QColor& a, const QColor& b, qreal t)
{
    t = std::clamp(t, 0.0, 1.0);
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t,
                            a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

// a fixed number in [0, 1) for each k: what is drawn from it is drawn the
// same way every frame, with nothing stored
qreal hash01(qreal k)
{
    const qreal v = std::sin(k * 12.9898 + 78.233) * 43758.5453;
    return v - std::floor(v);
}

////////////////////////////////////////////////////////////////////////////////
// the levels

// Caps on every knob of a level. The jump peaks at 152 px and carries 178 px
// at a full run (less when it lands higher or runs into a headwind); no
// hazard ever asks for more than three quarters of that, at any level.
constexpr int CAP_PIT = 5;                      // 120 px: 75 % of the reach landing 56 px higher
constexpr int CAP_ZONE_PIT = 4;                 // 96 px: 74 % of it in a 48 px/s headwind
constexpr qreal CAP_LANDING_RISE = 56.0;
constexpr qreal CAP_RISE = 112.0;               // 74 % of the apex
constexpr qreal CAP_GUST = 48.0;                // a fifth of the run speed
constexpr qreal CAP_KICK = 560.0;               // below the jump itself
constexpr qreal CAP_MOVING_AMPLITUDE = 48.0;
constexpr qreal CAP_MOVING_PERIOD = 2.4;        // the shortest
constexpr int CAP_PAD = 4;                      // a run-up and somewhere to land
constexpr qreal CAP_REVIEWER_SPEED = 70.0;      // never as fast as a running player
constexpr qreal CAP_PUFF_SPEED = 170.0;         // still 0.75 s to react on the narrowest view
constexpr qreal CAP_BUBBLE_PAUSE = 1.6;         // the shortest: twice the time in the air
constexpr qreal CAP_DEADLINE_PER_SLOT = 0.625;  // the shortest: over six times a flat-out run
constexpr int CAP_SLOTS = 432;                  // nine days
constexpr qreal CAP_TRACTOR_SPEED = 120.0;      // half a run: time to see it coming
constexpr qreal CAP_SWARM_SPEED = 110.0;        // a swarm can always be outrun
constexpr qreal CAP_DUST_DEVIL_SPEED = 60.0;
constexpr qreal REACH_SHARE = 0.75;
static_assert(CAP_RISE <= REACH_SHARE * JUMP_V * JUMP_V / (2.0 * G_RISE),
              "a wall the jump cannot clear with room to spare");
static_assert(CAP_GUST <= 0.2 * MAX_RUN, "a headwind stronger than the caps allow for");
static_assert(CAP_KICK < JUMP_V, "an eddy that outdoes the jump");
static_assert(CAP_SWARM_SPEED < 0.5 * MAX_RUN, "a swarm the player cannot outrun");
static_assert(CAP_TRACTOR_SPEED <= 0.5 * MAX_RUN, "a tractor too fast to react to");

// how far a full-speed jump carries, landing rise px higher, into a headwind
qreal reach(qreal rise, qreal headwind)
{
    const qreal apex = JUMP_V * JUMP_V / (2.0 * G_RISE);
    const qreal fall = std::sqrt(std::max(2.0 * (apex - rise) / G_FALL, 0.0));
    return (MAX_RUN - headwind) * (JUMP_V / G_RISE + fall);
}

int wrapDoy(int doy)
{
    return ((doy - 1) % 365 + 365) % 365 + 1;
}

enum SiteKind { Forest, Grassland, Wetland, Cropland, Tundra, Savanna, Mountain, Desert };
enum Season { Spring, Summer, Autumn, Winter, WetSeason, DrySeason, SeasonCount };
const std::array<const char*, SeasonCount> SEASON_NAMES {
    "spring", "summer", "autumn", "winter", "wet season", "dry season"
};
const std::array<const char*, 3> WEATHER_NAMES { "clear", "mixed", "overcast" };
enum Rhythm { Ramp, Waves, Clusters, HardFirst };

// what kind of place a level is: its fluxes, its seasons, its hazards and its colours
struct Site
{
    const char* kind;
    qreal uptake;           // the strongest daily uptake, umol m-2 s-1
    qreal respiration;
    qreal dayAmplitude;     // hours the day lengthens from the equinox to midsummer
    int seasonPeak;         // day of the year
    int seasonWidth;        // days either side
    qreal seasonFloor;      // uptake out of season, as a share
    bool wetAndDry;         // a wet and a dry season rather than four
    bool snowy;             // its showers fall as snow when it is cold
    qreal pits;             // how many of each, against an ordinary site;
    qreal walls;            // counts only, never widths or heights, so the
    qreal zones;            // site cannot make a level impossible
    qreal reviewers;
    qreal puffs;
    qreal bubbles;          // added to the share of pits with a bubble
    qreal moving;
    qreal showers;
    qreal dangers;          // its own danger, against the level's count
    QColor fillTop;
    QColor fillBottom;
    QColor hills;           // the far horizon
    QColor scenery;         // what stands on the near one
    QStringList suffixes;
    QStringList countries;
};

const QList<Site>& sites()
{
    static const QList<Site> list {
        { "forest", 30.0, 5.0, 3.5, 190, 70, 0.15, false, false,
          1.0, 1.25, 0.8, 1.0, 0.8, 0.0, 1.0, 1.0, 1.0,
          QColor(34, 70, 40, 240), QColor(12, 24, 16, 250), QColor(40, 85, 55), QColor(26, 58, 36),
          { QStringLiteral("Mixed Forest"), QStringLiteral("Beech Forest"), QStringLiteral("Spruce Stand"),
            QStringLiteral("Pine Forest"), QStringLiteral("Oak Woodland"), QStringLiteral("Larch Forest") },
          { QStringLiteral("CH"), QStringLiteral("DE"), QStringLiteral("FI"), QStringLiteral("US"),
            QStringLiteral("CA"), QStringLiteral("FR") } },
        { "grassland", 20.0, 4.0, 3.5, 160, 80, 0.2, false, false,
          1.0, 0.8, 1.2, 1.3, 1.0, 0.0, 1.0, 1.0, 0.8,
          QColor(70, 100, 45, 240), QColor(25, 35, 18, 250), QColor(110, 150, 80), QColor(70, 104, 52),
          { QStringLiteral("Grassland"), QStringLiteral("Meadow"), QStringLiteral("Pasture"),
            QStringLiteral("Steppe"), QStringLiteral("Prairie"), QStringLiteral("Hay Meadow") },
          { QStringLiteral("CH"), QStringLiteral("DE"), QStringLiteral("US"), QStringLiteral("CN"),
            QStringLiteral("IE"), QStringLiteral("NL") } },
        { "wetland", 14.0, 3.5, 3.5, 200, 60, 0.1, false, false,
          1.25, 1.0, 1.0, 1.0, 1.0, 0.3, 1.3, 1.0, 0.6,
          QColor(30, 75, 75, 240), QColor(10, 28, 30, 250), QColor(60, 110, 100), QColor(58, 88, 58),
          { QStringLiteral("Fen"), QStringLiteral("Bog"), QStringLiteral("Marsh"),
            QStringLiteral("Mire"), QStringLiteral("Peatland"), QStringLiteral("Reed Bed") },
          { QStringLiteral("FI"), QStringLiteral("SE"), QStringLiteral("CA"), QStringLiteral("DE"),
            QStringLiteral("US"), QStringLiteral("IE") } },
        { "cropland", 40.0, 4.0, 3.5, 195, 30, 0.03, false, false,
          1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0, 0.5,
          QColor(110, 95, 40, 240), QColor(40, 32, 14, 250), QColor(150, 130, 70), QColor(104, 112, 52),
          { QStringLiteral("Winter Wheat"), QStringLiteral("Maize Field"), QStringLiteral("Barley Field"),
            QStringLiteral("Sugar Beet"), QStringLiteral("Rapeseed"), QStringLiteral("Potato Field") },
          { QStringLiteral("DE"), QStringLiteral("FR"), QStringLiteral("US"), QStringLiteral("BE"),
            QStringLiteral("DK"), QStringLiteral("IT") } },
        { "tundra", 8.0, 2.0, 7.0, 200, 45, 0.02, false, true,
          1.0, 1.0, 1.4, 0.7, 0.7, 0.15, 1.0, 1.0, 1.0,
          QColor(90, 95, 90, 240), QColor(30, 34, 34, 250), QColor(150, 158, 160), QColor(96, 102, 88),
          { QStringLiteral("Tundra"), QStringLiteral("Polygon Mire"), QStringLiteral("Heath"),
            QStringLiteral("Permafrost Plateau"), QStringLiteral("Lichen Heath"), QStringLiteral("Palsa") },
          { QStringLiteral("CA"), QStringLiteral("RU"), QStringLiteral("NO"), QStringLiteral("SE"),
            QStringLiteral("FI"), QStringLiteral("US") } },
        { "savanna", 25.0, 4.5, 1.5, 30, 60, 0.05, true, false,
          1.0, 1.0, 1.3, 1.0, 0.8, 0.0, 1.0, 1.0, 1.2,
          QColor(120, 95, 50, 240), QColor(45, 34, 18, 250), QColor(170, 140, 80), QColor(112, 90, 50),
          { QStringLiteral("Woodland Savanna"), QStringLiteral("Tree Savanna"), QStringLiteral("Bushveld"),
            QStringLiteral("Miombo"), QStringLiteral("Acacia Savanna"), QStringLiteral("Grass Savanna") },
          { QStringLiteral("ZA"), QStringLiteral("AU"), QStringLiteral("KE"), QStringLiteral("BR"),
            QStringLiteral("SN"), QStringLiteral("BW") } },
        { "mountain", 18.0, 3.5, 3.5, 200, 45, 0.05, false, true,
          1.0, 1.3, 1.2, 1.0, 1.6, 0.0, 1.0, 1.0, 1.2,
          QColor(70, 78, 85, 240), QColor(25, 28, 32, 250), QColor(125, 135, 155), QColor(72, 78, 84),
          { QStringLiteral("Alpine Meadow"), QStringLiteral("Subalpine Forest"), QStringLiteral("Mountain Pasture"),
            QStringLiteral("Scree Slope"), QStringLiteral("High Valley"), QStringLiteral("Larch Treeline") },
          { QStringLiteral("CH"), QStringLiteral("AT"), QStringLiteral("IT"), QStringLiteral("FR"),
            QStringLiteral("US"), QStringLiteral("NP") } },
        { "desert", 3.0, 1.5, 3.0, 80, 30, 0.05, false, false,
          1.2, 1.0, 1.5, 1.0, 1.0, 0.0, 1.0, 0.25, 0.8,
          QColor(150, 120, 75, 240), QColor(60, 45, 28, 250), QColor(210, 175, 120), QColor(150, 112, 72),
          { QStringLiteral("Shrub Desert"), QStringLiteral("Dune Field"), QStringLiteral("Salt Flat"),
            QStringLiteral("Stony Desert"), QStringLiteral("Creosote Bush"), QStringLiteral("Sand Sea") },
          { QStringLiteral("US"), QStringLiteral("AU"), QStringLiteral("IL"), QStringLiteral("NA"),
            QStringLiteral("MX"), QStringLiteral("CN") } }
    };
    return list;
}

// what a level from the seventh on may add, within the caps
enum Modifier : unsigned
{
    StormWeek = 1, MethaneWeek = 2, ReviewerConference = 4, NightShift = 8,
    PowerCut = 16, PolarNight = 32, DataRich = 64, DeadlineCrunch = 128
};
const std::array<std::pair<unsigned, const char*>, 8> MODIFIERS { {
    { StormWeek, "storm week" },
    { MethaneWeek, "methane week" },
    { ReviewerConference, "Reviewer 2 conference" },
    { NightShift, "night shift" },
    { PowerCut, "power cut" },
    { PolarNight, "polar night" },
    { DataRich, "data-rich week" },
    { DeadlineCrunch, "deadline crunch" }
} };

////////////////////////////////////////////////////////////////////////////////
// the figure

enum Frame { Stand, Run1, Run2, Run3, Jump, Crouch, Hurt, Cheer, FrameCount };

// what stands in for a frame the strip does not have
const std::array<int, FrameCount> FALLBACK { Stand, Stand, Run1, Run2, Stand, Stand, Stand, Jump };

// where the hard hat sits on each frame, from the feet, facing right
struct HatSpot
{
    qreal dx;
    qreal top;
};
const std::array<HatSpot, FrameCount> HAT_SPOTS { {
    { 0, -60 }, { 0, -60 }, { 0, -60 }, { 0, -60 },
    { -4, -53 }, { 0, -46 }, { 0, 0 }, { 0, -57 }
} };

// A figure strip: square cells side by side, 2x of the game's 64 x 64
// cell, feet on the bottom row. It is rescaled once per device pixel ratio,
// smoothly - it is a drawing, not pixel art at its own size - and then drawn
// 1:1. A strip that is missing or malformed leaves the game to draw its own
// figure; an empty cell borrows its neighbour's frame.
class SpriteSheet
{
public:
    // the strip of one of the two figures, or the other if that one is broken
    static SpriteSheet& sheet(int figure)
    {
        static SpriteSheet first(QStringLiteral(":/icons/figure"));
        static SpriteSheet second(QStringLiteral(":/icons/figure2"));
        SpriteSheet& wanted = figure == 1 ? second : first;
        SpriteSheet& other = figure == 1 ? first : second;
        return (wanted.valid() || !other.valid()) ? wanted : other;
    }

    bool valid() const { return valid_; }

    int runFrames() const
    {
        return present_[Run3] ? 3 : present_[Run2] ? 2 : 1;
    }

    const QPixmap& frame(int index, bool left, qreal dpr)
    {
        if (!qFuzzyCompare(dpr, dpr_))
            rebuild(dpr);
        while (!present_[index] && index != Stand)
            index = FALLBACK[index];
        return left ? left_[index] : right_[index];
    }

private:
    explicit SpriteSheet(const QString& path)
    {
        const QImage strip(path);
        if (strip.isNull() || !strip.hasAlphaChannel() || strip.height() < 32
            || strip.width() % strip.height() != 0)
            return;

        const int cell = strip.height();
        const int count = strip.width() / cell;
        if (count < 5 || count > FrameCount)
            return;

        // five cells are the minimum set: stand, two runs, jump, crouch
        const std::array<int, 5> five { Stand, Run1, Run2, Jump, Crouch };
        const QImage image = strip.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        for (int i = 0; i < count; ++i)
        {
            const int slot = (count == 5) ? five[i] : i;
            source_[slot] = image.copy(i * cell, 0, cell, cell);
            present_[slot] = !isEmpty(source_[slot]);
        }
        valid_ = present_[Stand];
    }

    static bool isEmpty(const QImage& image)
    {
        for (int y = 0; y < image.height(); ++y)
        {
            const auto line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
            for (int x = 0; x < image.width(); ++x)
            {
                if (qAlpha(line[x]) > 0)
                    return false;
            }
        }
        return true;
    }

    void rebuild(qreal dpr)
    {
        dpr_ = dpr;
        const int size = std::max(qRound(64 * dpr), 1);
        for (int i = 0; i < FrameCount; ++i)
        {
            if (!present_[i])
                continue;
            const QImage scaled = source_[i].scaled(size, size, Qt::IgnoreAspectRatio,
                                                    Qt::SmoothTransformation);
            right_[i] = QPixmap::fromImage(scaled);
            right_[i].setDevicePixelRatio(dpr);
            left_[i] = QPixmap::fromImage(scaled.transformed(QTransform::fromScale(-1, 1)));
            left_[i].setDevicePixelRatio(dpr);
        }
    }

    bool valid_ = false;
    std::array<QImage, FrameCount> source_;
    std::array<bool, FrameCount> present_ {};
    std::array<QPixmap, FrameCount> right_;
    std::array<QPixmap, FrameCount> left_;
    qreal dpr_ = 0.0;
};

////////////////////////////////////////////////////////////////////////////////
// words

// the loading sequence before the run
const QStringList& loadingSteps()
{
    static const QStringList list {
        QStringLiteral("Unpacking a week of half-hourly fluxes..."),
        QStringLiteral("Recording a week of CO2 at ten hertz..."),
        QStringLiteral("Digging pits where the data went missing..."),
        QStringLiteral("Raising the spikes into proper walls..."),
        QStringLiteral("Teaching Reviewer 2 to walk..."),
        QStringLiteral("Bubbling methane under the gaps..."),
        QStringLiteral("Mixing the convective boundary layer..."),
        QStringLiteral("Spinning up the eddies..."),
        QStringLiteral("Laying the sampling tubes..."),
        QStringLiteral("Measuring the time lag of every tube..."),
        QStringLiteral("Hiding coffee in the loggers..."),
        QStringLiteral("Polishing the hard hats..."),
        QStringLiteral("Charging the despiking filter..."),
        QStringLiteral("Scheduling the afternoon thunderstorm..."),
        QStringLiteral("Letting the night-time air settle..."),
        QStringLiteral("Climbing the flux tower to hang the flag..."),
        QStringLiteral("Calibrating the gas analyser, again..."),
        QStringLiteral("Rotating the coordinates until the wind behaves..."),
        QStringLiteral("Advecting a few stray puffs of CO2..."),
        QStringLiteral("Checking the stationarity of the platforms (they fail)..."),
        QStringLiteral("Booking the backup station halfway along..."),
        QStringLiteral("Setting the deadline to something unreasonable..."),
        QStringLiteral("Warming up the sonic heaters..."),
        QStringLiteral("Emptying the rain gauge..."),
        QStringLiteral("Asking the cows to stay off the cables..."),
        QStringLiteral("Filling the gaps... no, wait, those are the level..."),
        QStringLiteral("Generating a diurnal cycle with feeling..."),
        QStringLiteral("Placing the good half-hours where you can reach them..."),
        QStringLiteral("Applying the WPL correction to the scenery..."),
        QStringLiteral("Lacing up the field boots...")
    };
    return list;
}

// above the Start button, a new one each time
const QStringList& introLines()
{
    static const QStringList list {
        QStringLiteral("A week of raw data stands between you and the flux tower. Run along the trace, jump the gaps, and get the data submitted."),
        QStringLiteral("The data are the ground. The missing data are the holes in it. Mind the holes."),
        QStringLiteral("Somewhere at the end of this week sits a flux tower with a flag on it. Go and get it."),
        QStringLiteral("Seven days of half-hourly NEE, one technician, and a deadline. What could go wrong?"),
        QStringLiteral("Every wiggle is a tenth of a second. Every gap is a power cut. Every spike is somebody else's problem, until now."),
        QStringLiteral("Reviewer 2 is loose in the time series. Jump on them. It is the only language they understand."),
        QStringLiteral("Collect the good half-hours, dodge the advected CO2, and stay out of the methane."),
        QStringLiteral("The daytime fluxes go down, the night-time fluxes come up, and you go across."),
        QStringLiteral("A run across the carbon balance of one long, gappy week."),
        QStringLiteral("The convective boundary layer is in a mood today. Expect gusts."),
        QStringLiteral("The sampling tubes go somewhere. Press down on one and find out where, after a short time lag."),
        QStringLiteral("Hard hat on. Coffee optional. Coverage mandatory."),
        QStringLiteral("You are the last line of defence between a perfectly good dataset and the -9999s."),
        QStringLiteral("Navigate the fluxes. Literally."),
        QStringLiteral("The CO2 piles up at night and is drawn down by day, and the gaps go all the way down to -9999."),
        QStringLiteral("The eddies will throw you around. Learn which side lifts and which side drops."),
        QStringLiteral("A thermal will carry you if you hold jump. A gap will not."),
        QStringLiteral("The rain makes the trace slippery. The night makes it dark. The deadline makes it all worse."),
        QStringLiteral("Find the backup station halfway along, or start the whole week again."),
        QStringLiteral("Somewhere in this week is the half-hour that will make your annual sum. Probably."),
        QStringLiteral("Stomp, jump, collect, submit. The scientific method, roughly."),
        QStringLiteral("The flux tower is waiting. So is the funding agency."),
        QStringLiteral("The platforms that bob up and down failed the stationarity test. Ride them anyway."),
        QStringLiteral("Bonk the loggers from below; some of them still have something useful inside."),
        QStringLiteral("A hard hat gives you one more mistake. A despiking filter gives you something to throw."),
        QStringLiteral("The coffee makes you invincible for eight seconds. Science has confirmed this."),
        QStringLiteral("The pits are u*-filtered nights. Nobody comes back from a u*-filtered night."),
        QStringLiteral("Seven days, one tower, three lives, and six million samples."),
        QStringLiteral("The field season starts now. Try not to fall into it."),
        QStringLiteral("Your mission: maximise data coverage. Your obstacles: everything else.")
    };
    return list;
}

// the level card under the days; {place} is filled in
const QStringList& cardLines()
{
    static const QStringList list {
        QStringLiteral("A week at {place}"),
        QStringLiteral("Fluxes from {place}"),
        QStringLiteral("Field campaign: {place}"),
        QStringLiteral("Site visit to {place}"),
        QStringLiteral("Seven days in {place}"),
        QStringLiteral("{place}, half-hourly"),
        QStringLiteral("Raw data from {place}"),
        QStringLiteral("The {place} flux tower"),
        QStringLiteral("Somewhere near {place}"),
        QStringLiteral("{place}: level 2 data"),
        QStringLiteral("Uploading from {place}"),
        QStringLiteral("The long week at {place}"),
        QStringLiteral("{place}, gaps and all"),
        QStringLiteral("Night shift at {place}"),
        QStringLiteral("The carbon balance of {place}"),
        QStringLiteral("{place}, unprocessed"),
        QStringLiteral("Maintenance trip: {place}"),
        QStringLiteral("{place}, before quality control"),
        QStringLiteral("A windy week at {place}"),
        QStringLiteral("{place} and its many spikes"),
        QStringLiteral("Fieldwork at {place}"),
        QStringLiteral("{place}, straight off the logger"),
        QStringLiteral("The {place} footprint"),
        QStringLiteral("Seven nights of advection at {place}"),
        QStringLiteral("{place}: the missing week"),
        QStringLiteral("Live from {place}"),
        QStringLiteral("{place}, in 10 Hz"),
        QStringLiteral("Data rescue at {place}"),
        QStringLiteral("The {place} time series"),
        QStringLiteral("{place}, with feeling")
    };
    return list;
}

// every tenth good half-hour; %1 is the count
const QStringList& streakPops()
{
    static const QStringList list {
        QStringLiteral("%1 GOOD HALF-HOURS!"),
        QStringLiteral("%1 AND COUNTING!"),
        QStringLiteral("COVERAGE: %1!"),
        QStringLiteral("%1 FLAG-0 PERIODS!"),
        QStringLiteral("%1 IN THE BAG!"),
        QStringLiteral("%1 USABLE HALF-HOURS!"),
        QStringLiteral("%1! THE ANNUAL SUM SMILES!"),
        QStringLiteral("%1 CLEAN AVERAGES!"),
        QStringLiteral("%1, ALL STATIONARY!"),
        QStringLiteral("%1 PASSED QC!"),
        QStringLiteral("%1 FOR THE ARCHIVE!"),
        QStringLiteral("%1! NO GAP-FILLING NEEDED!"),
        QStringLiteral("%1 WELL-MIXED HALF-HOURS!"),
        QStringLiteral("%1 OF THE GOOD STUFF!"),
        QStringLiteral("%1 HALF-HOURS SAVED!"),
        QStringLiteral("%1! THE PI WILL BE PLEASED!"),
        QStringLiteral("%1 WORTH PUBLISHING!"),
        QStringLiteral("%1 SOLID FLUXES!"),
        QStringLiteral("%1! THE DATA PAPER WRITES ITSELF!"),
        QStringLiteral("%1 GOOD ONES!"),
        QStringLiteral("%1, ZERO SPIKES!"),
        QStringLiteral("%1 IN THE FOOTPRINT!"),
        QStringLiteral("%1! FLUXNET-READY!"),
        QStringLiteral("%1 HIGH-QUALITY PERIODS!"),
        QStringLiteral("%1 AVERAGING PERIODS!"),
        QStringLiteral("%1! KEEP GOING!"),
        QStringLiteral("%1 DATA POINTS RESCUED!"),
        QStringLiteral("%1! THE OGIVE CONVERGES!"),
        QStringLiteral("%1 WITHOUT A -9999!"),
        QStringLiteral("%1 COVERAGE CREDITS!")
    };
    return list;
}

// Reviewer 2, stomped
const QStringList& reviewerPops()
{
    static const QStringList list {
        QStringLiteral("COMMENT ADDRESSED!"),
        QStringLiteral("REBUTTED!"),
        QStringLiteral("MAJOR REVISION: DONE!"),
        QStringLiteral("REVIEWER 2: FLATTENED!"),
        QStringLiteral("POINT TAKEN!"),
        QStringLiteral("WE THANK THE REVIEWER!"),
        QStringLiteral("CITATION ADDED!"),
        QStringLiteral("CLARIFIED!"),
        QStringLiteral("RESPONSE LETTER SENT!"),
        QStringLiteral("ACCEPTED WITH MINOR!"),
        QStringLiteral("OBJECTION OVERRULED!"),
        QStringLiteral("STOMPED INTO ACCEPTANCE!"),
        QStringLiteral("THE REVIEWER IS NOW SATISFIED!"),
        QStringLiteral("SEE LINE 245!"),
        QStringLiteral("AS SUGGESTED, WE JUMPED!"),
        QStringLiteral("DONE, PLEASE SEE TRACKED CHANGES!"),
        QStringLiteral("REVIEW: SQUASHED!"),
        QStringLiteral("NO FURTHER COMMENTS!"),
        QStringLiteral("THE EDITOR AGREES!"),
        QStringLiteral("FIGURE 3 IMPROVED!"),
        QStringLiteral("UNCERTAINTY QUANTIFIED!"),
        QStringLiteral("WE RESPECTFULLY DISAGREE!"),
        QStringLiteral("SUPPLEMENT EXPANDED!"),
        QStringLiteral("ONE LESS REVIEWER!"),
        QStringLiteral("PEER-REVIEWED, PEER-STOMPED!"),
        QStringLiteral("COMMENT 47: RESOLVED!"),
        QStringLiteral("THE MANUSCRIPT LIVES!"),
        QStringLiteral("RESUBMITTED!"),
        QStringLiteral("FLATTENED, AS REQUESTED!"),
        QStringLiteral("REVIEWER 3 IS NEXT!")
    };
    return list;
}

// an ebullition bubble, popped from above
const QStringList& bubblePops()
{
    static const QStringList list {
        QStringLiteral("BUBBLE POPPED!"),
        QStringLiteral("EBULLITION ENDED!"),
        QStringLiteral("CH4 VENTED!"),
        QStringLiteral("NO MORE BURPS!"),
        QStringLiteral("METHANE MANAGED!"),
        QStringLiteral("POP!"),
        QStringLiteral("WETLAND SILENCED!"),
        QStringLiteral("BUBBLE FLUX: ZERO!"),
        QStringLiteral("THE PEAT SIGHS!"),
        QStringLiteral("STOMPED THE STORAGE!"),
        QStringLiteral("METHANOGENS DISAPPOINTED!"),
        QStringLiteral("SPLASH!"),
        QStringLiteral("GLUG!"),
        QStringLiteral("THE SEDIMENT CALMS DOWN!"),
        QStringLiteral("A FLUX EVENT, CANCELLED!"),
        QStringLiteral("HOT SPOT COOLED!"),
        QStringLiteral("HOT MOMENT OVER!"),
        QStringLiteral("THE LGR IS RELIEVED!"),
        QStringLiteral("NO SPIKE IN THE CH4!"),
        QStringLiteral("PLOP!"),
        QStringLiteral("DE-GASSED!"),
        QStringLiteral("BUBBLE BURST!"),
        QStringLiteral("THE CHAMBER PEOPLE ARE JEALOUS!"),
        QStringLiteral("ONE LESS OUTLIER!"),
        QStringLiteral("METHANE: CONTAINED!"),
        QStringLiteral("THE POND IS QUIET!"),
        QStringLiteral("NO EBULLITION ON MY WATCH!"),
        QStringLiteral("SQUISH!"),
        QStringLiteral("BUBBLE DESPIKED!"),
        QStringLiteral("THE SWAMP LOSES ONE!")
    };
    return list;
}

// an advection puff, stomped
const QStringList& advectionPops()
{
    static const QStringList list {
        QStringLiteral("ADVECTION CANCELLED!"),
        QStringLiteral("HORIZONTAL FLUX: ZERO!"),
        QStringLiteral("PUFF DISPERSED!"),
        QStringLiteral("DRAINAGE FLOW DRAINED!"),
        QStringLiteral("THE CO2 GOES HOME!"),
        QStringLiteral("STORAGE TERM HANDLED!"),
        QStringLiteral("NO MORE LATERAL TRANSPORT!"),
        QStringLiteral("COLD POOL BROKEN!"),
        QStringLiteral("PLUME POPPED!"),
        QStringLiteral("THE CLOSURE IMPROVES!"),
        QStringLiteral("ADVECTED INTO OBLIVION!"),
        QStringLiteral("THE NIGHT IS SAFE!"),
        QStringLiteral("PUFF!"),
        QStringLiteral("NEIGHBOUR'S CO2 RETURNED!"),
        QStringLiteral("THE FOOTPRINT IS CLEAN!"),
        QStringLiteral("A KATABATIC CLEAN-UP!"),
        QStringLiteral("MASS BALANCE RESTORED!"),
        QStringLiteral("NO VERTICAL ADVECTION EITHER!"),
        QStringLiteral("THE PLANAR FIT APPROVES!"),
        QStringLiteral("DISSIPATED!"),
        QStringLiteral("CONCENTRATION NORMALISED!"),
        QStringLiteral("THE PROFILE SYSTEM CHEERS!"),
        QStringLiteral("ONE LESS NIGHT-TIME PROBLEM!"),
        QStringLiteral("SCATTERED!"),
        QStringLiteral("MIXED OUT!"),
        QStringLiteral("THE BIAS SHRINKS!"),
        QStringLiteral("FLATTENED THE CLOUD!"),
        QStringLiteral("AWAY WITH THE DRAINAGE!"),
        QStringLiteral("THE TOWER BREATHES AGAIN!"),
        QStringLiteral("WHOOSH... GONE!")
    };
    return list;
}

// anything taken out by the filter or by coffee
const QStringList& despikePops()
{
    static const QStringList list {
        QStringLiteral("DESPIKED!"),
        QStringLiteral("FILTERED!"),
        QStringLiteral("FLAGGED!"),
        QStringLiteral("REMOVED!"),
        QStringLiteral("MEDIAN'D!"),
        QStringLiteral("3.5 SIGMA!"),
        QStringLiteral("OUTLIER DOWN!"),
        QStringLiteral("HAMPEL'D!"),
        QStringLiteral("CLEANED!"),
        QStringLiteral("QC'D!"),
        QStringLiteral("REJECTED!"),
        QStringLiteral("BLASTED!"),
        QStringLiteral("NOISE REDUCED!"),
        QStringLiteral("ZAPPED!"),
        QStringLiteral("HIGH-PASSED!"),
        QStringLiteral("DETRENDED!"),
        QStringLiteral("GONE!"),
        QStringLiteral("SENT TO THE RAW ARCHIVE!"),
        QStringLiteral("FLAG 2!"),
        QStringLiteral("EXCLUDED!"),
        QStringLiteral("SMOOTHED!"),
        QStringLiteral("TAKEN OUT!"),
        QStringLiteral("PURGED!"),
        QStringLiteral("CORRECTED!"),
        QStringLiteral("DROPPED!"),
        QStringLiteral("MASKED!"),
        QStringLiteral("BINNED!"),
        QStringLiteral("SPIKE-FREE!"),
        QStringLiteral("DELETED!"),
        QStringLiteral("NO LONGER SIGNIFICANT!")
    };
    return list;
}

// a half-hour out of a logger block
const QStringList& blockPops()
{
    static const QStringList list {
        QStringLiteral("DATA RECOVERED!"),
        QStringLiteral("FROM THE BUFFER!"),
        QStringLiteral("CARD READ!"),
        QStringLiteral("ONE MORE HALF-HOUR!"),
        QStringLiteral("THE LOGGER HAD IT!"),
        QStringLiteral("DOWNLOADED!"),
        QStringLiteral("RETRIEVED!"),
        QStringLiteral("FOUND IN THE BACKUP!"),
        QStringLiteral("CRC OK!"),
        QStringLiteral("FILE INTACT!"),
        QStringLiteral("STILL ON THE CARD!"),
        QStringLiteral("RESCUED!"),
        QStringLiteral("BINARY DECODED!"),
        QStringLiteral("GHG FILE OPENED!"),
        QStringLiteral("THE MODEM WORKED!"),
        QStringLiteral("CHECKSUM PASSED!"),
        QStringLiteral("TIMESTAMP CORRECT!"),
        QStringLiteral("NOT CORRUPT AFTER ALL!"),
        QStringLiteral("ARCHIVED!"),
        QStringLiteral("SYNCHRONISED!"),
        QStringLiteral("UNZIPPED!"),
        QStringLiteral("METADATA INCLUDED!"),
        QStringLiteral("10 Hz ALL THE WAY!"),
        QStringLiteral("THE LOGGER COUGHS UP DATA!"),
        QStringLiteral("SALVAGED!"),
        QStringLiteral("MEMORY DUMP!"),
        QStringLiteral("COPIED!"),
        QStringLiteral("THE FTP ANSWERED!"),
        QStringLiteral("BYTES RECOVERED!"),
        QStringLiteral("+1 HALF-HOUR!")
    };
    return list;
}

// the hard hat
const QStringList& hatPops()
{
    static const QStringList list {
        QStringLiteral("HARD HAT ON!"),
        QStringLiteral("SAFETY FIRST!"),
        QStringLiteral("RISK ASSESSMENT PASSED!"),
        QStringLiteral("HEAD PROTECTED!"),
        QStringLiteral("PPE ACQUIRED!"),
        QStringLiteral("TOWER-CLIMBING READY!"),
        QStringLiteral("ONE MORE MISTAKE ALLOWED!"),
        QStringLiteral("THE SAFETY OFFICER APPROVES!"),
        QStringLiteral("HELMET EQUIPPED!"),
        QStringLiteral("FIELD-READY!"),
        QStringLiteral("CERTIFIED FOR HEIGHTS!"),
        QStringLiteral("SAFETY BRIEFING DONE!"),
        QStringLiteral("NO MORE BUMPED HEADS!"),
        QStringLiteral("THE HAT FITS!"),
        QStringLiteral("REGULATION HEADWEAR!"),
        QStringLiteral("ARMOURED!"),
        QStringLiteral("HEALTH AND SAFETY: SATISFIED!"),
        QStringLiteral("NOW WITH CHIN STRAP!"),
        QStringLiteral("HI-VIS NOT INCLUDED!"),
        QStringLiteral("STYLE AND SAFETY!"),
        QStringLiteral("PROTECTED FROM FALLING SENSORS!"),
        QStringLiteral("THE TOWER CREW SALUTES!"),
        QStringLiteral("HELMET HAIR INCOMING!"),
        QStringLiteral("ONE HIT SPARED!"),
        QStringLiteral("SAFETY: LEVEL UP!"),
        QStringLiteral("HARNESS NEXT!"),
        QStringLiteral("DROP-TESTED!"),
        QStringLiteral("THE HAT OF RESPONSIBILITY!"),
        QStringLiteral("SAFE AND SOUND!"),
        QStringLiteral("BUMP CAP UPGRADED!")
    };
    return list;
}

// the despiking filter
const QStringList& filterPops()
{
    static const QStringList list {
        QStringLiteral("DESPIKING FILTER ARMED!"),
        QStringLiteral("PRESS X TO DESPIKE!"),
        QStringLiteral("FILTER LOADED!"),
        QStringLiteral("QC ON DEMAND!"),
        QStringLiteral("MEDIAN FILTER READY!"),
        QStringLiteral("VICKERS AND MAHRT, ENGAGED!"),
        QStringLiteral("SIGMA THRESHOLD SET!"),
        QStringLiteral("SPIKES BEWARE!"),
        QStringLiteral("THE FILTER IS HOT!"),
        QStringLiteral("NOW THROWING QC!"),
        QStringLiteral("OUTLIER LAUNCHER!"),
        QStringLiteral("HIGH-PASS MODE!"),
        QStringLiteral("REMOVE ON SIGHT!"),
        QStringLiteral("THE SCRIPT IS RUNNING!"),
        QStringLiteral("FILTER CALIBRATED!"),
        QStringLiteral("FIRE AT WILL (X)!"),
        QStringLiteral("DESPIKE MODE!"),
        QStringLiteral("PULSES CHARGED!"),
        QStringLiteral("THE HAMPEL FILTER AWAKENS!"),
        QStringLiteral("DATA CLEANING, MOBILE!"),
        QStringLiteral("PORTABLE QUALITY CONTROL!"),
        QStringLiteral("FLAGGING GUN!"),
        QStringLiteral("SPIKE REMOVAL, HANDHELD!"),
        QStringLiteral("THE FILTER IS YOURS!"),
        QStringLiteral("ARMED AND DESPIKY!"),
        QStringLiteral("QC BOLTS READY!"),
        QStringLiteral("REVIEWERS, TAKE COVER!"),
        QStringLiteral("SPECTRAL WEAPON!"),
        QStringLiteral("LOW-PASS, HIGH-DAMAGE!"),
        QStringLiteral("THE OUTLIERS TREMBLE!")
    };
    return list;
}

// the coffee
const QStringList& coffeePops()
{
    static const QStringList list {
        QStringLiteral("COFFEE!"),
        QStringLiteral("CAFFEINATED!"),
        QStringLiteral("INVINCIBLE FOR 8 SECONDS!"),
        QStringLiteral("FIELD COFFEE: STRONG!"),
        QStringLiteral("THERMOS FOUND!"),
        QStringLiteral("DOUBLE ESPRESSO!"),
        QStringLiteral("BREW-POWERED!"),
        QStringLiteral("THE HUT COFFEE!"),
        QStringLiteral("JITTERS ENGAGED!"),
        QStringLiteral("NOTHING CAN STOP YOU NOW!"),
        QStringLiteral("CAFFEINE FLUX: MAXIMUM!"),
        QStringLiteral("8 SECONDS OF GLORY!"),
        QStringLiteral("THE MAGIC BEANS!"),
        QStringLiteral("INSTANT, BUT WORKS!"),
        QStringLiteral("COFFEE BREAK... NOT!"),
        QStringLiteral("HYPERCAFFEINATED!"),
        QStringLiteral("THE TECHNICIAN'S SECRET!"),
        QStringLiteral("FUEL FOR FIELDWORK!"),
        QStringLiteral("STILL WARM!"),
        QStringLiteral("BLACK, NO SUGAR!"),
        QStringLiteral("SPEED BOOST!"),
        QStringLiteral("THE MORNING BREW!"),
        QStringLiteral("COFFEE BEFORE QC!"),
        QStringLiteral("THE GRAD STUDENT SPECIAL!"),
        QStringLiteral("CAFFEINE SPIKE (THE GOOD KIND)!"),
        QStringLiteral("UNSTOPPABLE!"),
        QStringLiteral("JAVA ACQUIRED!"),
        QStringLiteral("PERCOLATED POWER!"),
        QStringLiteral("THE CUP OF INVINCIBILITY!"),
        QStringLiteral("REFILL!")
    };
    return list;
}

// a hard hat or filter knocked off
const QStringList& hurtPops()
{
    static const QStringList list {
        QStringLiteral("OUCH!"),
        QStringLiteral("HAT LOST!"),
        QStringLiteral("THAT HURT!"),
        QStringLiteral("SAFETY INCIDENT!"),
        QStringLiteral("INCIDENT REPORT REQUIRED!"),
        QStringLiteral("PPE DAMAGED!"),
        QStringLiteral("CAREFUL!"),
        QStringLiteral("DOWNGRADED!"),
        QStringLiteral("NEAR MISS!"),
        QStringLiteral("CLOSE ONE!"),
        QStringLiteral("THE HAT TOOK IT!"),
        QStringLiteral("FILTER DROPPED!"),
        QStringLiteral("BACK TO BASICS!"),
        QStringLiteral("THAT'LL BRUISE!"),
        QStringLiteral("STILL STANDING!"),
        QStringLiteral("ONE MORE AND YOU'RE OUT!"),
        QStringLiteral("LESSON LEARNED!"),
        QStringLiteral("THE SAFETY OFFICER SIGHS!"),
        QStringLiteral("WATCH OUT!"),
        QStringLiteral("HELMET SACRIFICED!"),
        QStringLiteral("SHAKEN, NOT STIRRED!"),
        QStringLiteral("SCRATCHED!"),
        QStringLiteral("BUMPED!"),
        QStringLiteral("THAT WAS A WARNING!"),
        QStringLiteral("STRIKE ONE!"),
        QStringLiteral("ARMOUR GONE!"),
        QStringLiteral("SMALL AGAIN!"),
        QStringLiteral("DEMOTED!"),
        QStringLiteral("FIRST AID KIT, PLEASE!"),
        QStringLiteral("DUCK NEXT TIME!")
    };
    return list;
}

// a hundred good half-hours: one more life
const QStringList& oneUpPops()
{
    static const QStringList list {
        QStringLiteral("1UP!"),
        QStringLiteral("EXTRA LIFE!"),
        QStringLiteral("CONTRACT EXTENDED!"),
        QStringLiteral("FUNDING RENEWED!"),
        QStringLiteral("ANOTHER FIELD SEASON!"),
        QStringLiteral("ONE MORE TRY!"),
        QStringLiteral("POSTDOC GRANTED!"),
        QStringLiteral("BACKUP TECHNICIAN!"),
        QStringLiteral("SPARE SONIC!"),
        QStringLiteral("THE GRANT CAME THROUGH!"),
        QStringLiteral("REPLACEMENT DEPLOYED!"),
        QStringLiteral("A SECOND CHANCE!"),
        QStringLiteral("NO-COST EXTENSION!"),
        QStringLiteral("LIFE +1!"),
        QStringLiteral("THE PI BELIEVES IN YOU!"),
        QStringLiteral("SABBATICAL EARNED!"),
        QStringLiteral("REDUNDANCY ACHIEVED!"),
        QStringLiteral("100 HALF-HOURS: REWARDED!"),
        QStringLiteral("THE ARCHIVE THANKS YOU!"),
        QStringLiteral("BONUS LIFE!"),
        QStringLiteral("TENURE TRACK!"),
        QStringLiteral("A NEW MAST!"),
        QStringLiteral("RESPAWN TOKEN!"),
        QStringLiteral("HIRED AGAIN!"),
        QStringLiteral("STILL IN THE PROJECT!"),
        QStringLiteral("COVERAGE PAYS OFF!"),
        QStringLiteral("THE PROJECT LIVES ON!"),
        QStringLiteral("EXTRA CONTINUE!"),
        QStringLiteral("THE REVIEW PANEL APPROVED!"),
        QStringLiteral("ONE MORE LIFE, NO PAPERWORK!")
    };
    return list;
}

// the backup station halfway along
const QStringList& checkpointPops()
{
    static const QStringList list {
        QStringLiteral("BACKUP SAVED!"),
        QStringLiteral("CHECKPOINT!"),
        QStringLiteral("HALFWAY THERE!"),
        QStringLiteral("DATA BACKED UP!"),
        QStringLiteral("PROGRESS SYNCHRONISED!"),
        QStringLiteral("SAVED TO THE SERVER!"),
        QStringLiteral("MIRRORED!"),
        QStringLiteral("SNAPSHOT TAKEN!"),
        QStringLiteral("FIRST HALF ARCHIVED!"),
        QStringLiteral("RESTORE POINT CREATED!"),
        QStringLiteral("THE BACKUP STATION WAVES!"),
        QStringLiteral("COPIED TO THE CLOUD!"),
        QStringLiteral("SAFE COPY!"),
        QStringLiteral("MID-WEEK UPLOAD!"),
        QStringLiteral("REDUNDANT STORAGE!"),
        QStringLiteral("HALF A WEEK SECURED!"),
        QStringLiteral("NO GOING BACK NOW!"),
        QStringLiteral("THE LAPTOP SURVIVED!"),
        QStringLiteral("USB STICK FILLED!"),
        QStringLiteral("DAY 4: SAVED!"),
        QStringLiteral("SECOND COPY MADE!"),
        QStringLiteral("THE RAID ARRAY HUMS!"),
        QStringLiteral("BACKED UP, FOR ONCE!"),
        QStringLiteral("THE IT DEPARTMENT IS PROUD!"),
        QStringLiteral("SAVE GAME!"),
        QStringLiteral("HALF THE FLUXES ARE SAFE!"),
        QStringLiteral("CHECKSUM WRITTEN!"),
        QStringLiteral("THE BACKUP TAPE SPINS!"),
        QStringLiteral("RESPAWN POINT SET!"),
        QStringLiteral("ONWARDS!")
    };
    return list;
}

// at the foot of the tower
const QStringList& flagPops()
{
    static const QStringList list {
        QStringLiteral("DATA SUBMITTED!"),
        QStringLiteral("UPLOADED TO THE ARCHIVE!"),
        QStringLiteral("THE WEEK IS IN!"),
        QStringLiteral("FLUXNET RECEIVED IT!"),
        QStringLiteral("ICOS CLASS 1!"),
        QStringLiteral("SUBMISSION CONFIRMED!"),
        QStringLiteral("THE FLAG IS DOWN!"),
        QStringLiteral("QC 0 ACHIEVED!"),
        QStringLiteral("DEADLINE MET!"),
        QStringLiteral("THE TOWER IS YOURS!"),
        QStringLiteral("DATA PAPER: READY!"),
        QStringLiteral("SENT TO THE PI!"),
        QStringLiteral("ARCHIVED FOREVER!"),
        QStringLiteral("THE WEEK IS PROCESSED!"),
        QStringLiteral("LEVEL 2 COMPLETE!"),
        QStringLiteral("THE FTP SAYS 226!"),
        QStringLiteral("CARBON BALANCE CLOSED!"),
        QStringLiteral("THE LOGGER HUT WELCOMES YOU!"),
        QStringLiteral("MISSION ACCOMPLISHED!"),
        QStringLiteral("THE FUNDER IS HAPPY!"),
        QStringLiteral("DOI MINTED!"),
        QStringLiteral("THE FLUXES ARE HOME!"),
        QStringLiteral("TIME FOR A FIELD BEER!"),
        QStringLiteral("WEEK COMPLETE!"),
        QStringLiteral("THE TOWER CLIMB WAS WORTH IT!"),
        QStringLiteral("ALL HALF-HOURS ACCOUNTED FOR!"),
        QStringLiteral("THE PRINCESS IS IN ANOTHER TOWER!"),
        QStringLiteral("CHECK THE INBOX: ACCEPTED!"),
        QStringLiteral("PROCESSING DONE!"),
        QStringLiteral("SUBMITTED, AT LAST!")
    };
    return list;
}

// the level card after falling into a pit
const QStringList& pitDeathLines()
{
    static const QStringList list {
        QStringLiteral("Lost in a gap. The gap-filler will invent you."),
        QStringLiteral("You fell into the missing data. It does not give anything back."),
        QStringLiteral("u*-filtered."),
        QStringLiteral("You are now -9999."),
        QStringLiteral("The power cut took you with it."),
        QStringLiteral("That gap was longer than it looked. They always are."),
        QStringLiteral("Into the void where the modem's data go."),
        QStringLiteral("You became a NaN."),
        QStringLiteral("The logger was full, and so was the pit."),
        QStringLiteral("Marginal distribution sampling could not save you."),
        QStringLiteral("You fell through the night-time filter."),
        QStringLiteral("Missing, presumed gap-filled."),
        QStringLiteral("The data went missing, and so did you."),
        QStringLiteral("A drop-out. A literal one."),
        QStringLiteral("The SD card is blank, and so is your record."),
        QStringLiteral("You jumped short. The time series did not."),
        QStringLiteral("Somewhere below, a stack of missing half-hours welcomes you."),
        QStringLiteral("The gap was u*-filtered. You were not u*-proof."),
        QStringLiteral("Down the hole with the battery that died."),
        QStringLiteral("You are now part of the coverage statistics."),
        QStringLiteral("The look-up table has no entry for you."),
        QStringLiteral("Your half-hour has been flagged as unrecoverable."),
        QStringLiteral("Even the neural network cannot interpolate you."),
        QStringLiteral("The mice ate the cable. Then you fell in."),
        QStringLiteral("Gap length: one technician."),
        QStringLiteral("You have been excluded from the annual sum."),
        QStringLiteral("That is not how gap-filling works."),
        QStringLiteral("Fell into July. July has a lot of gaps."),
        QStringLiteral("A hole in the record, and now a hole in your lives."),
        QStringLiteral("The pit was missing data. Now so is your character.")
    };
    return list;
}

// the level card after meeting a foe without a hat
const QStringList& foeDeathLines()
{
    static const QStringList list {
        QStringLiteral("Reviewer 2 recommended rejection."),
        QStringLiteral("Advected away by a puff of somebody else's CO2."),
        QStringLiteral("The methane got you. It always does, eventually."),
        QStringLiteral("Rejected without review."),
        QStringLiteral("You were flagged as an outlier."),
        QStringLiteral("Desk-rejected."),
        QStringLiteral("The reviewer wanted more data. You were the data."),
        QStringLiteral("Ebullition: 1, technician: 0."),
        QStringLiteral("Lateral advection carried you out of the footprint."),
        QStringLiteral("You should have jumped on it. That is the whole game."),
        QStringLiteral("Major revisions. Start again."),
        QStringLiteral("The storage term stored you."),
        QStringLiteral("The comments were too many. You were too few."),
        QStringLiteral("Bubbled."),
        QStringLiteral("A hard hat would have helped. Just saying."),
        QStringLiteral("Out of scope for this journal."),
        QStringLiteral("The cold pool swallowed you whole."),
        QStringLiteral("You failed the stationarity test."),
        QStringLiteral("Reviewer 2 has no further questions."),
        QStringLiteral("Sent back for resubmission."),
        QStringLiteral("You met the drainage flow. The drainage flow won."),
        QStringLiteral("Methane: one more reason to fear wetlands."),
        QStringLiteral("The reviewer said your jump was not novel."),
        QStringLiteral("Respawning with major corrections."),
        QStringLiteral("You were despiked."),
        QStringLiteral("That puff was 800 ppm of bad news."),
        QStringLiteral("Rejected: insufficient jumping."),
        QStringLiteral("The bubble burst. On you."),
        QStringLiteral("Reviewer 2 is still walking. You are not."),
        QStringLiteral("Advection: the night-time menace, now with your name on it.")
    };
    return list;
}

// on the pause overlay
const QStringList& pauseLines()
{
    static const QStringList list {
        QStringLiteral("Coffee break."),
        QStringLiteral("The logger is buffering."),
        QStringLiteral("The fluxes will wait. The deadline will not."),
        QStringLiteral("Time out for a quick calibration."),
        QStringLiteral("Paused, like a modem waiting for signal."),
        QStringLiteral("Even eddies need a rest."),
        QStringLiteral("The data are not going anywhere. Probably."),
        QStringLiteral("Lunch in the field hut."),
        QStringLiteral("Somebody unplugged the logger."),
        QStringLiteral("Stretch your legs. The tower is still there."),
        QStringLiteral("A moment of stationarity."),
        QStringLiteral("Paused for the safety briefing."),
        QStringLiteral("The averaging period is on hold."),
        QStringLiteral("Checking emails, briefly. Very briefly."),
        QStringLiteral("Hold on, the battery is changing."),
        QStringLiteral("Time stands still. The flux does not."),
        QStringLiteral("Waiting for the rain to stop."),
        QStringLiteral("The boundary layer holds its breath."),
        QStringLiteral("A short break for quality control."),
        QStringLiteral("Paused. The cows are crossing."),
        QStringLiteral("The flux tower is patient."),
        QStringLiteral("Taking a reading by hand."),
        QStringLiteral("Re-zeroing the analyser."),
        QStringLiteral("Downloading the card, please wait."),
        QStringLiteral("The deadline clock has stopped, for now."),
        QStringLiteral("Even Reviewer 2 takes weekends."),
        QStringLiteral("Paused for a firmware update."),
        QStringLiteral("Catching your breath at the backup station."),
        QStringLiteral("The wind has stopped. So have you."),
        QStringLiteral("Out of office.")
    };
    return list;
}

// the italic line under the result
const QStringList& resultTags()
{
    static const QStringList list {
        QStringLiteral("Somewhere, a flux tower is quietly proud of you."),
        QStringLiteral("The week is over. Next week starts on Monday, with new gaps."),
        QStringLiteral("Your annual sum is a little more certain now."),
        QStringLiteral("The eddies have been navigated. Mostly."),
        QStringLiteral("No technicians were permanently harmed in this run."),
        QStringLiteral("The data are submitted. The paper is not. Back to work."),
        QStringLiteral("Every gap you jumped is one less for the gap-filler."),
        QStringLiteral("Reviewer 2 will be back for the revised version."),
        QStringLiteral("The boundary layer is calm again, until tomorrow afternoon."),
        QStringLiteral("Remember: the CO2 is high at night, low by day, and the gaps are everywhere."),
        QStringLiteral("The flag is down, the hut is warm, and the coffee is cold."),
        QStringLiteral("Another week of fluxes, another week of field boots."),
        QStringLiteral("The time lag of the sampling tubes was within specification."),
        QStringLiteral("The methane bubbles will keep bubbling. That is what they do."),
        QStringLiteral("Data coverage is a state of mind."),
        QStringLiteral("Somewhere a postdoc is plotting your run as a cumulative sum."),
        QStringLiteral("Your footprint was 100 % inside the target ecosystem."),
        QStringLiteral("The night-time advection remains an open research question."),
        QStringLiteral("Navigating the fluxes: harder than it looks in the methods section."),
        QStringLiteral("The tower crew is buying the first round."),
        QStringLiteral("This run has been archived with DOI 10.0000/please-go-back-to-work."),
        QStringLiteral("The fluxes are processed. Your inbox is not."),
        QStringLiteral("All good things come in half-hourly averages."),
        QStringLiteral("The energy balance still does not close. It never will."),
        QStringLiteral("Your despiking filter has been returned to the equipment room."),
        QStringLiteral("The eddy covariance method thanks you for your service."),
        QStringLiteral("The u* threshold remains unimpressed."),
        QStringLiteral("You have been added to the acknowledgements."),
        QStringLiteral("The next week of data has already started arriving."),
        QStringLiteral("Back to the real fluxes, which are less forgiving.")
    };
    return list;
}

// added when the run had full coverage or no lives lost
const QStringList& flawlessTemplates()
{
    static const QStringList list {
        QStringLiteral("Not a single life lost. {who} wants to know your secret."),
        QStringLiteral("A flawless week. {place} has never had data this complete."),
        QStringLiteral("Full marks. {tool} could not have done it better."),
        QStringLiteral("Perfect. Reviewer 2 has nothing left to say, which has never happened before."),
        QStringLiteral("Flawless. The data paper practically writes itself."),
        QStringLiteral("No mistakes at all. {who} is framing the cumulative sum."),
        QStringLiteral("A clean run. Even the u* filter is impressed."),
        QStringLiteral("Immaculate. The gap-filler had nothing to do and went home early."),
        QStringLiteral("Perfect coverage, or near enough to fool the reviewers."),
        QStringLiteral("Not a scratch on the hard hat. The safety officer is in tears."),
        QStringLiteral("Flawless. {place} is naming a half-hour after you."),
        QStringLiteral("All lives intact. The field insurance premium just went down."),
        QStringLiteral("A textbook run. It will be in the next edition of the textbook."),
        QStringLiteral("Perfect. {who} has asked you to run the next field campaign."),
        QStringLiteral("Unblemished. The quality flags are all zero, and so are your losses."),
        QStringLiteral("No losses. The PI is drafting a nomination letter."),
        QStringLiteral("Clean. Like a sonic after the rain."),
        QStringLiteral("Flawless. The archive curators wept."),
        QStringLiteral("Not one life lost. {tool} wants your autograph."),
        QStringLiteral("A perfect week. The eddies are filing a complaint."),
        QStringLiteral("Zero lives lost. Statistically improbable. Scientifically delightful."),
        QStringLiteral("Immaculate. {place} will remember this week."),
        QStringLiteral("Perfect. Your annual sum has the smallest error bar in history."),
        QStringLiteral("Flawless. The methods section can be one sentence long."),
        QStringLiteral("No mistakes. Reviewer 2 is reconsidering their career."),
        QStringLiteral("A clean sweep. The time series looks better than the real one."),
        QStringLiteral("Flawless. {who} has stopped checking your work."),
        QStringLiteral("Perfect coverage. The FLUXNET team sent a fruit basket."),
        QStringLiteral("No losses. The tower crew has written a song about you."),
        QStringLiteral("Immaculate. Even the advection behaved.")
    };
    return list;
}

// added when the deadline ran out
const QStringList& deadlineTemplates()
{
    static const QStringList list {
        QStringLiteral("The deadline passed somewhere around {place}."),
        QStringLiteral("Out of time. {who} has already emailed twice."),
        QStringLiteral("The deadline won. It usually does."),
        QStringLiteral("Time is up. The data will be submitted next year, with apologies."),
        QStringLiteral("The submission window closed while you were admiring the eddies."),
        QStringLiteral("Too slow. The funding agency has moved on."),
        QStringLiteral("The clock ran out. {tool} would have been quicker, but only just."),
        QStringLiteral("Deadline missed. The extension request is already drafted."),
        QStringLiteral("You ran out of time, which is the most realistic part of this game."),
        QStringLiteral("The deadline was unreasonable. You were also slow."),
        QStringLiteral("Time over. The flux tower is still waiting, patiently."),
        QStringLiteral("Late again. {who} is not surprised."),
        QStringLiteral("The report was due yesterday. So was the run."),
        QStringLiteral("Out of time. Next time, fewer coffee breaks."),
        QStringLiteral("The deadline came and went, like a cold front."),
        QStringLiteral("Time is up. The tower's flag remains stubbornly up."),
        QStringLiteral("Deadline exceeded. {place} has started without you."),
        QStringLiteral("The clock beat you. The clock always beats you."),
        QStringLiteral("You missed the deadline by a week of fluxes."),
        QStringLiteral("Time out. The reviewers have gone on holiday."),
        QStringLiteral("The deadline was a suggestion. A firm one."),
        QStringLiteral("Out of time, but with excellent coverage of the first few days."),
        QStringLiteral("The submission portal closed with a polite but final error."),
        QStringLiteral("Late. {who} has rescheduled the meeting to discuss it."),
        QStringLiteral("Time ran out somewhere between the eddies and the rain."),
        QStringLiteral("Deadline missed. At least the data are safe on the backup station."),
        QStringLiteral("The deadline has been extended. Just kidding."),
        QStringLiteral("You took the scenic route. The deadline did not."),
        QStringLiteral("Too late for this year's synthesis paper."),
        QStringLiteral("The clock says no. The tower says maybe next time.")
    };
    return list;
}

// across the top on entering a turbulence zone
const QStringList& turbulenceBanners()
{
    static const QStringList list {
        QStringLiteral("CONVECTIVE BOUNDARY LAYER AHEAD"),
        QStringLiteral("HIGH TURBULENCE: HOLD ON"),
        QStringLiteral("GUSTS! u* RISING"),
        QStringLiteral("THE EDDIES ARE AWAKE"),
        QStringLiteral("WELL-MIXED AND UNSTABLE"),
        QStringLiteral("TKE WARNING"),
        QStringLiteral("THERMALS DETECTED"),
        QStringLiteral("STRONG MIXING ZONE"),
        QStringLiteral("NEGATIVE OBUKHOV LENGTH AHEAD"),
        QStringLiteral("FREE CONVECTION: BRACE YOURSELF"),
        QStringLiteral("TURBULENT FLUXES INCOMING"),
        QStringLiteral("GUSTY, WITH A CHANCE OF EDDIES"),
        QStringLiteral("THE SONIC IS EXCITED"),
        QStringLiteral("INTEGRAL TURBULENCE CHARACTERISTICS: WILD"),
        QStringLiteral("MIDDAY MIXING"),
        QStringLiteral("z/L < 0, HANG ON"),
        QStringLiteral("COHERENT STRUCTURES AHEAD"),
        QStringLiteral("WIND SHEAR ZONE"),
        QStringLiteral("RIDE THE EDDIES"),
        QStringLiteral("BUOYANCY PRODUCTION: HIGH"),
        QStringLiteral("THE BOUNDARY LAYER BOILS"),
        QStringLiteral("SWEEPS AND EJECTIONS"),
        QStringLiteral("TURBULENCE: FULLY DEVELOPED"),
        QStringLiteral("THERMALS: HOLD JUMP TO RISE"),
        QStringLiteral("FRICTION VELOCITY UP"),
        QStringLiteral("GOOD FOR FLUXES, BAD FOR JUMPING"),
        QStringLiteral("INERTIAL SUBRANGE AHEAD"),
        QStringLiteral("THE WINDSOCK IS HORIZONTAL"),
        QStringLiteral("EDDY COVARIANCE AT WORK"),
        QStringLiteral("UNSTABLE CONDITIONS")
    };
    return list;
}

// thrown upward by an eddy
const QStringList& eddyPops()
{
    static const QStringList list {
        QStringLiteral("WHOOSH!"),
        QStringLiteral("UPDRAFT!"),
        QStringLiteral("EJECTION!"),
        QStringLiteral("CAUGHT AN EDDY!"),
        QStringLiteral("w' > 0!"),
        QStringLiteral("LIFTED!"),
        QStringLiteral("RIDING THE VORTEX!"),
        QStringLiteral("COHERENT STRUCTURE!"),
        QStringLiteral("SPUN UP!"),
        QStringLiteral("TURBULENT TRANSPORT!"),
        QStringLiteral("UP WITH THE SENSIBLE HEAT!"),
        QStringLiteral("CARRIED ALOFT!"),
        QStringLiteral("EDDY-POWERED!"),
        QStringLiteral("VERTICAL FLUX: YOU!"),
        QStringLiteral("SWIRL!"),
        QStringLiteral("FLUNG!"),
        QStringLiteral("THE EDDY LIKES YOU!"),
        QStringLiteral("POSITIVE COVARIANCE!"),
        QStringLiteral("WHEEE!"),
        QStringLiteral("CONVECTED!"),
        QStringLiteral("BUOYANT!"),
        QStringLiteral("SHEAR BOOST!"),
        QStringLiteral("UPWARD TRANSPORT!"),
        QStringLiteral("THE BIG EDDY!"),
        QStringLiteral("RECIRCULATED!"),
        QStringLiteral("CORRELATED WITH w'!"),
        QStringLiteral("AIRBORNE!"),
        QStringLiteral("THERMALLY ASSISTED!"),
        QStringLiteral("TOSSED!"),
        QStringLiteral("SPINNING!")
    };
    return list;
}

// through a sampling tube; %1 is the time lag in seconds
const QStringList& tubePops()
{
    static const QStringList list {
        QStringLiteral("TIME LAG: %1 s"),
        QStringLiteral("τ = %1 s"),
        QStringLiteral("THROUGH THE INTAKE! (%1 s)"),
        QStringLiteral("SAMPLED! LAG %1 s"),
        QStringLiteral("%1 s THROUGH THE TUBE!"),
        QStringLiteral("CLOSED-PATH TRAVEL: %1 s"),
        QStringLiteral("CROSS-CORRELATION PEAK: %1 s"),
        QStringLiteral("PUMPED THROUGH IN %1 s!"),
        QStringLiteral("LAG COMPENSATED: %1 s"),
        QStringLiteral("DELIVERED! τ = %1 s"),
        QStringLiteral("TUBE FLOW OK, %1 s"),
        QStringLiteral("NOMINAL LAG %1 s"),
        QStringLiteral("ARRIVED AFTER %1 s"),
        QStringLiteral("THE PUMP WORKS! (%1 s)"),
        QStringLiteral("%1 s OF TUBE ATTENUATION"),
        QStringLiteral("SHORTCUT! LAG %1 s"),
        QStringLiteral("SPECTRAL LOSS ACCEPTED, %1 s"),
        QStringLiteral("THROUGH THE RAIN CAP! %1 s"),
        QStringLiteral("COVARIANCE MAXIMISED AT %1 s"),
        QStringLiteral("SAMPLE LINE: %1 s"),
        QStringLiteral("FLUSHED THROUGH IN %1 s!"),
        QStringLiteral("THE LAG WINDOW SAYS %1 s"),
        QStringLiteral("TUBE TRANSIT %1 s"),
        QStringLiteral("WARP! (LAG %1 s)"),
        QStringLiteral("SYNCHRONISED: %1 s"),
        QStringLiteral("FROM INLET TO CELL: %1 s"),
        QStringLiteral("%1 s, WITHIN SPEC!"),
        QStringLiteral("LAG DETECTED: %1 s"),
        QStringLiteral("DELAYED BUT DELIVERED: %1 s"),
        QStringLiteral("PIPED! τ = %1 s")
    };
    return list;
}

// above the player on walking into a shower
const QStringList& rainPops()
{
    static const QStringList list {
        QStringLiteral("RAIN! MIND THE SLIPPERY TRACE!"),
        QStringLiteral("WET SONIC!"),
        QStringLiteral("PRECIPITATION FLAG!"),
        QStringLiteral("THE OPEN PATH IS WET!"),
        QStringLiteral("SLIPPERY WHEN WET!"),
        QStringLiteral("SHOWER INCOMING!"),
        QStringLiteral("RAIN CAP ON!"),
        QStringLiteral("THE TRANSDUCERS ARE DRIPPING!"),
        QStringLiteral("SIGNAL STRENGTH DROPPING!"),
        QStringLiteral("AFTERNOON THUNDERSTORM!"),
        QStringLiteral("THE RAIN GAUGE TIPS!"),
        QStringLiteral("NO GRIP!"),
        QStringLiteral("CAREFUL, IT'S WET!"),
        QStringLiteral("WIPERS ON!"),
        QStringLiteral("THE IRGA HATES THIS!"),
        QStringLiteral("DATA FLAGGED: RAIN!"),
        QStringLiteral("PUDDLES!"),
        QStringLiteral("CONVECTIVE SHOWER!"),
        QStringLiteral("SLIDING!"),
        QStringLiteral("LATENT HEAT, LITERALLY!"),
        QStringLiteral("SOAKED!"),
        QStringLiteral("SPLASH ZONE!"),
        QStringLiteral("RAINY HALF-HOURS!"),
        QStringLiteral("THE MIRRORS FOG UP!"),
        QStringLiteral("LOW AGC, HIGH DRAMA!"),
        QStringLiteral("STOP EARLY: IT'S SLIPPERY!"),
        QStringLiteral("WET BULB, WET BOOTS!"),
        QStringLiteral("THE HUT ROOF LEAKS!"),
        QStringLiteral("DRIZZLE ALERT!"),
        QStringLiteral("THE SKY OPENS!")
    };
    return list;
}

// across the top when the night sets in
const QStringList& nightLines()
{
    static const QStringList list {
        QStringLiteral("NIGHT: THE STABLE LAYER SETTLES"),
        QStringLiteral("NIGHTFALL: u* BELOW THRESHOLD"),
        QStringLiteral("STABLE CONDITIONS: VISIBILITY DOWN"),
        QStringLiteral("THE BOUNDARY LAYER COLLAPSES"),
        QStringLiteral("NIGHT-TIME ADVECTION WARNING"),
        QStringLiteral("COLD AIR POOLING"),
        QStringLiteral("THE MIST ROLLS IN"),
        QStringLiteral("DECOUPLED FROM THE SURFACE"),
        QStringLiteral("z/L > 0: QUIET AND DARK"),
        QStringLiteral("RESPIRATION HOURS"),
        QStringLiteral("THE PITS ARE DARKER AT NIGHT"),
        QStringLiteral("INTERMITTENT TURBULENCE, MOSTLY NOT"),
        QStringLiteral("NOCTURNAL JET OVERHEAD"),
        QStringLiteral("THE NIGHT SHIFT BEGINS"),
        QStringLiteral("STRATIFIED AND SPOOKY"),
        QStringLiteral("LOW TURBULENCE, LOW VISIBILITY"),
        QStringLiteral("THE u* FILTER IS HUNGRY TONIGHT"),
        QStringLiteral("DRAINAGE FLOWS AHEAD"),
        QStringLiteral("RADIATIVE COOLING IN PROGRESS"),
        QStringLiteral("THE STARS ARE OUT, THE DATA ARE NOT"),
        QStringLiteral("NIGHT: WATCH FOR ADVECTED CO2"),
        QStringLiteral("THE INVERSION TIGHTENS"),
        QStringLiteral("DEW ON THE SENSORS"),
        QStringLiteral("CALM, COLD AND GAPPY"),
        QStringLiteral("THE ECOSYSTEM EXHALES"),
        QStringLiteral("GRAVITY WAVES, PROBABLY"),
        QStringLiteral("STORAGE BUILDS UNDER THE CANOPY"),
        QStringLiteral("NIGHT-TIME: HERE BE GAPS"),
        QStringLiteral("THE STABLE BOUNDARY LAYER"),
        QStringLiteral("DARKNESS AND RESPIRATION")
    };
    return list;
}

// under a cleared level; {level}, {site}, {coverage} and {lives} are filled in
const QStringList& levelClearedLines()
{
    static const QStringList list {
        QStringLiteral("Level {level} is in the archive. {site} will be in the next synthesis paper."),
        QStringLiteral("{site} submitted with {coverage} coverage. The next tower is already waiting."),
        QStringLiteral("The data from {site} are in. The data from the next site are not. Yet."),
        QStringLiteral("Level {level} done. Somewhere, a postdoc is plotting your cumulative sum."),
        QStringLiteral("{site}: processed, quality-controlled, submitted. Next."),
        QStringLiteral("The tower at {site} lowers its flag. Another one raises its own."),
        QStringLiteral("Level {level} cleared with {lives} lost. The field season goes on."),
        QStringLiteral("One site down. FLUXNET has many more."),
        QStringLiteral("The {site} week is safe on the server. Pack the van."),
        QStringLiteral("{coverage} coverage at {site}. The next site has heard about you."),
        QStringLiteral("Level {level} is archived with a DOI. The next one has a deadline."),
        QStringLiteral("You leave {site} with muddy boots and a complete week."),
        QStringLiteral("The data logger at {site} waves goodbye."),
        QStringLiteral("Level {level}: done. The funding agency asks what is next. This is next."),
        QStringLiteral("{site} is filed. The next tower is taller, the gaps are wider."),
        QStringLiteral("Another week, another tower, another flag down."),
        QStringLiteral("The reviewers at {site} have been dealt with. New ones await."),
        QStringLiteral("You conquered level {level}. Level {level} would like to apologise."),
        QStringLiteral("{site} is in the bag, with {coverage} of its half-hours."),
        QStringLiteral("The PI is pleased with {site}. The PI has another site in mind."),
        QStringLiteral("Level {level} cleared. The van is fuelled for the next site."),
        QStringLiteral("{site}: done. The eddies there will tell stories about you."),
        QStringLiteral("The cumulative sum of {site} looks respectable. On to the next."),
        QStringLiteral("You handed in {site} on time. That never happens."),
        QStringLiteral("Level {level} is behind you, and so is its weather."),
        QStringLiteral("The data from {site} survived. So did you, mostly."),
        QStringLiteral("The {site} tower crew is buying the first round. You have no time to drink it."),
        QStringLiteral("Level {level} cleared. Your inbox has a new field campaign in it."),
        QStringLiteral("{site} is complete. The network map just got one dot greener."),
        QStringLiteral("Onwards: {site} was only the beginning.")
    };
    return list;
}

// while the next level loads: a taste of what is coming
const QStringList& nextLevelLines()
{
    static const QStringList list {
        QStringLiteral("Loading the next site's raw data..."),
        QStringLiteral("Reviewer 2 has been promoted to editor..."),
        QStringLiteral("Widening the gaps a little..."),
        QStringLiteral("Raising the spikes a little more..."),
        QStringLiteral("Stirring up the boundary layer..."),
        QStringLiteral("Booking flights to a new field site..."),
        QStringLiteral("Packing the van with spare sonics..."),
        QStringLiteral("Asking the methane to bubble faster..."),
        QStringLiteral("Shortening the deadline, as is tradition..."),
        QStringLiteral("Calibrating a fresh gas analyser..."),
        QStringLiteral("Unrolling a longer week of data..."),
        QStringLiteral("Teaching the puffs of CO2 to fly faster..."),
        QStringLiteral("Making the platforms less stationary..."),
        QStringLiteral("Finding a site with worse weather..."),
        QStringLiteral("Sharpening the outliers..."),
        QStringLiteral("Hiding the coffee more carefully..."),
        QStringLiteral("Briefing the next tower crew..."),
        QStringLiteral("Increasing the friction velocity..."),
        QStringLiteral("Scheduling more power cuts..."),
        QStringLiteral("Recruiting Reviewer 3..."),
        QStringLiteral("Tilting the sonic slightly..."),
        QStringLiteral("Lengthening the sampling tubes..."),
        QStringLiteral("Warning the logger about you..."),
        QStringLiteral("Rolling a new season..."),
        QStringLiteral("Choosing a new time zone..."),
        QStringLiteral("Downloading another week, slowly..."),
        QStringLiteral("Re-zeroing the analyser for the next site..."),
        QStringLiteral("Drawing up the next risk assessment..."),
        QStringLiteral("Feeding the eddies..."),
        QStringLiteral("Mixing a fresh boundary layer...")
    };
    return list;
}

// added to the rank for a run that cleared levels; {levels}, {level},
// {site} and {firstsite} are filled in
const QStringList& levelsTemplates()
{
    static const QStringList list {
        QStringLiteral("{levels} cleared, from {firstsite} to {site}."),
        QStringLiteral("The run took in {levels}, starting at {firstsite}."),
        QStringLiteral("{levels} behind you; {site} was the last to see you."),
        QStringLiteral("From {firstsite} to {site}: {levels} in one field season."),
        QStringLiteral("You made it through {levels} before {site} had the last word."),
        QStringLiteral("{levels} cleared. {firstsite} seems a long time ago."),
        QStringLiteral("A tour of {levels}, from {firstsite} onwards."),
        QStringLiteral("{levels} cleared. The network map remembers your route."),
        QStringLiteral("You reached level {level}, by way of {levels}."),
        QStringLiteral("{levels} archived, {firstsite} first and {site} last."),
        QStringLiteral("{levels} in the bag. The van is now mostly mud."),
        QStringLiteral("The {levels} you cleared are going into a synthesis paper."),
        QStringLiteral("{firstsite}, then onwards: {levels} in all."),
        QStringLiteral("The run stopped at level {level}, after {levels}."),
        QStringLiteral("{levels} cleared, and {site} still talks about you."),
        QStringLiteral("{levels} done: a field season to remember."),
        QStringLiteral("Your road ran from {firstsite} to {site}, {levels} long."),
        QStringLiteral("{levels} cleared. The funding agency would like a progress report."),
        QStringLiteral("You saw {levels} through, then {site} saw you off."),
        QStringLiteral("{levels} in the archive, all with your name on them."),
        QStringLiteral("{levels} cleared. Each tower has a story about you."),
        QStringLiteral("From {firstsite} to level {level}: {levels}."),
        QStringLiteral("{levels} completed. The network coordinator is impressed."),
        QStringLiteral("The run covered {levels}, and quite a few time zones."),
        QStringLiteral("{levels} cleared: more sites than most PhD theses."),
        QStringLiteral("{levels} behind you, from {firstsite} to wherever {site} is."),
        QStringLiteral("The field season ran for {levels}. {site} was its last stop."),
        QStringLiteral("{levels} cleared, which is {levels} more than Reviewer 2 has visited."),
        QStringLiteral("A run of {levels}, starting where the sun rose over {firstsite}."),
        QStringLiteral("{levels} cleared. The towers will miss you.")
    };
    return list;
}

// the first half of a site's name
const QStringList& placeStems()
{
    static const QStringList list {
        QStringLiteral("Ashgrove"), QStringLiteral("Bramblecombe"), QStringLiteral("Cedar Hollow"),
        QStringLiteral("Dunmore"), QStringLiteral("Elderwick"), QStringLiteral("Fernhill"),
        QStringLiteral("Glenrock"), QStringLiteral("Harrowmere"), QStringLiteral("Ironwood"),
        QStringLiteral("Juniper Flats"), QStringLiteral("Kestrel Ridge"), QStringLiteral("Larkspur"),
        QStringLiteral("Millbrook"), QStringLiteral("Northfell"), QStringLiteral("Oakhaven"),
        QStringLiteral("Pinecrest"), QStringLiteral("Quarry Bend"), QStringLiteral("Ravensmoor"),
        QStringLiteral("Stonebridge"), QStringLiteral("Thornfield"), QStringLiteral("Upper Wold"),
        QStringLiteral("Valebrook"), QStringLiteral("Willowmere"), QStringLiteral("Yarrow Downs"),
        QStringLiteral("Alderwood"), QStringLiteral("Brackenridge"), QStringLiteral("Coldwater"),
        QStringLiteral("Driftwood"), QStringLiteral("Emberly"), QStringLiteral("Foxglove")
    };
    return list;
}

// a line on the level card for each kind of site, eight each
const QStringList& cowPops()
{
    static const QStringList list {
        QStringLiteral("MOO-VED!"),
        QStringLiteral("UDDERLY SPRINGY!"),
        QStringLiteral("THE HERD APPROVES!"),
        QStringLiteral("FOOTPRINT: TRAMPLED!"),
        QStringLiteral("METHANE SOURCE: VAULTED!"),
        QStringLiteral("ENTERIC EMISSIONS: AVOIDED!"),
        QStringLiteral("BOVINE BOOST!"),
        QStringLiteral("OVER THE COW!"),
        QStringLiteral("GRAZING INTERRUPTED!"),
        QStringLiteral("MOOVING ON!"),
        QStringLiteral("RUMINANT RECOIL!"),
        QStringLiteral("COW-ABUNGA!"),
        QStringLiteral("THE FARMER WILL HEAR OF THIS!"),
        QStringLiteral("FETCH: NOW A PASTURE!"),
        QStringLiteral("BELL RUNG!"),
        QStringLiteral("HOOVES CLEARED!"),
        QStringLiteral("STEAK-HOLDER ENGAGED!"),
        QStringLiteral("THE COW IS UNIMPRESSED!"),
        QStringLiteral("SPRUNG FROM THE SPOTS!"),
        QStringLiteral("THE TOWER FENCE WAS THERE FOR A REASON!")
    };
    return list;
}

const QStringList& tractorPops()
{
    static const QStringList list {
        QStringLiteral("TRACTOR BEAM!"),
        QStringLiteral("HITCHED A RIDE!"),
        QStringLiteral("OVER THE BONNET!"),
        QStringLiteral("DIESEL DODGED!"),
        QStringLiteral("SOIL COMPACTION: AVOIDED!"),
        QStringLiteral("TRAMLINES CROSSED!"),
        QStringLiteral("HAY THERE!"),
        QStringLiteral("MANAGEMENT EVENT: SURVIVED!"),
        QStringLiteral("FLAGGED IN THE SITE LOG!"),
        QStringLiteral("PLOUGHED THROUGH!"),
        QStringLiteral("TREAD: VAULTED!"),
        QStringLiteral("THE FOOTPRINT IS NOT A ROAD!"),
        QStringLiteral("CAB CLEARED!"),
        QStringLiteral("HARVEST DELAYED!"),
        QStringLiteral("CROP ROTATION!"),
        QStringLiteral("THE FARMER WAVES!"),
        QStringLiteral("EXHAUST PLUME: NOTED!"),
        QStringLiteral("RIGHT OF WAY: CONTESTED!"),
        QStringLiteral("THE TOWER STAYS WHERE IT IS!"),
        QStringLiteral("MOWING CANCELLED!")
    };
    return list;
}

const QStringList& dustDevilPops()
{
    static const QStringList list {
        QStringLiteral("SPUN UP!"),
        QStringLiteral("CONVECTION!"),
        QStringLiteral("FREE THERMAL!"),
        QStringLiteral("SAND IN THE SONIC!"),
        QStringLiteral("VORTEX RIDDEN!"),
        QStringLiteral("VERTICAL WIND: CONFIRMED!"),
        QStringLiteral("W' > 0!"),
        QStringLiteral("LIFT-OFF!"),
        QStringLiteral("THE ENERGY BALANCE STILL WON'T CLOSE!"),
        QStringLiteral("WHIRLED!"),
        QStringLiteral("BUOYANCY!"),
        QStringLiteral("DUST: ADVECTED!"),
        QStringLiteral("SPINNING UP THE SPECTRUM!"),
        QStringLiteral("TWISTED!"),
        QStringLiteral("DUST DEVIL: OUTWITTED!"),
        QStringLiteral("SENSIBLE HEAT, INSENSIBLE JUMP!"),
        QStringLiteral("UPDRAFT, SANDY!"),
        QStringLiteral("THE DESERT EXPRESS!"),
        QStringLiteral("SWIRL!"),
        QStringLiteral("HOT AIR RISES. SO DO YOU!")
    };
    return list;
}

const QStringList& mosquitoPops()
{
    static const QStringList list {
        QStringLiteral("SWATTED!"),
        QStringLiteral("BZZZT!"),
        QStringLiteral("BITE AVOIDED!"),
        QStringLiteral("ONE LESS ITCH!"),
        QStringLiteral("THE SWARM THINS!"),
        QStringLiteral("SPLAT!"),
        QStringLiteral("NO BLOOD SAMPLE TODAY!"),
        QStringLiteral("INSECT IN THE PATH: REMOVED!"),
        QStringLiteral("THE OPEN PATH THANKS YOU!"),
        QStringLiteral("WHINE SILENCED!"),
        QStringLiteral("A FIELD SEASON CLASSIC!"),
        QStringLiteral("MOSQUITO: DESPIKED!"),
        QStringLiteral("SIGNAL STRENGTH: RESTORED!"),
        QStringLiteral("FLY-BY CANCELLED!"),
        QStringLiteral("SWAMP RULES!"),
        QStringLiteral("NETTED!"),
        QStringLiteral("ONE DOWN, TEN THOUSAND TO GO!"),
        QStringLiteral("SCRATCH THAT!"),
        QStringLiteral("REPELLED!"),
        QStringLiteral("THE MIRE WILL SEND MORE!")
    };
    return list;
}

const QStringList& fallPops()
{
    static const QStringList list {
        QStringLiteral("LITTERFALL!"),
        QStringLiteral("MISSED!"),
        QStringLiteral("HEADS UP!"),
        QStringLiteral("THAT WAS CLOSE!"),
        QStringLiteral("GRAVITY: CONFIRMED!"),
        QStringLiteral("NEWTON WAS RIGHT!"),
        QStringLiteral("INCOMING!"),
        QStringLiteral("DUCK!"),
        QStringLiteral("SOMETHING FELL!"),
        QStringLiteral("CHECK THE HARD HAT!"),
        QStringLiteral("NEAR MISS LOGGED!"),
        QStringLiteral("THE SLOPE IS RESTLESS!"),
        QStringLiteral("THUD!"),
        QStringLiteral("FIR ENOUGH!"),
        QStringLiteral("ROCK BOTTOM!"),
        QStringLiteral("LOOK UP!"),
        QStringLiteral("THE CANOPY LETS GO!"),
        QStringLiteral("DOWNWARD FLUX!"),
        QStringLiteral("NOT ON THE SONIC, PLEASE!"),
        QStringLiteral("THE MOUNTAIN SENDS ITS REGARDS!")
    };
    return list;
}

const QStringList& firePops()
{
    static const QStringList list {
        QStringLiteral("SCORCHED!"),
        QStringLiteral("HOT FOOT!"),
        QStringLiteral("FIRE SEASON!"),
        QStringLiteral("COMBUSTION FLUX!"),
        QStringLiteral("A CO2 SOURCE, PERSONALLY!"),
        QStringLiteral("BURNT!"),
        QStringLiteral("SMOKE IN THE ANALYSER!"),
        QStringLiteral("TOO HOT!"),
        QStringLiteral("BIOMASS BURNING!"),
        QStringLiteral("BLACK CARBON!"),
        QStringLiteral("SINGED!"),
        QStringLiteral("FIREBREAK MISSED!"),
        QStringLiteral("CHARRED!"),
        QStringLiteral("PYROGENIC!"),
        QStringLiteral("HEAT FLUX: EXCESSIVE!"),
        QStringLiteral("SENSIBLE HEAT: VERY SENSIBLE!"),
        QStringLiteral("FEET: TOASTED!"),
        QStringLiteral("THE SAVANNA BITES!"),
        QStringLiteral("JUMP THE FLAMES NEXT TIME!"),
        QStringLiteral("THAT WAS THE FIRE SEASON!")
    };
    return list;
}

const QStringList& icePops()
{
    static const QStringList list {
        QStringLiteral("ICE!"),
        QStringLiteral("SLIPPERY!"),
        QStringLiteral("FRICTION: LOW!"),
        QStringLiteral("WHEEE!"),
        QStringLiteral("SKATING!"),
        QStringLiteral("ICE PATCH!"),
        QStringLiteral("CAREFUL NOW!"),
        QStringLiteral("ZERO GRIP!"),
        QStringLiteral("FROZEN SOLID!"),
        QStringLiteral("THAW PENDING!"),
        QStringLiteral("SLIDING!"),
        QStringLiteral("HIGH ALBEDO, LOW GRIP!"),
        QStringLiteral("MIND YOUR STEP!"),
        QStringLiteral("COLD FEET!"),
        QStringLiteral("GLIDE PATH!"),
        QStringLiteral("BRAKES: OPTIONAL!"),
        QStringLiteral("FROST HEAVE!"),
        QStringLiteral("THE ACTIVE LAYER IS NOT ACTIVE!"),
        QStringLiteral("PERMAFROST, SURFACE EDITION!"),
        QStringLiteral("STOPPING DISTANCE: GENEROUS!")
    };
    return list;
}

// the first sight of a site's own danger in a level, four for each site
const QStringList& dangerBanners(int kind)
{
    static const QList<QStringList> lines {
        {   // forest
            QStringLiteral("THE CANOPY IS SHEDDING"),
            QStringLiteral("CONES INCOMING: WATCH FOR SHADOWS"),
            QStringLiteral("LITTERFALL SEASON"),
            QStringLiteral("MIND THE SHADOWS UNDER THE TREES")
        },
        {   // grassland
            QStringLiteral("COWS IN THE FOOTPRINT"),
            QStringLiteral("THE HERD IS GRAZING THE FETCH"),
            QStringLiteral("LIVESTOCK ON THE PLOT"),
            QStringLiteral("MIND THE COWS")
        },
        {   // wetland
            QStringLiteral("MOSQUITO SWARMS"),
            QStringLiteral("THE MIRE IS BUZZING"),
            QStringLiteral("SWARMS AT DUSK"),
            QStringLiteral("BRING THE NET")
        },
        {   // cropland
            QStringLiteral("TRACTOR IN THE FOOTPRINT"),
            QStringLiteral("MANAGEMENT EVENT IN PROGRESS"),
            QStringLiteral("THE FARMER IS MOWING TODAY"),
            QStringLiteral("HEAVY MACHINERY AHEAD")
        },
        {   // tundra
            QStringLiteral("ICE ON THE TRACE"),
            QStringLiteral("SLIPPERY GROUND AHEAD"),
            QStringLiteral("THE MELTWATER HAS FROZEN"),
            QStringLiteral("ICE PATCHES: LOW GRIP")
        },
        {   // savanna
            QStringLiteral("GRASS FIRES"),
            QStringLiteral("FIRE SEASON: JUMP THE FLAMES"),
            QStringLiteral("THE DRY SEASON IS BURNING"),
            QStringLiteral("SMOKE ON THE HORIZON")
        },
        {   // mountain
            QStringLiteral("ROCKFALL"),
            QStringLiteral("LOOSE ROCK ABOVE: WATCH FOR SHADOWS"),
            QStringLiteral("THE SCREE IS MOVING"),
            QStringLiteral("FALLING ROCKS AHEAD")
        },
        {   // desert
            QStringLiteral("DUST DEVILS"),
            QStringLiteral("WHIRLWINDS ON THE FLATS"),
            QStringLiteral("CONVECTION IS SPINNING UP"),
            QStringLiteral("SAND IN THE AIR")
        }
    };
    return lines.at(std::clamp(kind, 0, static_cast<int>(lines.size()) - 1));
}

const QStringList& siteLines(int kind)
{
    static const QList<QStringList> lines {
        {   // forest
            QStringLiteral("Tall trees, deep valleys of daytime uptake, and a canopy full of storage."),
            QStringLiteral("The forest breathes in by day and out by night. Mind the walls."),
            QStringLiteral("Somewhere above the canopy, a sonic is counting eddies."),
            QStringLiteral("Roughness length: considerable. Spikes: plentiful."),
            QStringLiteral("The tower rises above the trees. So should you."),
            QStringLiteral("Woodpeckers have been at the cables again."),
            QStringLiteral("The canopy decouples at night. The puffs of CO2 do not."),
            QStringLiteral("A forest week: big swings, tall needles, few gusts.")
        },
        {   // grassland
            QStringLiteral("Open grass, wide skies, and a lot of Reviewer 2s grazing."),
            QStringLiteral("Short canopy, long fetch, lively turbulence."),
            QStringLiteral("The cows have been told to stay away from the tower. They have not listened."),
            QStringLiteral("Mowing day changed everything. Watch the gaps."),
            QStringLiteral("The footprint stretches for miles. So does the level."),
            QStringLiteral("A grassland week: gusty afternoons and walking reviewers."),
            QStringLiteral("The hay is cut, the fluxes are not."),
            QStringLiteral("Flat terrain, honest fluxes, and wind from every side.")
        },
        {   // wetland
            QStringLiteral("Wet feet guaranteed. The methane bubbles are everywhere."),
            QStringLiteral("The peat is deep, the gaps are deeper."),
            QStringLiteral("Every pit seems to burp. That is ebullition."),
            QStringLiteral("The boardwalk wobbles. So do the platforms."),
            QStringLiteral("A wetland week: small uptake, many bubbles, mosquitoes."),
            QStringLiteral("The water table is high, and so is the methane."),
            QStringLiteral("Sphagnum underfoot, CH4 overhead."),
            QStringLiteral("The mire keeps its carbon, mostly. Watch out for what it lets go.")
        },
        {   // cropland
            QStringLiteral("Rows of crops to the horizon, and a farmer who wants the tower moved."),
            QStringLiteral("Huge uptake in the growing season, bare soil after harvest."),
            QStringLiteral("The tractor drove through the footprint again."),
            QStringLiteral("Fertiliser week. The fluxes are excited."),
            QStringLiteral("A cropland week: management events and very deep valleys."),
            QStringLiteral("The harvest is in. The data are not."),
            QStringLiteral("Irrigation started on Tuesday. So did the gaps."),
            QStringLiteral("The field was ploughed the day after the tower went up.")
        },
        {   // tundra
            QStringLiteral("Permafrost below, endless light or endless dark above."),
            QStringLiteral("The sun never quite sets here. Or never quite rises."),
            QStringLiteral("Small fluxes, big wind, and a polar bear warning on the hut door."),
            QStringLiteral("The active layer is thin. The battery is thinner."),
            QStringLiteral("A tundra week: weak uptake, strong turbulence, snow in the air."),
            QStringLiteral("The heater on the sonic is working overtime."),
            QStringLiteral("Thaw slumps opened new gaps in the data."),
            QStringLiteral("The nearest technician is a helicopter flight away.")
        },
        {   // savanna
            QStringLiteral("Scattered trees, tall grass, and a wet season that changes everything."),
            QStringLiteral("When the rains come, the uptake comes. Otherwise, dust."),
            QStringLiteral("Hot afternoons, thermals everywhere. Hold jump."),
            QStringLiteral("An elephant leaned on the tower. It is still mostly vertical."),
            QStringLiteral("A savanna week: wet and green, or dry and still."),
            QStringLiteral("Fire season left some gaps in the record."),
            QStringLiteral("The termite mounds are taller than the sonic."),
            QStringLiteral("The first rain of the season arrived with a flux pulse.")
        },
        {   // mountain
            QStringLiteral("Steep slopes, thin air, and drainage flows every night."),
            QStringLiteral("The cold air pours down the valley at night. So does the CO2."),
            QStringLiteral("Complex terrain: the planar fit gave up."),
            QStringLiteral("Gusts off the ridge, snow on the peaks, puffs in the valley."),
            QStringLiteral("A mountain week: strong winds, tall walls, advection everywhere."),
            QStringLiteral("The tower is anchored to the rock. Hold on to something too."),
            QStringLiteral("The cable car stopped running. The data did not."),
            QStringLiteral("Katabatic winds tonight, anabatic tomorrow.")
        },
        {   // desert
            QStringLiteral("Tiny fluxes, a nearly flat trace, and heat you can see."),
            QStringLiteral("Sand on the sensors, sand in the logger, sand in the gaps."),
            QStringLiteral("The strongest convection on the network. Thermals for days."),
            QStringLiteral("It rains here once a year. This might be the week."),
            QStringLiteral("A desert week: flat ground, fierce afternoons, sandy pits."),
            QStringLiteral("The signal is small and the noise is not."),
            QStringLiteral("Dust devils have been sighted near the tower."),
            QStringLiteral("The energy balance does not close here either.")
        }
    };
    return lines.at(std::clamp(kind, 0, static_cast<int>(lines.size()) - 1));
}

// the first banner of a level, eight for each season
const QStringList& seasonLines(int season)
{
    static const QList<QStringList> lines {
        {   // spring
            QStringLiteral("SPRING: GREEN-UP IN PROGRESS"),
            QStringLiteral("SPRING: THE UPTAKE IS WAKING UP"),
            QStringLiteral("SPRING: BUDBURST AND BIG GUSTS"),
            QStringLiteral("SPRING: LONGER DAYS, DEEPER VALLEYS"),
            QStringLiteral("SPRING: THE SNOW IS MELTING INTO THE GAPS"),
            QStringLiteral("SPRING: THE CARBON SINK IS OPEN"),
            QStringLiteral("SPRING: FRESH LEAVES, FRESH SPIKES"),
            QStringLiteral("SPRING: THE GROWING SEASON BEGINS")
        },
        {   // summer
            QStringLiteral("SUMMER: PEAK UPTAKE"),
            QStringLiteral("SUMMER: SHORT NIGHTS, DEEP VALLEYS"),
            QStringLiteral("SUMMER: THUNDERSTORMS IN THE AFTERNOON"),
            QStringLiteral("SUMMER: THE CONVECTIVE BOUNDARY LAYER IS HUGE"),
            QStringLiteral("SUMMER: THE ECOSYSTEM IS HUNGRY"),
            QStringLiteral("SUMMER: HEATWAVE WARNING"),
            QStringLiteral("SUMMER: THE LONGEST DAYS OF THE YEAR"),
            QStringLiteral("SUMMER: FULL CANOPY, FULL FLUXES")
        },
        {   // autumn
            QStringLiteral("AUTUMN: SENESCENCE IN PROGRESS"),
            QStringLiteral("AUTUMN: THE UPTAKE IS FADING"),
            QStringLiteral("AUTUMN: FALLING LEAVES, FALLING FLUXES"),
            QStringLiteral("AUTUMN: LONGER NIGHTS, MORE ADVECTION"),
            QStringLiteral("AUTUMN: THE FOG SETTLES IN"),
            QStringLiteral("AUTUMN: HARVEST TIME"),
            QStringLiteral("AUTUMN: THE SOURCE IS WINNING"),
            QStringLiteral("AUTUMN: THE GROWING SEASON ENDS")
        },
        {   // winter
            QStringLiteral("WINTER: LONG NIGHTS, SHALLOW VALLEYS"),
            QStringLiteral("WINTER: THE SONIC HEATERS ARE ON"),
            QStringLiteral("WINTER: RESPIRATION ONLY"),
            QStringLiteral("WINTER: THE BATTERIES DO NOT LIKE THIS"),
            QStringLiteral("WINTER: SNOW ON THE TRANSDUCERS"),
            QStringLiteral("WINTER: THE STABLE LAYER NEVER LEAVES"),
            QStringLiteral("WINTER: THE SHORTEST DAYS OF THE YEAR"),
            QStringLiteral("WINTER: DORMANT, BUT STILL GAPPY")
        },
        {   // wet season
            QStringLiteral("WET SEASON: THE RAINS HAVE COME"),
            QStringLiteral("WET SEASON: GREEN AND GROWING"),
            QStringLiteral("WET SEASON: MIND THE SLIPPERY TRACE"),
            QStringLiteral("WET SEASON: THE UPTAKE IS BACK"),
            QStringLiteral("WET SEASON: STORMS EVERY AFTERNOON"),
            QStringLiteral("WET SEASON: THE RIVERS ARE UP"),
            QStringLiteral("WET SEASON: THE GRASS IS TALLER THAN YOU"),
            QStringLiteral("WET SEASON: THE FLUX PULSE IS OVER, THE FLUX IS NOT")
        },
        {   // dry season
            QStringLiteral("DRY SEASON: DUST AND STILLNESS"),
            QStringLiteral("DRY SEASON: ALMOST NO UPTAKE"),
            QStringLiteral("DRY SEASON: THE GRASS HAS GONE BROWN"),
            QStringLiteral("DRY SEASON: FIRE RISK HIGH"),
            QStringLiteral("DRY SEASON: HOT DAYS, COLD NIGHTS"),
            QStringLiteral("DRY SEASON: THE TREES ARE WAITING"),
            QStringLiteral("DRY SEASON: NOT A CLOUD IN SIGHT"),
            QStringLiteral("DRY SEASON: THE SOIL IS CRACKED")
        }
    };
    return lines.at(std::clamp(season, 0, static_cast<int>(lines.size()) - 1));
}

// above the player on walking into a shower that falls as snow
const QStringList& snowPops()
{
    static const QStringList list {
        QStringLiteral("SNOW! MIND THE ICY TRACE!"),
        QStringLiteral("SNOW ON THE SONIC!"),
        QStringLiteral("ICE ON THE TRANSDUCERS!"),
        QStringLiteral("HEATERS ON!"),
        QStringLiteral("WHITE-OUT!"),
        QStringLiteral("THE IRGA IS FREEZING!"),
        QStringLiteral("SLIPPERY AND COLD!"),
        QStringLiteral("SNOWFALL FLAG!"),
        QStringLiteral("THE SOLAR PANEL IS BURIED!"),
        QStringLiteral("FROST ON THE MIRRORS!"),
        QStringLiteral("SNOWFLAKES IN THE PATH!"),
        QStringLiteral("BLIZZARD HALF-HOURS!"),
        QStringLiteral("THE TOWER IS ICING UP!"),
        QStringLiteral("COLD HANDS, WARM DATA!"),
        QStringLiteral("SKI SEASON!"),
        QStringLiteral("THE BATTERY HATES THIS!"),
        QStringLiteral("SUBLIMATION IN PROGRESS!"),
        QStringLiteral("SNOW DEPTH: RISING!"),
        QStringLiteral("STOP EARLY: IT'S ICY!"),
        QStringLiteral("THE PATH LENGTH IS FROZEN!"),
        QStringLiteral("DRIFTING SNOW!"),
        QStringLiteral("HOAR FROST EVERYWHERE!"),
        QStringLiteral("THE LOGGER HUT IS AN IGLOO!"),
        QStringLiteral("ALBEDO: MAXIMUM!"),
        QStringLiteral("SNOW AT TEN HERTZ!"),
        QStringLiteral("BRRR!"),
        QStringLiteral("WINTER WONDERLAND, POOR DATA!"),
        QStringLiteral("NO GRIP ON THE SNOW!"),
        QStringLiteral("FLAKES ON THE CAMERA!"),
        QStringLiteral("THE SKY IS FALLING, IN FLAKES!")
    };
    return list;
}

// the card after the deadline took a life
const QStringList& deadlineDeathLines()
{
    static const QStringList list {
        QStringLiteral("The deadline passed. You have been given an extension, and one life fewer."),
        QStringLiteral("Out of time. The editor grants two more weeks, of game time."),
        QStringLiteral("The clock ran out. The PI sighed and reset it."),
        QStringLiteral("Deadline missed. Your extension request was approved, at a cost."),
        QStringLiteral("Too slow. The funding agency took a life as a late fee."),
        QStringLiteral("The submission portal closed. It reopened, but it remembers."),
        QStringLiteral("You missed the deadline. Everyone does. Some pay with a life."),
        QStringLiteral("Time is up. The tower waits, a little impatiently."),
        QStringLiteral("The deadline was yesterday. You are now running on the extension."),
        QStringLiteral("The clock beat you. The clock always beats you."),
        QStringLiteral("Late again. The report is due, now with less time."),
        QStringLiteral("Deadline exceeded. A new, shorter one has been set."),
        QStringLiteral("The data were due at noon. It is not noon any more."),
        QStringLiteral("You ran out of time admiring the eddies."),
        QStringLiteral("The deadline won this round. There is a next round."),
        QStringLiteral("Extension granted. Do not ask for another."),
        QStringLiteral("The editor has noted the delay in your file."),
        QStringLiteral("The clock struck zero. So did one of your lives."),
        QStringLiteral("Overdue. The tower crew has started a betting pool."),
        QStringLiteral("You took the scenic route. The deadline did not."),
        QStringLiteral("Time over. The second deadline is shorter than the first."),
        QStringLiteral("The deadline came and went, like a cold front."),
        QStringLiteral("The report is late. The fluxes are patient. The PI is not."),
        QStringLiteral("You missed it by a week of half-hours."),
        QStringLiteral("The submission window shut. A smaller one has opened."),
        QStringLiteral("Late submission accepted, with penalties."),
        QStringLiteral("Out of time, but still in the game."),
        QStringLiteral("The calendar won. Try running faster."),
        QStringLiteral("The deadline was a suggestion. A firm one."),
        QStringLiteral("The clock is reset. Your lives are not.")
    };
    return list;
}

// A rank tier holds titles and sentence templates. The templates take
// {coverage}, {coins}, {time}, {stomps}, {gaps}, {lives} and {doy} from the
// run, and {tool}, {place} and {who}, which are filled in at random.
struct RankTier
{
    int maxRating;
    QStringList titles;
    QStringList templates;
};

// out of lives, or out of time
const RankTier& gameOverTier()
{
    static const RankTier tier
    { 0,
      {
        QStringLiteral("Lost in the Gaps"),
        QStringLiteral("Missing Data Point"),
        QStringLiteral("Honorary -9999"),
        QStringLiteral("Gap-Filled Technician"),
        QStringLiteral("u*-Filtered"),
        QStringLiteral("Reviewer 2's Case Study"),
        QStringLiteral("Unrecoverable Half-Hour"),
        QStringLiteral("Corrupted SD Card"),
        QStringLiteral("Dead Battery"),
        QStringLiteral("Night-Time Outlier"),
        QStringLiteral("Desk-Rejected Field Assistant"),
        QStringLiteral("Unpublished Dataset"),
        QStringLiteral("The Week That Wasn't"),
        QStringLiteral("Advected Away"),
        QStringLiteral("Methane Casualty"),
        QStringLiteral("Lapsed Logger"),
        QStringLiteral("Flatlined Sensor"),
        QStringLiteral("Broken Sampling Tube"),
        QStringLiteral("Power-Cut Survivor (Almost)"),
        QStringLiteral("Data Gap, Personified"),
        QStringLiteral("Flagged and Forgotten"),
        QStringLiteral("Seasonal Help, Not Renewed"),
        QStringLiteral("Lost Field Notebook"),
        QStringLiteral("Uncalibrated Analyser"),
        QStringLiteral("Wet Sonic"),
        QStringLiteral("Wind-Blown Intern"),
        QStringLiteral("Expired Field Permit"),
        QStringLiteral("Failed Stationarity Test"),
        QStringLiteral("NaN Enthusiast"),
        QStringLiteral("Missing, Presumed Interpolated")
      },
      {
        QStringLiteral("The run ended with {coverage} coverage and {gaps} jumped. The rest of the week is now a gap."),
        QStringLiteral("{who} found your hard hat near {doy}. The data were never recovered."),
        QStringLiteral("You made it as far as {doy}. The flux tower is still waiting at the end of the week."),
        QStringLiteral("{coins} collected before the week collected you."),
        QStringLiteral("The field season is over, and so is your contract. {place} sends its regards."),
        QStringLiteral("You stomped {stomps}, but the time series stomped back."),
        QStringLiteral("Somewhere around {doy}, the -9999s took over."),
        QStringLiteral("{coverage} coverage. The gap-filler will have to make up the rest, and it will be inventive."),
        QStringLiteral("You lost {lives} to the fluxes. {tool} would have lost fewer, but not many."),
        QStringLiteral("The week of {place} was too much for one technician."),
        QStringLiteral("{who} has filed your run under \"lessons learned\"."),
        QStringLiteral("You jumped {gaps} and fell into the one that mattered."),
        QStringLiteral("Reviewer 2 is still out there, along with {coins} you never collected."),
        QStringLiteral("The data from {place} will be submitted next year. Maybe."),
        QStringLiteral("You ran out of lives at {doy}. The logger kept recording without you."),
        QStringLiteral("Game over in {time}. The flux tower flag stays up for another week."),
        QStringLiteral("{coverage} coverage and a lot of character building."),
        QStringLiteral("The boundary layer won this round. {who} has seen it happen before."),
        QStringLiteral("You were last seen heading into a gap near {doy}."),
        QStringLiteral("The week ended early for you, and late for the data."),
        QStringLiteral("{stomps} defeated, {lives} lost. The annual sum is not impressed."),
        QStringLiteral("The methane, the advection and Reviewer 2 send their condolences."),
        QStringLiteral("Your run has been flagged as incomplete. So has the week."),
        QStringLiteral("You got {coins}. The tower wanted all of them."),
        QStringLiteral("{place} lost a technician and gained a gap."),
        QStringLiteral("The deadline, the gaps and the gusts: you met all of them, briefly."),
        QStringLiteral("{who} is updating the risk assessment because of you."),
        QStringLiteral("You made {doy}. That is further than the last intern got."),
        QStringLiteral("A brave run of {time}. The fluxes are unmoved."),
        QStringLiteral("The data coverage plot from your week will be shown as a warning to students.")
      } };
    return tier;
}

const QList<RankTier>& rankTiers()
{
    static const QList<RankTier> tiers {
        { 20,
          {
            QStringLiteral("Summer Intern"),
            QStringLiteral("Cable Carrier"),
            QStringLiteral("Bucket Holder"),
            QStringLiteral("Assistant to the Assistant"),
            QStringLiteral("Probationary Technician"),
            QStringLiteral("Visitor With a Lanyard"),
            QStringLiteral("Battery Fetcher"),
            QStringLiteral("Field Trip Participant"),
            QStringLiteral("Clipboard Holder"),
            QStringLiteral("First-Week PhD Student"),
            QStringLiteral("Trainee Gap Jumper"),
            QStringLiteral("Hut Sweeper"),
            QStringLiteral("Junior Rain Gauge Emptier"),
            QStringLiteral("Tourist at the Tower"),
            QStringLiteral("Zip-Tie Specialist"),
            QStringLiteral("Mud Inspector"),
            QStringLiteral("Assistant Cow Herder"),
            QStringLiteral("Backup Backup Technician"),
            QStringLiteral("Volunteer Ladder Holder"),
            QStringLiteral("Logger Watcher"),
            QStringLiteral("Novice Flux Runner"),
            QStringLiteral("Field Boot Breaker-In"),
            QStringLiteral("Data Gap Explorer"),
            QStringLiteral("Unpaid Helper"),
            QStringLiteral("Tower Base Camper"),
            QStringLiteral("Cable Tangler"),
            QStringLiteral("Sandwich Courier"),
            QStringLiteral("Mosquito Bait"),
            QStringLiteral("Apprentice of the Fluxes"),
            QStringLiteral("Hard Hat Model")
          },
          {
            QStringLiteral("You made it to the tower with {coverage} coverage. {who} is being kind about it."),
            QStringLiteral("Submitted, technically. {coins} out of a week is a start."),
            QStringLiteral("The flag is down, but the gaps outnumber the data. {place} will need another week."),
            QStringLiteral("You finished in {time}. {tool} would have finished in less, but {tool} is not here."),
            QStringLiteral("The data are in. Most of them are -9999, but they are in."),
            QStringLiteral("A run of {time} with {coverage} coverage. Everyone starts somewhere."),
            QStringLiteral("You reached the tower. The tower is surprised."),
            QStringLiteral("{coins} collected and {lives} lost. {who} has seen worse. Not much worse."),
            QStringLiteral("The week of {place} is submitted, in the loosest sense of the word."),
            QStringLiteral("You jumped {gaps}. The rest of the gaps jumped you."),
            QStringLiteral("You stomped {stomps}. The rest are still walking around {place}."),
            QStringLiteral("Submitted with a lot of help from the gap-filler."),
            QStringLiteral("Coverage {coverage}. The cumulative sum looks like a staircase."),
            QStringLiteral("You finished. {who} is already preparing the training slides."),
            QStringLiteral("The flux tower accepts your data with visible reluctance."),
            QStringLiteral("A week of fluxes, roughly a third of it present. Good effort."),
            QStringLiteral("You made it. The data paper will need a long limitations section."),
            QStringLiteral("{time} of running for {coins}. Every half-hour counts, apparently."),
            QStringLiteral("You arrived at the hut. The hut asks where the rest of the data are."),
            QStringLiteral("Submitted. {place} has filed it under \"preliminary\"."),
            QStringLiteral("The tower flag came down, and so did the quality flags."),
            QStringLiteral("A modest run. {tool} sends its encouragement."),
            QStringLiteral("You reached {doy} and kept going. That counts for something."),
            QStringLiteral("Coverage {coverage}: enough for a poster, not for a paper."),
            QStringLiteral("You finished the week. The week finished you a little too."),
            QStringLiteral("The data are in the archive, mostly as gaps."),
            QStringLiteral("{who} says the next run will be better. {who} says that every time."),
            QStringLiteral("Submitted in {time}. The reviewers will have questions."),
            QStringLiteral("You made the tower. The eddies made you work for it."),
            QStringLiteral("A start. Every flux researcher was once a summer intern.")
          } },
        { 40,
          {
            QStringLiteral("Field Assistant"),
            QStringLiteral("Junior Technician"),
            QStringLiteral("Logger Whisperer in Training"),
            QStringLiteral("Second-Year PhD Student"),
            QStringLiteral("Data Wrangler"),
            QStringLiteral("Tube Cleaner"),
            QStringLiteral("Rain Cap Specialist"),
            QStringLiteral("Calibration Assistant"),
            QStringLiteral("Mast Climber, Lower Half"),
            QStringLiteral("Half-Hour Collector"),
            QStringLiteral("Gap Hopper"),
            QStringLiteral("Assistant Station Manager"),
            QStringLiteral("Hut Keeper"),
            QStringLiteral("Seasonal Technician"),
            QStringLiteral("Cable Router"),
            QStringLiteral("Reviewer 2 Dodger"),
            QStringLiteral("Bubble Avoider"),
            QStringLiteral("Night-Shift Runner"),
            QStringLiteral("Junior Despiker"),
            QStringLiteral("Field Season Regular"),
            QStringLiteral("Sonic Wiper"),
            QStringLiteral("Analyser Babysitter"),
            QStringLiteral("Promising Student"),
            QStringLiteral("Weekend Flux Runner"),
            QStringLiteral("Data Courier"),
            QStringLiteral("Coverage Apprentice"),
            QStringLiteral("Upper Ladder Holder"),
            QStringLiteral("Time Lag Estimator"),
            QStringLiteral("Assistant Eddy Tamer"),
            QStringLiteral("Hard Hat Wearer, Certified")
          },
          {
            QStringLiteral("Coverage {coverage} in {time}. {who} nods, slowly."),
            QStringLiteral("A decent week at {place}: {coins} collected and {gaps} jumped."),
            QStringLiteral("You submitted the data with a respectable {coverage}. The gap-filler has less to invent."),
            QStringLiteral("You stomped {stomps} on the way. Reviewer 2 is regrouping."),
            QStringLiteral("Not bad. {tool} would like a word about your jumping technique."),
            QStringLiteral("{time} from the first sample to the hut. The tower crew clocked you."),
            QStringLiteral("You lost {lives}, but the week is in. That is what counts."),
            QStringLiteral("The flux tower accepts your data and suggests a refresher course."),
            QStringLiteral("Coverage {coverage}: enough for a conference talk."),
            QStringLiteral("You jumped {gaps} without falling into most of them. Progress."),
            QStringLiteral("A solid run. {place} will use it in the annual report, with caveats."),
            QStringLiteral("{coins} collected. The cumulative sum has fewer steps now."),
            QStringLiteral("The data from {doy} onwards look particularly good. Well done."),
            QStringLiteral("{who} has upgraded you from \"helper\" to \"useful\"."),
            QStringLiteral("You handled the eddies reasonably. The eddies handled you sometimes."),
            QStringLiteral("A week of fluxes, about half of it rescued. The PI is cautiously optimistic."),
            QStringLiteral("Submitted in {time}, with {coverage} coverage. Next year, the full week."),
            QStringLiteral("Your run shows promise and a certain enthusiasm for falling."),
            QStringLiteral("The tower flag came down for you. It does not do that for everyone."),
            QStringLiteral("{stomps} defeated. The rest have filed a complaint with the editor."),
            QStringLiteral("You navigated the fluxes with moderate success, which is all anyone can ask."),
            QStringLiteral("The gap-filler and you share the credit for this week roughly equally."),
            QStringLiteral("A respectable showing. {tool} is mildly impressed."),
            QStringLiteral("Coverage {coverage}: the reviewers will accept it after one round."),
            QStringLiteral("You ran {place}'s week in {time}. The logger approves."),
            QStringLiteral("{coins} good half-hours. The data paper now has a results section."),
            QStringLiteral("You made it with {lives} to spare. The tower crew is taking bets on next time."),
            QStringLiteral("The week is submitted. The quality flags are mostly green."),
            QStringLiteral("You found {coins} in a week of gaps. That is fieldwork."),
            QStringLiteral("A good run. {who} wants you back for the next campaign.")
          } },
        { 60,
          {
            QStringLiteral("Field Technician"),
            QStringLiteral("Station Manager"),
            QStringLiteral("Senior PhD Student"),
            QStringLiteral("Logger Whisperer"),
            QStringLiteral("Gap Jumper"),
            QStringLiteral("Eddy Rider"),
            QStringLiteral("Tower Climber"),
            QStringLiteral("Thermal Surfer"),
            QStringLiteral("Calibration Expert"),
            QStringLiteral("Despiking Specialist"),
            QStringLiteral("Coverage Champion (Regional)"),
            QStringLiteral("Reviewer 2 Hunter"),
            QStringLiteral("Night-Shift Veteran"),
            QStringLiteral("Tube Navigator"),
            QStringLiteral("Postdoc in Waiting"),
            QStringLiteral("Experienced Flux Runner"),
            QStringLiteral("Advection Wrangler"),
            QStringLiteral("Methane Dodger"),
            QStringLiteral("Data Rescuer"),
            QStringLiteral("Senior Technician"),
            QStringLiteral("Boundary Layer Navigator"),
            QStringLiteral("Quality Controller"),
            QStringLiteral("Hard Hat Veteran"),
            QStringLiteral("Field Campaign Lead"),
            QStringLiteral("Friend of the Tower"),
            QStringLiteral("Covariance Practitioner"),
            QStringLiteral("Master of the Raw Data"),
            QStringLiteral("Keeper of the Half-Hours"),
            QStringLiteral("Trusted With the Car Keys"),
            QStringLiteral("Rain-Proof Runner")
          },
          {
            QStringLiteral("A strong week: {coverage} coverage in {time}. {who} is impressed."),
            QStringLiteral("You rescued {coins} and stomped {stomps}. {place} is in good hands."),
            QStringLiteral("The flux tower lowers its flag with respect. {coverage} coverage is serious work."),
            QStringLiteral("{tool} would have struggled to match that. Well run."),
            QStringLiteral("You jumped {gaps} and rode the eddies like a professional."),
            QStringLiteral("Submitted in {time}. The reviewers will ask for minor revisions only."),
            QStringLiteral("Coverage {coverage}: the annual sum can finally be trusted."),
            QStringLiteral("You lost {lives} along the way, but the data are excellent."),
            QStringLiteral("A fine run. {who} has started copying your technique."),
            QStringLiteral("The gap-filler barely had to work. It sent a thank-you note."),
            QStringLiteral("{coins} good half-hours from {place}. That is a publishable week."),
            QStringLiteral("You navigated the fluxes like someone who has read the manual. Twice."),
            QStringLiteral("The data paper is half written already. The other half is your run."),
            QStringLiteral("You handled the turbulence, the rain and the night. The tower salutes you."),
            QStringLiteral("{stomps} defeated. Reviewer 2 now sends polite emails."),
            QStringLiteral("A professional run of {time}. The funding agency is renewing."),
            QStringLiteral("Coverage {coverage} with {lives} lost. The safety officer and the PI are both happy."),
            QStringLiteral("You took the shortcuts and the long way, and both worked."),
            QStringLiteral("Excellent work at {place}. The data will be in next year's synthesis."),
            QStringLiteral("The tower crew applauded as you slid down the mast."),
            QStringLiteral("{who} says you are ready to run your own site."),
            QStringLiteral("You found {coins} and lost very little. That is the job."),
            QStringLiteral("A clean, quick week. {tool} has asked for your settings."),
            QStringLiteral("The cumulative sum is smooth, the coverage is {coverage}, and the flag is down."),
            QStringLiteral("The eddies tried. You rode them anyway."),
            QStringLiteral("A strong week from {doy} to the end. Very nicely done."),
            QStringLiteral("You submitted in {time}. The deadline never stood a chance."),
            QStringLiteral("Respectable coverage, excellent stomping, decent hat management."),
            QStringLiteral("The flux tower has requested you by name."),
            QStringLiteral("Well navigated. The fluxes were in good hands.")
          } },
        { 80,
          {
            QStringLiteral("Postdoc"),
            QStringLiteral("Principal Technician"),
            QStringLiteral("Site PI"),
            QStringLiteral("Lead Flux Runner"),
            QStringLiteral("Eddy Tamer"),
            QStringLiteral("Master of Thermals"),
            QStringLiteral("Senior Tower Climber"),
            QStringLiteral("Coverage Champion (National)"),
            QStringLiteral("Gap Annihilator"),
            QStringLiteral("Reviewer 2's Nemesis"),
            QStringLiteral("Lord of the Loggers"),
            QStringLiteral("Covariance Virtuoso"),
            QStringLiteral("The Technician They Call"),
            QStringLiteral("Boundary Layer Expert"),
            QStringLiteral("Keeper of the Tower"),
            QStringLiteral("Data Coverage Hero"),
            QStringLiteral("Night-Time Specialist"),
            QStringLiteral("Advection Slayer"),
            QStringLiteral("Methane Master"),
            QStringLiteral("Senior Scientist (Field)"),
            QStringLiteral("Tenure-Track Runner"),
            QStringLiteral("Network Coordinator"),
            QStringLiteral("Calibration Guru"),
            QStringLiteral("The Despiker"),
            QStringLiteral("Flux Tower Legend (Local)"),
            QStringLiteral("Grant Winner"),
            QStringLiteral("Thermal Glider"),
            QStringLiteral("Hard Hat of Honour"),
            QStringLiteral("Unflappable in the Rain"),
            QStringLiteral("Master of the Half-Hour")
          },
          {
            QStringLiteral("Outstanding: {coverage} coverage in {time}. {who} is speechless."),
            QStringLiteral("You ran {place}'s week like it was your own backyard. {coins} rescued."),
            QStringLiteral("The flux tower flies its flag at half-mast in your honour. Wait, that is the wrong way round."),
            QStringLiteral("{tool} has been retired. You are the method now."),
            QStringLiteral("{stomps} defeated and {gaps} jumped. Reviewer 2 has changed career."),
            QStringLiteral("Coverage {coverage}. The annual sum has the smallest error bar {place} has ever seen."),
            QStringLiteral("You navigated the fluxes in {time}. The tower crew is still clapping."),
            QStringLiteral("The gap-filler has filed for unemployment. Well done."),
            QStringLiteral("A superb week, lost {lives} and submitted early. The PI wants to co-author."),
            QStringLiteral("You rode every eddy and surfed every thermal. The boundary layer respects you."),
            QStringLiteral("Excellent. {who} is citing your run in the next proposal."),
            QStringLiteral("The data from this week will be used to validate satellites."),
            QStringLiteral("{coins} good half-hours. FLUXNET wants to talk to you."),
            QStringLiteral("You made a hard week look easy, which is how you know it was hard."),
            QStringLiteral("Masterful. The safety officer only flinched twice."),
            QStringLiteral("The reviewers accepted it without comments. That has never happened before."),
            QStringLiteral("{time} from {doy} to the hut. The deadline is jealous."),
            QStringLiteral("You took the tubes, ducked the advection and popped the methane. Textbook."),
            QStringLiteral("A brilliant run. {tool} is updating its default settings to match."),
            QStringLiteral("Coverage {coverage}. The data paper is now a Nature paper."),
            QStringLiteral("The eddy covariance community would like you to give a keynote."),
            QStringLiteral("You lost almost nothing and found almost everything."),
            QStringLiteral("A top-tier week. The network is naming a site after you."),
            QStringLiteral("You stomped {stomps} and collected {coins}. The tower is in awe."),
            QStringLiteral("Excellent work at {place}. Next year, they want you for the whole season."),
            QStringLiteral("You handled the night, the rain and Reviewer 2. Nothing frightens you now."),
            QStringLiteral("Submitted early with {coverage} coverage. The funding is renewed for ten years."),
            QStringLiteral("{who} has started calling you \"the flux runner\"."),
            QStringLiteral("A beautiful run. The cumulative sum is a work of art."),
            QStringLiteral("The flag came down, the data went up, and everyone is happy.")
          } },
        { std::numeric_limits<int>::max(),
          {
            QStringLiteral("Senior Scientist"),
            QStringLiteral("Flux Tower Legend"),
            QStringLiteral("Grand Master of the Fluxes"),
            QStringLiteral("Supreme Eddy Covariant"),
            QStringLiteral("The Ultimate Technician"),
            QStringLiteral("Lord of the Boundary Layer"),
            QStringLiteral("Keeper of All the Half-Hours"),
            QStringLiteral("Emperor of Coverage"),
            QStringLiteral("Gap-Free Visionary"),
            QStringLiteral("Reviewer 2's Final Boss"),
            QStringLiteral("Thermal Royalty"),
            QStringLiteral("Archmage of the Analysers"),
            QStringLiteral("Covariance Incarnate"),
            QStringLiteral("The One Who Closed the Energy Balance"),
            QStringLiteral("Legend of the Long Week"),
            QStringLiteral("Head of the Network"),
            QStringLiteral("Director of the Institute"),
            QStringLiteral("Nobel Laureate (Fieldwork)"),
            QStringLiteral("The Data Whisperer"),
            QStringLiteral("Master of Time Lags"),
            QStringLiteral("Grand Despiker"),
            QStringLiteral("Saviour of the Annual Sum"),
            QStringLiteral("The Flux Runner"),
            QStringLiteral("Champion of the Tower"),
            QStringLiteral("Wielder of the Hard Hat"),
            QStringLiteral("Tamer of All Eddies"),
            QStringLiteral("The Uninterrupted"),
            QStringLiteral("Emeritus Field Hero"),
            QStringLiteral("Guardian of the Footprint"),
            QStringLiteral("The Technician of Legend")
          },
          {
            QStringLiteral("Legendary: {coverage} coverage in {time}. {who} has framed your run."),
            QStringLiteral("You ran {place}'s week to perfection. {coins} rescued, {stomps} defeated, the tower in awe."),
            QStringLiteral("The flux tower lowered its flag and then bowed. Towers do not usually bow."),
            QStringLiteral("{tool} wants to be you when it grows up."),
            QStringLiteral("Coverage {coverage}. The energy balance closed out of respect."),
            QStringLiteral("You navigated the fluxes in {time}, and the fluxes thanked you for it."),
            QStringLiteral("Reviewer 2 has retired. Reviewer 3 is considering it."),
            QStringLiteral("The gap-filler has nothing left to fill. It is writing a memoir."),
            QStringLiteral("A perfect week at {place}. They are building a statue at the foot of the tower."),
            QStringLiteral("You surfed the boundary layer like it was designed for you. Perhaps it was."),
            QStringLiteral("{who} has rewritten the field manual. It now just says \"do what they did\"."),
            QStringLiteral("The data from this week are the new gold standard for {place}."),
            QStringLiteral("{coins} good half-hours, {gaps} jumped, {lives} lost. Mythical."),
            QStringLiteral("Your run has been submitted, accepted, cited and taught in one afternoon."),
            QStringLiteral("The eddies line up to carry you now."),
            QStringLiteral("The deadline asked for an extension, from you."),
            QStringLiteral("Masterful from {doy} to the hut. The tower crew wept openly."),
            QStringLiteral("You are what the eddy covariance method dreams of at night."),
            QStringLiteral("FLUXNET, ICOS and AmeriFlux are fighting over your data."),
            QStringLiteral("The annual sum has no error bar any more. It no longer needs one."),
            QStringLiteral("{tool} has been renamed in your honour."),
            QStringLiteral("You took every shortcut and none of the falls. Incredible."),
            QStringLiteral("A run for the history books, and the history books are being updated."),
            QStringLiteral("The cumulative sum from your week now hangs in a museum."),
            QStringLiteral("You have achieved the impossible: a gap-free week."),
            QStringLiteral("The boundary layer will tell stories about you to its children."),
            QStringLiteral("The flux tower has asked to be your assistant."),
            QStringLiteral("Rip and tear? No: run and submit. {who} is in awe."),
            QStringLiteral("Even the methane bubbles applauded."),
            QStringLiteral("There is no higher rank. There is only more data.")
          } }
    };
    return tiers;
}

struct Rank
{
    QString title;
    QString sentence;
};

QString counted(int count, const char* one, const char* many)
{
    return count == 1 ? QStringLiteral("1 %1").arg(QLatin1String(one))
                      : QStringLiteral("%1 %2").arg(count).arg(QLatin1String(many));
}

QString minutes(qreal seconds)
{
    const int total = std::max(qRound(seconds), 0);
    return QStringLiteral("%1:%2").arg(total / 60).arg(total % 60, 2, 10, QLatin1Char('0'));
}

// the run's totals in the rank sentences; levelOnly for a level's summary
QString fillRunTemplate(const QString& sentence, const FluxRunner::Result& r, bool levelOnly = false)
{
    const int coins = levelOnly ? r.coins : r.runCoins;
    const int total = levelOnly ? r.totalCoins : r.runTotalCoins;
    const int coverage = qRound(100.0 * coins / std::max(total, 1));
    return fillFlavour(sentence, {
        { QStringLiteral("coverage"), QStringLiteral("%1%").arg(coverage) },
        { QStringLiteral("coins"), counted(coins, "good half-hour", "good half-hours") },
        { QStringLiteral("time"), minutes(levelOnly ? r.seconds : r.runSeconds) },
        { QStringLiteral("stomps"), counted(levelOnly ? r.defeated : r.runDefeated, "foe", "foes") },
        { QStringLiteral("gaps"), counted(levelOnly ? r.gapsBridged : r.runGaps, "gap", "gaps") },
        { QStringLiteral("lives"), counted(levelOnly ? r.livesLost : r.runLivesLost, "life", "lives") },
        { QStringLiteral("doy"), QStringLiteral("DOY %1").arg(r.firstDoy) },
        { QStringLiteral("levels"), counted(r.levelsCleared, "level", "levels") },
        { QStringLiteral("level"), QString::number(r.level) },
        { QStringLiteral("site"), r.site },
        { QStringLiteral("firstsite"), r.firstSite.isEmpty() ? r.site : r.firstSite }
    });
}

// the rank of the whole run: nothing cleared is a game over, otherwise
// the tiers follow the run's rating
Rank runRank(const FluxRunner::Result& r)
{
    const RankTier* tier = &gameOverTier();
    if (r.levelsCleared > 0)
    {
        for (const auto& candidate : rankTiers())
        {
            if (r.rating <= candidate.maxRating)
            {
                tier = &candidate;
                break;
            }
        }
    }

    QString sentence = fillRunTemplate(pickFresh(tier->templates), r);
    if (r.levelsCleared > 0)
        sentence += QLatin1Char(' ') + fillRunTemplate(pickFresh(levelsTemplates()), r);
    if (r.timedOut)
        sentence += QLatin1Char(' ') + fillRunTemplate(pickFresh(deadlineTemplates()), r);

    return { pickFresh(tier->titles), sentence };
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// FluxRunner: setting up

FluxRunner::FluxRunner(QWidget* parent) :
    QWidget(parent),
    state_(State::Idle),
    timer_(nullptr),
    accumulator_(0.0),
    stateTime_(0.0),
    clockTime_(0.0),
    held_(0),
    jumpBuffer_(0.0),
    coyote_(0.0),
    invulnerable_(0.0),
    coffee_(0.0),
    fireCooldown_(0.0),
    runDistance_(0.0),
    facingLeft_(false),
    crouching_(false),
    power_(Power::Small),
    player_({ QPointF(), QPointF(), PLAYER_HALF_W, PLAYER_HEIGHT, false }),
    cameraX_(0.0),
    stompChain_(0),
    figure_(0),
    blockCoins_(0),
    spec_(specFor(1)),
    level_(1),
    site_(Forest),
    seasonIndex_(Summer),
    modifiers_(0),
    seasonFactor_(1.0),
    sunrise_(6.0),
    sunset_(18.0),
    startSlot_(12),
    dataSlots_(264),
    weather_(1),
    frontDay_(-1),
    rhythm_(Ramp),
    rhythmPhase_(0.0),
    rhythmPeriod_(120.0),
    cloudCount_(6),
    snowy_(false),
    cardDuration_(CARD_S),
    checkpointColumn_(0),
    towerColumn_(0),
    hutColumn_(0),
    respawnColumn_(0),
    checkpointTaken_(false),
    nextGap_(0),
    firstDoy_(180),
    tubeIndex_(-1),
    clearStage_(0),
    flagY_(0.0),
    nextEddy_(0.0),
    diedInPit_(false),
    diedOfDeadline_(false),
    nightAnnounced_(false),
    dangerAnnounced_(false),
    sky_(DAY_SKY),
    bannerColor_(AMBER),
    bannerAge_(BANNER_S),
    lives_(START_LIVES),
    livesLost_(0),
    coinCount_(0),
    defeated_(0),
    gapsBridged_(0),
    deadline_(0.0),
    levelDeadline_(0.0),
    runTime_(0.0),
    cleared_(false),
    timedOut_(false),
    score_(0),
    levelsCleared_(0),
    runCoins_(0),
    runTotalCoins_(0),
    runDefeated_(0),
    runGaps_(0),
    runLivesLost_(0),
    runSeconds_(0.0),
    qualitySum_(0.0),
    qualityWeight_(0.0)
{
    // the arrows and Space belong to the game while it runs
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(560, PLAY_HEIGHT + HUD_HEIGHT);
    setMaximumHeight(PLAY_HEIGHT + HUD_HEIGHT);
    setAttribute(Qt::WA_OpaquePaintEvent);

    timer_ = new QTimer(this);
    timer_->setInterval(TICK_MS);
    timer_->setTimerType(Qt::PreciseTimer);
    connect(timer_, &QTimer::timeout, this, &FluxRunner::tick);
}

QSize FluxRunner::sizeHint() const
{
    return QSize(720, PLAY_HEIGHT + HUD_HEIGHT);
}

void FluxRunner::startRun(int figure)
{
    figure_ = std::clamp(figure, 0, 1);
    lives_ = START_LIVES;
    score_ = 0;
    levelsCleared_ = 0;
    runCoins_ = 0;
    runTotalCoins_ = 0;
    runDefeated_ = 0;
    runGaps_ = 0;
    runLivesLost_ = 0;
    runSeconds_ = 0.0;
    qualitySum_ = 0.0;
    qualityWeight_ = 0.0;
    firstSite_.clear();
    power_ = Power::Small;
    timedOut_ = false;
    startLevel(1);
}

void FluxRunner::nextLevel()
{
    startLevel(level_ + 1);
}

void FluxRunner::startLevel(int level)
{
    level_ = std::max(level, 1);
    spec_ = specFor(level_);
    drawSetting();

    // every level is checked before it is played; one that will not pass
    // after twenty tries takes the previous level's hazards, then none
    bool built = false;
    for (int attempt = 0; attempt < 20 && !built; ++attempt)
        built = generateLevel(true) && verifyLevel();
    if (!built && level_ > 1)
    {
        const LevelSpec own = spec_;
        spec_ = specFor(level_ - 1);
        spec_.pointsFactor = own.pointsFactor;
        for (int attempt = 0; attempt < 20 && !built; ++attempt)
            built = generateLevel(true) && verifyLevel();
    }
    if (!built)
        generateLevel(false);
    buildSamples();
    placeThings();
    if (firstSite_.isEmpty())
        firstSite_ = siteName_;

    stars_.clear();
    for (int i = 0; i < 70; ++i)
        stars_.append(QPointF(uniform(0.0, 1.0), uniform(0.0, 0.55)));

    livesLost_ = 0;
    coinCount_ = 0;
    defeated_ = 0;
    gapsBridged_ = 0;
    levelDeadline_ = spec_.deadlinePerSlot * dataSlots_;
    deadline_ = levelDeadline_;
    runTime_ = 0.0;
    clockTime_ = 0.0;
    cleared_ = false;
    timedOut_ = false;
    diedOfDeadline_ = false;
    checkpointTaken_ = false;
    respawnColumn_ = 3;
    nextGap_ = 0;
    clearStage_ = 0;
    nightAnnounced_ = false;
    dangerAnnounced_ = false;
    tubeIndex_ = -1;
    flagY_ = bars_.at(towerColumn_).top - 250.0;
    nextEddy_ = 0.0;
    pops_.clear();
    eddies_.clear();
    held_ = 0;
    accumulator_ = 0.0;

    cameraX_ = 0.0;
    sky_ = mix(DAY_SKY, NIGHT_SKY, nightAt(LEAD * BAR_W));
    cardLine_ = fillFlavour(pickFresh(siteLines(site_)), { { QStringLiteral("place"), siteName_ } });

    // the power-up carries over from the level before, as in the original
    const Power carried = power_;
    respawn();
    power_ = carried;
    cardDuration_ = NEW_LEVEL_CARD_S;
    announce(pickFresh(seasonLines(seasonIndex_)), ICE);
    bannerAge_ = -cardDuration_;    // shown once the card has gone

    setFocus(Qt::OtherFocusReason);
    clock_.start();
    timer_->start();
    update();
}

void FluxRunner::stop()
{
    timer_->stop();
    state_ = State::Idle;
    held_ = 0;
    foes_.clear();
    pickups_.clear();
    shots_.clear();
    pops_.clear();
    eddies_.clear();
    update();
}

FluxRunner::LevelSpec FluxRunner::specFor(int level)
{
    // level 1 is gentle, level 6 is everything at its cap, and the points
    // keep climbing after that
    const qreal t = std::clamp((level - 1) / 5.0, 0.0, 1.0);
    auto at = [t](qreal a, qreal b) { return a + (b - a) * t; };
    auto atInt = [t](int a, int b) { return static_cast<int>(std::round(a + (b - a) * t)); };

    LevelSpec s;
    s.level = level;
    s.minSlots = std::min(atInt(240, 336), CAP_SLOTS);
    s.maxSlots = std::min(atInt(288, 432), CAP_SLOTS);
    s.pitsPer100 = at(3.5, 4.5);
    s.wallsPer100 = at(2.75, 3.75);
    s.earlyPitMin = 2;
    s.earlyPitMax = std::min(atInt(2, 4), CAP_PIT);
    s.latePitMin = std::min(atInt(2, 4), CAP_PIT);
    s.latePitMax = std::min(atInt(3, 5), CAP_PIT);
    s.maxPit = std::min(atInt(3, 5), CAP_PIT);
    s.maxZonePit = std::min(atInt(2, 4), CAP_ZONE_PIT);
    s.maxLandingRise = std::min(at(40.0, 56.0), CAP_LANDING_RISE);
    s.earlyRiseMin = at(32.0, 64.0);
    s.earlyRiseMax = at(48.0, 88.0);
    s.lateRiseMin = at(48.0, 88.0);
    s.lateRiseMax = std::min(at(64.0, 112.0), CAP_RISE);
    s.doubleWallChance = at(0.25, 0.45);
    s.pad = std::max(atInt(5, 4), CAP_PAD);
    s.movingMin = atInt(2, 3);
    s.movingMax = atInt(3, 5);
    s.movingAmplitude = std::min(at(28.0, 48.0), CAP_MOVING_AMPLITUDE);
    s.movingPeriod = std::max(at(3.4, 2.4), CAP_MOVING_PERIOD);
    s.tubesMin = 2;
    s.tubesMax = 3;
    s.showersMin = atInt(1, 3);
    s.showersMax = atInt(2, 4);
    s.zoneShare = at(0.5, 1.0);
    s.zoneStrengthMin = at(0.4, 0.7);
    s.zoneStrengthMax = at(0.7, 1.0);
    s.gustMax = std::min(at(30.0, 48.0), CAP_GUST);
    s.eddyKick = std::min(at(420.0, 560.0), CAP_KICK);
    s.eddyIntervalMin = at(1.6, 1.0);
    s.eddyIntervalMax = at(2.8, 2.0);
    s.maxEddies = atInt(4, 5);
    s.reviewerGapMin = atInt(12, 7);
    s.reviewerGapMax = atInt(18, 11);
    s.reviewerSpeed = std::min(at(40.0, 70.0), CAP_REVIEWER_SPEED);
    s.puffsPer100 = at(2.0, 5.0);
    s.puffSpeed = std::min(at(115.0, 170.0), CAP_PUFF_SPEED);
    s.bubbleChance = at(0.35, 0.8);
    s.bubblePauseMin = std::max(at(2.4, 1.6), CAP_BUBBLE_PAUSE);
    s.bubblePauseMax = std::max(at(3.4, 2.4), s.bubblePauseMin + 0.4);
    s.dangersPer100 = at(1.0, 2.5);
    s.hats = std::max(atInt(4, 2), 2);
    s.filters = std::max(atInt(2, 1), 1);
    s.coffees = std::max(atInt(2, 1), 1);
    s.maxBlocks = atInt(18, 12);
    s.coinRowChance = at(0.6, 0.45);
    s.deadlinePerSlot = std::max(at(0.95, 0.65), CAP_DEADLINE_PER_SLOT);
    s.pointsFactor = 1.0 + 0.25 * (level - 1);
    return s;
}

void FluxRunner::drawSetting()
{
    // modifiers first, from the seventh level on: some choose the site or the hour
    modifiers_ = 0;
    modifierNames_.clear();
    const int wanted = level_ >= 10 ? 2 : (level_ >= 7 ? 1 : 0);
    while (static_cast<int>(modifierNames_.size()) < wanted)
    {
        const auto& modifier = MODIFIERS.at(QRandomGenerator::global()->bounded(
            static_cast<int>(MODIFIERS.size())));
        if (modifiers_ & modifier.first)
            continue;
        modifiers_ |= modifier.first;
        modifierNames_ << QString::fromLatin1(modifier.second);
    }

    // the site, the date, and what the date means there
    site_ = (modifiers_ & PolarNight) ? Tundra
                                      : QRandomGenerator::global()->bounded(static_cast<int>(sites().size()));
    const Site& site = sites().at(site_);
    firstDoy_ = (modifiers_ & PolarNight) ? wrapDoy(between(345, 375)) : between(1, 365);
    const int fromPeak = std::abs(firstDoy_ - site.seasonPeak);
    const int distance = std::min(fromPeak, 365 - fromPeak);
    seasonFactor_ = site.seasonFloor + (1.0 - site.seasonFloor)
                    * std::exp(-std::pow(distance / static_cast<qreal>(site.seasonWidth), 2));
    if (site.wetAndDry)
        seasonIndex_ = seasonFactor_ > 0.4 ? WetSeason : DrySeason;
    else if (firstDoy_ >= 60 && firstDoy_ < 152)
        seasonIndex_ = Spring;
    else if (firstDoy_ >= 152 && firstDoy_ < 244)
        seasonIndex_ = Summer;
    else if (firstDoy_ >= 244 && firstDoy_ < 335)
        seasonIndex_ = Autumn;
    else
        seasonIndex_ = Winter;
    season_ = QString::fromLatin1(SEASON_NAMES.at(seasonIndex_));
    const qreal dayLength = std::clamp(
        12.0 + site.dayAmplitude * std::cos(2.0 * PI * (firstDoy_ - 172) / 365.0), 5.0, 20.0);
    sunrise_ = 12.0 - dayLength / 2.0;
    sunset_ = 12.0 + dayLength / 2.0;
    snowy_ = site.snowy && (seasonIndex_ == Winter || (site_ == Tundra && seasonFactor_ < 0.5));

    // a name and a code, as on the network's map
    const QString stem = pickFresh(placeStems());
    siteName_ = stem + QLatin1Char(' ') + pickAny(site.suffixes);
    siteCode_ = pickAny(site.countries) + QLatin1Char('-') + QString(stem).remove(QLatin1Char(' ')).left(3);

    // how long, and from what time of day: the first level in daylight
    dataSlots_ = between(spec_.minSlots, spec_.maxSlots);
    const int sunriseSlot = qRound(sunrise_ * 2.0);
    const int sunsetSlot = qRound(sunset_ * 2.0);
    if (modifiers_ & NightShift)
        startSlot_ = (sunsetSlot - 2 + 48) % 48;
    else if (level_ == 1)
        startSlot_ = between(sunriseSlot, sunriseSlot + 12) % 48;
    else if (chance(0.6))
        startSlot_ = between(0, 47);
    else
        startSlot_ = ((chance(0.5) ? sunriseSlot : sunsetSlot) + between(-4, 4) + 48) % 48;

    // the weather of the week
    const qreal draw = uniform(0.0, 1.0);
    weather_ = draw < 0.3 ? 0 : (draw < 0.75 ? 1 : 2);
    weatherName_ = QString::fromLatin1(WEATHER_NAMES.at(weather_));
    const int days = (dataSlots_ + startSlot_ + 47) / 48;
    frontDay_ = chance(0.3) ? between(0, days - 1) : -1;
    cloudCount_ = weather_ == 0 ? between(1, 3) : (weather_ == 1 ? between(4, 7) : between(8, 11));

    // the rhythm of its hazards
    rhythm_ = between(Ramp, HardFirst);
    rhythmPhase_ = uniform(0.0, 2.0 * PI);
    rhythmPeriod_ = uniform(96.0, 144.0);
    clusters_.clear();
    const int centres = between(3, 5);
    for (int i = 0; i < centres; ++i)
        clusters_.append(between(16, std::max(dataSlots_ - 12, 17)));

    // and what the modifiers change, within the caps
    if (modifiers_ & StormWeek)
        spec_.zoneStrengthMin = spec_.zoneStrengthMax = 1.0;
    if (modifiers_ & MethaneWeek)
        spec_.bubbleChance = 1.0;
    if (modifiers_ & ReviewerConference)
    {
        spec_.reviewerGapMin = std::max(spec_.reviewerGapMin * 2 / 3, 5);
        spec_.reviewerGapMax = std::max(spec_.reviewerGapMax * 2 / 3, spec_.reviewerGapMin + 1);
    }
    if (modifiers_ & NightShift)
        spec_.puffsPer100 *= 2.0;
    if (modifiers_ & PowerCut)
    {
        spec_.tubesMin = spec_.tubesMax = 0;
        spec_.coffees += 1;
    }
    if (modifiers_ & DataRich)
        spec_.coinRowChance = std::min(spec_.coinRowChance * 1.5, 0.9);
    if (modifiers_ & DeadlineCrunch)
        spec_.deadlinePerSlot = CAP_DEADLINE_PER_SLOT;
}

////////////////////////////////////////////////////////////////////////////////
// the level

int FluxRunner::columnOf(qreal x) const
{
    return static_cast<int>(std::floor(x / BAR_W));
}

int FluxRunner::sampleOf(qreal x) const
{
    return static_cast<int>(std::floor(x / SAMPLE_W));
}

qreal FluxRunner::surfaceAt(int sample) const
{
    // the start of the level is a wall; past its end the last point carries on
    if (sample < 0)
        return -INF;
    if (samples_.isEmpty())
        return FLOOR;
    sample = std::min(sample, static_cast<int>(samples_.size()) - 1);
    const Sample& s = samples_.at(sample);
    if (s.gap)
        return INF;
    qreal top = s.top;
    if (s.moving >= 0)
    {
        const auto& run = moving_.at(s.moving);
        top -= run.amplitude * std::sin(2.0 * PI * clockTime_ / run.period + run.phase);
    }
    return top;
}

qreal FluxRunner::groundTop(qreal x) const
{
    return surfaceAt(sampleOf(x));
}

qreal FluxRunner::standTop(qreal x, qreal halfW) const
{
    // the highest point of the trace under a body standing at x
    qreal best = INF;
    for (int i = sampleOf(x - halfW + EPS); i <= sampleOf(x + halfW - EPS); ++i)
        best = std::min(best, surfaceAt(i));
    return best;
}

qreal FluxRunner::supportTop(qreal left, qreal right, qreal feet) const
{
    // the highest surface under the body that is not above its knees
    qreal best = INF;
    for (int i = sampleOf(left + EPS); i <= sampleOf(right - EPS); ++i)
    {
        const qreal top = surfaceAt(i);
        if (top >= feet - STEP_UP - EPS && top < best)
            best = top;
    }
    for (const auto& block : blocks_)
    {
        const qreal x0 = block.column * BAR_W;
        if (right > x0 + EPS && left < x0 + BLOCK - EPS)
        {
            const qreal top = block.bottom - BLOCK;
            if (top >= feet - STEP_UP - EPS && top < best)
                best = top;
        }
    }
    return best;
}

bool FluxRunner::nearGap(int column, int reach) const
{
    for (int c = std::max(column - reach, 0); c <= std::min(column + reach, columnCount() - 1); ++c)
    {
        if (bars_.at(c).gap)
            return true;
    }
    return false;
}

qreal FluxRunner::hourOfSlot(int t) const
{
    return ((std::max(t, 0) + startSlot_) % 48) * 0.5;
}

qreal FluxRunner::sunElevation(qreal hour) const
{
    // up from sunrise to sunset, and a night branch of its own, so that a
    // winter midnight is as dark as it should be rather than reading as dawn
    const qreal day = sunset_ - sunrise_;
    if (hour >= sunrise_ && hour <= sunset_)
        return std::sin(PI * (hour - sunrise_) / day);
    const qreal sinceSunset = std::fmod(hour - sunset_ + 24.0, 24.0);
    return -std::sin(PI * sinceSunset / (24.0 - day));
}

qreal FluxRunner::hourAt(qreal x) const
{
    return hourOfSlot(columnOf(x) - LEAD);
}

qreal FluxRunner::nightAt(qreal x) const
{
    return std::clamp(0.4 - 3.0 * sunElevation(hourAt(x)), 0.0, 1.0);
}

int FluxRunner::doyAt(qreal x) const
{
    return wrapDoy(firstDoy_ + (std::max(columnOf(x) - LEAD, 0) + startSlot_) / 48);
}

int FluxRunner::zoneAt(qreal x) const
{
    const int c = columnOf(x);
    for (int i = 0; i < zones_.size(); ++i)
    {
        if (c >= zones_.at(i).start && c <= zones_.at(i).end)
            return i;
    }
    return -1;
}

bool FluxRunner::inThermal(qreal x) const
{
    const int c = columnOf(x);
    for (const auto& zone : zones_)
    {
        if (zone.thermal >= 0 && c >= zone.thermal && c < zone.thermal + THERMAL_WIDTH)
            return true;
    }
    return false;
}

bool FluxRunner::raining(qreal x) const
{
    const int c = columnOf(x);
    for (const auto& shower : showers_)
    {
        if (c >= shower.start && c <= shower.end)
            return true;
    }
    return false;
}

int FluxRunner::patchAt(qreal x) const
{
    const int c = columnOf(x);
    return (c >= 0 && c < columnCount()) ? bars_.at(c).patch : -1;
}

qreal FluxRunner::gust(int zone) const
{
    // surges and lulls around a mean wind, never a jitter
    const Zone& z = zones_.at(zone);
    const qreal t = clockTime_;
    const qreal swing = 0.55 * std::sin(0.9 * t + z.phase)
                        + 0.30 * std::sin(2.3 * t + 1.7 * z.phase)
                        + 0.15 * z.noise;
    return spec_.gustMax * z.strength * std::clamp(0.35 * z.direction + 0.65 * swing, -1.0, 1.0);
}

qreal FluxRunner::rhythmDensity(int t) const
{
    // how likely a hazard is kept at half-hour t
    switch (rhythm_)
    {
        case Waves:
            return 0.6 + 0.4 * std::sin(2.0 * PI * t / rhythmPeriod_ + rhythmPhase_);
        case Clusters:
            for (int centre : clusters_)
            {
                if (std::abs(t - centre) <= 12)
                    return 1.0;
            }
            return 0.3;
        default:
            return 1.0;
    }
}

qreal FluxRunner::rhythmLateness(int t) const
{
    // how likely a hazard at half-hour t takes the harder of its ranges
    const qreal p = t / static_cast<qreal>(std::max(dataSlots_, 1));
    switch (rhythm_)
    {
        case Waves:
            return rhythmDensity(t);
        case Clusters:
            return rhythmDensity(t) > 0.5 ? 0.8 : 0.2;
        case HardFirst:
            return 1.0 - p;
        default:
            return p;
    }
}

int FluxRunner::points(int base) const
{
    return qRound(base * spec_.pointsFactor);
}

bool FluxRunner::generateLevel(bool hazards)
{
    bars_.clear();
    zones_.clear();
    tubes_.clear();
    moving_.clear();
    showers_.clear();
    patches_.clear();
    gapEnds_.clear();

    const Site& site = sites().at(site_);
    const int data = dataSlots_;

    // half-hourly NEE through the site's season: daytime uptake under the
    // week's cloud, respiration that follows the time of year, noisier nights
    const qreal warmth = 0.5 + 0.5 * std::cos(2.0 * PI * (firstDoy_ - 200) / 365.0);
    const qreal respirationBase = site.respiration * (0.4 + 0.6 * (site.wetAndDry ? seasonFactor_ : warmth));
    const qreal cloudLow = weather_ == 0 ? 0.85 : (weather_ == 1 ? 0.4 : 0.3);
    const qreal cloudHigh = weather_ == 2 ? 0.6 : 1.0;
    const qreal cloudStep = weather_ == 0 ? 0.03 : (weather_ == 1 ? 0.06 : 0.08);
    const qreal offset = uniform(-15.0, 15.0);
    QList<qreal> flux;
    qreal cloud = uniform(cloudLow, cloudHigh);
    qreal uptake = uniform(0.6, 1.0) * site.uptake * seasonFactor_;
    for (int t = 0; t < data; ++t)
    {
        if ((t + startSlot_) % 48 == 0)
            uptake = uniform(0.6, 1.0) * site.uptake * seasonFactor_;
        const qreal hour = hourOfSlot(t);
        const int day = (t + startSlot_) / 48;
        cloud = std::clamp(cloud + uniform(-cloudStep, cloudStep), cloudLow, cloudHigh);
        const qreal shade = (day == frontDay_) ? 0.2 : cloud;
        const qreal daylight = std::max(0.0, sunElevation(hour));
        const qreal respiration = respirationBase + 1.5 * std::sin(2.0 * PI * (hour - 9.0) / 24.0);
        // Box-Muller
        const qreal u1 = std::max(uniform(0.0, 1.0), 1e-9);
        const qreal gauss = std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * PI * uniform(0.0, 1.0));
        const qreal noise = (daylight > 0.0 ? 1.5 : 2.5) * gauss;
        flux.append(respiration - uptake * daylight * shade + noise);
    }

    // heights: zero flux near 150 px, 4 px per unit, never steeper than one
    // step-up per half-hour so that ordinary ground can always be walked
    QList<qreal> height;
    for (int t = 0; t < data; ++t)
    {
        qreal h = std::clamp(150.0 + offset + 4.0 * flux.at(t), 32.0, 200.0);
        h = std::round(h / 4.0) * 4.0;
        if (t > 0)
            h = std::clamp(h, height.last() - SLOPE, height.last() + SLOPE);
        height.append(h);
    }

    const int n = LEAD + data + TAIL;
    for (int c = 0; c < n; ++c)
    {
        const int t = std::clamp(c - LEAD, 0, data - 1);
        bars_.append({ FLOOR - height.at(t), flux.at(t), false, false, false, -1, -1 });
    }

    checkpointColumn_ = LEAD + data / 2;
    towerColumn_ = LEAD + data + TOWER_OFFSET;
    hutColumn_ = towerColumn_ + HUT_OFFSET;

    // nothing in the first sixteen half-hours, around the backup station or
    // close to the tower
    QList<bool> reserved(n, false);
    for (int c = 0; c < LEAD + 16; ++c)
        reserved[c] = true;
    for (int c = checkpointColumn_ - 10; c <= checkpointColumn_ + 10; ++c)
        reserved[c] = true;
    for (int c = LEAD + data - 10; c < n; ++c)
        reserved[c] = true;

    auto isFree = [&](int from, int to)
    {
        if (from < 0 || to >= n)
            return false;
        for (int c = from; c <= to; ++c)
        {
            if (reserved.at(c))
                return false;
        }
        return true;
    };
    auto reserve = [&](int from, int to)
    {
        for (int c = std::max(from, 0); c <= std::min(to, n - 1); ++c)
            reserved[c] = true;
    };
    auto heightOf = [&](int c) { return FLOOR - bars_.at(c).top; };
    auto setHeight = [&](int c, qreal h) { bars_[c].top = FLOOR - h; };
    auto count = [&](qreal per100, qreal multiplier)
    {
        return std::max(qRound(per100 * data / 100.0 * multiplier), 0);
    };
    auto isNight = [&](int t) { return sunElevation(hourOfSlot(t)) < 0.0; };

    // turbulence: by day, from the morning on, on a share of the days (never
    // around the backup station), each with a strength of its own
    const int days = (data + startSlot_ + 47) / 48;
    const qreal morning = std::max(8.0, sunrise_ + 1.0);
    const qreal share = std::clamp(spec_.zoneShare * site.zones, 0.0, 1.0);
    QList<QPair<int, int>> candidates;
    for (int day = 0; day < days; ++day)
    {
        const int start = day * 48 - startSlot_ + qRound(morning * 2.0) + between(0, 2);
        const int length = std::min(between(18, 24), static_cast<int>(2.0 * (sunset_ - morning)));
        const int end = std::min(start + length, data - 12);
        if (start < 16 || end - start < 10)
            continue;
        if (LEAD + end >= checkpointColumn_ - 10 && LEAD + start <= checkpointColumn_ + 10)
            continue;
        candidates.append({ start, end });
    }
    for (int i = 0; i < candidates.size(); ++i)
    {
        const bool last = i + 1 == candidates.size() && zones_.isEmpty();
        if (!chance(share) && !last)
            continue;
        zones_.append({ LEAD + candidates.at(i).first, LEAD + candidates.at(i).second,
                        uniform(spec_.zoneStrengthMin, spec_.zoneStrengthMax),
                        uniform(0.0, 2.0 * PI), 0.0, chance(0.65) ? 1 : -1, -1, false });
    }
    auto inZone = [&](int c)
    {
        for (const auto& zone : zones_)
        {
            if (c >= zone.start && c <= zone.end)
                return true;
        }
        return false;
    };

    // sampling tubes first, or the hazards leave them no room: a pair of
    // raised half-hours in, another pair out further on
    const int tubeCount = between(spec_.tubesMin, spec_.tubesMax);
    for (int attempt = 0; attempt < 400 && tubes_.size() < tubeCount; ++attempt)
    {
        const int entry = LEAD + between(24, std::max(data - 80, 25));
        const int exit = entry + between(30, 60);
        if (!isFree(entry - 4, entry + 1 + 4) || !isFree(exit - 4, exit + 1 + 4))
            continue;
        // never past the backup station or the tower in one go
        if (entry < checkpointColumn_ && exit > checkpointColumn_ - 10)
            continue;
        if (exit > towerColumn_ - 12)
            continue;
        for (int column : { entry, exit })
        {
            const qreal h = heightOf(column - 1) + TUBE_HEIGHT;
            for (int c = column; c <= column + 1; ++c)
            {
                setHeight(c, h);
                bars_[c].tube = true;
            }
            reserve(column, column + 1);
        }
        tubes_.append({ entry, exit, std::round(uniform(0.5, 1.5) * 10.0) / 10.0 });
    }

    if (hazards)
    {
        // missing data: pits, mostly at night (u*-filtered), in the level's rhythm
        const int pits = count(spec_.pitsPer100, site.pits);
        int placed = 0;
        for (int attempt = 0; attempt < 1000 && placed < pits; ++attempt)
        {
            const int t = between(16, data - 12);
            if (!isNight(t) && chance(0.6))
                continue;
            if (!chance(rhythmDensity(t)))
                continue;
            const bool late = chance(rhythmLateness(t));
            int width = late ? between(spec_.latePitMin, spec_.latePitMax)
                             : between(spec_.earlyPitMin, spec_.earlyPitMax);
            width = std::min(width, spec_.maxPit);
            const int c0 = LEAD + t;
            if (inZone(c0) || inZone(c0 + width - 1))
                width = std::min(width, spec_.maxZonePit);
            if (!isFree(c0 - spec_.pad, c0 + width - 1 + spec_.pad))
                continue;
            // the landing may be at most a little higher than the take-off
            if (bars_.at(c0 - 1).top - bars_.at(c0 + width).top > spec_.maxLandingRise)
                continue;
            for (int c = c0; c < c0 + width; ++c)
                bars_[c].gap = true;
            reserve(c0, c0 + width - 1);
            gapEnds_.append(c0 + width);
            ++placed;
        }

        // spikes: walls of one or two half-hours, always a jumpable rise
        // above the half-hour you jump from
        const int walls = count(spec_.wallsPer100, site.walls);
        placed = 0;
        for (int attempt = 0; attempt < 1000 && placed < walls; ++attempt)
        {
            const int t = between(16, data - 12);
            if (!chance(rhythmDensity(t)))
                continue;
            const int c0 = LEAD + t;
            const int width = chance(spec_.doubleWallChance) ? 2 : 1;
            if (!isFree(c0 - spec_.pad, c0 + width - 1 + spec_.pad))
                continue;
            const bool late = chance(rhythmLateness(t));
            qreal rise = late ? uniform(spec_.lateRiseMin, spec_.lateRiseMax)
                              : uniform(spec_.earlyRiseMin, spec_.earlyRiseMax);
            rise = std::floor(std::min(rise, CAP_RISE) / 4.0) * 4.0;
            const qreal h = std::min(heightOf(c0 - 1) + rise, 300.0);
            for (int c = c0; c < c0 + width; ++c)
            {
                setHeight(c, h);
                bars_[c].spike = true;
            }
            reserve(c0, c0 + width - 1);
            ++placed;
        }
    }

    // non-stationary stretches last, in what room is left: they bob as one,
    // level with the half-hour before
    const int runs = std::max(qRound(between(spec_.movingMin, spec_.movingMax) * site.moving), 0);
    for (int attempt = 0; attempt < 300 && moving_.size() < runs; ++attempt)
    {
        const int c0 = LEAD + between(20, data - 20);
        const int length = between(3, 6);
        if (!isFree(c0 - 4, c0 + length - 1 + 4))
            continue;
        const qreal h = heightOf(c0 - 1);
        const int index = static_cast<int>(moving_.size());
        for (int c = c0; c < c0 + length; ++c)
        {
            setHeight(c, h);
            bars_[c].moving = index;
        }
        moving_.append({ c0, c0 + length - 1, spec_.movingAmplitude, spec_.movingPeriod,
                         uniform(0.0, 2.0 * PI) });
        reserve(c0, c0 + length - 1);
    }

    // ice on the tundra, and in the dry season fire in the savanna's grass:
    // on free ground only, a pad clear of every pit and wall, so that no
    // take-off is ever made from either; fire is never wider than a hop
    const bool icy = site_ == Tundra;
    const bool burning = site_ == Savanna && seasonIndex_ == DrySeason;
    if (hazards && (icy || burning))
    {
        const int wanted = count(spec_.dangersPer100, site.dangers);
        for (int attempt = 0; attempt < 400 && patches_.size() < wanted; ++attempt)
        {
            const int c0 = LEAD + between(20, data - 16);
            const int width = icy ? between(2, 6) : between(1, 2);
            if (!isFree(c0 - spec_.pad, c0 + width - 1 + spec_.pad))
                continue;
            const int index = static_cast<int>(patches_.size());
            for (int c = c0; c < c0 + width; ++c)
                bars_[c].patch = index;
            patches_.append({ c0, c0 + width - 1, burning, false });
            reserve(c0, c0 + width - 1);
        }
    }

    // showers, out of the turbulence zones: the site, the season and the
    // week's weather decide how many; a desert has a flash storm at most
    qreal showerScale = site.showers * (weather_ == 2 ? 1.3 : (weather_ == 0 ? 0.7 : 1.0));
    if (site.wetAndDry)
        showerScale *= 0.3 + 1.4 * seasonFactor_;
    if (modifiers_ & StormWeek)
        showerScale *= 2.0;
    int showerCount = std::min(qRound(between(spec_.showersMin, spec_.showersMax) * showerScale), 6);
    if (site_ == Desert)
        showerCount = chance(0.4) ? 1 : 0;
    if (frontDay_ >= 0 && site_ != Desert)
    {
        // the front's own shower, in its daytime
        const int start = frontDay_ * 48 - startSlot_ + 22;
        const int end = start + 16;
        if (start >= 20 && end < data - 12)
            showers_.append({ LEAD + start, LEAD + end, false });
    }
    for (int attempt = 0; attempt < 200 && showers_.size() < showerCount; ++attempt)
    {
        const int start = LEAD + between(20, std::max(data - 40, 21));
        const int end = start + between(15, 25);
        bool clear = true;
        for (int c = start; c <= end && clear; ++c)
            clear = !inZone(c);
        for (const auto& other : showers_)
        {
            if (start <= other.end + 10 && end >= other.start - 10)
                clear = false;
        }
        if (clear)
            showers_.append({ start, end, false });
    }

    // an updraft in the middle of each zone, where there is ground to stand on
    for (auto& zone : zones_)
    {
        const int middle = (zone.start + zone.end) / 2 - 1;
        bool solid = true;
        for (int c = middle - 2; c <= middle + THERMAL_WIDTH + 1; ++c)
            solid = solid && !bars_.at(c).gap;
        zone.thermal = solid ? middle : -1;
    }

    std::sort(gapEnds_.begin(), gapEnds_.end());
    return true;
}

bool FluxRunner::verifyLevel() const
{
    const int n = columnCount();

    // the highest and lowest a surface ever gets
    auto highest = [this](int c)
    {
        const Bar& bar = bars_.at(c);
        return bar.moving >= 0 ? bar.top - moving_.at(bar.moving).amplitude : bar.top;
    };
    auto lowest = [this](int c)
    {
        const Bar& bar = bars_.at(c);
        return bar.moving >= 0 ? bar.top + moving_.at(bar.moving).amplitude : bar.top;
    };
    // how far apart two surfaces can be; half-hours of one run move together
    auto spread = [&](int a, int b)
    {
        const Bar& first = bars_.at(a);
        const Bar& second = bars_.at(b);
        if (first.moving >= 0 && first.moving == second.moving)
            return std::abs(first.top - second.top);
        return std::max(std::abs(highest(a) - lowest(b)), std::abs(lowest(a) - highest(b)));
    };
    auto runUp = [&](int before)
    {
        // RUN_UP solid, level-enough half-hours ending at column before
        if (before - RUN_UP + 1 < 0)
            return false;
        for (int c = before - RUN_UP + 1; c <= before; ++c)
        {
            if (bars_.at(c).gap)
                return false;
            if (c > before - RUN_UP + 1 && spread(c - 1, c) > STEP_UP)
                return false;
        }
        return true;
    };
    auto zoned = [this](int c)
    {
        for (const auto& zone : zones_)
        {
            if (c >= zone.start && c <= zone.end)
                return true;
        }
        return false;
    };

    // the caps, whatever the level asked for, and the jump itself as the
    // final judge: no pit may need more than three quarters of its reach
    for (const auto& run : moving_)
    {
        if (run.amplitude > CAP_MOVING_AMPLITUDE || run.period < CAP_MOVING_PERIOD)
            return false;
    }
    for (int c = 1; c < n; ++c)
    {
        if (bars_.at(c).gap)
        {
            if (bars_.at(c - 1).gap)
                continue;
            int end = c;
            while (end < n && bars_.at(end).gap)
                ++end;
            const int width = end - c;
            bool inZone = false;
            for (int g = c; g < end; ++g)
                inZone = inZone || zoned(g);
            if (end >= n || width > (inZone ? CAP_ZONE_PIT : CAP_PIT))
                return false;
            const qreal landing = lowest(c - 1) - highest(end);
            if (landing > CAP_LANDING_RISE)
                return false;
            if (width * BAR_W > REACH_SHARE * reach(landing, inZone ? spec_.gustMax : 0.0))
                return false;
            if (!runUp(c - 1))
                return false;
            continue;
        }
        if (bars_.at(c - 1).gap)
            continue;
        const qreal rise = lowest(c - 1) - highest(c);
        if (bars_.at(c - 1).moving >= 0 && bars_.at(c - 1).moving == bars_.at(c).moving)
            continue;
        if (rise > STEP_UP && (rise > CAP_RISE || !runUp(c - 1)))
            return false;
    }
    return true;
}

void FluxRunner::buildSamples()
{
    // The raw trace through the half-hourly levels: joined smoothly from
    // one half-hour's centre to the next, with turbulence on top - ragged by
    // day, quiet at night apart from the odd burst. Spikes are what they are
    // in raw data, single-sample outliers standing out of the trace; tubes
    // stay flat for their pipe; the tower and the hut stand on level ground.
    samples_.clear();
    const int n = columnCount();
    auto gauss = []()
    {
        const qreal u1 = std::max(uniform(0.0, 1.0), 1e-9);
        return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * PI * uniform(0.0, 1.0));
    };
    auto smooth = [&](int c)
    {
        const Bar& bar = bars_.at(c);
        return !bar.gap && !bar.spike && !bar.tube;
    };

    qreal turbulence = 0.0;
    int burst = 0;
    for (int c = 0; c < n; ++c)
    {
        const Bar& bar = bars_.at(c);

        if (bar.spike)
        {
            if (c > 0 && bars_.at(c - 1).spike)
                continue;
            int end = c;
            while (end < n && bars_.at(end).spike)
                ++end;
            // the trace carries on from the ground before to the ground after;
            // the outliers stand out of it, a lone needle for one half-hour and
            // a jagged cluster for two, the tallest at the half-hour's level
            const qreal before = bars_.at(std::max(c - 1, 0)).top;
            const qreal after = bars_.at(std::min(end, n - 1)).top;
            const QList<QPair<int, qreal>> needles = (end - c == 1)
                ? QList<QPair<int, qreal>> { { 2, 1.0 } }
                : QList<QPair<int, qreal>> { { 3, 0.8 }, { 5, 1.0 }, { 8, 0.65 } };
            const int count = (end - c) * SAMPLES_PER_SLOT;
            for (int i = 0; i < count; ++i)
            {
                turbulence = std::clamp(0.55 * turbulence + TURBULENCE_DAY * gauss(),
                                        -TURBULENCE_MAX, TURBULENCE_MAX);
                qreal ground = before + (after - before) * (i + 0.5) / count + turbulence;
                if (!samples_.isEmpty() && !samples_.last().gap && !samples_.last().tube)
                    ground = std::clamp(ground, samples_.last().base - SAMPLE_STEP,
                                        samples_.last().base + SAMPLE_STEP);
                Sample sample { ground, ground, -1, false, false, false };
                for (const auto& needle : needles)
                {
                    if (needle.first == i)
                    {
                        sample.top = ground - needle.second * (ground - bar.top);
                        sample.spike = true;
                    }
                }
                samples_.append(sample);
            }
            continue;
        }

        const bool night = sunElevation(hourOfSlot(c - LEAD)) < 0.0;
        const bool level = bar.tube || c >= towerColumn_ - 3;
        for (int k = 0; k < SAMPLES_PER_SLOT; ++k)
        {
            Sample sample { bar.top, bar.top, bar.moving, bar.gap, false, bar.tube };
            if (bar.gap)
            {
                samples_.append(sample);
                continue;
            }

            // towards the neighbouring half-hour's centre, if it is ordinary ground
            const qreal f = (k + 0.5) / SAMPLES_PER_SLOT - 0.5;
            const int neighbour = f < 0.0 ? c - 1 : c + 1;
            if (!level && smooth(c) && neighbour >= 0 && neighbour < n && smooth(neighbour)
                && bars_.at(neighbour).moving == bar.moving)
                sample.top += (bars_.at(neighbour).top - bar.top) * std::abs(f);

            if (!level)
            {
                if (night && burst == 0 && chance(0.004))
                    burst = between(10, 20);
                const qreal sigma = burst > 0 ? TURBULENCE_BURST
                                              : (night ? TURBULENCE_NIGHT : TURBULENCE_DAY);
                burst = std::max(burst - 1, 0);
                turbulence = std::clamp(0.55 * turbulence + sigma * gauss(),
                                        -TURBULENCE_MAX, TURBULENCE_MAX);
                sample.top += turbulence;
                // never a step between two points that cannot be walked
                if (!samples_.isEmpty())
                {
                    const Sample& previous = samples_.last();
                    if (!previous.gap && !previous.tube && previous.moving == sample.moving)
                        sample.top = std::clamp(sample.top, previous.base - SAMPLE_STEP,
                                                previous.base + SAMPLE_STEP);
                }
            }
            sample.base = sample.top;
            samples_.append(sample);
        }
    }
}

void FluxRunner::placeThings()
{
    foes_.clear();
    blocks_.clear();
    coins_.clear();
    pickups_.clear();
    shots_.clear();

    const int n = columnCount();
    auto plain = [this](int c)
    {
        const Bar& bar = bars_.at(c);
        return !bar.gap && !bar.spike && !bar.tube && bar.moving < 0 && bar.patch < 0;
    };
    // a flat, plain stretch from c of the given length
    auto flat = [&](int c, int length)
    {
        if (c < 0 || c + length > n)
            return false;
        for (int i = c; i < c + length; ++i)
        {
            if (!plain(i) || std::abs(bars_.at(i).top - bars_.at(c).top) > 8.0)
                return false;
        }
        return true;
    };
    // no hazard starting within reach columns after c
    auto clearAhead = [&](int c, int reach)
    {
        for (int i = c; i <= std::min(c + reach, n - 1); ++i)
        {
            if (!plain(i))
                return false;
        }
        return true;
    };

    // good half-hours: arcs along the jump over every pit and wall, which
    // also show the way
    for (int c = 1; c < n - 1; ++c)
    {
        const Bar& bar = bars_.at(c);
        const bool pitStart = bar.gap && !bars_.at(c - 1).gap;
        const bool wallStart = bar.spike && !bars_.at(c - 1).spike;
        if (!pitStart && !wallStart)
            continue;
        int end = c;
        while (end < n && (pitStart ? bars_.at(end).gap : bars_.at(end).spike))
            ++end;
        const qreal base = bars_.at(c - 1).top - 36.0;
        const qreal x0 = (c - 1) * BAR_W;
        const qreal x1 = (end + 1) * BAR_W;
        const qreal apex = pitStart ? 80.0 : (bars_.at(c - 1).top - bar.top) + 50.0;
        for (int k = 1; k <= 5; ++k)
        {
            const qreal f = k / 6.0;
            coins_.append({ QPointF(x0 + f * (x1 - x0), base - 4.0 * apex * f * (1.0 - f)), false });
        }
    }

    // rows on the ground, and logger blocks above it
    const int data = dataSlots_;
    const Site& site = sites().at(site_);
    QList<int> blockColumns;
    for (int c = LEAD + 6; c < LEAD + data - 8; c += between(9, 15))
    {
        if (flat(c, 6) && clearAhead(c, 10) && chance(spec_.coinRowChance))
        {
            const int count = between(3, 5);
            for (int i = 0; i < count; ++i)
                coins_.append({ QPointF((c + i) * BAR_W + BAR_W / 2, bars_.at(c).top - 36.0), false });
        }
        else if (flat(c - 1, 6) && clearAhead(c, 6) && blockColumns.size() < spec_.maxBlocks
                 && chance(0.6))
        {
            const int group = between(1, 3);
            for (int i = 0; i < group; ++i)
                blockColumns.append(c + i);
        }
    }

    // what is in the blocks: a hat early and one after the backup station,
    // filters around 40 % and 75 %, coffee, as many as the level allows; the
    // rest hold half-hours
    QList<Item> items(blockColumns.size(), Item::Coin);
    auto nearestTo = [&](int column, Item item)
    {
        int best = -1;
        for (int i = 0; i < blockColumns.size(); ++i)
        {
            if (items.at(i) != Item::Coin)
                continue;
            if (best < 0 || std::abs(blockColumns.at(i) - column) < std::abs(blockColumns.at(best) - column))
                best = i;
        }
        if (best >= 0)
            items[best] = item;
    };
    nearestTo(LEAD + data / 8, Item::Hat);
    nearestTo(checkpointColumn_ + 24, Item::Hat);
    nearestTo(LEAD + data * 2 / 5, Item::Filter);
    for (int i = 1; i < spec_.filters; ++i)
        nearestTo(LEAD + data * 3 / 4, Item::Filter);
    for (int i = 0; i < spec_.coffees; ++i)
        nearestTo(LEAD + between(data / 5, data - 30), Item::Coffee);
    for (int i = 2; i < spec_.hats; ++i)
        nearestTo(LEAD + between(data / 4, data - 20), Item::Hat);
    blockCoins_ = 0;
    for (int i = 0; i < blockColumns.size(); ++i)
    {
        const int c = blockColumns.at(i);
        const bool many = items.at(i) == Item::Coin && chance(0.3);
        const int coins = items.at(i) == Item::Coin ? (many ? 5 : 1) : 0;
        blockCoins_ += coins;
        blocks_.append({ c, bars_.at(c).top - BLOCK_HEIGHT, items.at(i), std::max(coins, 1), 0.0, false });
    }

    auto farFromRespawn = [&](int c, int distance)
    {
        return std::abs(c - 3) > distance && std::abs(c - checkpointColumn_) > distance;
    };

    // Reviewer 2: on flat ground, clear of take-offs and respawn points
    const int gapMin = std::max(qRound(spec_.reviewerGapMin / site.reviewers), 4);
    const int gapMax = std::max(qRound(spec_.reviewerGapMax / site.reviewers), gapMin + 1);
    for (int c = LEAD + 20; c < LEAD + data - 6; c += between(gapMin, gapMax))
    {
        if (!flat(c - 2, 6) || !clearAhead(c - 4, 8) || !farFromRespawn(c, 6))
            continue;
        Foe foe { FoeKind::Reviewer,
                  { QPointF(c * BAR_W + BAR_W / 2, bars_.at(c).top), QPointF(), 11.0, 24.0, true },
                  {}, 0.0, 0.0, -1, true, false, false };
        foe.spawn = foe.body;
        foes_.append(foe);
    }

    // ebullition bubbles in a share of the pits
    const qreal bubbles = std::min(spec_.bubbleChance + site.bubbles, 1.0);
    for (int c = 1; c < n; ++c)
    {
        if (!bars_.at(c).gap || bars_.at(c - 1).gap || !chance(bubbles))
            continue;
        int end = c;
        while (end < n && bars_.at(end).gap)
            ++end;
        const qreal x = (c + end) * BAR_W / 2.0;
        Foe foe { FoeKind::Bubble,
                  { QPointF(x, FLOOR + 30.0), QPointF(), 9.0, 18.0, false },
                  {}, uniform(0.5, 2.5), 0.0, 1, true, false, false };
        foe.spawn = foe.body;
        foes_.append(foe);
    }

    // advection puffs, mostly at night
    const int puffCount = std::max(qRound(spec_.puffsPer100 * data / 100.0 * site.puffs), 0);
    int puffs = 0;
    for (int attempt = 0; attempt < 400 && puffs < puffCount; ++attempt)
    {
        const int c = LEAD + between(30, data - 10);
        const bool night = sunElevation(hourOfSlot(c - LEAD)) < 0.0;
        if ((!night && chance(0.75)) || !farFromRespawn(c, 10))
            continue;
        Foe foe { FoeKind::Puff,
                  { QPointF(c * BAR_W, bars_.at(c).top - 41.0), QPointF(), 13.0, 14.0, false },
                  {}, 0.0, 0.0, -1, true, false, false };
        foe.spawn = foe.body;
        foes_.append(foe);
        ++puffs;
    }

    // the site's own danger: a cone or a rock above plain ground, a cow, a
    // tractor or a dust devil on a stretch of its own, a swarm from dusk to
    // dawn; ice and fire are already part of the ground
    if (site_ == Tundra || site_ == Savanna)
        return;
    const int dangerCount = std::max(qRound(spec_.dangersPer100 * data / 100.0 * site.dangers), 0);
    auto add = [this](FoeKind kind, const QPointF& feet, qreal halfW, qreal height, bool grounded,
                      qreal timer)
    {
        Foe foe { kind, { feet, QPointF(), halfW, height, grounded }, {}, timer, 0.0, -1, true, false, false };
        foe.spawn = foe.body;
        foes_.append(foe);
    };
    QList<int> taken;
    for (int attempt = 0; attempt < 400 && taken.size() < dangerCount; ++attempt)
    {
        const int c = LEAD + between(24, data - 12);
        if (!farFromRespawn(c, 8))
            continue;
        bool crowded = false;
        for (int other : taken)
            crowded = crowded || std::abs(other - c) < 8;
        if (crowded)
            continue;
        const qreal x = c * BAR_W + BAR_W / 2;
        const qreal top = bars_.at(c).top;
        const qreal sun = sunElevation(hourOfSlot(c - LEAD));
        if (site_ == Forest || site_ == Mountain)
        {
            if (!flat(c - 2, 5))
                continue;
            const bool rock = site_ == Mountain;
            add(FoeKind::Faller, QPointF(x, top), rock ? 9.0 : 6.0, rock ? 18.0 : 14.0, false, FALL_WARNING_S);
        }
        else if (site_ == Wetland)
        {
            if (sun > 0.15)
                continue;
            for (int i = 0; i < 3; ++i)
                add(FoeKind::Mosquito, QPointF(x + 16.0 * i, top - 70.0 - 14.0 * i), 6.0, 10.0, false,
                    uniform(0.0, 2.0 * PI));
        }
        else
        {
            if (!flat(c - 2, 6) || !clearAhead(c - 4, 8) || (site_ == Desert && sun <= 0.0))
                continue;
            if (site_ == Grassland)
                add(FoeKind::Cow, QPointF(x, top), 16.0, 26.0, true, 0.0);
            else if (site_ == Cropland)
                add(FoeKind::Tractor, QPointF(x, top), 20.0, 30.0, true, 0.0);
            else
                add(FoeKind::DustDevil, QPointF(x, top), 10.0, 52.0, true, 0.0);
        }
        taken.append(c);
    }
}

////////////////////////////////////////////////////////////////////////////////
// FluxRunner: the run

namespace {

bool isGameKey(int key)
{
    switch (key)
    {
        case Qt::Key_Left: case Qt::Key_Right: case Qt::Key_Up: case Qt::Key_Down:
        case Qt::Key_A: case Qt::Key_D: case Qt::Key_W: case Qt::Key_S:
        case Qt::Key_Space: case Qt::Key_X: case Qt::Key_Shift:
        case Qt::Key_P: case Qt::Key_Escape:
            return true;
        default:
            return false;
    }
}

// Ctrl is never a game key: Ctrl+A runs the processing
bool commandModifiers(const QKeyEvent* event)
{
    return event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
}

} // namespace

void FluxRunner::tick()
{
    const qreal dt = std::min(clock_.restart() / 1000.0, 0.1);

    if (state_ != State::Paused && state_ != State::Idle)
    {
        for (int i = pops_.size() - 1; i >= 0; --i)
        {
            pops_[i].age += dt;
            if (pops_[i].age > POP_S)
                pops_.removeAt(i);
        }
        bannerAge_ += dt;

        // the sky follows the time of day under the camera, without jumps
        const qreal centre = cameraX_ + width() / 2.0;
        const qreal night = nightAt(centre);
        const qreal elevation = std::sin(PI * (hourAt(centre) - 5.5) / 15.0);
        QColor target = mix(DAY_SKY, NIGHT_SKY, night);
        target = mix(target, DAWN_SKY, 0.65 * std::exp(-std::pow(elevation / 0.15, 2)));
        sky_ = mix(sky_, target, std::min(1.0, dt / 0.6));
    }

    switch (state_)
    {
        case State::Card:
            stateTime_ += dt;
            if (stateTime_ >= cardDuration_)
            {
                state_ = State::Running;
                accumulator_ = 0.0;
            }
            break;

        case State::Running:
            accumulator_ += dt;
            while (accumulator_ >= STEP_S && state_ == State::Running)
            {
                step(STEP_S);
                accumulator_ -= STEP_S;
            }
            break;

        case State::Dying:
        {
            const qreal before = stateTime_;
            stateTime_ += dt;
            // a moment's stillness, then the hop off the screen; a pit
            // has already taken care of the leaving
            if (!diedInPit_)
            {
                if (before < 0.4 && stateTime_ >= 0.4)
                    player_.vel = QPointF(0.0, -600.0);
                if (stateTime_ >= 0.4)
                {
                    player_.vel.setY(std::min(player_.vel.y() + G_FALL * dt, FALL_MAX));
                    player_.feet.ry() += player_.vel.y() * dt;
                }
            }
            if (stateTime_ >= 1.6)
            {
                --lives_;
                ++livesLost_;
                if (lives_ <= 0)
                {
                    timedOut_ = diedOfDeadline_;
                    finishLevel(false);
                }
                else
                {
                    cardLine_ = pickFresh(diedOfDeadline_ ? deadlineDeathLines()
                                          : diedInPit_ ? pitDeathLines() : foeDeathLines());
                    // a missed deadline is extended, shorter
                    if (diedOfDeadline_)
                        deadline_ = std::max(60.0, 0.4 * levelDeadline_);
                    diedOfDeadline_ = false;
                    respawn();
                }
            }
            break;
        }

        case State::Piping:
        {
            stateTime_ += dt;
            const Tube& tube = tubes_.at(tubeIndex_);
            if (clearStage_ == 0)
            {
                // drawn in
                player_.feet.ry() += 140.0 * dt;
                if (stateTime_ >= 0.45)
                {
                    clearStage_ = 1;
                    stateTime_ = 0.0;
                    player_.feet = QPointF(tube.exitColumn * BAR_W + BAR_W,
                                           bars_.at(tube.exitColumn).top + PLAYER_HEIGHT);
                    const qreal view = width();
                    cameraX_ = std::clamp(std::max(cameraX_, player_.feet.x() - 0.45 * view), 0.0,
                                          std::max(columnCount() * BAR_W - view, 0.0));
                    addPop(QPointF(player_.feet.x(), bars_.at(tube.exitColumn).top - 20.0),
                           pickFresh(tubePops()).arg(tube.lag, 0, 'f', 1), ICE);
                }
            }
            else if (clearStage_ == 1)
            {
                // the time lag of this tube
                if (stateTime_ >= tube.lag)
                {
                    clearStage_ = 2;
                    stateTime_ = 0.0;
                }
            }
            else
            {
                player_.feet.ry() -= 140.0 * dt;
                const qreal top = bars_.at(tube.exitColumn).top;
                if (player_.feet.y() <= top)
                {
                    player_.feet.setY(top);
                    player_.vel = QPointF();
                    player_.grounded = true;
                    clearStage_ = 0;
                    state_ = State::Running;
                    accumulator_ = 0.0;
                    // pits skipped through the tube were not jumped
                    while (nextGap_ < gapEnds_.size()
                           && gapEnds_.at(nextGap_) * BAR_W <= player_.feet.x())
                        ++nextGap_;
                }
            }
            break;
        }

        case State::Cleared:
        {
            stateTime_ += dt;
            const qreal ground = bars_.at(towerColumn_).top;
            if (clearStage_ == 0)
            {
                // down the mast, with the flag
                flagY_ = std::min(flagY_ + 300.0 * dt, ground - 30.0);
                player_.feet.setY(std::min(player_.feet.y() + 300.0 * dt, ground));
                if (player_.feet.y() >= ground && flagY_ >= ground - 30.0)
                {
                    clearStage_ = 1;
                    stateTime_ = 0.0;
                }
            }
            else if (clearStage_ == 1)
            {
                // and into the logger hut
                facingLeft_ = false;
                player_.feet.rx() += 120.0 * dt;
                runDistance_ += 120.0 * dt;
                player_.feet.setY(groundTop(player_.feet.x()));
                const qreal view = width();
                cameraX_ = std::clamp(std::max(cameraX_, player_.feet.x() - 0.45 * view), 0.0,
                                      std::max(columnCount() * BAR_W - view, 0.0));
                if (player_.feet.x() >= hutColumn_ * BAR_W + BAR_W / 2)
                {
                    clearStage_ = 2;
                    stateTime_ = 0.0;
                    addPop(QPointF(player_.feet.x(), player_.feet.y() - 70.0),
                           pickFresh(flagPops()), COIN_GREEN);
                }
            }
            else if (stateTime_ >= 0.8)
            {
                finishLevel(true);
            }
            break;
        }

        case State::Over:
            stateTime_ += dt;
            if (stateTime_ >= OVER_S)
            {
                timer_->stop();
                state_ = State::Idle;
                held_ = 0;
                if (cleared_)
                    emit levelCleared(result());
                else
                    emit runOver(result());
                return;
            }
            break;

        case State::Idle:
        case State::Paused:
            break;
    }

    update();
}

void FluxRunner::step(qreal dt)
{
    runTime_ += dt;
    clockTime_ += dt;
    deadline_ -= dt;
    invulnerable_ = std::max(invulnerable_ - dt, 0.0);
    coffee_ = std::max(coffee_ - dt, 0.0);
    fireCooldown_ = std::max(fireCooldown_ - dt, 0.0);
    for (auto& block : blocks_)
        block.bump = std::max(block.bump - 40.0 * dt, 0.0);

    updateAtmosphere(dt);

    const qreal previousFeet = player_.feet.y();
    updatePlayer(dt);
    if (state_ != State::Running)
        return;

    // the camera only goes forward, as in the original
    const qreal view = width();
    cameraX_ = std::clamp(std::max(cameraX_, player_.feet.x() - 0.45 * view), 0.0,
                          std::max(columnCount() * BAR_W - view, 0.0));

    updateFoes(dt);
    updatePickups(dt);
    updateShots(dt);
    collectCoins();
    touchFoes(previousFeet);
    if (state_ != State::Running)
        return;

    const qreal x = player_.feet.x();

    while (nextGap_ < gapEnds_.size() && x - player_.halfW > gapEnds_.at(nextGap_) * BAR_W)
    {
        ++nextGap_;
        ++gapsBridged_;
    }

    if (!checkpointTaken_ && x > checkpointColumn_ * BAR_W + BAR_W / 2)
    {
        checkpointTaken_ = true;
        respawnColumn_ = checkpointColumn_;
        addPop(QPointF(checkpointColumn_ * BAR_W + BAR_W / 2,
                       groundTop(checkpointColumn_ * BAR_W + BAR_W / 2) - 70.0),
               pickFresh(checkpointPops()), COIN_GREEN);
    }

    const int zone = zoneAt(x);
    if (zone >= 0 && !zones_.at(zone).announced)
    {
        zones_[zone].announced = true;
        announce(pickFresh(turbulenceBanners()), AMBER);
    }
    for (auto& shower : showers_)
    {
        const int c = columnOf(x);
        if (!shower.announced && c >= shower.start && c <= shower.end)
        {
            shower.announced = true;
            addPop(QPointF(x, player_.feet.y() - 80.0),
                   pickFresh(snowy_ ? snowPops() : rainPops()), ICE);
        }
    }

    // the site's own ground: named as it comes into view, ice to slide on,
    // fire to keep out of
    for (int i = 0; i < patches_.size() && !dangerAnnounced_; ++i)
    {
        if (patches_.at(i).start * BAR_W < cameraX_ + 0.8 * width())
            noticeDanger();
    }
    const int patch = patchAt(x);
    if (patch >= 0 && player_.grounded)
    {
        Patch& here = patches_[patch];
        if (!here.fire && !here.announced)
        {
            here.announced = true;
            addPop(QPointF(x, player_.feet.y() - 80.0), pickFresh(icePops()), ICE);
        }
        if (here.fire && invulnerable_ <= 0.0 && coffee_ <= 0.0)
        {
            hurt(pickFresh(firePops()));
            if (state_ != State::Running)
                return;
        }
    }
    const qreal night = nightAt(x);
    if (night > 0.8 && !nightAnnounced_ && runTime_ > 2.0)
    {
        nightAnnounced_ = true;
        announce(pickFresh(nightLines()), ICE);
    }
    else if (night < 0.2)
    {
        nightAnnounced_ = false;
    }

    if (x + player_.halfW >= towerColumn_ * BAR_W + BAR_W / 2 - 2.0)
    {
        reachTower();
        return;
    }
    if (player_.feet.y() > FLOOR + DEATH_DEPTH)
    {
        die(true);
        return;
    }
    if (deadline_ <= 0.0)
    {
        // the deadline costs a life rather than the run, as in the original
        deadline_ = 0.0;
        diedOfDeadline_ = true;
        die(false);
    }
}

FluxRunner::Move FluxRunner::moveBody(Body &body, qreal dt, qreal stepUp)
{
    Move move { false, false, -1 };

    // across first: walk the columns the leading edge enters this step
    const qreal vx = body.vel.x();
    if (vx != 0.0)
    {
        const int dir = vx > 0.0 ? 1 : -1;
        const qreal x = body.feet.x();
        const qreal nx = x + vx * dt;
        const int from = sampleOf(x + dir * body.halfW - dir * EPS);
        const int to = sampleOf(nx + dir * body.halfW - dir * EPS);
        qreal resolved = nx;
        qreal feet = body.feet.y();
        for (int c = from + dir; dir > 0 ? c <= to : c >= to; c += dir)
        {
            // the trace is solid down to the floor; blocks only where they are
            qreal obstacle = surfaceAt(c);
            const qreal sampleLeft = c * SAMPLE_W;
            for (const auto& block : blocks_)
            {
                const qreal x0 = block.column * BAR_W;
                if (sampleLeft + SAMPLE_W <= x0 + EPS || sampleLeft >= x0 + BLOCK - EPS)
                    continue;
                const qreal top = block.bottom - BLOCK;
                if (top < feet - EPS && block.bottom > feet - body.height + EPS)
                    obstacle = std::min(obstacle, top);
            }
            if (obstacle < feet - stepUp - EPS)
            {
                resolved = dir > 0 ? c * SAMPLE_W - body.halfW - EPS
                                   : (c + 1) * SAMPLE_W + body.halfW + EPS;
                move.hitWall = true;
                break;
            }
            if (body.grounded && obstacle < feet)
                feet = obstacle;    // a step up, walked rather than jumped
        }
        body.feet = QPointF(resolved, feet);
        if (move.hitWall)
            body.vel.setX(0.0);
    }

    // then down or up
    const qreal left = body.feet.x() - body.halfW;
    const qreal right = body.feet.x() + body.halfW;
    const qreal feet = body.feet.y();
    const qreal next = feet + body.vel.y() * dt;
    if (body.vel.y() >= 0.0)
    {
        // a sweep from where the feet were, so nothing falls through a top
        qreal land = INF;
        for (int c = sampleOf(left + EPS); c <= sampleOf(right - EPS); ++c)
        {
            const qreal top = surfaceAt(c);
            if (feet <= top + 0.5 && next >= top)
                land = std::min(land, top);
        }
        for (const auto& block : blocks_)
        {
            const qreal x0 = block.column * BAR_W;
            if (right <= x0 + EPS || left >= x0 + BLOCK - EPS)
                continue;
            const qreal top = block.bottom - BLOCK;
            if (feet <= top + 0.5 && next >= top)
                land = std::min(land, top);
        }

        if (land < INF)
        {
            body.feet.setY(land);
            body.vel.setY(0.0);
            move.landed = !body.grounded;
            body.grounded = true;
        }
        else if (body.grounded)
        {
            // follow the ground down a gentle slope, or up a rising
            // platform, or leave it
            const qreal support = supportTop(left, right, feet);
            if (support < INF && support - feet <= SNAP_DOWN)
            {
                body.feet.setY(support);
                body.vel.setY(0.0);
            }
            else
            {
                body.grounded = false;
                body.feet.setY(next);
            }
        }
        else
        {
            body.feet.setY(next);
        }
    }
    else
    {
        // rising: only blocks have ceilings
        body.grounded = false;
        qreal resolved = next;
        const qreal head = feet - body.height;
        const qreal nextHead = next - body.height;
        for (int i = 0; i < blocks_.size(); ++i)
        {
            const auto& block = blocks_.at(i);
            const qreal x0 = block.column * BAR_W;
            const qreal x1 = x0 + BLOCK;
            const qreal overlap = std::min(right, x1) - std::max(left, x0);
            if (overlap <= 0.0 || head < block.bottom - 0.5 || nextHead >= block.bottom)
                continue;
            if (overlap <= 6.0)
            {
                // a corner: slip past it, as in the original
                body.feet.setX(body.feet.x() < (x0 + x1) / 2 ? x0 - body.halfW - EPS
                                                             : x1 + body.halfW + EPS);
                continue;
            }
            resolved = block.bottom + body.height;
            body.vel.setY(0.0);
            move.bumpedBlock = i;
            break;
        }
        body.feet.setY(resolved);
    }

    return move;
}

void FluxRunner::updatePlayer(qreal dt)
{
    Body& b = player_;
    const int dir = ((held_ & KeyRight) ? 1 : 0) - ((held_ & KeyLeft) ? 1 : 0);
    crouching_ = (held_ & KeyDown) && b.grounded;
    b.height = crouching_ ? CROUCH_HEIGHT : PLAYER_HEIGHT;

    // wet bars: less grip to start, turn and stop; ice, still less
    const int patch = b.grounded ? patchAt(b.feet.x()) : -1;
    const bool icy = patch >= 0 && !patches_.at(patch).fire;
    const bool wet = icy || (b.grounded && raining(b.feet.x()));
    const qreal grip = icy ? ICE_GRIP : (wet ? RAIN_GRIP : 1.0);
    const qreal maxRun = MAX_RUN * (coffee_ > 0.0 ? 1.2 : 1.0);

    qreal vx = b.vel.x();
    if (dir != 0 && !crouching_)
    {
        facingLeft_ = dir < 0;
        qreal accel = AIR_ACCEL;
        if (b.grounded)
            accel = (dir * vx < 0.0) ? TURN_DECEL * grip : GROUND_ACCEL * (wet ? 0.6 : 1.0);
        vx = std::clamp(vx + dir * accel * dt, -maxRun, maxRun);
    }
    else
    {
        const qreal decel = (b.grounded ? GROUND_FRICTION * grip : AIR_DRAG) * dt;
        vx = (std::abs(vx) <= decel) ? 0.0 : vx - std::copysign(decel, vx);
    }

    qreal vy = b.vel.y();
    // a buffered press jumps the moment it can, including just after an edge
    if (jumpBuffer_ > 0.0 && (b.grounded || coyote_ > 0.0))
    {
        vy = -JUMP_V;
        b.grounded = false;
        jumpBuffer_ = 0.0;
        coyote_ = 0.0;
        crouching_ = false;
        b.height = PLAYER_HEIGHT;
    }

    const bool jumpHeld = held_ & KeyJump;
    const bool lifted = !b.grounded && jumpHeld && inThermal(b.feet.x()) && b.feet.y() > 90.0;
    if (lifted)
    {
        vy = std::max(vy - THERMAL_LIFT * dt, -THERMAL_MAX_RISE);
    }
    else
    {
        if (vy < 0.0 && !jumpHeld)
            vy = std::max(vy, -JUMP_CUT_V);   // let go early, jump lower
        const qreal gravity = (vy < 0.0 && jumpHeld) ? G_RISE : G_FALL;
        vy = std::min(vy + gravity * dt, FALL_MAX);
    }

    // eddies: a kick along the swirl where the player meets it - up on its
    // rising side, down on its sinking one, with the flow across its top and
    // against it across its bottom; a shove, never a hurt
    for (auto& eddy : eddies_)
    {
        if (eddy.cooldown > 0.0)
            continue;
        const QPointF centre(b.feet.x(), b.feet.y() - b.height / 2);
        const QPointF d = centre - eddy.pos;
        const qreal distance = std::hypot(d.x(), d.y());
        if (distance > eddy.radius + 10.0 || distance < 1.0)
            continue;
        QPointF kick(-d.y() / distance * eddy.spin, d.x() / distance * eddy.spin);
        kick *= spec_.eddyKick * (0.6 + 0.4 * zones_.at(eddy.zone).strength);
        // over missing data an eddy only lifts: never down into it, and never
        // sideways out of a jump across it
        if (nearGap(columnOf(b.feet.x()), 2))
        {
            kick.setX(0.0);
            kick.setY(std::min(kick.y(), 0.0));
        }
        // the whole kick sideways, enough to throw a runner back against
        // the run for a moment rather than only slow them
        vx = std::clamp(vx + kick.x(), -1.6 * maxRun, 1.6 * maxRun);
        vy = std::max(vy + kick.y(), -JUMP_V);
        if (kick.y() < -150.0)
        {
            b.grounded = false;
            addPop(centre, pickFresh(eddyPops()), ICE);
        }
        eddy.cooldown = 0.6;
    }

    // the gust carries whatever is in the zone, on top of its own speed
    const int zone = zoneAt(b.feet.x());
    const qreal drift = zone >= 0 ? gust(zone) : 0.0;
    b.vel = QPointF(vx + drift, vy);
    const Move move = moveBody(b, dt, b.grounded ? STEP_UP : AIR_STEP_UP);
    b.vel.setX(move.hitWall ? 0.0 : b.vel.x() - drift);

    if (b.grounded)
    {
        coyote_ = COYOTE_S;
        if (move.landed)
            stompChain_ = 0;
        runDistance_ += std::abs(b.vel.x()) * dt;
    }
    else
    {
        coyote_ = std::max(coyote_ - dt, 0.0);
    }
    jumpBuffer_ = std::max(jumpBuffer_ - dt, 0.0);

    if (move.bumpedBlock >= 0)
        bumpBlock(move.bumpedBlock);

    // nobody goes back past the camera, or off the end
    const qreal leftEdge = cameraX_ + b.halfW;
    if (b.feet.x() < leftEdge)
    {
        b.feet.setX(leftEdge);
        b.vel.setX(std::max(b.vel.x(), 0.0));
    }
    b.feet.setX(std::min(b.feet.x(), columnCount() * BAR_W - b.halfW));
}

void FluxRunner::updateAtmosphere(qreal dt)
{
    for (auto& zone : zones_)
        zone.noise = std::clamp(0.97 * zone.noise + uniform(-0.15, 0.15), -1.0, 1.0);

    // the eddies ride the mean wind and wander, and leave their zone
    for (int i = eddies_.size() - 1; i >= 0; --i)
    {
        auto& eddy = eddies_[i];
        eddy.age += dt;
        eddy.cooldown = std::max(eddy.cooldown - dt, 0.0);
        const Zone& zone = zones_.at(eddy.zone);
        eddy.pos.rx() += (zone.direction * 45.0 + 0.6 * gust(eddy.zone)) * dt;
        eddy.pos.ry() += 12.0 * std::sin(eddy.age * 1.7) * dt;
        const int c = columnOf(eddy.pos.x());
        if (eddy.age > 7.0 || c < zone.start - 2 || c > zone.end + 2)
            eddies_.removeAt(i);
    }

    nextEddy_ -= dt;
    if (nextEddy_ > 0.0 || eddies_.size() >= spec_.maxEddies)
        return;
    nextEddy_ = uniform(spec_.eddyIntervalMin, spec_.eddyIntervalMax);
    const qreal viewLeft = cameraX_;
    const qreal viewRight = cameraX_ + width();
    for (int z = 0; z < zones_.size(); ++z)
    {
        const qreal from = std::max(zones_.at(z).start * BAR_W, viewLeft);
        const qreal to = std::min((zones_.at(z).end + 1) * BAR_W, viewRight + 200.0);
        if (to - from < 60.0)
            continue;
        // born upwind, and turning as the shear turns it: the wind is
        // stronger aloft, so its top moves downwind and its bottom upwind
        const qreal x = zones_.at(z).direction > 0 ? from + uniform(0.0, 80.0)
                                                   : to - uniform(0.0, 80.0);
        qreal ground = groundTop(x);
        if (!std::isfinite(ground))
            ground = FLOOR - 150.0;
        eddies_.append({ QPointF(x, ground - uniform(50.0, 130.0)), uniform(15.0, 35.0),
                         zones_.at(z).direction, 0.0, 0.0, z });
        break;
    }
}

void FluxRunner::updateFoes(qreal dt)
{
    const qreal viewRight = cameraX_ + width();
    for (auto& foe : foes_)
    {
        if (!foe.alive)
            continue;
        foe.squash = std::max(foe.squash - dt, 0.0);
        if (foe.defeated)
        {
            // the flattened sheet lingers a moment
            if (foe.squash <= 0.0)
                foe.alive = false;
            continue;
        }
        if (!foe.active)
        {
            if (foe.kind == FoeKind::Faller)
            {
                // it lets go while the player is far enough back to arrive
                // under it as it lands, or to stop short of its shadow
                const qreal ahead = foe.spawn.feet.x() - player_.feet.x();
                if (ahead <= 0.0 || ahead > std::min(FALL_WAKE, 0.5 * width()))
                    continue;
                foe.body.feet.setY(-8.0);
                foe.body.vel = QPointF();
                foe.timer = FALL_WARNING_S;
            }
            else if (foe.spawn.feet.x() - foe.spawn.halfW > viewRight + 48.0)
            {
                continue;
            }
            foe.active = true;
            if (foe.kind != FoeKind::Reviewer && foe.kind != FoeKind::Bubble && foe.kind != FoeKind::Puff)
                noticeDanger();
            if (foe.kind == FoeKind::Tractor)
                addPop(QPointF(std::min(foe.body.feet.x(), viewRight - 40.0), foe.body.feet.y() - 44.0),
                       QStringLiteral("HONK!"), AMBER);
        }

        Body& b = foe.body;
        switch (foe.kind)
        {
            case FoeKind::Reviewer:
            case FoeKind::Cow:
            case FoeKind::Tractor:
            case FoeKind::DustDevil:
            {
                const qreal speed = foe.kind == FoeKind::Cow       ? COW_SPEED
                                  : foe.kind == FoeKind::Tractor   ? std::min(1.7 * spec_.reviewerSpeed, CAP_TRACTOR_SPEED)
                                  : foe.kind == FoeKind::DustDevil ? std::min(0.85 * spec_.reviewerSpeed, CAP_DUST_DEVIL_SPEED)
                                                                   : spec_.reviewerSpeed;
                // a dust devil wanders as it pleases
                if (foe.kind == FoeKind::DustDevil && chance(0.4 * dt))
                    foe.facing = -foe.facing;
                b.vel.setX(foe.facing * speed);
                b.vel.setY(std::min(b.vel.y() + G_FALL * dt, FALL_MAX));
                const Move move = moveBody(b, dt, STEP_UP);
                if (move.hitWall)
                {
                    foe.facing = -foe.facing;
                }
                else if (b.grounded)
                {
                    // it keeps to its own stretch: back at a pit or a drop
                    const qreal ahead = b.feet.x() + foe.facing * (b.halfW + 2.0);
                    if (groundTop(ahead) > b.feet.y() + STEP_UP)
                        foe.facing = -foe.facing;
                }
                if (b.feet.y() > FLOOR + DEATH_DEPTH)
                    foe.alive = false;
                break;
            }
            case FoeKind::Bubble:
            {
                const qreal rest = foe.spawn.feet.y();
                if (b.feet.y() >= rest && b.vel.y() >= 0.0)
                {
                    b.feet.setY(rest);
                    b.vel.setY(0.0);
                    foe.timer -= dt;
                    if (foe.timer <= 0.0)
                    {
                        // a leap to between 60 and 120 px above the rim
                        int rim = columnOf(b.feet.x());
                        while (rim > 0 && bars_.at(rim).gap)
                            --rim;
                        const qreal apex = bars_.at(rim).top - uniform(60.0, 120.0);
                        b.vel.setY(-std::sqrt(2.0 * BUBBLE_G * std::max(rest - apex, 10.0)));
                        foe.timer = uniform(spec_.bubblePauseMin, spec_.bubblePauseMax);
                    }
                }
                else
                {
                    b.vel.setY(b.vel.y() + BUBBLE_G * dt);
                    b.feet.ry() += b.vel.y() * dt;
                }
                break;
            }
            case FoeKind::Puff:
            {
                b.feet.rx() -= spec_.puffSpeed * dt;
                // at head height over the ground; over a pit it holds its height
                const qreal ground = groundTop(b.feet.x());
                if (std::isfinite(ground))
                    b.feet.ry() += (ground - 41.0 - b.feet.y()) * std::min(1.0, 3.0 * dt);
                break;
            }
            case FoeKind::Mosquito:
            {
                // after the player, never faster than half a run, each of the
                // swarm wobbling round a point of its own
                foe.timer += dt;
                const qreal own = foe.spawn.feet.x();
                const QPointF target(player_.feet.x() + 14.0 * std::sin(2.0 * foe.timer + own),
                                     player_.feet.y() - 0.6 * player_.height + 10.0 * std::cos(2.6 * foe.timer + own));
                const QPointF to = target - QPointF(b.feet.x(), b.feet.y() - b.height / 2);
                const qreal distance = std::max(std::hypot(to.x(), to.y()), 1.0);
                const qreal speed = std::min(1.6 * spec_.reviewerSpeed, CAP_SWARM_SPEED);
                b.feet += to / distance * speed * dt + QPointF(0.0, 50.0 * std::cos(9.0 * foe.timer) * dt);
                b.feet.setY(std::clamp(b.feet.y(), 30.0, FLOOR));
                foe.facing = to.x() < 0.0 ? -1 : 1;
                break;
            }
            case FoeKind::Faller:
            {
                // the shadow first, then the fall
                if (foe.timer > 0.0)
                {
                    foe.timer -= dt;
                    break;
                }
                b.vel.setY(std::min(b.vel.y() + G_FALL * dt, FALL_MAX));
                b.feet.ry() += b.vel.y() * dt;
                const qreal ground = groundTop(b.feet.x());
                if (std::isfinite(ground) && b.feet.y() >= ground)
                {
                    // landed: a cone breaks up, a rock lies a moment
                    b.feet.setY(ground);
                    foe.defeated = true;
                    foe.squash = site_ == Mountain ? 1.0 : 0.3;
                    if (std::abs(b.feet.x() - player_.feet.x()) < 4.0 * BAR_W)
                        addPop(QPointF(b.feet.x(), ground - 30.0), pickFresh(fallPops()), AMBER);
                }
                else if (b.feet.y() > FLOOR + DEATH_DEPTH)
                {
                    foe.alive = false;
                }
                break;
            }
        }

        if (b.feet.x() < cameraX_ - 64.0)
            foe.alive = false;      // behind the camera, which never goes back
    }
}

void FluxRunner::updatePickups(qreal dt)
{
    const QRectF me(player_.feet.x() - player_.halfW, player_.feet.y() - player_.height,
                    2 * player_.halfW, player_.height);
    for (int i = pickups_.size() - 1; i >= 0; --i)
    {
        auto& pickup = pickups_[i];
        Body& b = pickup.body;
        if (pickup.rise < 18.0)
        {
            // out of the block first
            const qreal d = std::min(36.0 * dt, 18.0 - pickup.rise);
            pickup.rise += d;
            b.feet.ry() -= d;
            b.grounded = true;
        }
        else if (pickup.kind == Item::Hat || pickup.kind == Item::Coffee)
        {
            b.vel.setX(pickup.facing * (pickup.kind == Item::Hat ? HAT_SPEED : COFFEE_SPEED));
            b.vel.setY(std::min(b.vel.y() + G_FALL * dt, FALL_MAX));
            const Move move = moveBody(b, dt, STEP_UP);
            if (move.hitWall)
                pickup.facing = -pickup.facing;
            if (pickup.kind == Item::Coffee && b.grounded)
            {
                b.vel.setY(-420.0);
                b.grounded = false;
            }
        }

        if (b.feet.y() > FLOOR + DEATH_DEPTH || b.feet.x() < cameraX_ - 64.0)
        {
            pickups_.removeAt(i);
            continue;
        }

        const QRectF it(b.feet.x() - b.halfW, b.feet.y() - b.height, 2 * b.halfW, b.height);
        if (!me.intersects(it))
            continue;

        const QPointF where(b.feet.x(), b.feet.y() - 30.0);
        switch (pickup.kind)
        {
            case Item::Hat:
                if (power_ == Power::Small)
                    power_ = Power::Hat;
                addPop(where, pickFresh(hatPops()), HAT_YELLOW);
                break;
            case Item::Filter:
                power_ = Power::Filter;
                addPop(where, pickFresh(filterPops()), FILTER_ORANGE);
                break;
            case Item::Coffee:
                coffee_ = COFFEE_S;
                addPop(where, pickFresh(coffeePops()), AMBER);
                break;
            case Item::Coin:
                break;
        }
        score_ += points(POWER_POINTS);
        pickups_.removeAt(i);
    }
}

void FluxRunner::updateShots(qreal dt)
{
    for (int i = shots_.size() - 1; i >= 0; --i)
    {
        auto& shot = shots_[i];
        shot.age += dt;
        Body& b = shot.body;
        b.vel.setY(std::min(b.vel.y() + SHOT_G * dt, FALL_MAX));
        const Move move = moveBody(b, dt, AIR_STEP_UP);
        if (b.grounded)
        {
            b.vel.setY(-300.0);     // it bounces along the bar tops
            b.grounded = false;
        }
        bool gone = move.hitWall || shot.age > 1.5 || b.feet.y() > FLOOR + 40.0;

        const QRectF pulse(b.feet.x() - b.halfW, b.feet.y() - b.height, 2 * b.halfW, b.height);
        for (auto& foe : foes_)
        {
            if (!foe.alive || !foe.active || foe.defeated)
                continue;
            const Body& f = foe.body;
            // a falling cone or rock is too quick to hit
            if (f.feet.y() - f.height > FLOOR || foe.kind == FoeKind::Faller)
                continue;
            if (pulse.intersects(QRectF(f.feet.x() - f.halfW, f.feet.y() - f.height,
                                        2 * f.halfW, f.height)))
            {
                // a cow or a tractor shrugs a filter off
                if (foe.kind != FoeKind::Cow && foe.kind != FoeKind::Tractor)
                    defeat(foe, KILL_POINTS, pickFresh(despikePops()));
                gone = true;
                break;
            }
        }
        if (gone)
            shots_.removeAt(i);
    }
}

void FluxRunner::collectCoins()
{
    const Body& p = player_;
    for (auto& coin : coins_)
    {
        if (coin.taken)
            continue;
        if (std::abs(coin.pos.x() - p.feet.x()) <= p.halfW + 7.0
            && coin.pos.y() >= p.feet.y() - p.height - 7.0
            && coin.pos.y() <= p.feet.y() + 7.0)
        {
            coin.taken = true;
            gainCoin(coin.pos);
        }
    }
}

void FluxRunner::gainCoin(const QPointF &where)
{
    ++coinCount_;
    score_ += points(COIN_POINTS);
    // counted over the whole run; no line for every one, that would be noise
    const int total = runCoins_ + coinCount_;
    if (total % 100 == 0)
    {
        ++lives_;
        addPop(where, pickFresh(oneUpPops()), COIN_GREEN);
    }
    else if (total % 10 == 0)
    {
        addPop(where, pickFresh(streakPops()).arg(total), COIN_GREEN);
    }
}

void FluxRunner::touchFoes(qreal previousFeet)
{
    static const int CHAIN[] = { 100, 200, 400, 800, 1000 };

    for (auto& foe : foes_)
    {
        if (!foe.alive || !foe.active || foe.defeated)
            continue;
        // a bubble waiting down in its pit is no stepping stone out of it
        if (foe.body.feet.y() - foe.body.height > FLOOR)
            continue;
        // a cone or a rock still up in the canopy is only a shadow so far
        if (foe.kind == FoeKind::Faller && foe.timer > 0.0)
            continue;
        const QRectF me(player_.feet.x() - player_.halfW, player_.feet.y() - player_.height,
                        2 * player_.halfW, player_.height);
        const Body& b = foe.body;
        const QRectF it(b.feet.x() - b.halfW, b.feet.y() - b.height, 2 * b.halfW, b.height);
        if (!me.intersects(it))
            continue;

        if (coffee_ > 0.0)
        {
            defeat(foe, KILL_POINTS, pickFresh(despikePops()));
            continue;
        }

        // from above, falling: a stomp, and each one before landing is worth
        // more; a cow, a tractor or a dust devil throws you back up unbeaten,
        // and a falling cone or rock is no place to land
        const qreal top = b.feet.y() - b.height;
        const bool fromAbove = player_.vel.y() > 0.0 && previousFeet <= top + 6.0;
        if (fromAbove && (foe.kind == FoeKind::Cow || foe.kind == FoeKind::Tractor
                          || foe.kind == FoeKind::DustDevil))
        {
            const bool high = foe.kind != FoeKind::Tractor || (held_ & KeyJump);
            player_.vel.setY(-(high ? STOMP_HELD_V : STOMP_V));
            player_.grounded = false;
            const QStringList& pool = foe.kind == FoeKind::Cow     ? cowPops()
                                    : foe.kind == FoeKind::Tractor ? tractorPops()
                                                                   : dustDevilPops();
            addPop(QPointF(b.feet.x(), top - 8.0), pickFresh(pool), ICE);
            continue;
        }
        if (fromAbove && foe.kind != FoeKind::Faller)
        {
            const int points = CHAIN[std::min(stompChain_, 4)];
            ++stompChain_;
            const QStringList& pool = foe.kind == FoeKind::Reviewer ? reviewerPops()
                                    : foe.kind == FoeKind::Bubble   ? bubblePops()
                                    : foe.kind == FoeKind::Mosquito ? mosquitoPops()
                                                                    : advectionPops();
            defeat(foe, points, pickFresh(pool));
            player_.vel.setY(-((held_ & KeyJump) ? STOMP_HELD_V : STOMP_V));
            player_.grounded = false;
            continue;
        }

        if (invulnerable_ <= 0.0)
        {
            // a cone or a rock that hits glances off
            if (foe.kind == FoeKind::Faller)
            {
                foe.defeated = true;
                foe.squash = 0.3;
            }
            hurt();
            if (state_ != State::Running)
                return;
        }
    }
}

void FluxRunner::defeat(Foe &foe, int value, const QString &text)
{
    foe.defeated = true;
    foe.squash = foe.kind == FoeKind::Reviewer ? 0.4 : 0.0;
    ++defeated_;
    const int scored = points(value);
    score_ += scored;
    const Body& b = foe.body;
    addPop(QPointF(b.feet.x(), b.feet.y() - b.height - 6.0),
           QStringLiteral("%1 +%2").arg(text).arg(scored), AMBER);
}

void FluxRunner::bumpBlock(int index)
{
    Block& block = blocks_[index];
    block.bump = 6.0;
    if (block.used)
        return;

    const QPointF top(block.column * BAR_W + BLOCK / 2, block.bottom - BLOCK);
    if (block.item == Item::Coin)
    {
        gainCoin(top);
        addPop(top, pickFresh(blockPops()), COIN_GREEN);
        if (--block.coinsLeft <= 0)
            block.used = true;
        return;
    }

    // a hat block holds a filter for somebody already wearing a hat
    Item item = block.item;
    if (item == Item::Hat && power_ != Power::Small)
        item = Item::Filter;
    pickups_.append({ item, { QPointF(top.x(), top.y() + 18.0), QPointF(), 9.0, 18.0, true },
                      0.0, 1 });
    block.used = true;
}

void FluxRunner::fire()
{
    if (state_ != State::Running || power_ != Power::Filter || shots_.size() >= 2
        || fireCooldown_ > 0.0)
        return;
    const int dir = facingLeft_ ? -1 : 1;
    const QPointF start(player_.feet.x() + dir * 14.0, player_.feet.y() - 28.0);
    // not out of the far side of a wall
    if (groundTop(start.x()) < start.y())
        return;
    shots_.append({ { start, QPointF(dir * SHOT_SPEED + 0.5 * player_.vel.x(), 0.0),
                      4.0, 8.0, false }, 0.0 });
    fireCooldown_ = 0.25;
}

void FluxRunner::hurt(const QString &why)
{
    if (invulnerable_ > 0.0 || coffee_ > 0.0)
        return;
    if (power_ != Power::Small)
    {
        power_ = Power::Small;
        invulnerable_ = HURT_INVULNERABLE_S;
        addPop(QPointF(player_.feet.x(), player_.feet.y() - 70.0),
               why.isEmpty() ? pickFresh(hurtPops()) : why, DANGER);
        return;
    }
    die(false);
}

void FluxRunner::die(bool pit)
{
    diedInPit_ = pit;
    state_ = State::Dying;
    stateTime_ = 0.0;
    player_.vel = QPointF();
    shots_.clear();
}

void FluxRunner::respawn()
{
    const int c = respawnColumn_;
    const qreal x = c * BAR_W + BAR_W / 2;
    player_ = { QPointF(x, standTop(x, PLAYER_HALF_W)), QPointF(), PLAYER_HALF_W,
                PLAYER_HEIGHT, true };
    power_ = Power::Small;
    invulnerable_ = 0.0;
    coffee_ = 0.0;
    jumpBuffer_ = 0.0;
    coyote_ = 0.0;
    fireCooldown_ = 0.0;
    crouching_ = false;
    facingLeft_ = false;
    stompChain_ = 0;
    cardDuration_ = CARD_S;

    const qreal view = width();
    cameraX_ = std::clamp(player_.feet.x() - 0.45 * view, 0.0,
                          std::max(columnCount() * BAR_W - view, 0.0));

    // whatever is still undefeated ahead comes back to where it started;
    // what was collected and beaten stays that way
    for (auto& foe : foes_)
    {
        if (foe.defeated || foe.spawn.feet.x() <= player_.feet.x() + 4 * BAR_W)
            continue;
        foe.body = foe.spawn;
        foe.alive = true;
        foe.active = false;
        foe.facing = -1;
        foe.timer = uniform(0.5, 2.5);
    }
    pickups_.clear();
    shots_.clear();
    eddies_.clear();

    nextGap_ = 0;
    while (nextGap_ < gapEnds_.size() && gapEnds_.at(nextGap_) * BAR_W <= player_.feet.x())
        ++nextGap_;

    state_ = State::Card;
    stateTime_ = 0.0;
    accumulator_ = 0.0;
}

void FluxRunner::enterTube(int index)
{
    tubeIndex_ = index;
    clearStage_ = 0;
    stateTime_ = 0.0;
    state_ = State::Piping;
    player_.vel = QPointF();
    player_.feet.setX(tubes_.at(index).column * BAR_W + BAR_W);
    crouching_ = false;
    player_.height = PLAYER_HEIGHT;
}

void FluxRunner::reachTower()
{
    // the higher up the mast, the more it is worth
    const qreal ground = bars_.at(towerColumn_).top;
    const qreal above = ground - player_.feet.y();
    const int bonus = points(above > 200.0 ? 5000 : above > 150.0 ? 2000 : above > 100.0 ? 800
                             : above > 50.0 ? 400 : 100);
    score_ += bonus;
    addPop(QPointF(towerColumn_ * BAR_W + BAR_W / 2, player_.feet.y() - 20.0),
           QStringLiteral("+%1").arg(bonus), AMBER);

    state_ = State::Cleared;
    clearStage_ = 0;
    stateTime_ = 0.0;
    player_.vel = QPointF();
    player_.feet.setX(towerColumn_ * BAR_W + BAR_W / 2 - player_.halfW - 2.0);
    player_.feet.setY(std::min(player_.feet.y(), ground));
    crouching_ = false;
    player_.height = PLAYER_HEIGHT;
}

void FluxRunner::finishLevel(bool cleared)
{
    cleared_ = cleared;
    if (cleared)
    {
        score_ += points(10 * static_cast<int>(std::ceil(std::max(deadline_, 0.0))));
        ++levelsCleared_;
        // the higher the level, the more its quality counts
        qualitySum_ += level_ * levelQuality();
        qualityWeight_ += level_;
    }
    // what this level adds to the run, cleared or not
    runCoins_ += coinCount_;
    runTotalCoins_ += static_cast<int>(coins_.size()) + blockCoins_;
    runDefeated_ += defeated_;
    runGaps_ += gapsBridged_;
    runLivesLost_ += livesLost_;
    runSeconds_ += runTime_;
    state_ = State::Over;
    stateTime_ = 0.0;
    held_ = 0;
}

void FluxRunner::togglePause()
{
    if (state_ == State::Running)
    {
        state_ = State::Paused;
        pauseLine_ = pickFresh(pauseLines());
        held_ = 0;
    }
    else if (state_ == State::Paused)
    {
        state_ = State::Running;
        accumulator_ = 0.0;
        clock_.restart();
    }
    update();
}

void FluxRunner::noticeDanger()
{
    // the site's own danger is named the first time it shows in a level
    if (dangerAnnounced_)
        return;
    dangerAnnounced_ = true;
    announce(pickFresh(dangerBanners(site_)), AMBER);
}

void FluxRunner::addPop(const QPointF &world, const QString &text, const QColor &color)
{
    pops_.append({ world, 0.0, text, color });
}

void FluxRunner::announce(const QString &text, const QColor &color)
{
    banner_ = text;
    bannerColor_ = color;
    bannerAge_ = 0.0;
}

qreal FluxRunner::levelQuality() const
{
    // as before, on this level alone: coverage, foes, pace for its length,
    // and the lives kept on it
    const int total = static_cast<int>(coins_.size()) + blockCoins_;
    const qreal coverage = std::min(1.0, coinCount_ / static_cast<qreal>(std::max(total, 1)));
    const qreal beaten = defeated_ / static_cast<qreal>(std::max(static_cast<int>(foes_.size()), 1));
    const qreal par = 150.0 * dataSlots_ / 336.0;
    const qreal pace = std::clamp((par - runTime_) / (2.0 / 3.0 * par), 0.0, 1.0);
    const qreal kept = std::clamp((3.0 - livesLost_) / 3.0, 0.0, 1.0);
    return 0.45 * coverage + 0.20 * beaten + 0.20 * pace + 0.15 * kept;
}

FluxRunner::Result FluxRunner::result() const
{
    Result r;
    r.coins = coinCount_;
    r.totalCoins = static_cast<int>(coins_.size()) + blockCoins_;
    r.defeated = defeated_;
    r.totalFoes = static_cast<int>(foes_.size());
    r.gapsBridged = gapsBridged_;
    r.seconds = runTime_;
    r.livesLost = livesLost_;
    r.firstDoy = firstDoy_;
    r.level = level_;
    r.site = siteName_;
    r.siteCode = siteCode_;
    r.season = season_;
    r.cleared = cleared_;
    r.flawless = cleared_ && (livesLost_ == 0 || r.coins >= r.totalCoins);
    r.score = score_;
    r.levelsCleared = levelsCleared_;
    r.lives = lives_;
    r.runCoins = runCoins_;
    r.runTotalCoins = runTotalCoins_;
    r.runDefeated = runDefeated_;
    r.runGaps = runGaps_;
    r.runLivesLost = runLivesLost_;
    r.runSeconds = runSeconds_;
    r.firstSite = firstSite_;
    r.timedOut = timedOut_;

    // the run's rating: how many levels, and how well, the higher ones
    // counting more; a run that cleared nothing only has its coverage
    const qreal cleared = levelsCleared_;
    if (levelsCleared_ <= 0)
    {
        r.rating = qRound(25.0 * r.runCoins / static_cast<qreal>(std::max(r.runTotalCoins, 1)));
    }
    else
    {
        const qreal quality = qualitySum_ / std::max(qualityWeight_, 1e-9);
        r.rating = qRound(100.0 * (0.5 * cleared / (cleared + 2.0) + 0.5 * quality));
    }
    return r;
}

////////////////////////////////////////////////////////////////////////////////
// FluxRunner: the keyboard

bool FluxRunner::event(QEvent *event)
{
    // a main-window shortcut on one of these keys would otherwise take it
    if (event->type() == QEvent::ShortcutOverride && state_ != State::Idle)
    {
        const auto key = static_cast<QKeyEvent*>(event);
        if (isGameKey(key->key()) && !commandModifiers(key))
        {
            event->accept();
            return true;
        }
    }
    return QWidget::event(event);
}

void FluxRunner::keyPressEvent(QKeyEvent *event)
{
    if (state_ == State::Idle || !isGameKey(event->key()) || commandModifiers(event))
    {
        QWidget::keyPressEvent(event);
        return;
    }

    // never passed on: the scroll area around the page scrolls on arrows
    event->accept();
    // held keys are state; the repeats only add noise
    if (event->isAutoRepeat())
        return;

    switch (event->key())
    {
        case Qt::Key_Left:
        case Qt::Key_A:
            held_ |= KeyLeft;
            break;
        case Qt::Key_Right:
        case Qt::Key_D:
            held_ |= KeyRight;
            break;
        case Qt::Key_Space:
        case Qt::Key_Up:
        case Qt::Key_W:
            held_ |= KeyJump;
            if (state_ == State::Running)
                jumpBuffer_ = JUMP_BUFFER_S;
            break;
        case Qt::Key_Down:
        case Qt::Key_S:
            held_ |= KeyDown;
            if (state_ == State::Running && player_.grounded)
            {
                for (int i = 0; i < tubes_.size(); ++i)
                {
                    const qreal x0 = tubes_.at(i).column * BAR_W;
                    if (player_.feet.x() >= x0 + 4.0 && player_.feet.x() <= x0 + 2 * BAR_W - 4.0
                        && std::abs(player_.feet.y() - bars_.at(tubes_.at(i).column).top) < 1.0)
                    {
                        enterTube(i);
                        break;
                    }
                }
            }
            break;
        case Qt::Key_X:
        case Qt::Key_Shift:
            fire();
            break;
        case Qt::Key_P:
        case Qt::Key_Escape:
            togglePause();
            break;
        default:
            break;
    }
}

void FluxRunner::keyReleaseEvent(QKeyEvent *event)
{
    if (state_ == State::Idle || !isGameKey(event->key()) || commandModifiers(event))
    {
        QWidget::keyReleaseEvent(event);
        return;
    }
    event->accept();
    if (event->isAutoRepeat())
        return;

    switch (event->key())
    {
        case Qt::Key_Left:
        case Qt::Key_A:
            held_ &= ~static_cast<unsigned>(KeyLeft);
            break;
        case Qt::Key_Right:
        case Qt::Key_D:
            held_ &= ~static_cast<unsigned>(KeyRight);
            break;
        case Qt::Key_Space:
        case Qt::Key_Up:
        case Qt::Key_W:
            held_ &= ~static_cast<unsigned>(KeyJump);
            break;
        case Qt::Key_Down:
        case Qt::Key_S:
            held_ &= ~static_cast<unsigned>(KeyDown);
            break;
        default:
            break;
    }
}

void FluxRunner::focusOutEvent(QFocusEvent *event)
{
    // a key released while somewhere else would otherwise stay held
    held_ = 0;
    if (state_ == State::Running)
        togglePause();
    QWidget::focusOutEvent(event);
}

void FluxRunner::hideEvent(QHideEvent *event)
{
    held_ = 0;
    if (state_ == State::Running)
        togglePause();
    QWidget::hideEvent(event);
}

void FluxRunner::mousePressEvent(QMouseEvent *event)
{
    setFocus(Qt::MouseFocusReason);
    if (state_ == State::Paused)
        togglePause();
    event->accept();
}

bool FluxRunner::focusNextPrevChild(bool next)
{
    // Tab does not leave a run in progress
    if (state_ != State::Idle)
        return false;
    return QWidget::focusNextPrevChild(next);
}

////////////////////////////////////////////////////////////////////////////////
// FluxRunner: drawing

void FluxRunner::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF play(0, 0, width(), PLAY_HEIGHT);

    if (bars_.isEmpty())
    {
        p.fillRect(rect(), NIGHT_SKY);
        return;
    }

    p.save();
    p.setClipRect(play);
    paintSky(p);
    paintParallax(p);
    paintAxes(p);

    // the world, at a whole-pixel offset so nothing shimmers
    p.save();
    p.translate(-std::round(cameraX_), 0.0);
    paintAtmosphereBack(p);
    paintGround(p);
    paintThings(p);
    paintFoes(p);
    paintEddies(p);
    paintPlayer(p);

    auto popFont = font();
    popFont.setBold(true);
    p.setFont(popFont);
    for (const auto& pop : pops_)
    {
        QColor color = pop.color;
        color.setAlphaF(std::clamp(1.0 - pop.age / POP_S, 0.0, 1.0));
        p.setPen(color);
        const qreal half = p.fontMetrics().horizontalAdvance(pop.text) / 2.0;
        p.drawText(pop.pos + QPointF(-half, -30.0 * pop.age), pop.text);
    }
    p.restore();

    paintWeather(p);
    p.restore();

    paintHud(p);
    paintOverlay(p);
}

void FluxRunner::paintSky(QPainter &p) const
{
    const QRectF play(0, 0, width(), PLAY_HEIGHT);
    QLinearGradient gradient(0, 0, 0, PLAY_HEIGHT);
    const qreal centre = cameraX_ + width() / 2.0;
    const qreal night = nightAt(centre);
    // an overcast week greys the day
    const QColor sky = weather_ == 2 ? mix(sky_, QColor(125, 130, 135), 0.45 * (1.0 - night)) : sky_;
    gradient.setColorAt(0.0, sky.darker(115));
    gradient.setColorAt(1.0, sky.lighter(125));
    p.fillRect(play, gradient);
    if (night > 0.05)
    {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 240, static_cast<int>(200 * night)));
        for (const auto& star : stars_)
            p.drawEllipse(QPointF(star.x() * width(), star.y() * PLAY_HEIGHT), 1.0, 1.0);
    }

    // the sun by day and the moon by night, each on its arc
    const qreal hour = hourAt(centre);
    const bool day = hour >= 5.5 && hour <= 20.5;
    const qreal f = day ? (hour - 5.5) / 15.0 : std::fmod(hour + 24.0 - 20.5, 24.0) / 9.0;
    const QPointF body(width() * (0.1 + 0.8 * f),
                       PLAY_HEIGHT * 0.45 - std::sin(PI * std::clamp(f, 0.0, 1.0)) * PLAY_HEIGHT * 0.35);
    p.setPen(Qt::NoPen);
    if (day)
    {
        p.setBrush(QColor(255, 225, 120, 70));
        p.drawEllipse(body, 24, 24);
        p.setBrush(QColor(255, 225, 120));
        p.drawEllipse(body, 14, 14);
    }
    else
    {
        p.setBrush(QColor(235, 235, 215));
        p.drawEllipse(body, 10, 10);
        p.setBrush(sky_.darker(110));
        p.drawEllipse(body + QPointF(4, -3), 9, 9);
    }
}

void FluxRunner::paintParallax(QPainter &p) const
{
    const qreal w = width();
    const qreal night = nightAt(cameraX_ + w / 2.0);

    paintFarScenery(p, night);

    // clouds, by day
    if (night < 0.95)
    {
        p.setPen(Qt::NoPen);
        const QColor cloud = weather_ == 2 ? QColor(205, 205, 210) : QColor(255, 255, 255);
        p.setBrush(QColor(cloud.red(), cloud.green(), cloud.blue(),
                          static_cast<int>(170 * (1.0 - night))));
        const qreal span = w + 240;
        for (int i = 0; i < cloudCount_; ++i)
        {
            qreal x = std::fmod(i * 260.0 - cameraX_ * 0.3, span);
            if (x < 0)
                x += span;
            x -= 120;
            const qreal y = 30 + (i * 37) % 70;
            p.drawEllipse(QPointF(x, y), 28, 11);
            p.drawEllipse(QPointF(x + 22, y - 6), 20, 10);
            p.drawEllipse(QPointF(x - 20, y + 2), 16, 8);
        }
    }

    paintNearScenery(p, night);

    // distant flux towers, a red light at the top at night
    const qreal shift2 = cameraX_ * 0.5;
    const QColor lattice = mix(QColor(70, 90, 90), QColor(30, 34, 50), night);
    for (qreal tower = std::floor(shift2 / 900.0) * 900.0; tower < shift2 + w + 900.0; tower += 900.0)
    {
        const qreal x = tower - shift2 + 300.0;
        const qreal base = PLAY_HEIGHT - 140.0;
        p.setPen(QPen(lattice, 1.2));
        p.drawLine(QPointF(x - 3, base), QPointF(x - 1, base - 110));
        p.drawLine(QPointF(x + 3, base), QPointF(x + 1, base - 110));
        for (qreal y = base; y > base - 110; y -= 14)
            p.drawLine(QPointF(x - 3, y), QPointF(x + 3, y - 14));
        if (night > 0.3 && std::fmod(clockTime_, 1.2) < 0.6)
        {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 40, 30));
            p.drawEllipse(QPointF(x, base - 112), 2.2, 2.2);
        }
    }
}

void FluxRunner::paintFarScenery(QPainter &p, qreal night) const
{
    // the far horizon: what kind of place this is, seen from a long way off
    const qreal w = width();
    const qreal shift = cameraX_ * 0.15;
    const qreal base = PLAY_HEIGHT - 165.0;
    QColor colour = mix(sites().at(site_).hills, sky_, 0.3);
    if (snowy_)
        colour = mix(colour, QColor(235, 240, 248), 0.45);
    colour = mix(colour, QColor(20, 28, 45), night);

    // the mountains: a summit in every span, each of its own height and
    // place, and a valley of its own height between each two
    const qreal span = 150.0;
    auto valley = [](qreal k) { return 25.0 + 15.0 * hash01(k + 911.0); };
    auto summit = [](qreal k) { return 60.0 + 110.0 * hash01(k + 311.0); };
    auto summitAt = [](qreal k) { return 0.3 + 0.4 * hash01(k + 517.0); };

    // how high the skyline stands above the base at world position x
    auto skyline = [&](qreal x) -> qreal
    {
        switch (site_)
        {
            case Forest:
                // wooded ridges, the treeline ragged against the sky
                return 22.0 + 18.0 * std::sin(x * 0.004) + 9.0 * std::sin(x * 0.011 + 1.3)
                       + 5.0 * std::abs(std::sin(x * 0.12)) + 3.0 * std::abs(std::sin(x * 0.31 + 0.7));
            case Grassland:
                return 14.0 + 12.0 * std::sin(x * 0.003) + 6.0 * std::sin(x * 0.008 + 2.0);
            case Cropland:
                // hedgerows along a flat horizon
                return 6.0 + 3.0 * std::sin(x * 0.004) + 5.0 * std::abs(std::sin(x * 0.07));
            case Tundra:
                return 18.0 + 15.0 * std::sin(x * 0.0035) + 7.0 * std::sin(x * 0.009 + 0.5);
            case Mountain:
            {
                const qreal k = std::floor(x / span);
                const qreal f = x / span - k;
                const qreal top = summitAt(k);
                const qreal edge = f < top
                    ? valley(k) + (summit(k) - valley(k)) * f / top
                    : summit(k) + (valley(k + 1.0) - summit(k)) * (f - top) / (1.0 - top);
                return edge + 3.0 * std::sin(x * 0.09);
            }
            case Desert:
                // rolling dunes
                return 10.0 + 14.0 * std::pow(std::abs(std::sin(x * 0.005)), 1.5) + 4.0 * std::sin(x * 0.013);
            default:
                // a wetland and a savanna are flat to the horizon
                return 6.0 + 2.5 * std::sin(x * 0.006);
        }
    };

    QPainterPath horizon;
    horizon.moveTo(0, PLAY_HEIGHT);
    for (qreal x = 0; x <= w + 6; x += 6)
        horizon.lineTo(x, base - skyline(x + shift));
    horizon.lineTo(w + 6, PLAY_HEIGHT);
    horizon.closeSubpath();
    p.fillPath(horizon, colour);

    // snow on the peaks, further down in a snowy week
    if (site_ == Mountain)
    {
        const qreal snowline = base - (snowy_ ? 70.0 : 105.0);
        QPainterPath snow;
        snow.moveTo(0, 0);
        for (qreal x = 0; x <= w + 6; x += 6)
            snow.lineTo(x, snowline + 6.0 * std::sin((x + shift) * 0.08) + 4.0 * std::sin((x + shift) * 0.21));
        snow.lineTo(w + 6, 0);
        snow.closeSubpath();
        p.fillPath(snow.intersected(horizon), mix(QColor(240, 244, 250), QColor(40, 48, 66), night));
    }

    // now and then something on the horizon, at a place of its own
    auto landmarks = [&](qreal every, qreal salt, qreal share, auto draw)
    {
        for (qreal k = std::floor((shift - 200.0) / every); k * every - shift < w + 200.0; k += 1.0)
        {
            if (hash01(k + salt) >= share)
                continue;
            const qreal x = k * every + 0.6 * every * hash01(k + salt + 0.5) - shift;
            draw(x, base - skyline(x + shift) + 2.0, k);
        }
    };
    p.setPen(Qt::NoPen);
    if (site_ == Cropland)
    {
        // a farmstead: a barn and its silo
        p.setBrush(colour.darker(115));
        landmarks(700.0, 41.0, 0.6, [&](qreal x, qreal y, qreal)
        {
            p.drawRect(QRectF(x, y - 18, 34, 18));
            p.drawPolygon(QPolygonF { QPointF(x - 3, y - 18), QPointF(x + 17, y - 30), QPointF(x + 37, y - 18) });
            p.drawRect(QRectF(x + 40, y - 36, 10, 36));
            p.drawEllipse(QRectF(x + 40, y - 41, 10, 10));
        });
    }
    else if (site_ == Savanna || site_ == Desert)
    {
        // flat-topped mesas, and in a desert the narrow buttes they wear down to
        p.setBrush(mix(colour, QColor(165, 95, 65), 0.25 * (1.0 - night)).darker(108));
        landmarks(site_ == Desert ? 650.0 : 900.0, 73.0, 0.5, [&](qreal x, qreal y, qreal k)
        {
            const bool butte = site_ == Desert && hash01(k + 74.0) < 0.4;
            const qreal across = butte ? 26.0 + 16.0 * hash01(k + 75.0) : 90.0 + 70.0 * hash01(k + 75.0);
            const qreal high = 30.0 + 25.0 * hash01(k + 76.0) + (butte ? 12.0 : 0.0);
            const qreal slope = std::min(14.0, 0.2 * across);
            p.drawPolygon(QPolygonF { QPointF(x, y), QPointF(x + slope, y - high + 3),
                                      QPointF(x + slope + 4, y - high), QPointF(x + across - slope - 4, y - high),
                                      QPointF(x + across - slope, y - high + 3), QPointF(x + across, y) });
        });
    }
}

void FluxRunner::paintNearScenery(QPainter &p, qreal night) const
{
    // the near distance: what grows or stands at the site, between the far
    // horizon and the trace
    const qreal w = width();
    const qreal shift = cameraX_ * 0.35;
    const qreal base = PLAY_HEIGHT - 135.0;
    const Site& site = sites().at(site_);
    auto shade = [night](const QColor& c) { return mix(c, QColor(12, 16, 28), 0.85 * night); };
    // a cropland out of season is bare soil, as on the ground itself
    const bool bare = site_ == Cropland && seasonFactor_ < 0.3;
    QColor ground = bare ? QColor(110, 85, 55) : site.scenery;
    if (snowy_)
        ground = mix(ground, QColor(225, 230, 238), 0.5);
    const QColor body = shade(ground);
    const QColor trunk = shade(site.scenery.darker(160));
    const QColor leaf = shade(site.scenery.lighter(115));
    const QColor snow = shade(QColor(240, 244, 250));

    // the ground they stand on; in the mountains a ridge of rock
    auto groundAt = [&](qreal x) -> qreal
    {
        const qreal wx = x + shift;
        if (site_ == Mountain)
        {
            const qreal k = std::floor(wx / 70.0);
            const qreal f = wx / 70.0 - k;
            const qreal a = 12.0 + 40.0 * hash01(k + 1201.0);
            const qreal b = 12.0 + 40.0 * hash01(k + 1202.0);
            return base - (a + (b - a) * f) - 6.0 * std::abs(std::sin(wx * 0.11));
        }
        return base - 4.0 * std::sin(wx * 0.01) - 2.0 * std::sin(wx * 0.027 + 1.0);
    };

    QPainterPath band;
    band.moveTo(0, PLAY_HEIGHT);
    for (qreal x = 0; x <= w + 6; x += 6)
        band.lineTo(x, groundAt(x));
    band.lineTo(w + 6, PLAY_HEIGHT);
    band.closeSubpath();
    if (site_ == Wetland)
    {
        // open water between the reeds, with the sky in it
        p.fillPath(band, shade(mix(sky_, QColor(50, 80, 80), 0.45)));
        p.setPen(QPen(shade(mix(sky_, QColor(255, 255, 255), 0.4)), 1.0));
        const qreal across = w + 80.0;
        for (int i = 0; i < 14; ++i)
        {
            qreal x = std::fmod(i * 97.0 - shift, across);
            if (x < 0)
                x += across;
            x -= 40.0;
            const qreal y = base + 6.0 + (i * 23) % 40;
            p.drawLine(QPointF(x, y), QPointF(x + 14.0 + (i % 3) * 6.0, y));
        }
    }
    else
    {
        p.fillPath(band, body);
    }

    // one thing or none per tile: its kind, place and size from the tile's number
    auto tiles = [&](qreal tile, qreal salt, auto draw)
    {
        for (qreal k = std::floor((shift - 120.0) / tile); k * tile - shift < w + 120.0; k += 1.0)
        {
            const qreal x = (k + 0.15 + 0.7 * hash01(k * 1.31 + salt)) * tile - shift;
            draw(k, x, groundAt(x) + 2.0, hash01(k + salt));
        }
    };

    auto conifer = [&](qreal x, qreal y, qreal h, const QColor& c)
    {
        p.setPen(Qt::NoPen);
        p.setBrush(trunk);
        p.drawRect(QRectF(x - 1.5, y - 0.2 * h, 3.0, 0.2 * h));
        for (int i = 0; i < 3; ++i)
        {
            const qreal bottom = y - (0.15 + 0.25 * i) * h;
            const qreal half = (0.27 - 0.06 * i) * h;
            const qreal tier = 0.42 * h;
            p.setBrush(c);
            p.drawPolygon(QPolygonF { QPointF(x - half, bottom), QPointF(x, bottom - tier),
                                      QPointF(x + half, bottom) });
            if (snowy_)
            {
                p.setBrush(snow);
                p.drawPolygon(QPolygonF { QPointF(x - 0.3 * half, bottom - 0.7 * tier),
                                          QPointF(x, bottom - tier),
                                          QPointF(x + 0.3 * half, bottom - 0.7 * tier) });
            }
        }
    };
    auto broadleaf = [&](qreal x, qreal y, qreal h, const QColor& c)
    {
        p.setPen(Qt::NoPen);
        p.setBrush(trunk);
        p.drawRect(QRectF(x - 2.0, y - 0.5 * h, 4.0, 0.5 * h));
        const qreal r = 0.26 * h;
        p.setBrush(c);
        p.drawEllipse(QPointF(x, y - 0.72 * h), r, 0.95 * r);
        p.drawEllipse(QPointF(x - 0.7 * r, y - 0.56 * h), 0.75 * r, 0.7 * r);
        p.drawEllipse(QPointF(x + 0.7 * r, y - 0.58 * h), 0.8 * r, 0.72 * r);
        if (snowy_)
        {
            p.setBrush(snow);
            p.drawEllipse(QPointF(x, y - 0.72 * h - 0.6 * r), 0.6 * r, 0.3 * r);
        }
    };
    auto acacia = [&](qreal x, qreal y, qreal h, const QColor& c)
    {
        p.setPen(QPen(trunk, 2.2, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(x, y), QPointF(x, y - 0.55 * h));
        p.drawLine(QPointF(x, y - 0.55 * h), QPointF(x - 0.22 * h, y - 0.85 * h));
        p.drawLine(QPointF(x, y - 0.55 * h), QPointF(x + 0.2 * h, y - 0.88 * h));
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawEllipse(QPointF(x, y - 0.92 * h), 0.55 * h, 0.12 * h);
        p.drawEllipse(QPointF(x - 0.2 * h, y - 0.98 * h), 0.3 * h, 0.08 * h);
    };
    auto cactus = [&](qreal x, qreal y, qreal h, const QColor& c)
    {
        p.setPen(QPen(c, 6.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(x, y), QPointF(x, y - h));
        p.drawPolyline(QPolygonF { QPointF(x, y - 0.45 * h), QPointF(x - 9, y - 0.45 * h),
                                   QPointF(x - 9, y - 0.72 * h) });
        p.drawPolyline(QPolygonF { QPointF(x, y - 0.58 * h), QPointF(x + 8, y - 0.58 * h),
                                   QPointF(x + 8, y - 0.8 * h) });
    };
    auto reeds = [&](qreal x, qreal y, qreal h, qreal seed)
    {
        const QColor head = shade(QColor(110, 70, 40));
        for (int i = 0; i < 5; ++i)
        {
            const qreal foot = x + 2.0 * i - 4.0;
            const qreal lean = 2.5 * (i - 2) + 4.0 * (hash01(seed + i) - 0.5);
            const qreal top = h * (0.65 + 0.35 * hash01(seed + i + 9.0));
            QPainterPath stem;
            stem.moveTo(foot, y);
            stem.quadTo(foot, y - 0.6 * top, foot + lean, y - top);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(leaf, 1.3));
            p.drawPath(stem);
            if (i == 1 || i == 3)
            {
                // a cattail
                p.setPen(Qt::NoPen);
                p.setBrush(head);
                p.drawRoundedRect(QRectF(foot + 0.85 * lean - 1.8, y - 0.92 * top, 3.6, 9.0), 1.5, 1.5);
            }
        }
    };
    auto snag = [&](qreal x, qreal y, qreal h)
    {
        p.setPen(QPen(trunk, 3.0, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(x, y), QPointF(x + 2, y - h));
        p.setPen(QPen(trunk, 1.8, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(x + 1, y - 0.6 * h), QPointF(x - 0.25 * h, y - 0.8 * h));
        p.drawLine(QPointF(x + 1.5, y - 0.75 * h), QPointF(x + 0.22 * h, y - 0.9 * h));
    };
    // a shrub, a boulder or a mound: the top half of an ellipse
    auto lump = [&](qreal x, qreal y, qreal across, qreal high, const QColor& c)
    {
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawChord(QRectF(x - across / 2, y - high, across, 2 * high), 0, 180 * 16);
        if (snowy_)
        {
            p.setBrush(snow);
            p.drawChord(QRectF(x - 0.35 * across, y - high, 0.7 * across, 0.8 * high), 0, 180 * 16);
        }
    };
    auto termites = [&](qreal x, qreal y, qreal h)
    {
        QPainterPath mound;
        mound.moveTo(x - 0.32 * h, y);
        mound.quadTo(x - 0.12 * h, y - 0.7 * h, x - 0.04 * h, y - h);
        mound.quadTo(x + 0.02 * h, y - 1.06 * h, x + 0.06 * h, y - 0.96 * h);
        mound.quadTo(x + 0.14 * h, y - 0.6 * h, x + 0.3 * h, y);
        mound.closeSubpath();
        p.fillPath(mound, shade(QColor(150, 85, 55)));
    };
    auto bale = [&](qreal x, qreal y)
    {
        p.setPen(QPen(shade(QColor(150, 125, 60)), 1.0));
        p.setBrush(shade(QColor(200, 175, 95)));
        p.drawEllipse(QRectF(x - 9, y - 13, 18, 13));
        p.drawEllipse(QRectF(x - 4, y - 9, 6, 5));
    };
    auto turbine = [&](qreal x, qreal y, qreal k)
    {
        const QColor white = shade(QColor(225, 228, 232));
        const QPointF hub(x, y - 105.0);
        p.setPen(QPen(white, 3.0, Qt::SolidLine, Qt::FlatCap));
        p.drawLine(QPointF(x, y), hub);
        p.setPen(QPen(white, 2.4, Qt::SolidLine, Qt::RoundCap));
        const qreal turn = 1.4 * clockTime_ + 2.0 * PI * hash01(k + 5.0);
        for (int blade = 0; blade < 3; ++blade)
        {
            const qreal a = turn + blade * 2.0 * PI / 3.0;
            p.drawLine(hub, hub + QPointF(38.0 * std::cos(a), 38.0 * std::sin(a)));
        }
        p.setPen(Qt::NoPen);
        p.setBrush(white);
        p.drawEllipse(hub, 3.0, 3.0);
    };
    // a varied green, so that trees standing together stay apart
    auto tree = [&](qreal k, const QColor& c)
    {
        return shade(mix(snowy_ ? mix(c, QColor(225, 230, 238), 0.25) : c, c.lighter(135),
                         0.6 * hash01(k + 14.0)));
    };

    switch (site_)
    {
        case Forest:
            tiles(24.0, 11.0, [&](qreal k, qreal x, qreal y, qreal r)
            {
                const qreal h = 48.0 + 34.0 * hash01(k + 12.0);
                if (r < 0.6)
                {
                    conifer(x, y, h, tree(k, site.scenery));
                }
                else
                {
                    // in autumn some of the broadleaves turn
                    const bool turned = seasonIndex_ == Autumn && hash01(k + 13.0) > 0.4;
                    broadleaf(x, y, 0.9 * h, turned ? shade(QColor(190, 110, 40)) : leaf);
                }
            });
            break;

        case Grassland:
        {
            tiles(64.0, 21.0, [&](qreal k, qreal x, qreal y, qreal r)
            {
                if (r < 0.12)
                    broadleaf(x, y, 70.0 + 20.0 * hash01(k + 22.0), leaf);
                else if (r < 0.3)
                    bale(x, y);
            });
            // fences along some stretches, eight posts at a time
            const qreal post = 30.0;
            p.setPen(QPen(trunk, 1.6));
            for (qreal k = std::floor((shift - 60.0) / post); k * post - shift < w + 60.0; k += 1.0)
            {
                if (hash01(std::floor(k / 8.0) + 77.0) > 0.45)
                    continue;
                const qreal x = k * post - shift;
                const qreal y = groundAt(x) + 2.0;
                p.drawLine(QPointF(x, y), QPointF(x, y - 14.0));
                if (std::floor((k + 1.0) / 8.0) != std::floor(k / 8.0))
                    continue;
                const qreal next = groundAt(x + post) + 2.0;
                p.drawLine(QPointF(x, y - 11.0), QPointF(x + post, next - 11.0));
                p.drawLine(QPointF(x, y - 6.0), QPointF(x + post, next - 6.0));
            }
            break;
        }

        case Wetland:
            tiles(20.0, 31.0, [&](qreal k, qreal x, qreal y, qreal r)
            {
                if (r < 0.07)
                    snag(x, y, 60.0 + 20.0 * hash01(k + 32.0));
                else if (r < 0.75)
                    reeds(x, y, 22.0 + 20.0 * hash01(k + 33.0), 7.0 * k);
            });
            break;

        case Cropland:
        {
            // the field: rows of the crop, or stubble, and its furrows
            p.setPen(QPen(shade(site.scenery.darker(120)), 1.0));
            for (qreal y = base + 10.0; y < PLAY_HEIGHT; y += 9.0)
                p.drawLine(QPointF(0, y), QPointF(w, y));
            p.setPen(QPen(shade(bare ? QColor(140, 110, 70) : site.scenery.lighter(125)), 1.2));
            const qreal row = 7.0;
            for (qreal x = -std::fmod(shift, row); x < w + row; x += row)
            {
                const qreal y = groundAt(x) + 1.0;
                const qreal h = bare ? 3.0 : 6.0 + 3.0 * hash01(std::floor((x + shift) / row));
                p.drawLine(QPointF(x, y), QPointF(x - 1.5, y - h));
                p.drawLine(QPointF(x + 2.5, y), QPointF(x + 3.5, y - 0.8 * h));
            }
            tiles(420.0, 23.0, [&](qreal k, qreal x, qreal y, qreal r)
            {
                if (r < 0.45)
                    turbine(x, y, k);
            });
            break;
        }

        case Tundra:
            tiles(30.0, 41.0, [&](qreal k, qreal x, qreal y, qreal r)
            {
                if (r < 0.5)
                    lump(x, y, 12.0 + 10.0 * hash01(k + 42.0), 5.0 + 5.0 * hash01(k + 43.0), leaf);
                else if (r < 0.68)
                    lump(x, y, 10.0 + 8.0 * hash01(k + 44.0), 6.0 + 5.0 * hash01(k + 45.0),
                         shade(QColor(120, 120, 115)));
                else if (r > 0.95)
                    lump(x, y, 60.0, 14.0, body.darker(110));   // a palsa
            });
            break;

        case Savanna:
        {
            // the grass, green in the wet season and straw in the dry
            const bool dry = seasonIndex_ == DrySeason;
            p.setPen(QPen(shade(dry ? QColor(190, 165, 95) : QColor(110, 140, 60)), 1.2));
            const qreal tuft = 14.0;
            for (qreal x = -std::fmod(shift, tuft); x < w + tuft; x += tuft)
            {
                const qreal y = groundAt(x) + 1.0;
                const qreal h = 5.0 + 6.0 * hash01(std::floor((x + shift) / tuft) + 51.0);
                p.drawLine(QPointF(x, y), QPointF(x - 3, y - h));
                p.drawLine(QPointF(x, y), QPointF(x, y - 1.1 * h));
                p.drawLine(QPointF(x, y), QPointF(x + 3, y - h));
            }
            const QColor crown = shade(dry ? QColor(130, 120, 60) : site.scenery.darker(110));
            tiles(76.0, 51.0, [&](qreal k, qreal x, qreal y, qreal r)
            {
                if (r < 0.38)
                    acacia(x, y, 46.0 + 24.0 * hash01(k + 52.0), crown);
                else if (r < 0.58)
                    termites(x, y, 22.0 + 16.0 * hash01(k + 53.0));
            });
            break;
        }

        case Mountain:
            tiles(28.0, 61.0, [&](qreal k, qreal x, qreal y, qreal r)
            {
                if (r < 0.55)
                    conifer(x, y, 30.0 + 18.0 * hash01(k + 62.0), tree(k, QColor(35, 60, 42)));
            });
            break;

        case Desert:
            tiles(84.0, 71.0, [&](qreal k, qreal x, qreal y, qreal r)
            {
                if (r < 0.28)
                    cactus(x, y, 34.0 + 20.0 * hash01(k + 72.0), shade(QColor(70, 110, 60)));
                else if (r < 0.5)
                    lump(x, y, 12.0 + 10.0 * hash01(k + 73.0), 5.0 + 4.0 * hash01(k + 74.0),
                         shade(QColor(140, 100, 70)));
                else if (r < 0.72)
                    lump(x, y, 14.0, 6.0, shade(QColor(120, 115, 70)));   // scrub
            });
            break;

        default:
            break;
    }
}

void FluxRunner::paintAxes(QPainter &p) const
{
    // the plot the level is drawn in: CO2 on the left, faint grid lines
    auto small = font();
    small.setPointSizeF(std::max(small.pointSizeF() - 2.0, 7.0));
    p.setFont(small);
    for (int co2 = 380; co2 <= 440; co2 += 20)
    {
        const qreal y = FLOOR - (co2 - CO2_AT_FLOOR) / CO2_PER_PX;
        p.setPen(QPen(QColor(255, 255, 255, 30), 1, Qt::DashLine));
        p.drawLine(QPointF(0, y), QPointF(width(), y));
        p.setPen(QColor(255, 255, 255, 130));
        p.drawText(QRectF(4, y - 15, 40, 14), Qt::AlignLeft | Qt::AlignVCenter, QString::number(co2));
    }
    p.setPen(QColor(255, 255, 255, 160));
    p.drawText(QRectF(8, 6, 220, 16), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("CO₂ (µmol mol⁻¹), 10 Hz"));
}

void FluxRunner::paintAtmosphereBack(QPainter &p) const
{
    const qreal viewLeft = cameraX_;
    const qreal viewRight = cameraX_ + width();

    // a desert by day shimmers above the ground
    if (site_ == Desert && nightAt(viewLeft + width() / 2.0) < 0.3)
    {
        p.setPen(QPen(QColor(255, 240, 210, 45), 1.4));
        p.setBrush(Qt::NoBrush);
        for (int k = 0; k < 4; ++k)
        {
            QPainterPath haze;
            const qreal base = FLOOR - 190.0 - 14.0 * k;
            haze.moveTo(viewLeft, base);
            for (qreal x = viewLeft; x <= viewRight; x += 6.0)
                haze.lineTo(x, base + 2.5 * std::sin(x * 0.05 + clockTime_ * (3.0 + k)));
            p.drawPath(haze);
        }
    }
    for (int z = 0; z < zones_.size(); ++z)
    {
        const Zone& zone = zones_.at(z);
        const qreal x0 = zone.start * BAR_W;
        const qreal x1 = (zone.end + 1) * BAR_W;
        if (x1 < viewLeft || x0 > viewRight)
            continue;

        p.fillRect(QRectF(x0, 0, x1 - x0, FLOOR), QColor(255, 255, 255, 14));

        // wind streaks moving with the gust, so the next surge can be read
        const qreal g = gust(z);
        const qreal speed = 4.0 * g;
        const qreal span = x1 - x0;
        const qreal length = 10.0 + 16.0 * std::abs(g) / spec_.gustMax;
        p.setPen(QPen(QColor(255, 255, 255, 70), 1.2));
        for (int i = 0; i < 18; ++i)
        {
            qreal x = std::fmod(i * 97.0 + clockTime_ * speed + 1000.0 * span, span);
            if (x < 0)
                x += span;
            const qreal y = 40.0 + std::fmod(i * 53.0, FLOOR - 160.0);
            p.drawLine(QPointF(x0 + x, y), QPointF(x0 + x + (g >= 0 ? length : -length), y));
        }

        // the updraft shimmers upwards
        if (zone.thermal >= 0)
        {
            const qreal tx = zone.thermal * BAR_W;
            qreal ground = groundTop(tx + 1.5 * BAR_W);
            if (!std::isfinite(ground))
                ground = FLOOR;
            p.setPen(QPen(QColor(255, 240, 200, 70), 1.4));
            p.setBrush(Qt::NoBrush);
            for (int k = 0; k < 3; ++k)
            {
                QPainterPath wave;
                const qreal x = tx + BAR_W * (0.5 + k);
                wave.moveTo(x, ground);
                for (qreal y = ground; y > ground - 230; y -= 6)
                    wave.lineTo(x + 3.0 * std::sin((y + clockTime_ * 90.0) * 0.08 + k), y);
                p.drawPath(wave);
            }
        }

        // a windsock at the edge of the zone
        const qreal wx = zone.start * BAR_W + BAR_W / 2;
        qreal ground = groundTop(wx);
        if (!std::isfinite(ground))
            continue;
        const qreal poleTop = ground - 46.0;
        p.setPen(QPen(QColor(90, 90, 90), 2));
        p.drawLine(QPointF(wx, ground), QPointF(wx, poleTop));
        const qreal strength = std::abs(g) / spec_.gustMax;
        const qreal reach = 26.0 * (0.3 + 0.7 * strength) * (g >= 0 ? 1 : -1);
        const qreal droop = 14.0 * (1.0 - strength);
        const QPointF a(wx, poleTop);
        const QPointF b(wx, poleTop + 9);
        const QPointF tip(wx + reach, poleTop + 3 + droop);
        const QPointF mid1 = a + (tip - a) * 0.5;
        const QPointF mid2 = b + (tip - b) * 0.5;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(250, 120, 40));
        p.drawPolygon(QPolygonF { a, mid1, mid2, b });
        p.setBrush(QColor(245, 245, 245));
        p.drawPolygon(QPolygonF { mid1, tip, mid2 });
    }
}

void FluxRunner::paintGround(QPainter &p) const
{
    const int count = static_cast<int>(samples_.size());
    const int first = std::max(sampleOf(cameraX_) - 2, 0);
    const int last = std::min(sampleOf(cameraX_ + width()) + 2, count - 1);
    const int firstSlot = std::max(columnOf(cameraX_) - 1, 0);
    const int lastSlot = std::min(columnOf(cameraX_ + width()) + 1, columnCount() - 1);

    auto small = font();
    small.setPointSizeF(std::max(small.pointSizeF() - 2.0, 7.0));
    p.setFont(small);

    // the time axis: a faint line every three hours, the day at midnight
    for (int c = std::max(firstSlot, LEAD); c <= lastSlot; ++c)
    {
        const int slot = c - LEAD + startSlot_;
        if (slot % 6 != 0)
            continue;
        const qreal x = c * BAR_W;
        p.setPen(QPen(QColor(255, 255, 255, 28), 1));
        p.drawLine(QPointF(x, 0), QPointF(x, FLOOR));
        p.setPen(QColor(255, 255, 255, 150));
        const QString label = slot % 48 == 0
            ? QStringLiteral("DOY %1").arg(wrapDoy(firstDoy_ + slot / 48))
            : QStringLiteral("%1:00").arg((slot % 48) / 2, 2, 10, QLatin1Char('0'));
        p.drawText(QRectF(x + 3, FLOOR - 16, 80, 14), Qt::AlignLeft | Qt::AlignVCenter, label);
    }

    // a fixed scatter per pixel: the drawing of the data only, so that the
    // trace looks like the thousands of 10 Hz points it stands for while the
    // ground under the feet stays the smooth line through the samples
    const auto hash = hash01;
    // the trace without its needles, where it is right now
    auto baseAt = [this](int i)
    {
        const Sample& s = samples_.at(i);
        return surfaceAt(i) - (s.top - s.base);
    };

    QLinearGradient earth(0, FLOOR - 220, 0, FLOOR);
    // the site's own ground; a cropland out of season is bare soil
    const Site& site = sites().at(site_);
    const bool bare = site_ == Cropland && seasonFactor_ < 0.3;
    earth.setColorAt(0.0, bare ? QColor(85, 60, 40, 240) : site.fillTop);
    earth.setColorAt(1.0, bare ? QColor(30, 22, 15, 250) : site.fillBottom);

    // one stretch of valid data at a time
    int i = first;
    while (i <= last)
    {
        if (samples_.at(i).gap)
        {
            ++i;
            continue;
        }
        const int start = i;
        QPolygonF line;
        while (i <= last && !samples_.at(i).gap)
        {
            line << QPointF(i * SAMPLE_W + SAMPLE_W / 2, baseAt(i));
            ++i;
        }

        QPainterPath ground;
        ground.moveTo(line.first().x(), FLOOR + 2);
        for (const auto& point : line)
            ground.lineTo(point);
        ground.lineTo(line.last().x(), FLOOR + 2);
        ground.closeSubpath();
        p.fillPath(ground, earth);

        // the scatter: wide by day, narrow on a stable night, with the odd
        // burst of intermittent turbulence; flat where a tube sits
        QVector<QLineF> scatter;
        scatter.reserve(static_cast<int>(line.last().x() - line.first().x()) + 2);
        for (int s = 0; s + 1 < line.size(); ++s)
        {
            const QPointF a = line.at(s);
            const QPointF b = line.at(s + 1);
            const int sample = start + s;
            const bool flat = samples_.at(sample).tube || samples_.at(sample + 1).tube;
            const qreal night = nightAt(a.x());
            const bool burst = night > 0.5 && hash(std::floor(a.x() / 40.0) + 7777.0) > 0.93;
            const qreal width = flat ? 0.0 : (8.5 - 6.9 * night) + (burst ? 5.0 : 0.0);
            for (qreal x = std::floor(a.x()); x < b.x(); x += 1.0)
            {
                const qreal y = a.y() + (b.y() - a.y()) * (x - a.x()) / (b.x() - a.x());
                const qreal spread = width * (0.45 + 0.55 * hash(x));
                scatter << QLineF(x + 0.5, y - 0.7 * spread * hash(x * 3.0 + 1.0),
                                  x + 0.5, y + spread * hash(x * 3.0 + 2.0) + 0.6);
            }
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(90, 225, 90, 45), 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(line);
        p.setPen(QPen(QColor(120, 240, 110, 190), 1.0));
        p.drawLines(scatter);
        p.setPen(QPen(QColor(170, 255, 160, 110), 1.0));
        p.drawPolyline(line);

        // where the data stop, the plot drops to the -9999 they were filled with
        p.setPen(QPen(QColor(230, 50, 40, 170), 1.3));
        if (start > 0 && samples_.at(start - 1).gap)
            p.drawLine(QPointF(line.first().x() - 3, FLOOR + 8), line.first());
        if (i < count && samples_.at(i).gap)
            p.drawLine(line.last(), QPointF(line.last().x() + 3, FLOOR + 8));
    }

    // the outliers: needles standing out of the trace
    for (int g = first; g <= last; ++g)
    {
        if (!samples_.at(g).spike)
            continue;
        const qreal x = g * SAMPLE_W + SAMPLE_W / 2;
        const QPointF tip(x, surfaceAt(g));
        p.setPen(QPen(QColor(120, 240, 110), 1.6, Qt::SolidLine, Qt::FlatCap));
        p.drawLine(QPointF(x, baseAt(g)), tip);
        p.setPen(Qt::NoPen);
        p.setBrush(DANGER);
        p.drawEllipse(tip, 2.2, 2.2);
    }

    for (int g = first; g <= last; ++g)
    {
        const Sample& sample = samples_.at(g);

        // the pit itself: -9999 at the bottom of the axis
        if (sample.gap && (g == 0 || !samples_.at(g - 1).gap))
        {
            int end = g;
            while (end < count && samples_.at(end).gap)
                ++end;
            p.setPen(QPen(QColor(230, 50, 40, 120), 1, Qt::DotLine));
            p.drawLine(QPointF(g * SAMPLE_W, FLOOR - 1), QPointF(end * SAMPLE_W, FLOOR - 1));
            p.setPen(QColor(230, 50, 40, 170));
            p.drawText(QRectF(g * SAMPLE_W, FLOOR - 18, BAR_W * 3, 16), Qt::AlignLeft | Qt::AlignVCenter,
                       QStringLiteral("-9999"));
        }

        // the tallest outlier of each outburst gets a flag at its tip
        bool tallest = sample.spike;
        for (int s = std::max(g - 12, 0); tallest && s <= std::min(g + 12, count - 1); ++s)
        {
            if (s != g && samples_.at(s).spike && surfaceAt(s) < surfaceAt(g))
                tallest = false;
        }
        if (tallest)
        {
            const QPointF tip(g * SAMPLE_W + SAMPLE_W / 2, surfaceAt(g));
            p.setBrush(DANGER);
            p.setPen(QPen(QColor(60, 60, 60), 1.2));
            p.drawLine(tip, tip + QPointF(0, -14));
            p.setPen(Qt::NoPen);
            p.drawPolygon(QPolygonF { tip + QPointF(0, -14), tip + QPointF(10, -10.5), tip + QPointF(0, -7) });
        }

        // a stretch that bobs is labelled for what it is
        if (sample.moving >= 0 && (g == 0 || samples_.at(g - 1).moving != sample.moving))
        {
            p.setPen(QColor(255, 255, 255, 120));
            p.drawText(QRectF(g * SAMPLE_W - 10, surfaceAt(g) - 22, BAR_W * 6, 14),
                       Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("non-stationary"));
        }
    }

    // the site's own ground: ice, glinting, and grass fires, flickering
    for (const auto& patch : patches_)
    {
        const qreal x0 = patch.start * BAR_W;
        const qreal x1 = (patch.end + 1) * BAR_W;
        if (x1 < cameraX_ - 40.0 || x0 > cameraX_ + width() + 40.0)
            continue;
        if (patch.fire)
        {
            p.setPen(Qt::NoPen);
            for (qreal x = x0 + 2.5; x < x1 - 1.0; x += 5.0)
            {
                const qreal top = groundTop(x);
                if (!std::isfinite(top))
                    continue;
                const qreal flicker = 0.5 + 0.5 * std::sin(14.0 * clockTime_ + 0.7 * x);
                const qreal tall = 10.0 + 9.0 * flicker;
                const qreal sway = 1.5 * std::sin(9.0 * clockTime_ + x);
                p.setBrush(QColor(230, 90, 30, 210));
                p.drawPolygon(QPolygonF { QPointF(x - 3.5, top), QPointF(x + sway, top - tall),
                                          QPointF(x + 3.5, top) });
                p.setBrush(QColor(255, 210, 80, 230));
                p.drawPolygon(QPolygonF { QPointF(x - 1.8, top), QPointF(x + 0.5 * sway, top - 0.55 * tall),
                                          QPointF(x + 1.8, top) });
            }
            // and the smoke over it
            p.setBrush(QColor(90, 85, 80, 70));
            const qreal mid = (x0 + x1) / 2.0;
            const qreal ground = groundTop(mid);
            for (int k = 0; k < 3 && std::isfinite(ground); ++k)
            {
                const qreal age = std::fmod(0.8 * clockTime_ + k / 3.0, 1.0);
                p.drawEllipse(QPointF(mid + 8.0 * age, ground - 24.0 - 40.0 * age), 5.0 + 6.0 * age, 4.0 + 5.0 * age);
            }
        }
        else
        {
            QPolygonF sheet;
            for (qreal x = x0; x <= x1; x += SAMPLE_W)
            {
                const qreal top = groundTop(std::min(x, x1 - 0.5));
                if (std::isfinite(top))
                    sheet << QPointF(x, top - 1.0);
            }
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(200, 230, 255, 200), 4.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPolyline(sheet);
            p.setPen(QPen(QColor(255, 255, 255, 220), 1.0));
            p.drawPolyline(sheet.translated(0.0, -1.5));
            // a glint here and there
            for (qreal x = x0 + 10.0; x < x1 - 4.0; x += 22.0)
            {
                const qreal top = groundTop(x);
                const int alpha = static_cast<int>(255 * std::max(0.0, std::sin(3.0 * clockTime_ + x)));
                if (!std::isfinite(top) || alpha < 20)
                    continue;
                p.setPen(QPen(QColor(255, 255, 255, alpha), 1.0));
                p.drawLine(QPointF(x - 3.0, top - 4.0), QPointF(x + 3.0, top - 4.0));
                p.drawLine(QPointF(x, top - 7.0), QPointF(x, top - 1.0));
            }
        }
    }

    // sampling tubes over their raised pair of half-hours, with a rain cap
    for (int c = firstSlot; c <= lastSlot; ++c)
    {
        if (!bars_.at(c).tube || (c > 0 && bars_.at(c - 1).tube))
            continue;
        const qreal x = c * BAR_W;
        const qreal top = bars_.at(c).top;
        QLinearGradient steel(x, 0, x + 2 * BAR_W, 0);
        steel.setColorAt(0.0, QColor(120, 125, 130));
        steel.setColorAt(0.45, QColor(215, 220, 225));
        steel.setColorAt(1.0, QColor(100, 105, 110));
        p.setPen(QPen(QColor(60, 62, 66), 1));
        p.setBrush(steel);
        p.drawRect(QRectF(x + 4, top + 6, 2 * BAR_W - 8, TUBE_HEIGHT));
        p.drawRect(QRectF(x, top, 2 * BAR_W, 8));
        p.setBrush(QColor(80, 82, 86));
        p.drawRect(QRectF(x + 14, top - 10, 2 * BAR_W - 28, 4));
        p.drawLine(QPointF(x + 18, top - 6), QPointF(x + 18, top));
        p.drawLine(QPointF(x + 2 * BAR_W - 18, top - 6), QPointF(x + 2 * BAR_W - 18, top));
    }

}

void FluxRunner::paintThings(QPainter &p) const
{
    const qreal viewLeft = cameraX_ - 40;
    const qreal viewRight = cameraX_ + width() + 40;

    // good half-hours, spinning
    for (const auto& coin : coins_)
    {
        if (coin.taken || coin.pos.x() < viewLeft || coin.pos.x() > viewRight)
            continue;
        const qreal spin = std::abs(std::cos(clockTime_ * 4.0 + coin.pos.x() * 0.05));
        p.setPen(QPen(COIN_GREEN.darker(160), 1));
        p.setBrush(COIN_GREEN);
        p.drawEllipse(coin.pos, 6.0 * std::max(spin, 0.15), 6.0);
        if (spin > 0.5)
        {
            p.setPen(QPen(Qt::white, 1.4));
            p.drawPolyline(QPolygonF { coin.pos + QPointF(-3, 0), coin.pos + QPointF(-1, 2.5),
                                       coin.pos + QPointF(3, -2.5) });
        }
    }

    // logger blocks
    auto bold = font();
    bold.setBold(true);
    p.setFont(bold);
    for (const auto& block : blocks_)
    {
        const qreal x = block.column * BAR_W;
        if (x + BLOCK < viewLeft || x > viewRight)
            continue;
        const QRectF box(x, block.bottom - BLOCK - block.bump, BLOCK, BLOCK);
        if (block.used)
        {
            p.setPen(QPen(QColor(60, 60, 60), 1.2));
            p.setBrush(QColor(110, 110, 110));
            p.drawRect(box);
            continue;
        }
        p.setPen(QPen(QColor(90, 60, 10), 1.2));
        p.setBrush(AMBER);
        p.drawRect(box);
        p.setBrush(QColor(30, 40, 30));
        p.drawRect(box.adjusted(4, 4, -4, -8));
        p.setPen(COIN_GREEN);
        p.drawText(box.adjusted(4, 3, -4, -8), Qt::AlignCenter, QStringLiteral("?"));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(90, 60, 10));
        p.drawEllipse(QPointF(box.left() + 6, box.bottom() - 4), 1.5, 1.5);
        p.drawEllipse(QPointF(box.right() - 6, box.bottom() - 4), 1.5, 1.5);
    }

    // what came out of them
    for (const auto& pickup : pickups_)
    {
        const QPointF f = pickup.body.feet;
        switch (pickup.kind)
        {
            case Item::Hat:
                p.setPen(QPen(HAT_YELLOW.darker(150), 1));
                p.setBrush(HAT_YELLOW);
                p.drawChord(QRectF(f.x() - 9, f.y() - 16, 18, 22), 0, 180 * 16);
                p.drawRect(QRectF(f.x() - 11, f.y() - 6, 22, 3));
                break;
            case Item::Filter:
            {
                QColor glow = FILTER_ORANGE;
                glow.setAlpha(70);
                p.setPen(Qt::NoPen);
                p.setBrush(glow);
                p.drawEllipse(QPointF(f.x(), f.y() - 9), 13, 13);
                p.setPen(QPen(FILTER_ORANGE.darker(160), 1));
                p.setBrush(FILTER_ORANGE);
                p.drawRect(QRectF(f.x() - 6, f.y() - 10, 12, 10));
                p.setPen(QPen(QColor(70, 70, 70), 1.5));
                for (int i = -1; i <= 1; ++i)
                    p.drawLine(QPointF(f.x() + i * 4, f.y() - 10), QPointF(f.x() + i * 6, f.y() - 18));
                break;
            }
            case Item::Coffee:
                p.setPen(QPen(COFFEE_BROWN.darker(160), 1));
                p.setBrush(QColor(240, 235, 225));
                p.drawRect(QRectF(f.x() - 6, f.y() - 13, 12, 13));
                p.setBrush(Qt::NoBrush);
                p.drawArc(QRectF(f.x() + 3, f.y() - 10, 8, 7), -90 * 16, 180 * 16);
                p.setBrush(COFFEE_BROWN);
                p.drawRect(QRectF(f.x() - 5, f.y() - 12, 10, 3));
                p.setPen(QPen(QColor(255, 255, 255, 140), 1));
                for (int i = 0; i < 2; ++i)
                {
                    const qreal x = f.x() - 2 + i * 4;
                    p.drawLine(QPointF(x, f.y() - 15), QPointF(x + std::sin(clockTime_ * 6 + i) * 1.5, f.y() - 21));
                }
                break;
            case Item::Coin:
                break;
        }
    }

    // pulses from the despiking filter
    for (const auto& shot : shots_)
    {
        const QPointF c(shot.body.feet.x(), shot.body.feet.y() - 4);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 170, 60, 90));
        p.drawEllipse(c, 7, 7);
        p.setBrush(FILTER_ORANGE);
        p.drawEllipse(c, 4, 4);
    }

    // the backup station: a laptop on a tripod
    {
        const qreal x = checkpointColumn_ * BAR_W + BAR_W / 2;
        const qreal ground = groundTop(x);
        if (x > viewLeft && x < viewRight && std::isfinite(ground))
        {
            p.setPen(QPen(QColor(50, 50, 50), 1.5));
            p.drawLine(QPointF(x, ground - 34), QPointF(x - 10, ground));
            p.drawLine(QPointF(x, ground - 34), QPointF(x, ground));
            p.drawLine(QPointF(x, ground - 34), QPointF(x + 10, ground));
            p.setPen(QPen(QColor(40, 40, 40), 1));
            p.setBrush(QColor(170, 170, 175));
            p.drawRect(QRectF(x - 12, ground - 37, 24, 3));
            p.setBrush(checkpointTaken_ ? COIN_GREEN.darker(120) : QColor(40, 45, 55));
            p.drawRect(QRectF(x - 11, ground - 51, 22, 14));
        }
    }

    // the flux tower and its flag
    const qreal tx = towerColumn_ * BAR_W + BAR_W / 2;
    const qreal ground = bars_.at(towerColumn_).top;
    if (tx > viewLeft - 60 && tx < viewRight + 60 && std::isfinite(ground))
    {
        const qreal top = ground - 250.0;
        p.setPen(QPen(QColor(150, 155, 160), 2));
        p.drawLine(QPointF(tx - 5, ground), QPointF(tx - 5, top));
        p.drawLine(QPointF(tx + 5, ground), QPointF(tx + 5, top));
        p.setPen(QPen(QColor(150, 155, 160), 1));
        for (qreal y = ground; y > top; y -= 18)
            p.drawLine(QPointF(tx - 5, y), QPointF(tx + 5, y - 18));
        // a boom, a sonic and an analyser at the top
        p.setPen(QPen(QColor(110, 115, 120), 2));
        p.drawLine(QPointF(tx, top + 10), QPointF(tx + 26, top + 10));
        p.setPen(QPen(QColor(60, 60, 60), 1.5));
        for (int i = -1; i <= 1; ++i)
            p.drawLine(QPointF(tx + 26, top + 10), QPointF(tx + 26 + i * 5, top - 2));
        p.setPen(QPen(QColor(40, 40, 40), 1));
        p.setBrush(QColor(200, 200, 205));
        p.drawRect(QRectF(tx + 14, top + 12, 6, 12));
        // the QC 0 flag
        p.setPen(Qt::NoPen);
        p.setBrush(COIN_GREEN);
        const QPolygonF flag { { tx - 5, flagY_ }, { tx - 37, flagY_ + 10 }, { tx - 5, flagY_ + 20 } };
        p.drawPolygon(flag);
        auto tiny = font();
        tiny.setPointSizeF(std::max(tiny.pointSizeF() - 3.0, 6.0));
        tiny.setBold(true);
        p.setFont(tiny);
        p.setPen(QColor(20, 50, 20));
        p.drawText(QRectF(tx - 30, flagY_ + 3, 24, 14), Qt::AlignCenter, QStringLiteral("QC 0"));
    }

    // the logger hut
    const qreal hx = hutColumn_ * BAR_W;
    const qreal hutGround = bars_.at(hutColumn_).top;
    if (hx > viewLeft - 80 && hx < viewRight + 80 && std::isfinite(hutGround))
    {
        p.setPen(QPen(QColor(70, 45, 25), 1.2));
        p.setBrush(QColor(150, 105, 65));
        p.drawRect(QRectF(hx - 10, hutGround - 44, 56, 44));
        p.setBrush(QColor(120, 40, 30));
        p.drawPolygon(QPolygonF { { hx - 16, hutGround - 44 }, { hx + 18, hutGround - 68 },
                                  { hx + 52, hutGround - 44 } });
        p.setBrush(QColor(50, 35, 25));
        p.drawRect(QRectF(hx + 6, hutGround - 30, 12, 30));
        p.setPen(QPen(QColor(60, 60, 60), 1));
        p.drawLine(QPointF(hx + 40, hutGround - 56), QPointF(hx + 40, hutGround - 80));
    }
}

void FluxRunner::paintFoes(QPainter &p) const
{
    const qreal viewLeft = cameraX_ - 40;
    const qreal viewRight = cameraX_ + width() + 40;
    auto tiny = font();
    tiny.setPointSizeF(std::max(tiny.pointSizeF() - 3.0, 6.0));
    tiny.setBold(true);
    p.setFont(tiny);

    for (const auto& foe : foes_)
    {
        if (!foe.alive || !foe.active)
            continue;
        const QPointF f = foe.body.feet;
        if (f.x() < viewLeft || f.x() > viewRight)
            continue;

        switch (foe.kind)
        {
            case FoeKind::Reviewer:
            {
                if (foe.defeated)
                {
                    // flattened into a single page
                    p.setPen(QPen(QColor(120, 120, 120), 1));
                    p.setBrush(QColor(245, 245, 240));
                    p.drawRect(QRectF(f.x() - 12, f.y() - 5, 24, 5));
                    break;
                }
                const qreal stride = std::sin(clockTime_ * 12.0 + foe.spawn.feet.x());
                p.setPen(QPen(QColor(40, 40, 40), 2));
                p.drawLine(QPointF(f.x() - 4, f.y() - 5), QPointF(f.x() - 4 + 3 * stride, f.y()));
                p.drawLine(QPointF(f.x() + 4, f.y() - 5), QPointF(f.x() + 4 - 3 * stride, f.y()));
                const QRectF sheet(f.x() - 10, f.y() - 24, 20, 20);
                p.setPen(QPen(QColor(110, 110, 110), 1));
                p.setBrush(QColor(248, 248, 244));
                p.drawPolygon(QPolygonF { sheet.topLeft(), sheet.topRight() - QPointF(6, 0),
                                          sheet.topRight() + QPointF(0, 6), sheet.bottomRight(),
                                          sheet.bottomLeft() });
                p.setBrush(QColor(200, 200, 195));
                p.drawPolygon(QPolygonF { sheet.topRight() - QPointF(6, 0), sheet.topRight() + QPointF(-6, 6),
                                          sheet.topRight() + QPointF(0, 6) });
                // red ink, and an expression to match
                p.setPen(QPen(DANGER, 1.2));
                p.drawLine(QPointF(sheet.left() + 3, sheet.top() + 14), QPointF(sheet.right() - 4, sheet.top() + 12));
                p.drawLine(QPointF(sheet.left() + 3, sheet.top() + 17), QPointF(sheet.right() - 7, sheet.top() + 17));
                p.setPen(QPen(QColor(30, 30, 30), 1.4));
                const qreal look = foe.facing * 1.5;
                p.drawLine(QPointF(sheet.left() + 4, sheet.top() + 4), QPointF(sheet.left() + 9, sheet.top() + 6));
                p.drawLine(QPointF(sheet.right() - 4, sheet.top() + 4), QPointF(sheet.right() - 9, sheet.top() + 6));
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(30, 30, 30));
                p.drawEllipse(QPointF(sheet.left() + 7 + look, sheet.top() + 8.5), 1.4, 1.4);
                p.drawEllipse(QPointF(sheet.right() - 7 + look, sheet.top() + 8.5), 1.4, 1.4);
                break;
            }
            case FoeKind::Bubble:
            {
                if (foe.defeated)
                    break;
                const QPointF c(f.x(), f.y() - 9);
                const qreal wobble = 0.08 * std::sin(clockTime_ * 9.0 + foe.spawn.feet.x());
                p.setPen(QPen(QColor(220, 255, 220, 200), 1.2));
                p.setBrush(QColor(170, 220, 170, 110));
                p.drawEllipse(c, 9.5 * (1 + wobble), 9.5 * (1 - wobble));
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(255, 255, 255, 170));
                p.drawEllipse(c + QPointF(-3, -4), 2.5, 1.8);
                p.setPen(QColor(30, 80, 30));
                p.drawText(QRectF(c.x() - 10, c.y() - 5, 20, 12), Qt::AlignCenter, QStringLiteral("CH₄"));
                break;
            }
            case FoeKind::Puff:
            {
                if (foe.defeated)
                    break;
                const QPointF c(f.x(), f.y() - 7);
                p.setPen(QPen(QColor(255, 255, 255, 80), 1));
                for (int i = 0; i < 3; ++i)
                {
                    const qreal y = c.y() - 4 + i * 4;
                    p.drawLine(QPointF(c.x() + 16 + i * 3, y), QPointF(c.x() + 28 + i * 3, y));
                }
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(150, 130, 110, 210));
                p.drawEllipse(c + QPointF(-6, 1), 9, 6);
                p.drawEllipse(c + QPointF(5, 0), 10, 7);
                p.drawEllipse(c + QPointF(0, -4), 8, 6);
                p.setPen(QColor(40, 30, 20));
                p.drawText(QRectF(c.x() - 12, c.y() - 6, 24, 12), Qt::AlignCenter, QStringLiteral("CO₂"));
                break;
            }
            case FoeKind::Cow:
            {
                if (foe.defeated)
                    break;
                const int s = foe.facing;
                const qreal stride = 2.0 * std::sin(6.0 * clockTime_ + foe.spawn.feet.x());
                p.setPen(QPen(QColor(40, 40, 40), 2.2, Qt::SolidLine, Qt::RoundCap));
                for (qreal leg : { -10.0, -4.0, 5.0, 11.0 })
                    p.drawLine(QPointF(f.x() + leg, f.y() - 9), QPointF(f.x() + leg + (leg < 0 ? stride : -stride), f.y()));
                p.setPen(QPen(QColor(60, 60, 60), 1));
                p.setBrush(QColor(245, 245, 240));
                p.drawRoundedRect(QRectF(f.x() - 15, f.y() - 22, 30, 14), 6, 6);
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(35, 35, 35));
                p.drawEllipse(QPointF(f.x() - 6, f.y() - 17), 4.5, 3.5);
                p.drawEllipse(QPointF(f.x() + 5, f.y() - 13), 3.5, 2.5);
                // the head, on the side it walks towards
                const QPointF head(f.x() + s * 17, f.y() - 22);
                p.setPen(QPen(QColor(60, 60, 60), 1));
                p.setBrush(QColor(245, 245, 240));
                p.drawEllipse(head, 6.0, 5.0);
                p.setBrush(QColor(240, 170, 160));
                p.drawEllipse(head + QPointF(s * 4, 2.5), 3.5, 2.5);
                p.setPen(QPen(QColor(220, 210, 180), 1.6));
                p.drawLine(head + QPointF(-3, -4), head + QPointF(-5, -8));
                p.drawLine(head + QPointF(3, -4), head + QPointF(5, -8));
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(20, 20, 20));
                p.drawEllipse(head + QPointF(s * 1.5, -1), 1.1, 1.1);
                break;
            }
            case FoeKind::Tractor:
            {
                if (foe.defeated)
                    break;
                const int s = foe.facing;
                // the chassis, the bonnet in front, the cab behind
                p.setPen(QPen(QColor(30, 50, 25), 1));
                p.setBrush(QColor(60, 140, 50));
                p.drawRect(QRectF(f.x() - 18, f.y() - 20, 36, 10));
                p.drawRect(QRectF(s > 0 ? f.x() : f.x() - 18, f.y() - 27, 18, 8));
                const qreal cabX = s > 0 ? f.x() - 18 : f.x() + 4;
                p.drawRect(QRectF(cabX, f.y() - 36, 14, 17));
                p.setBrush(QColor(190, 220, 235));
                p.drawRect(QRectF(cabX + 3, f.y() - 33, 8, 7));
                // the exhaust, puffing
                const QPointF pipe(f.x() + s * 8, f.y() - 27);
                p.setPen(QPen(QColor(50, 50, 50), 2));
                p.drawLine(pipe, pipe + QPointF(0, -8));
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(110, 110, 110, 120));
                for (int k = 0; k < 2; ++k)
                {
                    const qreal age = std::fmod(2.0 * clockTime_ + 0.5 * k, 1.0);
                    p.drawEllipse(pipe + QPointF(-s * 10.0 * age, -10.0 - 14.0 * age), 2.0 + 3.0 * age, 2.0 + 3.0 * age);
                }
                // the wheels, turning
                const qreal turn = -s * 8.0 * clockTime_;
                for (const auto& wheel : { QPointF(f.x() - s * 9, f.y() - 10), QPointF(f.x() + s * 12, f.y() - 6) })
                {
                    const qreal r = wheel.y() < f.y() - 8 ? 10.0 : 6.0;
                    p.setPen(QPen(QColor(25, 25, 25), 1));
                    p.setBrush(QColor(45, 45, 45));
                    p.drawEllipse(wheel, r, r);
                    p.setBrush(QColor(230, 190, 40));
                    p.drawEllipse(wheel, 0.45 * r, 0.45 * r);
                    p.setPen(QPen(QColor(25, 25, 25), 1.2));
                    p.drawLine(wheel + QPointF(r * std::cos(turn), r * std::sin(turn)),
                               wheel - QPointF(r * std::cos(turn), r * std::sin(turn)));
                }
                break;
            }
            case FoeKind::DustDevil:
            {
                if (foe.defeated)
                    break;
                // a funnel of sand, wide at the top, spinning
                p.setBrush(Qt::NoBrush);
                for (int k = 0; k < 9; ++k)
                {
                    const qreal t = k / 8.0;
                    const qreal y = f.y() - 4.0 - 48.0 * t;
                    const qreal r = 3.0 + 13.0 * t;
                    const qreal sway = 3.0 * std::sin(5.0 * clockTime_ + 0.8 * k);
                    const int start = static_cast<int>(900.0 * clockTime_ + 70.0 * k) % 360;
                    p.setPen(QPen(QColor(210, 180, 130, 110 + 12 * k), 1.6));
                    p.drawArc(QRectF(f.x() + sway - r, y - 0.3 * r, 2 * r, 0.6 * r), start * 16, 250 * 16);
                }
                // grit kicked up at its foot
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(200, 170, 120, 160));
                for (int k = 0; k < 5; ++k)
                {
                    const qreal a = 7.0 * clockTime_ + 1.3 * k;
                    p.drawEllipse(QPointF(f.x() + 9.0 * std::cos(a), f.y() - 3.0 - 2.5 * std::sin(a)), 1.4, 1.4);
                }
                break;
            }
            case FoeKind::Mosquito:
            {
                if (foe.defeated)
                    break;
                const QPointF c(f.x(), f.y() - 5);
                const int s = foe.facing;
                const qreal flap = std::sin(60.0 * clockTime_ + foe.spawn.feet.x());
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(220, 230, 240, 130));
                p.drawEllipse(c + QPointF(-2.5, -4.0 - 1.5 * flap), 4.5, 2.2);
                p.drawEllipse(c + QPointF(2.5, -4.0 + 1.5 * flap), 4.5, 2.2);
                p.setBrush(QColor(40, 35, 30));
                p.drawEllipse(c, 4.0, 2.4);
                p.drawEllipse(c + QPointF(s * 4.0, -0.5), 1.8, 1.8);
                p.setPen(QPen(QColor(40, 35, 30), 0.8));
                p.drawLine(c + QPointF(s * 5.5, 0.0), c + QPointF(s * 9.5, 1.5));
                for (int k = -1; k <= 1; ++k)
                    p.drawLine(c + QPointF(2.0 * k, 1.0), c + QPointF(3.5 * k, 5.0));
                break;
            }
            case FoeKind::Faller:
            {
                const bool rock = site_ == Mountain;
                const QPointF landing = foe.spawn.feet;
                if (foe.timer > 0.0)
                {
                    // the warning: a shadow growing where it will land, and
                    // a trickle of debris
                    const qreal t = 1.0 - foe.timer / FALL_WARNING_S;
                    p.setPen(Qt::NoPen);
                    p.setBrush(QColor(0, 0, 0, static_cast<int>(40 + 80 * t)));
                    p.drawEllipse(landing + QPointF(0.0, -1.0), 4.0 + 8.0 * t, 1.5 + 1.5 * t);
                    p.setBrush(rock ? QColor(150, 145, 140) : QColor(120, 90, 50));
                    for (int k = 0; k < 4; ++k)
                    {
                        const qreal y = std::fmod(260.0 * clockTime_ + 61.0 * k, std::max(landing.y(), 1.0));
                        p.drawEllipse(QPointF(landing.x() + 5.0 * std::sin(2.1 * k), y), 1.3, 1.3);
                    }
                    break;
                }
                p.setOpacity(foe.defeated ? std::clamp(foe.squash / 0.3, 0.0, 1.0) : 1.0);
                const QPointF c(f.x(), f.y() - foe.body.height / 2);
                if (rock)
                {
                    const qreal turn = foe.defeated ? 0.0 : 6.0 * clockTime_;
                    QPolygonF stone;
                    for (int k = 0; k < 7; ++k)
                    {
                        const qreal a = turn + k * 2.0 * PI / 7.0;
                        const qreal r = 8.0 + 2.5 * hash01(k + foe.spawn.feet.x());
                        stone << c + QPointF(r * std::cos(a), 0.85 * r * std::sin(a));
                    }
                    p.setPen(QPen(QColor(70, 70, 72), 1));
                    p.setBrush(QColor(140, 138, 135));
                    p.drawPolygon(stone);
                }
                else
                {
                    p.setPen(QPen(QColor(80, 50, 25), 1));
                    p.setBrush(QColor(150, 100, 50));
                    p.drawEllipse(c, 4.5, 7.5);
                    p.setPen(QPen(QColor(90, 60, 30), 0.8));
                    for (int k = -1; k <= 1; ++k)
                        p.drawLine(c + QPointF(-4.0, 3.5 * k), c + QPointF(4.0, 3.5 * k + 1.5));
                }
                p.setOpacity(1.0);
                break;
            }
        }
    }
}

void FluxRunner::paintEddies(QPainter &p) const
{
    p.setBrush(Qt::NoBrush);
    for (const auto& eddy : eddies_)
    {
        const qreal alpha = std::min(1.0, eddy.age / 0.5) * std::clamp((7.0 - eddy.age) / 0.8, 0.0, 1.0);
        // Qt's angles run anticlockwise, the kick of a positive spin clockwise
        const int turn = static_cast<int>(-eddy.age * 220.0 * eddy.spin) % 360;
        for (int k = 0; k < 3; ++k)
        {
            const qreal r = eddy.radius * (1.0 - 0.33 * k);
            const int start = turn + k * 50;
            p.setPen(QPen(QColor(255, 255, 255, static_cast<int>(110 * alpha)), 1.8 - 0.4 * k));
            p.drawArc(QRectF(eddy.pos.x() - r, eddy.pos.y() - r, 2 * r, 2 * r), start * 16, 240 * 16);

            // an arrowhead on the leading end of each ring, so that which way
            // it turns, and so where it pushes, can be read at a glance
            const qreal lead = (eddy.spin > 0 ? start : start + 240) * PI / 180.0;
            const QPointF tip = eddy.pos + QPointF(r * std::cos(lead), -r * std::sin(lead));
            const QPointF ahead = eddy.spin * QPointF(std::sin(lead), std::cos(lead));
            const QPointF side(-ahead.y(), ahead.x());
            const qreal size = 4.5 - k;
            p.drawLine(tip, tip - size * ahead + 0.7 * size * side);
            p.drawLine(tip, tip - size * ahead - 0.7 * size * side);
        }
    }
}

int FluxRunner::playerFrame() const
{
    if (state_ == State::Dying)
        return Hurt;

    // the run cycle follows the distance covered, so its cadence follows the speed
    auto running = [this]()
    {
        const SpriteSheet& sheet = SpriteSheet::sheet(figure_);
        const int frames = sheet.valid() ? sheet.runFrames() : 3;
        const int step = static_cast<int>(runDistance_ / 20.0);
        if (frames <= 1)
            return step % 2 == 0 ? static_cast<int>(Run1) : static_cast<int>(Stand);
        return Run1 + step % frames;
    };

    if (state_ == State::Cleared)
        return clearStage_ == 0 ? Stand : clearStage_ == 1 ? running() : Cheer;
    if (!player_.grounded && state_ != State::Piping)
        return Jump;
    if (crouching_)
        return Crouch;
    if (std::abs(player_.vel.x()) < 10.0)
        return Stand;
    return running();
}

void FluxRunner::paintPlayer(QPainter &p) const
{
    if (state_ == State::Idle)
        return;
    // inside the hut, or inside a tube
    if ((state_ == State::Cleared && clearStage_ == 2)
        || (state_ == State::Piping && clearStage_ == 1))
        return;
    if (invulnerable_ > 0.0 && std::fmod(clockTime_ * 10.0, 2.0) < 1.0)
        return;

    p.save();
    if (state_ == State::Piping)
    {
        // only what is above the mouth of the tube shows
        const Tube& tube = tubes_.at(tubeIndex_);
        const int column = clearStage_ == 0 ? tube.column : tube.exitColumn;
        p.setClipRect(QRectF(cameraX_ - 100, -1000, width() + 200, bars_.at(column).top + 1000),
                      Qt::IntersectClip);
    }

    const int frame = playerFrame();
    paintFigure(p, player_.feet, frame, facingLeft_, 1.0);

    if (power_ != Power::Small && frame != Hurt)
    {
        const HatSpot spot = HAT_SPOTS[frame];
        const qreal hx = player_.feet.x() + (facingLeft_ ? -spot.dx : spot.dx);
        const qreal top = player_.feet.y() + spot.top;
        const QColor colour = power_ == Power::Hat ? HAT_YELLOW : FILTER_ORANGE;
        p.setPen(QPen(colour.darker(150), 1));
        p.setBrush(colour);
        p.drawChord(QRectF(hx - 11, top - 4, 22, 18), 0, 180 * 16);
        p.drawRect(QRectF(hx - 14, top + 4, 28, 3));
        if (power_ == Power::Filter)
        {
            // the sonic's three arms on top
            p.setPen(QPen(QColor(70, 70, 70), 1.4));
            for (int i = -1; i <= 1; ++i)
                p.drawLine(QPointF(hx + i * 3, top - 4), QPointF(hx + i * 5, top - 11));
        }
    }
    p.restore();
}

void FluxRunner::paintFigure(QPainter &p, const QPointF &feet, int frame, bool left,
                             qreal opacity) const
{
    auto& sheet = SpriteSheet::sheet(figure_);
    if (sheet.valid())
    {
        const qreal dpr = devicePixelRatioF();
        const QPixmap& pixmap = sheet.frame(frame, left, dpr);
        const QPointF at(std::round(feet.x() - 32.0), std::round(feet.y() - 64.0));
        p.save();
        p.setOpacity(opacity);
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        p.drawPixmap(at, pixmap);
        if (coffee_ > 0.0 && state_ != State::Card)
        {
            // coffee: the silhouette cycles through the colours, slower near the end
            QPixmap tint(pixmap.size());
            tint.setDevicePixelRatio(dpr);
            tint.fill(Qt::transparent);
            QPainter tp(&tint);
            tp.drawPixmap(0, 0, pixmap);
            tp.setCompositionMode(QPainter::CompositionMode_SourceIn);
            tp.fillRect(QRectF(0, 0, 64, 64),
                        QColor::fromHsvF(std::fmod(clockTime_ * 1.5, 1.0), 0.8, 1.0));
            tp.end();
            const bool fading = coffee_ < 2.0 && std::fmod(clockTime_ * 4.0, 2.0) < 1.0;
            p.setOpacity(fading ? 0.2 : 0.55);
            p.drawPixmap(at, tint);
        }
        p.restore();
        return;
    }

    // no strip: a figure drawn here, in the same colours and on the same feet
    const qreal x = feet.x();
    const qreal y = feet.y();
    const int s = left ? -1 : 1;
    const qreal h = frame == Crouch ? 44.0 : 60.0;
    const bool runFrame = frame == Run1 || frame == Run2 || frame == Run3;
    const qreal swing = runFrame ? 7.0 * std::sin(runDistance_ / 20.0 * PI) : 0.0;
    const qreal tuck = frame == Jump ? 8.0 : 0.0;

    p.save();
    p.setOpacity(opacity);
    const QColor skin(230, 170, 130);
    const QColor jeans(60, 100, 180);
    const QColor shirt(25, 35, 70);
    const QColor shoes(120, 70, 35);

    p.setPen(QPen(jeans, 5, Qt::SolidLine, Qt::RoundCap));
    const QPointF hip(x, y - 0.45 * h);
    const QPointF footA(x - 4 + swing, y - 3 - tuck);
    const QPointF footB(x + 4 - swing, y - 3 - tuck * 0.5);
    p.drawLine(hip, footA);
    p.drawLine(hip, footB);
    p.setPen(Qt::NoPen);
    p.setBrush(shoes);
    p.drawRect(QRectF(footA.x() - 3 + s, footA.y() - 1, 7, 4));
    p.drawRect(QRectF(footB.x() - 3 + s, footB.y() - 1, 7, 4));

    p.setBrush(shirt);
    p.drawRoundedRect(QRectF(x - 9, y - 0.82 * h, 18, 0.4 * h), 3, 3);
    p.setPen(QPen(skin, 3.5, Qt::SolidLine, Qt::RoundCap));
    const qreal shoulder = y - 0.76 * h;
    const qreal armUp = (frame == Cheer || frame == Hurt) ? -16.0 : 0.0;
    p.drawLine(QPointF(x - 8, shoulder), QPointF(x - 10 - swing * 0.6, shoulder + 14 + armUp));
    p.drawLine(QPointF(x + 8, shoulder), QPointF(x + 10 + swing * 0.6, shoulder + 14 + armUp));

    const QPointF head(x + s, y - 0.9 * h);
    p.setPen(Qt::NoPen);
    p.setBrush(skin);
    p.drawEllipse(head, 7, 7.5);
    p.setBrush(QColor(110, 75, 45));
    p.drawChord(QRectF(head.x() - 7, head.y() - 7.5, 14, 10), 20 * 16, 140 * 16);
    p.setPen(QPen(QColor(30, 30, 30), 1));
    p.drawLine(QPointF(head.x() + s * 1, head.y() - 1), QPointF(head.x() + s * 6, head.y() - 1));
    p.restore();
}

void FluxRunner::paintWeather(QPainter &p) const
{
    // rain, and the puddles on the bars beneath it
    for (const auto& shower : showers_)
    {
        const qreal x0 = shower.start * BAR_W - cameraX_;
        const qreal x1 = (shower.end + 1) * BAR_W - cameraX_;
        if (x1 < 0 || x0 > width())
            continue;
        const qreal from = std::max(x0, 0.0);
        const qreal to = std::min(x1, static_cast<qreal>(width()));
        const qreal span = std::max(to - from, 1.0);
        const int drops = static_cast<int>(span / 6.0);
        if (snowy_)
        {
            // snow: slow, drifting flakes
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(250, 250, 255, 200));
            for (int i = 0; i < drops; ++i)
            {
                const qreal seed = (i * 37 % 97) / 97.0;
                const qreal x = from + std::fmod(i * 13.7 + seed * 50.0
                                                 + 12.0 * std::sin(clockTime_ * 1.3 + i), span);
                const qreal y = std::fmod(seed * PLAY_HEIGHT + clockTime_ * 60.0 * (0.7 + 0.6 * seed),
                                          static_cast<qreal>(PLAY_HEIGHT));
                p.drawEllipse(QPointF(x, y), 1.6, 1.6);
            }
        }
        else
        {
            p.setPen(QPen(QColor(170, 200, 255, 130), 1));
            for (int i = 0; i < drops; ++i)
            {
                const qreal seed = (i * 37 % 97) / 97.0;
                const qreal x = from + std::fmod(i * 13.7 + seed * 50.0, span);
                const qreal y = std::fmod(seed * PLAY_HEIGHT + clockTime_ * 420.0 * (0.8 + 0.4 * seed),
                                          static_cast<qreal>(PLAY_HEIGHT));
                p.drawLine(QPointF(x, y), QPointF(x - 2, y + 9));
            }
        }
        p.setPen(Qt::NoPen);
        p.setBrush(snowy_ ? QColor(250, 250, 255, 170) : QColor(170, 200, 255, 110));
        for (int c = shower.start; c <= shower.end; ++c)
        {
            const qreal top = groundTop(c * BAR_W + BAR_W / 2);
            const qreal x = c * BAR_W - cameraX_;
            if (std::isfinite(top) && x > -BAR_W && x < width())
                p.drawEllipse(QRectF(x + 6, top - 1.5, BAR_W - 12, 3));
        }
    }

    // the stable night: mist over the bars, and the view closing in
    const qreal night = nightAt(cameraX_ + width() / 2.0);
    if (night > 0.05)
    {
        QLinearGradient mist(0, FLOOR - 200, 0, FLOOR);
        mist.setColorAt(0.0, QColor(200, 210, 230, 0));
        mist.setColorAt(1.0, QColor(200, 210, 230, static_cast<int>(70 * night)));
        p.fillRect(QRectF(0, FLOOR - 200, width(), 200), mist);

        const QPointF centre(player_.feet.x() - cameraX_, player_.feet.y() - 30.0);
        QRadialGradient vignette(centre, 360);
        vignette.setColorAt(0.0, QColor(0, 0, 0, 0));
        vignette.setColorAt(0.35, QColor(0, 0, 0, 0));
        vignette.setColorAt(1.0, QColor(0, 0, 0, static_cast<int>(150 * night)));
        p.fillRect(QRectF(0, 0, width(), PLAY_HEIGHT), vignette);
    }

    // how turbulent it is where the player stands
    QString ustar;
    const int zone = zoneAt(player_.feet.x());
    if (zone >= 0)
    {
        const qreal value = 0.25 + 0.65 * zones_.at(zone).strength
                                       * (0.5 + 0.5 * std::abs(gust(zone)) / spec_.gustMax);
        ustar = QStringLiteral("u* %1 m/s").arg(value, 0, 'f', 2);
    }
    else if (nightAt(player_.feet.x()) > 0.7)
    {
        ustar = QStringLiteral("u* < threshold");
    }
    if (!ustar.isEmpty() && state_ != State::Card)
    {
        auto bold = font();
        bold.setBold(true);
        p.setFont(bold);
        p.setPen(zone >= 0 ? AMBER : ICE);
        p.drawText(QRectF(0, 6, width() - 10, 18), Qt::AlignRight | Qt::AlignVCenter, ustar);
    }
}

void FluxRunner::paintHud(QPainter &p) const
{
    const QRectF hud(0, PLAY_HEIGHT, width(), HUD_HEIGHT);
    p.fillRect(hud, HUD_BG);
    p.setPen(QPen(QColor(90, 80, 70), 1));
    p.drawLine(hud.topLeft(), hud.topRight());

    auto hudFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    hudFont.setBold(true);
    hudFont.setPointSize(11);
    p.setFont(hudFont);
    const QRectF text = hud.adjusted(10, 0, -10, 0);

    p.setPen(lives_ <= 1 ? DANGER : AMBER);
    p.drawText(text, Qt::AlignVCenter | Qt::AlignLeft,
               QStringLiteral("♥ ×%1 · L%2").arg(lives_).arg(level_));

    const int total = static_cast<int>(coins_.size()) + blockCoins_;
    const int coverage = qRound(100.0 * coinCount_ / std::max(total, 1));
    p.setPen(AMBER);
    p.drawText(text, Qt::AlignCenter,
               tr("COVERAGE %1%  ✓%2  SCORE %3").arg(coverage).arg(coinCount_).arg(score_));

    const qreal x = player_.feet.x();
    const qreal hour = hourAt(x);
    const int hh = static_cast<int>(hour);
    const int mm = (hour - hh) > 0.25 ? 30 : 0;
    const int left = static_cast<int>(std::ceil(std::max(deadline_, 0.0)));
    p.setPen(deadline_ < 60.0 ? DANGER : AMBER);
    p.drawText(text, Qt::AlignVCenter | Qt::AlignRight,
               tr("DOY %1 %2:%3  DEADLINE %4:%5")
                   .arg(doyAt(x))
                   .arg(hh, 2, 10, QLatin1Char('0'))
                   .arg(mm, 2, 10, QLatin1Char('0'))
                   .arg(left / 60)
                   .arg(left % 60, 2, 10, QLatin1Char('0')));
}

void FluxRunner::paintOverlay(QPainter &p) const
{
    const QRectF play(0, 0, width(), PLAY_HEIGHT);

    // a banner across the top
    if (!banner_.isEmpty() && bannerAge_ < BANNER_S && state_ != State::Card)
    {
        const qreal alpha = std::min(bannerAge_ / 0.2, 1.0) * std::clamp((BANNER_S - bannerAge_) / 0.5, 0.0, 1.0);
        p.fillRect(QRectF(0, 28, width(), 26), QColor(0, 0, 0, static_cast<int>(130 * alpha)));
        auto big = font();
        big.setBold(true);
        big.setPointSizeF(big.pointSizeF() + 2.0);
        p.setFont(big);
        QColor colour = bannerColor_;
        colour.setAlphaF(alpha);
        p.setPen(colour);
        p.drawText(QRectF(0, 28, width(), 26), Qt::AlignCenter, banner_);
    }

    auto title = font();
    title.setBold(true);
    title.setPointSizeF(title.pointSizeF() + 10.0);
    auto line = font();
    line.setPointSizeF(line.pointSizeF() + 1.0);

    switch (state_)
    {
        case State::Card:
        {
            // the level, where and when it is, and what it is like
            p.fillRect(play, QColor(10, 10, 16, 235));
            p.setFont(title);
            p.setPen(AMBER);
            p.drawText(QRectF(0, PLAY_HEIGHT * 0.08, width(), 40), Qt::AlignCenter,
                       tr("LEVEL %1").arg(level_));
            auto name = font();
            name.setBold(true);
            name.setPointSizeF(name.pointSizeF() + 4.0);
            p.setFont(name);
            p.setPen(QColor(240, 240, 240));
            p.drawText(QRectF(0, PLAY_HEIGHT * 0.08 + 42, width(), 28), Qt::AlignCenter, siteName_);
            p.setFont(line);
            p.setPen(QColor(200, 200, 200));
            const int lastDoy = doyAt((LEAD + dataSlots_ - 1) * BAR_W);
            const int hour = startSlot_ / 2;
            QString facts = QStringLiteral("%1 · DOY %2–%3 · %4 · from %5:%6 · %7")
                                .arg(siteCode_).arg(firstDoy_).arg(lastDoy).arg(season_)
                                .arg(hour, 2, 10, QLatin1Char('0'))
                                .arg(startSlot_ % 2 ? 30 : 0, 2, 10, QLatin1Char('0'))
                                .arg(weatherName_);
            p.drawText(QRectF(20, PLAY_HEIGHT * 0.08 + 74, width() - 40, 22), Qt::AlignCenter, facts);
            p.setPen(QColor(230, 230, 230));
            p.drawText(QRectF(30, PLAY_HEIGHT * 0.08 + 102, width() - 60, 44),
                       Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, cardLine_);
            if (!modifierNames_.isEmpty())
            {
                p.setPen(AMBER);
                p.drawText(QRectF(20, PLAY_HEIGHT * 0.08 + 150, width() - 40, 22), Qt::AlignCenter,
                           tr("This week: %1").arg(modifierNames_.join(QStringLiteral(", "))));
            }
            paintFigure(p, QPointF(width() / 2.0 - 34.0, PLAY_HEIGHT * 0.86), Stand, false, 1.0);
            p.setFont(title);
            p.setPen(QColor(230, 230, 230));
            p.drawText(QRectF(width() / 2.0 - 4.0, PLAY_HEIGHT * 0.86 - 44, 120, 40),
                       Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("× %1").arg(lives_));
            break;
        }
        case State::Paused:
            p.fillRect(play, QColor(0, 0, 0, 160));
            p.setFont(title);
            p.setPen(AMBER);
            p.drawText(QRectF(0, PLAY_HEIGHT * 0.3, width(), 40), Qt::AlignCenter, tr("PAUSED"));
            p.setFont(line);
            p.setPen(QColor(230, 230, 230));
            p.drawText(QRectF(20, PLAY_HEIGHT * 0.3 + 46, width() - 40, 24), Qt::AlignCenter, pauseLine_);
            p.drawText(QRectF(20, PLAY_HEIGHT * 0.3 + 74, width() - 40, 24), Qt::AlignCenter,
                       tr("P, Esc or a click to carry on"));
            break;
        case State::Over:
        {
            p.fillRect(play, QColor(0, 0, 0, 120));
            p.setFont(title);
            QString text = tr("GAME OVER");
            QColor colour = DANGER;
            if (cleared_)
            {
                text = tr("DATA SUBMITTED");
                colour = COIN_GREEN;
            }
            else if (timedOut_)
            {
                text = tr("DEADLINE MISSED");
                colour = AMBER;
            }
            p.setPen(colour);
            p.drawText(play, Qt::AlignCenter, text);
            break;
        }
        default:
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////
// FluxRunnerPanel

FluxRunnerPanel::FluxRunnerPanel(QWidget *parent) :
    QWidget(parent),
    loading_(new FakeLoading(this)),
    intro_(makeText()),
    controls_(makeText(tr("← → or A D to run · Space, ↑ or W to jump · ↓ to crouch, or to go "
                          "down a sampling tube · X or Shift to throw the filter · P or Esc "
                          "to pause"))),
    startButton_(makeButton(tr("Start the run"))),
    bar_(makeBar()),
    status_(makeText()),
    game_(new FluxRunner),
    result_(new QWidget),
    rank_(makeText()),
    stats_(makeText()),
    tag_(makeText()),
    again_(makeButton(tr("Play again"))),
    next_(makeButton(tr("Next level")))
{
    connect(startButton_, &QPushButton::clicked, this, &FluxRunnerPanel::launch);
    connect(game_, &FluxRunner::levelCleared, this, &FluxRunnerPanel::showLevelSummary);
    connect(game_, &FluxRunner::runOver, this, &FluxRunnerPanel::showResult);

    auto rankFont = rank_->font();
    rankFont.setPointSize(rankFont.pointSize() + 5);
    rankFont.setBold(true);
    rank_->setFont(rankFont);

    connect(again_, &QPushButton::clicked, this, &FluxRunnerPanel::play);
    connect(next_, &QPushButton::clicked, this, &FluxRunnerPanel::continueRun);
    auto back = makeButton(tr("Back to the fluxes"));
    connect(back, &QPushButton::clicked, this, [this]()
    {
        reset();
        emit finished();
    });

    auto resultLayout = new QVBoxLayout(result_);
    resultLayout->setContentsMargins(0, 0, 0, 0);
    resultLayout->setSpacing(12);
    resultLayout->addWidget(rank_);
    resultLayout->addWidget(stats_);
    resultLayout->addWidget(tag_);
    resultLayout->addLayout(centredRow({ next_, again_, back }));

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    layout->addWidget(makeTitle(QStringLiteral("Super Flux Run")));
    layout->addWidget(intro_);
    layout->addWidget(controls_);
    layout->addLayout(centredRow({ startButton_ }));
    layout->addWidget(bar_);
    layout->addWidget(status_);
    layout->addWidget(game_);
    layout->addWidget(result_);

    reset();
}

void FluxRunnerPanel::reset()
{
    loading_->stop();
    game_->stop();
    game_->setVisible(false);
    result_->setVisible(false);
    bar_->setVisible(false);
    status_->clear();
    status_->setVisible(false);
    intro_->setText(pickFresh(introLines()));
    intro_->setVisible(true);
    controls_->setVisible(true);
    startButton_->setEnabled(true);
    startButton_->setVisible(true);
}

void FluxRunnerPanel::launch()
{
    startButton_->setEnabled(false);
    bar_->setVisible(true);
    status_->setVisible(true);
    loading_->run(bar_, status_, sample(loadingSteps(), 4), 450, [this]()
    {
        play();
    });
}

// the game in view, with the keyboard
static void bringIntoView(QWidget* panel, FluxRunner* game)
{
    QTimer::singleShot(0, panel, [panel, game]()
    {
        for (QWidget* w = panel->parentWidget(); w; w = w->parentWidget())
        {
            if (auto area = qobject_cast<QScrollArea*>(w))
            {
                area->ensureWidgetVisible(game, 0, 0);
                break;
            }
        }
        game->setFocus(Qt::OtherFocusReason);
    });
}

void FluxRunnerPanel::play()
{
    // the Start button goes first, or Space would press it
    startButton_->setVisible(false);
    bar_->setVisible(false);
    status_->setVisible(false);
    result_->setVisible(false);
    game_->setVisible(true);
    // a coin flip for who runs this time, kept for every level of the run
    game_->startRun(between(0, 1));
    bringIntoView(this, game_);
}

void FluxRunnerPanel::continueRun()
{
    result_->setVisible(false);
    bar_->setVisible(true);
    status_->setVisible(true);
    loading_->run(bar_, status_, sample(nextLevelLines(), 2), 300, [this]()
    {
        bar_->setVisible(false);
        status_->setVisible(false);
        game_->setVisible(true);
        game_->nextLevel();
        bringIntoView(this, game_);
    });
}

void FluxRunnerPanel::showLevelSummary(const FluxRunner::Result &result)
{
    const int coverage = qRound(100.0 * result.coins / std::max(result.totalCoins, 1));
    rank_->setText(tr("Level %1 cleared: %2").arg(result.level).arg(result.site.toHtmlEscaped()));
    stats_->setText(QStringLiteral("%1<br>%2")
                        .arg(tr("%1 · coverage %2% (%3 of %4 good half-hours), %5 foes stomped, "
                                "%6 gaps bridged, time %7, lives lost %8.")
                                 .arg(result.siteCode.toHtmlEscaped()).arg(coverage)
                                 .arg(result.coins).arg(result.totalCoins).arg(result.defeated)
                                 .arg(result.gapsBridged).arg(minutes(result.seconds))
                                 .arg(result.livesLost),
                             tr("The run so far: score %1, %2 left, %3 cleared. Rank so far: %4.")
                                 .arg(result.score)
                                 .arg(counted(result.lives, "life", "lives"))
                                 .arg(counted(result.levelsCleared, "level", "levels"))
                                 .arg(runRank(result).title.toHtmlEscaped())));
    QString tag = fillRunTemplate(pickFresh(levelClearedLines()), result, true);
    if (result.flawless)
        tag += QLatin1Char(' ') + fillRunTemplate(pickFresh(flawlessTemplates()), result, true);
    tag_->setText(QStringLiteral("<i>%1</i>").arg(tag.toHtmlEscaped()));

    next_->setVisible(true);
    again_->setVisible(false);
    game_->setVisible(false);
    result_->setVisible(true);
    // away from the buttons, so a Space still held cannot press Next level
    result_->setFocus(Qt::OtherFocusReason);
}

void FluxRunnerPanel::showResult(const FluxRunner::Result &result)
{
    const Rank rank = runRank(result);
    const int coverage = qRound(100.0 * result.runCoins / std::max(result.runTotalCoins, 1));
    rank_->setText(tr("Rank: %1").arg(rank.title.toHtmlEscaped()));
    stats_->setText(QStringLiteral("%1<br>%2")
                        .arg(rank.sentence.toHtmlEscaped(),
                             tr("Score %1: reached level %2, cleared %3; coverage %4% (%5 of %6 "
                                "good half-hours), %7 foes stomped, %8 gaps bridged, time %9.")
                                 .arg(result.score).arg(result.level).arg(result.levelsCleared)
                                 .arg(coverage).arg(result.runCoins).arg(result.runTotalCoins)
                                 .arg(result.runDefeated).arg(result.runGaps)
                                 .arg(minutes(result.runSeconds))));
    tag_->setText(QStringLiteral("<i>%1</i>").arg(pickFresh(resultTags()).toHtmlEscaped()));

    next_->setVisible(false);
    again_->setVisible(true);
    game_->setVisible(false);
    result_->setVisible(true);
    // away from the buttons, so a Space still held cannot press Play again
    result_->setFocus(Qt::OtherFocusReason);
}
