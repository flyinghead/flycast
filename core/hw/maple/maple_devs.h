#pragma once
#include <memory>
#include "types.h"
#include "maple_cfg.h"
#include "maple_helper.h"
#include <cmath>
#include "input/gamepad.h"
#include "serialize.h"

enum MapleFunctionID
{
	MFID_0_Input       = 0x01000000, //DC Controller, Lightgun buttons, arcade stick .. stuff like that
	MFID_1_Storage     = 0x02000000, //VMU , VMS
	MFID_2_LCD         = 0x04000000, //VMU
	MFID_3_Clock       = 0x08000000, //VMU
	MFID_4_Mic         = 0x10000000, //DC Mic (, dreameye too ?)
	MFID_5_ARGun       = 0x20000000, //Artificial Retina gun ? seems like this one was never developed or smth -- I only remember of lightguns
	MFID_6_Keyboard    = 0x40000000, //DC Keyboard
	MFID_7_LightGun    = 0x80000000, //DC Lightgun
	MFID_8_Vibration   = 0x00010000, //Puru Puru Puur~~~
	MFID_9_Mouse       = 0x00020000, //DC Mouse
	MFID_10_StorageExt = 0x00040000, //Storage ? probably never used
	MFID_11_Camera     = 0x00080000, //DreamEye
};

enum MapleDeviceCommand
{
	MDC_DeviceRequest   = 0x01, //7 words.Note : Initialises device
	MDC_AllStatusReq    = 0x02, //7 words + device dependent ( seems to be 8 words)
	MDC_DeviceReset     = 0x03, //0 words
	MDC_DeviceKill      = 0x04, //0 words
	MDC_DeviceStatus    = 0x05, //Same as MDC_DeviceRequest ?
	MDC_DeviceAllStatus = 0x06, //Same as MDC_AllStatusReq ?

	//Various Functions
	MDCF_GetCondition   = 0x09, //FT
	MDCF_GetMediaInfo   = 0x0A, //FT,PT,3 pad
	MDCF_BlockRead      = 0x0B, //FT,PT,Phase,Block #
	MDCF_BlockWrite     = 0x0C, //FT,PT,Phase,Block #,data ...
	MDCF_GetLastError   = 0x0D, //FT,PT,Phase,Block #
	MDCF_SetCondition   = 0x0E, //FT,data ...
	MDCF_MICControl     = 0x0F, //FT,MIC data ...
	MDCF_ARGunControl   = 0x10, //FT,AR-Gun data ...

	MDC_JVSUploadFirmware = 0x80, // JVS bridge firmware
	MDC_JVSGetId		 = 0x82,
	MDC_JVSSelfTest		 = 0x84, // JVS Self Test
	MDC_JVSCommand		 = 0x86, // JVS I/O
};

enum MapleDeviceRV
{
	MDRS_JVSNone		 = 0x00, // No reply, used for multiple JVS I/O boards

	MDRS_DeviceStatus    = 0x05, //28 words
	MDRS_DeviceStatusAll = 0x06, //28 words + device dependent data
	MDRS_DeviceReply     = 0x07, //0 words
	MDRS_DataTransfer    = 0x08, //FT,depends on the command

	MDRE_UnknownFunction = 0xFE, //0 words
	MDRE_UnknownCmd      = 0xFD, //0 words
	MDRE_TransmitAgain   = 0xFC, //0 words
	MDRE_FileError       = 0xFB, //1 word, bitfield
	MDRE_LCDError        = 0xFA, //1 word, bitfield
	MDRE_ARGunError      = 0xF9, //1 word, bitfield
	MDRS_JVSSelfTestReply= 0x85, // JVS I/O SelfTest

	MDRS_JVSReply		 = 0x87, // JVS I/O
};

enum NAOMI_KEYS
{
	NAOMI_START_KEY = 1 << 15,
	NAOMI_SERVICE_KEY = 1 << 14,

	NAOMI_UP_KEY = 1 << 13,
	NAOMI_DOWN_KEY = 1 << 12,
	NAOMI_LEFT_KEY = 1 << 11,
	NAOMI_RIGHT_KEY = 1 << 10,

	NAOMI_BTN0_KEY = 1 << 9,
	NAOMI_BTN1_KEY = 1 << 8,
	NAOMI_BTN2_KEY = 1 << 7,
	NAOMI_BTN3_KEY = 1 << 6,
	NAOMI_BTN4_KEY = 1 << 5,
	NAOMI_BTN5_KEY = 1 << 4,
	NAOMI_BTN6_KEY = 1 << 3,
	NAOMI_BTN7_KEY = 1 << 2,
	NAOMI_BTN8_KEY = 1 << 16,

	NAOMI_TEST_KEY = 1 << 1,

	// Not an actual button
	NAOMI_COIN_KEY = 1 << 0,
	NAOMI_RELOAD_KEY = 1 << 17,
};

enum AWAVE_KEYS
{
	AWAVE_START_KEY = 1 << 3,

	AWAVE_BTN0_KEY  = 1 << 2,
	AWAVE_BTN1_KEY  = 1 << 1,
	AWAVE_BTN2_KEY  = 1 << 0,
	AWAVE_BTN3_KEY  = 1 << 10,
	AWAVE_BTN4_KEY  = 1 << 9,

	AWAVE_UP_KEY    = 1 << 4,
	AWAVE_DOWN_KEY  = 1 << 5,
	AWAVE_LEFT_KEY  = 1 << 6,
	AWAVE_RIGHT_KEY = 1 << 7,

	AWAVE_SERVICE_KEY = 1 << 13,
	AWAVE_TEST_KEY 	  = 1 << 14,

	// Not an actual button
	AWAVE_COIN_KEY    = 1 << 15,
	AWAVE_TRIGGER_KEY = 1 << 12,
};

struct maple_device
{
	u8 maple_port;          //raw maple port
	u8 bus_port;            //0 .. 5
	u8 bus_id;              //0 .. 3
	u8 player_num;			// for Atomiswave
	char logical_port[3];  //A0, etc
	MapleConfigMap* config;

	//fill in the info
	void Setup(u32 port, int playerNum = -1);

	virtual void OnSetup() {};
	virtual ~maple_device();

	virtual u32 RawDma(u32* buffer_in, u32 buffer_in_len, u32* buffer_out) = 0;

	virtual void serialize(Serializer& ser) const {
		ser << player_num;
	}
	virtual void deserialize(Deserializer& deser) {
		if (deser.version() >= Deserializer::V14)
			deser >> player_num;
	}

	virtual MapleDeviceType get_device_type() = 0;
	virtual bool get_lightgun_pos() { return false; }
	virtual const void *getData(size_t& size) const { size = 0; return nullptr; }
};

maple_device* maple_Create(MapleDeviceType type);

#define MAPLE_PORTS 4

template<int Magnitude>
void limit_joystick_magnitude(s8& joyx, s8& joyy)
{
	float mag = joyx * joyx + joyy * joyy;
	if (mag > (float)Magnitude * Magnitude)
	{
		mag = sqrtf(mag) / (float)Magnitude;
		joyx = (s8)lroundf(joyx / mag);
		joyy = (s8)lroundf(joyy / mag);
	}
}

extern u8 *EEPROM;

#define SWAP32(a) ((((a) & 0xff) << 24)  | (((a) & 0xff00) << 8) | (((a) >> 8) & 0xff00) | (((a) >> 24) & 0xff))

const char *GetCurrentGameButtonName(DreamcastKey key);
const char *GetCurrentGameAxisName(DreamcastKey axis);

/*
	Base class with dma helpers and stuff
*/
struct maple_base: maple_device
{
	u8* dma_buffer_out;
	u32* dma_count_out;

	u8* dma_buffer_in;
	u32 dma_count_in;

	void w8(u8 data) { *(u8*)dma_buffer_out = data; dma_buffer_out += 1; dma_count_out[0] += 1; }
	void w16(u16 data) { *(u16*)dma_buffer_out = data; dma_buffer_out += 2; dma_count_out[0] += 2; }
	void w32(u32 data) { *(u32*)dma_buffer_out = data; dma_buffer_out += 4; dma_count_out[0] += 4; }

	void wptr(const void* src, u32 len)
	{
		u8* src8 = (u8*)src;
		while (len--)
			w8(*src8++);
	}

	void wstr(const char* str, u32 len)
	{
		size_t ln = strlen(str);
		verify(len >= ln);
		len -= ln;
		while (ln--)
			w8(*str++);

		while (len--)
			w8(' ');
	}

	u8 r8() { u8  rv = *(u8*)dma_buffer_in; dma_buffer_in += 1; dma_count_in -= 1; return rv; }
	u16 r16() { u16 rv = *(u16*)dma_buffer_in; dma_buffer_in += 2; dma_count_in -= 2; return rv; }
	u32 r32() { u32 rv = *(u32*)dma_buffer_in; dma_buffer_in += 4; dma_count_in -= 4; return rv; }
	void skip(u32 len) {
		dma_buffer_in += len;
		dma_count_in -= len;
	}

	void rptr(void* dst, u32 len)
	{
		u8* dst8 = (u8*)dst;
		while (len--)
			*dst8++ = r8();
	}
	u32 r_count() { return dma_count_in; }

	u32 Dma(u32 Command, u32* buffer_in, u32 buffer_in_len, u32* buffer_out, u32& buffer_out_len)
	{
		dma_buffer_out = (u8*)buffer_out;
		dma_count_out = &buffer_out_len;

		dma_buffer_in = (u8*)buffer_in;
		dma_count_in = buffer_in_len;

		return dma(Command);
	}
	virtual u32 dma(u32 cmd) = 0;

	u32 RawDma(u32* buffer_in, u32 buffer_in_len, u32* buffer_out) override
	{
		u32 command=buffer_in[0] &0xFF;
		//Recipient address
		u32 reci = (buffer_in[0] >> 8) & 0xFF;
		//Sender address
		u32 send = (buffer_in[0] >> 16) & 0xFF;
		u32 outlen = 0;
		u32 resp = Dma(command, &buffer_in[1], buffer_in_len - 4, &buffer_out[1], outlen);

		if (reci & 0x20)
			reci |= maple_GetAttachedDevices(maple_GetBusId(reci));

		verify(u8(outlen / 4) * 4 == outlen);
		buffer_out[0] = (resp << 0 ) | (send << 8) | (reci << 16) | ((outlen / 4) << 24);

		return outlen + 4;
	}
};

class jvs_io_board;

struct maple_naomi_jamma : maple_base
{
	static constexpr u8 ALL_NODES = 0xff;

	std::vector<std::unique_ptr<jvs_io_board>> io_boards;
	bool crazy_mode = false;

	u8 jvs_repeat_request[32][256];
	u8 jvs_receive_buffer[32][258];
	u32 jvs_receive_length[32] = { 0 };
	u8 eeprom[128];

	maple_naomi_jamma();
	~maple_naomi_jamma();

	MapleDeviceType get_device_type() override
	{
		return MDT_NaomiJamma;
	}

	u8 sense_line(u32 node_id)
	{
		bool last_node = node_id == io_boards.size();
		return last_node ? 0x8E : 0x8F;
	}

	void send_jvs_message(u32 node_id, u32 channel, u32 length, u8 *data);
	void send_jvs_messages(u32 node_id, u32 channel, bool use_repeat, u32 length, u8 *data, bool repeat_first);
	bool receive_jvs_messages(u32 channel);

	void handle_86_subcommand();

	u32 RawDma(u32* buffer_in, u32 buffer_in_len, u32* buffer_out) override;
	u32 dma(u32 cmd) override { return 0; }

	void serialize(Serializer& ser) const override;
	void deserialize(Deserializer& deser) override;
};
