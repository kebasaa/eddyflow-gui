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

const std::array<int, 4> SEQUENCE { Qt::Key_E, Qt::Key_D, Qt::Key_D, Qt::Key_Y };
const qint64 MAX_GAP_MS = 1500;

// typing "eddy" into a field is data entry, not a cheat code
bool isTextInput(QWidget* w)
{
    if (!w)
        return false;
    if (qobject_cast<QLineEdit*>(w)
        || qobject_cast<QTextEdit*>(w)
        || qobject_cast<QPlainTextEdit*>(w)
        || qobject_cast<QAbstractSpinBox*>(w))
        return true;
    if (auto combo = qobject_cast<QComboBox*>(w))
        return combo->isEditable();
    return false;
}

} // namespace

EasterEggKeyFilter::EasterEggKeyFilter(QObject *parent) :
    QObject(parent),
    progress_(0)
{
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
                progress_ = 0;
            }
            else
            {
                if (progress_ > 0 && lastPress_.elapsed() > MAX_GAP_MS)
                    progress_ = 0;

                if (key->key() == SEQUENCE[progress_])
                    ++progress_;
                else // a wrong key may still start the sequence over
                    progress_ = (key->key() == SEQUENCE[0]) ? 1 : 0;
                lastPress_.start();

                if (progress_ == static_cast<int>(SEQUENCE.size()))
                {
                    progress_ = 0;
                    emit unlocked();
                }
            }
        }
    }
    return QObject::eventFilter(o, e);
}
