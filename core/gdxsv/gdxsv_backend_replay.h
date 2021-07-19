// Mock network implementation to replay local battle log

#include "gdxsv.pb.h"
#include "lbs_message.h"
#include "mcs_message.h"

class GdxsvBackendReplay {
public:
    GdxsvBackendReplay(const std::map<std::string, u32> &symbols) : symbols_(symbols) {
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
        if (fp != nullptr) {
            fclose(fp);
        }

        replay_data_.clear();
        for (int i = 0; i < log_file_.battle_data_size(); ++i) {
            const auto &data = log_file_.battle_data(i);
            replay_data_.push_back(data);
        }

        NOTICE_LOG(COMMON, "log size = %d", replay_data_.size());

        state_ = State::Start;
        return true;
    }

    void Open() {
        recv_buf_.assign({0x0e, 0x61, 0x00, 0x22, 0x10, 0x31, 0x66,
                          0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd});
        state_ = State::McsSessionExchange;
    }

    void Close() {
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

        int n = std::min<int>(recv_buf_.size(), static_cast<int>(gdx_queue_avail(&q)));
        /*
        std::string hexstr(n * 2, ' ');
        for (int i = 0; i < n; ++i) {
            std::sprintf(&hexstr[0] + i * 2, "%02x", recv_buf_[i]);
        }
        NOTICE_LOG(COMMON, "%s", hexstr.c_str());
         */
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
                int me = 0;
                LbsMessage::SvAnswer(msg).Write8(me + 1)->Serialize(recv_buf_);
            }

            if (msg.command == LbsMessage::lbsAskPlayerInfo) {
                int pos = msg.Read8();
                NOTICE_LOG(COMMON, "pos=%d", pos);
                const auto &user = log_file_.users(pos - 1);
                LbsMessage::SvAnswer(msg).
                        Write8(pos)->
                        WriteString(user.user_id())->
                        WriteString(user.user_name())-> // TODO: need UTF8 -> SJIS
                        WriteString(user.game_param())->
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
            switch (msg.type) {
                case McsMessage::MsgType::ConnectionIdMsg:
                    state_ = State::McsInBattle;
                    break;
                case McsMessage::MsgType::IntroMsg:
                    //TODO
                    break;
                default:
                    WARN_LOG(COMMON, "unhandled mcs msg: %s", McsMessage::MsgTypeName(msg.type));
                    WARN_LOG(COMMON, "%s", msg.to_hex().c_str());
                    break;
            }
        }
    }

    const std::map<std::string, u32> &symbols_;
    State state_;
    LbsMessageReader lbs_tx_reader_;
    McsMessageReader mcs_tx_reader_;
    proto::BattleLogFile log_file_;
    std::deque<u8> recv_buf_;
    int recv_delay_;
    std::vector<proto::BattleLogMessage> replay_data_;
};

