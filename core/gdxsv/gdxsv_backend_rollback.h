#pragma once
#include <future>

#include "gdxsv_network.h"
#include "lbs_message.h"

class GdxsvBackendRollback {
   public:
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
		CloseWait,
		End,
		Closed,
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
	void SetCloseReason(const char *reason);
	void SaveReplay();
	proto::P2PMatchingReport &GetReport() { return report_; }
	void ClearReport() { report_.Clear(); }

   private:
	void ApplyPatch(bool first_time);
	void RestorePatch();
	void ProcessLbsMessage();

	State state_ = State::None;
	bool is_local_test_ = false;
	int recv_delay_ = 0;
	int port_ = 0;
	std::deque<u8> recv_buf_;
	LbsMessageReader lbs_tx_reader_;
	proto::P2PMatching matching_;
	proto::P2PMatchingReport report_;
	UdpPingPong ping_pong_;
	std::future<bool> start_network_;

	uint64_t start_at_ = 0;
	std::vector<std::pair<int, u64>> input_logs_;
};
