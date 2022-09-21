#include "gdxsv_backend_rollback.h"

#include <vector>
#include <string>
#include <map>
#include <future>

#include "libs.h"
#include "gdx_rpc.h"
#include "gdxsv.pb.h"
#include "network/ggpo.h"
#include "rend/gui.h"
#include "emulator.h"


namespace {
    u8 dummy_game_param[640] = { 0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0x02, 0x00, 0x05, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x83, 0x76, 0x83, 0x8c, 0x83, 0x43, 0x83, 0x84, 0x81, 0x5b, 0x82, 0x50, 0x00, 0x00, 0x00, 0x00, 0x07 };
    const u8 dummy_rule_data[] = { 0x03,0x02,0x03,0x00,0x00,0x01,0x58,0x02,0x58,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0x3f,0xff,0xff,0xff,0x3f,0x00,0x00,0xff,0x01,0xff,0xff,0xff,0x3f,0xff,0xff,0xff,0x3f,0x00 };

    std::mutex wait_mutex;
    int waiting_keyframe_players = 0;
    int waiting_keyframe_type = 0;
    int waiting_keyframe_max = 0;

    void onKeyFrameMessage(int playerNum, int frameType, int frameCount)
    {
        std::lock_guard<std::mutex> lock(wait_mutex);

        NOTICE_LOG(NETWORK, "Player:%d is waiting for key frame to %s Message since %d",
            playerNum, McsMessage::MsgTypeName((McsMessage::MsgType)frameType), frameCount);

        if (waiting_keyframe_type != frameType) {
            NOTICE_LOG(NETWORK, "Waiting key frame type is updated %d -> %d", waiting_keyframe_type, frameType);
            waiting_keyframe_players = 0;
        }

        if (waiting_keyframe_players >> playerNum & 1) {
            return;
        }

        if (waiting_keyframe_max < frameCount) {
            NOTICE_LOG(NETWORK, "Waiting key frame max is updated %d -> %d", waiting_keyframe_max, frameCount);
        }

        waiting_keyframe_players |= 1 << playerNum;
        waiting_keyframe_type = frameType;
        waiting_keyframe_max = std::max(waiting_keyframe_max, frameCount);
    }

    // maple input to mcs pad input
    u16 conv_input(MapleInputState input) {
        u16 r = 0;
        if (~input.kcode & 0x0004) r |= 0x4000; // A
        if (~input.kcode & 0x0002) r |= 0x2000; // B
        if (~input.kcode & 0x0400) r |= 0x0002; // X
        if (~input.kcode & 0x0200) r |= 0x0001; // Y
        if (~input.kcode & 0x0010) r |= 0x0020; // up
        if (~input.kcode & 0x0020) r |= 0x0010; // down
        if (~input.kcode & 0x0080) r |= 0x0004; // right
        if (~input.kcode & 0x0040) r |= 0x0008; // left
        if (~input.kcode & 0x0008) r |= 0x0080; // Start
        if (~input.kcode & 0x00020000) r |= 0x8000; // LT
        if (~input.kcode & 0x00040000) r |= 0x1000; // RT
        return r;
    }

}

void GdxsvBackendRollback::Reset() {
    RestorePatch();
    state_ = State::None;
    lbs_tx_reader_.Clear();
    mcs_tx_reader_.Clear();
    recv_buf_.clear();
    recv_delay_ = 0;
    me_ = 0;
}


void GdxsvBackendRollback::OnGuiMainUiLoop() {
    if (frame_info.start_session && !ggpo::active()) {
        emu.stop();
        config::GGPOEnable.override(1);
        settings.aica.NoBatch = 1;
        start_network_ = ggpo::gdxsvStartNetwork(0, me_);
        ggpo::receiveKeyFrameMessages(onKeyFrameMessage);
    }

    if (start_network_.valid() && start_network_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        start_network_ = std::future<bool>();

        if (!ggpo::active()) {
            NOTICE_LOG(COMMON, "StartNetwork failure");
        }
        emu.start();
    }

    if (frame_info.end_session && ggpo::active()) {
        emu.stop();
        ggpo::stopSession();
        config::GGPOEnable.override(0);
        ggpo::receiveKeyFrameMessages(nullptr);
        emu.start();
    }

    frame_info.Reset();
}

bool GdxsvBackendRollback::StartLocalTest(const char* param) {
    player_count_ = 4;
    auto args = std::string(param);
    if (0 < args.size() && '1' <= args[0] && args[0] <= '4') {
        me_ = args[0] - '1';
    }
    state_ = State::StartLocalTest;
    maxlag_ = 0;
    NOTICE_LOG(COMMON, "RollbackNet StartLocalTest %d", me_);
    return true;
}

void GdxsvBackendRollback::Open() {
    recv_buf_.assign({ 0x0e, 0x61, 0x00, 0x22, 0x10, 0x31, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd });
    state_ = State::McsSessionExchange;
    ApplyPatch(true);
}

void GdxsvBackendRollback::Close() {
    frame_info.end_session = true;
    RestorePatch();
    state_ = State::End;
}

u32 GdxsvBackendRollback::OnSockWrite(u32 addr, u32 size) {
    u8 buf[InetBufSize];
    for (int i = 0; i < size; ++i) {
        buf[i] = gdxsv_ReadMem8(addr + i);
    }

    if (state_ <= State::LbsStartBattleFlow) {
        lbs_tx_reader_.Write((const char*)buf, size);
    }
    else {
        mcs_tx_reader_.Write((const char*)buf, size);
    }

    if (state_ <= State::LbsStartBattleFlow) {
        ProcessLbsMessage();
    }
    else {
        McsMessage msg;
        if (mcs_tx_reader_.Read(msg)) {
            int frame = 0;
            bool rollback = false;
            ggpo::getCurrentFrame(&frame, &rollback);
            NOTICE_LOG(COMMON, "[FRAME:%4d :RBK=%d] OnSockSend: %s %s",
                frame, rollback, McsMessage::MsgTypeName(msg.Type()), msg.ToHex().c_str());
        }
    }

    ApplyPatch(false);

    return size;
}

u32 GdxsvBackendRollback::OnSockRead(u32 addr, u32 size) {
    if (state_ <= State::LbsStartBattleFlow) {
        ProcessLbsMessage();
    }
    else {
        int frame = 0;
        bool rollback = false;
        ggpo::getCurrentFrame(&frame, &rollback);
        const int InetBuf = 0x0c3ab984;
        const int ConnectionStatus = 0x0c3abb88;
        NOTICE_LOG(COMMON, "[FRAME:%4d :RBK=%d] OnSockRead CONNECTION: %d vvvvvvvv", frame, rollback, gdxsv_ReadMem8(ConnectionStatus));

        int msg_len = gdxsv_ReadMem8(InetBuf);
        McsMessage msg;
        if (0 < msg_len) {
            if (msg_len == 0x82) {
                msg_len = 20;
            }
            msg.body.resize(msg_len);
            for (int i = 0; i < msg_len; i++) {
                msg.body[i] = gdxsv_ReadMem8(InetBuf + i);
                gdxsv_WriteMem8(InetBuf + i, 0);
            }

            NOTICE_LOG(COMMON, "InetBuf:%s %s", McsMessage::MsgTypeName(msg.Type()), msg.ToHex().c_str());

            switch (msg.Type()) {
            case McsMessage::MsgType::ConnectionIdMsg:
                frame_info.start_session = true;
                state_ = State::McsInBattle;
                break;
            case McsMessage::MsgType::IntroMsg:
                for (int i = 0; i < player_count_; i++) {
                    if (i != me_) {
                        auto intro_msg = McsMessage::Create(McsMessage::MsgType::IntroMsg, i);
                        std::copy(intro_msg.body.begin(), intro_msg.body.end(), std::back_inserter(recv_buf_));
                    }
                }
                break;
            case McsMessage::MsgType::IntroMsgReturn:
                for (int i = 0; i < player_count_; i++) {
                    if (i != me_) {
                        auto intro_msg = McsMessage::Create(McsMessage::MsgType::IntroMsgReturn, i);
                        std::copy(intro_msg.body.begin(), intro_msg.body.end(), std::back_inserter(recv_buf_));
                    }
                }
                break;
            case McsMessage::MsgType::PingMsg:
                for (int i = 0; i < player_count_; i++) {
                    if (i != me_) {
                        auto pong_msg = McsMessage::Create(McsMessage::MsgType::PongMsg, i);
                        pong_msg.SetPongTo(me_);
                        pong_msg.PongCount(msg.PingCount());
                        std::copy(pong_msg.body.begin(), pong_msg.body.end(), std::back_inserter(recv_buf_));
                    }
                }
                break;
            case McsMessage::MsgType::PongMsg:
                break;
            case McsMessage::MsgType::StartMsg:
                if (!rollback) {
                    gui_display_notification("Sync...", 1000);
                    NOTICE_LOG(COMMON, "StartMsg KeyFrame:%d", frame);
                    ggpo::sendKeyFrameMessage(me_, McsMessage::MsgType::StartMsg, frame);
                    onKeyFrameMessage(me_, McsMessage::MsgType::StartMsg, frame);
                }
                break;
            case McsMessage::MsgType::ForceMsg:
                break;
            case McsMessage::MsgType::KeyMsg1:
                NOTICE_LOG(COMMON, "<- KeyInput:%d", frame);
                for (int i = 0; i < player_count_; ++i) {
                    auto msg = McsMessage::Create(McsMessage::KeyMsg1, i);
                    auto input = conv_input(mapleInputState[i]);
                    msg.body[2] = input >> 8 & 0xff;
                    msg.body[3] = input & 0xff;
                    std::copy(msg.body.begin(), msg.body.end(), std::back_inserter(recv_buf_));
                }
                break;
            case McsMessage::MsgType::KeyMsg2:
                verify(false);
                break;
            case McsMessage::MsgType::LoadStartMsg:
                // It will be dropped because InetBuf is cleared. 
                break;
            case McsMessage::MsgType::LoadEndMsg:
                for (int i = 0; i < player_count_; i++) {
                    if (i != me_) {
                        auto a = McsMessage::Create(McsMessage::MsgType::LoadStartMsg, i);
                        std::copy(a.body.begin(), a.body.end(), std::back_inserter(recv_buf_));
                    }
                }

                if (ggpo::getCurrentFrame(&frame, &rollback) && !rollback) {
                    gui_display_notification("Sync...", 1000);
                    NOTICE_LOG(COMMON, "LoadEndMsg KeyFrame:%d", frame);
                    ggpo::sendKeyFrameMessage(me_, McsMessage::MsgType::LoadEndMsg, frame);
                    onKeyFrameMessage(me_, McsMessage::MsgType::LoadEndMsg, frame);
                }
                break;
            default:
                WARN_LOG(COMMON, "unhandled mcs msg: %s", McsMessage::MsgTypeName(msg.Type()));
                WARN_LOG(COMMON, "%s", msg.ToHex().c_str());
                break;
            }

            verify(recv_buf_.size() <= size);
        }

        wait_mutex.lock();
        const int wait_type = waiting_keyframe_type;
        const int join_frame = waiting_keyframe_max + WaitKeyFrameDelta;
        wait_mutex.unlock();

        if (frame == join_frame) {
            if (wait_type == McsMessage::MsgType::StartMsg) {
                NOTICE_LOG(COMMON, "StartMsg Join:%d", frame);
                for (int i = 0; i < player_count_; i++) {
                    if (i != me_) {
                        auto start_msg = McsMessage::Create(McsMessage::MsgType::StartMsg, i);
                        std::copy(start_msg.body.begin(), start_msg.body.end(), std::back_inserter(recv_buf_));
                    }
                }
            }
            if (wait_type == McsMessage::MsgType::LoadEndMsg) {
                NOTICE_LOG(COMMON, "LoadEndMsg Join:%d", frame);
                for (int i = 0; i < player_count_; i++) {
                    if (i != me_) {
                        auto b = McsMessage::Create(McsMessage::MsgType::LoadEndMsg, i);
                        std::copy(b.body.begin(), b.body.end(), std::back_inserter(recv_buf_));
                    }
                }
            }
        }

        verify(recv_buf_.size() <= size);

        NOTICE_LOG(COMMON, "[FRAME:%4d :RBK=%d] OnSockRead CONNECTION: %d END ======",
            frame, rollback, gdxsv_ReadMem8(ConnectionStatus));
    }

    if (recv_buf_.empty()) {
        return 0;
    }

    int n = std::min<int>(recv_buf_.size(), size);
    for (int i = 0; i < n; ++i) {
        gdxsv_WriteMem8(addr + i, recv_buf_.front());
        recv_buf_.pop_front();
    }
    return n;
}

u32 GdxsvBackendRollback::OnSockPoll() {
    if (state_ <= State::LbsStartBattleFlow) {
        ProcessLbsMessage();
    }
    if (0 < recv_delay_) {
        recv_delay_--;
        return 0;
    }

    return recv_buf_.size();
}

void GdxsvBackendRollback::ProcessLbsMessage() {
    if (state_ == State::StartLocalTest) {
        LbsMessage::SvNotice(LbsMessage::lbsReadyBattle).Serialize(recv_buf_);
        recv_delay_ = 1;
        state_ = State::LbsStartBattleFlow;
    }

    LbsMessage msg;
    if (lbs_tx_reader_.Read(msg)) {
        if (state_ == State::StartLocalTest) {
            state_ = State::LbsStartBattleFlow;
        }

        if (msg.command == LbsMessage::lbsLobbyMatchingEntry) {
            LbsMessage::SvAnswer(msg).Serialize(recv_buf_);
            LbsMessage::SvNotice(LbsMessage::lbsReadyBattle).Serialize(recv_buf_);
        }

        if (msg.command == LbsMessage::lbsAskMatchingJoin) {
            LbsMessage::SvAnswer(msg).Write8(player_count_)->Serialize(recv_buf_);
        }

        if (msg.command == LbsMessage::lbsAskPlayerSide) {
            LbsMessage::SvAnswer(msg).Write8(me_ + 1)->Serialize(recv_buf_);
        }

        if (msg.command == LbsMessage::lbsAskPlayerInfo) {
            int pos = msg.Read8();
            dummy_game_param[16] = '0' + pos;
            dummy_game_param[17] = 0;
            LbsMessage::SvAnswer(msg).
                Write8(pos)->
                WriteString("USER0" + std::to_string(pos))->
                WriteString("USER0" + std::to_string(pos))->
                WriteBytes(reinterpret_cast<char*>(dummy_game_param), sizeof(dummy_game_param))->
                Write16(1)->
                Write16(0)->
                Write16(0)->
                Write16(0)->
                Write16(0)->
                Write16(0)->
                Write16(1 + (pos - 1) / 2)->
                Write16(0)->
                Serialize(recv_buf_);
        }

        if (msg.command == LbsMessage::lbsAskRuleData) {
            LbsMessage::SvAnswer(msg).
                WriteBytes((char*)dummy_rule_data, sizeof(dummy_rule_data))->
                Serialize(recv_buf_);
        }

        if (msg.command == LbsMessage::lbsAskBattleCode) {
            LbsMessage::SvAnswer(msg).
                WriteString("012345")->
                Serialize(recv_buf_);
        }

        if (msg.command == LbsMessage::lbsAskMcsVersion) {
            LbsMessage::SvAnswer(msg).
                Write8(10)->Serialize(recv_buf_);
        }

        if (msg.command == LbsMessage::lbsAskMcsAddress) {
            LbsMessage::SvAnswer(msg).
                Write16(4)->Write8(127)->Write8(0)->Write8(0)->Write8(1)->
                Write16(2)->Write16(3333)->Serialize(recv_buf_);
        }

        if (msg.command == LbsMessage::lbsLogout) {
            state_ = State::McsWaitJoin;
        }

        recv_delay_ = 1;
    }
}

void GdxsvBackendRollback::ApplyPatch(bool first_time) {
    if (state_ == State::None || state_ == State::End) {
        return;
    }

    // Skip Key MsgPush
    auto it = symbols_.find("disk");
    if (it != symbols_.end() && gdxsv_ReadMem32(it->second) == 2) {
        gdxsv_WriteMem16(0x8c045f64, 9);
        gdxsv_WriteMem8(0x0c3abb90, 1);
    }
}

void GdxsvBackendRollback::RestorePatch() {
    // Skip Key MsgPush
    auto it = symbols_.find("disk");
    if (it != symbols_.end() && gdxsv_ReadMem32(it->second) == 2) {
        gdxsv_WriteMem16(0x8c045f64, 0x410b);
        gdxsv_WriteMem8(0x0c3abb90, 2);
    }
}
