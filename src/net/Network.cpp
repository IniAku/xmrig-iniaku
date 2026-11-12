/* XMRig
 * Copyright (c) 2019      Howard Chu  <https://github.com/hyc>
 * Copyright (c) 2018-2023 SChernykh   <https://github.com/SChernykh>
 * Copyright (c) 2016-2023 XMRig       <https://github.com/xmrig>, <support@xmrig.com>
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

#ifdef _MSC_VER
#pragma warning(disable:4244)
#endif

#include "net/Network.h"
#include "3rdparty/rapidjson/document.h"
#include "backend/common/Tags.h"
#include "base/io/log/Log.h"
#include "base/io/log/Tags.h"
#include "base/net/stratum/Client.h"
#include "base/net/stratum/IniakuPools.h"
#include "base/net/stratum/Pools.h"
#include "base/net/stratum/NetworkState.h"
#include "base/net/stratum/SubmitResult.h"
#include "base/tools/Chrono.h"
#include "base/tools/Timer.h"
#include "core/config/Config.h"
#include "core/Controller.h"
#include "core/Miner.h"
#ifdef XMRIG_ALGO_RANDOMX
#   include "crypto/rx/Rx.h"
#endif
#include "net/JobResult.h"
#include "net/JobResults.h"
#include "net/strategies/DonateStrategy.h"


#ifdef XMRIG_FEATURE_API
#   include "base/api/Api.h"
#   include "base/api/interfaces/IApiRequest.h"
#endif


#ifdef XMRIG_FEATURE_BENCHMARK
#   include "backend/common/benchmark/BenchState.h"
#endif


#include <algorithm>
#include <cinttypes>
#include <ctime>
#include <iterator>
#include <memory>


xmrig::Network::Network(Controller *controller) :
    m_controller(controller)
{
    JobResults::setListener(this, controller->config()->cpu().isHwAES());
    controller->addListener(this);

#   ifdef XMRIG_FEATURE_API
    controller->api()->addListener(this);
#   endif

    m_state = new NetworkState(this);

    const Pools &pools = controller->config()->pools();
    m_strategy = pools.createStrategy(m_state);
    m_fallback = pools.createFallbackStrategy(m_state);
    m_iniaku   = pools.isIniaku();

    if (pools.donateLevel() > 0) {
        m_donate = new DonateStrategy(controller, this);
    }

    // Spread the first fee slice so that miners started together don't switch together.
    m_otherTime = static_cast<uint64_t>(Chrono::steadyMSecs() * 7919) % kFeeEvery;
    m_lastTick  = Chrono::steadyMSecs();
    m_timer     = new Timer(this, kTickInterval, kTickInterval);
}


xmrig::Network::~Network()
{
    JobResults::stop();

    delete m_timer;
    delete m_donate;
    delete m_fee;
    delete m_fallback;
    delete m_strategy;
    delete m_state;
}


void xmrig::Network::connect()
{
    m_strategy->connect();

    if (m_fallback) {
        m_fallback->connect();
    }
}


void xmrig::Network::execCommand(char command)
{
    switch (command) {
    case 's':
    case 'S':
        m_state->printResults();
        break;

    case 'c':
    case 'C':
        m_state->printConnection();
        break;

    default:
        break;
    }
}


void xmrig::Network::onActive(IStrategy *strategy, IClient *client)
{
    if (m_donate && m_donate == strategy) {
        LOG_NOTICE("%s " WHITE_BOLD("dev donate started"), Tags::network());
        return;
    }

    if (strategy == m_fee) {
        return;
    }

    const auto &pool = client->pool();

#   ifdef XMRIG_FEATURE_BENCHMARK
    if (pool.mode() == Pool::MODE_BENCHMARK) {
        return;
    }
#   endif

    char zmq_buf[32] = {};
    if (client->pool().zmq_port() >= 0) {
        snprintf(zmq_buf, sizeof(zmq_buf), " (ZMQ:%d)", client->pool().zmq_port());
    }

    const char *tlsVersion = client->tlsVersion();
    LOG_INFO("%s " WHITE_BOLD("use %s%s ") CYAN_BOLD("%s:%d%s ") GREEN_BOLD("%s") " " BLACK_BOLD("%s"),
             Tags::network(), strategy == m_fallback ? "fallback " : "", client->mode(), pool.host().data(), pool.port(), zmq_buf, tlsVersion ? tlsVersion : "", client->ip().data());

    const char *fingerprint = client->tlsFingerprint();
    if (fingerprint != nullptr) {
        LOG_INFO("%s " BLACK_BOLD("fingerprint (SHA-256): \"%s\""), Tags::network(), fingerprint);
    }
}


void xmrig::Network::onConfigChanged(Config *config, Config *previousConfig)
{
    if (config->pools() == previousConfig->pools() || !config->pools().active()) {
        return;
    }

    m_strategy->stop();
    if (m_fallback) {
        m_fallback->stop();
    }
    if (m_fee) {
        m_fee->stop();
    }

    config->pools().print();

    delete m_strategy;
    delete m_fallback;
    delete m_fee;

    m_fee         = nullptr;
    m_feeAlgo     = Algorithm();
    m_primaryJob  = Job();
    m_fallbackJob = Job();
    m_feeJob      = Job();
    m_source      = SOURCE_NONE;

    m_strategy = config->pools().createStrategy(m_state);
    m_fallback = config->pools().createFallbackStrategy(m_state);
    m_iniaku   = config->pools().isIniaku();
    connect();
}


// Epic: the node hands out a job for the algorithm of the next block (RandomX, ProgPoW and
// Cuckatoo take turns); a RandomX share can only become a block while that is "randomx".
static bool isRandomXTurn(const xmrig::Job &job, const rapidjson::Value &params)
{
    if (job.algorithm() != xmrig::Algorithm::RX_EPIC || !params.IsObject()) {
        return true;
    }

    const auto i = params.FindMember("algorithm");

    return i == params.MemberEnd() || !i->value.IsString() || strcmp(i->value.GetString(), "randomx") == 0;
}


void xmrig::Network::onJob(IStrategy *strategy, IClient *client, const Job &job, const rapidjson::Value &params)
{
    if (m_donate && m_donate == strategy) {
        return setJob(client, job, true);
    }

    if (m_donate && m_donate->isActive()) {
        return;
    }

    const bool turn = isRandomXTurn(job, params);

    if (strategy == m_fee) {
        m_feeJob  = job;
        m_feeTurn = turn;
    }
    else if (strategy == m_fallback) {
        m_fallbackJob  = job;
        m_fallbackTurn = turn;
        ensureFee(job.algorithm());

#       ifdef XMRIG_ALGO_RANDOMX
        // Build the fallback's RandomX dataset now (second slot), so the first switch is instant.
        if (job.algorithm().family() == Algorithm::RANDOM_X) {
            Rx::init(job, m_controller->config()->rx(), m_controller->config()->cpu());
        }
#       endif
    }
    else {
        m_primaryJob  = job;
        m_primaryTurn = turn;

        if (!m_iniaku) {
            ensureFee(job.algorithm());
        }
    }

    select(strategy);
}


void xmrig::Network::onJobResult(const JobResult &result)
{
    if (result.index == 1) {
        if (m_fee && m_source == SOURCE_FEE) {
            m_fee->submit(result);
        }
        else if (m_donate) {
            m_donate->submit(result);
        }

        return;
    }

    if (m_fallback && result.algorithm == m_fallbackJob.algorithm() && result.clientId == m_fallbackJob.clientId()) {
        m_fallback->submit(result);
        return;
    }

    if (!m_fallback || result.algorithm == m_primaryJob.algorithm()) {
        m_strategy->submit(result);
    }
}


void xmrig::Network::onLogin(IStrategy *strategy, IClient *client, rapidjson::Document &doc, rapidjson::Value &params)
{
    using namespace rapidjson;
    auto &allocator = doc.GetAllocator();

    // Fee pool: only the algorithm of the other pool, so the fee needs no extra dataset/switch.
    if (strategy == m_fee && m_feeAlgo.isValid()) {
        Value algo(kArrayType);
        algo.PushBack(StringRef(m_feeAlgo.name()), allocator);
        params.AddMember("algo", algo, allocator);

        return;
    }

    Algorithms algorithms     = m_controller->miner()->algorithms();
    const Algorithm algorithm = client->pool().algorithm();
    if (algorithm.isValid()) {
        const size_t index = static_cast<size_t>(std::distance(algorithms.begin(), std::find(algorithms.begin(), algorithms.end(), algorithm)));
        if (index > 0 && index < algorithms.size()) {
            std::swap(algorithms[0], algorithms[index]);
        }
    }

    Value algo(kArrayType);

    for (const auto &a : algorithms) {
        algo.PushBack(StringRef(a.name()), allocator);
    }

    params.AddMember("algo", algo, allocator);
}


void xmrig::Network::onPause(IStrategy *strategy)
{
    if (m_donate && m_donate == strategy) {
        LOG_NOTICE("%s " WHITE_BOLD("dev donate finished"), Tags::network());
        m_strategy->resume();
    }

    if (strategy == m_fee) {
        m_feeJob = Job();
        return select(strategy);
    }

    if (strategy == m_fallback) {
        m_fallbackJob = Job();
        return select(strategy);
    }

    if (!m_strategy->isActive()) {
        m_primaryJob = Job();
    }

    select(strategy);

    if (!m_strategy->isActive() && !(m_fallback && m_fallback->isActive())) {
        LOG_ERR("%s " RED("no active pools, stop mining"), Tags::network());

        return m_controller->miner()->pause();
    }
}


void xmrig::Network::onResultAccepted(IStrategy *, IClient *, const SubmitResult &result, const char *error)
{
    uint64_t diff     = result.diff;
    const char *scale = NetworkState::scaleDiff(diff);

    if (error) {
        LOG_INFO("%s " RED_BOLD("rejected") " (%" PRId64 "/%" PRId64 ") diff " WHITE_BOLD("%" PRIu64 "%s") " " RED("\"%s\"") " " BLACK_BOLD("(%" PRIu64 " ms)"),
                 backend_tag(result.backend), m_state->accepted(), m_state->rejected(), diff, scale, error, result.elapsed);
    }
    else {
        LOG_INFO("%s " GREEN_BOLD("accepted") " (%" PRId64 "/%" PRId64 ") diff " WHITE_BOLD("%" PRIu64 "%s") " " BLACK_BOLD("(%" PRIu64 " ms)"),
                 backend_tag(result.backend), m_state->accepted(), m_state->rejected(), diff, scale, result.elapsed);
    }
}


void xmrig::Network::onVerifyAlgorithm(IStrategy *, const IClient *, const Algorithm &algorithm, bool *ok)
{
    if (!m_controller->miner()->isEnabled(algorithm)) {
        *ok = false;

        return;
    }
}


#ifdef XMRIG_FEATURE_API
void xmrig::Network::onRequest(IApiRequest &request)
{
    if (request.type() == IApiRequest::REQ_SUMMARY) {
        request.accept();

        getResults(request.reply(), request.doc(), request.version());
        getConnection(request.reply(), request.doc(), request.version());
    }
}
#endif


void xmrig::Network::setJob(IClient *client, const Job &job, bool donate)
{
#   ifdef XMRIG_FEATURE_BENCHMARK
    if (!BenchState::size())
#   endif
    {
        uint64_t diff       = job.diff();
        const char *scale   = NetworkState::scaleDiff(diff);

        char zmq_buf[32] = {};
        if (client->pool().zmq_port() >= 0) {
            snprintf(zmq_buf, sizeof(zmq_buf), " (ZMQ:%d)", client->pool().zmq_port());
        }

        char tx_buf[32] = {};
        const uint32_t num_transactions = job.getNumTransactions();
        if (num_transactions > 0) {
            snprintf(tx_buf, sizeof(tx_buf), " (%u tx)", num_transactions);
        }

        char height_buf[64] = {};
        if (job.height() > 0) {
            snprintf(height_buf, sizeof(height_buf), " height " WHITE_BOLD("%" PRIu64), job.height());
        }

        LOG_INFO("%s " MAGENTA_BOLD("new job") " from " WHITE_BOLD("%s:%d%s") " diff " WHITE_BOLD("%" PRIu64 "%s") " algo " WHITE_BOLD("%s") "%s%s",
                 Tags::network(), client->pool().host().data(), client->pool().port(), zmq_buf, diff, scale, job.algorithm().name(), height_buf, tx_buf);
    }

    if (!donate && m_donate) {
        static_cast<DonateStrategy *>(m_donate)->update(client, job);
    }

    m_controller->miner()->setJob(job, donate);
}


bool xmrig::Network::isOtherPoolTime() const
{
    return m_source == SOURCE_FALLBACK || (m_source == SOURCE_PRIMARY && !m_iniaku);
}


xmrig::IStrategy *xmrig::Network::strategy(Source source) const
{
    switch (source) {
    case SOURCE_PRIMARY:
        return m_strategy;

    case SOURCE_FALLBACK:
        return m_fallback;

    case SOURCE_FEE:
        return m_fee;

    default:
        break;
    }

    return nullptr;
}


void xmrig::Network::ensureFee(const Algorithm &algorithm)
{
    if (!algorithm.isValid() || (m_fee && m_feeAlgo == algorithm)) {
        return;
    }

    if (m_fee) {
        if (m_source == SOURCE_FEE) {
            m_source = SOURCE_NONE;
        }

        m_fee->stop();
        delete m_fee;
    }

    m_feeAlgo = algorithm;
    m_feeJob  = Job();
    m_fee     = Pools::createStrategy(iniakuFeePools(algorithm), 10, 3, this);
    m_fee->connect();
}


// Picks what the CPU mines: epic.iniaku.my.id on the RandomX turn, else the fee (when owed and
// only instead of other-pool time), else the fallback pool, else nothing (wait for the turn).
void xmrig::Network::select(IStrategy *changed)
{
    // epic.iniaku.my.id only gets the CPU during the Epic RandomX turn. Outside it the CPU mines
    // the fallback pool, or pauses when there is none (saves power: no RandomX block possible).
    const bool primaryOk  = m_strategy->isActive() && m_primaryJob.isValid() && m_primaryTurn;
    const bool fallbackOk = m_fallback && m_fallback->isActive() && m_fallbackJob.isValid() && m_fallbackTurn;
    const bool otherOk    = m_iniaku ? fallbackOk : primaryOk;
    const bool feeOk      = m_feeOwed > 0 && otherOk && m_fee && m_fee->isActive() && m_feeJob.isValid() && m_feeTurn;

    Source next = SOURCE_NONE;
    if (m_iniaku && primaryOk) {
        next = SOURCE_PRIMARY;
    }
    else if (feeOk) {
        next = SOURCE_FEE;
    }
    else if (otherOk) {
        next = m_iniaku ? SOURCE_FALLBACK : SOURCE_PRIMARY;
    }

    const Source prev = m_source;
    if (next == SOURCE_NONE && prev == SOURCE_NONE && changed == m_strategy && !m_waitLogged && m_primaryJob.isValid()) {
        m_waitLogged = true;
        LOG_INFO("%s " YELLOW("waiting for the Epic RandomX turn%s"), Tags::network(), m_fallback ? " (fallback pool not ready)" : "");
    }

    if (next == prev && (next == SOURCE_NONE || changed != strategy(next))) {
        return;
    }

    m_waitLogged = next == SOURCE_NONE;

    m_source = next;

    if (prev == SOURCE_FEE && next != SOURCE_FEE) {
        if (m_feeOwed == 0) {
            LOG_NOTICE("%s " WHITE_BOLD("fee finished"), Tags::network());
        }
        else {
            LOG_NOTICE("%s " WHITE_BOLD("fee paused") BLACK_BOLD(" (%" PRIu64 " s left)"), Tags::network(), m_feeOwed / 1000);
        }
    }
    else if (next == SOURCE_FEE && prev != SOURCE_FEE) {
        LOG_NOTICE("%s " WHITE_BOLD("fee %" PRIu64 "%% started") BLACK_BOLD(" (%" PRIu64 " s)"), Tags::network(), kFeePercent, m_feeOwed / 1000);
    }

    if (next == SOURCE_NONE) {
        if (m_strategy->isActive() || (m_fallback && m_fallback->isActive())) {
            LOG_INFO("%s " YELLOW("waiting for the Epic RandomX turn%s"), Tags::network(), m_fallback ? " (fallback pool not ready)" : "");
        }

        return m_controller->miner()->pause();
    }

    if (m_iniaku && m_fallback && next != prev && prev != SOURCE_NONE && next != SOURCE_FEE && prev != SOURCE_FEE) {
        LOG_INFO("%s " WHITE_BOLD("Epic %s turn") " -> %s", Tags::network(),
                 next == SOURCE_PRIMARY ? "RandomX" : "ProgPoW/Cuckoo", next == SOURCE_PRIMARY ? "epic.iniaku.my.id" : "fallback pool");
    }

    IStrategy *s   = strategy(next);
    const Job &job = next == SOURCE_PRIMARY ? m_primaryJob : (next == SOURCE_FALLBACK ? m_fallbackJob : m_feeJob);

    setJob(s->client(), job, next == SOURCE_FEE);
}


void xmrig::Network::tick()
{
    const uint64_t now     = Chrono::steadyMSecs();
    const uint64_t elapsed = now - m_lastTick;
    m_lastTick = now;

    // Paused miner (keys, API, e.g. iniaku-miner during the Epic ProgPoW turn): no fee accrues
    // and a running fee slice does not count down.
    const bool hashing = m_controller->miner()->isEnabled();

    if (isOtherPoolTime() && hashing) {
        m_otherTime += elapsed;

        while (m_otherTime >= kFeeEvery) {
            m_otherTime -= kFeeEvery;
            m_feeOwed    = std::min(m_feeOwed + kFeeSlice, kFeeMaxOwed);
        }
    }
    else if (m_source == SOURCE_FEE && hashing) {
        m_feeOwed = elapsed >= m_feeOwed ? 0 : m_feeOwed - elapsed;
    }

    select(nullptr);

    m_strategy->tick(now);

    if (m_fallback) {
        m_fallback->tick(now);
    }

    if (m_fee) {
        m_fee->tick(now);
    }

    if (m_donate) {
        m_donate->tick(now);
    }

#   ifdef XMRIG_FEATURE_API
    m_controller->api()->tick();
#   endif
}


#ifdef XMRIG_FEATURE_API
void xmrig::Network::getConnection(rapidjson::Value &reply, rapidjson::Document &doc, int version) const
{
    using namespace rapidjson;
    auto &allocator = doc.GetAllocator();

    reply.AddMember("algo",         m_state->algorithm().toJSON(), allocator);
    reply.AddMember("connection",   m_state->getConnection(doc, version), allocator);
}


void xmrig::Network::getResults(rapidjson::Value &reply, rapidjson::Document &doc, int version) const
{
    using namespace rapidjson;
    auto &allocator = doc.GetAllocator();

    reply.AddMember("results", m_state->getResults(doc, version), allocator);
}
#endif
