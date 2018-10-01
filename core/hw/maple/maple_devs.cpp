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

#include "deps/zlib/zlib.h"

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
};

enum MapleDeviceRV
{
	MDRS_DeviceStatus    = 0x05, //28 words
	MDRS_DeviceStatusAll = 0x06, //28 words + device dependent data
	MDRS_DeviceReply     = 0x07, //0 words
	MDRS_DataTransfer    = 0x08, //FT,depends on the command

	MDRE_UnknownFunction = 0xFE, //0 words
	MDRE_UnknownCmd      = 0xFD, //0 words
	MDRE_TransminAgain   = 0xFC, //0 words
	MDRE_FileError       = 0xFB, //1 word, bitfield
	MDRE_LCDError        = 0xFA, //1 word, bitfield
	MDRE_ARGunError      = 0xF9, //1 word, bitfield
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

	virtual u32 Dma(u32 Command,u32* buffer_in,u32 buffer_in_len,u32* buffer_out,u32& buffer_out_len)
	{
		dma_buffer_out=(u8*)buffer_out;
		dma_count_out=&buffer_out_len;

		dma_buffer_in=(u8*)buffer_in;
		dma_count_in=buffer_in_len;

		return dma(Command);
	}
	virtual u32 dma(u32 cmd)=0;
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
							return MDRE_TransminAgain; //invalid params
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
							return MDRE_TransminAgain;	//invalid params ...
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
				case MDRE_TransminAgain:
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

_NaomiState State;


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


void printState(u32 cmd, u32* buffer_in, u32 buffer_in_len)
{
	printf("Command : 0x%X", cmd);
	if (buffer_in_len>0)
		printf(",Data : %d bytes\n", buffer_in_len);
	else
		printf("\n");
	buffer_in_len >>= 2;
	while (buffer_in_len-->0)
	{
		printf("%08X ", *buffer_in++);
		if (buffer_in_len == 0)
			printf("\n");
	}
}

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
Sega Dreamcast Controller
No error checking of any kind, but works just fine
*/
struct maple_naomi_jamma : maple_sega_controller
{
	virtual MapleDeviceType get_device_type()
	{
		return MDT_NaomiJamma;
	}

	virtual u32 dma(u32 cmd)
	{
		u32* buffer_in = (u32*)dma_buffer_in;
		u32* buffer_out = (u32*)dma_buffer_out;

		u8* buffer_in_b = dma_buffer_in;
		u8* buffer_out_b = dma_buffer_out;

		u32& buffer_out_len = *dma_count_out;
		u32 buffer_in_len = dma_count_in;

		switch (cmd)
		{
		case 0x86:
		{
			u32 subcode = *(u8*)buffer_in;
			//printf("Naomi 0x86 : %x\n",SubCode);
			switch (subcode)
			{
			case 0x15:
			case 0x33:
			{
				PlainJoystickState pjs;
				config->GetInput(&pjs);

				buffer_out[0] = 0xffffffff;
				buffer_out[1] = 0xffffffff;
				u32 keycode = ~kcode[0];
				u32 keycode2 = ~kcode[1];

				if (keycode&NAOMI_SERVICE_KEY_2)		//Service
					buffer_out[0] &= ~(1 << 0x1b);

				if (keycode&NAOMI_TEST_KEY_2)		//Test
					buffer_out[0] &= ~(1 << 0x1a);

				if (State.Mode == 0 && subcode != 0x33)	//Get Caps
				{
					buffer_out_b[0x11 + 1] = 0x8E;	//Valid data check
					buffer_out_b[0x11 + 2] = 0x01;
					buffer_out_b[0x11 + 3] = 0x00;
					buffer_out_b[0x11 + 4] = 0xFF;
					buffer_out_b[0x11 + 5] = 0xE0;
					buffer_out_b[0x11 + 8] = 0x01;

					switch (State.Cmd)
					{
						//Reset, in : 2 bytes, out : 0
					case 0xF0:
						break;

						//Find nodes?
						//In addressing Slave address, in : 2 bytes, out : 1
					case 0xF1:
					{
						buffer_out_len = 4 * 4;
					}
					break;

					//Speed Change, in : 2 bytes, out : 0
					case 0xF2:
						break;

						//Name
						//"In the I / O ID" "Reading each slave ID data"
						//"NAMCO LTD.; I / O PCB-1000; ver1.0; for domestic only, no analog input"
						//in : 1 byte, out : max 102
					case 0x10:
					{
						static char ID1[102] = "nullDC Team; I/O Plugin-1; ver0.2; for nullDC or other emus";
						buffer_out_b[0x8 + 0x10] = (u8)strlen(ID1) + 3;
						for (int i = 0; ID1[i] != 0; ++i)
						{
							buffer_out_b[0x8 + 0x13 + i] = ID1[i];
						}
					}
					break;

					//CMD Version
					//REV in command|Format command to read the (revision)|One|Two
					//in : 1 byte, out : 2 bytes
					case 0x11:
					{
						buffer_out_b[0x8 + 0x13] = 0x13;
					}
					break;

					//JVS Version
					//In JV REV|JAMMA VIDEO standard reading (revision)|One|Two
					//in : 1 byte, out : 2 bytes
					case 0x12:
					{
						buffer_out_b[0x8 + 0x13] = 0x30;
					}
					break;

					//COM Version
					//VER in the communication system|Read a communication system compliant version of |One|Two
					//in : 1 byte, out : 2 bytes
					case 0x13:
					{
						buffer_out_b[0x8 + 0x13] = 0x10;
					}
					break;

					//Features
					//Check in feature |Each features a slave to read |One |6 to
					//in : 1 byte, out : 6 + (?)
					case 0x14:
					{
						unsigned char *FeatPtr = buffer_out_b + 0x8 + 0x13;
						buffer_out_b[0x8 + 0x9 + 0x3] = 0x0;
						buffer_out_b[0x8 + 0x9 + 0x9] = 0x1;
#define ADDFEAT(Feature,Count1,Count2,Count3)	*FeatPtr++=Feature; *FeatPtr++=Count1; *FeatPtr++=Count2; *FeatPtr++=Count3;
						ADDFEAT(1, 2, 12, 0);	//Feat 1=Digital Inputs.  2 Players. 12 bits
						ADDFEAT(2, 2, 0, 0);	//Feat 2=Coin inputs. 2 Inputs
						ADDFEAT(3, 4, 0, 0);	//Feat 3=Analog. 4 Chans

						ADDFEAT(0, 0, 0, 0);	//End of list
					}
					break;

					default:
						printf("unknown CAP %X\n", State.Cmd);
						return 0;
					}
					buffer_out_len = 4 * 4;
				}
				else if (State.Mode == 1 || State.Mode == 2 || subcode == 0x33)	//Get Data
				{
					unsigned char glbl = 0x00;
					unsigned char p1_1 = 0x00;
					unsigned char p1_2 = 0x00;
					unsigned char p2_1 = 0x00;
					unsigned char p2_2 = 0x00;
					static unsigned char LastKey[256];
					static unsigned short coin1 = 0x0000;
					static unsigned short coin2 = 0x0000;
					unsigned char Key[256] = { 0 };
#if HOST_OS == OS_WINDOWS
					GetKeyboardState(Key);
#endif
					u8 *buttons = &buffer_out_b[8 + 0x12 + 2];
					for (int i = 0; i < 4; i++)
						buttons[i] = 0;
					for (int i = 0; i < 16; i++)
						if (keycode & (1 << i))
						{
							buttons[Naomi_Mapping.button_mapping_byte[i]] |= Naomi_Mapping.button_mapping_mask[i];
						}
					u16 axis_values[4];
					for (int i = 0; i < 4; i++)
					{
						if (Naomi_Mapping.axis[i] != NULL)
							axis_values[i] = Naomi_Mapping.axis[i]();
						else
						{
							switch (i) {
							case 0:
								axis_values[i] = (joyx[bus_id * 2 + 0] + 128) << 8;
								break;
							case 1:
								axis_values[i] = (joyy[bus_id * 2 + 0] + 128) << 8;
								break;
							case 2:
								axis_values[i] = rt[bus_id * 2] << 8;
								break;
							case 3:
								axis_values[i] = lt[bus_id * 2] << 8;
								break;
							}
						}
					}
					
					if (keycode&NAOMI_SERVICE_KEY_1)			//Service ?
						glbl |= 0x80;
					
					static bool old_coin = false;
					static bool old_coin2 = false;

					if ((old_coin == false) && (keycode&NAOMI_COIN_KEY))
						coin1++;
					old_coin = (keycode&NAOMI_COIN_KEY) ? true : false;

					if ((old_coin2 == false) && (keycode2&NAOMI_COIN_KEY))
						coin2++;
					old_coin2 = (keycode2&NAOMI_COIN_KEY) ? true : false;

					buffer_out_b[0x11 + 0] = 0x00;
					buffer_out_b[0x11 + 1] = 0x8E;	//Valid data check
					buffer_out_b[0x11 + 2] = 0x01;
					buffer_out_b[0x11 + 3] = 0x00;
					buffer_out_b[0x11 + 4] = 0xFF;
					buffer_out_b[0x11 + 5] = 0xE0;
					buffer_out_b[0x11 + 8] = 0x01;

					//memset(OutData+8+0x11,0x00,0x100);

					buffer_out_b[8 + 0x12 + 0] = 1;
					buffer_out_b[8 + 0x12 + 1] = glbl;
//					buffer_out_b[8 + 0x12 + 2] = p1_1;
//					buffer_out_b[8 + 0x12 + 3] = p1_2;
//					buffer_out_b[8 + 0x12 + 4] = p2_1;
//					buffer_out_b[8 + 0x12 + 5] = p2_2;
					buffer_out_b[8 + 0x12 + 6] = 1;
					buffer_out_b[8 + 0x12 + 7] = coin1 >> 8;
					buffer_out_b[8 + 0x12 + 8] = coin1 & 0xff;
					buffer_out_b[8 + 0x12 + 9] = coin2 >> 8;
					buffer_out_b[8 + 0x12 + 10] = coin2 & 0xff;
					buffer_out_b[8 + 0x12 + 11] = 1;
					buffer_out_b[8 + 0x12 + 12] = axis_values[0] >> 8;
					buffer_out_b[8 + 0x12 + 13] = axis_values[0];
					buffer_out_b[8 + 0x12 + 14] = axis_values[1] >> 8;
					buffer_out_b[8 + 0x12 + 15] = axis_values[1];
					buffer_out_b[8 + 0x12 + 16] = axis_values[2] >> 8;
					buffer_out_b[8 + 0x12 + 17] = axis_values[2];
					buffer_out_b[8 + 0x12 + 18] = axis_values[3] >> 8;
					buffer_out_b[8 + 0x12 + 19] = axis_values[3];
					buffer_out_b[8 + 0x12 + 20] = 0x00;

					memcpy(LastKey, Key, sizeof(Key));

					if (State.Mode == 1)
					{
						buffer_out_b[0x11 + 0x7] = 19;
						buffer_out_b[0x11 + 0x4] = 19 + 5;
					}
					else
					{
						buffer_out_b[0x11 + 0x7] = 17;
						buffer_out_b[0x11 + 0x4] = 17 - 1;
					}

					//OutLen=8+0x11+16;
					buffer_out_len = 8 + 0x12 + 22;
				}
				/*ID.Keys=0xFFFFFFFF;
				if(GetKeyState(VK_F1)&0x8000)		//Service
				ID.Keys&=~(1<<0x1b);
				if(GetKeyState(VK_F2)&0x8000)		//Test
				ID.Keys&=~(1<<0x1a);
				memcpy(OutData,&ID,sizeof(ID));
				OutData[0x12]=0x8E;
				OutLen=sizeof(ID);
				*/
			}
			return 8;

			case 0x17:	//Select Subdevice
			{
				State.Mode = 0;
				State.Cmd = buffer_in_b[8];
				State.Node = buffer_in_b[9];
				buffer_out_len = 0;
			}
			return (7);

			case 0x27:	//Transfer request
			{
				State.Mode = 1;
				State.Cmd = buffer_in_b[8];
				State.Node = buffer_in_b[9];
				buffer_out_len = 0;
			}
			return (7);
			case 0x21:		//Transfer request with repeat
			{
				State.Mode = 2;
				State.Cmd = buffer_in_b[8];
				State.Node = buffer_in_b[9];
				buffer_out_len = 0;
			}
			return (7);

			case 0x0B:	//EEPROM write
			{
				int address = buffer_in_b[1];
				int size = buffer_in_b[2];
				//printf("EEprom write %08X %08X\n",address,size);
				//printState(Command,buffer_in,buffer_in_len);
				memcpy(EEPROM + address, buffer_in_b + 4, size);

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
			}
			return (7);
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
				//printf("EEprom READ ?\n");
				int address = buffer_in_b[1];
				//printState(Command,buffer_in,buffer_in_len);
				memcpy(buffer_out, EEPROM + address, 0x80);
				buffer_out_len = 0x80;
			}
			return 8;
			//IF I return all FF, then board runs in low res
			case 0x31:
			{
				buffer_out[0] = 0xffffffff;
				buffer_out[1] = 0xffffffff;
			}
			return (8);

			//case 0x3:
			//	break;

			//case 0x1:
			//	break;
			default:
				printf("Unknown 0x86 : SubCommand 0x%X - State: Cmd 0x%X Mode :  0x%X Node : 0x%X\n", subcode, State.Cmd, State.Mode, State.Node);
				printState(cmd, buffer_in, buffer_in_len);
			}

			return 8;//MAPLE_RESPONSE_DATATRF
		}
		break;
		case 0x82:
		{
			const char *ID = "315-6149    COPYRIGHT SEGA E\x83\x00\x20\x05NTERPRISES CO,LTD.  ";
			memset(buffer_out_b, 0x20, 256);
			memcpy(buffer_out_b, ID, 0x38 - 4);
			buffer_out_len = 256;
			return (0x83);
		}

		case 1:
		case 9:
			return maple_sega_controller::dma(cmd);


		default:
			printf("unknown MAPLE Frame\n");
			//printState(Command, buffer_in, buffer_in_len);
			break;
		}
		return MDRE_UnknownFunction;
	}
};
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
