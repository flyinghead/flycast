#include "maple_devs.h"
#include "maple_cfg.h"
#include "maple_helper.h"
#include "maple_if.h"
#include "hw/pvr/spg.h"
#include "audio/audiostream.h"
#include "oslib/oslib.h"
#include "oslib/storage.h"
#include "hw/aica/sgc_if.h"
#include "cfg/option.h"
#include <zlib.h>
#include <cerrno>
#include <ctime>
#include <thread>

const char* maple_sega_controller_name = "Dreamcast Controller";
const char* maple_sega_vmu_name        = "Visual Memory";
const char* maple_sega_kbd_name        = "Emulated Dreamcast Keyboard";
const char* maple_sega_mouse_name      = "Emulated Dreamcast Mouse";
const char* maple_sega_dreameye_name_1 = "Dreamcast Camera Flash  Device";
const char* maple_sega_dreameye_name_2 = "Dreamcast Camera Flash LDevice";
const char* maple_sega_mic_name        = "MicDevice for Dreameye";
const char* maple_sega_purupuru_name   = "Puru Puru Pack";
const char* maple_sega_lightgun_name   = "Dreamcast Gun";
const char* maple_sega_twinstick_name  = "Twin Stick";
const char* maple_ascii_stick_name     = "ASCII STICK";
const char* maple_maracas_controller_name   = "Maracas Controller";
const char* maple_fishing_controller_name   = "Dreamcast Fishing Controller";
const char* maple_popnmusic_controller_name = "pop'n music controller";
const char* maple_racing_controller_name    = "Racing Controller";
const char* maple_densha_controller_name    = "TAITO 001 Controller";

const char* maple_sega_brand = "Produced By or Under License From SEGA ENTERPRISES,LTD.";

//fill in the info
void maple_device::Setup(u32 bus, u32 port, int playerNum)
{
	maple_port = (bus << 6) | (1 << port);
	bus_port = port;
	bus_id = bus;
	logical_port[0] = 'A' + bus_id;
	logical_port[1] = bus_port == 5 ? 'x' : '1' + bus_port;
	logical_port[2] = 0;
	player_num = playerNum == -1 ? bus_id : playerNum;

	config = new MapleConfigMap(this);
	OnSetup();
	MapleDevices[bus][port] = shared_from_this();
}
maple_device::~maple_device()
{
    delete config;
}

static inline void mutualExclusion(u32& keycode, u32 mask)
{
	if ((keycode & mask) == 0)
		keycode |= mask;
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

	virtual u16 getButtonState(const PlainJoystickState &pjs)
	{
		u32 kcode = pjs.kcode;
		mutualExclusion(kcode, DC_DPAD_UP | DC_DPAD_DOWN);
		mutualExclusion(kcode, DC_DPAD_LEFT | DC_DPAD_RIGHT);
		return kcode | 0xF901;		// mask off DPad2, C, D and Z;
	}

	virtual u32 getAnalogAxis(int index, const PlainJoystickState &pjs)
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

	virtual u32 get_device_current(int get_max_current)
	{
		return get_max_current ? 0x01F4 : 0x01AE; // Max. 50 mA, standby: 43 mA
	}

	u32 dma(u32 cmd) override
	{
		//printf("maple_sega_controller::dma Called 0x%X;Command %d\n", bus_id, cmd);
		switch (cmd)
		{
		case MDC_DeviceRequest:
		case MDC_AllStatusReq:
			// Fixed Device Status
			// (Device ID)
			//caps
			//4
			w32(MFID_0_Input);

			//struct data
			//3*4
			w32(get_capabilities());
			w32(0);
			w32(0);

			//1	area code (Country specification)
			w8(0xFF);

			//1	direction (Connection method)
			w8(0);

			//30 (Model name)
			wstr(get_device_name(), 30);

			//60 (License)
			wstr(get_device_brand(), 60);

			//2 (Standby current consumption)
			w16(get_device_current(0));

			//2 (Maximum current consumption)
			w16(get_device_current(1));

			if (cmd == MDC_AllStatusReq)
			{
				const char *extra = "Version 1.010,1998/09/28,315-6211-AB   ,Analog Module : The 4th Edition.5/8  +DF";
				wptr(extra, strlen(extra));
				return MDRS_DeviceStatusAll;
			}
			else {
				return MDRS_DeviceStatus;
			}

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
				w16(getButtonState(pjs));

				// analog axes
				for (int axis = 0; axis < 6; axis++)
					w8(getAnalogAxis(axis, pjs));
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

	u16 getButtonState(const PlainJoystickState &pjs) override
	{
		u32 kcode = pjs.kcode;
		mutualExclusion(kcode, AWAVE_UP_KEY | AWAVE_DOWN_KEY);
		mutualExclusion(kcode, AWAVE_LEFT_KEY | AWAVE_RIGHT_KEY);
		return kcode | AWAVE_TRIGGER_KEY;
	}

	u32 getAnalogAxis(int index, const PlainJoystickState &pjs) override {
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

	u16 getButtonState(const PlainJoystickState &pjs) override
	{
		u32 kcode = pjs.kcode;
		mutualExclusion(kcode, DC_DPAD_UP | DC_DPAD_DOWN);
		mutualExclusion(kcode, DC_DPAD_LEFT | DC_DPAD_RIGHT);
		mutualExclusion(kcode, DC_DPAD2_UP | DC_DPAD2_DOWN);
		mutualExclusion(kcode, DC_DPAD2_LEFT | DC_DPAD2_RIGHT);
		return kcode | 0x0101;
	}

	MapleDeviceType get_device_type() override {
		return MDT_TwinStick;
	}

	u32 getAnalogAxis(int index, const PlainJoystickState &pjs) override {
		return 0x80;
	}

	const char *get_device_name() override {
		return maple_sega_twinstick_name;
	}

	u32 get_device_current(int get_max_current) override {
		return get_max_current ? 0x012C : 0x00DC; // Max. 30 mA, standby: 22 mA
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

	u16 getButtonState(const PlainJoystickState &pjs) override
	{
		u32 kcode = pjs.kcode;
		mutualExclusion(kcode, DC_DPAD_UP | DC_DPAD_DOWN);
		mutualExclusion(kcode, DC_DPAD_LEFT | DC_DPAD_RIGHT);
		return kcode | 0xF800;
	}

	MapleDeviceType get_device_type() override {
		return MDT_AsciiStick;
	}

	u32 getAnalogAxis(int index, const PlainJoystickState &pjs) override {
		return 0x80;
	}

	const char *get_device_name() override {
		return maple_ascii_stick_name;
	}

	u32 get_device_current(int get_max_current) override {
		return get_max_current ? 0x0172 : 0x010E; // Max. 37 mA, standby: 27 mA
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
	FILE *file = nullptr;
	u8 flash_data[128_KB];
	u8 lcd_data[192];
	u8 lcd_data_decoded[48*32];
	bool fullSaveNeeded = false;

	MapleDeviceType get_device_type() override
	{
		return MDT_SegaVMU;
	}

	void serialize(Serializer& ser) const override
	{
		maple_base::serialize(ser);
		ser << flash_data;
		ser << lcd_data;
		ser << lcd_data_decoded;
	}
	void deserialize(Deserializer& deser) override
	{
		maple_base::deserialize(deser);
		deser >> flash_data;
		deser >> lcd_data;
		deser >> lcd_data_decoded;
		for (u8 b : lcd_data)
			if (b != 0)
			{
				config->SetImage(lcd_data_decoded);
				break;
			}
		fullSaveNeeded = true;
	}

	virtual bool fullSave()
	{
		if (file == nullptr)
			return false;
		if (std::fseek(file, 0, SEEK_SET) != 0) {
			ERROR_LOG(MAPLE, "VMU %s: I/O error", logical_port);
			return false;
		}
		if (std::fwrite(flash_data, sizeof(flash_data), 1, file) != 1) {
			ERROR_LOG(MAPLE, "Failed to write the VMU %s to disk", logical_port);
			return false;
		}
		fullSaveNeeded = false;
		return true;
	}

	void initializeVmu()
	{
		INFO_LOG(MAPLE, "Initialising empty VMU %s...", logical_port);

		uLongf dec_sz = sizeof(flash_data);
		int rv = uncompress(flash_data, &dec_sz, vmu_default, sizeof(vmu_default));

		verify(rv == Z_OK);
		verify(dec_sz == sizeof(flash_data));

		fullSave();
	}

	void OnSetup() override
	{
		memset(flash_data, 0, sizeof(flash_data));
		memset(lcd_data, 0, sizeof(lcd_data));

        // Load existing vmu file if found
        std::string rpath = hostfs::getVmuPath(logical_port, false);
		// this might be a storage url
		FILE *rfile = hostfs::storage().openFile(rpath, "rb");
        if (rfile == nullptr) {
            INFO_LOG(MAPLE, "Unable to open VMU file \"%s\", creating new file", rpath.c_str());
        }
        else
        {
            if (std::fread(flash_data, sizeof(flash_data), 1, rfile) != 1)
                WARN_LOG(MAPLE, "Failed to read the VMU file \"%s\" from disk", rpath.c_str());
            std::fclose(rfile);
        }
        // Open or create the vmu file to save to
        std::string wpath = hostfs::getVmuPath(logical_port, true);
        file = nowide::fopen(wpath.c_str(), "rb+");
        if (file == nullptr)
        {
            file = nowide::fopen(wpath.c_str(), "wb+");
			if (file == nullptr) {
                ERROR_LOG(MAPLE, "Failed to create VMU save file \"%s\"", wpath.c_str());
			}
			else if (rfile != nullptr)
			{
				// VMU file is being renamed so save it fully now
				// and delete the old file
				if (fullSave())
					nowide::remove(rpath.c_str());
			}
        }

		u8 sum = 0;
		for (u32 i = 0; i < sizeof(flash_data); i++)
			sum |= flash_data[i];

		if (sum == 0)
			// This means the existing VMU file is completely empty and needs to be recreated
			initializeVmu();
		fullSaveNeeded = false;
	}

	~maple_sega_vmu() override
	{
		if (file != nullptr)
			std::fclose(file);
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

			if (cmd == MDC_AllStatusReq)
			{
				const char *extra = "Version 1.005,1999/04/15,315-6208-03,SEGA Visual Memory System BIOS Produced by ";
				wptr(extra, strlen(extra));
				return MDRS_DeviceStatusAll;
			}
			else {
				return MDRS_DeviceStatus;
			}

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
				u32 function = r32();
				switch (function)
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
							skip(write_len);
							return MDRE_FileError; //invalid params
						}
						rptr(&flash_data[write_adr],write_len);

						if (file != nullptr)
						{
							if (fullSaveNeeded) {
								if (!fullSave())
									return MDRE_FileError;
							}
							else if (std::fseek(file, write_adr, SEEK_SET) != 0
									|| std::fwrite(&flash_data[write_adr], write_len, 1, file) != 1)
							{
								ERROR_LOG(MAPLE, "Failed to save VMU %s: I/O error", logical_port);
								return MDRE_FileError; // I/O error
							}
						}
						else {
							WARN_LOG(MAPLE, "Failed to save VMU %s data", logical_port);
						}
						return MDRS_DeviceReply;
					}

					case MFID_2_LCD:
					{
						DEBUG_LOG(MAPLE, "VMU %s LCD write", logical_port);
						r32();	// PT, phase, block#
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
						INFO_LOG(MAPLE, "VMU: command MDCF_BlockWrite -> Unknown function %x", function);
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
						u8 alw = r8();
						u8 ald = r8();
						r16(); // Alarm 2
						INFO_LOG(MAPLE, "BEEP: %d/%d", alw, ald);
						aica::sgc::vmuBeep(alw, ald);
					}
					return MDRS_DeviceReply;

				default:
					INFO_LOG(MAPLE, "VMU: command MDCF_SetCondition -> Bad function used, returning MDRE_UnknownFunction");
					return MDRE_UnknownFunction; //bad function
				}
			}
			break;

		case MDC_DeviceReset:
			aica::sgc::vmuBeep(0, 0);
			return MDRS_DeviceReply;

		case MDC_DeviceKill:
			aica::sgc::vmuBeep(0, 0);
			return MDRS_DeviceReply;

		default:
			DEBUG_LOG(MAPLE, "Unknown MAPLE COMMAND %d", cmd);
			return MDRE_UnknownCmd;
		}
	}

	const void *getData(size_t& size) const override
	{
		size = sizeof(flash_data);
		return flash_data;
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

	void serialize(Serializer& ser) const override
	{
		maple_base::serialize(ser);
		ser << gain;
		ser << sampling;
		ser << eight_khz;
	}
	void deserialize(Deserializer& deser) override
	{
		if (sampling)
			StopAudioRecording();
		maple_base::deserialize(deser);
		deser >> gain;
		deser >> sampling;
		deser >> eight_khz;
		deser.skip(480 - sizeof(u32) - sizeof(bool) * 2, Deserializer::V23);
		if (sampling)
			StartAudioRecording(eight_khz);
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

	void serialize(Serializer& ser) const override
	{
		maple_base::serialize(ser);
		ser << AST;
		ser << AST_ms;
		ser << VIBSET;
	}
	void deserialize(Deserializer& deser) override
	{
		maple_base::deserialize(deser);
		deser >> AST;
		deser >> AST_ms;
		deser >> VIBSET;
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

			if (cmd == MDC_AllStatusReq)
			{
				const char *extra = "Version 1.000,1998/11/10,315-6211-AH   ,Vibration Motor:1,Fm:4 - 30Hz,Pow:7     ";
				wptr(extra, strlen(extra));
				return MDRS_DeviceStatusAll;
			}
			else {
				return MDRS_DeviceStatus;
			}

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
			{
				u8 shift;
				u8 keys[6];
				config->GetKeyboardInput(shift, keys);

				w32(MFID_6_Keyboard);
				//struct data
				//int8 shift          ; shift keys pressed (bitmask)	//1
				w8(shift);
				//int8 led            ; leds currently lit			//1
				w8(0);
				//int8 key[6]         ; normal keys pressed			//6
				for (std::size_t i = 0; i < std::size(keys); i++)
					w8(keys[i]);
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
			w16(0x0190);	// 40 mA

			// Maximum current consumption (2)
			w16(0x01f4);	// 50 mA

			return cmd == MDC_DeviceRequest ? MDRS_DeviceStatus : MDRS_DeviceStatusAll;

		case MDCF_GetCondition:
			{
				u8 buttons;
				int x, y, wheel;
				config->GetMouseInput(buttons, x, y, wheel);

				w32(MFID_9_Mouse);
				// buttons (RLDUSABC, where A is left btn, B is right, and S is middle/scrollwheel)
				w8(buttons);
				// options
				w8(0);
				// axes overflow
				w8(0);
				// reserved
				w8(0);
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
	virtual u32 transform_kcode(u32 kcode)
	{
		mutualExclusion(kcode, DC_DPAD_UP | DC_DPAD_DOWN);
		mutualExclusion(kcode, DC_DPAD_LEFT | DC_DPAD_RIGHT);
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

			//6 analog (not used)
			w16(0);
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
		mutualExclusion(kcode, AWAVE_UP_KEY | AWAVE_DOWN_KEY);
		mutualExclusion(kcode, AWAVE_LEFT_KEY | AWAVE_RIGHT_KEY);
		// No need for reload on AW
		return (kcode & AWAVE_TRIGGER_KEY) == 0 ? ~AWAVE_BTN0_KEY : ~0;
	}
};

struct maple_maracas_controller: maple_sega_controller
{
	u32 get_capabilities() override {
		// byte 0: 0  0  0  0  0  0  0  0
		// byte 1: 0  0  a5 a4 a3 a2 a1 a0
		// byte 2: R2 L2 D2 U2 D  X  Y  Z
		// byte 3: R  L  D  U  St A  B  C

		return 0x0f093c00;	// 4 analog axes (2-5) A B C D Z Start
	}

	u16 getButtonState(const PlainJoystickState &pjs) override {
		return pjs.kcode | 0xf6f0;		// mask off DPad2, X, Y, DPad;
	}

	MapleDeviceType get_device_type() override {
		return MDT_MaracasController;
	}

	u32 getAnalogAxis(int index, const PlainJoystickState &pjs) override {
		if (index < 2 || index > 5)
			return 0;
		return pjs.joy[index -2];
		/* // This should be tested with real maracas to see if it is worth implementing or not
		u8 maracas_saturation_reduction = 2;
		s32 axis_val = (pjs.joy[index -2] - 0x80) / maracas_saturation_reduction + 0x80;
		if      (axis_val <    0) axis_val = 0;
		else if (axis_val > 0xff) axis_val = 0xFF;
		return axis_val; */
	}

	const char *get_device_name() override {
		return maple_maracas_controller_name;
	}

	u32 get_device_current(int get_max_current) override {
		return get_max_current ? 0x0546 : 0x044C; // Max. 130 mA, standby: 100 mA
	}
};

struct maple_fishing_controller: maple_sega_controller
{
	u32 analogToDPad = ~0;

	u32 get_capabilities() override {
		// byte 0: 0  0  0  0  0  0  0  0
		// byte 1: 0  0  a5 a4 a3 a2 a1 a0
		// byte 2: R2 L2 D2 U2 D  X  Y  Z
		// byte 3: R  L  D  U  St A  B  C

		return 0x0fe063f00;	// Ra,La,Da,Ua,A,B,X,Y,Start,A1,A2,A3,A4,A5,A6
	}

	u16 getButtonState(const PlainJoystickState &pjs) override
	{
		// Analog to DPad handling
		if (pjs.joy[PJAI_X1] < 0x30) {
			analogToDPad &= ~DC_DPAD_LEFT;
			analogToDPad |= DC_DPAD_RIGHT;
		}
		else if (pjs.joy[PJAI_X1] > 0xd0) {
			analogToDPad &= ~DC_DPAD_RIGHT;
			analogToDPad |= DC_DPAD_LEFT;
		}
		else
		{
			if (pjs.joy[PJAI_X1] >= 0x40)
				analogToDPad |= DC_DPAD_LEFT;
			if (pjs.joy[PJAI_X1] <= 0xc0)
				analogToDPad |= DC_DPAD_RIGHT;
		}
		if (pjs.joy[PJAI_Y1] < 0x30) {
			analogToDPad &= ~DC_DPAD_UP;
			analogToDPad |= DC_DPAD_DOWN;
		}
		else if (pjs.joy[PJAI_Y1] > 0xd0) {
			analogToDPad &= ~DC_DPAD_DOWN;
			analogToDPad |= DC_DPAD_UP;
		}
		else
		{
			if (pjs.joy[PJAI_Y1] >= 0x40)
				analogToDPad |= DC_DPAD_UP;
			if (pjs.joy[PJAI_Y1] <= 0xc0)
				analogToDPad |= DC_DPAD_DOWN;
		}
		u32 kcode = pjs.kcode & analogToDPad;
		mutualExclusion(kcode, DC_DPAD_UP   | DC_DPAD_DOWN);
		mutualExclusion(kcode, DC_DPAD_LEFT | DC_DPAD_RIGHT);
		return kcode | 0xf901;		// mask off DPad2, D, Z, C;
	}

	MapleDeviceType get_device_type() override {
		return MDT_FishingController;
	}

	u32 getAnalogAxis(int index, const PlainJoystickState &pjs) override
	{
		// In the XYZ axes, acceleration sensor outputs 80 ± 8H (home position)
		//   in the static state (± 0G), F0h or greater for maximum force (+10G)
		//   in the positive direction and 11h or less
		//   for the maximum force (-10G) applied in the negative direction
		// From the perspective of the player operating the controller:
		//   X: Right is positive, left is negative
		//   Y: Down is positive, up is negative
		//   Z: Forward is positive, backward is negative
		switch (index)
		{
		case 0:
			return pjs.trigger[PJTI_R];		// A1: Reel handle output
		case 1:
			return pjs.joy[PJAI_X3];		// A2: acceleration sensor Z
		case 2:
			return pjs.joy[PJAI_X1];		// A3: analog stick X
		case 3:
			return pjs.joy[PJAI_Y1];		// A4: analog stick Y
		case 4:
			return pjs.joy[PJAI_X2];		// A5: acceleration sensor X
		case 5:
			return pjs.joy[PJAI_Y2];		// A6: acceleration sensor Y
		default:
			return 0x80;
		}
	}

	const char *get_device_name() override {
		return maple_fishing_controller_name;
	}

	u32 get_device_current(int get_max_current) override {
		return get_max_current ? 0x0960 : 0x0258; // Max. 240 mA, standby: 60 mA
	}
};

struct maple_popnmusic_controller: maple_sega_controller
{
	u32 get_capabilities() override {
		// byte 0: 0  0  0  0  0  0  0  0
		// byte 1: 0  0  a5 a4 a3 a2 a1 a0
		// byte 2: R2 L2 D2 U2 D  X  Y  Z
		// byte 3: R  L  D  U  St A  B  C

		return 0xff060000;	// no analog axes, X Y A B C Start U/D/L/R
	}

	u16 getButtonState(const PlainJoystickState &pjs) override {
		return pjs.kcode | 0xf100; // mask off DPad2 and Z
	}

	MapleDeviceType get_device_type() override {
		return MDT_PopnMusicController;
	}

	u32 getAnalogAxis(int index, const PlainJoystickState &pjs) override {
		if (index == 0 || index == 1)
			return 0;		// Right and left triggers
		return 0x80;
	}

	const char *get_device_name() override {
		return maple_popnmusic_controller_name;
	}

	u32 get_device_current(int get_max_current) override {
		return get_max_current ? 0x012C : 0x00AA; // Max. 30 mA, standby: 17 mA
	}
};

struct maple_racing_controller: maple_sega_controller
{
	u32 get_capabilities() override {
		// byte 0: 0  0  0  0  0  0  0  0
		// byte 1: 0  0  a5 a4 a3 a2 a1 a0
		// byte 2: R2 L2 D2 U2 D  X  Y  Z
		// byte 3: R  L  D  U  St A  B  C

		return 0xfe000700;	// Steering only: Ra,La,Da,Ua,A,B,Start,A1,A2,A3
	}

	u16 getButtonState(const PlainJoystickState &pjs) override
	{
		// Ra, La are ON when A3 threshold values (La: 40h, Ra: BEh) are exceeded
		u32 kcode = pjs.kcode;
		if (pjs.joy[PJAI_X1] < 0x40)
			kcode &= ~DC_DPAD_LEFT;
		else if (pjs.joy[PJAI_X1] > 0xBE)
			kcode &= ~DC_DPAD_RIGHT;
		mutualExclusion(kcode, DC_DPAD_UP   | DC_DPAD_DOWN);
		mutualExclusion(kcode, DC_DPAD_LEFT | DC_DPAD_RIGHT);
		return kcode | 0xff01;	// mask off DPad2, D, X, Y, Z, C
	}

	MapleDeviceType get_device_type() override {
		return MDT_RacingController;
	}

	u32 getAnalogAxis(int index, const PlainJoystickState &pjs) override
	{
		switch (index)
		{
		case 0: return pjs.trigger[PJTI_R];	 // A1: lever, 0 at rest
		case 1: return pjs.trigger[PJTI_L];	 // A2: lever, 0 at rest
		case 2: return pjs.joy[PJAI_X1];	 // A3: 0-0xff, 0x80 at rest
		default: return 0x80;				 // unused
		}
	}

	const char *get_device_name() override {
		return maple_racing_controller_name;
	}

	u32 get_device_current(int get_max_current) override {
		return get_max_current ? 0x0226 : 0x01B8; // Max. 55 mA, standby: 44 mA
	}
};

struct maple_densha_controller: maple_sega_controller
{
	u32 get_capabilities() override {
		// byte 0: 0  0  0  0  0  0  0  0
		// byte 1: 0  0  a5 a4 a3 a2 a1 a0
		// byte 2: R2 L2 D2 U2 D  X  Y  Z
		// byte 3: R  L  D  U  St A  B  C

		return 0xff0f3f00;	// Ra,La,Da,Ua A,B,C,D,X,Y,Z,Start Xa,Ya,Xb,Yb Analog levers R,L
	}

	u16 getButtonState(const PlainJoystickState &pjs) override {
		// Ra,La,Da,Ua are used together, corresponding to the brake lever.
		return pjs.kcode | 0xF000; // mask off DPad2
	}

	MapleDeviceType get_device_type() override {
		return MDT_DenshaDeGoController;
	}

	u32 getAnalogAxis(int index, const PlainJoystickState &pjs) override {
		if (index == 2 || index == 3)
			return 0;
		if (index == 0 || index == 1 || index == 4 || index == 5)
			return 0xff;
		return 0xff;
	}

	const char *get_device_name() override {
		return maple_densha_controller_name;
	}

	u32 get_device_current(int get_max_current) override {
		return get_max_current ? 0x01F4 : 0x00DC; // Max. 50 mA, standby: 22 mA
	}
};

struct FullController : maple_sega_controller
{
	u32 get_capabilities() override
	{
		// byte 0: 0  0  0  0  0  0  0  0
		// byte 1: 0  0  a5 a4 a3 a2 a1 a0
		// byte 2: R2 L2 D2 U2 D  X  Y  Z
		// byte 3: R  L  D  U  St A  B  C
		return 0xffff3f00;	// 6 axes, all buttons
	}

	u16 getButtonState(const PlainJoystickState &pjs) override
	{
		u32 kcode = pjs.kcode;
		mutualExclusion(kcode, DC_DPAD_UP | DC_DPAD_DOWN);
		mutualExclusion(kcode, DC_DPAD_LEFT | DC_DPAD_RIGHT);
		mutualExclusion(kcode, DC_DPAD2_UP | DC_DPAD2_DOWN);
		mutualExclusion(kcode, DC_DPAD2_LEFT | DC_DPAD2_RIGHT);
		return kcode;
	}

	u32 getAnalogAxis(int index, const PlainJoystickState &pjs) override
	{
		if (index == 4 || index == 5)
		{
			// Limit the magnitude of the analog axes to 128
			s8 xaxis = pjs.joy[PJAI_X2] - 128;
			s8 yaxis = pjs.joy[PJAI_Y2] - 128;
			limit_joystick_magnitude<128>(xaxis, yaxis);
			if (index == 4)
				return xaxis + 128;
			else
				return yaxis + 128;
		}
		return maple_sega_controller::getAnalogAxis(index, pjs);
	}

	const char *get_device_name() override {
		return "Dreamcast Controller XL";
	}

	MapleDeviceType get_device_type() override {
		return MDT_SegaControllerXL;
	}
};

// Emulates a 838-14245-92 maple to RS232 converter
// wired to a 838-14243 RFID reader/writer (apparently Saxa HW210)
struct RFIDReaderWriter : maple_base
{
	u32 getStatus() const
	{
		// b0: !card switch
		// b1: state=4	errors?
		// b2: state=5
		// b3: state=6
		// b4: state=7
		// b5: state=8
		// b6: card lock
		// when 0x40 trying to read the card
		u32 status = 1;
		if (cardInserted)
			status &= ~1;
		if (cardLocked)
			status |= 0x40;
		return status;
	}

	// Surprisingly recipient and sender aren't swapped in the response so we override RawDma for this reason
	// vf4tuned and mushiking do care
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
			reci |= maple_GetAttachedDevices(bus_id);

		verify(u8(outlen / 4) * 4 == outlen);
		buffer_out[0] = (resp << 0 ) | (reci << 8) | (send << 16) | ((outlen / 4) << 24);

		return outlen + 4;
	}

	u32 dma(u32 cmd) override
	{
		switch (cmd)
		{
		case MDC_DeviceRequest:
		case MDC_AllStatusReq:
			// custom function
			w32(0x00100000);
			// function flags
			w32(0);
			w32(0);
			w32(0);
			//1	area code
			w8(0xff);				// FF: Worldwide, 01: North America
			//1	direction
			w8(0);
			// Product name (totally made up)
			wstr("MAPLE/232C CONVERT BD", 30);

			// License (60)
			wstr(maple_sega_brand, 60);

			// Low-consumption standby current (2)
			w16(0x0069);	// 10.5 mA

			// Maximum current consumption (2)
			w16(0x0120);	// 28.8 mA

			return cmd == MDC_DeviceRequest ? MDRS_DeviceStatus : MDRS_DeviceStatusAll;

		case MDCF_GetCondition:
			w32(0x00100000); // custom function

			return MDRS_DataTransfer;

		case MDC_DeviceReset:
		case MDC_DeviceKill:
			return MDRS_DeviceReply;

		// 90	get status
		//
		// read test:
		// d0	?
		// 91	get last cmd status?
		// a0	?
		// 91
		// a1	read md5 in data
		//			or data itself if after D4 xx xx xx xx
		// d4	in=d2 03 aa db
		// 91
		//
		// d9	lock
		// da	unlock
		//
		// write test:
		// D0
		// 91
		// B1 05 06 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 (28 bytes)
		// 91
		// B1 0b 06 00 00 00 00 c6 41 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 (28 bytes)
		// 91
		// B1 11 06 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 (28 bytes)
		// 91
		// B1 17 06 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 (28 bytes)
		// 91
		// B1 1d 03 00 00 00 00 00 00 00 00 00 00 00 00 00 00 (16 bytes)
		// 91
		// C1: 00 00 00 00
		// 91

		case 0xD0:
			d4Seen = false;
			[[fallthrough]];
		case 0x90:
		case 0x91:
		case 0xA0:
		case 0xD4:
		case 0xC1:
			w32(getStatus());
			if (cmd == 0xd4)
				d4Seen = true;
			return (MapleDeviceRV)0xfe;

		case 0xA1:	// read card data
			DEBUG_LOG(MAPLE, "RFID card read (data? %d)", d4Seen);
			w32(getStatus());
			if (!d4Seen)
				// serial0 and serial1 only
				wptr(cardData, 8);
			else
				wptr(cardData, sizeof(cardData));
			return (MapleDeviceRV)0xfe;

		case 0xD9:	// lock card
			cardLocked = true;
			w32(getStatus());
			INFO_LOG(MAPLE, "RFID card %d locked", player_num);
			return (MapleDeviceRV)0xfe;

		case 0xDA:	// unlock card
			cardLocked = false;
			cardInserted = false;
			w32(getStatus());
			NOTICE_LOG(MAPLE, "RFID card %d unlocked", player_num);
			os_notify("Card ejected", 2000);
			return (MapleDeviceRV)0xfe;

		case 0xB1:	// write to card
			{
				w32(getStatus());
				u32 offset = r8() * 4;
				size_t size = r8() * 4;
				skip(2);
				DEBUG_LOG(MAPLE, "RFID card write: offset 0x%x len %d", offset, (int)size);
				rptr(cardData + offset, std::min(size, sizeof(cardData) - offset));
				saveCard();
				return (MapleDeviceRV)0xfe;
			}

		case 0xD1:	// decrement counter
			{
				int counter = r8();
				switch (counter) {
				case 0x03:
					counter = 0;
					break;
				case 0x0c:
					counter = 1;
					break;
				case 0x30:
					counter = 2;
					break;
				case 0xc0:
					counter = 3;
					break;
				default:
					WARN_LOG(MAPLE, "Unknown counter selector %x", counter);
					counter = 0;
					break;
				}
				DEBUG_LOG(MAPLE, "RFID decrement %d", counter);
				cardData[19 - counter]--;
				saveCard();
				w32(getStatus());
				return (MapleDeviceRV)0xfe;
			}

		default:
			INFO_LOG(MAPLE, "RFIDReaderWriter: unknown MAPLE COMMAND %d", cmd);
			return MDRE_UnknownCmd;
		}
	}

	MapleDeviceType get_device_type() override {
		return MDT_RFIDReaderWriter;
	}

	void OnSetup() override
	{
		memset(cardData, 0, sizeof(cardData));
		transientData = false;
	}

	std::string getCardPath() const
	{
		int playerNum;
		if (config::GGPOEnable && !config::ActAsServer)
			// Always load P1 card with GGPO to be consistent with P1 inputs being used
			playerNum = 1;
		else
			playerNum = player_num + 1;
		return hostfs::getArcadeFlashPath() + "-p" + std::to_string(playerNum) + ".card";
	}

	void loadCard()
	{
		if (transientData)
			return;
		std::string path = getCardPath();
		FILE *fp = nowide::fopen(path.c_str(), "rb");
		if (fp == nullptr)
		{
			if (settings.content.gameId.substr(0, 8) == "MKG TKOB")
			{
				constexpr u8 MUSHIKING_CHIP_DATA[128] = {
					0x12, 0x34, 0x56, 0x78, // Serial No.0
					0x31, 0x00, 0x86, 0x07, // Serial No.1
					0x00, 0x00, 0x00, 0x00, // Key
					0x04, 0xf6, 0x00, 0xAA, // Extend  Extend  Access  Mode
					0x23, 0xFF, 0xFF, 0xFF, // Counter4  Counter3  Counter2  Counter1
					0x00, 0x00, 0x00, 0x00, // User Data (first set date: day bits 0-4, month bits 5-8, year bits 9-... + 2000)
					0x00, 0x00, 0x00, 0x00, // User Data
					0x00, 0x00, 0x00, 0x00, // User Data
					0x00, 0x00, 0x00, 0x00, // User Data
					0x00, 0x00, 0x00, 0x00, // User Data
					0x00, 0x00, 0x00, 0x00, // User Data
					0x23, 0xFF, 0xFF, 0xFF, // User Data (max counters)
				};
				memcpy(cardData, MUSHIKING_CHIP_DATA, sizeof(MUSHIKING_CHIP_DATA));
				for (int i = 0; i < 8; i++)
					cardData[i] = rand() & 0xff;
				u32 mask = 0;
				if (settings.content.gameId == "MKG TKOB 2 JPN VER2.001-"			// mushik2e
						|| settings.content.gameId == "MKG TKOB 4 JPN VER2.000-")	// mushik4e
					mask = 0x40;
				cardData[4] &= ~0xc0;
				cardData[4] |= mask;

				u32 serial1 = (cardData[4] << 24) | (cardData[5] << 16) | (cardData[6] << 8) | cardData[7];
				u32 key = ~serial1;
				key = ((key >> 4) & 0x0f0f0f0f)
					  | ((key << 4) & 0xf0f0f0f0);
				cardData[8] = key >> 24;
				cardData[9] = key >> 16;
				cardData[10] = key >> 8;
				cardData[11] = key;
			}
			else
			{
				constexpr u8 VF4_CARD_DATA[128] = {
						0x10,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   4,0x6c,   0,   0,
						   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
						   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
						   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
						   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
						   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
						   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
						   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,0xff
				};
				memcpy(cardData, VF4_CARD_DATA, sizeof(VF4_CARD_DATA));
				// Generate random bytes used by vf4 vanilla to make the card id
				srand(time(0));
				cardData[2] = rand() & 0xff;
				cardData[4] = rand() & 0xff;
				cardData[5] = rand() & 0xff;
				cardData[6] = rand() & 0xff;
				cardData[7] = rand() & 0xff;
			}
			INFO_LOG(NAOMI, "Card P%d initialized", player_num + 1);
		}
		else
		{
			INFO_LOG(NAOMI, "Loading card file from %s", path.c_str());
			if (fread(cardData, 1, sizeof(cardData), fp) != sizeof(cardData))
				WARN_LOG(NAOMI, "Truncated or empty card file: %s" ,path.c_str());
			fclose(fp);
		}
	}

	void saveCard() const
	{
		if (transientData)
			return;
		std::string path = getCardPath();
		FILE *fp = nowide::fopen(path.c_str(), "wb");
		if (fp == nullptr)
		{
			WARN_LOG(NAOMI, "Can't create card file %s: errno %d", path.c_str(), errno);
			return;
		}
		INFO_LOG(NAOMI, "Saving card file to %s", path.c_str());
		if (fwrite(cardData, 1, sizeof(cardData), fp) != sizeof(cardData))
			WARN_LOG(NAOMI, "Truncated write to file: %s", path.c_str());
		fclose(fp);
	}

	void serialize(Serializer& ser) const override
	{
		maple_device::serialize(ser);
		ser << cardData;
		ser << d4Seen;
		ser << cardInserted;
		ser << cardLocked;
	}
	void deserialize(Deserializer& deser) override
	{
		maple_device::deserialize(deser);
		deser >> cardData;
		deser >> d4Seen;
		deser >> cardInserted;
		deser >> cardLocked;
	}

	void insertCard()
	{
		if (!cardInserted) {
			cardInserted = true;
			loadCard();
		}
		else if (!cardLocked)
		{
			cardInserted = false;
			if (!transientData)
				memset(cardData, 0, sizeof(cardData));
		}
	}

	const u8 *getCardData() {
		loadCard();
		return cardData;
	}

	void setCardData(u8 *data) {
		memcpy(cardData, data, sizeof(cardData));
		transientData = true;
	}

	u8 cardData[128];
	bool d4Seen = false;
	bool cardInserted = false;
	bool cardLocked = false;
	bool transientData = false;
};

void insertRfidCard(int playerNum)
{
	std::shared_ptr<maple_device> mapleDev = MapleDevices[1 + playerNum][5];
	if (mapleDev != nullptr && mapleDev->get_device_type() == MDT_RFIDReaderWriter)
		std::static_pointer_cast<RFIDReaderWriter>(mapleDev)->insertCard();
}

void setRfidCardData(int playerNum, u8 *data)
{
	std::shared_ptr<maple_device> mapleDev = MapleDevices[1 + playerNum][5];
	if (mapleDev != nullptr && mapleDev->get_device_type() == MDT_RFIDReaderWriter)
		std::static_pointer_cast<RFIDReaderWriter>(mapleDev)->setCardData(data);
}

const u8 *getRfidCardData(int playerNum)
{
	std::shared_ptr<maple_device> mapleDev = MapleDevices[1 + playerNum][5];
	if (mapleDev != nullptr && mapleDev->get_device_type() == MDT_RFIDReaderWriter)
		return std::static_pointer_cast<RFIDReaderWriter>(mapleDev)->getCardData();
	else
		return nullptr;
}

std::shared_ptr<maple_device> maple_Create(MapleDeviceType type)
{
	switch(type)
	{
	case MDT_SegaController:
		if (!settings.platform.isAtomiswave())
			return std::make_shared<maple_sega_controller>();
		else
			return std::make_shared<maple_atomiswave_controller>();
	case MDT_Microphone:		return std::make_shared<maple_microphone>();
	case MDT_SegaVMU:			return std::make_shared<maple_sega_vmu>();
	case MDT_PurupuruPack:		return std::make_shared<maple_sega_purupuru>();
	case MDT_Keyboard:			return std::make_shared<maple_keyboard>();
	case MDT_Mouse:				return std::make_shared<maple_mouse>();
	case MDT_LightGun:
		if (!settings.platform.isAtomiswave())
			return std::make_shared<maple_lightgun>();
		else
			return std::make_shared<atomiswave_lightgun>();
	case MDT_NaomiJamma:		return std::make_shared<maple_naomi_jamma>();
	case MDT_TwinStick:			return std::make_shared<maple_sega_twinstick>();
	case MDT_AsciiStick:		return std::make_shared<maple_ascii_stick>();
	case MDT_MaracasController:	return std::make_shared<maple_maracas_controller>();
	case MDT_FishingController:	return std::make_shared<maple_fishing_controller>();
	case MDT_PopnMusicController:	return std::make_shared<maple_popnmusic_controller>();
	case MDT_RacingController:	return std::make_shared<maple_racing_controller>();
	case MDT_DenshaDeGoController:	return std::make_shared<maple_densha_controller>();
	case MDT_SegaControllerXL:	return std::make_shared<FullController>();
	case MDT_RFIDReaderWriter:	return std::make_shared<RFIDReaderWriter>();

	default:
		ERROR_LOG(MAPLE, "Invalid device type %d", type);
		die("Invalid maple device type");
		break;
	}
	return nullptr;
}

#if (defined(_WIN32) || defined(__linux__) || (defined(__APPLE__) && defined(TARGET_OS_MAC))) && !defined(TARGET_UWP) && defined(USE_SDL) && !defined(LIBRETRO)
#include "sdl/dreamconn.h"
#include <list>
#include <memory>

struct DreamConnVmu : public maple_sega_vmu
{
	std::shared_ptr<DreamConn> dreamconn;
	bool useRealVmu = false;
	bool readIn = true;

	DreamConnVmu(std::shared_ptr<DreamConn> dreamconn) : dreamconn(dreamconn) {
	}

private:
	std::atomic<bool> loadingComplete{ false };
	std::thread loadingThread;

	void loadBlocksAsync() {
		for (u32 block = 0; block < 256; ++block) {
			// Try up to 4 times to read
			for (u32 i = 0; i < 4; ++i) {
				MapleMsg msg;
				msg.command = 0x0B;
				msg.destAP = 1;
				msg.originAP = 0;
				msg.size = 2;
				msg.data[0] = 0;
				msg.data[1] = 0;
				msg.data[2] = 0;
				msg.data[3] = 0x02;
				msg.data[4] = 0;
				msg.data[5] = 0;
				msg.data[6] = 0;
				msg.data[7] = block;

				dreamconn->send(msg);

				if (dreamconn->receive(msg) && msg.size == 130) {
					// Something read!
					memcpy(&flash_data[block * 512], &msg.data[8], 4 * 128);
					break;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			}
		}
		loadingComplete = true;
	}

public:
	void OnSetup() override
	{
		maple_sega_vmu::OnSetup();

		if (readIn)
		{
			readIn = false;
			// Start background loading
			loadingThread = std::thread(&DreamConnVmu::loadBlocksAsync, this);
			loadingThread.detach();  // Let it run independently
		}
		else
		{
			memset(lcd_data, 0, sizeof(lcd_data));
		}
	}

	// Add this to check loading status
	bool isLoadingComplete() const {
		return loadingComplete;
	}

	// Add destructor to ensure clean shutdown
	~DreamConnVmu() {
		if (loadingThread.joinable())
			loadingThread.join();
	}

	bool fullSave() override
	{
		if (useRealVmu)
		{
			// do nothing
			DEBUG_LOG(MAPLE, "Not saving because this is a real vmu");
			return true;
		}
		else
		{
			return maple_sega_vmu::fullSave();
		}
	}

	u32 dma(u32 cmd) override
	{
		if (dma_count_in >= 4)
		{
			const u32 functionId = *(u32*)dma_buffer_in;
			const MapleMsg* msg = reinterpret_cast<const MapleMsg*>(dma_buffer_in - 4);

			if (functionId == MFID_1_Storage)
			{
				switch (cmd)
				{
				case MDCF_GetMediaInfo:
					DEBUG_LOG(MAPLE, "VMU GetMediaInfo request");
					dreamconn->send(*msg);
					break;

				case MDCF_GetLastError:
					NOTICE_LOG(MAPLE, "VMU GetLastError request");
					dreamconn->send(*msg);
					// Need to slow down writes so that flash has a chance to write
					std::this_thread::sleep_for(std::chrono::milliseconds(30));
					break;

				case MDCF_BlockWrite:
				{
					u32 bph = *(u32*)(dma_buffer_in + 4);
					u32 Block = (SWAP32(bph)) & 0xffff;
					u32 Phase = ((SWAP32(bph)) >> 16) & 0xff;
					u32 write_adr = Block * 512 + Phase * (512 / 4);
					u32 write_len = r_count();

					NOTICE_LOG(MAPLE, "VMU mirroring write - Block:%d Phase:%d Addr:%x Len:%d",
						Block, Phase, write_adr, write_len);

					// Send exact same DreamConnVmu::DreamConnVmudata to physical VMU
					dreamconn->send(*msg);

					// Need to slow down writes so that flash has a chance to write
					std::this_thread::sleep_for(std::chrono::milliseconds(30));

					break;
				}

				// case MDCF_BlockRead:
				{
					DEBUG_LOG(MAPLE, "VMU Storage cmd %02x", cmd);
					dreamconn->send(*msg);
					break;
				}

				case MDC_DeviceRequest:
					DEBUG_LOG(MAPLE, "VMU Device request");
					dreamconn->send(*msg);
					break;

				default:
					DEBUG_LOG(MAPLE, "VMU Storage cmd %02x", cmd);
					dreamconn->send(*msg);
					break;
				}
			}
			else if (cmd == MDCF_BlockWrite && functionId == MFID_2_LCD)
			{
				dreamconn->send(*msg);
			}
			else if (cmd == MDCF_SetCondition && functionId == MFID_3_Clock)
			{
				dreamconn->send(*msg);
			}
		}

		return maple_sega_vmu::dma(cmd);
	}

	void copyIn(std::shared_ptr<maple_sega_vmu> other)
	{
		memcpy(flash_data, other->flash_data, sizeof(flash_data));
		memcpy(lcd_data, other->lcd_data, sizeof(lcd_data));
		memcpy(lcd_data_decoded, other->lcd_data_decoded, sizeof(lcd_data_decoded));
		fullSaveNeeded = other->fullSaveNeeded;
	}

	void copyOut(std::shared_ptr<maple_sega_vmu> other)
	{
		memcpy(other->flash_data, flash_data, sizeof(other->flash_data));
		memcpy(other->lcd_data, lcd_data, sizeof(other->lcd_data));
		memcpy(other->lcd_data_decoded, lcd_data_decoded, sizeof(other->lcd_data_decoded));
		other->fullSaveNeeded = fullSaveNeeded;
	}

	void updateScreen()
	{
		MapleMsg msg;
		msg.command = MDCF_BlockWrite;
		msg.destAP = maple_port;
		msg.originAP = bus_id << 6;
		msg.size = 2 + sizeof(lcd_data) / 4;
		*(u32 *)&msg.data[0] = MFID_2_LCD;
		*(u32 *)&msg.data[4] = 0;	// PT, phase, block#
		memcpy(&msg.data[8], lcd_data, sizeof(lcd_data));
		dreamconn->send(msg);
	}
};

struct DreamConnPurupuru : public maple_sega_purupuru
{
	std::shared_ptr<DreamConn> dreamconn;

	DreamConnPurupuru(std::shared_ptr<DreamConn> dreamconn) : dreamconn(dreamconn) {
	}

	u32 dma(u32 cmd) override
	{
		if (cmd == MDCF_BlockWrite || cmd == MDCF_SetCondition) {
			const MapleMsg *msg = reinterpret_cast<const MapleMsg*>(dma_buffer_in - 4);
			dreamconn->send(*msg);
		}
		return maple_sega_purupuru::dma(cmd);
	}
};

static std::list<std::shared_ptr<DreamConnVmu>> dreamConnVmus;
static std::list<std::shared_ptr<DreamConnPurupuru>> dreamConnPurupurus;

void createDreamConnDevices(std::shared_ptr<DreamConn> dreamconn, bool gameStart)
{
	const int bus = dreamconn->getBus();
	if (dreamconn->hasVmu())
	{
		std::shared_ptr<DreamConnVmu> vmu;
		for (const std::shared_ptr<DreamConnVmu>& vmuIter : dreamConnVmus)
		{
			if (vmuIter->dreamconn.get() == dreamconn.get())
			{
				vmu = vmuIter;
				break;
			}
		}

		std::shared_ptr<maple_device> dev = MapleDevices[bus][0];
		if (gameStart || (dev != nullptr && dev->get_device_type() == MDT_SegaVMU))
		{
			bool vmuCreated = false;
			if (!vmu)
			{
				vmu = std::make_shared<DreamConnVmu>(dreamconn);
				vmuCreated = true;
			}

			vmu->Setup(bus, 0);

			if ((!gameStart || !vmuCreated) && dev) {
				// if loading a state or DreamConnVmu existed, copy data from the regular vmu and send a screen update
				vmu->copyIn(std::static_pointer_cast<maple_sega_vmu>(dev));
				if (!gameStart) {
					vmu->updateScreen();
				}
			}

			dreamConnVmus.push_back(vmu);
		}
	}
	if (dreamconn->hasRumble())
	{
		std::shared_ptr<DreamConnPurupuru> rumble;
		for (const std::shared_ptr<DreamConnPurupuru>& purupuru : dreamConnPurupurus)
		{
			if (purupuru->dreamconn.get() == dreamconn.get())
			{
				rumble = purupuru;
				break;
			}
		}

		std::shared_ptr<maple_device> dev = MapleDevices[bus][1];
		if (gameStart || (dev != nullptr && dev->get_device_type() == MDT_PurupuruPack))
		{
			if (!rumble)
			{
				rumble = std::make_shared<DreamConnPurupuru>(dreamconn);
			}
			rumble->Setup(bus, 1);
			dreamConnPurupurus.push_back(rumble);
		}
	}
}

void tearDownDreamConnDevices(std::shared_ptr<DreamConn> dreamconn)
{
	const int bus = dreamconn->getBus();
	for (std::list<std::shared_ptr<DreamConnVmu>>::const_iterator iter = dreamConnVmus.begin();
		iter != dreamConnVmus.end();)
	{
		if ((*iter)->dreamconn.get() == dreamconn.get())
		{
			std::shared_ptr<maple_device> dev = maple_Create(MDT_SegaVMU);
			dev->Setup(bus, 0);
			(*iter)->copyOut(std::static_pointer_cast<maple_sega_vmu>(dev));
			iter = dreamConnVmus.erase(iter);
			break;
		}
		else
		{
			++iter;
		}
	}
	for (std::list<std::shared_ptr<DreamConnPurupuru>>::const_iterator iter = dreamConnPurupurus.begin();
		iter != dreamConnPurupurus.end();)
	{
		if ((*iter)->dreamconn.get() == dreamconn.get())
		{
			std::shared_ptr<maple_device> dev = maple_Create(MDT_PurupuruPack);
			dev->Setup(bus, 1);
			iter = dreamConnPurupurus.erase(iter);
			break;
		}
		else
		{
			++iter;
		}
	}
}

void initDreamConnVmuAsync()
{
	static bool initStarted = false;
	if (initStarted)
		return;
	initStarted = true;

	std::thread([] {
		// Wait for BIOS to start
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		for (const auto& vmu : dreamConnVmus)
		{
			if (vmu && vmu->readIn)
			{
				vmu->OnSetup();
			}
		}
		}).detach();
}

bool isDreamConnVmuReady()
{
	for (const auto& vmu : dreamConnVmus)
	{
		if (vmu && !vmu->isLoadingComplete())
			return false;
	}
	return true;
}

#endif