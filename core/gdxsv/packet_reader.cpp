#include "packet_reader.h"

PacketReader::PacketReader(const uint8_t *buf, uint32_t len) : buf_(buf), len_(len), pos_(0) {
}

uint32_t PacketReader::get_size() const {
    return len_ - pos_;
}

uint32_t PacketReader::get_max_size() const {
    return len_;
}

bool PacketReader::peek(uint8_t &byte) const {
    if (len_ <= pos_) return false;
    byte = buf_[pos_];
    return true;
}

void PacketReader::advance() {
    if (len_ <= pos_) return;
    pos_++;
}

void PacketReader::advance(const uint32_t N) {
    pos_ += N;
    if (len_ < pos_) {
        pos_ = len_;
    }
}

bool PacketReader::pop(uint8_t &byte) {
    if (len_ <= pos_) return false;
    byte = buf_[pos_++];
    return true;
}
