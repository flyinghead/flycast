#pragma once
#include <string>
#include <map>
#include <future>

#include "gdxsv_network.h"
#include "lbs_message.h"
#include "mcs_message.h"

class GdxsvBackendRollback {
public:
    GdxsvBackendRollback(const std::map<std::string, u32> &symbols, std::atomic<int> &maxlag)
            : symbols_(symbols), maxlag_(maxlag) {
    }

    enum class State {
        None,
        StartLocalTest,
        LbsStartBattleFlow,
        McsWaitJoin,

        McsSessionExchange,
        WaitPingPong,
        StartGGPOSession,
        WaitGGPOSession,
        McsInBattle,
        End,
    };

    void Reset();
    void OnMainUiLoop();
    bool StartLocalTest(const char* param);
    void Prepare(const proto::P2PMatching matching, int port);
    void Open();
    void Close();
    u32 OnSockWrite(u32 addr, u32 size);
    u32 OnSockRead(u32 addr, u32 size);
    u32 OnSockPoll();
    void ProcessLbsMessage();
    void ApplyPatch(bool first_time);
    void RestorePatch();

    const std::map<std::string, u32> &symbols_;
    std::atomic<int> &maxlag_;
    State state_;
    LbsMessageReader lbs_tx_reader_;
    McsMessageReader mcs_tx_reader_;
    std::deque<u8> recv_buf_;
    int recv_delay_;
    int player_count_;
    int me_;
    int port_;
    UdpPingPong ping_pong_;
    std::future<bool> start_network_;
    proto::P2PMatching matching_;
    struct FrameInfo {
        void Reset() {
            start_session = false;
            end_session = false;
        }

        bool start_session;
        bool end_session;
    } frame_info_;

};
