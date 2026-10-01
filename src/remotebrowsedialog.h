/***************************************************************************
  remotebrowsedialog.h
  --------------------
  Copyright © 2026, ETH Zurich, Jonathan Muller

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

#ifndef REMOTEBROWSEDIALOG_H
#define REMOTEBROWSEDIALOG_H

#include <QDialog>
#include <QList>
#include <QPair>

#include "remotesource.h"

class QComboBox;
class QLabel;
class QPushButton;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

///
/// \brief Asks for the link to a shared drive, and checks it can be listed.
///
class RemoteLinkDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RemoteLinkDialog(const QString& initial, QWidget* parent = nullptr);

    /// The link to a shared folder, or empty if cancelled
    static QString getLink(QWidget* parent, const QString& initial = QString());

    QString link() const { return link_; }

public slots:
    void accept() override;

private:
    QComboBox* linkEdit_;
    QString link_;
};

///
/// \brief A shared drive, browsed like the file dialog browses a folder.
///
/// One folder at a time; double click opens a folder, Up and Back go where
/// they would. Directory mode selects the folder that is open, or the one
/// selected in it; File mode selects a file.
///
class RemoteBrowseDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode { Directory, File };

    RemoteBrowseDialog(Mode mode, const QString& title, const QString& filter,
                       const QString& rootLink, QWidget* parent = nullptr);

    ///
    /// \brief The whole "Remote drive..." flow.
    ///
    /// Asks for a link only when no drive is in use yet; otherwise opens the
    /// current drive at once, at currentValue's own folder when it is on that
    /// drive. Returns the chosen folder or file as the value to store, or
    /// empty if cancelled.
    ///
    static QString pick(QWidget* parent, Mode mode, const QString& title,
                        const QString& filter, const QString& currentValue);

    QString selected() const { return selected_; }

    /// Open the folder at relPath below the root, one level at a time
    void openAt(const QString& relPath);

public slots:
    void accept() override;

private slots:
    void itemActivated(QTreeWidgetItem* item);
    void goUp();
    void goBack();
    void differentDrive();
    void refill();

private:
    struct Level
    {
        QString link;
        QString name;
    };

    bool load(const QList<Level>& path);
    QString relPath(const QString& leaf = QString()) const;
    bool matchesFilter(const QString& name) const;
    void setRoot(const QString& rootLink);

    Mode mode_;
    QString root_;
    QList<Level> path_;
    QList<QList<Level>> history_;
    QList<RemoteSource::Entry> entries_;
    QString selected_;

    QToolButton* backButton_;
    QToolButton* upButton_;
    QLabel* location_;
    QPushButton* driveButton_;
    QTreeWidget* view_;
    QComboBox* filterCombo_;
    QPushButton* okButton_;
};

#endif // REMOTEBROWSEDIALOG_H
