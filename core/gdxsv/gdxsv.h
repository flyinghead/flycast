#pragma once

#include <atomic>
#include <chrono>
#include <deque>
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
	enum class NetMode {
		Offline,
		Lbs,
		McsUdp,
		McsRollback,
		Replay,
	};

	Gdxsv()
		: lbs_net(symbols),
		  udp_net(symbols, maxlag),
		  replay_net(symbols, maxlag),
		  rollback_net(symbols, maxlag, maxrebattle),
		  upnp_port(0),
		  udp_port(0){};

	bool Enabled() const;

	bool InGame() const;

	void DisplayOSD();

	void Reset();

	void Update();

	void HookMainUiLoop();

	void HandleRPC();

	void RestoreOnlinePatch();

	void StartPingTest();

	bool StartReplayFile(const char* path);

	bool StartRollbackTest(const char* param);

	void WritePatch();

	int Disk() const { return disk; }

	MiniUPnP& UPnP() { return upnp; }

   private:
	void GcpPingTest();

	static std::string GenerateLoginKey();

	std::vector<u8> GeneratePlatformInfoPacket();

	std::string GeneratePlatformInfoString();

	void ApplyOnlinePatch(bool first_time);

	void WritePatchDisk1();

	void WritePatchDisk2();

	void WriteWidescreenPatchDisk2();

	NetMode netmode = NetMode::Offline;
	std::atomic<bool> enabled;
	std::atomic<int> disk;
	std::atomic<int> maxlag;
	std::atomic<int> maxrebattle;

	std::string server;
	std::string loginkey;
	std::map<std::string, u32> symbols;

	MiniUPnP upnp;
	std::future<std::string> upnp_result;
	int upnp_port;
	int udp_port;
	std::string user_id;

	UdpRemote lbs_remote;
	UdpClient udp;

	proto::GamePatchList patch_list;

	GdxsvBackendTcp lbs_net;
	GdxsvBackendUdp udp_net;
	GdxsvBackendReplay replay_net;
	GdxsvBackendRollback rollback_net;

	std::atomic<bool> gcp_ping_test_finished;
	std::map<std::string, int> gcp_ping_test_result;
};

extern Gdxsv gdxsv;
