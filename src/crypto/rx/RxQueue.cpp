/* XMRig
 * Copyright (c) 2018      Lee Clagett <https://github.com/vtnerd>
 * Copyright (c) 2018-2019 tevador     <tevador@gmail.com>
 * Copyright (c) 2018-2021 SChernykh   <https://github.com/SChernykh>
 * Copyright (c) 2016-2021 XMRig       <https://github.com/xmrig>, <support@xmrig.com>
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

#include "crypto/rx/RxQueue.h"
#include "backend/common/interfaces/IRxListener.h"
#include "base/io/Async.h"
#include "base/io/log/Log.h"
#include "base/io/log/Tags.h"
#include "base/tools/Cvt.h"
#include "base/tools/Chrono.h"
#include "crypto/rx/RxAlgo.h"
#include "crypto/rx/RxBasicStorage.h"
#include "crypto/rx/RxGate.h"


#ifdef XMRIG_FEATURE_HWLOC
#   include "crypto/rx/RxNUMAStorage.h"
#endif


xmrig::RxQueue::RxQueue(IRxListener *listener) :
    m_listener(listener)
{
    m_async  = std::make_shared<Async>(this);
    m_thread = std::thread(&RxQueue::backgroundInit, this);
}


xmrig::RxQueue::~RxQueue()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_state = STATE_SHUTDOWN;
    lock.unlock();

    m_cv.notify_one();

    m_thread.join();

    for (auto &slot : m_slots) {
        delete slot.storage;
    }
}


xmrig::RxDataset *xmrig::RxQueue::dataset(const Job &job, uint32_t nodeId)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    const int index = findReady(job);
    if (index < 0) {
        return nullptr;
    }

    auto &slot = m_slots[index];
    slot.used  = Chrono::steadyMSecs();

    return slot.storage->dataset(job, nodeId);
}


xmrig::HugePagesInfo xmrig::RxQueue::hugePages()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    HugePagesInfo info;
    if (m_state != STATE_IDLE) {
        return info;
    }

    for (const auto &slot : m_slots) {
        if (slot.storage) {
            info += slot.storage->hugePages();
        }
    }

    return info;
}


template<typename T>
bool xmrig::RxQueue::isReady(const T &seed)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    return isReadyUnsafe(seed);
}


void xmrig::RxQueue::enqueue(const RxSeed &seed, const std::vector<uint32_t> &nodeset, uint32_t threads, bool hugePages, bool oneGbPages, RxConfig::Mode mode, int priority)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    // Ready, queued or being initialized right now.
    for (const auto &slot : m_slots) {
        if (slot.storage && slot.seed == seed) {
            return;
        }
    }

    const size_t index = chooseSlot(seed);
    auto &slot         = m_slots[index];

    if (!slot.storage) {
#       ifdef XMRIG_FEATURE_HWLOC
        if (!nodeset.empty()) {
            slot.storage = new RxNUMAStorage(nodeset);
        }
        else
#       endif
        {
            slot.storage = new RxBasicStorage();
        }
    }

    slot.ready = false;
    slot.seed  = seed;
    slot.used  = Chrono::steadyMSecs();

    // One request per slot: a newer seed for the same slot replaces the queued one.
    for (auto it = m_queue.begin(); it != m_queue.end();) {
        it = it->slot == index ? m_queue.erase(it) : it + 1;
    }

    m_queue.emplace_back(index, seed, nodeset, threads, hugePages, oneGbPages, mode, priority);
    m_state = STATE_PENDING;

    lock.unlock();

    m_cv.notify_one();
}


template<typename T>
bool xmrig::RxQueue::isReadyUnsafe(const T &seed) const
{
    return findReady(seed) >= 0;
}


template<typename T>
int xmrig::RxQueue::findReady(const T &seed) const
{
    for (size_t i = 0; i < kSlots; ++i) {
        const auto &slot = m_slots[i];
        if (slot.ready && slot.storage && slot.storage->isAllocated() && slot.seed == seed) {
            return static_cast<int>(i);
        }
    }

    return -1;
}


// Same variant -> the slot it already has (a new seed epoch replaces the old one), then an
// unused slot, then the least recently used one.
size_t xmrig::RxQueue::chooseSlot(const RxSeed &seed) const
{
    for (size_t i = 0; i < kSlots; ++i) {
        if (m_slots[i].storage && m_slots[i].seed.algorithm() == seed.algorithm()) {
            return i;
        }
    }

    for (size_t i = 0; i < kSlots; ++i) {
        if (!m_slots[i].storage) {
            return i;
        }
    }

    size_t lru = 0;
    for (size_t i = 1; i < kSlots; ++i) {
        if (m_slots[i].used < m_slots[lru].used) {
            lru = i;
        }
    }

    return lru;
}


void xmrig::RxQueue::backgroundInit()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    while (true) {
        m_cv.wait(lock, [this]{ return m_state == STATE_SHUTDOWN || !m_queue.empty(); });

        if (m_state == STATE_SHUTDOWN) {
            break;
        }

        const RxQueueItem item = m_queue.front();
        m_queue.erase(m_queue.begin());
        IRxStorage *storage = m_slots[item.slot].storage;

        lock.unlock();

        LOG_INFO("%s" MAGENTA_BOLD("init dataset%s") " algo " WHITE_BOLD("%s (") CYAN_BOLD("%u") WHITE_BOLD(" threads)") BLACK_BOLD(" seed %s..."),
                 Tags::randomx(),
                 item.nodeset.size() > 1 ? "s" : "",
                 item.seed.algorithm().name(),
                 item.threads,
                 Cvt::toHex(item.seed.data().data(), 8).data()
                 );

        // The variant parameters are global: no CPU thread may hash while they change.
        RxGate::lock();
        RxAlgo::apply(item.seed.algorithm());
        storage->init(item.seed, item.threads, item.hugePages, item.oneGbPages, item.mode, item.priority);
        RxGate::unlock(item.seed.algorithm());

        lock.lock();

        if (m_state == STATE_SHUTDOWN) {
            break;
        }

        // A newer request for this slot is queued: this dataset is already outdated.
        bool superseded = false;
        for (const auto &queued : m_queue) {
            superseded = superseded || queued.slot == item.slot;
        }

        if (!superseded && m_slots[item.slot].seed == item.seed) {
            m_slots[item.slot].ready = true;
        }

        if (m_queue.empty()) {
            m_state = STATE_IDLE;
            m_async->send();
        }
    }
}


void xmrig::RxQueue::onReady()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    const bool ready = m_listener && m_state == STATE_IDLE;
    lock.unlock();

    if (ready) {
        m_listener->onDatasetReady();
    }
}


namespace xmrig {


template bool RxQueue::isReady(const Job &);
template bool RxQueue::isReady(const RxSeed &);


} // namespace xmrig
