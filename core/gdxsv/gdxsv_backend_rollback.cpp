#include "gdxsv_backend_rollback.h"

#include <future>
#include <map>
#include <string>
#include <vector>

#include "emulator.h"
#include "gdx_rpc.h"
#include "gdxsv.h"
#include "gdxsv.pb.h"
#include "hw/maple/maple_if.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "libs.h"
#include "network/ggpo.h"
#include "network/net_platform.h"
#include "rend/gui.h"
#include "rend/gui_util.h"

namespace {
u8 DummyGameParam[640] = {0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0x02, 0x00, 0x05, 0x00, 0x04,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x83, 0x76, 0x83, 0x8c, 0x83, 0x43,
                          0x83, 0x84, 0x81, 0x5b, 0x82, 0x50, 0x00, 0x00, 0x00, 0x00, 0x07};
u8 DummyRuleData[] = {0x03, 0x02, 0x03, 0x00, 0x00, 0x01, 0x58, 0x02, 0x58, 0x02, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x3f, 0xff, 0xff, 0xff, 0x3f, 0x00, 0x00,
                      0xff, 0x01, 0xff, 0xff, 0xff, 0x3f, 0xff, 0xff, 0xff, 0x3f, 0x00};

const u16 ExInputNone = 0;
const u16 ExInputWaitStart = 1;
const u16 ExInputWaitLoadEnd = 2;

// maple input to mcs pad input
u16 convertInput(MapleInputState input) {
    u16 r = 0;
    if (~input.kcode & 0x0004) r |= 0x4000;      // A
    if (~input.kcode & 0x0002) r |= 0x2000;      // B
    if (~input.kcode & 0x0400) r |= 0x0002;      // X
    if (~input.kcode & 0x0200) r |= 0x0001;      // Y
    if (~input.kcode & 0x0010) r |= 0x0020;      // up
    if (~input.kcode & 0x0020) r |= 0x0010;      // down
    if (~input.kcode & 0x0080) r |= 0x0004;      // right
    if (~input.kcode & 0x0040) r |= 0x0008;      // left
    if (~input.kcode & 0x0008) r |= 0x0080;      // Start
    if (~input.kcode & 0x00020000) r |= 0x8000;  // LT
    if (~input.kcode & 0x00040000) r |= 0x1000;  // RT

    if (input.fullAxes[0] + 128 <= 128 - 0x20) r |= 0x0008;  // left
    if (input.fullAxes[0] + 128 >= 128 + 0x20) r |= 0x0004;  // right
    if (input.fullAxes[1] + 128 <= 128 - 0x20) r |= 0x0020;  // up
    if (input.fullAxes[1] + 128 >= 128 + 0x20) r |= 0x0010;  // down
    return r;
}

ImVec2 fromCenter(float x, float y) {
    const auto W = ImGui::GetIO().DisplaySize.x;
    const auto H = ImGui::GetIO().DisplaySize.y;
    const auto S = std::min(W / 640.f, H / 480.f);
    const auto CX = W / 2.f;
    const auto CY = H / 2.f;
    return ImVec2(CX + (x * S), CY + (y * S));
}

float scaled(float size) {
    const auto W = ImGui::GetIO().DisplaySize.x;
    const auto H = ImGui::GetIO().DisplaySize.y;
    const auto S = std::min(W / 640.f, H / 480.f);
    return S * size;
}

ImColor msColor(int ms) {
    if (ms <= 30) return ImColor(87, 213, 213);
    if (ms <= 60) return ImColor(0, 255, 149);
    if (ms <= 90) return ImColor(255, 255, 0);
    if (ms <= 120) return ImColor(255, 170, 0);
    if (ms <= 150) return ImColor(255, 0, 0);
    return ImColor(128, 128, 128);
}
}  // namespace

void GdxsvBackendRollback::DisplayOSD() {
    const auto ms = ping_pong_.ElapsedMs();
    if (2000 < ms && ms < 6500) {
        uint8_t matrix[4][4];
        ping_pong_.GetRttMatrix(matrix);

        const auto W = ImGui::GetIO().DisplaySize.x;
        const auto H = ImGui::GetIO().DisplaySize.y;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f),
                                ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(W, H));
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::Begin("##gdxsvosd", NULL,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar |
                         ImGuiWindowFlags_NoInputs);

        ImDrawList *draw_list = ImGui::GetWindowDrawList();
        ImVec2 points[] = {fromCenter(-48, -55), fromCenter(48, -55), fromCenter(-48, 55), fromCenter(48, 55)};
        for (int i = 0; i < 4; ++i) {
            auto ms = matrix[matching_.peer_id()][i];
            draw_list->AddCircleFilled(points[i], scaled(10), ImColor(0, 0, 0), 12);
            draw_list->AddCircleFilled(points[i], scaled(7), msColor(ms), 12);
        }

        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleVar();
    }
}

void GdxsvBackendRollback::Reset() {
    RestorePatch();
    state_ = State::None;
    lbs_tx_reader_.Clear();
    recv_buf_.clear();
    recv_delay_ = 0;
}

void GdxsvBackendRollback::OnMainUiLoop() {
    if (state_ == State::StopEmulator) {
        emu.stop();
        state_ = State::WaitPingPong;
    }

    if (state_ == State::WaitPingPong && !ping_pong_.Running()) {
        state_ = State::StartGGPOSession;
    }

    static auto session_start_time = std::chrono::high_resolution_clock::now();
    if (state_ == State::StartGGPOSession) {
        bool ok = true;
        std::vector<std::string> ips(matching_.player_count());
        std::vector<u16> ports(matching_.player_count());
        int max_rtt = 0;
        for (int i = 0; i < matching_.player_count(); i++) {
            if (i == matching_.peer_id()) {
                ips[i] = "";
                ports[i] = port_;
            } else {
                sockaddr_in addr;
                int rtt;
                if (ping_pong_.GetAvailableAddress(i, &addr, &rtt)) {
                    max_rtt = std::max(max_rtt, rtt);
                    char str[INET_ADDRSTRLEN] = {};
                    inet_ntop(AF_INET, &(addr.sin_addr), str, INET_ADDRSTRLEN);
                    ips[i] = str;
                    ports[i] = ntohs(addr.sin_port);
                } else {
                    ok = false;
                }
            }
        }

        if (ok) {
            int delay = std::max(2, int(max_rtt / 2.0 / 16.66 + 0.5));
            NOTICE_LOG(COMMON, "max_rtt=%d delay=%d", max_rtt, delay);
            config::GGPOEnable.override(1);
            config::GGPODelay.override(delay);
            start_network_ = ggpo::gdxsvStartNetwork(matching_.battle_code().c_str(), matching_.peer_id(), ips, ports);
            ggpo::receiveChatMessages(nullptr);
            session_start_time = std::chrono::high_resolution_clock::now();
            state_ = State::WaitGGPOSession;
        } else {
            // TODO: error handle
            emu.start();
            state_ = State::End;
        }
    }

    if (state_ == State::WaitGGPOSession) {
        auto now = std::chrono::high_resolution_clock::now();
        auto timeout = 5000 <= std::chrono::duration_cast<std::chrono::milliseconds>(now - session_start_time).count();
        if (timeout) {
            ggpo::stopSession();
        }

        if (start_network_.valid() &&
            start_network_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            start_network_ = std::future<bool>();
            if (!ggpo::active()) {
                NOTICE_LOG(COMMON, "StartNetwork failure");
            }
            state_ = State::McsInBattle;
            emu.start();
        }
    }

    const int COM_R_No0 = 0x0c391d79;  // TODO:disk2
    if (gdxsv_ReadMem8(COM_R_No0) == 4 && gdxsv_ReadMem8(COM_R_No0 + 5) == 3 && ggpo::active()) {
        ggpo::stopSession();
        state_ = State::End;
    }
}

bool GdxsvBackendRollback::StartLocalTest(const char *param) {
    auto args = std::string(param);
    int me = 0;
    if (0 < args.size() && '1' <= args[0] && args[0] <= '4') {
        me = args[0] - '1';
    }
    Reset();
    DummyRuleData[6] = 1;
    DummyRuleData[7] = 0;
    DummyRuleData[8] = 1;
    DummyRuleData[9] = 0;
    state_ = State::StartLocalTest;
    maxlag_ = 0;
    matching_.set_battle_code("0123456");
    matching_.set_peer_id(me);
    matching_.set_session_id(12345);
    matching_.set_timeout_min_ms(1000);
    matching_.set_timeout_max_ms(10000);
    matching_.set_player_count(4);
    for (int i = 0; i < 4; i++) {
        proto::PlayerAddress player{};
        player.set_ip("127.0.0.1");
        player.set_port(10010 + i);
        player.set_user_id(std::to_string(i));
        player.set_peer_id(i);
        matching_.mutable_candidates()->Add(std::move(player));
    }
    Prepare(matching_, 10010 + me);
    NOTICE_LOG(COMMON, "RollbackNet StartLocalTest %d", me);
    return true;
}

void GdxsvBackendRollback::Prepare(const proto::P2PMatching &matching, int port) {
    matching_ = matching;
    port_ = port;

    ping_pong_.Reset();
    for (const auto &c : matching.candidates()) {
        if (c.peer_id() != matching_.peer_id()) {
            ping_pong_.AddCandidate(c.user_id(), c.peer_id(), c.ip(), c.port());
        }
    }
    ping_pong_.Start(matching.session_id(), matching.peer_id(), port, matching.timeout_min_ms(),
                     matching.timeout_max_ms());
}

void GdxsvBackendRollback::Open() {
    recv_buf_.assign({0x0e, 0x61, 0x00, 0x22, 0x10, 0x31, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd});
    state_ = State::McsSessionExchange;
    maxlag_ = 0;
    ApplyPatch(true);
}

void GdxsvBackendRollback::Close() {
    ggpo::stopSession();
    RestorePatch();
    state_ = State::End;
}

u32 GdxsvBackendRollback::OnSockWrite(u32 addr, u32 size) {
    if (state_ <= State::LbsStartBattleFlow) {
        u8 buf[InetBufSize];
        for (int i = 0; i < size; ++i) {
            buf[i] = gdxsv_ReadMem8(addr + i);
        }

        lbs_tx_reader_.Write((const char *)buf, size);
        ProcessLbsMessage();
    }

    ApplyPatch(false);
    return size;
}

u32 GdxsvBackendRollback::OnSockRead(u32 addr, u32 size) {
    if (state_ <= State::LbsStartBattleFlow) {
        ProcessLbsMessage();
    } else {
        int frame = 0;
        ggpo::getCurrentFrame(&frame);
        const int InetBuf = 0x0c3ab984;           // TODO: disk2
        const int ConnectionStatus = 0x0c3abb84;  // TODO: disk2
        NOTICE_LOG(COMMON, "[FRAME:%4d :RBK=%d] State=%d OnSockRead CONNECTION: %d %d", frame, ggpo::rollbacking(),
                   state_, gdxsv_ReadMem16(ConnectionStatus), gdxsv_ReadMem16(ConnectionStatus + 4));

        // Fast disconnect
        if (gdxsv_ReadMem16(ConnectionStatus + 4) < 10) {
            for (int i = 0; i < matching_.player_count(); ++i) {
                if (!ggpo::isConnected(i)) {
                    gdxsv_WriteMem16(ConnectionStatus + 4, 0x0a);
                    ggpo::setExInput(ExInputNone);
                    break;
                }
            }
        }

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

            switch (msg.Type()) {
                case McsMessage::MsgType::ConnectionIdMsg:
                    state_ = State::StopEmulator;
                    break;
                case McsMessage::MsgType::IntroMsg:
                    for (int i = 0; i < matching_.player_count(); i++) {
                        if (i != matching_.peer_id()) {
                            auto intro_msg = McsMessage::Create(McsMessage::MsgType::IntroMsg, i);
                            std::copy(intro_msg.body.begin(), intro_msg.body.end(), std::back_inserter(recv_buf_));
                        }
                    }
                    break;
                case McsMessage::MsgType::IntroMsgReturn:
                    for (int i = 0; i < matching_.player_count(); i++) {
                        if (i != matching_.peer_id()) {
                            auto intro_msg = McsMessage::Create(McsMessage::MsgType::IntroMsgReturn, i);
                            std::copy(intro_msg.body.begin(), intro_msg.body.end(), std::back_inserter(recv_buf_));
                        }
                    }
                    break;
                case McsMessage::MsgType::PingMsg:
                    for (int i = 0; i < matching_.player_count(); i++) {
                        if (i != matching_.peer_id()) {
                            auto pong_msg = McsMessage::Create(McsMessage::MsgType::PongMsg, i);
                            pong_msg.SetPongTo(matching_.peer_id());
                            pong_msg.PongCount(msg.PingCount());
                            std::copy(pong_msg.body.begin(), pong_msg.body.end(), std::back_inserter(recv_buf_));
                        }
                    }
                    break;
                case McsMessage::MsgType::PongMsg:
                    break;
                case McsMessage::MsgType::StartMsg:
                    if (!ggpo::rollbacking()) {
                        ggpo::setExInput(ExInputWaitStart);
                        NOTICE_LOG(COMMON, "StartMsg KeyFrame:%d", frame);
                    }
                    break;
                case McsMessage::MsgType::ForceMsg:
                    break;
                case McsMessage::MsgType::KeyMsg1:
                    NOTICE_LOG(COMMON, "KeyMsg1: %02x%02x", msg.body[2], msg.body[3]);

                    for (int i = 0; i < matching_.player_count(); ++i) {
                        if (ggpo::isConnected(i)) {
                            auto msg = McsMessage::Create(McsMessage::KeyMsg1, i);
                            auto input = convertInput(mapleInputState[i]);
                            msg.body[2] = input >> 8 & 0xff;
                            msg.body[3] = input & 0xff;
                            if (i == 0) NOTICE_LOG(COMMON, "ModMsg1: %02x%02x", msg.body[2], msg.body[3]);
                            std::copy(msg.body.begin(), msg.body.end(), std::back_inserter(recv_buf_));
                        }
                    }
                    break;
                case McsMessage::MsgType::KeyMsg2:
                    verify(false);
                    break;
                case McsMessage::MsgType::LoadStartMsg:
                    // It will be dropped because InetBuf is cleared.
                    break;
                case McsMessage::MsgType::LoadEndMsg:
                    for (int i = 0; i < matching_.player_count(); i++) {
                        if (i != matching_.peer_id()) {
                            auto a = McsMessage::Create(McsMessage::MsgType::LoadStartMsg, i);
                            std::copy(a.body.begin(), a.body.end(), std::back_inserter(recv_buf_));
                        }
                    }

                    if (!ggpo::rollbacking()) {
                        ggpo::setExInput(ExInputWaitLoadEnd);
                        NOTICE_LOG(COMMON, "LoadEndMsg KeyFrame:%d", frame);
                    }
                    break;
                default:
                    WARN_LOG(COMMON, "unhandled mcs msg: %s", McsMessage::MsgTypeName(msg.Type()));
                    WARN_LOG(COMMON, "%s", msg.ToHex().c_str());
                    break;
            }

            verify(recv_buf_.size() <= size);
        }

        if (mapleInputState[0].exInput) {
            auto exInput = mapleInputState[0].exInput;
            bool ok = true;
            for (int i = 0; i < matching_.player_count(); i++) {
                ok &= mapleInputState[i].exInput == exInput;
            }
            if (ok && exInput == ExInputWaitStart) {
                NOTICE_LOG(COMMON, "StartMsg Join:%d", frame);
                for (int i = 0; i < matching_.player_count(); i++) {
                    if (i != matching_.peer_id()) {
                        auto start_msg = McsMessage::Create(McsMessage::MsgType::StartMsg, i);
                        std::copy(start_msg.body.begin(), start_msg.body.end(), std::back_inserter(recv_buf_));
                    }
                }
                if (!ggpo::rollbacking()) {
                    ggpo::setExInput(ExInputNone);
                }
            }
            if (ok && exInput == ExInputWaitLoadEnd) {
                NOTICE_LOG(COMMON, "LoadEndMsg Join:%d", frame);
                for (int i = 0; i < matching_.player_count(); i++) {
                    if (i != matching_.peer_id()) {
                        auto b = McsMessage::Create(McsMessage::MsgType::LoadEndMsg, i);
                        std::copy(b.body.begin(), b.body.end(), std::back_inserter(recv_buf_));
                    }
                }
                if (!ggpo::rollbacking()) {
                    ggpo::setExInput(ExInputNone);
                }
            }
        }
        verify(recv_buf_.size() <= size);

        NOTICE_LOG(COMMON, "[FRAME:%4d :RBK=%d] OnSockRead CONNECTION: %d %d", frame, ggpo::rollbacking(),
                   gdxsv_ReadMem16(ConnectionStatus), gdxsv_ReadMem16(ConnectionStatus + 4));
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
        NOTICE_LOG(COMMON, "%s", msg.to_hex().c_str());
        if (state_ == State::StartLocalTest) {
            state_ = State::LbsStartBattleFlow;
        }

        if (msg.command == LbsMessage::lbsLobbyMatchingEntry) {
            LbsMessage::SvAnswer(msg).Serialize(recv_buf_);
            LbsMessage::SvNotice(LbsMessage::lbsReadyBattle).Serialize(recv_buf_);
        }

        if (msg.command == LbsMessage::lbsAskMatchingJoin) {
            LbsMessage::SvAnswer(msg).Write8(matching_.player_count())->Serialize(recv_buf_);
        }

        if (msg.command == LbsMessage::lbsAskPlayerSide) {
            LbsMessage::SvAnswer(msg).Write8(matching_.peer_id() + 1)->Serialize(recv_buf_);
        }

        if (msg.command == LbsMessage::lbsAskPlayerInfo) {
            int pos = msg.Read8();
            DummyGameParam[16] = '0' + pos;
            DummyGameParam[17] = 0;
            LbsMessage::SvAnswer(msg)
                .Write8(pos)
                ->WriteString("USER0" + std::to_string(pos))
                ->WriteString("USER0" + std::to_string(pos))
                ->WriteBytes(reinterpret_cast<char *>(DummyGameParam), sizeof(DummyGameParam))
                ->Write16(1)
                ->Write16(0)
                ->Write16(0)
                ->Write16(0)
                ->Write16(0)
                ->Write16(0)
                ->Write16(1 + (pos - 1) / 2)
                ->Write16(0)
                ->Serialize(recv_buf_);
        }

        if (msg.command == LbsMessage::lbsAskRuleData) {
            LbsMessage::SvAnswer(msg).WriteBytes((char *)DummyRuleData, sizeof(DummyRuleData))->Serialize(recv_buf_);
        }

        if (msg.command == LbsMessage::lbsAskBattleCode) {
            LbsMessage::SvAnswer(msg).WriteString("012345")->Serialize(recv_buf_);
        }

        if (msg.command == LbsMessage::lbsAskMcsVersion) {
            LbsMessage::SvAnswer(msg).Write8(10)->Serialize(recv_buf_);
        }

        if (msg.command == LbsMessage::lbsAskMcsAddress) {
            LbsMessage::SvAnswer(msg)
                .Write16(4)
                ->Write8(255)
                ->Write8(255)
                ->Write8(255)
                ->Write8(255)
                ->Write16(2)
                ->Write16(255)
                ->Serialize(recv_buf_);
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

    gdxsv.WritePatch();

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
