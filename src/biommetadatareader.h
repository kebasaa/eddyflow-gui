/***************************************************************************
  biometmetadatareader.h
  -------------------
  Copyright © 2013-2018, LI-COR Biosciences, Antonio Forgione
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

#ifndef BIOMMETADATAREADER_H
#define BIOMMETADATAREADER_H

#include <QList>
#include <QStringList>

#include "biomitem.h"

class BiomMetadataReader
{
public:
    explicit BiomMetadataReader(QList<BiomItem>* biomMetadata);

    bool readEmbMetadata(const QString& fileName);
    bool readAltMetadata(const QString& fileName);

    static const QString getVAR_TA();
    static const QString getVAR_PA();
    static const QString getVAR_RH();
    static const QString getVAR_RG();
    static const QString getVAR_LWIN();
    static const QString getVAR_PPFD();

private:
    int countEmbVariables(const QStringList& list);

    QList<BiomItem>* biomMetadata_;
};

#endif // BIOMMETADATAREADER_H
