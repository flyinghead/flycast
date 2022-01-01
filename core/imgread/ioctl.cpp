#include "build.h"
#if defined(_WIN32) && !defined(TARGET_UWP)
#include "types.h"
#include "common.h"

#include <cstddef>
#include <windows.h>

#include <ntddcdrm.h>
#include <ntddscsi.h>

#ifdef _MSC_VER
#define _NTSCSI_USER_MODE_
#include <scsi.h>
#undef _NTSCSI_USER_MODE_
#else
#define CD_RAW_READ_SUBCODE_SIZE               (         96)

#pragma pack(push, cdb, 1)
typedef union _CDB {
    struct _READ_CD {
        UCHAR OperationCode;    // 0xBE - SCSIOP_READ_CD
        UCHAR RelativeAddress : 1;
        UCHAR Reserved0 : 1;
        UCHAR ExpectedSectorType : 3;
        UCHAR Lun : 3;
        UCHAR StartingLBA[4];
        UCHAR TransferBlocks[3];
        UCHAR Reserved2 : 1;
        UCHAR ErrorFlags : 2;
        UCHAR IncludeEDC : 1;
        UCHAR IncludeUserData : 1;
        UCHAR HeaderCode : 2;
        UCHAR IncludeSyncData : 1;
        UCHAR SubChannelSelection : 3;
        UCHAR Reserved3 : 5;
        UCHAR Control;
    } READ_CD;
} CDB, *PCDB;
#pragma pack(pop, cdb)

#define READ_TOC_FORMAT_FULL_TOC    0x02

#define SCSIOP_READ                     0x28
#define SCSIOP_READ_CD                  0xBE
#endif

#define RAW_SECTOR_SIZE         2352
#define CD_SECTOR_SIZE          2048
#define SECTORS_AT_READ         20
#define CD_BLOCKS_PER_SECOND    75

struct spti_s 
{
	SCSI_PASS_THROUGH_DIRECT sptd;
	DWORD alignmentDummy;
	BYTE  senseBuf[0x12];
};

ULONG msf2fad(const UCHAR Addr[4])
{
	ULONG Sectors = ( Addr[0] * (CD_BLOCKS_PER_SECOND*60) ) + ( Addr[1]*CD_BLOCKS_PER_SECOND) + Addr[2];
	return Sectors;
}


// Msf: Hours, Minutes, Seconds, Frames
ULONG AddressToSectors( UCHAR Addr[4] );


bool spti_SendCommand(HANDLE hand,spti_s& s,SCSI_ADDRESS& ioctl_addr)
{
	s.sptd.Length             = sizeof(SCSI_PASS_THROUGH_DIRECT);
	s.sptd.PathId             = ioctl_addr.PathId;
	s.sptd.TargetId           = ioctl_addr.TargetId;
	s.sptd.Lun                = ioctl_addr.Lun;
	s.sptd.TimeOutValue       = 30;
	//s.sptd.CdbLength        = 0x0A;
	s.sptd.SenseInfoLength    = 0x12;
	s.sptd.SenseInfoOffset    = offsetof(spti_s, senseBuf);
//	s.sptd.DataIn             = SCSI_IOCTL_DATA_IN;
//	s.sptd.DataTransferLength = 0x800;
//	s.sptd.DataBuffer         = pdata;

	DWORD bytesReturnedIO = 0;
	if(!DeviceIoControl(hand, IOCTL_SCSI_PASS_THROUGH_DIRECT, &s, sizeof(s), &s, sizeof(s), &bytesReturnedIO, NULL)) 
		return false;

	if(s.sptd.ScsiStatus)
		return false;
	return true;
}

bool spti_Read10(HANDLE hand,void * pdata,u32 sector,SCSI_ADDRESS& ioctl_addr)
{
	spti_s s;
	memset(&s,0,sizeof(spti_s));

	s.sptd.Cdb[0] = SCSIOP_READ;
	s.sptd.Cdb[1] = (ioctl_addr.Lun&7) << 5;// | DPO ;	DPO = 8

	s.sptd.Cdb[2] = (BYTE)(sector >> 0x18 & 0xFF); // MSB
	s.sptd.Cdb[3] = (BYTE)(sector >> 0x10 & 0xFF);
	s.sptd.Cdb[4] = (BYTE)(sector >> 0x08 & 0xFF);
	s.sptd.Cdb[5] = (BYTE)(sector >> 0x00 & 0xFF); // LSB

	s.sptd.Cdb[7] = 0;
	s.sptd.Cdb[8] = 1;
	
	s.sptd.CdbLength          = 0x0A;
	s.sptd.DataIn             = SCSI_IOCTL_DATA_IN;
	s.sptd.DataTransferLength = 0x800;
	s.sptd.DataBuffer         = pdata;

	return spti_SendCommand(hand,s,ioctl_addr);
}
bool spti_ReadCD(HANDLE hand,void * pdata,u32 sector,SCSI_ADDRESS& ioctl_addr)
{
	spti_s s;
	memset(&s,0,sizeof(spti_s));
	CDB& r = *(PCDB)s.sptd.Cdb;

	r.READ_CD.OperationCode = SCSIOP_READ_CD;

	r.READ_CD.StartingLBA[0] = (BYTE)(sector >> 0x18 & 0xFF);
	r.READ_CD.StartingLBA[1] = (BYTE)(sector >> 0x10 & 0xFF);
	r.READ_CD.StartingLBA[2] = (BYTE)(sector >> 0x08 & 0xFF);
	r.READ_CD.StartingLBA[3] = (BYTE)(sector >> 0x00 & 0xFF);

	// 1 sector
	r.READ_CD.TransferBlocks[0] = 0;
	r.READ_CD.TransferBlocks[1] = 0;
	r.READ_CD.TransferBlocks[2] = 1;

	// 0xF8
	r.READ_CD.IncludeSyncData = 1;
	r.READ_CD.HeaderCode = 3;
	r.READ_CD.IncludeUserData = 1;
	r.READ_CD.IncludeEDC = 1;

	r.READ_CD.SubChannelSelection = 1;

	s.sptd.CdbLength          = 12;
	s.sptd.DataIn             = SCSI_IOCTL_DATA_IN;
	s.sptd.DataTransferLength = 2448;
	s.sptd.DataBuffer         = pdata;
	return spti_SendCommand(hand,s,ioctl_addr);
}

struct PhysicalDrive;
struct PhysicalTrack:TrackFile
{
	PhysicalDrive* disc;
	PhysicalTrack(PhysicalDrive* disc) { this->disc=disc; }

	bool Read(u32 FAD,u8* dst,SectorFormat* sector_type,u8* subcode,SubcodeFormat* subcode_type) override;
};

struct PhysicalDrive:Disc
{
	HANDLE drive;
	SCSI_ADDRESS scsi_addr;
	bool use_scsi;

	PhysicalDrive()
	{
		drive=INVALID_HANDLE_VALUE;
		memset(&scsi_addr,0,sizeof(scsi_addr));
		use_scsi=false;
	}

	bool Build(char* path)
	{
		drive = CreateFile( path, GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL, OPEN_EXISTING, 0, NULL);

		if ( INVALID_HANDLE_VALUE == drive )
			return false; //failed to open

		printf(" Opened device %s, reading TOC ...",path);
		// Get track-table and parse it
		CDROM_READ_TOC_EX tocrq={0};
		 
	 	tocrq.Format = READ_TOC_FORMAT_FULL_TOC;
	 	tocrq.Msf=1;
	 	tocrq.SessionTrack=1;
		u8 buff[2048];
	 	CDROM_TOC_FULL_TOC_DATA *ftd=(CDROM_TOC_FULL_TOC_DATA*)buff;
	
		ULONG BytesRead;
		memset(buff,0,sizeof(buff));
		int code = DeviceIoControl(drive,IOCTL_CDROM_READ_TOC_EX,&tocrq,sizeof(tocrq),ftd, 2048, &BytesRead, NULL);
		
//		CDROM_TOC toc;
		int currs=-1;
		if (0==code)
		{
			printf(" failed\n");
			//failed to read toc
			CloseHandle(drive);
			return false;
		}
		else
		{
			printf(" done !\n");

			type=CdRom_XA;

			BytesRead-=sizeof(CDROM_TOC_FULL_TOC_DATA);
			BytesRead/=sizeof(ftd->Descriptors[0]);

			for (u32 i=0;i<BytesRead;i++)
			{
				if (ftd->Descriptors[i].Point==0xA2)
				{
					this->EndFAD=msf2fad(ftd->Descriptors[i].Msf);
					continue;
				}
				if (ftd->Descriptors[i].Point>=1 && ftd->Descriptors[i].Point<=0x63 &&
					ftd->Descriptors[i].Adr==1)
				{
					u32 trackn=ftd->Descriptors[i].Point-1;
					verify(trackn==tracks.size());
					Track t;

					t.ADDR=ftd->Descriptors[i].Adr;
					t.CTRL=ftd->Descriptors[i].Control;
					t.StartFAD=msf2fad(ftd->Descriptors[i].Msf);
					t.file = new PhysicalTrack(this);

					tracks.push_back(t);

					if (currs!=ftd->Descriptors[i].SessionNumber)
					{
						currs=ftd->Descriptors[i].SessionNumber;
						verify(sessions.size()==(currs-1));
						Session s;
						s.FirstTrack=trackn+1;
						s.StartFAD=t.StartFAD;

						sessions.push_back(s);
					}
				}
			}
			LeadOut.StartFAD=EndFAD;
			LeadOut.ADDR=0;
			LeadOut.CTRL=0;
		}

		DWORD bytesReturnedIO = 0;
		BOOL resultIO = DeviceIoControl(drive, IOCTL_SCSI_GET_ADDRESS, NULL, 0, &scsi_addr, sizeof(scsi_addr), &bytesReturnedIO, NULL);
		//done !
		if (resultIO)
			use_scsi=true;
		else
			use_scsi=false;

		return true;
	}
};

bool PhysicalTrack::Read(u32 FAD,u8* dst,SectorFormat* sector_type,u8* subcode,SubcodeFormat* subcode_type)
{
	u32 fmt=0;
	static u8 temp[2500];

	u32 LBA=FAD-150;

	if (disc->use_scsi)
	{
		if (!spti_ReadCD(disc->drive, temp,LBA,disc->scsi_addr))
		{
			if (spti_Read10(disc->drive, dst,LBA,disc->scsi_addr))
			{
				//sector read success, just user data
				*sector_type=SECFMT_2048_MODE2_FORM1; //m2f1 seems more common ? is there some way to detect it properly here?
				return true;
			}
		}
		else
		{
			//sector read success, with subcode
			memcpy(dst, temp, 2352);
			memcpy(subcode, temp + 2352, CD_RAW_READ_SUBCODE_SIZE);

			*sector_type=SECFMT_2352;
			*subcode_type=SUBFMT_96;
			return true;
		}
	}

	//hmm, spti failed/cannot be used


	//try IOCTL_CDROM_RAW_READ


	static __RAW_READ_INFO Info;
	
	Info.SectorCount=1;
	Info.DiskOffset.QuadPart = LBA * CD_SECTOR_SIZE; //CD_SECTOR_SIZE, even though we read RAW sectors. Its how winapi works.
	ULONG Dummy;
	
	//try all 3 track modes, starting from the one that succeeded last time (Info is static) to save time !
	for (int tr=0;tr<3;tr++)
	{
		if ( 0 == DeviceIoControl( disc->drive, IOCTL_CDROM_RAW_READ, &Info, sizeof(Info), dst, RAW_SECTOR_SIZE, &Dummy, NULL ) )
		{
			Info.TrackMode=(TRACK_MODE_TYPE)((Info.TrackMode+1)%3);	//try next mode
		}
		else
		{
			//sector read success
			*sector_type=SECFMT_2352;
			return true;
		}
	}

	//finally, try ReadFile
	if (SetFilePointer(disc->drive,LBA*2048,0,FILE_BEGIN)!=INVALID_SET_FILE_POINTER)
	{
		DWORD BytesRead;
		if (FALSE!=ReadFile(disc->drive,dst,2048,&BytesRead,0) && BytesRead==2048)
		{
			//sector read success, just user data
			*sector_type=SECFMT_2048_MODE2_FORM1; //m2f1 seems more common ? is there some way to detect it properly here?
			return true;
		}
	}

	printf("IOCTL: Totally failed to read sector @LBA %d\n", LBA);
	return false;
}


Disc* ioctl_parse(const char* file, std::vector<u8> *digest)
{
	
	if (strlen(file)==3 && GetDriveType(file)==DRIVE_CDROM)
	{
		printf("Opening device %s ...",file);
		char fn[]={ '\\', '\\', '.', '\\', file[0],':', '\0' };
		PhysicalDrive* rv = new PhysicalDrive();	

		if (rv->Build(fn))
		{
			if (digest != nullptr)
				digest->clear();
			return rv;
		}
		else
		{
			delete rv;
			return 0;
		}
	}
	else
	{
		return 0;
	}
}
#endif
