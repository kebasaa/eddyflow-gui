/***************************************************************************
  container_helpers.h
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

#ifndef CONTAINER_HELPERS_H
#define CONTAINER_HELPERS_H

#include <algorithm>

namespace ContainerHelper {

// wrapper around std::equal to compare two containers on the same range
// with a cleaner call
template<typename T>
bool rangeEqual(const T& s1, const T& s2, int first, int last)
{
    return std::equal(s1.begin() + first, s1.begin() + last, s2.begin() + first);
}

}  // namespace ContainerHelper

#endif  // CONTAINER_HELPERS_H
