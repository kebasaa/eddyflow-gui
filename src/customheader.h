/***************************************************************************
  customheader.h
  -------------------
  Custom header for table view classes
  -------------------
  Copyright © 2007-2011, Eco2s team, Antonio Forgione
  Copyright © 2011-2018, LI-COR Biosciences, Antonio Forgione
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

#ifndef CUSTOMHEADER_H
#define CUSTOMHEADER_H

#include <QWidget>

#include "clicklabel.h"

class QGridLayout;

class CustomHeader : public QWidget
{
Q_OBJECT

public:
    enum class  QuestionMarkHint { NoQuestionMark, QuestionMark };

    explicit CustomHeader(QWidget* parent = nullptr);
    void addSection(const QString& txt,
                    const QString& tooltipTxt,
                    QuestionMarkHint questionMark = QuestionMarkHint::NoQuestionMark,
                    ClickLabel::HeaderData headerData = ClickLabel::NoHeader);
    int sectionCount();
    //> Make every section exactly one table row tall. The labels sit beside
    //> the table in a grid of their own, so without this the layout shares the
    //> widget's height out by each label's natural height: one label taller
    //> than a row - rich text such as k<sub>W</sub> renders taller - steals
    //> from the rest and every label below it slides off its row.
    void setSectionHeight(int height);

signals:

private slots:
    void onlineHelpTrigger();

private:
    void applySectionHeight(QWidget* w);

    QGridLayout *layout;
    //> 0 until a view states a row height, so a header nobody sizes keeps the
    //> natural layout it had before.
    int sectionHeight {0};
    //> Counted here rather than read back from the layout: the bottom stretch
    //> setSectionHeight adds is a grid row too, and rowCount() would include it.
    int sections {0};
};

#endif // CUSTOMHEADER_H
