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

#include <QList>
#include <QSet>
#include <QStringList>
#include <QWidget>

#include <array>
#include <functional>

class QFrame;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTimer;

class DespikeArena;

/// The hidden page unlocked by typing "eddy". Each unlock shows one
/// of several variants; when the joke is over the page emits finished() and
/// MainWindow hides its tab again.
class EasterEggPage : public QWidget
{
    Q_OBJECT
public:
    enum class Variant { Doom, Hitchhiker, DnD, Count };

    explicit EasterEggPage(QWidget* parent = nullptr);

    void setVariant(Variant variant);
    static QString tabText(Variant variant);

signals:
    void finished();

protected:
    void hideEvent(QHideEvent* event) override;

private:
    QWidget* createDoomPanel();
    QWidget* createHitchhikerPanel();
    QWidget* createDicePanel();

    void runLoading(QProgressBar* bar, QLabel* status, const QStringList& steps,
                    int stepMs, std::function<void()> done);
    void advanceLoading();
    void stopLoading();

    void launchDoom();
    void startGame();
    void showGameResult(int despiked, int escaped, int shots, bool died);
    void resetDoomPanel();

    void askDeepThought();
    void showAnswer(const QString& question);
    void resetQuestion();
    void resetHitchhikerPanel();

    void roll();
    void advanceRoll();
    void finishRoll();
    void resetDicePanel();

    void resetAll();
    int nextIndex(Variant variant, int size);

    Variant variant_;
    int rollTicks_;
    int scenario_;
    int answersGiven_;
    QSet<QString> askedQuestions_;

    // per-variant shuffled order, so nothing repeats until all has been seen
    std::array<QList<int>, static_cast<int>(Variant::Count)> orders_;
    std::array<int, static_cast<int>(Variant::Count)> positions_;

    // shared fake-loading machinery
    QTimer* loadingTimer_;
    QTimer* holdTimer_;
    QProgressBar* loadingBar_;
    QLabel* loadingStatus_;
    QStringList loadingSteps_;
    int loadingStep_;
    std::function<void()> loadingDone_;

    QWidget* column_;
    QWidget* doomPanel_;
    QWidget* hitchhikerPanel_;
    QWidget* dicePanel_;

    QPushButton* launchButton_;
    QProgressBar* doomBar_;
    QLabel* doomStatus_;
    DespikeArena* arena_;
    QWidget* doomResult_;
    QLabel* doomRank_;
    QLabel* doomStats_;
    QLabel* doomTag_;

    QLineEdit* questionEdit_;
    QPushButton* askButton_;
    QProgressBar* thinkBar_;
    QLabel* thinkStatus_;
    QFrame* answerFrame_;
    QLabel* questionEcho_;
    QLabel* answerLabel_;
    QPushButton* anotherButton_;
    QPushButton* backToWorkButton_;

    QLabel* dmLabel_;
    QPushButton* rollButton_;
    QLabel* rollResult_;
    QLabel* rollOutcome_;
    QPushButton* continueButton_;
    QTimer* rollTimer_;
};

#endif // EASTEREGGPAGE_H
