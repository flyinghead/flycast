#pragma once

#include <string>
#include <mutex>
#include <deque>

#include "types.h"
#include "network/net_platform.h"
#include "packet.pb.h"

class TcpClient {
public:
    ~TcpClient() {
        Close();
    }

    bool Connect(const char *host, int port);

    void SetNonBlocking();

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
    static const int kBufSize = 50;

    MessageBuffer() = default;

    bool CanPush() const;

    bool PushBattleMessage(const std::string &id, u8 *body, u32 body_length);

    const proto::Packet& Packet();

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
    std::mutex mtx;
    std::map<std::string, u32> recv_seq;
};


