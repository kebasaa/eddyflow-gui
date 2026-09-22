/***************************************************************************
  eastereggwidgets.h
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

#ifndef EASTEREGGWIDGETS_H
#define EASTEREGGWIDGETS_H

#include <QHash>
#include <QList>
#include <QObject>
#include <QStringList>

#include <functional>
#include <initializer_list>

class QFrame;
class QHBoxLayout;
class QLabel;
class QProgressBar;
class QPushButton;
class QTimer;
class QWidget;

/// Small building blocks shared by the easter-egg panels.
namespace EggUi {

QLabel* makeTitle(const QString& text);
QLabel* makeText(const QString& text = QString());
QPushButton* makeButton(const QString& text = QString());
QProgressBar* makeBar();
QFrame* makeFrame();
QHBoxLayout* centredRow(std::initializer_list<QWidget*> widgets);

QString pickAny(const QStringList& list);

/// A string from list; nothing repeats until the whole list has been used.
/// The list must outlive the call (the data tables are all statics).
QString pickFresh(const QStringList& list);

/// n distinct entries of list, in random order.
QStringList sample(const QStringList& list, int n);

/// Fills {tool}, {place} and {who} with fresh picks, and any other {key}
/// from values, then capitalises the first letter.
QString fillFlavour(QString sentence, const QHash<QString, QString>& values = {});

/// Hands out 0..size-1 in shuffled order, so nothing repeats until
/// everything has been seen.
class ShuffleBag
{
public:
    int next(int size);

private:
    QList<int> order_;
    int position_ = 0;
};

} // namespace EggUi

/// A fake loading sequence: steps through status lines while a progress
/// bar fills, holds at 99% for a moment, then calls done.
class FakeLoading : public QObject
{
    Q_OBJECT
public:
    explicit FakeLoading(QObject* parent = nullptr);

    void run(QProgressBar* bar, QLabel* status, const QStringList& steps,
             int stepMs, std::function<void()> done);
    void stop();

private:
    void advance();

    QTimer* stepTimer_;
    QTimer* holdTimer_;
    QProgressBar* bar_;
    QLabel* status_;
    QStringList steps_;
    int step_;
    std::function<void()> done_;
};

#endif // EASTEREGGWIDGETS_H
