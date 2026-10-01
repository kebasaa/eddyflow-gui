/***************************************************************************
  eastereggkeyfilter.h
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

#ifndef EASTEREGGKEYFILTER_H
#define EASTEREGGKEYFILTER_H

#include <QElapsedTimer>
#include <QObject>

/// Application-wide filter that watches for the keys e, d, d, y typed in
/// order and announces them with unlocked(). It never consumes an event.
class EasterEggKeyFilter : public QObject
{
    Q_OBJECT
public:
    explicit EasterEggKeyFilter(QObject* parent = nullptr);

signals:
    void unlocked();

protected:
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    int progress_; // how many keys of the sequence have been matched
    QElapsedTimer lastPress_;
};

#endif // EASTEREGGKEYFILTER_H
