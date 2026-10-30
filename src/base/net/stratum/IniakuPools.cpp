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

// winsock2.h before anything that may include windows.h
#ifdef _WIN32
#   include <winsock2.h>
#   include <ws2tcpip.h>
#else
#   include <fcntl.h>
#   include <netdb.h>
#   include <sys/select.h>
#   include <sys/socket.h>
#   include <unistd.h>
#endif


#include "base/net/stratum/IniakuPools.h"
#include "3rdparty/rapidjson/document.h"
#include "base/io/json/Json.h"
#include "base/io/log/Log.h"
#include "base/net/stratum/Pool.h"


#include <algorithm>
#include <cinttypes>
#include <chrono>
#include <cstring>
#include <string>




namespace xmrig {


struct IniakuServer
{
    const char *region;
    const char *host;
};


static const IniakuServer kServers[] = {
    { "id", "id.epic.iniaku.my.id" },
    { "us", "us.epic.iniaku.my.id" },
    { "au", "au.epic.iniaku.my.id" },
};

static constexpr uint16_t kPort       = 3334; // stratum TLS
static constexpr int kProbeTimeoutMs  = 3000;
static constexpr int64_t kUnreachable = 1000000;


// TCP connect time to host:kPort in ms (approximate ping), kUnreachable if it fails.
static int64_t probe(const char *host)
{
#   ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#   endif

    addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo *res = nullptr;
    if (getaddrinfo(host, std::to_string(kPort).c_str(), &hints, &res) != 0 || !res) {
        return kUnreachable;
    }

    int64_t result = kUnreachable;
    const auto start = std::chrono::steady_clock::now();

#   ifdef _WIN32
    SOCKET fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd != INVALID_SOCKET) {
        u_long nb = 1;
        ioctlsocket(fd, FIONBIO, &nb);
#   else
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd >= 0) {
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
#   endif
        connect(fd, res->ai_addr, static_cast<int>(res->ai_addrlen));

        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(fd, &wfds);
        timeval tv{ kProbeTimeoutMs / 1000, (kProbeTimeoutMs % 1000) * 1000 };

        if (select(static_cast<int>(fd) + 1, nullptr, &wfds, nullptr, &tv) > 0) {
            int err      = 0;
            socklen_t len = sizeof(err);
            getsockopt(fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&err), &len);
            if (err == 0) {
                result = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
            }
        }

#   ifdef _WIN32
        closesocket(fd);
#   else
        close(fd);
#   endif
    }

    freeaddrinfo(res);

    return result;
}


bool isIniakuRegion(const char *region)
{
    if (!region || strcmp(region, "auto") == 0) {
        return true;
    }

    return std::any_of(std::begin(kServers), std::end(kServers), [region](const IniakuServer &s) { return strcmp(s.region, region) == 0; });
}


// This build mines only to epic.iniaku.my.id: pool URLs from the config/command line are
// ignored, only the wallet (user), password and rig-id of the first pool entry are used.
// Servers are ordered by the chosen region ("auto" = lowest connect time); xmrig fails over
// to the next server when one is down.
void applyIniakuPools(std::vector<Pool> &pools, const rapidjson::Value &config, const char *region)
{
    std::string user;
    std::string pass = "x";
    std::string rigId;

    if (config.IsArray()) {
        for (const auto &value : config.GetArray()) {
            if (value.IsObject() && Json::getString(value, Pool::kUser)) {
                user = Json::getString(value, Pool::kUser);
                if (Json::getString(value, Pool::kPass)) {
                    pass = Json::getString(value, Pool::kPass);
                }
                if (Json::getString(value, Pool::kRigId)) {
                    rigId = Json::getString(value, Pool::kRigId);
                }
                break;
            }
        }
    }

    if (!region || !isIniakuRegion(region)) {
        LOG_ERR("unknown region \"%s\", using auto (valid: auto, id, us, au)", region ? region : "");
        region = "auto";
    }

    struct Ranked { const IniakuServer *server; int64_t ms; };
    std::vector<Ranked> ranked;
    for (const auto &s : kServers) {
        ranked.push_back({ &s, probe(s.host) });
        if (ranked.back().ms >= kUnreachable) {
            LOG_WARN("pool %s (%s): unreachable", s.region, s.host);
        }
        else {
            LOG_INFO("pool %s (%s): %" PRId64 " ms", s.region, s.host, ranked.back().ms);
        }
    }

    std::stable_sort(ranked.begin(), ranked.end(), [](const Ranked &a, const Ranked &b) { return a.ms < b.ms; });
    if (strcmp(region, "auto") != 0) {
        std::stable_partition(ranked.begin(), ranked.end(), [region](const Ranked &r) { return strcmp(r.server->region, region) == 0; });
    }

    pools.clear();
    for (const auto &r : ranked) {
        Pool pool(r.server->host, kPort, user.c_str(), pass.c_str(), nullptr, Pool::kKeepAliveTimeout, false, true, Pool::MODE_POOL);
        pool.setAlgo(Algorithm::RX_EPIC);
        if (!rigId.empty()) {
            pool.setRigId(rigId.c_str());
        }
        pools.push_back(std::move(pool));
    }
}


} /* namespace xmrig */
