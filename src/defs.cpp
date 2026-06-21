/***************************************************************************
  defs.cpp
  -------------------
  Copyright © 2014-2018, LI-COR Biosciences, Antonio Forgione
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

#include "defs.h"

#include <QMetaType>

// debug helper to show Defs::RawFileType enum values
QDebug Defs::operator<<(QDebug dbg, const RawFileType& fileType)
{
    switch (fileType) {
    case RawFileType::GHG:
        dbg << "Defs::RawFileType::GHG";
        break;
    case RawFileType::ASCII:
        dbg << "Defs::RawFileType::ASCII";
        break;
    case RawFileType::BIN:
        dbg << "Defs::RawFileType::BIN";
        break;
    case RawFileType::TOB1:
        dbg << "Defs::RawFileType::TOB1";
        break;
    case RawFileType::SLT1:
        dbg << "Defs::RawFileType::SLT1";
        break;
    case RawFileType::SLT2:
        dbg << "Defs::RawFileType::SLT2";
        break;
//    default:
//        dbg << "Defs::RawFileType(" << static_cast<int>(fileType) << ')';
//        break;
    }
    return dbg;
}

// NOTE: necessary if streaming directly Defs::RawFileType in QSettings
// QDataStream& Defs::operator<<(QDataStream& out, const RawFileType& fileType)
// {
//     out << static_cast<int>(fileType);
//     return out;
// }

// NOTE: necessary if streaming directly Defs::RawFileType in QSettings
// QDataStream& Defs::operator>>(QDataStream& in, RawFileType& fileType)
// {
//     int ft;
//     in >> ft;
//     fileType = static_cast<Defs::RawFileType>(ft);
//     return in;
// }

void Defs::qt_registerCustomTypes()
{
    qRegisterMetaType<Defs::RawFileType>("Defs::RawFileType");

    // NOTE: necessary if streaming directly Defs::RawFileType in QSettings
//    qRegisterMetaTypeStreamOperators<Defs::RawFileType>("Defs::RawFileType");
}


