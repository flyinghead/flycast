/*
	Extremely primitive bios replacement

	Many thanks to Lars Olsson (jlo@ludd.luth.se) for bios decompile work
		http://www.ludd.luth.se/~jlo/dc/bootROM.c
		http://www.ludd.luth.se/~jlo/dc/bootROM.h
		http://www.ludd.luth.se/~jlo/dc/security_stuff.c
	Bits and pieces from redream (https://github.com/inolen/redream)
*/

#include "reios.h"

#include "reios_elf.h"

#include "gdrom_hle.h"
#include "descrambl.h"

#include "hw/sh4/sh4_core.h"
#undef r
#include "hw/sh4/sh4_mem.h"
#include "hw/holly/sb.h"
#include "hw/naomi/naomi_cart.h"
#include "hw/aica/aica.h"
#include "hw/aica/aica_if.h"
#include "hw/pvr/pvr_regs.h"
#include "imgread/common.h"
#include "imgread/isofs.h"
#include "hw/sh4/sh4_mmr.h"
#include <cmrc/cmrc.hpp>

#include <map>

CMRC_DECLARE(flycast);

#define debugf(...) DEBUG_LOG(REIOS, __VA_ARGS__)

#define dc_bios_syscall_system				0x8C0000B0
#define dc_bios_syscall_font				0x8C0000B4
#define dc_bios_syscall_flashrom			0x8C0000B8
#define dc_bios_syscall_gd					0x8C0000BC
#define dc_bios_syscall_gd2					0x8C0000C0
#define dc_bios_syscall_misc				0x8c0000E0

//At least one game (ooga) uses this directly
#define dc_bios_entrypoint_gd2	0x8c0010F0

#define SYSINFO_ID_ADDR 0x8C001010
#define FONT_TABLE_ADDR 0xa0100020

static MemChip *flashrom;
static u32 base_fad = 45150;
static bool descrambl = false;
static u32 bootSectors;
extern Disc *disc;

static void reios_pre_init()
{
	if (disc != nullptr)
	{
		base_fad = disc->GetBaseFAD();
		descrambl = disc->type != GdRom;
	}
}

static bool reios_locate_bootfile(const char* bootfile)
{
	if (disc == nullptr)
	{
		ERROR_LOG(REIOS, "No disk loaded");
		return false;
	}
	reios_pre_init();

	// Load IP.BIN bootstrap
	libGDR_ReadSector(GetMemPtr(0x8c008000, 0), base_fad, 16, 2048);

	IsoFs isofs(disc);
	std::unique_ptr<IsoFs::Directory> root(isofs.getRoot());
	if (root == nullptr)
	{
		ERROR_LOG(REIOS, "ISO file system root not found");
		return false;
	}
	std::unique_ptr<IsoFs::Entry> bootEntry(root->getEntry(trim_trailing_ws(bootfile)));
	if (bootEntry == nullptr || bootEntry->isDirectory())
	{
		ERROR_LOG(REIOS, "Boot file '%s' not found", bootfile);
		return false;
	}
	IsoFs::File *bootFile = (IsoFs::File *)bootEntry.get();

	u32 offset = 0;
	u32 size = bootFile->getSize();
	if (ip_meta.wince == '1' && !descrambl)
	{
		bootFile->read(GetMemPtr(0x8ce01000, 2048), 2048);
		offset = 2048;
		size -= offset;
	}
	bootSectors = size / 2048;

	if (descrambl)
	{
		std::vector<u8> buf(size);
		bootFile->read(buf.data(), size, offset);
		descrambl_buffer(buf.data(), GetMemPtr(0x8c010000, size), size);
	}
	else
	{
		bootFile->read(GetMemPtr(0x8c010000, size), size, offset);
	}

	u8 data[24] = {0};
	// system id
	for (u32 j = 0; j < 8; j++)
		data[j] = addrspace::read8(0x0021a056 + j);

	// system properties
	for (u32 j = 0; j < 5; j++)
		data[8 + j] = addrspace::read8(0x0021a000 + j);

	// system settings
	flash_syscfg_block syscfg{};
	int rc = static_cast<DCFlashChip*>(flashrom)->ReadBlock(FLASH_PT_USER, FLASH_USER_SYSCFG, &syscfg);
	if (rc == 0)
		WARN_LOG(REIOS, "Can't read system settings from flash");
	else
		memcpy(&data[16], &syscfg.time_lo, 8);

	memcpy(GetMemPtr(0x8c000068, sizeof(data)), data, sizeof(data));

	return true;
}

ip_meta_t ip_meta;

void reios_disk_id()
{
	if (libGDR_GetDiscType() == Open || libGDR_GetDiscType() == NoDisk)
	{
		memset(&ip_meta, 0, sizeof(ip_meta));
		return;
	}
	reios_pre_init();

	u8 buf[2048];
	libGDR_ReadSector(buf, base_fad, 1, sizeof(buf));
	memcpy(&ip_meta, buf, sizeof(ip_meta));
	INFO_LOG(REIOS, "hardware %.16s maker %.16s ks %.5s type %.6s num %.5s area %.8s ctrl %.4s dev %c vga %c wince %c "
			"product %.10s version %.6s date %.8s boot %.16s softco %.16s name %.128s",
			ip_meta.hardware_id, ip_meta.maker_id, ip_meta.ks, ip_meta.disk_type, ip_meta.disk_num, ip_meta.area_symbols,
			ip_meta.ctrl, ip_meta.dev, ip_meta.vga, ip_meta.wince,
			ip_meta.product_number, ip_meta.product_version,
			ip_meta.release_date, ip_meta.boot_filename, ip_meta.software_company, ip_meta.software_name);
}

static void reios_sys_system() {
	u32 cmd = p_sh4rcb->cntx.r[7];

	switch (cmd)
	{
	case 0:	//SYSINFO_INIT
		{
			debugf("reios_sys_system: SYSINFO_INIT");
			// 0x00-0x07: system_id
			// 0x08-0x0c: system_props
			// 0x0d-0x17: padding (zeroed out)
			u8 data[24] = {0};

			// read system_id from 0x0001a056
			for (u32 i  = 0; i < 8; i++)
				data[i] = flashrom->Read8(0x1a056 + i);

			// read system_props from 0x0001a000
			for (u32 i  = 0; i < 5; i++)
				data[8 + i] = flashrom->Read8(0x1a000 + i);

			memcpy(GetMemPtr(0x8c000068, sizeof(data)), data, sizeof(data));

			p_sh4rcb->cntx.r[0] = 0;
		}
		break;

	case 2: //SYSINFO_ICON
		debugf("reios_sys_system: SYSINFO_ICON");
		// r4 = icon number (0-9, but only 5-9 seems to really be icons)
		// r5 = destination buffer (704 bytes in size)
		p_sh4rcb->cntx.r[0] = p_sh4rcb->cntx.r[4] > 9 ? -1 : 704;
		break;

	case 3: //SYSINFO_ID
		debugf("reios_sys_system: SYSINFO_ID");
		p_sh4rcb->cntx.r[0] = 0x8c000068;
		break;

	default:
		WARN_LOG(REIOS, "reios_sys_system: unhandled cmd %d", cmd);
		p_sh4rcb->cntx.r[0] = -1;
		break;
	}
}

static void reios_sys_font() {
	u32 cmd = p_sh4rcb->cntx.r[1];

	switch (cmd)
	{
	case 0:		// FONTROM_ADDRESS
		debugf("FONTROM_ADDRESS");
		p_sh4rcb->cntx.r[0] = FONT_TABLE_ADDR;	// in ROM
		break;

	case 1:		// FONTROM_LOCK
		debugf("FONTROM_LOCK");
		p_sh4rcb->cntx.r[0] = 0;
		break;

	case 2:		// FONTROM_UNLOCK
		debugf("FONTROM_UNLOCK");
		p_sh4rcb->cntx.r[0] = 0;
		break;

	default:
		WARN_LOG(REIOS, "reios_sys_font cmd %x", cmd);
		break;
	}
}

static void reios_sys_flashrom() {
	u32 cmd = p_sh4rcb->cntx.r[7];

	switch (cmd)
	{
		case 0: // FLASHROM_INFO
			{
				/*
					r4 = partition number(0 - 4)
					r5 = pointer to two 32 bit integers to receive the result.
						The first will be the offset of the partition start, in bytes from the start of the flashrom.
						The second will be the size of the partition, in bytes.
					Returns:
					r0 = 0 if successful, -1 if no such partition exists
				 */

				u32 part = p_sh4rcb->cntx.r[4];
				u32 dest = p_sh4rcb->cntx.r[5];
				debugf("reios_sys_flashrom: FLASHROM_INFO part %d dest %08x", part, dest);

				if (part < FLASH_PT_NUM)
				{
					int offset, size;
					static_cast<DCFlashChip*>(flashrom)->GetPartitionInfo(part, &offset, &size);
					WriteMem32(dest, offset);
					WriteMem32(dest + 4, size);

					p_sh4rcb->cntx.r[0] = 0;
				}
				else {
					p_sh4rcb->cntx.r[0] = -1;
				}
			}
			break;

		case 1:	//FLASHROM_READ
			{
				/*
					r4 = read start position, in bytes from the start of the flashrom
					r5 = pointer to destination buffer
					r6 = number of bytes to read
					Returns:
					r0 = 0 if successful, -1 if read failed
				*/
				u32 offset = p_sh4rcb->cntx.r[4];
				u32 dest = p_sh4rcb->cntx.r[5];
				u32 size = p_sh4rcb->cntx.r[6];

				debugf("reios_sys_flashrom: FLASHROM_READ offs %x dest %08x size %x", offset, dest, size);
				for (u32 i = 0; i < size; i++)
					WriteMem8(dest++, flashrom->Read8(offset + i));

				p_sh4rcb->cntx.r[0] = 0;
			}
			break;

			
		case 2:	//FLASHROM_WRITE
			{
				/*
					r4 = write start position, in bytes from the start of the flashrom
					r5 = pointer to source buffer
					r6 = number of bytes to write
					Returns:
					r0 = number of written bytes if successful, -1 if write failed
				*/

				u32 offs = p_sh4rcb->cntx.r[4];
				u32 src = p_sh4rcb->cntx.r[5];
				u32 size = p_sh4rcb->cntx.r[6];

				debugf("reios_sys_flashrom: FLASHROM_WRITE offs %x src %08x size %x", offs, src, size);

				for (u32 i = 0; i < size; i++)
					flashrom->data[offs + i] &= ReadMem8(src + i);

				p_sh4rcb->cntx.r[0] = size;
			}
			break;

		case 3:	//FLASHROM_DELETE
			{			
				/*
				   r4 = offset of the start of the partition you want to delete, in bytes from the start of the flashrom
				   Returns:
				   r0 = zero if successful, -1 if delete failed
				*/
				u32 offset = p_sh4rcb->cntx.r[4];

				debugf("reios_sys_flashrom: FLASHROM_DELETE offs %x", offset);

				bool found = false;

				for (int part = 0; part < FLASH_PT_NUM; part++)
				{
					int part_offset;
					int size;
					static_cast<DCFlashChip*>(flashrom)->GetPartitionInfo(part, &part_offset, &size);
					if (offset == (u32)part_offset)
					{
						found = true;
						memset(flashrom->data + offset, 0xFF, size);
					}
				}

				p_sh4rcb->cntx.r[0] = found ? 0 : -1;
			}
			break;

		default:
			WARN_LOG(REIOS, "reios_sys_flashrom: not handled, %d", cmd);
			break;
	}
}

static void reios_sys_misc()
{
	INFO_LOG(REIOS, "reios_sys_misc - r7: 0x%08X, r4 0x%08X, r5 0x%08X, r6 0x%08X", p_sh4rcb->cntx.r[7], p_sh4rcb->cntx.r[4], p_sh4rcb->cntx.r[5], p_sh4rcb->cntx.r[6]);
	switch (p_sh4rcb->cntx.r[4])
	{
	case 0: // normal init
		SB_GDSTARD = 0xc010000 + bootSectors * 2048;
		SB_IML2NRM = 0;
		p_sh4rcb->cntx.r[0] = 0xc0bebc;
		VO_BORDER_COL.full = p_sh4rcb->cntx.r[0];
		break;

	case 1:	// Exit to BIOS menu
		WARN_LOG(REIOS, "SYS_MISC 1");
		throw FlycastException("Reboot to BIOS");
		break;

	case 2:	// check disk
		p_sh4rcb->cntx.r[0] = 0;
		// Reload part of IP.BIN bootstrap
		libGDR_ReadSector(GetMemPtr(0x8c008100, 0), base_fad, 7, 2048);
		break;

	case 3: // Exit to CD menu
		WARN_LOG(REIOS, "SYS_MISC 3");
		break;

	default:
		WARN_LOG(REIOS, "Unknown SYS_MISC call: %d", p_sh4rcb->cntx.r[4]);
		break;
	}
}

typedef void hook_fp();

static void setup_syscall(u32 hook_addr, u32 syscall_addr)
{
	WriteMem32(syscall_addr, hook_addr);
	WriteMem16(hook_addr, REIOS_OPCODE);

	debugf("Patching syscall vector %08X, points to %08X", syscall_addr, hook_addr);
	debugf(" - address %08X: data %04X [%04X]", hook_addr, ReadMem16(hook_addr), REIOS_OPCODE);
}

static void reios_setup_state(u32 boot_addr)
{
	// San Francisco Rush checksum
	short *p = (short *)GetMemPtr(0x8c0010f0, 2);
	int chksum = (int)0xFFF937D1;
	for (int i = 0; i < 10; i++)
		chksum -= *p++;
	p += 0xee - 1;
	for (int i = 0; i < 3; i++)
		chksum += *p++;
	p += 0x347 - 1;
	for (int i = 0; i < 11; i++)
		chksum -= *p++;
	p += 0xbf8 - 1;
	for (int i = 0; i < 98; i++)
	{
		short v = chksum < 0 ? std::min(-chksum, 32767) : std::max(-chksum, -32768);
		*p = v;
		chksum += *p++;
	}

	// Set up AICA interrupt masks
	aica::writeAicaReg(SCIEB_addr, (u16)0x48);
	aica::writeAicaReg(SCILV0_addr, (u8)0x18);
	aica::writeAicaReg(SCILV1_addr, (u8)0x50);
	aica::writeAicaReg(SCILV2_addr, (u8)0x08);

	// KOS seems to expect this
	DMAC_DMAOR.full = 0x8201;

	// WinCE needs this to detect PAL
	if (config::Broadcast == 1)
		BSC_PDTRA.full = 4;
	BSC_PCTRA.full = 0x000A03F0;

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

	//Setup registers to imitate a normal boot
	p_sh4rcb->cntx.r[15] = 0x8d000000;

	gbr = 0x8c000000;
	ssr = 0x40000001;
	spc = 0x8c000776;
	sgr = 0x8d000000;
	dbr = 0x8c000010;
	vbr = 0x8c000000;
	pr = 0xac00043c;
	fpul = 0x00000000;
	next_pc = boot_addr;

	sr.status = 0x400000f0;
	sr.T = 1;

	old_sr.status = 0x400000f0;

	fpscr.full = 0x00040001;
	old_fpscr.full = 0x00040001;
}

static void reios_setup_naomi(u32 boot_addr) {
	/*
		SR 0x60000000 0x00000001
		FPSRC 0x00040001

		-		xffr	0x13e1fe40	float [32]
		[0x0]	1.00000000	float
		[0x1]	0.000000000	float
		[0x2]	0.000000000	float
		[0x3]	0.000000000	float
		[0x4]	0.000000000	float
		[0x5]	1.00000000	float
		[0x6]	0.000000000	float
		[0x7]	0.000000000	float
		[0x8]	0.000000000	float
		[0x9]	0.000000000	float
		[0xa]	1.00000000	float
		[0xb]	0.000000000	float
		[0xc]	0.000000000	float
		[0xd]	0.000000000	float
		[0xe]	0.000000000	float
		[0xf]	1.00000000	float
		[0x10]	1.00000000	float
		[0x11]	2.14748365e+009	float
		[0x12]	0.000000000	float
		[0x13]	480.000000	float
		[0x14]	9.99999975e-006	float
		[0x15]	0.000000000	float
		[0x16]	0.00208333321	float
		[0x17]	0.000000000	float
		[0x18]	0.000000000	float
		[0x19]	2.14748365e+009	float
		[0x1a]	1.00000000	float
		[0x1b]	-1.00000000	float
		[0x1c]	0.000000000	float
		[0x1d]	0.000000000	float
		[0x1e]	0.000000000	float
		[0x1f]	0.000000000	float
		
		-		r	0x13e1fec0	unsigned int [16]
		[0x0]	0x0c021000	unsigned int
		[0x1]	0x0c01f820	unsigned int
		[0x2]	0xa0710004	unsigned int
		[0x3]	0x0c01f130	unsigned int
		[0x4]	0x5bfccd08	unsigned int
		[0x5]	0xa05f7000	unsigned int
		[0x6]	0xa05f7008	unsigned int
		[0x7]	0x00000007	unsigned int
		[0x8]	0x00000000	unsigned int
		[0x9]	0x00002000	unsigned int
		[0xa]	0xffffffff	unsigned int
		[0xb]	0x0c0e0000	unsigned int
		[0xc]	0x00000000	unsigned int
		[0xd]	0x00000000	unsigned int
		[0xe]	0x00000000	unsigned int
		[0xf]	0x0cc00000	unsigned int

		-		mac	{full=0x0000000000002000 l=0x00002000 h=0x00000000 }	Sh4Context::<unnamed-tag>::<unnamed-tag>::<unnamed-type-mac>
		full	0x0000000000002000	unsigned __int64
		l	0x00002000	unsigned int
		h	0x00000000	unsigned int
		
		-		r_bank	0x13e1ff08	unsigned int [8]
		[0x0]	0x00000000	unsigned int
		[0x1]	0x00000000	unsigned int
		[0x2]	0x00000000	unsigned int
		[0x3]	0x00000000	unsigned int
		[0x4]	0x00000000	unsigned int
		[0x5]	0x00000000	unsigned int
		[0x6]	0x00000000	unsigned int
		[0x7]	0x00000000	unsigned int
		gbr	0x0c2abcc0	unsigned int
		ssr	0x60000000	unsigned int
		spc	0x0c041738	unsigned int
		sgr	0x0cbfffb0	unsigned int
		dbr	0x00000fff	unsigned int
		vbr	0x0c000000	unsigned int
		pr	0xac0195ee	unsigned int
		fpul	0x000001e0	unsigned int
		pc	0x0c021000	unsigned int
		jdyn	0x0c021000	unsigned int

	*/

	//Setup registers to imitate a normal boot
	p_sh4rcb->cntx.r[0] = 0x0c021000;
	p_sh4rcb->cntx.r[1] = 0x0c01f820;
	p_sh4rcb->cntx.r[2] = 0xa0710004;
	p_sh4rcb->cntx.r[3] = 0x0c01f130;
	p_sh4rcb->cntx.r[4] = 0x5bfccd08;
	p_sh4rcb->cntx.r[5] = 0xa05f7000;
	p_sh4rcb->cntx.r[6] = 0xa05f7008;
	p_sh4rcb->cntx.r[7] = 0x00000007;
	p_sh4rcb->cntx.r[8] = 0x00000000;
	p_sh4rcb->cntx.r[9] = 0x00002000;
	p_sh4rcb->cntx.r[10] = 0xffffffff;
	p_sh4rcb->cntx.r[11] = 0x0c0e0000;
	p_sh4rcb->cntx.r[12] = 0x00000000;
	p_sh4rcb->cntx.r[13] = 0x00000000;
	p_sh4rcb->cntx.r[14] = 0x00000000;
	p_sh4rcb->cntx.r[15] = 0x0cc00000;

	gbr = 0x0c2abcc0;
	ssr = 0x60000000;
	spc = 0x0c041738;
	sgr = 0x0cbfffb0;
	dbr = 0x00000fff;
	vbr = 0x0c000000;
	pr = 0xac0195ee;
	fpul = 0x000001e0;
	next_pc = boot_addr;

	sr.status = 0x60000000;
	sr.T = 1;

	old_sr.status = 0x60000000;

	fpscr.full = 0x00040001;
	old_fpscr.full = 0x00040001;
}

static void reios_boot()
{
	NOTICE_LOG(REIOS, "-----------------");
	NOTICE_LOG(REIOS, "REIOS: Booting up");
	NOTICE_LOG(REIOS, "-----------------");
	//setup syscalls
	//find boot file
	//boot it

	memset(GetMemPtr(0x8C000000, 0), 0xFF, 64_KB);

	setup_syscall(0x8C001000, dc_bios_syscall_system);
	setup_syscall(0x8C001002, dc_bios_syscall_font);
	setup_syscall(0x8C001004, dc_bios_syscall_flashrom);
	setup_syscall(0x8C001006, dc_bios_syscall_gd);
	setup_syscall(dc_bios_entrypoint_gd2, dc_bios_syscall_gd2);
	setup_syscall(0x8C001008, dc_bios_syscall_misc);

	//Infinite loop for arm !
	WriteMem32(0x80800000, 0xEAFFFFFE);

	std::string extension = get_file_extension(settings.content.path);
	if (extension == "elf")
	{
		if (!reios_loadElf(settings.content.path))
			throw FlycastException(std::string("Failed to open ELF ") + settings.content.path);
		reios_setup_state(0x8C010000);
	}
	else {
		if (settings.platform.isConsole())
		{
			char bootfile[sizeof(ip_meta.boot_filename) + 1] = {0};
			memcpy(bootfile, ip_meta.boot_filename, sizeof(ip_meta.boot_filename));
			if (bootfile[0] == '\0' || !reios_locate_bootfile(bootfile))
				throw FlycastException(std::string("Failed to locate bootfile ") + bootfile);
			reios_setup_state(0xac008300);
		}
		else {
			verify(settings.platform.isNaomi());
			if (CurrentCartridge == NULL)
			{
				WARN_LOG(REIOS, "No cartridge loaded");
				return;
			}
			u32 data_size = 4;
			u32* sz = (u32*)CurrentCartridge->GetPtr(0x368, data_size);
			if (sz == nullptr || data_size != 4)
				throw FlycastException("Naomi boot failure");

			const u32 size = *sz;

			data_size = 1;
			if (size > RAM_SIZE || CurrentCartridge->GetPtr(size - 1, data_size) == nullptr)
				throw FlycastException("Invalid cart size");

			data_size = size;
			WriteMemBlock_nommu_ptr(0x0c020000, (u32*)CurrentCartridge->GetPtr(0, data_size), size);

			reios_setup_naomi(0x0c021000);
		}
	}
}

static std::map<u32, hook_fp*> hooks;

#define SYSCALL_ADDR_MAP(addr) (((addr) & 0x1FFFFFFF) | 0x80000000)

static void register_hook(u32 pc, hook_fp* fn) {
	hooks[SYSCALL_ADDR_MAP(pc)] = fn;
}

void DYNACALL reios_trap(u32 op) {
	verify(op == REIOS_OPCODE);
	u32 pc = next_pc - 2;

	u32 mapd = SYSCALL_ADDR_MAP(pc);

	//debugf("dispatch %08X -> %08X", pc, mapd);

	auto it = hooks.find(mapd);
	if (it == hooks.end()) {
		ERROR_LOG(REIOS, "Unknown trap vector %08x pc %08x", mapd, pc);
		return;
	}

	it->second();

	// Return from syscall, except if pc was modified
	if (pc == next_pc - 2)
		next_pc = pr;
}

bool reios_init()
{
	INFO_LOG(REIOS, "reios: Init");

	register_hook(0xA0000000, reios_boot);

	register_hook(0x8C001000, reios_sys_system);
	register_hook(0x8C001002, reios_sys_font);
	register_hook(0x8C001004, reios_sys_flashrom);
	register_hook(0x8C001006, gdrom_hle_op);
	register_hook(0x8C001008, reios_sys_misc);

	register_hook(dc_bios_entrypoint_gd2, gdrom_hle_op);

	return true;
}

void reios_set_flash(MemChip* flash)
{
	flashrom = flash;
}

void reios_reset(u8* rom)
{
	memset(rom, 0x00, BIOS_SIZE);
	memset(GetMemPtr(0x8C000000, 0), 0, RAM_SIZE);

	u16* rom16 = (u16*)rom;

	rom16[0] = REIOS_OPCODE;

	// The Grinch game bug
	*(u32 *)&rom[0x44c] = 0xe303d463;
	// Jeremy McGrath game bug
	*(u32 *)&rom[0x1c] = 0x71294118;
	// Rent a Hero 1 game bug
	*(u32 *)&rom[0x8] = 0x44094409;

	u8 *pFont = rom + (FONT_TABLE_ADDR % BIOS_SIZE);

	// 288 12 × 24 pixels (36 bytes) characters
	// 7078 24 × 24 pixels (72 bytes) characters
	// 129 32 × 32 pixels (128 bytes) characters
	memset(pFont, 0, 536496);
	try {
		cmrc::embedded_filesystem fs = cmrc::flycast::get_filesystem();
		cmrc::file fontFile = fs.open("fonts/biosfont.bin");
		memcpy(pFont, fontFile.begin(), fontFile.end() - fontFile.begin());
	} catch (const std::system_error& e) {
		ERROR_LOG(REIOS, "Failed to load the bios font: %s", e.what());
		throw;
	}

	gd_hle_state = {};
}

void reios_term() {
}
