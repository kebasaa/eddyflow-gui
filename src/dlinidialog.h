/***************************************************************************
  dlinidialog.h
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
 ***************************************************************************/

#ifndef DLINIDIALOG_H
#define DLINIDIALOG_H

#include <QDialog>
#include <QFileDialog>

class QAbstractButton;
class QKeyEvent;
class QPushButton;
class QTabWidget;

struct ConfigState;
class DlInstrTab;
class DlProject;
class DlRawfileDescTab;
class DlSiteTab;

class DlIniDialog : public QDialog
{
    Q_OBJECT

public:
    DlIniDialog(QWidget *parent, DlProject *dlProject, ConfigState* config);

    ~DlIniDialog();

    inline QString iniFilename() { return filename_; }
    inline DlRawfileDescTab* rawFileDescTab() { return rawfile_tab; }
    inline DlProject *dlProject() { return dlProject_; }
    inline void setNewFlag(bool b) { newFlag_ = b; }
    bool requestMetadataReset();

    //> Hold the metadata in memory instead of writing it.
    //>
    //> Every other edit here autosaves: setModified(true) reaches
    //> saveAvailable() through DlProject::projectModified, and that calls
    //> apply(), which writes. That is deliberate - there is no Save button in
    //> this dialog because there does not need to be one - but it is wrong for
    //> an imported EddyPro project, where the file on disk belongs to EddyPro
    //> and the user has not agreed to anything yet. Between beginDeferredSave()
    //> and the first project save, nothing is written; commitDeferredSave()
    //> then writes once, to wherever the project went.
    void beginDeferredSave();
    inline bool hasDeferredSave() const { return deferredSave_; }
    bool commitDeferredSave(const QString& path);

signals:
    void metadataFileSaved(const QString &);
    void mdFileEditClearRequest();

public slots:
    bool openFile(const QString &fileName = QString(), bool embedded = true);
    void refresh();
    void reset();
    void defaults();
    void defaults_2();
    void saveAvailable();

private slots:
    void accept();
    void disableResetButton(bool ok);
    void saveAsButtonClicked();

private:
    void apply();
    bool saveFile(const QString& filename);
    void fileSaveAs();

    QTabWidget *tabwidget_;
    DlSiteTab *site_tab;
    DlInstrTab *instruments_tab;
    DlRawfileDescTab *rawfile_tab;

    QPushButton *saveAsButton;
    QPushButton *resetButton;

    QString filename_;
    bool newFlag_;
    bool deferredSave_;

    DlProject *dlProject_;
    ConfigState *configState_;
};

#endif // DLINIDIALOG_H
