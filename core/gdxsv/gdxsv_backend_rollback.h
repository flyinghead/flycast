#pragma once
#include <future>
#include <map>
#include <string>

#include "gdxsv_network.h"
#include "lbs_message.h"
#include "mcs_message.h"

class GdxsvBackendRollback {
   public:
    GdxsvBackendRollback(const std::map<std::string, u32> &symbols, std::atomic<int> &maxlag)
        : state_(State::None), symbols_(symbols), maxlag_(maxlag), recv_delay_(0), port_(0) {}

    enum class State {
        None,
        StartLocalTest,
        LbsStartBattleFlow,
        McsWaitJoin,

        McsSessionExchange,
        StopEmulator,
        WaitPingPong,
        StartGGPOSession,
        WaitGGPOSession,
        McsInBattle,
        End,
    };

    void DisplayOSD();
    void Reset();
    void OnMainUiLoop();
    bool StartLocalTest(const char *param);
    void Prepare(const proto::P2PMatching &matching, int port);
    void Open();
    void Close();
    u32 OnSockWrite(u32 addr, u32 size);
    u32 OnSockRead(u32 addr, u32 size);
    u32 OnSockPoll();

   private:
    void ApplyPatch(bool first_time);
    void RestorePatch();
    void ProcessLbsMessage();

    State state_;
    const std::map<std::string, u32> &symbols_;
    std::atomic<int> &maxlag_;
    int recv_delay_;
    int port_;
    std::deque<u8> recv_buf_;
    LbsMessageReader lbs_tx_reader_;
    proto::P2PMatching matching_;
    UdpPingPong ping_pong_;
    std::future<bool> start_network_;
};
