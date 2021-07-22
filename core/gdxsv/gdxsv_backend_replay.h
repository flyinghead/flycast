// Mock network implementation to replay local battle log

#include <array>
#include <vector>
#include <queue>
#include "gdxsv.pb.h"
#include "lbs_message.h"
#include "mcs_message.h"

class GdxsvBackendReplay {
public:
    GdxsvBackendReplay(const std::map<std::string, u32> &symbols, std::atomic<int> &maxlag)
            : symbols_(symbols), maxlag_(maxlag) {
    }

    enum class State {
        None,
        Start,
        LoadState,
        LbsStartBattleFlow,
        McsWaitJoin,
        McsSessionExchange,
        McsInBattle,
        End,
    };

    void Reset() {
        state_ = State::None;
        lbs_tx_reader_.Clear();
    }

    bool StartFile(const char *path) {
        FILE *fp = nowide::fopen(path, "rb");
        if (fp == nullptr) {
            NOTICE_LOG(COMMON, "fopen failed");
        }

        bool ok = log_file_.ParseFromFileDescriptor(fp->_file);
        if (!ok) {
            NOTICE_LOG(COMMON, "ParseFromFileDescriptor failed");
        }

        NOTICE_LOG(COMMON, "game_disk = %s", log_file_.game_disk().c_str());
        fclose(fp);

        msg_list_.clear();
        McsMessageReader r;
        McsMessage msg;
        for (int i = 0; i < log_file_.battle_data_size(); ++i) {
            const auto &data = log_file_.battle_data(i);
            r.Write(data.body().data(), data.body().size());
            while (r.Read(msg)) {
                msg_list_.emplace_back(msg);
            }
        }

        std::fill(start_index_.begin(), start_index_.end(), 0);

        state_ = State::Start;
        maxlag_ = 2;
        return true;
    }

    void Open() {
        recv_buf_.assign({0x0e, 0x61, 0x00, 0x22, 0x10, 0x31, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd});
        state_ = State::McsSessionExchange;
        ApplyPatch(true);
    }

    void Close() {
        RestorePatch();
    }

    void OnGameWrite() {
        gdx_queue q{};
        u32 gdx_txq_addr = symbols_.at("gdx_txq");
        if (gdx_txq_addr == 0) return;
        q.head = ReadMem16_nommu(gdx_txq_addr);
        q.tail = ReadMem16_nommu(gdx_txq_addr + 2);
        u32 buf_addr = gdx_txq_addr + 4;

        int n = gdx_queue_size(&q);
        if (0 < n) {
            u8 buf[GDX_QUEUE_SIZE] = {};
            for (int i = 0; i < n; ++i) {
                buf[i] = ReadMem8_nommu(buf_addr + q.head);
                gdx_queue_pop(&q); // dummy pop
            }
            WriteMem16_nommu(gdx_txq_addr, q.head);
            if (state_ <= State::LbsStartBattleFlow) {
                lbs_tx_reader_.Write((const char *) buf, n);
            } else {
                mcs_tx_reader_.Write((const char *) buf, n);
            }
        }

        ApplyPatch(false);
    }

    void OnGameRead() {
        static int connection_status = 0;
        int new_connection_status = ReadMem8_nommu(0x0c3abb88);
        if (connection_status != new_connection_status) {
            NOTICE_LOG(COMMON, "CON_ST: %x -> %x", connection_status, new_connection_status);
            connection_status = new_connection_status;
        }

        if (state_ <= State::LbsStartBattleFlow) {
            ProcessLbsMessage();
        } else {
            ProcessMcsMessage();
        }

        if (recv_buf_.empty()) {
            return;
        }

        if (0 < recv_delay_) {
            recv_delay_--;
            return;
        }

        u32 gdx_rxq_addr = symbols_.at("gdx_rxq");
        gdx_queue q{};
        q.head = ReadMem16_nommu(gdx_rxq_addr);
        q.tail = ReadMem16_nommu(gdx_rxq_addr + 2);
        u32 buf_addr = gdx_rxq_addr + 4;

        if (gdx_queue_avail(&q) < GDX_QUEUE_SIZE / 2) {
            recv_delay_ = 1;
            return;
        }

        int n = std::min<int>(recv_buf_.size(), static_cast<int>(gdx_queue_avail(&q)));
        std::string hexstr(n * 2, ' ');
        for (int i = 0; i < n; ++i) {
            std::sprintf(&hexstr[0] + i * 2, "%02x", recv_buf_[i]);
        }
        NOTICE_LOG(COMMON, "write %s", hexstr.c_str());
        if (0 < n) {
            for (int i = 0; i < n; ++i) {
                WriteMem8_nommu(buf_addr + q.tail, recv_buf_.front());
                recv_buf_.pop_front();
                gdx_queue_push(&q, 0); // dummy push
            }
            WriteMem16_nommu(gdx_rxq_addr + 2, q.tail);
        }
    }

private:
    void PrepareKeyMsgIndex() {
        for (int p = 0; p < log_file_.users_size(); ++p) {
            key_msg_index_[p].clear();

            for (int i = start_index_[p]; i < msg_list_.size(); ++i) {
                const auto &msg = msg_list_[i];
                if (msg.sender == p) {
                    if (!key_msg_index_[p].empty()) {
                        if (msg.type == McsMessage::MsgType::StartMsg) {
                            start_index_[p] = i + 1;
                            break;
                        }
                    }

                    if (msg.type == McsMessage::MsgType::KeyMsg) {
                        key_msg_index_[p].emplace_back(i);
                    }
                }
            }
        }
    }

    void ProcessLbsMessage() {
        LbsMessage msg;
        if (lbs_tx_reader_.Read(msg)) {
            NOTICE_LOG(COMMON, "RECV cmd=%04x seq=%d", msg.command, msg.seq);

            if (msg.command == LbsMessage::lbsLobbyMatchingEntry) {
                LbsMessage::SvAnswer(msg).Serialize(recv_buf_);
                LbsMessage::SvNotice(LbsMessage::lbsReadyBattle).Serialize(recv_buf_);
            }

            if (msg.command == LbsMessage::lbsAskMatchingJoin) {
                int n = log_file_.users_size();
                LbsMessage::SvAnswer(msg).Write8(n)->Serialize(recv_buf_);
            }

            if (msg.command == LbsMessage::lbsAskPlayerSide) {
                // camera player id
                me = 0;
                LbsMessage::SvAnswer(msg).Write8(me + 1)->Serialize(recv_buf_);
            }

            if (msg.command == LbsMessage::lbsAskPlayerInfo) {
                int pos = msg.Read8();
                const auto &user = log_file_.users(pos - 1);
                NOTICE_LOG(COMMON, "pos=%d game_param.size=%d", pos, user.game_param().size());
                LbsMessage::SvAnswer(msg).
                        Write8(pos)->
                        WriteString(user.user_id())->
                        WriteString(user.user_id())->
                        // WriteString(user.user_name())-> // TODO: need UTF8 -> SJIS
                        WriteBytes(user.game_param().data(), user.game_param().size())->
                        Write16(0)-> // grade
                        Write16(user.win_count())->
                        Write16(user.lose_count())->
                        Write16(0)->
                        Write16(user.battle_count() - user.win_count() - user.lose_count())->
                        Write16(0)->
                        Write16(1 + (pos - 1) / 2)-> // TODO TEAM
                        Write16(0)->
                        Serialize(recv_buf_);
            }

            if (msg.command == LbsMessage::lbsAskRuleData) {
                LbsMessage::SvAnswer(msg).
                        WriteBytes(log_file_.rule_bin())->
                        Serialize(recv_buf_);
            }

            if (msg.command == LbsMessage::lbsAskBattleCode) {
                LbsMessage::SvAnswer(msg).
                        WriteString(log_file_.battle_code())->
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

    void ProcessMcsMessage() {
        McsMessage msg;
        if (mcs_tx_reader_.Read(msg)) {
            NOTICE_LOG(COMMON, "Read %s %s", McsMessage::MsgTypeName(msg.type), msg.to_hex().c_str());
            switch (msg.type) {
                case McsMessage::MsgType::ConnectionIdMsg:
                    state_ = State::McsInBattle;
                    break;
                case McsMessage::MsgType::IntroMsg:
                    for (int i = 0; i < log_file_.users_size(); ++i) {
                        if (i != me) {
                            auto intro_msg = McsMessage::Create(McsMessage::MsgType::IntroMsg, i);
                            std::copy(intro_msg.body.begin(), intro_msg.body.end(), std::back_inserter(recv_buf_));
                        }
                    }
                    break;
                case McsMessage::MsgType::IntroMsgReturn:
                    for (int i = 0; i < log_file_.users_size(); ++i) {
                        if (i != me) {
                            auto intro_msg = McsMessage::Create(McsMessage::MsgType::IntroMsgReturn, i);
                            std::copy(intro_msg.body.begin(), intro_msg.body.end(), std::back_inserter(recv_buf_));
                        }
                    }
                    break;
                case McsMessage::MsgType::PingMsg:
                    for (int i = 0; i < log_file_.users_size(); ++i) {
                        if (i != me) {
                            auto pong_msg = McsMessage::Create(McsMessage::MsgType::PongMsg, i);
                            pong_msg.SetPongTo(me);
                            pong_msg.SetPongCount(msg.PingCount());
                            std::copy(pong_msg.body.begin(), pong_msg.body.end(), std::back_inserter(recv_buf_));
                        }
                    }
                    break;
                case McsMessage::MsgType::PongMsg:
                    break;
                case McsMessage::MsgType::StartMsg:
                    for (int i = 0; i < log_file_.users_size(); ++i) {
                        auto start_msg = McsMessage::Create(McsMessage::MsgType::StartMsg, i);
                        std::copy(start_msg.body.begin(), start_msg.body.end(), std::back_inserter(recv_buf_));
                    }
                    PrepareKeyMsgIndex();
                    break;
                case McsMessage::MsgType::ForceMsg:
                    break;
                case McsMessage::MsgType::KeyMsg:
                    for (int i = 0; i < log_file_.users_size(); ++i) {
                        auto key_msg = msg_list_[key_msg_index_[i][msg.HeadFrame() / 2]];
                        std::copy(key_msg.body.begin(), key_msg.body.end(), std::back_inserter(recv_buf_));
                        verify(key_msg.HeadFrame() == msg.HeadFrame());
                    }
                    break;
                case McsMessage::MsgType::LoadStartMsg:
                    for (int i = 0; i < log_file_.users_size(); ++i) {
                        if (i != me) {
                            auto load_start_msg = McsMessage::Create(McsMessage::MsgType::LoadStartMsg, i);
                            std::copy(load_start_msg.body.begin(), load_start_msg.body.end(),
                                      std::back_inserter(recv_buf_));
                        }
                    }
                    break;
                case McsMessage::MsgType::LoadEndMsg:
                    for (int i = 0; i < log_file_.users_size(); ++i) {
                        if (i != me) {
                            auto load_end_msg = McsMessage::Create(McsMessage::MsgType::LoadEndMsg, i);
                            std::copy(load_end_msg.body.begin(), load_end_msg.body.end(),
                                      std::back_inserter(recv_buf_));
                        }
                    }
                    break;
                default:
                    WARN_LOG(COMMON, "unhandled mcs msg: %s", McsMessage::MsgTypeName(msg.type));
                    WARN_LOG(COMMON, "%s", msg.to_hex().c_str());
                    break;
            }
        }
    }

    void ApplyPatch(bool first_time) {
        // Skip MsgPush
        // TODO: Restore the patch
        WriteMem16_nommu(0x8c045f64, 9);
        // WriteMem16_nommu(0x8c045e70, 9);

        for (int i = 0; i < log_file_.patches_size(); ++i) {
            if (log_file_.patches(i).write_once() && !first_time) {
                continue;
            }

            for (int j = 0; j < log_file_.patches(i).codes_size(); ++j) {
                const auto &code = log_file_.patches(i).codes(j);
                if (code.size() == 8) {
                    WriteMem8_nommu(code.address(), code.changed());
                }
                if (code.size() == 16) {
                    WriteMem16_nommu(code.address(), code.changed());
                }
                if (code.size() == 32) {
                    WriteMem32_nommu(code.address(), code.changed());
                }
            }
        }
    }

    void RestorePatch() {
        for (int i = 0; i < log_file_.patches_size(); ++i) {
            for (int j = 0; j < log_file_.patches(i).codes_size(); ++j) {
                const auto &code = log_file_.patches(i).codes(j);
                if (code.size() == 8) {
                    WriteMem8_nommu(code.address(), code.original());
                }
                if (code.size() == 16) {
                    WriteMem16_nommu(code.address(), code.original());
                }
                if (code.size() == 32) {
                    WriteMem32_nommu(code.address(), code.original());
                }
            }
        }
    }

    const std::map<std::string, u32> &symbols_;
    std::atomic<int> &maxlag_;
    State state_;
    LbsMessageReader lbs_tx_reader_;
    McsMessageReader mcs_tx_reader_;
    proto::BattleLogFile log_file_;
    std::deque<u8> recv_buf_;
    int recv_delay_;
    int me = 0;
    std::vector<McsMessage> msg_list_;
    std::array<int, 4> start_index_;
    std::array<std::vector<int>, 4> key_msg_index_;
};

