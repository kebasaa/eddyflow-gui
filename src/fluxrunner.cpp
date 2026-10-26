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
const qreal BAR_W = 24.0;
const int TICK_MS = 16;
const qreal STEP_S = 1.0 / 120.0;   // fixed physics step: jumps come out the same at any frame rate
const qreal INF = std::numeric_limits<qreal>::infinity();
const qreal EPS = 0.001;
const qreal PI = 3.14159265358979323846;

// level layout, in columns: a flat lead-in, a week of half-hours, the tower
const int LEAD = 12;
const int DATA = 336;
const int TAIL = 24;
const int TOWER_OFFSET = 4;
const int HUT_OFFSET = 12;
const int START_SLOT = 12;          // the first half-hour is 06:00, so a run starts at sunrise

// feel
const qreal MAX_RUN = 240.0;
const qreal GROUND_ACCEL = 1400.0;
const qreal GROUND_FRICTION = 1800.0;
const qreal TURN_DECEL = 2600.0;
const qreal AIR_ACCEL = 900.0;
const qreal AIR_DRAG = 300.0;
const qreal JUMP_V = 760.0;
const qreal G_RISE = 1900.0;        // while rising with jump held
const qreal G_FALL = 2600.0;
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

// The generator's limits. The jump peaks at 152 px and carries 178 px at a
// full run; everything here stays well inside that, so every level drawn
// can be finished.
const qreal MAX_RISE = 96.0;
const int MAX_PIT = 4;
const int MAX_ZONE_PIT = 3;
const qreal MAX_LANDING_RISE = 48.0;
const int RUN_UP = 3;
const qreal SLOPE = 8.0;

// the boundary layer
const qreal GUST_MAX = 36.0;        // px/s of drift: a headwind costs at most 15 % of the reach
const qreal EDDY_KICK = 480.0;
const qreal THERMAL_LIFT = 600.0;
const qreal THERMAL_MAX_RISE = 420.0;
const int THERMAL_WIDTH = 3;
const qreal RAIN_GRIP = 0.35;
const qreal TUBE_HEIGHT = 40.0;
const int MAX_EDDIES = 4;

// things
const qreal REVIEWER_SPEED = 45.0;
const qreal PUFF_SPEED = 130.0;
const qreal BUBBLE_G = 1400.0;
const qreal SHOT_SPEED = 400.0;
const qreal SHOT_G = 2200.0;
const qreal HAT_SPEED = 80.0;
const qreal COFFEE_SPEED = 100.0;
const qreal COFFEE_S = 8.0;
const qreal HURT_INVULNERABLE_S = 1.5;
const qreal BLOCK = 24.0;
const qreal BLOCK_HEIGHT = 104.0;   // a block's bottom above the ground
const int START_LIVES = 3;
const qreal DEADLINE_S = 300.0;
const qreal CARD_S = 1.6;
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

// The figure strip: square cells side by side, 2x of the game's 64 x 64
// cell, feet on the bottom row. It is rescaled once per device pixel ratio,
// smoothly - it is a drawing, not pixel art at its own size - and then drawn
// 1:1. A strip that is missing or malformed leaves the game to draw its own
// figure; an empty cell borrows its neighbour's frame.
class SpriteSheet
{
public:
    static SpriteSheet& instance()
    {
        static SpriteSheet sheet;
        return sheet;
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
    SpriteSheet()
    {
        const QImage strip(QStringLiteral(":/icons/figure"));
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
        QStringLiteral("Levelling the bar chart so it can be stood on..."),
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
        QStringLiteral("A week of fluxes stands between you and the flux tower. Run across the bars, jump the gaps, and get the data submitted."),
        QStringLiteral("The data are the ground. The missing data are the holes in it. Mind the holes."),
        QStringLiteral("Somewhere at the end of this week sits a flux tower with a flag on it. Go and get it."),
        QStringLiteral("Seven days of half-hourly NEE, one technician, and a deadline. What could go wrong?"),
        QStringLiteral("Every bar is a half-hour. Every gap is a power cut. Every spike is somebody else's problem, until now."),
        QStringLiteral("Reviewer 2 is loose in the time series. Jump on them. It is the only language they understand."),
        QStringLiteral("Collect the good half-hours, dodge the advected CO2, and stay out of the methane."),
        QStringLiteral("The daytime fluxes go down, the night-time fluxes come up, and you go across."),
        QStringLiteral("A run across the carbon balance of one long, gappy week."),
        QStringLiteral("The convective boundary layer is in a mood today. Expect gusts."),
        QStringLiteral("The sampling tubes go somewhere. Press down on one and find out where, after a short time lag."),
        QStringLiteral("Hard hat on. Coffee optional. Coverage mandatory."),
        QStringLiteral("You are the last line of defence between a perfectly good dataset and the -9999s."),
        QStringLiteral("Navigate the fluxes. Literally."),
        QStringLiteral("Uptake is green, release is brown, and the gaps are bottomless."),
        QStringLiteral("The eddies will throw you around. Learn which side lifts and which side drops."),
        QStringLiteral("A thermal will carry you if you hold jump. A gap will not."),
        QStringLiteral("The rain makes the bars slippery. The night makes them dark. The deadline makes it all worse."),
        QStringLiteral("Find the backup station halfway along, or start the whole week again."),
        QStringLiteral("Somewhere in this week is the half-hour that will make your annual sum. Probably."),
        QStringLiteral("Stomp, jump, collect, submit. The scientific method, roughly."),
        QStringLiteral("The flux tower is waiting. So is the funding agency."),
        QStringLiteral("The platforms that bob up and down failed the stationarity test. Ride them anyway."),
        QStringLiteral("Bonk the loggers from below; some of them still have something useful inside."),
        QStringLiteral("A hard hat gives you one more mistake. A despiking filter gives you something to throw."),
        QStringLiteral("The coffee makes you invincible for eight seconds. Science has confirmed this."),
        QStringLiteral("The pits are u*-filtered nights. Nobody comes back from a u*-filtered night."),
        QStringLiteral("Seven days, one tower, three lives, and a lot of bars."),
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
        QStringLiteral("Remember: uptake is down, release is up, and the gaps are everywhere."),
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
        QStringLiteral("RAIN! MIND THE SLIPPERY BARS!"),
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
            QStringLiteral("{time} from the first bar to the hut. The tower crew clocked you."),
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
            QStringLiteral("Master of the Bars"),
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

QString fillRunTemplate(const QString& sentence, const FluxRunner::Result& r)
{
    const int coverage = qRound(100.0 * r.coins / std::max(r.totalCoins, 1));
    return fillFlavour(sentence, {
        { QStringLiteral("coverage"), QStringLiteral("%1%").arg(coverage) },
        { QStringLiteral("coins"), counted(r.coins, "good half-hour", "good half-hours") },
        { QStringLiteral("time"), minutes(r.seconds) },
        { QStringLiteral("stomps"), counted(r.defeated, "foe", "foes") },
        { QStringLiteral("gaps"), counted(r.gapsBridged, "gap", "gaps") },
        { QStringLiteral("lives"), counted(r.livesLost, "life", "lives") },
        { QStringLiteral("doy"), QStringLiteral("DOY %1").arg(r.firstDoy) }
    });
}

Rank runRank(const FluxRunner::Result& r)
{
    const RankTier* tier = &gameOverTier();
    if (r.cleared)
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
    if (r.timedOut)
        sentence += QLatin1Char(' ') + fillRunTemplate(pickFresh(deadlineTemplates()), r);
    else if (r.cleared && (r.livesLost == 0 || r.coins == r.totalCoins))
        sentence += QLatin1Char(' ') + fillRunTemplate(pickFresh(flawlessTemplates()), r);

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
    blockCoins_(0),
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
    nightAnnounced_(false),
    sky_(DAY_SKY),
    bannerColor_(AMBER),
    bannerAge_(BANNER_S),
    lives_(START_LIVES),
    livesLost_(0),
    score_(0),
    coinCount_(0),
    defeated_(0),
    gapsBridged_(0),
    deadline_(DEADLINE_S),
    runTime_(0.0),
    timedOut_(false),
    cleared_(false)
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

void FluxRunner::start()
{
    // every level is checked before it is played; a level that will not
    // pass after twenty tries gives way to one without pits
    bool built = false;
    for (int attempt = 0; attempt < 20 && !built; ++attempt)
        built = generateLevel(true) && verifyLevel();
    if (!built)
        generateLevel(false);
    placeThings();

    stars_.clear();
    for (int i = 0; i < 70; ++i)
        stars_.append(QPointF(uniform(0.0, 1.0), uniform(0.0, 0.55)));

    lives_ = START_LIVES;
    livesLost_ = 0;
    score_ = 0;
    coinCount_ = 0;
    defeated_ = 0;
    gapsBridged_ = 0;
    deadline_ = DEADLINE_S;
    runTime_ = 0.0;
    clockTime_ = 0.0;
    timedOut_ = false;
    cleared_ = false;
    checkpointTaken_ = false;
    respawnColumn_ = 3;
    nextGap_ = 0;
    clearStage_ = 0;
    nightAnnounced_ = false;
    tubeIndex_ = -1;
    flagY_ = topAt(towerColumn_) - 250.0;
    nextEddy_ = 0.0;
    banner_.clear();
    bannerAge_ = BANNER_S;
    pops_.clear();
    eddies_.clear();
    held_ = 0;
    accumulator_ = 0.0;

    cameraX_ = 0.0;
    sky_ = mix(DAY_SKY, NIGHT_SKY, nightAt(LEAD * BAR_W));
    cardLine_ = fillFlavour(pickFresh(cardLines()));
    respawn();

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

////////////////////////////////////////////////////////////////////////////////
// the level

int FluxRunner::columnOf(qreal x) const
{
    return static_cast<int>(std::floor(x / BAR_W));
}

qreal FluxRunner::topAt(int column) const
{
    // the start of the level is a wall; past its end the last bar carries on
    if (column < 0)
        return -INF;
    if (bars_.isEmpty())
        return FLOOR;
    column = std::min(column, columnCount() - 1);
    const Bar& bar = bars_.at(column);
    if (bar.gap)
        return INF;
    qreal top = bar.top;
    if (bar.moving >= 0)
    {
        const auto& run = moving_.at(bar.moving);
        top -= run.amplitude * std::sin(2.0 * PI * clockTime_ / run.period + run.phase);
    }
    return top;
}

qreal FluxRunner::groundTop(qreal x) const
{
    return topAt(columnOf(x));
}

qreal FluxRunner::supportTop(qreal left, qreal right, qreal feet) const
{
    // the highest surface under the body that is not above its knees
    qreal best = INF;
    const int first = columnOf(left + EPS);
    const int last = columnOf(right - EPS);
    for (int c = first; c <= last; ++c)
    {
        const qreal top = topAt(c);
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

qreal FluxRunner::hourAt(qreal x) const
{
    const int t = std::max(columnOf(x) - LEAD, 0);
    return ((t + START_SLOT) % 48) * 0.5;
}

qreal FluxRunner::nightAt(qreal x) const
{
    const qreal elevation = std::sin(PI * (hourAt(x) - 5.5) / 15.0);
    return std::clamp(0.4 - 3.0 * elevation, 0.0, 1.0);
}

int FluxRunner::doyAt(qreal x) const
{
    return firstDoy_ + (std::max(columnOf(x) - LEAD, 0) + START_SLOT) / 48;
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

qreal FluxRunner::gust(int zone) const
{
    // surges and lulls around a mean wind, never a jitter
    const Zone& z = zones_.at(zone);
    const qreal t = clockTime_;
    const qreal swing = 0.55 * std::sin(0.9 * t + z.phase)
                        + 0.30 * std::sin(2.3 * t + 1.7 * z.phase)
                        + 0.15 * z.noise;
    return GUST_MAX * z.strength * std::clamp(0.35 * z.direction + 0.65 * swing, -1.0, 1.0);
}

bool FluxRunner::generateLevel(bool hazards)
{
    bars_.clear();
    zones_.clear();
    tubes_.clear();
    moving_.clear();
    showers_.clear();
    gapEnds_.clear();

    firstDoy_ = between(150, 200);

    // a week of half-hourly NEE: daytime uptake under a wandering cloud
    // cover, night-time respiration, and more noise at night
    QList<qreal> flux;
    qreal cloud = uniform(0.6, 1.0);
    qreal uptake = uniform(15.0, 30.0);
    for (int t = 0; t < DATA; ++t)
    {
        if ((t + START_SLOT) % 48 == 0)
            uptake = uniform(15.0, 30.0);
        const qreal hour = ((t + START_SLOT) % 48) * 0.5;
        cloud = std::clamp(cloud + uniform(-0.06, 0.06), 0.4, 1.0);
        const qreal daylight = std::max(0.0, std::sin(PI * (hour - 5.5) / 15.0));
        const qreal respiration = 4.0 + 1.5 * std::sin(2.0 * PI * (hour - 9.0) / 24.0);
        // Box-Muller
        const qreal u1 = std::max(uniform(0.0, 1.0), 1e-9);
        const qreal gauss = std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * PI * uniform(0.0, 1.0));
        const qreal noise = (daylight > 0.0 ? 1.5 : 2.5) * gauss;
        flux.append(respiration - uptake * daylight * cloud + noise);
    }

    // heights: zero flux at 150 px, 4 px per unit, never steeper than one
    // step-up per bar so that ordinary ground can always be walked
    QList<qreal> height;
    for (int t = 0; t < DATA; ++t)
    {
        qreal h = std::clamp(150.0 + 4.0 * flux.at(t), 32.0, 200.0);
        h = std::round(h / 4.0) * 4.0;
        if (t > 0)
            h = std::clamp(h, height.last() - SLOPE, height.last() + SLOPE);
        height.append(h);
    }

    const int n = LEAD + DATA + TAIL;
    for (int c = 0; c < n; ++c)
    {
        const int t = std::clamp(c - LEAD, 0, DATA - 1);
        bars_.append({ FLOOR - height.at(t), flux.at(t), false, false, false, -1 });
    }

    checkpointColumn_ = LEAD + DATA / 2;
    towerColumn_ = LEAD + DATA + TOWER_OFFSET;
    hutColumn_ = towerColumn_ + HUT_OFFSET;

    // nothing in the first sixteen half-hours, around the backup station or
    // close to the tower
    QList<bool> reserved(n, false);
    for (int c = 0; c < LEAD + 16; ++c)
        reserved[c] = true;
    for (int c = checkpointColumn_ - 10; c <= checkpointColumn_ + 10; ++c)
        reserved[c] = true;
    for (int c = LEAD + DATA - 10; c < n; ++c)
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

    // turbulence: daytime, one zone on each of four to six days (not the
    // day of the backup station), stronger as the week goes on
    QList<int> days { 0, 1, 2, 4, 5, 6 };
    const int zoneCount = between(4, 6);
    while (days.size() > zoneCount)
        days.removeAt(QRandomGenerator::global()->bounded(static_cast<int>(days.size())));
    for (int day : days)
    {
        // from 08:00 or 09:00 into the evening, day by day
        const int start = LEAD + std::max(day * 48 + between(4, 6), 16);
        const int end = std::min(start + between(18, 24), LEAD + DATA - 12);
        if (end - start < 12)
            continue;
        zones_.append({ start, end, 0.5 + 0.5 * day / 6.0, uniform(0.0, 2.0 * PI), 0.0,
                        chance(0.65) ? 1 : -1, -1, false });
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

    if (hazards)
    {
        // missing data: pits, mostly at night (u*-filtered), wider later on
        const int pits = between(14, 20);
        int placed = 0;
        for (int attempt = 0; attempt < 600 && placed < pits; ++attempt)
        {
            const int t = between(16, DATA - 12);
            const qreal hour = ((t + START_SLOT) % 48) * 0.5;
            const bool night = hour < 6.0 || hour > 20.0;
            if (!night && chance(0.6))
                continue;
            const qreal p = t / static_cast<qreal>(DATA);
            int width = (p < 0.5) ? between(2, 3) : between(3, MAX_PIT);
            const int c0 = LEAD + t;
            if (inZone(c0) || inZone(c0 + width - 1))
                width = std::min(width, MAX_ZONE_PIT);
            if (!isFree(c0 - 5, c0 + width - 1 + 5))
                continue;
            // the landing may be at most a little higher than the take-off
            if (bars_.at(c0 - 1).top - bars_.at(c0 + width).top > MAX_LANDING_RISE)
                continue;
            for (int c = c0; c < c0 + width; ++c)
                bars_[c].gap = true;
            reserve(c0, c0 + width - 1);
            gapEnds_.append(c0 + width);
            ++placed;
        }

        // spikes: walls of one or two bars, taller later on, always a
        // jumpable rise above the bar you jump from
        const int walls = between(14, 20);
        placed = 0;
        for (int attempt = 0; attempt < 600 && placed < walls; ++attempt)
        {
            const int t = between(16, DATA - 12);
            const int c0 = LEAD + t;
            const int width = chance(0.7) ? 1 : 2;
            if (!isFree(c0 - 5, c0 + width - 1 + 5))
                continue;
            const qreal p = t / static_cast<qreal>(DATA);
            qreal rise = (p < 0.5) ? uniform(40.0, 64.0) : uniform(64.0, MAX_RISE);
            rise = std::floor(rise / 4.0) * 4.0;
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

    // non-stationary stretches: they bob as one, level with the bar before
    const int runs = between(3, 5);
    for (int attempt = 0; attempt < 300 && moving_.size() < runs; ++attempt)
    {
        const int c0 = LEAD + between(20, DATA - 20);
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
        moving_.append({ c0, c0 + length - 1, 40.0, 3.0, uniform(0.0, 2.0 * PI) });
        reserve(c0, c0 + length - 1);
    }

    // sampling tubes: a pair of raised columns in, another pair out further on
    const int tubeCount = between(3, 4);
    for (int attempt = 0; attempt < 400 && tubes_.size() < tubeCount; ++attempt)
    {
        const int entry = LEAD + between(24, DATA - 80);
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

    // rain showers, out of the turbulence zones
    const int showerCount = between(2, 3);
    for (int attempt = 0; attempt < 200 && showers_.size() < showerCount; ++attempt)
    {
        const int start = LEAD + between(20, DATA - 40);
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
    // how far apart two surfaces can be; bars of one run move together
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
        // RUN_UP solid, level-enough bars ending at column before
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

    for (int c = 1; c < n; ++c)
    {
        if (bars_.at(c).gap)
        {
            if (bars_.at(c - 1).gap)
                continue;
            // a pit starts here
            int end = c;
            while (end < n && bars_.at(end).gap)
                ++end;
            const int width = end - c;
            bool inZone = false;
            for (int g = c; g < end; ++g)
                inZone = inZone || zoned(g);
            if (end >= n || width > (inZone ? MAX_ZONE_PIT : MAX_PIT))
                return false;
            if (lowest(c - 1) - highest(end) > MAX_LANDING_RISE)
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
        if (rise > STEP_UP && (rise > MAX_RISE || !runUp(c - 1)))
            return false;
    }
    return true;
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
        return !bar.gap && !bar.spike && !bar.tube && bar.moving < 0;
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
    QList<int> blockColumns;
    for (int c = LEAD + 6; c < LEAD + DATA - 8; c += between(9, 15))
    {
        if (flat(c, 6) && clearAhead(c, 10) && chance(0.55))
        {
            const int count = between(3, 5);
            for (int i = 0; i < count; ++i)
                coins_.append({ QPointF((c + i) * BAR_W + BAR_W / 2, bars_.at(c).top - 36.0), false });
        }
        else if (flat(c - 1, 6) && clearAhead(c, 6) && blockColumns.size() < 18 && chance(0.6))
        {
            const int group = between(1, 3);
            for (int i = 0; i < group; ++i)
                blockColumns.append(c + i);
        }
    }

    // what is in the blocks: a hat early and one after the backup station,
    // a filter around 40 % and 75 %, a coffee or two, the rest half-hours
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
    nearestTo(LEAD + DATA / 8, Item::Hat);
    nearestTo(checkpointColumn_ + 24, Item::Hat);
    nearestTo(LEAD + DATA * 2 / 5, Item::Filter);
    nearestTo(LEAD + DATA * 3 / 4, Item::Filter);
    nearestTo(LEAD + between(DATA / 5, DATA - 40), Item::Coffee);
    if (chance(0.5))
        nearestTo(LEAD + between(DATA / 3, DATA - 30), Item::Coffee);
    nearestTo(LEAD + between(DATA / 4, DATA * 3 / 4), Item::Hat);
    nearestTo(LEAD + between(DATA / 2, DATA - 20), Item::Hat);
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
    for (int c = LEAD + 20; c < LEAD + DATA - 6; c += between(10, 16))
    {
        if (!flat(c - 2, 6) || !clearAhead(c - 4, 8) || !farFromRespawn(c, 6))
            continue;
        Foe foe { FoeKind::Reviewer,
                  { QPointF(c * BAR_W + BAR_W / 2, bars_.at(c).top), QPointF(), 11.0, 24.0, true },
                  {}, 0.0, 0.0, -1, true, false, false };
        foe.spawn = foe.body;
        foes_.append(foe);
    }

    // ebullition bubbles in about half of the pits
    for (int c = 1; c < n; ++c)
    {
        if (!bars_.at(c).gap || bars_.at(c - 1).gap || !chance(0.5))
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
    int puffs = 0;
    for (int attempt = 0; attempt < 200 && puffs < 10; ++attempt)
    {
        const int c = LEAD + between(30, DATA - 10);
        const qreal hour = ((c - LEAD + START_SLOT) % 48) * 0.5;
        const bool night = hour < 6.0 || hour > 20.0;
        if ((!night && chance(0.75)) || !farFromRespawn(c, 10))
            continue;
        Foe foe { FoeKind::Puff,
                  { QPointF(c * BAR_W, bars_.at(c).top - 41.0), QPointF(), 13.0, 14.0, false },
                  {}, 0.0, 0.0, -1, true, false, false };
        foe.spawn = foe.body;
        foes_.append(foe);
        ++puffs;
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
            if (stateTime_ >= CARD_S)
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
                    finishRun(false, false);
                }
                else
                {
                    cardLine_ = pickFresh(diedInPit_ ? pitDeathLines() : foeDeathLines());
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
                                           topAt(tube.exitColumn) + PLAYER_HEIGHT);
                    const qreal view = width();
                    cameraX_ = std::clamp(std::max(cameraX_, player_.feet.x() - 0.45 * view), 0.0,
                                          std::max(columnCount() * BAR_W - view, 0.0));
                    addPop(QPointF(player_.feet.x(), topAt(tube.exitColumn) - 20.0),
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
                const qreal top = topAt(tube.exitColumn);
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
            const qreal ground = topAt(towerColumn_);
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
                finishRun(true, false);
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
        addPop(QPointF(checkpointColumn_ * BAR_W + BAR_W / 2, topAt(checkpointColumn_) - 70.0),
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
            addPop(QPointF(x, player_.feet.y() - 80.0), pickFresh(rainPops()), ICE);
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
        deadline_ = 0.0;
        finishRun(false, true);
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
        const int from = columnOf(x + dir * body.halfW - dir * EPS);
        const int to = columnOf(nx + dir * body.halfW - dir * EPS);
        qreal resolved = nx;
        qreal feet = body.feet.y();
        for (int c = from + dir; dir > 0 ? c <= to : c >= to; c += dir)
        {
            // bars are solid down to the floor; blocks only where they are
            qreal obstacle = topAt(c);
            for (const auto& block : blocks_)
            {
                if (block.column != c)
                    continue;
                const qreal top = block.bottom - BLOCK;
                if (top < feet - EPS && block.bottom > feet - body.height + EPS)
                    obstacle = std::min(obstacle, top);
            }
            if (obstacle < feet - stepUp - EPS)
            {
                resolved = dir > 0 ? c * BAR_W - body.halfW - EPS
                                   : (c + 1) * BAR_W + body.halfW + EPS;
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
        for (int c = columnOf(left + EPS); c <= columnOf(right - EPS); ++c)
        {
            const qreal top = topAt(c);
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

    // wet bars: less grip to start, turn and stop
    const bool wet = b.grounded && raining(b.feet.x());
    const qreal grip = wet ? RAIN_GRIP : 1.0;
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

    // eddies: a kick along the swirl, up on its rising side
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
        kick *= EDDY_KICK * (0.6 + 0.4 * zones_.at(eddy.zone).strength);
        // never pushed down where there is missing data to fall into
        if (nearGap(columnOf(b.feet.x()), 2))
            kick.setY(std::min(kick.y(), 0.0));
        vx = std::clamp(vx + 0.5 * kick.x(), -1.4 * maxRun, 1.4 * maxRun);
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
    if (nextEddy_ > 0.0 || eddies_.size() >= MAX_EDDIES)
        return;
    nextEddy_ = uniform(1.4, 2.6);
    const qreal viewLeft = cameraX_;
    const qreal viewRight = cameraX_ + width();
    for (int z = 0; z < zones_.size(); ++z)
    {
        const qreal from = std::max(zones_.at(z).start * BAR_W, viewLeft);
        const qreal to = std::min((zones_.at(z).end + 1) * BAR_W, viewRight + 200.0);
        if (to - from < 60.0)
            continue;
        // born upwind
        const qreal x = zones_.at(z).direction > 0 ? from + uniform(0.0, 80.0)
                                                   : to - uniform(0.0, 80.0);
        qreal ground = groundTop(x);
        if (!std::isfinite(ground))
            ground = FLOOR - 150.0;
        eddies_.append({ QPointF(x, ground - uniform(50.0, 130.0)), uniform(15.0, 35.0),
                         chance(0.5) ? 1 : -1, 0.0, 0.0, z });
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
            if (foe.spawn.feet.x() - foe.spawn.halfW > viewRight + 48.0)
                continue;
            foe.active = true;
        }

        Body& b = foe.body;
        switch (foe.kind)
        {
            case FoeKind::Reviewer:
            {
                b.vel.setX(foe.facing * REVIEWER_SPEED);
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
                    if (topAt(columnOf(ahead)) > b.feet.y() + STEP_UP)
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
                        const qreal apex = topAt(rim) - uniform(60.0, 120.0);
                        b.vel.setY(-std::sqrt(2.0 * BUBBLE_G * std::max(rest - apex, 10.0)));
                        foe.timer = uniform(2.2, 3.2);
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
                b.feet.rx() -= PUFF_SPEED * dt;
                // at head height over the ground; over a pit it holds its height
                const qreal ground = groundTop(b.feet.x());
                if (std::isfinite(ground))
                    b.feet.ry() += (ground - 41.0 - b.feet.y()) * std::min(1.0, 3.0 * dt);
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
        score_ += POWER_POINTS;
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
            if (f.feet.y() - f.height > FLOOR)
                continue;
            if (pulse.intersects(QRectF(f.feet.x() - f.halfW, f.feet.y() - f.height,
                                        2 * f.halfW, f.height)))
            {
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
    score_ += COIN_POINTS;
    // no line for every one: that would be noise
    if (coinCount_ % 100 == 0)
    {
        ++lives_;
        addPop(where, pickFresh(oneUpPops()), COIN_GREEN);
    }
    else if (coinCount_ % 10 == 0)
    {
        addPop(where, pickFresh(streakPops()).arg(coinCount_), COIN_GREEN);
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

        // from above, falling: a stomp, and each one before landing is worth more
        const qreal top = b.feet.y() - b.height;
        if (player_.vel.y() > 0.0 && previousFeet <= top + 6.0)
        {
            const int points = CHAIN[std::min(stompChain_, 4)];
            ++stompChain_;
            const QStringList& pool = foe.kind == FoeKind::Reviewer ? reviewerPops()
                                    : foe.kind == FoeKind::Bubble   ? bubblePops()
                                                                    : advectionPops();
            defeat(foe, points, pickFresh(pool));
            player_.vel.setY(-((held_ & KeyJump) ? STOMP_HELD_V : STOMP_V));
            player_.grounded = false;
            continue;
        }

        if (invulnerable_ <= 0.0)
        {
            hurt();
            if (state_ != State::Running)
                return;
        }
    }
}

void FluxRunner::defeat(Foe &foe, int points, const QString &text)
{
    foe.defeated = true;
    foe.squash = foe.kind == FoeKind::Reviewer ? 0.4 : 0.0;
    ++defeated_;
    score_ += points;
    const Body& b = foe.body;
    addPop(QPointF(b.feet.x(), b.feet.y() - b.height - 6.0),
           QStringLiteral("%1 +%2").arg(text).arg(points), AMBER);
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
    if (topAt(columnOf(start.x())) < start.y())
        return;
    shots_.append({ { start, QPointF(dir * SHOT_SPEED + 0.5 * player_.vel.x(), 0.0),
                      4.0, 8.0, false }, 0.0 });
    fireCooldown_ = 0.25;
}

void FluxRunner::hurt()
{
    if (invulnerable_ > 0.0 || coffee_ > 0.0)
        return;
    if (power_ != Power::Small)
    {
        power_ = Power::Small;
        invulnerable_ = HURT_INVULNERABLE_S;
        addPop(QPointF(player_.feet.x(), player_.feet.y() - 70.0), pickFresh(hurtPops()), DANGER);
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
    player_ = { QPointF(c * BAR_W + BAR_W / 2, topAt(c)), QPointF(), PLAYER_HALF_W,
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
    const qreal ground = topAt(towerColumn_);
    const qreal above = ground - player_.feet.y();
    const int bonus = above > 200.0 ? 5000 : above > 150.0 ? 2000 : above > 100.0 ? 800
                    : above > 50.0 ? 400 : 100;
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

void FluxRunner::finishRun(bool cleared, bool timedOut)
{
    cleared_ = cleared;
    timedOut_ = timedOut;
    if (cleared)
        score_ += 10 * static_cast<int>(std::ceil(std::max(deadline_, 0.0)));
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

FluxRunner::Result FluxRunner::result() const
{
    Result r;
    r.score = score_;
    r.coins = coinCount_;
    r.totalCoins = static_cast<int>(coins_.size()) + blockCoins_;
    r.defeated = defeated_;
    r.totalFoes = static_cast<int>(foes_.size());
    r.gapsBridged = gapsBridged_;
    r.seconds = runTime_;
    r.livesLost = livesLost_;
    r.cleared = cleared_;
    r.timedOut = timedOut_;
    r.firstDoy = firstDoy_;

    // a rating rather than the score, since every level is different
    const qreal coverage = std::min(1.0, r.coins / static_cast<qreal>(std::max(r.totalCoins, 1)));
    const qreal beaten = r.defeated / static_cast<qreal>(std::max(r.totalFoes, 1));
    const qreal pace = r.cleared ? std::clamp((150.0 - r.seconds) / 100.0, 0.0, 1.0) : 0.0;
    const qreal kept = std::clamp((3.0 - r.livesLost) / 3.0, 0.0, 1.0);
    r.rating = qRound(100.0 * (0.45 * coverage + 0.20 * beaten + 0.20 * pace + 0.15 * kept));
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
                        && std::abs(player_.feet.y() - topAt(tubes_.at(i).column)) < 1.0)
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

    // the world, at a whole-pixel offset so nothing shimmers
    p.save();
    p.translate(-std::round(cameraX_), 0.0);
    paintAtmosphereBack(p);
    paintBars(p);
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
    gradient.setColorAt(0.0, sky_.darker(115));
    gradient.setColorAt(1.0, sky_.lighter(125));
    p.fillRect(play, gradient);

    const qreal centre = cameraX_ + width() / 2.0;
    const qreal night = nightAt(centre);
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

    // far hills
    QPainterPath hills;
    const qreal shift = cameraX_ * 0.2;
    hills.moveTo(0, PLAY_HEIGHT);
    for (qreal x = 0; x <= w + 8; x += 8)
    {
        const qreal wx = x + shift;
        hills.lineTo(x, PLAY_HEIGHT - 150 - 18 * std::sin(wx * 0.004) - 10 * std::sin(wx * 0.011 + 1.3));
    }
    hills.lineTo(w, PLAY_HEIGHT);
    hills.closeSubpath();
    p.fillPath(hills, mix(QColor(90, 130, 110), QColor(20, 28, 45), night));

    // clouds, by day
    if (night < 0.95)
    {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, static_cast<int>(170 * (1.0 - night))));
        const qreal span = w + 240;
        for (int i = 0; i < 6; ++i)
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

void FluxRunner::paintAtmosphereBack(QPainter &p) const
{
    const qreal viewLeft = cameraX_;
    const qreal viewRight = cameraX_ + width();
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
        const qreal length = 10.0 + 16.0 * std::abs(g) / GUST_MAX;
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
            qreal ground = topAt(zone.thermal);
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
        qreal ground = topAt(zone.start);
        if (!std::isfinite(ground))
            continue;
        const qreal poleTop = ground - 46.0;
        p.setPen(QPen(QColor(90, 90, 90), 2));
        p.drawLine(QPointF(wx, ground), QPointF(wx, poleTop));
        const qreal strength = std::abs(g) / GUST_MAX;
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

void FluxRunner::paintBars(QPainter &p) const
{
    const int first = std::max(columnOf(cameraX_) - 1, 0);
    const int last = std::min(columnOf(cameraX_ + width()) + 1, columnCount() - 1);

    auto small = font();
    small.setPointSizeF(std::max(small.pointSizeF() - 2.0, 7.0));
    p.setFont(small);

    for (int c = first; c <= last; ++c)
    {
        const Bar& bar = bars_.at(c);
        const qreal x = c * BAR_W;
        if (bar.gap)
        {
            // a ghost of what is missing
            const qreal ghost = FLOOR - std::clamp(150.0 + 4.0 * bar.flux, 32.0, 200.0);
            p.setPen(QPen(QColor(230, 50, 40, 90), 1, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRect(QRectF(x + 2, ghost, BAR_W - 4, FLOOR - ghost));
            if (c > 0 && !bars_.at(c - 1).gap)
            {
                p.setPen(QColor(230, 50, 40, 160));
                p.drawText(QRectF(x, FLOOR - 18, BAR_W * 3, 16), Qt::AlignLeft | Qt::AlignVCenter,
                           QStringLiteral("-9999"));
            }
            continue;
        }

        const qreal top = topAt(c);
        QColor fill = bar.spike ? SPIKE_RED : (bar.flux < 0 ? UPTAKE : RELEASE);
        if (bar.moving >= 0)
            fill = fill.lighter(112);
        QLinearGradient gradient(0, top, 0, FLOOR);
        gradient.setColorAt(0.0, fill);
        gradient.setColorAt(1.0, fill.darker(140));
        p.setPen(QPen(fill.darker(160), 1));
        p.setBrush(gradient);
        p.drawRect(QRectF(x + 0.5, top, BAR_W - 1, FLOOR - top + 1));
        p.fillRect(QRectF(x + 1, top, BAR_W - 2, 3), fill.lighter(135));

        if (bar.spike)
        {
            p.setPen(QPen(QColor(120, 20, 15, 120), 1));
            for (qreal y = top + 8; y < FLOOR; y += 10)
                p.drawLine(QPointF(x + 2, y + 6), QPointF(x + BAR_W - 2, y));
            // an outlier gets a flag
            if (c == 0 || !bars_.at(c - 1).spike)
            {
                p.setPen(QPen(QColor(60, 60, 60), 1.2));
                p.drawLine(QPointF(x + 4, top), QPointF(x + 4, top - 14));
                p.setPen(Qt::NoPen);
                p.setBrush(DANGER);
                p.drawPolygon(QPolygonF { { x + 4, top - 14 }, { x + 14, top - 10.5 }, { x + 4, top - 7 } });
            }
        }

        if (bar.moving >= 0 && (c == 0 || bars_.at(c - 1).moving != bar.moving))
        {
            p.setPen(QColor(255, 255, 255, 110));
            p.drawText(QRectF(x - 20, top - 20, BAR_W * 6, 14), Qt::AlignLeft | Qt::AlignVCenter,
                       QStringLiteral("non-stationary"));
        }
    }

    // sampling tubes over their raised pair of columns, with a rain cap
    for (int c = first; c <= last; ++c)
    {
        if (!bars_.at(c).tube || (c > 0 && bars_.at(c - 1).tube))
            continue;
        const qreal x = c * BAR_W;
        const qreal top = topAt(c);
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

    // the zero line and the days
    const qreal zero = FLOOR - 150.0;
    p.setPen(QPen(QColor(255, 255, 255, 70), 1, Qt::DashLine));
    p.drawLine(QPointF(cameraX_, zero), QPointF(cameraX_ + width(), zero));
    p.setPen(QColor(255, 255, 255, 150));
    p.drawText(QRectF(cameraX_ + 4, zero - 15, 30, 14), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("0"));
    for (int c = first; c <= last; ++c)
    {
        if (c >= LEAD && (c - LEAD + START_SLOT) % 48 == 0)
        {
            p.drawText(QRectF(c * BAR_W + 3, FLOOR - 34, 90, 14), Qt::AlignLeft | Qt::AlignVCenter,
                       QStringLiteral("DOY %1").arg(firstDoy_ + (c - LEAD + START_SLOT) / 48));
        }
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
        const qreal ground = topAt(checkpointColumn_);
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
    const qreal ground = topAt(towerColumn_);
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
    const qreal hutGround = topAt(hutColumn_);
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
        }
    }
}

void FluxRunner::paintEddies(QPainter &p) const
{
    p.setBrush(Qt::NoBrush);
    for (const auto& eddy : eddies_)
    {
        const qreal alpha = std::min(1.0, eddy.age / 0.5) * std::clamp((7.0 - eddy.age) / 0.8, 0.0, 1.0);
        const int turn = static_cast<int>(eddy.age * 220.0 * eddy.spin) % 360;
        for (int k = 0; k < 3; ++k)
        {
            const qreal r = eddy.radius * (1.0 - 0.33 * k);
            p.setPen(QPen(QColor(255, 255, 255, static_cast<int>(110 * alpha)), 1.8 - 0.4 * k));
            p.drawArc(QRectF(eddy.pos.x() - r, eddy.pos.y() - r, 2 * r, 2 * r),
                      (turn + k * 50) * 16, 240 * 16);
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
        const int frames = SpriteSheet::instance().valid() ? SpriteSheet::instance().runFrames() : 3;
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
        p.setClipRect(QRectF(cameraX_ - 100, -1000, width() + 200, topAt(column) + 1000),
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
    auto& sheet = SpriteSheet::instance();
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
        p.setPen(QPen(QColor(170, 200, 255, 130), 1));
        const int drops = static_cast<int>(span / 6.0);
        for (int i = 0; i < drops; ++i)
        {
            const qreal seed = (i * 37 % 97) / 97.0;
            const qreal x = from + std::fmod(i * 13.7 + seed * 50.0, span);
            const qreal y = std::fmod(seed * PLAY_HEIGHT + clockTime_ * 420.0 * (0.8 + 0.4 * seed),
                                      static_cast<qreal>(PLAY_HEIGHT));
            p.drawLine(QPointF(x, y), QPointF(x - 2, y + 9));
        }
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(170, 200, 255, 110));
        for (int c = shower.start; c <= shower.end; ++c)
        {
            const qreal top = topAt(c);
            const qreal x = c * BAR_W - cameraX_;
            if (std::isfinite(top) && x > -BAR_W && x < width())
                p.drawEllipse(QRectF(x + 4, top - 1.5, BAR_W - 8, 3));
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
                                       * (0.5 + 0.5 * std::abs(gust(zone)) / GUST_MAX);
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
    p.drawText(text, Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("♥ ×%1").arg(lives_));

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
            p.fillRect(play, QColor(10, 10, 16, 235));
            p.setFont(title);
            p.setPen(AMBER);
            p.drawText(QRectF(0, PLAY_HEIGHT * 0.18, width(), 40), Qt::AlignCenter,
                       QStringLiteral("DOY %1–%2").arg(firstDoy_)
                           .arg(firstDoy_ + (DATA - 1 + START_SLOT) / 48));
            p.setFont(line);
            p.setPen(QColor(230, 230, 230));
            p.drawText(QRectF(20, PLAY_HEIGHT * 0.18 + 46, width() - 40, 44),
                       Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, cardLine_);
            paintFigure(p, QPointF(width() / 2.0 - 34.0, PLAY_HEIGHT * 0.72), Stand, false, 1.0);
            p.setFont(title);
            p.setPen(QColor(230, 230, 230));
            p.drawText(QRectF(width() / 2.0 - 4.0, PLAY_HEIGHT * 0.72 - 44, 120, 40),
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
    tag_(makeText())
{
    connect(startButton_, &QPushButton::clicked, this, &FluxRunnerPanel::launch);
    connect(game_, &FluxRunner::runOver, this, &FluxRunnerPanel::showResult);

    auto rankFont = rank_->font();
    rankFont.setPointSize(rankFont.pointSize() + 5);
    rankFont.setBold(true);
    rank_->setFont(rankFont);

    auto again = makeButton(tr("Play again"));
    connect(again, &QPushButton::clicked, this, &FluxRunnerPanel::play);
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
    resultLayout->addLayout(centredRow({ again, back }));

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

void FluxRunnerPanel::play()
{
    // the Start button goes first, or Space would press it
    startButton_->setVisible(false);
    bar_->setVisible(false);
    status_->setVisible(false);
    result_->setVisible(false);
    game_->setVisible(true);
    game_->start();

    // the whole game in view, and the keyboard with it
    QTimer::singleShot(0, this, [this]()
    {
        for (QWidget* w = parentWidget(); w; w = w->parentWidget())
        {
            if (auto area = qobject_cast<QScrollArea*>(w))
            {
                area->ensureWidgetVisible(game_, 0, 0);
                break;
            }
        }
        game_->setFocus(Qt::OtherFocusReason);
    });
}

void FluxRunnerPanel::showResult(const FluxRunner::Result &result)
{
    const Rank rank = runRank(result);
    const int coverage = qRound(100.0 * result.coins / std::max(result.totalCoins, 1));
    rank_->setText(tr("Rank: %1").arg(rank.title.toHtmlEscaped()));
    stats_->setText(QStringLiteral("%1<br>%2")
                        .arg(rank.sentence.toHtmlEscaped(),
                             tr("Score %1: coverage %2% (%3 of %4 good half-hours), %5 foes "
                                "stomped, %6 gaps bridged, time %7, lives lost %8.")
                                 .arg(result.score).arg(coverage).arg(result.coins)
                                 .arg(result.totalCoins).arg(result.defeated)
                                 .arg(result.gapsBridged).arg(minutes(result.seconds))
                                 .arg(result.livesLost)));
    tag_->setText(QStringLiteral("<i>%1</i>").arg(pickFresh(resultTags()).toHtmlEscaped()));

    game_->setVisible(false);
    result_->setVisible(true);
    // away from the buttons, so a Space still held cannot press Play again
    result_->setFocus(Qt::OtherFocusReason);
}
