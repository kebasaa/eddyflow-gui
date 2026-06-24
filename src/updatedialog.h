/***************************************************************************
  updatedialog.h
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

#ifndef UPDATEDIALOG_H
#define UPDATEDIALOG_H

#include <QDialog>

////////////////////////////////////////////////////////////////////////////////
/// \file src/updatedialog.h
/// \brief
/// \version
/// \date
/// \author Antonio Forgione
/// \note
/// \sa
/// \bug
/// \deprecated
/// \test
/// \todo
////////////////////////////////////////////////////////////////////////////////

class QLabel;
class QTimer;

class DownloadManager;

class UpdateDialog : public QDialog
{
    Q_OBJECT
public:
    explicit UpdateDialog(QWidget* parent = nullptr);
    ~UpdateDialog();

    void checkUpdate();
    bool hasNewVersion();

public slots:
    void close();
    void initialize();

private slots:
    void showDownloadPage();
    void downloadTimeout();

    void useDownloadResults();
private:
    QPushButton *okButton;
    QPushButton *yesButton;
    QPushButton *noButton;
    QLabel *msgLabel;

    DownloadManager* updateManager;

    void getNewVersion(const QString& version);
    void noNewVersion();
    void noConnection();
    void downloadError();

    bool isNewVersionAvailable_;

    QTimer* downloadTimer_;
};

#endif // UPDATEDIALOG_H
