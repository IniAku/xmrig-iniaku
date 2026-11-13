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

#ifndef XMRIG_NETWORK_H
#define XMRIG_NETWORK_H


#include "3rdparty/rapidjson/fwd.h"
#include "base/api/interfaces/IApiListener.h"
#include "base/kernel/interfaces/IBaseListener.h"
#include "base/kernel/interfaces/IStrategyListener.h"
#include "base/kernel/interfaces/ITimerListener.h"
#include "base/tools/Object.h"
#include "base/crypto/Algorithm.h"
#include "base/net/stratum/Job.h"
#include "interfaces/IJobResultListener.h"


#include <vector>


namespace xmrig {


class Controller;
class IStrategy;
class NetworkState;


class Network : public IJobResultListener, public IStrategyListener, public IBaseListener, public ITimerListener, public IApiListener
{
public:
    XMRIG_DISABLE_COPY_MOVE_DEFAULT(Network)

    Network(Controller *controller);
    ~Network() override;

    inline IStrategy *strategy() const { return m_strategy; }

    void connect();
    void execCommand(char command);

protected:
    inline void onTimer(const Timer *) override { tick(); }

    void onActive(IStrategy *strategy, IClient *client) override;
    void onConfigChanged(Config *config, Config *previousConfig) override;
    void onJob(IStrategy *strategy, IClient *client, const Job &job, const rapidjson::Value &params) override;
    void onJobResult(const JobResult &result) override;
    void onLogin(IStrategy *strategy, IClient *client, rapidjson::Document &doc, rapidjson::Value &params) override;
    void onPause(IStrategy *strategy) override;
    void onResultAccepted(IStrategy *strategy, IClient *client, const SubmitResult &result, const char *error) override;
    void onVerifyAlgorithm(IStrategy *strategy, const  IClient *client, const Algorithm &algorithm, bool *ok) override;

#   ifdef XMRIG_FEATURE_API
    void onRequest(IApiRequest &request) override;
#   endif

private:
    constexpr static int kTickInterval = 1 * 1000;

    // epic.iniaku.my.id edition: the primary pool (epic.iniaku.my.id) gets the CPU whenever the
    // Epic network is on the RandomX turn; the rest of the time the CPU mines the fallback pool.
    // Time on other pools (fallback, or all pools when none is epic.iniaku.my.id) carries the fee.
    enum Source {
        SOURCE_NONE,
        SOURCE_PRIMARY,
        SOURCE_FALLBACK,
        SOURCE_FEE
    };

    constexpr static uint64_t kFeePercent = 1;                                                 // = kIniakuFeePercent
    constexpr static uint64_t kFeeSlice   = 60 * 1000;                                       // fee is mined in 1 minute slices
    constexpr static uint64_t kFeeEvery   = kFeeSlice * (100 - kFeePercent) / kFeePercent;     // after 99 minutes on other pools
    constexpr static uint64_t kFeeMaxOwed = 10 * kFeeSlice;

    bool isOtherPoolTime() const;
    IStrategy *strategy(Source source) const;
    void ensureFee(const Algorithm &algorithm);
    void select(IStrategy *changed);
    void setJob(IClient *client, const Job &job, bool donate);
    void tick();

#   ifdef XMRIG_FEATURE_API
    void getConnection(rapidjson::Value &reply, rapidjson::Document &doc, int version) const;
    void getResults(rapidjson::Value &reply, rapidjson::Document &doc, int version) const;
#   endif

    Algorithm m_feeAlgo;
    bool m_iniaku           = false;
    bool m_fallbackTurn     = true;
    bool m_feeTurn          = true;
    bool m_primaryTurn      = true;
    bool m_waitLogged       = false;
    Controller *m_controller;
    IStrategy *m_donate     = nullptr;
    IStrategy *m_fallback   = nullptr;
    IStrategy *m_fee        = nullptr;
    IStrategy *m_strategy   = nullptr;
    Job m_fallbackJob;
    Job m_feeJob;
    Job m_primaryJob;
    NetworkState *m_state   = nullptr;
    Source m_source         = SOURCE_NONE;
    Timer *m_timer          = nullptr;
    uint64_t m_feeOwed      = 0;
    uint64_t m_lastTick     = 0;
    uint64_t m_otherTime    = 0;
};


} // namespace xmrig


#endif // XMRIG_NETWORK_H
