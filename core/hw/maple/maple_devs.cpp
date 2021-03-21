#include "maple_devs.h"
#include "maple_cfg.h"
#include "maple_helper.h"
#include "hw/pvr/spg.h"
#include "stdclass.h"
#include "oslib/audiostream.h"
#include "cfg/option.h"

#include <zlib.h>

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

//fill in the info
void maple_device::Setup(u32 port, int playerNum)
{
	maple_port = port;
	bus_port = maple_GetPort(port);
	bus_id = maple_GetBusId(port);
	logical_port[0] = 'A' + bus_id;
	logical_port[1] = bus_port == 5 ? 'x' : '1' + bus_port;
	logical_port[2] = 0;
	player_num = playerNum == -1 ? bus_id : playerNum;
}
maple_device::~maple_device()
{
    delete config;
}

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

	MapleDeviceType get_device_type() override
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

	u32 dma(u32 cmd) override
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
	u32 get_capabilities() override {
		// byte 0: 0  0  0  0  0  0  0  0
		// byte 1: 0  0  a5 a4 a3 a2 a1 a0
		// byte 2: R2 L2 D2 U2 D  X  Y  Z
		// byte 3: R  L  D  U  St A  B  C

		return 0xff663f00;	// 6 analog axes, X Y L2/D2(?) A B C Start U D L R
	}

	u32 transform_kcode(u32 kcode) override {
		return kcode | AWAVE_TRIGGER_KEY;
	}

	u32 get_analog_axis(int index, const PlainJoystickState &pjs) override {
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
	u32 get_capabilities() override {
		// byte 0: 0  0  0  0  0  0  0  0
		// byte 1: 0  0  a5 a4 a3 a2 a1 a0
		// byte 2: R2 L2 D2 U2 D  X  Y  Z
		// byte 3: R  L  D  U  St A  B  C

		return 0xfefe0000;	// no analog axes, X Y A B D Start U/D/L/R U2/D2/L2/R2
	}

	u32 transform_kcode(u32 kcode) override {
		return kcode | 0x0101;
	}

	MapleDeviceType get_device_type() override {
		return MDT_TwinStick;
	}

	u32 get_analog_axis(int index, const PlainJoystickState &pjs) override {
		return 0x80;
	}

	const char *get_device_name() override {
		return maple_sega_twinstick_name;
	}
};


/*
	Ascii Stick (Arcade/FT Stick)
*/
struct maple_ascii_stick: maple_sega_controller
{
	u32 get_capabilities() override {
		// byte 0: 0  0  0  0  0  0  0  0
		// byte 1: 0  0  a5 a4 a3 a2 a1 a0
		// byte 2: R2 L2 D2 U2 D  X  Y  Z
		// byte 3: R  L  D  U  St A  B  C

		return 0xff070000;	// no analog axes, X Y Z A B C Start U/D/L/R
	}

	u32 transform_kcode(u32 kcode) override {
		return kcode | 0xF800;
	}

	MapleDeviceType get_device_type() override {
		return MDT_AsciiStick;
	}

	u32 get_analog_axis(int index, const PlainJoystickState &pjs) override {
		return 0x80;
	}

	const char *get_device_name() override {
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

	MapleDeviceType get_device_type() override
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

	bool serialize(void **data, unsigned int *total_size) override
	{
		maple_base::serialize(data, total_size);
		REICAST_SA(flash_data,128*1024);
		REICAST_SA(lcd_data,192);
		REICAST_SA(lcd_data_decoded,48*32);
		return true ;
	}
	bool unserialize(void **data, unsigned int *total_size, serialize_version_enum version) override
	{
		maple_base::unserialize(data, total_size, version);
		REICAST_USA(flash_data,128*1024);
		REICAST_USA(lcd_data,192);
		REICAST_USA(lcd_data_decoded,48*32);
		return true ;
	}
	void OnSetup() override
	{
		memset(flash_data, 0, sizeof(flash_data));
		memset(lcd_data, 0, sizeof(lcd_data));
		char tempy[512];
		sprintf(tempy, "vmu_save_%s.bin", logical_port);
		// VMU saves used to be stored in .reicast, not in .reicast/data
		std::string apath = get_writable_config_path(tempy);
		if (!file_exists(apath))
			apath = get_writable_data_path(tempy);

		file = nowide::fopen(apath.c_str(), "rb+");
		if (!file)
		{
			INFO_LOG(MAPLE, "Unable to open VMU save file \"%s\", creating new file", apath.c_str());
			file = nowide::fopen(apath.c_str(), "wb");
			if (file) {
				if (!init_emptyvmu())
					WARN_LOG(MAPLE, "Failed to initialize an empty VMU, you should reformat it using the BIOS");

				std::fwrite(flash_data, sizeof(flash_data), 1, file);
				std::fseek(file, 0, SEEK_SET);
			}
			else
			{
				ERROR_LOG(MAPLE, "Failed to create VMU save file \"%s\"", apath.c_str());
			}
		}

		if (file != nullptr)
			std::fread(flash_data, 1, sizeof(flash_data), file);

		u8 sum = 0;
		for (u32 i = 0; i < sizeof(flash_data); i++)
			sum |= flash_data[i];

		if (sum == 0) {
			// This means the existing VMU file is completely empty and needs to be recreated

			if (init_emptyvmu())
			{
				if (file != nullptr)
				{
					std::fwrite(flash_data, sizeof(flash_data), 1, file);
					std::fseek(file, 0, SEEK_SET);
				}
			}
			else
			{
				WARN_LOG(MAPLE, "Failed to initialize an empty VMU, you should reformat it using the BIOS");
			}
		}

	}
	~maple_sega_vmu() override
	{
		if (file) std::fclose(file);
	}
	u32 dma(u32 cmd) override
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
							std::fseek(file,write_adr,SEEK_SET);
							std::fwrite(&flash_data[write_adr],1,write_len,file);
							std::fflush(file);
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

						return  MDRS_DeviceReply;
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

	~maple_microphone() override
	{
		if (sampling)
			StopAudioRecording();
	}
	MapleDeviceType get_device_type() override
	{
		return MDT_Microphone;
	}

	bool serialize(void **data, unsigned int *total_size) override
	{
		maple_base::serialize(data, total_size);
		REICAST_S(gain);
		REICAST_S(sampling);
		REICAST_S(eight_khz);
		REICAST_SKIP(480 - sizeof(u32) - sizeof(bool) * 2);
		return true;
	}
	bool unserialize(void **data, unsigned int *total_size, serialize_version_enum version) override
	{
		if (sampling)
			StopAudioRecording();
		maple_base::unserialize(data, total_size, version);
		REICAST_US(gain);
		REICAST_US(sampling);
		REICAST_US(eight_khz);
		REICAST_SKIP(480 - sizeof(u32) - sizeof(bool) * 2);
		if (sampling)
			StartAudioRecording(eight_khz);
		return true;
	}
	void OnSetup() override
	{
		gain = 0xf;
		sampling = false;
		eight_khz = false;
	}

	u32 dma(u32 cmd) override
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

	MapleDeviceType get_device_type() override
	{
		return MDT_PurupuruPack;
	}

   bool serialize(void **data, unsigned int *total_size) override
   {
	  maple_base::serialize(data, total_size);
      REICAST_S(AST);
      REICAST_S(AST_ms);
      REICAST_S(VIBSET);
      return true ;
   }
   bool unserialize(void **data, unsigned int *total_size, serialize_version_enum version) override
   {
	  maple_base::unserialize(data, total_size, version);
      REICAST_US(AST);
      REICAST_US(AST_ms);
      REICAST_US(VIBSET);
      return true ;
   }
	u32 dma(u32 cmd) override
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
	MapleDeviceType get_device_type() override
	{
		return MDT_Keyboard;
	}

	u32 dma(u32 cmd) override
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
			w8((u8)settings.input.keyboardLangId);
			switch (settings.input.keyboardLangId)
			{
			case KeyboardLayout::JP:
				w8(2);	// 92 keys
				break;
			case KeyboardLayout::US:
				w8(5);	// 104 keys
				break;
			default:
				w8(6);	// 105 keys
				break;
			}
			w8(0);
			w8(0x80);	// keyboard-controlled LEDs

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
u32 mo_buttons[4] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
// Relative mouse coordinates [-512:511]
f32 mo_x_delta[4];
f32 mo_y_delta[4];
f32 mo_wheel_delta[4];
// Absolute mouse coordinates
// Range [0:639] [0:479]
// but may be outside this range if the pointer is offscreen or outside the 4:3 window.
s32 mo_x_abs[4];
s32 mo_y_abs[4];
// previous mouse coordinates for relative motion
s32 mo_x_prev[4] = { -1, -1, -1, -1 };
s32 mo_y_prev[4] = { -1, -1, -1, -1 };
// physical mouse coordinates (relative to window/screen)
s32 mo_x_phy;
s32 mo_y_phy;
// last known screen/window size
static s32 mo_width;
static s32 mo_height;

struct maple_mouse : maple_base
{
	MapleDeviceType get_device_type() override
	{
		return MDT_Mouse;
	}

	static u16 mo_cvt(int delta)
	{
		return (u16)std::min(0x3FF, std::max(0, delta + 0x200));
	}

	u32 dma(u32 cmd) override
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
			{
				u32 buttons;
				int x, y, wheel;
				config->GetMouseInput(buttons, x, y, wheel);

				w32(MFID_9_Mouse);
				//struct data
				//int32 buttons       ; digital buttons bitfield (little endian)
				w32(buttons);
				//int16 axis1         ; horizontal movement (0-$3FF) (little endian)
				w16(mo_cvt(x));
				//int16 axis2         ; vertical movement (0-$3FF) (little endian)
				w16(mo_cvt(y));
				//int16 axis3         ; mouse wheel movement (0-$3FF) (little endian)
				w16(mo_cvt(wheel));
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
			}

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

	MapleDeviceType get_device_type() override
	{
		return MDT_LightGun;
	}

	u32 dma(u32 cmd) override
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

	bool get_lightgun_pos() override
	{
		PlainJoystickState pjs;
		config->GetInput(&pjs);
		int x, y;
		config->GetAbsCoordinates(x, y);

		if ((pjs.kcode & DC_BTN_RELOAD) == 0)
			read_lightgun_position(-1, -1);
		else
			read_lightgun_position(x, y);
		return true;
	}
};

struct atomiswave_lightgun : maple_lightgun
{
	u32 transform_kcode(u32 kcode) override {
		// No need for reload on AW
		return (kcode & AWAVE_TRIGGER_KEY) == 0 ? ~AWAVE_BTN0_KEY : ~0;
	}
};

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

static void screenToNative(int& x, int& y, int width, int height)
{
	float fx, fy;
	if (!config::Rotate90)
	{
		float scale = 480.f / height;
		fy = y * scale;
		scale /= config::ScreenStretching / 100.f;
		fx = (x - (width - 640.f / scale) / 2.f) * scale;
	}
	else
	{
		float scale = 640.f / width;
		fx = x * scale;
		scale /= config::ScreenStretching / 100.f;
		fy = (y - (height - 480.f / scale) / 2.f) * scale;
	}
	x = (int)std::round(fx);
	y = (int)std::round(fy);
}

void SetMousePosition(int x, int y, int width, int height, u32 mouseId)
{
	if (mouseId == 0)
	{
		mo_x_phy = x;
		mo_y_phy = y;
	}
	mo_width = width;
	mo_height = height;

	if (config::Rotate90)
	{
		int t = y;
		y = x;
		x = height - 1 - t;
		std::swap(width, height);
	}
	screenToNative(x, y, width, height);
	mo_x_abs[mouseId] = x;
	mo_y_abs[mouseId] = y;

	if (mo_x_prev[mouseId] != -1)
	{
		mo_x_delta[mouseId] += (f32)(x - mo_x_prev[mouseId]) * config::MouseSensitivity / 100.f;
		mo_y_delta[mouseId] += (f32)(y - mo_y_prev[mouseId]) * config::MouseSensitivity / 100.f;
	}
	mo_x_prev[mouseId] = x;
	mo_y_prev[mouseId] = y;
}

void SetRelativeMousePosition(int xrel, int yrel, u32 mouseId)
{
	int width = mo_width;
	int height = mo_height;
	if (config::Rotate90)
	{
		std::swap(xrel, yrel);
		xrel = -xrel;
		std::swap(width, height);
	}
	float dx = (float)xrel * config::MouseSensitivity / 100.f;
	float dy = (float)yrel * config::MouseSensitivity / 100.f;
	mo_x_delta[mouseId] += dx;
	mo_y_delta[mouseId] += dy;
	int minX = -width / 32;
	int minY = -height / 32;
	int maxX = width + width / 32;
	int maxY = height + height / 32;
	screenToNative(minX, minY, width, height);
	screenToNative(maxX, maxY, width, height);
	mo_x_abs[mouseId] += (int)std::round(dx);
	mo_y_abs[mouseId] += (int)std::round(dy);
	mo_x_abs[mouseId] = std::min(std::max(mo_x_abs[mouseId], minX), maxX);
	mo_y_abs[mouseId] = std::min(std::max(mo_y_abs[mouseId], minY), maxY);
}
