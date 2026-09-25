/***************************************************************************
  remotebrowsedialog.cpp
  ----------------------
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

#include "remotebrowsedialog.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileIconProvider>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QRegularExpression>
#include <QStyle>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// RemoteLinkDialog
// ---------------------------------------------------------------------------

RemoteLinkDialog::RemoteLinkDialog(const QString& initial, QWidget* parent) :
    QDialog(parent),
    linkEdit_(new QComboBox)
{
    setWindowTitle(tr("Remote Drive"));

    auto label = new QLabel(tr("Paste the link to a Google Drive or Dropbox folder "
                               "shared with <b>Anyone with the link</b>:"));
    label->setWordWrap(true);

    linkEdit_->setEditable(true);
    linkEdit_->addItems(RemoteSource::recentDrives());
    linkEdit_->setEditText(initial);
    linkEdit_->lineEdit()->setPlaceholderText(
        QStringLiteral("https://drive.google.com/drive/folders/..."));
    linkEdit_->setMinimumWidth(520);
    linkEdit_->setToolTip(tr("Dropbox or Google Drive link"));

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Open"));
    connect(buttons, &QDialogButtonBox::accepted, this, &RemoteLinkDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &RemoteLinkDialog::reject);

    auto layout = new QVBoxLayout(this);
    layout->addWidget(label);
    layout->addWidget(linkEdit_);
    layout->addWidget(buttons);
}

void RemoteLinkDialog::accept()
{
    const auto text = linkEdit_->currentText().trimmed();
    if (RemoteSource::provider(text) == RemoteSource::Provider::None)
    {
        RemoteSource::warn(this, tr("This is not a Google Drive or Dropbox link."),
                           tr("A folder link looks like https://drive.google.com/drive/folders/... "
                              "or https://www.dropbox.com/scl/fo/..."));
        return;
    }

    // Checked here, so a link that does not work never becomes the drive
    QList<RemoteSource::Entry> entries;
    QString error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = RemoteSource::listFolder(text, &entries, &error);
    QApplication::restoreOverrideCursor();
    if (!ok)
    {
        RemoteSource::warn(this, tr("Could not open the shared folder."), error);
        return;
    }

    link_ = RemoteSource::canonical(text);
    QDialog::accept();
}

QString RemoteLinkDialog::getLink(QWidget* parent, const QString& initial)
{
    RemoteLinkDialog dialog(initial, parent);
    return dialog.exec() == QDialog::Accepted ? dialog.link() : QString();
}

// ---------------------------------------------------------------------------
// RemoteBrowseDialog
// ---------------------------------------------------------------------------

RemoteBrowseDialog::RemoteBrowseDialog(Mode mode, const QString& title,
                                       const QString& filter, const QString& rootLink,
                                       QWidget* parent) :
    QDialog(parent),
    mode_(mode),
    backButton_(new QToolButton),
    upButton_(new QToolButton),
    location_(new QLabel),
    driveButton_(new QPushButton(tr("Different drive..."))),
    view_(new QTreeWidget),
    filterCombo_(new QComboBox),
    okButton_(nullptr)
{
    setWindowTitle(title.isEmpty() ? tr("Remote Drive") : title);
    resize(720, 480);

    backButton_->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    backButton_->setToolTip(tr("Back"));
    upButton_->setIcon(style()->standardIcon(QStyle::SP_FileDialogToParent));
    upButton_->setToolTip(tr("Parent folder"));
    driveButton_->setToolTip(tr("Dropbox or Google Drive link"));
    location_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto top = new QHBoxLayout;
    top->addWidget(backButton_);
    top->addWidget(upButton_);
    top->addWidget(location_, 1);
    top->addWidget(driveButton_);

    view_->setColumnCount(3);
    view_->setHeaderLabels({tr("Name"), tr("Date modified"), tr("Size")});
    view_->setRootIsDecorated(false);
    view_->setUniformRowHeights(true);
    view_->setSortingEnabled(false);
    view_->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    // The caller's own filters, as the file dialog would offer them
    const auto filters = filter.split(QStringLiteral(";;"), Qt::SkipEmptyParts);
    filterCombo_->addItems(filters.isEmpty() ? QStringList{tr("All Files (*.*)")} : filters);
    filterCombo_->setVisible(mode_ == Mode::File);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    okButton_ = buttons->button(QDialogButtonBox::Ok);
    okButton_->setText(mode_ == Mode::Directory ? tr("Select Folder") : tr("Open"));

    auto bottom = new QHBoxLayout;
    bottom->addWidget(filterCombo_, 1);
    bottom->addWidget(buttons);

    auto layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(view_, 1);
    layout->addLayout(bottom);

    connect(backButton_, &QToolButton::clicked, this, &RemoteBrowseDialog::goBack);
    connect(upButton_, &QToolButton::clicked, this, &RemoteBrowseDialog::goUp);
    connect(driveButton_, &QPushButton::clicked, this, &RemoteBrowseDialog::differentDrive);
    connect(view_, &QTreeWidget::itemActivated, this, &RemoteBrowseDialog::itemActivated);
    connect(filterCombo_, &QComboBox::currentIndexChanged, this, &RemoteBrowseDialog::refill);
    connect(buttons, &QDialogButtonBox::accepted, this, &RemoteBrowseDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &RemoteBrowseDialog::reject);

    setRoot(rootLink);
}

void RemoteBrowseDialog::setRoot(const QString& rootLink)
{
    root_ = RemoteSource::canonical(rootLink);
    history_.clear();
    load({{root_, QString()}});
}

bool RemoteBrowseDialog::load(const QList<Level>& path)
{
    view_->clear();
    auto loading = new QTreeWidgetItem(view_, {tr("Loading...")});
    loading->setFlags(Qt::NoItemFlags);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    QList<RemoteSource::Entry> entries;
    QString error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = RemoteSource::listFolder(path.last().link, &entries, &error);
    QApplication::restoreOverrideCursor();

    if (!ok)
    {
        view_->clear();
        auto failed = new QTreeWidgetItem(view_, {tr("This folder could not be listed.")});
        failed->setFlags(Qt::NoItemFlags);
        RemoteSource::warn(this, tr("Could not open the folder on the shared drive."), error);
        return false;
    }

    path_ = path;
    entries_ = entries;
    // Folders first, then by name, as the file dialog sorts
    std::stable_sort(entries_.begin(), entries_.end(),
                     [](const RemoteSource::Entry& a, const RemoteSource::Entry& b) {
                         if (a.isDir != b.isDir) { return a.isDir; }
                         return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
                     });
    refill();
    return true;
}

void RemoteBrowseDialog::refill()
{
    view_->clear();
    QFileIconProvider icons;
    for (int i = 0; i < entries_.size(); ++i)
    {
        const auto& e = entries_.at(i);
        if (!e.isDir && (mode_ == Mode::Directory || !matchesFilter(e.name))) { continue; }
        auto item = new QTreeWidgetItem(view_);
        item->setText(0, e.name);
        item->setText(1, e.modified);
        if (e.size >= 0) { item->setText(2, QLocale().formattedDataSize(e.size)); }
        item->setIcon(0, icons.icon(e.isDir ? QFileIconProvider::Folder
                                            : QFileIconProvider::File));
        item->setData(0, Qt::UserRole, i);
    }

    const auto provider = RemoteSource::provider(root_) == RemoteSource::Provider::Dropbox
                              ? tr("Dropbox") : tr("Google Drive");
    location_->setText(QStringLiteral("<b>%1</b> / %2").arg(provider,
                                                             relPath().toHtmlEscaped()));
    location_->setToolTip(path_.isEmpty() ? QString() : path_.last().link);
    upButton_->setEnabled(path_.size() > 1);
    backButton_->setEnabled(!history_.isEmpty());
}

bool RemoteBrowseDialog::matchesFilter(const QString& name) const
{
    static const QRegularExpression inParens(QStringLiteral("\\(([^)]*)\\)"));
    const auto m = inParens.match(filterCombo_->currentText());
    if (!m.hasMatch()) { return true; }
    for (const auto& glob : m.captured(1).split(QLatin1Char(' '), Qt::SkipEmptyParts))
    {
        if (glob == QLatin1String("*.*") || glob == QLatin1String("*")) { return true; }
        const QRegularExpression re(QRegularExpression::wildcardToRegularExpression(glob),
                                    QRegularExpression::CaseInsensitiveOption);
        if (re.match(name).hasMatch()) { return true; }
    }
    return false;
}

QString RemoteBrowseDialog::relPath(const QString& leaf) const
{
    QStringList parts;
    for (int i = 1; i < path_.size(); ++i) { parts << path_.at(i).name; }
    if (!leaf.isEmpty()) { parts << leaf; }
    return parts.join(QLatin1Char('/'));
}

void RemoteBrowseDialog::itemActivated(QTreeWidgetItem* item)
{
    const auto index = item->data(0, Qt::UserRole);
    if (!index.isValid()) { return; }
    const auto& e = entries_.at(index.toInt());
    if (e.isDir)
    {
        auto next = path_;
        next.append({e.link, e.name});
        const auto previous = path_;
        if (load(next)) { history_.append(previous); }
    }
    else
    {
        accept();
    }
}

void RemoteBrowseDialog::goUp()
{
    if (path_.size() <= 1) { return; }
    const auto previous = path_;
    if (load(path_.mid(0, path_.size() - 1))) { history_.append(previous); }
}

void RemoteBrowseDialog::goBack()
{
    if (history_.isEmpty()) { return; }
    const auto previous = history_.takeLast();
    load(previous);
    backButton_->setEnabled(!history_.isEmpty());
}

void RemoteBrowseDialog::differentDrive()
{
    const auto link = RemoteLinkDialog::getLink(this);
    if (link.isEmpty()) { return; }
    RemoteSource::setCurrentDrive(link);
    setRoot(link);
}

void RemoteBrowseDialog::openAt(const QString& relPath)
{
    auto path = path_;
    for (const auto& name : relPath.split(QLatin1Char('/'), Qt::SkipEmptyParts))
    {
        const auto it = std::find_if(entries_.cbegin(), entries_.cend(),
                                     [&name](const RemoteSource::Entry& e) {
                                         return e.isDir && e.name == name;
                                     });
        if (it == entries_.cend()) { return; }
        path.append({it->link, it->name});
        if (!load(path)) { return; }
    }
}

void RemoteBrowseDialog::accept()
{
    const auto item = view_->currentItem();
    const auto index = item ? item->data(0, Qt::UserRole) : QVariant();
    const RemoteSource::Entry* chosen = index.isValid() ? &entries_.at(index.toInt()) : nullptr;

    if (mode_ == Mode::File)
    {
        if (!chosen || chosen->isDir)
        {
            if (chosen) { itemActivated(item); }
            return;
        }
        selected_ = RemoteSource::withLocation(chosen->link, root_, relPath(chosen->name));
    }
    else if (chosen && chosen->isDir)
    {
        selected_ = RemoteSource::withLocation(chosen->link, root_, relPath(chosen->name));
    }
    else
    {
        if (path_.isEmpty()) { return; }
        selected_ = RemoteSource::withLocation(path_.last().link, root_, relPath());
    }
    RemoteSource::setCurrentDrive(root_);
    QDialog::accept();
}

QString RemoteBrowseDialog::pick(QWidget* parent, Mode mode, const QString& title,
                                 const QString& filter, const QString& currentValue)
{
    // Start where the field already points, on its own drive
    QString root;
    QString rel;
    if (RemoteSource::isRemote(currentValue) && !RemoteSource::rootOf(currentValue).isEmpty())
    {
        root = RemoteSource::rootOf(currentValue);
        rel = RemoteSource::relPathOf(currentValue);
        if (mode == Mode::File) { rel = rel.section(QLatin1Char('/'), 0, -2); }
    }
    if (root.isEmpty()) { root = RemoteSource::currentDrive(); }
    if (root.isEmpty())
    {
        root = RemoteLinkDialog::getLink(parent);
        if (root.isEmpty()) { return {}; }
        RemoteSource::setCurrentDrive(root);
    }

    RemoteBrowseDialog dialog(mode, title, filter, root, parent);
    if (!rel.isEmpty()) { dialog.openAt(rel); }
    return dialog.exec() == QDialog::Accepted ? dialog.selected() : QString();
}
