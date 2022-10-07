#include "gdxsv_network.h"

#include <cmath>

#ifndef _WIN32
#include <sys/ioctl.h>
#endif

bool TcpClient::Connect(const char *host, int port) {
	NOTICE_LOG(COMMON, "TCP Connect: %s:%d", host, port);

	sock_t new_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (new_sock == INVALID_SOCKET) {
		WARN_LOG(COMMON, "Connect fail 1 %d", get_last_error());
		return false;
	}
	auto host_entry = gethostbyname(host);
	if (host_entry == nullptr || host_entry->h_addr_list[0] == nullptr) {
		WARN_LOG(COMMON, "Connect fail 2 gethostbyname");
		return false;
	}

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
#ifdef _WIN32
	addr.sin_addr = *((LPIN_ADDR)host_entry->h_addr_list[0]);
#else
	memcpy(&addr.sin_addr, host_entry->h_addr_list[0], host_entry->h_length);
#endif
	addr.sin_port = htons(port);

	fd_set setW, setE;
	struct timeval timeout = {5, 0};
	int res;

	auto set_blocking_mode = [](const int &socket, bool is_blocking) {
#ifdef _WIN32
		u_long flags = is_blocking ? 0 : 1;
		ioctlsocket(socket, FIONBIO, &flags);
#else
		const int flags = fcntl(socket, F_GETFL, 0);
		fcntl(socket, F_SETFL, is_blocking ? flags ^ O_NONBLOCK : flags | O_NONBLOCK);
#endif
	};

	set_blocking_mode(new_sock, false);

	if (::connect(new_sock, (const sockaddr *)&addr, sizeof(addr)) != 0) {
		if (get_last_error() != EINPROGRESS && get_last_error() != L_EWOULDBLOCK) {
			WARN_LOG(COMMON, "Connect fail 2 %d", get_last_error());
			return false;
		} else {
			do {
				FD_ZERO(&setW);
				FD_SET(new_sock, &setW);
				FD_ZERO(&setE);
				FD_SET(new_sock, &setE);

				res = select(new_sock + 1, NULL, &setW, &setE, &timeout);
				if (res < 0 && errno != EINTR) {
					WARN_LOG(COMMON, "Connect fail 3 %d", get_last_error());
					return false;
				} else if (res > 0) {
					int error;
					socklen_t l = sizeof(int);
#ifdef _WIN32
					if (getsockopt(new_sock, SOL_SOCKET, SO_ERROR, (char *)&error, &l) < 0 || error) {
#else
					if (getsockopt(new_sock, SOL_SOCKET, SO_ERROR, &error, &l) < 0 || error) {
#endif
						WARN_LOG(COMMON, "Connect fail 4 %d", error);
						return false;
					}

					if (FD_ISSET(new_sock, &setE)) {
						WARN_LOG(COMMON, "Connect fail 5 %d", get_last_error());
						return false;
					}

					break;
				} else {
					WARN_LOG(COMMON, "Timeout in select() - Cancelling!");
					return false;
				}
			} while (1);
		}
		set_blocking_mode(new_sock, true);
	}

	if (sock_ != INVALID_SOCKET) {
		closesocket(sock_);
	}

	set_tcp_nodelay(new_sock);

	sock_ = new_sock;
	host_ = std::string(host);
	port_ = port;

	{
		sockaddr_in name{};
		socklen_t namelen = sizeof(name);
		if (getsockname(new_sock, reinterpret_cast<sockaddr *>(&name), &namelen) != 0) {
			WARN_LOG(COMMON, "getsockname failed");
		} else {
			char buf[INET_ADDRSTRLEN];
			local_ip_ = std::string(inet_ntop(AF_INET, &name.sin_addr, buf, INET_ADDRSTRLEN));
		}
	}

	NOTICE_LOG(COMMON, "TCP Connect: %s:%d ok", host, port);
	return true;
}

int TcpClient::IsConnected() const { return sock_ != INVALID_SOCKET; }

void TcpClient::SetNonBlocking() {
	set_recv_timeout(sock_, 1);
	set_send_timeout(sock_, 1);
	set_non_blocking(sock_);
}

int TcpClient::Recv(char *buf, int len) {
	int n = ::recv(sock_, buf, len, 0);
	if (n < 0 && get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK) {
		WARN_LOG(COMMON, "TCP Recv failed. errno=%d", get_last_error());
		this->Close();
	}
	if (n < 0) return 0;
	return n;
}

int TcpClient::Send(const char *buf, int len) {
	int n = ::send(sock_, buf, len, 0);
	if (n < 0 && get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK) {
		WARN_LOG(COMMON, "TCP Send failed. errno=%d", get_last_error());
		this->Close();
	}
	if (n < 0) return 0;
	return n;
}

u32 TcpClient::ReadableSize() const {
	u_long n = 0;
#ifndef _WIN32
	ioctl(sock_, FIONREAD, &n);
#else
	ioctlsocket(sock_, FIONREAD, &n);
#endif
	return u32(n);
}

void TcpClient::Close() {
	if (sock_ != INVALID_SOCKET) {
		closesocket(sock_);
		sock_ = INVALID_SOCKET;
	}
}

bool UdpRemote::Open(const char *host, int port) {
	auto host_entry = gethostbyname(host);
	if (host_entry == nullptr) {
		WARN_LOG(COMMON, "UDP Remote::Initialize failed. gethostbyname %s", host);
		return false;
	}

	is_open_ = true;
	str_addr_ = std::string(host) + ":" + std::to_string(port);
	net_addr_.sin_family = AF_INET;
	memcpy(&(net_addr_.sin_addr.s_addr), host_entry->h_addr, host_entry->h_length);
	net_addr_.sin_port = htons(port);
	return true;
}

bool UdpRemote::Open(const std::string &addr) {
	if (std::count(addr.begin(), addr.end(), ':') != 1) {
		return false;
	}
	size_t colon_pos = addr.find_first_of(':');
	std::string host = addr.substr(0, colon_pos);
	int port = std::stoi(addr.substr(colon_pos + 1));
	return Open(host.c_str(), port);
}

void UdpRemote::Close() {
	is_open_ = false;
	str_addr_.clear();
	memset(&net_addr_, 0, sizeof(net_addr_));
}

bool UdpRemote::is_open() const { return is_open_; }

const std::string &UdpRemote::str_addr() const { return str_addr_; }

const sockaddr_in &UdpRemote::net_addr() const { return net_addr_; }

bool UdpClient::Bind(int port) {
	sock_t new_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (new_sock == INVALID_SOCKET) {
		WARN_LOG(COMMON, "UDP Connect fail %d", get_last_error());
		return false;
	}

	int optval = 0;
	if (port != 0) {
		setsockopt(new_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&optval, sizeof optval);
	}
	optval = 0;
	setsockopt(new_sock, SOL_SOCKET, SO_LINGER, (const char *)&optval, sizeof optval);

	sockaddr_in recv_addr;
	memset(&recv_addr, 0, sizeof(recv_addr));
	recv_addr.sin_port = htons(port);  // if port == 0 then the system assign an available port.
	recv_addr.sin_family = AF_INET;
	recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (::bind(new_sock, (struct sockaddr *)&recv_addr, sizeof(recv_addr)) < 0) {
		ERROR_LOG(COMMON, "gdxsv: bind() failed. errno=%d", get_last_error());
		closesocket(new_sock);
		return false;
	}

	memset(&recv_addr, 0, sizeof(recv_addr));
	socklen_t addr_len = sizeof(recv_addr);
	if (::getsockname(new_sock, (struct sockaddr *)&recv_addr, &addr_len) < 0) {
		ERROR_LOG(COMMON, "gdxsv: getsockname() failed. errno=%d", get_last_error());
		closesocket(new_sock);
		return false;
	}

	set_recv_timeout(new_sock, 1);
	set_send_timeout(new_sock, 1);
	set_non_blocking(new_sock);

	sock_ = new_sock;
	bind_ip_ = std::string(::inet_ntoa(recv_addr.sin_addr));
	bind_port_ = ntohs(recv_addr.sin_port);
	NOTICE_LOG(COMMON, "UDP Initialize ok: %s:%d", bind_ip_.c_str(), bind_port_);
	return true;
}

bool UdpClient::Initialized() const { return sock_ != INVALID_SOCKET; }

int UdpClient::RecvFrom(char *buf, int len, std::string &sender) {
	sender.clear();
	sockaddr_in from_addr;
	socklen_t addrlen = sizeof(from_addr);
	memset(&from_addr, 0, sizeof(from_addr));

	int n = ::recvfrom(sock_, buf, len, 0, (struct sockaddr *)&from_addr, &addrlen);
	if (n < 0 && get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK) {
		WARN_LOG(COMMON, "UDP Recv failed. errno=%d", get_last_error());
		return 0;
	}
	sender = std::string(::inet_ntoa(from_addr.sin_addr)) + ":" + std::to_string(ntohs(from_addr.sin_port));
	if (n < 0) return 0;
	return n;
}

int UdpClient::SendTo(const char *buf, int len, const UdpRemote &remote) {
	int n = ::sendto(sock_, buf, len, 0, (const sockaddr *)&remote.net_addr(), sizeof(remote.net_addr()));
	if (n < 0 && get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK) {
		WARN_LOG(COMMON, "UDP Send failed. errno=%d", get_last_error());
		return 0;
	}
	if (n < 0) return 0;
	return n;
}

u32 UdpClient::ReadableSize() const {
	u_long n = 0;
#ifndef _WIN32
	ioctl(sock_, FIONREAD, &n);
#else
	ioctlsocket(sock_, FIONREAD, &n);
#endif
	return u32(n);
}

void UdpClient::Close() {
	if (sock_ != INVALID_SOCKET) {
		closesocket(sock_);
		sock_ = INVALID_SOCKET;
	}
}

MessageBuffer::MessageBuffer() { Clear(); }

bool MessageBuffer::CanPush() const { return packet_.battle_data().size() < kBufSize; }

void MessageBuffer::SessionId(const std::string &session_id) { packet_.set_session_id(session_id.c_str()); }

bool MessageBuffer::PushBattleMessage(const std::string &user_id, u8 *body, u32 body_length) {
	if (!CanPush()) {
		// buffer full
		return false;
	}

	auto msg = packet_.add_battle_data();
	msg->set_seq(msg_seq_);
	msg->set_user_id(user_id);
	msg->set_body(body, body_length);
	packet_.set_seq(msg_seq_);
	msg_seq_++;
	return true;
}

const proto::Packet &MessageBuffer::Packet() { return packet_; }

void MessageBuffer::ApplySeqAck(u32 seq, u32 ack) {
	if (snd_seq_ <= ack) {
		packet_.mutable_battle_data()->DeleteSubrange(0, ack - snd_seq_ + 1);
		snd_seq_ = ack + 1;
	}
	if (packet_.ack() < seq) {
		packet_.set_ack(seq);
	}
}

void MessageBuffer::Clear() {
	packet_.Clear();
	packet_.set_type(proto::MessageType::Battle);
	msg_seq_ = 1;
	snd_seq_ = 1;
}

bool MessageFilter::IsNextMessage(const proto::BattleMessage &msg) {
	auto last_seq = recv_seq[msg.user_id()];
	if (last_seq == 0 || msg.seq() == last_seq + 1) {
		recv_seq[msg.user_id()] = msg.seq();
		return true;
	}
	return false;
}

void MessageFilter::Clear() { recv_seq.clear(); }

void UdpPingPong::Start(uint32_t session_id, uint8_t peer_id, int port, int timeout_min_ms, int timeout_max_ms) {
	if (running_) return;
	verify(peer_id < N);
	client_.Close();
	client_.Bind(port);
	running_ = true;
	int network_delay = 0;
	const auto delay_option = std::getenv("GGPO_NETWORK_DELAY");
	if (delay_option != nullptr) {
		network_delay = atoi(delay_option);
		NOTICE_LOG(COMMON, "GGPO_NETWORK_DELAY is %d", network_delay);
	}

	std::set<int> peer_ids;
	peer_ids.insert(peer_id);
	for (const auto &c : candidates_) {
		peer_ids.insert(c.peer_id);
	}
	int peer_count = peer_ids.size();

	std::thread([this, session_id, peer_id, timeout_min_ms, timeout_max_ms, peer_count, network_delay]() {
		WARN_LOG(COMMON, "Start UdpPingPong Thread");
		start_time_ = std::chrono::high_resolution_clock::now();
		std::string sender;

		for (int loop_count = 0; running_; loop_count++) {
			auto now = std::chrono::high_resolution_clock::now();
			auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_).count();

			while (true) {
				Packet recv{};

				int n = client_.ReadableSize();
				if (n <= 0) {
					break;
				}

				n = client_.RecvFrom(reinterpret_cast<char *>(&recv), sizeof(Packet), sender);
				if (n <= 0) {
					break;
				}

				if (recv.magic != MAGIC) {
					WARN_LOG(COMMON, "invalid magic");
					continue;
				}

				if (recv.session_id != session_id) {
					WARN_LOG(COMMON, "invalid session_id");
					continue;
				}

				if (recv.to_peer_id != peer_id) {
					WARN_LOG(COMMON, "invalid to_peer_id");
					continue;
				}

				if (recv.from_peer_id == recv.to_peer_id) {
					WARN_LOG(COMMON, "invalid peer_id");
					continue;
				}

				if (recv.type == PING) {
					NOTICE_LOG(COMMON, "Recv PING from %d", recv.from_peer_id);
					std::lock_guard<std::recursive_mutex> lock(mutex_);

					Packet p{};
					p.magic = MAGIC;
					p.type = PONG;
					p.session_id = session_id;
					p.from_peer_id = peer_id;
					p.to_peer_id = recv.from_peer_id;
					p.send_timestamp =
						std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch())
							.count();
					p.ping_timestamp = recv.send_timestamp;
					p.ping_timestamp -= network_delay;
					UdpRemote remote;
					memcpy(p.rtt_matrix, rtt_matrix_, sizeof(rtt_matrix_));

					auto it = std::find_if(candidates_.begin(), candidates_.end(), [&recv, &sender](const Candidate &c) {
						return c.peer_id == recv.from_peer_id && c.remote.str_addr() == sender;
					});
					if (it != candidates_.end()) {
						remote = it->remote;
					} else {
						Candidate c{};
						c.peer_id = recv.from_peer_id;
						c.remote.Open(sender);
						candidates_.push_back(c);
						remote = c.remote;
					}
					if (remote.is_open()) {
						client_.SendTo(reinterpret_cast<const char *>(&p), sizeof(p), remote);
					}
				}

				if (recv.type == PONG) {
					NOTICE_LOG(COMMON, "Recv PONG from %d", recv.from_peer_id);
					const auto now =
						std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch())
							.count();
					auto rtt = static_cast<int>(now - recv.ping_timestamp);
					if (rtt <= 0) rtt = 1;

					auto it = std::find_if(candidates_.begin(), candidates_.end(), [&recv, &sender](const Candidate &c) {
						return c.peer_id == recv.from_peer_id && c.remote.str_addr() == sender;
					});
					if (it != candidates_.end()) {
						it->rtt = float(it->pong_count * it->rtt + rtt) / float(it->pong_count + 1);
						it->pong_count++;
						rtt_matrix_[peer_id][recv.from_peer_id] = static_cast<uint8_t>(std::min(255, (int)std::ceil(it->rtt)));
						for (int j = 0; j < N; j++) {
							rtt_matrix_[recv.from_peer_id][j] = recv.rtt_matrix[recv.from_peer_id][j];
						}
					} else {
						Candidate c{};
						c.peer_id = recv.from_peer_id;
						c.remote.Open(sender);
						candidates_.push_back(c);
					}
				}
			}

			if (loop_count % 100 == 0) {
				std::lock_guard<std::recursive_mutex> lock(mutex_);

				for (auto &c : candidates_) {
					NOTICE_LOG(COMMON, "Send PING to %d %s", c.peer_id, c.remote.str_addr().c_str());
					if (c.remote.is_open()) {
						Packet p{};
						p.magic = MAGIC;
						p.type = PING;
						p.session_id = session_id;
						p.from_peer_id = peer_id;
						p.to_peer_id = c.peer_id;
						p.send_timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
											   std::chrono::high_resolution_clock::now().time_since_epoch())
											   .count();
						p.send_timestamp -= network_delay;
						p.ping_timestamp = 0;
						memcpy(p.rtt_matrix, rtt_matrix_, sizeof(rtt_matrix_));
						client_.SendTo(reinterpret_cast<const char *>(&p), sizeof(p), c.remote);
						c.ping_count++;
					}
				}
			}

			if (loop_count % 500 == 0) {
				bool matrix_ok = true;
				std::lock_guard<std::recursive_mutex> lock(mutex_);

				NOTICE_LOG(COMMON, "RTT MATRIX");
				NOTICE_LOG(COMMON, "  %4d%4d%4d%4d", 0, 1, 2, 3);
				for (int i = 0; i < 4; i++) {
					NOTICE_LOG(COMMON, "%d>%4d%4d%4d%4d", i, rtt_matrix_[i][0], rtt_matrix_[i][1], rtt_matrix_[i][2], rtt_matrix_[i][3]);
				}

				NOTICE_LOG(COMMON, "peer_count%d", peer_count);
				for (int i = 0; i < peer_count; i++) {
					for (int j = 0; j < peer_count; j++) {
						if (i != j) {
							if (rtt_matrix_[i][j] == 0) {
								matrix_ok = false;
							}
						}
					}
				}

				if (matrix_ok && timeout_min_ms < ms) {
					NOTICE_LOG(COMMON, "UdpPingTest Finish ok");
					client_.Close();
					running_ = false;
					break;
				}
			}

			if (timeout_max_ms < ms) {
				NOTICE_LOG(COMMON, "UdpPingTest Finish timeout");
				client_.Close();
				running_ = false;
				break;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		WARN_LOG(COMMON, "End UdpPingPong Thread");
	}).detach();
}

void UdpPingPong::Stop() { running_ = false; }

void UdpPingPong::Reset() {
	running_ = false;
	start_time_ = std::chrono::high_resolution_clock::time_point{};
	client_.Close();

	std::lock_guard<std::recursive_mutex> lock(mutex_);
	memset(rtt_matrix_, 0, sizeof(rtt_matrix_));
	candidates_.clear();
	user_to_peer_.clear();
	peer_to_user_.clear();
}

bool UdpPingPong::Running() const { return running_; }

int UdpPingPong::ElapsedMs() const {
	auto now = std::chrono::high_resolution_clock::now();
	return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_).count();
}

void UdpPingPong::AddCandidate(const std::string &user_id, uint8_t peer_id, const std::string &ip, int port) {
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	Candidate c{};
	user_to_peer_[user_id] = peer_id;
	peer_to_user_[peer_id] = user_id;
	c.peer_id = peer_id;
	c.remote.Open(ip.c_str(), port);
	candidates_.push_back(c);
}

bool UdpPingPong::GetAvailableAddress(uint8_t peer_id, sockaddr_in *dst, float *rtt) {
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	float min_rtt = 1000.0f;
	bool found = false;
	for (auto &c : candidates_) {
		if (c.peer_id == peer_id && c.pong_count) {
			if (c.rtt < min_rtt) {
				min_rtt = c.rtt;
				*dst = c.remote.net_addr();
				*rtt = c.rtt;
				found = true;
			}
		}
	}
	return found;
}

void UdpPingPong::GetRttMatrix(uint8_t matrix[N][N]) {
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	memcpy(matrix, rtt_matrix_, sizeof(rtt_matrix_));
}
