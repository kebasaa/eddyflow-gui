/***************************************************************************
  customlineedit.h
  -------------------
  Copyright © 2014, LI-COR Biosciences, Antonio Forgione
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
 ***************************************************************************/

#ifndef CUSTOMLINEEDIT_H
#define CUSTOMLINEEDIT_H

#include <QIcon>
#include <QLineEdit>

class QAction;

class CustomLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit CustomLineEdit(QWidget *parent = nullptr);

    QIcon icon() const;
    void setIcon(const QIcon &icon);

    QIcon defaultIcon() const;
    void setDefaultIcon(const QIcon &icon);

    QString regExp() const;
    void setRegExp(const QString &s);

signals:
    void buttonClicked();

protected:
    QAction *action() const;

private slots:
    virtual void updateAction(const QString &text) = 0;

private:
    QAction *action_;
    QIcon icon_;
    QIcon defaultIcon_;
    QString re_;
};

#endif // CUSTOMLINEEDIT_H
