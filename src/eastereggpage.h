/***************************************************************************
  eastereggpage.h
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

#ifndef EASTEREGGPAGE_H
#define EASTEREGGPAGE_H

#include <QIcon>
#include <QWidget>

#include "eastereggwidgets.h"

class QLabel;
class QProgressBar;
class QPushButton;

class DeepThoughtPanel;
class DespikeArena;
class DndPanel;
class FakeLoading;

/// The hidden page unlocked by typing "eddy". Each unlock shows one of
/// several variants; when the joke is over the page emits finished() and
/// MainWindow hides its tab again.
class EasterEggPage : public QWidget
{
    Q_OBJECT
public:
    enum class Variant { Doom, Hitchhiker, DnD, Count };

    explicit EasterEggPage(QWidget* parent = nullptr);

    void setVariant(Variant variant);
    static QString tabText(Variant variant);
    static QIcon tabIcon(Variant variant);

signals:
    void finished();

protected:
    void hideEvent(QHideEvent* event) override;

private:
    QWidget* createDoomPanel();

    void launchDoom();
    void startGame();
    void showGameResult(int score, int despiked, int escaped, int validRemoved,
                        int shots, bool died);
    void resetDoomPanel();

    void resetAll();

    Variant variant_;
    FakeLoading* loading_;

    QWidget* doomPanel_;
    DeepThoughtPanel* deepThought_;
    DndPanel* dnd_;

    QPushButton* launchButton_;
    QProgressBar* doomBar_;
    QLabel* doomStatus_;
    DespikeArena* arena_;
    QWidget* doomResult_;
    QLabel* doomRank_;
    QLabel* doomStats_;
    QLabel* doomTag_;
};

#endif // EASTEREGGPAGE_H
