/***************************************************************************
  matrixrain.cpp
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

#include "matrixrain.h"

#include <QFontDatabase>
#include <QFutureWatcher>
#include <QGlyphRun>
#include <QHash>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>

#include "defs.h"
#include "dlproject.h"
#include "ecproject.h"
#include "eastereggwidgets.h"
#include "fileutils.h"
#include "remotesource.h"
#include "variable_desc.h"

using RawPlayback::Recording;
using RawPlayback::Role;

namespace {

const double NaN = std::numeric_limits<double>::quiet_NaN();

const int FRAME_MS = 16;
const int TRAIL_MIN = 12;           // samples a glyph stays lit: the
const int TRAIL_MAX = 44;           // length of its stream
const double FLICKER_PER_S = 1.6;   // how often a lit glyph changes
const double LAG_S_PER_COLUMN = 0.1;  // frozen turbulence: older rows downstream
const int GLYPH_PIXELS = 16;
// depth from v: far smaller and dimmer, near larger and brighter
const double SIZE_SCALE[MatrixStats::SIZES] { 0.55, 0.68, 0.83, 1.0, 1.22, 1.50, 1.80 };
const int BASE_SIZE = 3;
// the raw wind on screen, in cells per second, zero where a component
// changes sign; w is the smallest of the three, so it is scaled up most
const double CELLS_PER_W = 40.0;    // 1 m/s of w: 40 cells/s, down when negative
const double CELLS_PER_U = 5.0;     // 1 m/s of u: 5 cells/s sideways
const double NO_WIND_FALL = 6.0;    // a file without a sonic still rains
const double FALL_MAX = 60.0;
const double SIDE_MAX = 30.0;
const double V_MIN_SCALE = 0.05;    // m/s: still air does not blow every small v up

// air parcels: released when w surges past PARCEL_Z standard deviations
const double PARCEL_Z = 1.8;
const double PARCEL_GAP_S = 0.8;    // at most one parcel per this long
const double PARCEL_SPREAD = 3.0;   // cells/s apart, at most
const double PARCEL_LIFE_S = 4.0;   // and up to 2 s more
const int PARCEL_MAX_DROPS = 120;

const int HUD_MS = 4500;
const double PREFETCH_S = 20.0;     // ask for the next file this long before the end
const int HUD_FADE_MS = 900;
const int INTRO_CHAR_MS = 85;
const int INTRO_HOLD_MS = 1700;

const QColor HEAD(226, 255, 230);
const QColor GLOW(60, 255, 110, 60);
const QColor TEXT_GREEN(70, 255, 110);

double clamp(double x, double lo, double hi)
{
    return std::min(std::max(x, lo), hi);
}

float valueAt(const RawPlayback::Channel* ch, int row)
{
    return (ch && row < ch->values.size()) ? ch->values.at(row)
                                           : std::numeric_limits<float>::quiet_NaN();
}

// How long one intro line takes to type and to hold
int lineMs(const QString& line)
{
    return int(line.size()) * INTRO_CHAR_MS + INTRO_HOLD_MS;
}

int introTotalMs()
{
    int total = 0;
    for (const auto& line : MatrixRain::introLines())
        total += lineMs(line);
    return total;
}

// The film's set: half-width katakana, digits, a Z and a handful of
// symbols. Only what the system font can draw is kept, so a font without
// one of the symbols never shows an empty box.
QString glyphChars(bool katakana, const QRawFont& font)
{
    QString wanted;
    if (katakana)
    {
        // half-width katakana ｦ … ﾝ
        for (char16_t c = 0xFF66; c <= 0xFF9D; ++c)
            wanted += QChar(c);
    }
    else
    {
        wanted += QStringLiteral("ABCDEFGHJKLMNPRSTUVXY");
    }
    wanted += QStringLiteral("0123456789Z:.\"=*+-<>¦|ç");

    QString chars;
    for (const QChar c : wanted)
        if (font.supportsCharacter(c))
            chars += c;

    // a fixed shuffle, so neighbouring fluctuations do not look alike
    std::mt19937 rng(42);
    std::shuffle(chars.begin(), chars.end(), rng);
    return chars;
}

// Fonts likely to hold half-width katakana, on each platform
const QStringList& katakanaFamilies()
{
    static const QStringList list {
        QStringLiteral("MS Gothic"),
        QStringLiteral("Yu Gothic"),
        QStringLiteral("Meiryo"),
        QStringLiteral("Noto Sans Mono CJK JP"),
        QStringLiteral("Noto Sans CJK JP"),
        QStringLiteral("IPAGothic"),
        QStringLiteral("TakaoGothic"),
        QStringLiteral("Osaka"),
        QStringLiteral("Hiragino Sans")
    };
    return list;
}

// the engine's names of the variables an ASCII project describes
QString engineName(const VariableDesc& var)
{
    if (var.ignore() == QLatin1String("yes") || var.numeric() == QLatin1String("no"))
        return QStringLiteral("ignore");

    const QString& s = var.variable();
    const std::pair<const QString, const char*> names[] {
        { VariableDesc::getVARIABLE_VAR_STRING_0(), "u" },
        { VariableDesc::getVARIABLE_VAR_STRING_1(), "v" },
        { VariableDesc::getVARIABLE_VAR_STRING_2(), "w" },
        { VariableDesc::getVARIABLE_VAR_STRING_3(), "ts" },
        { VariableDesc::getVARIABLE_VAR_STRING_4(), "sos" },
        { VariableDesc::getVARIABLE_VAR_STRING_5(), "co2" },
        { VariableDesc::getVARIABLE_VAR_STRING_6(), "h2o" },
        { VariableDesc::getVARIABLE_VAR_STRING_7(), "ch4" },
        { VariableDesc::getVARIABLE_VAR_STRING_8(), "n2o" },
        { VariableDesc::getVARIABLE_VAR_STRING_19(), "co" },
        { VariableDesc::getVARIABLE_VAR_STRING_20(), "so2" },
        { VariableDesc::getVARIABLE_VAR_STRING_21(), "o3" },
        { VariableDesc::getVARIABLE_VAR_STRING_22(), "nh3" },
        { VariableDesc::getVARIABLE_VAR_STRING_23(), "no" },
        { VariableDesc::getVARIABLE_VAR_STRING_24(), "no2" },
        { VariableDesc::getVARIABLE_VAR_STRING_31(), "cos" },
        { VariableDesc::getVARIABLE_VAR_STRING_32(), "n2" },
        { VariableDesc::getVARIABLE_VAR_STRING_33(), "o2" },
        { VariableDesc::getVARIABLE_VAR_STRING_34(), "ar" }
    };
    for (const auto& name : names)
        if (s == name.first)
            return QString::fromLatin1(name.second);
    if (VariableDesc::isGasVariable(s))
        return QStringLiteral("gas:") + s;
    return QStringLiteral("ignore");
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// MatrixStats

void MatrixStats::reset(int scalarCount, double frequency)
{
    *this = MatrixStats();
    frequency_ = frequency;
    scalars_.resize(scalarCount);
    cov_.fill(0.0, scalarCount);
    scalarZ_.fill(NaN, scalarCount);
}

// the cumulative mean until the window is full, then a sliding one
double MatrixStats::alpha(long long n) const
{
    const double window = std::max(WINDOW_S * frequency_, 1.0);
    return std::max(1.0 / double(std::max(n, 1LL)), 1.0 / window);
}

double MatrixStats::zOf(double fluctuation, double var)
{
    return var > 1e-12 ? fluctuation / std::sqrt(var) : 0.0;
}

void MatrixStats::push(float u, float v, float w, const QVector<float>& scalars)
{
    windGap_ = std::isnan(u) || std::isnan(v) || std::isnan(w);
    double wp = 0.0;
    if (!windGap_)
    {
        ++windN_;
        const double a = alpha(windN_);
        if (windN_ == 1)
        {
            u_.mean = u;
            v_.mean = v;
            w_.mean = w;
        }
        // into the mean wind: streamwise, cross-wind, vertical
        const double theta = std::atan2(v_.mean, u_.mean);
        const double c = std::cos(theta);
        const double s = std::sin(theta);
        const double speed = std::hypot(u_.mean, v_.mean);
        const double up = u * c + v * s - speed;
        const double vp = -u * s + v * c;
        wp = w - w_.mean;

        uz_ = zOf(up, varU_);
        vz_ = zOf(vp, varV_);
        wz_ = zOf(wp, w_.var);

        u_.mean += a * (u - u_.mean);
        v_.mean += a * (v - v_.mean);
        w_.mean += a * (w - w_.mean);
        varU_ += a * (up * up - varU_);
        varV_ += a * (vp * vp - varV_);
        w_.var += a * (wp * wp - w_.var);
        const double vRaw = v - v_.mean;
        v_.var += a * (vRaw * vRaw - v_.var);
        windSeen_ = true;
    }
    else
    {
        uz_ = vz_ = wz_ = 0.0;
    }

    for (int i = 0; i < scalars_.size(); ++i)
    {
        const float x = i < scalars.size() ? scalars.at(i) : std::numeric_limits<float>::quiet_NaN();
        if (std::isnan(x))
        {
            scalarZ_[i] = NaN;
            continue;
        }
        Moment& m = scalars_[i];
        ++m.n;
        const double a = alpha(m.n);
        if (m.n == 1)
            m.mean = x;
        const double sp = x - m.mean;
        scalarZ_[i] = zOf(sp, m.var);
        m.mean += a * (x - m.mean);
        m.var += a * (sp * sp - m.var);
        if (!windGap_)
            cov_[i] += a * (wp * sp - cov_[i]);
    }
}

double MatrixStats::direction() const
{
    return std::atan2(v_.mean, u_.mean);
}

double MatrixStats::meanSpeed() const
{
    return std::hypot(u_.mean, v_.mean);
}

double MatrixStats::covariance(int i) const
{
    return cov_.value(i);
}

double MatrixStats::correlation(int i) const
{
    const double denominator = std::sqrt(w_.var * scalars_.value(i).var);
    return denominator > 1e-12 ? clamp(cov_.value(i) / denominator, -1.0, 1.0) : 0.0;
}

double MatrixStats::fallRate(double w)
{
    return std::isnan(w) ? 0.0 : clamp(-CELLS_PER_W * w, -FALL_MAX, FALL_MAX);
}

double MatrixStats::sideRate(double u)
{
    return std::isnan(u) ? 0.0 : clamp(CELLS_PER_U * u, -SIDE_MAX, SIDE_MAX);
}

//> v = 0 is the middle size and its sign says larger or smaller, but the
//> scale is the data's own: with a fixed one, a site whose mean v sits
//> beyond it drew every glyph at the largest size and none ever changed.
int MatrixStats::sizeOf(double v, double scale)
{
    if (std::isnan(v) || !(scale > 0.0))
        return BASE_SIZE;
    return BASE_SIZE + int(std::lround(clamp(v / scale, -1.0, 1.0) * BASE_SIZE));
}

double MatrixStats::vScale() const
{
    return std::max(std::fabs(v_.mean) + 2.0 * std::sqrt(std::max(v_.var, 0.0)), V_MIN_SCALE);
}

double MatrixStats::brightness(double wz, double sz)
{
    return std::tanh(wz * sz);
}

int MatrixStats::glyphFromValue(double value, int decimals, int count, int part)
{
    if (std::isnan(value) || count <= 0)
        return -1;
    //> The value's digits as the logger wrote them, 453.125 -> 453125, round
    //> the set: one raw value is always the same glyph, and the glyph
    //> changes exactly when the logged value does.
    double digits = std::round(double(value) * std::pow(10.0, std::clamp(decimals, 0, 9)));
    for (int i = 0; i < part; ++i)
        digits = std::floor(digits / count);
    const double m = std::fmod(digits, double(count));
    return int(m < 0.0 ? m + count : m);
}

////////////////////////////////////////////////////////////////////////////////
// MatrixRain

MatrixRain::MatrixRain(QWidget* parent) :
    QWidget(parent),
    state_(State::Idle),
    timer_(new QTimer(this)),
    pausedMs_(0.0),
    paused_(false),
    introShiftMs_(0),
    legend_(false),
    endSignalled_(false),
    swapNow_(false),
    u_(nullptr),
    v_(nullptr),
    w_(nullptr),
    tsScalar_(-1),
    warm_(0.0),
    consumed_(0),
    steps_(0),
    historyBase_(0),
    katakana_(false),
    cellW_(10.0),
    cellH_(18.0),
    cols_(0),
    rows_(0),
    parcelCooldown_(0)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    timer_->setInterval(FRAME_MS);
    connect(timer_, &QTimer::timeout, this, &MatrixRain::tick);
}

const QStringList& MatrixRain::introLines()
{
    static const QStringList lines {
        QStringLiteral("Wake up, Neo…"),
        QStringLiteral("The Matrix has you…"),
        QStringLiteral("Follow the white rabbit."),
        QStringLiteral("Knock, knock, Neo.")
    };
    return lines;
}

QSize MatrixRain::sizeHint() const
{
    return QSize(1200, 800);
}

QSize MatrixRain::minimumSizeHint() const
{
    return QSize(320, 480);
}

void MatrixRain::chooseFont()
{
    QFont font;
    katakana_ = false;
    for (const auto& family : katakanaFamilies())
    {
        QFont candidate(family);
        candidate.setPixelSize(GLYPH_PIXELS);
        const QRawFont raw = QRawFont::fromFont(candidate);
        if (raw.isValid() && raw.familyName() == family && raw.supportsCharacter(QChar(0xFF71)))
        {
            font = candidate;
            katakana_ = true;
            break;
        }
    }
    if (!katakana_)
    {
        font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        font.setPixelSize(GLYPH_PIXELS);
    }

    const QString chars = glyphChars(katakana_, QRawFont::fromFont(font));
    for (int k = 0; k < MatrixStats::SIZES; ++k)
    {
        QFont sized = font;
        sized.setPixelSize(std::max(qRound(GLYPH_PIXELS * SIZE_SCALE[k]), 6));
        // sub-pixel anti-aliasing leaves colour fringes on a black screen
        sized.setStyleStrategy(QFont::NoSubpixelAntialias);
        rawFont_[k] = QRawFont::fromFont(sized);
        glyphIndexes_[k] = rawFont_[k].glyphIndexesForString(chars);
    }

    const auto zero = rawFont_[BASE_SIZE].glyphIndexesForString(QStringLiteral("0"));
    const auto advance = rawFont_[BASE_SIZE].advancesForGlyphIndexes(zero);
    cellW_ = std::max(advance.isEmpty() ? 9.0 : advance.first().x() * 1.25, 9.0);
    cellH_ = std::max(rawFont_[BASE_SIZE].ascent() + rawFont_[BASE_SIZE].descent(), 14.0);
}

void MatrixRain::playIntro()
{
    stop();
    // looked up on first use, not when the main window is built
    if (glyphIndexes_[BASE_SIZE].isEmpty())
        chooseFont();
    state_ = State::Intro;
    introShiftMs_ = 0;
    introClock_.start();
    timer_->start();
    setFocus(Qt::OtherFocusReason);
    update();
}

// While it rains, the next file waits until the current one has played
// out, or N asked for it, so the rain never stops between files.
void MatrixRain::setRecording(std::shared_ptr<const Recording> recording)
{
    pending_ = std::move(recording);
    if (state_ == State::Waiting)
        startRain();
}

void MatrixRain::stop()
{
    timer_->stop();
    state_ = State::Idle;
    recording_.reset();
    pending_.reset();
    u_ = v_ = w_ = nullptr;
    scalars_.clear();
    grid_.clear();
    drops_.clear();
    parcels_.clear();
    history_.clear();
    cols_ = rows_ = 0;
    legend_ = false;
    paused_ = false;
    update();
}

void MatrixRain::skipIntro()
{
    introShiftMs_ = introTotalMs();
}

void MatrixRain::startRain()
{
    if (!pending_)
        return;
    recording_ = std::move(pending_);
    pending_.reset();

    u_ = recording_->channel(Role::U);
    v_ = recording_->channel(Role::V);
    w_ = recording_->channel(Role::W);
    scalars_ = recording_->scalars();
    scalarLabels_.clear();
    tsScalar_ = -1;
    for (int i = 0; i < scalars_.size(); ++i)
    {
        scalarLabels_.append(scalars_.at(i)->label);
        if (scalars_.at(i)->key == QLatin1String("ts"))
            tsScalar_ = i;
    }
    // a sonic on its own: the drops carry the horizontal wind speed
    const bool speedOnly = scalars_.isEmpty();
    if (speedOnly)
        scalarLabels_.append(QStringLiteral("|U|"));
    decimals_.clear();
    for (const auto* scalar : scalars_)
        decimals_.append(scalar->decimals);
    if (speedOnly)
        decimals_.append(u_ ? u_->decimals : 2);

    const int n = scalarLabels_.size();
    scalarHue_.resize(n);
    for (int i = 0; i < n; ++i)
        scalarHue_[i] = n > 1 ? 165.0 - i * 85.0 / (n - 1) : 125.0;
    rowScalars_.resize(n);

    //> The statistics start from the file's first 30 s, so the first glyphs
    //> already mean something; playback then starts at the first row.
    stats_.reset(n, recording_->frequency);
    const int prime = std::min(recording_->rows, int(MatrixStats::WINDOW_S * recording_->frequency));
    for (int row = 0; row < prime; ++row)
    {
        const float u = valueAt(u_, row);
        const float v = valueAt(v_, row);
        for (int i = 0; i < n; ++i)
            rowScalars_[i] = speedOnly ? float(std::hypot(u, v)) : valueAt(scalars_.at(i), row);
        stats_.push(u, v, valueAt(w_, row), rowScalars_);
    }

    warm_ = 0.0;
    consumed_ = 0;
    endSignalled_ = false;
    swapNow_ = false;
    paused_ = false;
    pausedMs_ = 0.0;
    clock_.start();
    hudClock_.start();
    frameClock_.start();
    state_ = State::Rain;
    if (cols_ == 0)
        layoutGrid();
    // rows of the previous file hold other variables; streams start afresh
    const double f = recording_->frequency;
    const int needed = int(cols_ * LAG_S_PER_COLUMN * f) + 2;
    if (history_.size() != needed)
        history_ = QVector<Frame>(needed);
    historyBase_ = steps_;
    for (int i = 0; i < drops_.size(); ++i)
        drops_[i].scalar = i % n;

    //> Streams downwind show older rows. Until there are enough of them,
    //> every stream past the newest would show the same first row, so one
    //> glyph would fill half the screen; the first seconds therefore play
    //> into the history unseen, and the visible rain starts after them.
    const int prefill = std::min(recording_->rows, needed - 1);
    for (int row = 0; row < prefill; ++row)
        step(row, false);
    consumed_ = prefill;
    pausedMs_ = prefill * 1000.0 / f;
    clock_.start();
    timer_->start();
    setFocus(Qt::OtherFocusReason);
}

void MatrixRain::layoutGrid()
{
    cols_ = std::max(1, int(width() / cellW_));
    rows_ = std::max(1, int(height() / cellH_));
    grid_ = QVector<Cell>(cols_ * rows_);
    parcels_.clear();

    const int n = std::max(1, int(scalarLabels_.size()));
    drops_ = QVector<Drop>(int(cols_ * 2.0));
    for (int i = 0; i < drops_.size(); ++i)
    {
        drops_[i].scalar = i % n;
        spawn(drops_[i], true);
    }

    const double f = recording_ ? recording_->frequency : 10.0;
    history_ = QVector<Frame>(int(cols_ * LAG_S_PER_COLUMN * f) + 2);
    historyBase_ = steps_;
}

void MatrixRain::spawn(Drop& drop, bool anywhere)
{
    auto* rng = QRandomGenerator::global();
    drop.x = rng->bounded(double(std::max(cols_, 1)));
    drop.pace = 0.85 + rng->bounded(0.3);
    drop.trail = TRAIL_MIN + rng->bounded(TRAIL_MAX - TRAIL_MIN + 1);
    if (anywhere)
    {
        drop.y = rng->bounded(2.0 * rows_) - rows_;
        drop.wait = 0;
    }
    else
    {
        drop.y = 0.0;
        drop.wait = rng->bounded(std::max(rows_ / 2, 1));
    }
}

void MatrixRain::wake()
{
    hudClock_.start();
}

double MatrixRain::playedMs() const
{
    return paused_ ? pausedMs_ : pausedMs_ + double(clock_.elapsed());
}

void MatrixRain::setPaused(bool paused)
{
    if (paused == paused_)
        return;
    if (paused)
        pausedMs_ += double(clock_.elapsed());
    else
    {
        clock_.start();
        frameClock_.start();
    }
    paused_ = paused;
    wake();
}

void MatrixRain::tick()
{
    switch (state_)
    {
        case State::Idle:
            timer_->stop();
            return;
        case State::Intro:
            if (introClock_.elapsed() + introShiftMs_ >= introTotalMs())
            {
                state_ = State::Waiting;
                if (pending_)
                    startRain();
            }
            break;
        case State::Waiting:
            if (pending_)
                startRain();
            break;
        case State::Rain:
        {
            if (paused_ || !recording_)
                break;
            //> The row due now follows from the wall clock alone, so the
            //> rain plays at the rate the data were measured at, however
            //> irregularly frames come. After a long stall it catches up by
            //> at most two seconds rather than racing.
            const double f = recording_->frequency;
            const int due = std::min(int(std::floor(playedMs() / 1000.0 * f)), recording_->rows);
            const int catchUp = std::max(int(2.0 * f), 1);
            if (due - consumed_ > catchUp)
                consumed_ = due - catchUp;
            while (consumed_ < due)
                step(consumed_++);
            flicker(frameClock_.isValid() ? frameClock_.restart() / 1000.0 : 0.0);

            //> It goes on until Esc: the next file is asked for well before
            //> this one ends, and changed to at the boundary. Should it not
            //> be in yet, this one plays again rather than the rain freezing.
            if (!endSignalled_ && consumed_ >= recording_->rows - int(PREFETCH_S * f))
            {
                endSignalled_ = true;
                emit nextFileRequested();
            }
            if (pending_ && (swapNow_ || consumed_ >= recording_->rows))
            {
                startRain();
            }
            else if (consumed_ >= recording_->rows)
            {
                consumed_ = 0;
                pausedMs_ = 0.0;
                clock_.start();
            }
            break;
        }
    }
    update();
}

void MatrixRain::step(int row, bool draw)
{
    const int n = scalarLabels_.size();
    const bool speedOnly = scalars_.isEmpty();
    const float u = valueAt(u_, row);
    const float v = valueAt(v_, row);
    const float w = valueAt(w_, row);
    for (int i = 0; i < n; ++i)
        rowScalars_[i] = speedOnly ? float(std::hypot(u, v)) : valueAt(scalars_.at(i), row);
    stats_.push(u, v, w, rowScalars_);

    Frame frame;
    frame.wind = stats_.hasWind() && !stats_.windGap();
    frame.u = u;
    frame.v = v;
    frame.w = w;
    frame.wz = float(stats_.wz());
    frame.z.resize(n);
    frame.bright.resize(n);
    frame.raw = rowScalars_;
    for (int i = 0; i < n; ++i)
    {
        const double z = stats_.scalarZ(i);
        frame.z[i] = float(z);
        if (std::isnan(z))
            frame.bright[i] = 0.0f;
        else if (frame.wind)
            frame.bright[i] = float(MatrixStats::brightness(stats_.wz(), z));
        else
            frame.bright[i] = float(2.0 * std::tanh(std::fabs(z)) - 1.0);
    }
    if (tsScalar_ >= 0 && !std::isnan(stats_.scalarZ(tsScalar_)))
        warm_ += 0.1 * (stats_.scalarZ(tsScalar_) - warm_);
    frame.warm = float(warm_);

    const int size = history_.size();
    history_[int(steps_ % size)] = frame;
    ++steps_;
    if (!draw)
        return;

    const double f = recording_->frequency;
    for (auto& drop : drops_)
    {
        if (drop.wait > 0)
        {
            --drop.wait;
            continue;
        }

        const Frame& seen = *frameAt(drop.x);

        // the raw wind vector moves the drop: w down, u across
        const double x0 = drop.x;
        const double y0 = drop.y;
        const double dy = drop.pace * (seen.wind ? MatrixStats::fallRate(seen.w) : NO_WIND_FALL) / f;
        const double dx = seen.wind ? MatrixStats::sideRate(seen.u) / f : 0.0;
        drop.y += dy;
        drop.x += dx;

        if (drop.y >= rows_)
        {
            spawn(drop, false);
            continue;
        }
        // rising code leaves at the top and comes back in at the bottom
        if (dy < 0.0 && drop.y < 0.0)
        {
            drop.y += rows_;
            drop.x = std::fmod(std::fmod(drop.x, cols_) + cols_, cols_);
            continue;
        }

        lay(drop, x0, y0, seen, frame, false);
        drop.x = std::fmod(std::fmod(drop.x, cols_) + cols_, cols_);
    }

    //> An eddy is air moving up or down as a body. When w surges, a parcel
    //> of code is released and carried by that surge, spreading as it goes.
    if (parcelCooldown_ > 0)
        --parcelCooldown_;
    if (frame.wind && std::fabs(frame.wz) > PARCEL_Z && parcelCooldown_ == 0)
    {
        releaseParcel(frame);
        parcelCooldown_ = std::max(int(PARCEL_GAP_S * f), 1);
    }
    moveParcels(frame);
}

//> One sample, one glyph, at the head: the glyph is the sample playing now,
//> while the movement, brightness and size come from the row the drop is
//> shown (seen), which is older downwind. Taking the glyph from that older
//> row too made a drop moving downwind with the eddy lay one value again
//> and again. A drop moving more than a cell per sample would leave holes,
//> so the cells it crossed get the same value's further digits.
void MatrixRain::lay(const Drop& drop, double x0, double y0, const Frame& seen, const Frame& now, bool parcel)
{
    const int n = scalarLabels_.size();
    const int glyphCount = glyphIndexes_[BASE_SIZE].size();
    const int s = drop.scalar % std::max(n, 1);
    const float value = now.raw.value(s, std::numeric_limits<float>::quiet_NaN());
    if (std::isnan(value))
        return;     // a gap, or a slow gas between its samples

    const int c0 = int(std::floor(x0));
    const int r0 = int(std::floor(y0));
    const int c1 = int(std::floor(drop.x));
    const int r1 = int(std::floor(drop.y));
    const int span = std::max({ std::abs(c1 - c0), std::abs(r1 - r0), 1 });
    for (int k = (y0 < 0.0 ? span : 1); k <= span; ++k)
    {
        const int r = r0 + (r1 - r0) * k / span;
        const int c = ((c0 + (c1 - c0) * k / span) % cols_ + cols_) % cols_;
        if (r < 0 || r >= rows_)
            continue;
        Cell& cell = grid_[r * cols_ + c];
        cell.glyph = qint16(MatrixStats::glyphFromValue(value, decimals_.value(s), glyphCount, span - k));
        cell.scalar = quint8(s);
        // a parcel carries the flux, so it is drawn at its brightest
        cell.bright = parcel ? qint8(2) : qint8(qRound(clamp(seen.bright.value(s), -1.0, 1.0) * 2.0));
        cell.warm = qint8(qRound(clamp(seen.warm * 2.0, -2.0, 2.0)));
        cell.size = quint8(seen.wind ? MatrixStats::sizeOf(seen.v, stats_.vScale()) : BASE_SIZE);
        cell.trail = quint8(drop.trail);
        cell.stamp = steps_;
    }
}

void MatrixRain::releaseParcel(const Frame& frame)
{
    auto* rng = QRandomGenerator::global();
    const double f = recording_->frequency;
    const bool rising = frame.wz > 0;
    // rising air starts low and climbs, sinking air starts high and plunges
    const double cx = rng->bounded(double(cols_));
    const double cy = rows_ * (rising ? 0.7 + rng->bounded(0.25) : 0.05 + rng->bounded(0.25));
    const int count = 8 + rng->bounded(7);
    const int n = std::max(int(scalarLabels_.size()), 1);
    for (int i = 0; i < count && parcels_.size() < PARCEL_MAX_DROPS; ++i)
    {
        Drop drop;
        drop.x = cx + rng->bounded(3.0) - 1.5;
        drop.y = cy + rng->bounded(3.0) - 1.5;
        drop.scalar = rng->bounded(n);
        drop.trail = TRAIL_MIN + rng->bounded(TRAIL_MAX - TRAIL_MIN + 1);
        drop.spread = (rng->bounded(2.0) - 1.0) * PARCEL_SPREAD;
        drop.pu = frame.u;
        drop.pw = frame.w;
        drop.life = int((PARCEL_LIFE_S + rng->bounded(2.0)) * f);
        parcels_.append(drop);
    }
}

// A parcel keeps the u and w that released it, losing them as it mixes
// into its surroundings, and spreads sideways meanwhile. Its glyphs are the
// raw values arriving as it travels.
void MatrixRain::moveParcels(const Frame& newest)
{
    const double f = recording_->frequency;
    for (int i = parcels_.size() - 1; i >= 0; --i)
    {
        Drop& drop = parcels_[i];
        const double x0 = drop.x;
        const double y0 = drop.y;
        const double decay = std::exp(-double(drop.age) / std::max(drop.life / 2, 1));
        drop.y += MatrixStats::fallRate(drop.pw * decay) / f;
        drop.x += (MatrixStats::sideRate(drop.pu) + drop.spread) / f;
        ++drop.age;
        if (drop.age > drop.life || drop.y < 0.0 || drop.y >= rows_)
        {
            parcels_.removeAt(i);
            continue;
        }
        lay(drop, x0, y0, newest, newest, true);
        drop.x = std::fmod(std::fmod(drop.x, cols_) + cols_, cols_);
    }
}

// In the film a stream's glyphs keep changing while it falls. Here a lit
// glyph is replaced, now and then, by the glyph of another recent raw value
// of the same variable, so whatever is on screen is always a real sample.
// The more a glyph flared (the stronger its flux term), the more it flickers.
void MatrixRain::flicker(double seconds)
{
    const int count = glyphIndexes_[BASE_SIZE].size();
    const int size = history_.size();
    const long long available = std::min<long long>(steps_ - historyBase_, size);
    if (seconds <= 0.0 || count == 0 || available <= 0)
        return;
    auto* rng = QRandomGenerator::global();
    for (auto& cell : grid_)
    {
        if (cell.glyph < 0 || steps_ - cell.stamp >= cell.trail)
            continue;
        const double rate = FLICKER_PER_S * (0.6 + 0.3 * std::abs(cell.bright));
        if (rng->generateDouble() >= rate * seconds)
            continue;
        const long long back = rng->bounded(int(available));
        const Frame& recent = history_[int((steps_ - 1 - back) % size)];
        const int glyph = MatrixStats::glyphFromValue(
            recent.raw.value(cell.scalar, std::numeric_limits<float>::quiet_NaN()),
            decimals_.value(cell.scalar), count);
        if (glyph >= 0)
            cell.glyph = qint16(glyph);
    }
}

void MatrixRain::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
        case Qt::Key_Escape:
            emit leaveRequested();
            return;
        case Qt::Key_Space:
            if (state_ == State::Intro)
                skipIntro();
            else if (state_ == State::Rain)
                setPaused(!paused_);
            update();
            return;
        case Qt::Key_N:
            if (state_ == State::Rain)
            {
                swapNow_ = true;
                if (!endSignalled_)
                {
                    endSignalled_ = true;
                    emit nextFileRequested();
                }
            }
            return;
        case Qt::Key_H:
            legend_ = !legend_;
            wake();
            update();
            return;
        default:
            break;
    }
    QWidget::keyPressEvent(event);
}

void MatrixRain::mousePressEvent(QMouseEvent* event)
{
    setFocus(Qt::MouseFocusReason);
    if (state_ == State::Intro)
        skipIntro();
    wake();
    QWidget::mousePressEvent(event);
}

void MatrixRain::mouseMoveEvent(QMouseEvent* event)
{
    wake();
    QWidget::mouseMoveEvent(event);
}

void MatrixRain::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (state_ == State::Rain)
        layoutGrid();
    else
        cols_ = rows_ = 0;
}

QColor MatrixRain::cellColour(const Cell& cell, long long age) const
{
    if (age == 0)
        return HEAD;
    const double fade = std::ceil((1.0 - double(age) / cell.trail) * 8.0) / 8.0;
    const double b = cell.bright / 2.0;
    const int n = std::max(int(scalarHue_.size()), 1);
    // warmer than its mean leans yellow, colder leans cyan
    const double hue = scalarHue_.value(cell.scalar % n, 125.0) - cell.warm * 14.0;
    const double sat = clamp(0.95 - 0.6 * std::max(b, 0.0), 0.2, 1.0);
    // going away dims, coming near brightens
    const double depth = 0.6 + 0.8 * cell.size / (MatrixStats::SIZES - 1);
    const double val = clamp(std::pow(fade, 1.3) * (0.62 + 0.38 * b) * depth, 0.05, 1.0);
    return QColor::fromHsvF(float(clamp(hue, 0.0, 359.0) / 360.0), float(sat), float(val));
}

void MatrixRain::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    if (state_ == State::Intro || state_ == State::Waiting)
        paintIntro(p);
    else if (state_ == State::Rain)
    {
        paintRain(p);
        paintHud(p);
    }
}

void MatrixRain::paintIntro(QPainter& p)
{
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPixelSize(18);
    p.setFont(font);
    p.setPen(TEXT_GREEN);

    //> One line at a time, typed out and held, then cleared for the next;
    //> after the last one only the cursor blinks until the data are in.
    QString shown;
    qint64 t = introClock_.elapsed() + introShiftMs_;
    for (const auto& line : introLines())
    {
        const int ms = lineMs(line);
        if (t < ms)
        {
            shown = line.left(int(std::min<qint64>(t / INTRO_CHAR_MS, line.size())));
            break;
        }
        t -= ms;
    }
    const bool cursorOn = (introClock_.elapsed() / 500) % 2 == 0;
    const QString text = shown + (cursorOn ? QStringLiteral("█") : QStringLiteral(" "));
    p.drawText(QPointF(28, 48), text);
}

//> Downwind is later: a drop further along u is shown the row measured a
//> little earlier, as if the eddy had been carried there (Taylor's frozen
//> turbulence), so eddies sweep across in the direction of u.
const MatrixRain::Frame* MatrixRain::frameAt(double x) const
{
    const int size = history_.size();
    const long long available = std::min<long long>(steps_ - historyBase_, size);
    if (available <= 0 || !recording_)
        return nullptr;
    const double fromUpwind = stats_.meanU() >= 0.0 ? x : cols_ - x;
    const long long lag = std::min<long long>(llround(fromUpwind * LAG_S_PER_COLUMN * recording_->frequency),
                                              available - 1);
    return &history_[int((steps_ - 1 - lag) % size)];
}

void MatrixRain::paintRain(QPainter& p)
{
    if (cols_ == 0)
        layoutGrid();

    struct Batch
    {
        QColor colour;
        int size;
        bool glow;
        QVector<quint32> glyphs;
        QVector<QPointF> positions;
    };
    QHash<quint32, int> index;
    QVector<Batch> batches;

    const double descent = rawFont_[BASE_SIZE].descent();
    for (int r = 0; r < rows_; ++r)
    {
        for (int c = 0; c < cols_; ++c)
        {
            const Cell& cell = grid_.at(r * cols_ + c);
            if (cell.glyph < 0)
                continue;
            const long long age = steps_ - cell.stamp;
            if (age >= cell.trail)
                continue;
            const int fadeLevel = age == 0 ? 15 : int(std::ceil((1.0 - double(age) / cell.trail) * 8.0));
            const quint32 key = quint32(cell.scalar & 0xF)
                                | (quint32(fadeLevel & 0xF) << 4)
                                | (quint32(cell.bright + 2) << 8)
                                | (quint32(cell.warm + 2) << 11)
                                | (quint32(cell.size) << 14);
            auto it = index.constFind(key);
            int b;
            if (it == index.constEnd())
            {
                b = batches.size();
                index.insert(key, b);
                batches.append({ cellColour(cell, age), cell.size, fadeLevel >= 7, {}, {} });
            }
            else
            {
                b = it.value();
            }
            const auto& glyphs = glyphIndexes_[cell.size];
            batches[b].glyphs.append(glyphs.value(cell.glyph % std::max(int(glyphs.size()), 1)));
            // mirrored katakana are drawn from the cell's right edge
            // a glyph larger or smaller than its cell grows from the cell's centre
            const double grow = SIZE_SCALE[cell.size] - 1.0;
            const double x = katakana_ ? -(c + 1) * cellW_ + 1.0 - grow * cellW_ / 2.0
                                       : c * cellW_ + 1.0 - grow * cellW_ / 2.0;
            batches[b].positions.append(QPointF(x, (r + 1) * cellH_ - descent + grow * cellH_ / 2.0));
        }
    }

    //> The film's code is mirrored; Latin letters and digits mirrored would
    //> only look like a rendering fault, so the fallback set stays upright.
    p.save();
    if (katakana_)
        p.scale(-1.0, 1.0);
    //> The film's heads glow: each head and the glyphs just behind it are
    //> first drawn faintly, offset a pixel each way, under the sharp glyph.
    for (const auto& batch : batches)
    {
        QGlyphRun run;
        run.setRawFont(rawFont_[batch.size]);
        run.setGlyphIndexes(batch.glyphs);
        run.setPositions(batch.positions);
        if (batch.glow)
        {
            p.setPen(GLOW);
            for (const QPointF& offset : { QPointF(-1.2, 0), QPointF(1.2, 0),
                                           QPointF(0, -1.2), QPointF(0, 1.2) })
                p.drawGlyphRun(offset, run);
        }
        p.setPen(batch.colour);
        p.drawGlyphRun(QPointF(0, 0), run);
    }
    p.restore();
}

void MatrixRain::paintHud(QPainter& p)
{
    if (!recording_)
        return;
    const qint64 since = hudClock_.isValid() ? hudClock_.elapsed() : HUD_MS;
    double alpha = legend_ ? 1.0 : clamp(double(HUD_MS - since) / HUD_FADE_MS, 0.0, 1.0);
    if (paused_)
        alpha = 1.0;
    if (alpha <= 0.0)
        return;

    p.save();
    p.setOpacity(alpha);
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPixelSize(12);
    p.setFont(font);
    const QFontMetrics fm(font);
    const int lh = fm.height();

    const double f = recording_->frequency;
    const double seconds = consumed_ / f;
    QStringList lines;
    lines << recording_->label
                 + (recording_->isSample ? tr("  (bundled sample: the project's raw data could not be read)")
                                         : QString());
    lines << tr("%1 Hz, real time%2").arg(f, 0, 'g', 4)
                 .arg(paused_ ? tr("  (paused)") : QString());
    lines << (recording_->start.isValid()
                  ? recording_->start.addMSecs(qint64(seconds * 1000.0))
                        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
                  : tr("t = %1 s").arg(seconds, 0, 'f', 1));

    const int n = scalarLabels_.size();
    const bool wind = stats_.hasWind();
    const int compass = wind ? 64 : 0;
    int width = 0;
    for (const auto& line : lines)
        width = std::max(width, int(fm.horizontalAdvance(line)));
    width = std::max(width, 260) + compass + 24;
    const int height = (int(lines.size()) + n + 2) * lh + 16;

    const QRect box(10, 10, width, height);
    p.setPen(QColor(40, 120, 60));
    p.setBrush(QColor(0, 0, 0, 200));
    p.drawRect(box);

    p.setPen(TEXT_GREEN);
    int y = box.top() + 8 + fm.ascent();
    for (const auto& line : lines)
    {
        p.drawText(box.left() + 10, y, line);
        y += lh;
    }

    // running covariance with w, each scalar in its own shade
    y += lh / 2;
    for (int i = 0; i < n; ++i)
    {
        const double cov = stats_.covariance(i);
        p.setPen(QColor::fromHsvF(float(scalarHue_.value(i) / 360.0), 0.8f, 1.0f));
        const QString arrow = !wind ? QString()
                              : cov > 0 ? QStringLiteral(" ↑") : cov < 0 ? QStringLiteral(" ↓") : QString();
        p.drawText(box.left() + 10, y,
                   wind ? QStringLiteral("w′%1′ %2%3").arg(scalarLabels_.at(i)).arg(cov, 9, 'g', 3).arg(arrow)
                        : scalarLabels_.at(i));
        y += lh;
    }
    p.setPen(QColor(40, 170, 70));
    p.drawText(box.left() + 10, y,
               wind ? tr("raw running covariance (30 s), not a corrected flux")
                    : tr("no wind in this file: the rain falls straight"));

    if (wind)
    {
        // the mean horizontal wind in the sonic's frame, and w now
        const QPointF centre(box.right() - compass / 2 - 10, box.top() + 10 + compass / 2);
        const double r = compass / 2 - 6;
        p.setPen(QPen(QColor(40, 170, 70), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(centre, r, r);
        const double theta = stats_.direction();
        const QPointF tip = centre + QPointF(r * std::cos(theta), -r * std::sin(theta));
        p.setPen(QPen(TEXT_GREEN, 2.0, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(centre, tip);
        p.setBrush(TEXT_GREEN);
        p.drawEllipse(tip, 2.5, 2.5);
        p.setPen(TEXT_GREEN);
        const QString speed = tr("%1 m/s").arg(stats_.meanSpeed(), 0, 'f', 1);
        p.drawText(QPointF(centre.x() - fm.horizontalAdvance(speed) / 2.0, centre.y() + r + lh + 2), speed);

        const double bar = clamp(stats_.wz(), -3.0, 3.0) / 3.0 * r;
        const QRectF wBar(box.right() - 8, centre.y(), 4, -bar);
        p.fillRect(wBar.normalized(), bar > 0 ? QColor(150, 255, 170) : QColor(20, 120, 50));
        p.drawText(QPointF(box.right() - 10, centre.y() + r + 2 * lh + 2), QStringLiteral("w"));
    }

    // what is what, on H
    if (legend_)
    {
        const QStringList legend {
            tr("Stream: one variable, in its shade below."),
            tr("Glyph: one raw sample, as logged; one per sample, in real time."),
            tr("Head: moves with that sample's w (up, down), u (sideways), v (size)."),
            tr("Further along u: slightly older samples, so eddies sweep across."),
            tr("Bright: carried up just then (w′s′ > 0). Dark: carried down."),
            tr("Tint: Ts warmer (yellow) or colder (cyan) than its 30 s mean."),
            tr("Burst: a surge of w releases a parcel that climbs or plunges.")
        };
        int lw = 0;
        for (const auto& line : legend)
            lw = std::max(lw, int(fm.horizontalAdvance(line)));
        const QRect lbox(10, this->height() - (int(legend.size()) + 1) * lh - 20, lw + 20,
                         (int(legend.size()) + 1) * lh + 10);
        p.setPen(QColor(40, 120, 60));
        p.setBrush(QColor(0, 0, 0, 200));
        p.drawRect(lbox);
        p.setPen(TEXT_GREEN);
        int ly = lbox.top() + 6 + fm.ascent();
        for (const auto& line : legend)
        {
            p.drawText(lbox.left() + 10, ly, line);
            ly += lh;
        }
        int lx = lbox.left() + 10;
        for (int i = 0; i < n; ++i)
        {
            p.setPen(QColor::fromHsvF(float(scalarHue_.value(i) / 360.0), 0.9f, 0.95f));
            p.drawText(lx, ly, scalarLabels_.at(i));
            lx += fm.horizontalAdvance(scalarLabels_.at(i)) + 14;
        }
    }

    const QString keys = tr("Space pause · N next file · H legend · Esc leave");
    p.setPen(QColor(40, 170, 70));
    p.drawText(this->width() - fm.horizontalAdvance(keys) - 12, this->height() - 10, keys);
    p.restore();
}

////////////////////////////////////////////////////////////////////////////////
// MatrixPanel

MatrixPanel::MatrixPanel(DlProject* dlProject, EcProject* ecProject, QWidget* parent) :
    QWidget(parent),
    dlProject_(dlProject),
    ecProject_(ecProject),
    rain_(new MatrixRain),
    fileIndex_(-1),
    generation_(0),
    started_(false),
    projectFailed_(false)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(rain_, &MatrixRain::leaveRequested, this, &MatrixPanel::finished);
    connect(rain_, &MatrixRain::nextFileRequested, this, &MatrixPanel::loadNext);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(rain_, 1);
    layout->addWidget(EggUi::makeText(
        tr("Your own raw data, at the speed it was measured. Space pauses, N plays the next raw "
           "file, H shows the legend, and Esc takes the blue pill.")));

    // how the screen fills, so what is on it can be read, on demand
    auto howTo = EggUi::makeText(tr("ⓘ How to read it"));
    howTo->setCursor(Qt::WhatsThisCursor);
    howTo->setToolTip(tr(
        "<b>How to read it</b>"
        "<ul>"
        "<li><b>Streams.</b> Each falling stream carries one variable of the file (CO₂, H₂O, "
        "sonic temperature, CH₄, …), each in its own shade of green.</li>"
        "<li><b>Glyphs.</b> Every sample the logger wrote adds one glyph to the head of every "
        "stream, in real time: at 10 Hz, ten a second. The glyph is that sample's raw value as "
        "logged. Glyphs stay where they were laid and fade; while lit they now and then switch "
        "to another recent raw value of the same variable. A gap in the data leaves a gap.</li>"
        "<li><b>Movement.</b> The head moves with that sample's raw wind: w carries it down or "
        "up (it stands still where w is zero), u carries it sideways, and v makes the glyphs "
        "larger as if coming towards you, or smaller going away.</li>"
        "<li><b>Eddies.</b> Streams further along u show slightly older samples, as if the air "
        "measured at the sonic had been carried there, so an eddy sweeps across the screen. "
        "When w surges, a parcel of bright code is released and climbs or plunges with it, "
        "spreading as it goes.</li>"
        "<li><b>Brightness.</b> Bright glyphs are being carried up at that moment (w′s′ above "
        "zero), dark ones carried down. Sonic temperature tints the code yellow where the air "
        "is warmer than its last 30 s, cyan where it is colder.</li>"
        "</ul>"
        "Nothing here is processed: the values are the raw file's, and the covariances in the "
        "corner are running 30 s averages, not fluxes."));
    layout->addWidget(howTo);
}

void MatrixPanel::reset()
{
    ++generation_;
    started_ = false;
    projectFailed_ = false;
    files_.clear();
    fileIndex_ = -1;
    rain_->stop();
}

void MatrixPanel::keyPressEvent(QKeyEvent* event)
{
    // Esc leaves wherever the focus is on the panel
    if (event->key() == Qt::Key_Escape)
    {
        emit finished();
        return;
    }
    QWidget::keyPressEvent(event);
}

void MatrixPanel::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (!event->spontaneous() && !started_)
        start();
}

void MatrixPanel::start()
{
    started_ = true;
    ++generation_;
    rain_->playIntro();

    files_.clear();
    fileIndex_ = -1;
    if (ecProject_ && dlProject_ && !ecProject_->screenDataPath().isEmpty())
    {
        const auto type = ecProject_->generalFileType();
        QString pattern;
        if (type == Defs::RawFileType::GHG)
        {
            pattern = QStringLiteral("*.") + Defs::GHG_NATIVE_DATA_FILE_EXT;
        }
        else if (type == Defs::RawFileType::ASCII)
        {
            const auto& prototype = ecProject_->generalFilePrototype();
            pattern = QStringLiteral("*.") + prototype.mid(prototype.lastIndexOf(QLatin1Char('.')) + 1);
        }
        if (!pattern.isEmpty())
        {
            files_ = FileUtils::getFiles(ecProject_->screenDataPath(), pattern,
                                         ecProject_->screenRecurse());
            files_.sort();
        }
    }
    loadNext();
}

void MatrixPanel::loadNext()
{
    if (files_.isEmpty() || projectFailed_)
    {
        loadSample();
        return;
    }
    fileIndex_ = (fileIndex_ + 1) % files_.size();
    load(files_.at(fileIndex_));
}

void MatrixPanel::load(const QString& file)
{
    //> From a shared drive the listing names files not downloaded yet
    const QString local = RemoteSource::ensureLocal(file, this);
    if (local.isEmpty())
    {
        projectFailed_ = true;
        loadSample();
        return;
    }

    const bool ghg = ecProject_->generalFileType() == Defs::RawFileType::GHG;
    RawPlayback::AsciiFormat format;
    if (!ghg)
    {
        format.separator = dlProject_->fieldSep();
        format.headerRows = dlProject_->headerRows();
        format.frequency = dlProject_->acquisitionFrequency();
        format.filenamePrototype = ecProject_->generalFilePrototype();
        for (const auto& var : *dlProject_->variables())
            format.columns.append(engineName(var));
    }

    const int generation = generation_;
    auto watcher = new QFutureWatcher<Recording>(this);
    connect(watcher, &QFutureWatcher<Recording>::finished, this, [this, watcher, generation]()
    {
        Recording recording = watcher->result();
        watcher->deleteLater();
        if (generation != generation_)
            return;     // left the page, or started again, meanwhile
        if (!recording.isValid())
        {
            projectFailed_ = true;
            loadSample();
            return;
        }
        rain_->setRecording(std::make_shared<const Recording>(std::move(recording)));
    });
    watcher->setFuture(QtConcurrent::run([local, ghg, format]()
    {
        return ghg ? RawPlayback::readGhg(local) : RawPlayback::readAscii(local, format);
    }));
}

void MatrixPanel::loadSample()
{
    Recording sample = RawPlayback::readSample();
    if (sample.isValid())
        rain_->setRecording(std::make_shared<const Recording>(std::move(sample)));
}
