#pragma once
#include <future>
#include <map>
#include <string>

#include "gdxsv_network.h"
#include "lbs_message.h"

class GdxsvBackendRollback {
   public:
	GdxsvBackendRollback(const std::map<std::string, u32> &symbols, std::atomic<int> &maxlag, std::atomic<int> &maxrebattle)
		: state_(State::None), symbols_(symbols), maxlag_(maxlag), maxrebattle_(maxrebattle), recv_delay_(0), port_(0) {}

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
	proto::P2PMatchingReport &GetReport() { return report_; }
	void ClearReport() { report_.Clear(); }

   private:
	void ApplyPatch(bool first_time);
	void RestorePatch();
	void ProcessLbsMessage();

	State state_;
	bool is_local_test_;
	const std::map<std::string, u32> &symbols_;
	std::atomic<int> &maxlag_;
	std::atomic<int> &maxrebattle_;
	int recv_delay_;
	int port_;
	std::deque<u8> recv_buf_;
	LbsMessageReader lbs_tx_reader_;
	proto::P2PMatching matching_;
	proto::P2PMatchingReport report_;
	UdpPingPong ping_pong_;
	std::future<bool> start_network_;
};
