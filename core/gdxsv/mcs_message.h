#pragma once

#include "types.h"

#include <vector>
#include <deque>

class McsMessage {
public:
    enum class MsgType {
        ConnectionIdMsg,
        StartMsg,
        IntroMsg,
        IntroMsgReturn,
        KeyMsg,
        PingMsg,
        PongMsg,
        LoadStartMsg,
        LoadEndMsg,
        LagControlTestMsg,
        UnknownMsg,

    };

    static const char *MsgTypeName(MsgType m) {
        switch (m) {
            case MsgType::ConnectionIdMsg:
                return "ConnectionIdMsg";
            case MsgType::StartMsg:
                return "StartMsg";
            case MsgType::IntroMsg:
                return "IntroMsg";
            case MsgType::IntroMsgReturn:
                return "IntroMsgReturn";
            case MsgType::KeyMsg:
                return "KeyMsg";
            case MsgType::PingMsg:
                return "PingMsg";
            case MsgType::PongMsg:
                return "PongMsg";
            case MsgType::LoadStartMsg:
                return "LoadStartMsg";
            case MsgType::LoadEndMsg:
                return "LoadEndMsg";
            case MsgType::LagControlTestMsg:
                return "LagControlTestMsg";
            case MsgType::UnknownMsg:
                return "UnknownMsg";
            default:
                return "UnknownDefault";
        }
    }

    template<typename T>
    int Deserialize(const T &buf) {
        if (buf.size() < 4) {
            return 0;
        }

        type = MsgType::UnknownMsg;

        if (buf[0] == 0x82 && buf[1] == 0x02) {
            type = MsgType::ConnectionIdMsg;
            int n = 20;
            sender = 0;
            body.clear();
            for (int i = 0; i < n; ++i) {
                body.push_back(buf[i]);
            }
            return n;
        }

        int n = buf[0];
        if (buf.size() < n) {
            return 0;
        }

        body.clear();
        for (int i = 0; i < n; ++i) {
            body.push_back(buf[i]);
        }

        int k = (buf[1] & 0xf0) >> 4;
        int p = buf[1] & 0x0f;
        int param1 = buf[2];
        int param2 = buf[3];
        sender = p;

        if (k == 1 && param1 == 0) type = MsgType::IntroMsg;
        if (k == 1 && param1 == 1) type = MsgType::IntroMsgReturn;
        if (k == 2) type = MsgType::KeyMsg;
        if (k == 3 && param1 == 0) type = MsgType::PingMsg;
        if (k == 3 && param1 == 1) type = MsgType::PongMsg;
        if (k == 4) type = MsgType::StartMsg;
        if (k == 5 && param1 == 0) type = MsgType::LoadStartMsg;
        if (k == 5 && param1 == 1) type = MsgType::LoadEndMsg;
        if (k == 9) type = MsgType::LagControlTestMsg;

        return n;
    }

    std::string to_hex() const {
        std::string ret(body.size() * 2, ' ');
        for (int i = 0; i < body.size(); i++) {
            std::sprintf(&ret[0] + i * 2, "%02x", body[i]);
        }
        return ret;
    }

    int tail_frame() const {
        if (type == MsgType::KeyMsg && 18 <= body.size()) {
            return int(body[17]) << 8 | int(body[16]);
        }
        return 0;
    }

    int sender;
    MsgType type;
    std::vector<u8> body;
};

class McsMessageReader {
public:
    void Write(const char *buf, int size) {
        std::copy(buf, buf + size, std::back_inserter(buf_));
    }

    bool Read(McsMessage &msg) {
        int size = msg.Deserialize(buf_);
        if (size == 0) {
            return false;
        }
        for (int i = 0; i < size; ++i) {
            buf_.pop_front();
        }
        return true;
    }

    void Clear() {
        buf_.clear();
    }

private:
    std::deque<u8> buf_;
};
