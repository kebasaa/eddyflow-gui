/***************************************************************************
  filebrowsewidget.cpp
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

#include "filebrowsewidget.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QPushButton>

#include "customdroplineedit.h"
#include "remotebrowsedialog.h"
#include "widget_utils.h"

FileBrowseWidget::FileBrowseWidget() :
    dialogFilter_(QString())
{
    button()->setText(tr("Load..."));
    lineEdit()->setCanBeFile(true);

//#if defined(Q_OS_MACOS)
    lineEdit()->setPlaceholderText(tr("drag and drop here"));
//#endif

    lineEdit()->setAcceptDrops(true);

    connect(lineEdit(), &CustomDropLineEdit::dropped,
            this, &LineEditAndBrowseWidget::pathSelected);
}

FileBrowseWidget::~FileBrowseWidget()
{
}

void FileBrowseWidget::onButtonClick()
{
    //> The file itself when it is there, so the dialog opens with it
    //> selected; otherwise the directory to start in. Never a path that has
    //> gone - Qt answers one of those with a directory of its own choosing.
    const auto startFile = dialogStartFile();

    QString filename = QFileDialog::getOpenFileName(this,
                           dialogTitle(),
                           startFile.isEmpty() ? dialogStartDir() : startFile,
                           dialogFilter());

    if (filename.isEmpty()) { return; }

    auto fileInfo = QFileInfo(filename);
    if (!fileInfo.canonicalPath().isEmpty())
    {
        setDialogWorkingDir(fileInfo.canonicalPath());
    }

    emit pathSelected(filename);
}

void FileBrowseWidget::onRemoteButtonClick()
{
    const auto link = RemoteBrowseDialog::pick(this,
        RemoteBrowseDialog::Mode::File, dialogTitle(), dialogFilter(), path());
    if (link.isEmpty()) { return; }

    emit pathSelected(link);
}
