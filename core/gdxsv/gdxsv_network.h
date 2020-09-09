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
static const int kBattleDataMaxLength = 64;

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

private:
    sock_t sock = INVALID_SOCKET;
};

class MessageBuffer {
public:
    static const int kRingSize = 4096;

    MessageBuffer();

    bool CanPush() const;

    bool PushBattleMessage(const std::string& id, u8 *body, u32 body_length);

    void FillSendData(Packet &packet);

    void ApplySeqAck(u32 seq, u32 ack);

    void Clear();

private:
    u32 msg_seq_;
    u32 pkt_ack_;
    u32 begin_;
    u32 end_;
    std::vector<BattleMessage> rbuf_;
};

class MessageFilter {
public:
    void Clear();

    bool Filter(const BattleMessage &msg);

private:
    std::mutex mtx;
    std::map<std::string, u32> recv_seq;
};


class UdpClient {
public:
    bool Connect(const char *host, int port);

    int IsConnected() const;

    int Recv(char *buf, int len);

    int Send(const char *buf, int len);

    u32 ReadableSize() const;

    void Close();

private:
    sock_t sock = INVALID_SOCKET;
    struct sockaddr_in remote_addr;

    std::mutex mtx;
    std::string id;
    u32 ack;
    u32 begin;
    u32 end;
};
