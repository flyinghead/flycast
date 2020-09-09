#pragma once

#include "ReadBufferInterface.h"

class PacketReader : public ::EmbeddedProto::ReadBufferInterface {
public:
    PacketReader(const uint8_t *buf, uint32_t len);

    ~PacketReader() override = default;

    uint32_t get_size() const override;

    uint32_t get_max_size() const override;

    bool peek(uint8_t &byte) const override;

    void advance() override;

    void advance(const uint32_t N) override;

    bool pop(uint8_t &byte) override;

private:
    const uint8_t *buf_;
    uint32_t len_;
    uint32_t pos_;
};

