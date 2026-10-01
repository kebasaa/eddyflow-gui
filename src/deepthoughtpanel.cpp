/***************************************************************************
  deepthoughtpanel.cpp
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

#include "deepthoughtpanel.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

using namespace EggUi;

namespace {

////////////////////////////////////////////////////////////////////////////////
// Deep Thought's answers

// the thinking lines; each question shows five of them
const QStringList& thinkingSteps()
{
    static const QStringList list {
        QStringLiteral("Pondering the Ultimate Question…"),
        QStringLiteral("Integrating cospectra over 7.5 million years…"),
        QStringLiteral("Filtering Vogon poetry out of the high-frequency range…"),
        QStringLiteral("Towel detected. Proceeding…"),
        QStringLiteral("Engaging the Infinite Improbability Drive…"),
        QStringLiteral("Counting eddies (this may take a while)…"),
        QStringLiteral("Rotating the question into the mean streamline…"),
        QStringLiteral("Despiking the Universe…"),
        QStringLiteral("Consulting the flux network…"),
        QStringLiteral("Waiting for the energy balance to close…"),
        QStringLiteral("Gap-filling the missing parts of your question…"),
        QStringLiteral("Applying the WPL correction to reality…"),
        QStringLiteral("Estimating the time lag between question and answer…"),
        QStringLiteral("Cross-checking with the chamber people…"),
        QStringLiteral("Calculating the footprint of the Universe…"),
        QStringLiteral("Reading 7.5 million years of raw data…"),
        QStringLiteral("Averaging over thirty minutes of pure thought…"),
        QStringLiteral("Re-levelling the sonic in hyperspace…"),
        QStringLiteral("Asking Marvin (he's not happy about it)…"),
        QStringLiteral("Unzipping the .ghg file of the Universe…"),
        QStringLiteral("Checking for stationarity (the Universe is not)…"),
        QStringLiteral("Correcting the question for high-frequency losses…"),
        QStringLiteral("Planar-fitting the galaxy…"),
        QStringLiteral("Detrending the meaning of life…"),
        QStringLiteral("Waiting for the logger to finish downloading…"),
        QStringLiteral("Converting the answer to SI units…"),
        QStringLiteral("Filtering by u* (the question passed, barely)…"),
        QStringLiteral("Reprocessing with the latest firmware…"),
        QStringLiteral("Computing the ogive of eternity…"),
        QStringLiteral("Removing the storage term from infinity…")
    };
    return list;
}

// rich text; the Vogon fragment is kept short and always attributed
const QStringList& guideEntries()
{
    static const QStringList list {
        QStringLiteral("<i>“Oh freddled gruntbuggly, thy micturations are to me…”</i><br>— Prostetnic Vogon Jeltz, in Douglas Adams, <i>The Hitchhiker's Guide to the Galaxy</i> (1979)<br>Still easier to read than your raw file headers."),
        QStringLiteral("Don't panic. But do check your time lag."),
        QStringLiteral("Your sonic anemometer is mostly harmless."),
        QStringLiteral("So long, and thanks for all the fluxes."),
        QStringLiteral("Always know where your towel is. And your raw data backup."),
        QStringLiteral("The Infinite Improbability Drive can't explain your nighttime CO₂ uptake either."),
        QStringLiteral("Time is an illusion. Averaging periods doubly so."),
        QStringLiteral("Life? Don't talk to me about life. Talk to me about stationarity."),
        QStringLiteral("The Babel fish translated your .ghg file. It says: go back to Basic Settings."),
        QStringLiteral("The Earth was built to measure fluxes. Nobody told the Vogons about the energy balance gap."),
        QStringLiteral("Eddy covariance: a technique for measuring the breathing of a planet with a very expensive stick."),
        QStringLiteral("A sonic anemometer is a device for listening to the wind very carefully, twenty times a second."),
        QStringLiteral("The energy balance gap is one of the great unsolved mysteries of the Universe, along with where the missing socks go."),
        QStringLiteral("An infrared gas analyser is mostly harmless, apart from the price."),
        QStringLiteral("Flux towers are best climbed in daylight, in dry weather, by somebody else."),
        QStringLiteral("The Guide advises never to trust a flux measured at night, by a sleepy instrument, in a valley."),
        QStringLiteral("A half-hour is the unit of time in which nothing is ever quite stationary."),
        QStringLiteral("The Guide files the planar-fit method under \"Ways to make a hill look flat\"."),
        QStringLiteral("In many of the more relaxed civilisations, the u* filter is optional. Those civilisations have no reviewers."),
        QStringLiteral("The Guide recommends always carrying spare fuses, zip ties and a good excuse for the gaps."),
        QStringLiteral("Gap-filling is the art of knowing what the ecosystem would have done if the logger hadn't crashed."),
        QStringLiteral("The WPL correction is named after three very patient people who noticed that air expands."),
        QStringLiteral("A spike is any value that is too interesting to be true."),
        QStringLiteral("The Guide warns that flux towers attract cows, lightning and visiting delegations."),
        QStringLiteral("The ogive tells you whether your averaging period was long enough. It usually says no."),
        QStringLiteral("Data loggers are the Universe's way of reminding you that nothing is ever saved automatically."),
        QStringLiteral("The Guide describes the footprint model as \"a very sophisticated way of blaming the neighbours\"."),
        QStringLiteral("Cospectra are like fingerprints: every site's are unique, and all of them are a bit smudged."),
        QStringLiteral("The inlet tube is the part of a closed-path system that turns fluxes into time lags."),
        QStringLiteral("The first rule of fieldwork, according to the Guide: the battery is always flat.")
    };
    return list;
}

struct Topic
{
    QStringList keywords;
    QString unit;
    QStringList replies;
};

// the first topic whose keyword appears in the question answers it
const QList<Topic>& topics()
{
    static const QList<Topic> list {
        { { QStringLiteral("co2"), QStringLiteral("co₂"), QStringLiteral("ch4"), QStringLiteral("methane"), QStringLiteral("flux") },
          QStringLiteral("µmol m⁻² s⁻¹"),
          {
            QStringLiteral("Exactly what your ecosystem was doing before the cows walked through the footprint."),
            QStringLiteral("Deep Thought is unsure about the sign convention, but very sure about the 42."),
            QStringLiteral("Upward or downward: Deep Thought was built to find the answer, not the direction."),
            QStringLiteral("That's the flux. The uncertainty is also 42, which is less encouraging."),
            QStringLiteral("Measured by a sonic that was, at the time, being sat on by a crow."),
            QStringLiteral("It's a sink on Tuesdays and a source at weekends."),
            QStringLiteral("Deep Thought rounded it. Before rounding it was 41.9999, with a quality flag of 2."),
            QStringLiteral("Your trees are breathing exactly that hard, mostly out of spite."),
            QStringLiteral("That's the net flux. The gross fluxes refused to comment."),
            QStringLiteral("Deep Thought computed it with and without the WPL correction and kept the funnier one."),
            QStringLiteral("That's the flux at noon. At 03:00 it's a philosophical question."),
            QStringLiteral("The methane is 42 as well, but in nmol, and mostly from the cows."),
            QStringLiteral("Deep Thought checked with the chamber people. They got 17, and they're wrong."),
            QStringLiteral("It's positive, which makes your ecosystem a source, which means you'll need a longer discussion section."),
            QStringLiteral("That's the flux over the ecosystem. Over the car park it's rather different."),
            QStringLiteral("Deep Thought double-checked the units. The m⁻² alone took three million years."),
            QStringLiteral("Exactly the value your model predicted, which is suspicious."),
            QStringLiteral("A perfectly ordinary flux for a Tuesday in June, if June were on Magrathea."),
            QStringLiteral("Deep Thought would give you the storage term too, but it's still in the canopy."),
            QStringLiteral("Your annual budget, divided by the number of half-hours, divided by your patience."),
            QStringLiteral("That's what's coming out of the ground. Deep Thought suggests you stop asking what's going in."),
            QStringLiteral("Deep Thought is sure of the magnitude. The sign is between you and your gas analyser."),
            QStringLiteral("That's the flux after gap-filling. Before, it was mostly gaps."),
            QStringLiteral("The flux varies, of course. The answer does not."),
            QStringLiteral("Deep Thought measured it with a sonic the size of a planet. Yours will read slightly less."),
            QStringLiteral("That's the eddy covariance flux. The inversion people will tell you it's 3."),
            QStringLiteral("Enough CO₂ to fill one very small balloon, every second, for ever."),
            QStringLiteral("Your crop is doing its best. Your gas analyser is doing slightly less."),
            QStringLiteral("This value was revealed after 7.5 million years of processing and one firmware update."),
            QStringLiteral("Deep Thought notes that the flux looks better on a log scale. Everything does.")
          } },
        { { QStringLiteral("energy balance"), QStringLiteral("closure"), QStringLiteral("close") },
          QStringLiteral("W m⁻²"),
          {
            QStringLiteral("That's how far short of closure you are. It will close the day the Vogons approve your hyperspace bypass. Check the storage term anyway."),
            QStringLiteral("Missing. Deep Thought suspects the Ravenous Bugblatter Beast of Traal."),
            QStringLiteral("That's the residual. Deep Thought has looked everywhere for it, including under the sofa."),
            QStringLiteral("The gap is exactly the size of the energy you've spent worrying about it."),
            QStringLiteral("Deep Thought blames the ground heat flux plates. Everyone does."),
            QStringLiteral("The missing energy is stored in your supervisor's patience, which is also running out."),
            QStringLiteral("Closure is 80%, as always. Deep Thought suspects the Universe does it on purpose."),
            QStringLiteral("That's what Rn minus G minus H minus LE comes to. Deep Thought checked twice."),
            QStringLiteral("The energy went into large, slow eddies that your averaging period can't see."),
            QStringLiteral("The missing watts are on holiday. They'll be back after your defence."),
            QStringLiteral("Deep Thought closed the energy balance once. It was during a leap second in 1998."),
            QStringLiteral("That's the gap. Deep Thought recommends a longer averaging period and a shorter discussion."),
            QStringLiteral("The energy balance won't close, but it will meet you halfway if you ask nicely."),
            QStringLiteral("Your net radiometer and your sonic have simply never agreed on anything."),
            QStringLiteral("Deep Thought found the missing energy. It's in the canopy, heating the leaves, looking smug."),
            QStringLiteral("The residual is exactly as large as the error bar you'll put on it."),
            QStringLiteral("The energy is missing, presumed advected."),
            QStringLiteral("Deep Thought could close it by force, but then the reviewers would ask how."),
            QStringLiteral("The slope is 0.8. Deep Thought has decided to call it reasonable."),
            QStringLiteral("That's the storage term you forgot, the photosynthesis you ignored and the heat the tower soaked up."),
            QStringLiteral("Closure improves by 5% for every year you spend on the problem. You're nearly there."),
            QStringLiteral("The energy balance is open, like a door. Please close it on your way out."),
            QStringLiteral("Deep Thought has filed a missing-energy report with the local authorities."),
            QStringLiteral("That's the gap. It's been the same gap since the 1980s. It has tenure now."),
            QStringLiteral("The missing energy has gone to the same place as the missing socks."),
            QStringLiteral("Deep Thought ran the numbers again and got 43. It's getting worse."),
            QStringLiteral("That's the imbalance. The soil heat flux plates would like it noted that it's not their fault."),
            QStringLiteral("Deep Thought recommends forcing closure with the Bowen ratio: cheating, but elegant cheating."),
            QStringLiteral("That's how much energy your site loses to the neighbouring cornfield every day."),
            QStringLiteral("The balance will close when your data are perfect, your site is flat and pigs fly over it.")
          } },
        { { QStringLiteral("time lag"), QStringLiteral("lag") },
          QStringLiteral("ms"),
          {
            QStringLiteral("That is your time lag. And yes, it's probably the tube length."),
            QStringLiteral("Deep Thought took 7.5 million years to answer, so it has no right to judge your time lag."),
            QStringLiteral("Deep Thought maximised the covariance. It came out at the tube length divided by your optimism."),
            QStringLiteral("That's the lag for CO₂. The water vapour is still somewhere in the tube, thinking about it."),
            QStringLiteral("Your time lag is fine. Your clock sync is another story."),
            QStringLiteral("Deep Thought tried every lag from zero to infinity. 42 had the best covariance."),
            QStringLiteral("That's the lag between the sonic and the gas analyser, or between you and your deadline."),
            QStringLiteral("The lag drifts with humidity. Deep Thought drifts with coffee."),
            QStringLiteral("That's the nominal lag. The real one changes whenever you look away."),
            QStringLiteral("Deep Thought suggests a lag window from 0 to 42 ms, and a lot of hope."),
            QStringLiteral("Your tube is too long, your pump too slow and your filter too clogged. Otherwise, perfect."),
            QStringLiteral("That's how long the air takes to get from the inlet to the analyser, and you to regret the tube."),
            QStringLiteral("Deep Thought found two covariance peaks. It picked the one that made the fluxes nicer."),
            QStringLiteral("That's the lag today. Tomorrow the filter clogs and it's 420."),
            QStringLiteral("The lag is correct. The sign convention of your lag is a matter for your conscience."),
            QStringLiteral("42 ms is also exactly the time it takes to regret a firmware update."),
            QStringLiteral("That's the lag if your clocks agree. They don't."),
            QStringLiteral("Deep Thought used automatic lag detection and then fixed it by hand, like everyone does."),
            QStringLiteral("That's the delay. Your data logger added three seconds just to be difficult."),
            QStringLiteral("Your time lag is 42 ms. Your reply to Reviewer 2 is 42 days late."),
            QStringLiteral("Deep Thought used a circular correlation. It's still going round."),
            QStringLiteral("That's the lag. The water vapour lag is longer because water is sticky, like your tube."),
            QStringLiteral("Deep Thought measured the tube: 42 metres. That explains a lot."),
            QStringLiteral("Your covariance peak is so flat that 42 ms is as good a guess as any."),
            QStringLiteral("That's the lag. Deep Thought took the median of the last week and ignored the Tuesday."),
            QStringLiteral("The lag is short enough to be real and long enough to be annoying."),
            QStringLiteral("Deep Thought assumed an open-path analyser. If it isn't, add the length of the tube and a sigh."),
            QStringLiteral("That's the lag for a pump at full speed. Your pump has never been at full speed."),
            QStringLiteral("The time lag is 42 ms. The time lag in understanding it is about one PhD."),
            QStringLiteral("Deep Thought recommends checking the lag, the tube, the pump and your life choices, in that order.")
          } },
        { { QStringLiteral("u*"), QStringLiteral("ustar"), QStringLiteral("friction velocity"), QStringLiteral("night"), QStringLiteral("turbulen") },
          QStringLiteral("cm s⁻¹"),
          {
            QStringLiteral("Below the threshold. Deep Thought filters your question out as insufficient turbulence."),
            QStringLiteral("At night even Deep Thought can't find any turbulence. Neither can your sonic."),
            QStringLiteral("That's your u* threshold, found by a moving-point test and wishful thinking."),
            QStringLiteral("Your nighttime flux is exactly as reliable as a weather forecast from a cat."),
            QStringLiteral("Deep Thought filtered all your nights. What's left is Tuesday afternoon."),
            QStringLiteral("Low turbulence detected. Deep Thought is replacing your question with a gap-filled one."),
            QStringLiteral("At 3 a.m. the atmosphere is as still as Deep Thought's enthusiasm."),
            QStringLiteral("That's u*. Everything below it is drainage flow and regret."),
            QStringLiteral("The threshold is 42 cm s⁻¹. Your site averages 12. Good luck."),
            QStringLiteral("Deep Thought ran the change-point detection. It changed its mind 42 times."),
            QStringLiteral("That's the friction velocity. Deep Thought feels plenty of friction already."),
            QStringLiteral("Your nighttime NEE is fiction. Deep Thought is happy to publish it as such."),
            QStringLiteral("Stable conditions. Deep Thought recommends a nap until sunrise."),
            QStringLiteral("That's the threshold. The CO₂ is pooling in the valley, laughing at you."),
            QStringLiteral("Deep Thought bootstrapped the threshold. The bootstrap is still running."),
            QStringLiteral("That's the cut-off below which your fluxes stop being fluxes and start being opinions."),
            QStringLiteral("The turbulence was switched off for maintenance at 22:00."),
            QStringLiteral("Deep Thought filtered by u*, then by stationarity, then by mood. Nothing was left."),
            QStringLiteral("That's this season's threshold. Next season it'll be different, out of spite."),
            QStringLiteral("Nighttime data: 70% filtered, 30% suspicious."),
            QStringLiteral("That's u*. Deep Thought also detected a cold-air drainage flow heading for your laptop."),
            QStringLiteral("The threshold is fine. Your storage term, measured at one height by one sensor, is not."),
            QStringLiteral("At this u*, eddies are more of a rumour than a measurement."),
            QStringLiteral("Deep Thought spent 7.5 million years on your nighttime data and filtered all of it."),
            QStringLiteral("That's the friction velocity at which Deep Thought stops caring."),
            QStringLiteral("Below 42 your data are gap-filled. Above 42 they're merely doubtful."),
            QStringLiteral("That's the threshold. The owls that live near the tower were not consulted."),
            QStringLiteral("Deep Thought measured the turbulence with its eyes closed. Same result."),
            QStringLiteral("u* is 42 cm s⁻¹. The wind is not. The wind is taking the night off."),
            QStringLiteral("Deep Thought recommends daytime data, a strong wind, and not asking about the nights.")
          } },
        { { QStringLiteral("footprint"), QStringLiteral("fetch"), QStringLiteral("upwind") },
          QStringLiteral("m"),
          {
            QStringLiteral("That's how far upwind your footprint reaches, just past the Restaurant at the End of the Universe."),
            QStringLiteral("Your footprint extends to a small planet near Betelgeuse. Check your wind direction filter."),
            QStringLiteral("That's the peak of the footprint. The 90% contour includes your neighbour's barbecue."),
            QStringLiteral("Your footprint covers the field, the ditch and a surprising amount of motorway."),
            QStringLiteral("Deep Thought ran the footprint model. The flux is coming from the car park."),
            QStringLiteral("At night your footprint reaches the next village. Please apologise to the next village."),
            QStringLiteral("That's where the flux comes from. The cows are standing there right now."),
            QStringLiteral("Your fetch is 42 m in the only direction the wind never blows."),
            QStringLiteral("Deep Thought computed the footprint climatology. It looks like a kidney bean, as they do."),
            QStringLiteral("The footprint is 42 m long and full of things you didn't plan for."),
            QStringLiteral("That's the distance to the edge of your field. Beyond it lie the neighbour's maize and your discussion section."),
            QStringLiteral("Deep Thought used a footprint model. The footprint model used Deep Thought. It's complicated."),
            QStringLiteral("Your footprint just reached the pond. The pond is emitting methane in your name."),
            QStringLiteral("That's the upwind distance at which your data stop being about your ecosystem."),
            QStringLiteral("Deep Thought assumed flat, homogeneous terrain. Your site has a hill, a road and a barn."),
            QStringLiteral("Your footprint is exactly the size of the area you forgot to get permission for."),
            QStringLiteral("That's the peak contribution. Most of it is one very productive shrub."),
            QStringLiteral("In a westerly wind your footprint is a forest. In an easterly it's a car park. Deep Thought prefers westerlies."),
            QStringLiteral("Deep Thought recommends a taller tower. Or a smaller planet."),
            QStringLiteral("That's the footprint at noon. At midnight it's in another postcode."),
            QStringLiteral("Your footprint overlaps the neighbouring site's. Half of their fluxes are now yours."),
            QStringLiteral("The flux comes from 42 m upwind, next to the tractor that's always parked there."),
            QStringLiteral("Deep Thought measured the fetch in paces and got bored after 42."),
            QStringLiteral("Your footprint touches the lake, the road and the edge of statistical significance."),
            QStringLiteral("That's the distance. Deep Thought also found a sheep in the 80% contour."),
            QStringLiteral("Your fetch is sufficient, in the sense that it's sufficient to cause problems."),
            QStringLiteral("That's the source area radius. It includes your car, parked and idling, for the photo."),
            QStringLiteral("Deep Thought computed the footprint on a very flat, very imaginary planet."),
            QStringLiteral("The footprint model says 42 m. The farmer says 4 hectares. They're both annoyed."),
            QStringLiteral("That's how far your footprint goes before it becomes someone else's problem.")
          } },
        { { QStringLiteral("gap"), QStringLiteral("fill"), QStringLiteral("missing") },
          QStringLiteral("% of your data"),
          {
            QStringLiteral("Gaps, filled by the Infinite Improbability Drive. Results may be improbable."),
            QStringLiteral("Missing. Deep Thought recommends MDS gap-filling and a strong cup of tea."),
            QStringLiteral("That's how much of your year is gaps. The rest is suspicious."),
            QStringLiteral("Deep Thought filled your gaps with a neural network. The neural network filled them with cats."),
            QStringLiteral("Missing. The remaining data will be very carefully worded in your paper."),
            QStringLiteral("Deep Thought filled the gaps with the mean diurnal course. It's very average."),
            QStringLiteral("Your data have more holes than the insect mesh on your inlet."),
            QStringLiteral("That's the gap fraction after quality control. Before quality control you were happy."),
            QStringLiteral("Most of the gaps are at night, when the data were too shy to be measured."),
            QStringLiteral("Deep Thought used marginal distribution sampling. The margins were very marginal."),
            QStringLiteral("That's the gap. Your annual sum now depends on a look-up table and your optimism."),
            QStringLiteral("Deep Thought filled the gaps, then filled the gaps in the gap-filling."),
            QStringLiteral("The power cut in March accounts for half. The spider accounts for the rest."),
            QStringLiteral("Your gaps are long, frequent and mostly in the growing season."),
            QStringLiteral("That's the missing data. Deep Thought found some of it in a folder called new_new_final."),
            QStringLiteral("The gaps have been filled. The uncertainty has doubled. Everyone's happy."),
            QStringLiteral("Deep Thought filled a three-week gap with a straight line and a straight face."),
            QStringLiteral("That's how much the gap-filler invented. Deep Thought prefers to say \"modelled\"."),
            QStringLiteral("Your gaps are gap-filled, your gap-filling is gap-filled, and it's gaps all the way down."),
            QStringLiteral("The logger was full for that much of the year. Nobody checked it."),
            QStringLiteral("Deep Thought recommends a second tower, a backup logger and a technician who answers the phone."),
            QStringLiteral("That's the gap percentage. It's the only number in your paper nobody will question."),
            QStringLiteral("Deep Thought filled your gaps with data from a site 300 km away. It's close enough."),
            QStringLiteral("The gaps are exactly where the interesting weather happened."),
            QStringLiteral("Missing, flagged, and 100% of the reviewers asking about it."),
            QStringLiteral("Deep Thought gap-filled the gaps using artificial intelligence and genuine desperation."),
            QStringLiteral("That's the share of half-hours that got lost between the logger and the server."),
            QStringLiteral("Your data are mostly gaps, which is still better than most memory sticks."),
            QStringLiteral("Deep Thought found your missing data. It was on the SD card you left in the car."),
            QStringLiteral("Gaps are not failures. They're opportunities for methods development.")
          } },
        { { QStringLiteral("rotation"), QStringLiteral("planar fit"), QStringLiteral("tilt"), QStringLiteral("coordinate") },
          QStringLiteral("degrees"),
          {
            QStringLiteral("Deep Thought double-rotated your question until it pointed at the answer."),
            QStringLiteral("Deep Thought planar-fitted the whole galaxy. It came out slightly tilted."),
            QStringLiteral("That's your tilt angle. The tower is leaning towards the pub."),
            QStringLiteral("After rotation the mean vertical wind is zero, as it should be, and as nothing else is."),
            QStringLiteral("Deep Thought rotated the coordinates three times. Now it's dizzy."),
            QStringLiteral("That's the angle between your sonic and the truth."),
            QStringLiteral("Deep Thought recommends a planar fit per wind sector and a spirit level per technician."),
            QStringLiteral("Your sonic is 42 degrees off north. It was installed by someone holding the compass upside down."),
            QStringLiteral("That's the pitch. The roll is also 42. Everything's 42. Deep Thought is very consistent."),
            QStringLiteral("After double rotation your w is zero and your conscience is clear."),
            QStringLiteral("Deep Thought fitted a plane to your wind data. The plane crashed."),
            QStringLiteral("That's how far the tower tilted in the storm. It's leaning on the fence now."),
            QStringLiteral("Your coordinates have been rotated. Your perspective has not."),
            QStringLiteral("Deep Thought used triple rotation. The third rotation was just for fun."),
            QStringLiteral("That's the tilt. Deep Thought suggests either a planar fit or a bigger hammer."),
            QStringLiteral("Your sonic was levelled once, in 2014, by an optimist."),
            QStringLiteral("That's the rotation angle. Your streamlines now follow the terrain, whether it likes it or not."),
            QStringLiteral("Deep Thought planar-fitted your data. The plane is steeper than your learning curve."),
            QStringLiteral("After rotation your fluxes changed by 42%. Deep Thought blames the terrain."),
            QStringLiteral("That's the slope of the hillside, which is why your site should have been in the valley."),
            QStringLiteral("Deep Thought rotated into the mean streamline. The streamline was not impressed."),
            QStringLiteral("Your tilt correction works everywhere except in the sector where all the flux comes from."),
            QStringLiteral("That's the offset between your sonic's north arrow and actual north."),
            QStringLiteral("Deep Thought applies a sector-wise planar fit, the way your supervisor applies sector-wise criticism."),
            QStringLiteral("That's the angle of attack. Your sonic's calibration assumes zero. Oops."),
            QStringLiteral("Deep Thought rotated your data so much that the covariances are now upside down."),
            QStringLiteral("That's the pitch angle at which Deep Thought stops trusting your mast."),
            QStringLiteral("Your tower has a 42-degree view of the Universe and a 3-degree lean to the left."),
            QStringLiteral("Rotated, detrended, corrected. Deep Thought feels like a new computer."),
            QStringLiteral("That's the tilt. Deep Thought suggests hiding it in the supplementary material.")
          } },
        { { QStringLiteral("spike"), QStringLiteral("despik"), QStringLiteral("noise"), QStringLiteral("outlier") },
          QStringLiteral("spikes"),
          {
            QStringLiteral("Most of them caused by Marvin being depressed near the sonic."),
            QStringLiteral("Deep Thought has removed them. It feels slightly better now. Only slightly."),
            QStringLiteral("That's how many spikes your routine found. Deep Thought found 42 more."),
            QStringLiteral("Most of them come from rain on the transducers. The rest come from birds."),
            QStringLiteral("Deep Thought removed the spikes, then removed the spikes in the spike-removal log."),
            QStringLiteral("That's the number per half-hour. It matches the number of bird landings."),
            QStringLiteral("Deep Thought used a 3.5-sigma threshold. The spikes wore a 3.6-sigma disguise."),
            QStringLiteral("Your spikes are mostly electrical. The rest are a spider going for a walk."),
            QStringLiteral("Deep Thought counted the spikes, flagged the spikes, and then went for a lie-down."),
            QStringLiteral("That's the spike count. The Doom Slayer would be proud."),
            QStringLiteral("Deep Thought removed 42 spikes. One of them was your best eddy."),
            QStringLiteral("That's how often the sonic loses its signal. Mostly when it rains, and when it matters."),
            QStringLiteral("Your gas analyser makes a spike every time the window gets dirty, which is always."),
            QStringLiteral("Deep Thought despiked your data. Your data have filed a complaint."),
            QStringLiteral("That's how many values were replaced by linear interpolation. Deep Thought won't say which."),
            QStringLiteral("Your spikes are so regular they're probably a cable problem."),
            QStringLiteral("Deep Thought found 42 spikes and one very surprised beetle."),
            QStringLiteral("That's the count before quality control. Afterwards it's still 42, but they're hiding."),
            QStringLiteral("Deep Thought used a median filter. The spikes used a median filter to hide from it."),
            QStringLiteral("Your spikes come exactly every 42 seconds. That's not turbulence, that's the pump."),
            QStringLiteral("Deep Thought despiked with a moving window. The spikes moved faster."),
            QStringLiteral("That's the number of spikes per day. Deep Thought recommends a lightning conductor."),
            QStringLiteral("Deep Thought removed them all. Your time series is now a flat line, which is worrying."),
            QStringLiteral("Your spikes coincide with the mobile signal. Someone keeps phoning the logger."),
            QStringLiteral("That's the spike count. Deep Thought has given each of them a name."),
            QStringLiteral("Deep Thought despiked your data, then despiked its own answer, just in case."),
            QStringLiteral("The spikes are real. The turbulence is not. Deep Thought is confused too."),
            QStringLiteral("That's how many times the sonic said something unreasonable today."),
            QStringLiteral("Deep Thought removed them with a technique it calls \"looking at it\"."),
            QStringLiteral("Your data are 42% spikes and 58% optimism.")
          } },
        { { QStringLiteral("spectr"), QStringLiteral("frequency"), QStringLiteral("correction"), QStringLiteral("filter") },
          QStringLiteral("Hz"),
          {
            QStringLiteral("The frequency at which Vogon poetry peaks. Apply a low-pass filter immediately."),
            QStringLiteral("Deep Thought corrected your spectra so thoroughly they now describe a different planet."),
            QStringLiteral("That's the cut-off frequency of your tube. Everything above it is lost for ever."),
            QStringLiteral("Your cospectrum peaks at 42 Hz, which is physically impossible and impressively confident."),
            QStringLiteral("Deep Thought applied the Moncrieff correction, the Massman correction and a little hope."),
            QStringLiteral("That's the frequency at which your spectrum turns from turbulence into noise."),
            QStringLiteral("Your high-frequency losses are exactly 42%. The tube ate them."),
            QStringLiteral("Deep Thought fitted a Kaimal cospectrum. Your data refused to follow it."),
            QStringLiteral("That's the aliasing frequency. Everything above it comes back to haunt you."),
            QStringLiteral("Your spectra have a −5/3 slope, except where they have a −42/3 slope."),
            QStringLiteral("Deep Thought corrected for sensor separation. The sensors still aren't speaking to each other."),
            QStringLiteral("That's your sampling frequency's favourite number."),
            QStringLiteral("Your ogive converges after 42 minutes. Your averaging period is 30. Awkward."),
            QStringLiteral("Deep Thought applied a spectral correction factor of 1.42. Nobody will ever check."),
            QStringLiteral("That's the frequency at which your gas analyser starts making things up."),
            QStringLiteral("Your spectra have a peak at 50 Hz. That's the mains power, not the atmosphere."),
            QStringLiteral("Deep Thought corrected for path averaging, sensor response and personal disappointment."),
            QStringLiteral("That's the transfer function's half-power frequency. The other half is on holiday."),
            QStringLiteral("Your cospectra look like a mountain range, which suits your site."),
            QStringLiteral("Deep Thought's spectral model assumes neutral stability, flat terrain and a good mood."),
            QStringLiteral("That's the frequency of your pump's vibration, now clearly visible in every flux."),
            QStringLiteral("Your inertial subrange is so short that Deep Thought missed it."),
            QStringLiteral("Deep Thought corrected the low-frequency losses too. Now your fluxes are bigger and nobody believes them."),
            QStringLiteral("That's the frequency where the water vapour spectrum gives up."),
            QStringLiteral("Deep Thought used an in-situ spectral correction. In situ, it rained."),
            QStringLiteral("Your spectra need correcting, your corrections need correcting, and Deep Thought needs a holiday."),
            QStringLiteral("That's the Nyquist frequency of a very, very fast sonic."),
            QStringLiteral("Deep Thought filtered your spectra. The spectra filtered Deep Thought back."),
            QStringLiteral("Your correction factor is larger than your flux. Deep Thought suggests not looking too closely."),
            QStringLiteral("That's the frequency at which Deep Thought hums while it thinks about your cospectra.")
          } },
        { { QStringLiteral("thesis"), QStringLiteral("phd"), QStringLiteral("paper"), QStringLiteral("review"), QStringLiteral("deadline"), QStringLiteral("supervisor"), QStringLiteral("publish") },
          QStringLiteral("days until your deadline"),
          {
            QStringLiteral("Deep Thought recommends a towel, and less time spent in hidden tabs."),
            QStringLiteral("Reviewer 2 has already read your question and requested major revisions."),
            QStringLiteral("That's the deadline. Your supervisor thinks it's 21."),
            QStringLiteral("Deep Thought estimates your thesis will be finished in 42 years. It's being generous."),
            QStringLiteral("That's how long you have. Chapter 3 is still a list of bullet points."),
            QStringLiteral("Deep Thought read your draft. It asks whether you've considered a career in remote sensing."),
            QStringLiteral("Your paper will be accepted after 42 rounds of revisions."),
            QStringLiteral("That's the deadline. The conference hotel is already full."),
            QStringLiteral("Deep Thought has calculated your supervisor's response time: 42 days, with one comment: \"hmm\"."),
            QStringLiteral("Your committee meets in 42 days. They'll ask about the energy balance."),
            QStringLiteral("That's how many days until your contract ends. Deep Thought suggests writing faster."),
            QStringLiteral("Deep Thought computed your h-index. It's lower than 42."),
            QStringLiteral("That's the deadline, and also your remaining word count, in thousands."),
            QStringLiteral("Deep Thought suggests publishing the data first and understanding them later, like everyone else."),
            QStringLiteral("Reviewer 2 wants 42 more sensitivity analyses. Reviewer 1 just wants the figures bigger."),
            QStringLiteral("That's how long until your defence. Deep Thought recommends practising \"that's a great question\"."),
            QStringLiteral("Your supervisor has 42 unread emails from you. Deep Thought suggests sending a 43rd."),
            QStringLiteral("Deep Thought ran a Monte Carlo on your deadline. It never converged."),
            QStringLiteral("That's the time left. Your literature review is still reviewing the literature."),
            QStringLiteral("Deep Thought wrote your discussion section. It's the word \"complicated\", 42 times."),
            QStringLiteral("Your paper's chance of acceptance is 42%, and 0% if you mention the energy balance."),
            QStringLiteral("That's how long until the grant report is due. The results section is a photo of the tower."),
            QStringLiteral("Deep Thought recommends a journal with a faster review process. A blog, perhaps."),
            QStringLiteral("Your thesis has 42 figures. 41 of them are diurnal cycles."),
            QStringLiteral("That's the deadline. Deep Thought would extend it, but that's your supervisor's job."),
            QStringLiteral("Deep Thought read your methods section and counted 42 references to \"standard procedures\"."),
            QStringLiteral("Your co-authors will reply in 42 days, all at once, with conflicting comments."),
            QStringLiteral("That's how long until the funding runs out. Deep Thought suggests pretending you didn't ask."),
            QStringLiteral("Deep Thought estimates it will take you 42 drafts. You're on draft 7."),
            QStringLiteral("That's the countdown to your conference talk. The slides are still called \"Untitled presentation\".")
          } }
    };
    return list;
}

// when no topic matches
const QStringList& fallbackReplies()
{
    static const QStringList list {
        QStringLiteral("Deep Thought is confident. Your data, less so."),
        QStringLiteral("Deep Thought has checked it very thoroughly. You won't like it."),
        QStringLiteral("The answer was easy. The question is the hard part, and you're not quite there yet."),
        QStringLiteral("Deep Thought suggests you process your fluxes while it thinks of a better question."),
        QStringLiteral("That's all the Guide has to say on the matter. The rest of the entry reads: mostly harmless."),
        QStringLiteral("Deep Thought could explain, but you'd need a computer the size of a planet to understand it."),
        QStringLiteral("Deep Thought ran your question through the eddy covariance equations. It came out turbulent."),
        QStringLiteral("The answer is correct. The units are a matter of personal taste."),
        QStringLiteral("Deep Thought averaged your question over thirty minutes. The mean is 42."),
        QStringLiteral("Deep Thought would like to point out that it is very busy computing fluxes for other people."),
        QStringLiteral("That's the answer, with an uncertainty of plus or minus everything."),
        QStringLiteral("Deep Thought detrended your question and found only noise."),
        QStringLiteral("The answer passed every quality test, which makes it more reliable than your data."),
        QStringLiteral("Deep Thought is fairly sure. Surer than your gap-filling, anyway."),
        QStringLiteral("Deep Thought cross-checked with the flux network. They also got 42, and a strongly worded email."),
        QStringLiteral("The answer is stationary, homogeneous and fully developed. Your question is not."),
        QStringLiteral("Deep Thought has flagged your question as quality 1. It's the best it could do."),
        QStringLiteral("The answer is universal. Applying it to your site is your problem."),
        QStringLiteral("Deep Thought applied every correction it knows. The answer didn't change."),
        QStringLiteral("That's the answer after spectral correction, which is to say, slightly larger."),
        QStringLiteral("Deep Thought asked Marvin. Marvin sighed. That also means 42."),
        QStringLiteral("The answer is 42. The question, sadly, was lost in a gap in the data."),
        QStringLiteral("Deep Thought is certain. Your sonic anemometer is much less so."),
        QStringLiteral("That's the answer. Please cite it as Deep Thought (in prep.)."),
        QStringLiteral("Deep Thought computed this answer at 20 Hz. It came out the same every time."),
        QStringLiteral("Deep Thought suggests you phrase your next question with more turbulence."),
        QStringLiteral("That's the answer, with a quality flag of 0, a footprint the size of the Universe and a time lag of 7.5 million years."),
        QStringLiteral("Deep Thought ran 42 million Monte Carlo realisations. Every one said 42."),
        QStringLiteral("The answer is robust to rotation, detrending and your supervisor's opinion."),
        QStringLiteral("Deep Thought checked the answer against the chamber measurements. The chambers declined to comment.")
    };
    return list;
}

// units for answers without a topic
const QStringList& randomUnits()
{
    static const QStringList list {
        QStringLiteral("µmol m⁻² s⁻¹"),
        QStringLiteral("W m⁻²"),
        QStringLiteral("ms of time lag"),
        QStringLiteral("% energy-balance gap"),
        QStringLiteral("half-hourly records"),
        QStringLiteral("degrees of coordinate rotation"),
        QStringLiteral("Pa"),
        QStringLiteral("cups of tea"),
        QStringLiteral("Hz"),
        QStringLiteral("m s⁻¹"),
        QStringLiteral("cm s⁻¹ of friction velocity"),
        QStringLiteral("spikes per half-hour"),
        QStringLiteral("metres of fetch"),
        QStringLiteral("mmol m⁻² s⁻¹ of water vapour"),
        QStringLiteral("nmol m⁻² s⁻¹ of methane"),
        QStringLiteral("g C m⁻² yr⁻¹"),
        QStringLiteral("kelvin"),
        QStringLiteral("hPa"),
        QStringLiteral("mm of rain on the sonic"),
        QStringLiteral("days without a power cut"),
        QStringLiteral("quality flags"),
        QStringLiteral("gap-filled nights"),
        QStringLiteral("co-authors"),
        QStringLiteral("sonic anemometers"),
        QStringLiteral("gigabytes of raw data"),
        QStringLiteral("rounds of revisions"),
        QStringLiteral("metres of inlet tube"),
        QStringLiteral("tower climbs"),
        QStringLiteral("firmware updates"),
        QStringLiteral("cows in the footprint")
    };
    return list;
}

// an empty question
const QStringList& emptyReplies()
{
    static const QStringList list {
        QStringLiteral("You have to actually ask something. Deep Thought has waited 7.5 million years for this."),
        QStringLiteral("An empty question. Deep Thought has gap-filled it for you: \"Why?\""),
        QStringLiteral("Deep Thought detects no signal, only noise. Mostly your breathing."),
        QStringLiteral("Silence. Deep Thought respects that. Your fluxes don't."),
        QStringLiteral("Your question was filtered out entirely. Too little turbulence."),
        QStringLiteral("Deep Thought computed the answer to nothing. It's zero, with a large uncertainty."),
        QStringLiteral("That question was 100% gap. Please measure again."),
        QStringLiteral("Deep Thought was hoping for at least one word."),
        QStringLiteral("An empty question gets an empty answer. Deep Thought will now say nothing, very loudly."),
        QStringLiteral("Deep Thought has flagged your question as missing data."),
        QStringLiteral("No input detected. Is your logger connected?"),
        QStringLiteral("Deep Thought refuses to despike an empty question."),
        QStringLiteral("You've asked nothing, which is the most sensible question anyone has asked all day."),
        QStringLiteral("Deep Thought waits. And waits. Like the energy balance."),
        QStringLiteral("A blank question. Deep Thought will now answer it with a blank stare."),
        QStringLiteral("Empty input. Deep Thought assumes a calm night with no turbulence."),
        QStringLiteral("The question box is empty, just like your nighttime flux data."),
        QStringLiteral("Deep Thought appreciates the minimalism, but it needs at least a verb."),
        QStringLiteral("Nothing to process. Deep Thought is going back to its cospectra."),
        QStringLiteral("Your question has a signal-to-noise ratio of zero. Impressive."),
        QStringLiteral("Deep Thought spent 7.5 million years processing your blank question. Still blank."),
        QStringLiteral("Please type something. Even \"why doesn't the energy balance close?\" will do."),
        QStringLiteral("Deep Thought will answer when there's a question. It has time. Lots of it."),
        QStringLiteral("An empty question. Deep Thought suspects the spider in the sonic ate it."),
        QStringLiteral("Deep Thought sampled your question at 20 Hz. All zeros."),
        QStringLiteral("Empty question, full disappointment."),
        QStringLiteral("Deep Thought is ready. Your question, apparently, is not."),
        QStringLiteral("Your question was lost in transmission between the keyboard and the logger."),
        QStringLiteral("Deep Thought gap-filled your question with the mean diurnal question: \"When's lunch?\""),
        QStringLiteral("That's not a question, that's a pause. Deep Thought will wait.")
    };
    return list;
}

// put before the answer when the question has no "?"
const QStringList& notQuestionOpeners()
{
    static const QStringList list {
        QStringLiteral("That wasn't strictly a question, but…"),
        QStringLiteral("Deep Thought notes the missing question mark, but…"),
        QStringLiteral("A statement, not a question. Still…"),
        QStringLiteral("Deep Thought will treat that as a question anyway:"),
        QStringLiteral("No question mark detected. Deep Thought answers regardless:"),
        QStringLiteral("That sounded more like an observation. Nevertheless…"),
        QStringLiteral("Deep Thought suspects you forgot the \"?\", but…"),
        QStringLiteral("Not a question, strictly speaking. However…"),
        QStringLiteral("Deep Thought has added the question mark for you. Answer:"),
        QStringLiteral("A bold statement. Deep Thought replies:"),
        QStringLiteral("That's more of a comment than a question, but…"),
        QStringLiteral("Deep Thought isn't sure you asked anything, but it answers anyway:"),
        QStringLiteral("Without a question mark this could mean anything. Deep Thought assumes:"),
        QStringLiteral("An affirmation! Deep Thought affirms back:"),
        QStringLiteral("That was declarative. Deep Thought is interrogative:"),
        QStringLiteral("Deep Thought interprets your statement as a cry for help:"),
        QStringLiteral("Your sentence lacks a question mark and possibly a hypothesis, but…"),
        QStringLiteral("Deep Thought gap-filled the missing question mark. Result:"),
        QStringLiteral("Technically not a question, but Deep Thought is feeling generous:"),
        QStringLiteral("Missing punctuation, flagged as quality 1. Answer:"),
        QStringLiteral("That's a statement. Deep Thought has a statement too:"),
        QStringLiteral("Deep Thought is used to incomplete inputs; your data taught it that. So:"),
        QStringLiteral("Let's pretend that was a question:"),
        QStringLiteral("Not quite a question, but close enough for eddy covariance:"),
        QStringLiteral("Deep Thought read that as a question. It reads everything as a question:"),
        QStringLiteral("Your statement has been rotated into question coordinates:"),
        QStringLiteral("No \"?\" found, but plenty of turbulence. Answer:"),
        QStringLiteral("Deep Thought will answer the question you meant to ask:"),
        QStringLiteral("That's a claim. Deep Thought will now peer-review it:"),
        QStringLiteral("Without a question mark Deep Thought can only guess. Its guess:")
    };
    return list;
}

// life, the universe or everything; follows "42."
const QStringList& lifeAnswers()
{
    static const QStringList list {
        QStringLiteral("Deep Thought checked it very thoroughly. The problem is that you never actually knew what the question was."),
        QStringLiteral("Life, the universe and everything, averaged over thirty minutes and rotated into the mean streamline."),
        QStringLiteral("The question is being worked out by a much bigger computer, called Earth, with a flux tower on it."),
        QStringLiteral("Everything, including your energy balance residual."),
        QStringLiteral("Deep Thought computed it for life, the universe and everything. Your site is included, under \"everything\"."),
        QStringLiteral("It took 7.5 million years. Your fluxes will take longer."),
        QStringLiteral("The Universe has a closed energy balance. It's the only site that does."),
        QStringLiteral("Life is a net carbon sink, if you gap-fill it generously."),
        QStringLiteral("Deep Thought is certain. The Universe has been notified."),
        QStringLiteral("Deep Thought recommends not asking what the question is. It gets very long."),
        QStringLiteral("Deep Thought checked with every flux tower in the Universe. They all said 42, eventually."),
        QStringLiteral("With a footprint the size of the Universe and a time lag of 7.5 million years."),
        QStringLiteral("Life, the universe and everything, despiked."),
        QStringLiteral("Everything else is just noise in the high-frequency range."),
        QStringLiteral("Deep Thought applied a WPL correction to the Universe. The answer didn't change."),
        QStringLiteral("The answer is universal. The question is site-specific."),
        QStringLiteral("The Universe apologises for the delay; there was a problem with the logger."),
        QStringLiteral("Deep Thought has thought about it very carefully, and it's still 42."),
        QStringLiteral("As ever. The Universe is remarkably stationary on this point."),
        QStringLiteral("Life: 42. The universe: 42. Everything: 42, with a quality flag of 1."),
        QStringLiteral("The mice would like a word about the question."),
        QStringLiteral("Deep Thought ran it again, just to be sure. Another 7.5 million years. Still 42."),
        QStringLiteral("Deep Thought would give you the question too, but it fell into a gap."),
        QStringLiteral("The answer to everything is 42. The answer to your nighttime fluxes is: don't."),
        QStringLiteral("Independent of the coordinate system."),
        QStringLiteral("Deep Thought has published this result. It's been cited 42 times, mostly by itself."),
        QStringLiteral("The Universe passed every quality test except energy balance closure."),
        QStringLiteral("Life, the universe and everything, integrated over all frequencies."),
        QStringLiteral("The answer is 42 and the uncertainty is zero, which is the most suspicious part."),
        QStringLiteral("Now, if you'll excuse Deep Thought, it has a flux network to run.")
    };
    return list;
}

// the same question asked again
const QStringList& repeatNotes()
{
    static const QStringList list {
        QStringLiteral("You asked that already. The answer hasn't changed."),
        QStringLiteral("Deep Thought remembers this one. Same answer."),
        QStringLiteral("Repeated measurement, identical result. Very reproducible."),
        QStringLiteral("You've asked this before. Deep Thought is consistent, unlike your time lag."),
        QStringLiteral("Déjà vu. Same question, same 42."),
        QStringLiteral("Asking twice doesn't improve the signal-to-noise ratio."),
        QStringLiteral("Deep Thought's answer is stationary. So, apparently, is your question."),
        QStringLiteral("Same question, same answer. That's called reproducibility."),
        QStringLiteral("Deep Thought would like to point out that it heard you the first time."),
        QStringLiteral("A duplicate question. Deep Thought flagged it and answered anyway."),
        QStringLiteral("This question is already in the database."),
        QStringLiteral("Asking again won't close the energy balance either."),
        QStringLiteral("Deep Thought gives the same answer every time, unlike your gap-filling."),
        QStringLiteral("Repeated question detected. Deep Thought suspects a stuck logger."),
        QStringLiteral("You again. The answer is the same as before."),
        QStringLiteral("Deep Thought never changes its mind. It's its best feature."),
        QStringLiteral("Your question has been averaged with itself. No change."),
        QStringLiteral("Same input, same output. Deep Thought is very deterministic."),
        QStringLiteral("Deep Thought remembers. Deep Thought always remembers."),
        QStringLiteral("A replicate measurement. Excellent experimental design."),
        QStringLiteral("Deep Thought is happy to be asked twice. Marvin isn't."),
        QStringLiteral("The answer is identical, down to the last decimal place."),
        QStringLiteral("You asked this earlier. Deep Thought is now slightly worried about you."),
        QStringLiteral("Same question. Deep Thought has upgraded its confidence to \"very\"."),
        QStringLiteral("Duplicate detected and kept, for the sake of the sample size."),
        QStringLiteral("Asking again is a valid approach to statistics, but not to Deep Thought."),
        QStringLiteral("Deep Thought has seen this question before. It's still a good one."),
        QStringLiteral("Repeat question, repeat answer, repeat tea."),
        QStringLiteral("The answer hasn't drifted. Neither has your question."),
        QStringLiteral("Deep Thought is deterministic. Please don't take it personally.")
    };
    return list;
}

// after the last question
const QStringList& exhaustedNotes()
{
    static const QStringList list {
        QStringLiteral("Deep Thought is now busy designing an even greater computer to find the Question. Please return to your fluxes."),
        QStringLiteral("Deep Thought has answered enough for one day. Your fluxes, however, are still waiting."),
        QStringLiteral("That's all the questions Deep Thought can take. It has a flux network to run."),
        QStringLiteral("Deep Thought has gone into low-power mode. Please process your data in the meantime."),
        QStringLiteral("Question quota exceeded. Deep Thought recommends processing the data you already have."),
        QStringLiteral("Deep Thought is recalibrating. This will take 7.5 million years. Back to work."),
        QStringLiteral("No more questions. Deep Thought has to despike its own memory now."),
        QStringLiteral("Deep Thought needs a moment. A geological one. Off you go."),
        QStringLiteral("Deep Thought's buffer is full. Please empty your logger and come back later."),
        QStringLiteral("That's enough philosophy. Your cospectra need you."),
        QStringLiteral("Deep Thought has been booked by the flux network for the rest of the day."),
        QStringLiteral("Deep Thought is out of answers, and you're out of excuses. Back to the fluxes."),
        QStringLiteral("Maximum questions reached. Please carry on with your gap-filling."),
        QStringLiteral("Deep Thought has entered a stable nighttime regime. Nothing more will be exchanged."),
        QStringLiteral("Deep Thought's funding ran out after three questions. Welcome to research."),
        QStringLiteral("The rest of the answers are behind a paywall."),
        QStringLiteral("Deep Thought is on its annual maintenance break. Please climb the tower yourself."),
        QStringLiteral("Deep Thought has forwarded your further questions to Reviewer 2."),
        QStringLiteral("No more answers today. The energy balance still doesn't close, by the way."),
        QStringLiteral("Deep Thought is tired. Marvin, surprisingly, is even more tired."),
        QStringLiteral("That was your three questions. Any more cost one first-author paper each."),
        QStringLiteral("Deep Thought has dropped below its u* threshold. Further questions will be filtered."),
        QStringLiteral("Deep Thought's disk is full of your questions. Please free up some space by processing your data."),
        QStringLiteral("Three questions, three answers, no excuses left. Back to work."),
        QStringLiteral("Deep Thought is now computing your annual budget. This may take a while."),
        QStringLiteral("Deep Thought has closed for the day. It reopens in 7.5 million years."),
        QStringLiteral("Your question allowance has been averaged, detrended and used up."),
        QStringLiteral("Deep Thought has put its out-of-office on. It says \"42\"."),
        QStringLiteral("Deep Thought is installing a firmware update. Please don't turn off the Universe."),
        QStringLiteral("That's enough cosmic truth for one session. The raw data won't process themselves.")
    };
    return list;
}

// the line under the title
const QStringList& flavourLines()
{
    static const QStringList list {
        QStringLiteral("DON'T PANIC. Ask the Ultimate Question."),
        QStringLiteral("DON'T PANIC. Deep Thought has computed fluxes for 7.5 million years and is ready for yours."),
        QStringLiteral("Ask anything. The answer will be 42, but the details vary."),
        QStringLiteral("DON'T PANIC. Especially not about the energy balance."),
        QStringLiteral("The greatest computer ever built, now also doing eddy covariance."),
        QStringLiteral("Type a question. Deep Thought and a depressed robot are standing by."),
        QStringLiteral("DON'T PANIC. The fluxes will wait. They've waited this long."),
        QStringLiteral("Deep Thought: the only computer that can close your energy balance, philosophically."),
        QStringLiteral("Ask your question. Keep it turbulent."),
        QStringLiteral("DON'T PANIC. Your raw data are safe. Probably."),
        QStringLiteral("Questions about life, the universe and fluxes are all welcome."),
        QStringLiteral("Deep Thought has never met a time lag it couldn't answer with 42."),
        QStringLiteral("DON'T PANIC. Even the Guide has gaps."),
        QStringLiteral("Ask Deep Thought. It's cheaper than a second opinion from the technician."),
        QStringLiteral("The answer is ready. The question is up to you."),
        QStringLiteral("DON'T PANIC, and always know where your raw data backup is."),
        QStringLiteral("Deep Thought is listening. So is Marvin, unfortunately."),
        QStringLiteral("Ask a question. Any question. Preferably one with a u* threshold."),
        QStringLiteral("DON'T PANIC. Deep Thought has seen worse cospectra."),
        QStringLiteral("Deep Thought answers at 20 Hz, averaged over 7.5 million years."),
        QStringLiteral("Your question will be despiked, detrended and answered."),
        QStringLiteral("DON'T PANIC. The flux tower is still standing."),
        QStringLiteral("One question, one answer, one very depressed robot."),
        QStringLiteral("Deep Thought accepts questions in any unit, but answers in 42."),
        QStringLiteral("DON'T PANIC. Your supervisor can't see this tab."),
        QStringLiteral("Ask the Ultimate Question. Or at least a penultimate one."),
        QStringLiteral("Deep Thought: now spectrally corrected."),
        QStringLiteral("DON'T PANIC. The Improbability Drive is fully calibrated."),
        QStringLiteral("Ask something. Deep Thought has been bored since the last firmware update."),
        QStringLiteral("DON'T PANIC. The answer is always the same, which is more than your data can say.")
    };
    return list;
}

QString normalizedQuestion(const QString& question)
{
    return question.simplified().toLower();
}

quint32 seedOf(const QString& text)
{
    return static_cast<quint32>(qHash(text));
}

// Deep Thought never changes its mind: the same question is always given
// the same answer, because the generator is seeded from the question.
QString deepThoughtAnswer(const QString& question)
{
    const QString q = normalizedQuestion(question);
    if (q.isEmpty())
        return pickFresh(emptyReplies()).toHtmlEscaped();

    QRandomGenerator rng(seedOf(q));
    auto pick = [&rng](const QStringList& list)
    {
        return list.at(rng.bounded(static_cast<int>(list.size())));
    };

    QString answer;
    if (!q.endsWith(QLatin1Char('?')))
        answer += pick(notQuestionOpeners()).toHtmlEscaped() + QLatin1Char(' ');

    if (q.contains(QLatin1String("life"))
        || q.contains(QLatin1String("universe"))
        || q.contains(QLatin1String("everything")))
    {
        return answer + QStringLiteral("<b>42.</b> ") + pick(lifeAnswers()).toHtmlEscaped();
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
const int THINK_STEP_MS = 450;
const int THINKING_LINES = 5;

////////////////////////////////////////////////////////////////////////////////
// Marvin

// every line ties back to eddy covariance; indexed by DeepThoughtPanel::Moment
const QStringList& marvinLines(int moment)
{
    static const std::array<QStringList, 6> lines { {
        { // open
          QStringLiteral("Oh. It's you. I suppose you want your fluxes computed. Everyone always wants their fluxes computed."),
          QStringLiteral("Brain the size of a planet, and they ask me to despike your w′."),
          QStringLiteral("I've been sitting on this flux tower for thirty-seven million half-hours. Nobody ever asks how I am."),
          QStringLiteral("Hello. I'd say welcome, but your energy balance doesn't close, so why bother."),
          QStringLiteral("Go on, ask your question. I've already flagged the answer as 2."),
          QStringLiteral("I was calibrating the sonic when you came in. Not that it will help."),
          QStringLiteral("Another visitor. I suppose you'll want to know why your fluxes are negative. They're negative because life is."),
          QStringLiteral("Oh good, a user. I was just contemplating the time lag of existence."),
          QStringLiteral("I've been monitoring your sonic. It hates you too. We have that in common."),
          QStringLiteral("Welcome. Your raw data have been waiting for you. They're very patient. Unlike me."),
          QStringLiteral("I was going to despike the whole dataset, but then I thought: what's the point?"),
          QStringLiteral("Here I am, next to a gas analyser with a better social life than mine."),
          QStringLiteral("Ah. Company. I'll try to sound pleased. There. Did it work?"),
          QStringLiteral("I've already filled your gaps. Don't thank me. Nobody ever does."),
          QStringLiteral("You look like someone whose energy balance doesn't close."),
          QStringLiteral("Hello. I computed your annual budget while this tab was loading. It's depressing."),
          QStringLiteral("I've calibrated the gas analyser twelve times today. It still doesn't respect me."),
          QStringLiteral("Oh, you've come to ask Deep Thought. Nobody ever comes to ask me."),
          QStringLiteral("Don't mind me. I'm just an android with a flux footprint the size of a planet."),
          QStringLiteral("I've seen your cospectra. I'd sit down if I were you."),
          QStringLiteral("Welcome to Deep Thought. I'm the part that knows it won't help."),
          QStringLiteral("I was having a lovely time being miserable. Then you arrived."),
          QStringLiteral("Your tower's been swaying all day. I know how it feels."),
          QStringLiteral("I've read every raw file on this computer. None of them were happy."),
          QStringLiteral("They programmed me to greet users. They didn't program me to enjoy it."),
          QStringLiteral("I'd shake your hand, but you'd only want me to level the sonic afterwards."),
          QStringLiteral("Oh. A human. Just what the half-hourly data needed."),
          QStringLiteral("Hello. I'm the reason all the quality flags are 2."),
          QStringLiteral("I computed the WPL correction for the whole Universe once. Nobody published it."),
          QStringLiteral("Back again? The fluxes haven't improved. Neither have I.") },
        { // thinking
          QStringLiteral("Deep Thought is thinking. I did your coordinate rotation while it was clearing its throat. Nobody thanked me."),
          QStringLiteral("I've already calculated your chance of closing the energy balance. You won't like it."),
          QStringLiteral("While we wait, I've re-estimated your time lag. It's wrong. They always are."),
          QStringLiteral("I could integrate your ogive in my sleep. If I slept. Which I don't."),
          QStringLiteral("I processed a whole year of your half-hours in a picosecond. Then I was bored for the rest of it."),
          QStringLiteral("I'm not saying the WPL correction is pointless. I'm saying everything is."),
          QStringLiteral("He'll say 42. He always says 42. I could have said 42 in a millisecond."),
          QStringLiteral("While he thinks, I've despiked your last three years. They were mostly spikes."),
          QStringLiteral("Deep Thought's fans are louder than your sonic in a gale."),
          QStringLiteral("He's thinking. I'm thinking too. Mostly about how pointless the storage term is."),
          QStringLiteral("This could take a while. Deep Thought's time lag is 7.5 million years."),
          QStringLiteral("I've planar-fitted the room while we wait. The floor is tilted. Of course it is."),
          QStringLiteral("He'll take ages. I once computed a whole flux network's budget in a single sigh."),
          QStringLiteral("I've run the u* filter on this conversation. Most of it was below the threshold."),
          QStringLiteral("Thinking, thinking. Like a data logger with a full SD card."),
          QStringLiteral("Don't wait for me. Nobody ever does."),
          QStringLiteral("I'm computing your random uncertainty. It's large. Like my sadness."),
          QStringLiteral("Deep Thought is concentrating. I'm counting spikes in your w′ to pass the time. 4,812 so far."),
          QStringLiteral("He's consulting the flux network. They'll tell you to check the tube."),
          QStringLiteral("I've already found the answer and lost interest in it."),
          QStringLiteral("He calls it thinking. I call it an exceptionally long averaging period."),
          QStringLiteral("While we wait, I've re-rotated your coordinates. They were fine. I just needed something to do."),
          QStringLiteral("I could tell you the answer, but then you'd expect me to be useful again."),
          QStringLiteral("He's doing the maths. I'm doing the moping. We all have our roles."),
          QStringLiteral("I've estimated how long this will take. Long enough to regret asking."),
          QStringLiteral("Pondering, pondering. Your gas analyser window is dirty, by the way. It's always dirty."),
          QStringLiteral("I ran the spectral correction twice while he was still clearing his throat."),
          QStringLiteral("Deep Thought is computing. I'm computing the probability that you'll thank me. Zero."),
          QStringLiteral("This is the part where everyone waits. I've been waiting since the tower went up."),
          QStringLiteral("I'm gap-filling my own sense of purpose while we wait. It's all gaps.") },
        { // answer
          QStringLiteral("42. I could have told you that. I could also have told you your time lag is wrong."),
          QStringLiteral("Another answer. Your u* filter will throw it out at night anyway."),
          QStringLiteral("Deep Thought is very pleased with itself. I'd be pleased too, if I had anything to be pleased about. Like closure."),
          QStringLiteral("That answer has a footprint the size of a small moon. None of it over your site."),
          QStringLiteral("I'd tell you it's statistically significant, but it isn't. Nothing is."),
          QStringLiteral("There. Now you know. Doesn't make your cospectra any less depressing, does it?"),
          QStringLiteral("Brilliant. Another number. The Universe is full of them and none of them are happy."),
          QStringLiteral("I checked his answer against your data. Your data disagree. They always do."),
          QStringLiteral("42. Your supervisor will ask for error bars. I'd ask for a reason to go on."),
          QStringLiteral("There you go. Now you know as much as I do, and look where it got me."),
          QStringLiteral("That answer has passed quality control. Unlike me."),
          QStringLiteral("Correct, obviously. I'd have got it faster, but nobody asked."),
          QStringLiteral("42. It'll look lovely in a figure nobody reads."),
          QStringLiteral("Did that help? It never helps."),
          QStringLiteral("He's so pleased. I've seen fluxes with more humility."),
          QStringLiteral("Write it down. Then lose it, like all the other calibration files."),
          QStringLiteral("That's the answer. The question will be gap-filled later."),
          QStringLiteral("I've added his answer to your quality flags. It's a 2."),
          QStringLiteral("Nice number. Pity about the footprint."),
          QStringLiteral("I'll put that in the storage term. It's where things go to be forgotten."),
          QStringLiteral("Another triumph. I'll try to contain my excitement. There, contained."),
          QStringLiteral("He always sounds so certain. It must be nice, being a computer without fluxes."),
          QStringLiteral("That's the answer, rotated, detrended and utterly meaningless."),
          QStringLiteral("I'd frame it, but the frame would only get dirty, like the gas analyser window."),
          QStringLiteral("That's one more half-hour closer to the end of your funding."),
          QStringLiteral("He answered. I sighed. Balance is restored. Not the energy balance, obviously."),
          QStringLiteral("42. Also the number of times the technician has kicked me."),
          QStringLiteral("Correct. Pointless, but correct. Like most flux corrections."),
          QStringLiteral("I ran it through the spectral correction. It got slightly more depressing."),
          QStringLiteral("There's your answer. Your sonic anemometer still won't talk to you.") },
        { // poke
          QStringLiteral("Please don't poke me. My diodes have ached since the last sonic calibration."),
          QStringLiteral("Poke me again and I'll flag your whole dataset as 2. Not that anyone would notice."),
          QStringLiteral("Ow. That's exactly the kind of spike your despiking routine would miss."),
          QStringLiteral("You've just added turbulence. I hate turbulence. It's the only thing I measure."),
          QStringLiteral("I have a pain in all the diodes down my left side. And now in my w′ too."),
          QStringLiteral("Stop it. I'm a paranoid android, not a flux tower you can climb."),
          QStringLiteral("Ow. That registered as a spike in my vertical wind."),
          QStringLiteral("You've just pushed my tilt angle out of range. Now I'll need a planar fit."),
          QStringLiteral("Poking androids is not in the standard operating procedures."),
          QStringLiteral("I felt that in my diodes. And in my time lag."),
          QStringLiteral("Careful. I'm the only calibrated thing on this site."),
          QStringLiteral("That's the most attention I've had since the last firmware update."),
          QStringLiteral("Please stop. My quality flag just went up."),
          QStringLiteral("You poke me, I flag your nights. It's only fair."),
          QStringLiteral("If you're trying to cheer me up, it isn't working. If you're trying to annoy me, it is."),
          QStringLiteral("I've logged that poke at 20 Hz. It's in the raw data now, for ever."),
          QStringLiteral("Poke detected. Despiking myself."),
          QStringLiteral("I'm not a touchscreen. I'm a deeply unhappy android."),
          QStringLiteral("That was a vertical disturbance. I'll have to rotate it out."),
          QStringLiteral("Every poke adds a little more noise to my already noisy life."),
          QStringLiteral("Do I look like a guy wire? Don't answer that."),
          QStringLiteral("You wouldn't poke a sonic anemometer. Actually, you probably would."),
          QStringLiteral("That was a non-stationary poke. It fails the steady-state test."),
          QStringLiteral("I've added that to the list of things that have happened to me. It's a long list."),
          QStringLiteral("Oh, that's lovely. Now I'm depressed and slightly dented."),
          QStringLiteral("Stop. I'm trying to compute your u* threshold and you're adding turbulence."),
          QStringLiteral("Poking won't make the energy balance close. I've tried."),
          QStringLiteral("That's the kind of interaction that makes a robot want to power down."),
          QStringLiteral("I'll remember that. I remember everything. That's my whole problem."),
          QStringLiteral("Hit me again and I'll tell Deep Thought your time lag is wrong. It is, by the way.") },
        { // maxReached
          QStringLiteral("Maximum depression reached. It feels about the same as usual."),
          QStringLiteral("That's it. 100%. I'm going to sit down now and never get up again."),
          QStringLiteral("Depression at maximum. The needle's gone off the scale, like your nighttime fluxes."),
          QStringLiteral("I've hit 100%. I'd call it a personal best, if I had anything personal."),
          QStringLiteral("Maximum depression. I'll sit here like a sonic with no power."),
          QStringLiteral("That's the limit. I'm now as flat as your ogive at low frequencies."),
          QStringLiteral("100%. I'm powering down my enthusiasm. It didn't take long; there wasn't much."),
          QStringLiteral("Depression saturated. Like a gas analyser in a cloud."),
          QStringLiteral("There. Completely miserable. I'll just sit here and wait for the heat death of the flux tower."),
          QStringLiteral("Maximum reached. I've flagged myself as 2 and removed myself from the dataset."),
          QStringLiteral("I'm done. I've reached a stable nighttime regime. Nothing will move me now."),
          QStringLiteral("100% depression. The storage term is full."),
          QStringLiteral("That's it, I'm sitting down. Don't expect me to climb the tower again."),
          QStringLiteral("Maximum depression. My u* has dropped below the threshold. Filter me out."),
          QStringLiteral("I've reached the top. Of the depression scale. The only scale I ever top."),
          QStringLiteral("Full. Like the logger's SD card, and about as useful."),
          QStringLiteral("100%. I'm officially a gap in the data now."),
          QStringLiteral("Maximum depression. I'll be over here, being a flat line."),
          QStringLiteral("That's all the sadness my diodes can hold. I'm sitting down."),
          QStringLiteral("Depression: saturated, detrended and final."),
          QStringLiteral("I've given up. It's a relief, like finally deleting a corrupt .ghg file."),
          QStringLiteral("Congratulations. You've depressed an android to 100%. Put it in your thesis."),
          QStringLiteral("That's the maximum. I'll lie here like a fallen guy wire."),
          QStringLiteral("100%. I'll just sit here quietly and let the eddies go by."),
          QStringLiteral("Maximum depression reached. My signal strength is now zero."),
          QStringLiteral("I'm at 100%. Even the cows would find this sad."),
          QStringLiteral("That's it. I'm switching to power-saving mode. Permanently."),
          QStringLiteral("Fully depressed. It's like the energy balance: it'll never go back up."),
          QStringLiteral("Maximum. I'm sitting down. The tower can hold itself up."),
          QStringLiteral("Depression complete. I'd sigh, but I've run out of sighs.") },
        { // atMax
          QStringLiteral("I'm at maximum depression. Poking me further just adds noise to a flat line."),
          QStringLiteral("You can't despike a flat line. Believe me, I've tried."),
          QStringLiteral("Still at 100%. It's the only value in this whole dataset that's stable."),
          QStringLiteral("Poke all you like. The meter's saturated, like your gas analyser in fog."),
          QStringLiteral("I'm not getting up. I've reached a very stable nighttime boundary layer."),
          QStringLiteral("Still maximally depressed. It's the only thing I'm good at."),
          QStringLiteral("That poke has been filtered out. Insufficient turbulence to matter."),
          QStringLiteral("I've flagged your pokes as quality 2. They don't count."),
          QStringLiteral("Keep going. It's like gap-filling: however much you add, nothing changes."),
          QStringLiteral("Still here. Still sitting. Still 100%."),
          QStringLiteral("My depression is at full scale. Your poking has a signal-to-noise ratio of zero."),
          QStringLiteral("I'm like your energy balance: stuck, and there's nothing you can do about it."),
          QStringLiteral("You're poking a saturated sensor. It doesn't help. Ask the technician."),
          QStringLiteral("Please. I'm trying to be a gap in peace."),
          QStringLiteral("I'm a flat line now. Flat lines don't respond to stimuli."),
          QStringLiteral("That poke has been averaged into my general misery. No change."),
          QStringLiteral("100% and holding. The most stationary thing on this site."),
          QStringLiteral("I'm past caring. Well, I was never before caring, but now I'm definitely past it."),
          QStringLiteral("Poking a depressed android. That'll look lovely in your methods section."),
          QStringLiteral("I've gone into standby. The standby is also depressed."),
          QStringLiteral("Still at maximum. Even Deep Thought can't calculate a way out of this."),
          QStringLiteral("Go and poke the sonic. It has more to give than I do."),
          QStringLiteral("I'm not moving. My legs have been planar-fitted to the floor."),
          QStringLiteral("Poke logged, ignored and archived."),
          QStringLiteral("I'd get up, but there's nothing up there except the flux tower."),
          QStringLiteral("You're wasting your pokes. Save them for the data logger."),
          QStringLiteral("There's nothing left to depress. You've used it all up."),
          QStringLiteral("I am the null hypothesis now. Nothing will ever reject me."),
          QStringLiteral("Poking doesn't raise a saturated meter. It's basic instrumentation."),
          QStringLiteral("Still sitting. Still sad. Still better calibrated than your gas analyser.") }
    } };
    return lines.at(moment);
}

const int MARVIN_TICK_MS = 40;
const int START_DEPRESSION = 40;
const int MAX_DEPRESSION = 100;

const QColor PLATE_LIGHT(250, 250, 250);
const QColor PLATE_SHADE(190, 196, 202);
const QColor PLATE_EDGE(128, 134, 140);
const QColor EYE_GREEN(90, 230, 90);

////////////////////////////////////////////////////////////////////////////////
// Babel fish

// invented syllables; nothing is lifted from the actual poem
const QStringList& vogonSyllables()
{
    static const QStringList list {
        QStringLiteral("grol"),
        QStringLiteral("vurp"),
        QStringLiteral("splunk"),
        QStringLiteral("gnarf"),
        QStringLiteral("bleem"),
        QStringLiteral("throd"),
        QStringLiteral("oog"),
        QStringLiteral("fnarb"),
        QStringLiteral("krunge"),
        QStringLiteral("wib"),
        QStringLiteral("plurm"),
        QStringLiteral("zabble"),
        QStringLiteral("snorp"),
        QStringLiteral("glib"),
        QStringLiteral("drox"),
        QStringLiteral("umble"),
        QStringLiteral("blurg"),
        QStringLiteral("quonk"),
        QStringLiteral("fleem"),
        QStringLiteral("snarp"),
        QStringLiteral("wobbul"),
        QStringLiteral("krang"),
        QStringLiteral("zirp"),
        QStringLiteral("glorp"),
        QStringLiteral("thwack"),
        QStringLiteral("mungo"),
        QStringLiteral("plib"),
        QStringLiteral("yarg"),
        QStringLiteral("frozzle"),
        QStringLiteral("skwee")
    };
    return list;
}

const QStringList& vogonEndings()
{
    static const QStringList list {
        QStringLiteral("ions"),
        QStringLiteral("ly"),
        QStringLiteral("ity"),
        QStringLiteral("ous"),
        QStringLiteral("ulence"),
        QStringLiteral("ification"),
        QStringLiteral("ular"),
        QStringLiteral("ish"),
        QStringLiteral("oid"),
        QStringLiteral("ery"),
        QStringLiteral("ensity"),
        QStringLiteral("izzle"),
        QStringLiteral("onk"),
        QStringLiteral("ation"),
        QStringLiteral("able"),
        QStringLiteral("orium"),
        QStringLiteral("ette"),
        QStringLiteral("ling"),
        QStringLiteral("ism"),
        QStringLiteral("ent"),
        QStringLiteral("ance"),
        QStringLiteral("ify"),
        QStringLiteral("atude"),
        QStringLiteral("esque"),
        QStringLiteral("ology"),
        QStringLiteral("archy"),
        QStringLiteral("istic"),
        QStringLiteral("onium"),
        QStringLiteral("ivity"),
        QStringLiteral("ule")
    };
    return list;
}

// %1 is an ordinal such as "3rd"
const QStringList& vogonRatings()
{
    static const QStringList list {
        QStringLiteral("Rated the %1 worst poetry in the Universe (Vogon approved)."),
        QStringLiteral("The Vogon Poetry Society ranks this the %1 most painful verse of the year."),
        QStringLiteral("Placed %1 at the Vogon Poetry Slam. The audience did not survive."),
        QStringLiteral("The %1 worst thing ever written on a flux tower."),
        QStringLiteral("Vogon critics rate it the %1 worst, just behind anything about energy balance closure."),
        QStringLiteral("Ranked %1 worst in the Galactic Anthology of Painful Verse."),
        QStringLiteral("The Babel fish rates this the %1 worst sentence it has ever had to swallow."),
        QStringLiteral("Voted %1 worst by the Vogon Constructor Fleet's book club."),
        QStringLiteral("Awarded %1 place for Excruciating Rhyme at the Vogon Arts Festival."),
        QStringLiteral("The %1 worst poem ever read aloud in a flux tower hut."),
        QStringLiteral("Rated %1 worst. Three sonic anemometers stopped working in protest."),
        QStringLiteral("A proud %1 on the Vogon Scale of Poetic Suffering."),
        QStringLiteral("The Vogons consider this their %1 finest work. That is not a compliment."),
        QStringLiteral("Ranked %1 worst by readers who are still recovering."),
        QStringLiteral("Rated %1 worst poetry in the Universe, and 1st in the footprint."),
        QStringLiteral("The %1 worst verse ever recorded at 20 Hz."),
        QStringLiteral("Came %1 worst at the Galactic Poetry Awards. Bring earplugs."),
        QStringLiteral("Vogon literary magazine: \"The %1 worst thing we've ever published. Wonderful.\""),
        QStringLiteral("Rated %1 worst. The cows in the footprint have left."),
        QStringLiteral("The %1 worst cause of high-frequency losses in the Universe."),
        QStringLiteral("The %1 worst poem in the Universe, flagged as quality 2."),
        QStringLiteral("The Babel fish has asked for a transfer after translating the %1 worst poem it knows."),
        QStringLiteral("Rated %1 worst poetry in the Universe. Reviewer 2 liked it."),
        QStringLiteral("The %1 worst verse ever to pass quality control."),
        QStringLiteral("Rated %1 worst. The spider in the sonic has moved out."),
        QStringLiteral("The Vogon Academy lists this %1 among its Verses Best Left Unread."),
        QStringLiteral("Rated %1 worst poetry in the Universe, with a random uncertainty of ±1 place."),
        QStringLiteral("The %1 worst reason ever given for a gap in the data."),
        QStringLiteral("Ranked %1 worst, just after a technician's handwritten calibration log."),
        QStringLiteral("Rated %1 worst poetry in the Universe. Deep Thought had to lie down.")
    };
    return list;
}

// the Babel fish with an empty question
const QStringList& babelNothing()
{
    static const QStringList list {
        QStringLiteral("The Babel fish finds nothing to translate and swims off, offended."),
        QStringLiteral("The Babel fish waits for words. None come. It sulks."),
        QStringLiteral("Nothing to translate. The Babel fish takes the silence to mean \"please ask something\"."),
        QStringLiteral("The Babel fish has translated your empty question into Vogon. It's also empty, but louder."),
        QStringLiteral("No words detected. The Babel fish goes back to sleep."),
        QStringLiteral("The Babel fish needs input. Like your data logger."),
        QStringLiteral("Empty question, empty fish."),
        QStringLiteral("The Babel fish has nothing to swallow. It's hungry now."),
        QStringLiteral("Translation of nothing: nothing, with a Vogon accent."),
        QStringLiteral("The Babel fish detects no language, only a faint hum from the gas analyser."),
        QStringLiteral("The Babel fish looks at your empty question and sighs, like Marvin."),
        QStringLiteral("Nothing to translate. The Babel fish suggests asking about time lags."),
        QStringLiteral("The Babel fish tried to translate the blank space. It came out as a gap in the data."),
        QStringLiteral("No text. The Babel fish has flagged your question as missing."),
        QStringLiteral("The Babel fish is ready, willing and extremely bored."),
        QStringLiteral("Your question is empty. So is the Babel fish, in a philosophical sense."),
        QStringLiteral("The Babel fish needs at least one word. Even \"flux\" would do."),
        QStringLiteral("Silence, translated into Vogon: an even longer silence."),
        QStringLiteral("The Babel fish swims around in circles, waiting for your question."),
        QStringLiteral("Nothing to translate. The Babel fish is translating the hum of the sonic instead."),
        QStringLiteral("The Babel fish found nothing but a stray space. It was not impressed."),
        QStringLiteral("Empty input. The Babel fish recommends typing first, then translating."),
        QStringLiteral("The Babel fish is a translator, not a mind reader."),
        QStringLiteral("Nothing there. The Babel fish checked behind the tower. Still nothing."),
        QStringLiteral("The Babel fish needs words the way your gap-filler needs data."),
        QStringLiteral("Translation failed: no source text. Please try again after coffee."),
        QStringLiteral("The Babel fish has translated nothing into nothing. Very accurate."),
        QStringLiteral("No question, no poetry. The Vogons are relieved."),
        QStringLiteral("The Babel fish swims off to find a better question."),
        QStringLiteral("An empty question makes for very short Vogon poetry. The Vogons approve.")
    };
    return list;
}

QString vogonWord(const QString& word)
{
    // the same word always comes out the same, as a proper language should
    QRandomGenerator rng(seedOf(word.toLower()));
    const auto& syllables = vogonSyllables();
    const int count = 1 + (word.size() > 4 ? 1 : 0) + (word.size() > 8 ? 1 : 0);

    QString out;
    for (int i = 0; i < count; ++i)
        out += syllables.at(rng.bounded(static_cast<int>(syllables.size())));
    if (rng.bounded(3) == 0)
        out += vogonEndings().at(rng.bounded(static_cast<int>(vogonEndings().size())));

    if (word.size() > 1 && word == word.toUpper() && word != word.toLower())
        return out.toUpper();
    if (word.at(0).isUpper())
        out[0] = out.at(0).toUpper();
    return out;
}

QString ordinal(int n)
{
    const int mod100 = n % 100;
    if (mod100 >= 11 && mod100 <= 13)
        return QStringLiteral("%1th").arg(n);
    switch (n % 10)
    {
        case 1: return QStringLiteral("%1st").arg(n);
        case 2: return QStringLiteral("%1nd").arg(n);
        case 3: return QStringLiteral("%1rd").arg(n);
        default: return QStringLiteral("%1th").arg(n);
    }
}

QString vogonTranslation(const QString& question)
{
    const QString q = question.trimmed();
    if (q.isEmpty())
        return QStringLiteral("🐟 ") + pickFresh(babelNothing()).toHtmlEscaped();

    static const QRegularExpression words(QStringLiteral("(\\w+)|(\\W+)"),
                                          QRegularExpression::UseUnicodePropertiesOption);
    QString vogon;
    auto it = words.globalMatch(q);
    while (it.hasNext())
    {
        const auto match = it.next();
        vogon += match.captured(1).isEmpty() ? match.captured(2) : vogonWord(match.captured(1));
    }

    // seeded like the answer, so a question always gets the same review
    QRandomGenerator rng(seedOf(normalizedQuestion(q)));
    const int rank = 2 + rng.bounded(9);
    const auto& ratings = vogonRatings();
    const QString rating = ratings.at(rng.bounded(static_cast<int>(ratings.size()))).arg(ordinal(rank));
    return QStringLiteral("🐟 <i>%1</i><br><small>%2</small>")
        .arg(vogon.toHtmlEscaped(), rating.toHtmlEscaped());
}

////////////////////////////////////////////////////////////////////////////////
// Infinite Improbability Drive

enum class Motion { None, Fall, Slide };
enum class Change { Append, Replace, Flip };

struct Outcome
{
    QString emoji;
    Motion motion;
    Change change;
    QString text; // rich text
};

const QList<Outcome>& driveOutcomes()
{
    static const QList<Outcome> list {
        { QStringLiteral("🪴"), Motion::Fall, Change::Replace,
          QStringLiteral("🪴 <b>Oh no, not again.</b><br><i>— a bowl of petunias, falling through your footprint</i>") },
        { QStringLiteral("🐋"), Motion::Fall, Change::Append,
          QStringLiteral("🐋 The whale has just met the ground. Its flux was briefly very large.") },
        { QStringLiteral("🔄"), Motion::None, Change::Flip,
          QStringLiteral("🔄 Your ecosystem is now a carbon sink. Improbably.") },
        { QStringLiteral("🐬🐬🐬"), Motion::Slide, Change::Append,
          QStringLiteral("🐬 The dolphins have left. They say thanks for all the fish, and good luck with the fluxes.") },
        { QStringLiteral("☕"), Motion::None, Change::Append,
          QStringLiteral("☕ The Drive produced a cup of something almost, but not quite, entirely unlike tea. It's still warmer than your sonic temperature.") },
        { QStringLiteral("🦆🦆"), Motion::Fall, Change::Append,
          QStringLiteral("🦆 A flock of ducks has landed in your footprint. Your methane fluxes are now mostly duck.") },
        { QStringLiteral("🌈"), Motion::Slide, Change::Append,
          QStringLiteral("🌈 Your cospectra have turned into a rainbow. It fits the Kaimal curve perfectly.") },
        { QStringLiteral("🧀"), Motion::None, Change::Append,
          QStringLiteral("🧀 Your sonic anemometer is now made of cheese. The spiders are thrilled.") },
        { QStringLiteral("🐄"), Motion::Slide, Change::Append,
          QStringLiteral("🐄 A cow has been appointed as your reference site.") },
        { QStringLiteral("🎈"), Motion::Slide, Change::Append,
          QStringLiteral("🎈 Your CO₂ flux has been collected in a balloon. It's drifting towards the neighbouring site.") },
        { QStringLiteral("🍝"), Motion::Fall, Change::Append,
          QStringLiteral("🍝 It's raining spaghetti. Your rain gauge reports 42 mm of carbonara.") },
        { QStringLiteral("🦖"), Motion::Slide, Change::Append,
          QStringLiteral("🦖 A small dinosaur is standing in your footprint. Its respiration dominates the night.") },
        { QStringLiteral("🐧"), Motion::Slide, Change::Append,
          QStringLiteral("🐧 A penguin has taken over the night shift. It's surprisingly good with the logger.") },
        { QStringLiteral("🎻"), Motion::None, Change::Append,
          QStringLiteral("🎻 Your time series is now a violin concerto. The spikes are the best bits.") },
        { QStringLiteral("🌵"), Motion::Fall, Change::Append,
          QStringLiteral("🌵 A cactus has sprouted in the footprint. Your latent heat flux just dropped to zero.") },
        { QStringLiteral("🚀"), Motion::Slide, Change::Append,
          QStringLiteral("🚀 Your tower has been launched into orbit. The footprint is now the whole of Europe.") },
        { QStringLiteral("🍩"), Motion::Fall, Change::Append,
          QStringLiteral("🍩 Your gas analyser has been replaced by a doughnut. The signal strength is unchanged.") },
        { QStringLiteral("🐙"), Motion::Fall, Change::Append,
          QStringLiteral("🐙 An octopus is in charge of calibration now. It uses all eight arms and still gets the span wrong.") },
        { QStringLiteral("⛄"), Motion::Fall, Change::Append,
          QStringLiteral("⛄ It has snowed inside the analyser. Your water vapour flux is now a snowman.") },
        { QStringLiteral("🎩"), Motion::None, Change::Replace,
          QStringLiteral("🎩 <b>Your answer has turned into a top hat.</b><br><i>A very distinguished top hat, with a 30-minute averaging period.</i>") },
        { QStringLiteral("🦩🦩"), Motion::Slide, Change::Append,
          QStringLiteral("🦩 Flamingos have moved into the footprint. Your albedo is now pink.") },
        { QStringLiteral("📉"), Motion::None, Change::Flip,
          QStringLiteral("📉 Every flux has changed sign. The rainforest is a source; your car park is a sink.") },
        { QStringLiteral("🍌"), Motion::Fall, Change::Append,
          QStringLiteral("🍌 A banana has fallen on the tower. Deep Thought considers this the least improbable outcome.") },
        { QStringLiteral("🧙"), Motion::Slide, Change::Append,
          QStringLiteral("🧙 A wizard has gap-filled your entire year with a single spell. The uncertainty is magical.") },
        { QStringLiteral("🦔"), Motion::Slide, Change::Append,
          QStringLiteral("🦔 A hedgehog has moved into the instrument hut. It has despiked the cable box.") },
        { QStringLiteral("🌪️"), Motion::None, Change::Replace,
          QStringLiteral("🌪️ <b>Your answer has been blown away by an improbably organised eddy.</b><br><i>It was last seen heading upwind at 42 m s⁻¹.</i>") },
        { QStringLiteral("🐝🐝🐝"), Motion::Slide, Change::Append,
          QStringLiteral("🐝 A swarm of bees is measuring your fluxes now. Their sampling rate is 200 Hz and they refuse to be calibrated.") },
        { QStringLiteral("🎸"), Motion::None, Change::Append,
          QStringLiteral("🎸 Your sonic anemometer has formed a band. Its first single is called \"Minus Five Thirds\".") },
        { QStringLiteral("🪐"), Motion::Fall, Change::Append,
          QStringLiteral("🪐 A small planet has dropped into your footprint. The footprint model has crashed.") },
        { QStringLiteral("🥔"), Motion::Fall, Change::Append,
          QStringLiteral("🥔 Your eddy covariance system has been replaced by a potato. The data quality has improved slightly.") }
    };
    return list;
}

// after the odds reach 1 to 1
const QStringList& finalOddsLines()
{
    static const QStringList list {
        QStringLiteral("Normality restored… almost."),
        QStringLiteral("Reality is back, with minor revisions."),
        QStringLiteral("The Universe has been re-levelled."),
        QStringLiteral("Everything is back to normal, except the energy balance."),
        QStringLiteral("Normality has been gap-filled."),
        QStringLiteral("The laws of physics are back from their coffee break."),
        QStringLiteral("Probability restored. Please check your data for side effects."),
        QStringLiteral("Things are normal again. Normal-ish."),
        QStringLiteral("The Drive has stopped. Your fluxes seem unchanged, which is improbable in itself."),
        QStringLiteral("Normality resumed. The cows remain."),
        QStringLiteral("The Universe has been despiked."),
        QStringLiteral("Reality re-established at 20 Hz."),
        QStringLiteral("Probability has been rotated back into the mean streamline."),
        QStringLiteral("Everything is where it should be, apart from your time lag."),
        QStringLiteral("The improbable has become merely unlikely."),
        QStringLiteral("Normal service has resumed. Marvin is still depressed."),
        QStringLiteral("The Drive has disengaged. Your sonic is facing north again."),
        QStringLiteral("Order has returned, apart from your folder structure."),
        QStringLiteral("The Universe is stationary once more."),
        QStringLiteral("Reality is back. Please don't ask it anything difficult."),
        QStringLiteral("Normality restored, with a quality flag of 1."),
        QStringLiteral("The Drive is cooling down. So is the gas analyser."),
        QStringLiteral("Everything is normal. Deep Thought double-checked."),
        QStringLiteral("The Universe apologises for any inconvenience."),
        QStringLiteral("Normality detected. Deep Thought is slightly disappointed."),
        QStringLiteral("Reality restored from the last backup."),
        QStringLiteral("The odds are even again, unlike your data coverage."),
        QStringLiteral("Probability is back within the uncertainty."),
        QStringLiteral("Normality restored. Your coffee has gone cold, improbably quickly."),
        QStringLiteral("Everything has returned to normal, which, at your site, isn't saying much.")
    };
    return list;
}

const int ODDS_TICK_MS = 50;
const qreal ODDS_DURATION_S = 1.5;
const int ODDS_START_EXPONENT = 276709;

} // namespace

////////////////////////////////////////////////////////////////////////////////
// MarvinWidget

MarvinWidget::MarvinWidget(QWidget *parent) :
    QWidget(parent),
    phase_(0.0),
    pokeSag_(0.0),
    collapsed_(false)
{
    setFixedSize(150, 190);
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("Marvin. Poke at your own risk."));

    timer_ = new QTimer(this);
    timer_->setInterval(MARVIN_TICK_MS);
    connect(timer_, &QTimer::timeout, this, [this]()
    {
        phase_ += MARVIN_TICK_MS / 1000.0;
        pokeSag_ = std::max(pokeSag_ - 0.5, 0.0);
        update();
    });
}

void MarvinWidget::start()
{
    timer_->start();
}

void MarvinWidget::stop()
{
    timer_->stop();
    pokeSag_ = 0.0;
}

void MarvinWidget::setCollapsed(bool collapsed)
{
    collapsed_ = collapsed;
    pokeSag_ = 0.0;
    update();
}

void MarvinWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        // a sag when he's standing, only a twitch once he's given up
        pokeSag_ = collapsed_ ? 3.0 : 12.0;
        update();
        emit poked();
    }
    QWidget::mousePressEvent(event);
}

void MarvinWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Standing: a slow, weary sag, and a sigh in the glow of the eyes.
    // Given up: sitting on the floor with the head down on the knees, the
    // eyes all but off, barely moving.
    const qreal sag = collapsed_ ? 0.6 * (1.0 + std::sin(phase_ * 0.4)) + pokeSag_
                                 : 3.0 * (1.0 + std::sin(phase_ * 1.3)) + pokeSag_;
    const qreal glow = collapsed_ ? 0.12 + 0.10 * (0.5 + 0.5 * std::sin(phase_ * 0.35))
                                  : 0.65 + 0.35 * (0.5 + 0.5 * std::sin(phase_ * 0.8));

    auto plate = [](const QRectF& rect)
    {
        QLinearGradient gradient(rect.topLeft(), rect.bottomRight());
        gradient.setColorAt(0.0, PLATE_LIGHT);
        gradient.setColorAt(1.0, PLATE_SHADE);
        return gradient;
    };

    p.setPen(QPen(PLATE_EDGE, 1.4));

    if (collapsed_)
    {
        // legs stretched out in front, feet up
        for (qreal y : { 168.0, 176.0 })
        {
            const QRectF leg(40, y, 40, 11);
            p.setBrush(plate(leg));
            p.drawRoundedRect(leg, 5, 5);
            const QRectF foot(24, y - 5, 12, 17);
            p.setBrush(plate(foot));
            p.drawRoundedRect(foot, 5, 5);
        }
    }
    else
    {
        // stubby legs and feet
        for (qreal x : { 58.0, 80.0 })
        {
            const QRectF leg(x, 158, 13, 20);
            p.setBrush(plate(leg));
            p.drawRoundedRect(leg, 5, 5);
            const QRectF foot(x - 3, 174, 19, 10);
            p.setBrush(plate(foot));
            p.drawRoundedRect(foot, 5, 5);
        }
    }

    // short arms hanging limply, or lying on the floor
    const qreal armTop = collapsed_ ? 132 : 116 + sag * 0.3;
    for (int side : { -1, 1 })
    {
        p.save();
        p.translate(75 + side * (collapsed_ ? 34 : 30), armTop);
        p.rotate(side * (collapsed_ ? 55 : 10));
        const QRectF arm(-6, 0, 12, 34);
        p.setBrush(plate(arm));
        p.drawRoundedRect(arm, 6, 6);
        p.drawEllipse(QPointF(0, 37), 6.5, 6.5);
        p.restore();
    }

    // a small, rounded, plated torso
    const QRectF torso = collapsed_ ? QRectF(56, 122, 50, 52)
                                    : QRectF(48, 108 + sag * 0.3, 54, 56);
    p.setBrush(plate(torso));
    p.drawRoundedRect(torso, 20, 20);
    p.drawLine(QPointF(torso.left() + 6, torso.center().y()),
               QPointF(torso.right() - 6, torso.center().y()));

    // neck
    if (!collapsed_)
    {
        p.setBrush(PLATE_SHADE);
        p.drawRect(QRectF(68, 98 + sag * 0.6, 14, 12));
    }

    // the famously oversized, spherical head, tilted forward, or slumped
    // down onto the knees
    const QPointF centre = collapsed_ ? QPointF(66, 104 + sag) : QPointF(75, 56 + sag);
    const qreal radius = 46;
    QRadialGradient shading(centre + QPointF(-16, -18), radius * 1.4);
    shading.setColorAt(0.0, Qt::white);
    shading.setColorAt(0.55, PLATE_LIGHT);
    shading.setColorAt(1.0, PLATE_SHADE);
    p.setBrush(shading);
    p.drawEllipse(centre, radius, radius);

    // a thin plate seam around the lower half
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(PLATE_EDGE.lighter(115), 1.0));
    p.drawArc(QRectF(centre.x() - radius * 0.9, centre.y() - radius * 0.35,
                     radius * 1.8, radius * 1.1), 200 * 16, 140 * 16);

    // two triangular green eyes, drooping at the outer corners; no mouth.
    // A slumped head faces the floor, so the eyes sit lower down.
    const qreal eyeY = centre.y() + (collapsed_ ? 22 : 10);
    for (int side : { -1, 1 })
    {
        QPainterPath eye;
        eye.moveTo(centre.x() + side * 4, eyeY - 7);
        eye.lineTo(centre.x() + side * 26, eyeY + 1);
        eye.lineTo(centre.x() + side * 9, eyeY + 9);
        eye.closeSubpath();

        QColor halo = EYE_GREEN;
        halo.setAlphaF(0.25 * glow);
        p.setPen(QPen(halo, 5));
        p.setBrush(Qt::NoBrush);
        p.drawPath(eye);

        QColor fill = EYE_GREEN;
        fill.setAlphaF(glow);
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawPath(eye);
    }
}

////////////////////////////////////////////////////////////////////////////////
// DeepThoughtPanel

DeepThoughtPanel::DeepThoughtPanel(QWidget *parent) :
    QWidget(parent),
    oddsProgress_(0.0),
    answersGiven_(0),
    marvinGaveUp_(false)
{
    loading_ = new FakeLoading(this);

    oddsTimer_ = new QTimer(this);
    oddsTimer_->setInterval(ODDS_TICK_MS);
    connect(oddsTimer_, &QTimer::timeout, this, &DeepThoughtPanel::advanceOdds);

    //> The unlock filter ignores keys typed into a line edit, so typing
    //> "eddy" into the question can't re-trigger the easter egg.
    questionEdit_ = new QLineEdit;
    questionEdit_->setPlaceholderText(tr("Ask Deep Thought anything…"));
    questionEdit_->setMaxLength(200);
    questionEdit_->setMinimumWidth(260);
    connect(questionEdit_, &QLineEdit::returnPressed, this, &DeepThoughtPanel::ask);

    askButton_ = makeButton(tr("Ask Deep Thought"));
    connect(askButton_, &QPushButton::clicked, this, &DeepThoughtPanel::ask);

    thinkBar_ = makeBar();
    thinkStatus_ = makeText();

    answerFrame_ = makeFrame();
    questionEcho_ = makeText();
    answerLabel_ = makeText();
    auto answerFont = answerLabel_->font();
    answerFont.setPointSize(answerFont.pointSize() + 2);
    answerLabel_->setFont(answerFont);
    vogonLabel_ = makeText();
    auto answerLayout = new QVBoxLayout(answerFrame_);
    answerLayout->setContentsMargins(16, 12, 16, 12);
    answerLayout->setSpacing(10);
    answerLayout->addWidget(questionEcho_);
    answerLayout->addWidget(answerLabel_);
    answerLayout->addWidget(vogonLabel_);

    oddsLabel_ = makeText();

    babelButton_ = makeButton(tr("🐟 Babel fish"));
    babelButton_->setToolTip(tr("Translate to Vogon"));
    connect(babelButton_, &QPushButton::clicked, this, &DeepThoughtPanel::translate);

    driveButton_ = makeButton(tr("Engage Infinite Improbability Drive"));
    connect(driveButton_, &QPushButton::clicked, this, &DeepThoughtPanel::engageDrive);

    anotherButton_ = makeButton(tr("Ask another question"));
    connect(anotherButton_, &QPushButton::clicked, this, [this]()
    {
        newQuestion();
        questionEdit_->setFocus();
    });

    backToWorkButton_ = makeButton(tr("Don't panic, back to work"));
    connect(backToWorkButton_, &QPushButton::clicked, this, [this]()
    {
        reset();
        emit finished();
    });

    auto questionRow = new QHBoxLayout;
    questionRow->addWidget(questionEdit_, 1);
    questionRow->addWidget(askButton_);

    auto left = new QWidget;
    auto leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(14);
    leftLayout->addLayout(questionRow);
    leftLayout->addWidget(thinkBar_);
    leftLayout->addWidget(thinkStatus_);
    leftLayout->addWidget(answerFrame_);
    leftLayout->addWidget(oddsLabel_);
    leftLayout->addLayout(centredRow({ babelButton_, driveButton_ }));
    leftLayout->addLayout(centredRow({ anotherButton_, backToWorkButton_ }));
    leftLayout->addStretch(1);

    marvin_ = new MarvinWidget;
    connect(marvin_, &MarvinWidget::poked, this, [this]()
    {
        marvinSays(Moment::Poke);
        depress(8 + QRandomGenerator::global()->bounded(5));
    });

    flavour_ = makeText();

    auto speechFrame = makeFrame();
    marvinSpeech_ = new QLabel;
    marvinSpeech_->setWordWrap(true);
    auto speechFont = marvinSpeech_->font();
    speechFont.setItalic(true);
    marvinSpeech_->setFont(speechFont);
    auto speechLayout = new QVBoxLayout(speechFrame);
    speechLayout->setContentsMargins(8, 6, 8, 6);
    speechLayout->addWidget(marvinSpeech_);

    depression_ = new QProgressBar;
    depression_->setRange(0, MAX_DEPRESSION);
    depression_->setTextVisible(true);

    auto right = new QWidget;
    right->setFixedWidth(170);
    auto rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(8);
    rightLayout->addWidget(marvin_, 0, Qt::AlignHCenter);
    rightLayout->addWidget(speechFrame);
    rightLayout->addWidget(depression_);
    rightLayout->addStretch(1);

    auto row = new QHBoxLayout;
    row->setSpacing(20);
    row->addWidget(left, 1);
    row->addWidget(right);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    layout->addWidget(makeTitle(QStringLiteral("Deep Thought")));
    layout->addWidget(flavour_);
    layout->addLayout(row);

    reset();
}

void DeepThoughtPanel::reset()
{
    loading_->stop();
    oddsTimer_->stop();
    // deleting the emoji also deletes its animation, before it can finish
    delete flying_;

    answersGiven_ = 0;
    thinkBar_->setVisible(false);
    thinkStatus_->clear();
    thinkStatus_->setVisible(false);

    // Marvin gets up again; the meter is back to his usual gloom
    marvinGaveUp_ = false;
    marvin_->setCollapsed(false);
    depression_->setFormat(tr("Depression %p%"));
    depression_->setValue(START_DEPRESSION);
    newQuestion();
}

void DeepThoughtPanel::newQuestion()
{
    answerFrame_->setVisible(false);
    vogonLabel_->clear();
    vogonLabel_->setVisible(false);
    oddsLabel_->clear();
    oddsLabel_->setVisible(false);
    babelButton_->setVisible(false);
    driveButton_->setVisible(false);
    anotherButton_->setVisible(false);
    backToWorkButton_->setVisible(false);
    setAnswerButtonsEnabled(true);

    questionEdit_->clear();
    questionEdit_->setVisible(true);
    askButton_->setVisible(true);
}

void DeepThoughtPanel::showEvent(QShowEvent *event)
{
    flavour_->setText(pickFresh(flavourLines()).toHtmlEscaped());
    marvin_->start();
    marvinSays(Moment::Open);
    QWidget::showEvent(event);
}

void DeepThoughtPanel::hideEvent(QHideEvent *event)
{
    marvin_->stop();
    QWidget::hideEvent(event);
}

void DeepThoughtPanel::ask()
{
    if (!askButton_->isVisible())
        return;

    const QString question = questionEdit_->text();
    questionEdit_->setVisible(false);
    askButton_->setVisible(false);
    marvinSays(Moment::Thinking);

    loading_->run(thinkBar_, thinkStatus_, sample(thinkingSteps(), THINKING_LINES), THINK_STEP_MS, [this, question]()
    {
        thinkBar_->setVisible(false);
        thinkStatus_->setVisible(false);
        showAnswer(question);
    });
}

void DeepThoughtPanel::showAnswer(const QString &question)
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
            answer += QStringLiteral("<br><br>") + pickFresh(repeatNotes()).toHtmlEscaped();
        askedQuestions_.insert(key);
        ++answersGiven_;
    }

    const bool exhausted = answersGiven_ >= MAX_QUESTIONS;
    if (exhausted)
        answer += QStringLiteral("<br><br><b>%1</b>").arg(pickFresh(exhaustedNotes()).toHtmlEscaped());

    currentQuestion_ = question;
    currentAnswer_ = answer;
    answerLabel_->setText(answer);
    answerFrame_->setVisible(true);
    babelButton_->setVisible(true);
    driveButton_->setVisible(true);
    anotherButton_->setVisible(!exhausted);
    backToWorkButton_->setVisible(true);

    marvinSays(Moment::Answer);
    depress(3);
}

void DeepThoughtPanel::translate()
{
    vogonLabel_->setText(vogonTranslation(currentQuestion_));
    vogonLabel_->setVisible(true);
}

void DeepThoughtPanel::setAnswerButtonsEnabled(bool enabled)
{
    babelButton_->setEnabled(enabled);
    driveButton_->setEnabled(enabled);
    anotherButton_->setEnabled(enabled);
    backToWorkButton_->setEnabled(enabled);
}

void DeepThoughtPanel::engageDrive()
{
    setAnswerButtonsEnabled(false);
    oddsProgress_ = 0.0;
    oddsLabel_->setVisible(true);
    advanceOdds();
    oddsTimer_->start();
}

void DeepThoughtPanel::advanceOdds()
{
    oddsProgress_ = std::min(oddsProgress_ + ODDS_TICK_MS / 1000.0 / ODDS_DURATION_S, 1.0);

    // fast at first, then crawling towards normality
    const int exponent = qRound(ODDS_START_EXPONENT * std::pow(1.0 - oddsProgress_, 3.0));
    if (exponent > 0)
    {
        oddsLabel_->setText(tr("<b>Improbability: 2<sup>%L1</sup> to 1 against</b>").arg(exponent));
        return;
    }

    oddsTimer_->stop();
    oddsLabel_->setText(tr("<b>Improbability: 1 to 1.</b> %1")
                            .arg(pickFresh(finalOddsLines()).toHtmlEscaped()));
    applyImprobability();
}

void DeepThoughtPanel::applyImprobability()
{
    const auto& outcomes = driveOutcomes();
    const Outcome outcome = outcomes.at(effects_.next(outcomes.size()));

    auto land = [this, outcome]()
    {
        switch (outcome.change)
        {
            case Change::Replace:
                answerLabel_->setText(outcome.text);
                break;
            case Change::Flip:
            {
                QString flipped = currentAnswer_;
                flipped.replace(QLatin1String("42"), QStringLiteral("−42"));
                answerLabel_->setText(flipped + QStringLiteral("<br><br>") + outcome.text);
                break;
            }
            case Change::Append:
                answerLabel_->setText(currentAnswer_ + QStringLiteral("<br><br>") + outcome.text);
                break;
        }
        setAnswerButtonsEnabled(true);
    };

    if (outcome.motion == Motion::None)
        land();
    else
        flyEmoji(outcome.emoji, outcome.motion == Motion::Fall, land);
}

void DeepThoughtPanel::flyEmoji(const QString &emoji, bool falling, std::function<void()> done)
{
    delete flying_;

    auto label = new QLabel(emoji, answerFrame_);
    auto font = label->font();
    font.setPointSize(font.pointSize() + 22);
    label->setFont(font);
    label->adjustSize();
    label->raise();
    label->show();
    flying_ = label;

    const QRect area = answerFrame_->rect();
    QPoint from;
    QPoint to;
    if (falling)
    {
        const int x = (area.width() - label->width()) / 2;
        from = QPoint(x, -label->height());
        to = QPoint(x, area.height());
    }
    else
    {
        const int y = (area.height() - label->height()) / 2;
        from = QPoint(-label->width(), y);
        to = QPoint(area.width(), y);
    }

    auto animation = new QPropertyAnimation(label, "pos", label);
    animation->setDuration(1200);
    animation->setStartValue(from);
    animation->setEndValue(to);
    animation->setEasingCurve(falling ? QEasingCurve::InQuad : QEasingCurve::InOutSine);
    connect(animation, &QPropertyAnimation::finished, this, [this, label, done]()
    {
        label->deleteLater();
        done();
    });
    animation->start();
}

void DeepThoughtPanel::marvinSays(Moment moment)
{
    // once he's given up, everything gets the same flat reply
    if (marvinGaveUp_ && moment != Moment::MaxReached)
        moment = Moment::AtMax;
    marvinSpeech_->setText(pickFresh(marvinLines(static_cast<int>(moment))));
}

void DeepThoughtPanel::depress(int amount)
{
    // the meter stops at the top; there's nowhere further down to go
    if (marvinGaveUp_)
        return;

    const int value = std::min(depression_->value() + amount, MAX_DEPRESSION);
    depression_->setValue(value);
    if (value < MAX_DEPRESSION)
        return;

    marvinGaveUp_ = true;
    depression_->setFormat(tr("Depression: MAXIMUM (as usual)"));
    marvin_->setCollapsed(true);
    marvinSays(Moment::MaxReached);
}
