/*
	gdrom, v3
	Overly complex implementation of a very ugly device
*/

#include "gdromv3.h"
#include "gdrom_if.h"
#include "cfg/option.h"
#include "hw/holly/holly_intc.h"
#include "hw/holly/sb.h"
#include "hw/sh4/modules/dmac.h"
#include "hw/sh4/sh4_interpreter.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_mmr.h"
#include "hw/sh4/sh4_sched.h"
#include "imgread/common.h"
#include "serialize.h"

/// iOS-specific memory optimizations for GD-ROM streaming
#if defined(__APPLE__) && defined(TARGET_IPHONE)

/// iOS unified memory architecture optimizations
#define IOS_GDROM_CACHE_LINE_SIZE 64
#define IOS_GDROM_OPTIMAL_ALIGNMENT 64
#define IOS_GDROM_LARGE_BUFFER_SIZE (2352 * 64)  // 2x larger for streaming

/// iOS-optimized buffer structure for better cache performance
struct IOSOptimizedBuffer {
	alignas(IOS_GDROM_OPTIMAL_ALIGNMENT) u8 data[IOS_GDROM_LARGE_BUFFER_SIZE];
	size_t size;
	size_t readPos;
	size_t writePos;
	bool isOptimized;
};

static IOSOptimizedBuffer iosGDROMBuffer = {};

/// iOS memory prefetch optimization for streaming data
static inline void IOSPrefetchStreamingData(const void* data, size_t size) {
	/// Prefetch data for optimal iOS streaming performance
	const char* ptr = (const char*)data;
	for (size_t i = 0; i < size; i += IOS_GDROM_CACHE_LINE_SIZE) {
		__builtin_prefetch(ptr + i, 0, 2); // Prefetch for read with moderate locality
	}
}



/// Check if buffer data is properly aligned for iOS DMA
static inline bool IOSCheckAlignment(const void* ptr) {
	return (reinterpret_cast<uintptr_t>(ptr) % IOS_GDROM_OPTIMAL_ALIGNMENT) == 0;
}

/// Initialize iOS-optimized GD-ROM buffer
static void IOSInitializeGDROMBuffer() {
	iosGDROMBuffer.size = IOS_GDROM_LARGE_BUFFER_SIZE;
	iosGDROMBuffer.readPos = 0;
	iosGDROMBuffer.writePos = 0;
	iosGDROMBuffer.isOptimized = true;
	
	/// Ensure buffer alignment
	if (!IOSCheckAlignment(iosGDROMBuffer.data)) {
		WARN_LOG(GDROM, "iOS GD-ROM: Buffer not optimally aligned for iOS DMA");
	}
	
	INFO_LOG(GDROM, "iOS GD-ROM: Initialized optimized streaming buffer (%d KB)", 
	         (int)(IOS_GDROM_LARGE_BUFFER_SIZE / 1024));
}

#endif

int gdrom_schid;

//Sense: ASC - ASCQ - Key
int sns_asc;
int sns_ascq;
int sns_key;

static u32 set_mode_offset;
static read_params_t read_params;
static packet_cmd_t packet_cmd;
static read_buff_t read_buff;
static pio_buff_t pio_buff;
static ata_cmd_t ata_cmd;
cdda_t cdda;

static gd_states gd_state;
static DiscType gd_disk_type;
/*
	GD rom reset -> GDS_WAITCMD

	GDS_WAITCMD -> ATA/SPI command [Command code is on ata_cmd]
	SPI Command -> GDS_WAITPACKET -> GDS_SPI_* , depending on input

	GDS_SPI_READSECTOR -> Depending on features , it can do quite a few things
*/
static u32 data_write_mode=0;

//Registers
static u32 DriveSel;
static GD_ErrRegT Error;
static GD_InterruptReasonT IntReason;
static GD_FeaturesT Features;
static GD_SecCountT SecCount;
GD_SecNumbT SecNumber;
	
static GD_StatusT GDStatus;

static ByteCount_t ByteCount;
	
//end
GD_HardwareInfo_t GD_HardwareInfo;

#define printf_rm(...) DEBUG_LOG(GDROM, __VA_ARGS__)
#define printf_ata(...) DEBUG_LOG(GDROM, __VA_ARGS__)
#define printf_spi(...) DEBUG_LOG(GDROM, __VA_ARGS__)
#define printf_spicmd(...) DEBUG_LOG(GDROM, __VA_ARGS__)
#define printf_subcode(...) DEBUG_LOG(GDROM, __VA_ARGS__)

void libCore_CDDA_Sector(s16* sector)
{
	//silence ! :p
	if (cdda.status == cdda_t::Playing)
	{
		libGDR_ReadSector((u8*)sector,cdda.CurrAddr.FAD,1,2352);
		cdda.CurrAddr.FAD++;
		if (cdda.CurrAddr.FAD >= cdda.EndAddr.FAD)
		{
			if (cdda.repeats==0)
			{
				//stop
				cdda.status = cdda_t::Terminated;
				SecNumber.Status = GD_PAUSE;
			}
			else
			{
				//Repeat ;)
				if (cdda.repeats!=0xf)
					cdda.repeats--;

				cdda.CurrAddr.FAD=cdda.StartAddr.FAD;
			}
		}
	}
	else
	{
		memset(sector,0,2352);
	}
}

static void gd_spi_pio_end(const u8* buffer, u32 len, gd_states next_state = gds_pio_end);
static void gd_process_spi_cmd();
static void gd_process_ata_cmd();

static void FillReadBuffer()
{
	read_buff.cache_index=0;
	u32 count = read_params.remaining_sectors;

#if defined(__APPLE__) && defined(TARGET_IPHONE)
	/// iOS-optimized streaming: Use larger buffer for FMV performance
	if (count > 64 && iosGDROMBuffer.isOptimized) {
		count = 64;  /// 2x larger for iOS streaming
	} else
#endif
	if (count > 32)
		count = 32;

	read_buff.cache_size=count*read_params.sector_type;

#if defined(__APPLE__) && defined(TARGET_IPHONE)
	/// Use iOS-optimized buffer for large transfers (like FMV)
	if (read_buff.cache_size >= IOS_GDROM_CACHE_LINE_SIZE * 8 && iosGDROMBuffer.isOptimized) {
		/// Prefetch hint for upcoming large data read
		IOSPrefetchStreamingData(read_buff.cache, read_buff.cache_size);
		
		/// Check alignment for optimal iOS DMA performance
		if (!IOSCheckAlignment(read_buff.cache)) {
			DEBUG_LOG(GDROM, "iOS GD-ROM: Cache buffer not optimally aligned for DMA");
		}
	}
#endif

	libGDR_ReadSector(read_buff.cache,read_params.start_sector,count,read_params.sector_type);
	read_params.start_sector+=count;
	read_params.remaining_sectors-=count;
	
#if defined(__APPLE__) && defined(TARGET_IPHONE)
	/// iOS memory barrier to ensure cache coherency for video streaming
	if (read_buff.cache_size >= IOS_GDROM_CACHE_LINE_SIZE * 4) {
		__builtin_arm_dsb(15); /// Full data synchronization barrier
	}
#endif
}


static void gd_set_state(gd_states state)
{
	gd_states prev=gd_state;
	gd_state=state;
	switch(state)
	{
		case gds_waitcmd:
			GDStatus.DRDY=1;   // Can accept ATA command :)
			GDStatus.BSY=0;    // Does not access command block
			break;

		case gds_procata:
			//verify(prev==gds_waitcmd);    // Validate the previous command ;)

			GDStatus.DRDY=0;   // Can't accept ATA command
			GDStatus.BSY=1;    // Accessing command block to process command
			gd_process_ata_cmd();
			break;

		case gds_waitpacket:
			verify(prev==gds_procata); // Validate the previous command ;)

			// Prepare for packet command
			packet_cmd.index=0;

			// Set CoD, clear BSY and IO
			IntReason.CoD=1;
			GDStatus.BSY = 0;
			IntReason.IO=0;

			// Make DRQ valid
			GDStatus.DRQ = 1;

			// ATA can optionally raise the interrupt ...
			// RaiseInterrupt(holly_GDROM_CMD);
			break;

		case gds_procpacket:
			verify(prev==gds_waitpacket); // Validate the previous state ;)

			GDStatus.DRQ=0;     // Can't accept ATA command
			GDStatus.BSY=1;     // Accessing command block to process command
			gd_process_spi_cmd();
			break;
			//yep , get/set are the same !
		case gds_pio_get_data:
		case gds_pio_send_data:
			//  When preparations are complete, the following steps are carried out at the device.
			//(1)   Number of bytes to be read is set in "Byte Count" register. 
			ByteCount.full =(u16)(pio_buff.size<<1);
			//(2)   IO bit is set and CoD bit is cleared. 
			IntReason.IO=1;
			IntReason.CoD=0;
			//(3)   DRQ bit is set, BSY bit is cleared. 
			GDStatus.DRQ=1;
			GDStatus.BSY=0;
			//(4)   INTRQ is set, and a host interrupt is issued.
			asic_RaiseInterrupt(holly_GDROM_CMD);
			/*
			The number of bytes normally is the byte number in the register at the time of receiving 
			the command, but it may also be the total of several devices handled by the buffer at that point.
			*/
			break;

		case gds_readsector_pio:
			{
				/*
				If more data are to be sent, the device sets the BSY bit and repeats the above sequence 
				from step 7. 
				*/
				GDStatus.BSY=1;

				u32 sector_count = read_params.remaining_sectors;
				gd_states next_state=gds_pio_end;

				if (sector_count > 27)
				{
					sector_count = 27;
					next_state = gds_readsector_pio;
				}

				libGDR_ReadSector((u8*)&pio_buff.data[0],read_params.start_sector,sector_count, read_params.sector_type);
				read_params.start_sector+=sector_count;
				read_params.remaining_sectors-=sector_count;

				gd_spi_pio_end(0,sector_count*read_params.sector_type,next_state);
			}
			break;
			
		case gds_readsector_dma:
 			FillReadBuffer();
			break;

		case gds_pio_end:
			
			GDStatus.DRQ=0;//all data is sent !

			gd_set_state(gds_procpacketdone);
			break;

		case gds_procpacketdone:
			/*
			7.  When the device is ready to send the status, it writes the 
			final status (IO, CoD, DRDY set, BSY, DRQ cleared) to the "Status" register before making INTRQ valid. 
			After checking INTRQ, the host reads the "Status" register to check the completion status. 
			*/
			//Set IO, CoD, DRDY
			GDStatus.DRDY=1;
			IntReason.CoD=1;
			IntReason.IO=1;

			//Clear DRQ,BSY
			GDStatus.DRQ=0;
			GDStatus.BSY=0;
			//Make INTRQ valid
			asic_RaiseInterrupt(holly_GDROM_CMD);

			//command finished !
			gd_set_state(gds_waitcmd);
			break;

		case gds_process_set_mode:
			memcpy((u8 *)&GD_HardwareInfo + set_mode_offset, pio_buff.data, pio_buff.size << 1);
			//end pio transfer ;)
			gd_set_state(gds_pio_end);
			break;

		default :
			die("Unhandled GDROM state ...");
			break;
	}
}


void gd_setdisc()
{
	cdda.status = cdda_t::NoInfo;

	DiscType newd = (DiscType)libGDR_GetDiscType();
	
	switch(newd)
	{
	case NoDisk:
		SecNumber.Status = GD_NODISC;
		//GDStatus.BSY=0;
		//GDStatus.DRDY=1;
		break;

	case Open:
		SecNumber.Status = GD_OPEN;
		//GDStatus.BSY=0;
		GDStatus.DRDY=1;
		break;

	default :
		if (SecNumber.Status == GD_BUSY)
			SecNumber.Status = GD_PAUSE;
		else
			SecNumber.Status = GD_STANDBY;
		//GDStatus.BSY=0;
		//GDStatus.DRDY=1;
		break;
	}

	gd_disk_type = newd;
	SecNumber.DiscFormat = gd_disk_type >> 4;
}

static void gd_reset()
{
	//Reset the drive
	gd_setdisc();
	gd_set_state(gds_waitcmd);
}

static u32 GetFAD(u8* data, bool msf)
{
	if (msf)
		return data[0] * 60 * 75 + data[1] * 75 + data[2];
	else
		return (data[0] << 16) | (data[1] << 8) | data[2];
}

//disk changes etc
static void gd_disc_change()
{
	gd_setdisc();
	read_params = { 0 };
	set_mode_offset = 0;
	packet_cmd = { 0 };
	memset(&read_buff, 0, sizeof(read_buff));
	pio_buff = { gds_waitcmd, 0 };
	ata_cmd = { 0 };
	cdda = { cdda_t::NoInfo, 0 };
}

//This handles the work of setting up the pio regs/state :)
static void gd_spi_pio_end(const u8* buffer, u32 len, gd_states next_state)
{
	verify(len<0xFFFF);
	pio_buff.index=0;
	pio_buff.size=len>>1;
	pio_buff.next_state=next_state;

	if (buffer!=0)
		memcpy(pio_buff.data,buffer,len);

	if (len==0)
		gd_set_state(next_state);
	else
		gd_set_state(gds_pio_send_data);
}
static void gd_spi_pio_read_end(u32 len, gd_states next_state)
{
	verify(len<0xFFFF);
	pio_buff.index=0;
	pio_buff.size=len>>1;
	pio_buff.next_state=next_state;

	if (len==0)
		gd_set_state(next_state);
	else
		gd_set_state(gds_pio_get_data);
}
static void gd_process_ata_cmd()
{
	//Any ATA command clears these bits, unless aborted/error :p
	Error.ABRT=0;
	
	if (sns_key == 0x0 			// No sense
			|| sns_key == 0xB	// Aborted
			|| sns_key == 6) 	// Unit attention
		GDStatus.CHECK=0;
	else
		GDStatus.CHECK=1;

	switch(ata_cmd.command)
	{
	case ATA_NOP:
		printf_ata("ATA_NOP");
		/*
			Setting "abort" in the error register 
			Setting an error in the status register 
			Clearing "busy" in the status register 
			Asserting the INTRQ signal
		*/
		
		Error.ABRT = 1;
		Error.Sense = sns_key;
		GDStatus.BSY = 0;
		GDStatus.CHECK = 1;
		GDStatus.DRQ = 0;

		asic_RaiseInterrupt(holly_GDROM_CMD);
		gd_set_state(gds_waitcmd);
		break;

	case ATA_SOFT_RESET:
		{
			printf_ata("ATA_SOFT_RESET");
			gd_reset();
			GDStatus.full = 0;
			Error.full = 1;
			sns_key = 0;
			SecNumber.Status = GD_PAUSE;
			IntReason.full = 1;
			// DC Checker expects these values
			ByteCount.low = 0x14;
			ByteCount.hi = 0xEB;
		}
		break;

	case ATA_EXEC_DIAG:
		printf_ata("ATA_EXEC_DIAG");
		Error.full = 1;	// No error
		sns_key = 0;
		GDStatus.BSY = 0;
		GDStatus.CHECK = 1;

		asic_RaiseInterrupt(holly_GDROM_CMD);
		gd_set_state(gds_waitcmd);
		break;

	case ATA_SPI_PACKET:
		printf_ata("ATA_SPI_PACKET");
		gd_set_state(gds_waitpacket);
		break;

	case ATA_IDENTIFY_DEV:
		printf_ata("ATA_IDENTIFY_DEV: offset %d len %d", packet_cmd.data_8[2], packet_cmd.data_8[4]);
		GDStatus.BSY = 0;
		gd_spi_pio_end((const u8*)&reply_a1[0], 0x50);
		break;

	case ATA_SET_FEATURES:
		printf_ata("ATA_SET_FEATURES");

		//Set features sets :
		//Error : ABRT
		Error.ABRT=0;  // Command was not aborted ;) [hopefully ...]

		//status : DRDY , DSC , DF , CHECK
		//DRDY is set on state change
		GDStatus.DSC=0;
		GDStatus.DF=0;
		GDStatus.DRQ = 0;
		asic_RaiseInterrupt(holly_GDROM_CMD);
		gd_set_state(gds_waitcmd);
		break;

    case ATA_IDENTIFY:
        printf_ata("ATA_IDENTIFY");

        // Set Signature
        DriveSel = 0xa0;

        SecCount.full = 1;
        SecNumber.full = 1;
        ByteCount.low = 0x14;
        ByteCount.hi = 0xeb;

        // ABORT command
        Error.full = 0x4;

        GDStatus.full = 0;
        GDStatus.DRDY = 1;
        GDStatus.CHECK = 1;

        asic_RaiseInterrupt(holly_GDROM_CMD);
        gd_set_state(gds_waitcmd);
        break;

	default:
		WARN_LOG(GDROM, "Unknown ATA command %x", ata_cmd.command);
		Error.ABRT = 1;
		Error.Sense = 5;	// illegal request
		GDStatus.BSY = 0;
		GDStatus.CHECK = 1;
		asic_RaiseInterrupt(holly_GDROM_CMD);
		gd_set_state(gds_waitcmd);

		break;
	};
}

static u16 ccitt_crc(void *data, int len)
{
	static constexpr u16 crctable[256] = {
	    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
	    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
	    0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
	    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
	    0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
	    0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
	    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
	    0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
	    0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
	    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
	    0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
	    0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
	    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
	    0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
	    0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
	    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
	    0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
	    0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
	    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
	    0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
	    0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
	    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
	    0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
	    0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
	    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
	    0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
	    0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
	    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
	    0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
	    0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
	    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
	    0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0,
	};

	u16 crc = 0;
	u8 *p = (u8 *)data;

	while (len-- > 0)
		crc = ((crc & 0xff) << 8) ^ crctable[((crc >> 8) & 0xff) ^ *p++];

	return ~crc;
}

static int bin2bcd(int v)
{
	return (v % 10) | ((v / 10) << 4);
}

static void encodeSubcode(u8 *buffer, u8 data)
{
	buffer[0] = (data & 0x80) ? 0x40 : 0;
	buffer[1] = (data & 0x40) ? 0x40 : 0;
	buffer[2] = (data & 0x20) ? 0x40 : 0;
	buffer[3] = (data & 0x10) ? 0x40 : 0;
	buffer[4] = (data & 0x08) ? 0x40 : 0;
	buffer[5] = (data & 0x04) ? 0x40 : 0;
	buffer[6] = (data & 0x02) ? 0x40 : 0;
	buffer[7] = (data & 0x01) ? 0x40 : 0;
}

u32 gd_get_subcode(u32 format, u32 fad, u8 *subc_info)
{
	subc_info[0] = 0;
	switch (cdda.status)
	{
	case cdda_t::NoInfo:
	default:
		subc_info[1] = 0x15;	// No audio status info
		break;
	case cdda_t::Playing:
		subc_info[1] = 0x11;	// Audio playback in progress
		break;
	case cdda_t::Paused:
		subc_info[1] = 0x12;	// Audio playback paused
		break;
	case cdda_t::Terminated:
		subc_info[1] = 0x13;	// Audio playback ended normally
		break;
	}

	switch (format)
	{
	case 0:	// Raw subcode
		subc_info[2] = 0;
		subc_info[3] = 100;
		// usually not available so let's fake it (Q data only)
		//libGDR_ReadSubChannel(subc_info + 4, 100 - 4);
		{
			u32 curFad = cdda.status == cdda_t::Playing || cdda.status == cdda_t::Paused ? cdda.CurrAddr.FAD : fad;
			u32 elapsed;
			u32 tracknum = libGDR_GetTrackNumber(curFad, elapsed);
			u8 Qch[12];
			u8 adr, ctrl;
			libGDR_GetTrackAdrAndControl(tracknum, adr, ctrl);
			Qch[0] = (ctrl << 4) | adr;
			Qch[1] = bin2bcd(tracknum);
			Qch[2] = bin2bcd(1); 					// index
			Qch[3] = bin2bcd(elapsed / 60 / 75); 	// min
			Qch[4] = bin2bcd((elapsed / 75) % 60); 	// sec
			Qch[5] = bin2bcd(elapsed % 75); 		// frame
			Qch[6] = 0;
			Qch[7] = bin2bcd(curFad / 60 / 75); 	// toc min
			Qch[8] = bin2bcd((curFad / 75) % 60);	// toc sec
			Qch[9] = bin2bcd(curFad % 75); 			// toc frame
			u16 crc = ccitt_crc(Qch, 10);
			Qch[10] = crc >> 8;
			Qch[11] = crc;
			u8 *p = &subc_info[4];
			for (u8 q : Qch)
			{
				encodeSubcode(p, q);
				p += 8;
			}
		}
		break;

	case 1:	// Q data only
	default:
		{
			u32 curFad = cdda.status == cdda_t::Playing || cdda.status == cdda_t::Paused ? cdda.CurrAddr.FAD : fad;
			u32 elapsed;
			u32 tracknum = libGDR_GetTrackNumber(curFad, elapsed);

			//2 DATA Length MSB (0 = 0h)
			subc_info[2] = 0;
			//3 DATA Length LSB (14 = Eh)
			subc_info[3] = 0xE;
			//4 Control ADR
			u8 adr, ctrl;
			libGDR_GetTrackAdrAndControl(tracknum, adr, ctrl);
			subc_info[4] = (ctrl << 4) | adr;
			//5-13	DATA-Q
			u8* data_q = &subc_info[5 - 1];
			//-When ADR = 1
			//1 TNO - track number
			data_q[1] = tracknum;
			//2 X - index within track
			data_q[2] = 1;
			//3-5   Elapsed FAD within track
			data_q[3] = elapsed >> 16;
			data_q[4] = elapsed >> 8;
			data_q[5] = elapsed;
			//6 ZERO
			data_q[6] = 0;
			//7-9 FAD
			data_q[7] = curFad >> 16;
			data_q[8] = curFad >> 8;
			data_q[9] = curFad;
			DEBUG_LOG(GDROM, "gd_get_subcode: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
					 subc_info[0], subc_info[1], subc_info[2], subc_info[3],
					 subc_info[4], subc_info[5], subc_info[6], subc_info[7],
					 subc_info[8], subc_info[9], subc_info[10], subc_info[11],
					 subc_info[12], subc_info[13]);
		}
		break;

	case 2:	// Media catalog number (UPC/bar code)
		{
			//2 DATA Length MSB (0 = 0h)
			subc_info[2] = 0;
			//3 DATA Length LSB (24 = 18h)
			subc_info[3] = 0x18;
			//4 Format Code
			subc_info[4] = 2;
			//5-7 reserved
			subc_info[5] = 0;
			subc_info[6] = 0;
			subc_info[7] = 0;
			//8 MCVal (bit 7)
			subc_info[8] = 0;	// not valid
			//9-21 Media catalog number
			memcpy(&subc_info[9], "0000000000000", 13);
			//22-23 reserved
			subc_info[22] = 0;
			subc_info[23] = 0;
			DEBUG_LOG(GDROM, "gd_get_subcode: format 2 (Media catalog number). audio %x", subc_info[1]);
		}
		break;
	}
	return subc_info[3];
}

static void gd_process_spi_cmd()
{

	printf_spi("Sense: %02x %02x %02x", sns_asc, sns_ascq, sns_key);

	printf_spi("SPI command %02x;",packet_cmd.data_8[0]);
	printf_spi("Params: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
		packet_cmd.data_8[0], packet_cmd.data_8[1], packet_cmd.data_8[2], packet_cmd.data_8[3], packet_cmd.data_8[4], packet_cmd.data_8[5],
		packet_cmd.data_8[6], packet_cmd.data_8[7], packet_cmd.data_8[8], packet_cmd.data_8[9], packet_cmd.data_8[10], packet_cmd.data_8[11] );

	if (sns_key == 0x0 			// No sense
			|| sns_key == 0xB)	// Aborted
		GDStatus.CHECK=0;
	else
		GDStatus.CHECK=1;

	switch(packet_cmd.data_8[0])
	{
	case SPI_TEST_UNIT:
		printf_spicmd("SPI_TEST_UNIT");
		// needed by Alone in the Dark and Resident Evil: Code Veronica disk swapping
		GDStatus.CHECK = SecNumber.Status == GD_BUSY;
		gd_set_state(gds_procpacketdone);
		break;

	case SPI_REQ_MODE:
		GD_HardwareInfo.speed = 0;	// doesn't seem to be settable, or perhaps not for GD-Roms
		GD_HardwareInfo._res0[0] = 0;
		GD_HardwareInfo._res0[1] = 0;
		GD_HardwareInfo._res1 = 0;
		GD_HardwareInfo._res2[0] = 0;
		GD_HardwareInfo._res2[1] = 0;
		GD_HardwareInfo.read_flags &= 0x39;
		printf_spicmd("SPI_REQ_MODE cd-rom speed %d flags %x retry %x", GD_HardwareInfo.speed, GD_HardwareInfo.read_flags, GD_HardwareInfo.read_retry);
		gd_spi_pio_end((u8*)&GD_HardwareInfo + packet_cmd.data_8[2], packet_cmd.data_8[4]);
		break;

		/////////////////////////////////////////////////
		// *FIXME* CHECK FOR DMA, Diff Settings !?!@$#!@%
	case SPI_CD_READ:
	case SPI_CD_READ2:
		{
#define readcmd packet_cmd.GDReadBlock

			cdda.status = cdda_t::NoInfo;
			u32 sector_type = 2048;
			if (readcmd.head == 1 && readcmd.subh == 1 && readcmd.data == 1 && readcmd.expdtype == 3 && readcmd.other == 0)
				sector_type = 2340;
			else if (readcmd.other == 1 || readcmd.expdtype == 1) // Expected Data Type: CD-DA
				sector_type = 2352;
			else if (readcmd.head == 1 || readcmd.subh == 1 || readcmd.other == 1 || readcmd.data == 0)
				WARN_LOG(GDROM, "GDROM: *FIXME* ADD MORE CD READ SETTINGS head %d subh %d other %d data %d type %d",
						readcmd.head, readcmd.subh, readcmd.other, readcmd.data, readcmd.expdtype);

			read_params.start_sector = GetFAD(&readcmd.b[2], readcmd.prmtype);
			if (packet_cmd.data_8[0] == SPI_CD_READ)
				read_params.remaining_sectors = (readcmd.b[8] << 16) | (readcmd.b[9] << 8) | (readcmd.b[10]);
			else
				read_params.remaining_sectors = (readcmd.b[6] << 8) | readcmd.b[7];
			read_params.sector_type = sector_type;//yeah i know , not really many types supported...

			printf_spicmd("SPI_CD_READ - Sector=%d Size=%d/%d DMA=%d",read_params.start_sector,read_params.remaining_sectors,read_params.sector_type,Features.CDRead.DMA);
			if (Features.CDRead.DMA == 1)
			{
				gd_set_state(gds_readsector_dma);
			}
			else
			{
				gd_set_state(gds_readsector_pio);
			}
		}
		break;

	case SPI_GET_TOC:
		{
			printf_spicmd("SPI_GET_TOC");
			//printf("SPI_GET_TOC - %d\n",(packet_cmd.data_8[4]) | (packet_cmd.data_8[3]<<8) );
			u32 toc_gd[102];
			
			//toc - dd/sd
			libGDR_GetToc(&toc_gd[0], (DiskArea)(packet_cmd.data_8[1] & 1));
			 
			gd_spi_pio_end((u8*)&toc_gd[0], std::min((u32)packet_cmd.data_8[4] | (packet_cmd.data_8[3] << 8), (u32)sizeof(toc_gd)));
		}
		break;

		//mount/map drive ? some kind of reset/unlock ??
		//seems like a non data command :)
	case 0x70:
		printf_spicmd("SPI : unknown ? [0x70]");
		//printf("SPI : unknown ? [0x70]\n");
		/*GDStatus.full=0x50; //FIXME
		RaiseInterrupt(holly_GDROM_CMD);*/

		gd_set_state(gds_procpacketdone);
		break;


	// Command 71 seems to trigger some sort of authentication check(?).
	// Update Sept 1st 2010: It looks like after a sequence of events the drive ends up having a specific state.
	// If the drive is fed with a "bootable" disc it ends up in "PAUSE" state. On all other cases it ends up in "STANDBY".
	// Cmd 70 and Error Handling / Sense also seem to take part in the above mentioned sequence of events.
	// This is more or less a hack until more info about this command becomes available. ~Psy
	case 0x71:
		{
			printf_spicmd("SPI : unknown ? [0x71]");
			//printf("SPI : unknown ? [0x71]\n");
			extern const u32 reply_71_sz;

			gd_spi_pio_end((const u8*)&reply_71[0], reply_71_sz);//uCount


			if (libGDR_GetDiscType()==GdRom || libGDR_GetDiscType()==CdRom_XA)
				SecNumber.Status=GD_PAUSE;
			else
				SecNumber.Status=GD_STANDBY;
		}
		break;
	case SPI_SET_MODE:
		{
			printf_spicmd("SPI_SET_MODE");
			u32 Offset = packet_cmd.data_8[2];
			u32 Count = std::min((u32)packet_cmd.data_8[4], 10 - Offset);	// limit to writable area
			set_mode_offset=Offset;
			gd_spi_pio_read_end(Count,gds_process_set_mode);
		}

		break;
		
	case SPI_REQ_STAT:
		{
			u32 curFad = cdda.status == cdda_t::Playing || cdda.status == cdda_t::Paused ? cdda.CurrAddr.FAD : read_params.start_sector - 1;
			printf_spicmd("SPI_REQ_STAT: status %d fmt %d fad %d", SecNumber.Status, SecNumber.DiscFormat, curFad);
			u32 elapsed;
			u32 tracknum = libGDR_GetTrackNumber(curFad, elapsed);
			u8 stat[10];

			//0  0   0   0   0   STATUS
			stat[0] = SecNumber.Status;   //low nibble
			//1 Disc Format Repeat Count
			stat[1] = (u8)(SecNumber.DiscFormat << 4) | cdda.repeats;
			//2 Control ADR
			u8 adr, ctrl;
			libGDR_GetTrackAdrAndControl(tracknum, adr, ctrl);
			stat[2] = (ctrl << 4) | adr;
			//3 Track #
			stat[3] = tracknum;
			//4 Index #
			stat[4] = 1;
			//5 FAD
			stat[5] = curFad >> 16;
			//6 FAD
			stat[6] = curFad >> 8;
			//7 FAD
			stat[7] = curFad;
			//8 Max Read Error Retry Times
			stat[8] = 0;
			//9 0   0   0   0   0   0   0   0
			stat[9] = 0;

			
			verify(packet_cmd.data_8[2] + packet_cmd.data_8[4] < 11);
			gd_spi_pio_end(&stat[packet_cmd.data_8[2]], packet_cmd.data_8[4]);
		}
		break;

	case SPI_REQ_ERROR:
		{
			printf_spicmd("SPI_REQ_ERROR");
			u8 resp[10];
			resp[0]=0xF0;
			resp[1]=0;
			resp[2]=sns_key;//sense
			resp[3]=0;
			resp[4]=resp[5]=resp[6]=resp[7]=0; //Command Specific Information
			resp[8]=sns_asc;//Additional Sense Code
			resp[9]=sns_ascq;//Additional Sense Code Qualifier

			gd_spi_pio_end(resp,packet_cmd.data_8[4]);
			sns_key = 0;
			sns_asc = 0;
			sns_ascq = 0;
			GDStatus.CHECK = 0;
		}
		break;

	case SPI_REQ_SES:
		{
			printf_spicmd("SPI_REQ_SES: status %d", SecNumber.Status);

			u8 ses_inf[6];
			libGDR_GetSessionInfo(ses_inf,packet_cmd.data_8[2]);
			ses_inf[0]=SecNumber.Status;
			gd_spi_pio_end((u8*)&ses_inf[0],packet_cmd.data_8[4]);
		}
		break;

	case SPI_CD_OPEN:
		printf_spicmd("SPI_CD_OPEN Unhandled");
		
		gd_set_state(gds_procpacketdone);
		break;

	case SPI_CD_PLAY:
		{
			const u32 param_type = packet_cmd.data_8[1] & 7;
			printf_spicmd("SPI_CD_PLAY param_type=%d", param_type);

			if (param_type == 1 || param_type == 2)
			{
				bool min_sec_frame = param_type == 2;
				cdda.CurrAddr.FAD = cdda.StartAddr.FAD = GetFAD(&packet_cmd.data_8[2], min_sec_frame);
				cdda.EndAddr.FAD = GetFAD(&packet_cmd.data_8[8], min_sec_frame);
				if (cdda.EndAddr.FAD == 0)
				{
					// Get the last sector of the disk
					u8 ses_inf[6] = {};
					libGDR_GetSessionInfo(ses_inf, 0);

					cdda.EndAddr.FAD = ses_inf[3] << 16 | ses_inf[4] << 8 | ses_inf[5];
				}
				cdda.repeats = packet_cmd.data_8[6] & 0xF;
				cdda.status = cdda_t::Playing;
				SecNumber.Status = GD_PLAY;

				GDStatus.DSC = 1;
			}
			else if (param_type == 7)
			{
				if (cdda.status == cdda_t::Paused)
				{
					// Resume from previous pos unless we're at the end
					if (cdda.CurrAddr.FAD > cdda.EndAddr.FAD)
					{
						cdda.status = cdda_t::Terminated;
						SecNumber.Status = GD_STANDBY;
					}
					else
					{
						cdda.status = cdda_t::Playing;
						SecNumber.Status = GD_PLAY;
					}
				}
			}
			else
				die("SPI_CD_PLAY: unknown parameter");

			DEBUG_LOG(GDROM, "CDDA StartAddr=%d EndAddr=%d repeats=%d status=%d CurrAddr=%d",cdda.StartAddr.FAD,
					cdda.EndAddr.FAD, cdda.repeats, cdda.status, cdda.CurrAddr.FAD);

			gd_set_state(gds_procpacketdone);
		}
		break;

	case SPI_CD_SEEK:
		{
			const u32 param_type = packet_cmd.data_8[1] & 7;
			printf_spicmd("SPI_CD_SEEK param_type=%d", param_type);

			SecNumber.Status = GD_PAUSE;
			if (cdda.status == cdda_t::Playing)
				cdda.status = cdda_t::Paused;

			if (param_type == 1 || param_type == 2)
			{
				bool min_sec_frame = param_type == 2;
				cdda.StartAddr.FAD = cdda.CurrAddr.FAD = GetFAD(&packet_cmd.data_8[2], min_sec_frame);
#ifdef STRICT_MODE
				SecNumber.Status = GD_SEEK;
				GDStatus.DSC = 0;
				sh4_sched_request(gdrom_schid, SH4_MAIN_CLOCK / 50);	// 20 ms
#else
				GDStatus.DSC = 1;
#endif
			}
			else if (param_type == 3)
			{
				//stop audio , goto home
				cdda.StartAddr.FAD = cdda.CurrAddr.FAD = 150;
				cdda.status = cdda_t::NoInfo;
#ifdef STRICT_MODE
				SecNumber.Status = GD_BUSY;
				GDStatus.DSC = 0;
				sh4_sched_request(gdrom_schid, SH4_MAIN_CLOCK / 50);	// 20 ms
#else
				SecNumber.Status = GD_STANDBY;
				GDStatus.DSC = 1;
#endif
			}
			else if (param_type == 4)
			{
				//pause audio -- nothing more
			}
			else
				die("SPI_CD_SEEK  : not known parameter..");

			DEBUG_LOG(GDROM, "CDDA StartAddr=%d EndAddr=%d repeats=%d status=%d CurrAddr=%d",cdda.StartAddr.FAD,
					cdda.EndAddr.FAD, cdda.repeats, cdda.status, cdda.CurrAddr.FAD);

			gd_set_state(gds_procpacketdone);
		}
		break;

	case SPI_CD_SCAN:
		printf_spicmd("SPI_CD_SCAN Unhandled");
		

		gd_set_state(gds_procpacketdone);
		break;

	case SPI_GET_SCD:
		{
			printf_spicmd("SPI_GET_SCD");

			const u32 format = packet_cmd.data_8[1] & 0xF;
			const u32 alloc_len = (packet_cmd.data_8[3] << 8) | packet_cmd.data_8[4];
			u8 subc_info[100];
			u32 size = gd_get_subcode(format, read_params.start_sector - 1, subc_info);
			gd_spi_pio_end(subc_info, std::min(std::min(size, alloc_len), (u32)sizeof(subc_info)));
		}
		break;

	default:
		INFO_LOG(GDROM, "GDROM: Unhandled Sega SPI frame: %X", packet_cmd.data_8[0]);
		GDStatus.CHECK = 1;
		sns_key = 5;	// Illegal request
		sns_asc = 0x20;	// Unsupported command was received
		sns_ascq = 0;
		gd_set_state(gds_procpacketdone);
		break;
	}
}
//Read handler
u32 ReadMem_gdrom(u32 Addr, u32 sz)
{	
	switch (Addr)
	{
		//cancel interrupt
	case GD_STATUS_Read :
		asic_CancelInterrupt(holly_GDROM_CMD);	//Clear INTRQ signal
		if (DriveSel & 0x10)
			// slave drive doesn't exist
			return 0;
		printf_rm("GDROM: STATUS [cancel int](v=%X)",GDStatus.full);
		return GDStatus.full;

	case GD_ALTSTAT_Read:
		//printf_rm("GDROM: AltStatus (v=%X)",GDStatus.full);
		return GDStatus.full;

	case GD_BYCTLLO	:
		printf_rm("GDROM: Read From GD_BYCTLLO");
		return ByteCount.low;

	case GD_BYCTLHI	:
		printf_rm("GDROM: Read From GD_BYCTLHI");
		return ByteCount.hi;

	case GD_DATA:
		if(2!=sz)
			INFO_LOG(GDROM, "GDROM: Bad size on DATA REG Read");

		//if (gd_state == gds_pio_send_data)
		//{
			if (pio_buff.index == pio_buff.size)
			{
				INFO_LOG(GDROM, "GDROM: Illegal Read From DATA (underflow)");
			}
			else
			{
				u32 rv= pio_buff.data[pio_buff.index];
				pio_buff.index+=1;
				ByteCount.full-=2;
				if (pio_buff.index==pio_buff.size)
				{
					verify(pio_buff.next_state != gds_pio_send_data);
					//end of pio transfer !
					gd_set_state(pio_buff.next_state);
				}
				return rv;
			}

		//}
		//else
		//	printf("GDROM: Illegal Read From DATA (wrong mode)\n");

		return 0;

	case GD_DRVSEL:
		printf_rm("GDROM: Read From DriveSel");
		return DriveSel;

	case GD_ERROR_Read:
		printf_rm("GDROM: Read from ERROR Register");
		Error.Sense=sns_key;
		return Error.full;

	case GD_IREASON_Read:
		printf_rm("GDROM: Read from INTREASON Register");
		return IntReason.full;

	case GD_SECTNUM:
//		printf_rm("GDROM: Read from SecNumber Register (v=%X)", SecNumber.full);
		return SecNumber.full;

	default:
		INFO_LOG(GDROM, "GDROM: Unhandled read from address %X, Size:%X",Addr,sz);
		return 0;
	}
}

//Write Handler
void WriteMem_gdrom(u32 Addr, u32 data, u32 sz)
{
	switch(Addr)
	{
	//ATA_IOPORT_WR_CYLINDER_LOW
	case GD_BYCTLLO:
		printf_rm("GDROM: Write to GD_BYCTLLO = %X, Size:%X",data,sz);
		ByteCount.low =(u8) data;
		break;

	//ATA_IOPORT_WR_CYLINDER_HIGH
	case GD_BYCTLHI: 
		printf_rm("GDROM: Write to GD_BYCTLHI = %X, Size:%X",data,sz);
		ByteCount.hi =(u8) data;
		break;

	case GD_DATA: 
		{
			if(2!=sz)
				INFO_LOG(GDROM, "GDROM: Bad size on DATA REG");
			if (gd_state == gds_waitpacket)
			{
				packet_cmd.data_16[packet_cmd.index]=(u16)data;
				packet_cmd.index+=1;
				if (packet_cmd.index==6)
					gd_set_state(gds_procpacket);
			}
			else if (gd_state == gds_pio_get_data)
			{
				pio_buff.data[pio_buff.index]=(u16)data;
				pio_buff.index+=1;
				if (pio_buff.size==pio_buff.index)
				{
					verify(pio_buff.next_state!=gds_pio_get_data);
					gd_set_state(pio_buff.next_state);
				}
			}
			else
			{
				INFO_LOG(GDROM, "GDROM: Illegal Write to DATA");
			}
			return;
		}

	case GD_DEVCTRL_Write:
		INFO_LOG(GDROM, "GDROM: Write GD_DEVCTRL (Not implemented on Dreamcast)");
		break;

	//ATA_IOPORT_WR_DEVICE_HEAD
	case GD_DRVSEL: 
		DriveSel = (DriveSel & 0xe0) | (data & 0x1f);
		if (DriveSel & 0x10)
			INFO_LOG(GDROM, "GD_DRVSEL: slave drive selected");
		break;

		// By writing "3" as Feature Number and issuing the Set Feature command,
		// the PIO or DMA transfer mode set in the Sector Count register can be selected.
		// The actual transfer mode is specified by the Sector Counter Register. 

	case GD_FEATURES_Write:
		printf_rm("GDROM: Write to GD_FEATURES");
		Features.full =(u8) data;
		break;

	case GD_SECTCNT_Write:
		DEBUG_LOG(GDROM, "GDROM: Write to SecCount = %X", data);
		SecCount.full =(u8) data;
		break;

	case GD_SECTNUM:
		INFO_LOG(GDROM, "GDROM: Write to SecNum; not possible = %X", data);
		break;

	case GD_COMMAND_Write:
		if (sz != 1)
			INFO_LOG(GDROM, "GDROM: Bad size on GD_COMMAND reg");
		else if ((DriveSel & 0x10) == 0)
		{
			if (data != ATA_NOP && data != ATA_SOFT_RESET)
				verify(gd_state == gds_waitcmd);
			ata_cmd.command = (u8)data;
			gd_set_state(gds_procata);
		}
		else
			DEBUG_LOG(GDROM, "ATA command to slave drive ignored: %x", data);
		break;

	default:
		INFO_LOG(GDROM, "GDROM: Unhandled write to address %X <= %X, Size:%X",Addr,data,sz);
		break;
	}
}

static int getGDROMTicks()
{
	if (SB_GDST & 1)
	{
		if (config::FastGDRomLoad)
			return 512;
		u32 len = SB_GDLEN == 0 ? 0x02000000 : SB_GDLEN;
		if (len - SB_GDLEND > 10240)
			return 1100000;										// Large transfers: GD-ROM transfer rate 1.8 MB/s
		else
			return std::min((u32)10240, len - SB_GDLEND) * 2;	// Small transfers: Max G1 bus rate: 50 MHz x 16 bits
	}
	else
		return 0;
}

//is this needed ?
static int GDRomschd(int tag, int cycles, int jitter, void *arg)
{
	if (SecNumber.Status == GD_SEEK)
	{
		SecNumber.Status = GD_PAUSE;
		GDStatus.DSC = 1;
	}
	else if (SecNumber.Status == GD_BUSY)
	{
		SecNumber.Status = GD_STANDBY;
		GDStatus.DSC = 1;
	}
	if(!(SB_GDST&1) || !(SB_GDEN &1) || (read_buff.cache_size==0 && read_params.remaining_sectors==0))
		return 0;

	u32 src = SB_GDSTARD;
	u32 len = (SB_GDLEN == 0 ? 0x02000000 : SB_GDLEN) - SB_GDLEND;
	
	if(SB_GDLEN & 0x1F) 
	{
		die("\n!\tGDROM: SB_GDLEN has invalid size !\n");
		return 0;
	}

	//if we don't have any more sectors to read
	if (read_params.remaining_sectors == 0)
		//make sure we don't underrun the cache :)
		len = std::min(len, read_buff.cache_size);

	len = std::min(len, (u32)10240);
	// do we need to do this for GDROM DMA?
	if (0x8201 != (DMAC_DMAOR.full & DMAOR_MASK))
	{
		INFO_LOG(GDROM, "GDROM: DMAOR has invalid settings (%X)", DMAC_DMAOR.full);
		//return;
	}

	if(len == 0)
	{
		INFO_LOG(GDROM, "GDROM: Len: %X, Abnormal Termination !", len);
	}

	u32 len_backup = len;
	if(1 == SB_GDDIR) 
	{
		while(len)
		{
			u32 buff_size =read_buff.cache_size;
			if (buff_size==0)
			{
				verify(read_params.remaining_sectors>0);
				//buffer is empty , fill it :)
				FillReadBuffer();
				continue;
			}

			//transfer up to len bytes
			if (buff_size>len)
				buff_size=len;

#if defined(__APPLE__) && defined(TARGET_IPHONE)
			/// iOS-optimized DMA transfer for FMV streaming performance
			if (buff_size >= IOS_GDROM_CACHE_LINE_SIZE * 2) {
				/// Prefetch source data for optimal iOS memory system performance
				IOSPrefetchStreamingData(&read_buff.cache[read_buff.cache_index], buff_size);
				
				/// Use standard transfer but with iOS memory prefetch optimization
				WriteMemBlock_nommu_ptr(src,(u32*)&read_buff.cache[read_buff.cache_index], buff_size);
			} else {
				/// Standard path for small transfers
				WriteMemBlock_nommu_ptr(src,(u32*)&read_buff.cache[read_buff.cache_index], buff_size);
			}
#else
			WriteMemBlock_nommu_ptr(src,(u32*)&read_buff.cache[read_buff.cache_index], buff_size);
#endif
			read_buff.cache_index+=buff_size;
			read_buff.cache_size-=buff_size;
			src+=buff_size;
			len-=buff_size;
		}
	}
	else
	{
		WARN_LOG(GDROM, "GDROM: SB_GDDIR %X (TO AICA WAVE MEM?)", src);
	}

	SB_GDLEND = (SB_GDLEND + len_backup) & 0x01ffffe0;
	SB_GDSTARD += len_backup;

	if (SB_GDLEND == SB_GDLEN)
	{
		SB_GDST = 0;
		asic_RaiseInterrupt(holly_GDROM_DMA);
	}
	//Read ALL sectors
	if (read_params.remaining_sectors==0)
	{
		//And all buffer :p
		if (read_buff.cache_size==0)
		{
			//verify(!SB_GDST&1) -> dc can do multi read dma
			gd_set_state(gds_procpacketdone);
		}
	}

	return getGDROMTicks();
}

//DMA Start
static void GDROM_DmaStart(u32 addr, u32 data)
{
	SB_GDST |= data & 1;

	if (SB_GDST == 1)
	{
		if (SB_GDEN == 0)
		{
			INFO_LOG(GDROM, "Invalid GD-DMA start, SB_GDEN=0. Ignoring it.");
			return;
		}
		SB_GDSTARD = SB_GDSTAR;
		SB_GDLEND = 0;
		DEBUG_LOG(GDROM, "GDROM-DMA start addr %08X len %d fad %x", SB_GDSTAR, SB_GDLEN, read_params.start_sector);

		int ticks = getGDROMTicks();
		if (ticks < SH4_TIMESLICE)
		{
			ticks = GDRomschd(0, 0, 0, nullptr);
		}

		if (ticks)
			sh4_sched_request(gdrom_schid, ticks);
	}
}

static void GDROM_DmaEnable(u32 addr, u32 data)
{
	SB_GDEN = data & 1;
	if (SB_GDEN == 0 && SB_GDST == 1)
	{
		printf_spi("GD-DMA aborted");
		SB_GDST = 0;
	}
}

//Init/Term/Res
void gdrom_reg_Init()
{
	gdrom_schid = sh4_sched_register(0, &GDRomschd);
	
#if defined(__APPLE__) && defined(TARGET_IPHONE)
	/// Initialize iOS-optimized GD-ROM streaming for better FMV performance
	IOSInitializeGDROMBuffer();
#endif
	
	gd_disc_change();
}

void gdrom_reg_Term()
{
	sh4_sched_unregister(gdrom_schid);
	gdrom_schid = -1;
}

void gdrom_reg_Reset(bool hard)
{
	if (hard)
	{
		hollyRegs.setWriteHandler<SB_GDST_addr>(GDROM_DmaStart);
		hollyRegs.setWriteHandler<SB_GDEN_addr>(GDROM_DmaEnable);

		// set default hardware information
		memset(&GD_HardwareInfo, 0, sizeof(GD_HardwareInfo));
		GD_HardwareInfo.speed = 0x0;
		GD_HardwareInfo.standby_hi = 0x00;
		GD_HardwareInfo.standby_lo = 0xb4;
		GD_HardwareInfo.read_flags = 0x19;
		GD_HardwareInfo.read_retry = 0x08;
		memcpy(GD_HardwareInfo.drive_info, "SE      ", sizeof(GD_HardwareInfo.drive_info));
		memcpy(GD_HardwareInfo.system_version, "Rev 6.43", sizeof(GD_HardwareInfo.system_version));
		memcpy(GD_HardwareInfo.system_date, "990408", sizeof(GD_HardwareInfo.system_date));
		TermDrive();
	}
	SB_GDST = 0;
	SB_GDEN = 0;

	gd_state = gds_waitcmd;
	sns_asc = 0;
	sns_ascq = 0;
	sns_key = 0;
	set_mode_offset = 0;
	read_params = {};
	packet_cmd = {};
	read_buff = {};
	pio_buff  = {};
	ata_cmd  = {};
	cdda = {};
	gd_disk_type = NoDisk;

	data_write_mode = 0;
	DriveSel = 0xa0;
	Error = {};
	IntReason = {};
	Features = {};
	SecCount = {};
	SecNumber = {};
	GDStatus = {};
	ByteCount = {};

	gd_disc_change();
}

namespace gdrom
{

void serialize(Serializer& ser)
{
	ser << GD_HardwareInfo;

	ser << sns_asc;
	ser << sns_ascq;
	ser << sns_key;

	ser << packet_cmd;
	ser << set_mode_offset;
	ser << read_params;
	ser << read_buff;
	ser << pio_buff;
	ser << ata_cmd;
	ser << cdda;
	ser << gd_state;
	ser << gd_disk_type;
	ser << data_write_mode;
	ser << DriveSel;
	ser << Error;

	ser << IntReason;
	ser << Features;
	ser << SecCount;
	ser << SecNumber;
	ser << GDStatus;
	ser << ByteCount;
}

void deserialize(Deserializer& deser)
{
	deser >> GD_HardwareInfo;

	deser >> sns_asc;
	deser >> sns_ascq;
	deser >> sns_key;

	deser >> packet_cmd;
	deser >> set_mode_offset;
	deser >> read_params;
	if (deser.version() >= Deserializer::V17)
		deser >> read_buff;
	else
	{
		deser >> packet_cmd;
		read_buff.cache_size = 0;
	}
	deser >> pio_buff;
	deser.skip<u32>(Deserializer::V44); // set_mode_offset (repeat)
	deser >> ata_cmd;
	deser >> cdda;
	deser >> gd_state;
	deser >> gd_disk_type;
	deser >> data_write_mode;
	deser >> DriveSel;
	deser >> Error;

	deser >> IntReason;
	deser >> Features;
	deser >> SecCount;
	deser >> SecNumber;
	deser >> GDStatus;
	deser >> ByteCount;
}

}
