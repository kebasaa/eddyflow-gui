/***************************************************************************
  eastereggpage.cpp
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

#include "eastereggpage.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <initializer_list>
#include <limits>
#include <numeric>

#include "despikearena.h"

namespace {

////////////////////////////////////////////////////////////////////////////////
// Doom

const QStringList& doomSteps()
{
    static const QStringList steps {
        QStringLiteral("Loading DOOM.WAD…"),
        QStringLiteral("Despiking demons…"),
        QStringLiteral("Applying WPL correction to the plasma rifle…"),
        QStringLiteral("Rotating coordinates into hell (double rotation)…"),
        QStringLiteral("Computing the footprint of the Cyberdemon…")
    };
    return steps;
}

const QStringList& doomSentences()
{
    static const QStringList sentences {
        QStringLiteral("The only demons you're allowed to fight today are spikes in your w′ time series."),
        QStringLiteral("Your flux footprint is not supposed to extend into Hell."),
        QStringLiteral("The Cyberdemon failed the stationarity test anyway."),
        QStringLiteral("BFG stands for Big Flux Gap. Go fill yours."),
        QStringLiteral("No rip and tear: only despike and detrend."),
        QStringLiteral("Your u* filter says it's too calm for demon slaying."),
        QStringLiteral("The only thing to shoot at today is the energy balance closure."),
        QStringLiteral("Hell is warm, but the WPL correction still applies."),
        QStringLiteral("Keycard required: blue for Basic Settings, red for Advanced."),
        QStringLiteral("Nightmare difficulty is reserved for gap-filling winter nighttime data.")
    };
    return sentences;
}

// A rank tier holds a few titles and sentence templates. The templates take
// placeholders: {n} despiked count, {n_spikes} / {e_spikes} counted nouns,
// and {tool}, {place}, {who}, which are filled in at random.
struct RankTier
{
    int maxDespiked;
    QStringList titles;
    QStringList templates;
};

const RankTier& diedTier()
{
    static const RankTier tier {
        0,
        { QStringLiteral("Reviewer 2's Favourite Example"),
          QStringLiteral("Cautionary Tale"),
          QStringLiteral("Quality Flag 2 Incarnate") },
        { QStringLiteral("The spikes overran your w′ series after only {n_spikes} despiked. {who} has been informed."),
          QStringLiteral("{e_spikes} escaped into {place}. Not even {tool} can save this dataset now."),
          QStringLiteral("You fell after {n_spikes}. The data will be published anyway, with a long footnote.") }
    };
    return tier;
}

const QList<RankTier>& rankTiers()
{
    static const QList<RankTier> tiers {
        { 2,
          { QStringLiteral("Summer Intern"),
            QStringLiteral("Lab Tourist"),
            QStringLiteral("Unpaid Field Assistant") },
          { QStringLiteral("{n_spikes} despiked, {e_spikes} escaped into {place}. Have you tried {tool}?"),
            QStringLiteral("Only {n}? {who} hoped for more, but at least you didn't unplug the logger."),
            QStringLiteral("With {n} despiked, {place} now looks like a hedgehog. Consider {tool}.") } },
        { 6,
          { QStringLiteral("PhD Student"),
            QStringLiteral("Survivor of Thesis Chapter 2"),
            QStringLiteral("Junior Flux Wrangler") },
          { QStringLiteral("{n_spikes} despiked. {who} calls it a promising start and wants a draft by Friday."),
            QStringLiteral("{e_spikes} still got into {place}, but {tool} wouldn't have done any better."),
            QStringLiteral("{n_spikes} down. Your thesis now has a methods section.") } },
        { 11,
          { QStringLiteral("Postdoc of Doom"),
            QStringLiteral("Flux Tower Veteran"),
            QStringLiteral("Keeper of the Sonic") },
          { QStringLiteral("{n_spikes} despiked. {who} wants you on the next grant proposal."),
            QStringLiteral("Only {e_spikes} escaped into {place}. You and {tool} make a fine team."),
            QStringLiteral("{n_spikes} removed by hand. Who needs {tool}?") } },
        { 16,
          { QStringLiteral("Senior Scientist"),
            QStringLiteral("Principal Investigator"),
            QStringLiteral("Chair of the QC Committee") },
          { QStringLiteral("{n_spikes} despiked. {place} has never looked this clean."),
            QStringLiteral("{who} will cite your {n_spikes} in their next keynote."),
            QStringLiteral("With {n_spikes} despiked, you've made {tool} obsolete.") } },
        { std::numeric_limits<int>::max(),
          { QStringLiteral("Tenured Doom Slayer"),
            QStringLiteral("Lord of the Ogive"),
            QStringLiteral("Sonic Whisperer") },
          { QStringLiteral("{n_spikes} despiked! {place} is spotless and {who} is speechless."),
            QStringLiteral("{n_spikes} despiked, {e_spikes} escaped. Every flux network wants to hire you."),
            QStringLiteral("Legend says {tool} was named after you.") } }
    };
    return tiers;
}

const QStringList& rankTools()
{
    static const QStringList tools {
        QStringLiteral("a median filter"),
        QStringLiteral("a MAD-based despiking routine"),
        QStringLiteral("a 3.5-sigma threshold"),
        QStringLiteral("the Vickers & Mahrt tests"),
        QStringLiteral("a very patient undergrad")
    };
    return tools;
}

const QStringList& rankPlaces()
{
    static const QStringList places {
        QStringLiteral("your cospectra"),
        QStringLiteral("the ogive"),
        QStringLiteral("the 03:30 half-hour"),
        QStringLiteral("your annual carbon budget"),
        QStringLiteral("the footprint model")
    };
    return places;
}

const QStringList& rankBlamers()
{
    static const QStringList who {
        QStringLiteral("Your supervisor"),
        QStringLiteral("Reviewer 2"),
        QStringLiteral("The site PI"),
        QStringLiteral("The cows")
    };
    return who;
}

QString spikes(int count)
{
    return count == 1 ? QStringLiteral("1 spike")
                      : QStringLiteral("%1 spikes").arg(count);
}

QString pickAny(const QStringList& list)
{
    return list.at(QRandomGenerator::global()->bounded(static_cast<int>(list.size())));
}

struct Rank
{
    QString title;
    QString sentence;
};

Rank doomRank(int despiked, int escaped, bool died)
{
    const RankTier* tier = &diedTier();
    if (!died)
    {
        for (const auto& candidate : rankTiers())
        {
            if (despiked <= candidate.maxDespiked)
            {
                tier = &candidate;
                break;
            }
        }
    }

    QString sentence = pickAny(tier->templates);
    // {who} is capitalised in the list for when it opens a sentence, and
    // lower-cased (bar proper names) when it lands mid-sentence
    QString who = pickAny(rankBlamers());
    const auto whoAt = sentence.indexOf(QLatin1String("{who}"));
    const bool opensSentence = whoAt <= 0
                               || (whoAt >= 2 && QStringLiteral(".?!").contains(sentence.at(whoAt - 2)));
    if (!opensSentence && who != QLatin1String("Reviewer 2"))
        who[0] = who.at(0).toLower();

    sentence.replace(QLatin1String("{n_spikes}"), spikes(despiked));
    sentence.replace(QLatin1String("{e_spikes}"), spikes(escaped));
    sentence.replace(QLatin1String("{n}"), QString::number(despiked));
    sentence.replace(QLatin1String("{tool}"), pickAny(rankTools()));
    sentence.replace(QLatin1String("{place}"), pickAny(rankPlaces()));
    sentence.replace(QLatin1String("{who}"), who);

    // a placeholder at the very start leaves a lower-case first letter
    if (!sentence.isEmpty())
        sentence[0] = sentence.at(0).toUpper();

    return { pickAny(tier->titles), sentence };
}

////////////////////////////////////////////////////////////////////////////////
// Hitchhiker's Guide

const QStringList& thinkingSteps()
{
    static const QStringList steps {
        QStringLiteral("Pondering the Ultimate Question…"),
        QStringLiteral("Integrating cospectra over 7.5 million years…"),
        QStringLiteral("Filtering Vogon poetry out of the high-frequency range…"),
        QStringLiteral("Towel detected. Proceeding…"),
        QStringLiteral("Engaging the Infinite Improbability Drive…")
    };
    return steps;
}

// rich text; the Vogon fragment is kept short and always attributed
const QStringList& guideEntries()
{
    static const QStringList entries {
        QStringLiteral("<i>“Oh freddled gruntbuggly, thy micturations are to me…”</i><br>"
                       "— Prostetnic Vogon Jeltz, in Douglas Adams, "
                       "<i>The Hitchhiker's Guide to the Galaxy</i> (1979)<br>"
                       "Still easier to read than your raw file headers."),
        QStringLiteral("Don't panic. But do check your time lag."),
        QStringLiteral("Your sonic anemometer is mostly harmless."),
        QStringLiteral("So long, and thanks for all the fluxes."),
        QStringLiteral("Always know where your towel is. And your raw data backup."),
        QStringLiteral("The Infinite Improbability Drive can't explain your nighttime CO₂ uptake either."),
        QStringLiteral("Time is an illusion. Averaging periods doubly so."),
        QStringLiteral("Life? Don't talk to me about life. Talk to me about stationarity."),
        QStringLiteral("The Babel fish translated your .ghg file. It says: go back to Basic Settings."),
        QStringLiteral("The Earth was built to measure fluxes. Nobody told the Vogons about the energy balance gap.")
    };
    return entries;
}

struct Topic
{
    QStringList keywords;
    QString unit;
    QStringList replies;
};

const QList<Topic>& topics()
{
    static const QList<Topic> list {
        { { QStringLiteral("co2"), QStringLiteral("co₂"), QStringLiteral("ch4"),
            QStringLiteral("methane"), QStringLiteral("flux") },
          QStringLiteral("µmol m⁻² s⁻¹"),
          { QStringLiteral("Exactly what your ecosystem was doing before the cows walked through the footprint."),
            QStringLiteral("Deep Thought is unsure about the sign convention, but very sure about the 42."),
            QStringLiteral("Upward or downward: Deep Thought was built to find the answer, not the direction.") } },
        { { QStringLiteral("energy balance"), QStringLiteral("closure"), QStringLiteral("close") },
          QStringLiteral("W m⁻²"),
          { QStringLiteral("That's how far short of closure you are. It will close the day the Vogons "
                           "approve your hyperspace bypass. Check the storage term anyway."),
            QStringLiteral("Missing. Deep Thought suspects the Ravenous Bugblatter Beast of Traal.") } },
        { { QStringLiteral("time lag"), QStringLiteral("lag") },
          QStringLiteral("ms"),
          { QStringLiteral("That is your time lag. And yes, it's probably the tube length."),
            QStringLiteral("Deep Thought took 7.5 million years to answer, so it has no right to judge your time lag.") } },
        { { QStringLiteral("u*"), QStringLiteral("ustar"), QStringLiteral("friction velocity"),
            QStringLiteral("night"), QStringLiteral("turbulen") },
          QStringLiteral("cm s⁻¹"),
          { QStringLiteral("Below the threshold. Deep Thought filters your question out as insufficient turbulence."),
            QStringLiteral("At night even Deep Thought can't find any turbulence. Neither can your sonic.") } },
        { { QStringLiteral("footprint"), QStringLiteral("fetch"), QStringLiteral("upwind") },
          QStringLiteral("m"),
          { QStringLiteral("That's how far upwind your footprint reaches, just past the Restaurant at the End of the Universe."),
            QStringLiteral("Your footprint extends to a small planet near Betelgeuse. Check your wind direction filter.") } },
        { { QStringLiteral("gap"), QStringLiteral("fill"), QStringLiteral("missing") },
          QStringLiteral("% of your data"),
          { QStringLiteral("Gaps, filled by the Infinite Improbability Drive. Results may be improbable."),
            QStringLiteral("Missing. Deep Thought recommends MDS gap-filling and a strong cup of tea.") } },
        { { QStringLiteral("rotation"), QStringLiteral("planar fit"), QStringLiteral("tilt"),
            QStringLiteral("coordinate") },
          QStringLiteral("degrees"),
          { QStringLiteral("Deep Thought double-rotated your question until it pointed at the answer."),
            QStringLiteral("Deep Thought planar-fitted the whole galaxy. It came out slightly tilted.") } },
        { { QStringLiteral("spike"), QStringLiteral("despik"), QStringLiteral("noise"),
            QStringLiteral("outlier") },
          QStringLiteral("spikes"),
          { QStringLiteral("Most of them caused by Marvin being depressed near the sonic."),
            QStringLiteral("Deep Thought has removed them. It feels slightly better now. Only slightly.") } },
        { { QStringLiteral("spectr"), QStringLiteral("frequency"), QStringLiteral("correction"),
            QStringLiteral("filter") },
          QStringLiteral("Hz"),
          { QStringLiteral("The frequency at which Vogon poetry peaks. Apply a low-pass filter immediately."),
            QStringLiteral("Deep Thought corrected your spectra so thoroughly they now describe a different planet.") } },
        { { QStringLiteral("thesis"), QStringLiteral("phd"), QStringLiteral("paper"),
            QStringLiteral("review"), QStringLiteral("deadline"), QStringLiteral("supervisor"),
            QStringLiteral("publish") },
          QStringLiteral("days until your deadline"),
          { QStringLiteral("Deep Thought recommends a towel, and less time spent on easter eggs."),
            QStringLiteral("Reviewer 2 has already read your question and requested major revisions.") } }
    };
    return list;
}

const QStringList& fallbackReplies()
{
    static const QStringList replies {
        QStringLiteral("Deep Thought is confident. Your data, less so."),
        QStringLiteral("Deep Thought has checked it very thoroughly. You won't like it."),
        QStringLiteral("The answer was easy. The question is the hard part, and you're not quite there yet."),
        QStringLiteral("Deep Thought suggests you process your fluxes while it thinks of a better question."),
        QStringLiteral("That's all the Guide has to say on the matter. The rest of the entry reads: mostly harmless."),
        QStringLiteral("Deep Thought could explain, but you'd need a computer the size of a planet to understand it.")
    };
    return replies;
}

const QStringList& randomUnits()
{
    static const QStringList units {
        QStringLiteral("µmol m⁻² s⁻¹"),
        QStringLiteral("W m⁻²"),
        QStringLiteral("ms of time lag"),
        QStringLiteral("% energy-balance gap"),
        QStringLiteral("half-hourly records"),
        QStringLiteral("degrees of coordinate rotation"),
        QStringLiteral("Pa"),
        QStringLiteral("cups of tea")
    };
    return units;
}

QString normalizedQuestion(const QString& question)
{
    return question.simplified().toLower();
}

// Deep Thought never changes its mind: the same question is always given
// the same answer, because the generator is seeded from the question.
QString deepThoughtAnswer(const QString& question)
{
    const QString q = normalizedQuestion(question);
    if (q.isEmpty())
        return QStringLiteral("You have to actually ask something. "
                              "Deep Thought has waited 7.5 million years for this.");

    QRandomGenerator rng(static_cast<quint32>(qHash(q)));
    auto pick = [&rng](const QStringList& list)
    {
        return list.at(rng.bounded(static_cast<int>(list.size())));
    };

    QString answer;
    if (!q.endsWith(QLatin1Char('?')))
        answer += QStringLiteral("That wasn't strictly a question, but… ");

    if (q.contains(QLatin1String("life"))
        || q.contains(QLatin1String("universe"))
        || q.contains(QLatin1String("everything")))
    {
        return answer + QStringLiteral("<b>42.</b> Deep Thought checked it very thoroughly. "
                                       "The problem is that you never actually knew what the question was.");
    }

    const Topic* topic = nullptr;
    for (const auto& candidate : topics())
    {
        for (const auto& keyword : candidate.keywords)
        {
            if (q.contains(keyword))
            {
                topic = &candidate;
                break;
            }
        }
        if (topic)
            break;
    }

    const QString unit = topic ? topic->unit : pick(randomUnits());
    const QString reply = topic ? pick(topic->replies) : pick(fallbackReplies());
    answer += QStringLiteral("<b>42 %1.</b> %2").arg(unit.toHtmlEscaped(), reply.toHtmlEscaped());

    if (rng.bounded(3) == 0)
        answer += QStringLiteral("<br><br><i>The Guide adds:</i> ") + pick(guideEntries());

    return answer;
}

const int MAX_QUESTIONS = 3;

////////////////////////////////////////////////////////////////////////////////
// Dungeons & Dragons

struct DndScenario
{
    QString dmText;
    QString check;
    int dc;
    QString passText;
    QString failText;
};

const QList<DndScenario>& dndScenarios()
{
    static const QList<DndScenario> scenarios {
        { QStringLiteral("A wild Reviewer 2 emerges from behind the flux tower!"),
          QStringLiteral("for Initiative"), 12,
          QStringLiteral("You act first and resubmit before Reviewer 2 finishes the abstract."),
          QStringLiteral("Reviewer 2 goes first and demands a full reprocessing with planar fit instead of double rotation.") },
        { QStringLiteral("You climb the tower. Something about the sonic anemometer feels… off."),
          QStringLiteral("Perception"), 14,
          QStringLiteral("You spot a spider web between the transducers. You evict the spider, and your spikes vanish."),
          QStringLiteral("You notice nothing. The spider stays and becomes your dominant turbulent eddy.") },
        { QStringLiteral("It is 3 a.m., −20 °C, and the IRGA heater has failed."),
          QStringLiteral("a Constitution Saving Throw"), 15,
          QStringLiteral("You endure the cold and wipe the frost off the optics. The nighttime fluxes are saved. Mostly."),
          QStringLiteral("You retreat to the hut. The window fogs over and the whole night gets flagged 2.") },
        { QStringLiteral("An ancient scroll describes a forbidden ritual known only as 'WPL'."),
          QStringLiteral("Arcana"), 13,
          QStringLiteral("You grasp density fluctuations. Your CO₂ flux changes sign. Nobody believes you."),
          QStringLiteral("You cast the correction twice. Your fluxes now exist in another plane.") },
        { QStringLiteral("The funding agency's envoy eyes your energy balance with suspicion."),
          QStringLiteral("Persuasion"), 16,
          QStringLiteral("'It'll close this time, I promise.' Somehow it works. Funding secured."),
          QStringLiteral("Your energy balance gap is 20%. So is your budget cut.") },
        { QStringLiteral("A herd of cows approaches your guy wires."),
          QStringLiteral("Animal Handling"), 11,
          QStringLiteral("The cows wander downwind, out of your footprint. Your methane fluxes thank you."),
          QStringLiteral("The cows settle right inside your footprint. Congratulations on the record CH₄ emissions.") },
        { QStringLiteral("The data logger demands a riddle before it gives up its files."),
          QStringLiteral("Intelligence"), 14,
          QStringLiteral("'What has a mean of zero but is never nothing?' 'w′.' The logger opens."),
          QStringLiteral("You answer 'u*'. The logger formats its SD card.") },
        { QStringLiteral("The Lich of Gap-Filling offers you infinite data in exchange for your soul."),
          QStringLiteral("Wisdom"), 13,
          QStringLiteral("You refuse. Your gaps stay honest."),
          QStringLiteral("You accept. Your annual budget is now 100% modelled.") }
    };
    return scenarios;
}

////////////////////////////////////////////////////////////////////////////////
// widgets and timing

const int COLUMN_MAX_WIDTH = 640;
const int DOOM_STEP_MS = 600;
const int THINK_STEP_MS = 450;
const int LOADING_HOLD_MS = 1000;
const int ROLL_FLICKER_MS = 80;
const int ROLL_FLICKER_TICKS = 12;

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

QLabel* makeText(const QString& text = QString())
{
    auto label = new QLabel(text);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setTextFormat(Qt::RichText);
    return label;
}

QPushButton* makeButton(const QString& text = QString())
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

int rollD20()
{
    return QRandomGenerator::global()->bounded(1, 21);
}

} // namespace

EasterEggPage::EasterEggPage(QWidget *parent) :
    QWidget(parent),
    variant_(Variant::Doom),
    rollTicks_(0),
    scenario_(0),
    answersGiven_(0),
    loadingBar_(nullptr),
    loadingStatus_(nullptr),
    loadingStep_(0)
{
    positions_.fill(0);

    loadingTimer_ = new QTimer(this);
    connect(loadingTimer_, &QTimer::timeout, this, &EasterEggPage::advanceLoading);

    holdTimer_ = new QTimer(this);
    holdTimer_->setSingleShot(true);
    holdTimer_->setInterval(LOADING_HOLD_MS);
    connect(holdTimer_, &QTimer::timeout, this, [this]()
    {
        auto done = std::move(loadingDone_);
        loadingDone_ = nullptr;
        if (done)
            done();
    });

    rollTimer_ = new QTimer(this);
    rollTimer_->setInterval(ROLL_FLICKER_MS);
    connect(rollTimer_, &QTimer::timeout, this, &EasterEggPage::advanceRoll);

    doomPanel_ = createDoomPanel();
    hitchhikerPanel_ = createHitchhikerPanel();
    dicePanel_ = createDicePanel();

    //> Only the active panel is visible, so the others take no space. A
    //> plain box layout (unlike a stacked widget) passes height-for-width
    //> through, which is what word-wrapped labels need to get their height.
    column_ = new QWidget;
    column_->setMaximumWidth(COLUMN_MAX_WIDTH);
    column_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto columnLayout = new QVBoxLayout(column_);
    columnLayout->setContentsMargins(0, 0, 0, 0);
    columnLayout->addWidget(doomPanel_);
    columnLayout->addWidget(hitchhikerPanel_);
    columnLayout->addWidget(dicePanel_);

    // centre with stretches, not alignment, so the column gets a real width
    auto row = new QHBoxLayout;
    row->addStretch(1);
    row->addWidget(column_, 10);
    row->addStretch(1);

    auto content = new QWidget;
    auto contentLayout = new QVBoxLayout(content);
    contentLayout->addStretch(1);
    contentLayout->addLayout(row);
    contentLayout->addStretch(2);

    // a small window scrolls rather than cutting text off
    auto scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
    scroll->viewport()->setAutoFillBackground(false);
    content->setAutoFillBackground(false);
    scroll->setWidget(content);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scroll);

    setVariant(Variant::Doom);
}

QString EasterEggPage::tabText(Variant variant)
{
    switch (variant)
    {
        case Variant::Doom:
            return QStringLiteral("Doom");
        case Variant::Hitchhiker:
            return QStringLiteral("Deep Thought");
        case Variant::DnD:
        case Variant::Count:
            break;
    }
    return QStringLiteral("Dungeon");
}

void EasterEggPage::setVariant(Variant variant)
{
    variant_ = variant;
    resetAll();

    doomPanel_->setVisible(variant == Variant::Doom);
    hitchhikerPanel_->setVisible(variant == Variant::Hitchhiker);
    dicePanel_->setVisible(variant == Variant::DnD);

    if (variant == Variant::DnD)
    {
        scenario_ = nextIndex(variant, dndScenarios().size());
        const auto& scenario = dndScenarios().at(scenario_);
        dmLabel_->setText(tr("<b>Dungeon Master:</b> <i>%1</i>").arg(scenario.dmText.toHtmlEscaped()));
        rollButton_->setText(tr("Roll %1").arg(scenario.check));
    }
}

void EasterEggPage::hideEvent(QHideEvent *event)
{
    //> Leaving the page (not minimising the window) abandons whatever was
    //> running, without emitting finished(), so the tab stays available.
    if (!event->spontaneous())
        resetAll();
    QWidget::hideEvent(event);
}

void EasterEggPage::resetAll()
{
    stopLoading();
    resetDoomPanel();
    resetHitchhikerPanel();
    resetDicePanel();
}

////////////////////////////////////////////////////////////////////////////////
// shared fake loading

void EasterEggPage::runLoading(QProgressBar *bar, QLabel *status, const QStringList &steps,
                               int stepMs, std::function<void()> done)
{
    stopLoading();
    loadingBar_ = bar;
    loadingStatus_ = status;
    loadingSteps_ = steps;
    loadingStep_ = 0;
    loadingDone_ = std::move(done);

    loadingBar_->setValue(0);
    loadingBar_->setVisible(true);
    loadingStatus_->setVisible(true);

    advanceLoading();
    loadingTimer_->start(stepMs);
}

void EasterEggPage::advanceLoading()
{
    if (loadingStep_ < loadingSteps_.size())
    {
        loadingStatus_->setText(loadingSteps_.at(loadingStep_));
        ++loadingStep_;
        loadingBar_->setValue(static_cast<int>(90 * loadingStep_ / loadingSteps_.size()));
        return;
    }

    // almost there... the classic
    loadingTimer_->stop();
    loadingBar_->setValue(99);
    holdTimer_->start();
}

void EasterEggPage::stopLoading()
{
    loadingTimer_->stop();
    holdTimer_->stop();
    loadingDone_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// Doom

QWidget* EasterEggPage::createDoomPanel()
{
    launchButton_ = makeButton(tr("Launch Doom"));
    connect(launchButton_, &QPushButton::clicked, this, &EasterEggPage::launchDoom);

    doomBar_ = makeBar();
    doomStatus_ = makeText();

    arena_ = new DespikeArena;
    connect(arena_, &DespikeArena::gameOver, this, &EasterEggPage::showGameResult);

    doomRank_ = makeText();
    auto rankFont = doomRank_->font();
    rankFont.setPointSize(rankFont.pointSize() + 5);
    rankFont.setBold(true);
    doomRank_->setFont(rankFont);
    doomStats_ = makeText();
    doomTag_ = makeText();

    auto playAgain = makeButton(tr("Play again"));
    connect(playAgain, &QPushButton::clicked, this, &EasterEggPage::startGame);
    auto back = makeButton(tr("Back to the fluxes"));
    connect(back, &QPushButton::clicked, this, [this]()
    {
        resetDoomPanel();
        emit finished();
    });

    doomResult_ = new QWidget;
    auto resultLayout = new QVBoxLayout(doomResult_);
    resultLayout->setContentsMargins(0, 0, 0, 0);
    resultLayout->setSpacing(12);
    resultLayout->addWidget(doomRank_);
    resultLayout->addWidget(doomStats_);
    resultLayout->addWidget(doomTag_);
    resultLayout->addLayout(centredRow({ playAgain, back }));

    auto panel = new QWidget;
    auto layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    layout->addWidget(makeTitle(QStringLiteral("Doom")));
    layout->addWidget(makeText(tr("Despike or die. Click the demons before they escape into your data.")));
    layout->addLayout(centredRow({ launchButton_ }));
    layout->addWidget(doomBar_);
    layout->addWidget(doomStatus_);
    layout->addWidget(arena_);
    layout->addWidget(doomResult_);
    return panel;
}

void EasterEggPage::launchDoom()
{
    launchButton_->setEnabled(false);
    runLoading(doomBar_, doomStatus_, doomSteps(), DOOM_STEP_MS, [this]()
    {
        startGame();
    });
}

void EasterEggPage::startGame()
{
    launchButton_->setVisible(false);
    doomBar_->setVisible(false);
    doomStatus_->setVisible(false);
    doomResult_->setVisible(false);
    arena_->setVisible(true);
    arena_->start();
}

void EasterEggPage::showGameResult(int despiked, int escaped, int shots, bool died)
{
    const int accuracy = qRound(100.0 * despiked / std::max(shots, 1));

    const Rank rank = doomRank(despiked, escaped, died);
    doomRank_->setText(tr("Rank: %1").arg(rank.title.toHtmlEscaped()));
    doomStats_->setText(QStringLiteral("%1<br>%2")
                            .arg(rank.sentence.toHtmlEscaped(),
                                 tr("Despiked %1, escaped %2, accuracy %3%.")
                                     .arg(despiked).arg(escaped).arg(accuracy)));
    doomTag_->setText(QStringLiteral("<i>%1</i>").arg(
        doomSentences().at(nextIndex(Variant::Doom, doomSentences().size())).toHtmlEscaped()));

    arena_->setVisible(false);
    doomResult_->setVisible(true);
}

void EasterEggPage::resetDoomPanel()
{
    arena_->stop();
    arena_->setVisible(false);
    doomResult_->setVisible(false);
    doomBar_->setVisible(false);
    doomStatus_->clear();
    doomStatus_->setVisible(false);
    launchButton_->setEnabled(true);
    launchButton_->setVisible(true);
}

////////////////////////////////////////////////////////////////////////////////
// Hitchhiker's Guide

QWidget* EasterEggPage::createHitchhikerPanel()
{
    //> The unlock filter ignores keys typed into a line edit, so typing
    //> "eddy" into the question can't re-trigger the easter egg.
    questionEdit_ = new QLineEdit;
    questionEdit_->setPlaceholderText(tr("Ask Deep Thought anything…"));
    questionEdit_->setMaxLength(200);
    questionEdit_->setMinimumWidth(320);
    connect(questionEdit_, &QLineEdit::returnPressed, this, &EasterEggPage::askDeepThought);

    askButton_ = makeButton(tr("Ask Deep Thought"));
    connect(askButton_, &QPushButton::clicked, this, &EasterEggPage::askDeepThought);

    thinkBar_ = makeBar();
    thinkStatus_ = makeText();

    answerFrame_ = makeFrame();
    questionEcho_ = makeText();
    answerLabel_ = makeText();
    auto answerFont = answerLabel_->font();
    answerFont.setPointSize(answerFont.pointSize() + 2);
    answerLabel_->setFont(answerFont);
    auto answerLayout = new QVBoxLayout(answerFrame_);
    answerLayout->setContentsMargins(16, 12, 16, 12);
    answerLayout->setSpacing(10);
    answerLayout->addWidget(questionEcho_);
    answerLayout->addWidget(answerLabel_);

    anotherButton_ = makeButton(tr("Ask another question"));
    connect(anotherButton_, &QPushButton::clicked, this, [this]()
    {
        resetQuestion();
        questionEdit_->setFocus();
    });
    backToWorkButton_ = makeButton(tr("Don't panic, back to work"));
    connect(backToWorkButton_, &QPushButton::clicked, this, [this]()
    {
        resetHitchhikerPanel();
        emit finished();
    });

    auto panel = new QWidget;
    auto layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    layout->addWidget(makeTitle(QStringLiteral("Deep Thought")));
    layout->addWidget(makeText(tr("DON'T PANIC. Ask the Ultimate Question.")));
    layout->addLayout(centredRow({ questionEdit_, askButton_ }));
    layout->addWidget(thinkBar_);
    layout->addWidget(thinkStatus_);
    layout->addWidget(answerFrame_);
    layout->addLayout(centredRow({ anotherButton_, backToWorkButton_ }));
    return panel;
}

void EasterEggPage::askDeepThought()
{
    if (!askButton_->isVisible())
        return;

    const QString question = questionEdit_->text();
    questionEdit_->setVisible(false);
    askButton_->setVisible(false);

    runLoading(thinkBar_, thinkStatus_, thinkingSteps(), THINK_STEP_MS, [this, question]()
    {
        thinkBar_->setVisible(false);
        thinkStatus_->setVisible(false);
        showAnswer(question);
    });
}

void EasterEggPage::showAnswer(const QString &question)
{
    const QString key = normalizedQuestion(question);
    QString answer = deepThoughtAnswer(question);

    if (key.isEmpty())
    {
        questionEcho_->setText(tr("<i>(silence)</i>"));
    }
    else
    {
        questionEcho_->setText(QStringLiteral("<i>“%1”</i>").arg(question.trimmed().toHtmlEscaped()));
        if (askedQuestions_.contains(key))
            answer += tr("<br><br>You asked that already. The answer hasn't changed.");
        askedQuestions_.insert(key);
        ++answersGiven_;
    }

    const bool exhausted = answersGiven_ >= MAX_QUESTIONS;
    if (exhausted)
        answer += tr("<br><br><b>Deep Thought is now busy designing an even greater computer "
                     "to find the Question. Please return to your fluxes.</b>");

    answerLabel_->setText(answer);
    answerFrame_->setVisible(true);
    anotherButton_->setVisible(!exhausted);
    backToWorkButton_->setVisible(true);
}

void EasterEggPage::resetQuestion()
{
    answerFrame_->setVisible(false);
    anotherButton_->setVisible(false);
    backToWorkButton_->setVisible(false);
    questionEdit_->clear();
    questionEdit_->setVisible(true);
    askButton_->setVisible(true);
}

void EasterEggPage::resetHitchhikerPanel()
{
    answersGiven_ = 0;
    thinkBar_->setVisible(false);
    thinkStatus_->clear();
    thinkStatus_->setVisible(false);
    resetQuestion();
}

////////////////////////////////////////////////////////////////////////////////
// Dungeons & Dragons

QWidget* EasterEggPage::createDicePanel()
{
    auto dmFrame = makeFrame();
    dmLabel_ = new QLabel;
    dmLabel_->setWordWrap(true);
    dmLabel_->setTextFormat(Qt::RichText);
    auto dmLayout = new QVBoxLayout(dmFrame);
    dmLayout->setContentsMargins(16, 12, 16, 12);
    dmLayout->addWidget(dmLabel_);

    rollButton_ = makeButton();
    connect(rollButton_, &QPushButton::clicked, this, &EasterEggPage::roll);

    rollResult_ = makeText();
    auto resultFont = rollResult_->font();
    resultFont.setPointSize(resultFont.pointSize() + 4);
    rollResult_->setFont(resultFont);

    rollOutcome_ = makeText();

    continueButton_ = makeButton(tr("Continue"));
    connect(continueButton_, &QPushButton::clicked, this, [this]()
    {
        resetDicePanel();
        emit finished();
    });

    auto panel = new QWidget;
    auto layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    layout->addWidget(makeTitle(QStringLiteral("The Flux Dungeon")));
    layout->addWidget(makeText(tr("Your party: one sonic anemometer, one gas analyser, "
                                  "one sleep-deprived PhD student.")));
    layout->addWidget(dmFrame);
    layout->addLayout(centredRow({ rollButton_ }));
    layout->addWidget(rollResult_);
    layout->addWidget(rollOutcome_);
    layout->addLayout(centredRow({ continueButton_ }));
    return panel;
}

void EasterEggPage::roll()
{
    rollButton_->setEnabled(false);
    rollResult_->setVisible(true);
    rollTicks_ = 0;
    advanceRoll();
    rollTimer_->start();
}

void EasterEggPage::advanceRoll()
{
    if (rollTicks_ < ROLL_FLICKER_TICKS)
    {
        rollResult_->setText(QStringLiteral("🎲 %1").arg(rollD20()));
        ++rollTicks_;
        return;
    }

    rollTimer_->stop();
    finishRoll();
}

void EasterEggPage::finishRoll()
{
    const auto& scenario = dndScenarios().at(scenario_);
    const int result = rollD20();
    const bool passed = result == 20 || (result != 1 && result >= scenario.dc);

    QString rolled;
    if (result == 20)
        rolled = tr("<b>NATURAL 20!</b>");
    else if (result == 1)
        rolled = tr("<b>NATURAL 1!</b>");
    else
        rolled = tr("You rolled <b>%1</b>").arg(result);

    rollResult_->setText(QStringLiteral("🎲 %1 (DC %2): %3")
                             .arg(rolled)
                             .arg(scenario.dc)
                             .arg(passed ? tr("<b>Success!</b>") : tr("<b>Failure.</b>")));
    rollOutcome_->setText((passed ? scenario.passText : scenario.failText).toHtmlEscaped());
    rollOutcome_->setVisible(true);

    rollButton_->setVisible(false);
    continueButton_->setVisible(true);
}

void EasterEggPage::resetDicePanel()
{
    rollTimer_->stop();
    rollButton_->setEnabled(true);
    rollButton_->setVisible(true);
    rollResult_->clear();
    rollResult_->setVisible(false);
    rollOutcome_->clear();
    rollOutcome_->setVisible(false);
    continueButton_->setVisible(false);
}

int EasterEggPage::nextIndex(Variant variant, int size)
{
    const auto v = static_cast<int>(variant);
    auto& order = orders_[v];
    auto& position = positions_[v];

    if (position >= order.size())
    {
        order.resize(size);
        std::iota(order.begin(), order.end(), 0);
        std::shuffle(order.begin(), order.end(), *QRandomGenerator::global());
        position = 0;
    }
    return order.at(position++);
}
