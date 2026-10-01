/***************************************************************************
  lineeditandbrowsewidget.cpp
  -------------------
  Widget with combination of line edit and browse button
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

#include "lineeditandbrowsewidget.h"

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QPushButton>

#include "customdroplineedit.h"
#include "remotesource.h"
#include "widget_utils.h"

LineEditAndBrowseWidget::LineEditAndBrowseWidget(QWidget *parent) :
    QWidget(parent),
    lineEdit_{},
    button_{},
    remoteButton_{},
    remoteLink_(QString()),
    remoteEnabled_(false),
    dialogTitle_(QString()),
    dialogDir_(QString())
{
    lineEdit_ = new CustomDropLineEdit;
    lineEdit_->setReadOnly(true);
    lineEdit_->setProperty("loadEdit", true);

    button_ = new QPushButton;
    button_->setProperty("loadButton", true);

    remoteButton_ = new QPushButton(tr("Remote drive..."));
    remoteButton_->setProperty("remoteButton", true);
    remoteButton_->setToolTip(tr("Dropbox or Google Drive link"));
    remoteButton_->setVisible(false);

    auto container = new QHBoxLayout(this);
    container->addWidget(lineEdit_);
    container->addWidget(button_);
    container->addWidget(remoteButton_);
    container->setStretch(0, 1);
    container->setContentsMargins(0, 0, 0, 0);
    container->setSpacing(0);

    setLayout(container);

    connect(lineEdit_, &CustomClearLineEdit::buttonClicked,
            this, &LineEditAndBrowseWidget::clearRequested);
    connect(lineEdit_, &QLineEdit::textChanged,
            this, &LineEditAndBrowseWidget::onTextChanged);
    connect(lineEdit_, &QLineEdit::textChanged,
            this, &LineEditAndBrowseWidget::updatePathTooltip);
    connect(button_, &QPushButton::clicked,
            this, &LineEditAndBrowseWidget::onButtonClick);
    connect(remoteButton_, &QPushButton::clicked,
            this, &LineEditAndBrowseWidget::onRemoteButtonClick);
}

void LineEditAndBrowseWidget::setRemoteBrowseEnabled(bool on)
{
    remoteEnabled_ = on;
    remoteButton_->setVisible(on);
}

// What is shown stands for the link only while it is the name the link was
// shown as; clearing or replacing the text drops the link.
void LineEditAndBrowseWidget::onTextChanged()
{
    if (!remoteLink_.isEmpty()
        && lineEdit_->text() != RemoteSource::displayName(remoteLink_))
    {
        remoteLink_.clear();
    }
    emit pathChanged(path());
}

void LineEditAndBrowseWidget::clear()
{
    remoteLink_.clear();
    lineEdit_->clear();
}

void LineEditAndBrowseWidget::setEnabled(bool enable)
{
    lineEdit_->setEnabled(enable);
    button_->setEnabled(enable);
    remoteButton_->setEnabled(enable);
}

void LineEditAndBrowseWidget::setToolTip(const QString &text)
{
    lineEdit_->setToolTip(text);
    button_->setToolTip(text);
}

QString LineEditAndBrowseWidget::toolTip() const
{
    return lineEdit_->toolTip();
}

void LineEditAndBrowseWidget::setButtonText(const QString& text)
{
    button_->setText(text);
}

// NOTE: never used
QString LineEditAndBrowseWidget::buttonText() const
{
    return button_->text();
}

void LineEditAndBrowseWidget::setText(const QString &text)
{
    lineEdit_->setText(text);
}

void LineEditAndBrowseWidget::setPath(const QString &path)
{
    if (RemoteSource::isRemote(path))
    {
        if (!remoteEnabled_)
        {
            WidgetUtils::warning(this,
                                 tr("Remote Drive"),
                                 tr("Output locations must be local folders; "
                                    "EddyFlow cannot write to a shared drive."),
                                 path);
            return;
        }
        remoteLink_ = RemoteSource::cleanPath(path);
        lineEdit_->setText(RemoteSource::displayName(remoteLink_));
        lineEdit_->setToolTip(remoteLink_);
        return;
    }

    remoteLink_.clear();
    QFileInfo filePath(path);
    QString canonicalFilePath = filePath.canonicalFilePath();
    lineEdit_->setText(QDir::toNativeSeparators(canonicalFilePath));
}

void LineEditAndBrowseWidget::disableClearAction() const
{
    lineEdit_->setDisconnectedAction();
}

void LineEditAndBrowseWidget::disableClickAction() const
{
    disconnect(button_, &QPushButton::clicked,
               this, &LineEditAndBrowseWidget::onButtonClick);
}

int LineEditAndBrowseWidget::returnLineEditWidth() const
{
    return lineEdit_->width();
}

void LineEditAndBrowseWidget::focusAndSelect() const
{
    lineEdit_->setFocus();
    lineEdit_->selectAll();
}

CustomDropLineEdit *LineEditAndBrowseWidget::lineEdit() const
{
    return lineEdit_;
}

QPushButton *LineEditAndBrowseWidget::button() const
{
    return button_;
}

QString LineEditAndBrowseWidget::text() const
{
    return lineEdit_->text();
}

QString LineEditAndBrowseWidget::path() const
{
    return remoteLink_.isEmpty() ? text() : remoteLink_;
}

void LineEditAndBrowseWidget::updatePathTooltip()
{
    WidgetUtils::updateLineEditToolip(lineEdit_);
}
