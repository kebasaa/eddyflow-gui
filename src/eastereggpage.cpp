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

#include <QHBoxLayout>
#include <QHideEvent>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

#include "deepthoughtpanel.h"
#include "despikearena.h"
#include "dndpanel.h"

using namespace EggUi;

namespace {

////////////////////////////////////////////////////////////////////////////////
// Doom

// loading lines; each run shows five of them
// loading lines; each run shows five of them
const QStringList& doomSteps()
{
    static const QStringList list {
        QStringLiteral("Loading DOOM.WAD…"),
        QStringLiteral("Despiking demons…"),
        QStringLiteral("Applying WPL correction to the plasma rifle…"),
        QStringLiteral("Rotating coordinates into hell (double rotation)…"),
        QStringLiteral("Computing the footprint of the Cyberdemon…"),
        QStringLiteral("Calibrating the chainsaw against span gas…"),
        QStringLiteral("Detrending the Imp population…"),
        QStringLiteral("Estimating the time lag between shotgun and splatter…"),
        QStringLiteral("Planar-fitting the floor of Hell…"),
        QStringLiteral("Heating the IRGA to 666 °C…"),
        QStringLiteral("Applying a high-frequency correction to the BFG…"),
        QStringLiteral("Converting demons to µmol m⁻² s⁻¹…"),
        QStringLiteral("Checking the Barons of Hell for stationarity…"),
        QStringLiteral("Loading the blue keycard (sonic calibration file)…"),
        QStringLiteral("Filling gaps in the demon population with MDS…"),
        QStringLiteral("Removing the u* threshold from Hell…"),
        QStringLiteral("Synchronising the sonic with the rocket launcher…"),
        QStringLiteral("Spectrally correcting the screams…"),
        QStringLiteral("Unpacking the .ghg files of the damned…"),
        QStringLiteral("Averaging each demon over thirty minutes…"),
        QStringLiteral("Cleaning demon blood off the IRGA window…"),
        QStringLiteral("Aligning the sonic with the north gate of Hell…"),
        QStringLiteral("Computing the ogive of the Spider Mastermind…"),
        QStringLiteral("Flagging the Cacodemons as quality 2…"),
        QStringLiteral("Correcting the Cyberdemon for high-frequency losses…"),
        QStringLiteral("Tilt-correcting the Icon of Sin…"),
        QStringLiteral("Measuring the sensible heat flux of the lava…"),
        QStringLiteral("Filling the ammo storage term…"),
        QStringLiteral("Waking the Lost Souls from stable nighttime conditions…"),
        QStringLiteral("Checking the energy balance of Hell (it never closes)…")
    };
    return list;
}

// tag lines under the result
const QStringList& doomSentences()
{
    static const QStringList list {
        QStringLiteral("The only demons you're allowed to fight today are spikes in your w′ time series."),
        QStringLiteral("Your flux footprint is not supposed to extend into Hell."),
        QStringLiteral("The Cyberdemon failed the stationarity test anyway."),
        QStringLiteral("BFG stands for Big Flux Gap. Go fill yours."),
        QStringLiteral("No rip and tear: only despike and detrend."),
        QStringLiteral("Your u* filter says it's too calm for demon slaying."),
        QStringLiteral("The only thing to shoot at today is the energy balance closure."),
        QStringLiteral("Hell is warm, but the WPL correction still applies."),
        QStringLiteral("Keycard required: blue for Basic Settings, red for Advanced."),
        QStringLiteral("Nightmare difficulty is reserved for gap-filling winter nighttime data."),
        QStringLiteral("The Spider Mastermind has eight legs and still better spatial coverage than your tower."),
        QStringLiteral("Rip and tear your data? No: rotate and correct."),
        QStringLiteral("Your nighttime fluxes are scarier than any Cacodemon."),
        QStringLiteral("Even the Barons of Hell apply a density correction."),
        QStringLiteral("Hell has a closed energy balance. It's the only place that does."),
        QStringLiteral("IDKFA gives you every key. It still won't unlock your logger."),
        QStringLiteral("The Imps prefer Reynolds decomposition to fireballs."),
        QStringLiteral("Real demons don't sit in your footprint. Cows do."),
        QStringLiteral("The BFG 9000 fires one enormous flux. Please don't publish it."),
        QStringLiteral("Nobody has ever despiked the Icon of Sin. Nobody."),
        QStringLiteral("Your time lag is longer than a Doom level loading on a 386."),
        QStringLiteral("The Cyberdemon's footprint covers the whole map. Yours covers a car park."),
        QStringLiteral("Lost Souls are just half-hours without a quality flag."),
        QStringLiteral("Demons spawn at random. So do the gaps in your data."),
        QStringLiteral("The shotgun is just a median filter with attitude."),
        QStringLiteral("The gate of Hell is guarded by a quality flag."),
        QStringLiteral("Only the Doom Slayer and PhD students work without sleep."),
        QStringLiteral("Your cospectra look like a Doom level map. That's not a compliment."),
        QStringLiteral("Every demon you shot was a spike. Every spike you missed is in your paper."),
        QStringLiteral("Nightmare difficulty: processing a whole year of data the week before the deadline.")
    };
    return list;
}

// A rank tier holds titles and sentence templates. The templates take
// placeholders: {n} despiked count, {n_spikes} / {e_spikes} / {v_points}
// counted nouns, and {tool}, {place}, {who}, which are filled in at random.
struct RankTier
{
    int maxScore;
    QStringList titles;
    QStringList templates;
};

const RankTier& diedTier()
{
    static const RankTier tier
    { 0,
      {
        QStringLiteral("Reviewer 2's Favourite Example"),
        QStringLiteral("Cautionary Tale"),
        QStringLiteral("Quality Flag 2 Incarnate"),
        QStringLiteral("Spike Casualty"),
        QStringLiteral("Overrun by Outliers"),
        QStringLiteral("Former Flux Slayer"),
        QStringLiteral("Demon Chow"),
        QStringLiteral("Case Study in the Methods Section"),
        QStringLiteral("Victim of Non-Stationarity"),
        QStringLiteral("Lost in the Noise"),
        QStringLiteral("Buried in the Residual"),
        QStringLiteral("Gap in the Record"),
        QStringLiteral("Fallen at the 03:30 Half-Hour"),
        QStringLiteral("Statistically Insignificant"),
        QStringLiteral("Rejected Without Review"),
        QStringLiteral("Missing Value"),
        QStringLiteral("Sensor Offline"),
        QStringLiteral("Error Bar Personified"),
        QStringLiteral("Flagged and Filtered"),
        QStringLiteral("Awaiting Reprocessing"),
        QStringLiteral("Deleted by Quality Control"),
        QStringLiteral("Victim of the Spider Mastermind"),
        QStringLiteral("Out of Warranty"),
        QStringLiteral("Ghost in the Logger"),
        QStringLiteral("Below the Detection Limit"),
        QStringLiteral("Unrecoverable Half-Hour"),
        QStringLiteral("Corrupted .ghg File"),
        QStringLiteral("Casualty of the Cow Herd"),
        QStringLiteral("Footnote in the Annual Report"),
        QStringLiteral("Dropped from the Author List")
      },
      {
        QStringLiteral("The spikes overran your w′ series after only {n_spikes} despiked. {who} has been informed."),
        QStringLiteral("{e_spikes} escaped into {place}. Not even {tool} can save this dataset now."),
        QStringLiteral("You fell after {n_spikes}. The data will be published anyway, with a long footnote."),
        QStringLiteral("Your health hit zero with {e_spikes} still loose in {place}."),
        QStringLiteral("{who} found your remains next to the sonic, clutching {tool}."),
        QStringLiteral("The demons won. {place} belongs to them now."),
        QStringLiteral("You despiked {n}, but the {e_spikes} that escaped were a bit much. {who} sends condolences."),
        QStringLiteral("Game over. {tool} was right there, and you never used it."),
        QStringLiteral("Your w′ is now mostly demon. {who} wants to know why the fluxes doubled."),
        QStringLiteral("You held out for {n_spikes}. Then {place} went dark."),
        QStringLiteral("The spikes ate your health, then your deadline. {who} is not surprised."),
        QStringLiteral("{e_spikes} escaped and set up camp in {place}."),
        QStringLiteral("Down you go, with {n_spikes} to your name. Next time, bring {tool}."),
        QStringLiteral("The tower is now haunted by {e_spikes}. {who} refuses to climb it."),
        QStringLiteral("You fought bravely. {place} did not."),
        QStringLiteral("The data manager marks your run as unrecoverable. {who} agrees."),
        QStringLiteral("{n_spikes} despiked, one PhD lost. {who} will write the obituary in the methods section."),
        QStringLiteral("Your last words were \"it's probably just noise\". It wasn't."),
        QStringLiteral("The demons flagged you as a 2 and removed you from the dataset."),
        QStringLiteral("{e_spikes} got through. {place} will never be the same."),
        QStringLiteral("You should have trusted {tool}. {who} did, and they're fine."),
        QStringLiteral("Overrun after {n_spikes}. Your fluxes are now mostly fiction."),
        QStringLiteral("{who} reprocessed your data with {tool}. It made no difference."),
        QStringLiteral("The spikes escaped into {place}, and so did your sanity."),
        QStringLiteral("Your run ends here. The spikes are celebrating in {place}."),
        QStringLiteral("{e_spikes} made it into the annual budget. Your carbon sink is now a source."),
        QStringLiteral("You despiked {n}. The Cyberdemon despiked you."),
        QStringLiteral("Health: 0%. Spikes: plenty. {who}: disappointed."),
        QStringLiteral("The demons sent your data to {place} and your manuscript to Reviewer 2."),
        QStringLiteral("You'll respawn tomorrow. The spikes in {place} already have.")
      } };
    return tier;
}

const QList<RankTier>& rankTiers()
{
    static const QList<RankTier> tiers {
        { 5,
          {
            QStringLiteral("Summer Intern"),
            QStringLiteral("Lab Tourist"),
            QStringLiteral("Unpaid Field Assistant"),
            QStringLiteral("Work Experience Student"),
            QStringLiteral("Visitor with a Lanyard"),
            QStringLiteral("First-Week Master's Student"),
            QStringLiteral("Person Holding the Ladder"),
            QStringLiteral("Honorary Cable Tie"),
            QStringLiteral("Apprentice Button Presser"),
            QStringLiteral("Occasional Tower Climber"),
            QStringLiteral("Keeper of the Spare Screws"),
            QStringLiteral("Trainee Despiker"),
            QStringLiteral("Spike Spectator"),
            QStringLiteral("Lost Undergraduate"),
            QStringLiteral("Assistant to the Assistant"),
            QStringLiteral("Clipboard Holder"),
            QStringLiteral("Probationary Flux Enthusiast"),
            QStringLiteral("Tower Tourist"),
            QStringLiteral("Novice of the Noise"),
            QStringLiteral("Fresh Graduate"),
            QStringLiteral("Hut Sweeper"),
            QStringLiteral("Junior Guy-Wire Inspector"),
            QStringLiteral("Learner Driver of the Logger"),
            QStringLiteral("Casual Observer"),
            QStringLiteral("Weekend Volunteer"),
            QStringLiteral("Squire of the Sonic"),
            QStringLiteral("Page of the Processing Queue"),
            QStringLiteral("Data Entry Temp"),
            QStringLiteral("Enthusiastic Amateur"),
            QStringLiteral("Tea Maker to the Tower Crew")
          },
          {
            QStringLiteral("{n_spikes} despiked, {e_spikes} escaped into {place}. Have you tried {tool}?"),
            QStringLiteral("Only {n}? {who} hoped for more, but at least you didn't unplug the logger."),
            QStringLiteral("With {n} despiked, {place} now looks like a hedgehog. Consider {tool}."),
            QStringLiteral("{n_spikes}. It's a start. {who} has seen worse. Not often, but they have."),
            QStringLiteral("You despiked {n}. The rest are in {place}, waving."),
            QStringLiteral("{who} suggests {tool} and a long rest."),
            QStringLiteral("{e_spikes} got away. {place} will need a lot of {tool}."),
            QStringLiteral("A modest {n_spikes}. The tower thanks you for your enthusiasm."),
            QStringLiteral("You've discovered that spikes move. {who} is proud of this breakthrough."),
            QStringLiteral("{n_spikes} despiked. Most of your shots went into the sky, which is technically upwind."),
            QStringLiteral("{who} watched the whole thing and quietly reached for {tool}."),
            QStringLiteral("Your despiking has the precision of {tool} run with the wrong settings."),
            QStringLiteral("With {n} despiked, {place} is only slightly haunted."),
            QStringLiteral("{e_spikes} escaped. {who} is updating the risk assessment."),
            QStringLiteral("You got {n_spikes}. The demons are calling it a moral victory."),
            QStringLiteral("Keep practising. {tool} took years to get this good."),
            QStringLiteral("{n_spikes} removed. The rest are now peer-reviewing each other."),
            QStringLiteral("{who} says it looks fine, but they were looking at {place}."),
            QStringLiteral("Your aim is roughly as good as a sonic with a spider in it."),
            QStringLiteral("{n_spikes}: enough for one very short methods paragraph."),
            QStringLiteral("{e_spikes} are now living rent-free in {place}."),
            QStringLiteral("{who} asks if you'd like to try the tutorial level. It's called \"calibration\"."),
            QStringLiteral("You found the trigger. Next time, find the demons."),
            QStringLiteral("{n_spikes} despiked. The rest have been reclassified as natural variability."),
            QStringLiteral("{place} is still full of spikes, but you tried, and that's what matters to {who}."),
            QStringLiteral("{tool} would have got at least twice as many. Just saying."),
            QStringLiteral("A gentle {n_spikes}. The demons barely noticed."),
            QStringLiteral("{e_spikes} escaped. They've already emailed {who}."),
            QStringLiteral("You despiked {n}, which is {n} more than the default settings manage."),
            QStringLiteral("{who} awards you a participation certificate, printed on the back of an old calibration sheet.")
          } },
        { 12,
          {
            QStringLiteral("PhD Student"),
            QStringLiteral("Survivor of Thesis Chapter 2"),
            QStringLiteral("Junior Flux Wrangler"),
            QStringLiteral("Second-Year Despiker"),
            QStringLiteral("Methods Section Author"),
            QStringLiteral("Keeper of the Raw Files"),
            QStringLiteral("Night-Shift Observer"),
            QStringLiteral("Certified Tower Climber"),
            QStringLiteral("Holder of the Hut Key"),
            QStringLiteral("Apprentice Micrometeorologist"),
            QStringLiteral("Wrangler of the Time Lag"),
            QStringLiteral("Assistant Keeper of the Ogive"),
            QStringLiteral("Conference Poster Presenter"),
            QStringLiteral("First-Author Hopeful"),
            QStringLiteral("Spike Hunter, Second Class"),
            QStringLiteral("Guardian of the Logger"),
            QStringLiteral("Squire of the Spectral Correction"),
            QStringLiteral("Journeyman Despiker"),
            QStringLiteral("Friend of the Technician"),
            QStringLiteral("Operator of the Median Filter"),
            QStringLiteral("Junior Keeper of the Sonic"),
            QStringLiteral("Summer School Graduate"),
            QStringLiteral("Master's Student of Distinction"),
            QStringLiteral("Early-Career Flux Enthusiast"),
            QStringLiteral("Collector of Half-Hours"),
            QStringLiteral("Reader of the Manual"),
            QStringLiteral("Trusted Cable Carrier"),
            QStringLiteral("Apprentice to the Data Manager"),
            QStringLiteral("Adjuster of the Guy Wires"),
            QStringLiteral("Cadet of the Covariance")
          },
          {
            QStringLiteral("{n_spikes} despiked. {who} calls it a promising start and wants a draft by Friday."),
            QStringLiteral("{e_spikes} still got into {place}, but {tool} wouldn't have done any better."),
            QStringLiteral("{n_spikes} down. Your thesis now has a methods section."),
            QStringLiteral("{who} nods slowly, which counts as high praise."),
            QStringLiteral("A respectable {n_spikes}. {place} is noticeably cleaner."),
            QStringLiteral("You despiked {n}. {tool} would be proud, if it had feelings."),
            QStringLiteral("{e_spikes} escaped, which your co-authors will call \"within the uncertainty\"."),
            QStringLiteral("{n_spikes}! That's a whole figure's worth. {who} wants it in colour."),
            QStringLiteral("You've outperformed {tool}. Please don't tell it."),
            QStringLiteral("{n_spikes} despiked. {place} will only need minor revisions."),
            QStringLiteral("{who} forwards your score to the whole lab with the subject line \"see?\"."),
            QStringLiteral("Solid work. The remaining {e_spikes} are someone else's problem."),
            QStringLiteral("{n_spikes} removed. You may now call yourself a despiker, but only in the methods section."),
            QStringLiteral("{place} looks almost publishable. Almost."),
            QStringLiteral("With {n} despiked, you've earned a second coffee. {who} is buying."),
            QStringLiteral("{e_spikes} got away, but they looked scared."),
            QStringLiteral("{n_spikes}. Your supervisor would say \"good\", then ask for twice as many."),
            QStringLiteral("{tool} and you: a partnership for the ages."),
            QStringLiteral("You despiked {n}. The spikes have started a support group."),
            QStringLiteral("{who} would like to cite your score in their review."),
            QStringLiteral("{n_spikes} despiked. The demons have added you to their watchlist."),
            QStringLiteral("{place} thanks you. It had been getting noisy."),
            QStringLiteral("{e_spikes} escaped into {place}. You'll get them in the next round of revisions."),
            QStringLiteral("A good day on the tower: {n_spikes} and all ten fingers."),
            QStringLiteral("{who} says it's the best despiking they've seen since {tool}."),
            QStringLiteral("{n_spikes}. You're getting the hang of this. Terrifying."),
            QStringLiteral("You despiked {n}, roughly one per chapter of your thesis."),
            QStringLiteral("{place} is now clean enough to show at a conference."),
            QStringLiteral("{e_spikes} escaped. {who} has kindly offered to look into it."),
            QStringLiteral("{n_spikes} down, one thesis to go.")
          } },
        { 20,
          {
            QStringLiteral("Postdoc of Doom"),
            QStringLiteral("Flux Tower Veteran"),
            QStringLiteral("Keeper of the Sonic"),
            QStringLiteral("Knight of the Planar Fit"),
            QStringLiteral("Warden of the Footprint"),
            QStringLiteral("Spike Slayer, First Class"),
            QStringLiteral("Master of the Median"),
            QStringLiteral("Veteran of the Night Shift"),
            QStringLiteral("Guardian of the Ogive"),
            QStringLiteral("Lord of the Logger"),
            QStringLiteral("Time-Lag Tamer"),
            QStringLiteral("Senior Tower Climber"),
            QStringLiteral("Champion of the Cospectra"),
            QStringLiteral("Captain of the Covariance"),
            QStringLiteral("Ranger of the Raw Data"),
            QStringLiteral("Paladin of the Quality Flags"),
            QStringLiteral("Keeper of the Calibration Gas"),
            QStringLiteral("Defender of the Diurnal Cycle"),
            QStringLiteral("Scourge of the Outliers"),
            QStringLiteral("Sentinel of Stationarity"),
            QStringLiteral("Warden of the Webb Correction"),
            QStringLiteral("Protector of the Half-Hour"),
            QStringLiteral("Commander of the Gap-Filling"),
            QStringLiteral("Custodian of the Closure"),
            QStringLiteral("Hunter of the High Frequencies"),
            QStringLiteral("Tamer of Turbulence"),
            QStringLiteral("Vanquisher of Noise"),
            QStringLiteral("Guardian of the Guy Wires"),
            QStringLiteral("Marshal of the Metadata"),
            QStringLiteral("Keeper of the Sacred Spreadsheet")
          },
          {
            QStringLiteral("{n_spikes} despiked. {who} wants you on the next grant proposal."),
            QStringLiteral("Only {e_spikes} escaped into {place}. You and {tool} make a fine team."),
            QStringLiteral("{n_spikes} removed by hand. Who needs {tool}?"),
            QStringLiteral("{place} hasn't been this clean since the tower went up."),
            QStringLiteral("{n_spikes}! {who} has started calling you \"the despiker\"."),
            QStringLiteral("You despiked {n} while {tool} was still loading."),
            QStringLiteral("{e_spikes} got away, and they'll be telling stories about you."),
            QStringLiteral("{who} asked you to review {tool}. You gave it two stars."),
            QStringLiteral("{n_spikes}. The demons have requested a transfer to another site."),
            QStringLiteral("{place} is so clean it squeaks. {who} is suspicious."),
            QStringLiteral("With {n} despiked, your fluxes finally look like the textbook."),
            QStringLiteral("{n_spikes} despiked. The spikes now avoid your tower on purpose."),
            QStringLiteral("{who} offers you co-authorship. And the night shift."),
            QStringLiteral("Only {e_spikes} escaped. Reviewer 2 is running out of things to say."),
            QStringLiteral("{n_spikes}! Your ogive has never been flatter."),
            QStringLiteral("You've made {tool} look like a toy."),
            QStringLiteral("{place} applauds. Quietly, so as not to add noise."),
            QStringLiteral("{n_spikes} despiked. The network wants to adopt your settings."),
            QStringLiteral("{who} is writing a methods paper about you."),
            QStringLiteral("{e_spikes} escaped, all of them trembling."),
            QStringLiteral("With {n} despiked, you can finally close the laptop at a reasonable hour."),
            QStringLiteral("{n_spikes}. The Cyberdemon has quietly left the footprint."),
            QStringLiteral("{place} will be in the next annual report, thanks to you."),
            QStringLiteral("{who} tried {tool} and got fewer. They're not happy about it."),
            QStringLiteral("{n_spikes} despiked with the calm of a sonic sampling at 20 Hz."),
            QStringLiteral("You despiked {n}. The quality flags are mostly zeros now. Suspiciously so."),
            QStringLiteral("{e_spikes} escaped into {place}, where they'll be very lonely."),
            QStringLiteral("{n_spikes}: enough for a paper, a poster and a slightly smug email."),
            QStringLiteral("{who} has framed your score and hung it in the hut."),
            QStringLiteral("The demons are drafting a formal complaint to {who}.")
          } },
        { 30,
          {
            QStringLiteral("Senior Scientist"),
            QStringLiteral("Principal Investigator"),
            QStringLiteral("Chair of the QC Committee"),
            QStringLiteral("Director of Despiking"),
            QStringLiteral("Grand Master of the Median"),
            QStringLiteral("Keeper of the Network Standards"),
            QStringLiteral("Head of the Flux Lab"),
            QStringLiteral("Distinguished Micrometeorologist"),
            QStringLiteral("Archmage of the Ogive"),
            QStringLiteral("High Priest of the Planar Fit"),
            QStringLiteral("Warden of All Footprints"),
            QStringLiteral("Lord Protector of the Energy Balance"),
            QStringLiteral("Senior Keeper of the Sonic"),
            QStringLiteral("Emeritus Tower Climber"),
            QStringLiteral("Grand Inquisitor of Outliers"),
            QStringLiteral("Chancellor of the Covariance"),
            QStringLiteral("Principal Despiker"),
            QStringLiteral("Keynote Speaker"),
            QStringLiteral("Editor of the Flux Journal"),
            QStringLiteral("Guardian of the Annual Budget"),
            QStringLiteral("Commander of the Night Shift"),
            QStringLiteral("Marshal of the Measurement Network"),
            QStringLiteral("Supreme Leveller of Sonics"),
            QStringLiteral("Overseer of the Quality Flags"),
            QStringLiteral("Archivist of the Raw Data"),
            QStringLiteral("Lord of the Time Lag"),
            QStringLiteral("Sage of Stationarity"),
            QStringLiteral("Great Filter of the Spikes"),
            QStringLiteral("Protector of the Carbon Sink"),
            QStringLiteral("President of the Tower Society")
          },
          {
            QStringLiteral("{n_spikes} despiked. {place} has never looked this clean."),
            QStringLiteral("{who} will cite your {n_spikes} in their next keynote."),
            QStringLiteral("With {n_spikes} despiked, you've made {tool} obsolete."),
            QStringLiteral("{n_spikes}! The flux network is renaming its QC step after you."),
            QStringLiteral("{place} is pristine. {who} is taking photos."),
            QStringLiteral("Only {e_spikes} escaped, and they're writing their memoirs."),
            QStringLiteral("You despiked {n}. {tool} has asked for your autograph."),
            QStringLiteral("{who} wants your settings. You've told them to find their own."),
            QStringLiteral("{n_spikes}: enough to rewrite the textbook chapter on despiking."),
            QStringLiteral("{place} is so clean that the random uncertainty has gone home."),
            QStringLiteral("The demons have filed for bankruptcy. You despiked {n}."),
            QStringLiteral("{n_spikes} despiked. Your name now appears in the default configuration."),
            QStringLiteral("{who} invites you to give the plenary. The topic: \"Just shoot them\"."),
            QStringLiteral("{e_spikes} escaped, but they'll never work in this footprint again."),
            QStringLiteral("With {n} despiked, your energy balance almost closed out of respect."),
            QStringLiteral("{tool} is thinking about retiring."),
            QStringLiteral("{n_spikes}. {place} could be used as a calibration standard."),
            QStringLiteral("{who} is updating the network protocol to say \"do what they did\"."),
            QStringLiteral("You despiked {n}. The spikes have unionised."),
            QStringLiteral("{n_spikes} despiked. Reviewer 2 accepted without comments, for the first time in history."),
            QStringLiteral("{place} is spotless, and {who} can't stop talking about it."),
            QStringLiteral("{n_spikes}: the tower has put up a small plaque."),
            QStringLiteral("Your despiking is so good that {tool} now calls you for advice."),
            QStringLiteral("{e_spikes} escaped, and they'll be flagged the moment they land."),
            QStringLiteral("{n_spikes} despiked. The funding agency doubled your budget, then halved it again, as usual."),
            QStringLiteral("{who} says you're wasted on eddy covariance. They're wrong."),
            QStringLiteral("With {n} despiked, your paper writes itself."),
            QStringLiteral("{place} is ready for the database, the journal and the museum."),
            QStringLiteral("{n_spikes}. Future students will hear about you, in hushed tones."),
            QStringLiteral("The demons now warn their children about you.")
          } },
        { std::numeric_limits<int>::max(),
          {
            QStringLiteral("Tenured Doom Slayer"),
            QStringLiteral("Lord of the Ogive"),
            QStringLiteral("Sonic Whisperer"),
            QStringLiteral("Legend of the Flux Tower"),
            QStringLiteral("Eternal Despiker"),
            QStringLiteral("Grand Architect of the Median"),
            QStringLiteral("Sovereign of the Covariance"),
            QStringLiteral("Emperor of Eddies"),
            QStringLiteral("Keeper of the Perfect Half-Hour"),
            QStringLiteral("Immortal of the Night Shift"),
            QStringLiteral("Supreme Guardian of the Footprint"),
            QStringLiteral("Master of All Time Lags"),
            QStringLiteral("High Lord of the Planar Fit"),
            QStringLiteral("The One Who Closed the Energy Balance"),
            QStringLiteral("Terror of the Outliers"),
            QStringLiteral("Oracle of the Ogive"),
            QStringLiteral("Archon of Turbulence"),
            QStringLiteral("Eternal Keeper of the Sonic"),
            QStringLiteral("Destroyer of Noise"),
            QStringLiteral("Titan of the Tower"),
            QStringLiteral("Paragon of the Quality Flags"),
            QStringLiteral("Warlord of Wavelets"),
            QStringLiteral("Grand Duke of the Diurnal Cycle"),
            QStringLiteral("Sultan of Stationarity"),
            QStringLiteral("Pharaoh of the Flux Footprint"),
            QStringLiteral("Kaiser of the Kolmogorov Scale"),
            QStringLiteral("Deity of Despiking"),
            QStringLiteral("Champion of Champions (Micrometeorology Division)"),
            QStringLiteral("The Doom Slayer of Eddy Covariance"),
            QStringLiteral("Mythical Beast of the Flux Network")
          },
          {
            QStringLiteral("{n_spikes} despiked! {place} is spotless and {who} is speechless."),
            QStringLiteral("{n_spikes} despiked, {e_spikes} escaped. Every flux network wants to hire you."),
            QStringLiteral("Legend says {tool} was named after you."),
            QStringLiteral("{n_spikes}. The demons have formally surrendered to {who}."),
            QStringLiteral("{place} is now a protected area. Spikes are banned."),
            QStringLiteral("You despiked {n}. Somewhere, a sonic anemometer wept with joy."),
            QStringLiteral("{who} has retired, knowing the data are in safe hands."),
            QStringLiteral("{n_spikes}! Your cospectra follow the theoretical curve exactly. Nobody believes it."),
            QStringLiteral("{tool} has been deprecated in your honour."),
            QStringLiteral("{e_spikes} escaped. They were later found hiding in another site's data."),
            QStringLiteral("With {n} despiked, the energy balance closed. Briefly. Everyone saw it."),
            QStringLiteral("{n_spikes}. The Cyberdemon has asked for a signed photo."),
            QStringLiteral("{who} is building a statue of you out of old sonic parts."),
            QStringLiteral("{place} is so clean it now counts as a reference dataset."),
            QStringLiteral("You despiked {n}. The spikes have switched careers to remote sensing."),
            QStringLiteral("{n_spikes}: a new world record, pending review by {who}."),
            QStringLiteral("The demons now use your name as a curse."),
            QStringLiteral("{n_spikes} despiked. The half-hours are queueing up to thank you."),
            QStringLiteral("{who} has nominated you for a medal. It's made of recycled guy wire."),
            QStringLiteral("{place} will be taught in textbooks as \"the clean one\"."),
            QStringLiteral("With {n} despiked, your u* threshold lowered itself out of respect."),
            QStringLiteral("{n_spikes}! Hell has requested your despiking settings."),
            QStringLiteral("{tool} is now simply called \"doing what you did\"."),
            QStringLiteral("Only {e_spikes} escaped, and they're in therapy."),
            QStringLiteral("{n_spikes} despiked. The time lag synchronised itself out of sheer admiration."),
            QStringLiteral("{who} has renamed the tower after you. The tower agrees."),
            QStringLiteral("You despiked {n}. Your data now get cited more than you do."),
            QStringLiteral("{place} gleams. Even the cows are impressed."),
            QStringLiteral("{n_spikes}: the stuff of legends and very good annual budgets."),
            QStringLiteral("Rip and tear? No: despike and publish. {who} is in awe.")
          } }
    };
    return tiers;
}

QString spikes(int count)
{
    return count == 1 ? QStringLiteral("1 spike")
                      : QStringLiteral("%1 spikes").arg(count);
}

// added to the rank sentence when the player shot valid data
const QStringList& validDataTemplates()
{
    static const QStringList list {
        QStringLiteral("You also deleted {v_points} of perfectly good data. {who} noticed."),
        QStringLiteral("Sadly, {v_points} of valid data went down with the demons."),
        QStringLiteral("{v_points} of real turbulence got despiked too. {place} will remember."),
        QStringLiteral("You shot {v_points} of genuine eddies. They had families."),
        QStringLiteral("{who} would like a word about the {v_points} of valid data you removed."),
        QStringLiteral("Also: {v_points} of good data are now in the bin. {tool} would have spared them."),
        QStringLiteral("{v_points} of perfectly valid turbulence, gone. {place} feels emptier."),
        QStringLiteral("You flagged {v_points} of real data as spikes. Reviewer 2 will find them."),
        QStringLiteral("Collateral damage: {v_points} of honest fluxes."),
        QStringLiteral("{v_points} of valid data were harmed in the making of this score."),
        QStringLiteral("{who} is restoring your {v_points} from the raw archive, sighing loudly."),
        QStringLiteral("Those stars were data, and you shot {v_points} of them."),
        QStringLiteral("{v_points} of good measurements now count as gaps. The gap-filler thanks you."),
        QStringLiteral("You removed {v_points} of valid data. Your annual sum has quietly changed."),
        QStringLiteral("{place} has lost {v_points} of honest turbulence and wants them back."),
        QStringLiteral("{v_points} of good data, deleted. {who} is adding a sentence to the methods section."),
        QStringLiteral("The stars you shot ({v_points}) were the best data of the day."),
        QStringLiteral("Your trigger finger took out {v_points} of real eddies. Oops."),
        QStringLiteral("{v_points} of valid data won't make it into {place}."),
        QStringLiteral("{who} counted {v_points} of good data among the casualties."),
        QStringLiteral("Friendly fire cost you {v_points}. The eddies forgive you. The reviewers won't."),
        QStringLiteral("{v_points} of real turbulence despiked. The random uncertainty just went up."),
        QStringLiteral("You also filtered out {v_points} of good data, which, to be fair, is how most despiking works."),
        QStringLiteral("{v_points} of perfectly nice data are now missing. {who} has put up posters."),
        QStringLiteral("Friendly fire: {v_points}. The quality flags are confused."),
        QStringLiteral("After your {v_points} of friendly fire, {who} wrote \"please don't shoot the stars\" on the hut door."),
        QStringLiteral("You deleted {v_points} of valid data. {tool} would never."),
        QStringLiteral("Somewhere in {place}, {v_points} of good data are sadly missed."),
        QStringLiteral("{v_points} of valid data lost. It'll show up as a suspicious gap in July."),
        QStringLiteral("The stars were innocent. You still shot {v_points} of them.")
    };
    return list;
}

QString dataPoints(int count)
{
    return count == 1 ? QStringLiteral("1 point")
                      : QStringLiteral("%1 points").arg(count);
}

struct GameStats
{
    int despiked;
    int escaped;
    int validRemoved;
};

QString fillTemplate(const QString& sentence, const GameStats& stats)
{
    return fillFlavour(sentence, { { QStringLiteral("n_spikes"), spikes(stats.despiked) },
                                   { QStringLiteral("e_spikes"), spikes(stats.escaped) },
                                   { QStringLiteral("v_points"), dataPoints(stats.validRemoved) },
                                   { QStringLiteral("n"), QString::number(stats.despiked) } });
}

struct Rank
{
    QString title;
    QString sentence;
};

// the tier follows the score, so shooting valid data costs rank too
Rank doomRank(int score, const GameStats& stats, bool died)
{
    const RankTier* tier = &diedTier();
    if (!died)
    {
        for (const auto& candidate : rankTiers())
        {
            if (score <= candidate.maxScore)
            {
                tier = &candidate;
                break;
            }
        }
    }

    QString sentence = fillTemplate(pickFresh(tier->templates), stats);
    if (stats.validRemoved > 0)
        sentence += QLatin1Char(' ') + fillTemplate(pickFresh(validDataTemplates()), stats);

    return { pickFresh(tier->titles), sentence };
}

////////////////////////////////////////////////////////////////////////////////
// tab icons

// the palette and frame of the other view-toolbar icons (img/view_toolbar)
const QColor ICON_GREY(102, 102, 102);
const QColor ICON_GREEN(118, 189, 29);
const QSize ICON_SIZE(42, 40);
const QRectF ICON_FRAME(5.5, 8.5, 31, 24);
const QPointF ICON_CENTRE(21, 20.5);

using IconPainter = std::function<void(QPainter&, int scale)>;

QPixmap paintIcon(int scale, const IconPainter& paint)
{
    QPixmap pixmap(ICON_SIZE * scale);
    pixmap.fill(Qt::transparent);

    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.scale(scale, scale);

    p.setPen(QPen(ICON_GREY, 1.0));
    p.setBrush(Qt::white);
    p.drawRect(ICON_FRAME);

    paint(p, scale);
    return pixmap;
}

// Freedoom's flying demon head, sprite heada1.png. Copyright © 2001-2024
// Contributors to the Freedoom project, BSD 3-Clause; the full notice ships
// beside it as :/icons/egg-doom-licence (img/view_toolbar/freedoom-head.LICENSE.txt).
void paintDoomIcon(QPainter& p, int)
{
    const QImage sprite(QStringLiteral(":/icons/egg-doom"));
    if (sprite.isNull())
        return;
    const qreal height = ICON_FRAME.height() - 3;
    const qreal width = height * sprite.width() / sprite.height();
    p.drawImage(QRectF(ICON_CENTRE.x() - width / 2, ICON_CENTRE.y() - height / 2, width, height),
                sprite);
}

// a d20: hexagon rim, front triangle, facet edges, and its numbers
void paintD20Icon(QPainter& p, int scale)
{
    const qreal r = 10.5;
    std::array<QPointF, 6> rim;
    for (int i = 0; i < 6; ++i)
    {
        const qreal angle = (-90.0 + 60.0 * i) * 3.14159265358979323846 / 180.0;
        rim[i] = ICON_CENTRE + QPointF(r * std::cos(angle), r * std::sin(angle));
    }
    const QPointF top = ICON_CENTRE + QPointF(0, -0.52 * r);
    const QPointF left = ICON_CENTRE + QPointF(-0.45 * r, 0.26 * r);
    const QPointF right = ICON_CENTRE + QPointF(0.45 * r, 0.26 * r);

    p.setPen(QPen(ICON_GREEN, 1.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::white);
    p.drawPolygon(rim.data(), 6);
    const QPointF front[3] { top, left, right };
    p.drawPolygon(front, 3);
    for (int i : { 0, 1, 5 })
        p.drawLine(top, rim[i]);
    for (int i : { 5, 4, 3 })
        p.drawLine(left, rim[i]);
    for (int i : { 1, 2, 3 })
        p.drawLine(right, rim[i]);

    // sizes are in icon pixels; the painter's scale takes care of 2x
    auto number = [&p, scale](const QPointF& at, const QString& text, qreal size)
    {
        QFont font = p.font();
        font.setBold(true);
        font.setPixelSize(std::max(qRound(size * scale), 1));
        p.save();
        p.scale(1.0 / scale, 1.0 / scale);
        p.setFont(font);
        const QPointF c = at * scale;
        p.drawText(QRectF(c.x() - 8 * scale, c.y() - 5 * scale, 16 * scale, 10 * scale),
                   Qt::AlignCenter, text);
        p.restore();
    };
    p.setPen(ICON_GREEN);
    number(ICON_CENTRE + QPointF(0, 0.05 * r), QStringLiteral("20"), 5.0);

    // the neighbouring faces are only legible at 2x
    if (scale >= 2)
    {
        QColor faint = ICON_GREEN;
        faint.setAlpha(150);
        p.setPen(faint);
        number(ICON_CENTRE + QPointF(-0.62 * r, -0.12 * r), QStringLiteral("8"), 2.6);
        number(ICON_CENTRE + QPointF(0.62 * r, -0.12 * r), QStringLiteral("14"), 2.6);
        number(ICON_CENTRE + QPointF(0, 0.70 * r), QStringLiteral("2"), 2.6);
    }
}

// a hitchhiker's raised thumb
void paintThumbIcon(QPainter& p, int)
{
    p.setPen(QPen(ICON_GREEN, 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::white);

    QPainterPath thumb;
    thumb.moveTo(16.6, 20.5);
    thumb.lineTo(17.2, 13.2);
    thumb.cubicTo(17.4, 10.2, 21.4, 10.2, 21.3, 13.2);
    thumb.lineTo(21.0, 20.0);
    p.drawPath(thumb);

    // the fist in front of it, with three curled fingers
    p.drawRoundedRect(QRectF(15.0, 19.0, 13.0, 11.0), 3.0, 3.0);
    for (qreal y : { 22.3, 25.0, 27.6 })
        p.drawLine(QPointF(21.6, y), QPointF(27.4, y));
}

const int COLUMN_MAX_WIDTH = 720;
const int DOOM_STEP_MS = 600;

} // namespace

EasterEggPage::EasterEggPage(QWidget *parent) :
    QWidget(parent),
    variant_(Variant::Doom)
{
    loading_ = new FakeLoading(this);

    doomPanel_ = createDoomPanel();

    deepThought_ = new DeepThoughtPanel;
    connect(deepThought_, &DeepThoughtPanel::finished, this, &EasterEggPage::finished);

    dnd_ = new DndPanel;
    connect(dnd_, &DndPanel::finished, this, &EasterEggPage::finished);

    //> Only the active panel is visible, so the others take no space. A
    //> plain box layout (unlike a stacked widget) passes height-for-width
    //> through, which is what word-wrapped labels need to get their height.
    auto column = new QWidget;
    column->setMaximumWidth(COLUMN_MAX_WIDTH);
    column->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    // a way out at any moment, not just at the end of each joke
    auto leave = makeButton(tr("✕ Back to work"));
    leave->setToolTip(tr("Close this tab and get back to the fluxes"));
    connect(leave, &QPushButton::clicked, this, [this]()
    {
        resetAll();
        emit finished();
    });
    auto leaveRow = new QHBoxLayout;
    leaveRow->addStretch(1);
    leaveRow->addWidget(leave);

    auto columnLayout = new QVBoxLayout(column);
    columnLayout->setContentsMargins(0, 0, 0, 0);
    columnLayout->addLayout(leaveRow);
    columnLayout->addWidget(doomPanel_);
    columnLayout->addWidget(deepThought_);
    columnLayout->addWidget(dnd_);

    // centre with stretches, not alignment, so the column gets a real width
    auto row = new QHBoxLayout;
    row->addStretch(1);
    row->addWidget(column, 10);
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

QIcon EasterEggPage::tabIcon(Variant variant)
{
    IconPainter painter;
    switch (variant)
    {
        case Variant::Doom:
            painter = paintDoomIcon;
            break;
        case Variant::Hitchhiker:
            painter = paintThumbIcon;
            break;
        case Variant::DnD:
        case Variant::Count:
            painter = paintD20Icon;
            break;
    }

    // 1x and 2x, like the @2x files of the other toolbar icons
    QIcon icon;
    icon.addPixmap(paintIcon(1, painter));
    icon.addPixmap(paintIcon(2, painter));
    return icon;
}

void EasterEggPage::setVariant(Variant variant)
{
    variant_ = variant;
    resetAll();

    doomPanel_->setVisible(variant == Variant::Doom);
    deepThought_->setVisible(variant == Variant::Hitchhiker);
    dnd_->setVisible(variant == Variant::DnD);
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
    loading_->stop();
    resetDoomPanel();
    deepThought_->reset();
    dnd_->reset();
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
    layout->addWidget(makeText(tr("Despike or die. Shoot the demons before they escape into your data, "
                                  "spare the stars (they're valid data), and grab the downward spikes "
                                  "to reload.")));
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
    loading_->run(doomBar_, doomStatus_, sample(doomSteps(), 5), DOOM_STEP_MS, [this]()
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

void EasterEggPage::showGameResult(int score, int despiked, int escaped, int validRemoved,
                                   int shots, bool died)
{
    const int accuracy = qRound(100.0 * despiked / std::max(shots, 1));

    const Rank rank = doomRank(score, { despiked, escaped, validRemoved }, died);
    doomRank_->setText(tr("Rank: %1").arg(rank.title.toHtmlEscaped()));
    doomStats_->setText(QStringLiteral("%1<br>%2")
                            .arg(rank.sentence.toHtmlEscaped(),
                                 tr("Score %1: despiked %2, escaped %3, valid data removed %4 "
                                    "(−%5 each), accuracy %6%.")
                                     .arg(score).arg(despiked).arg(escaped).arg(validRemoved)
                                     .arg(DespikeArena::VALID_DATA_PENALTY).arg(accuracy)));
    doomTag_->setText(QStringLiteral("<i>%1</i>").arg(
        pickFresh(doomSentences()).toHtmlEscaped()));

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
