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
#include "dinokich.h"
#include "dinokich_card.h"
#include "cfg/option.h"
#include "oslib/oslib.h"
#include <algorithm>

namespace systemsp
{

namespace {

constexpr u8 REQ_SOH  = 0xAA;
constexpr u8 RESP_SOH = 0x55;
constexpr int EEPROM_SIZE   = 1024;
constexpr int CARD_OFFSET_20 = 0x20;
constexpr int CARD_OFFSET_30 = 0x30;
constexpr int CARD_OFFSET_40 = 0x40;
constexpr int CARD_OFFSET_80 = 0x80;
constexpr int CARD_OFFSET_3FE = 0x3FE;

u8 xor_checksum(const u8* data, std::size_t len) {
    u8 s = 0;
    for (std::size_t i = 0; i < len; ++i) s ^= data[i];
    return s;
}

}  // namespace

static std::array<u8, 1024> dinokich_make_card_image(bool loveBerry)
{
    /* Mirror FUN_8c16df3c: build a 56-byte plaintext consisting of the
     * 51-byte header + 5 random pad bytes, then take the last 2 of those
     * pad bytes as the "card-check" value bufout. */
    dinokich_plain_t plain{};
    memcpy(plain.serial_prefix, "DINO", sizeof(plain.serial_prefix));
    plain.games_played = 0;
    plain.prev_credits = 0;
   	memcpy(plain.tag, "CHINA-CARD!", 11);
    plain.credits = 999;	// maximum allowed
    for (auto& b : plain.pad)    b = rand();
    for (auto& b : plain.bufout) b = rand();

    std::array<u8, 8> key;
    for (auto& b : key)
    	b = rand();
    std::array<u8, 1024> eeprom{};

    /* DES-encrypt then odd-digit-interleave into a 64-byte card block. */
    u8 card64[64];
    dinokich_encode_card(&plain, key.data(), card64);

    /* Place at offset 0x40 on the 1024-byte EEPROM image. */
    std::copy(std::begin(card64), std::end(card64),
              eeprom.begin() + CARD_OFFSET_40);

    /* The 2-byte check value also lives at the very end of the EEPROM,
     * at the address msg63 (3FE) writes to. */
    eeprom[CARD_OFFSET_3FE]     = plain.bufout[0];
    eeprom[CARD_OFFSET_3FE + 1] = plain.bufout[1];

    /* Plaintext identity block at offset 0x20: the game reads this separately
     * (FUN_8c169f28) and requires "BKACHN" at [0..5].
     * Without it the card is rejected with -104. */
    if (loveBerry)
    	memcpy(&eeprom[CARD_OFFSET_20], "BLXCHN", 6);
    else
    	memcpy(&eeprom[CARD_OFFSET_20], "BKACHN", 6);
    // Generate a unique ID
    u32 cardId = ((rand() & 0xffff) << 16) | (rand() & 0xffff);
    memcpy(&eeprom[CARD_OFFSET_20 + 9], &cardId, sizeof(cardId));

    /* Optional extended verification: byte at 0x30 == 0x12 and the byte at
     * 0x80 is the low-byte of the int-sum of card64. */
    eeprom[CARD_OFFSET_30] = 0x12;
    int sum = dinokich_sum_bytes(card64, 64);
    eeprom[CARD_OFFSET_80] = static_cast<u8>(sum & 0xFF);

    return eeprom;
}

static std::string getCardDataPath(int index)
{
	std::string path = hostfs::getArcadeFlashPath();
	switch (config::Region)
	{
	case 0:
		path += "-jp";
		break;
	case 1:
		path += "-us";
		break;
	default:
		path += "-exp";
		break;
	}
	path += ".rfid" + std::to_string(index);

	return path;
}

DinokichCardReader::DinokichCardReader(SerialPort *port, int index, const std::string& gameName)
	: port(port), index(index), gameName(gameName)
{
	port->setPipe(this);

	for (Slot& slot : slots_)
	{
		FILE *f = nowide::fopen(getCardDataPath(index).c_str(), "rb");
		if (f != nullptr)
		{
			if (fread(slot.eeprom.data(), 1, slot.eeprom.size(), f) != slot.eeprom.size())
				WARN_LOG(NAOMI, "Rfid card %d: truncated read", index);
			fclose(f);
			// TODO Decrypt card and check game credits. Renew if zero.
		}
		else
		{
			slot.eeprom = dinokich_make_card_image(gameName.substr(0, 4) == "love");
			saveCard(index - this->index);
		}
		// insert card
		slot.present = true;
		slot.powered = false;
		index++;
	}
}

void DinokichCardReader::write(u8 v)
{
	rx_buf_.push_back(v);
	try_parse_frames();
	port->updateStatus();
}

u8 DinokichCardReader::read()
{
	u8 v = 0;
	if (!tx_buf_.empty()) {
		v = tx_buf_.front();
		tx_buf_.pop_front();
	}
	if (tx_buf_.empty())
		port->updateStatus();
	return v;
}

void DinokichCardReader::try_parse_frames()
{
    while (true) {
        /* Drop bytes until we find a SOH. */
        while (!rx_buf_.empty() && rx_buf_.front() != REQ_SOH)
            rx_buf_.erase(rx_buf_.begin());
        if (rx_buf_.size() < 5) return;  /* need at least header + checksum */

        std::size_t N = rx_buf_[3];
        std::size_t frame_size = N + 5;
        if (rx_buf_.size() < frame_size) return;  /* wait for more bytes */

        std::vector<u8> frame(rx_buf_.begin(), rx_buf_.begin() + frame_size);
        rx_buf_.erase(rx_buf_.begin(), rx_buf_.begin() + frame_size);

        if (xor_checksum(frame.data(), frame.size()) != 0) {
            WARN_LOG(NAOMI, "DinokichCardReader: bad checksum, dropping frame");
            continue;
        }
        handle_request(frame);
    }
}

void DinokichCardReader::handle_request(const std::vector<u8>& f)
{
    Reply r;
    switch (f[1]) {
    	case 0xB0: r = on_B0(f); break;
    	case 0xC0: r = on_C0(f); break;
        case 0xB9: r = on_B9(f); break;
        case 0x67: r = on_67(f); break;
        case 0xB2: r = on_B2(f); break;
        case 0xB3: r = on_B3(f); break;
        case 0x61: r = on_61(f); break;
        case 0x64: r = on_64(f); break;
        case 0x60: r = on_60(f); break;
        case 0x63: r = on_63(f); break;
        case 0x6D: r = on_6D(f); break;
        default:
        	WARN_LOG(NAOMI, "DinokichCardReader: %02X NOT HANDLED", f[1]);
            r.status = 0x80;  /* "unknown command" — game maps -0x80 -> -8 */
            break;
    }
    send_response(r.status, r.payload);
}

DinokichCardReader::Reply DinokichCardReader::on_B0(const std::vector<u8>& f) {
	DEBUG_LOG(NAOMI, "DinokichCardReader: B0 get firmware version");
    /* Firmware version query.
     *   Request : [AA B0 00 07 80 00 00 00 00 00 08 XOR]  (12 bytes)
     *   Response: status 0, 8 ASCII bytes of "version info" + XOR (13 bytes).
     *
     * The Naomi side (getCardReaderFwVersion @ 8c169d36) reads:
     *   *major = payload[5] - '0'
     *   *minor = payload[7] - '0'
     * Every other byte is ignored; loopRfidCards never inspects the values,
     * it only requires the call to succeed. We report version "1.0". */
    if (f.size() != 12 || f[10] != 0x08) return { 0x80, {} };
    return { 0x00, { 'D','K','_','C',' ','1','.','0' } };
}

DinokichCardReader::Reply DinokichCardReader::on_C0(const std::vector<u8>& /*f*/) {
	/* Per-slot status report from the game (sendRfidMsgC0; payload bytes
	 * [9],[10] are a slot index and the result code). The reader just stores
	 * it (e.g. drives an LED); loopRfidCards discards the return value. We
	 * ACK with status 0 so the exchange completes. */
	return { 0x00, {} };
}

void DinokichCardReader::send_response(u8 status, const std::vector<u8>& payload) {
    std::vector<u8> frame;
    frame.reserve(payload.size() + 5);
    frame.push_back(RESP_SOH);
    frame.push_back(status);
    frame.push_back(0x00);
    frame.push_back(static_cast<u8>(payload.size()));
    frame.insert(frame.end(), payload.begin(), payload.end());
    frame.push_back(xor_checksum(frame.data(), frame.size()));

    tx_buf_.insert(tx_buf_.end(), frame.begin(), frame.end());
}

DinokichCardReader::Reply DinokichCardReader::on_B9(const std::vector<u8>& /*f*/) {
	DEBUG_LOG(NAOMI, "DinokichCardReader: B9 card presence");
    /* "Card presence ping". The game places the bitmap in response[1] (the
     * status byte slot). bit0 = slot2 present, bit1 = slot1 present. */
    u8 presence = 0;
    if (slots_[0].present) presence |= 0x02;
    if (slots_[1].present) presence |= 0x01;
    return { presence, {} };
}

DinokichCardReader::Reply DinokichCardReader::on_67(const std::vector<u8>& f) {
	DEBUG_LOG(NAOMI, "DinokichCardReader: 67 activation handshake");
    /* Activation handshake. The game looks at response[4] after un-MD5
     * with the salt it sent and requires it to be > 0. We reply with
     * 0x01 ^ md5(salt)[0]. */
    int slot = f[4];
    if (slot != 1 && slot != 2) return { 0x80, {} };
    if (!slot_ref(slot).present) return { 0x10, {} };

    u8 salt[2] = { f[9], f[10] };
    u8 ok = 0x01;
    u8 scrambled;
    dinokich_md5_scramble(salt, &ok, 1, &scrambled);
    return { 0x00, { scrambled } };
}

DinokichCardReader::Reply DinokichCardReader::on_B2(const std::vector<u8>& f) {
	DEBUG_LOG(NAOMI, "DinokichCardReader: B2 power on card");
    int slot = f[4];
    if (slot != 1 && slot != 2) return { 0x80, {} };
    if (!slot_ref(slot).present) return { 0x10, {} };
    slot_ref(slot).powered = true;
    return { 0x00, {} };
}

DinokichCardReader::Reply DinokichCardReader::on_B3(const std::vector<u8>& f) {
	DEBUG_LOG(NAOMI, "DinokichCardReader: B3 power off card");
    int slot = f[4];
    if (slot != 1 && slot != 2) return { 0x80, {} };
    slot_ref(slot).powered = false;
    return { 0x00, {} };
}

DinokichCardReader::Reply DinokichCardReader::on_61(const std::vector<u8>& f) {
    /* Read N bytes at addr from card. Request layout (offsets inside f):
     *   4 slot   7 addr_hi   8 addr_lo   9 salt_lo   10 salt_hi
     *   11 0    12 N (read size)                                       */
    int slot = f[4];
    if (slot != 1 && slot != 2)         return { 0x80, {} };
    if (!slot_ref(slot).present)        return { 0x10, {} };
    /* NOTE: do NOT gate raw EEPROM reads on the B2/B3 "powered" state. The
     * game reads offset 0x20 (FUN_8c169f28) immediately after initSmartCardReader
     * has powered the card OFF with B3, without re-issuing B2. The real reader
     * allows EEPROM access regardless of activation, so we must too. */

    int addr = (f[7] << 8) | f[8];
    u8 salt[2] = { f[9], f[10] };
    int N = f[12];
    if (addr < 0 || N <= 0 || addr + N > EEPROM_SIZE) return { 0x70, {} };

	DEBUG_LOG(NAOMI, "DinokichCardReader: 61 read bytes: salt %04x addr %x len %x", *(u16 *)salt, addr, N);
    std::vector<u8> payload(N);
    const auto& eeprom = slot_ref(slot).eeprom;
    dinokich_md5_scramble(salt, eeprom.data() + addr, N, payload.data());
    return { 0x00, payload };
}

DinokichCardReader::Reply DinokichCardReader::on_60(const std::vector<u8>& f) {
    /* Block write (sendRfidMsg60). Unlike the read frame (0x61), the write
     * frame carries the payload DIRECTLY at offset 11 (no 00/len pair), and
     * the chunk size is encoded in the length field f[3]:
     *   [3]=chunksize+7  [7..8]=addr  [9..10]=salt  [11..]=payload  [last]=xor
     * (total frame = chunksize + 12 = f[3] + 5, already validated by framing). */
    int slot = f[4];
    if (slot != 1 && slot != 2)  return { 0x80, {} };
    if (!slot_ref(slot).present) return { 0x10, {} };
    /* EEPROM writes are not gated on the B2/B3 powered state. */

    int addr = (f[7] << 8) | f[8];
    u8 salt[2] = { f[9], f[10] };
    int N = static_cast<int>(f[3]) - 7;                /* chunk size */
    if (N <= 0 || addr < 0 || addr + N > EEPROM_SIZE)  return { 0x70, {} };
	DEBUG_LOG(NAOMI, "DinokichCardReader: 60 write bytes: salt %04x addr %x len %x payload sz %zx", *(u16 *)salt, addr, N, f.size());
    if (static_cast<int>(f.size()) < 11 + N + 1)       return { 0x70, {} };

    std::vector<u8> data(N);
    dinokich_md5_scramble(salt, &f[11], N, data.data());
    auto& eeprom = slot_ref(slot).eeprom;
    std::copy_n(data.begin(), N, eeprom.begin() + addr);
    saveCard(slot - 1);
    return { 0x00, {} };
}

DinokichCardReader::Reply DinokichCardReader::on_6D(const std::vector<u8>& f) {
	DEBUG_LOG(NAOMI, "DinokichCardReader: 6D");
    /* Same wire shape as 0x60 (chunked block write). The game only ever
     * sends 6D for slot 1, writing 16 bytes at offset 0x20.            */
    return on_60(f);
}

DinokichCardReader::Reply DinokichCardReader::on_64(const std::vector<u8>& f) {
    /* The Naomi proves to the reader that it knows the 2-byte "card check"
     * value by sending it back (scrambled). The reader is supposed to
     * gate further operations on a match; we simply accept anything and
     * store the value for inspection. The wire layout is:
     *   4 slot   9 salt_lo   10 salt_hi   11..12 data (scrambled)     */
    int slot = f[4];
    if (slot != 1 && slot != 2)  return { 0x80, {} };
    if (!slot_ref(slot).present) return { 0x10, {} };

    u8 salt[2] = { f[9], f[10] };
    u8 plain[2];
    dinokich_md5_scramble(salt, &f[11], 2, plain);
	DEBUG_LOG(NAOMI, "DinokichCardReader: 64 reader check -> %x %x", plain[0], plain[1]);
    slot_ref(slot).last_msg64 = { plain[0], plain[1] };
    return { 0x00, {} };
}

DinokichCardReader::Reply DinokichCardReader::on_63(const std::vector<u8>& f) {
	DEBUG_LOG(NAOMI, "DinokichCardReader: 63 write bytes");
    /* Write 2 bytes at fixed offset 0x03FE. Layout matches 0x64 except
     * the address is implied (f[7..8] = 0x03 0xFE).                    */
    int slot = f[4];
    if (slot != 1 && slot != 2)  return { 0x80, {} };
    if (!slot_ref(slot).present) return { 0x10, {} };

    u8 salt[2] = { f[9], f[10] };
    u8 plain[2];
    dinokich_md5_scramble(salt, &f[11], 2, plain);
    auto& eeprom = slot_ref(slot).eeprom;
    eeprom[CARD_OFFSET_3FE]     = plain[0];
    eeprom[CARD_OFFSET_3FE + 1] = plain[1];
    return { 0x00, {} };
}

void DinokichCardReader::saveCard(int slotId)
{
    Slot& slot = slots_[slotId];
	FILE *f = nowide::fopen(getCardDataPath(index + slotId).c_str(), "wb");
	if (f != nullptr)
	{
		if (fwrite(slot.eeprom.data(), 1, slot.eeprom.size(), f) != slot.eeprom.size())
			WARN_LOG(NAOMI, "Rfid card %d: truncated write", index);
		fclose(f);
	}
	else {
		WARN_LOG(NAOMI, "Card saved failed to %s", getCardDataPath(index).c_str());
	}
}

void DinokichCardReader::serialize(Serializer& ser) const
{
	for (const Slot& slot : slots_)
	{
		ser << slot.present;
		ser << slot.powered;
		ser << slot.eeprom;
	}
	ser << (u32)rx_buf_.size();
	ser.serialize(rx_buf_.data(), rx_buf_.size());
	ser << (u32)tx_buf_.size();
	for (u8 b : tx_buf_)
		ser << b;
}

void DinokichCardReader::deserialize(Deserializer& deser)
{
	if (deser.version() >= Deserializer::V59)
	{
		for (Slot& slot : slots_)
		{
			deser >> slot.present;
			deser >> slot.powered;
			deser >> slot.eeprom;
		}
		u32 size;
		deser >> size;
		rx_buf_.resize(size);
		deser.deserialize(rx_buf_.data(), size);
		deser >> size;
		for (u32 i = 0; i < size; i++)
		{
			u8 b;
			deser >> b;
			tx_buf_.push_back(b);
		}
	}
	else
	{
		// Need to skip 2 RfidReaderWriter instances in previous savestate versions
		for (int i = 0; i < 2; i++)
		{
			u32 size;
			deser >> size;
			deser.skip(size);
			deser.skip<u8>(); // expectedBytes
			deser >> size;
			deser.skip(size);
			if (deser.version() >= Deserializer::V41)
			{
				deser.skip<int>(); // state
				deser.skip<int>(); // rowCounter
				deser.skip(128); // cardData
			}
		}
	}
}

} // namespace systemsp

