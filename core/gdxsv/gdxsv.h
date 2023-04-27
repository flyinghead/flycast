#pragma once

#include <atomic>
#include <string>

#include "gdxsv.pb.h"
#include "gdxsv_backend_replay.h"
#include "gdxsv_backend_rollback.h"
#include "gdxsv_backend_tcp.h"
#include "gdxsv_backend_udp.h"
#include "network/miniupnp.h"
#include "types.h"

class Gdxsv {
   public:
	friend GdxsvBackendReplay;
	friend GdxsvBackendRollback;
	friend GdxsvBackendTcp;
	friend GdxsvBackendUdp;

	enum class NetMode {
		Offline,
		Lbs,
		McsUdp,
		McsRollback,
		Replay,
	};

	bool Enabled() const;
	bool InGame() const;
	void DisplayOSD();
	const char* NetModeString() const;
	void Reset();
	void Update();
	void HookMainUiLoop();
	void HandleRPC();
	void RestoreOnlinePatch();
	void StartPingTest();
	bool StartReplayFile(const char* path);
	bool StartRollbackTest(const char* param);
	void WritePatch();
	int Disk() const { return disk_; }
	std::string UserId() const { return user_id_; }
	MiniUPnP& UPnP() { return upnp_; }

   private:
	void GcpPingTest();
	static std::string GenerateLoginKey();
	std::vector<u8> GeneratePlatformInfoPacket();
	std::string GeneratePlatformInfoString();
	std::vector<u8> GenerateP2PMatchReportPacket();
	LbsMessage GenerateP2PMatchReportMessage();
	void ApplyOnlinePatch(bool first_time);
	void WritePatchDisk1();
	void WritePatchDisk2();

	NetMode netmode_ = NetMode::Offline;
	std::atomic<bool> enabled_;
	std::atomic<int> disk_;
	std::atomic<int> maxlag_;
	std::atomic<int> maxrebattle_;
	std::string server_;
	std::string loginkey_;
	std::string user_id_;
	std::map<std::string, u32> symbols_;
	proto::GamePatchList patch_list_;
	std::map<std::string, int> gcp_ping_test_result_;
	std::atomic<bool> gcp_ping_test_finished_;

	MiniUPnP upnp_;
	std::future<std::string> upnp_result_;
	int upnp_port_ = 0;
	int udp_port_ = 0;

	UdpRemote lbs_remote_ = {};
	UdpClient udp_ = {};
	GdxsvBackendTcp lbs_net_;
	GdxsvBackendUdp udp_net_;
	GdxsvBackendReplay replay_net_;
	GdxsvBackendRollback rollback_net_;
};

extern Gdxsv gdxsv;
