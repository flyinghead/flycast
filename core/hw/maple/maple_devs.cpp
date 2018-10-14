#include "types.h"
#include "maple_if.h"
#include "maple_helper.h"
#include "maple_devs.h"
#include "maple_cfg.h"
#include "cfg/cfg.h"
#include "hw/naomi/naomi.h"
#include "hw/pvr/spg.h"
#include <time.h>

#if _ANDROID
#include <android/log.h>
#include <jni.h>
#else
#define LOGW printf
#define LOGI printf
#endif
#ifndef RELEASE
#define LOGJVS(...) LOGI(__VA_ARGS__)
#else
#define LOGJVS(...)
#endif

#include "deps/zlib/zlib.h"
#include "deps/xxhash/xxhash.h"

const char* maple_sega_controller_name = "Dreamcast Controller";
const char* maple_sega_vmu_name = "Visual Memory";
const char* maple_sega_kbd_name = "Emulated Dreamcast Keyboard";
const char* maple_sega_mouse_name = "Emulated Dreamcast Mouse";
const char* maple_sega_dreameye_name_1 = "Dreamcast Camera Flash  Devic";
const char* maple_sega_dreameye_name_2 = "Dreamcast Camera Flash LDevic";
const char* maple_sega_mic_name = "MicDevice for Dreameye";
const char* maple_sega_purupuru_name = "Puru Puru Pack";
const char* maple_sega_lightgun_name = "Dreamcast Gun";

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
	MDRE_TransmitAgain   = 0xFC, //1 word, 1 or 2?
	MDRE_FileError       = 0xFB, //1 word, bitfield
	MDRE_LCDError        = 0xFA, //1 word, bitfield
	MDRE_ARGunError      = 0xF9, //1 word, bitfield

	MDRS_JVSReply		 = 0x87, // JVS I/O
};

NaomiInputMapping Naomi_Mapping;

#define SWAP32(a) ((((a) & 0xff) << 24)  | (((a) & 0xff00) << 8) | (((a) >> 8) & 0xff00) | (((a) >> 24) & 0xff))

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
	void rptr(const void* dst,u32 len)
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
	virtual MapleDeviceType get_device_type()
	{
		return MDT_SegaController;
	}

	virtual u32 dma(u32 cmd)
	{
		//printf("maple_sega_controller::dma Called 0x%X;Command %d\n",device_instance->port,Command);
		switch (cmd)
		{
		case MDC_DeviceRequest:
			//caps
			//4
			w32(MFID_0_Input);

			//struct data
			//3*4
			w32( 0xfe060f00);
			w32( 0);
			w32( 0);

			//1	area code
			w8(0xFF);

			//1	direction
			w8(0);

			//30
			wstr(maple_sega_controller_name,30);

			//60
			wstr(maple_sega_brand,60);

			//2
			w16(0x01AE);	// 43 mA

			//2
			w16(0x01F4);	// 50 mA

			return MDRS_DeviceStatus;

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
				w16(pjs.kcode);

				//triggers
				//1 R
				w8(pjs.trigger[PJTI_R]);
				//1 L
				w8(pjs.trigger[PJTI_L]);

				//joyx
				//1
				w8(pjs.joy[PJAI_X1]);
				//joyy
				//1
				w8(pjs.joy[PJAI_Y1]);

				//not used
				//1
				w8(0x80);
				//1
				w8(0x80);
			}

			return MDRS_DataTransfer;

		default:
			//printf("UNKOWN MAPLE COMMAND %d\n",cmd);
			return MDRE_UnknownCmd;
		}
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

	virtual MapleDeviceType get_device_type()
	{
		return MDT_SegaVMU;
	}

	// creates an empty VMU
	bool init_emptyvmu()
	{
		printf("Initialising empty VMU...\n");

		uLongf dec_sz = sizeof(flash_data);
		int rv = uncompress(flash_data, &dec_sz, vmu_default, sizeof(vmu_default));

		verify(rv == Z_OK);
		verify(dec_sz == sizeof(flash_data));

		return (rv == Z_OK && dec_sz == sizeof(flash_data));
	}

	virtual bool maple_serialize(void **data, unsigned int *total_size)
	{
		REICAST_SA(flash_data,128*1024);
		REICAST_SA(lcd_data,192);
		REICAST_SA(lcd_data_decoded,48*32);
		return true ;
	}
	virtual bool maple_unserialize(void **data, unsigned int *total_size)
	{
		REICAST_USA(flash_data,128*1024);
		REICAST_USA(lcd_data,192);
		REICAST_USA(lcd_data_decoded,48*32);
		return true ;
	}
	virtual void OnSetup()
	{
		memset(flash_data, 0, sizeof(flash_data));
		memset(lcd_data, 0, sizeof(lcd_data));
		wchar tempy[512];
		sprintf(tempy, "/vmu_save_%s.bin", logical_port);
		string apath = get_writable_data_path(tempy);

		file = fopen(apath.c_str(), "rb+");
		if (!file)
		{
			printf("Unable to open VMU save file \"%s\", creating new file\n",apath.c_str());
			file = fopen(apath.c_str(), "wb");
			if (file) {
				if (!init_emptyvmu())
					printf("Failed to initialize an empty VMU, you should reformat it using the BIOS\n");

				fwrite(flash_data, sizeof(flash_data), 1, file);
				fseek(file, 0, SEEK_SET);
			}
			else
			{
				printf("Unable to create VMU!\n");
			}
		}

		if (!file)
		{
			printf("Failed to create VMU save file \"%s\"\n",apath.c_str());
		}
		else
		{
			fread(flash_data, 1, sizeof(flash_data), file);
		}

		u8 sum = 0;
		for (int i = 0; i < sizeof(flash_data); i++)
			sum |= flash_data[i];

		if (sum == 0) {
			// This means the existing VMU file is completely empty and needs to be recreated

			if (init_emptyvmu())
			{
				if (!file)
					file = fopen(apath.c_str(), "wb");

				if (file) {
					fwrite(flash_data, sizeof(flash_data), 1, file);
					fseek(file, 0, SEEK_SET);
				}
				else {
					printf("Unable to create VMU!\n");
				}
			}
			else
			{
				printf("Failed to initialize an empty VMU, you should reformat it using the BIOS\n");
			}
		}

	}
	virtual ~maple_sega_vmu()
	{
		if (file) fclose(file);
	}
	virtual u32 dma(u32 cmd)
	{
		//printf("maple_sega_vmu::dma Called for port 0x%X, Command %d\n",device_instance->port,Command);
		switch (cmd)
		{
		case MDC_DeviceRequest:
			{
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
					{
						w32(MFID_1_Storage);

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

						return MDRS_DataTransfer;//data transfer
					}
					break;

				case MFID_2_LCD:
					{
						u32 pt=r32();
						if (pt!=0)
						{
							printf("VMU: MDCF_GetMediaInfo -> bad input |%08X|, returning MDRE_UnknownCmd\n",pt);
							return MDRE_UnknownCmd;
						}
						else
						{
							w32(MFID_2_LCD);

							w8(47);             //X dots -1
							w8(31);             //Y dots -1
							w8(((1)<<4) | (0)); //1 Color, 0 contrast levels
							w8(0);              //Padding

							return MDRS_DataTransfer;
						}
					}
					break;

				default:
					printf("VMU: MDCF_GetMediaInfo -> Bad function used |%08X|, returning -2\n",function);
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
							printf("Block read : %d\n",Block);
							printf("BLOCK READ ERROR\n");
							Block&=255;
						}
						wptr(flash_data+Block*512,512);

						return MDRS_DataTransfer;//data transfer
					}
					break;

				case MFID_2_LCD:
					{
						w32(MFID_2_LCD);
						w32(r32()); // mnn ?
						wptr(flash_data,192);

						return MDRS_DataTransfer;//data transfer
					}
					break;

				case MFID_3_Clock:
					{
						if (r32()!=0)
						{
							printf("VMU: Block read: MFID_3_Clock : invalid params \n");
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

							printf("VMU: CLOCK Read-> datetime is %04d/%02d/%02d ~ %02d:%02d:%02d!\n",timebuf[0]+timebuf[1]*256,timebuf[2],timebuf[3],timebuf[4],timebuf[5],timebuf[6]);

							return MDRS_DataTransfer;//transfer reply ...
						}
					}
					break;

				default:
					printf("VMU: cmd MDCF_BlockRead -> Bad function |%08X| used, returning -2\n",function);
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
						rptr(&flash_data[write_adr],write_len);

						if (file)
						{
							fseek(file,write_adr,SEEK_SET);
							fwrite(&flash_data[write_adr],1,write_len,file);
							fflush(file);
						}
						else
						{
							printf("Failed to save VMU %s data\n",logical_port);
						}
						return MDRS_DeviceReply;//just ko
					}
					break;


					case MFID_2_LCD:
					{
						u32 wat=r32();
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
#if !defined(TARGET_PANDORA) && HOST_OS != OS_DARWIN
						push_vmu_screen(lcd_data_decoded);
#endif
#if 0
						// Update LCD window
						if (!dev->lcd.visible)
						{
							dev->lcd.visible=true;
							ShowWindow(dev->lcd.handle,SHOW_OPENNOACTIVATE);
						}


							InvalidateRect(dev->lcd.handle,NULL,FALSE);
						}

						// Logitech G series stuff start
	#ifdef _HAS_LGLCD_
						{
							lgLcdBitmap160x43x1 bmp;
							bmp.hdr.Format = LGLCD_BMP_FORMAT_160x43x1;

							const u8 white=0x00;
							const u8 black=0xFF;

							//make it all black...
							memset(bmp.pixels,black,sizeof(bmp.pixels));

							//decode from the VMU
							for(int y=0;y<32;++y)
							{
								u8 *dst=bmp.pixels+5816+((-y)*(48+112)); //ugly way to make things look right :p
								u8 *src=dev->lcd.data+6*y+5;
								for(int x=0;x<48/8;++x)
								{
									u8 val=*src;
									for(int m=0;m<8;++m)
									{
										if(val&(1<<(m)))
											*dst++=black;
										else
											*dst++=white;
									}
									--src;
								}
							}

							//Set the damned bits
							res = lgLcdUpdateBitmap(openContext.device, &bmp.hdr, LGLCD_ASYNC_UPDATE(LGLCD_PRIORITY_NORMAL));
						}
	#endif
						//Logitech G series stuff end
#endif
						return  MDRS_DeviceReply;//just ko
					}
					break;

					case MFID_3_Clock:
					{
						if (r32()!=0 || r_count()!=8)
							return MDRE_TransmitAgain;	//invalid params ...
						else
						{
							u8 timebuf[8];
							rptr(timebuf,8);
							printf("VMU: CLOCK Write-> datetime is %04d/%02d/%02d ~ %02d:%02d:%02d! Nothing set tho ...\n",timebuf[0]+timebuf[1]*256,timebuf[2],timebuf[3],timebuf[4],timebuf[5],timebuf[6]);
							return  MDRS_DeviceReply;//ok !
						}
					}
					break;

					default:
					{
						printf("VMU: command MDCF_BlockWrite -> Bad function used, returning MDRE_UnknownFunction\n");
						return  MDRE_UnknownFunction;//bad function
					}
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
						u32 bp=r32();
						if (bp)
						{
							printf("BEEP : %08X\n",bp);
						}
						return  MDRS_DeviceReply;//just ko
					}
					break;

				default:
					{
						printf("VMU: command MDCF_SetCondition -> Bad function used, returning MDRE_UnknownFunction\n");
						return MDRE_UnknownFunction;//bad function
					}
					break;
				}
			}


		default:
			//printf("Unknown MAPLE COMMAND %d\n",cmd);
			return MDRE_UnknownCmd;
		}
	}
};


struct maple_microphone: maple_base
{
	u8 micdata[SIZE_OF_MIC_DATA];

	virtual MapleDeviceType get_device_type()
	{
		return MDT_Microphone;
	}

	virtual bool maple_serialize(void **data, unsigned int *total_size)
	{
		REICAST_SA(micdata,SIZE_OF_MIC_DATA);
		return true ;
	}
	virtual bool maple_unserialize(void **data, unsigned int *total_size)
	{
		REICAST_USA(micdata,SIZE_OF_MIC_DATA);
		return true ;
	}
	virtual void OnSetup()
	{
		memset(micdata,0,sizeof(micdata));
	}

	virtual u32 dma(u32 cmd)
	{
		//printf("maple_microphone::dma Called 0x%X;Command %d\n",this->maple_port,cmd);
		//LOGD("maple_microphone::dma Called 0x%X;Command %d\n",this->maple_port,cmd);

		switch (cmd)
		{
		case MDC_DeviceRequest:
			LOGI("maple_microphone::dma MDC_DeviceRequest");
			//this was copied from the controller case with just the id and name replaced!

			//caps
			//4
			w32(MFID_4_Mic);

			//struct data
			//3*4
			w32( 0xfe060f00);
			w32( 0);
			w32( 0);

			//1	area code
			w8(0xFF);

			//1	direction
			w8(0);

			//30
			wstr(maple_sega_mic_name,30);

			//60
			wstr(maple_sega_brand,60);

			//2
			w16(0x01AE);	// 43 mA

			//2
			w16(0x01F4);	// 50 mA

			return MDRS_DeviceStatus;

		case MDCF_GetCondition:
			{
				LOGI("maple_microphone::dma MDCF_GetCondition");
				//this was copied from the controller case with just the id replaced!

				//PlainJoystickState pjs;
				//config->GetInput(&pjs);
				//caps
				//4
				w32(MFID_4_Mic);

				//state data
				//2 key code
				//w16(pjs.kcode);

				//triggers
				//1 R
				//w8(pjs.trigger[PJTI_R]);
				//1 L
				//w8(pjs.trigger[PJTI_L]);

				//joyx
				//1
				//w8(pjs.joy[PJAI_X1]);
				//joyy
				//1
				//w8(pjs.joy[PJAI_Y1]);

				//not used
				//1
				w8(0x80);
				//1
				w8(0x80);
			}

			return MDRS_DataTransfer;

		case MDC_DeviceReset:
			//uhhh do nothing?
			LOGI("maple_microphone::dma MDC_DeviceReset");
			return MDRS_DeviceReply;

		case MDCF_MICControl:
		{
			//LOGD("maple_microphone::dma handling MDCF_MICControl %d\n",cmd);
			//MONEY
			u32 function=r32();
			//LOGD("maple_microphone::dma MDCF_MICControl function (1st word) %#010x\n", function);
			//LOGD("maple_microphone::dma MDCF_MICControl words: %d\n", dma_count_in);

			switch(function)
			{
			case MFID_4_Mic:
			{
				//MAGIC HERE
				//http://dcemulation.org/phpBB/viewtopic.php?f=34&t=69600
				// <3 <3 BlueCrab <3 <3
				/*
				2nd word               What it does:
				0x0000??03          Sets the amplifier gain, ?? can be from 00 to 1F
									0x0f = default
				0x00008002          Enables recording
				0x00000001          Returns sampled data while recording is enabled
									While not enabled, returns status of the mic.
				0x00000002          Disables recording
				 *
				 */
				u32 secondword=r32();
				//LOGD("maple_microphone::dma MDCF_MICControl subcommand (2nd word) %#010x\n", subcommand);

				u32 subcommand = secondword & 0xFF; //just get last byte for now, deal with params later

				//LOGD("maple_microphone::dma MDCF_MICControl (3rd word) %#010x\n", r32());
				//LOGD("maple_microphone::dma MDCF_MICControl (4th word) %#010x\n", r32());
				switch(subcommand)
				{
				case 0x01:
				{
					//LOGI("maple_microphone::dma MDCF_MICControl someone wants some data! (2nd word) %#010x\n", secondword);

					w32(MFID_4_Mic);

					//from what i can tell this is up to spec but results in transmit again
					//w32(secondword);

					//32 bit header
					w8(0x04);//status (just the bit for recording)
					w8(0x0f);//gain (default)
					w8(0);//exp ?
#ifndef TARGET_PANDORA
					if(get_mic_data(micdata)){
						w8(240);//ct (240 samples)
						wptr(micdata, SIZE_OF_MIC_DATA);
					}else
#endif
					{
						w8(0);
					}

					return MDRS_DataTransfer;
				}
				case 0x02:
					LOGI("maple_microphone::dma MDCF_MICControl toggle recording %#010x\n",secondword);
					return MDRS_DeviceReply;
				case 0x03:
					LOGI("maple_microphone::dma MDCF_MICControl set gain %#010x\n",secondword);
					return MDRS_DeviceReply;
				case MDRE_TransmitAgain:
					LOGW("maple_microphone::dma MDCF_MICControl MDRE_TransminAgain");
					//apparently this doesnt matter
					//wptr(micdata, SIZE_OF_MIC_DATA);
					return MDRS_DeviceReply;//MDRS_DataTransfer;
				default:
					LOGW("maple_microphone::dma UNHANDLED secondword %#010x\n",secondword);
					return MDRE_UnknownFunction;
				}
			}
			default:
				LOGW("maple_microphone::dma UNHANDLED function %#010x\n",function);
				return MDRE_UnknownFunction;
			}
		}

		default:
			LOGW("maple_microphone::dma UNHANDLED MAPLE COMMAND %d\n",cmd);
			return MDRE_UnknownCmd;
		}
	}
};


struct maple_sega_purupuru : maple_base
{
	u16 AST, AST_ms;
	u32 VIBSET;

	virtual MapleDeviceType get_device_type()
	{
		return MDT_PurupuruPack;
	}

   virtual bool maple_serialize(void **data, unsigned int *total_size)
   {
      REICAST_S(AST);
      REICAST_S(AST_ms);
      REICAST_S(VIBSET);
      return true ;
   }
   virtual bool maple_unserialize(void **data, unsigned int *total_size)
   {
      REICAST_US(AST);
      REICAST_US(AST_ms);
      REICAST_US(VIBSET);
      return true ;
   }
	virtual u32 dma(u32 cmd)
	{
		switch (cmd)
		{
		case MDC_DeviceRequest:
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

			return MDRS_DeviceStatus;

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
			//Do the rumble thing!
			config->SetVibration(VIBSET);

			return MDRS_DeviceReply;

		default:
			//printf("UNKOWN MAPLE COMMAND %d\n",cmd);
			return MDRE_UnknownCmd;
		}
	}
};

u8 kb_shift; 		// shift keys pressed (bitmask)
u8 kb_led; 			// leds currently lit
u8 kb_key[6]={0};	// normal keys pressed

struct maple_keyboard : maple_base
{
	virtual MapleDeviceType get_device_type()
	{
		return MDT_Keyboard;
	}

	virtual u32 dma(u32 cmd)
	{
		switch (cmd)
		{
		case MDC_DeviceRequest:
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
			for (u32 i = 0; i < 30; i++)
			{
				w8((u8)maple_sega_kbd_name[i]);
			}

			// License (60)
			for (u32 i = 0; i < 60; i++)
			{
				w8((u8)maple_sega_brand[i]);
			}

			// Low-consumption standby current (2)
			w16(0x01AE);	// 43 mA

			// Maximum current consumption (2)
			w16(0x01F5);	// 50.1 mA

			return MDRS_DeviceStatus;

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

		default:
			printf("Keyboard: unknown MAPLE COMMAND %d\n", cmd);
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
	virtual MapleDeviceType get_device_type()
	{
		return MDT_Mouse;
	}

	static u16 mo_cvt(f32 delta)
	{
		delta+=0x200 + 0.5;
		if (delta<=0)
			delta=0;
		else if (delta>0x3FF)
			delta=0x3FF;

		return (u16) delta;
	}

	virtual u32 dma(u32 cmd)
	{
		switch (cmd)
		{
		case MDC_DeviceRequest:
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
			for (u32 i = 0; i < 30; i++)
			{
				w8((u8)maple_sega_mouse_name[i]);
			}

			// License (60)
			for (u32 i = 0; i < 60; i++)
			{
				w8((u8)maple_sega_brand[i]);
			}

			// Low-consumption standby current (2)
			w16(0x0069);	// 10.5 mA

			// Maximum current consumption (2)
			w16(0x0120);	// 28.8 mA

			return MDRS_DeviceStatus;

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

		default:
			printf("Mouse: unknown MAPLE COMMAND %d\n", cmd);
			return MDRE_UnknownCmd;
		}
	}
};

struct maple_lightgun : maple_base
{
	virtual MapleDeviceType get_device_type()
	{
		return MDT_LightGun;
	}

	virtual u32 dma(u32 cmd)
	{
		switch (cmd)
		{
		case MDC_DeviceRequest:
			//caps
			//4
			w32(MFID_7_LightGun | MFID_0_Input);

			//struct data
			//3*4
			w32(0);				// Light gun
			w32(0xFE000000);	// Controller
			w32(0);
			//1	area code
			w8(0x01);		// FF: Worldwide, 01: North America
			//1	direction
			w8(0);
			// Product name (30)
			for (u32 i = 0; i < 30; i++)
			{
				w8((u8)maple_sega_lightgun_name[i]);
			}

			// License (60)
			for (u32 i = 0; i < 60; i++)
			{
				w8((u8)maple_sega_brand[i]);
			}

			// Low-consumption standby current (2)
			w16(0x0069);	// 10.5 mA

			// Maximum current consumption (2)
			w16(0x0120);	// 28.8 mA

			return MDRS_DeviceStatus;

		case MDCF_GetCondition:
		{
			PlainJoystickState pjs;
			config->GetInput(&pjs);

			// Also use the mouse buttons
			if (!(mo_buttons & 4))	// Left button
				pjs.kcode &= ~4;	// A
			if (!(mo_buttons & 2))	// Right button
				pjs.kcode &= ~2;	// B
			if (!(mo_buttons & 8))	// Wheel button
				pjs.kcode &= ~8;	// Start

			//caps
			//4
			w32(MFID_0_Input);

			//state data
			//2 key code
			w16(pjs.kcode | 0xFF01);

			//not used
			//2
			w16(0xFFFF);

			//not used
			//4
			w32(0x80808080);
		}

		return MDRS_DataTransfer;

		default:
			printf("Light gun: unknown MAPLE COMMAND %d\n", cmd);
			return MDRE_UnknownCmd;
		}
	}

	virtual void get_lightgun_pos()
	{
		read_lightgun_position(mo_x_abs, mo_y_abs);
		// TODO If NAOMI, set some bits at 0x600284 http://64darksoft.blogspot.com/2013/10/atomiswage-to-naomi-update-4.html
	}
};

extern u16 kcode[4];
extern s8 joyx[4],joyy[4];
extern u8 rt[4], lt[4];
char EEPROM[0x100];
bool EEPROM_loaded = false;

enum NAOMI_KEYS
{
	NAOMI_SERVICE_KEY_1 = 1 << 0,
	NAOMI_TEST_KEY_1 = 1 << 1,
	NAOMI_SERVICE_KEY_2 = 1 << 2,
	NAOMI_TEST_KEY_2 = 1 << 3,

	NAOMI_START_KEY = 1 << 4,

	NAOMI_UP_KEY = 1 << 5,
	NAOMI_DOWN_KEY = 1 << 6,
	NAOMI_LEFT_KEY = 1 << 7,
	NAOMI_RIGHT_KEY = 1 << 8,

	NAOMI_BTN0_KEY = 1 << 9,
	NAOMI_BTN1_KEY = 1 << 10,
	NAOMI_BTN2_KEY = 1 << 11,
	NAOMI_BTN3_KEY = 1 << 12,
	NAOMI_BTN4_KEY = 1 << 13,
	NAOMI_BTN5_KEY = 1 << 14,
	NAOMI_COIN_KEY = 1 << 15,
};


#ifdef SAVE_EPPROM
static string get_eeprom_file_path()
{
	char image_path[512];
	cfgLoadStr("config", "image", image_path, "naomi_boot.bin");
	string eeprom_file = image_path;
	size_t lastindex = eeprom_file.find_last_of("/");
	if (lastindex != -1)
		eeprom_file = eeprom_file.substr(lastindex + 1);
	lastindex = eeprom_file.find_last_of(".");
	if (lastindex != -1)
		eeprom_file = eeprom_file.substr(0, lastindex);
	eeprom_file = eeprom_file + ".eeprom";
	return get_writable_data_path("/data/") + eeprom_file;
}
#endif

/*
 * Sega JVS I/O board
*/
bool coin_chute;
static int coin_count;

struct maple_naomi_jamma;

class jvs_io_board
{
public:
	jvs_io_board(u8 node_id, maple_naomi_jamma *parent)
	{
		this->node_id = node_id;
		this->parent = parent;
	}

	u32 handle_jvs_message(u8 *buffer_in, u32 length_in, u8 *buffer_out);
	bool maple_serialize(void **data, unsigned int *total_size);
	bool maple_unserialize(void **data, unsigned int *total_size);

	bool rotary_encoders = false;
	bool lightgun_as_analog = false;

private:
	u8 node_id = 0;
	maple_naomi_jamma *parent;
};

struct maple_naomi_jamma : maple_sega_controller
{
	const u8 ALL_NODES = 0xff;

	std::vector<jvs_io_board *> io_boards;
	bool crazy_mode = false;

	u8 jvs_repeat_request[32][256];
	u32 jvs_repeat_length[32] = { 0 };
	u8 jvs_receive_buffer[512];
	u32 jvs_receive_length = 0;

	u8 *jvs_msg_start = NULL;

	maple_naomi_jamma()
	{
		io_boards.push_back(new jvs_io_board(1, this));
//		io_boards.back()->rotary_encoders = true;
//		io_boards.push_back(new jvs_io_board(2, this));
	}

	virtual MapleDeviceType get_device_type()
	{
		return MDT_NaomiJamma;
	}

	void jvs_recv8(u8 b)
	{
		if (jvs_receive_length < sizeof(jvs_receive_buffer))
			jvs_receive_buffer[jvs_receive_length++] = b;
	}
	void jvs_recv32(u32 dw)
	{
		if (jvs_receive_length < sizeof(jvs_receive_buffer) - 3)
		{
			*((u32 *)&jvs_receive_buffer[jvs_receive_length]) = dw;
			jvs_receive_length += 4;
		}
	}

	void make_jvs_reply_buffer(u32 node_id, u32 channel)
	{
		jvs_msg_start = &jvs_receive_buffer[jvs_receive_length];
		jvs_recv8(MDRS_JVSReply);
		jvs_recv8(0x00);
		jvs_recv8(0x20);
		jvs_recv8(0x00);	// length in dword
		jvs_recv32(0xffffff16);
		jvs_recv32(0xfffeff00);
		jvs_recv32(0);
		jvs_recv32(0);

		jvs_recv8(0);
		jvs_recv8(channel);	// channel id
		bool last_node = node_id == io_boards.size();
		jvs_recv8(last_node ? 0x8E : 0x8F);	// bit 0 is sense line level. If set during F1 <n>, more I/O boards need addressing
		jvs_recv8(node_id);	// node number
		jvs_recv8(0x00);	// 0: ok, 2: timeout, 3: dest node !=0, 4: checksum failed
		jvs_recv8(0x00);	// len of following data
	}

	void finish_jvs_reply_buffer()
	{
		u32 total_len = &jvs_receive_buffer[jvs_receive_length] - jvs_msg_start;
		jvs_msg_start[25] = total_len - 26;
		jvs_receive_length = ((jvs_receive_length - 1) / 4) * 4 + 4;
		jvs_msg_start[3] = (&jvs_receive_buffer[jvs_receive_length] - jvs_msg_start) / 4 - 1;
	}

	void send_jvs_message(u32 node_id, u32 channel, u32 length, u8 *data)
	{
		if (node_id - 1 < io_boards.size())
		{
			u8 temp_buffer[256];
			u32 out_len = io_boards[node_id - 1]->handle_jvs_message(data, length, temp_buffer);
			if (out_len > 0)
			{
				make_jvs_reply_buffer(node_id, channel);
				memcpy(&jvs_receive_buffer[jvs_receive_length], temp_buffer, out_len);
				jvs_receive_length += out_len;
				finish_jvs_reply_buffer();
			}
		}
	}

	void send_jvs_messages(u32 node_id, u32 channel, bool use_repeat, u32 length, u8 *data)
	{
		u8 temp_buffer[256];
		if (data)
		{
			memcpy(temp_buffer, data, length);
		}
		if (use_repeat && jvs_repeat_length[node_id - 1] > 0)
		{
			memcpy(temp_buffer + length, jvs_repeat_request[node_id - 1], jvs_repeat_length[node_id - 1]);
			length += jvs_repeat_length[node_id - 1];
		}
		if (node_id == ALL_NODES)
		{
			jvs_receive_length = 0;
			for (int i = 0; i < io_boards.size(); i++)
				send_jvs_message(i + 1, channel, length, temp_buffer);
		}
		else
			send_jvs_message(node_id, channel, length, temp_buffer);
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
				channel = dma_buffer_in[5];
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
				if (len > 0)
				{
					printf("JVS node %d: Storing %d cmd bytes\n", node_id, len);
					memcpy(jvs_repeat_request[node_id - 1], cmd, len);
					jvs_repeat_length[node_id - 1] = len;
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
				if (jvs_receive_length)
				{
					memcpy(dma_buffer_out, jvs_receive_buffer, jvs_receive_length);
					dma_buffer_out += jvs_receive_length;
					*dma_count_out += jvs_receive_length;
					jvs_receive_length = 0;
				}
				else
				{
					w8(MDRS_JVSReply);
					w8(0x00);
					w8(0x20);
					w8(0x00);
				}
				break;

			case 0x17:	// Transmit without repeat
				send_jvs_messages(node_id, channel, false, len, cmd);
				w8(MDRS_JVSReply);
				w8(0);
				w8(0x20);
				w8(0x01);
				w8(dma_buffer_in[0] + 1);	// subcommand + 1
				w8(channel);
				w8(0);
				break;

			case 0x19:	// Transmit with repeat
			case 0x21:
				send_jvs_messages(node_id, channel, true, len, cmd);
				w8(MDRS_JVSReply);
				w8(0);
				w8(0x20);
				w8(0x01);
				w8(dma_buffer_in[0] + 1);	// subcommand + 1
				w8(channel);
				w8(0);
				w8(0);
				break;

			case 0x35:	// Receive then transmit with repeat (15 then 27)
				if (jvs_receive_length)
				{
					memcpy(dma_buffer_out, jvs_receive_buffer, jvs_receive_length);
					dma_buffer_out += jvs_receive_length;
					*dma_count_out += jvs_receive_length;
					jvs_receive_length = 0;
				}
				// FALLTHROUGH

			case 0x27:	// Transmit with repeat
				{
					u32 cmd_count = dma_buffer_in[6];
					u32 idx = 7;
					for (int i = 0; i < cmd_count; i++)
					{
						node_id = dma_buffer_in[idx];
						if (dma_buffer_in[idx + 1] == 0)
						{
							len = 0;
							cmd = NULL;
							idx += 2;
						}
						else
						{
							len = dma_buffer_in[idx + 2];
							verify(len != 0xFF);	// Need implementation
							cmd = &dma_buffer_in[idx + 3];
							idx += len + 3;
						}
						send_jvs_messages(node_id, channel, true, len, cmd);
					}

					w8(MDRS_JVSReply);
					w8(0);
					w8(0x20);
					w8(0x01);
					w8(0x26);
					w8(channel);
					w8(0);
					w8(0);
				}
				break;

			case 0x33:	// Receive then transmit with repeat (15 then 21)
				if (jvs_receive_length)
				{
					memcpy(dma_buffer_out, jvs_receive_buffer, jvs_receive_length);
					*dma_count_out += jvs_receive_length;
					jvs_receive_length = 0;
				}
				send_jvs_messages(node_id, channel, true, len, cmd);
				w8(MDRS_JVSReply);
				w8(0);
				w8(0x20);
				w8(0x01);
				w8(dma_buffer_in[0] + 1);	// subcommand + 1
				w8(channel);
				w8(0);
				w8(0);
				break;

			case 0x0B:	//EEPROM write
			{
				int address = dma_buffer_in[1];
				int size = dma_buffer_in[2];
				//printf("EEprom write %08X %08X\n",address,size);
				//printState(Command,buffer_in,buffer_in_len);
				memcpy(EEPROM + address, dma_buffer_in + 4, size);

#ifdef SAVE_EPPROM
				string eeprom_file = get_eeprom_file_path();
				FILE* f = fopen(eeprom_file.c_str(), "wb");
				if (f)
				{
					fwrite(EEPROM, 1, 0x80, f);
					fclose(f);
					printf("Saved EEPROM to %s\n", eeprom_file.c_str());
				}
				else
					printf("EEPROM SAVE FAILED to %s\n", eeprom_file.c_str());
#endif
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
#ifdef SAVE_EPPROM
				if (!EEPROM_loaded)
				{
					EEPROM_loaded = true;
					string eeprom_file = get_eeprom_file_path();
					FILE* f = fopen(eeprom_file.c_str(), "rb");
					if (f)
					{
						fread(EEPROM, 1, 0x80, f);
						fclose(f);
						printf("Loaded EEPROM from %s\n", eeprom_file.c_str());
					}
					else
						printf("EEPROM file not found at %s\n", eeprom_file.c_str());
				}
#endif
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
//CT
//				w8(0x0);
//				w8(0x0);
//				w8(0x0);
//				w8(0x0);
				break;

			default:
				printf("JVS: Unknown 0x86 sub-command %x\n", subcode);
				w8(MDRE_UnknownCmd);
				w8(0x00);
				w8(0x20);
				w8(0x00);
				break;
		}
	}

	virtual u32 RawDma(u32* buffer_in, u32 buffer_in_len, u32* buffer_out)
	{
		/*
		printf("JVS IN: ");
		u8 *p = (u8*)buffer_in;
		for (int i = 0; i < buffer_in_len; i++) printf("%02x ", *p++);
		printf("\n");
		*/

		u32 out_len = 0;
		dma_buffer_out = (u8 *)buffer_out;
		dma_count_out = &out_len;

		dma_buffer_in = (u8 *)buffer_in + 4;
		dma_count_in = buffer_in_len - 4;

		u32 cmd = *(u8*)buffer_in;
		switch (cmd)
		{
			case MDC_JVSCommand:
				handle_86_subcommand();
				break;

			case MDC_JVSUploadFirmware:
			{
				static FILE *fw_dump;
				static XXH32_state_t* state;

				if (state == NULL)
				{
					 state = XXH32_createState();
					 XXH32_reset(state, 0);
				}

				if (dma_buffer_in[1] == 0xff)
				{
					XXH32_hash_t hash = XXH32_digest(state);
					LOGJVS("JVS Firmware hash %08x\n", hash);
					if (hash == 0xEF29B145)
						crazy_mode = true;
					else
						crazy_mode = false;
					XXH32_freeState(state);
					state = NULL;

					if (fw_dump)
						fclose(fw_dump);
					fw_dump = NULL;

					return MDRS_DeviceReply;
				}
#ifdef DUMP_JVS_FW
				if (fw_dump == NULL)
				{
					char filename[128];
					for (int i = 0; ; i++)
					{
						sprintf(filename, "z80_fw_%d.bin", i);
						fw_dump = fopen(filename, "r");
						if (fw_dump == NULL)
						{
							fw_dump = fopen(filename, "w");
							printf("Saving JVS firmware to %s\n", filename);
							break;
						}
					}
				}
#endif
				int xfer_bytes;
				if (dma_buffer_in[0] == 0xff)
					xfer_bytes = 0x1C;
				else
					xfer_bytes = 0x18;
				u16 addr = (dma_buffer_in[2] << 8) + dma_buffer_in[3];
				XXH32_update(state, &dma_buffer_in[4], xfer_bytes);
#ifdef DUMP_JVS_FW
				if (fw_dump)
				{
					fseek(fw_dump, addr, SEEK_SET);
					fwrite(&dma_buffer_in[4], 1, xfer_bytes, fw_dump);
				}
#endif
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
			w8(0x00);	// CT
//			w8(0x01);
//
//			w32(2);

			break;

		case MDC_DeviceReset:
			w8(MDRS_DeviceReply);
			w8(0x00);
			w8(0x20);
			w8(0x00);

			break;

		default:
			printf("Unknown Maple command %x\n", cmd);
			w8(MDRE_UnknownCmd);
			w8(0x00);
			w8(0x00);
			w8(0x00);

			break;
		}
		/*
		printf("JVS OUT: ");
		p = (u8 *)buffer_out;
		for (int i = 0; i < out_len; i++) printf("%02x ", p[i]);
		printf("\n");
		*/

		return out_len;
	}

	virtual bool maple_serialize(void **data, unsigned int *total_size)
	{
		REICAST_S(crazy_mode);
		REICAST_S(jvs_repeat_length);
		REICAST_S(jvs_repeat_request);
		REICAST_S(jvs_receive_length);
		REICAST_S(jvs_receive_buffer);
		size_t board_count = io_boards.size();
		REICAST_S(board_count);
		for (int i = 0; i < io_boards.size(); i++)
			io_boards[i]->maple_serialize(data, total_size);

		return true ;
	}

	virtual bool maple_unserialize(void **data, unsigned int *total_size)
	{
		REICAST_US(crazy_mode);
		REICAST_US(jvs_repeat_length);
		REICAST_US(jvs_repeat_request);
		REICAST_US(jvs_receive_length);
		REICAST_US(jvs_receive_buffer);
		size_t board_count;
		REICAST_US(board_count);
		for (int i = 0; i < board_count; i++)
		{
			io_boards.push_back(new jvs_io_board(i + 1, this));
			io_boards.back()->maple_unserialize(data, total_size);
		}

		return true ;
	}
};

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
		LOGJVS("JVS Node %d address assigned\n", node_id);
		break;

	case 0x10:	// Read ID data
		JVS_STATUS1();	// status
		JVS_STATUS1();	// report
		static char ID[] = "SEGA ENTERPRISES,LTD.;I/O BD JVS;837-13551 ;Ver1.00;98/10";
		for (char *p = ID; *p != 0; )
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
		JVS_OUT(rotary_encoders ? 1 : 2);		//   2 players
		JVS_OUT(rotary_encoders ? 9 : 13);		//   13 bits
		JVS_OUT(0);

		if (!rotary_encoders)
		{
			JVS_OUT(2);		// Coin inputs
			JVS_OUT(2);		//   2 inputs
			JVS_OUT(0);
			JVS_OUT(0);
		}

		if (!rotary_encoders)
		{
			JVS_OUT(3);		// Analog inputs
			JVS_OUT(8);		//   8 channels
			JVS_OUT(0);		//   bits per channel, 0: unknown
			JVS_OUT(0);
		}

		if (rotary_encoders)
		{
			JVS_OUT(4);		// Rotary encoders
			JVS_OUT(4);		//   4 channels
			JVS_OUT(0);
			JVS_OUT(0);
		}

//		JVS_OUT(6);		// Light gun
//		JVS_OUT(12);		//   X bits
//		JVS_OUT(12);		//   Y bits
//		JVS_OUT(1);			//   1 channel

		JVS_OUT(0x12);	// General output driver
		JVS_OUT(6);		//    6 outputs
		JVS_OUT(0);
		JVS_OUT(0);

		JVS_OUT(0);		// End of list
		JVS_OUT(0);
		JVS_OUT(0);
		JVS_OUT(0);
		break;

	case 0x15:	// Master board ID
		JVS_STATUS1();
		JVS_STATUS1();
		break;

	default:
		if (jvs_cmd >= 0x20 && jvs_cmd <= 0x38) // Read inputs and more
		{
			LOGJVS("JVS Node %d: ", node_id);
			PlainJoystickState pjs;
			parent->config->GetInput(&pjs);
			u32 keycode = ~kcode[0];
			u32 keycode2 = ~kcode[1];

			JVS_STATUS1();	// status
			for (int cmdi = 0; cmdi < length_in; )
			{
				switch (buffer_in[cmdi])
				{
				case 0x20:	// Read digital input
					{
						JVS_STATUS1();	// report byte

						LOGJVS("btns ");
						JVS_OUT((kcode[0] & 1) ? 0 : 0x80);		// test, tilt1, tilt2, tilt3, unused, unused, unused, unused
						// FIXME in-lst mapping
						u8 buttons[8] = { 0 };
						u32 keycode = ~kcode[0];
						for (int i = 0; i < 16; i++)
							if ((keycode & (1 << i)) != 0)
							{
								buttons[Naomi_Mapping.button_mapping_byte[i]] |= Naomi_Mapping.button_mapping_mask[i];
							}
						for (int player = 0; player < buffer_in[cmdi + 1]; player++)
						{
							LOGJVS("P%d %02x ", player + 1, buttons[player * 2]);
							JVS_OUT(buttons[player * 2]);
							if (buffer_in[cmdi + 2] == 2)
							{
								LOGJVS("%02x ", buttons[player * 2 + 1]);
								JVS_OUT(buttons[player * 2 + 1]);
							}
						}
//								for (int player = 0; player < jvs_request[channel][cmdi + 1]; player++)
//								{
//									u32 keycode = ~kcode[player];
//									if (keycode & DC_BTN_C)
//										keycode |= 0xFFff;
//
//									if (jvs_request[channel][cmdi + 2] == 1)
//										JVS_OUT(keycode);
//									else
//										w16(keycode);
//								}
						cmdi += 3;
					}
					break;

				case 0x21:	// Read coins
					{
						if (coin_chute)
						{
							coin_chute = false;
							coin_count++;
						}
						JVS_STATUS1();	// report byte
						LOGJVS("coins ");
						for (int slot = 0; slot < buffer_in[cmdi + 1]; slot++)
						{
							if (slot == 0)
							{
								LOGJVS("0:%d ", coin_count);
								JVS_OUT((coin_count >> 8) & 0x3F);		// status (2 highest bits, 0: normal), coin count MSB
								JVS_OUT(coin_count);						// coin count LSB
							}
							else
							{
								LOGJVS("%d:0 ", slot);
								JVS_OUT(0);
								JVS_OUT(0);
							}
						}
						cmdi += 2;
					}
					break;

				case 0x22:	// Read analog inputs
					{
						JVS_STATUS1();	// report byte
						int axis = 0;

						LOGJVS("ana ");
						if (lightgun_as_analog)
						{
							// Death Crimson / Confidential Mission
							u16 x = mo_x_abs * 0xFFFF / 639;
							u16 y = mo_y_abs * 0xFFFF / 479;
							if (mo_x_abs < 0 || mo_x_abs > 639 || mo_y_abs < 0 || mo_y_abs > 479)
							{
								x = 0;
								y = 0;
							}
							LOGJVS("x,y:%4x,%4x ", x, y);
							JVS_OUT(x >> 8);		// X, MSB
							JVS_OUT(x);			// X, LSB
							JVS_OUT(y >> 8);		// Y, MSB
							JVS_OUT(y);			// Y, LSB
							axis = 2;
						}

						for (; axis < buffer_in[cmdi + 1]; axis++)
						{
							u16 axis_value;
							if (Naomi_Mapping.axis[axis] != NULL)
								axis_value = Naomi_Mapping.axis[axis]();
							else
							{
								switch (axis) {
								case 0:
									axis_value = (joyx[axis / 4] + 128) << 8;
									break;
								case 1:
									axis_value = (joyy[axis / 4] + 128) << 8;
									break;
								case 2:
									axis_value = rt[axis / 4] << 8;
									break;
								case 3:
									axis_value = lt[axis / 4] << 8;
									break;
								}
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
						roty += mo_y_delta * 5;
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
								JVS_OUT(0x80);	// MSB
								JVS_OUT(0x80);	// LSB
							}
						}
						cmdi += 2;
					}
					break;

				case 0x25:	// Read screen pos inputs
					{
						JVS_STATUS1();	// report byte
						// Channel number is jvs_request[channel][cmdi + 1]
						u16 x = mo_x_abs * 0xFFFF / 639;
						u16 y = (479 - mo_y_abs) * 0xFFFF / 479;
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
					JVS_STATUS1();	// report byte
					cmdi += buffer_in[cmdi + 1] + 2;
					break;

				case 0x30:	// substract coin
					if (buffer_in[cmdi + 1] == 1)
						coin_count -= buffer_in[cmdi + 2] << 8 + buffer_in[cmdi + 3];
					JVS_STATUS1();	// report byte
					cmdi += 4;
					break;

				default:
					printf("JVS: Unknown input type %x\n", buffer_in[cmdi]);
					JVS_OUT(2);			// report byte: command error
					cmdi = length_in;	// Ignore subsequent commands
					break;
				}
			}
			LOGJVS("\n");
		}
		else
		{
			printf("JVS: Unknown JVS command %x\n", jvs_cmd);
			JVS_OUT(2);	// Unknown command
		}
		break;
	}
	jvs_length = length - 3;

	return length;
}

bool jvs_io_board::maple_serialize(void **data, unsigned int *total_size)
{
	REICAST_S(node_id);
	REICAST_S(rotary_encoders);
	REICAST_S(lightgun_as_analog);

	return true ;
}

bool jvs_io_board::maple_unserialize(void **data, unsigned int *total_size)
{
	REICAST_US(node_id);
	REICAST_US(rotary_encoders);
	REICAST_US(lightgun_as_analog);

	return true ;
}

maple_device* maple_Create(MapleDeviceType type)
{
	maple_device* rv=0;
	switch(type)
	{
	case MDT_SegaController:
		rv=new maple_sega_controller();
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
		rv = new maple_lightgun();
		break;

	case MDT_NaomiJamma:
		rv = new maple_naomi_jamma();
		break;

	default:
		return 0;
	}

	return rv;
}
