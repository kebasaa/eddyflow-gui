/***************************************************************************
  inputfilesdialog.cpp
  --------------------
  Copyright (C) 2026, ETH Zurich, Jonathan Muller
****************************************************************************/

#include "inputfilesdialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "filebrowsewidget.h"
#include "inputfilecheck.h"
#include "remotesource.h"
#include "widget_utils.h"

namespace
{
    //> Amber, readable on the light and the dark style alike: "look at this",
    //> not "this is wrong".
    const char* ChangedColor = "#c07000";
    const char* MissingColor = "#c62828";
    const char* ChangedFieldStyle =
        "QLineEdit { background-color: rgba(255, 176, 32, 60); }";
}

InputFilesDialog::InputFilesDialog(QWidget* parent, const Line (&lines)[KindCount]) :
    QDialog(parent)
{
    for (int k = 0; k < KindCount; ++k) { lines_[k] = lines[k]; }

    setWindowTitle(tr("Input Files Not Found"));
    setWindowModality(Qt::WindowModal);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setMinimumWidth(640);

    auto intro = new QLabel(tr("Some files this project uses no longer exist "
        "where it says they are. Choose them again, or accept the suggested "
        "ones, before the run starts. A path that differs from the one saved "
        "in the project is marked, and the project is saved with it when you "
        "continue."), this);
    intro->setWordWrap(true);

    const QString labels[KindCount] = {
        tr("Planar fit file:"),
        tr("Time lag file:"),
        tr("Spectral assessment file:")
    };
    //> The same title, filter and remembered folder as the page that owns
    //> each file, so these pickers are those pickers.
    const QString titles[KindCount] = {
        tr("Select the Planar Fit File"),
        tr("Select the Time Lag File"),
        tr("Select the Spectral Assessment File")
    };
    const QString hintKeys[KindCount] = {
        QStringLiteral("planar_fit_file"),
        QStringLiteral("timelag_file"),
        QStringLiteral("spectral_assessment_file")
    };

    auto grid = new QGridLayout;
    grid->setColumnStretch(1, 1);
    grid->setVerticalSpacing(2);

    for (int k = 0; k < KindCount; ++k)
    {
        auto label = new QLabel(labels[k], this);

        auto browse = new FileBrowseWidget;
        browse->setRemoteBrowseEnabled(true);
        browse->setDialogTitle(titles[k]);
        browse->setDialogWorkingDir(WidgetUtils::getDialogPathHint(hintKeys[k]));
        browse->setDialogFilter(tr("All Files (*.*)"));
        browse_[k] = browse;

        auto status = new QLabel(this);
        status->setWordWrap(true);
        status->setTextFormat(Qt::RichText);
        status->setContentsMargins(0, 0, 0, 8);
        status_[k] = status;

        grid->addWidget(label, 2 * k, 0, Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(browse, 2 * k, 1);
        grid->addWidget(status, 2 * k + 1, 1);

        const QString start = lines_[k].suggestion.isEmpty() ? lines_[k].saved
                                                             : lines_[k].suggestion;
        //> A path that is gone shows blank: setPath canonicalises, and there
        //> is nothing to canonicalise. Its folder is still the browse hint.
        browse->setPath(start);
        if (!lines_[k].suggestion.isEmpty())
        {
            browse->setDialogWorkingDir(QFileInfo(lines_[k].suggestion).absolutePath());
        }

        label->setEnabled(lines_[k].applies);
        browse->setEnabled(lines_[k].applies);

        const auto kind = static_cast<Kind>(k);
        connect(browse, &FileBrowseWidget::pathSelected, this,
                [this, kind](const QString& p)
                {
                    browse_[kind]->setPath(p);
                    updateLine(kind);
                    updateContinue();
                });
        connect(browse, &FileBrowseWidget::pathChanged, this,
                [this, kind](const QString&)
                {
                    updateLine(kind);
                    updateContinue();
                });
    }

    buttons_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons_->button(QDialogButtonBox::Ok)->setText(tr("Continue"));
    buttons_->button(QDialogButtonBox::Cancel)->setText(tr("Cancel Run"));
    connect(buttons_, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(intro);
    mainLayout->addSpacing(8);
    mainLayout->addLayout(grid);
    mainLayout->addStretch();
    mainLayout->addWidget(buttons_);
    setLayout(mainLayout);

    for (int k = 0; k < KindCount; ++k) { updateLine(static_cast<Kind>(k)); }
    updateContinue();
}

QString InputFilesDialog::path(Kind kind) const
{
    return browse_[kind]->path().trimmed();
}

bool InputFilesDialog::changed(Kind kind) const
{
    const QString now = path(kind);
    if (now.isEmpty()) { return false; }
    return !InputFileCheck::samePath(RemoteSource::cleanPath(now),
                                     RemoteSource::cleanPath(lines_[kind].saved));
}

void InputFilesDialog::updateLine(Kind kind)
{
    const auto& line = lines_[kind];
    auto status = status_[kind];
    const QString saved = QDir::toNativeSeparators(line.saved).toHtmlEscaped();

    if (!line.applies)
    {
        browse_[kind]->setStyleSheet(QString());
        status->setText(tr("<i>Not used with the current settings.</i>"));
        return;
    }

    const QString now = path(kind);
    const bool savedGone = !InputFileCheck::usable(line.saved);
    const bool isChanged = changed(kind);
    browse_[kind]->setStyleSheet(isChanged ? QString::fromLatin1(ChangedFieldStyle)
                                           : QString());

    QString text;
    if (savedGone)
    {
        text = QStringLiteral("<span style=\"color:%1;\">%2</span> %3")
                   .arg(QLatin1String(MissingColor),
                        tr("Not found:"), saved);
    }

    if (isChanged)
    {
        const bool isSuggestion = !line.suggestion.isEmpty()
            && InputFileCheck::samePath(now, line.suggestion);
        const QString what = isSuggestion
            ? tr("Suggested: a similar file found in %1.")
                  .arg(QDir::toNativeSeparators(QFileInfo(line.suggestion).absolutePath())
                           .toHtmlEscaped())
            : tr("Chosen here.");
        if (!text.isEmpty()) { text += QStringLiteral("<br />"); }
        text += QStringLiteral("<b><span style=\"color:%1;\">%2</span></b> %3")
                    .arg(QLatin1String(ChangedColor),
                         tr("Changed from the project's saved path."), what);
        if (!InputFileCheck::usable(now))
        {
            text += QStringLiteral("<br /><span style=\"color:%1;\">%2</span>")
                        .arg(QLatin1String(MissingColor),
                             tr("The chosen file does not exist either."));
        }
    }
    else if (savedGone)
    {
        text += QStringLiteral("<br />") + tr("No similar file was found nearby. "
            "Choose one with Browse or Remote drive.");
    }
    else
    {
        text = tr("Found.");
    }
    status->setText(text);
}

void InputFilesDialog::updateContinue()
{
    bool ok = true;
    for (int k = 0; k < KindCount; ++k)
    {
        if (!lines_[k].applies) { continue; }
        if (!InputFileCheck::usable(path(static_cast<Kind>(k)))) { ok = false; }
    }
    buttons_->button(QDialogButtonBox::Ok)->setEnabled(ok);
}
