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

#ifndef XMRIG_EPICCLIENT_H
#define XMRIG_EPICCLIENT_H


#include "base/net/stratum/Client.h"


namespace xmrig {


// Native Epic Cash stratum (the protocol of the Epic node, JSON-RPC 2.0):
//   login {login, pass, agent} -> "ok", getjobtemplate {algorithm} -> job template,
//   "job" notifications, submit {height, job_id, nonce, pow: {RandomX: [32 bytes]}}.
// Job blob = pre_pow + 8-byte big-endian nonce (see Job::nonceOffset for rx/epic).
class EpicClient : public Client
{
public:
    XMRIG_DISABLE_COPY_MOVE_DEFAULT(EpicClient)

    EpicClient(int id, const char *agent, IClientListener *listener);
    ~EpicClient() override = default;

protected:
    int64_t submit(const JobResult &result) override;
    void login() override;
    void onClose() override;

    bool handleResponse(int64_t id, const rapidjson::Value &result, const rapidjson::Value &error) override;
    void parseNotification(const char *method, const rapidjson::Value &params, const rapidjson::Value &error) override;

private:
    static const char *errorMessage(const rapidjson::Value &error);

    bool parseJob(const rapidjson::Value &params);
    void onJobTemplate(const rapidjson::Value &result, bool success, uint64_t elapsed);
    void onLoginResponse(const rapidjson::Value &result, bool success, uint64_t elapsed);
    void requestJob();

    bool m_authorized = false;
};


} /* namespace xmrig */


#endif /* XMRIG_EPICCLIENT_H */
