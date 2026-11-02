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

#include "crypto/rx/RxGate.h"
#include "crypto/common/Nonce.h"
#include "crypto/rx/RxAlgo.h"


#include <chrono>
#include <mutex>
#include <thread>


namespace xmrig {


std::atomic<int> RxGate::m_active(0);
std::atomic<int> RxGate::m_applied(Algorithm::INVALID);
std::atomic<bool> RxGate::m_locked(false);

static std::mutex lockMutex;


} // namespace xmrig


bool xmrig::RxGate::enter(const Algorithm &algorithm)
{
    m_active.fetch_add(1);

    if (!m_locked.load() && m_applied.load() == algorithm.id()) {
        return true;
    }

    m_active.fetch_sub(1);

    return false;
}


void xmrig::RxGate::leave()
{
    m_active.fetch_sub(1);
}


xmrig::Algorithm::Id xmrig::RxGate::applied()
{
    return static_cast<Algorithm::Id>(m_applied.load());
}


bool xmrig::RxGate::apply(const Algorithm &algorithm)
{
    if (m_applied.load() == algorithm.id()) {
        return true;
    }

    if (!lockMutex.try_lock()) {
        return false;
    }

    m_locked = true;
    Nonce::touchRunning(Nonce::CPU);

    while (m_active.load() > 0) {
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }

    RxAlgo::apply(algorithm.id());

    m_applied = algorithm.id();
    m_locked  = false;

    lockMutex.unlock();

    return true;
}


void xmrig::RxGate::lock()
{
    lockMutex.lock();

    m_locked = true;
    Nonce::touchRunning(Nonce::CPU);

    while (m_active.load() > 0) {
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
}


void xmrig::RxGate::unlock(const Algorithm &applied)
{
    m_applied = applied.id();
    m_locked  = false;

    lockMutex.unlock();
}
