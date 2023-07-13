#include "gdxsv_backend_udp.h"

#include <cmath>

#include "gdxsv.h"
#include "libs.h"
#include "rend/gui.h"

void GdxsvBackendUdp::Reset() {
	CloseMcsRemoteWithReason("cl_hard_reset");
	session_id_.clear();
	net_terminate_ = true;
}

bool GdxsvBackendUdp::Connect(const std::string& host, u16 port) {
	if (!udp_client_.Initialized()) {
		bool ok = udp_client_.Bind(0);
		if (!ok) {
			WARN_LOG(COMMON, "Failed to Initialize Udp %s:%d", host.c_str(), port);
			return false;
		}
	}

	CloseMcsRemoteWithReason("connect");
	bool ok = mcs_remote_.Open(host.c_str(), port);
	if (!ok) {
		WARN_LOG(COMMON, "Failed to open Udp %s:%d", host.c_str(), port);
		return false;
	}

	net_terminate_ = false;
	std::thread([this]() { NetThreadLoop(); }).detach();
	return true;
}

bool GdxsvBackendUdp::IsConnected() const { return mcs_remote_.is_open(); }

void GdxsvBackendUdp::CloseMcsRemoteWithReason(const char* reason) {
	if (udp_client_.Initialized() && mcs_remote_.is_open()) {
		proto::Packet pkt;
		pkt.Clear();
		pkt.set_type(proto::MessageType::Fin);
		pkt.set_session_id(session_id_.c_str(), session_id_.size());
		pkt.mutable_fin_data()->set_detail(reason);

		char buf[1024];
		if (pkt.SerializePartialToArray((void*)buf, (int)sizeof(buf))) {
			udp_client_.SendTo((const char*)buf, pkt.GetCachedSize(), mcs_remote_);
		} else {
			ERROR_LOG(COMMON, "packet serialize error");
		}

		mcs_remote_.Close();
	}
	net_terminate_ = true;
}

u32 GdxsvBackendUdp::OnSockRead(u32 addr, u32 size) {
	std::lock_guard<std::mutex> lock(recv_buf_mtx_);
	u32 n = std::min<u32>(recv_buf_.size(), size);
	for (int i = 0; i < n; ++i) {
		gdxsv_WriteMem8(addr + i, recv_buf_.front());
		recv_buf_.pop_front();
	}
	return n;
}

u32 GdxsvBackendUdp::OnSockWrite(u32 addr, u32 size) {
	std::lock_guard<std::mutex> lock(send_buf_mtx_);
	for (int i = 0; i < size; ++i) {
		send_buf_.push_back(gdxsv_ReadMem8(addr + i));
	}
	return size;
}

u32 GdxsvBackendUdp::OnSockPoll() {
	std::lock_guard<std::mutex> lock(recv_buf_mtx_);
	return recv_buf_.size();
}

void GdxsvBackendUdp::NetThreadLoop() {
	ClearBuffers();

	int net_loop_count = 0;
	int ping_send_count = 0;
	int ping_recv_count = 0;
	int rtt_sum = 0;
	int udp_retransmit_countdown = 0;
	std::string user_id;
	std::string session_id;
	u8 buf[16 * 1024];
	proto::Packet pkt;
	MessageBuffer msg_buf;
	MessageFilter msg_filter;

	enum class State {
		Start,
		McsSessionExchange,
		McsPingTest,
		McsInBattle,
		End,
	};
	State state = State::Start;

	while (!net_terminate_) {
		net_loop_count++;

		if (state == State::Start) {
			ClearBuffers();
			recv_buf_mtx_.lock();
			recv_buf_.assign({0x0e, 0x61, 0x00, 0x22, 0x10, 0x31, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd});
			recv_buf_mtx_.unlock();
			session_id_.clear();
			msg_buf.Clear();
			state = State::McsSessionExchange;
		}

		if (state == State::McsSessionExchange) {
			if (session_id.empty()) {
				send_buf_mtx_.lock();
				const int FirstMessageSize = 20;
				if (FirstMessageSize <= send_buf_.size()) {
					for (int j = 12; j < FirstMessageSize; ++j) {
						session_id.push_back((char)send_buf_[j]);
					}
					NOTICE_LOG(COMMON, "session_id:%s", session_id.c_str());
					session_id_ = session_id;
					msg_buf.SessionId(session_id);
					send_buf_.clear();
				}
				send_buf_mtx_.unlock();
			} else if (user_id.empty()) {
				if (net_loop_count % 100 == 0) {
					pkt.Clear();
					pkt.set_type(proto::MessageType::HelloServer);
					pkt.set_session_id(session_id);
					if (pkt.SerializeToArray((void*)buf, (int)sizeof(buf))) {
						udp_client_.SendTo((const char*)buf, pkt.GetCachedSize(), mcs_remote_);
					} else {
						ERROR_LOG(COMMON, "packet serialize error");
						CloseMcsRemoteWithReason("cl_error");
						state = State::End;
					}
				}
			} else {
				state = State::McsPingTest;
			}
		}

		if (state == State::McsPingTest) {
			if (ping_recv_count < 10) {
				if (net_loop_count % 100 == 0 || ping_send_count == ping_recv_count) {
					pkt.Clear();
					pkt.set_type(proto::MessageType::Ping);
					pkt.set_session_id(session_id_.c_str(), session_id_.size());
					pkt.mutable_ping_data()->set_user_id(user_id);
					pkt.mutable_ping_data()->set_timestamp(
						std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch())
							.count());
					ping_send_count++;
					if (pkt.SerializeToArray((void*)buf, (int)sizeof(buf))) {
						udp_client_.SendTo((const char*)buf, pkt.GetCachedSize(), mcs_remote_);
					} else {
						ERROR_LOG(COMMON, "packet serialize error");
						CloseMcsRemoteWithReason("cl_error");
						state = State::End;
					}
				}
			} else {
				auto rtt = float(rtt_sum) / ping_recv_count;
				NOTICE_LOG(COMMON, "PING AVG %.2f ms", rtt);
				gdxsv.maxlag_ = std::min<int>(0x7f, std::max(5, 4 + (int)std::floor(rtt / 16)));
				NOTICE_LOG(COMMON, "set maxlag %d", (int)gdxsv.maxlag_);

				char osd_msg[128] = {};
				sprintf(osd_msg, "PING:%.0fms DELAY:%dfr", rtt, (int)gdxsv.maxlag_);
				gui_display_notification(osd_msg, 3000);
				state = State::McsInBattle;
			}
		}

		if (state == State::McsInBattle) {
			std::lock_guard<std::mutex> lock(send_buf_mtx_);
			int n = send_buf_.size();
			if (0 < n || udp_retransmit_countdown-- == 0) {
				if (0 < n && msg_buf.CanPush()) {
					n = std::min<int>(n, sizeof(buf));
					for (int i = 0; i < n; ++i) {
						buf[i] = send_buf_.front();
						send_buf_.pop_front();
					}
					msg_buf.PushBattleMessage(user_id, buf, n);
				}

				if (msg_buf.Packet().SerializeToArray((void*)buf, (int)sizeof(buf))) {
					if (udp_client_.SendTo((const char*)buf, msg_buf.Packet().GetCachedSize(), mcs_remote_)) {
						udp_retransmit_countdown = 22;
					}
				}
			}
		}

		while (true) {
			sockaddr_storage sender{};
			socklen_t addrlen = sizeof(sockaddr_storage);
			int n = udp_client_.RecvFrom((char*)buf, sizeof(buf), &sender, &addrlen);
			if (n <= 0) {
				break;
			}

			if (!is_same_addr(reinterpret_cast<sockaddr*>(&sender), mcs_remote_.net_addr())) {
				continue;
			}

			if (!pkt.ParseFromArray(buf, n)) {
				ERROR_LOG(COMMON, "packet deserialize error");
				continue;
			}

			if (pkt.type() == proto::MessageType::HelloServer) {
				if (state == State::McsSessionExchange && pkt.hello_server_data().ok()) {
					user_id = pkt.hello_server_data().user_id();
					NOTICE_LOG(COMMON, "user_id:%s", user_id.c_str());
				}
			} else if (pkt.type() == proto::MessageType::Pong) {
				if (state == State::McsPingTest) {
					auto t2 =
						std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch())
							.count();
					auto rtt = static_cast<float>(t2 - pkt.pong_data().timestamp());
					ping_recv_count++;
					rtt_sum += rtt;
				}
			} else if (pkt.type() == proto::MessageType::Battle) {
				if (state == State::McsInBattle) {
					msg_buf.ApplySeqAck(pkt.seq(), pkt.ack());
					recv_buf_mtx_.lock();
					for (auto& msg : pkt.battle_data()) {
						if (msg_filter.IsNextMessage(msg)) {
							for (auto c : msg.body()) {
								recv_buf_.push_back(c);
							}
						}
					}
					recv_buf_mtx_.unlock();
				}
			} else if (pkt.type() == proto::Fin) {
				CloseMcsRemoteWithReason("cl_recv_fin");
				state = State::End;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	ClearBuffers();

	NOTICE_LOG(COMMON, "NetThread finished");
}

void GdxsvBackendUdp::ClearBuffers() {
	recv_buf_mtx_.lock();
	recv_buf_.clear();
	recv_buf_mtx_.unlock();

	send_buf_mtx_.lock();
	send_buf_.clear();
	send_buf_mtx_.unlock();
}
