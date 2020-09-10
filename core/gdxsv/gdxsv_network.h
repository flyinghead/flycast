#pragma once

#include <string>
#include <mutex>
#include <deque>

#include "types.h"
#include "network/net_platform.h"
#include "packet.h"

static const int kUserIdMaxLength = 16;
static const int kSessionIdMaxLength = 16;
static const int kMessageBodyMaxLength = 64;
static const int kPublicAddrMaxLength = 16;
static const int kBattleDataMaxLength = 128;

using BattleMessage = proto::BattleMessage<kUserIdMaxLength, kMessageBodyMaxLength>;
using PingMessage = proto::PingMessage<kUserIdMaxLength>;
using PongMessage = proto::PongMessage<kUserIdMaxLength, kPublicAddrMaxLength>;
using HelloServerMessage = proto::HelloServerMessage<kSessionIdMaxLength>;
using Packet = proto::Packet<kBattleDataMaxLength, kUserIdMaxLength, kMessageBodyMaxLength, kSessionIdMaxLength, kPublicAddrMaxLength>;

class TcpClient {
public:
    bool Connect(const char *host, int port);

    int IsConnected() const;

    int Recv(char *buf, int len);

    int Send(const char *buf, int len);

    void Close();

    u32 ReadableSize() const;

    const std::string &host() { return host_; }

    int port() const { return port_; }

private:
    sock_t sock_ = INVALID_SOCKET;
    std::string host_;
    int port_;
};


class UdpClient {
public:
    bool Connect(const char *host, int port);

    int IsConnected() const;

    int Recv(char *buf, int len);

    int Send(const char *buf, int len);

    u32 ReadableSize() const;

    void Close();

    const std::string &host() { return host_; }

    int port() const { return port_; }

private:
    sock_t sock_ = INVALID_SOCKET;
    sockaddr_in remote_addr_;
    std::string host_;
    int port_;
};

class MessageBuffer {
public:
    static const int kRingSize = 4096;

    MessageBuffer() = default;

    bool CanPush() const;

    bool PushBattleMessage(const std::string &id, u8 *body, u32 body_length);

    void FillSendData(Packet &packet);

    void ApplySeqAck(u32 seq, u32 ack);

    void Clear();

private:
    u32 msg_seq_{};
    u32 pkt_ack_{};
    u32 begin_{};
    u32 end_{};
    std::vector<BattleMessage> rbuf_;
};

class MessageFilter {
public:
    bool IsNextMessage(const BattleMessage &msg);

    void Clear();

private:
    std::mutex mtx;
    std::map<std::string, u32> recv_seq;
};


