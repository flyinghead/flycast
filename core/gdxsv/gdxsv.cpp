#include "gdxsv.h"

#include <xxhash.h>
#include <zlib.h>

#include <random>
#include <sstream>

#include "cfg/cfg.h"
#include "cfg/option.h"
#include "emulator.h"
#include "gdx_rpc.h"
#include "gdxsv_translation.h"
#include "hw/sh4/sh4_mem.h"
#include "imgui/imgui.h"
#include "libs.h"
#include "log/InMemoryListener.h"
#include "log/LogManager.h"
#include "lzma/CpuArch.h"
#include "network/ggpo.h"
#include "oslib/oslib.h"
#include "reios/reios.h"
#include "rend/boxart/http_client.h"
#include "rend/gui.h"
#include "version.h"

bool Gdxsv::InGame() const { return enabled_ && (netmode_ == NetMode::McsUdp || netmode_ == NetMode::McsRollback); }

bool Gdxsv::IsOnline() const {
	return enabled_ &&
		   (netmode_ == NetMode::McsUdp || netmode_ == NetMode::McsRollback || netmode_ == NetMode::Lbs || lbs_net_.IsConnected());
}

bool Gdxsv::IsSaveStateAllowed() const { return netmode_ == NetMode::Offline; }

bool Gdxsv::IsReplaying() const { return netmode_ == NetMode::Replay; }

bool Gdxsv::Enabled() const { return enabled_; }

void Gdxsv::DisplayOSD() { rollback_net_.DisplayOSD(); }

const char *Gdxsv::NetModeString() const {
	if (netmode_ == NetMode::Offline) return "Offline";
	if (netmode_ == NetMode::Lbs) return "Lbs";
	if (netmode_ == NetMode::McsUdp) return "McsUdp";
	if (netmode_ == NetMode::McsRollback) return "McsRollback";
	if (netmode_ == NetMode::Replay) return "Replay";
	return "Unknown";
}

void Gdxsv::Reset() {
	lbs_net_.Reset();
	udp_net_.Reset();
	rollback_net_.Reset();
	replay_net_.Reset();
	netmode_ = NetMode::Offline;
	http::init();

	// Automatically add ContentPath if it is empty.
	if (config::ContentPath.get().empty()) {
		config::ContentPath.get().push_back("./");
	}

	auto game_id = std::string(ip_meta.product_number, sizeof(ip_meta.product_number));
	if (game_id != "T13306M   ") {
		enabled_ = false;
		return;
	}
	enabled_ = true;

	RestoreOnlinePatch();

	server_ = cfgLoadStr("gdxsv", "server", "zdxsv.net");
	loginkey_ = cfgLoadStr("gdxsv", "loginkey", "");

	if (loginkey_.empty()) {
		loginkey_ = GenerateLoginKey();
	}

	cfgSaveStr("gdxsv", "server", server_.c_str());
	cfgSaveStr("gdxsv", "loginkey", loginkey_.c_str());

	std::string disk_num(ip_meta.disk_num, 1);
	if (disk_num == "1") disk_ = 1;
	if (disk_num == "2") disk_ = 2;

	maxrebattle_ = 5;

#ifdef __APPLE__
	signal(SIGPIPE, SIG_IGN);
#endif

	NOTICE_LOG(COMMON, "gdxsv disk:%d server:%s loginkey:%s udp_port:%d", (int)disk_, server_.c_str(), loginkey_.c_str(),
			   config::GdxLocalPort.get());

	lbs_net_.lbs_packet_filter([this](const LbsMessage &lbs_msg) -> bool {
		if (netmode_ != NetMode::Lbs) {
			if (lbs_net_.IsConnected()) {
				// The user is not in lobby but connection is alive
				if (lbs_msg.command == LbsMessage::lbsLineCheck) {
					std::vector<u8> buf;
					LbsMessage::ClAnswer(lbs_msg).Serialize(buf);
					lbs_net_.Send(buf);
				}
			}

			return false;
		}

		if (lbs_msg.command == LbsMessage::lbsUserRegist || lbs_msg.command == LbsMessage::lbsUserDecide) {
			std::string id(6, ' ');
			for (int i = 0; i < 6; i++) {
				id[i] = lbs_msg.body[2 + i];
			}
			user_id_ = id;
		}

		if (lbs_msg.command == LbsMessage::lbsUserRegist || lbs_msg.command == LbsMessage::lbsUserDecide ||
			lbs_msg.command == LbsMessage::lbsLineCheck) {
			if (udp_.Initialized()) {
				if (!lbs_remote_.is_open()) {
					lbs_remote_.Open(lbs_net_.RemoteHost().c_str(), lbs_net_.RemotePort());
				}

				proto::Packet pkt;
				pkt.set_type(proto::MessageType::HelloLbs);
				pkt.mutable_hello_lbs_data()->set_user_id(user_id_);
				char buf[128];
				if (pkt.SerializePartialToArray((void *)buf, (int)sizeof(buf))) {
					udp_.SendTo((const char *)buf, pkt.GetCachedSize(), lbs_remote_);
				} else {
					ERROR_LOG(COMMON, "packet serialize error");
				}
			}

			lbs_net_.Send(GeneratePlatformInfoPacket());
		}

		if (lbs_msg.command == LbsMessage::lbsLineCheck) {
			if (gui_is_open() || gui_state == GuiState::VJoyEdit) {
				return false;
			}
		}

		if (lbs_msg.command == LbsMessage::lbsReadyBattle) {
			// Reset current patches for no-patched game
			RestoreOnlinePatch();
			going_to_battle_ = true;
		}

		if (lbs_msg.command == LbsMessage::lbsP2PMatching) {
			proto::P2PMatching matching;
			if (matching.ParseFromArray(lbs_msg.body.data(), lbs_msg.body.size())) {
				int port = udp_.bound_port();
				udp_.Close();
				lbs_remote_.Close();
				rollback_net_.Prepare(matching, port);
			} else {
				ERROR_LOG(COMMON, "p2p matching deserialize error");
			}

			return false;
		}

		if (lbs_msg.command == LbsMessage::lbsGamePatch) {
			// Reset current patches and update patch_list
			RestoreOnlinePatch();
			if (patch_list_.ParseFromArray(lbs_msg.body.data(), lbs_msg.body.size())) {
				ApplyOnlinePatch(true);
			} else {
				ERROR_LOG(COMMON, "patch_list deserialize error");
			}

			return false;
		}

		if (lbs_msg.command == LbsMessage::lbsBattleUserCount && disk_ == 2 && GdxsvLanguage::Language() != GdxsvLanguage::Lang::Disabled) {
			u32 battle_user_count = u32(lbs_msg.body[0]) << 24 | u32(lbs_msg.body[1]) << 16 | u32(lbs_msg.body[2]) << 8 | lbs_msg.body[3];
			const u32 offset = 0x8C000000 + 0x00010000;
			gdxsv_WriteMem32(offset + 0x3839FC, battle_user_count);

			return false;
		}

		return true;
	});
}

bool Gdxsv::HookOpenMenu() {
	if (!enabled_) return true;

	if (InGame()) {
		if (netmode_ == NetMode::McsRollback) {
			rollback_net_.ToggleNetworkStat();
		}
		return false;
	}

	return true;
}

void Gdxsv::HookVBlank() {
	if (netmode_ != NetMode::Lbs && lbs_net_.IsConnected()) {
		lbs_net_.OnSockPoll();
	}
	if (!ggpo::active()) {
		// Don't edit memory at vsync if ggpo::active
		WritePatch();
	}
}

void Gdxsv::HookMainUiLoop() {
	if (!enabled_) return;

	if (InGame()) {
		settings.input.fastForwardMode = false;
	}

	if (netmode_ == NetMode::McsRollback) {
		gdxsv.rollback_net_.OnMainUiLoop();
	}

	if (netmode_ == NetMode::Replay) {
		gdxsv.replay_net_.OnMainUiLoop();
	}

	if (gui_is_open() || gui_state == GuiState::VJoyEdit) {
		if (netmode_ == NetMode::Lbs && lbs_net_.IsConnected()) {
			// Prevent disconnection from lobby server
			lbs_net_.OnSockPoll();
		}
	}
}

std::vector<u8> Gdxsv::GeneratePlatformInfoPacket() {
	std::stringstream ss;
	ss << "cpu="
	   <<
#if HOST_CPU == CPU_X86
		"x86"
#elif HOST_CPU == CPU_ARM
		"ARM"
#elif HOST_CPU == CPU_MIPS
		"MIPS"
#elif HOST_CPU == CPU_X64
		"x86/64"
#elif HOST_CPU == CPU_GENERIC
		"Generic"
#elif HOST_CPU == CPU_ARM64
		"ARM64"
#else
		"Unknown"
#endif
	   << "\n";
	ss << "os="
	   <<
#ifdef __ANDROID__
		"Android"
#elif defined(__unix__)
		"Linux"
#elif defined(__APPLE__)
#ifdef TARGET_IPHONE
		"iOS"
#else
		"macOS"
#endif
#elif defined(_WIN32)
		"Windows"
#else
		"Unknown"
#endif
	   << "\n";
	ss << "flycast=" << GIT_VERSION << "\n";
	ss << "git_hash=" << GIT_HASH << "\n";
	ss << "build_date=" << BUILD_DATE << "\n";
	ss << "disk=" << (int)disk_ << "\n";
	ss << "wireless=" << (int)(os_GetConnectionMedium() == "Wireless") << "\n";
	ss << "patch_id=" << symbols_[":patch_id"] << "\n";
	ss << "language=" << GdxsvLanguage::TextureDirectoryName() << "\n";
	ss << "local_ip=" << lbs_net_.LocalIP() << "\n";
	ss << "udp_port=" << config::GdxLocalPort << "\n";
	std::string machine_id = os_GetMachineID();
	if (machine_id.length()) {
		auto digest = XXH64(machine_id.c_str(), machine_id.size(), 37);
		ss << "machine_id=" << std::hex << digest << std::dec << "\n";
	}

	if (gcp_ping_test_mutex_.try_lock()) {
		for (const auto &res : gcp_ping_test_result_) {
			ss << res.first << "=" << res.second << "\n";
		}
		gcp_ping_test_mutex_.unlock();
	}

	auto s = ss.str();
	std::vector<u8> packet = {0x81, 0xff, 0x99, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff};
	packet.push_back((s.size() >> 8) & 0xffu);
	packet.push_back(s.size() & 0xffu);
	std::copy(std::begin(s), std::end(s), std::back_inserter(packet));
	std::vector<u8> e_loginkey(loginkey_.size());
	static const int magic[] = {0x46, 0xcf, 0x2d, 0x55};
	for (int i = 0; i < e_loginkey.size(); ++i) e_loginkey[i] ^= loginkey_[i] ^ magic[i & 3];
	packet.push_back((e_loginkey.size() >> 8) & 0xffu);
	packet.push_back(e_loginkey.size() & 0xffu);
	std::copy(std::begin(e_loginkey), std::end(e_loginkey), std::back_inserter(packet));
	u16 payload_size = (u16)(packet.size() - 12);
	packet[4] = (payload_size >> 8) & 0xffu;
	packet[5] = payload_size & 0xffu;
	return packet;
}

std::vector<u8> Gdxsv::GenerateP2PMatchReportPacket() {
	auto rbk_report = rollback_net_.GetReport();
	if (rbk_report.battle_code().empty()) {
		return {};
	}

	auto msg = LbsMessage::ClNotice(LbsMessage::lbsP2PMatchingReport);
	auto lines = InMemoryListener::getInstance()->getLog();

	for (const auto &line : lines) {
		*rbk_report.mutable_logs()->Add() = line;
	}

	std::string data;
	if (rbk_report.SerializeToString(&data)) {
		z_stream z{};
		int ret = deflateInit(&z, Z_DEFAULT_COMPRESSION);
		if (ret == Z_OK) {
			char zbuf[1024];
			z.next_in = (Bytef *)data.c_str();
			z.avail_in = (uInt)data.size();
			for (;;) {
				z.next_out = (Bytef *)zbuf;
				z.avail_out = sizeof(zbuf);
				ret = deflate(&z, 0 < z.avail_in ? Z_NO_FLUSH : Z_FINISH);
				if (ret != Z_OK && ret != Z_STREAM_END) {
					ERROR_LOG(COMMON, "zlib serialize error: %d", ret);
					break;
				}
				std::copy_n(zbuf, sizeof(zbuf) - z.avail_out, std::back_inserter(msg.body));
				if (ret == Z_STREAM_END) break;
			}
			deflateEnd(&z);
		} else {
			ERROR_LOG(COMMON, "zlib init error: %d", ret);
		}
	} else {
		ERROR_LOG(COMMON, "report serialize error");
	}

	rollback_net_.ClearReport();

	if (32700 <= msg.body.size()) {
		ERROR_LOG(COMMON, "too large body");
		return {};
	}

	std::vector<u8> buf;
	msg.Serialize(buf);
	NOTICE_LOG(COMMON, "report msg size %d", buf.size());
	return buf;
}

void Gdxsv::HandleRPC() {
	u32 gdx_rpc_addr = symbols_["gdx_rpc"];
	if (gdx_rpc_addr == 0) {
		return;
	}

	u32 response = 0;
	gdx_rpc_t gdx_rpc{};
	gdx_rpc.request = gdxsv_ReadMem32(gdx_rpc_addr);
	gdx_rpc.response = gdxsv_ReadMem32(gdx_rpc_addr + 4);
	gdx_rpc.param1 = gdxsv_ReadMem32(gdx_rpc_addr + 8);
	gdx_rpc.param2 = gdxsv_ReadMem32(gdx_rpc_addr + 12);
	gdx_rpc.param3 = gdxsv_ReadMem32(gdx_rpc_addr + 16);
	gdx_rpc.param4 = gdxsv_ReadMem32(gdx_rpc_addr + 20);

	if (gdx_rpc.request == GDX_RPC_SOCK_OPEN) {
		const u32 tolobby = gdx_rpc.param1;
		const u32 host_ip = gdx_rpc.param2;
		const u16 port = gdx_rpc.param3;

		if (netmode_ == NetMode::Replay) {
			replay_net_.Open();
		} else if (tolobby == 1) {
			if (lbs_net_.Connect(server_, port)) {
				netmode_ = NetMode::Lbs;
				lbs_net_.Send(GeneratePlatformInfoPacket());
				lbs_net_.Send(GenerateP2PMatchReportPacket());
				lbs_remote_.Open(server_.c_str(), port);
				InitUDP(true);
			} else {
				netmode_ = NetMode::Offline;
			}
		} else {
			if (~host_ip == 0) {
				lbs_remote_.Close();
				udp_.Close();
				rollback_net_.Open();
				netmode_ = NetMode::McsRollback;
			} else {
				char addr_buf[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &host_ip, addr_buf, INET_ADDRSTRLEN);
				if (udp_net_.Connect(std::string(addr_buf), port)) {
					netmode_ = NetMode::McsUdp;
				} else {
					netmode_ = NetMode::Offline;
				}
			}
		}
	}

	if (gdx_rpc.request == GDX_RPC_SOCK_CLOSE) {
		if (netmode_ == NetMode::Lbs) {
			netmode_ = NetMode::Offline;
			if (!going_to_battle_) {
				lbs_net_.Close();
			}
			going_to_battle_ = false;
		} else if (netmode_ == NetMode::McsUdp) {
			netmode_ = NetMode::Offline;
			udp_net_.CloseMcsRemoteWithReason("cl_app_close");

		} else if (netmode_ == NetMode::McsRollback) {
			rollback_net_.SetCloseReason("cl_app_close");
			rollback_net_.Close();

			netmode_ = NetMode::Offline;
		} else if (netmode_ == NetMode::Replay) {
			replay_net_.Close();
		}
	}

	if (gdx_rpc.request == GDX_RPC_SOCK_READ) {
		if (netmode_ == NetMode::Lbs) {
			response = lbs_net_.OnSockRead(gdx_rpc.param1, gdx_rpc.param2);
		} else if (netmode_ == NetMode::McsUdp) {
			response = udp_net_.OnSockRead(gdx_rpc.param1, gdx_rpc.param2);
		} else if (netmode_ == NetMode::Replay) {
			response = replay_net_.OnSockRead(gdx_rpc.param1, gdx_rpc.param2);
		} else if (netmode_ == NetMode::McsRollback) {
			response = rollback_net_.OnSockRead(gdx_rpc.param1, gdx_rpc.param2);
		}
	}

	if (gdx_rpc.request == GDX_RPC_SOCK_WRITE) {
		if (netmode_ == NetMode::Lbs) {
			response = lbs_net_.OnSockWrite(gdx_rpc.param1, gdx_rpc.param2);
		} else if (netmode_ == NetMode::McsUdp) {
			response = udp_net_.OnSockWrite(gdx_rpc.param1, gdx_rpc.param2);
		} else if (netmode_ == NetMode::Replay) {
			response = replay_net_.OnSockWrite(gdx_rpc.param1, gdx_rpc.param2);
		} else if (netmode_ == NetMode::McsRollback) {
			response = rollback_net_.OnSockWrite(gdx_rpc.param1, gdx_rpc.param2);
		}
	}

	if (gdx_rpc.request == GDX_RPC_SOCK_POLL) {
		if (netmode_ == NetMode::Lbs) {
			response = lbs_net_.OnSockPoll();
		} else if (netmode_ == NetMode::McsUdp) {
			response = udp_net_.OnSockPoll();
		} else if (netmode_ == NetMode::Replay) {
			response = replay_net_.OnSockPoll();
		} else if (netmode_ == NetMode::McsRollback) {
			response = rollback_net_.OnSockPoll();
		}
	}

	gdxsv_WriteMem32(gdx_rpc_addr, 0);
	gdxsv_WriteMem32(gdx_rpc_addr + 4, response);
	gdxsv_WriteMem32(gdx_rpc_addr + 8, 0);
	gdxsv_WriteMem32(gdx_rpc_addr + 12, 0);
	gdxsv_WriteMem32(gdx_rpc_addr + 16, 0);
	gdxsv_WriteMem32(gdx_rpc_addr + 20, 0);

	gdxsv_WriteMem32(symbols_["is_online"], netmode_ != NetMode::Offline);
}

void Gdxsv::StartPingTest() {
	std::thread([this]() {
		std::this_thread::sleep_for(std::chrono::seconds(3));
		GcpPingTest();
	}).detach();
}

void Gdxsv::GcpPingTest() {
	std::map<std::string, int> test_result;

	// powered by https://github.com/GoogleCloudPlatform/gcping
	static const std::string get_path = "/api/ping";
	static const std::map<std::string, std::string> gcp_region_hosts = {
		{"asia-east1", "asia-east1-5tkroniexa-de.a.run.app"},
		{"asia-east2", "asia-east2-5tkroniexa-df.a.run.app"},
		{"asia-northeast1", "asia-northeast1-5tkroniexa-an.a.run.app"},
		{"asia-northeast2", "asia-northeast2-5tkroniexa-dt.a.run.app"},
		{"asia-northeast3", "asia-northeast3-5tkroniexa-du.a.run.app"},
		{"asia-south1", "asia-south1-5tkroniexa-el.a.run.app"},
		{"asia-southeast1", "asia-southeast1-5tkroniexa-as.a.run.app"},
		{"australia-southeast1", "australia-southeast1-5tkroniexa-ts.a.run.app"},
		{"europe-north1", "europe-north1-5tkroniexa-lz.a.run.app"},
		{"europe-west1", "europe-west1-5tkroniexa-ew.a.run.app"},
		{"europe-west2", "europe-west2-5tkroniexa-nw.a.run.app"},
		{"europe-west3", "europe-west3-5tkroniexa-ey.a.run.app"},
		{"europe-west4", "europe-west4-5tkroniexa-ez.a.run.app"},
		{"europe-west6", "europe-west6-5tkroniexa-oa.a.run.app"},
		{"northamerica-northeast1", "northamerica-northeast1-5tkroniexa-nn.a.run.app"},
		{"southamerica-east1", "southamerica-east1-5tkroniexa-rj.a.run.app"},
		{"us-central1", "us-central1-5tkroniexa-uc.a.run.app"},
		{"us-east1", "us-east1-5tkroniexa-ue.a.run.app"},
		{"us-east4", "us-east4-5tkroniexa-uk.a.run.app"},
		{"us-west1", "us-west1-5tkroniexa-uw.a.run.app"},
		{"us-west2", "us-west2-5tkroniexa-wl.a.run.app"},
		{"us-west3", "us-west3-5tkroniexa-wm.a.run.app"},
	};

	for (const auto &region_host : gcp_region_hosts) {
		TcpClient client;
		std::stringstream ss;
		ss << "HEAD " << get_path << " HTTP/1.1"
		   << "\r\n";
		ss << "Host: " << region_host.second << "\r\n";
		ss << "User-Agent: flycast for gdxsv"
		   << "\r\n";
		ss << "Accept: */*"
		   << "\r\n";
		ss << "\r\n";  // end of header

		if (!client.Connect(region_host.second.c_str(), 80)) {
			ERROR_LOG(COMMON, "connect failed : %s", region_host.first.c_str());
			continue;
		}

		auto request_header = ss.str();
		auto t1 = std::chrono::high_resolution_clock::now();
		int n = client.Send(request_header.c_str(), request_header.size());
		if (n < request_header.size()) {
			ERROR_LOG(COMMON, "send failed : %s", region_host.first.c_str());
			client.Close();
			continue;
		}

		char buf[1024] = {0};
		n = client.Recv(buf, 1024);
		if (n <= 0) {
			ERROR_LOG(COMMON, "recv failed : %s", region_host.first.c_str());
			client.Close();
			continue;
		}

		auto t2 = std::chrono::high_resolution_clock::now();
		int rtt = (int)std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
		const std::string response_header(buf, n);
		if (response_header.find("200 OK") == std::string::npos && response_header.find("302 Found") == std::string::npos) {
			ERROR_LOG(COMMON, "error response : %s", response_header.c_str());
		} else {
			test_result[region_host.first] = rtt;
			char latency_str[256];
			snprintf(latency_str, 256, "%s : %d[ms]", region_host.first.c_str(), rtt);
			NOTICE_LOG(COMMON, "%s", latency_str);
		}
		client.Close();
	}

	gcp_ping_test_mutex_.lock();
	gcp_ping_test_result_.swap(test_result);
	gcp_ping_test_mutex_.unlock();
}

bool Gdxsv::InitUDP(bool upnp) {
	if (!udp_.Initialized() || config::GdxLocalPort != udp_.bound_port()) {
		if (!udp_.Bind(config::GdxLocalPort)) {
			return false;
		}
	}

	if (config::GdxLocalPort != udp_.bound_port()) {
		config::GdxLocalPort = udp_.bound_port();
	}

	if (upnp && config::EnableUPnP && upnp_port_ != udp_.bound_port()) {
		upnp_port_ = udp_.bound_port();
		upnp_result_ = std::async(std::launch::async, [this]() -> std::string {
			NOTICE_LOG(COMMON, "UPnP AddPortMapping port=%d", upnp_port_);
			std::string result = upnp_.Init() && upnp_.AddPortMapping(upnp_port_, false) ? "Success" : upnp_.getLastError();
			NOTICE_LOG(COMMON, "UPnP AddPortMapping port=%d %s", upnp_port_, result.c_str());
			return result;
		});
	}

	return true;
}

std::string Gdxsv::GenerateLoginKey() {
	const int n = 8;
	uint64_t seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
	std::mt19937 gen(seed);
	std::string chars = "0123456789";
	std::uniform_int_distribution<> dist(0, chars.length() - 1);
	std::string key(n, 0);
	std::generate_n(key.begin(), n, [&]() { return chars[dist(gen)]; });
	return key;
}

void Gdxsv::ApplyOnlinePatch(bool first_time) {
	for (int i = 0; i < patch_list_.patches_size(); ++i) {
		auto &patch = patch_list_.patches(i);
		if (patch.write_once() && !first_time) {
			continue;
		}
		if (first_time) {
			NOTICE_LOG(COMMON, "patch apply: %s", patch.name().c_str());
		}
		for (int j = 0; j < patch.codes_size(); ++j) {
			auto &code = patch.codes(j);
			if (code.size() == 8) {
				gdxsv_WriteMem8(code.address(), (u8)(code.changed() & 0xff));
			}
			if (code.size() == 16) {
				gdxsv_WriteMem16(code.address(), (u16)(code.changed() & 0xffff));
			}
			if (code.size() == 32) {
				gdxsv_WriteMem32(code.address(), code.changed());
			}
		}
	}
}

void Gdxsv::RestoreOnlinePatch() {
	for (int i = 0; i < patch_list_.patches_size(); ++i) {
		auto &patch = patch_list_.patches(i);
		NOTICE_LOG(COMMON, "patch restore: %s", patch.name().c_str());
		for (int j = 0; j < patch.codes_size(); ++j) {
			auto &code = patch.codes(j);
			if (code.size() == 8) {
				gdxsv_WriteMem8(code.address(), (u8)(code.original() & 0xff));
			}
			if (code.size() == 16) {
				gdxsv_WriteMem16(code.address(), (u16)(code.original() & 0xffff));
			}
			if (code.size() == 32) {
				gdxsv_WriteMem32(code.address(), code.original());
			}
		}
	}
	patch_list_.clear_patches();
}

void Gdxsv::WritePatch() {
	if (disk_ == 1) WritePatchDisk1();
	if (disk_ == 2) WritePatchDisk2();
	if (symbols_["patch_id"] == 0 || gdxsv_ReadMem32(symbols_["patch_id"]) != symbols_[":patch_id"]) {
		NOTICE_LOG(COMMON, "patch %d %d", gdxsv_ReadMem32(symbols_["patch_id"]), symbols_[":patch_id"]);

#include "gdxsv_patch.inc"

		gdxsv_WriteMem32(symbols_["disk"], (int)disk_);
	}

	if (disk_ == 2) {
		if (symbols_["lang_patch_id"] == 0 || gdxsv_ReadMem32(symbols_["lang_patch_id"]) != symbols_[":lang_patch_id"] ||
			symbols_[":lang_patch_lang"] != (u8)GdxsvLanguage::Language()) {
			NOTICE_LOG(COMMON, "lang_patch id=%d prev=%d lang=%d", gdxsv_ReadMem32(symbols_["lang_patch_id"]), symbols_[":lang_patch_id"],
					   GdxsvLanguage::Language());
#include "gdxsv_translation_patch.inc"
		}
	}
}

void Gdxsv::WritePatchDisk1() {
	const u32 offset = 0x8C000000 + 0x00010000;

	// Max Rebattle Patch
	gdxsv_WriteMem8(0x0c0345b0, maxrebattle_);

	// Fix cost 300 to 295
	gdxsv_WriteMem16(0x0c1b0fd0, 295);

	// Send key message every frame
	gdxsv_WriteMem8(0x0c310450, 1);

	// Reduce max lag-frame
	gdxsv_WriteMem8(0x0c310451, maxlag_);

	// Modem connection fix
	const char *atm1 = "ATM1\r                                ";
	for (int i = 0; i < strlen(atm1); ++i) {
		gdxsv_WriteMem8(offset + 0x0015e703 + i, u8(atm1[i]));
	}

	// Overwrite serve address (max 20 chars)
	for (int i = 0; i < 20; ++i) {
		gdxsv_WriteMem8(offset + 0x0015e788 + i, (i < server_.length()) ? u8(server_[i]) : u8(0));
	}

	// Skip form validation
	gdxsv_WriteMem16(offset + 0x0003b0c4, u16(9));	// nop
	gdxsv_WriteMem16(offset + 0x0003b0cc, u16(9));	// nop
	gdxsv_WriteMem16(offset + 0x0003b0d4, u16(9));	// nop
	gdxsv_WriteMem16(offset + 0x0003b0dc, u16(9));	// nop

	// Write LoginKey
	if (gdxsv_ReadMem8(offset - 0x10000 + 0x002f6924) == 0) {
		for (int i = 0; i < std::min(loginkey_.length(), size_t(8)) + 1; ++i) {
			gdxsv_WriteMem8(offset - 0x10000 + 0x002f6924 + i, (i < loginkey_.length()) ? u8(loginkey_[i]) : u8(0));
		}
	}

	// Ally HP
	u16 hp_offset = 0x0180;
	if (InGame() && gdxsv_ReadMem8(0x0c336254) == 2 && gdxsv_ReadMem8(0x0c336255) == 7) {
		u8 player_index = gdxsv_ReadMem8(0x0c2f6652);
		if (1 <= player_index && player_index <= 4) {
			player_index--;
			// depend on 4 player battle
			u8 ally_index = player_index - (player_index & 1) + !(player_index & 1);
			u16 ally_hp = gdxsv_ReadMem16(0x0c3369d6 + ally_index * 0x2000);
			gdxsv_WriteMem16(0x0c3369d2 + player_index * 0x2000, ally_hp);
			hp_offset -= 2;
		}
	}
	gdxsv_WriteMem16(0x0c01d336, hp_offset);
	gdxsv_WriteMem16(0x0c01d56e, hp_offset);
	gdxsv_WriteMem16(0x0c01d678, hp_offset);
	gdxsv_WriteMem16(0x0c01d89e, hp_offset);

	// Disable soft reset
	gdxsv_WriteMem8(0x0c2f6657, InGame() ? 1 : 0);

	// Dirty widescreen cheat
	if (config::WidescreenGameHacks.get()) {
		u32 ratio = 0x3faaaaab;	 // default 4/3
		int stretching = 100;
		if (gdxsv_ReadMem8(0x0c336254) == 2 && (gdxsv_ReadMem8(0x0c336255) == 5 || gdxsv_ReadMem8(0x0c336255) == 7)) {
			ratio = 0x40155555;
			stretching = 175;
		}
		config::ScreenStretching.override(stretching);
		gdxsv_WriteMem32(0x0c189198, ratio);
		gdxsv_WriteMem32(0x0c1891a8, ratio);
		gdxsv_WriteMem32(0x0c1891b8, ratio);
		gdxsv_WriteMem32(0x0c1891c8, ratio);
	}

	// Online patch
	ApplyOnlinePatch(false);
}

void Gdxsv::WritePatchDisk2() {
	const u32 offset = 0x8C000000 + 0x00010000;

	// Max Rebattle Patch
	gdxsv_WriteMem8(0x0c0219ec, maxrebattle_);

	// Fix cost 300 to 295
	gdxsv_WriteMem16(0x0c21bfec, 295);
	gdxsv_WriteMem16(0x0c21bff4, 295);
	gdxsv_WriteMem16(0x0c21c034, 295);

	// Send key message every frame
	gdxsv_WriteMem8(0x0c3abb90, 1);

	// Reduce max lag-frame
	gdxsv_WriteMem8(0x0c3abb91, maxlag_);

	// Modem connection fix
	const char *atm1 = "ATM1\r                                ";
	for (int i = 0; i < strlen(atm1); ++i) {
		gdxsv_WriteMem8(offset + 0x001be7c7 + i, u8(atm1[i]));
	}

	// Overwrite serve address (max 20 chars)
	for (int i = 0; i < 20; ++i) {
		gdxsv_WriteMem8(offset + 0x001be84c + i, (i < server_.length()) ? u8(server_[i]) : u8(0));
	}

	// Skip form validation
	gdxsv_WriteMem16(offset + 0x000284f0, u16(9));	// nop
	gdxsv_WriteMem16(offset + 0x000284f8, u16(9));	// nop
	gdxsv_WriteMem16(offset + 0x00028500, u16(9));	// nop
	gdxsv_WriteMem16(offset + 0x00028508, u16(9));	// nop

	// Write LoginKey
	if (gdxsv_ReadMem8(offset - 0x10000 + 0x00392064) == 0) {
		for (int i = 0; i < std::min(loginkey_.length(), size_t(8)) + 1; ++i) {
			gdxsv_WriteMem8(offset - 0x10000 + 0x00392064 + i, (i < loginkey_.length()) ? u8(loginkey_[i]) : u8(0));
		}
	}

	// Ally HP
	u16 hp_offset = 0x0180;
	if (InGame() && gdxsv_ReadMem8(0x0c3d16d4) == 2 && gdxsv_ReadMem8(0x0c3d16d5) == 7) {
		u8 player_index = gdxsv_ReadMem8(0x0c391d92);
		if (1 <= player_index && player_index <= 4) {
			player_index--;
			// depend on 4 player battle
			u8 ally_index = player_index - (player_index & 1) + !(player_index & 1);
			u16 ally_hp = gdxsv_ReadMem16(0x0c3d1e56 + ally_index * 0x2000);
			gdxsv_WriteMem16(0x0c3d1e52 + player_index * 0x2000, ally_hp);
			hp_offset -= 2;
		}
	}
	gdxsv_WriteMem16(0x0c11da88, hp_offset);
	gdxsv_WriteMem16(0x0c11dbbc, hp_offset);
	gdxsv_WriteMem16(0x0c11dcc0, hp_offset);
	gdxsv_WriteMem16(0x0c11ddd6, hp_offset);
	gdxsv_WriteMem16(0x0c11df08, hp_offset);
	gdxsv_WriteMem16(0x0c11e01a, hp_offset);

	// Disable soft reset
	gdxsv_WriteMem8(0x0c391d97, InGame() ? 1 : 0);

	// Dirty widescreen cheat
	if (config::WidescreenGameHacks.get()) {
		u32 ratio = 0x3faaaaab;	 // default 4/3
		int stretching = 100;
		if (gdxsv_ReadMem8(0x0c3d16d4) == 2 && (gdxsv_ReadMem8(0x0c3d16d5) == 5 || gdxsv_ReadMem8(0x0c3d16d5) == 7)) {
			ratio = 0x40155555;
			stretching = 175;
		}
		config::ScreenStretching.override(stretching);
		gdxsv_WriteMem32(0x0c1e7948, ratio);
		gdxsv_WriteMem32(0x0c1e7958, ratio);
		gdxsv_WriteMem32(0x0c1e7968, ratio);
		gdxsv_WriteMem32(0x0c1e7978, ratio);
	}

	// Online patch
	ApplyOnlinePatch(false);
}

bool Gdxsv::StartReplayFile(const char *path, int pov) {
	replay_net_.Reset();
	if (replay_net_.StartFile(path, pov)) {
		netmode_ = NetMode::Replay;
		return true;
	}

	auto str = std::string(path);
	if (4 <= str.length() && str.substr(0, 4) == "http") {
		auto resp = os_FetchStringFromURL(str);
		if (0 < resp.size() && replay_net_.StartBuffer(resp.data(), resp.size())) {
			netmode_ = NetMode::Replay;
			return true;
		}
	}
	return false;
}

void Gdxsv::StopReplay() { replay_net_.Close(true); }

bool Gdxsv::StartRollbackTest(const char *param) {
	rollback_net_.Reset();

	if (rollback_net_.StartLocalTest(param)) {
		netmode_ = NetMode::McsRollback;
		return true;
	}

	return false;
}

Gdxsv gdxsv;
