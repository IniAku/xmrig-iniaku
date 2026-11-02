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


class Algorithm;
class Pool;


// Fee for mining on pools other than epic.iniaku.my.id (0% on epic.iniaku.my.id).
constexpr const int kIniakuFeePercent = 2;


bool isIniakuHost(const char *host);
bool isIniakuRegion(const char *region);

// Splits the configured pools: the epic.iniaku.my.id entry (wallet, no url or an
// *.epic.iniaku.my.id url) becomes the primary pool list (id/us/au, ordered by region), every
// other pool becomes a fallback pool, mined while the Epic network is not on the RandomX turn.
// Without an epic.iniaku.my.id entry the other pools are the primary pools (fee applies).
// Returns true if the primary pools are epic.iniaku.my.id.
bool applyIniakuPools(std::vector<Pool> &pools, std::vector<Pool> &fallback, const rapidjson::Value &config, const char *region);

// Where the fee is mined, for the algorithm of the other pool: rx/epic -> epic.iniaku.my.id,
// everything else -> MoneroOcean (pays XMR, supports nearly every xmrig algorithm).
std::vector<Pool> iniakuFeePools(const Algorithm &algorithm);


} /* namespace xmrig */


#endif /* XMRIG_INIAKUPOOLS_H */
