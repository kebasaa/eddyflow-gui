/***************************************************************************
  lineeditandbrowsewidget.h
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

#ifndef LINEEDITANDBROWSEWIDGET_H
#define LINEEDITANDBROWSEWIDGET_H

#include <QWidget>

class CustomDropLineEdit;
class QPushButton;

class LineEditAndBrowseWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LineEditAndBrowseWidget(QWidget *parent = nullptr);

    QString toolTip() const;
    void setToolTip(const QString &text);

    QString buttonText() const;
    void setButtonText(const QString &text);

    QString text() const;
    void setText(const QString &text);

    /// The path, or for a value on a shared drive, its link
    QString path() const;
    /// A link to a shared drive is shown by name and kept whole for path().
    /// A widget without remote browsing is an output location, and refuses
    /// a link with a warning window.
    void setPath(const QString &path);

    ///
    /// \brief setRemoteBrowseEnabled
    /// Show the "Remote drive..." button, for an input that may come from a
    /// shared Google Drive or Dropbox link.
    void setRemoteBrowseEnabled(bool on);
    bool remoteBrowseEnabled() const { return remoteEnabled_; }

    QString dialogTitle() const { return dialogTitle_; }
    void setDialogTitle(const QString &title) { dialogTitle_ = title; }

    /// Where this field was last browsed from, remembered per field.
    QString dialogWorkingDir() const { return dialogDir_; }
    void setDialogWorkingDir(const QString &dir) { dialogDir_ = dir; }

    ///
    /// \brief setDialogPathHint
    /// The location this field was given, whether or not it still exists.
    /// setPath() records it itself; a page that blanks the field for a path
    /// that has gone states it here, so the browse dialog still opens as
    /// near to it as still exists.
    void setDialogPathHint(const QString &path) { dialogPathHint_ = path; }
    QString dialogPathHint() const { return dialogPathHint_; }

    void focusAndSelect() const;

    ///
    /// \brief disableClearAction
    /// Disable automatic clear on the lineedit.
    /// Useful for providing custom behaviors.
    void disableClearAction() const;

    int returnLineEditWidth() const;

    QPushButton *button() const;
    QPushButton *remoteButton() const { return remoteButton_; }

    void disableClickAction() const;

signals:
    ///
    /// \brief clearRequested
    /// Sent to notify clear button toggling.
    void clearRequested();

    ///
    /// \brief pathChanged
    /// Sent in case of programmatic line edit changes.
    void pathChanged(const QString&);

    ///
    /// \brief pathSelected
    /// Sent in case of successful user interaction.
    void pathSelected(const QString&);

protected:
    CustomDropLineEdit *lineEdit() const;

    ///
    /// \brief dialogStartDir
    /// An existing directory for the browse dialog to open in; never empty.
    /// The field's own location comes first - a path read from a project is
    /// where the user expects to land - and only then the location this
    /// field was last browsed from.
    QString dialogStartDir() const;

    ///
    /// \brief dialogStartFile
    /// The field's own value while it still names a file that is there, so
    /// the file dialog can open with it selected. Empty otherwise: Qt falls
    /// back to a remembered directory of its own for a path that has gone.
    QString dialogStartFile() const;

public slots:
    void clear();
    void setEnabled(bool enable);

private slots:
    void updatePathTooltip();
    void onTextChanged();
    virtual void onButtonClick() = 0;
    virtual void onRemoteButtonClick() {}

private:
    /// The field's own location, a link excluded; empty when it has none.
    QString dialogPathCandidate() const;

    CustomDropLineEdit *lineEdit_;
    QPushButton *button_;
    QPushButton *remoteButton_;
    QString remoteLink_;
    bool remoteEnabled_;
    QString dialogTitle_;
    QString dialogDir_;
    QString dialogPathHint_;
};

#endif  // LINEEDITANDBROWSEWIDGET_H
