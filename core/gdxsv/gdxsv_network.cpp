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
    if (host_entry == nullptr || host_entry->h_addr_list[0] == nullptr) {
        WARN_LOG(COMMON, "Connect fail 2 gethostbyname");
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
#ifdef _WIN32
    addr.sin_addr = *((LPIN_ADDR) host_entry->h_addr_list[0]);
#else
    memcpy(&addr.sin_addr, host_entry->h_addr_list[0], host_entry->h_length);
#endif
    addr.sin_port = htons(port);
    
    fd_set setW, setE;
    struct timeval timeout = {5,0};
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
    
    if(::connect(new_sock, (const sockaddr *) &addr, sizeof(addr)) != 0){
        if (get_last_error() != EINPROGRESS && get_last_error() != L_EWOULDBLOCK) {
            WARN_LOG(COMMON, "Connect fail 2 %d", get_last_error());
            return false;
        } else {
            do {
                FD_ZERO(&setW);
                FD_SET(new_sock, &setW);
                FD_ZERO(&setE);
                FD_SET(new_sock, &setE);
                
                res = select(new_sock+1, NULL, &setW, &setE, &timeout);
                if (res < 0 && errno != EINTR) {
                    WARN_LOG(COMMON, "Connect fail 3 %d", get_last_error());
                    return false;
                } else if (res > 0) {
                    
                    if (FD_ISSET(new_sock, &setE)){
#ifdef _WIN32
                        char error;
#else
                        int error;
#endif
                        socklen_t l = sizeof(int);
                        if (getsockopt(new_sock, SOL_SOCKET, SO_ERROR, &error, &l) < 0 || error) {
                            WARN_LOG(COMMON, "Connect fail 4 %d", get_last_error());
                            return false;
                        }
                        WARN_LOG(COMMON, "Connect fail 5 %d", get_last_error());
                        return false;
                    }
                    
                    break;
                } else {
                    WARN_LOG(COMMON, "Timeout in select() - Cancelling!");
                    return false;
                }
            } while(1);
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
    NOTICE_LOG(COMMON, "TCP Connect: %s:%d ok", host, port);
    return true;

}

int TcpClient::IsConnected() const {
    return sock_ != INVALID_SOCKET;
}

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


bool UdpClient::Connect(const char *host, int port) {
    NOTICE_LOG(COMMON, "UDP Connect : %s:%d", host, port);

    sock_t new_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (new_sock == INVALID_SOCKET) {
        WARN_LOG(COMMON, "UDP Connect fail %d", get_last_error());
        return false;
    }
    auto host_entry = gethostbyname(host);
    if (host_entry == nullptr) {
        WARN_LOG(COMMON, "UDP Connect fail. gethostbyname %s", host);
        return false;
    }

    remote_addr_.sin_family = AF_INET;
    memcpy(&(remote_addr_.sin_addr.s_addr), host_entry->h_addr, host_entry->h_length);
    remote_addr_.sin_port = htons(port);

    set_recv_timeout(new_sock, 1);
    set_send_timeout(new_sock, 1);
    set_non_blocking(new_sock);
    sock_ = new_sock;
    host_ = std::string(host);
    port_ = port;
    NOTICE_LOG(COMMON, "UDP Connect : %s:%d ok", host, port);
    return true;
}

int UdpClient::IsConnected() const {
    return sock_ != INVALID_SOCKET;
}

int UdpClient::Recv(char *buf, int len) {
    int n = ::recvfrom(sock_, buf, len, 0, nullptr, nullptr);
    if (n < 0 && get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK) {
        WARN_LOG(COMMON, "UDP Recv failed. errno=%d", get_last_error());
        Close();
    }
    if (n < 0) return 0;
    return n;
}

int UdpClient::Send(const char *buf, int len) {
    int n = ::sendto(sock_, buf, len, 0, (const sockaddr *) &remote_addr_, sizeof(remote_addr_));
    if (n < 0 && get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK) {
        WARN_LOG(COMMON, "UDP Send failed. errno=%d", get_last_error());
        Close();
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

bool MessageBuffer::CanPush() const {
    return packet_.battle_data().size() < kBufSize;
}

void MessageBuffer::SessionId(const std::string &session_id) {
    NOTICE_LOG(COMMON, "set session id %s", session_id.c_str());
    packet_.set_session_id(session_id.c_str());
}

bool
MessageBuffer::PushBattleMessage(const std::string &user_id, u8 *body, u32 body_length) {
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

const proto::Packet &MessageBuffer::Packet() {
    return packet_;
}

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

void MessageFilter::Clear() {
    recv_seq.clear();
}
