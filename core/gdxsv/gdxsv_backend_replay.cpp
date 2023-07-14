#include "gdxsv_backend_replay.h"

#include <sstream>

#include "cfg/option.h"
#include "gdx_rpc.h"
#include "gdxsv.h"
#include "gdxsv_replay_util.h"
#include "input/gamepad_device.h"
#include "libs.h"

void GdxsvBackendReplay::Reset() {
	RestorePatch();
	state_ = State::None;
	lbs_tx_reader_.Clear();
	log_file_.Clear();
	start_msg_index_.clear();
	recv_buf_.clear();
	recv_delay_ = 0;
	me_ = 0;
	key_msg_count_ = 0;
}

void GdxsvBackendReplay::OnMainUiLoop() {
	if (state_ == State::Start) {
		kcode[0] = ~0x0004;
	}

	if (state_ == State::McsInBattle) {
		const int disk = gdxsv.Disk();
		const int COM_R_No0 = disk == 1 ? 0x0c2f6639 : 0x0c391d79;
		if (gdxsv_ReadMem8(COM_R_No0) == 4 && (gdxsv_ReadMem8(COM_R_No0 + 5) == 3 || gdxsv_ReadMem8(COM_R_No0 + 5) == 4)) {
			state_ = State::End;
		}
	}

	if (state_ == State::End) {
		gdxsv.Reset();
		gdxsv_end_replay();
	}
}

bool GdxsvBackendReplay::StartFile(const char *path, int pov) {
#ifdef NOWIDE_CONFIG_H_INCLUDED
	FILE *fp = nowide::fopen(path, "rb");
#else
	FILE *fp = fopen(path, "rb");
#endif
	if (fp == nullptr) {
		NOTICE_LOG(COMMON, "fopen failed path:%s", path);
		return false;
	}

	bool ok = log_file_.ParseFromFileDescriptor(fileno(fp));
	if (!ok) {
		NOTICE_LOG(COMMON, "ParseFromFileDescriptor failed");
		return false;
	}
	fclose(fp);

	if (log_file_.users_size() <= pov) {
		return false;
	}

	me_ = pov;

	return Start();
}

bool GdxsvBackendReplay::StartBuffer(const std::vector<u8> &buf, int pov) {
	bool ok = log_file_.ParseFromArray(buf.data(), buf.size());
	if (!ok) {
		NOTICE_LOG(COMMON, "ParseFromArray failed");
		return false;
	}

	if (log_file_.users_size() <= pov) {
		return false;
	}

	me_ = pov;

	return Start();
}

bool GdxsvBackendReplay::isReplaying() { return state_ == State::McsInBattle; }

void GdxsvBackendReplay::Open() {
	recv_buf_.assign({0x0e, 0x61, 0x00, 0x22, 0x10, 0x31, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd});
	state_ = State::McsSessionExchange;
	ApplyPatch(true);
}

void GdxsvBackendReplay::Close(bool by_user) {
	if (by_user == false && state_ <= State::McsWaitJoin) {
		return;
	}

	if (state_ != State::End) {
		PrintDisconnectionSummary();
	}

	RestorePatch();
	state_ = State::End;
}

u32 GdxsvBackendReplay::OnSockWrite(u32 addr, u32 size) {
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

u32 GdxsvBackendReplay::OnSockRead(u32 addr, u32 size) {
	if (state_ <= State::LbsStartBattleFlow) {
		ProcessLbsMessage();
	} else {
		const int disk = gdxsv.Disk();
		const int InetBuf = disk == 1 ? 0x0c310244 : 0x0c3ab984;
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
			ProcessMcsMessage(msg);
		}
	}

	int n = std::min<int>(recv_buf_.size(), size);
	for (int i = 0; i < n; ++i) {
		gdxsv_WriteMem8(addr + i, recv_buf_.front());
		recv_buf_.pop_front();
	}
	return n;
}

u32 GdxsvBackendReplay::OnSockPoll() {
	if (state_ <= State::LbsStartBattleFlow) {
		ProcessLbsMessage();
	}
	if (0 < recv_delay_) {
		recv_delay_--;
		return 0;
	}
	return recv_buf_.size();
}

bool GdxsvBackendReplay::Start() {
	NOTICE_LOG(COMMON, "game_disk = %s", log_file_.game_disk().c_str());

	if (log_file_.log_file_version() < 20210802) {
		// Parse old replay file
		// proto2 required fields was moved to UnknownFields
		for (int i = 0; i < log_file_.battle_data_size(); ++i) {
			auto data = log_file_.mutable_battle_data(i);
			const auto &fields = proto::BattleLogFile::GetReflection()->GetUnknownFields(*data);
			if (!fields.empty()) {
				for (int j = 0; j < fields.field_count(); ++j) {
					const auto &field = fields.field(j);
					if (j == 0 && field.type() == google::protobuf::UnknownField::TYPE_LENGTH_DELIMITED) {
						const auto &body = field.length_delimited();
						data->set_body(body.data(), body.size());
					}
					if (j == 1 && field.type() == google::protobuf::UnknownField::TYPE_VARINT) {
						data->set_seq(field.varint());
					}
				}
			}
		}

		// Restore player position, team and grade.
		std::map<std::string, int> player_position;
		McsMessageReader r;
		McsMessage msg;
		for (int i = 0; i < log_file_.battle_data_size(); ++i) {
			const auto &data = log_file_.battle_data(i);
			if (player_position.find(data.user_id()) == player_position.end()) {
				r.Write(data.body().data(), data.body().size());
				while (r.Read(msg)) {
					if (msg.Type() == McsMessage::MsgType::PingMsg) {
						player_position[data.user_id()] = msg.Sender();
						break;
					}
				}
			}
			if (log_file_.users_size() == player_position.size()) {
				break;
			}
		}

		for (int i = 0; i < log_file_.users_size(); ++i) {
			const int pos = player_position[log_file_.users(i).user_id()];
			log_file_.mutable_users(i)->set_pos(pos + 1);
			log_file_.mutable_users(i)->set_team(1 + pos / 2);
			// NOTE: Surprisingly, player's grade seems to affect the game.
			log_file_.mutable_users(i)->set_grade(std::min(14, log_file_.users(i).win_count() / 100));
			log_file_.mutable_users(i)->set_user_name_sjis(log_file_.users(i).user_id());
		}

		std::sort(log_file_.mutable_users()->begin(), log_file_.mutable_users()->end(),
				  [](const proto::BattleLogUser &a, const proto::BattleLogUser &b) { return a.pos() < b.pos(); });
	}

	if (log_file_.inputs_size() == 0 && log_file_.battle_data_size() != 0) {
		// Convert McsMessage into uint64 input.
		// start_msg_index_ holds input indexes of round start.
		NOTICE_LOG(COMMON, "Converting inputs..");
		McsMessageReader r;
		McsMessage msg;
		std::vector<std::vector<std::vector<u16>>> player_chunked_inputs(log_file_.users_size());

		for (const auto &data : log_file_.battle_data()) {
			r.Write(data.body().data(), data.body().size());

			while (r.Read(msg)) {
				const int p = msg.Sender();
				if (msg.Type() == McsMessage::StartMsg) {
					player_chunked_inputs[p].emplace_back();
				}
				if (msg.Type() == McsMessage::KeyMsg1) {
					player_chunked_inputs[p].back().emplace_back(msg.FirstInput());
				}
				if (msg.Type() == McsMessage::KeyMsg2) {
					player_chunked_inputs[p].back().emplace_back(msg.FirstInput());
					player_chunked_inputs[p].back().emplace_back(msg.SecondInput());
				}
			}
		}

		for (int chunk = 0; chunk < player_chunked_inputs[0].size(); chunk++) {
			int min_t = player_chunked_inputs[0][chunk].size();
			for (int p = 0; p < log_file_.users_size(); p++) {
				min_t = std::min<int>(min_t, player_chunked_inputs[p][chunk].size());
			}

			start_msg_index_.push_back(log_file_.inputs_size());

			for (int t = 0; t < min_t; t++) {
				uint64_t input = 0;
				for (int p = 0; p < log_file_.users_size(); p++) {
					input |= static_cast<uint64_t>(player_chunked_inputs[p][chunk][t]) << (p * 16);
				}
				log_file_.add_inputs(input);
			}
		}

		PrintDisconnectionSummary();
	}

	NOTICE_LOG(COMMON, "users = %d", log_file_.users_size());
	NOTICE_LOG(COMMON, "patch_size = %d", log_file_.patches_size());
	NOTICE_LOG(COMMON, "inputs_size = %d", log_file_.inputs_size());
	std::ostringstream ss;
	for (const int a : start_msg_index_) ss << a << " ";
	NOTICE_LOG(COMMON, "start_msg_index = %s", ss.str().c_str());

	state_ = State::Start;
	gdxsv.maxlag_ = 0;
	key_msg_count_ = 0;
	NOTICE_LOG(COMMON, "Replay Start");
	return true;
}

void GdxsvBackendReplay::PrintDisconnectionSummary() {
	std::vector<McsMessage> msg_list;
	McsMessageReader r;
	McsMessage msg;

	for (int i = 0; i < log_file_.battle_data_size(); ++i) {
		const auto &data = log_file_.battle_data(i);
		r.Write(data.body().data(), data.body().size());
		while (r.Read(msg)) {
			if (msg.Type() == McsMessage::KeyMsg2) {
				msg_list.emplace_back(msg.FirstKeyMsg());
				msg_list.emplace_back(msg.SecondKeyMsg());
			} else {
				msg_list.emplace_back(msg);
			}
		}
	}

	std::vector<int> last_keymsg_seq(log_file_.users_size());
	std::vector<int> last_force_msg_index(log_file_.users_size());
	for (int i = 0; i < msg_list.size(); ++i) {
		const auto &msg = msg_list[i];
		if (msg.Type() == McsMessage::KeyMsg1) {
			last_keymsg_seq[msg.Sender()] = msg.FirstSeq();
			last_force_msg_index[msg.Sender()] = 0;
		}
		if (msg.Type() == McsMessage::KeyMsg2) {
			last_keymsg_seq[msg.Sender()] = msg.SecondSeq();
			last_force_msg_index[msg.Sender()] = 0;
		}
		if (msg.Type() == McsMessage::ForceMsg) {
			last_force_msg_index[msg.Sender()] = i;
		}
	}

	NOTICE_LOG(COMMON, "== Disconnection Summary ==");
	NOTICE_LOG(COMMON, " KeyCount LastForceMsg UserID Name");
	for (int i = 0; i < log_file_.users_size(); ++i) {
		NOTICE_LOG(COMMON, "%9d %12d %6s %s", last_keymsg_seq[i], last_force_msg_index[i], log_file_.users(i).user_id().c_str(),
				   log_file_.users(i).user_name().c_str());
	}

	const auto it_seq_min = std::min_element(begin(last_keymsg_seq), end(last_keymsg_seq));
	const auto it_seq_max = std::max_element(begin(last_keymsg_seq), end(last_keymsg_seq));
	if (*it_seq_min != *it_seq_max) {
		int i = it_seq_min - begin(last_keymsg_seq);
		bool no_force_msg = last_force_msg_index[i] == 0;
		bool other_player_send_force_msg = std::count(begin(last_force_msg_index), end(last_force_msg_index), 0) == 1;
		if (no_force_msg && other_player_send_force_msg) {
			NOTICE_LOG(COMMON, "!! Disconnected Player Detected !!");
			NOTICE_LOG(COMMON, " KeyCount LastForceMsg UserID Name");
			NOTICE_LOG(COMMON, "%9d %12d %6s %s", last_keymsg_seq[i], last_force_msg_index[i], log_file_.users(i).user_id().c_str(),
					   log_file_.users(i).user_name().c_str());
		}
	}
}

void GdxsvBackendReplay::ProcessLbsMessage() {
	if (state_ == State::Start) {
		LbsMessage::SvNotice(LbsMessage::lbsReadyBattle).Serialize(recv_buf_);
		recv_delay_ = 1;
		state_ = State::LbsStartBattleFlow;
	}

	LbsMessage msg;
	if (lbs_tx_reader_.Read(msg)) {
		if (state_ == State::Start) {
			state_ = State::LbsStartBattleFlow;
		}

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
			LbsMessage::SvAnswer(msg).Write8(me_ + 1)->Serialize(recv_buf_);
		}

		if (msg.command == LbsMessage::lbsAskPlayerInfo) {
			int pos = msg.Read8();
			auto user = log_file_.users(pos - 1);

			if (config::GdxReplayHideName) {
				user.set_user_id("USER0" + std::to_string(pos));
				user.set_user_name_sjis("USER0" + std::to_string(pos));
				auto game_param = user.game_param();
				user.set_game_param(game_param.replace(16, 17,
													   "\x82\x6F\x82\x68\x82\x6B\x82\x6E\x82\x73\x82\x4F\x82" +
														   std::string(1, static_cast<char>(0x4F + pos)) +
														   "\x01\x01\x07"));  // ＰＩＬＯＴ０１~０４
				user.set_battle_count(0);
				user.set_win_count(0);
				user.set_lose_count(0);
			}

			LbsMessage::SvAnswer(msg)
				.Write8(pos)
				->WriteString(user.user_id())
				->WriteBytes(user.user_name_sjis().data(), user.user_name_sjis().size())
				->WriteBytes(user.game_param().data(), user.game_param().size())
				->Write16(user.grade())
				->Write16(user.win_count())
				->Write16(user.lose_count())
				->Write16(0)
				->Write16(user.battle_count() - user.win_count() - user.lose_count())
				->Write16(0)
				->Write16(user.team())
				->Write16(0)
				->Serialize(recv_buf_);
		}

		if (msg.command == LbsMessage::lbsAskRuleData) {
			LbsMessage::SvAnswer(msg).WriteBytes(log_file_.rule_bin().data(), log_file_.rule_bin().size())->Serialize(recv_buf_);
		}

		if (msg.command == LbsMessage::lbsAskBattleCode) {
			LbsMessage::SvAnswer(msg).WriteBytes(log_file_.battle_code().data(), log_file_.battle_code().size())->Serialize(recv_buf_);
		}

		if (msg.command == LbsMessage::lbsAskMcsVersion) {
			LbsMessage::SvAnswer(msg).Write8(10)->Serialize(recv_buf_);
		}

		if (msg.command == LbsMessage::lbsAskMcsAddress) {
			LbsMessage::SvAnswer(msg).Write16(4)->Write8(127)->Write8(0)->Write8(0)->Write8(1)->Write16(2)->Write16(3333)->Serialize(
				recv_buf_);
		}

		if (msg.command == LbsMessage::lbsLogout) {
			state_ = State::McsWaitJoin;
		}

		recv_delay_ = 1;
	}
}

void GdxsvBackendReplay::ProcessMcsMessage(const McsMessage &msg) {
	const auto msg_type = msg.Type();

	if (msg_type == McsMessage::MsgType::ConnectionIdMsg) {
		state_ = State::McsInBattle;
	} else if (msg_type == McsMessage::MsgType::IntroMsg) {
		for (int i = 0; i < log_file_.users_size(); ++i) {
			if (i != me_) {
				auto intro_msg = McsMessage::Create(McsMessage::MsgType::IntroMsg, i);
				std::copy(intro_msg.body.begin(), intro_msg.body.end(), std::back_inserter(recv_buf_));
			}
		}
	} else if (msg_type == McsMessage::MsgType::IntroMsgReturn) {
		for (int i = 0; i < log_file_.users_size(); ++i) {
			if (i != me_) {
				auto intro_msg = McsMessage::Create(McsMessage::MsgType::IntroMsgReturn, i);
				std::copy(intro_msg.body.begin(), intro_msg.body.end(), std::back_inserter(recv_buf_));
			}
		}
	} else if (msg_type == McsMessage::MsgType::PingMsg) {
		for (int i = 0; i < log_file_.users_size(); ++i) {
			if (i != me_) {
				auto pong_msg = McsMessage::Create(McsMessage::MsgType::PongMsg, i);
				pong_msg.SetPongTo(me_);
				pong_msg.PongCount(msg.PingCount());
				std::copy(pong_msg.body.begin(), pong_msg.body.end(), std::back_inserter(recv_buf_));
			}
		}
	} else if (msg_type == McsMessage::MsgType::PongMsg) {
		// do nothing
	} else if (msg_type == McsMessage::MsgType::StartMsg) {
		if (!start_msg_index_.empty()) {
			auto it = std::lower_bound(start_msg_index_.begin(), start_msg_index_.end(), key_msg_count_);
			if (it != start_msg_index_.end()) {
				NOTICE_LOG(COMMON, "key_msg_count updates %d -> %d", key_msg_count_, *it);
				key_msg_count_ = *it;
			}
		}

		gdxsv.maxlag_ = 1;	// StartMsg needs this
		for (int i = 0; i < log_file_.users_size(); ++i) {
			if (i != me_) {
				auto start_msg = McsMessage::Create(McsMessage::MsgType::StartMsg, i);
				std::copy(start_msg.body.begin(), start_msg.body.end(), std::back_inserter(recv_buf_));
			}
		}
	} else if (msg_type == McsMessage::MsgType::ForceMsg) {
		// do nothing
	} else if (msg_type == McsMessage::MsgType::KeyMsg1) {
		gdxsv.maxlag_ = 0;
		if (log_file_.inputs_size()) {
			if (key_msg_count_ < log_file_.inputs_size()) {
				const u64 inputs = log_file_.inputs(key_msg_count_);

				for (int i = 0; i < log_file_.users_size(); ++i) {
					const u16 input = u16(inputs >> (i * 16));
					auto key_msg = McsMessage::Create(McsMessage::MsgType::KeyMsg1, i);
					key_msg.body[2] = input >> 8 & 0xff;
					key_msg.body[3] = input & 0xff;
					std::copy(key_msg.body.begin(), key_msg.body.end(), std::back_inserter(recv_buf_));
				}

				key_msg_count_++;
				if (key_msg_count_ == log_file_.inputs_size()) {
					state_ = State::End;
				}
			}
		}
	} else if (msg_type == McsMessage::MsgType::KeyMsg2) {
		verify(false);
	} else if (msg_type == McsMessage::MsgType::LoadEndMsg) {
		for (int i = 0; i < log_file_.users_size(); ++i) {
			if (i != me_) {
				auto load_start_msg = McsMessage::Create(McsMessage::MsgType::LoadStartMsg, i);
				std::copy(load_start_msg.body.begin(), load_start_msg.body.end(), std::back_inserter(recv_buf_));
			}
		}
		for (int i = 0; i < log_file_.users_size(); ++i) {
			if (i != me_) {
				auto load_end_msg = McsMessage::Create(McsMessage::MsgType::LoadEndMsg, i);
				std::copy(load_end_msg.body.begin(), load_end_msg.body.end(), std::back_inserter(recv_buf_));
			}
		}
	} else {
		WARN_LOG(COMMON, "unhandled mcs msg: %s", McsMessage::MsgTypeName(msg_type));
		WARN_LOG(COMMON, "%s", msg.ToHex().c_str());
	}
}

void GdxsvBackendReplay::ApplyPatch(bool first_time) {
	if (state_ == State::None || state_ == State::End) {
		return;
	}

	// Skip Key MsgPush
	if (gdxsv.Disk() == 1) {
		gdxsv_WriteMem16(0x8c058b7c, 9);
		gdxsv_WriteMem8(0x0c310450, 1);
	}
	if (gdxsv.Disk() == 2) {
		gdxsv_WriteMem16(0x8c045f64, 9);
		gdxsv_WriteMem8(0x0c3abb90, 1);
	}

	// Online Patch
	for (auto& patch : log_file_.patches()) {
		for (auto& code : patch.codes()) {
			gdxsv_WriteMem(code.size(), code.address(), code.changed());
		}
	}
}

void GdxsvBackendReplay::RestorePatch() {
	if (gdxsv.Disk() == 1) {
		gdxsv_WriteMem16(0x8c058b7c, 0x410b);
		gdxsv_WriteMem8(0x0c310450, 2);
	}
	if (gdxsv.Disk() == 2) {
		gdxsv_WriteMem16(0x8c045f64, 0x410b);
		gdxsv_WriteMem8(0x0c3abb90, 2);
	}

	// Online Patch
	for (int i = 0; i < log_file_.patches_size(); ++i) {
		for (int j = 0; j < log_file_.patches(i).codes_size(); ++j) {
			const auto &code = log_file_.patches(i).codes(j);
			if (code.size() == 8) {
				gdxsv_WriteMem8(code.address(), code.original());
			}
			if (code.size() == 16) {
				gdxsv_WriteMem16(code.address(), code.original());
			}
			if (code.size() == 32) {
				gdxsv_WriteMem32(code.address(), code.original());
			}
		}
	}
}
