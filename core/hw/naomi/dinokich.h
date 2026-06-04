/*
	Copyright 2026 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
//
// Card reader emulation for dinokich and loveber3cn
//
#pragma once
#include "systemsp.h"
#include <array>
#include <deque>
#include <vector>

namespace systemsp
{

class DinokichCardReader : public SerialPort::Pipe
{
public:
	DinokichCardReader(SerialPort *port, int index, const std::string& gameName);
	void write(u8 v) override;
	int available() override {
		return tx_buf_.size();
	}
	u8 read() override;
	void serialize(Serializer& ser) const override;
	void deserialize(Deserializer& deser) override;

private:
    void try_parse_frames();
    void handle_request(const std::vector<u8>& frame);
    void send_response(u8 status, const std::vector<u8>& payload);
    void saveCard(int slot);

    SerialPort *port;
	const int index;
	const std::string gameName;

    /* Per-command handlers. They return the response payload (bytes that
     * will appear at offsets 4..3+N of the response frame) plus the status
     * byte that goes at offset 1.                                        */
    struct Reply { u8 status = 0; std::vector<u8> payload; };
    Reply on_B0(const std::vector<u8>& f);
    Reply on_C0(const std::vector<u8>& f);
    Reply on_B9(const std::vector<u8>& f);
    Reply on_67(const std::vector<u8>& f);
    Reply on_B2(const std::vector<u8>& f);
    Reply on_B3(const std::vector<u8>& f);
    Reply on_61(const std::vector<u8>& f);
    Reply on_64(const std::vector<u8>& f);
    Reply on_60(const std::vector<u8>& f);
    Reply on_63(const std::vector<u8>& f);
    Reply on_6D(const std::vector<u8>& f);

    struct Slot {
        bool present = false;
        bool powered = false;
        std::array<u8, 1024> eeprom{};
        std::array<u8, 2>    last_msg64{};  // last "auth" value the Naomi
                                                  // sent us; emulator just stores it
    };
    Slot& slot_ref(int slot) { return slots_[slot - 1]; }
    const Slot& slot_ref(int slot) const { return slots_[slot - 1]; }

    std::array<Slot, 2> slots_;
    std::vector<u8> rx_buf_;
    std::deque<u8> tx_buf_;
};

} // namespace systemsp
