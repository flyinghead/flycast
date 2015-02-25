/*
	Extremely primitive bios replacement

	Many thanks to Lars Olsson (jlo@ludd.luth.se) for bios decompile work
		http://www.ludd.luth.se/~jlo/dc/bootROM.c
		http://www.ludd.luth.se/~jlo/dc/bootROM.h
		http://www.ludd.luth.se/~jlo/dc/security_stuff.c
*/

#include "reios.h"

#include "gdrom_hle.h"
#include "descrambl.h"

#include "hw/sh4/sh4_mem.h"

#include <map>

//#define debugf printf

#define debugf(...) 

#define dc_bios_syscall_system				0x8C0000B0
#define dc_bios_syscall_font				0x8C0000B4
#define dc_bios_syscall_flashrom			0x8C0000B8
#define dc_bios_syscall_gd					0x8C0000BC
#define dc_bios_syscall_misc				0x8c0000E0

//At least one game (ooga) uses this directly
#define dc_bios_entrypoint_gd_do_bioscall	0x8c0010F0

#define SYSINFO_ID_ADDR 0x8C001010

u8* biosrom;
u8* flashrom;
u32 base_fad = 45150;
bool descrambl = false;

bool reios_locate_bootfile(const char* bootfile="1ST_READ.BIN") {
	u32 data_len = 2048 * 1024;
	u8* temp = new u8[data_len];

	libGDR_ReadSector(temp, base_fad + 16, 1, 2048);

	if (memcmp(temp, "\001CD001\001", 7) == 0) {
		u32 lba = (u32&)temp[156 + 2]; //Root directory lba, should use the big endian one
		u32 len = (u32&)temp[156 + 10]; //should use the big endian one
		
		data_len = ((len + 2047) / 2048) *2048;

		libGDR_ReadSector(temp, 150 + lba, data_len/2048, 2048);
	}
	else {
		libGDR_ReadSector(temp, base_fad + 16, data_len / 2048, 2048);
	}

	for (int i = 0; i < (data_len-20); i++) {
		if (memcmp(temp+i, bootfile, strlen(bootfile)) == 0){
			printf("Found %s at %06X\n", bootfile, i);
			
			printf("filename len: %d\n", temp[i - 1]);
			printf("file LBA: %d, %d\n", (u32&)temp[i - 33 + 2], (u32&)temp[i - 33 + 2 + 4]);
			printf("file LEN: %d, %d\n", (u32&)temp[i - 33 + 10], (u32&)temp[i - 33 + 10 + 4]);

			u32 lba = (u32&)temp[i - 33 +  2]; //should use the big endian one, but i'm lazy
			u32 len = (u32&)temp[i - 33 + 10]; //should use the big endian one, but i'm lazy

			if (descrambl)
				descrambl_file(lba + 150, len, GetMemPtr(0x8c010000, 0));
			else
				libGDR_ReadSector(GetMemPtr(0x8c010000, 0), lba + 150, (len + 2047) / 2048, 2048);

			if (false) {
				FILE* f = fopen("z:\\1stboot.bin", "wb");
				fwrite(GetMemPtr(0x8c010000, 0), 1, len, f);
				fclose(f);
			}
			
			delete[] temp;
			return true;
		}
	}

	delete[] temp;
	return false;
}

char reios_bootfile[32];
const char* reios_locate_ip() {

	if (libGDR_GetDiscType() == GdRom) {
		base_fad = 45150;
		descrambl = false;
	}
	else {
		u8 ses[6];
		libGDR_GetSessionInfo(ses, 0);
		libGDR_GetSessionInfo(ses, ses[2]);
		base_fad = (ses[3] << 16) | (ses[4] << 8) | (ses[5] << 0);
		descrambl = true;
	}

	libGDR_ReadSector(GetMemPtr(0x8c008000, 0), base_fad, 16, 2048);
	
	memset(reios_bootfile, 0, sizeof(reios_bootfile));
	memcpy(reios_bootfile, GetMemPtr(0x8c008060, 0), 16);

	for (int i = 15; i >= 0; i--) {
		if (reios_bootfile[i] != ' ')
			break;
		reios_bootfile[i] = 0;
	}
	/*
		Post Boot registers from actual bios boot
	r
		[0x00000000]	0xac0005d8
		[0x00000001]	0x00000009
		[0x00000002]	0xac00940c
		[0x00000003]	0x00000000
		[0x00000004]	0xac008300
		[0x00000005]	0xf4000000
		[0x00000006]	0xf4002000
		[0x00000007]	0x00000070
		[0x00000008]	0x00000000
		[0x00000009]	0x00000000
		[0x0000000a]	0x00000000
		[0x0000000b]	0x00000000
		[0x0000000c]	0x00000000
		[0x0000000d]	0x00000000
		[0x0000000e]	0x00000000
		[0x0000000f]	0x8d000000
	mac	
		l	0x5bfcb024
		h	0x00000000
	r_bank
		[0x00000000]	0xdfffffff
		[0x00000001]	0x500000f1
		[0x00000002]	0x00000000
		[0x00000003]	0x00000000
		[0x00000004]	0x00000000
		[0x00000005]	0x00000000
		[0x00000006]	0x00000000
		[0x00000007]	0x00000000
	gbr	0x8c000000
	ssr	0x40000001
	spc	0x8c000776
	sgr	0x8d000000
	dbr	0x8c000010
	vbr	0x8c000000
	pr	0xac00043c
	fpul	0x00000000
	pc	0xac008300

	+		sr	{T=1 status = 0x400000f0}
	+		fpscr	{full=0x00040001}
	+		old_sr	{T=1 status=0x400000f0}
	+		old_fpscr	{full=0x00040001}

	*/

	//Setup registers to immitate a normal boot
	sh4rcb.cntx.r[15] = 0x8d000000;

	sh4rcb.cntx.gbr = 0x8c000000;
	sh4rcb.cntx.ssr = 0x40000001;
	sh4rcb.cntx.spc = 0x8c000776;
	sh4rcb.cntx.sgr = 0x8d000000;
	sh4rcb.cntx.dbr = 0x8c000010;
	sh4rcb.cntx.vbr = 0x8c000000;
	sh4rcb.cntx.pr = 0xac00043c;
	sh4rcb.cntx.fpul = 0x00000000;
	sh4rcb.cntx.pc = 0xac008300;

	sh4rcb.cntx.sr.status = 0x400000f0;
	sh4rcb.cntx.sr.T = 1;

	sh4rcb.cntx.old_sr.status = 0x400000f0;

	sh4rcb.cntx.fpscr.full = 0x00040001;
	sh4rcb.cntx.old_fpscr.full = 0x00040001;

	return reios_bootfile;
}


void reios_sys_system() {
	debugf("reios_sys_system\n");

	u32 cmd = Sh4cntx.r[7];

	switch (cmd) {
		case 0:	//SYSINFO_INIT
			Sh4cntx.r[0] = 0;
			break;

		case 2: //SYSINFO_ICON 
		{
			printf("SYSINFO_ICON\n");
			/*
				r4 = icon number (0-9, but only 5-9 seems to really be icons)
				r5 = destination buffer (704 bytes in size)
			*/
			Sh4cntx.r[0] = 704;
		}
		break;

		case 3: //SYSINFO_ID 
		{
			WriteMem32(SYSINFO_ID_ADDR + 0, 0xe1e2e3e4);
			WriteMem32(SYSINFO_ID_ADDR + 4, 0xe5e6e7e8);

			Sh4cntx.r[0] = SYSINFO_ID_ADDR;
		}
		break;

		default:
			printf("unhandled: reios_sys_system\n");
			break;
	}
}

void reios_sys_font() {
	printf("reios_sys_font\n");
}

void reios_sys_flashrom() {
	debugf("reios_sys_flashrom\n");

	u32 cmd = Sh4cntx.r[7];

	u32 flashrom_info[][2] = {
		{ 0 * 1024, 8 * 1024 },
		{ 8 * 1024, 8 * 1024 },
		{ 16 * 1024, 16 * 1024 },
		{ 32 * 1024, 32 * 1024 },
		{ 64 * 1024, 64 * 1024 },
	};

	switch (cmd) {
			case 0: // FLASHROM_INFO 
			{
				/*
					r4 = partition number(0 - 4)
					r5 = pointer to two 32 bit integers to receive the result.
						The first will be the offset of the partition start, in bytes from the start of the flashrom.
						The second will be the size of the partition, in bytes.

						#define FLASHROM_PT_SYSTEM      0   /< \brief Factory settings (read-only, 8K) 
						#define FLASHROM_PT_RESERVED    1   /< \brief reserved (all 0s, 8K) 
						#define FLASHROM_PT_BLOCK_1     2   /< \brief Block allocated (16K) 
						#define FLASHROM_PT_SETTINGS    3   /< \brief Game settings (block allocated, 32K) 
						#define FLASHROM_PT_BLOCK_2     4   /< \brief Block allocated (64K) 
				*/

				u32 part = Sh4cntx.r[4];
				u32 dest = Sh4cntx.r[5];

				u32* pDst = (u32*)GetMemPtr(dest, 8);

				if (part <= 4) {
					pDst[0] = flashrom_info[part][0];
					pDst[1] = flashrom_info[part][1];
					
					Sh4cntx.r[0] = 0;
				}
				else {
					Sh4cntx.r[0] = -1;
				}
			}
			break;

			case 1:	//FLASHROM_READ 
			{
				/*
				r4 = read start position, in bytes from the start of the flashrom
				r5 = pointer to destination buffer
				r6 = number of bytes to read
				*/
				u32 offs = Sh4cntx.r[4];
				u32 dest = Sh4cntx.r[5];
				u32 size = Sh4cntx.r[6];

				memcpy(GetMemPtr(dest, size), flashrom + offs, size);

				Sh4cntx.r[0] = size;
			}
			break;

			
			case 2:	//FLASHROM_WRITE 
			{
				/*
					r4 = write start position, in bytes from the start of the flashrom
					r5 = pointer to source buffer
					r6 = number of bytes to write
				*/

				u32 offs = Sh4cntx.r[4];
				u32 src = Sh4cntx.r[5];
				u32 size = Sh4cntx.r[6];

				u8* pSrc = GetMemPtr(src, size);

				for (int i = 0; i < size; i++) {
					flashrom[offs + i] &= pSrc[i];
				}
			}
			break;

			case 3:	//FLASHROM_DELETE  
			{			
				u32 offs = Sh4cntx.r[4];
				u32 dest = Sh4cntx.r[5];

				u32 part = 5;

				for (int i = 0; i <= 4; i++) {
					if (offs >= flashrom_info[i][0] && offs < (flashrom_info[i][0] + flashrom_info[i][1])) {
						part = i;
						break;
					}
				}

				if (part <= 4) {
					memset(flashrom + flashrom_info[part][0], 0xFF, flashrom_info[part][1]);
					Sh4cntx.r[0] = 0;
				}
				else {
					Sh4cntx.r[0] = -1;
				}
			}
			break;
			
	default:
		printf("reios_sys_flashrom: not handled, %d\n", cmd);
	}
}

void reios_sys_gd() {
	gdrom_hle_op();
}

/*
	- gdGdcReqCmd, 0
	- gdGdcGetCmdStat, 1
	- gdGdcExecServer, 2
	- gdGdcInitSystem, 3
	- gdGdcGetDrvStat, 4
*/
void gd_do_bioscall()
{
	//looks like the "real" entrypoint for this on a dreamcast
	gdrom_hle_op();
	return;

	/*
		int func1, func2, arg1, arg2;
	*/

	switch (Sh4cntx.r[7]) {
	case 0:	//gdGdcReqCmd, wth is r6 ?
		GD_HLE_Command(Sh4cntx.r[4], Sh4cntx.r[5]);
		Sh4cntx.r[0] = 0xf344312e;
		break;

	case 1:	//gdGdcGetCmdStat, r4 -> id as returned by gdGdcReqCmd, r5 -> buffer to get status in ram, r6 ?
		Sh4cntx.r[0] = 0; //All good, no status info
		break;

	case 2: //gdGdcExecServer
		//nop? returns something, though.
		//Bios seems to be based on a cooperative threading model
		//this is the "context" switch entry point
		break;

	case 3: //gdGdcInitSystem
		//nop? returns something, though.
		break;
	case 4: //gdGdcGetDrvStat
		/*
			Looks to same as GDROM_CHECK_DRIVE
		*/
		WriteMem32(Sh4cntx.r[4] + 0, 0x02);	// STANDBY
		WriteMem32(Sh4cntx.r[4] + 4, 0x80);	// CDROM | 0x80 for GDROM
		Sh4cntx.r[0] = 0;					// RET SUCCESS
		break;

	default:
		printf("gd_do_bioscall: (%d) %d, %d, %d\n", Sh4cntx.r[4], Sh4cntx.r[5], Sh4cntx.r[6], Sh4cntx.r[7]);
		break;
	}
	
	//gdGdcInitSystem
}

void reios_sys_misc() {
	printf("reios_sys_misc - r7: 0x%08X, r4 0x%08X, r5 0x%08X, r6 0x%08X\n", Sh4cntx.r[7], Sh4cntx.r[4], Sh4cntx.r[5], Sh4cntx.r[6]);
	Sh4cntx.r[0] = 0;
}

typedef void hook_fp();
u32 hook_addr(hook_fp* fn);

void setup_syscall(u32 hook_addr, u32 syscall_addr) {
	WriteMem32(syscall_addr, hook_addr);
	WriteMem32(hook_addr, REIOS_OPCODE);
}

void reios_boot() {
	//setup syscalls
	//find boot file
	//boot it

	memset(GetMemPtr(0x8C000000, 0), 0xFF, 64 * 1024);

	setup_syscall(hook_addr(&reios_sys_system), dc_bios_syscall_system);
	setup_syscall(hook_addr(&reios_sys_font), dc_bios_syscall_font);
	setup_syscall(hook_addr(&reios_sys_flashrom), dc_bios_syscall_flashrom);
	setup_syscall(hook_addr(&reios_sys_gd), dc_bios_syscall_gd);
	setup_syscall(hook_addr(&reios_sys_misc), dc_bios_syscall_misc);

	WriteMem32(dc_bios_entrypoint_gd_do_bioscall, REIOS_OPCODE);
	//Infinitive loop for arm !
	WriteMem32(0x80800000, 0xEAFFFFFE);

	const char* bootfile = reios_locate_ip();
	if (!bootfile || !reios_locate_bootfile(bootfile))
		msgboxf("Failed to locate bootfile", MBX_ICONERROR);
}

map<u32, hook_fp*> hooks;
map<hook_fp*, u32> hooks_rev;

#define SYSCALL_ADDR_MAP(addr) ((addr & 0x1FFFFFFF) | 0x80000000)

void register_hook(u32 pc, hook_fp* fn) {
	hooks[SYSCALL_ADDR_MAP(pc)] = fn;
	hooks_rev[fn] = pc;
}

void DYNACALL reios_trap(u32 op) {
	verify(op == REIOS_OPCODE);
	u32 pc = sh4rcb.cntx.pc - 2;
	sh4rcb.cntx.pc = sh4rcb.cntx.pr;

	u32 mapd = SYSCALL_ADDR_MAP(pc);

	debugf("reios: dispatch %08X -> %08X\n", pc, mapd);

	hooks[mapd]();
}

u32 hook_addr(hook_fp* fn) {
	if (hooks_rev.count(fn))
		return hooks_rev[fn];
	else
		return 0;
}

bool reios_init(u8* rom, u8* flash) {

	biosrom = rom;
	flashrom = flash;

	memset(rom, 0xEA, 2048 * 1024);
	memset(GetMemPtr(0x8C000000, 0), 0, RAM_SIZE);

	u16* rom16 = (u16*)rom;

	rom16[0] = REIOS_OPCODE;

	register_hook(0xA0000000, reios_boot);

	register_hook(0x8C001000, reios_sys_system);
	register_hook(0x8C001002, reios_sys_font);
	register_hook(0x8C001004, reios_sys_flashrom);
	register_hook(0x8C001006, reios_sys_gd);
	register_hook(0x8C001008, reios_sys_misc);

	register_hook(dc_bios_entrypoint_gd_do_bioscall, &gd_do_bioscall);

	return true;
}

void reios_reset() {

}

void reios_term() {

}
