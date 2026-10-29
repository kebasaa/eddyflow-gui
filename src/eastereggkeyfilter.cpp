/***************************************************************************
  eastereggkeyfilter.cpp
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

#include "eastereggkeyfilter.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>

#include <array>

namespace {

const std::array<int, 4> EDDY { Qt::Key_E, Qt::Key_D, Qt::Key_D, Qt::Key_Y };
const std::array<int, 6> MATRIX { Qt::Key_M, Qt::Key_A, Qt::Key_T, Qt::Key_R, Qt::Key_I, Qt::Key_X };
const std::array<int, 4> FLUX { Qt::Key_F, Qt::Key_L, Qt::Key_U, Qt::Key_X };
const std::array<int, 3> DND { Qt::Key_D, Qt::Key_N, Qt::Key_D };

// one more key of a sequence; true when it completes it
template <size_t N>
bool advance(const std::array<int, N>& sequence, int& progress, int key)
{
    if (key == sequence[progress])
        ++progress;
    else // a wrong key may still start the sequence over
        progress = (key == sequence[0]) ? 1 : 0;
    if (progress == static_cast<int>(N))
    {
        progress = 0;
        return true;
    }
    return false;
}
const qint64 MAX_GAP_MS = 1500;

// typing "eddy", "matrix", "flux" or "dnd" into a field is data entry, not a
// cheat code
//
// Only a field that can actually be typed into counts. A read-only view is
// not one: the console and the info dock are read-only QPlainTextEdits, and
// they hold the focus for as long as a run lasts, so judging by class alone
// swallowed every key while anything was being processed.
bool isTextInput(QWidget* w)
{
    if (!w)
        return false;
    if (auto line = qobject_cast<QLineEdit*>(w))
        return !line->isReadOnly();
    if (auto plain = qobject_cast<QPlainTextEdit*>(w))
        return !plain->isReadOnly();
    if (auto text = qobject_cast<QTextEdit*>(w))
        return !text->isReadOnly();
    if (auto spin = qobject_cast<QAbstractSpinBox*>(w))
        return !spin->isReadOnly();
    if (auto combo = qobject_cast<QComboBox*>(w))
        return combo->isEditable();
    return false;
}

} // namespace

EasterEggKeyFilter::EasterEggKeyFilter(QObject *parent) :
    QObject(parent),
    progress_{ 0, 0, 0, 0 }
{
}

void EasterEggKeyFilter::restart()
{
    for (auto& progress : progress_)
        progress = 0;
}

bool EasterEggKeyFilter::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::KeyPress)
    {
        //> An application filter sees the same press once per widget it
        //> propagates through, so count it only at its first recipient.
        QWidget* focus = QApplication::focusWidget();
        QObject* firstRecipient = focus ? static_cast<QObject*>(focus)
                                        : static_cast<QObject*>(QApplication::activeWindow());
        auto key = static_cast<QKeyEvent*>(e);

        if (o == firstRecipient && !key->isAutoRepeat())
        {
            const auto blocking = Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;

            if (key->key() == Qt::Key_Shift)
            {
                // Shift alone neither advances nor breaks the sequence
            }
            else if ((key->modifiers() & blocking) || isTextInput(focus))
            {
                restart();
            }
            else
            {
                if (lastPress_.isValid() && lastPress_.elapsed() > MAX_GAP_MS)
                    restart();
                lastPress_.start();

                if (advance(EDDY, progress_[0], key->key()))
                    emit unlocked();
                if (advance(MATRIX, progress_[1], key->key()))
                    emit matrixUnlocked();
                if (advance(FLUX, progress_[2], key->key()))
                    emit fluxUnlocked();
                if (advance(DND, progress_[3], key->key()))
                    emit dndUnlocked();
            }
        }
    }
    return QObject::eventFilter(o, e);
}
