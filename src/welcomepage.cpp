/***************************************************************************
  welcomepage.cpp
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

#include "welcomepage.h"

#include <QCheckBox>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QGridLayout>
#include <QListWidget>
#include <QIcon>
#include <QPixmap>
#include <QSize>
#include <QPushButton>
#include <QToolButton>
#include <QUrl>

#include "clicklabel.h"
#include "defs.h"
#include "ecproject.h"
#include "mainwidget.h"
#include "smartfluxbar.h"
#include "widget_utils.h"

WelcomePage::WelcomePage(QWidget *parent, EcProject *ecProject, ConfigState* configState) :
    QWidget(parent),
    ecProject_(ecProject),
    configState_(configState)
{
    appLogoLabel = new ClickLabel;
    appLogoLabel->setProperty("applogoLabel", true);
    appLogoLabel->setProperty("applogoSmallLabel", false);
    appLogoLabel->setPixmap(QPixmap(QStringLiteral(":/icons/app-logo-svg"))
                                .scaled(780, 139, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    appLogoLabel->setAlignment(Qt::AlignCenter);

    connect(appLogoLabel, &ClickLabel::clicked,
            this, &WidgetUtils::openAppWebsite);


    newButton = new QToolButton;
    newButton->setText(tr("New Project"));
    newButton->setObjectName(QStringLiteral("newButton"));
    newButton->setIcon(QPixmap(QStringLiteral(":/icons/new-big")));
    newButton->setIconSize(QSize(94, 103));
    newButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    openButton = new QToolButton;
    openButton->setText(tr("Open Project"));
    openButton->setObjectName(QStringLiteral("openButton"));
    openButton->setIcon(QPixmap(QStringLiteral(":/icons/open-big")));
    openButton->setIconSize(QSize(94, 103));
    openButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    auto mainButtonsLayout = new QHBoxLayout;
    mainButtonsLayout->addStretch(12);
    mainButtonsLayout->addWidget(newButton);
    mainButtonsLayout->addStretch(1);
    mainButtonsLayout->addWidget(openButton);
    mainButtonsLayout->addStretch(12);
    mainButtonsLayout->setContentsMargins(0, 0, 0, 0);
    mainButtonsLayout->setContentsMargins(0,0,0,0);

    auto recentListTitle = new QLabel(tr("Recent Projects"));
    recentListTitle->setProperty("openRecentGroupTitle", true);

    recentListWidget = new QListWidget;
    recentListWidget->setProperty("helpListItem", true);
    recentListWidget->setMinimumWidth(730);
    recentListWidget->setMinimumHeight(240);
    recentListWidget->setAlternatingRowColors(true);
    recentListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    recentListWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    updateRecentList();

    QString smartfluxTooltip = QStringLiteral("Use EddyFlow in "
                               "'SMARTFlux configuration' mode. <br>"
                               "This is an interface to customize "
                               "advanced processing options and create "
                               "the .smartflux configuration package, "
                               "which you will upload to the SMARTFlux "
                               "device for fully corrected, fully "
                               "customized fluxes on-the-fly out of "
                               "your LI-COR eddy-covariance system.");

    smartfluxModeCheckbox_ = new QCheckBox;
    smartfluxModeCheckbox_->setToolTip(smartfluxTooltip);

    auto title = new ClickLabel;
    title->setText(tr("LI-COR Smartflux Configuration"));
    title->setProperty("smartfluxConfigurationTitle", true);
    title->setToolTip(smartfluxTooltip);

    connect(title, &ClickLabel::clicked,
            smartfluxModeCheckbox_, &QCheckBox::toggle);

    createQuestionMark();

    auto smartfluxLabelContainerLayout = new QHBoxLayout;
    smartfluxLabelContainerLayout->addSpacing(100);
    smartfluxLabelContainerLayout->addWidget(smartfluxModeCheckbox_);
    smartfluxLabelContainerLayout->addSpacing(5);
    smartfluxLabelContainerLayout->addWidget(title, 0, Qt::AlignBottom);
    smartfluxLabelContainerLayout->addSpacing(5);
    smartfluxLabelContainerLayout->addWidget(questionMark_1);
    smartfluxLabelContainerLayout->addStretch();
    smartfluxLabelContainerLayout->setContentsMargins(0, 0, 0, 0);
    smartfluxLabelContainerLayout->setContentsMargins(0,0,0,0);

    auto projectsLayout = new QGridLayout;
    projectsLayout->addLayout(smartfluxLabelContainerLayout, 0, 1, 1, 1, Qt::AlignBottom);
    projectsLayout->addLayout(mainButtonsLayout, 1, 1, Qt::AlignCenter);
    projectsLayout->addWidget(recentListTitle, 2, 0, 1, -1, Qt::AlignLeft);
    projectsLayout->addWidget(recentListWidget, 3, 0, 1, -1, Qt::AlignCenter);
    projectsLayout->setRowStretch(4, 1);
    projectsLayout->setRowMinimumHeight(0, 40);

    auto projectsWidget = new QWidget;
    projectsWidget->setMinimumWidth(600);
    projectsWidget->setLayout(projectsLayout);

    auto helpWidget = new QWidget;

    auto helpTitle = new QLabel(tr("Help resources"));
    helpTitle->setProperty("groupTitle3", true);

    auto item_1 = new QListWidgetItem(tr("%1 Help").arg(Defs::APP_NAME));
    item_1->setData(Qt::UserRole, QStringLiteral("https://github.com/kebasaa/eddyflow-documentation"));
    auto item_2 = new QListWidgetItem(tr("Getting started"));
    item_2->setData(Qt::UserRole, QStringLiteral("https://github.com/kebasaa/eddyflow-documentation"));
    auto item_3 = new QListWidgetItem(tr("Download sample data files"));
    item_3->setData(Qt::UserRole, Defs::EP_SAMPLE_DATA_FILES);

    helpListWidget = new QListWidget;
    helpListWidget->setProperty("helpListItem", true);
    helpListWidget->setAlternatingRowColors(true);
    helpListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    helpListWidget->addItem(item_1);
    helpListWidget->addItem(item_2);
    helpListWidget->addItem(item_3);

    auto litTitle = new QLabel(tr("Literature"));
    litTitle->setProperty("groupTitle3", true);

    auto litItem_1 = new QListWidgetItem(tr("Conditional Eddy Covariance (Zahn et al. 2022)"));
    litItem_1->setData(Qt::UserRole, QStringLiteral("https://www.sciencedirect.com/science/article/pii/S0168192321004767"));

    litListWidget = new QListWidget;
    litListWidget->setProperty("helpListItem", true);
    litListWidget->setAlternatingRowColors(true);
    litListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    litListWidget->addItem(litItem_1);

    auto helpLayout = new QGridLayout;
    helpLayout->addWidget(helpTitle,      0, 0);
    helpLayout->addWidget(helpListWidget, 1, 0);
    helpLayout->addWidget(litTitle,       0, 1);
    helpLayout->addWidget(litListWidget,  1, 1);
    helpLayout->setRowMinimumHeight(0, 30);
    helpLayout->setColumnStretch(0, 1);
    helpLayout->setColumnStretch(1, 1);

    helpWidget->setLayout(helpLayout);
    helpWidget->setMinimumWidth(600);
//
    auto welcomeTab = new QTabWidget;
    welcomeTab->addTab(projectsWidget, tr("Manage Projects"));
    welcomeTab->addTab(helpWidget, tr("Help and Support"));

    smartfluxBar_ = new SmartFluxBar(ecProject_, configState_);

    smartfluxBarPlaceholder_ = new QWidget;
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    smartfluxBarPlaceholder_->setMinimumHeight(35);
#elif defined(Q_OS_MACOS)
    smartfluxBarPlaceholder_->setMinimumHeight(50);
#endif

    mainLayout_ = new QGridLayout(this);
    mainLayout_->addWidget(smartfluxBar_, 0, 0, 1, -1);
    mainLayout_->addWidget(smartfluxBarPlaceholder_, 0, 0, 1, -1, Qt::AlignHCenter);
    mainLayout_->addWidget(appLogoLabel, 1, 0, 1, -1, Qt::AlignHCenter | Qt::AlignVCenter);
    mainLayout_->addWidget(welcomeTab, 3, 0, 1, -1, Qt::AlignHCenter);
    mainLayout_->setRowStretch(0, 0);
    mainLayout_->setRowStretch(1, 1);
    mainLayout_->setRowStretch(2, 1);
    mainLayout_->setRowStretch(3, 5);
    mainLayout_->setContentsMargins(0, 0, 0, 0);
    mainLayout_->setRowMinimumHeight(0, 35);

    setLayout(mainLayout_);

    connect(recentListWidget, &QListWidget::itemClicked,
            this, &WelcomePage::recentOpenRequested, Qt::QueuedConnection);

    connect(recentListWidget, &QListWidget::itemDoubleClicked,
            this, &WelcomePage::doNothing);

    connect(helpListWidget, &QListWidget::itemClicked,
            this, &WelcomePage::helpItemRequested);

    connect(litListWidget, &QListWidget::itemClicked,
            this, &WelcomePage::literatureItemRequested);

    auto mainWidget = static_cast<MainWidget*>(parent);
    connect(mainWidget, &MainWidget::recentUpdated,
            this, &WelcomePage::updateRecentList);

    connect(openButton, &QToolButton::clicked,
            this, &WelcomePage::openProjectRequested);
    connect(newButton, &QToolButton::clicked,
            this, &WelcomePage::newProjectRequest);

    // routing signal to parent
    connect(smartfluxBar_, &SmartFluxBar::showSmartfluxBarRequest,
            mainWidget, &MainWidget::showSmartfluxBarRequest);

    connect(smartfluxBar_, &SmartFluxBar::saveSilentlyRequest,
            mainWidget, &MainWidget::saveSilentlyRequest);

    connect(smartfluxModeCheckbox_, &QCheckBox::toggled,
            mainWidget, &MainWidget::showSmartfluxBarRequest);

    connect(smartfluxBar_, &SmartFluxBar::saveRequest,
            mainWidget, &MainWidget::saveRequest);
}

WelcomePage::~WelcomePage()
{
}

void WelcomePage::openProjectRequested()
{
    emit openProjectRequest(QString());
}

void WelcomePage::recentOpenRequested(QListWidgetItem* item)
{
    emit openProjectRequest(item->text());
}

// NOTE: hack to avoid side effects opening a recent project with a double click
void WelcomePage::doNothing(QListWidgetItem* item)
{
    Q_UNUSED(item)

#ifdef QT_DEBUG
//    auto msgBox = new QMessageBox;
//    msgBox->setWindowTitle(QStringLiteral("Troppi click, Ger :-)"));
//    msgBox->setText(QStringLiteral("Detected Double Open"));
//    msgBox->show();
#endif
}

void WelcomePage::literatureItemRequested(QListWidgetItem* item)
{
    QDesktopServices::openUrl(QUrl(item->data(Qt::UserRole).toString()));
}

void WelcomePage::helpItemRequested(QListWidgetItem* item)
{
    if (item->text() == tr("Download sample data files"))
    {
        QDesktopServices::openUrl(QUrl(item->data(Qt::UserRole).toString()));
    }
    else
    {
        WidgetUtils::showHelp(QUrl(item->data(Qt::UserRole).toString()));
    }
}


void WelcomePage::updateRecentList()
{
    recentListWidget->clear();

    for (const auto &recentfile : configState_->general.recentfiles)
    {
        if (QFile::exists(recentfile))
        {
            auto item = new QListWidgetItem(QDir::toNativeSeparators(recentfile), recentListWidget);
            Q_UNUSED(item)
        }
    }
}

void WelcomePage::updateWelcomePage(bool small)
{
    QList<WidgetUtils::PropertyList> appLogoProp;

    if (small)
    {
        appLogoProp << WidgetUtils::PropertyList("applogoLabel", false)
                    << WidgetUtils::PropertyList("applogoSmallLabel", true);
        appLogoLabel->setPixmap(QPixmap(QStringLiteral(":/icons/app-logo-svg"))
                                    .scaled(390, 70, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    else
    {
        appLogoProp << WidgetUtils::PropertyList("applogoLabel", true)
                    << WidgetUtils::PropertyList("applogoSmallLabel", false);
        appLogoLabel->setPixmap(QPixmap(QStringLiteral(":/icons/app-logo-svg"))
                                    .scaled(780, 139, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    WidgetUtils::updatePropertyListAndStyle(appLogoLabel, appLogoProp);
}

void WelcomePage::updateSmartfluxBar()
{
    smartfluxBar_->setVisible(configState_->project.smartfluxMode);
}

void WelcomePage::updateSmartfluxCheckBox()
{
    smartfluxModeCheckbox_->blockSignals(true);
    smartfluxModeCheckbox_->setChecked(configState_->project.smartfluxMode);
    smartfluxModeCheckbox_->blockSignals(false);
}

void WelcomePage::createQuestionMark()
{
    questionMark_1 = new QPushButton;
    questionMark_1->setObjectName(QStringLiteral("questionMarkImg"));
    questionMark_1->setFlat(true);
    questionMark_1->setIcon(QIcon(QStringLiteral(":/icons/qm-enabled")));
    questionMark_1->setIconSize(QSize(12, 12));
    questionMark_1->setProperty("smartfluxQuestionMark", true);

    connect(questionMark_1, &QPushButton::clicked,
            this, &WelcomePage::onlineHelpTrigger_1);
}

void WelcomePage::onlineHelpTrigger_1()
{
    WidgetUtils::showHelp(QUrl(QStringLiteral("https://keba_saa.github.io/eddyflow-documentation/topics_EddyFlow/smartfluxSettings.html")));
}

