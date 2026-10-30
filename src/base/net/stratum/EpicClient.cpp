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

#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <string>


#include "base/net/stratum/EpicClient.h"
#include "3rdparty/rapidjson/document.h"
#include "base/io/json/Json.h"
#include "base/io/json/JsonRequest.h"
#include "base/io/log/Log.h"
#include "base/kernel/interfaces/IClientListener.h"
#include "base/tools/Cvt.h"
#include "net/JobResult.h"


namespace xmrig {

// Epic job ids restart at 0 for every height, so the xmrig job id is "<height>:<job_id>".
static bool splitJobId(const char *id, uint64_t &height, uint64_t &jobId)
{
    return id && sscanf(id, "%" SCNu64 ":%" SCNu64, &height, &jobId) == 2;
}

} // namespace xmrig


xmrig::EpicClient::EpicClient(int id, const char *agent, IClientListener *listener) :
    Client(id, agent, listener)
{
}


int64_t xmrig::EpicClient::submit(const JobResult &result)
{
#   ifndef XMRIG_PROXY_PROJECT
    if ((m_state != ConnectedState) || !m_authorized) {
        return -1;
    }
#   endif

    uint64_t height = 0;
    uint64_t jobId  = 0;
    if (!splitJobId(result.jobId.data(), height, jobId)) {
        LOG_ERR("%s " RED("invalid job id \"%s\""), tag(), result.jobId.data());
        return -1;
    }

    using namespace rapidjson;

    Document doc(kObjectType);
    auto &allocator = doc.GetAllocator();

    Value hash(kArrayType);
    for (size_t i = 0; i < 32; ++i) {
        hash.PushBack(result.result()[i], allocator);
    }

    Value pow(kObjectType);
    pow.AddMember("RandomX", hash, allocator);

    Value params(kObjectType);
    params.AddMember("height", height, allocator);
    params.AddMember("job_id", jobId, allocator);
    params.AddMember("nonce", result.nonce, allocator);
    params.AddMember("pow", pow, allocator);

    JsonRequest::create(doc, m_sequence, "submit", params);

    // Share difficulty for the log: 2^64 / first 8 hash bytes (big-endian).
    uint64_t top = 0;
    for (size_t i = 0; i < 8; ++i) {
        top = (top << 8) | result.result()[i];
    }
    const uint64_t actualDiff = top ? (uint64_t(-1) / top) : uint64_t(-1);

#   ifdef XMRIG_PROXY_PROJECT
    m_results[m_sequence] = SubmitResult(m_sequence, result.diff, actualDiff, result.id, 0);
#   else
    m_results[m_sequence] = SubmitResult(m_sequence, result.diff, actualDiff, 0, result.backend);
#   endif

    return send(doc);
}


void xmrig::EpicClient::login()
{
    using namespace rapidjson;

    m_results.clear();
    m_authorized = false;

    Document doc(kObjectType);
    auto &allocator = doc.GetAllocator();

    Value params(kObjectType);
    params.AddMember("login", m_user.toJSON(), allocator);
    params.AddMember("pass",  m_password.toJSON(), allocator);
    params.AddMember("agent", StringRef(agent()), allocator);

    JsonRequest::create(doc, m_sequence, "login", params);

    send(doc, [this](const rapidjson::Value &result, bool success, uint64_t elapsed) { onLoginResponse(result, success, elapsed); });
}


void xmrig::EpicClient::onClose()
{
    m_authorized = false;
    Client::onClose();
}


bool xmrig::EpicClient::handleResponse(int64_t id, const rapidjson::Value &result, const rapidjson::Value &error)
{
    auto it = m_callbacks.find(id);
    if (it != m_callbacks.end()) {
        const uint64_t elapsed = Chrono::steadyMSecs() - it->second.ts;
        const bool failed      = error.IsObject() || error.IsString();

        it->second.callback(failed ? error : result, !failed, elapsed);
        m_callbacks.erase(it);

        return true;
    }

    // submit: result "ok" or an error object {code, message}
    return handleSubmitResponse(id, errorMessage(error));
}


void xmrig::EpicClient::parseNotification(const char *method, const rapidjson::Value &params, const rapidjson::Value &)
{
    if (strcmp(method, "job") == 0) {
        parseJob(params);
    }
}


const char *xmrig::EpicClient::errorMessage(const rapidjson::Value &error)
{
    if (error.IsObject() && error.HasMember("message") && error["message"].IsString()) {
        return error["message"].GetString();
    }

    if (error.IsString()) {
        return error.GetString();
    }

    return nullptr;
}


bool xmrig::EpicClient::parseJob(const rapidjson::Value &params)
{
    if (!params.IsObject()) {
        return false;
    }

    const uint64_t height  = Json::getUint64(params, "height");
    const uint64_t jobId   = Json::getUint64(params, "job_id");
    const char *prePow     = Json::getString(params, "pre_pow");
    const auto &difficulty = Json::getArray(params, "difficulty");
    const auto &epochs     = Json::getArray(params, "epochs");

    if (!prePow || !difficulty.IsArray() || !epochs.IsArray()) {
        LOG_ERR("%s " RED("invalid job template"), tag());
        return false;
    }

    // Share difficulty for RandomX: difficulty = [["cuckoo", d], ["randomx", d], ["progpow", d]]
    uint64_t diff = 0;
    for (const auto &item : difficulty.GetArray()) {
        if (item.IsArray() && item.Size() == 2 && item[0].IsString() && strcmp(item[0].GetString(), "randomx") == 0 && item[1].IsUint64()) {
            diff = item[1].GetUint64();
        }
    }

    // RandomX key: epochs = [[start_height, end_height, [32 bytes seed]], ...]
    const rapidjson::Value *seed = nullptr;
    for (const auto &epoch : epochs.GetArray()) {
        if (!epoch.IsArray() || epoch.Size() != 3 || !epoch[2].IsArray() || epoch[2].Size() != 32) {
            continue;
        }

        if (!seed) {
            seed = &epoch[2];
        }

        if (epoch[0].IsUint64() && epoch[1].IsUint64() && epoch[0].GetUint64() <= height && height <= epoch[1].GetUint64()) {
            seed = &epoch[2];
            break;
        }
    }

    if (diff == 0 || !seed) {
        LOG_ERR("%s " RED("job template without randomx difficulty or seed"), tag());
        return false;
    }

    uint8_t seedBytes[32];
    for (size_t i = 0; i < 32; ++i) {
        seedBytes[i] = static_cast<uint8_t>((*seed)[static_cast<rapidjson::SizeType>(i)].GetUint());
    }

    Job job(false, Algorithm::RX_EPIC, rpcId());
    job.setAlgorithm(Algorithm::RX_EPIC);
    job.setHeight(height);
    job.setDiff(diff);

    const std::string id = std::to_string(height) + ":" + std::to_string(jobId);
    job.setId(id.c_str());

    // blob = pre_pow + room for the 8-byte nonce
    const std::string blob = std::string(prePow) + "0000000000000000";
    if (!job.setBlob(blob.c_str())) {
        LOG_ERR("%s " RED("invalid pre_pow (%zu hex chars)"), tag(), strlen(prePow));
        return false;
    }

    if (!job.setSeedHash(Cvt::toHex(seedBytes, sizeof(seedBytes)).data())) {
        LOG_ERR("%s " RED("invalid seed"), tag());
        return false;
    }

    if (m_job != job) {
        m_job = std::move(job);

        if (!m_authorized) {
            m_authorized = true;
            m_listener->onLoginSuccess(this);
        }

        m_listener->onJobReceived(this, m_job, params);
    }

    return true;
}


void xmrig::EpicClient::onJobTemplate(const rapidjson::Value &result, bool success, uint64_t)
{
    if (!success) {
        const char *message = errorMessage(result);
        LOG_ERR("%s " RED("getjobtemplate error: ") RED_BOLD("\"%s\""), tag(), message ? message : "unknown");
        return;
    }

    parseJob(result);
}


void xmrig::EpicClient::onLoginResponse(const rapidjson::Value &result, bool success, uint64_t)
{
    if (!success) {
        const char *message = errorMessage(result);
        LOG_ERR("%s " RED("login error: ") RED_BOLD("\"%s\""), tag(), message ? message : "unknown");
        close();
        return;
    }

    requestJob();
}


void xmrig::EpicClient::requestJob()
{
    using namespace rapidjson;

    Document doc(kObjectType);
    auto &allocator = doc.GetAllocator();

    Value params(kObjectType);
    params.AddMember("algorithm", "randomx", allocator);

    JsonRequest::create(doc, m_sequence, "getjobtemplate", params);

    send(doc, [this](const rapidjson::Value &result, bool success, uint64_t elapsed) { onJobTemplate(result, success, elapsed); });
}
