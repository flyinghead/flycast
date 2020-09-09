#pragma once

#include "WriteBufferInterface.h"

class PacketWriter : public ::EmbeddedProto::WriteBufferInterface {
public:
    PacketWriter(uint8_t *buf, uint32_t len);

    ~PacketWriter() override = default;

    void clear() override;

    uint32_t get_size() const override;

    uint32_t get_max_size() const override;

    uint32_t get_available_size() const override;

    bool push(uint8_t byte) override;

    bool push(const uint8_t *bytes, const uint32_t length) override;

private:
    uint8_t *buf_;
    uint32_t len_;
    uint32_t pos_;
};
