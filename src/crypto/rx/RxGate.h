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

#ifndef XMRIG_RXGATE_H
#define XMRIG_RXGATE_H


#include "base/crypto/Algorithm.h"


#include <atomic>


namespace xmrig {


// The RandomX variant parameters (instruction frequencies, AES keys, JIT tables) are process
// wide. With two datasets kept in memory (e.g. rx/epic + rx/0) the miner switches variants
// every few seconds, so the switch must happen while no CPU thread is hashing:
//   worker:   enter(algo) ... hash ... leave()   (enter fails while locked or another variant is applied)
//   switcher: apply(algo) = lock (wait until no thread hashes), RxAlgo::apply, unlock
class RxGate
{
public:
    static bool enter(const Algorithm &algorithm);
    static void leave();

    static Algorithm::Id applied();

    // Makes `algorithm` the applied variant. Returns false (and changes nothing) while a dataset
    // is being initialized: the init thread holds the gate and the caller retries on dataset ready.
    static bool apply(const Algorithm &algorithm);

    // Exclusive section for the dataset init thread (init applies the variant of its seed).
    static void lock();
    static void unlock(const Algorithm &applied);

private:
    static std::atomic<int> m_active;
    static std::atomic<int> m_applied;
    static std::atomic<bool> m_locked;
};


} /* namespace xmrig */


#endif /* XMRIG_RXGATE_H */
