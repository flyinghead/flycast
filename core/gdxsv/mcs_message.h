#pragma once

#include "types.h"

#include <vector>
#include <deque>

class McsMessage {
public:
    enum MsgType {
        ConnectionIdMsg,
        IntroMsg,
        IntroMsgReturn,
        PingMsg,
        PongMsg,
        StartMsg,
        ForceMsg,
        KeyMsg,
        LoadStartMsg,
        LoadEndMsg,
        LagControlTestMsg,
        UnknownMsg,
    };

    static const char *MsgTypeName(MsgType m) {
        if (m == ConnectionIdMsg) return "ConnectionIdMsg";
        if (m == StartMsg) return "StartMsg";
        if (m == IntroMsg) return "IntroMsg";
        if (m == IntroMsgReturn) return "IntroMsgReturn";
        if (m == KeyMsg) return "KeyMsg";
        if (m == PingMsg) return "PingMsg";
        if (m == PongMsg) return "PongMsg";
        if (m == LoadStartMsg) return "LoadStartMsg";
        if (m == LoadEndMsg) return "LoadEndMsg";
        if (m == LagControlTestMsg) return "LagControlTestMsg";
        if (m == ForceMsg) return "ForceMsg";
        if (m == UnknownMsg) return "UnknownMsg";
        return "UnknownDefault";
    }

    MsgType Type() const {
        if (body.size() < 4) return UnknownMsg;
        if (body[0] == 0x82 && body[1] == 0x02) return ConnectionIdMsg;

        const int k = (body[1] & 0xf0) >> 4;
        const int p = body[2];

        if (k == 1 && p == 0) return IntroMsg;
        if (k == 1 && p == 1) return IntroMsgReturn;
        if (k == 2) return KeyMsg;
        if (k == 3 && p == 0) return PingMsg;
        if (k == 3 && p == 1) return PongMsg;
        if (k == 4) return StartMsg;
        if (k == 5 && p == 0) return LoadStartMsg;
        if (k == 5 && p == 1) return LoadEndMsg;
        if (k == 7) return ForceMsg;
        if (k == 9) return LagControlTestMsg;
        return UnknownMsg;
    }

    template<typename T>
    int Deserialize(const T &buf) {
        if (buf.size() < 4) {
            return 0;
        }

        if (buf[0] == 0x82 && buf[1] == 0x02) {
            const int n = 20;
            body.clear();
            std::copy_n(buf.begin(), n, std::back_inserter(body));
            return n;
        }

        const int n = buf[0];
        if (buf.size() < n) {
            return 0;
        }

        body.clear();
        std::copy_n(buf.begin(), n, std::back_inserter(body));
        return n;
    }

    static McsMessage Create(MsgType type, u8 p) {
        McsMessage msg;
        if (type == IntroMsg) {
            msg.body.assign({0x04, 0x10, 0x00, 0x00});
        } else if (type == IntroMsgReturn) {
            msg.body.assign({0x04, 0x10, 0x01, 0x00});
        } else if (type == StartMsg) {
            msg.body.assign({0x04, 0x40, 0x00, 0x00});
        } else if (type == KeyMsg) {
            msg.body.assign(
                    {0x12, 0x20,
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
        } else if (type == PingMsg) {
            // TODO unknown detail
            msg.body.assign(
                    {0x14, 0x30,
                     0x00, 0x00,
                     0x04, 0x12, 0x01, 0x00,
                     0x04, 0x12, 0x01, 0x00,
                     0x00, 0x36, 0x36, 0x38, 0x39, 0x31, 0x32, 0x32});
        } else if (type == PongMsg) {
            // TODO unknown detail
            msg.body.assign({0x06, 0x30, 0x01, 0x00, 0x02, 0x00});
        } else if (type == LoadStartMsg) {
            msg.body.assign({0x04, 0x50, 0x00, 0x00});
        } else if (type == LoadEndMsg) {
            msg.body.assign({0x04, 0x50, 0x01, 0x00});
        } else if (type == LagControlTestMsg) {
            msg.body.assign({0x04, 0x90, 0x00, 0x00});
        } else if (type == ForceMsg) {
            msg.body.assign({0x04, 0x70, 0x00, 0x00});
        } else if (type == ConnectionIdMsg) {
            verify(false);
        } else if (type == UnknownMsg) {
            verify(false);
        }

        if (2 <= msg.body.size()) {
            msg.body[1] |= p;
        }
        return msg;
    }

    std::string ToHex() const {
        std::string ret(body.size() * 2, ' ');
        for (int i = 0; i < body.size(); i++) {
            std::sprintf(&ret[0] + i * 2, "%02x", body[i]);
        }
        return ret;
    }

    McsMessage *SetPongTo(int id) {
        verify(Type() == MsgType::PongMsg);
        body[4] = id;
        return this;
    }

    int PingCount() const {
        verify(Type() == MsgType::PingMsg);
        return body[4];
    }

    void PongCount(int n) {
        verify(Type() == MsgType::PongMsg);
        body[3] = n;
    }

    int FirstFrame() const {
        verify(Type() == MsgType::KeyMsg);
        return int(body[9]) << 8 | int(body[8]);
    }

    void FirstFrame(u16 frame) {
        verify(Type() == MsgType::KeyMsg);
        body[8] = u8((frame >> 8) & 0xff);
        body[9] = u8(frame & 0xff);
    }

    int SecondFrame() const {
        verify(Type() == MsgType::KeyMsg);
        return int(body[17]) << 8 | int(body[16]);
    }

    void SecondFrame(u16 frame) {
        verify(Type() == MsgType::KeyMsg);
        body[16] = u8((frame >> 8) & 0xff);
        body[17] = u8(frame & 0xff);
    }

    u16 FirstKey() const {
        verify(Type() == MsgType::KeyMsg);
        return u16(body[2]) << 8 | u16(body[3]);
    }

    void FirstKey(u16 code) {
        verify(Type() == MsgType::KeyMsg);
        body[2] = u8((code >> 8) & 0xff);
        body[3] = u8(code & 0xff);
    }

    u16 SecondKey() const {
        verify(Type() == MsgType::KeyMsg);
        return u16(body[10]) << 8 | u16(body[11]);
    }

    void SecondKey(u16 code) {
        verify(Type() == MsgType::KeyMsg);
        body[10] = u8((code >> 8) & 0xff);
        body[11] = u8(code & 0xff);
    }

    int Sender() const {
        return body[1] & 0x0f;
    }

    void Sender(int p) {
        body[1] = (body[1] & 0xf0) | p;
    }

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