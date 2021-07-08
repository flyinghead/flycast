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

    if (::connect(new_sock, (const sockaddr *) &addr, sizeof(addr)) != 0) {
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
                    if (getsockopt(new_sock, SOL_SOCKET, SO_ERROR, (char *) &error, &l) < 0 || error) {
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
    msg_buf_.Clear();
}

bool UdpRemote::is_open() const {
    return is_open_;
}

const std::string &UdpRemote::str_addr() const {
    return str_addr_;
}

const sockaddr_in &UdpRemote::net_addr() const {
    return net_addr_;
}

MessageBuffer &UdpRemote::msg_buf() {
    return msg_buf_;
}

bool UdpClient::Bind(int port) {
    sock_t new_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (new_sock == INVALID_SOCKET) {
        WARN_LOG(COMMON, "UDP Connect fail %d", get_last_error());
        return false;
    }

    sockaddr_in recv_addr;
    memset(&recv_addr, 0, sizeof(recv_addr));
    recv_addr.sin_port = htons(port); // if port == 0 then the system assign an available port.
    recv_addr.sin_family = AF_INET;
    recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (::bind(new_sock, (struct sockaddr *) &recv_addr, sizeof(recv_addr)) < 0) {
        ERROR_LOG(COMMON, "gdxsv: bind() failed. errno=%d", get_last_error());
        closesocket(new_sock);
        return false;
    }

    memset(&recv_addr, 0, sizeof(recv_addr));
    int addr_len = sizeof(recv_addr);
    if (::getsockname(new_sock, (struct sockaddr *) &recv_addr, &addr_len) < 0) {
        ERROR_LOG(COMMON, "gdxsv: getsockname() failed. errno=%d", get_last_error());
        closesocket(new_sock);
        return false;
    }

    set_recv_timeout(new_sock, 1);
    set_send_timeout(new_sock, 1);
    set_non_blocking(new_sock);

    sock_ = new_sock;
    bind_ip_ = std::string(::inet_ntoa(recv_addr.sin_addr));
    bind_port_ = ::ntohs(recv_addr.sin_port);
    NOTICE_LOG(COMMON, "UDP Initialize ok: %s:%d", bind_ip_.c_str(), bind_port_);
    return true;
}

bool UdpClient::Initialized() const {
    return sock_ != INVALID_SOCKET;
}

int UdpClient::RecvFrom(char *buf, int len, std::string &sender) {
    sender.clear();
    sockaddr_in from_addr;
    socklen_t addrlen = sizeof(from_addr);
    memset(&from_addr, 0, sizeof(from_addr));

    int n = ::recvfrom(sock_, buf, len, 0, (struct sockaddr *) &from_addr, &addrlen);
    if (n < 0 && get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK) {
        WARN_LOG(COMMON, "UDP Recv failed. errno=%d", get_last_error());
        Close();
    }
    sender = std::string(::inet_ntoa(from_addr.sin_addr)) + ":" + std::to_string(::ntohs(from_addr.sin_port));
    if (n < 0) return 0;
    return n;
}

int UdpClient::SendTo(const char *buf, int len, const UdpRemote &remote) {
    int n = ::sendto(sock_, buf, len, 0, (const sockaddr *) &remote.net_addr(), sizeof(remote.net_addr()));
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
