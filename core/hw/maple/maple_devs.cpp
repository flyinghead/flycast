#include "maple_devs.h"
#include "maple_cfg.h"
#include "maple_helper.h"
#include "cfg/cfg.h"
#include "hw/naomi/naomi_cart.h"
#include "hw/pvr/spg.h"
#include "input/gamepad_device.h"
#include "stdclass.h"
#include "oslib/audiostream.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <zlib.h>
#include <xxhash.h>

#define LOGJVS(...) DEBUG_LOG(JVS, __VA_ARGS__)

const char* maple_sega_controller_name = "Dreamcast Controller";
const char* maple_sega_vmu_name = "Visual Memory";
const char* maple_sega_kbd_name = "Emulated Dreamcast Keyboard";
const char* maple_sega_mouse_name = "Emulated Dreamcast Mouse";
const char* maple_sega_dreameye_name_1 = "Dreamcast Camera Flash  Devic";
const char* maple_sega_dreameye_name_2 = "Dreamcast Camera Flash LDevic";
const char* maple_sega_mic_name = "MicDevice for Dreameye";
const char* maple_sega_purupuru_name = "Puru Puru Pack";
const char* maple_sega_lightgun_name = "Dreamcast Gun";
const char* maple_sega_twinstick_name = "Twin Stick";
const char* maple_ascii_stick_name = "ASCII STICK";

const char* maple_sega_brand = "Produced By or Under License From SEGA ENTERPRISES,LTD.";

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

//fill in the info
void maple_device::Setup(u32 prt)
{
	maple_port = prt;
	bus_port = maple_GetPort(prt);
	bus_id = maple_GetBusId(prt);
	logical_port[0] = 'A' + bus_id;
	logical_port[1] = bus_port == 5 ? 'x' : '1' + bus_port;
	logical_port[2] = 0;
}
maple_device::~maple_device()
{
	if (config)
		delete config;
}

/*
	Base class with dma helpers and stuff
*/
struct maple_base: maple_device
{
	u8* dma_buffer_out;
	u32* dma_count_out;

	u8* dma_buffer_in;
	u32 dma_count_in;

	void w8(u8 data) { *((u8*)dma_buffer_out)=data;dma_buffer_out+=1;dma_count_out[0]+=1; }
	void w16(u16 data) { *((u16*)dma_buffer_out)=data;dma_buffer_out+=2;dma_count_out[0]+=2; }
	void w32(u32 data) { *(u32*)dma_buffer_out=data;dma_buffer_out+=4;dma_count_out[0]+=4; }

	void wptr(const void* src,u32 len)
	{
		u8* src8=(u8*)src;
		while(len--)
			w8(*src8++);
	}
	void wstr(const char* str,u32 len)
	{
		size_t ln=strlen(str);
		verify(len>=ln);
		len-=ln;
		while(ln--)
			w8(*str++);

		while(len--)
			w8(0x20);
	}

	u8 r8()	  { u8  rv=*((u8*)dma_buffer_in);dma_buffer_in+=1;dma_count_in-=1; return rv; }
	u16 r16() { u16 rv=*((u16*)dma_buffer_in);dma_buffer_in+=2;dma_count_in-=2; return rv; }
	u32 r32() { u32 rv=*(u32*)dma_buffer_in;dma_buffer_in+=4;dma_count_in-=4; return rv; }
	void rptr(void* dst, u32 len)
	{
		u8* dst8=(u8*)dst;
		while(len--)
			*dst8++=r8();
	}
	u32 r_count() { return dma_count_in; }

	u32 Dma(u32 Command,u32* buffer_in,u32 buffer_in_len,u32* buffer_out,u32& buffer_out_len)
	{
		dma_buffer_out=(u8*)buffer_out;
		dma_count_out=&buffer_out_len;

		dma_buffer_in=(u8*)buffer_in;
		dma_count_in=buffer_in_len;

		return dma(Command);
	}
	virtual u32 dma(u32 cmd)=0;

	virtual u32 RawDma(u32* buffer_in, u32 buffer_in_len, u32* buffer_out)
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

		verify(u8(outlen/4)*4==outlen);
		buffer_out[0] = (resp <<0 ) | (send << 8) | (reci << 16) | ((outlen / 4) << 24);

		return outlen + 4;
	}
};

/*
	Sega Dreamcast Controller
	No error checking of any kind, but works just fine
*/
struct maple_sega_controller: maple_base
{
	virtual u32 get_capabilities() {
		// byte 0: 0  0  0  0  0  0  0  0
		// byte 1: 0  0  a5 a4 a3 a2 a1 a0
		// byte 2: R2 L2 D2 U2 D  X  Y  Z
		// byte 3: R  L  D  U  St A  B  C

		return 0xfe060f00;	// 4 analog axes (0-3) X Y A B Start U D L R
	}

	virtual u32 transform_kcode(u32 kcode) {
		return kcode | 0xF901;		// mask off DPad2, C, D and Z;
	}

	virtual u32 get_analog_axis(int index, const PlainJoystickState &pjs)
	{
		if (index == 2 || index == 3)
		{
			// Limit the magnitude of the analog axes to 128
			s8 xaxis = pjs.joy[PJAI_X1] - 128;
			s8 yaxis = pjs.joy[PJAI_Y1] - 128;
			limit_joystick_magnitude<128>(xaxis, yaxis);
			if (index == 2)
				return xaxis + 128;
			else
				return yaxis + 128;
		}
		else if (index == 0)
			return pjs.trigger[PJTI_R];		// Right trigger
		else if (index == 1)
			return pjs.trigger[PJTI_L];		// Left trigger
		else
			return 0x80;					// unused
	}

	virtual MapleDeviceType get_device_type() override
	{
		return MDT_SegaController;
	}

	virtual const char *get_device_name()
	{
		return maple_sega_controller_name;
	}

	virtual const char *get_device_brand()
	{
		return maple_sega_brand;
	}

	virtual u32 dma(u32 cmd) override
	{
		//printf("maple_sega_controller::dma Called 0x%X;Command %d\n", bus_id, cmd);
		switch (cmd)
		{
		case MDC_DeviceRequest:
		case MDC_AllStatusReq:
			//caps
			//4
			w32(MFID_0_Input);

			//struct data
			//3*4
			w32(get_capabilities());
			w32(0);
			w32(0);

			//1	area code
			w8(0xFF);

			//1	direction
			w8(0);

			//30
			wstr(get_device_name(), 30);

			//60
			wstr(get_device_brand(), 60);

			//2
			w16(0x01AE);	// 43 mA

			//2
			w16(0x01F4);	// 50 mA

			return cmd == MDC_DeviceRequest ? MDRS_DeviceStatus : MDRS_DeviceStatusAll;

			//controller condition
		case MDCF_GetCondition:
			{
				PlainJoystickState pjs;
				config->GetInput(&pjs);
				//caps
				//4
				w32(MFID_0_Input);

				//state data
				//2 key code
				w16(transform_kcode(pjs.kcode));

				//triggers
				//1 R
				w8(get_analog_axis(0, pjs));
				//1 L
				w8(get_analog_axis(1, pjs));

				//joyx
				//1
				w8(get_analog_axis(2, pjs));
				//joyy
				//1
				w8(get_analog_axis(3, pjs));

				//not used on dreamcast
				//1
				w8(get_analog_axis(4, pjs));
				//1
				w8(get_analog_axis(5, pjs));
			}

			return MDRS_DataTransfer;

		case MDC_DeviceReset:
			return MDRS_DeviceReply;

		case MDC_DeviceKill:
			return MDRS_DeviceReply;

		default:
			INFO_LOG(MAPLE, "maple_sega_controller: Unknown maple command %d", cmd);
			return MDRE_UnknownCmd;
		}
	}
};

struct maple_atomiswave_controller: maple_sega_controller
{
	virtual u32 get_capabilities() override {
		// byte 0: 0  0  0  0  0  0  0  0
		// byte 1: 0  0  a5 a4 a3 a2 a1 a0
		// byte 2: R2 L2 D2 U2 D  X  Y  Z
		// byte 3: R  L  D  U  St A  B  C

		return 0xff663f00;	// 6 analog axes, X Y L2/D2(?) A B C Start U D L R
	}

	virtual u32 transform_kcode(u32 kcode) override {
		return kcode | AWAVE_TRIGGER_KEY;
	}

	virtual u32 get_analog_axis(int index, const PlainJoystickState &pjs) override {
		if (index < 2 || index > 5)
			return 0x80;
		index -= 2;
		return pjs.joy[index];
	}
};

/*
	Sega Twin Stick Controller
*/
struct maple_sega_twinstick: maple_sega_controller
{
	virtual u32 get_capabilities() override {
		// byte 0: 0  0  0  0  0  0  0  0
		// byte 1: 0  0  a5 a4 a3 a2 a1 a0
		// byte 2: R2 L2 D2 U2 D  X  Y  Z
		// byte 3: R  L  D  U  St A  B  C

		return 0xfefe0000;	// no analog axes, X Y A B D Start U/D/L/R U2/D2/L2/R2
	}

	virtual u32 transform_kcode(u32 kcode) override {
		return kcode | 0x0101;
	}

	virtual MapleDeviceType get_device_type() override {
		return MDT_TwinStick;
	}

	virtual u32 get_analog_axis(int index, const PlainJoystickState &pjs) override {
		return 0x80;
	}

	virtual const char *get_device_name() override {
		return maple_sega_twinstick_name;
	}
};


/*
	Ascii Stick (Arcade/FT Stick)
*/
struct maple_ascii_stick: maple_sega_controller
{
	virtual u32 get_capabilities() override {
		// byte 0: 0  0  0  0  0  0  0  0
		// byte 1: 0  0  a5 a4 a3 a2 a1 a0
		// byte 2: R2 L2 D2 U2 D  X  Y  Z
		// byte 3: R  L  D  U  St A  B  C

		return 0xff070000;	// no analog axes, X Y Z A B C Start U/D/L/R
	}

	virtual u32 transform_kcode(u32 kcode) override {
		return kcode | 0xF800;
	}

	virtual MapleDeviceType get_device_type() override {
		return MDT_AsciiStick;
	}

	virtual u32 get_analog_axis(int index, const PlainJoystickState &pjs) override {
		return 0x80;
	}

	virtual const char *get_device_name() override {
		return maple_ascii_stick_name;
	}
};

/*
	Sega Dreamcast Visual Memory Unit
	This is pretty much done (?)
*/


u8 vmu_default[] = {
		0x78,0x9c,0xed,0xd2,0x31,0x4e,0x02,0x61,0x10,0x06,0xd0,0x8f,0x04,0x28,0x4c,0x2c,
		0x28,0x2d,0x0c,0xa5,0x57,0xe0,0x16,0x56,0x16,0x76,0x14,0x1e,0xc4,0x03,0x50,0x98,
		0x50,0x40,0x69,0xc1,0x51,0x28,0xbc,0x8e,0x8a,0x0a,0xeb,0xc2,0xcf,0x66,0x13,0x1a,
		0x13,0xa9,0x30,0x24,0xe6,0xbd,0xc9,0x57,0xcc,0x4c,0x33,0xc5,0x2c,0xb3,0x48,0x6e,
		0x67,0x01,0x00,0x00,0x00,0x00,0x00,0x4e,0xaf,0xdb,0xe4,0x7a,0xd2,0xcf,0x53,0x16,
		0x6d,0x46,0x99,0xb6,0xc9,0x78,0x9e,0x3c,0x5f,0x9c,0xfb,0x3c,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x80,0x5f,0xd5,0x45,0xfd,0xef,0xaa,0xca,0x6b,0xde,0xf2,0x9e,0x55,
		0x3e,0xf2,0x99,0xaf,0xac,0xb3,0x49,0x95,0xef,0xd4,0xa9,0x9a,0xdd,0xdd,0x0f,0x9d,
		0x52,0xca,0xc3,0x91,0x7f,0xb9,0x9a,0x0f,0x6e,0x92,0xfb,0xee,0xa1,0x2f,0x6d,0x76,
		0xe9,0x64,0x9b,0xcb,0xf4,0xf2,0x92,0x61,0x33,0x79,0xfc,0xeb,0xb7,0xe5,0x44,0xf6,
		0x77,0x19,0x06,0xef,
};

struct maple_sega_vmu: maple_base
{
	FILE* file;
	u8 flash_data[128*1024];
	u8 lcd_data[192];
	u8 lcd_data_decoded[48*32];

	virtual MapleDeviceType get_device_type() override
	{
		return MDT_SegaVMU;
	}

	// creates an empty VMU
	bool init_emptyvmu()
	{
		INFO_LOG(MAPLE, "Initialising empty VMU...");

		uLongf dec_sz = sizeof(flash_data);
		int rv = uncompress(flash_data, &dec_sz, vmu_default, sizeof(vmu_default));

		verify(rv == Z_OK);
		verify(dec_sz == sizeof(flash_data));

		return (rv == Z_OK && dec_sz == sizeof(flash_data));
	}

	virtual bool maple_serialize(void **data, unsigned int *total_size) override
	{
		REICAST_SA(flash_data,128*1024);
		REICAST_SA(lcd_data,192);
		REICAST_SA(lcd_data_decoded,48*32);
		return true ;
	}
	virtual bool maple_unserialize(void **data, unsigned int *total_size) override
	{
		REICAST_USA(flash_data,128*1024);
		REICAST_USA(lcd_data,192);
		REICAST_USA(lcd_data_decoded,48*32);
		return true ;
	}
	virtual void OnSetup() override
	{
		memset(flash_data, 0, sizeof(flash_data));
		memset(lcd_data, 0, sizeof(lcd_data));
		char tempy[512];
		sprintf(tempy, "vmu_save_%s.bin", logical_port);
		// VMU saves used to be stored in .reicast, not in .reicast/data
		std::string apath = get_writable_config_path(tempy);
		if (!file_exists(apath))
			apath = get_writable_data_path(tempy);

		file = fopen(apath.c_str(), "rb+");
		if (!file)
		{
			INFO_LOG(MAPLE, "Unable to open VMU save file \"%s\", creating new file", apath.c_str());
			file = fopen(apath.c_str(), "wb");
			if (file) {
				if (!init_emptyvmu())
					WARN_LOG(MAPLE, "Failed to initialize an empty VMU, you should reformat it using the BIOS");

				fwrite(flash_data, sizeof(flash_data), 1, file);
				fseek(file, 0, SEEK_SET);
			}
			else
			{
				ERROR_LOG(MAPLE, "Failed to create VMU save file \"%s\"", apath.c_str());
			}
		}

		if (file != nullptr)
			fread(flash_data, 1, sizeof(flash_data), file);

		u8 sum = 0;
		for (u32 i = 0; i < sizeof(flash_data); i++)
			sum |= flash_data[i];

		if (sum == 0) {
			// This means the existing VMU file is completely empty and needs to be recreated

			if (init_emptyvmu())
			{
				if (file != nullptr)
				{
					fwrite(flash_data, sizeof(flash_data), 1, file);
					fseek(file, 0, SEEK_SET);
				}
			}
			else
			{
				WARN_LOG(MAPLE, "Failed to initialize an empty VMU, you should reformat it using the BIOS");
			}
		}

	}
	virtual ~maple_sega_vmu()
	{
		if (file) fclose(file);
	}
	virtual u32 dma(u32 cmd) override
	{
		//printf("maple_sega_vmu::dma Called for port %d:%d, Command %d\n", bus_id, bus_port, cmd);
		switch (cmd)
		{
		case MDC_DeviceRequest:
		case MDC_AllStatusReq:
			//caps
			//4
			w32(MFID_1_Storage | MFID_2_LCD | MFID_3_Clock);

			//struct data
			//3*4
			w32( 0x403f7e7e); // for clock
			w32( 0x00100500); // for LCD
			w32( 0x00410f00); // for storage
			//1  area code
			w8(0xFF);
			//1  direction
			w8(0);
			//30
			wstr(maple_sega_vmu_name,30);

			//60
			wstr(maple_sega_brand,60);

			//2
			w16(0x007c);	// 12.4 mA

			//2
			w16(0x0082);	// 13 mA

			return cmd == MDC_DeviceRequest ? MDRS_DeviceStatus : MDRS_DeviceStatusAll;

			//in[0] is function used
			//out[0] is function used
		case MDCF_GetMediaInfo:
			{
				u32 function=r32();
				switch(function)
				{
				case MFID_1_Storage:
					DEBUG_LOG(MAPLE, "VMU %s GetMediaInfo storage", logical_port);
					w32(MFID_1_Storage);

					if (*(u16*)&flash_data[0xFF * 512 + 0x40] != 0xFF)
					{
						// Unformatted state: return predetermined media information
						//total_size;
						w16(0xff);
						//partition_number;
						w16(0);
						//system_area_block;
						w16(0xFF);
						//fat_area_block;
						w16(0xfe);
						//number_fat_areas_block;
						w16(1);
						//file_info_block;
						w16(0xfd);
						//number_info_blocks;
						w16(0xd);
						//volume_icon;
						w8(0);
						//reserved1;
						w8(0);
						//save_area_block;
						w16(0xc8);
						//number_of_save_blocks;
						w16(0x1f);
						//reserverd0 (something for execution files?)
						w32(0);
					}
					else
					{
						// Get data from the vmu system area (block 0xFF)
						wptr(flash_data + 0xFF * 512 + 0x40, 24);
					}
					return MDRS_DataTransfer;//data transfer

				case MFID_2_LCD:
					{
						u32 pt=r32();
						if (pt!=0)
						{
							INFO_LOG(MAPLE, "VMU: MDCF_GetMediaInfo -> bad input |%08X|, returning MDRE_UnknownCmd", pt);
							return MDRE_UnknownCmd;
						}
						else
						{
							DEBUG_LOG(MAPLE, "VMU %s GetMediaInfo LCD", logical_port);
							w32(MFID_2_LCD);

							w8(47);             //X dots -1
							w8(31);             //Y dots -1
							w8(((1)<<4) | (0)); //1 Color, 0 contrast levels
							w8(2);              //Padding

							return MDRS_DataTransfer;
						}
					}

				default:
					INFO_LOG(MAPLE, "VMU: MDCF_GetMediaInfo -> Bad function used |%08X|, returning -2", function);
					return MDRE_UnknownFunction;//bad function
				}
			}
			break;

		case MDCF_BlockRead:
			{
				u32 function=r32();
				switch(function)
				{
				case MFID_1_Storage:
					{
						w32(MFID_1_Storage);
						u32 xo=r32();
						u32 Block = (SWAP32(xo))&0xffff;
						w32(xo);

						if (Block>255)
						{
							DEBUG_LOG(MAPLE, "Block read : %d", Block);
							DEBUG_LOG(MAPLE, "BLOCK READ ERROR");
							Block&=255;
						}
						else
							DEBUG_LOG(MAPLE, "VMU %s block read: Block %d addr %x len %d", logical_port, Block, Block*512, 512);
						wptr(flash_data+Block*512,512);
					}
					return MDRS_DataTransfer;//data transfer

				case MFID_2_LCD:
					DEBUG_LOG(MAPLE, "VMU %s read LCD", logical_port);
					w32(MFID_2_LCD);
					w32(r32()); // mnn ?
					wptr(flash_data,192);

					return MDRS_DataTransfer;//data transfer

				case MFID_3_Clock:
					if (r32()!=0)
					{
						INFO_LOG(MAPLE, "VMU: Block read: MFID_3_Clock : invalid params");
						return MDRE_TransmitAgain; //invalid params
					}
					else
					{
						w32(MFID_3_Clock);

						time_t now;
						time(&now);
						tm* timenow=localtime(&now);

						u8* timebuf=dma_buffer_out;

						w8((timenow->tm_year+1900)%256);
						w8((timenow->tm_year+1900)/256);

						w8(timenow->tm_mon+1);
						w8(timenow->tm_mday);

						w8(timenow->tm_hour);
						w8(timenow->tm_min);
						w8(timenow->tm_sec);
						w8(0);

						DEBUG_LOG(MAPLE, "VMU: CLOCK Read-> datetime is %04d/%02d/%02d ~ %02d:%02d:%02d!",
								timebuf[0] + timebuf[1] * 256,
								timebuf[2],
								timebuf[3],
								timebuf[4],
								timebuf[5],
								timebuf[6]);

						return MDRS_DataTransfer;//transfer reply ...
					}

				default:
					INFO_LOG(MAPLE, "VMU: cmd MDCF_BlockRead -> Bad function |%08X| used, returning -2", function);
					return MDRE_UnknownFunction;//bad function
				}
			}
			break;

		case MDCF_BlockWrite:
			{
				switch(r32())
				{
					case MFID_1_Storage:
					{
						u32 bph=r32();
						u32 Block = (SWAP32(bph))&0xffff;
						u32 Phase = ((SWAP32(bph))>>16)&0xff;
						u32 write_adr=Block*512+Phase*(512/4);
						u32 write_len=r_count();
						DEBUG_LOG(MAPLE, "VMU %s block write: Block %d Phase %d addr %x len %d", logical_port, Block, Phase, write_adr, write_len);
						if (write_adr + write_len > sizeof(flash_data))
						{
							INFO_LOG(MAPLE, "Failed to write VMU %s: overflow", logical_port);
							return MDRE_TransmitAgain; //invalid params
						}
						rptr(&flash_data[write_adr],write_len);

						if (file)
						{
							fseek(file,write_adr,SEEK_SET);
							fwrite(&flash_data[write_adr],1,write_len,file);
							fflush(file);
						}
						else
						{
							INFO_LOG(MAPLE, "Failed to save VMU %s data", logical_port);
						}
						return MDRS_DeviceReply;//just ko
					}

					case MFID_2_LCD:
					{
						DEBUG_LOG(MAPLE, "VMU %s LCD write", logical_port);
						r32();
						rptr(lcd_data,192);

						u8 white=0xff,black=0x00;

						for(int y=0;y<32;++y)
						{
							u8* dst=lcd_data_decoded+y*48;
							u8* src=lcd_data+6*y+5;
							for(int x=0;x<6;++x)
							{
								u8 col=*src--;
								for(int l=0;l<8;l++)
								{
									*dst++=col&1?black:white;
									col>>=1;
								}
							}
						}
						config->SetImage(lcd_data_decoded);
						push_vmu_screen(bus_id, bus_port, lcd_data_decoded);
						return  MDRS_DeviceReply;//just ko
					}

					case MFID_3_Clock:
						if (r32()!=0 || r_count()!=8)
						{
							INFO_LOG(MAPLE, "VMU %s clock write invalid params: rcount %d", logical_port, r_count());
							return MDRE_TransmitAgain;	//invalid params ...
						}
						else
						{
							u8 timebuf[8];
							rptr(timebuf,8);
							DEBUG_LOG(MAPLE, "VMU: CLOCK Write-> datetime is %04d/%02d/%02d ~ %02d:%02d:%02d! Nothing set tho ...",
									timebuf[0]+timebuf[1]*256,timebuf[2],timebuf[3],timebuf[4],timebuf[5],timebuf[6]);
							return  MDRS_DeviceReply;//ok !
						}

					default:
						INFO_LOG(MAPLE, "VMU: command MDCF_BlockWrite -> Bad function used, returning MDRE_UnknownFunction");
						return  MDRE_UnknownFunction;//bad function
				}
			}
			break;

		case MDCF_GetLastError:
			return MDRS_DeviceReply;//just ko

		case MDCF_SetCondition:
			{
				switch(r32())
				{
				case MFID_3_Clock:
					{
						u32 bp = r32();
						if (bp)
							INFO_LOG(MAPLE, "BEEP : %08X", bp);
					}
					return MDRS_DeviceReply; //just ko

				default:
					INFO_LOG(MAPLE, "VMU: command MDCF_SetCondition -> Bad function used, returning MDRE_UnknownFunction");
					return MDRE_UnknownFunction; //bad function
				}
			}
			break;

		case MDC_DeviceReset:
			return MDRS_DeviceReply;

		case MDC_DeviceKill:
			return MDRS_DeviceReply;

		default:
			DEBUG_LOG(MAPLE, "Unknown MAPLE COMMAND %d", cmd);
			return MDRE_UnknownCmd;
		}
	}
};


struct maple_microphone: maple_base
{
	u32 gain;
	bool sampling;
	bool eight_khz;

	virtual ~maple_microphone()
	{
		if (sampling)
			StopAudioRecording();
	}
	virtual MapleDeviceType get_device_type() override
	{
		return MDT_Microphone;
	}

	virtual bool maple_serialize(void **data, unsigned int *total_size) override
	{
		REICAST_S(gain);
		REICAST_S(sampling);
		REICAST_S(eight_khz);
		REICAST_SKIP(480 - sizeof(u32) - sizeof(bool) * 2);
		return true;
	}
	virtual bool maple_unserialize(void **data, unsigned int *total_size) override
	{
		if (sampling)
			StopAudioRecording();
		REICAST_US(gain);
		REICAST_US(sampling);
		REICAST_US(eight_khz);
		REICAST_SKIP(480 - sizeof(u32) - sizeof(bool) * 2);
		if (sampling)
			StartAudioRecording(eight_khz);
		return true;
	}
	virtual void OnSetup() override
	{
		gain = 0xf;
		sampling = false;
		eight_khz = false;
	}

	virtual u32 dma(u32 cmd) override
	{
		switch (cmd)
		{
		case MDC_DeviceRequest:
		case MDC_AllStatusReq:
			DEBUG_LOG(MAPLE, "maple_microphone::dma MDC_DeviceRequest");

			//caps
			//4
			w32(MFID_4_Mic);

			//struct data
			//3*4
			w32(0xf0000000);
			w32(0);
			w32(0);

			//1	area code
			w8(0xFF);

			//1	direction
			w8(0);

			//30
			wstr(maple_sega_mic_name, 30);

			//60
			wstr(maple_sega_brand, 60);

			//2
			w16(0x012C);	// 30 mA

			//2
			w16(0x012C);	// 30 mA

			return cmd == MDC_DeviceRequest ? MDRS_DeviceStatus : MDRS_DeviceStatusAll;

		case MDC_DeviceReset:
			DEBUG_LOG(MAPLE, "maple_microphone::dma MDC_DeviceReset");
			if (sampling)
				StopAudioRecording();
			OnSetup();
			return MDRS_DeviceReply;

		case MDCF_MICControl:
		{
			u32 function=r32();

			switch(function)
			{
			case MFID_4_Mic:
			{
				u32 subcommand = r8();
				u32 dt1 = r8();
				u16 dt23 = r16();

				switch(subcommand)
				{
				case 0x01:	// Get_Sampling_Data
				{
					w32(MFID_4_Mic);

					u8 micdata[240 * 2];
					u32 samples = RecordAudio(micdata, 240);

					//32 bit header
			        //status: bit   7      6     5 4      3      2    1    0
			        //        name  EX_BIT SBFOV 0 14LSB1 14LSB0 SMPL ulaw Fs
					w8((sampling << 2) | eight_khz);
					w8(gain); // gain
					w8(0);    //(unused)
					w8(samples); // sample count (max 240)

					wptr(micdata, ((samples + 1) >> 1) << 2);

					return MDRS_DataTransfer;
				}

				case 0x02:	// Basic_Control
					DEBUG_LOG(MAPLE, "maple_microphone::dma MDCF_MICControl Basic_Control DT1 %02x", dt1);
					eight_khz = ((dt1 >> 2) & 3) == 1;
					if (((dt1 & 0x80) == 0x80) != sampling)
					{
						if (sampling)
							StopAudioRecording();
						else
							StartAudioRecording(eight_khz);
						sampling = (dt1 & 0x80) == 0x80;
					}
					return MDRS_DeviceReply;

				case 0x03:	// AMP_GAIN
					gain = dt1;
					DEBUG_LOG(MAPLE, "maple_microphone::dma MDCF_MICControl set gain %x", gain);
					return MDRS_DeviceReply;

				case 0x04:	// EXTU_BIT
					DEBUG_LOG(MAPLE, "maple_microphone::dma MDCF_MICControl EXTU_BIT %#010x", dt1);
					return MDRS_DeviceReply;

				case 0x05:	// Volume_Mode
					DEBUG_LOG(MAPLE, "maple_microphone::dma MDCF_MICControl Volume_Mode %#010x", dt1);
					return MDRS_DeviceReply;

				case MDRE_TransmitAgain:
					WARN_LOG(MAPLE, "maple_microphone::dma MDCF_MICControl MDRE_TransmitAgain");
					//apparently this doesnt matter
					//wptr(micdata, SIZE_OF_MIC_DATA);
					return MDRS_DeviceReply;//MDRS_DataTransfer;

				default:
					INFO_LOG(MAPLE, "maple_microphone::dma UNHANDLED DT1 %02x DT23 %04x", dt1, dt23);
					return MDRE_UnknownFunction;
				}
			}

			default:
				INFO_LOG(MAPLE, "maple_microphone::dma UNHANDLED function %#010x", function);
				return MDRE_UnknownFunction;
			}
			break;
		}

		case MDC_DeviceKill:
			return MDRS_DeviceReply;

		default:
			INFO_LOG(MAPLE, "maple_microphone::dma UNHANDLED MAPLE COMMAND %d", cmd);
			return MDRE_UnknownCmd;
		}
	}
};


struct maple_sega_purupuru : maple_base
{
	u16 AST = 19, AST_ms = 5000;
	u32 VIBSET;

	virtual MapleDeviceType get_device_type() override
	{
		return MDT_PurupuruPack;
	}

   virtual bool maple_serialize(void **data, unsigned int *total_size) override
   {
      REICAST_S(AST);
      REICAST_S(AST_ms);
      REICAST_S(VIBSET);
      return true ;
   }
   virtual bool maple_unserialize(void **data, unsigned int *total_size) override
   {
      REICAST_US(AST);
      REICAST_US(AST_ms);
      REICAST_US(VIBSET);
      return true ;
   }
	virtual u32 dma(u32 cmd) override
	{
		switch (cmd)
		{
		case MDC_DeviceRequest:
		case MDC_AllStatusReq:
			//caps
			//4
			w32(MFID_8_Vibration);

			//struct data
			//3*4
			w32(0x00000101);
			w32(0);
			w32(0);

			//1	area code
			w8(0xFF);

			//1	direction
			w8(0);

			//30
			wstr(maple_sega_purupuru_name, 30);

			//60
			wstr(maple_sega_brand, 60);

			//2
			w16(0x00C8);	// 20 mA

			//2
			w16(0x0640);	// 160 mA

			return cmd == MDC_DeviceRequest ? MDRS_DeviceStatus : MDRS_DeviceStatusAll;

			//get last vibration
		case MDCF_GetCondition:

			w32(MFID_8_Vibration);

			w32(VIBSET);

			return MDRS_DataTransfer;

		case MDCF_GetMediaInfo:

			w32(MFID_8_Vibration);

			// PuruPuru vib specs
			w32(0x3B07E010);

			return MDRS_DataTransfer;

		case MDCF_BlockRead:

			w32(MFID_8_Vibration);
			w32(0);

			w16(2);
			w16(AST);

			return MDRS_DataTransfer;

		case MDCF_BlockWrite:

			//Auto-stop time
			AST = dma_buffer_in[10];
			AST_ms = AST * 250 + 250;

			return MDRS_DeviceReply;

		case MDCF_SetCondition:

			VIBSET = *(u32*)&dma_buffer_in[4];
			{
				//Do the rumble thing!
				u8 POW_POS = (VIBSET >> 8) & 0x7;
				u8 POW_NEG = (VIBSET >> 12) & 0x7;
				u8 FREQ = (VIBSET >> 16) & 0xFF;
				s16 INC = (VIBSET >> 24) & 0xFF;
				if (VIBSET & 0x8000)			// INH
					INC = -INC;
				else if (!(VIBSET & 0x0800))	// EXH
					INC = 0;
				bool CNT = VIBSET & 1;

				float power = std::min((POW_POS + POW_NEG) / 7.0, 1.0);

				u32 duration_ms;
				if (FREQ > 0 && (!CNT || INC))
					duration_ms = std::min((int)(1000 * (INC ? abs(INC) * std::max(POW_POS, POW_NEG) : 1) / FREQ), (int)AST_ms);
				else
					duration_ms = AST_ms;
				float inclination;
				if (INC == 0 || power == 0)
					inclination = 0.0;
				else
					inclination = FREQ / (1000.0 * INC * std::max(POW_POS, POW_NEG));
				config->SetVibration(power, inclination, duration_ms);
			}

			return MDRS_DeviceReply;

		case MDC_DeviceReset:
			return MDRS_DeviceReply;

		case MDC_DeviceKill:
			return MDRS_DeviceReply;

		default:
			INFO_LOG(MAPLE, "UNKOWN MAPLE COMMAND %d", cmd);
			return MDRE_UnknownCmd;
		}
	}
};

u8 kb_shift; 		// shift keys pressed (bitmask)
u8 kb_led; 			// leds currently lit
u8 kb_key[6]={0};	// normal keys pressed

struct maple_keyboard : maple_base
{
	virtual MapleDeviceType get_device_type() override
	{
		return MDT_Keyboard;
	}

	virtual u32 dma(u32 cmd) override
	{
		switch (cmd)
		{
		case MDC_DeviceRequest:
		case MDC_AllStatusReq:
			//caps
			//4
			w32(MFID_6_Keyboard);

			//struct data
			//3*4
			w32(0x80000502);	// US, 104 keys
			w32(0);
			w32(0);
			//1	area code
			w8(0xFF);
			//1	direction
			w8(0);
			// Product name (30)
			wstr(maple_sega_kbd_name, 30);

			// License (60)
			wstr(maple_sega_brand, 60);

			// Low-consumption standby current (2)
			w16(0x01AE);	// 43 mA

			// Maximum current consumption (2)
			w16(0x01F5);	// 50.1 mA

			return cmd == MDC_DeviceRequest ? MDRS_DeviceStatus : MDRS_DeviceStatusAll;

		case MDCF_GetCondition:
			w32(MFID_6_Keyboard);
			//struct data
			//int8 shift          ; shift keys pressed (bitmask)	//1
			w8(kb_shift);
			//int8 led            ; leds currently lit			//1
			w8(kb_led);
			//int8 key[6]         ; normal keys pressed			//6
			for (int i = 0; i < 6; i++)
			{
				w8(kb_key[i]);
			}

			return MDRS_DataTransfer;

		case MDC_DeviceReset:
			return MDRS_DeviceReply;

		case MDC_DeviceKill:
			return MDRS_DeviceReply;

		default:
			INFO_LOG(MAPLE, "Keyboard: unknown MAPLE COMMAND %d", cmd);
			return MDRE_UnknownCmd;
		}
	}
};

// Mouse buttons
// bit 0: Button C
// bit 1: Right button (B)
// bit 2: Left button (A)
// bit 3: Wheel button
u32 mo_buttons = 0xFFFFFFFF;
// Relative mouse coordinates [-512:511]
f32 mo_x_delta;
f32 mo_y_delta;
f32 mo_wheel_delta;
// Absolute mouse coordinates
// Range [0:639] [0:479]
// but may be outside this range if the pointer is offscreen or outside the 4:3 window.
s32 mo_x_abs;
s32 mo_y_abs;

struct maple_mouse : maple_base
{
	virtual MapleDeviceType get_device_type() override
	{
		return MDT_Mouse;
	}

	static u16 mo_cvt(f32 delta)
	{
		delta += 0x200;
		if (delta <= 0)
			delta = 0;
		else if (delta > 0x3FF)
			delta = 0x3FF;

		return (u16)lroundf(delta);
	}

	virtual u32 dma(u32 cmd) override
	{
		switch (cmd)
		{
		case MDC_DeviceRequest:
		case MDC_AllStatusReq:
			//caps
			//4
			w32(MFID_9_Mouse);

			//struct data
			//3*4
			w32(0x00070E00);	// Mouse, 3 buttons, 3 axes
			w32(0);
			w32(0);
			//1	area code
			w8(0xFF);
			//1	direction
			w8(0);
			// Product name (30)
			wstr(maple_sega_mouse_name, 30);

			// License (60)
			wstr(maple_sega_brand, 60);

			// Low-consumption standby current (2)
			w16(0x0069);	// 10.5 mA

			// Maximum current consumption (2)
			w16(0x0120);	// 28.8 mA

			return cmd == MDC_DeviceRequest ? MDRS_DeviceStatus : MDRS_DeviceStatusAll;

		case MDCF_GetCondition:
			w32(MFID_9_Mouse);
			//struct data
			//int32 buttons       ; digital buttons bitfield (little endian)
			w32(mo_buttons);
			//int16 axis1         ; horizontal movement (0-$3FF) (little endian)
			w16(mo_cvt(mo_x_delta));
			//int16 axis2         ; vertical movement (0-$3FF) (little endian)
			w16(mo_cvt(mo_y_delta));
			//int16 axis3         ; mouse wheel movement (0-$3FF) (little endian)
			w16(mo_cvt(mo_wheel_delta));
			//int16 axis4         ; ? movement (0-$3FF) (little endian)
			w16(mo_cvt(0));
			//int16 axis5         ; ? movement (0-$3FF) (little endian)
			w16(mo_cvt(0));
			//int16 axis6         ; ? movement (0-$3FF) (little endian)
			w16(mo_cvt(0));
			//int16 axis7         ; ? movement (0-$3FF) (little endian)
			w16(mo_cvt(0));
			//int16 axis8         ; ? movement (0-$3FF) (little endian)
			w16(mo_cvt(0));

			mo_x_delta=0;
			mo_y_delta=0;
			mo_wheel_delta = 0;

			return MDRS_DataTransfer;

		case MDC_DeviceReset:
			return MDRS_DeviceReply;

		case MDC_DeviceKill:
			return MDRS_DeviceReply;

		default:
			INFO_LOG(MAPLE, "Mouse: unknown MAPLE COMMAND %d", cmd);
			return MDRE_UnknownCmd;
		}
	}
};

struct maple_lightgun : maple_base
{
	virtual u32 transform_kcode(u32 kcode) {
		if ((kcode & DC_BTN_RELOAD) == 0)
			kcode &= ~DC_BTN_A;	// trigger
		return kcode | 0xFF01;
	}

	virtual MapleDeviceType get_device_type() override
	{
		return MDT_LightGun;
	}

	virtual u32 dma(u32 cmd) override
	{
		switch (cmd)
		{
		case MDC_DeviceRequest:
		case MDC_AllStatusReq:
			//caps
			//4
			w32(MFID_7_LightGun | MFID_0_Input);

			//struct data
			//3*4
			w32(0);				// Light gun
			w32(0xFE000000);	// Controller
			w32(0);
			//1	area code
			w8(1);				// FF: Worldwide, 01: North America
			//1	direction
			w8(0);
			// Product name (30)
			wstr(maple_sega_lightgun_name, 30);

			// License (60)
			wstr(maple_sega_brand, 60);

			// Low-consumption standby current (2)
			w16(0x0069);	// 10.5 mA

			// Maximum current consumption (2)
			w16(0x0120);	// 28.8 mA

			return cmd == MDC_DeviceRequest ? MDRS_DeviceStatus : MDRS_DeviceStatusAll;

		case MDCF_GetCondition:
		{
			PlainJoystickState pjs;
			config->GetInput(&pjs);

			//caps
			//4
			w32(MFID_0_Input);

			//state data
			//2 key code
			w16(transform_kcode(pjs.kcode));

			//not used
			//2
			w16(0xFFFF);

			//not used
			//4
			w32(0x80808080);
		}
		return MDRS_DataTransfer;

		case MDC_DeviceReset:
			return MDRS_DeviceReply;

		case MDC_DeviceKill:
			return MDRS_DeviceReply;

		default:
			INFO_LOG(MAPLE, "Light gun: unknown MAPLE COMMAND %d", cmd);
			return MDRE_UnknownCmd;
		}
	}

	virtual bool get_lightgun_pos() override
	{
		PlainJoystickState pjs;
		config->GetInput(&pjs);

		if ((pjs.kcode & DC_BTN_RELOAD) == 0)
			read_lightgun_position(-1, -1);
		else
			read_lightgun_position(mo_x_abs, mo_y_abs);
		return true;
	}
};

struct atomiswave_lightgun : maple_lightgun
{
	virtual u32 transform_kcode(u32 kcode) override {
		// No need for reload on AW
		return (kcode & AWAVE_TRIGGER_KEY) == 0 ? ~AWAVE_BTN0_KEY : ~0;
	}
};

u8 EEPROM[0x100];
bool EEPROM_loaded = false;

void load_naomi_eeprom()
{
	if (!EEPROM_loaded)
	{
		EEPROM_loaded = true;
		std::string nvmemSuffix = cfgLoadStr("net", "nvmem", "");
		std::string eeprom_file = get_game_save_prefix() + nvmemSuffix + ".eeprom";
		FILE* f = fopen(eeprom_file.c_str(), "rb");
		if (f)
		{
			fread(EEPROM, 1, 0x80, f);
			fclose(f);
			DEBUG_LOG(MAPLE, "Loaded EEPROM from %s", eeprom_file.c_str());
		}
		else if (naomi_default_eeprom != NULL)
		{
			DEBUG_LOG(MAPLE, "Using default EEPROM file");
			memcpy(EEPROM, naomi_default_eeprom, 0x80);
		}
		else
			DEBUG_LOG(MAPLE, "EEPROM file not found at %s and no default found", eeprom_file.c_str());
	}
}

static u32 naomi_button_mapping[32] = {
		NAOMI_SERVICE_KEY,	// DC_BTN_C
		NAOMI_BTN1_KEY,		// DC_BTN_B
		NAOMI_BTN0_KEY,		// DC_BTN_A
		NAOMI_START_KEY,	// DC_BTN_START
		NAOMI_UP_KEY,		// DC_DPAD_UP
		NAOMI_DOWN_KEY,		// DC_DPAD_DOWN
		NAOMI_LEFT_KEY,		// DC_DPAD_LEFT
		NAOMI_RIGHT_KEY,	// DC_DPAD_RIGHT
		NAOMI_TEST_KEY,		// DC_BTN_Z
		NAOMI_BTN3_KEY,		// DC_BTN_Y
		NAOMI_BTN2_KEY,		// DC_BTN_X
		NAOMI_COIN_KEY,		// DC_BTN_D
		NAOMI_BTN4_KEY,		// DC_DPAD2_UP
		NAOMI_BTN5_KEY,		// DC_DPAD2_DOWN
		NAOMI_BTN6_KEY,		// DC_DPAD2_LEFT
		NAOMI_BTN7_KEY,		// DC_DPAD2_RIGHT

		0,					// DC_BTN_RELOAD
		NAOMI_BTN8_KEY,
};
extern u32 awave_button_mapping[32];
extern u32 awavelg_button_mapping[32];

const char *GetCurrentGameButtonName(DreamcastKey key)
{
	if (NaomiGameInputs == nullptr || key == EMU_BTN_NONE)
		return nullptr;
	u32 pos = 0;
	u32 val = (u32)key;
	while ((val & 1) == 0)
	{
		pos++;
		val >>= 1;
	}
	u32 arcade_key;
	if (settings.platform.system == DC_PLATFORM_NAOMI)
	{
		if (pos >= ARRAY_SIZE(naomi_button_mapping))
			return nullptr;
		arcade_key = naomi_button_mapping[pos];
	}
	else
	{
		if (pos >= ARRAY_SIZE(awave_button_mapping))
			return nullptr;
		const u32* mapping = settings.input.JammaSetup == JVS::LightGun ? awavelg_button_mapping : awave_button_mapping;
		arcade_key = mapping[pos];
	}
	for (int i = 0; NaomiGameInputs->buttons[i].source != 0; i++)
		if (NaomiGameInputs->buttons[i].source == arcade_key)
			return NaomiGameInputs->buttons[i].name;

	return nullptr;
}

const char *GetCurrentGameAxisName(DreamcastKey axis)
{
	if (NaomiGameInputs == nullptr || axis == EMU_BTN_NONE)
		return nullptr;

	for (int i = 0; NaomiGameInputs->axes[i].name != nullptr; i++)
	{
		DreamcastKey cur_axis;
		switch (NaomiGameInputs->axes[i].axis)
		{
		case 0:
			cur_axis = DC_AXIS_X;
			break;
		case 1:
			cur_axis = DC_AXIS_Y;
			break;
		case 2:
			cur_axis = DC_AXIS_X2;
			break;
		case 3:
			cur_axis = DC_AXIS_Y2;
			break;
		case 4:
			cur_axis = DC_AXIS_RT;
			break;
		case 5:
			cur_axis = DC_AXIS_LT;
			break;
		default:
			cur_axis = EMU_BTN_NONE;
			break;
		}
		if (cur_axis == axis)
			return NaomiGameInputs->axes[i].name;
	}

	return nullptr;
}

/*
 * Sega JVS I/O board
*/
static bool old_coin_chute[4];
static int coin_count[4];

struct maple_naomi_jamma;

class jvs_io_board
{
public:
	jvs_io_board(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
	{
		this->node_id = node_id;
		this->parent = parent;
		this->first_player = first_player;
		init_mappings();
	}
	virtual ~jvs_io_board() = default;

	u32 handle_jvs_message(u8 *buffer_in, u32 length_in, u8 *buffer_out);
	bool maple_serialize(void **data, unsigned int *total_size);
	bool maple_unserialize(void **data, unsigned int *total_size);

	bool lightgun_as_analog = false;

protected:
	virtual const char *get_id() = 0;
	virtual u16 read_analog_axis(int player_num, int player_axis, bool inverted);

	virtual void read_digital_in(u16 *v)
	{
		memset(v, 0, sizeof(u16) * 4);
		for (u32 player = first_player; player < ARRAY_SIZE(kcode); player++)
		{
			u32 keycode = ~kcode[player];
			if (keycode == 0)
				continue;
			if (keycode & DC_BTN_RELOAD)
				keycode |= DC_BTN_A;

			// P1 mapping (only for P2)
			if (player == 1)
			{
				for (u32 i = 0; i < p1_mapping.size(); i++)
					if ((keycode & (1 << i)) != 0)
						v[0] |= p1_mapping[i];
			}
			// normal mapping
			for (u32 i = 0; i < cur_mapping.size(); i++)
				if ((keycode & (1 << i)) != 0)
					v[player - first_player] |= cur_mapping[i];
			// P2 mapping (only for P1)
			if (player == 0)
			{
				bool found = false;
				for (u32 i = 0; i < p2_mapping.size(); i++)
				{
					if ((keycode & (1 << i)) != 0)
						v[1] |= p2_mapping[i];
					if (p2_mapping[i] != 0)
						found = true;
				}
				if (found)
					// if there are P2 mappings for P1 then there's only 1 player
					break;
			}
		}
	}

	virtual void write_digital_out(int count, u8 *data) { }

	u32 player_count = 0;
	u32 digital_in_count = 0;
	u32 coin_input_count = 0;
	u32 analog_count = 0;
	u32 encoder_count = 0;
	u32 light_gun_count = 0;
	u32 output_count = 0;
	bool init_in_progress = false;

private:
	void init_mappings()
	{
		p1_mapping.fill(0);
		p2_mapping.fill(0);
		memcpy(&cur_mapping[0], naomi_button_mapping, sizeof(naomi_button_mapping));
		if (NaomiGameInputs == nullptr)
			// Use default mapping
			return;

		for (int i = 0; NaomiGameInputs->buttons[i].source != 0; i++)
		{
			u32 source = NaomiGameInputs->buttons[i].source;
			for (u32 j = 0; j < ARRAY_SIZE(naomi_button_mapping); j++)
			{
				if (naomi_button_mapping[j] == source)
				{
					p1_mapping[j] = NaomiGameInputs->buttons[i].p1_target;
					p2_mapping[j] = NaomiGameInputs->buttons[i].p2_target;
					u32 target = NaomiGameInputs->buttons[i].target;
					if (target == 0 && p1_mapping[j] == 0 && p2_mapping[j] == 0)
						target = source;
					cur_mapping[j] = target;
					break;
				}
			}
		}
	}

	u8 node_id;
	maple_naomi_jamma *parent;
	u8 first_player;

	std::array<u32, 32> cur_mapping;
	std::array<u32, 32> p1_mapping;
	std::array<u32, 32> p2_mapping;
};

// Most common JVS board
class jvs_837_13551 : public jvs_io_board
{
public:
	jvs_837_13551(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
		: jvs_io_board(node_id, parent, first_player)
	{
		player_count = 2;
		digital_in_count = 13;
		coin_input_count = 2;
		analog_count = 8;
		output_count = 6;
	}
protected:
	virtual const char *get_id() override { return "SEGA ENTERPRISES,LTD.;I/O BD JVS;837-13551 ;Ver1.00;98/10"; }

};

class jvs_837_13551_noanalog : public jvs_837_13551
{
public:
	jvs_837_13551_noanalog(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
		: jvs_837_13551(node_id, parent, first_player)
	{
		analog_count = 0;
	}
};

// Same in 4-player mode
class jvs_837_13551_4P : public jvs_837_13551
{
public:
	jvs_837_13551_4P(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
		: jvs_837_13551(node_id, parent, first_player)
	{
		player_count = 4;
		digital_in_count = 12;
		coin_input_count = 4;
		analog_count = 0;
	}
};

// Rotary encoders 2nd board
// Virtua Golf, Outtrigger, Shootout Pool
class jvs_837_13938 : public jvs_io_board
{
public:
	jvs_837_13938(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
		: jvs_io_board(node_id, parent, first_player)
	{
		player_count = 1;
		digital_in_count = 9;
		encoder_count = 4;
		output_count = 8;
	}
protected:
	virtual const char *get_id() override { return "SEGA ENTERPRISES,LTD.;837-13938 ENCORDER BD  ;Ver0.01;99/08"; }

};

// Sega Marine Fishing, 18 Wheeler (TODO)
class jvs_837_13844 : public jvs_io_board
{
public:
	jvs_837_13844(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
		: jvs_io_board(node_id, parent, first_player)
	{
		player_count = 2;
		coin_input_count = 2;
		digital_in_count = 12;
		analog_count = 8;
		output_count = 22;
	}
protected:
	virtual const char *get_id() override { return "SEGA ENTERPRISES,LTD.;837-13844-01 I/O CNTL BD2 ;Ver1.00;99/07"; }

};

class jvs_837_13844_encoders : public jvs_837_13844
{
public:
	jvs_837_13844_encoders(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
		: jvs_837_13844(node_id, parent, first_player)
	{
		digital_in_count = 8;
		encoder_count = 4;
	}
};

class jvs_837_13844_touch : public jvs_837_13844
{
public:
	jvs_837_13844_touch(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
		: jvs_837_13844(node_id, parent, first_player)
	{
		light_gun_count = 1;
	}
};

// Wave Runner GP: fake the drive board
class jvs_837_13844_wrungp : public jvs_837_13844
{
public:
	jvs_837_13844_wrungp(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
		: jvs_837_13844(node_id, parent, first_player)
	{
	}
protected:
	virtual void read_digital_in(u16 *v) override
	{
		jvs_837_13844::read_digital_in(v);

		// The drive board RX0-7 is connected to the following player inputs
		v[0] |= NAOMI_BTN2_KEY | NAOMI_BTN3_KEY | NAOMI_BTN4_KEY | NAOMI_BTN5_KEY;
		if (drive_board & 16)
			v[0] &= ~NAOMI_BTN5_KEY;
		if (drive_board & 32)
			v[0] &= ~NAOMI_BTN4_KEY;
		if (drive_board & 64)
			v[0] &= ~NAOMI_BTN3_KEY;
		if (drive_board & 128)
			v[0] &= ~NAOMI_BTN2_KEY;
		v[1] |= NAOMI_BTN2_KEY | NAOMI_BTN3_KEY | NAOMI_BTN4_KEY | NAOMI_BTN5_KEY;
		if (drive_board & 1)
			v[1] &= ~NAOMI_BTN5_KEY;
		if (drive_board & 2)
			v[1] &= ~NAOMI_BTN4_KEY;
		if (drive_board & 4)
			v[1] &= ~NAOMI_BTN3_KEY;
		if (drive_board & 8)
			v[1] &= ~NAOMI_BTN2_KEY;
	}

	virtual void write_digital_out(int count, u8 *data) override {
		if (count != 3)
			return;

		// The drive board TX0-7 is connected to outputs 15-22
		// shifting right by 2 to get the last 8 bits of the output
		u16 out = (data[1] << 6) | (data[2] >> 2);
		// reverse
		out = (out & 0xF0) >> 4 | (out & 0x0F) << 4;
		out = (out & 0xCC) >> 2 | (out & 0x33) << 2;
		out = (out & 0xAA) >> 1 | (out & 0x55) << 1;

		if (out == 0xff)
			drive_board = 0xff;
		else if ((out & 0xf) == 0xf)
		{
			out >>= 4;
			if (out > 7)
				drive_board = 0xff & ~(1 << (14 - out));
			else
				drive_board = 0xff & ~(1 << out);
		}
		else if ((out & 0xf0) == 0xf0)
		{
			out &= 0xf;
			if (out > 7)
				drive_board = 0xff & ~(1 << (out - 7));
			else
				drive_board = 0xff & ~(1 << (7 - out));
		}
	}

private:
	u8 drive_board = 0;
};

// Ninja assault
class jvs_namco_jyu : public jvs_io_board
{
public:
	jvs_namco_jyu(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
		: jvs_io_board(node_id, parent, first_player)
	{
		player_count = 2;
		coin_input_count = 2;
		digital_in_count = 12;
		output_count = 16;
		light_gun_count = 2;
	}
protected:
	virtual const char *get_id() override { return "namco ltd.;JYU-PCB;Ver1.00;JPN,2Coins 2Guns"; }
};

// Mazan
class jvs_namco_fcb : public jvs_io_board
{
public:
	jvs_namco_fcb(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
		: jvs_io_board(node_id, parent, first_player)
	{
		player_count = 1;
		coin_input_count = 2;
		digital_in_count = 16;
		output_count = 6;
		light_gun_count = 1;
		analog_count = 7;
		encoder_count = 2;
	}
protected:
	virtual const char *get_id() override { return "namco ltd.;FCB;Ver1.0;JPN,Touch Panel & Multipurpose"; }

	virtual u16 read_analog_axis(int player_num, int player_axis, bool inverted) override {
		if (init_in_progress)
			return 0;
		if (mo_x_abs < 0 || mo_x_abs > 639 || mo_y_abs < 0 || mo_y_abs > 479)
			return 0;
		else
			return 0x8000;
	}
};

// Gun Survivor
class jvs_namco_fca : public jvs_io_board
{
public:
	jvs_namco_fca(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
		: jvs_io_board(node_id, parent, first_player)
	{
		player_count = 1;
		coin_input_count = 1; // 2 makes bios crash
		digital_in_count = 16;
		output_count = 6;
		analog_count = 7;
		encoder_count = 2;
	}
protected:
	virtual const char *get_id() override { return "namco ltd.;FCA-1;Ver1.01;JPN,Multipurpose + Rotary Encoder"; }
};

// World Kicks
class jvs_namco_v226 : public jvs_io_board
{
public:
	jvs_namco_v226(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
		: jvs_io_board(node_id, parent, first_player)
	{
		player_count = 1;
		digital_in_count = 16;
		coin_input_count = 1;
		analog_count = 12;
		output_count = 6;
	}
protected:
	virtual const char *get_id() override { return "SEGA ENTERPRISES,LTD.;I/O BD JVS;837-13551 ;Ver1.00;98/10"; }

	virtual void read_digital_in(u16 *v) override
	{
		jvs_io_board::read_digital_in(v);
				// main button
		v[0] = ((v[0] & NAOMI_BTN0_KEY) << 6)		// start
				| ((v[1] & NAOMI_BTN0_KEY) << 2)	// left
				| ((v[2] & NAOMI_BTN0_KEY) << 1)	// right
				| ((v[3] & NAOMI_BTN0_KEY) >> 1)	// btn1
				// enter
				| ((v[0] & NAOMI_BTN3_KEY) << 3)	// btn0
				// service menu
				| (v[0] & (NAOMI_TEST_KEY | NAOMI_SERVICE_KEY | NAOMI_UP_KEY | NAOMI_DOWN_KEY));
	}

	u16 read_joystick_x(int joy_num)
	{
		s8 axis_x = joyx[joy_num];
		axis_y = joyy[joy_num];
		limit_joystick_magnitude<64>(axis_x, axis_y);
		return std::min(0xff, 0x80 - axis_x) << 8;
	}

	u16 read_joystick_y(int joy_num)
	{
		return std::min(0xff, 0x80 - axis_y) << 8;
	}

	virtual u16 read_analog_axis(int player_num, int player_axis, bool inverted) override {
		switch (player_axis)
		{
		case 0:
			return read_joystick_x(0);
		case 1:
			return read_joystick_y(0);
		case 2:
			return read_joystick_x(1);
		case 3:
			return read_joystick_y(1);
		case 4:
			return read_joystick_x(2);
		case 5:
			return read_joystick_y(2);
		case 6:
			return read_joystick_x(3);
		case 7:
			return read_joystick_y(3);
		case 8:
			return rt[0] << 8;
		case 9:
			return rt[1] << 8;
		case 10:
			return rt[2] << 8;
		case 11:
			return rt[3] << 8;
		default:
			return 0x8000;
		}
	}

private:
	s8 axis_y = 0;
};

// World Kicks PCB
class jvs_namco_v226_pcb : public jvs_io_board
{
public:
	jvs_namco_v226_pcb(u8 node_id, maple_naomi_jamma *parent, int first_player = 0)
		: jvs_io_board(node_id, parent, first_player)
	{
		player_count = 2;
		digital_in_count = 16;
		coin_input_count = 1;
		analog_count = 12;
		output_count = 6;
	}
protected:
	virtual const char *get_id() override { return "SEGA ENTERPRISES,LTD.;I/O BD JVS;837-13551 ;Ver1.00;98/10"; }

	virtual void read_digital_in(u16 *v) override
	{
		jvs_io_board::read_digital_in(v);
		for (u32 player = 0; player < player_count; player++)
		{
			u8 trigger = rt[player] >> 2;
					// Ball button
			v[player] = ((trigger & 0x20) << 3) | ((trigger & 0x10) << 5) | ((trigger & 0x08) << 7)
					| ((trigger & 0x04) << 9) | ((trigger & 0x02) << 11) | ((trigger & 0x01) << 13)
					// other buttons
					| (v[player] & (NAOMI_SERVICE_KEY | NAOMI_TEST_KEY | NAOMI_START_KEY))
					| ((v[player] & NAOMI_BTN0_KEY) >> 4);		// remap button4 to button0 (change button)
		}
	}

	u16 read_joystick_x(int joy_num)
	{
		s8 axis_x = joyx[joy_num];
		axis_y = joyy[joy_num];
		limit_joystick_magnitude<48>(axis_x, axis_y);
		return (axis_x + 128) << 8;
	}

	u16 read_joystick_y(int joy_num)
	{
		return std::min(0xff, 0x80 - axis_y) << 8;
	}

	virtual u16 read_analog_axis(int player_num, int player_axis, bool inverted) override {
		switch (player_axis)
		{
		case 0:
			return read_joystick_x(0);
		case 1:
			return read_joystick_y(0);
		case 4:
			return read_joystick_x(1);
		case 5:
			return read_joystick_y(1);
		default:
			return 0x8000;
		}
	}

private:
	s8 axis_y = 0;
};

struct maple_naomi_jamma : maple_sega_controller
{
	const u8 ALL_NODES = 0xff;

	std::vector<std::unique_ptr<jvs_io_board>> io_boards;
	bool crazy_mode = false;

	u8 jvs_repeat_request[32][256];
	u8 jvs_receive_buffer[32][258];
	u32 jvs_receive_length[32] = { 0 };

	maple_naomi_jamma()
	{
		switch (settings.input.JammaSetup)
		{
		case JVS::Default:
		default:
			io_boards.emplace_back(new jvs_837_13551(1, this));
			break;
		case JVS::FourPlayers:
			io_boards.emplace_back(new jvs_837_13551_4P(1, this));
			break;
		case JVS::RotaryEncoders:
			io_boards.emplace_back(new jvs_837_13938(1, this));
			io_boards.emplace_back(new jvs_837_13551(2, this));
			break;
		case JVS::OutTrigger:
			io_boards.emplace_back(new jvs_837_13938(1, this));
			io_boards.emplace_back(new jvs_837_13551_noanalog(2, this));
			break;
		case JVS::SegaMarineFishing:
			io_boards.emplace_back(new jvs_837_13844(1, this));
			break;
		case JVS::DualIOBoards4P:
			io_boards.emplace_back(new jvs_837_13551(1, this));
			io_boards.emplace_back(new jvs_837_13551(2, this, 2));
			break;
		case JVS::LightGun:
			io_boards.emplace_back(new jvs_namco_jyu(1, this));
			break;
		case JVS::LightGunAsAnalog:
			// Regular board sending lightgun coords as axis 0/1
			io_boards.emplace_back(new jvs_837_13551(1, this));
			io_boards.back()->lightgun_as_analog = true;
			break;
		case JVS::Mazan:
			io_boards.emplace_back(new jvs_namco_fcb(1, this));
			io_boards.emplace_back(new jvs_namco_fcb(2, this));
			break;
		case JVS::GunSurvivor:
			io_boards.emplace_back(new jvs_namco_fca(1, this));
			break;
		case JVS::DogWalking:
			io_boards.emplace_back(new jvs_837_13844_encoders(1, this));
			break;
		case JVS::TouchDeUno:
			io_boards.emplace_back(new jvs_837_13844_touch(1, this));
			break;
		case JVS::WorldKicks:
			io_boards.emplace_back(new jvs_namco_v226(1, this));
			break;
		case JVS::WorldKicksPCB:
			io_boards.emplace_back(new jvs_namco_v226_pcb(1, this));
			break;
		case JVS::WaveRunnerGP:
			io_boards.emplace_back(new jvs_837_13844_wrungp(1, this));
			break;
		}
	}
	virtual ~maple_naomi_jamma()
	{
		EEPROM_loaded = false;
	}

	virtual MapleDeviceType get_device_type()
	{
		return MDT_NaomiJamma;
	}

	u8 sense_line(u32 node_id)
	{
		bool last_node = node_id == io_boards.size();
		return last_node ? 0x8E : 0x8F;
	}

	void send_jvs_message(u32 node_id, u32 channel, u32 length, u8 *data)
	{
		if (node_id - 1 < io_boards.size())
		{
			u8 temp_buffer[256];
			u32 out_len = io_boards[node_id - 1]->handle_jvs_message(data, length, temp_buffer);
			if (out_len > 0)
			{
				u8 *pbuf = &jvs_receive_buffer[channel][jvs_receive_length[channel]];
				if (jvs_receive_length[channel] + out_len + 3 <= sizeof(jvs_receive_buffer[0]))
				{
					if (crazy_mode)
					{
						pbuf[0] = 0x00;		// ? 0: ok, 2: timeout, 3: dest node !=0, 4: checksum failed
						pbuf[1] = out_len;
						memcpy(&pbuf[2], temp_buffer, out_len);
						jvs_receive_length[channel] += out_len + 2;
					}
					else
					{
						pbuf[0] = node_id;
						pbuf[1] = 0x00;		// 0: ok, 2: timeout, 3: dest node !=0, 4: checksum failed
						pbuf[2] = out_len;
						memcpy(&pbuf[3], temp_buffer, out_len);
						jvs_receive_length[channel] += out_len + 3;
					}
				}
			}
		}
	}

	void send_jvs_messages(u32 node_id, u32 channel, bool use_repeat, u32 length, u8 *data, bool repeat_first)
	{
		u8 temp_buffer[256];
		if (data)
		{
			memcpy(temp_buffer, data, length);
		}
		if (node_id == ALL_NODES)
		{
			for (u32 i = 0; i < io_boards.size(); i++)
				send_jvs_message(i + 1, channel, length, temp_buffer);
		}
		else if (node_id >= 1 && node_id <= 32)
		{
			u32 repeat_len = jvs_repeat_request[node_id - 1][0];
			if (use_repeat && repeat_len > 0)
			{
				if (repeat_first)
				{
					memmove(temp_buffer + repeat_len, temp_buffer, length);
					memcpy(temp_buffer, &jvs_repeat_request[node_id - 1][1], repeat_len);
				}
				else
				{
					memcpy(temp_buffer + length, &jvs_repeat_request[node_id - 1][1], repeat_len);
				}
				length += repeat_len;
			}
			send_jvs_message(node_id, channel, length, temp_buffer);
		}
	}

	bool receive_jvs_messages(u32 channel)
	{
		u32 dword_length = (jvs_receive_length[channel] + 0x10 + 3 - 1) / 4 + 1;

		w8(MDRS_JVSReply);
		w8(0x00);
		w8(0x20);
		if (jvs_receive_length[channel] == 0)
		{
			w8(0x05);
			w8(0x32);
		}
		else
		{
			w8(dword_length);
			w8(0x16);
		}
		w8(0xff);
		w8(0xff);
		w8(0xff);
		w32(0xffffff00);
		w32(0);
		w32(0);
		
		if (jvs_receive_length[channel] == 0)
		{
			w32(0);
			return false;
		}

		w8(0);
		w8(channel);
		if (crazy_mode)
			w8(0x8E);
		else
			w8(sense_line(jvs_receive_buffer[channel][0]));	// bit 0 is sense line level. If set during F1 <n>, more I/O boards need addressing

		memcpy(dma_buffer_out, jvs_receive_buffer[channel], jvs_receive_length[channel]);
		dma_buffer_out += dword_length * 4 - 0x10 - 3;
		*dma_count_out += dword_length * 4 - 0x10 - 3;
		jvs_receive_length[channel] = 0;

		return true;
	}

	void handle_86_subcommand()
	{
		if (dma_count_in == 0)
		{
			w8(MDRS_JVSReply);
			w8(0);
			w8(0x20);
			w8(0x00);

			return;
		}
		u32 subcode = dma_buffer_in[0];

		// CT fw uses 13 as a 17, and 17 as 13 and also uses 19
		if (crazy_mode)
		{
			switch (subcode)
			{
			case 0x13:
				subcode = 0x17;
				break;
			case 0x17:
				subcode = 0x13;
				break;
			}
		}
		u8 node_id = 0;
		u8 *cmd = NULL;
		u32 len = 0;
		u8 channel = 0;
		if (dma_count_in >= 3)
		{
			if (dma_buffer_in[1] > 31 && dma_buffer_in[1] != 0xff && dma_count_in >= 8)	// TODO what is this?
			{
				node_id = dma_buffer_in[6];
				len = dma_buffer_in[7];
				cmd = &dma_buffer_in[8];
				channel = dma_buffer_in[5] & 0x1f;
			}
			else
			{
				node_id = dma_buffer_in[1];
				len = dma_buffer_in[2];
				cmd = &dma_buffer_in[3];
			}
		}

		switch (subcode)
		{
			case 0x13:	// Store repeated request
				if (len > 0 && node_id > 0 && node_id <= 0x1f)
				{
					INFO_LOG(MAPLE, "JVS node %d: Storing %d cmd bytes", node_id, len);
					jvs_repeat_request[node_id - 1][0] = len;
					memcpy(&jvs_repeat_request[node_id - 1][1], cmd, len);
				}
				w8(MDRS_JVSReply);
				w8(0);
				w8(0x20);
				w8(0x01);
				w8(dma_buffer_in[0] + 1);	// subcommand + 1
				w8(0);
				w8(len + 1);
				w8(0);
				break;

			case 0x15:	// Receive JVS data
				receive_jvs_messages(dma_buffer_in[1]);
				break;

			case 0x17:	// Transmit without repeat
				jvs_receive_length[channel] = 0;
				send_jvs_messages(node_id, channel, false, len, cmd, false);
				w8(MDRS_JVSReply);
				w8(0);
				w8(0x20);
				w8(0x01);
				w8(0x18);	// always
				w8(channel);
				w8(0x8E);	//sense_line(node_id));
				w8(0);
				break;

			case 0x19:	// Transmit with repeat
				jvs_receive_length[channel] = 0;
				send_jvs_messages(node_id, channel, true, len, cmd, true);
				w8(MDRS_JVSReply);
				w8(0);
				w8(0x20);
				w8(0x01);
				w8(0x18);	// always
				w8(channel);
				w8(sense_line(node_id));
				w8(0);
				break;

			case 0x21:	// Transmit with repeat
				jvs_receive_length[channel] = 0;
				send_jvs_messages(node_id, channel, true, len, cmd, false);
				w8(MDRS_JVSReply);
				w8(0);
				w8(0x20);
				w8(0x01);
				w8(0x18);	// always
				w8(channel);
				w8(sense_line(node_id));
				w8(0);
				break;

			case 0x35:	// Receive then transmit with repeat (15 then 27)
				receive_jvs_messages(channel);
				// FALLTHROUGH

			case 0x27:	// Transmit with repeat
				{
					jvs_receive_length[channel] = 0;

					u32 cmd_count = dma_buffer_in[6];
					u32 idx = 7;
					for (u32 i = 0; i < cmd_count; i++)
					{
						node_id = dma_buffer_in[idx];
						len = dma_buffer_in[idx + 1];
						cmd = &dma_buffer_in[idx + 2];
						idx += len + 2;

						send_jvs_messages(node_id, channel, true, len, cmd, false);
					}

					w8(MDRS_JVSReply);
					w8(0);
					w8(0x20);
					w8(0x01);
					w8(0x26);
					w8(channel);
					w8(sense_line(node_id));
					w8(0);
				}
				break;

			case 0x33:	// Receive then transmit with repeat (15 then 21)
				receive_jvs_messages(channel);
				send_jvs_messages(node_id, channel, true, len, cmd, false);
				w8(MDRS_JVSReply);
				w8(0);
				w8(0x20);
				w8(0x01);
				w8(0x18);	// always
				w8(channel);
				w8(sense_line(node_id));
				w8(0);
				break;

			case 0x0B:	//EEPROM write
			{
				load_naomi_eeprom();
				int address = dma_buffer_in[1];
				int size = dma_buffer_in[2];
				DEBUG_LOG(MAPLE, "EEprom write %08X %08X\n", address, size);
				//printState(Command,buffer_in,buffer_in_len);
				memcpy(EEPROM + address, dma_buffer_in + 4, size);

				std::string nvmemSuffix = cfgLoadStr("net", "nvmem", "");
				std::string eeprom_file = get_game_save_prefix() + nvmemSuffix + ".eeprom";
				FILE* f = fopen(eeprom_file.c_str(), "wb");
				if (f)
				{
					fwrite(EEPROM, 1, 0x80, f);
					fclose(f);
					INFO_LOG(MAPLE, "Saved EEPROM to %s", eeprom_file.c_str());
				}
				else
					WARN_LOG(MAPLE, "EEPROM SAVE FAILED to %s", eeprom_file.c_str());

				w8(MDRS_JVSReply);
				w8(0x00);
				w8(0x20);
				w8(0x01);
				memcpy(dma_buffer_out, EEPROM, 4);
				dma_buffer_out += 4;
				*dma_count_out += 4;
			}
			break;

			case 0x3:	//EEPROM read
			{
				load_naomi_eeprom();
				//printf("EEprom READ\n");
				int address = dma_buffer_in[1];
				//printState(Command,buffer_in,buffer_in_len);
				w8(MDRS_JVSReply);
				w8(0x00);
				w8(0x20);
				w8(0x20);
				memcpy(dma_buffer_out, EEPROM + address, 0x80);
				dma_buffer_out += 0x80;
				*dma_count_out += 0x80;
			}
			break;

			case 0x31:	// DIP switches
			{
				w8(MDRS_JVSReply);
				w8(0x00);
				w8(0x20);
				w8(0x05);

				w8(0x32);
				w8(0xff);		// in(0)
				w8(0xff);		// in(1)
				w8(0xff);		// in(2)

				w8(0x00);
				w8(0xff);		// in(4)
				w8(0xff);		// in(5) bit0: 1=VGA, 0=NTSCi
				w8(0xff);		// in(6)

				w32(0x00);
				w32(0x00);
				w32(0x00);
			}
			break;

			//case 0x3:
			//	break;

			case 0x1:
				w8(MDRS_JVSReply);
				w8(0x00);
				w8(0x20);
				w8(0x01);

				w8(0x2);
				w8(0x0);
				w8(0x0);
				w8(0x0);
				break;

			default:
				INFO_LOG(MAPLE, "JVS: Unknown 0x86 sub-command %x", subcode);
				w8(MDRE_UnknownCmd);
				w8(0x00);
				w8(0x20);
				w8(0x00);
				break;
		}
	}

	virtual u32 RawDma(u32* buffer_in, u32 buffer_in_len, u32* buffer_out)
	{
#ifdef DUMP_JVS
		printf("JVS IN: ");
		u8 *p = (u8*)buffer_in;
		for (int i = 0; i < buffer_in_len; i++) printf("%02x ", *p++);
		printf("\n");
#endif

		u32 out_len = 0;
		dma_buffer_out = (u8 *)buffer_out;
		dma_count_out = &out_len;

		dma_buffer_in = (u8 *)buffer_in + 4;
		dma_count_in = buffer_in_len - 4;

		u32 cmd = *(u8*)buffer_in;
		switch (cmd)
		{
			case MDC_JVSSelfTest:
				w8(MDRS_JVSSelfTestReply);
				w8(0x00);
				w8(0x20);
				w8(0x01);
				w8(0x00);
				break;

			case MDC_JVSCommand:
				handle_86_subcommand();
				break;

			case MDC_JVSUploadFirmware:
			{
				static u8 *ram;

				if (ram == NULL)
					ram = (u8 *)calloc(0x10000, 1);

				if (dma_buffer_in[1] == 0xff)
				{
					u32 hash = XXH32(ram, 0x10000, 0);
					LOGJVS("JVS Firmware hash %08x\n", hash);
					if (hash == 0xa7c50459	// CT
							|| hash == 0xae841e36)	// HOTD2
						crazy_mode = true;
					else
						crazy_mode = false;
#ifdef DUMP_JVS_FW
					FILE *fw_dump;
					char filename[128];
					for (int i = 0; ; i++)
					{
						sprintf(filename, "z80_fw_%d.bin", i);
						fw_dump = fopen(filename, "r");
						if (fw_dump == NULL)
						{
							fw_dump = fopen(filename, "w");
							INFO_LOG(MAPLE, "Saving JVS firmware to %s", filename);
							break;
						}
					}
					if (fw_dump)
					{
						fwrite(ram, 1, 0x10000, fw_dump);
						fclose(fw_dump);
					}
#endif
					free(ram);
					ram = NULL;
					for (int i = 0; i < 32; i++)
						jvs_repeat_request[i][0] = 0;

					return MDRS_DeviceReply;
				}
				int xfer_bytes;
				if (dma_buffer_in[0] == 0xff)
					xfer_bytes = 0x1C;
				else
					xfer_bytes = 0x18;
				u16 addr = (dma_buffer_in[2] << 8) + dma_buffer_in[3];
				memcpy(ram + addr, &dma_buffer_in[4], xfer_bytes);
				u8 sum = 0;
				for (int i = 0; i < 0x1C; i++)
					sum += dma_buffer_in[i];

				w8(0x80);	// or 0x81 on bootrom?
				w8(0);
				w8(0x20);
				w8(0x01);
				w8(sum);
				w8(0);
				w8(0);
				w8(0);

				w8(MDRS_DeviceReply);
				w8(0x00);
				w8(0x20);
				w8(0x00);
			}
			break;

		case MDC_JVSGetId:
			{
				const char ID1[] = "315-6149    COPYRIGHT SEGA E";
				const char ID2[] = "NTERPRISES CO,LTD.  1998    ";
				w8(0x83);
				w8(0x00);
				w8(0x20);
				w8(0x07);
				wstr(ID1, 28);

				w8(0x83);
				w8(0x00);
				w8(0x20);
				w8(0x05);
				wstr(ID2, 28);
			}
			break;

		case MDC_DeviceRequest:
			w8(MDRS_DeviceStatus);
			w8(0x00);
			w8(0x20);
			w8(0x00);
			break;

		case MDC_AllStatusReq:
			w8(MDRS_DeviceStatusAll);
			w8(0x00);
			w8(0x20);
			w8(0x00);
			break;

		case MDC_DeviceReset:
		case MDC_DeviceKill:
			w8(MDRS_DeviceReply);
			w8(0x00);
			w8(0x20);
			w8(0x00);
			break;

		case MDCF_GetCondition:
			w8(MDRE_UnknownCmd);
			w8(0x00);
			w8(0x00);
			w8(0x00);

			break;

		default:
			INFO_LOG(MAPLE, "Unknown Maple command %x", cmd);
			w8(MDRE_UnknownCmd);
			w8(0x00);
			w8(0x00);
			w8(0x00);

			break;
		}

#ifdef DUMP_JVS
		printf("JVS OUT: ");
		p = (u8 *)buffer_out;
		for (int i = 0; i < out_len; i++) printf("%02x ", p[i]);
		printf("\n");
#endif

		return out_len;
	}

	virtual bool maple_serialize(void **data, unsigned int *total_size)
	{
		REICAST_S(crazy_mode);
		REICAST_S(jvs_repeat_request);
		REICAST_S(jvs_receive_length);
		REICAST_S(jvs_receive_buffer);
		size_t board_count = io_boards.size();
		REICAST_S(board_count);
		for (u32 i = 0; i < io_boards.size(); i++)
			io_boards[i]->maple_serialize(data, total_size);

		return true ;
	}

	virtual bool maple_unserialize(void **data, unsigned int *total_size)
	{
		REICAST_US(crazy_mode);
		REICAST_US(jvs_repeat_request);
		REICAST_US(jvs_receive_length);
		REICAST_US(jvs_receive_buffer);
		size_t board_count;
		REICAST_US(board_count);
		for (u32 i = 0; i < board_count; i++)
			io_boards[i]->maple_unserialize(data, total_size);

		return true ;
	}
};

u16 jvs_io_board::read_analog_axis(int player_num, int player_axis, bool inverted)
{
	u16 v;
	switch (player_axis)
	{
	case 0:
		v = (joyx[player_num] + 128) << 8;
		break;
	case 1:
		v = (joyy[player_num] + 128) << 8;
		break;
	case 2:
		v = (joyrx[player_num] + 128) << 8;
		break;
	case 3:
		v = (joyry[player_num] + 128) << 8;
		break;
	default:
		return 0x8000;
	}
	return inverted ? 0xff00 - v : v;
}

#define JVS_OUT(b) buffer_out[length++] = b
#define JVS_STATUS1() JVS_OUT(1)

u32 jvs_io_board::handle_jvs_message(u8 *buffer_in, u32 length_in, u8 *buffer_out)
{
	u8 jvs_cmd = buffer_in[0];
	if (jvs_cmd == 0xF0)		// JVS reset
		// Nothing to do
		return 0;
	if (jvs_cmd == 0xF1 && (length_in < 2 || buffer_in[1] != node_id))
		// Not for us
		return 0;

	u32 length = 0;
	JVS_OUT(0xE0);	// sync
	JVS_OUT(0);		// master node id
	u8& jvs_length = buffer_out[length++];

	switch (jvs_cmd)
	{
	case 0xF1:		// set board address
		JVS_STATUS1();	// status
		JVS_STATUS1();	// report
		JVS_OUT(5);		// ?
		LOGJVS("JVS Node %d address assigned\n", node_id);
		break;

	case 0x10:	// Read ID data
		JVS_STATUS1();	// status
		JVS_STATUS1();	// report
		for (const char *p = get_id(); *p != 0; )
			JVS_OUT(*p++);
		JVS_OUT(0);
		break;

	case 0x11:	// Get command format version
		JVS_STATUS1();
		JVS_STATUS1();
		JVS_OUT(0x11);	// 1.1
		break;

	case 0x12:	// Get JAMMA VIDEO version
		JVS_STATUS1();
		JVS_STATUS1();
		JVS_OUT(0x20);	// 2.0
		break;

	case 0x13:	// Get communication version
		JVS_STATUS1();
		JVS_STATUS1();
		JVS_OUT(0x10);	// 1.0
		break;

	case 0x14:	// Get slave features
		JVS_STATUS1();
		JVS_STATUS1();

		JVS_OUT(1);		// Digital inputs
		JVS_OUT(player_count);
		JVS_OUT(digital_in_count);
		JVS_OUT(0);

		if (coin_input_count > 0)
		{
			JVS_OUT(2);		// Coin inputs
			JVS_OUT(coin_input_count);
			JVS_OUT(0);
			JVS_OUT(0);
		}

		if (analog_count > 0)
		{
			JVS_OUT(3);		// Analog inputs
			JVS_OUT(analog_count);	//   channel count
			JVS_OUT(0x10);			//   16 bits per channel, 0: unknown
			JVS_OUT(0);
		}

		if (encoder_count > 0)
		{
			JVS_OUT(4);		// Rotary encoders
			JVS_OUT(encoder_count);
			JVS_OUT(0);
			JVS_OUT(0);
		}

		if (light_gun_count > 0)
		{
			JVS_OUT(6);		// Light gun
			JVS_OUT(16);		//   X bits
			JVS_OUT(16);		//   Y bits
			JVS_OUT(light_gun_count);
		}

		JVS_OUT(0x12);	// General output driver
		JVS_OUT(output_count);
		JVS_OUT(0);
		JVS_OUT(0);

		JVS_OUT(0);		// End of list
		break;

	case 0x15:	// Master board ID
		JVS_STATUS1();
		JVS_STATUS1();
		break;

	case 0x70:
		LOGJVS("JVS 0x70: %02x %02x %02x %02x", buffer_in[1], buffer_in[2], buffer_in[3], buffer_in[4]);
		init_in_progress = true;
		JVS_STATUS1();
		JVS_STATUS1();
		if (buffer_in[2] == 3)
		{
			JVS_OUT(0x10);
			for (int i = 0; i < 16; i++)
				JVS_OUT(0x7f);
			if (buffer_in[4] == 0x10 || buffer_in[4] == 0x11)
				init_in_progress = false;
		}
		else
		{
			JVS_OUT(2);
			JVS_OUT(3);
			JVS_OUT(1);
		}
		break;

	default:
		if (jvs_cmd >= 0x20 && jvs_cmd <= 0x38) // Read inputs and more
		{
			LOGJVS("JVS Node %d: ", node_id);

			JVS_STATUS1();	// status
			for (u32 cmdi = 0; cmdi < length_in; )
			{
				switch (buffer_in[cmdi])
				{
				case 0x20:	// Read digital input
					{
						JVS_STATUS1();	// report byte

						u16 btns[4];
						read_digital_in(btns);
						JVS_OUT((btns[0] & NAOMI_TEST_KEY) ? 0x80 : 0x00); // test, tilt1, tilt2, tilt3, unused, unused, unused, unused
						LOGJVS("btns ");
						for (int player = 0; player < buffer_in[cmdi + 1]; player++)
						{
							LOGJVS("P%d %02x ", player + 1 + first_player, btns[player] >> 8);
							JVS_OUT(btns[player] >> 8);
							if (buffer_in[cmdi + 2] == 2)
							{
								LOGJVS("%02x ", btns[player] & 0xFF);
								JVS_OUT(btns[player]);
							}
						}
						cmdi += 3;
					}
					break;

				case 0x21:	// Read coins
					{
						JVS_STATUS1();	// report byte
						LOGJVS("coins ");
						u32 mask = 0;
						for (u32 i = 0; i < ARRAY_SIZE(naomi_button_mapping); i++)
						{
							if (naomi_button_mapping[i] == NAOMI_COIN_KEY)
							{
								mask = 1 << i;
								break;
							}
						}
						for (int slot = 0; slot < buffer_in[cmdi + 1]; slot++)
						{
							u32 keycode = ~kcode[first_player + slot];
							bool coin_chute = false;
							if (keycode & mask)
							{
								coin_chute = true;
								if (!old_coin_chute[first_player + slot])
									coin_count[first_player + slot] += 1;
							}
							old_coin_chute[first_player + slot] = coin_chute;

							LOGJVS("%d:%d ", slot + 1 + first_player, coin_count[first_player + slot]);
							// status (2 highest bits, 0: normal), coin count MSB
							JVS_OUT((coin_count[first_player + slot] >> 8) & 0x3F);
							// coin count LSB
							JVS_OUT(coin_count[first_player + slot]);
						}
						cmdi += 2;
					}
					break;

				case 0x22:	// Read analog inputs
					{
						JVS_STATUS1();	// report byte
						u32 axis = 0;

						LOGJVS("ana ");
						if (lightgun_as_analog)
						{
							u16 x;
							u16 y;
							if (mo_x_abs < 0 || mo_x_abs > 639 || mo_y_abs < 0 || mo_y_abs > 479
									|| (kcode[first_player] & DC_BTN_RELOAD) == 0)
							{
								x = 0;
								y = 0;
							}
							else
							{
								x = mo_x_abs * 0xFFFF / 639;
								y = mo_y_abs * 0xFFFF / 479;
							}
							LOGJVS("x,y:%4x,%4x ", x, y);
							JVS_OUT(x >> 8);		// X, MSB
							JVS_OUT(x);				// X, LSB
							JVS_OUT(y >> 8);		// Y, MSB
							JVS_OUT(y);				// Y, LSB
							axis = 2;
						}

						int full_axis_count = 0;
						for (; axis < buffer_in[cmdi + 1]; axis++)
						{
							u16 axis_value;
							if (NaomiGameInputs != NULL
									&& axis < ARRAY_SIZE(NaomiGameInputs->axes)
									&& NaomiGameInputs->axes[axis].name != NULL)
							{
								const AxisDescriptor& axisDesc = NaomiGameInputs->axes[axis];
								if (axisDesc.type == Half)
								{
									if (axisDesc.axis == 4)
										axis_value = rt[first_player] << 8;
									else if (axisDesc.axis == 5)
										axis_value = lt[first_player] << 8;
									else
										axis_value = 0;
									if (axisDesc.inverted)
										axis_value = 0xff00u - axis_value;
								}
								else
								{
									axis_value =  read_analog_axis(first_player, axisDesc.axis, axisDesc.inverted);
									full_axis_count++;
								}
							}
							else
							{
								axis_value = read_analog_axis(first_player, full_axis_count, false);
								full_axis_count++;
							}
							LOGJVS("%d:%4x ", axis, axis_value);
							JVS_OUT(axis_value >> 8);
							JVS_OUT(axis_value);
						}
						cmdi += 2;
					}
					break;

				case 0x23:	// Read rotary encoders
					{
						JVS_STATUS1();	// report byte
						static s16 rotx = 0;
						static s16 roty = 0;
						rotx += mo_x_delta * 5;
						roty -= mo_y_delta * 5;
						mo_x_delta = 0;
						mo_y_delta = 0;
						LOGJVS("rotenc ");
						for (int chan = 0; chan < buffer_in[cmdi + 1]; chan++)
						{
							if (chan == 0)
							{
								LOGJVS("%d:%4x ", chan, rotx & 0xFFFF);
								JVS_OUT(rotx >> 8);	// MSB
								JVS_OUT(rotx);		// LSB
							}
							else if (chan == 1)
							{
								LOGJVS("%d:%4x ", chan, roty & 0xFFFF);
								JVS_OUT(roty >> 8);	// MSB
								JVS_OUT(roty);		// LSB
							}
							else
							{
								LOGJVS("%d:%4x ", chan, 0);
								JVS_OUT(0x00);		// MSB
								JVS_OUT(0x00);		// LSB
							}
						}
						cmdi += 2;
					}
					break;

				case 0x25:	// Read screen pos inputs
					{
						JVS_STATUS1();	// report byte
						// Channel number is jvs_request[channel][cmdi + 1]
						// specs:
						//u16 x = mo_x_abs * 0xFFFF / 639;
						//u16 y = (479 - mo_y_abs) * 0xFFFF / 479;
						// Ninja Assault:
						u32 xr = 0x19d - 0x37;
						u32 yr = 0x1fe - 0x40;
						s16 x = mo_x_abs * xr / 639 + 0x37;
						s16 y = mo_y_abs * yr / 479 + 0x40;
						if ((kcode[first_player] & DC_BTN_RELOAD) == 0)
							x = y = 0;
						LOGJVS("lightgun %4x,%4x ", x, y);
						JVS_OUT(x >> 8);		// X, MSB
						JVS_OUT(x);				// X, LSB
						JVS_OUT(y >> 8);		// Y, MSB
						JVS_OUT(y);				// Y, LSB
						cmdi += 2;
					}
					break;

				case 0x32:	// switched outputs
				case 0x33:
					LOGJVS("output(%d) %x", buffer_in[cmdi + 1], buffer_in[cmdi + 2]);
					write_digital_out(buffer_in[cmdi + 1], &buffer_in[cmdi + 2]);
					JVS_STATUS1();	// report byte
					cmdi += buffer_in[cmdi + 1] + 2;
					break;

				case 0x30:	// substract coin
					if (buffer_in[cmdi + 1] > 0 && first_player + buffer_in[cmdi + 1] - 1 < (int)ARRAY_SIZE(coin_count))
						coin_count[first_player + buffer_in[cmdi + 1] - 1] -= (buffer_in[cmdi + 2] << 8) + buffer_in[cmdi + 3];
					JVS_STATUS1();	// report byte
					cmdi += 4;
					break;

				default:
					DEBUG_LOG(MAPLE, "JVS: Unknown input type %x", buffer_in[cmdi]);
					JVS_OUT(2);			// report byte: command error
					cmdi = length_in;	// Ignore subsequent commands
					break;
				}
			}
			LOGJVS("\n");
		}
		else
		{
			INFO_LOG(MAPLE, "JVS: Unknown JVS command %x", jvs_cmd);
			JVS_OUT(2);	// Unknown command
		}
		break;
	}
	jvs_length = length - 2;

	u8 calc_crc = 0;
	for (u32 i = 1; i < length; i++)
		calc_crc = ((calc_crc + buffer_out[i]) & 0xFF);

	JVS_OUT(calc_crc);
	LOGJVS("CRC %x", calc_crc);

	return length;
}

bool jvs_io_board::maple_serialize(void **data, unsigned int *total_size)
{
	REICAST_S(node_id);
	REICAST_S(lightgun_as_analog);

	return true ;
}

bool jvs_io_board::maple_unserialize(void **data, unsigned int *total_size)
{
	REICAST_US(node_id);
	REICAST_US(lightgun_as_analog);

	return true ;
}

maple_device* maple_Create(MapleDeviceType type)
{
	maple_device* rv=0;
	switch(type)
	{
	case MDT_SegaController:
		if (settings.platform.system != DC_PLATFORM_ATOMISWAVE)
			rv = new maple_sega_controller();
		else
			rv = new maple_atomiswave_controller();
		break;

	case MDT_Microphone:
		rv=new maple_microphone();
		break;

	case MDT_SegaVMU:
		rv = new maple_sega_vmu();
		break;

	case MDT_PurupuruPack:
		rv = new maple_sega_purupuru();
		break;

	case MDT_Keyboard:
		rv = new maple_keyboard();
		break;

	case MDT_Mouse:
		rv = new maple_mouse();
		break;

	case MDT_LightGun:
		if (settings.platform.system != DC_PLATFORM_ATOMISWAVE)
			rv = new maple_lightgun();
		else
			rv = new atomiswave_lightgun();
		break;

	case MDT_NaomiJamma:
		rv = new maple_naomi_jamma();
		break;

	case MDT_TwinStick:
		rv = new maple_sega_twinstick();
		break;

	case MDT_AsciiStick:
		rv = new maple_ascii_stick();
		break;

	default:
		ERROR_LOG(MAPLE, "Invalid device type %d", type);
		die("Invalid maple device type");
		break;
	}

	return rv;
}
