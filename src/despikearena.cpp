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
const qreal COUNTDOWN_S = 4.0;      // long enough to read the legend
const qreal ROUND_S = 40.0;
const qreal POP_S = 0.7;
const qreal FLASH_S = 0.25;

// demons: they rise faster and escape sooner as the round goes on
const qreal HIT_RADIUS = 18.0;
const qreal HEAD_RADIUS = 9.0;
const int ESCAPE_DAMAGE = 8;
const int RIME_ESCAPE_DAMAGE = 14;  // an iced demon hurts more on the way out
const int RIME_POINTS = 2;

// ammo: a small magazine, refilled by grabbing downward spikes
const int START_AMMO = 15;
const int MAX_AMMO = 30;
const int LOW_AMMO = 7;
const int AMMO_PICKUP = 10;
const qreal AMMO_LIFETIME_S = 4.0;
const qreal AMMO_GRAB_RADIUS = 18.0;

// valid data: stars on the series that must not be shot
const qreal STAR_RADIUS = 6.0;
const qreal STAR_HIT_RADIUS = 11.0;
const qreal STAR_LIFETIME_S = 4.5;

// drop-outs: gaps in the series, filled in a third at a time
const qreal GAP_LIFETIME_S = 4.5;
const int GAP_DAMAGE = 16;
const int GAP_POINTS = 3;
const int MAX_GAPS = 2;
const qreal GAP_SPACING = 150.0;

// supplies: one at a time, below the series, free to grab
const qreal POWER_LIFETIME_S = 3.5;
const qreal POWER_HALF = 13.0;
const qreal POWER_GRAB_RADIUS = 20.0;
const int BERSERK_HEALTH = 10;
const int GAP_FILL_HEALTH = 3;
const int ROCKETS_PER_PICKUP = 4;
const int MAX_ROCKETS = 8;
const qreal ROCKET_RADIUS = 70.0;
const qreal BERSERK_REACH = 1.8;

// the ultimate charges from kills, never from a pickup
const qreal BFG_RADIUS = 190.0;
const qreal BFG_PER_SPIKE = 0.06;
const qreal BFG_PER_RIME = 0.10;
const qreal BFG_PER_GAP = 0.12;
const qreal BFG_NAG_S = 0.5;
const qreal BLAST_S = 0.45;

const int START_HEALTH = 100;
const qreal PI = 3.14159265358979323846;

const QColor AMBER(232, 163, 23);
const QColor DANGER(230, 50, 40);
const QColor AMMO_GREEN(150, 210, 70);
const QColor STAR_BLUE(150, 220, 255);
const QColor ICE_BLUE(190, 225, 245);
const QColor QUAD_VIOLET(170, 110, 255);
const QColor BERSERK_RED(235, 70, 50);
const QColor SLOW_BLUE(120, 190, 255);
const QColor HEAL_GREEN(120, 220, 140);
const QColor GOD_WHITE(240, 240, 230);
const QColor ROCKET_ORANGE(255, 150, 40);
const QColor NAG_GREY(150, 150, 150);

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

// a drop-out filled in completely
const QStringList& gapPops()
{
    static const QStringList list {
        QStringLiteral("GAP FILLED!"),
        QStringLiteral("MDS INTERPOLATED!"),
        QStringLiteral("NO MORE −9999!"),
        QStringLiteral("RECORD CONTINUOUS!"),
        QStringLiteral("MARGINAL DISTRIBUTION SAMPLED!"),
        QStringLiteral("LOOK-UP TABLE APPLIED!"),
        QStringLiteral("HALF-HOUR RESTORED!"),
        QStringLiteral("COVERAGE UP!"),
        QStringLiteral("LINEARLY INTERPOLATED!"),
        QStringLiteral("GAP-FREE!"),
        QStringLiteral("THE ANNUAL SUM THANKS YOU!"),
        QStringLiteral("FILLED, NOT FAKED!"),
        QStringLiteral("NEURAL NETWORK'D!"),
        QStringLiteral("RECOVERED FROM THE LOGGER!"),
        QStringLiteral("BACKUP CARD FOUND!"),
        QStringLiteral("CONTINUITY!"),
        QStringLiteral("FLAG 0 AT LAST!"),
        QStringLiteral("NO MISSING VALUES!"),
        QStringLiteral("SEAMLESS!"),
        QStringLiteral("THE BUDGET CLOSES!"),
        QStringLiteral("COPIED FROM THE BACKUP SYSTEM!"),
        QStringLiteral("SPLINED!"),
        QStringLiteral("THE −9999 IS DEAD!"),
        QStringLiteral("GAP CLOSED!"),
        QStringLiteral("MEAN DIURNAL COURSE'D!"),
        QStringLiteral("RECONSTRUCTED!"),
        QStringLiteral("PATCHED!"),
        QStringLiteral("DOWNLOADED AT LAST!"),
        QStringLiteral("IMPUTED!"),
        QStringLiteral("THE MODEM CALLED BACK!")
    };
    return list;
}

// a drop-out that got away
const QStringList& gapEscapePops()
{
    static const QStringList list {
        QStringLiteral("GAP IN THE RECORD!"),
        QStringLiteral("THE HALF-HOUR IS LOST!"),
        QStringLiteral("−9999 FOREVER!"),
        QStringLiteral("COVERAGE DOWN!"),
        QStringLiteral("LOGGER FULL!"),
        QStringLiteral("THE CARD WAS CORRUPT!"),
        QStringLiteral("POWER CUT!"),
        QStringLiteral("NaN!"),
        QStringLiteral("THE BATTERY DIED!"),
        QStringLiteral("SOLAR PANEL SNOWED UNDER!"),
        QStringLiteral("THE MODEM HUNG UP!"),
        QStringLiteral("MICE ATE THE CABLE!"),
        QStringLiteral("FIRMWARE CRASH!"),
        QStringLiteral("NO BACKUP!"),
        QStringLiteral("THE ANNUAL SUM WOBBLES!"),
        QStringLiteral("GAP-FILLER OVERTIME!"),
        QStringLiteral("THAT'S A HOLE IN JULY!"),
        QStringLiteral("−9999 ESCAPED!"),
        QStringLiteral("LOST TO THE VOID!"),
        QStringLiteral("UNFILLABLE!"),
        QStringLiteral("SENSOR OFFLINE!"),
        QStringLiteral("THE SD CARD IS BLANK!"),
        QStringLiteral("TOO LONG TO FILL!"),
        QStringLiteral("THE DROP-OUT WINS!"),
        QStringLiteral("THE HUT FLOODED!"),
        QStringLiteral("COWS UNPLUGGED THE LOGGER!"),
        QStringLiteral("THE RECORD HAS A HOLE!"),
        QStringLiteral("REVIEWER 2 WILL ASK ABOUT THIS!"),
        QStringLiteral("THE PUMP STOPPED!"),
        QStringLiteral("CLOCK DRIFT! DATA GONE!")
    };
    return list;
}

// the first hit on an iced demon: the shell goes, the demon stays
const QStringList& rimePops()
{
    static const QStringList list {
        QStringLiteral("RIME CRACKED!"),
        QStringLiteral("DE-ICER ON!"),
        QStringLiteral("HEATER ENGAGED!"),
        QStringLiteral("ICE SHATTERED!"),
        QStringLiteral("THAWING!"),
        QStringLiteral("TRANSDUCERS DEFROSTED!"),
        QStringLiteral("THE SONIC CAN HEAR AGAIN!"),
        QStringLiteral("HOAR FROST GONE!"),
        QStringLiteral("ONE MORE!"),
        QStringLiteral("IT'S STILL IN THERE!"),
        QStringLiteral("SHELL BROKEN!"),
        QStringLiteral("SUBLIMATED!"),
        QStringLiteral("HIT IT AGAIN!"),
        QStringLiteral("DEFROST CYCLE!"),
        QStringLiteral("WINTER IS LEAVING!"),
        QStringLiteral("ICE OFF THE PATH!"),
        QStringLiteral("PATH LENGTH RESTORED!"),
        QStringLiteral("MELTING…"),
        QStringLiteral("CRACK!"),
        QStringLiteral("NOT DONE YET!"),
        QStringLiteral("FROZEN NO MORE!"),
        QStringLiteral("HEATED ARMS ON!"),
        QStringLiteral("ICE FLAG CLEARED!"),
        QStringLiteral("THE RIME GIVES WAY!"),
        QStringLiteral("SNOW CAP REMOVED!"),
        QStringLiteral("BRRR… CRACK!"),
        QStringLiteral("STILL ANGRY!"),
        QStringLiteral("NAKED DEMON!"),
        QStringLiteral("ARMOUR DOWN!"),
        QStringLiteral("ONCE MORE, WITH FEELING!")
    };
    return list;
}

// any supply grabbed; %1 is its name
const QStringList& powerUpPops()
{
    static const QStringList list {
        QStringLiteral("%1!"),
        QStringLiteral("%1 ENGAGED!"),
        QStringLiteral("%1 ONLINE!"),
        QStringLiteral("QUALITY CONTROL: %1!"),
        QStringLiteral("+%1!"),
        QStringLiteral("%1 ACQUIRED!"),
        QStringLiteral("%1, FRESH FROM THE HUT!"),
        QStringLiteral("%1 INSTALLED!"),
        QStringLiteral("%1: FIRMWARE OK!"),
        QStringLiteral("%1 DEPLOYED!"),
        QStringLiteral("%1 IN THE FIELD KIT!"),
        QStringLiteral("%1, CALIBRATED!"),
        QStringLiteral("%1 ARMED!"),
        QStringLiteral("%1: THE TECHNICIAN APPROVES!"),
        QStringLiteral("%1 ACTIVATED!"),
        QStringLiteral("%1, JUST LIKE THE MANUAL SAID!"),
        QStringLiteral("GOT %1!"),
        QStringLiteral("%1 UNLOCKED!"),
        QStringLiteral("%1 POWERED UP!"),
        QStringLiteral("%1, NOW WITH ERROR BARS!"),
        QStringLiteral("%1 (WARRANTY VOID)!"),
        QStringLiteral("%1, PAID FOR BY THE GRANT!"),
        QStringLiteral("%1, NO PAPERWORK!"),
        QStringLiteral("%1 OUT OF STANDBY!"),
        QStringLiteral("%1 READY!"),
        QStringLiteral("%1: SPAN AND ZERO OK!"),
        QStringLiteral("%1, PEER REVIEWED!"),
        QStringLiteral("%1 AT 10 Hz!"),
        QStringLiteral("%1 LOADED!"),
        QStringLiteral("BEHOLD: %1!")
    };
    return list;
}

// a rocket going off
const QStringList& rocketPops()
{
    static const QStringList list {
        QStringLiteral("DETRENDED!"),
        QStringLiteral("LINEAR DETREND!"),
        QStringLiteral("KABOOM!"),
        QStringLiteral("SPLASH DESPIKE!"),
        QStringLiteral("AREA FLAGGED!"),
        QStringLiteral("BLOCK AVERAGED!"),
        QStringLiteral("TREND REMOVED!"),
        QStringLiteral("BOOM: HIGH-PASS!"),
        QStringLiteral("ROCKET-PASS FILTER!"),
        QStringLiteral("SPLASH DAMAGE!"),
        QStringLiteral("THE WHOLE NEIGHBOURHOOD!"),
        QStringLiteral("WIDE-BAND REMOVAL!"),
        QStringLiteral("RUNNING MEAN'D!"),
        QStringLiteral("MOVING-WINDOW BLAST!"),
        QStringLiteral("DESPIKED IN BULK!"),
        QStringLiteral("SECTOR FLAGGED!"),
        QStringLiteral("BARRAGE!"),
        QStringLiteral("FIRE IN THE FOOTPRINT!"),
        QStringLiteral("DIRECT HIT!"),
        QStringLiteral("CLUSTER CLEANED!"),
        QStringLiteral("BULK QC!"),
        QStringLiteral("OUTLIERS, PLURAL!"),
        QStringLiteral("BOOM BOOM DESPIKE!"),
        QStringLiteral("AREA-OF-EFFECT QC!"),
        QStringLiteral("INCOMING!"),
        QStringLiteral("DETREND ROCKET AWAY!"),
        QStringLiteral("SPLATTERED!"),
        QStringLiteral("SMOOTHED!"),
        QStringLiteral("BLOWN OUT OF THE SPECTRUM!"),
        QStringLiteral("WIPED!")
    };
    return list;
}

// the screen nuke
const QStringList& nukePops()
{
    static const QStringList list {
        QStringLiteral("HARD FLAG: EVERYTHING!"),
        QStringLiteral("THE WHOLE HALF-HOUR IS FLAG 2!"),
        QStringLiteral("SCREEN FLAGGED!"),
        QStringLiteral("NUKED FROM ORBIT!"),
        QStringLiteral("TOTAL QC!"),
        QStringLiteral("ALL OUTLIERS, GONE!"),
        QStringLiteral("THE NUCLEAR OPTION!"),
        QStringLiteral("MASS DESPIKING EVENT!"),
        QStringLiteral("ABOVE 3.5 SIGMA: ALL OF IT!"),
        QStringLiteral("THE QC SCRIPT RAN ON EVERYTHING!"),
        QStringLiteral("BLANKET FILTER!"),
        QStringLiteral("SCORCHED DATA!"),
        QStringLiteral("THE BIG RED BUTTON!"),
        QStringLiteral("CTRL-A, DELETE!"),
        QStringLiteral("EVERY DEMON, ONE FLAG!"),
        QStringLiteral("REVIEWER 2 SURRENDERS!"),
        QStringLiteral("FLAGGED INTO OBLIVION!"),
        QStringLiteral("ALL CLEAR!"),
        QStringLiteral("A NIGHT-TIME FILTER FOR DEMONS!"),
        QStringLiteral("PURGE!"),
        QStringLiteral("MAXIMUM FLAGGING!"),
        QStringLiteral("SCREEN WIPE!"),
        QStringLiteral("QC LEVEL 9000!"),
        QStringLiteral("NOT A SPIKE LEFT!"),
        QStringLiteral("THE NUKE HAS SPOKEN!"),
        QStringLiteral("DEMONS FLAGGED, STARS SPARED!"),
        QStringLiteral("CLEAN SWEEP!"),
        QStringLiteral("THE WHOLE WINDOW DESPIKED!"),
        QStringLiteral("GLOBAL MASK APPLIED!"),
        QStringLiteral("BOOM. SILENCE. DATA.")
    };
    return list;
}

// the BFG going off
const QStringList& bfgPops()
{
    static const QStringList list {
        QStringLiteral("BFG 42!"),
        QStringLiteral("BIG FLUX GUN!"),
        QStringLiteral("SPECTRAL CORRECTION: MAXIMUM!"),
        QStringLiteral("FULL-SPECTRUM DESPIKE!"),
        QStringLiteral("BIG FILTERING GUN!"),
        QStringLiteral("A WAVE OF QC!"),
        QStringLiteral("GREEN FLASH!"),
        QStringLiteral("THE ULTIMATE DESPIKE!"),
        QStringLiteral("OGIVE CONVERGED!"),
        QStringLiteral("PLASMA DESPIKING!"),
        QStringLiteral("COSPECTRAL ANNIHILATION!"),
        QStringLiteral("BFG 42 FIRED!"),
        QStringLiteral("THE BIG ONE!"),
        QStringLiteral("FLUX-DENSITY OVERLOAD!"),
        QStringLiteral("COSPECTRUM CLEANSED!"),
        QStringLiteral("MAXIMUM BANDWIDTH!"),
        QStringLiteral("HIGH-PASS, LOW-PASS, ALL-PASS!"),
        QStringLiteral("MOMENT OF TRUTH!"),
        QStringLiteral("THE ANSWER IS 42!"),
        QStringLiteral("SUPER DESPIKE!"),
        QStringLiteral("THE GREEN WAVE!"),
        QStringLiteral("SPECTRUM FLATTENED!"),
        QStringLiteral("EVERY FREQUENCY, PURGED!"),
        QStringLiteral("BFG ENGAGED!"),
        QStringLiteral("SHOCKWAVE!"),
        QStringLiteral("OVERKILL, BUT SATISFYING!"),
        QStringLiteral("AREA OF EFFECT: ONE HECTARE!"),
        QStringLiteral("FOOTPRINT-WIDE QC!"),
        QStringLiteral("RIP AND DESPIKE!"),
        QStringLiteral("BIG. FLUX. GUN.")
    };
    return list;
}

// a right-click before the BFG has charged
const QStringList& bfgNotReadyPops()
{
    static const QStringList list {
        QStringLiteral("BFG charging…"),
        QStringLiteral("not charged!"),
        QStringLiteral("*whine*"),
        QStringLiteral("despike more first!"),
        QStringLiteral("charging… keep despiking!"),
        QStringLiteral("*capacitor hum*"),
        QStringLiteral("not yet!"),
        QStringLiteral("the BFG is warming up"),
        QStringLiteral("BFG: insufficient spikes"),
        QStringLiteral("*spin up*… *spin down*"),
        QStringLiteral("the BFG needs more kills"),
        QStringLiteral("error: BFG not ready"),
        QStringLiteral("*buzz*"),
        QStringLiteral("not enough flux!"),
        QStringLiteral("charge it first!"),
        QStringLiteral("BFG offline"),
        QStringLiteral("*fizzle*"),
        QStringLiteral("keep shooting demons"),
        QStringLiteral("the BFG says no"),
        QStringLiteral("low charge!"),
        QStringLiteral("*sad beep*"),
        QStringLiteral("the battery is flat"),
        QStringLiteral("despike to charge"),
        QStringLiteral("the gauge isn't full!"),
        QStringLiteral("*bzzt*"),
        QStringLiteral("ready soon…"),
        QStringLiteral("not now!"),
        QStringLiteral("wait for the green bar"),
        QStringLiteral("BFG: please hold"),
        QStringLiteral("*click* (it's a BFG, not a mouse)")
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

// does a circle reach a rectangle?
bool touches(const QPointF& centre, qreal radius, const QRectF& rect)
{
    const qreal x = std::clamp(centre.x(), rect.left(), rect.right());
    const qreal y = std::clamp(centre.y(), rect.top(), rect.bottom());
    return distance(centre, QPointF(x, y)) <= radius;
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
    nextDropout_(0.0),
    nextPower_(0.0),
    flash_(0.0),
    flashWhite_(false),
    scrollCarry_(0.0),
    bfgCharge_(0.0),
    bfgNag_(BFG_NAG_S),
    walk_(0.0),
    health_(START_HEALTH),
    ammo_(START_AMMO),
    rockets_(0),
    points_(0),
    despiked_(0),
    escaped_(0),
    validRemoved_(0),
    shots_(0),
    gapsSealed_(0),
    gapsLost_(0)
{
    setMinimumSize(560, 300);
    setCursor(Qt::CrossCursor);
    setMouseTracking(false);
    // the right button fires the BFG; no ancestor gets to open a menu on it
    setContextMenuPolicy(Qt::PreventContextMenu);

    timer_ = new QTimer(this);
    timer_->setInterval(TICK_MS);
    timer_->setTimerType(Qt::PreciseTimer);
    connect(timer_, &QTimer::timeout, this, &DespikeArena::tick);
}

void DespikeArena::start()
{
    health_ = START_HEALTH;
    ammo_ = START_AMMO;
    rockets_ = 0;
    points_ = 0;
    despiked_ = 0;
    escaped_ = 0;
    validRemoved_ = 0;
    shots_ = 0;
    gapsSealed_ = 0;
    gapsLost_ = 0;
    roundTime_ = 0.0;
    countdown_ = COUNTDOWN_S;
    nextSpawn_ = 0.4;
    nextStar_ = 1.0;
    nextAmmo_ = 0.8;
    nextDropout_ = 7.0;
    nextPower_ = 6.0;
    flash_ = 0.0;
    flashWhite_ = false;
    scrollCarry_ = 0.0;
    bfgCharge_ = 0.0;
    bfgNag_ = BFG_NAG_S;
    walk_ = 0.0;
    series_.clear();
    spikes_.clear();
    ammoSpikes_.clear();
    stars_.clear();
    dropouts_.clear();
    powerUps_.clear();
    buffs_.clear();
    blasts_.clear();
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
    dropouts_.clear();
    powerUps_.clear();
    buffs_.clear();
    blasts_.clear();
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
    // downsampling slows the data down, not the clock: the round, the
    // supplies' own timers and the effects all run on real time
    const qreal worldDt = dt * timeScale();

    scroll(worldDt);

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
    bfgNag_ += dt;

    spawn(worldDt);
    age(worldDt);

    for (int i = buffs_.size() - 1; i >= 0; --i)
    {
        buffs_[i].remaining -= dt;
        if (buffs_[i].remaining <= 0.0)
            buffs_.removeAt(i);
    }
    for (int i = blasts_.size() - 1; i >= 0; --i)
    {
        blasts_[i].age += dt;
        if (blasts_[i].age > BLAST_S)
            blasts_.removeAt(i);
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
    const qreal speed = 90.0 + 110.0 * progress();
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
        for (auto& dropout : dropouts_)
            dropout.x -= STEP_PX;
        for (auto& powerUp : powerUps_)
            powerUp.x -= STEP_PX;
    }
}

void DespikeArena::spawn(qreal dt)
{
    const qreal width = plotRect().width();
    const qreal p = progress();

    nextSpawn_ -= dt;
    if (nextSpawn_ <= 0.0)
    {
        // more of them come iced as the round goes on, and they last a
        // little longer so that two shots are possible
        const bool rimed = uniform(0.0, 1.0) < 0.10 + 0.25 * p;
        const qreal lifetime = (1.8 - 1.0 * p) * (rimed ? 1.25 : 1.0);
        spikes_.append({ rimed ? Spike::Kind::Rimed : Spike::Kind::Plain,
                         uniform(0.3, 0.95) * width,
                         uniform(0.25, 0.45),
                         0.0,
                         lifetime,          // 1.8 s at the start, 0.8 s at the end
                         0.30 - 0.18 * p,
                         rimed ? 2 : 1 });
        nextSpawn_ = (1.0 - 0.70 * p) * uniform(0.7, 1.3);
    }

    nextStar_ -= dt;
    if (nextStar_ <= 0.0)
    {
        // valid data are not found in the middle of missing data
        const qreal x = uniform(0.4, 0.95) * width;
        if (!inDropout(x))
            stars_.append({ x, 0.0 });
        nextStar_ = uniform(1.0, 2.2);
    }

    nextDropout_ -= dt;
    if (nextDropout_ <= 0.0)
    {
        const qreal gapWidth = uniform(70.0, 120.0);
        const qreal x = uniform(0.45 * width, std::max(0.45 * width, width - gapWidth - 8.0));
        bool room = dropouts_.size() < MAX_GAPS;
        for (const auto& other : dropouts_)
        {
            if (std::abs(other.x - x) < GAP_SPACING + gapWidth)
                room = false;
        }
        if (room)
        {
            dropouts_.append({ x, gapWidth, 0.0, 0.0 });
            nextDropout_ = uniform(4.5, 7.0) - 1.5 * p;
        }
        else
        {
            nextDropout_ = 0.5;
        }
    }

    nextPower_ -= dt;
    if (nextPower_ <= 0.0 && powerUps_.isEmpty())
    {
        using Kind = PowerUp::Kind;
        const bool lowHealth = health_ <= 30;
        const QList<QPair<Kind, int>> weights {
            { Kind::Quad, 20 },
            { Kind::Berserk, 18 },
            { Kind::Downsample, 14 },
            // no healing at full health; twice as likely when it is needed
            { Kind::GapFill, health_ >= START_HEALTH ? 0 : (lowHealth ? 28 : 14) },
            { Kind::Invulnerable, lowHealth ? 24 : 12 },
            { Kind::HardFlag, 8 },
            { Kind::Rocket, 14 }
        };
        int total = 0;
        for (const auto& weight : weights)
            total += weight.second;
        int draw = QRandomGenerator::global()->bounded(total);
        Kind kind = Kind::Quad;
        for (const auto& weight : weights)
        {
            if (draw < weight.second)
            {
                kind = weight.first;
                break;
            }
            draw -= weight.second;
        }
        powerUps_.append({ kind, uniform(0.35, 0.9) * width, uniform(0.68, 0.86), 0.0 });
        nextPower_ = uniform(8.5, 12.0);
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
            const bool rimed = spike.kind == Spike::Kind::Rimed;
            spikes_.removeAt(i);
            ++escaped_;
            hurt(rimed ? RIME_ESCAPE_DAMAGE : ESCAPE_DAMAGE);
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

    for (int i = dropouts_.size() - 1; i >= 0; --i)
    {
        auto& dropout = dropouts_[i];
        dropout.age += dt;
        if (dropout.age > GAP_LIFETIME_S || dropout.x + dropout.width < 0)
        {
            const auto plot = plotRect();
            const QPointF where(std::clamp(dropout.x + dropout.width / 2, 60.0, plot.width() - 60.0),
                                plot.center().y() - 0.25 * plot.height());
            addPop(where, EggUi::pickFresh(gapEscapePops()), DANGER);
            dropouts_.removeAt(i);
            ++gapsLost_;
            hurt(GAP_DAMAGE);
        }
    }

    for (int i = powerUps_.size() - 1; i >= 0; --i)
    {
        auto& powerUp = powerUps_[i];
        powerUp.age += dt;
        if (powerUp.age > POWER_LIFETIME_S || powerUp.x < 0)
            powerUps_.removeAt(i);
    }
}

void DespikeArena::hurt(int amount)
{
    // under the shield an escape still counts, it just costs nothing
    if (hasBuff(PowerUp::Kind::Invulnerable))
    {
        flashWhite_ = true;
        flash_ = FLASH_S;
        return;
    }
    health_ = std::max(health_ - amount, 0);
    flashWhite_ = false;
    flash_ = amount > ESCAPE_DAMAGE ? 1.6 * FLASH_S : FLASH_S;
}

void DespikeArena::endRound(bool died)
{
    timer_->stop();
    state_ = State::Over;
    const int score = points_ - VALID_DATA_PENALTY * validRemoved_;
    emit gameOver(score, despiked_, escaped_, validRemoved_, shots_,
                  gapsSealed_, gapsLost_, died);
}

void DespikeArena::addPop(const QPointF &pos, const QString &text, const QColor &color)
{
    pops_.append({ pos, 0.0, text, color });
}

qreal DespikeArena::timeScale() const
{
    return hasBuff(PowerUp::Kind::Downsample) ? 0.5 : 1.0;
}

bool DespikeArena::hasBuff(PowerUp::Kind kind) const
{
    return std::any_of(buffs_.cbegin(), buffs_.cend(),
                       [kind](const Buff& buff) { return buff.kind == kind; });
}

int DespikeArena::damage() const
{
    return hasBuff(PowerUp::Kind::Quad) ? 4 : 1;
}

void DespikeArena::applyPowerUp(const PowerUp &powerUp)
{
    const QPointF where = powerUpPos(powerUp);
    const QColor color = kindColor(powerUp.kind);

    switch (powerUp.kind)
    {
        case PowerUp::Kind::HardFlag:
        {
            const auto plot = plotRect();
            const qreal everything = std::hypot(plot.width(), plot.height());
            shots_ += detonate(plot.center(), everything, DANGER, false, true);
            addPop(where, EggUi::pickFresh(nukePops()), DANGER);
            return;
        }
        case PowerUp::Kind::Rocket:
            rockets_ = std::min(rockets_ + ROCKETS_PER_PICKUP, MAX_ROCKETS);
            break;
        case PowerUp::Kind::Berserk:
            health_ = std::min(health_ + BERSERK_HEALTH, START_HEALTH);
            [[fallthrough]];
        default:
        {
            // grabbing one that is already running starts it again
            const qreal duration = kindDuration(powerUp.kind);
            bool running = false;
            for (auto& buff : buffs_)
            {
                if (buff.kind == powerUp.kind)
                {
                    buff.remaining = duration;
                    running = true;
                }
            }
            if (!running)
                buffs_.append({ powerUp.kind, duration, duration });
            break;
        }
    }

    addPop(where, EggUi::pickFresh(powerUpPops()).arg(kindName(powerUp.kind)), color);
}

void DespikeArena::rewardKill(int points, qreal charge)
{
    points_ += points * (hasBuff(PowerUp::Kind::Quad) ? 2 : 1);
    bfgCharge_ = std::min(bfgCharge_ + charge, 1.0);
    if (hasBuff(PowerUp::Kind::GapFill))
        health_ = std::min(health_ + GAP_FILL_HEALTH, START_HEALTH);
}

void DespikeArena::killSpike(int index, bool charge, bool announce)
{
    const Spike spike = spikes_.at(index);
    const QPointF head = headPos(spike);
    const bool rimed = spike.kind == Spike::Kind::Rimed;
    spikes_.removeAt(index);
    ++despiked_;
    rewardKill(rimed ? RIME_POINTS : 1,
               charge ? (rimed ? BFG_PER_RIME : BFG_PER_SPIKE) : 0.0);
    if (announce)
        addPop(head, EggUi::pickFresh(hitPops()), AMBER);
}

void DespikeArena::sealDropout(int index, bool charge, bool announce)
{
    const Dropout dropout = dropouts_.at(index);
    const auto plot = plotRect();
    dropouts_.removeAt(index);
    ++gapsSealed_;
    rewardKill(GAP_POINTS, charge ? BFG_PER_GAP : 0.0);
    if (announce)
        addPop(QPointF(dropout.x + dropout.width / 2, plot.center().y() - 0.25 * plot.height()),
               EggUi::pickFresh(gapPops()), AMMO_GREEN);
}

void DespikeArena::shootStar(int index)
{
    const auto& star = stars_.at(index);
    addPop(QPointF(star.x, seriesY(star.x)),
           EggUi::pickFresh(starPops()).arg(VALID_DATA_PENALTY), STAR_BLUE);
    stars_.removeAt(index);
    ++validRemoved_;
}

int DespikeArena::detonate(const QPointF &centre, qreal radius, const QColor &color,
                           bool hitsStars, bool charge)
{
    int kills = 0;
    for (int i = spikes_.size() - 1; i >= 0; --i)
    {
        if (distance(headPos(spikes_.at(i)), centre) <= radius)
        {
            // ice is no protection against a blast
            killSpike(i, charge, false);
            ++kills;
        }
    }
    for (int i = dropouts_.size() - 1; i >= 0; --i)
    {
        if (touches(centre, radius, gapRect(dropouts_.at(i))))
            sealDropout(i, charge, false);
    }
    if (hitsStars)
    {
        // a blast is not a surgical despiking tool
        for (int i = stars_.size() - 1; i >= 0; --i)
        {
            const auto& star = stars_.at(i);
            if (distance(QPointF(star.x, seriesY(star.x)), centre) <= radius)
                shootStar(i);
        }
    }
    blasts_.append({ centre, 0.0, radius, color });
    return kills;
}

void DespikeArena::fireRocket(const QPointF &click)
{
    --rockets_;
    // one shot per demon it takes out, and a miss is a miss
    shots_ += std::max(detonate(click, ROCKET_RADIUS, ROCKET_ORANGE, true, true), 1);
    addPop(click, EggUi::pickFresh(rocketPops()), ROCKET_ORANGE);
}

void DespikeArena::fireBfg(const QPointF &click)
{
    if (bfgCharge_ < 1.0)
    {
        // people mash the button: say so once in a while, not every time
        if (bfgNag_ >= BFG_NAG_S)
        {
            addPop(click, EggUi::pickFresh(bfgNotReadyPops()), NAG_GREY);
            bfgNag_ = 0.0;
        }
        return;
    }

    bfgCharge_ = 0.0;
    shots_ += detonate(click, BFG_RADIUS, AMMO_GREEN, true, false);
    addPop(click, EggUi::pickFresh(bfgPops()), AMMO_GREEN);
}

QColor DespikeArena::kindColor(PowerUp::Kind kind)
{
    switch (kind)
    {
        case PowerUp::Kind::Quad: return QUAD_VIOLET;
        case PowerUp::Kind::Berserk: return BERSERK_RED;
        case PowerUp::Kind::Downsample: return SLOW_BLUE;
        case PowerUp::Kind::GapFill: return HEAL_GREEN;
        case PowerUp::Kind::Invulnerable: return GOD_WHITE;
        case PowerUp::Kind::HardFlag: return DANGER;
        case PowerUp::Kind::Rocket: return ROCKET_ORANGE;
    }
    return AMBER;
}

QString DespikeArena::kindChip(PowerUp::Kind kind)
{
    switch (kind)
    {
        case PowerUp::Kind::Quad: return QStringLiteral("QUAD");
        case PowerUp::Kind::Berserk: return QStringLiteral("BERSERK");
        case PowerUp::Kind::Downsample: return QStringLiteral("2 Hz");
        case PowerUp::Kind::GapFill: return QStringLiteral("GAP-FILL");
        case PowerUp::Kind::Invulnerable: return QStringLiteral("IDDQD");
        case PowerUp::Kind::HardFlag: return QStringLiteral("FLAG");
        case PowerUp::Kind::Rocket: return QStringLiteral("ROCKETS");
    }
    return QString();
}

QString DespikeArena::kindName(PowerUp::Kind kind)
{
    switch (kind)
    {
        case PowerUp::Kind::Quad: return QStringLiteral("QUAD-PASS FILTER");
        case PowerUp::Kind::Berserk: return QStringLiteral("MANUAL QC");
        case PowerUp::Kind::Downsample: return QStringLiteral("DOWNSAMPLING");
        case PowerUp::Kind::GapFill: return QStringLiteral("GAP-FILLING");
        case PowerUp::Kind::Invulnerable: return QStringLiteral("ZERO/SPAN SHIELD");
        case PowerUp::Kind::HardFlag: return QStringLiteral("HARD FLAG");
        case PowerUp::Kind::Rocket: return QStringLiteral("DETREND ROCKETS");
    }
    return QString();
}

qreal DespikeArena::kindDuration(PowerUp::Kind kind)
{
    switch (kind)
    {
        case PowerUp::Kind::Quad: return 5.0;
        case PowerUp::Kind::Berserk: return 6.0;
        case PowerUp::Kind::Downsample: return 5.0;
        case PowerUp::Kind::GapFill: return 7.0;
        case PowerUp::Kind::Invulnerable: return 6.0;
        case PowerUp::Kind::HardFlag:
        case PowerUp::Kind::Rocket:
            break;
    }
    return 0.0;
}

void DespikeArena::mousePressEvent(QMouseEvent *event)
{
    if (state_ != State::Running)
    {
        QWidget::mousePressEvent(event);
        return;
    }

    const QPointF click = event->position();

    if (event->button() == Qt::RightButton)
    {
        fireBfg(click);
        update();
        return;
    }
    if (event->button() != Qt::LeftButton)
    {
        QWidget::mousePressEvent(event);
        return;
    }

    const bool berserk = hasBuff(PowerUp::Kind::Berserk);
    const qreal reach = berserk ? BERSERK_REACH : 1.0;

    // supplies and ammo are grabbed, not shot: they cost nothing and work
    // with an empty gun
    for (int i = 0; i < powerUps_.size(); ++i)
    {
        if (distance(powerUpPos(powerUps_.at(i)), click) <= POWER_GRAB_RADIUS * reach)
        {
            const PowerUp powerUp = powerUps_.at(i);
            powerUps_.removeAt(i);
            applyPowerUp(powerUp);
            update();
            return;
        }
    }
    for (int i = 0; i < ammoSpikes_.size(); ++i)
    {
        const QPointF box = boxPos(ammoSpikes_.at(i));
        if (distance(box, click) <= AMMO_GRAB_RADIUS * reach)
        {
            ammo_ = std::min(ammo_ + AMMO_PICKUP, MAX_AMMO);
            addPop(box, EggUi::pickFresh(ammoPops()).arg(AMMO_PICKUP), AMMO_GREEN);
            ammoSpikes_.removeAt(i);
            update();
            return;
        }
    }

    // a loaded rocket goes before the gun
    if (rockets_ > 0)
    {
        fireRocket(click);
        update();
        return;
    }

    if (ammo_ <= 0 && !berserk)
    {
        addPop(click, EggUi::pickFresh(emptyPops()), NAG_GREY);
        update();
        return;
    }
    if (!berserk)
        --ammo_;

    // a demon first, so it wins a tie as it always has; then a gap, so a
    // star drifting across missing data is shielded rather than a trap
    int spikeHit = -1;
    qreal spikeDistance = HIT_RADIUS * reach;
    for (int i = 0; i < spikes_.size(); ++i)
    {
        const qreal d = distance(headPos(spikes_.at(i)), click);
        if (d <= spikeDistance)
        {
            spikeDistance = d;
            spikeHit = i;
        }
    }

    if (spikeHit >= 0)
    {
        auto& spike = spikes_[spikeHit];
        spike.hp -= damage();
        if (spike.hp > 0)
        {
            // the shell cracks; this shot is not yet a hit or a miss
            addPop(headPos(spike), EggUi::pickFresh(rimePops()), ICE_BLUE);
        }
        else
        {
            ++shots_;
            killSpike(spikeHit, true, true);
        }
        update();
        return;
    }

    for (int i = 0; i < dropouts_.size(); ++i)
    {
        const QRectF target = gapRect(dropouts_.at(i)).adjusted(-4 * reach, -6 * reach,
                                                                4 * reach, 6 * reach);
        if (target.contains(click))
        {
            // filling in is work, not shooting: it does not count as a shot
            auto& dropout = dropouts_[i];
            dropout.sealed += damage() / 3.0;
            if (dropout.sealed >= 0.999)
                sealDropout(i, true, true);
            update();
            return;
        }
    }

    ++shots_;
    int starHit = -1;
    qreal starDistance = STAR_HIT_RADIUS * reach;
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
    if (starHit >= 0)
        shootStar(starHit);
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
    // room for the ice shell at the top edge
    return QPointF(spike.x, std::max(y, plot.top() + HEAD_RADIUS * 1.6 + 12));
}

QPointF DespikeArena::boxPos(const AmmoSpike &ammo) const
{
    const auto plot = plotRect();
    const qreal grown = std::min(ammo.age / 0.3, 1.0);
    const qreal y = seriesY(ammo.x) + grown * ammo.depth * plot.height();
    return QPointF(ammo.x, std::min(y, plot.bottom() - 12));
}

QPointF DespikeArena::powerUpPos(const PowerUp &powerUp) const
{
    const auto plot = plotRect();
    return QPointF(powerUp.x,
                   plot.top() + powerUp.y * plot.height() + 4.0 * std::sin(powerUp.age * 4.0));
}

QRectF DespikeArena::gapRect(const Dropout &dropout) const
{
    const auto plot = plotRect();
    const qreal left = dropout.x + dropout.sealed * dropout.width;
    const qreal right = dropout.x + dropout.width;
    return QRectF(left, plot.center().y() - 0.25 * plot.height(),
                  std::max(right - left, 0.0), 0.5 * plot.height());
}

bool DespikeArena::inDropout(qreal x) const
{
    for (const auto& dropout : dropouts_)
    {
        if (x >= dropout.x + dropout.sealed * dropout.width && x <= dropout.x + dropout.width)
            return true;
    }
    return false;
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
    {
        const qreal strength = std::min(flash_ / FLASH_S, 1.0);
        p.fillRect(plot, flashWhite_
                             ? QColor(255, 255, 255, static_cast<int>(70 * strength))
                             : QColor(200, 0, 0, static_cast<int>(90 * strength)));
    }

    p.setPen(QColor(120, 120, 120));
    p.drawText(plot.adjusted(8, 6, -8, -6), Qt::AlignLeft | Qt::AlignTop,
               QStringLiteral("w′ (m s⁻¹)"));

    // behind the series, so that its missing stretch shows through
    for (const auto& dropout : dropouts_)
        paintDropout(p, dropout);
    paintSeries(p);
    for (const auto& star : stars_)
        paintStar(p, star);
    for (const auto& ammo : ammoSpikes_)
        paintAmmoSpike(p, ammo);
    for (const auto& powerUp : powerUps_)
        paintPowerUp(p, powerUp);
    for (const auto& spike : spikes_)
        paintSpike(p, spike);
    for (const auto& blast : blasts_)
        paintBlast(p, blast);

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

    // the loudest single cue that something is different
    if (!buffs_.isEmpty())
    {
        p.setPen(QPen(kindColor(buffs_.last().kind), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(plot.adjusted(1, 1, -1, -1));
    }

    if (state_ != State::Idle)
    {
        paintBuffChips(p);
        paintBfgGauge(p);
    }

    if (state_ == State::Countdown)
    {
        auto big = font();
        big.setBold(true);
        big.setPointSize(48);
        p.setFont(big);
        p.setPen(AMBER);
        p.drawText(plot.adjusted(0, 0, 0, -110), Qt::AlignCenter,
                   QString::number(static_cast<int>(std::ceil(countdown_))));

        // how to play, while the player waits
        auto legend = font();
        legend.setBold(true);
        p.setFont(legend);
        const qreal lineHeight = p.fontMetrics().height() + 2;
        const QRectF lines = plot.adjusted(0, plot.height() / 2 - 10, 0, 0);
        const QList<QPair<QColor, QString>> legendLines {
            { QColor(230, 70, 60), tr("Demons: shoot them (the iced ones take two)") },
            { STAR_BLUE, tr("★ Stars: valid data, leave them alone") },
            { AMMO_GREEN, tr("▼ Downward spikes: grab them to reload") },
            { DANGER, tr("-9999 Drop-outs: click them until they are filled in") },
            { QUAD_VIOLET, tr("Supplies drift below the series: grab them") },
            { AMMO_GREEN, tr("BFG 42 charges as you despike: right-click to fire") }
        };
        for (int i = 0; i < legendLines.size(); ++i)
        {
            p.setPen(legendLines.at(i).first);
            p.drawText(lines.adjusted(0, i * lineHeight, 0, 0), Qt::AlignHCenter | Qt::AlignTop,
                       legendLines.at(i).second);
        }
    }

    paintHud(p);
}

void DespikeArena::paintSeries(QPainter &p) const
{
    if (series_.size() < 2)
        return;

    // missing data are missing: the line breaks across every open drop-out
    QPainterPath path;
    bool drawing = false;
    for (int i = 0; i < series_.size(); ++i)
    {
        const qreal x = i * STEP_PX;
        if (inDropout(x))
        {
            drawing = false;
            continue;
        }
        const QPointF point(x, seriesY(x));
        if (!drawing)
            path.moveTo(point);
        else
            path.lineTo(point);
        drawing = true;
    }
    p.setPen(QPen(QColor(80, 220, 80), 1.6));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    // what has been filled in so far is marked as filled
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(170, 240, 150));
    for (const auto& dropout : dropouts_)
    {
        const qreal end = dropout.x + dropout.sealed * dropout.width;
        for (qreal x = dropout.x + 3; x < end; x += 9)
            p.drawRect(QRectF(x - 1.5, seriesY(x) - 1.5, 3, 3));
    }
}

void DespikeArena::paintSpike(QPainter &p, const Spike &spike) const
{
    const QPointF base(spike.x, seriesY(spike.x));
    const QPointF head = headPos(spike);
    const bool iced = spike.kind == Spike::Kind::Rimed && spike.hp >= 2;

    p.setPen(QPen(QColor(220, 40, 40), 2.5));
    p.drawLine(base, head);
    if (iced)
    {
        p.setPen(QPen(QColor(255, 255, 255, 200), 1.2, Qt::DashLine));
        p.drawLine(base, head);
    }

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

    if (spike.kind != Spike::Kind::Rimed)
        return;

    // a hexagonal shell of rime with needles; once cracked, only shards
    const qreal shell = HEAD_RADIUS * 1.45;
    QPolygonF hexagon;
    for (int i = 0; i < 6; ++i)
    {
        const qreal angle = PI / 6 + i * PI / 3;
        hexagon << head + QPointF(shell * std::cos(angle), shell * std::sin(angle));
    }
    if (iced)
    {
        QColor fill = ICE_BLUE;
        fill.setAlphaF(0.35);
        p.setBrush(fill);
        p.setPen(QPen(ICE_BLUE, 1.5));
        p.drawPolygon(hexagon);
        for (const auto& corner : hexagon)
        {
            const QPointF out = head + (corner - head) * 1.35;
            p.drawLine(corner, out);
        }
    }
    else
    {
        p.setPen(QPen(ICE_BLUE, 1.2));
        for (int i = 0; i < 6; i += 2)
            p.drawLine(hexagon.at(i), hexagon.at(i + 1));
    }
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

void DespikeArena::paintDropout(QPainter &p, const Dropout &dropout) const
{
    const QRectF rest = gapRect(dropout);
    if (rest.width() < 0.5)
        return;

    QColor band = DANGER;
    band.setAlpha(28);
    p.fillRect(rest, band);

    p.setPen(QPen(DANGER, 1.2, Qt::DashLine));
    p.drawLine(rest.topLeft(), rest.bottomLeft());
    p.drawLine(rest.topRight(), rest.bottomRight());

    // the interpolation that has not happened yet
    p.setPen(QPen(QColor(150, 150, 150), 1, Qt::DotLine));
    p.drawLine(QPointF(rest.left(), seriesY(rest.left())),
               QPointF(rest.right(), seriesY(rest.right())));

    // the missing-data sentinel, blinking in its last second
    if (GAP_LIFETIME_S - dropout.age < 1.0 && std::fmod(dropout.age * 8.0, 2.0) < 1.0)
        return;
    auto sentinel = font();
    sentinel.setBold(true);
    p.setFont(sentinel);
    p.setPen(DANGER);
    p.drawText(QRectF(rest.left() - 30, rest.top() - 22, rest.width() + 60, 20),
               Qt::AlignCenter, QStringLiteral("-9999"));
}

void DespikeArena::paintPowerUp(QPainter &p, const PowerUp &powerUp) const
{
    if (powerUp.age > POWER_LIFETIME_S * 0.7
        && std::fmod(powerUp.age * 8.0, 2.0) < 1.0)
        return;

    const qreal alpha = std::clamp(powerUp.age / 0.3, 0.0, 1.0);
    const QColor color = kindColor(powerUp.kind);
    const QPointF centre = powerUpPos(powerUp);

    p.save();
    p.translate(centre);
    p.setOpacity(alpha);

    QColor halo = color;
    halo.setAlphaF(0.2);
    p.setPen(Qt::NoPen);
    p.setBrush(halo);
    const qreal haloRadius = POWER_HALF * 1.6 * (1.0 + 0.12 * std::sin(powerUp.age * 7.0));
    p.drawEllipse(QPointF(0, 0), haloRadius, haloRadius);

    p.setPen(QPen(color, 2.5));
    p.setBrush(QColor(24, 24, 24));
    p.drawRoundedRect(QRectF(-POWER_HALF, -POWER_HALF, 2 * POWER_HALF, 2 * POWER_HALF), 6, 6);

    switch (powerUp.kind)
    {
        case PowerUp::Kind::Quad:
        {
            auto glyph = font();
            glyph.setBold(true);
            p.setFont(glyph);
            p.setPen(color);
            p.drawText(QRectF(-POWER_HALF, -POWER_HALF, 2 * POWER_HALF, 2 * POWER_HALF),
                       Qt::AlignCenter, QStringLiteral("×4"));
            break;
        }
        case PowerUp::Kind::Berserk:
        {
            // a jagged burst
            QPolygonF burst;
            for (int i = 0; i < 16; ++i)
            {
                const qreal radius = (i % 2 == 0) ? 8.5 : 4.0;
                const qreal angle = -PI / 2 + i * PI / 8;
                burst << QPointF(radius * std::cos(angle), radius * std::sin(angle));
            }
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawPolygon(burst);
            break;
        }
        case PowerUp::Kind::Downsample:
        {
            // sample and hold: the slow-motion sign and the pun in one
            const QPolygonF stairs { { -8, 4 }, { -4, 4 }, { -4, -1 }, { 0, -1 },
                                     { 0, -6 }, { 4, -6 }, { 4, 2 }, { 8, 2 } };
            p.setPen(QPen(color, 2.0));
            p.setBrush(Qt::NoBrush);
            p.drawPolyline(stairs);
            break;
        }
        case PowerUp::Kind::GapFill:
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawRect(QRectF(-7, -2.5, 14, 5));
            p.drawRect(QRectF(-2.5, -7, 5, 14));
            break;
        case PowerUp::Kind::Invulnerable:
        {
            QColor shield = color;
            shield.setAlphaF(0.6 + 0.4 * std::sin(powerUp.age * 20.0));
            p.setPen(QPen(shield, 2.0));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(QPointF(0, 0), 8, 8);
            p.setPen(Qt::NoPen);
            p.setBrush(shield);
            p.drawEllipse(QPointF(0, 0), 4, 4);
            break;
        }
        case PowerUp::Kind::HardFlag:
        {
            // a radiation trefoil
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            const QRectF disc(-9, -9, 18, 18);
            for (int i = 0; i < 3; ++i)
                p.drawPie(disc, (60 + i * 120) * 16, 60 * 16);
            p.setBrush(QColor(24, 24, 24));
            p.drawEllipse(QPointF(0, 0), 2.6, 2.6);
            p.setBrush(color);
            p.drawEllipse(QPointF(0, 0), 1.6, 1.6);
            break;
        }
        case PowerUp::Kind::Rocket:
        {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 230, 120));
            p.drawPolygon(QPolygonF { { -2, 5 }, { 0, 10 }, { 2, 5 } });
            p.setBrush(color);
            p.drawRect(QRectF(-3, -5, 6, 10));
            p.drawPolygon(QPolygonF { { -3, -5 }, { 0, -10 }, { 3, -5 } });
            p.drawPolygon(QPolygonF { { -3, 2 }, { -6, 6 }, { -3, 5 } });
            p.drawPolygon(QPolygonF { { 3, 2 }, { 6, 6 }, { 3, 5 } });
            break;
        }
    }

    p.restore();
}

void DespikeArena::paintBlast(QPainter &p, const Blast &blast) const
{
    const qreal t = std::clamp(blast.age / BLAST_S, 0.0, 1.0);
    const qreal fade = 1.0 - t;
    const auto plot = plotRect();

    // a nuke lights up the whole plot
    if (blast.radius >= std::hypot(plot.width(), plot.height()) * 0.9)
    {
        QColor wash = blast.color;
        wash.setAlphaF(0.30 * fade);
        p.fillRect(plot, wash);
    }

    p.setBrush(Qt::NoBrush);
    for (int k = 0; k < 3; ++k)
    {
        QColor ring = blast.color;
        ring.setAlphaF(fade * (1.0 - 0.25 * k));
        p.setPen(QPen(ring, 3.0 - k));
        const qreal r = blast.radius * (0.35 + 0.65 * t) * (1.0 - 0.18 * k);
        p.drawEllipse(blast.pos, r, r);
    }

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, static_cast<int>(200 * fade)));
    const qreal core = std::min(blast.radius, 60.0) * 0.3 * fade;
    p.drawEllipse(blast.pos, core, core);
}

void DespikeArena::paintBuffChips(QPainter &p) const
{
    const auto plot = plotRect();
    auto chipFont = font();
    chipFont.setBold(true);
    chipFont.setPointSizeF(std::max(chipFont.pointSizeF() - 1.0, 7.0));
    p.setFont(chipFont);

    qreal x = 10;
    const qreal y = plot.bottom() - 26;
    auto chip = [&](const QString& label, const QColor& color, qreal fraction)
    {
        const QRectF rect(x, y, 86, 18);
        QColor fill = color;
        fill.setAlpha(40);
        p.setBrush(fill);
        p.setPen(QPen(color, 1.2));
        p.drawRoundedRect(rect, 4, 4);
        p.setPen(color);
        p.drawText(rect, Qt::AlignCenter, label);
        // what is left of it, at a glance
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRect(QRectF(rect.left() + 3, rect.bottom() - 3,
                          (rect.width() - 6) * std::clamp(fraction, 0.0, 1.0), 2));
        x += 92;
    };

    for (const auto& buff : buffs_)
        chip(kindChip(buff.kind), kindColor(buff.kind), buff.remaining / buff.total);
    if (rockets_ > 0)
        chip(QStringLiteral("%1 %2").arg(kindChip(PowerUp::Kind::Rocket)).arg(rockets_),
             ROCKET_ORANGE, rockets_ / static_cast<qreal>(ROCKETS_PER_PICKUP));
}

void DespikeArena::paintBfgGauge(QPainter &p) const
{
    const auto plot = plotRect();
    const bool ready = bfgCharge_ >= 1.0;
    const qreal width = ready ? 180.0 : 110.0;
    const QRectF gauge(plot.right() - width - 10, plot.bottom() - 24, width, 14);

    auto gaugeFont = font();
    gaugeFont.setBold(true);
    gaugeFont.setPointSizeF(std::max(gaugeFont.pointSizeF() - 1.0, 7.0));
    p.setFont(gaugeFont);

    QColor fill = AMMO_GREEN;
    fill.setAlpha(ready ? static_cast<int>(140 + 90 * std::sin(roundTime_ * 8.0)) : 150);
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawRect(QRectF(gauge.left(), gauge.top(), gauge.width() * bfgCharge_, gauge.height()));

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(AMMO_GREEN, 1.2));
    p.drawRect(gauge);
    p.setPen(ready ? QColor(16, 16, 16) : AMMO_GREEN);
    p.drawText(gauge, Qt::AlignCenter,
               ready ? tr("BFG 42 — RIGHT-CLICK") : QStringLiteral("BFG 42"));
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
