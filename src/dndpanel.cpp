/***************************************************************************
  dndpanel.cpp
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

#include "dndpanel.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

using namespace EggUi;

namespace {

////////////////////////////////////////////////////////////////////////////////
// rules and data

enum Ability { Str, Dex, Con, Int, Wis, Cha, AbilityCount };

const std::array<const char*, AbilityCount> ABILITY_NAMES { "STR", "DEX", "CON", "INT", "WIS", "CHA" };

struct DndClass
{
    QString name;
    std::array<int, AbilityCount> bonus;
};

// every class's bonuses add up to +4; each new character is offered three
const QList<DndClass>& classes()
{
    static const QList<DndClass> list {
        { QStringLiteral("Field Technician"), { 2, 0, 2, 0, 0, 0 } },
        { QStringLiteral("Data Wizard"), { 0, 0, 0, 2, 2, 0 } },
        { QStringLiteral("Modelling Bard"), { 0, 1, 0, 0, 1, 2 } },
        { QStringLiteral("Flux Paladin"), { 2, 0, 0, 0, 0, 2 } },
        { QStringLiteral("Spectral Ranger"), { 0, 2, 0, 0, 2, 0 } },
        { QStringLiteral("Gap-Filling Necromancer"), { 0, 0, 0, 2, 0, 2 } },
        { QStringLiteral("Calibration Cleric"), { 0, 0, 2, 0, 2, 0 } },
        { QStringLiteral("Postdoc Rogue"), { 0, 2, 0, 2, 0, 0 } },
        { QStringLiteral("Tower Barbarian"), { 3, 0, 1, 0, 0, 0 } },
        { QStringLiteral("Footprint Druid"), { 0, 1, 0, 0, 3, 0 } },
        { QStringLiteral("Despiking Monk"), { 0, 3, 0, 0, 1, 0 } },
        { QStringLiteral("Metadata Warlock"), { 0, 0, 0, 3, 0, 1 } },
        { QStringLiteral("Night-Shift Fighter"), { 1, 0, 3, 0, 0, 0 } },
        { QStringLiteral("Grant-Writing Sorcerer"), { 0, 0, 0, 1, 0, 3 } },
        { QStringLiteral("Logger Artificer"), { 0, 2, 0, 2, 0, 0 } },
        { QStringLiteral("Guy-Wire Acrobat"), { 2, 2, 0, 0, 0, 0 } },
        { QStringLiteral("Reviewer-Taming Bard"), { 0, 0, 0, 0, 2, 2 } },
        { QStringLiteral("Soil-Chamber Heretic"), { 0, 0, 2, 2, 0, 0 } },
        { QStringLiteral("Remote-Sensing Envoy"), { 0, 0, 0, 1, 1, 2 } },
        { QStringLiteral("Storage-Term Hermit"), { 0, 0, 1, 1, 2, 0 } },
        { QStringLiteral("Planar-Fit Templar"), { 1, 0, 0, 1, 2, 0 } },
        { QStringLiteral("Closure Crusader"), { 0, 0, 2, 0, 1, 1 } },
        { QStringLiteral("Time-Lag Chronomancer"), { 0, 1, 0, 2, 1, 0 } },
        { QStringLiteral("Methane Alchemist"), { 0, 0, 2, 2, 0, 0 } },
        { QStringLiteral("Sonic Whisperer"), { 0, 0, 0, 0, 2, 2 } },
        { QStringLiteral("Firmware Warlock"), { 0, 1, 0, 2, 0, 1 } },
        { QStringLiteral("Cable-Tie Artisan"), { 0, 3, 1, 0, 0, 0 } },
        { QStringLiteral("Energy-Balance Oracle"), { 0, 0, 0, 1, 3, 0 } },
        { QStringLiteral("Undergraduate Squire"), { 1, 1, 1, 0, 0, 1 } },
        { QStringLiteral("Emeritus Archmage"), { 0, 0, 0, 3, 1, 0 } }
    };
    return list;
}

struct Approach
{
    QString label;
    Ability ability;
    QString pass;
    QString fail;
    int damage;
    QString disadvantage; // empty: a fail leaves no lasting mark
};

struct Encounter
{
    QString dmText;
    QList<Approach> approaches;
};

const QString SOAKED = QStringLiteral("Soaked to the bone: disadvantage on this roll.");
const QString RINGING = QStringLiteral("The mast is still ringing: disadvantage on this roll.");
const QString NUMB = QStringLiteral("Numb fingers: disadvantage on this roll.");
const QString STUNG = QStringLiteral("Still swelling from the stings: disadvantage on this roll.");

const QList<Encounter>& encounterPool()
{
    static const QList<Encounter> pool {
        { QStringLiteral("Reviewer 2 leaps out from behind the flux tower, waving a fourteen-page report!"),
          {
            { QStringLiteral("Rebut point by point"), Int,
              QStringLiteral("Your rebuttal is so thorough that Reviewer 2 accepts, then asks to co-author."),
              QStringLiteral("Reviewer 2 demands a full reprocessing with planar fit instead of double rotation."), 3, {} },
            { QStringLiteral("Charm the editor"), Cha,
              QStringLiteral("The editor overrules Reviewer 2 over coffee. Accepted with minor revisions."),
              QStringLiteral("The editor sides with Reviewer 2. Major revisions, and a third reviewer."), 2, {} },
            { QStringLiteral("Resubmit elsewhere at speed"), Dex,
              QStringLiteral("You're out of the door before the report hits the table. The new journal loves it."),
              QStringLiteral("You trip over a guy wire. Reviewer 2 also reviews for the new journal."), 3, {} }
          } },
        { QStringLiteral("Something about the sonic anemometer feels… off. A spider has moved in between the transducers."),
          {
            { QStringLiteral("Evict it gently"), Dex,
              QStringLiteral("You coax the spider into a cup. Your spikes vanish."),
              QStringLiteral("The spider bites back and scuttles deeper into the sonic path."), 2, {} },
            { QStringLiteral("Out-stare it"), Wis,
              QStringLiteral("The spider blinks first, with all eight eyes, and leaves."),
              QStringLiteral("You lose the staring contest. The spider becomes your dominant turbulent eddy."), 2, {} },
            { QStringLiteral("Whack the mast"), Str,
              QStringLiteral("One firm whack and the spider falls out. So do a few bolts, but never mind."),
              QStringLiteral("The spider stays put and the whole mast rings like a bell."), 1, RINGING }
          } },
        { QStringLiteral("It is 3 a.m., −20 °C, and the IRGA heater has failed. The window is frosting over."),
          {
            { QStringLiteral("Endure the cold"), Con,
              QStringLiteral("You wipe the frost off the optics until dawn. The nighttime fluxes are saved. Mostly."),
              QStringLiteral("You lose the feeling in your fingers, and the whole night gets flagged 2."), 4, NUMB },
            { QStringLiteral("Rig a heater from the logger"), Int,
              QStringLiteral("The logger's power supply makes a fine heater. The signal strength recovers."),
              QStringLiteral("The logger reboots and forgets the last six hours."), 2, {} },
            { QStringLiteral("Phone the technician"), Cha,
              QStringLiteral("The technician talks you through it, half asleep. It works."),
              QStringLiteral("The technician's voicemail is full."), 2, {} }
          } },
        { QStringLiteral("A herd of curious cows approaches your guy wires, chewing thoughtfully."),
          {
            { QStringLiteral("Calm them (Animal Handling)"), Wis,
              QStringLiteral("The cows wander downwind, out of your footprint. Your methane fluxes thank you."),
              QStringLiteral("The cows settle right inside your footprint. Congratulations on the record CH₄ emissions."), 2, {} },
            { QStringLiteral("Build a fence"), Str,
              QStringLiteral("A fence goes up in record time. The cows look offended."),
              QStringLiteral("The fence falls over. The cows use it as a scratching post."), 3, {} },
            { QStringLiteral("Negotiate with the farmer"), Cha,
              QStringLiteral("The farmer moves the herd in exchange for a tour of the flux tower."),
              QStringLiteral("The farmer asks who gave you permission to put a tower in their field."), 2, {} }
          } },
        { QStringLiteral("The data logger's screen flickers: \"Answer my riddle, or lose your files.\""),
          {
            { QStringLiteral("Answer the riddle"), Int,
              QStringLiteral("'What has a mean of zero but is never nothing?' 'w′.' The logger opens."),
              QStringLiteral("You answer 'u*'. The logger formats its SD card."), 3, {} },
            { QStringLiteral("Reboot it firmly"), Str,
              QStringLiteral("A firm press of the power button. The files come back, slightly out of order."),
              QStringLiteral("You hold the button too long. Factory reset."), 3, {} },
            { QStringLiteral("Guess the password"), Wis,
              QStringLiteral("It was 'password'. It's always 'password'."),
              QStringLiteral("Three wrong guesses. The logger locks you out until next field season."), 2, {} }
          } },
        { QStringLiteral("The Lich of Gap-Filling rises from the missing data. \"Infinite data,\" it whispers, \"for your soul.\""),
          {
            { QStringLiteral("Refuse the bargain"), Wis,
              QStringLiteral("You refuse. Your gaps stay honest."),
              QStringLiteral("You accept before you can stop yourself. Your annual budget is now 100% modelled."), 3, {} },
            { QStringLiteral("Counter with MDS"), Int,
              QStringLiteral("Marginal distribution sampling! The Lich is filled in and fades away."),
              QStringLiteral("Your look-up table has gaps of its own. The Lich laughs."), 3, {} },
            { QStringLiteral("Flee to the field"), Dex,
              QStringLiteral("You escape to the tower, where the data are real and cold."),
              QStringLiteral("You flee straight into a nighttime gap. It is very dark in there."), 2, {} }
          } },
        { QStringLiteral("A thunderstorm rolls in while you are halfway up the tower. Hail hammers the sonic, and at the edge of the clearing the first trees start to fall."),
          {
            { QStringLiteral("Climb down fast"), Dex,
              QStringLiteral("You're on the ground before the first lightning strike, dodging hailstones the size of IRGA windows."),
              QStringLiteral("You slip on an icy rung and slide the rest of the way down, into a fresh layer of hail."), 3, SOAKED },
            { QStringLiteral("Wait it out"), Con,
              QStringLiteral("You cling to the mast until it passes. The sonic records the gust of the year, and a falling spruce misses the tower by a metre."),
              QStringLiteral("You wait. And wait. The storm doesn't, and a falling tree takes out your solar panel."), 3, SOAKED },
            { QStringLiteral("Reason with the storm"), Cha,
              QStringLiteral("Flattered, the storm moves on to the neighbouring site."),
              QStringLiteral("The storm is not interested in your data management plan."), 4, {} }
          } },
        { QStringLiteral("The funding agency's envoy arrives and eyes your energy balance with suspicion."),
          {
            { QStringLiteral("Present the closure plot"), Int,
              QStringLiteral("Your plot has error bars so wide that closure falls inside them. The envoy is satisfied."),
              QStringLiteral("The envoy notices the slope is 0.7 and asks where the rest went."), 2, {} },
            { QStringLiteral("Promise it'll close"), Cha,
              QStringLiteral("'It'll close this time, I promise.' Somehow it works. Funding secured."),
              QStringLiteral("Your energy balance gap is 20%. So is your budget cut."), 3, {} },
            { QStringLiteral("Hide the residual"), Dex,
              QStringLiteral("The residual stays under the table. The envoy nods approvingly."),
              QStringLiteral("The residual falls out of your folder in front of everyone."), 3, {} }
          } },
        { QStringLiteral("A pair of kestrels has built a nest on top of your sonic anemometer."),
          {
            { QStringLiteral("Relocate the nest"), Dex,
              QStringLiteral("You move the nest to a lovely new pole. The kestrels approve, eventually."),
              QStringLiteral("The kestrels do not approve. Neither does your face."), 3, {} },
            { QStringLiteral("Consult the bird people"), Cha,
              QStringLiteral("The ornithologist says it's fine and hands you a nest box and a smile."),
              QStringLiteral("The ornithologist says you'll need a permit. It takes four months."), 2, {} },
            { QStringLiteral("Wait for them to fledge"), Wis,
              QStringLiteral("You wait. The chicks fledge. The sonic is covered in droppings, but free."),
              QStringLiteral("Six weeks of data are now mostly wingbeats."), 2, {} }
          } },
        { QStringLiteral("Heavy rain has turned your footprint into a lake. Your latent heat flux is enthusiastic."),
          {
            { QStringLiteral("Wade out to the tower"), Con,
              QStringLiteral("You wade through knee-deep water and keep the logger dry. Heroic, and damp."),
              QStringLiteral("The water was deeper than it looked. So was the mud."), 3, SOAKED },
            { QStringLiteral("Reclassify the site as a wetland"), Int,
              QStringLiteral("Brilliant: a new paper on flood-pulse fluxes."),
              QStringLiteral("The reviewers ask why a wetland is labelled \"cornfield\"."), 2, {} },
            { QStringLiteral("Build a boardwalk"), Str,
              QStringLiteral("A sturdy boardwalk made of pallets. The ducks use it too."),
              QStringLiteral("The boardwalk floats away. It's in the next field now."), 3, {} }
          } },
        { QStringLiteral("The power has been off since Tuesday. The logger is running on hope."),
          {
            { QStringLiteral("Swap in the spare battery"), Str,
              QStringLiteral("Clunk. The logger springs back to life."),
              QStringLiteral("The spare battery was also flat. It always is."), 2, {} },
            { QStringLiteral("Rig the solar panel"), Int,
              QStringLiteral("You angle the panel perfectly. Watts flow. Data flow."),
              QStringLiteral("It's cloudy. It's always cloudy when the power's out."), 2, {} },
            { QStringLiteral("Call the power company"), Cha,
              QStringLiteral("They come the same day. A genuine miracle."),
              QStringLiteral("You're number 137 in the queue."), 2, {} }
          } },
        { QStringLiteral("The gas analyser announces a mandatory firmware update. It will take \"a few minutes\"."),
          {
            { QStringLiteral("Let it install"), Wis,
              QStringLiteral("Twenty minutes later it reboots with every setting intact. Suspicious, but fine."),
              QStringLiteral("It reboots into factory settings, in Norwegian."), 3, {} },
            { QStringLiteral("Read the release notes"), Int,
              QStringLiteral("You spot the bug that resets the calibration and skip that version."),
              QStringLiteral("The release notes say \"minor improvements\". They lie."), 2, {} },
            { QStringLiteral("Unplug it and pretend it never asked"), Dex,
              QStringLiteral("It forgets it ever asked. Bliss."),
              QStringLiteral("It remembers. Now it asks every five minutes."), 2, {} }
          } },
        { QStringLiteral("The field laptop has locked you out. The password was on a sticky note, and the rain took it."),
          {
            { QStringLiteral("Try the usual suspects"), Wis,
              QStringLiteral("It was the site name followed by 123. Of course it was."),
              QStringLiteral("Three attempts. Locked for an hour."), 2, {} },
            { QStringLiteral("Phone IT"), Cha,
              QStringLiteral("IT resets it remotely, with only mild judgement."),
              QStringLiteral("IT asks you to bring the laptop in. It's 200 km away."), 3, {} },
            { QStringLiteral("Boot from a USB stick"), Int,
              QStringLiteral("You're in. You feel like a hacker in a film about flux towers."),
              QStringLiteral("The laptop now boots into a very old Linux and nothing else."), 3, {} }
          } },
        { QStringLiteral("Freezing rain has coated your sonic anemometer in a thick layer of ice."),
          {
            { QStringLiteral("Chip it off carefully"), Dex,
              QStringLiteral("Delicate work. The transducers are free and unharmed."),
              QStringLiteral("You chip off the ice, and a transducer cover with it."), 3, {} },
            { QStringLiteral("Wait for the sun"), Wis,
              QStringLiteral("Noon arrives, the ice slides off, and the sonic carries on as if nothing happened."),
              QStringLiteral("The sun never comes. Three days of −9999."), 2, {} },
            { QStringLiteral("Warm it with your hands"), Con,
              QStringLiteral("Your hands are numb, but the sonic is running."),
              QStringLiteral("Your hands freeze to the mast. Briefly, but memorably."), 3, NUMB }
          } },
        { QStringLiteral("A ghost haunts your data logger. Every night it moves the clock forward by three seconds."),
          {
            { QStringLiteral("Sync it with GPS"), Int,
              QStringLiteral("The GPS pins the ghost to UTC. It fades, defeated by accuracy."),
              QStringLiteral("No satellite fix under the canopy. The ghost laughs."), 2, {} },
            { QStringLiteral("Exorcise it with NTP"), Wis,
              QStringLiteral("The network time server drives it out. Time moves correctly once more."),
              QStringLiteral("The time server is down. The ghost is up."), 2, {} },
            { QStringLiteral("Make friends with it"), Cha,
              QStringLiteral("You and the ghost agree on UTC+0. It's quite nice, really."),
              QStringLiteral("The ghost insists on daylight saving time. Chaos follows."), 2, {} }
          } },
        { QStringLiteral("A raccoon has moved into the instrument hut and is chewing the cables."),
          {
            { QStringLiteral("Shoo it out"), Str,
              QStringLiteral("The raccoon leaves, taking only a cable tie as a souvenir."),
              QStringLiteral("The raccoon shoos you out instead."), 3, {} },
            { QStringLiteral("Bribe it with snacks"), Cha,
              QStringLiteral("The raccoon accepts your biscuits and swears loyalty to the tower."),
              QStringLiteral("The raccoon eats the biscuits and stays."), 2, {} },
            { QStringLiteral("Seal the gaps in the hut"), Int,
              QStringLiteral("Not one raccoon-sized gap remains. The cables are safe."),
              QStringLiteral("You sealed the raccoon in. With the cables."), 3, {} }
          } },
        { QStringLiteral("The pump of your closed-path analyser has stopped. The flow reads zero."),
          {
            { QStringLiteral("Hit it firmly"), Str,
              QStringLiteral("The classic repair. It whirrs back into life."),
              QStringLiteral("It's dented now, and still not pumping."), 2, {} },
            { QStringLiteral("Replace the diaphragm"), Dex,
              QStringLiteral("Tiny screws, cold hands, perfect result."),
              QStringLiteral("One tiny screw is now somewhere in the grass for ever."), 3, NUMB },
            { QStringLiteral("Diagnose it from the flow data"), Int,
              QStringLiteral("It was a clogged filter, not the pump. Easy fix."),
              QStringLiteral("The data say the pump died at 03:12, which doesn't help at all."), 2, {} }
          } },
        { QStringLiteral("The conference abstract deadline is in two hours, and you have no results."),
          {
            { QStringLiteral("Write it anyway"), Cha,
              QStringLiteral("\"Preliminary results suggest…\" Accepted for an oral talk."),
              QStringLiteral("Rejected. The reviewers noticed there were no results."), 2, {} },
            { QStringLiteral("Process the data at speed"), Int,
              QStringLiteral("One heroic hour later you have a figure, and it even makes sense."),
              QStringLiteral("The script crashes at 98%. It always crashes at 98%."), 3, {} },
            { QStringLiteral("Ask for an extension"), Wis,
              QStringLiteral("The organisers extend the deadline by a week. Everybody asked."),
              QStringLiteral("No extensions. There are never extensions."), 2, {} }
          } },
        { QStringLiteral("The institute director is coming to see the tower in ten minutes."),
          {
            { QStringLiteral("Tidy the hut"), Dex,
              QStringLiteral("The hut looks almost professional. The director is impressed."),
              QStringLiteral("The director opens the one cupboard you didn't tidy."), 2, {} },
            { QStringLiteral("Prepare an impressive plot"), Int,
              QStringLiteral("A diurnal cycle with error bars. The director nods sagely."),
              QStringLiteral("The plot shows the energy balance. The director asks why it doesn't close."), 2, {} },
            { QStringLiteral("Climb the tower to look busy"), Str,
              QStringLiteral("You wave from the top. The director takes a photo for the website."),
              QStringLiteral("You get stuck halfway. The director takes a photo anyway."), 3, {} }
          } },
        { QStringLiteral("Wasps have built a nest inside the logger enclosure."),
          {
            { QStringLiteral("Remove it at dusk"), Dex,
              QStringLiteral("Silent, swift, unstung. The wasps never knew."),
              QStringLiteral("The wasps knew."), 4, STUNG },
            { QStringLiteral("Call pest control"), Cha,
              QStringLiteral("They arrive promptly and sort it out. You buy them a coffee."),
              QStringLiteral("They can't come until next month."), 2, {} },
            { QStringLiteral("Endure the stings"), Con,
              QStringLiteral("You swap the SD card through the pain. Legendary."),
              QStringLiteral("You drop the SD card into the nest."), 4, STUNG }
          } },
        { QStringLiteral("The farmer has mown the field, and the soil sensor cables with it."),
          {
            { QStringLiteral("Splice the cables"), Dex,
              QStringLiteral("Neat splices, all colour-coded. Beautiful."),
              QStringLiteral("Red to black. Something starts smoking."), 3, {} },
            { QStringLiteral("Negotiate a buffer strip"), Cha,
              QStringLiteral("The farmer agrees to mow around the tower. For ever, hopefully."),
              QStringLiteral("The farmer mows a smiley face instead. Through the cables."), 2, {} },
            { QStringLiteral("Bury the cables deeper"), Str,
              QStringLiteral("A proper trench. The mower will never find them."),
              QStringLiteral("You hit a water pipe."), 3, SOAKED }
          } },
        { QStringLiteral("The inlet tube is full of condensation. Your water vapour signal looks like a hillside."),
          {
            { QStringLiteral("Heat the tube"), Int,
              QStringLiteral("The heating cable does its job. The signal smooths out."),
              QStringLiteral("The heating cable was never connected."), 2, {} },
            { QStringLiteral("Blow it out"), Con,
              QStringLiteral("One mighty breath and the tube is dry."),
              QStringLiteral("You inhale a mouthful of tube water."), 2, {} },
            { QStringLiteral("Accept it and flag the data"), Wis,
              QStringLiteral("You flag it honestly. The reviewers respect that."),
              QStringLiteral("You flagged so much that nothing is left."), 2, {} }
          } },
        { QStringLiteral("Lightning has struck the tower. Everything smells faintly of toast."),
          {
            { QStringLiteral("Check the surge protection"), Int,
              QStringLiteral("The surge protector took the hit. Everything else survived."),
              QStringLiteral("There was no surge protection. There is now no logger."), 4, {} },
            { QStringLiteral("Reset everything"), Dex,
              QStringLiteral("One by one the instruments come back. Even the fussy one."),
              QStringLiteral("The sonic now reads the wind backwards."), 3, {} },
            { QStringLiteral("Stay calm"), Wis,
              QStringLiteral("You breathe, assess and fix. A professional."),
              QStringLiteral("You panic and unplug the only thing that was still working."), 2, {} }
          } },
        { QStringLiteral("The mobile modem has stopped sending data. The server hasn't heard from the tower in a week."),
          {
            { QStringLiteral("Reboot the modem"), Str,
              QStringLiteral("A firm restart. Seven days of data flood in."),
              QStringLiteral("The modem reboots and forgets its SIM card."), 2, {} },
            { QStringLiteral("Top up the SIM"), Cha,
              QStringLiteral("The data plan was empty. Now it's full, and so is your inbox."),
              QStringLiteral("The phone company needs a form. In triplicate."), 2, {} },
            { QStringLiteral("Drive out with a USB stick"), Con,
              QStringLiteral("Four hours of driving, one stick, all the data."),
              QStringLiteral("The USB stick is empty when you get back."), 3, {} }
          } },
        { QStringLiteral("A mouse is living in the sonic anemometer's cable conduit."),
          {
            { QStringLiteral("Lure it out with cheese"), Wis,
              QStringLiteral("The mouse comes out for the cheese. Everyone's happy."),
              QStringLiteral("The mouse eats the cheese from inside the conduit."), 2, {} },
            { QStringLiteral("Replace the cable"), Dex,
              QStringLiteral("New cable, no mouse. Clean."),
              QStringLiteral("The mouse moves into the new cable."), 2, {} },
            { QStringLiteral("Recruit the farm cat"), Cha,
              QStringLiteral("The farm cat takes the job very seriously."),
              QStringLiteral("The cat sleeps on the solar panel instead."), 2, {} }
          } },
        { QStringLiteral("The span gas cylinder is empty, and the next delivery is three weeks away."),
          {
            { QStringLiteral("Borrow from the neighbouring site"), Cha,
              QStringLiteral("The neighbours lend you a cylinder and a biscuit."),
              QStringLiteral("The neighbours want co-authorship in return."), 2, {} },
            { QStringLiteral("Skip this calibration"), Wis,
              QStringLiteral("The drift is negligible. You got lucky."),
              QStringLiteral("The drift is not negligible. Nothing is negligible."), 3, {} },
            { QStringLiteral("Calibrate against scrubbed air"), Int,
              QStringLiteral("A clever zero with scrubbed air. Close enough for science."),
              QStringLiteral("The air was not what you thought it was."), 2, {} }
          } },
        { QStringLiteral("Your annual carbon budget says the forest is a massive source. It really shouldn't be."),
          {
            { QStringLiteral("Check the gap-filling"), Int,
              QStringLiteral("A three-month gap had been filled with winter data. Fixed."),
              QStringLiteral("The gap-filling is fine. The forest is just being difficult."), 2, {} },
            { QStringLiteral("Check the sign convention"), Wis,
              QStringLiteral("It was upside down. The forest is a sink after all."),
              QStringLiteral("The signs are right. That's the worrying part."), 2, {} },
            { QStringLiteral("Blame the footprint"), Cha,
              QStringLiteral("You argue convincingly that the flux came from the car park."),
              QStringLiteral("Nobody believes the car-park explanation."), 2, {} }
          } },
        { QStringLiteral("The wind is gusting at 25 m s⁻¹, and the sonic needs levelling. Now."),
          {
            { QStringLiteral("Climb anyway"), Str,
              QStringLiteral("You level the sonic in a gale. Songs will be sung."),
              QStringLiteral("The gust wins. You cling on for twenty minutes."), 4, {} },
            { QStringLiteral("Level it with a very long pole"), Dex,
              QStringLiteral("Improbably, it works."),
              QStringLiteral("The pole bends. The sonic doesn't."), 2, {} },
            { QStringLiteral("Wait for a calm spell"), Wis,
              QStringLiteral("The wind drops at dusk. An easy job."),
              QStringLiteral("It never calms down. It's that kind of site."), 2, {} }
          } },
        { QStringLiteral("The new logger writes its files in a format nobody has ever seen."),
          {
            { QStringLiteral("Reverse-engineer it"), Int,
              QStringLiteral("A hex editor, three coffees, one triumph."),
              QStringLiteral("The bytes mean nothing. The bytes will always mean nothing."), 3, {} },
            { QStringLiteral("Email the manufacturer"), Cha,
              QStringLiteral("They send a converter and an apology."),
              QStringLiteral("They reply with a link to a page that no longer exists."), 2, {} },
            { QStringLiteral("Find someone who's done it before"), Wis,
              QStringLiteral("A forum post from 2011 has the answer."),
              QStringLiteral("The forum post from 2011 says \"never mind, solved it\"."), 2, {} }
          } },
        { QStringLiteral("A tractor is reversing towards your guy wires."),
          {
            { QStringLiteral("Wave your arms"), Dex,
              QStringLiteral("The driver sees you and stops a metre short."),
              QStringLiteral("The driver waves back cheerfully and keeps reversing."), 3, {} },
            { QStringLiteral("Shout very loudly"), Cha,
              QStringLiteral("The tractor stops. So do the cows."),
              QStringLiteral("The engine is louder than you are."), 3, {} },
            { QStringLiteral("Stand between the tractor and the tower"), Con,
              QStringLiteral("The driver stops. You are a hero of the guy wires."),
              QStringLiteral("You leap aside at the last second. The guy wire doesn't."), 4, {} }
          } }
    };
    return pool;
}

// usable items: armed before a roll, each gives advantage once
struct Item
{
    QString name;
    QString useLine;
    QString tip;
};

const QList<Item>& itemTable()
{
    static const QList<Item> list {
        { QStringLiteral("☕ Flask of Coffee"),
          QStringLiteral("You down the coffee. Your hands stop shaking, mostly."),
          QStringLiteral("Hot, strong and slightly metallic from the thermos.") },
        { QStringLiteral("🔌 Spare Fuse"),
          QStringLiteral("You swap in the spare fuse. Something, somewhere, starts working."),
          QStringLiteral("The one fuse that actually fits.") },
        { QStringLiteral("🧽 Lens Wipe for the IRGA Window"),
          QStringLiteral("You polish the window until the signal strength reads 100%."),
          QStringLiteral("Removes dust, pollen and three months of neglect.") },
        { QStringLiteral("🦆 Duct Tape of Holding"),
          QStringLiteral("You duct-tape the problem. It holds, as duct tape always does."),
          QStringLiteral("Fixes everything, temporarily, for ever.") },
        { QStringLiteral("👒 Sombrero"),
          QStringLiteral("You don the sombrero. The midday sun can't touch you now."),
          QStringLiteral("For the midday shift up the tower.") },
        { QStringLiteral("🔋 Fully Charged Battery (Legendary)"),
          QStringLiteral("A fully charged battery. The logger weeps with gratitude."),
          QStringLiteral("Legend says it charged on the first try.") },
        { QStringLiteral("🪛 Screwdriver That Actually Fits"),
          QStringLiteral("The screwdriver fits. First time. You feel invincible."),
          QStringLiteral("Torx, of course. It's always Torx.") },
        { QStringLiteral("💾 Backup Floppy Disk"),
          QStringLiteral("You insert the floppy. It holds 1.44 MB of pure confidence."),
          QStringLiteral("Contains a backup of the 1998 calibration. Somehow still useful.") },
        { QStringLiteral("🗝️ Key to the Instrument Hut"),
          QStringLiteral("You unlock the hut. For once, the key was where it should be."),
          QStringLiteral("Labelled \"hut (probably)\".") },
        { QStringLiteral("🍫 Emergency Chocolate"),
          QStringLiteral("You eat the emergency chocolate. This is an emergency."),
          QStringLiteral("Slightly melted, deeply motivating.") },
        { QStringLiteral("🧊 Fresh Desiccant Pack"),
          QStringLiteral("You swap the desiccant. The analyser dries out and cheers up."),
          QStringLiteral("Orange means fresh. Green means too late.") },
        { QStringLiteral("📶 Five Bars of Mobile Signal"),
          QStringLiteral("Five bars! You phone for help before they vanish."),
          QStringLiteral("Only works standing on one leg at the top of the tower.") },
        { QStringLiteral("🗺️ Map with the Guy Wires Marked"),
          QStringLiteral("You follow the map and don't trip over a single guy wire."),
          QStringLiteral("Drawn by a technician who clearly tripped over all of them.") },
        { QStringLiteral("🔧 Wrench of Levelling"),
          QStringLiteral("Two turns of the wrench and everything is level."),
          QStringLiteral("+2 to planar-fit coefficients.") },
        { QStringLiteral("🧭 Compass That Knows Where North Is"),
          QStringLiteral("The compass points north. Actual north. The sonic lines up perfectly."),
          QStringLiteral("Somehow unaffected by the steel tower.") },
        { QStringLiteral("🥾 Genuinely Waterproof Boots"),
          QStringLiteral("You stride through the bog in dry socks. A rare triumph."),
          QStringLiteral("Waterproof in the shop and, amazingly, in the field.") },
        { QStringLiteral("🔦 Head Torch with Fresh Batteries"),
          QStringLiteral("The head torch lights up the night shift. And the spider."),
          QStringLiteral("Batteries included, and actually charged.") },
        { QStringLiteral("🧯 Nearly Full Span Gas Cylinder"),
          QStringLiteral("You calibrate with gas to spare. The analyser behaves."),
          QStringLiteral("Nearly full, according to a gauge you choose to trust.") },
        { QStringLiteral("🧪 Fresh Chemical Scrubber"),
          QStringLiteral("The new scrubber gives you a perfect zero. Mmm, zero."),
          QStringLiteral("Soda lime, freshly packed.") },
        { QStringLiteral("📓 Notebook with the Password in It"),
          QStringLiteral("You flip to page 37. There's the password."),
          QStringLiteral("Also contains the wifi code and a shopping list.") },
        { QStringLiteral("🍪 Biscuits for the Farmer"),
          QStringLiteral("You hand over the biscuits. The farmer is now your greatest ally."),
          QStringLiteral("Chocolate digestives: the currency of fieldwork.") },
        { QStringLiteral("📣 Cow-Repelling Whistle"),
          QStringLiteral("One blast and the cows retreat out of the footprint."),
          QStringLiteral("Tested on cows. Mildly effective on reviewers.") },
        { QStringLiteral("⚡ Surge Protector of Warding"),
          QStringLiteral("The surge protector hums protectively. Nothing will fry today."),
          QStringLiteral("Absorbs lightning, mains spikes and bad luck.") },
        { QStringLiteral("🛰️ GPS Time Sync"),
          QStringLiteral("The GPS locks on. Your clocks agree to the millisecond."),
          QStringLiteral("Makes time lags behave.") },
        { QStringLiteral("🧵 Spare Sonic Cable"),
          QStringLiteral("A fresh cable. The sonic's signal is crystal clear."),
          QStringLiteral("The right length, with the right connectors, for once.") },
        { QStringLiteral("🧤 Gloves with Working Fingertips"),
          QStringLiteral("Warm hands, nimble fingers. Tiny screws hold no fear."),
          QStringLiteral("Touchscreen-friendly, frost-resistant, slightly smelly.") },
        { QStringLiteral("🕷️ Spider Eviction Notice"),
          QStringLiteral("You serve the notice. The spider packs up its web and leaves."),
          QStringLiteral("Legally binding, as far as all eight of its eyes are concerned.") },
        { QStringLiteral("🌡️ Thermocouple Wire, Unkinked"),
          QStringLiteral("A clean, unkinked thermocouple. The temperatures make sense again."),
          QStringLiteral("Rare in the wild.") },
        { QStringLiteral("📋 Calibration Certificate (Current)"),
          QStringLiteral("You wave the current calibration certificate. Reviewer 2 backs down."),
          QStringLiteral("Actually up to date. A collector's item.") },
        { QStringLiteral("🪜 Ladder Rung That Doesn't Wobble"),
          QStringLiteral("You stand on the one rung that doesn't wobble. Confidence +10."),
          QStringLiteral("Carry it with you. All the others wobble.") }
    };
    return list;
}

// loot that just looks good in the summary
const QStringList& trophyLoot()
{
    static const QStringList list {
        QStringLiteral("🏆 +1 Sonic Anemometer of Precision"),
        QStringLiteral("🏆 Bag of Holding (raw data only)"),
        QStringLiteral("🏆 Scroll of Planar Fit"),
        QStringLiteral("🏆 Amulet of Closed Energy Balance"),
        QStringLiteral("🏆 Potion of Stationarity"),
        QStringLiteral("🏆 Cursed USB Stick"),
        QStringLiteral("🏆 Ring of Perfect Time Lags"),
        QStringLiteral("🏆 Cloak of Low Turbulence"),
        QStringLiteral("🏆 Tome of Spectral Corrections"),
        QStringLiteral("🏆 Wand of Gap-Filling"),
        QStringLiteral("🏆 Boots of the Night Shift"),
        QStringLiteral("🏆 Crown of the Flux Network"),
        QStringLiteral("🏆 Shield of Quality Flags"),
        QStringLiteral("🏆 Orb of the Footprint"),
        QStringLiteral("🏆 Gauntlets of the Guy Wire"),
        QStringLiteral("🏆 Helm of the High Frequencies"),
        QStringLiteral("🏆 Lantern of the Ogive"),
        QStringLiteral("🏆 Mirror of the Mean Streamline"),
        QStringLiteral("🏆 Staff of the Storage Term"),
        QStringLiteral("🏆 Chalice of Carbon Uptake"),
        QStringLiteral("🏆 Map of the Upwind Lands"),
        QStringLiteral("🏆 Compass of True North"),
        QStringLiteral("🏆 Hourglass of Averaging Periods"),
        QStringLiteral("🏆 Horn of the Herd (Cows Obey)"),
        QStringLiteral("🏆 Golden Data Logger"),
        QStringLiteral("🏆 Enchanted Desiccant"),
        QStringLiteral("🏆 Tiara of Eternal Calibration"),
        QStringLiteral("🏆 Belt of Energy Closure +1"),
        QStringLiteral("🏆 Medal of Accepted Manuscripts"),
        QStringLiteral("🏆 Deed to a Perfectly Flat Site")
    };
    return list;
}

const QStringList& defeatedTitles()
{
    static const QStringList list {
        QStringLiteral("Slain by Reviewer 2"),
        QStringLiteral("Flagged 2, Permanently"),
        QStringLiteral("Lost in the Footprint"),
        QStringLiteral("Consumed by the Gap"),
        QStringLiteral("Struck Down by a Firmware Update"),
        QStringLiteral("Fallen at the Guy Wire"),
        QStringLiteral("Victim of the Night Shift"),
        QStringLiteral("Buried in the Residual"),
        QStringLiteral("Eaten by the Raccoon"),
        QStringLiteral("Swept Away by the Flood"),
        QStringLiteral("Frozen to the Sonic"),
        QStringLiteral("Stung Beyond Measure"),
        QStringLiteral("Outvoted by the Cows"),
        QStringLiteral("Undone by Clock Drift"),
        QStringLiteral("Crushed by the Deadline"),
        QStringLiteral("Blown Off the Tower"),
        QStringLiteral("Overwhelmed by Spikes"),
        QStringLiteral("Taken by the Lich of Gap-Filling"),
        QStringLiteral("Defeated by the Data Format"),
        QStringLiteral("Toasted by Lightning"),
        QStringLiteral("Rejected Without Review"),
        QStringLiteral("Stranded Without Signal"),
        QStringLiteral("Drowned in Condensation"),
        QStringLiteral("Buried in Paperwork"),
        QStringLiteral("Reversed Over by a Tractor"),
        QStringLiteral("Locked Out of the Laptop"),
        QStringLiteral("Mourned by the Technician"),
        QStringLiteral("Written Off in the Annual Report"),
        QStringLiteral("Deleted by Quality Control"),
        QStringLiteral("Out of Funding")
    };
    return list;
}

const QStringList& flawlessTitles()
{
    static const QStringList list {
        QStringLiteral("Hero of the Half-Hour"),
        QStringLiteral("Keeper of the Closed Balance"),
        QStringLiteral("Legend of the Flux Tower"),
        QStringLiteral("Champion of the Footprint"),
        QStringLiteral("Lord of the Logger"),
        QStringLiteral("Master of the Night Shift"),
        QStringLiteral("Slayer of Reviewer 2"),
        QStringLiteral("Guardian of the Guy Wires"),
        QStringLiteral("Sovereign of the Sonic"),
        QStringLiteral("Paragon of Calibration"),
        QStringLiteral("Tamer of Turbulence"),
        QStringLiteral("Hero of the Annual Budget"),
        QStringLiteral("Knight of the Planar Fit"),
        QStringLiteral("Keeper of the Perfect Dataset"),
        QStringLiteral("Defender of the Diurnal Cycle"),
        QStringLiteral("Archmage of the Ogive"),
        QStringLiteral("Saviour of the Instrument Hut"),
        QStringLiteral("Protector of the Carbon Sink"),
        QStringLiteral("Warden of the Wetland"),
        QStringLiteral("Ruler of the Raw Data"),
        QStringLiteral("Undefeated Field Technician"),
        QStringLiteral("Scourge of the Spiders"),
        QStringLiteral("Friend of the Farmer"),
        QStringLiteral("Lightning Rod Supreme"),
        QStringLiteral("Monarch of Micrometeorology"),
        QStringLiteral("Legend of the Logger Hut"),
        QStringLiteral("Wielder of the Duct Tape"),
        QStringLiteral("Master of Time Lags"),
        QStringLiteral("Bringer Back of All the Data"),
        QStringLiteral("Fieldwork Immortal")
    };
    return list;
}

const QStringList& survivedTitles()
{
    static const QStringList list {
        QStringLiteral("Survivor of the Night Shift"),
        QStringLiteral("Battle-Scarred Field Technician"),
        QStringLiteral("Veteran of the Muddy Footprint"),
        QStringLiteral("Slightly Singed Survivor"),
        QStringLiteral("Weathered Tower Climber"),
        QStringLiteral("Keeper of Most of the Data"),
        QStringLiteral("Returner with Stories"),
        QStringLiteral("Bruised but Calibrated"),
        QStringLiteral("Holder of Partial Closure"),
        QStringLiteral("Survivor of the Firmware Update"),
        QStringLiteral("Damp but Victorious"),
        QStringLiteral("Wiser Field Scientist"),
        QStringLiteral("Wearer of Many Scars"),
        QStringLiteral("Owner of Half a Dataset"),
        QStringLiteral("Guardian of the Remaining Gaps"),
        QStringLiteral("Grizzled Flux Veteran"),
        QStringLiteral("Mostly Intact Adventurer"),
        QStringLiteral("Hardy Hut Keeper"),
        QStringLiteral("Recipient of Minor Revisions"),
        QStringLiteral("Tired but Standing"),
        QStringLiteral("Defender of the Diurnal Median"),
        QStringLiteral("Proud Owner of Error Bars"),
        QStringLiteral("Muddy Hero"),
        QStringLiteral("Survivor of Reviewer 2 (Just)"),
        QStringLiteral("Keeper of the Spare Fuse"),
        QStringLiteral("Wanderer of the Guy Wires"),
        QStringLiteral("Brave Data Salvager"),
        QStringLiteral("Heroic Gap-Filler"),
        QStringLiteral("Honourable Mention in the Annual Report"),
        QStringLiteral("Lived to Process Another Day")
    };
    return list;
}

// placeholders: {n} encounter reached, {loot}, {hp}, and {who}/{place}/{tool}
const QStringList& defeatedEndings()
{
    static const QStringList list {
        QStringLiteral("You fall in encounter {n}. {who} will tell the tale at the next flux meeting."),
        QStringLiteral("Your adventure ends in {place}. Not even {tool} could have saved you."),
        QStringLiteral("You collapse in encounter {n}. {who} finishes the fieldwork, grumbling."),
        QStringLiteral("Defeated. Your data will be gap-filled, and so, in a sense, will you."),
        QStringLiteral("The quest ends in encounter {n}. {who} has kept your spare fuse."),
        QStringLiteral("You fall, clutching {loot}. {who} says you should have brought {tool}."),
        QStringLiteral("Your hit points hit zero in {place}. It'll be in the methods section."),
        QStringLiteral("{who} carries you back to the hut. The dataset stays behind."),
        QStringLiteral("You are defeated in encounter {n}. The tower stands. Barely."),
        QStringLiteral("The adventure ends. {who} files an incident report about {place}."),
        QStringLiteral("You fall in the line of duty. {tool} will be named after you."),
        QStringLiteral("Game over in encounter {n}. Your data will be marked \"provisional\", like you."),
        QStringLiteral("You fought well, but {place} fought better."),
        QStringLiteral("Defeated, with {loot} in your pack. {who} is sorting through it already."),
        QStringLiteral("Your journey ends in encounter {n}. The cows hold a small ceremony."),
        QStringLiteral("You collapse in the mud. {who} asks whether the logger is still running."),
        QStringLiteral("The quest is lost. {place} will need a new adventurer, and a new grant."),
        QStringLiteral("You fall in encounter {n}. {who} adds your name to the risk assessment."),
        QStringLiteral("Defeated. Your field notebook ends mid-sentence."),
        QStringLiteral("You are carried off the site. {who} is keeping your {loot} \"safe\"."),
        QStringLiteral("Encounter {n} was one too many. {place} claims another victim."),
        QStringLiteral("The adventure ends in defeat. Next time, pack {tool}."),
        QStringLiteral("You fall, and the tower sways sadly in the wind."),
        QStringLiteral("{who} will finish your thesis chapter. And take the credit."),
        QStringLiteral("Defeated in encounter {n}. The half-hourly data carry on without you."),
        QStringLiteral("You drop in {place}. A passing technician checks your quality flag. It's 2."),
        QStringLiteral("Your quest ends. {who} sends flowers and a request for the raw data."),
        QStringLiteral("You fall in encounter {n}. The instruments observe a minute's silence, then resume at 20 Hz."),
        QStringLiteral("Defeated. The logger records your last reading: −9999."),
        QStringLiteral("The adventure is over. {place} is quiet now, apart from the cows.")
    };
    return list;
}

const QStringList& flawlessEndings()
{
    static const QStringList list {
        QStringLiteral("A flawless quest. {who} is speechless, and {place} is safe."),
        QStringLiteral("You return with {loot} and {hp} HP to spare. Legend says {tool} was named after you."),
        QStringLiteral("Three encounters, three victories. {who} demands to know your secret."),
        QStringLiteral("Not a single failed roll. {place} will be taught in field courses."),
        QStringLiteral("Perfect. You walk back to the car with {loot} and a smug expression."),
        QStringLiteral("The quest is complete, flawlessly. {who} is rewriting the protocol to say \"do what they did\"."),
        QStringLiteral("A spotless campaign. Even the energy balance nearly closed."),
        QStringLiteral("You conquered everything. {who} wants you on every field trip from now on."),
        QStringLiteral("Flawless. The tower bows slightly as you leave. It might be the wind."),
        QStringLiteral("Victory in every encounter, plus {loot} and {hp} HP. Show-off."),
        QStringLiteral("A perfect run. {place} has never been so well looked after."),
        QStringLiteral("You return triumphant. {who} has already written the press release."),
        QStringLiteral("Every roll a success. The dice are considering early retirement."),
        QStringLiteral("Flawless. {tool} feels unnecessary now."),
        QStringLiteral("Complete victory. The farmer, the cows and {who} all applaud."),
        QStringLiteral("Three for three. The data are complete, calibrated and gleaming."),
        QStringLiteral("A flawless quest, with {loot} to prove it. {who} would like to borrow some."),
        QStringLiteral("Perfect. You've saved {place}, and the annual budget with it."),
        QStringLiteral("Undefeated. The flux network wants you as its patron saint."),
        QStringLiteral("A clean sweep. Reviewer 2 has nothing left to complain about."),
        QStringLiteral("You return with {hp} HP and not a scratch. Nobody at the institute believes you."),
        QStringLiteral("Flawless. The logger hut now has a small shrine in your honour."),
        QStringLiteral("Every encounter won. {who} is lost for words, which has never happened before."),
        QStringLiteral("A perfect campaign. Your field notebook reads like an epic."),
        QStringLiteral("You won everything, including the respect of the raccoon."),
        QStringLiteral("Victory, total and complete. {place} is spotless."),
        QStringLiteral("Flawless. The gap-filler has nothing to do."),
        QStringLiteral("All three encounters conquered. {tool} is in awe."),
        QStringLiteral("A perfect quest. You even brought {loot} home."),
        QStringLiteral("You are the stuff of fieldwork legend. {who} will be telling this story for years.")
    };
    return list;
}

const QStringList& survivedEndings()
{
    static const QStringList list {
        QStringLiteral("You limp back from the tower with {loot} and {hp} HP. {who} is mildly impressed."),
        QStringLiteral("The quest is over. {place} is safe for now, and {who} wants the data by Monday."),
        QStringLiteral("You survived, mostly. The data did, partly."),
        QStringLiteral("Back at the car with {hp} HP and {loot}. It could have been worse. It usually is."),
        QStringLiteral("You made it. {who} asks where the rest of the data went."),
        QStringLiteral("A hard-won survival. {place} is scarred but standing."),
        QStringLiteral("You return, damp and triumphant, with {loot}."),
        QStringLiteral("Survived. Your field notebook is now 40% mud."),
        QStringLiteral("You live to process another day. {who} has a list of revisions ready."),
        QStringLiteral("You made it out with {hp} HP. {tool} would have made it easier."),
        QStringLiteral("Not a perfect quest, but the logger is running and so are you."),
        QStringLiteral("You survived {place}. Nobody else wanted to go there."),
        QStringLiteral("Home at last with {loot}. {who} asks why it took so long."),
        QStringLiteral("A mixed campaign. The data are mostly fine. So are you."),
        QStringLiteral("You got through it. The cows watched the whole thing."),
        QStringLiteral("Battered but alive, with {hp} HP left. The tower waves goodbye."),
        QStringLiteral("You survived, and so did most of {place}."),
        QStringLiteral("Victory, of sorts. {who} calls it \"a learning experience\"."),
        QStringLiteral("You return with {loot}, three bruises and one good story."),
        QStringLiteral("Survived. The rest of the dataset will be gap-filled."),
        QStringLiteral("You made it back with {hp} HP. {who} has put the kettle on."),
        QStringLiteral("Not every roll went your way, but you're still standing."),
        QStringLiteral("A respectable survival. {place} will need some maintenance."),
        QStringLiteral("You survived the campaign. Your annual budget has a few gaps, but it exists."),
        QStringLiteral("Home with {loot}. You'll tell everyone it went perfectly."),
        QStringLiteral("You lived. {tool} was a big help. Mostly the idea of it."),
        QStringLiteral("Survived, with {hp} HP and a new respect for the weather."),
        QStringLiteral("The quest ends. {who} marks your data as \"good enough\"."),
        QStringLiteral("You made it, and so did {loot}. The rest is somewhere in the footprint."),
        QStringLiteral("A survivor's return. {place} remembers you fondly.")
    };
    return list;
}

// the Dungeon Master's greeting
const QStringList& welcomeLines()
{
    static const QStringList list {
        QStringLiteral("Welcome, adventurer. Choose your class."),
        QStringLiteral("Ah, a new adventurer approaches the flux tower. Choose your class."),
        QStringLiteral("The tower looms before you. Who will you be today?"),
        QStringLiteral("Greetings, traveller. The data will not collect themselves. Choose your class."),
        QStringLiteral("The logger hut creaks open. A new hero is needed. Choose wisely."),
        QStringLiteral("A new campaign begins. The guy wires are taut, the sonic is humming. Pick your class."),
        QStringLiteral("Welcome to the Flux Dungeon. Mind the cables. Choose your class."),
        QStringLiteral("Adventurer, the ecosystem calls. Which path will you take?"),
        QStringLiteral("The cows watch silently as you arrive. Choose your class."),
        QStringLiteral("Roll for initiative… once you've picked a class."),
        QStringLiteral("A mysterious technician hands you a clipboard. \"Class?\" they ask."),
        QStringLiteral("The wind whistles through the mast. It wants to know your class."),
        QStringLiteral("Welcome back, brave soul. Or welcome for the first time. Choose your class."),
        QStringLiteral("The raw data await. So does the paperwork. Choose your class."),
        QStringLiteral("Every great flux campaign starts with one choice. Make it."),
        QStringLiteral("The instrument hut smells of damp and adventure. Pick your class."),
        QStringLiteral("A new season of fieldwork dawns. Who will face it?"),
        QStringLiteral("The Dungeon Master cracks their knuckles. \"Choose your class, adventurer.\""),
        QStringLiteral("Three paths lie before you, each muddier than the last. Choose one."),
        QStringLiteral("The sonic anemometer blinks at you expectantly. Choose your class."),
        QStringLiteral("Welcome, adventurer. The half-hours are ticking. Choose quickly."),
        QStringLiteral("Legends are made at flux towers. Mostly legends about mud. Choose your class."),
        QStringLiteral("Your quest: survive three encounters and bring back the data. First, a class."),
        QStringLiteral("A hush falls over the footprint. A new adventurer has arrived."),
        QStringLiteral("The farmer eyes you suspiciously. \"Another one,\" they mutter. Choose your class."),
        QStringLiteral("Adventurer! The tower needs you. Or at least someone. Choose your class."),
        QStringLiteral("Grab your field kit. It's time to pick a class."),
        QStringLiteral("The dice are warm; the coffee is not. Choose your class."),
        QStringLiteral("Destiny, and a slightly wobbly ladder, await. Choose your class."),
        QStringLiteral("Welcome to the campaign. Please sign the risk assessment and choose your class.")
    };
    return list;
}

// the line under the title
const QStringList& partyLines()
{
    static const QStringList list {
        QStringLiteral("Your party: one sonic anemometer, one gas analyser, one sleep-deprived PhD student."),
        QStringLiteral("Your party: a data logger, a solar panel and a very optimistic postdoc."),
        QStringLiteral("Your party: one tower, three guy wires and a technician who has seen things."),
        QStringLiteral("Your party: two sonics, one pump and a thermos of lukewarm coffee."),
        QStringLiteral("Your party: a closed-path analyser, forty metres of tube and a lot of hope."),
        QStringLiteral("Your party: one Master's student, one clipboard, no spare fuses."),
        QStringLiteral("Your party: a net radiometer, a soil heat flux plate and an unresolved grudge."),
        QStringLiteral("Your party: a laptop on 3% battery and a field notebook full of mud."),
        QStringLiteral("Your party: one open-path analyser, one lens wipe and a strong wind."),
        QStringLiteral("Your party: a PI on the phone, a technician in the hut and you, up the tower."),
        QStringLiteral("Your party: a pickup truck, a toolbox and a mysterious bag of cable ties."),
        QStringLiteral("Your party: an intern, a ladder and a risk assessment nobody read."),
        QStringLiteral("Your party: a sonic, a spider and an uneasy truce."),
        QStringLiteral("Your party: one very old data logger and one very new firmware update."),
        QStringLiteral("Your party: a flux tower, a herd of cows and a farmer with opinions."),
        QStringLiteral("Your party: three researchers, one working pen and a lot of rain."),
        QStringLiteral("Your party: one tripod, one spirit level and one person who can use it."),
        QStringLiteral("Your party: a methane analyser, a wetland and a pair of wellies."),
        QStringLiteral("Your party: an eddy covariance system and the people it has broken."),
        QStringLiteral("Your party: one supervisor (remote), one student (present), one tower (swaying)."),
        QStringLiteral("Your party: a GPS clock, a laptop and a vague sense of time."),
        QStringLiteral("Your party: a four-wheel drive, a map and no mobile signal."),
        QStringLiteral("Your party: two sonic anemometers that disagree about everything."),
        QStringLiteral("Your party: one spare battery (flat) and another spare battery (also flat)."),
        QStringLiteral("Your party: a technician, a postdoc and the smell of burnt electronics."),
        QStringLiteral("Your party: a flask of coffee, a bag of biscuits and grim determination."),
        QStringLiteral("Your party: an infrared gas analyser and its high-maintenance moods."),
        QStringLiteral("Your party: a field team of three, and a field of maize taller than all of them."),
        QStringLiteral("Your party: a data manager, a spreadsheet and a lot of unsaved changes."),
        QStringLiteral("Your party: one flux tower, one thunderstorm forecast and poor decision-making.")
    };
    return list;
}

const int ENCOUNTERS_PER_CAMPAIGN = 3;
const int CLASSES_OFFERED = 3;
const int BASE_HP = 12;

// the difficulty is rolled afresh for every approach: never below 8, and
// biased towards the hard end (7 + the higher of two d12, so 8 to 19)
int rollDifficulty()
{
    const int a = QRandomGenerator::global()->bounded(1, 13);
    const int b = QRandomGenerator::global()->bounded(1, 13);
    return 7 + std::max(a, b);
}

int d(int sides)
{
    return QRandomGenerator::global()->bounded(1, sides + 1);
}

// 4d6, drop the lowest
int rollAbility()
{
    std::array<int, 4> dice { d(6), d(6), d(6), d(6) };
    std::sort(dice.begin(), dice.end());
    return dice[1] + dice[2] + dice[3];
}

QString signedNumber(int value)
{
    return value < 0 ? QStringLiteral("−%1").arg(-value) : QStringLiteral("+%1").arg(value);
}

QString counted(int count, const QString& one, const QString& many)
{
    if (count == 0)
        return QStringLiteral("no %1").arg(many);
    return count == 1 ? QStringLiteral("1 %1").arg(one) : QStringLiteral("%1 %2").arg(count).arg(many);
}

////////////////////////////////////////////////////////////////////////////////
// the d20

const int DICE_TICK_MS = 40;
const qreal TUMBLE_S = 1.0;
const QColor DIE_GREEN(118, 189, 29);
const QColor DIE_GOLD(222, 170, 20);
const QColor DIE_RED(215, 50, 40);
const QColor DIE_GREY(160, 160, 160);

QPointF hexVertex(qreal radius, int i)
{
    // pointy top: vertices at -90°, -30°, 30°, 90°, 150°, 210°
    const qreal angle = (-90.0 + 60.0 * i) * 3.14159265358979323846 / 180.0;
    return QPointF(radius * std::cos(angle), radius * std::sin(angle));
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
// D20Widget

D20Widget::D20Widget(QWidget *parent) :
    QWidget(parent),
    elapsed_(0.0),
    tumbling_(false),
    values_ { 0, 0 },
    shown_ { 0, 0 },
    keep_(0)
{
    setMinimumSize(240, 120);

    timer_ = new QTimer(this);
    timer_->setInterval(DICE_TICK_MS);
    connect(timer_, &QTimer::timeout, this, [this]()
    {
        elapsed_ += DICE_TICK_MS / 1000.0;
        if (elapsed_ < TUMBLE_S)
        {
            shown_ = { d(20), d(20) };
            update();
            return;
        }
        timer_->stop();
        tumbling_ = false;
        shown_ = values_;
        update();
        emit settled();
    });
}

void D20Widget::roll(int first, int second, int keep)
{
    values_ = { first, second };
    shown_ = { d(20), d(20) };
    keep_ = keep;
    elapsed_ = 0.0;
    tumbling_ = true;
    timer_->start();
    update();
}

void D20Widget::stop()
{
    timer_->stop();
    tumbling_ = false;
}

void D20Widget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int count = values_[1] > 0 ? 2 : 1;
    const qreal radius = std::min(height() / 2.0 - 6, 50.0);
    for (int i = 0; i < count; ++i)
    {
        const qreal offset = count == 1 ? 0.0 : (i == 0 ? -1.0 : 1.0) * (radius + 12);
        const QPointF centre(width() / 2.0 + offset, height() / 2.0);

        p.save();
        p.translate(centre);
        if (tumbling_)
        {
            const qreal t = elapsed_ / TUMBLE_S;
            p.rotate((1.0 - t) * 540.0 + i * 37.0);
            const qreal wobble = 1.0 + 0.12 * std::sin(elapsed_ * 25.0 + i);
            p.scale(wobble, wobble);
        }
        paintDie(p, QPointF(0, 0), radius, shown_[i], i == keep_, tumbling_);
        p.restore();
    }
}

void D20Widget::paintDie(QPainter &p, const QPointF &centre, qreal radius, int value,
                         bool kept, bool tumbling) const
{
    QColor ink = DIE_GREEN;
    if (!tumbling)
    {
        if (!kept)
            ink = DIE_GREY;
        else if (value == 20)
            ink = DIE_GOLD;
        else if (value == 1)
            ink = DIE_RED;
    }

    std::array<QPointF, 6> outer;
    for (int i = 0; i < 6; ++i)
        outer[i] = centre + hexVertex(radius, i);
    const QPointF top = centre + QPointF(0, -0.52 * radius);
    const QPointF left = centre + QPointF(-0.45 * radius, 0.26 * radius);
    const QPointF right = centre + QPointF(0.45 * radius, 0.26 * radius);

    p.setPen(QPen(ink, std::max(radius / 22.0, 1.5), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::white);
    p.drawPolygon(outer.data(), 6);

    QColor tint = ink;
    tint.setAlpha(40);
    p.setBrush(tint);
    const QPointF front[3] { top, left, right };
    p.drawPolygon(front, 3);

    // the facet edges from the front face out to the rim
    for (int i : { 0, 1, 5 })
        p.drawLine(top, outer[i]);
    for (int i : { 5, 4, 3 })
        p.drawLine(left, outer[i]);
    for (int i : { 1, 2, 3 })
        p.drawLine(right, outer[i]);

    auto face = [&p](const QPointF& at, const QString& text, qreal size, const QColor& colour)
    {
        QFont font = p.font();
        font.setBold(true);
        font.setPixelSize(std::max(qRound(size), 6));
        p.setFont(font);
        p.setPen(colour);
        p.drawText(QRectF(at.x() - size * 2, at.y() - size, size * 4, size * 2),
                   Qt::AlignCenter, text);
    };

    QColor faint = ink;
    faint.setAlpha(110);
    face(centre + QPointF(-0.60 * radius, -0.10 * radius), QStringLiteral("8"), radius * 0.2, faint);
    face(centre + QPointF(0.60 * radius, -0.10 * radius), QStringLiteral("14"), radius * 0.2, faint);
    face(centre + QPointF(0, 0.66 * radius), QStringLiteral("2"), radius * 0.2, faint);
    face(centre + QPointF(0, 0.03 * radius), QString::number(value), radius * 0.36, ink);
}

////////////////////////////////////////////////////////////////////////////////
// DndPanel

DndPanel::DndPanel(QWidget *parent) :
    QWidget(parent),
    classIndex_(-1),
    startItem_(-1),
    offered_ {},
    scores_ {},
    hp_(0),
    maxHp_(0),
    encounterIndex_(0),
    passed_(0),
    armedItem_(-1),
    approachIndex_(-1),
    difficulties_ {},
    keptRoll_(0)
{
    // --- create a character
    auto welcome = makeFrame();
    welcomeLabel_ = new QLabel;
    welcomeLabel_->setWordWrap(true);
    welcomeLabel_->setTextFormat(Qt::RichText);
    auto welcomeLayout = new QVBoxLayout(welcome);
    welcomeLayout->setContentsMargins(16, 12, 16, 12);
    welcomeLayout->addWidget(welcomeLabel_);

    // the three classes on offer are drawn afresh for every new character
    auto classRow = new QHBoxLayout;
    classRow->addStretch(1);
    for (int i = 0; i < static_cast<int>(classButtons_.size()); ++i)
    {
        classButtons_[i] = makeButton();
        connect(classButtons_[i], &QPushButton::clicked, this, [this, i]() { chooseClass(i); });
        classRow->addWidget(classButtons_[i]);
    }
    classRow->addStretch(1);

    statBlock_ = makeText();

    beginButton_ = makeButton(tr("Begin the quest"));
    connect(beginButton_, &QPushButton::clicked, this, &DndPanel::beginQuest);
    rerollButton_ = makeButton(tr("Reroll"));
    connect(rerollButton_, &QPushButton::clicked, this, &DndPanel::rollStats);

    createScreen_ = new QWidget;
    auto createLayout = new QVBoxLayout(createScreen_);
    createLayout->setContentsMargins(0, 0, 0, 0);
    createLayout->setSpacing(16);
    createLayout->addWidget(welcome);
    createLayout->addLayout(classRow);
    createLayout->addWidget(statBlock_);
    createLayout->addLayout(centredRow({ rerollButton_, beginButton_ }));

    // --- an encounter
    statusLabel_ = makeText();

    auto dmFrame = makeFrame();
    dmLabel_ = new QLabel;
    dmLabel_->setWordWrap(true);
    dmLabel_->setTextFormat(Qt::RichText);
    auto dmLayout = new QVBoxLayout(dmFrame);
    dmLayout->setContentsMargins(16, 12, 16, 12);
    dmLayout->addWidget(dmLabel_);

    auto approachLayout = new QVBoxLayout;
    approachLayout->setSpacing(8);
    for (int i = 0; i < static_cast<int>(approachButtons_.size()); ++i)
    {
        approachButtons_[i] = makeButton();
        connect(approachButtons_[i], &QPushButton::clicked, this, [this, i]() { chooseApproach(i); });
        approachLayout->addLayout(centredRow({ approachButtons_[i] }));
    }

    inventoryRow_ = new QWidget;
    auto inventoryLayout = new QHBoxLayout(inventoryRow_);
    inventoryLayout->setContentsMargins(0, 0, 0, 0);

    hintLabel_ = makeText();

    dice_ = new D20Widget;
    connect(dice_, &D20Widget::settled, this, &DndPanel::finishRoll);

    rollResult_ = makeText();
    auto resultFont = rollResult_->font();
    resultFont.setPointSize(resultFont.pointSize() + 3);
    rollResult_->setFont(resultFont);
    rollOutcome_ = makeText();

    onwardButton_ = makeButton(tr("Onward"));
    connect(onwardButton_, &QPushButton::clicked, this, &DndPanel::onward);

    encounterScreen_ = new QWidget;
    auto encounterLayout = new QVBoxLayout(encounterScreen_);
    encounterLayout->setContentsMargins(0, 0, 0, 0);
    encounterLayout->setSpacing(14);
    encounterLayout->addWidget(statusLabel_);
    encounterLayout->addWidget(dmFrame);
    encounterLayout->addLayout(approachLayout);
    encounterLayout->addWidget(inventoryRow_);
    encounterLayout->addWidget(hintLabel_);
    encounterLayout->addWidget(dice_);
    encounterLayout->addWidget(rollResult_);
    encounterLayout->addWidget(rollOutcome_);
    encounterLayout->addLayout(centredRow({ onwardButton_ }));

    // --- how it ended
    summaryTitle_ = makeText();
    auto titleFont = summaryTitle_->font();
    titleFont.setPointSize(titleFont.pointSize() + 5);
    titleFont.setBold(true);
    summaryTitle_->setFont(titleFont);
    summaryText_ = makeText();

    auto again = makeButton(tr("New adventurer"));
    connect(again, &QPushButton::clicked, this, &DndPanel::reset);
    auto done = makeButton(tr("Continue"));
    connect(done, &QPushButton::clicked, this, [this]()
    {
        reset();
        emit finished();
    });

    summaryScreen_ = new QWidget;
    auto summaryLayout = new QVBoxLayout(summaryScreen_);
    summaryLayout->setContentsMargins(0, 0, 0, 0);
    summaryLayout->setSpacing(14);
    summaryLayout->addWidget(summaryTitle_);
    summaryLayout->addWidget(summaryText_);
    summaryLayout->addLayout(centredRow({ again, done }));

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    layout->addWidget(makeTitle(QStringLiteral("The Flux Dungeon")));
    partyLabel_ = makeText();
    layout->addWidget(partyLabel_);
    layout->addWidget(createScreen_);
    layout->addWidget(encounterScreen_);
    layout->addWidget(summaryScreen_);

    reset();
}

void DndPanel::showScreen(Screen screen)
{
    createScreen_->setVisible(screen == Create);
    encounterScreen_->setVisible(screen == Encounter);
    summaryScreen_->setVisible(screen == Summary);
}

int DndPanel::modifier(int ability) const
{
    return static_cast<int>(std::floor((scores_[ability] - 10) / 2.0));
}

void DndPanel::reset()
{
    dice_->stop();
    classIndex_ = -1;
    startItem_ = -1;
    items_.clear();
    trophies_.clear();
    disadvantage_.clear();
    armedItem_ = -1;

    welcomeLabel_->setText(tr("<b>Dungeon Master:</b> <i>%1</i>")
                               .arg(pickFresh(welcomeLines()).toHtmlEscaped()));
    partyLabel_->setText(pickFresh(partyLines()).toHtmlEscaped());

    for (int i = 0; i < static_cast<int>(classButtons_.size()); ++i)
    {
        int next = classBag_.next(classes().size());
        while (std::find(offered_.begin(), offered_.begin() + i, next) != offered_.begin() + i)
            next = classBag_.next(classes().size());
        offered_[i] = next;

        const auto& dndClass = classes().at(next);
        QStringList bonuses;
        for (int a = 0; a < AbilityCount; ++a)
        {
            if (dndClass.bonus[a] != 0)
                bonuses << QStringLiteral("%1 %2").arg(QLatin1String(ABILITY_NAMES[a]),
                                                       signedNumber(dndClass.bonus[a]));
        }
        classButtons_[i]->setText(QStringLiteral("%1\n%2")
                                      .arg(dndClass.name, bonuses.join(QStringLiteral(", "))));
        classButtons_[i]->setEnabled(true);
    }
    statBlock_->clear();
    statBlock_->setVisible(false);
    beginButton_->setVisible(false);
    rerollButton_->setVisible(false);
    showScreen(Create);
}

void DndPanel::chooseClass(int index)
{
    classIndex_ = offered_[index];
    for (int i = 0; i < static_cast<int>(classButtons_.size()); ++i)
        classButtons_[i]->setEnabled(i != index);
    rollStats();
}

void DndPanel::rollStats()
{
    const auto& dndClass = classes().at(classIndex_);
    for (int a = 0; a < AbilityCount; ++a)
        scores_[a] = rollAbility() + dndClass.bonus[a];
    maxHp_ = std::max(BASE_HP + modifier(Con), 6);
    startItem_ = QRandomGenerator::global()->bounded(static_cast<int>(itemTable().size()));

    QStringList cells;
    for (int a = 0; a < AbilityCount; ++a)
    {
        cells << QStringLiteral("<b>%1</b> %2 (%3)")
                     .arg(QLatin1String(ABILITY_NAMES[a]))
                     .arg(scores_[a])
                     .arg(signedNumber(modifier(a)));
    }
    statBlock_->setText(tr("<b>%1</b><br>%2<br>%3<br>❤️ %4 HP · starts with %5")
                            .arg(dndClass.name,
                                 cells.mid(0, 3).join(QStringLiteral(" · ")),
                                 cells.mid(3, 3).join(QStringLiteral(" · ")))
                            .arg(maxHp_)
                            .arg(itemTable().at(startItem_).name));
    statBlock_->setVisible(true);
    beginButton_->setVisible(true);
    rerollButton_->setVisible(true);
}

void DndPanel::beginQuest()
{
    hp_ = maxHp_;
    items_ = { startItem_ };
    trophies_.clear();
    disadvantage_.clear();
    armedItem_ = -1;
    passed_ = 0;
    encounterIndex_ = 0;

    encounters_.clear();
    while (encounters_.size() < ENCOUNTERS_PER_CAMPAIGN)
    {
        const int next = encounterBag_.next(encounterPool().size());
        if (!encounters_.contains(next))
            encounters_.append(next);
    }

    showScreen(Encounter);
    showEncounter();
}

void DndPanel::showEncounter()
{
    const auto& encounter = encounterPool().at(encounters_.at(encounterIndex_));
    dmLabel_->setText(tr("<b>Dungeon Master:</b> <i>%1</i>").arg(encounter.dmText.toHtmlEscaped()));

    for (int i = 0; i < static_cast<int>(approachButtons_.size()); ++i)
    {
        auto button = approachButtons_[i];
        if (i < encounter.approaches.size())
        {
            const auto& approach = encounter.approaches.at(i);
            difficulties_[i] = rollDifficulty();
            button->setText(tr("%1 — %2, DC %3")
                                .arg(approach.label, QLatin1String(ABILITY_NAMES[approach.ability]))
                                .arg(difficulties_[i]));
            button->setVisible(true);
            button->setEnabled(true);
        }
        else
        {
            button->setVisible(false);
        }
    }

    armedItem_ = -1;
    rebuildInventory();
    updateStatus();
    updateRollHint();
    inventoryRow_->setVisible(true);
    dice_->setVisible(false);
    rollResult_->setVisible(false);
    rollOutcome_->setVisible(false);
    onwardButton_->setVisible(false);
}

void DndPanel::updateStatus()
{
    statusLabel_->setText(tr("❤️ %1 / %2 HP · Encounter %3 of %4")
                              .arg(std::max(hp_, 0)).arg(maxHp_)
                              .arg(encounterIndex_ + 1).arg(ENCOUNTERS_PER_CAMPAIGN));
}

void DndPanel::rebuildInventory()
{
    auto layout = inventoryRow_->layout();
    while (auto item = layout->takeAt(0))
    {
        delete item->widget();
        delete item;
    }

    auto hbox = static_cast<QHBoxLayout*>(layout);
    hbox->addStretch(1);
    if (items_.isEmpty())
    {
        hbox->addWidget(new QLabel(tr("Inventory: empty")));
    }
    else
    {
        hbox->addWidget(new QLabel(tr("Use for advantage:")));
        for (int i = 0; i < items_.size(); ++i)
        {
            const auto& item = itemTable().at(items_.at(i));
            auto button = makeButton(item.name);
            button->setToolTip(item.tip);
            button->setCheckable(true);
            button->setChecked(i == armedItem_);
            button->setProperty("itemIndex", i);
            connect(button, &QPushButton::toggled, this, [this, i](bool on)
            {
                armedItem_ = on ? i : (armedItem_ == i ? -1 : armedItem_);
                // only one item at a time; the buttons are re-checked in place,
                // since this one is still busy emitting
                for (auto other : inventoryRow_->findChildren<QPushButton*>())
                {
                    const QSignalBlocker block(other);
                    other->setChecked(other->property("itemIndex").toInt() == armedItem_);
                }
                updateRollHint();
            });
            hbox->addWidget(button);
        }
    }
    hbox->addStretch(1);
}

void DndPanel::updateRollHint()
{
    const bool advantage = armedItem_ >= 0;
    const bool disadvantage = !disadvantage_.isEmpty();

    QString hint;
    if (advantage && disadvantage)
        hint = tr("Advantage and disadvantage cancel out: one die it is.");
    else if (advantage)
        hint = tr("Advantage: %1 will be used on this roll (2d20, keep the higher).")
                   .arg(itemTable().at(items_.at(armedItem_)).name);
    else if (disadvantage)
        hint = tr("⚠️ %1 (2d20, keep the lower)").arg(disadvantage_);

    hintLabel_->setText(hint.toHtmlEscaped());
    hintLabel_->setVisible(!hint.isEmpty());
}

void DndPanel::chooseApproach(int index)
{
    approachIndex_ = index;
    for (auto button : approachButtons_)
        button->setEnabled(false);
    inventoryRow_->setEnabled(false);

    bool advantage = armedItem_ >= 0;
    bool disadvantage = !disadvantage_.isEmpty();
    if (advantage && disadvantage)
        advantage = disadvantage = false;

    usedItemLine_.clear();
    if (armedItem_ >= 0)
    {
        usedItemLine_ = itemTable().at(items_.at(armedItem_)).useLine;
        items_.removeAt(armedItem_);
    }
    armedItem_ = -1;
    disadvantage_.clear();
    rebuildInventory();

    const int first = d(20);
    if (advantage || disadvantage)
    {
        const int second = d(20);
        const bool firstKept = advantage ? first >= second : first <= second;
        keptRoll_ = firstKept ? first : second;
        rolledDice_ = QStringLiteral("[%1, %2] → %3").arg(first).arg(second).arg(keptRoll_);
        dice_->roll(first, second, firstKept ? 0 : 1);
    }
    else
    {
        keptRoll_ = first;
        rolledDice_ = QString::number(first);
        dice_->roll(first, 0, 0);
    }
    dice_->setVisible(true);
}

void DndPanel::finishRoll()
{
    const auto& encounter = encounterPool().at(encounters_.at(encounterIndex_));
    const auto& approach = encounter.approaches.at(approachIndex_);

    const int mod = modifier(approach.ability);
    const int total = keptRoll_ + mod;
    const int dc = difficulties_[approachIndex_];
    const bool passed = keptRoll_ == 20 || (keptRoll_ != 1 && total >= dc);

    QString verdict = passed ? tr("<b>Success!</b>") : tr("<b>Failure.</b>");
    if (keptRoll_ == 20)
        verdict = tr("<b>NATURAL 20!</b> ") + verdict;
    else if (keptRoll_ == 1)
        verdict = tr("<b>NATURAL 1!</b> ") + verdict;

    rollResult_->setText(tr("🎲 %1 %2 = %3 vs DC %4: %5")
                             .arg(rolledDice_.toHtmlEscaped(),
                                  (mod < 0 ? QStringLiteral("− %1").arg(-mod)
                                           : QStringLiteral("+ %1").arg(mod)))
                             .arg(total)
                             .arg(dc)
                             .arg(verdict));

    QString outcome;
    if (passed)
    {
        ++passed_;
        QString loot;
        if (QRandomGenerator::global()->bounded(2) == 0)
        {
            const int item = lootBag_.next(itemTable().size());
            items_.append(item);
            loot = itemTable().at(item).name;
        }
        else
        {
            loot = pickFresh(trophyLoot());
            trophies_.append(loot);
        }
        outcome = tr("%1<br><br>You gain: <b>%2</b>")
                      .arg(approach.pass.toHtmlEscaped(), loot.toHtmlEscaped());
    }
    else
    {
        hp_ -= approach.damage;
        outcome = tr("%1<br><br>You take <b>%2 damage</b>.")
                      .arg(approach.fail.toHtmlEscaped())
                      .arg(approach.damage);
        if (!approach.disadvantage.isEmpty())
        {
            disadvantage_ = approach.disadvantage;
            outcome += QStringLiteral(" ") + tr("You'll roll the next check with disadvantage.");
        }
    }

    if (!usedItemLine_.isEmpty())
        outcome = usedItemLine_.toHtmlEscaped() + QStringLiteral("<br><br>") + outcome;

    rollResult_->setVisible(true);
    rollOutcome_->setText(outcome);
    rollOutcome_->setVisible(true);
    updateStatus();

    const bool over = hp_ <= 0 || encounterIndex_ + 1 >= ENCOUNTERS_PER_CAMPAIGN;
    onwardButton_->setText(over ? tr("See how it ended") : tr("Onward"));
    onwardButton_->setVisible(true);
}

void DndPanel::onward()
{
    inventoryRow_->setEnabled(true);
    if (hp_ <= 0 || encounterIndex_ + 1 >= ENCOUNTERS_PER_CAMPAIGN)
    {
        showSummary();
        return;
    }
    ++encounterIndex_;
    showEncounter();
}

void DndPanel::showSummary()
{
    const bool survived = hp_ > 0;

    const QStringList* titles = &survivedTitles();
    const QStringList* endings = &survivedEndings();
    if (!survived)
    {
        titles = &defeatedTitles();
        endings = &defeatedEndings();
    }
    else if (passed_ == ENCOUNTERS_PER_CAMPAIGN)
    {
        titles = &flawlessTitles();
        endings = &flawlessEndings();
    }

    const QString ending = fillFlavour(pickFresh(*endings),
                                       { { QStringLiteral("n"), QString::number(encounterIndex_ + 1) },
                                         { QStringLiteral("loot"), counted(trophies_.size() + items_.size(),
                                                                           tr("piece of loot"),
                                                                           tr("pieces of loot")) },
                                         { QStringLiteral("hp"), QString::number(std::max(hp_, 0)) } });

    QStringList loot = trophies_;
    for (int item : items_)
        loot.append(itemTable().at(item).name);
    const QString lootText = loot.isEmpty() ? tr("Loot: none. Not even a spare fuse.")
                                            : tr("Loot: %1").arg(loot.join(QStringLiteral(", ")));

    summaryTitle_->setText(tr("%1 %2").arg(survived ? QStringLiteral("🏰") : QStringLiteral("💀"),
                                           pickFresh(*titles).toHtmlEscaped()));
    summaryText_->setText(tr("%1<br><br>%2 of %3 encounters passed · ❤️ %4 / %5 HP<br>%6")
                              .arg(ending.toHtmlEscaped())
                              .arg(passed_).arg(ENCOUNTERS_PER_CAMPAIGN)
                              .arg(std::max(hp_, 0)).arg(maxHp_)
                              .arg(lootText.toHtmlEscaped()));
    showScreen(Summary);
}
