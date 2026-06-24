/***************************************************************************
  richtextcheckbox.h
  -------------------
  Checkbox with rich text support
  -------------------
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

#ifndef RICHTEXTCHECKBOX_H
#define RICHTEXTCHECKBOX_H

#include <QWidget>
#include <QUrl>

class QPushButton;

class ClickableLabel;
class HoverCheckBox;

class RichTextCheckBox : public QWidget
{
    Q_OBJECT
public:
    explicit RichTextCheckBox(QWidget *parent = nullptr);
    ~RichTextCheckBox();

    HoverCheckBox* getCheckBox() { return checkBox; }

    QString text() const;
    void setText(const QString &text);

    QString plainText() const;

    Qt::CheckState checkState() const;
    bool isChecked() const;

    void clearStates();

    void setQuestionMark(const QString& url);

signals:
    void toggled(bool on);
    void clicked();

public slots:
    void setChecked(bool checked);
    void setCheckState(Qt::CheckState state);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private slots:
    void triggerOnlineHelp();

private:
    HoverCheckBox* checkBox;
    ClickableLabel* label;
    QPushButton* questionMark;
    QUrl questionMarkLink;
};

#endif  // RICHTEXTCHECKBOX_H


