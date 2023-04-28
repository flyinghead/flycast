#include "gdxsv_backend_rollback.h"

#include <algorithm>
#include <future>
#include <map>
#include <string>
#include <vector>

#include "emulator.h"
#include "gdx_rpc.h"
#include "gdxsv.h"
#include "gdxsv.pb.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "input/gamepad_device.h"
#include "libs.h"
#include "network/ggpo.h"
#include "network/net_platform.h"
#include "rend/gui_util.h"
#include "rend/transform_matrix.h"

namespace {
u8 DummyGameParam[] = {0x00, 0x00, 0x01, 0x00, 0x03, 0x00, 0x02, 0x00, 0x05, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x83,
					   0x76, 0x83, 0x8c, 0x83, 0x43, 0x83, 0x84, 0x81, 0x5b, 0x82, 0x50, 0x00, 0x00, 0x00, 0x00, 0x07};
u8 DummyRuleData[] = {0x03, 0x02, 0x03, 0x00, 0x00, 0x01, 0x58, 0x02, 0x58, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff,
					  0x3f, 0xff, 0xff, 0xff, 0x3f, 0x00, 0x00, 0xff, 0x01, 0xff, 0xff, 0xff, 0x3f, 0xff, 0xff, 0xff, 0x3f, 0x00};

const u16 ExInputNone = 0;
const u16 ExInputWaitStart = 1;
const u16 ExInputWaitLoadEnd = 2;

// maple input to mcs pad input
u16 convertInput(MapleInputState input) {
	u16 r = 0;
	if (~input.kcode & 0x0004) r |= 0x4000;					 // A
	if (~input.kcode & 0x0002) r |= 0x2000;					 // B
	if (~input.kcode & 0x0001) r |= 0x8000;					 // C
	if (~input.kcode & 0x0400) r |= 0x0002;					 // X
	if (~input.kcode & 0x0200) r |= 0x0001;					 // Y
	if (~input.kcode & 0x0100) r |= 0x1000;					 // Z
	if (~input.kcode & 0x0010) r |= 0x0020;					 // up
	if (~input.kcode & 0x0020) r |= 0x0010;					 // down
	if (~input.kcode & 0x0080) r |= 0x0004;					 // right
	if (~input.kcode & 0x0040) r |= 0x0008;					 // left
	if (~input.kcode & 0x0008) r |= 0x0080;					 // Start
	if (~input.kcode & 0x00020000) r |= 0x8000;				 // LT
	if (~input.kcode & 0x00040000) r |= 0x1000;				 // RT
	if (input.fullAxes[0] + 128 <= 128 - 0x20) r |= 0x0008;	 // left
	if (input.fullAxes[0] + 128 >= 128 + 0x20) r |= 0x0004;	 // right
	if (input.fullAxes[1] + 128 <= 128 - 0x20) r |= 0x0020;	 // up
	if (input.fullAxes[1] + 128 >= 128 + 0x20) r |= 0x0010;	 // down
	return r;
}

void drawConnectionDiagram(int elapsed, const uint8_t matrix[4][4], const std::map<int, int>& pos_to_id);
}  // namespace

void GdxsvBackendRollback::DisplayOSD() {
	const auto elapsed = ping_pong_.ElapsedMs();
	if (1550 < elapsed && elapsed < 6900) {
		uint8_t matrix[4][4] = {};
		ping_pong_.GetRttMatrix(matrix);
		std::map<int, int> id_to_team, pos_to_id;
		for (const auto& c : matching_.candidates()) {
			id_to_team[c.peer_id()] = c.team();
		}
		for (const auto& p : id_to_team) {
			int pos = (p.second == 2 ? 2 : 0);
			if (pos_to_id.find(pos) != pos_to_id.end()) pos++;
			pos_to_id[pos] = p.first;
		}
		drawConnectionDiagram(elapsed, matrix, pos_to_id);
	}
}

void GdxsvBackendRollback::Reset() {
	RestorePatch();
	state_ = State::None;
	lbs_tx_reader_.Clear();
	recv_delay_ = 0;
	port_ = 0;
	recv_buf_.clear();
	lbs_tx_reader_.Clear();
	matching_.Clear();
	report_.Clear();
	ping_pong_.Reset();
	start_network_ = std::future<bool>();
	input_logs_.clear();
	ggpo::stopSession();
}

void GdxsvBackendRollback::OnMainUiLoop() {
	const int disk = gdxsv.Disk();
	const int COM_R_No0 = disk == 1 ? 0x0c2f6639 : 0x0c391d79;
	const int ConnectionStatus = disk == 1 ? 0x0c310444 : 0x0c3abb84;
	const int NetCountDown = disk == 1 ? 0x0c310202 : 0x0c3ab942;

	if (state_ == State::StartLocalTest) {
		kcode[0] = ~0x0004;
	}

	if (state_ == State::StopEmulator) {
		NOTICE_LOG(COMMON, "StopEmulator");
		emu.stop();
		state_ = State::WaitPingPong;
	}

	if (state_ == State::WaitPingPong && !ping_pong_.Running()) {
		state_ = State::StartGGPOSession;
	}

	static auto session_start_time = std::chrono::high_resolution_clock::now();
	if (state_ == State::StartGGPOSession) {
		NOTICE_LOG(COMMON, "StartGGPOSession");
		bool ok = true;
		uint8_t rtt_matrix[4][4] = {};
		ping_pong_.GetRttMatrix(rtt_matrix);

		std::vector<std::string> ips(matching_.player_count());
		std::vector<u16> ports(matching_.player_count());
		std::vector<u8> relays(matching_.player_count());
		float max_rtt = 0;
		NOTICE_LOG(COMMON, "Peer count %d", matching_.player_count());
		for (int i = 0; i < matching_.player_count(); i++) {
			if (i == matching_.peer_id()) {
				NOTICE_LOG(COMMON, "Peer%d is self", i);
				ips[i] = "";
				ports[i] = port_;
			} else {
				sockaddr_in addr;
				float rtt;
				if (ping_pong_.GetAvailableAddress(i, &addr, &rtt)) {
					NOTICE_LOG(COMMON, "Peer%d rtt%.2f", i, rtt);
					max_rtt = std::max(max_rtt, rtt);
					char str[INET_ADDRSTRLEN] = {};
					inet_ntop(AF_INET, &(addr.sin_addr), str, INET_ADDRSTRLEN);
					ips[i] = str;
					ports[i] = ntohs(addr.sin_port);
				} else {
					NOTICE_LOG(COMMON, "No available address %d", i);
					int relay_rtt = INT_MAX;
					int relay_peer = -1;
					for (int j = 0; j < matching_.player_count(); j++) {
						if (j == i) continue;
						if (j == matching_.peer_id()) continue;
						if (rtt_matrix[matching_.peer_id()][j] && rtt_matrix[j][i]) {
							int rtt = rtt_matrix[matching_.peer_id()][j] + rtt_matrix[j][i];
							if (rtt < relay_rtt) {
								relay_rtt = rtt;
								relay_peer = j;
							}
						}
					}

					if (relay_peer != -1 && ping_pong_.GetAvailableAddress(relay_peer, &addr, &rtt)) {
						NOTICE_LOG(COMMON, "Use relay via %d", relay_peer);
						max_rtt = std::max(max_rtt, rtt + (float)rtt_matrix[relay_peer][i]);
						char str[INET_ADDRSTRLEN] = {};
						inet_ntop(AF_INET, &(addr.sin_addr), str, INET_ADDRSTRLEN);
						ips[i] = str;
						ports[i] = ntohs(addr.sin_port);
						relays[i] = true;
					} else {
						NOTICE_LOG(COMMON, "Peer %d unreachable", i);
						ok = false;
					}
				}
			}
		}

		if (ok) {
			int delay = std::max(2, std::max(config::GdxMinDelay.get(), int(max_rtt / 2.0 / 16.66 + 0.5)));
			NOTICE_LOG(COMMON, "max_rtt=%.2f delay=%d", max_rtt, delay);
			config::GGPOEnable.override(true);
			config::GGPODelay.override(delay);
			config::FixedFrequency.override(2);
			config::VSync.override(true);
			config::LimitFPS.override(false);
			config::ThreadedRendering.override(false);

			start_network_ = ggpo::gdxsvStartNetwork(matching_.battle_code().c_str(), matching_.peer_id(), ips, ports, relays);
			ggpo::receiveChatMessages(nullptr);
			session_start_time = std::chrono::high_resolution_clock::now();
			state_ = State::WaitGGPOSession;
		} else {
			gdxsv_WriteMem16(NetCountDown, 1);
			emu.start();
			state_ = State::End;
		}
	}

	if (state_ == State::WaitGGPOSession) {
		auto now = std::chrono::high_resolution_clock::now();
		auto timeout = 10000 <= std::chrono::duration_cast<std::chrono::milliseconds>(now - session_start_time).count();

		if (start_network_.valid() && start_network_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
			if (ggpo::active()) {
				start_network_ = std::future<bool>();
				state_ = State::McsInBattle;
				emu.start();
			} else {
				NOTICE_LOG(COMMON, "StartNetwork failure");
				SetCloseReason("ggpo_start_failure");
				state_ = State::End;
				emu.start();
			}
		} else if (timeout) {
			NOTICE_LOG(COMMON, "StartNetwork timeout");
			SetCloseReason("ggpo_start_timeout");
			ggpo::stopSession();
			state_ = State::End;
			emu.start();
		}
	}

	static int disconnect_frame = 0;

	// Rebattle end
	if (gdxsv_ReadMem8(COM_R_No0) == 4 && gdxsv_ReadMem8(COM_R_No0 + 5) == 3 && ggpo::active() && !ggpo::rollbacking()) {
		if (state_ != State::CloseWait) {
			ggpo::disconnect(matching_.peer_id());
			ggpo::getCurrentFrame(&disconnect_frame);
			state_ = State::CloseWait;
		}
	}

	// Friend save scene
	if (gdxsv_ReadMem8(COM_R_No0) == 4 && gdxsv_ReadMem8(COM_R_No0 + 5) == 4 && ggpo::active() && !ggpo::rollbacking()) {
		SetCloseReason("game_end");
		int frame = 0;
		ggpo::getCurrentFrame(&frame);

		if (16 < frame - disconnect_frame) {
			ggpo::stopSession();
			config::GGPOEnable.reset();
			state_ = State::End;
		}
	}

	// Fast return to lobby on error
	if (gdxsv_ReadMem16(ConnectionStatus) == 1 && gdxsv_ReadMem16(ConnectionStatus + 4) == 10 && 1 < gdxsv_ReadMem16(NetCountDown)) {
		SetCloseReason("error_fast_return");
		ggpo::stopSession();
		config::GGPOEnable.reset();
		state_ = State::End;
		gdxsv_WriteMem16(NetCountDown, 1);
	}

	if (is_local_test_ && state_ == State::End) {
		dc_exit();
	}
}

bool GdxsvBackendRollback::StartLocalTest(const char* param) {
	auto args = std::string(param);
	int me = 0;
	int n = 4;
	if (0 < args.size() && '1' <= args[0] && args[0] <= '4') {
		me = args[0] - '1';
	}
	if (2 < args.size() && args[1] == '/' && '1' <= args[2] && args[2] <= '4') {
		n = args[2] - '0';
	}

	u64 seed = cfgLoadInt64("gdxsv", "rand_input", 0);
	if (seed) {
		NOTICE_LOG(COMMON, "RandomInput Seed=%d", seed + me);
		ggpo::randomInput(true, seed + me, 0x0004 | 0x0400 | 0x0200 | 0x0010 | 0x0040);
	}
	DummyRuleData[6] = 1;
	DummyRuleData[7] = 0;
	DummyRuleData[8] = 1;
	DummyRuleData[9] = 0;

	proto::P2PMatching matching;
	matching.set_battle_code("0123456");
	matching.set_peer_id(me);
	matching.set_session_id(12345);
	matching.set_timeout_min_ms(1000);
	matching.set_timeout_max_ms(10000);
	matching.set_player_count(n);
	for (int i = 0; i < n; i++) {
		proto::PlayerAddress player{};
		player.set_ip("127.0.0.1");
		player.set_port(20010 + i);
		player.set_user_id(std::to_string(i));
		player.set_peer_id(i);
		player.set_team(i / 2 + 1);
		matching.mutable_candidates()->Add(std::move(player));
	}
	Prepare(matching, 20010 + me);
	state_ = State::StartLocalTest;
	is_local_test_ = true;
	gdxsv.maxlag_ = 0;
	gdxsv.maxrebattle_ = 1;

	if (getenv("MAXREBATTLE")) {
		gdxsv.maxrebattle_ = atoi(getenv("MAXREBATTLE"));
	}

	NOTICE_LOG(COMMON, "RollbackNet StartLocalTest %d", me);
	return true;
}

void GdxsvBackendRollback::Prepare(const proto::P2PMatching& matching, int port) {
	NOTICE_LOG(COMMON, "GdxsvBackendRollback.Prepare");
	Reset();

	matching_ = matching;
	port_ = port;

	for (const auto& c : matching.candidates()) {
		if (c.peer_id() != matching_.peer_id()) {
			ping_pong_.AddCandidate(c.user_id(), c.peer_id(), c.ip(), c.port());
		}
	}
	ping_pong_.Start(matching.session_id(), matching.peer_id(), port, matching.timeout_min_ms(), matching.timeout_max_ms());

	report_.Clear();
	report_.set_battle_code(matching.battle_code());
	report_.set_session_id(matching.session_id());
	report_.set_frame_count(0);
	report_.set_peer_id(matching.peer_id());
	report_.set_player_count(matching.player_count());
	start_at_ = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void GdxsvBackendRollback::Open() {
	NOTICE_LOG(COMMON, "GdxsvBackendRollback.Open");
	recv_buf_.assign({0x0e, 0x61, 0x00, 0x22, 0x10, 0x31, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd});
	state_ = State::McsSessionExchange;
	gdxsv.maxlag_ = 0;
	ApplyPatch(true);
}

void GdxsvBackendRollback::Close() {
	if (state_ < State::McsWaitJoin) return;
	if (state_ == State::Closed) return;

	SetCloseReason("close");
	ggpo::stopSession();
	config::GGPOEnable.reset();
	config::FixedFrequency.load();
	config::VSync.load();
	config::LimitFPS.load();
	config::ThreadedRendering.load();
	RestorePatch();
	KillTex = true;
	SaveReplay();
	state_ = State::Closed;
}

u32 GdxsvBackendRollback::OnSockWrite(u32 addr, u32 size) {
	if (state_ <= State::LbsStartBattleFlow) {
		u8 buf[InetBufSize];
		for (int i = 0; i < size; ++i) {
			buf[i] = gdxsv_ReadMem8(addr + i);
		}

		lbs_tx_reader_.Write((const char*)buf, size);
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

		const int disk = gdxsv.Disk();
		const int COM_R_No0 = disk == 1 ? 0x0c2f6639 : 0x0c391d79;
		const int ConnectionStatus = disk == 1 ? 0x0c310444 : 0x0c3abb84;
		const int InetBuf = disk == 1 ? 0x0c310244 : 0x0c3ab984;

		// Notify disconnect in game part if other player is disconnect on ggpo
		if (gdxsv_ReadMem8(COM_R_No0) == 4 && gdxsv_ReadMem8(COM_R_No0 + 5) == 0 && gdxsv_ReadMem16(ConnectionStatus + 4) < 10) {
			for (int i = 0; i < matching_.player_count(); ++i) {
				if (!ggpo::isConnected(i)) {
					SetCloseReason("player_disconnected");
					gdxsv_WriteMem16(ConnectionStatus + 4, 0x0a);
					ggpo::setExInput(ExInputNone);
					break;
				}
			}
		}

		auto inputState = mapleInputState;
		auto memExInputAddr = gdxsv.symbols_.at("rbk_ex_input");

		int msg_len = gdxsv_ReadMem8(InetBuf);
		if (0 < msg_len) {
			if (msg_len == 0x82) {
				msg_len = 20;
			}
			McsMessage msg;
			msg.body.resize(msg_len);
			for (int i = 0; i < msg_len; i++) {
				msg.body[i] = gdxsv_ReadMem8(InetBuf + i);
				gdxsv_WriteMem8(InetBuf + i, 0);
			}

			if (msg.Type() == McsMessage::ConnectionIdMsg) {
				state_ = State::StopEmulator;
			}

			if (msg.Type() == McsMessage::IntroMsg) {
				for (int i = 0; i < matching_.player_count(); i++) {
					if (i == matching_.peer_id()) continue;
					auto a = McsMessage::Create(McsMessage::IntroMsg, i);
					std::copy(a.body.begin(), a.body.end(), std::back_inserter(recv_buf_));
				}
			}

			if (msg.Type() == McsMessage::IntroMsgReturn) {
				for (int i = 0; i < matching_.player_count(); i++) {
					if (i == matching_.peer_id()) continue;
					auto a = McsMessage::Create(McsMessage::IntroMsgReturn, i);
					std::copy(a.body.begin(), a.body.end(), std::back_inserter(recv_buf_));
				}
			}

			if (msg.Type() == McsMessage::PingMsg) {
				for (int i = 0; i < matching_.player_count(); i++) {
					if (i == matching_.peer_id()) continue;
					auto a = McsMessage::Create(McsMessage::PongMsg, i);
					a.SetPongTo(matching_.peer_id());
					a.PongCount(msg.PingCount());
					std::copy(a.body.begin(), a.body.end(), std::back_inserter(recv_buf_));
				}
			}

			if (msg.Type() == McsMessage::StartMsg) {
				gdxsv_WriteMem16(memExInputAddr, ExInputWaitStart);
				if (!ggpo::rollbacking()) {
					ggpo::setExInput(ExInputWaitStart);
					NOTICE_LOG(COMMON, "StartMsg KeyFrame:%d", frame);
				}
			}

			if (msg.Type() == McsMessage::KeyMsg1) {
				u64 inputs = 0;
				for (int i = 0; i < matching_.player_count(); ++i) {
					auto a = McsMessage::Create(McsMessage::KeyMsg1, i);
					auto input = convertInput(inputState[i]);
					a.body[2] = input >> 8 & 0xff;
					a.body[3] = input & 0xff;
					std::copy(a.body.begin(), a.body.end(), std::back_inserter(recv_buf_));
					inputs |= u64(input) << (i * 16);
				}

				while (!input_logs_.empty() && frame <= input_logs_.back().first) {
					input_logs_.pop_back();
				}
				input_logs_.emplace_back(frame, inputs);
			}

			if (msg.Type() == McsMessage::LoadEndMsg) {
				for (int i = 0; i < matching_.player_count(); i++) {
					if (i == matching_.peer_id()) continue;
					auto a = McsMessage::Create(McsMessage::LoadStartMsg, i);
					std::copy(a.body.begin(), a.body.end(), std::back_inserter(recv_buf_));
				}

				gdxsv_WriteMem16(memExInputAddr, ExInputWaitLoadEnd);
				if (!ggpo::rollbacking()) {
					ggpo::setExInput(ExInputWaitLoadEnd);
					NOTICE_LOG(COMMON, "LoadEndMsg KeyFrame:%d", frame);
				}
			}

			verify(recv_buf_.size() <= size);
		}

		if (gdxsv_ReadMem16(memExInputAddr) != ExInputNone) {
			auto exInput = gdxsv_ReadMem16(memExInputAddr);
			bool ok = true;
			for (int i = 0; i < matching_.player_count(); i++) {
				ok &= inputState[i].exInput == exInput;
			}

			if (ok && exInput == ExInputWaitStart) {
				NOTICE_LOG(COMMON, "StartMsg Join:%d", frame);
				gdxsv_WriteMem16(memExInputAddr, ExInputNone);
				if (!ggpo::rollbacking()) {
					ggpo::setExInput(ExInputNone);
				}
				for (int i = 0; i < matching_.player_count(); i++) {
					if (i == matching_.peer_id()) continue;
					auto a = McsMessage::Create(McsMessage::MsgType::StartMsg, i);
					std::copy(a.body.begin(), a.body.end(), std::back_inserter(recv_buf_));
				}
			}

			if (ok && exInput == ExInputWaitLoadEnd) {
				NOTICE_LOG(COMMON, "LoadEndMsg Join:%d", frame);
				gdxsv_WriteMem16(memExInputAddr, ExInputNone);
				if (!ggpo::rollbacking()) {
					ggpo::setExInput(ExInputNone);
				}
				for (int i = 0; i < matching_.player_count(); i++) {
					if (i == matching_.peer_id()) continue;
					auto a = McsMessage::Create(McsMessage::MsgType::LoadEndMsg, i);
					std::copy(a.body.begin(), a.body.end(), std::back_inserter(recv_buf_));
				}
			}
		}

		if (!ggpo::rollbacking()) {
			report_.set_frame_count(frame);
		}

		verify(recv_buf_.size() <= size);
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
				->WriteBytes(reinterpret_cast<char*>(DummyGameParam), sizeof(DummyGameParam))
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
			LbsMessage::SvAnswer(msg).WriteBytes((char*)DummyRuleData, sizeof(DummyRuleData))->Serialize(recv_buf_);
		}

		if (msg.command == LbsMessage::lbsAskBattleCode) {
			LbsMessage::SvAnswer(msg).WriteString("012345")->Serialize(recv_buf_);
		}

		if (msg.command == LbsMessage::lbsAskMcsVersion) {
			LbsMessage::SvAnswer(msg).Write8(10)->Serialize(recv_buf_);
		}

		if (msg.command == LbsMessage::lbsAskMcsAddress) {
			LbsMessage::SvAnswer(msg).Write16(4)->Write8(255)->Write8(255)->Write8(255)->Write8(255)->Write16(2)->Write16(255)->Serialize(
				recv_buf_);
		}

		if (msg.command == LbsMessage::lbsLogout) {
			state_ = State::McsWaitJoin;
		}

		recv_delay_ = 1;
	}
}

void GdxsvBackendRollback::SetCloseReason(const char* reason) {
	if (!report_.close_reason().empty()) {
		report_.set_close_reason(reason);
	}
}

void GdxsvBackendRollback::SaveReplay() {
	if (matching_.battle_code().empty() || input_logs_.empty()) {
		return;
	}

	auto log = std::make_unique<proto::BattleLogFile>();
	log->set_game_disk(gdxsv.Disk() == 1 ? "dc1" : "dc2");
	log->set_battle_code(matching_.battle_code());
	log->set_log_file_version(20230426);
	for (int i = 0; i < gdxsv.patch_list_.patches_size(); ++i) {
		log->add_patches()->CopyFrom(gdxsv.patch_list_.patches(i));
	}
	log->set_rule_bin(matching_.rule_bin());
	log->mutable_users()->CopyFrom(matching_.users());

	for (const auto& kv : input_logs_) {
		log->add_inputs(kv.second);
	}

	const auto now = std::chrono::system_clock::now();
	log->set_end_at(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count());

	std::thread([log = std::move(log)]() {
		auto replay_dir = get_writable_data_path("replays");
		if (!file_exists(replay_dir)) {
			if (!make_directory(replay_dir)) {
				ERROR_LOG(COMMON, "Failed to create replay directory");
				return;
			}
		}

#if _WIN32
		auto replay_file = replay_dir + "\\" + log->battle_code() + ".pb";
#else
		auto replay_file = replay_dir + "/" + log->battle_code() + ".pb";
#endif

		FILE* f = nowide::fopen(replay_file.c_str(), "wb");
		if (f == nullptr) {
			ERROR_LOG(COMMON, "SaveReplay: fopen failure");
			return;
		}

		int fd = fileno(f);
		if (fd == -1) {
			ERROR_LOG(COMMON, "SaveReplay: fileno failure");
			return;
		}

		if (!log->SerializeToFileDescriptor(fd)) {
			ERROR_LOG(COMMON, "SaveReplay: SerializeToFileDescriptor failure");
			return;
		}
		fclose(f);

		std::vector<http::PostField> fields;
		fields.emplace_back("file", replay_file, "application/octet-stream");
		int rc = http::post("https://asia-northeast1-gdxsv-274515.cloudfunctions.net/uploader", fields);
		if (rc == 200 || rc == 409) {
			NOTICE_LOG(COMMON, "SaveReplay: upload OK");
		} else {
			ERROR_LOG(COMMON, "SaveReplay: upload Failed staus: %d", rc);
		}
	}).detach();
}

void GdxsvBackendRollback::ApplyPatch(bool first_time) {
	if (state_ == State::None || state_ == State::End) {
		return;
	}

	gdxsv.WritePatch();

	// Skip Key MsgPush
	if (gdxsv.Disk() == 1) {
		gdxsv_WriteMem16(0x8c058b7c, 9);
		gdxsv_WriteMem8(0x0c310450, 1);
	}
	if (gdxsv.Disk() == 2) {
		gdxsv_WriteMem16(0x8c045f64, 9);
		gdxsv_WriteMem8(0x0c3abb90, 1);
	}
}

void GdxsvBackendRollback::RestorePatch() {
	if (gdxsv.Disk() == 1) {
		gdxsv_WriteMem16(0x8c058b7c, 0x410b);
		gdxsv_WriteMem8(0x0c310450, 2);
	}
	if (gdxsv.Disk() == 2) {
		gdxsv_WriteMem16(0x8c045f64, 0x410b);
		gdxsv_WriteMem8(0x0c3abb90, 2);
	}
}

namespace {
float getScale() {
	const float w = ImGui::GetIO().DisplaySize.x;
	const float h = ImGui::GetIO().DisplaySize.y;
	const float renderAR = getOutputFramebufferAspectRatio();
	const float screenAR = w / h;
	float dx = 0;
	float dy = 0;
	if (renderAR > screenAR)
		dy = h * (1 - screenAR / renderAR) / 2;
	else
		dx = w * (1 - renderAR / screenAR) / 2;

	return std::min((w - dx * 2) / 640.f, (h - dy * 2) / 480.f);
}

ImVec2 fromCenter(float x, float y, float scale) {
	const float w = ImGui::GetIO().DisplaySize.x;
	const float h = ImGui::GetIO().DisplaySize.y;
	const float cx = w / 2.f;
	const float cy = h / 2.f;
	return ImVec2(cx + (x * scale), cy + (y * scale));
}

ImColor fadeColor(ImColor color, int elapsed) {
	if (elapsed <= 1800)
		color.Value.w *= (elapsed - 1550) / 250.0;
	else if (elapsed >= 6600 && elapsed < 6900)
		color.Value.w *= 1.0 - (elapsed - 6600) / 300.0;
	return color;
}

ImColor barColor(int ms) {
	if (ms <= 0) return ImColor(64, 64, 64);
	if (ms <= 30) return ImColor(87, 213, 213);
	if (ms <= 60) return ImColor(0, 255, 149);
	if (ms <= 90) return ImColor(255, 255, 0);
	if (ms <= 120) return ImColor(255, 170, 0);
	return ImColor(255, 0, 0);
}

ImColor barStep(int ms) {
	if (ms <= 0) return 5;
	if (ms <= 30) return 5;
	if (ms <= 60) return 4;
	if (ms <= 90) return 3;
	if (ms <= 120) return 2;
	return 1;
}

ImColor barColor(int ms, int elapsed) { return fadeColor(barColor(ms), elapsed); }

void drawDot(ImDrawList* draw_list, ImVec2 center, ImColor c, float scale) {
	draw_list->AddCircleFilled(center, 6.5 * scale, ImColor(0, 0, 0, 128), 20);
	draw_list->AddCircleFilled(center, 5.5 * scale, c, 20);
}

void baseRect(ImVec2 points[4], float sx, float sy) {
	const float v = sx / 2.0;
	const float w = sy / 2.0;
	points[0].x = -v;
	points[0].y = -w;
	points[1].x = v;
	points[1].y = -w;
	points[2].x = v;
	points[2].y = w;
	points[3].x = -v;
	points[3].y = w;
}

void scaleRect(ImVec2 points[4], float scale) {
	for (int i = 0; i < 4; i++) {
		points[i].x *= scale;
		points[i].y *= scale;
	}
}

void scaleRectX(ImVec2 points[4], float scale) {
	for (int i = 0; i < 4; i++) {
		points[i].x *= scale;
	}
}

void moveRect(ImVec2 points[4], ImVec2 delta) {
	for (int i = 0; i < 4; i++) {
		points[i].x += delta.x;
		points[i].y += delta.y;
	}
}

void rotRect(ImVec2 points[4], float rad) {
	for (int i = 0; i < 4; i++) {
		auto x = points[i].x;
		auto y = points[i].y;
		points[i].x = x * cos(rad) - y * sin(rad);
		points[i].y = y * cos(rad) + x * sin(rad);
	}
}

void drawRectWave(ImDrawList* draw_list, ImVec2 anchor, ImColor color, float scale, int step, int dir, int elapsed) {
	const float rad = (3.141592 / 4) * dir;
	ImVec2 points[4] = {};
	for (int i = 0; i < 5; i++) {
		baseRect(points, 5, 3.5);
		auto c = color;
		if (step <= i)
			c = ImColor(64, 64, 64);
		else if (i == (elapsed / 100 % 5)) {
			c.Value.x *= 2;
			c.Value.y *= 2;
			c.Value.z *= 2;
		}
		moveRect(points, ImVec2(0, i * 5.3));
		scaleRectX(points, 1 + i * 0.50);
		scaleRect(points, scale);
		moveRect(points, ImVec2(0, scale * 9.5));
		rotRect(points, rad);
		moveRect(points, anchor);
		draw_list->AddConvexPolyFilled(points, sizeof(points) / sizeof(points[0]), c);
		draw_list->AddPolyline(points, sizeof(points) / sizeof(points[0]), ImColor(0, 0, 0, 128), true, 1.0 * scale);
	}
}

void drawConnectionDiagram(int elapsed, const uint8_t matrix[4][4], const std::map<int, int>& pos_to_id) {
	const float w = ImGui::GetIO().DisplaySize.x;
	const float h = ImGui::GetIO().DisplaySize.y;
	const float scale = getScale();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f), ImGuiCond_Always,
							ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(w, h));
	ImGui::SetNextWindowBgAlpha(0.0f);
	ImGui::Begin("##gdxsvosd", NULL,
				 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs);

	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	draw_list->AddRectFilled(fromCenter(-45, -97, scale), fromCenter(45.25, -51.875, scale), fadeColor(ImColor(0, 0, 0, 128), elapsed));
	draw_list->AddRectFilled(fromCenter(-45, 53.125, scale), fromCenter(45.25, 98.25, scale), fadeColor(ImColor(0, 0, 0, 128), elapsed));

	ImVec2 d(36, 89);
	ImVec2 origins[4] = {
		fromCenter(0, 0, scale) + ImVec2(-d.x * scale, -d.y * scale),
		fromCenter(0, 0, scale) + ImVec2(d.x * scale, -d.y * scale),
		fromCenter(0, 0, scale) + ImVec2(-d.x * scale, d.y * scale),
		fromCenter(0, 0, scale) + ImVec2(d.x * scale, d.y * scale),
	};

	int dirs[4][4] = {};
	dirs[0][1] = dirs[2][3] = 6;
	dirs[0][3] = 7;
	dirs[0][2] = dirs[1][3] = 0;
	dirs[1][2] = 1;
	dirs[1][0] = dirs[3][2] = 2;
	dirs[3][0] = 3;
	dirs[2][0] = dirs[3][1] = 4;
	dirs[2][1] = 5;

	for (const auto& p : pos_to_id) {
		int i = p.first;
		if (i < 0 || i >= 4) continue;
		int max_ms = 0;
		for (int j = 0; j < 4; j++) {
			if (i == j) continue;
			if (pos_to_id.find(j) == pos_to_id.end()) continue;
			auto ms = matrix[pos_to_id.at(i)][pos_to_id.at(j)];
			drawRectWave(draw_list, origins[i], barColor(ms, elapsed), scale, barStep(ms), dirs[i][j], elapsed);
			max_ms = std::max(max_ms, (int)ms);
		}
		drawDot(draw_list, origins[i], barColor(max_ms, elapsed), scale);
	}

	ImGui::End();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();
}
}  // namespace
