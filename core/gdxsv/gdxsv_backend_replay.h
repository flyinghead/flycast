#pragma once

#include "gdxsv.pb.h"
#include "lbs_message.h"
#include "mcs_message.h"
#include "types.h"

// Mock network implementation to replay local battle log
class GdxsvBackendReplay {
   public:
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
	void OnMainUiLoop();
	bool StartFile(const char *path, int pov);
	bool StartBuffer(const std::vector<u8> &buf, int pov);
	bool isReplaying();
	void Open();
	void Close(bool by_user = false);
	u32 OnSockWrite(u32 addr, u32 size);
	u32 OnSockRead(u32 addr, u32 size);
	u32 OnSockPoll();

   private:
	bool Start();
	void PrintDisconnectionSummary();
	void ProcessLbsMessage();
	void ProcessMcsMessage(const McsMessage &msg);
	void ApplyPatch(bool first_time);
	void RestorePatch();

	State state_;
	LbsMessageReader lbs_tx_reader_;
	proto::BattleLogFile log_file_;
	std::deque<u8> recv_buf_;
	int recv_delay_;
	int me_;
	int key_msg_count_;
};