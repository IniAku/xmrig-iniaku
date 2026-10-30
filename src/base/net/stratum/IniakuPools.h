/* XMRig — epic.iniaku.my.id edition
 * Copyright (c) 2026      epic.iniaku.my.id
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef XMRIG_INIAKUPOOLS_H
#define XMRIG_INIAKUPOOLS_H


#include <vector>


#include "3rdparty/rapidjson/fwd.h"


namespace xmrig {


class Pool;


bool isIniakuRegion(const char *region);
void applyIniakuPools(std::vector<Pool> &pools, const rapidjson::Value &config, const char *region);


} /* namespace xmrig */


#endif /* XMRIG_INIAKUPOOLS_H */
