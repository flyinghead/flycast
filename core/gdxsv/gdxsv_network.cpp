#include "gdxsv_network.h"

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
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
#ifdef _WIN32
    addr.sin_addr = *((LPIN_ADDR) host_entry->h_addr_list[0]);
#else
    memcpy(&addr.sin_addr, host_entry->h_addr_list[0], host_entry->h_length);
#endif
    addr.sin_port = htons(port);
    set_recv_timeout(new_sock, 5000);

    if (::connect(new_sock, (const sockaddr *) &addr, sizeof(addr)) != 0) {
        WARN_LOG(COMMON, "Connect fail 2 %d", get_last_error());
        return false;
    }

    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }

    set_tcp_nodelay(new_sock);
    set_recv_timeout(new_sock, 1);
    set_send_timeout(new_sock, 1);
    sock = new_sock;
    NOTICE_LOG(COMMON, "TCP Connect: %s:%d ok", host, port);
    return true;
}

int TcpClient::IsConnected() const {
    return sock != INVALID_SOCKET;
}

int TcpClient::Recv(char *buf, int len) {
    int n = ::recv(sock, buf, len, 0);
    if (n < 0 && get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK) {
        WARN_LOG(COMMON, "Recv failed. errno=%d", get_last_error());
        this->Close();
    }
    if (n < 0) return 0;
    return n;
}

int TcpClient::Send(const char *buf, int len) {
    int n = ::send(sock, buf, len, 0);
    if (n < 0 && get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK) {
        WARN_LOG(COMMON, "Recv failed. errno=%d", get_last_error());
        this->Close();
    }
    if (n < 0) return 0;
    return n;
}

void TcpClient::Close() {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
}

u32 TcpClient::ReadableSize() const {
    u_long n = 0;
#ifndef _WIN32
    ioctl(sock, FIONREAD, &n);
#else
    ioctlsocket(sock, FIONREAD, &n);
#endif
    return u32(n);
}

BattleBuffer::BattleBuffer() : id_(), ack_(0), begin_(1), end_(1), rbuf_(kRingSize) {
}

void BattleBuffer::SetId(std::string &id) {
    std::lock_guard<std::mutex> lock(mtx);
    id_ = id;
}

const std::string &BattleBuffer::GetId() {
    std::lock_guard<std::mutex> lock(mtx);
    return id_;
}

void BattleBuffer::PushBattleMessage(BattleMessage msg) {
    std::lock_guard<std::mutex> lock(mtx);
    auto index = end_;
    rbuf_[index % kRingSize] = msg;
    end_++;
}

void BattleBuffer::FillSendData(Packet &packet) {
    packet.clear_battle_data();

    std::lock_guard<std::mutex> lock(mtx);
    std::vector<BattleMessage *> msgs;
    u32 l = begin_ % kRingSize;
    u32 e = end_;
    if (begin_ + 50 < e) {
        e = begin_ + 50;
    }
    u32 r = e % kRingSize;
    if (l <= r) {
        for (int i = l; i < r; ++i) {
            packet.add_battle_data(rbuf_[i]);
        }
    } else {
        for (int i = l; i < kRingSize; ++i) {
            packet.add_battle_data(rbuf_[i]);
        }
        for (int i = 0; i < r; ++i) {
            packet.add_battle_data(rbuf_[i]);
        }
    }
    packet.set_seq(e - 1);
    packet.set_ack(ack_);
}

void BattleBuffer::ApplySeqAck(u32 seq, u32 ack) {
    std::lock_guard<std::mutex> lock(mtx);
    begin_ = ack + 1;
    ack_ = seq;
}

void MessageFilter::Reset() {
    std::lock_guard<std::mutex> lock(mtx);
    seq = 1;
    recv_seq.clear();
}

BattleMessage MessageFilter::GenerateMessage(const std::string &user_id, const u8 *body, u32 body_len) {
    std::lock_guard<std::mutex> lock(mtx);
    BattleMessage msg;
    msg.set_seq(seq);
    msg.mutable_user_id().set(user_id.c_str(), user_id.size());
    msg.mutable_body().set(body, body_len);
    seq++;
    return msg;
}

bool MessageFilter::Filter(const BattleMessage &msg) {
    std::lock_guard<std::mutex> lock(mtx);
    auto ack = recv_seq[msg.get_user_id()];
    if (ack == 0 || msg.get_seq() == ack + 1) {
        recv_seq[msg.get_user_id()] = msg.get_seq();
        return true;
    }
    return false;
}

bool UdpClient::Connect(const char *host, int port) {
    NOTICE_LOG(COMMON, "UDP Connect : %s:%d", host, port);

    sock_t new_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (new_sock == INVALID_SOCKET) {
        WARN_LOG(COMMON, "Connect fail 1 %d", get_last_error());
        return false;
    }
    auto host_entry = gethostbyname(host);
    if (host_entry == nullptr) {
        WARN_LOG(COMMON, "Connect fail. gethostbyname %s", host);
        return false;
    }

    remote_addr.sin_family = AF_INET;
    memcpy(&(remote_addr.sin_addr.s_addr), host_entry->h_addr, host_entry->h_length);
    remote_addr.sin_port = htons(port);

    set_recv_timeout(new_sock, 1);
    set_send_timeout(new_sock, 1);
    sock = new_sock;
    NOTICE_LOG(COMMON, "UDP Connect : %s:%d ok", host, port);
    return true;
}

int UdpClient::IsConnected() const {
    return sock != INVALID_SOCKET;
}

int UdpClient::Recv(char *buf, int len) {
    int n = ::recvfrom(sock, buf, len, 0, nullptr, nullptr);
    if (n < 0 && get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK) {
        WARN_LOG(COMMON, "recv failed. errno=%d", get_last_error());
        Close();
    }
    if (n < 0) return 0;
    return n;
}

int UdpClient::Send(const char *buf, int len) {
    int n = ::sendto(sock, buf, len, 0, (const sockaddr *) &remote_addr, sizeof(remote_addr));
    if (n < 0 && get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK) {
        WARN_LOG(COMMON, "send failed. errno=%d", get_last_error());
        Close();
    }
    if (n < 0) return 0;
    return n;
}

u32 UdpClient::ReadableSize() const {
    u_long n = 0;
#ifndef _WIN32
    ioctl(sock, FIONREAD, &n);
#else
    ioctlsocket(sock, FIONREAD, &n);
#endif
    return u32(n);
}

void UdpClient::Close() {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
}
