#pragma once

#include <deque>
#include <future>
#include <mutex>
#include <string>

#include "gdxsv.pb.h"
#include "network/net_platform.h"
#include "types.h"

class TcpClient {
   public:
	~TcpClient() { Close(); }

	bool Connect(const char *host, int port);

	void SetNonBlocking();

	int IsConnected() const;

	int Recv(char *buf, int len);

	int Send(const char *buf, int len);

	void Close();

	u32 ReadableSize() const;

	const std::string &host() const { return host_; }

	const std::string &local_ip() const { return local_ip_; }

	int port() const { return port_; }

   private:
	sock_t sock_ = INVALID_SOCKET;
	std::string host_;
	std::string local_ip_;
	int port_;
};

class MessageBuffer {
   public:
	static const int kBufSize = 50;

	MessageBuffer();

	void SessionId(const std::string &session_id);

	bool CanPush() const;

	bool PushBattleMessage(const std::string &user_id, u8 *body, u32 body_length);

	const proto::Packet &Packet();

	void ApplySeqAck(u32 seq, u32 ack);

	void Clear();

   private:
	u32 msg_seq_;
	u32 snd_seq_;
	proto::Packet packet_;
};

class MessageFilter {
   public:
	bool IsNextMessage(const proto::BattleMessage &msg);

	void Clear();

   private:
	std::map<std::string, u32> recv_seq;
};

class UdpRemote {
   public:
	bool Open(const char *host, int port);

	bool Open(const std::string &addr);

	void Close();

	bool is_open() const;

	const std::string &str_addr() const;

	const sockaddr_in &net_addr() const;

   private:
	bool is_open_;
	std::string str_addr_;
	sockaddr_in net_addr_;
};

class UdpClient {
   public:
	bool Bind(int port);

	bool Initialized() const;

	int RecvFrom(char *buf, int len, std::string &sender);

	int SendTo(const char *buf, int len, const UdpRemote &remote);

	u32 ReadableSize() const;

	void Close();

	int bind_port() const { return bind_port_; }

   private:
	sock_t sock_ = INVALID_SOCKET;
	int bind_port_;
	std::string bind_ip_;
};

class UdpPingPong {
   public:
	static const int N = 4;

	void Start(uint32_t session_id, uint8_t peer_id, int port, int timeout_min_ms, int timeout_max_ms);

	void Stop();

	void Reset();

	bool Running();

	int ElapsedMs();

	void AddCandidate(const std::string &user_id, uint8_t peer_id, const std::string &ip, int port);

	bool GetAvailableAddress(uint8_t peer_id, sockaddr_in *dst, float *rtt);

	void GetRttMatrix(uint8_t matrix[N][N]);

   private:
	static const uint32_t MAGIC = 1434750950;
	static const uint8_t PING = 1;
	static const uint8_t PONG = 2;

	struct Candidate {
		uint8_t peer_id;
		UdpRemote remote;
		int ping_count;
		int pong_count;
		float rtt;
	};

#pragma pack(1)
	struct Packet {
		uint32_t magic;
		uint32_t session_id;
		uint8_t type;
		uint8_t from_peer_id;
		uint8_t to_peer_id;
		uint64_t send_timestamp;
		uint64_t ping_timestamp;
		uint8_t rtt_matrix[N][N];
	};
#pragma pack()

	std::atomic<bool> running_;
	std::chrono::high_resolution_clock::time_point start_time_;
	UdpClient client_ = UdpClient{};
	std::recursive_mutex mutex_;
	uint8_t rtt_matrix_[N][N];
	std::vector<Candidate> candidates_;
	std::map<std::string, int> user_to_peer_;
	std::map<int, std::string> peer_to_user_;
};
