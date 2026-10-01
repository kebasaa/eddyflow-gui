/***************************************************************************
  dndpanel.h
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

#ifndef DNDPANEL_H
#define DNDPANEL_H

#include <QList>
#include <QStringList>
#include <QWidget>

#include <array>

#include "eastereggwidgets.h"

class QFrame;
class QLabel;
class QPushButton;
class QTimer;

/// One or two d20s that tumble for a moment and then land. With two dice
/// (advantage or disadvantage) the discarded one is greyed out.
class D20Widget : public QWidget
{
    Q_OBJECT
public:
    explicit D20Widget(QWidget* parent = nullptr);

    /// second = 0 rolls a single die; keep is the index of the die that counts
    void roll(int first, int second, int keep);
    void stop();

signals:
    void settled();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void paintDie(QPainter& p, const QPointF& centre, qreal radius, int value,
                  bool kept, bool tumbling) const;

    QTimer* timer_;
    qreal elapsed_;
    bool tumbling_;
    std::array<int, 2> values_;
    std::array<int, 2> shown_;
    int keep_;
};

/// A short D&D campaign on the flux tower: make a character, face three
/// encounters with a choice of approach each, and see how it ended.
class DndPanel : public QWidget
{
    Q_OBJECT
public:
    explicit DndPanel(QWidget* parent = nullptr);

    /// back to character creation; stops anything that is running
    void reset();

signals:
    void finished();

private:
    void chooseClass(int index);
    void rollStats();
    void beginQuest();

    void showEncounter();
    void chooseApproach(int index);
    void finishRoll();
    void onward();
    void showSummary();

    void rebuildInventory();
    void updateStatus();
    void updateRollHint();

    enum Screen { Create, Encounter, Summary };
    void showScreen(Screen screen);

    int modifier(int ability) const;

    // character
    int classIndex_;
    int startItem_;
    std::array<int, 3> offered_;
    std::array<int, 6> scores_;
    int hp_;
    int maxHp_;
    QList<int> items_;      // indices into the item table
    QStringList trophies_;

    // campaign
    QList<int> encounters_;
    int encounterIndex_;
    int passed_;
    QString disadvantage_;
    int armedItem_;
    int approachIndex_;
    std::array<int, 3> difficulties_; // rolled when the encounter is shown
    int keptRoll_;
    QString rolledDice_;
    QString usedItemLine_;
    EggUi::ShuffleBag encounterBag_;
    EggUi::ShuffleBag classBag_;
    EggUi::ShuffleBag lootBag_;

    QWidget* createScreen_;
    QWidget* encounterScreen_;
    QWidget* summaryScreen_;

    QLabel* partyLabel_;

    // create
    QLabel* welcomeLabel_;
    std::array<QPushButton*, 3> classButtons_;
    QLabel* statBlock_;
    QPushButton* beginButton_;
    QPushButton* rerollButton_;

    // encounter
    QLabel* statusLabel_;
    QLabel* dmLabel_;
    std::array<QPushButton*, 3> approachButtons_;
    QWidget* inventoryRow_;
    QLabel* hintLabel_;
    D20Widget* dice_;
    QLabel* rollResult_;
    QLabel* rollOutcome_;
    QPushButton* onwardButton_;

    // summary
    QLabel* summaryTitle_;
    QLabel* summaryText_;
};

#endif // DNDPANEL_H
