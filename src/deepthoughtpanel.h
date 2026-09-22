/***************************************************************************
  deepthoughtpanel.h
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

#ifndef DEEPTHOUGHTPANEL_H
#define DEEPTHOUGHTPANEL_H

#include <QPointer>
#include <QSet>
#include <QWidget>

#include <array>

#include "eastereggwidgets.h"

class QFrame;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTimer;

/// Marvin, drawn after the 2005 film: white plating, an oversized round
/// head that is featureless but for two triangular green eyes, and stubby
/// limbs. He sags, sighs, and complains when poked. At maximum depression
/// he gives up and sits on the floor.
class MarvinWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MarvinWidget(QWidget* parent = nullptr);

    void start();
    void stop();
    void setCollapsed(bool collapsed);

signals:
    void poked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    QTimer* timer_;
    qreal phase_;
    qreal pokeSag_;
    bool collapsed_;
};

/// Deep Thought: answers a typed question (always 42, in some unit), with
/// Marvin muttering alongside, an Infinite Improbability Drive and a Babel
/// fish that translates the question into Vogon.
class DeepThoughtPanel : public QWidget
{
    Q_OBJECT
public:
    explicit DeepThoughtPanel(QWidget* parent = nullptr);

    /// back to an empty question; stops anything that is running
    void reset();

signals:
    void finished();

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    // the order of marvinLines() in the .cpp
    enum class Moment { Open, Thinking, Answer, Poke, MaxReached, AtMax };

    void ask();
    void showAnswer(const QString& question);
    void newQuestion();

    void engageDrive();
    void advanceOdds();
    void applyImprobability();
    void flyEmoji(const QString& emoji, bool falling, std::function<void()> done);
    void setAnswerButtonsEnabled(bool enabled);

    void translate();

    void marvinSays(Moment moment);
    void depress(int amount);

    FakeLoading* loading_;
    QTimer* oddsTimer_;
    qreal oddsProgress_;

    int answersGiven_;
    QSet<QString> askedQuestions_;
    QString currentQuestion_;
    QString currentAnswer_;
    EggUi::ShuffleBag effects_;
    bool marvinGaveUp_;

    QLabel* flavour_;
    QLineEdit* questionEdit_;
    QPushButton* askButton_;
    QProgressBar* thinkBar_;
    QLabel* thinkStatus_;
    QFrame* answerFrame_;
    QLabel* questionEcho_;
    QLabel* answerLabel_;
    QLabel* vogonLabel_;
    QLabel* oddsLabel_;
    QPushButton* babelButton_;
    QPushButton* driveButton_;
    QPushButton* anotherButton_;
    QPushButton* backToWorkButton_;
    QPointer<QLabel> flying_;

    MarvinWidget* marvin_;
    QLabel* marvinSpeech_;
    QProgressBar* depression_;
};

#endif // DEEPTHOUGHTPANEL_H
