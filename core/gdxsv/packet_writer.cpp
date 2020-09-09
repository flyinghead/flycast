#include "packet_writer.h"
#include <cstring>

PacketWriter::PacketWriter(uint8_t *buf, uint32_t len) : buf_(buf), len_(len), pos_(0) {
}

void PacketWriter::clear() {
    pos_ = 0;
}

uint32_t PacketWriter::get_size() const {
    return pos_;
}

uint32_t PacketWriter::get_max_size() const {
    return len_;
}

uint32_t PacketWriter::get_available_size() const {
    return len_ - pos_;
}

bool PacketWriter::push(const uint8_t byte) {
    if (len_ <= pos_) return false;
    buf_[pos_++] = byte;
    return true;
}

bool PacketWriter::push(const uint8_t *bytes, const uint32_t length) {
    if (len_ <= pos_ + length) return false;
    std::memcpy(buf_ + pos_, bytes, length);
    pos_ += length;
    return true;
}


