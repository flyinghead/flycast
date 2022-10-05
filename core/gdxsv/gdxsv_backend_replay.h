#pragma once

#include <array>
#include <atomic>
#include <map>
#include <queue>
#include <string>
#include <vector>

#include "gdxsv.pb.h"
#include "lbs_message.h"
#include "mcs_message.h"
#include "types.h"

// Mock network implementation to replay local battle log
class GdxsvBackendReplay {
   public:
	GdxsvBackendReplay(const std::map<std::string, u32> &symbols, std::atomic<int> &maxlag) : symbols_(symbols), maxlag_(maxlag) {}

	enum class State {
		None,
		Start,
		LbsStartBattleFlow,
		McsWaitJoin,
		McsSessionExchange,
		McsInBattle,
		End,
	};

	void Reset();
	bool StartFile(const char *path);
	bool StartBuffer(const char *buf, int size);
	void Open();
	void Close();
	u32 OnSockWrite(u32 addr, u32 size);
	u32 OnSockRead(u32 addr, u32 size);
	u32 OnSockPoll();

   private:
	bool Start();
	void PrintDisconnectionSummary();
	void PrepareKeyMsgIndex();
	void ProcessLbsMessage();
	void ProcessMcsMessage();
	void ApplyPatch(bool first_time);
	void RestorePatch();

	const std::map<std::string, u32> &symbols_;
	std::atomic<int> &maxlag_;
	State state_;
	LbsMessageReader lbs_tx_reader_;
	McsMessageReader mcs_tx_reader_;
	proto::BattleLogFile log_file_;
	std::deque<u8> recv_buf_;
	int recv_delay_;
	int me_;
	std::vector<McsMessage> msg_list_;
	std::array<int, 4> start_index_;
	std::array<std::vector<int>, 4> key_msg_index_;
};