#pragma once

#include "types.h"
#include <vector>
#include <deque>

class LbsMessage {
public:
    u8 direction = 0;
    u8 category = 0;
    u16 command = 0;
    u16 body_size = 0;
    u16 seq = 0;
    u32 status = 0;
    std::vector<u8> body;
    int reading = 0;

    static const int HeaderSize = 12;
    static const u8 ServerToClient = 0x18;
    static const u8 ClientToServer = 0x81;
    static const u8 CategoryQuestion = 0x01;
    static const u8 CategoryAnswer = 0x02;
    static const u8 CategoryNotice = 0x10;
    static const u8 CategoryCustom = 0xFF;
    static const u32 StatusError = 0xFFFFFFFFu;
    static const u32 StatusSuccess = 0x00FFFFFFu;


    static const u16 lbsReadyBattle = 0x6910;
    static const u16 lbsGamePatch = 0x9960;

    int Serialize(std::deque<u8> &buf) const {
        buf.push_back(direction);
        buf.push_back(category);
        buf.push_back((command >> 8) & 0xffu);
        buf.push_back((command) & 0xffu);
        buf.push_back((body.size() >> 8) & 0xffu);
        buf.push_back((body.size()) & 0xffu);
        buf.push_back((seq >> 8) & 0xffu);
        buf.push_back((seq & 0xffu));
        buf.push_back((status >> 24) & 0xffu);
        buf.push_back((status >> 16) & 0xffu);
        buf.push_back((status >> 8) & 0xffu);
        buf.push_back((status & 0xffu));
        std::copy(std::begin(body), std::end(body), std::back_inserter(buf));
        return int(HeaderSize) + int(body.size());
    }

    int Deserialize(const std::deque<u8> &buf) {
        if (buf.size() < HeaderSize) {
            return 0;
        }

        int pkt_size = HeaderSize + (int(buf[4] << 8) | int(buf[5]));
        if (buf.size() < pkt_size) {
            return 0;
        }

        direction = buf[0];
        category = buf[1];
        command = u16(buf[2]) << 8 | u16(buf[3]);
        body_size = u16(buf[4] << 8) | u16(buf[5]);
        seq = u16(buf[6] << 8) | u16(buf[7]);
        status = u32(buf[8]) << 24 | u32(buf[9]) << 16 | u32(buf[10]) << 8 | u32(buf[11]);
        body.clear();
        for (int i = HeaderSize; i < HeaderSize + body_size; ++i) {
            body.push_back(buf[i]);
        }
        reading = 0;
        return pkt_size;
    }

    static int MessageSize(const char *buf, int size) { //TODO
        if (size < HeaderSize) {
            return 0;
        }

        int pkt_size = HeaderSize + (int(buf[4] << 8) | int(buf[5]));
        if (size < pkt_size) {
            return 0;
        }

        return pkt_size;
    }

    static LbsMessage SvAnswer(const LbsMessage &q) {
        LbsMessage msg;
        msg.direction = ServerToClient;
        msg.category = CategoryAnswer;
        msg.command = q.command;
        msg.seq = q.seq;
        msg.status = StatusSuccess;
        return msg;
    }

    static LbsMessage SvNotice(u16 cmd) {
        LbsMessage msg;
        msg.direction = ServerToClient;
        msg.category = CategoryNotice;
        msg.command = cmd;
        msg.seq = 1;
        msg.status = StatusSuccess;
        return msg;
    }

    LbsMessage *Write8(u8 v) {
        body.push_back(v);
        return this;
    }

    LbsMessage *Write16(u16 v) {
        body.push_back(u8(v >> 8));
        body.push_back(u8(v & 0xff));
        return this;
    }

    LbsMessage *Write32(u32 v) {
        body.push_back(u8((v >> 24) & 0xff));
        body.push_back(u8((v >> 16) & 0xff));
        body.push_back(u8((v >> 8) & 0xff));
        body.push_back(u8((v) & 0xff));
        return this;
    }

    LbsMessage *WriteBytes(const std::string &v) {
        u16 size = v.size();
        Write16(size);
        std::copy(std::begin(v), std::end(v), std::back_inserter(body));
        return this;
    }

    LbsMessage *WriteBytes(const char *buf, int size) {
        Write16(size);
        std::copy(buf, buf + size, std::back_inserter(body));
        return this;
    }

    LbsMessage *WriteString(const std::string &s) {
        // TODO utf8 -> sjis
        return WriteBytes(s);
    }

    u8 Read8() {
        u8 v = body[reading];
        reading++;
        return v;
    }

    u16 Read16() {
        u16 v = u16(body[reading]) << 8 | u16(body[reading]);
        reading += 2;
        return v;
    }

    u32 Read32() {
        u32 v = u32(body[reading]) << 24 | u32(body[reading]) << 16 | u32(body[reading]) << 8 | body[reading];
        reading += 4;
        return v;
    }
};

class LbsMessageReader {
public:
    void Write(const char *buf, int size) {
        std::copy(buf, buf + size, std::back_inserter(buf_));
    }

    bool Read(LbsMessage &msg) {
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