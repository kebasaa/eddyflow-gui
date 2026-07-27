/***************************************************************************
  dirbrowsewidget.cpp
  -------------------
  Variation of lineedit and browse widget
  -------------------
  Copyright © 2016-2018, LI-COR Biosciences, Antonio Forgione
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

#include "dirbrowsewidget.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QPushButton>

#include "customdroplineedit.h"
#include "widget_utils.h"

DirBrowseWidget::DirBrowseWidget()
{
    button()->setText(tr("Browse..."));

//#if defined(Q_OS_MACOS)
    lineEdit()->setPlaceholderText(tr("drag and drop here"));
//#endif

    lineEdit()->setAcceptDrops(true);

    connect(lineEdit(), &CustomDropLineEdit::dropped,
            this, &LineEditAndBrowseWidget::pathSelected);
}

DirBrowseWidget::~DirBrowseWidget()
{
}

bool DirBrowseWidget::isReadOnly() const
{
    return lineEdit()->isReadOnly();
}

void DirBrowseWidget::setReadOnly(bool on)
{
    lineEdit()->setReadOnly(on);
}

void DirBrowseWidget::onButtonClick()
{
    auto workingDir = dialogWorkingDir();
    if (workingDir.isEmpty() || !QFileInfo(workingDir).isDir())
    {
        workingDir = WidgetUtils::getSearchPathHint();
    }

    QString dirname = QFileDialog::getExistingDirectory(this,
                          dialogTitle(),
                          workingDir);

    if (dirname.isEmpty()) { return; }

    auto dirInfo = QFileInfo(dirname);
    if (!dirInfo.canonicalFilePath().isEmpty())
    {
        setDialogWorkingDir(dirInfo.canonicalFilePath());
    }

    emit pathSelected(dirname);
}
