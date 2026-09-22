/***************************************************************************
  eastereggwidgets.cpp
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

#include "eastereggwidgets.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QRandomGenerator>
#include <QTimer>

#include <algorithm>
#include <numeric>

namespace {

const int LOADING_HOLD_MS = 1000;

// {tool}: what could have been used against the spikes
const QStringList& tools()
{
    static const QStringList list {
        QStringLiteral("a median filter"),
        QStringLiteral("a MAD-based despiking routine"),
        QStringLiteral("a 3.5-sigma threshold"),
        QStringLiteral("the Vickers & Mahrt tests"),
        QStringLiteral("a very patient undergrad"),
        QStringLiteral("a moving-window standard deviation"),
        QStringLiteral("a Hampel filter"),
        QStringLiteral("the Mauder & Foken flags"),
        QStringLiteral("a despiking routine written in 1998"),
        QStringLiteral("an Excel macro nobody understands"),
        QStringLiteral("a strongly worded email to the manufacturer"),
        QStringLiteral("a second opinion from the technician"),
        QStringLiteral("a rolling median of doubtful provenance"),
        QStringLiteral("the delete key"),
        QStringLiteral("a spike detector trained on one good day"),
        QStringLiteral("a threshold chosen by squinting"),
        QStringLiteral("a Kalman filter nobody can tune"),
        QStringLiteral("a 5-sigma rule and a prayer"),
        QStringLiteral("a wavelet transform"),
        QStringLiteral("a spectral correction from 1990"),
        QStringLiteral("a stationarity test"),
        QStringLiteral("an integral turbulence test"),
        QStringLiteral("a skewness and kurtosis check"),
        QStringLiteral("a rubber mallet"),
        QStringLiteral("a histogram and good intentions"),
        QStringLiteral("the undo button"),
        QStringLiteral("a despiking plug-in with three GitHub stars"),
        QStringLiteral("a Python script called final_v7.py"),
        QStringLiteral("an R package with no documentation"),
        QStringLiteral("a MATLAB licence that expired yesterday")
    };
    return list;
}

// {place}: where spikes and lost data end up
const QStringList& places()
{
    static const QStringList list {
        QStringLiteral("your cospectra"),
        QStringLiteral("the ogive"),
        QStringLiteral("the 03:30 half-hour"),
        QStringLiteral("your annual carbon budget"),
        QStringLiteral("the footprint model"),
        QStringLiteral("the energy balance residual"),
        QStringLiteral("your time-lag histogram"),
        QStringLiteral("the nighttime NEE"),
        QStringLiteral("the storage term"),
        QStringLiteral("your u* threshold"),
        QStringLiteral("the planar-fit coefficients"),
        QStringLiteral("the latent heat flux"),
        QStringLiteral("your methane budget"),
        QStringLiteral("the sensible heat flux"),
        QStringLiteral("the Webb correction"),
        QStringLiteral("your quality flags"),
        QStringLiteral("the gap-filling look-up table"),
        QStringLiteral("the half-hour before sunrise"),
        QStringLiteral("the raw data archive"),
        QStringLiteral("your PhD thesis"),
        QStringLiteral("the site's metadata file"),
        QStringLiteral("the conference poster"),
        QStringLiteral("the flux network's database"),
        QStringLiteral("your co-authors' inboxes"),
        QStringLiteral("the spectral correction factors"),
        QStringLiteral("the random uncertainty estimate"),
        QStringLiteral("your supervisor's favourite figure"),
        QStringLiteral("the monthly report"),
        QStringLiteral("the annual sums"),
        QStringLiteral("the diurnal cycle plot")
    };
    return list;
}

// {who}, capitalised for when it opens a sentence
const QStringList& blamers()
{
    static const QStringList list {
        QStringLiteral("Your supervisor"),
        QStringLiteral("Reviewer 2"),
        QStringLiteral("The site PI"),
        QStringLiteral("The cows"),
        QStringLiteral("The technician"),
        QStringLiteral("The flux network coordinator"),
        QStringLiteral("The funding agency"),
        QStringLiteral("Your co-author"),
        QStringLiteral("The department head"),
        QStringLiteral("The farmer next door"),
        QStringLiteral("The postdoc"),
        QStringLiteral("The data manager"),
        QStringLiteral("The spider in the sonic"),
        QStringLiteral("The instrument manufacturer"),
        QStringLiteral("Your office mate"),
        QStringLiteral("The student who set up the tower"),
        QStringLiteral("The editor"),
        QStringLiteral("The IT department"),
        QStringLiteral("The conference audience"),
        QStringLiteral("Reviewer 3"),
        QStringLiteral("The modelling group"),
        QStringLiteral("The remote sensing people"),
        QStringLiteral("The nightly cron job"),
        QStringLiteral("Your thesis committee"),
        QStringLiteral("The ICOS assessor"),
        QStringLiteral("The FLUXNET database"),
        QStringLiteral("The grant officer"),
        QStringLiteral("The tower itself"),
        QStringLiteral("The raccoon in the hut"),
        QStringLiteral("Your future self")
    };
    return list;
}

// {who} entries that keep their capital letter mid-sentence
const QStringList& keptCase()
{
    static const QStringList list {
        QStringLiteral("Reviewer 2"),
        QStringLiteral("Reviewer 3"),
        QStringLiteral("ICOS"),
        QStringLiteral("FLUXNET")
    };
    return list;
}

} // namespace

namespace EggUi {

QLabel* makeTitle(const QString& text)
{
    auto label = new QLabel(text);
    auto font = label->font();
    font.setPointSize(font.pointSize() + 12);
    font.setBold(true);
    label->setFont(font);
    label->setAlignment(Qt::AlignCenter);
    return label;
}

QLabel* makeText(const QString& text)
{
    auto label = new QLabel(text);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setTextFormat(Qt::RichText);
    return label;
}

QPushButton* makeButton(const QString& text)
{
    auto button = new QPushButton(text);
    button->setProperty("commonButton2", true);
    return button;
}

QProgressBar* makeBar()
{
    auto bar = new QProgressBar;
    bar->setObjectName(QStringLiteral("mainProgress"));
    bar->setRange(0, 100);
    return bar;
}

QFrame* makeFrame()
{
    auto frame = new QFrame;
    frame->setFrameShape(QFrame::StyledPanel);
    return frame;
}

QHBoxLayout* centredRow(std::initializer_list<QWidget*> widgets)
{
    auto row = new QHBoxLayout;
    row->addStretch(1);
    for (auto widget : widgets)
        row->addWidget(widget);
    row->addStretch(1);
    return row;
}

QString pickAny(const QStringList& list)
{
    return list.at(QRandomGenerator::global()->bounded(static_cast<int>(list.size())));
}

QString pickFresh(const QStringList& list)
{
    static QHash<const QStringList*, ShuffleBag> bags;
    return list.at(bags[&list].next(list.size()));
}

QStringList sample(const QStringList& list, int n)
{
    QStringList shuffled = list;
    std::shuffle(shuffled.begin(), shuffled.end(), *QRandomGenerator::global());
    return shuffled.mid(0, n);
}

QString fillFlavour(QString sentence, const QHash<QString, QString>& values)
{
    // {who} is lower-cased (bar proper names) when it lands mid-sentence
    QString who = pickFresh(blamers());
    const auto whoAt = sentence.indexOf(QLatin1String("{who}"));
    const bool opensSentence = whoAt <= 0
                               || (whoAt >= 2 && QStringLiteral(".?!").contains(sentence.at(whoAt - 2)));
    const bool keepsCase = std::any_of(keptCase().cbegin(), keptCase().cend(),
                                       [&who](const QString& name) { return who.contains(name); });
    if (!opensSentence && !keepsCase)
        who[0] = who.at(0).toLower();

    for (auto it = values.cbegin(); it != values.cend(); ++it)
        sentence.replace(QLatin1Char('{') + it.key() + QLatin1Char('}'), it.value());
    sentence.replace(QLatin1String("{tool}"), pickFresh(tools()));
    sentence.replace(QLatin1String("{place}"), pickFresh(places()));
    sentence.replace(QLatin1String("{who}"), who);

    // a placeholder at the very start leaves a lower-case first letter
    if (!sentence.isEmpty())
        sentence[0] = sentence.at(0).toUpper();
    return sentence;
}

int ShuffleBag::next(int size)
{
    if (size <= 0)
        return 0;

    if (position_ >= order_.size() || order_.size() != size)
    {
        order_.resize(size);
        std::iota(order_.begin(), order_.end(), 0);
        std::shuffle(order_.begin(), order_.end(), *QRandomGenerator::global());
        position_ = 0;
    }
    return order_.at(position_++);
}

} // namespace EggUi

FakeLoading::FakeLoading(QObject *parent) :
    QObject(parent),
    bar_(nullptr),
    status_(nullptr),
    step_(0)
{
    stepTimer_ = new QTimer(this);
    connect(stepTimer_, &QTimer::timeout, this, &FakeLoading::advance);

    holdTimer_ = new QTimer(this);
    holdTimer_->setSingleShot(true);
    holdTimer_->setInterval(LOADING_HOLD_MS);
    connect(holdTimer_, &QTimer::timeout, this, [this]()
    {
        auto done = std::move(done_);
        done_ = nullptr;
        if (done)
            done();
    });
}

void FakeLoading::run(QProgressBar *bar, QLabel *status, const QStringList &steps,
                      int stepMs, std::function<void()> done)
{
    stop();
    bar_ = bar;
    status_ = status;
    steps_ = steps;
    step_ = 0;
    done_ = std::move(done);

    bar_->setValue(0);
    bar_->setVisible(true);
    status_->setVisible(true);

    advance();
    stepTimer_->start(stepMs);
}

void FakeLoading::advance()
{
    if (step_ < steps_.size())
    {
        status_->setText(steps_.at(step_));
        ++step_;
        bar_->setValue(static_cast<int>(90 * step_ / steps_.size()));
        return;
    }

    // almost there... the classic
    stepTimer_->stop();
    bar_->setValue(99);
    holdTimer_->start();
}

void FakeLoading::stop()
{
    stepTimer_->stop();
    holdTimer_->stop();
    done_ = nullptr;
}
