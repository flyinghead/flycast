// serialize.cpp : save states
#if 1
#include "types.h"
#include "hw/aica/dsp.h"
#include "hw/aica/aica.h"
#include "hw/aica/sgc_if.h"
#include "hw/holly/sb_mem.h"
#include "hw/flashrom/flashrom.h"
#include "hw/mem/_vmem.h"
#include "hw/gdrom/gdromv3.h"
#include "hw/maple/maple_cfg.h"
#include "hw/pvr/Renderer_if.h"
#include "hw/pvr/ta_structs.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/sh4/sh4_mmr.h"
#include "hw/sh4/modules/mmu.h"
#include "imgread/common.h"
#include "reios/reios.h"
#include <map>
#include <set>
#include "rend/gles/gles.h"
#include "hw/sh4/dyna/blockmanager.h"
#include "hw/sh4/dyna/ngen.h"

/*
 * search for "maybe" to find items that were left out that may be needed
 */

extern "C" void DYNACALL TAWriteSQ(u32 address,u8* sqb);

enum serialize_version_enum {
	V1
} ;

//./core/hw/arm7/arm_mem.cpp
extern bool aica_interr;
extern u32 aica_reg_L;
extern bool e68k_out;
extern u32 e68k_reg_L;
extern u32 e68k_reg_M;



//./core/hw/arm7/arm7.cpp
extern DECL_ALIGN(8) reg_pair arm_Reg[RN_ARM_REG_COUNT];
extern bool armIrqEnable;
extern bool armFiqEnable;
extern int armMode;
extern bool Arm7Enabled;
extern u8 cpuBitsSet[256];
extern bool intState ;
extern bool stopState ;
extern bool holdState ;
/*
	if AREC dynarec enabled:
	vector<ArmDPOP> ops;
	u8* icPtr;
	u8* ICache;
	u8 ARM7_TCB[ICacheSize+4096];
	void* EntryPoints[8*1024*1024/4];
	map<u32,u32> renamed_regs;
	u32 rename_reg_base;
	u32 nfb,ffb,bfb,mfb;
	static x86_block* x86e;
	x86_Label* end_lbl;
	u8* ARM::emit_opt=0;
	eReg ARM::reg_addr;
	eReg ARM::reg_dst;
	s32 ARM::imma;
*/




//./core/hw/aica/dsp.o
extern DECL_ALIGN(4096) dsp_t dsp;
//recheck dsp.cpp if FEAT_DSPREC == DYNAREC_JIT




//./core/hw/aica/aica.o
//these are all just pointers into aica_reg
//extern CommonData_struct* CommonData;
//extern DSPData_struct* DSPData;
//extern InterruptInfo* MCIEB;
//extern InterruptInfo* MCIPD;
//extern InterruptInfo* MCIRE;
//extern InterruptInfo* SCIEB;
//extern InterruptInfo* SCIPD;
//extern InterruptInfo* SCIRE;

extern AicaTimer timers[3];



//./core/hw/aica/aica_if.o
extern VArray2 aica_ram;
extern u32 VREG;//video reg =P
extern u32 ARMRST;//arm reset reg
extern u32 rtc_EN;
//extern s32 aica_pending_dma ;
extern int dma_sched_id;


//./core/hw/aica/aica_mem.o
extern u8 aica_reg[0x8000];



//./core/hw/aica/sgc_if.o
struct ChannelEx;
#define AicaChannel ChannelEx
extern s32 volume_lut[16];
extern s32 tl_lut[256 + 768];	//xx.15 format. >=255 is muted
extern u32 AEG_ATT_SPS[64];
extern u32 AEG_DSR_SPS[64];
extern s16 pl;
extern s16 pr;
//this is just a pointer to aica_reg
//extern DSP_OUT_VOL_REG* dsp_out_vol;
//not needed - one-time init
//void(* 	STREAM_STEP_LUT [5][2][2])(ChannelEx *ch)
//void(* 	STREAM_INITAL_STEP_LUT [5])(ChannelEx *ch)
//void(* 	AEG_STEP_LUT [4])(ChannelEx *ch)
//void(* 	FEG_STEP_LUT [4])(ChannelEx *ch)
//void(* 	ALFOWS_CALC [4])(ChannelEx *ch)
//void(* 	PLFOWS_CALC [4])(ChannelEx *ch)

//special handling
//extern AicaChannel AicaChannel::Chans[64];
#define Chans AicaChannel::Chans
#define CDDA_SIZE  (2352/2)
extern s16 cdda_sector[CDDA_SIZE];
extern u32 cdda_index;
extern SampleType mxlr[64];
extern u32 samples_gen;





//./core/hw/holly/sb.o
extern Array<RegisterStruct> sb_regs;
extern u32 SB_ISTNRM;
extern u32 SB_FFST_rc;
extern u32 SB_FFST;



//./core/hw/holly/sb_mem.o
//unused
//static HollyInterruptID dmatmp1;
//static HollyInterruptID dmatmp2;
//static HollyInterruptID OldDmaId;

//this is one-time init, no updates - don't need to serialize
//extern RomChip sys_rom;
#ifdef FLASH_SIZE
extern DCFlashChip sys_nvmem;
#endif

#ifdef BBSRAM_SIZE
extern SRamChip sys_nvmem;
#endif
//this is one-time init, no updates - don't need to serialize
//extern _vmem_handler area0_handler;




//./core/hw/gdrom/gdrom_response.o
extern u16 reply_11[] ;




//./core/hw/gdrom/gdromv3.o
extern int gdrom_schid;
extern signed int sns_asc;
extern signed int sns_ascq;
extern signed int sns_key;
extern packet_cmd_t packet_cmd;
extern u32 set_mode_offset;
extern read_params_t read_params ;
extern packet_cmd_t packet_cmd;
//Buffer for sector reads [dma]
extern read_buff_t read_buff ;
//pio buffer
extern pio_buff_t pio_buff ;
extern u32 set_mode_offset;
extern ata_cmd_t ata_cmd ;
extern cdda_t cdda ;
extern gd_states gd_state;
extern DiscType gd_disk_type;
extern u32 data_write_mode;
//Registers
extern u32 DriveSel;
extern GD_ErrRegT Error;
extern GD_InterruptReasonT IntReason;
extern GD_FeaturesT Features;
extern GD_SecCountT SecCount;
extern GD_SecNumbT SecNumber;
extern GD_StatusT GDStatus;
extern ByteCount_t ByteCount ;





//./core/hw/maple/maple_devs.o
extern char EEPROM[0x100];
extern bool EEPROM_loaded;
extern _NaomiState State;

//./core/hw/maple/maple_if.o
//one time set
//extern int maple_sched;
//incremented but never read
//extern u32 dmacount;
extern bool maple_ddt_pending_reset;






//./core/hw/pvr/Renderer_if.o
//only written - not read
//extern u32 VertexCount;
extern u32 FrameCount;
//one-time init
//extern Renderer* renderer;
//these are just mutexes used during rendering
//extern cResetEvent rs;
//extern cResetEvent re;
//these max_?? through ovrn are written and not read
//extern int max_idx;
//extern int max_mvo;
//extern int max_op;
//extern int max_pt;
//extern int max_tr;
//extern int max_vtx;
//extern int max_modt;
//extern int ovrn;
//seems safe to omit this - gets refreshed every frame from a pool
//extern TA_context* _pvrrc;
//just a flag indiating the rendering thread is running
//extern int rend_en ;
//the renderer thread - one time set
//extern cThread rthd;
extern bool pend_rend;

//these will all get cleared out after a few frames - no need to serialize
//static bool render_called = false;
//u32 fb1_watch_addr_start;
//u32 fb1_watch_addr_end;
//u32 fb2_watch_addr_start;
//u32 fb2_watch_addr_end;
//bool fb_dirty;


//maybe
//extern u32 memops_t,memops_l;


//./core/hw/pvr/pvr_mem.o
extern u32 YUV_tempdata[512/4];//512 bytes
extern u32 YUV_dest;
extern u32 YUV_blockcount;
extern u32 YUV_x_curr;
extern u32 YUV_y_curr;
extern u32 YUV_x_size;
extern u32 YUV_y_size;




//./core/hw/pvr/pvr_regs.o
extern bool fog_needs_update;
extern u8 pvr_regs[pvr_RegSize];




//./core/hw/pvr/spg.o
extern u32 in_vblank;
extern u32 clc_pvr_scanline;
extern u32 pvr_numscanlines;
extern u32 prv_cur_scanline;
extern u32 vblk_cnt;
extern u32 Line_Cycles;
extern u32 Frame_Cycles;
extern int render_end_schid;
extern int vblank_schid;
extern int time_sync;
extern double speed_load_mspdf;
extern int mips_counter;
extern double full_rps;
extern u32 fskip;




//./core/hw/pvr/ta.o
extern u32 ta_type_lut[256];
extern u8 ta_fsm[2049];	//[2048] stores the current state
extern u32 ta_fsm_cl;




//./core/hw/pvr/ta_ctx.o
//these frameskipping don't need to be saved
//extern int frameskip;
//extern bool FrameSkipping;		// global switch to enable/disable frameskip
//maybe need these - but hopefully not
//extern TA_context* ta_ctx;
//extern tad_context ta_tad;
//extern TA_context*  vd_ctx;
//extern rend_context vd_rc;
//extern cMutex mtx_rqueue;
//extern TA_context* rqueue;
//extern cResetEvent frame_finished;
//extern cMutex mtx_pool;
//extern vector<TA_context*> ctx_pool;
//extern vector<TA_context*> ctx_list;
//end maybe



//./core/hw/pvr/ta_vtx.o
extern bool pal_needs_update;
extern u32 palette16_ram[1024];
extern u32 palette32_ram[1024];
//extern u32 decoded_colors[3][65536];
extern u32 tileclip_val;
extern u8 f32_su8_tbl[65536];
//written but never read
//extern ModTriangle* lmr;
//never changed
//extern PolyParam nullPP;
//maybe
//extern PolyParam* CurrentPP;
//maybe
//extern List<PolyParam>* CurrentPPlist;
//TA state vars
extern DECL_ALIGN(4) u8 FaceBaseColor[4];
extern DECL_ALIGN(4) u8 FaceOffsColor[4];
#ifdef HAVE_OIT
extern DECL_ALIGN(4) u8 FaceBaseColor1[4];
extern DECL_ALIGN(4) u8 FaceOffsColor1[4];
#endif
extern DECL_ALIGN(4) u32 SFaceBaseColor;
extern DECL_ALIGN(4) u32 SFaceOffsColor;
//maybe
//extern TaListFP* TaCmd;
//maybe
//extern u32 CurrentList;
//maybe
//extern TaListFP* VertexDataFP;
//written but never read
//extern bool ListIsFinished[5];
//maybe ; need special handler
//FifoSplitter<0> TAFifo0;
//counter for frameskipping - doesn't need to be saved
//extern int ta_parse_cnt;




//./core/rend/TexCache.o
//maybe
//extern u8* vq_codebook;
//extern u32 palette_index;
//extern bool KillTex;
//extern u32 palette16_ram[1024];
//extern u32 palette32_ram[1024];
//extern u32 detwiddle[2][8][1024];
//maybe
//extern vector<vram_block*> VramLocks[/*VRAM_SIZE*/(16*1024*1024)/PAGE_SIZE];
//maybe - probably not - just a locking mechanism
//extern cMutex vramlist_lock;
extern VArray2 vram;




//./core/hw/sh4/sh4_mmr.o
extern Array<u8> OnChipRAM;
extern Array<RegisterStruct> CCN;  //CCN  : 14 registers
extern Array<RegisterStruct> UBC;   //UBC  : 9 registers
extern Array<RegisterStruct> BSC;  //BSC  : 18 registers
extern Array<RegisterStruct> DMAC; //DMAC : 17 registers
extern Array<RegisterStruct> CPG;   //CPG  : 5 registers
extern Array<RegisterStruct> RTC;  //RTC  : 16 registers
extern Array<RegisterStruct> INTC;  //INTC : 4 registers
extern Array<RegisterStruct> TMU;  //TMU  : 12 registers
extern Array<RegisterStruct> SCI;   //SCI  : 8 registers
extern Array<RegisterStruct> SCIF; //SCIF : 10 registers




//./core/hw/sh4/sh4_mem.o
extern VArray2 mem_b;
//one-time init
//extern _vmem_handler area1_32b;
//one-time init
//extern _vmem_handler area5_handler;




//./core/hw/sh4/sh4_interrupts.o
extern u16 IRLPriority;
//one-time init
//extern InterptSourceList_Entry InterruptSourceList[28];
extern DECL_ALIGN(64) u16 InterruptEnvId[32];
extern DECL_ALIGN(64) u32 InterruptBit[32];
extern DECL_ALIGN(64) u32 InterruptLevelBit[16];
extern u32 interrupt_vpend; // Vector of pending interrupts
extern u32 interrupt_vmask; // Vector of masked interrupts             (-1 inhibits all interrupts)
extern u32 decoded_srimask; // Vector of interrupts allowed by SR.IMSK (-1 inhibits all interrupts)




//./core/hw/sh4/sh4_core_regs.o
extern Sh4RCB* p_sh4rcb;
//just method pointers
//extern sh4_if  sh4_cpu;
//one-time set
//extern u8* sh4_dyna_rcb;
extern u32 old_rm;
extern u32 old_dn;




//./core/hw/sh4/sh4_sched.o
extern u64 sh4_sched_ffb;
extern u32 sh4_sched_intr;
extern vector<sched_list> list;
extern int sh4_sched_next_id;




//./core/hw/sh4/interpr/sh4_interpreter.o
extern int aica_schid;
extern int rtc_schid;




//./core/hw/sh4/modules/serial.o
extern SCIF_SCFSR2_type SCIF_SCFSR2;
extern u8 SCIF_SCFRDR2;
extern SCIF_SCFDR2_type SCIF_SCFDR2;




//./core/hw/sh4/modules/bsc.o
extern BSC_PDTRA_type BSC_PDTRA;




//./core/hw/sh4/modules/tmu.o
extern u32 tmu_shift[3];
extern u32 tmu_mask[3];
extern u64 tmu_mask64[3];
extern u32 old_mode[3];
extern int tmu_sched[3];
extern u32 tmu_ch_base[3];
extern u64 tmu_ch_base64[3];




//./core/hw/sh4/modules/ccn.o
extern u32 CCN_QACR_TR[2];




//./core/hw/sh4/modules/mmu.o
extern TLB_Entry UTLB[64];
extern TLB_Entry ITLB[4];
#if defined(NO_MMU)
extern u32 sq_remap[64];
#else
extern u32 ITLB_LRU_USE[64];
extern u32 mmu_error_TT;
#endif



//./core/imgread/common.o
extern u32 NullDriveDiscType;
//maybe - seems to all be one-time inits ;    needs special handler
//extern Disc* disc;
extern u8 q_subchannel[96];




//./core/nullDC.o
//extern unsigned FLASH_SIZE;
//extern unsigned BBSRAM_SIZE;
//extern unsigned BIOS_SIZE;
//extern unsigned RAM_SIZE;
//extern unsigned ARAM_SIZE;
//extern unsigned VRAM_SIZE;
//extern unsigned RAM_MASK;
//extern unsigned ARAM_MASK;
//extern unsigned VRAM_MASK;
//settings can be dynamic
//extern settings_t settings;




//./core/reios/reios.o
//never used
//extern u8* biosrom;
//one time init
//extern u8* flashrom;
//one time init
//extern u32 base_fad ;
//one time init
//extern bool descrambl;
//one time init
//extern bool bootfile_inited;
//all these reios_?? are one-time inits
//extern char reios_bootfile[32];
//extern char reios_hardware_id[17];
//extern char reios_maker_id[17];
//extern char reios_device_info[17];
//extern char reios_area_symbols[9];
//extern char reios_peripherals[9];
//extern char reios_product_number[11];
//extern char reios_product_version[7];
//extern char reios_releasedate[17];
//extern char reios_boot_filename[17];
//extern char reios_software_company[17];
//extern char reios_software_name[129];
//one time init
//extern map<u32, hook_fp*> hooks;
//one time init
//extern map<hook_fp*, u32> hooks_rev;




//./core/reios/gdrom_hle.o
//never used in any meaningful way
//extern u32 SecMode[4];




//./core/reios/descrambl.o
//random seeds can be...random
//extern unsigned int seed;



//./core/rend/gles/gles.o
//maybe
//extern GLCache glcache;
//maybe
//extern gl_ctx gl;
//maybe
//extern struct ShaderUniforms_t ShaderUniforms;
//maybe
//extern u32 gcflip;
//maybe
//extern float fb_scale_x;
//extern float fb_scale_y;
//extern float scale_x;
//extern float scale_y;
//extern int screen_width;
//extern int screen_height;
//extern GLuint fogTextureId;
//end maybe



//./core/rend/gles/gldraw.o
//maybe
//extern PipelineShader* CurrentShader;
//written, but never used
//extern Vertex* vtx_sort_base;
//maybe
//extern vector<SortTrigDrawParam>	pidx_sort;




//./core/rend/gles/gltex.o
//maybe ; special handler
//extern map<u64,TextureCacheData> TexCache;
//maybe
//extern FBT fb_rtt;
//not used
//static int TexCacheLookups;
//static int TexCacheHits;
//static float LastTexCacheStats;
//maybe should get reset naturally if needed
//GLuint fbTextureId;




//./core/hw/naomi/naomi.o
extern u32 naomi_updates;
extern u32 RomPioOffset;
extern u32 DmaOffset;
extern u32 DmaCount;
extern u32 BoardID;
extern u32 GSerialBuffer;
extern u32 BSerialBuffer;
extern int GBufPos;
extern int BBufPos;
extern int GState;
extern int BState;
extern int GOldClk;
extern int BOldClk;
extern int BControl;
extern int BCmd;
extern int BLastCmd;
extern int GControl;
extern int GCmd;
extern int GLastCmd;
extern int SerStep;
extern int SerStep2;
extern unsigned char BSerial[];
extern unsigned char GSerial[];
extern u32 reg_dimm_3c;	//IO window ! writen, 0x1E03 some flag ?
extern u32 reg_dimm_40;	//parameters
extern u32 reg_dimm_44;	//parameters
extern u32 reg_dimm_48;	//parameters
extern u32 reg_dimm_4c;	//status/control reg ?
extern bool NaomiDataRead;
extern u32 NAOMI_ROM_OFFSETH;
extern u32 NAOMI_ROM_OFFSETL;
extern u32 NAOMI_ROM_DATA;
extern u32 NAOMI_DMA_OFFSETH;
extern u32 NAOMI_DMA_OFFSETL;
extern u32 NAOMI_DMA_COUNT;
extern u32 NAOMI_BOARDID_WRITE;
extern u32 NAOMI_BOARDID_READ;
extern u32 NAOMI_COMM_OFFSET;
extern u32 NAOMI_COMM_DATA;




//./core/hw/naomi/naomi_cart.o
//all one-time loads
//u8* RomPtr;
//u32 RomSize;
//fd_t*	RomCacheMap;
//u32		RomCacheMapCount;




//./core/rec-x64/rec_x64.o
//maybe need special handler
//extern BlockCompilerx64 *compilerx64_data;




//./core/rec.o
extern int cycle_counter;
#if FEAT_SHREC == DYNAREC_CPP
extern int idxnxx;
#endif



//./core/hw/sh4/dyna/decoder.o
//temp storage only
//extern RuntimeBlockInfo* blk;
extern state_t state;
extern Sh4RegType div_som_reg1;
extern Sh4RegType div_som_reg2;
extern Sh4RegType div_som_reg3;




//./core/hw/sh4/dyna/driver.o
//extern u8 SH4_TCB[CODE_SIZE+4096];
//one time ptr set
//extern u8* CodeCache;
extern u32 LastAddr;
extern u32 LastAddr_min;
//temp storage only
//extern u32* emit_ptr;
extern char block_hash[1024];




//./core/hw/sh4/dyna/blockmanager.o
//cleared but never read
//extern bm_List blocks_page[/*BLOCKS_IN_PAGE_LIST_COUNT*/(32*1024*1024)/4096];
//maybe - the next three seem to be list of precompiled blocks of code - but if not found will populate
//extern bm_List all_blocks;
//extern bm_List del_blocks;
//extern blkmap_t blkmap;
//these two are never referenced
//extern u32 bm_gc_luc;
//extern u32 bm_gcf_luc;
//data is never written to this
//extern u32 PAGE_STATE[(32*1024*1024)/*RAM_SIZE*//32];
//never read
//extern u32 total_saved;
//counter with no real controlling logic behind it
//extern u32 rebuild_counter;
//just printf output
//extern bool print_stats;




//./core/hw/sh4/dyna/shil.o
extern u32 RegisterWrite[sh4_reg_count];
extern u32 RegisterRead[sh4_reg_count];
extern u32 fallback_blocks;
extern u32 total_blocks;
extern u32 REMOVED_OPS;




//./core/linux-dist/main.cpp, ./core/windows/winmain.cpp , ...
extern u16 kcode[4];
extern u8 rt[4];
extern u8 lt[4];
extern u32 vks[4];
extern s8 joyx[4];
extern s8 joyy[4];


bool rc_serialize(void *src, unsigned int src_size, void **dest, unsigned int *total_size)
{
	if ( *dest != NULL )
	{
		memcpy(*dest, src, src_size) ;
		*dest = ((unsigned char*)*dest) + src_size ;
	}

	*total_size += src_size ;
	return true ;
}

bool rc_unserialize(void *src, unsigned int src_size, void **dest, unsigned int *total_size)
{
	if ( *dest != NULL )
	{
		memcpy(src, *dest, src_size) ;
		*dest = ((unsigned char*)*dest) + src_size ;
	}

	*total_size += src_size ;
	return true ;
}

bool register_serialize(Array<RegisterStruct>& regs,void **data, unsigned int *total_size )
{
	int i = 0 ;

	for ( i = 0 ; i < regs.Size ; i++ )
	{
		REICAST_S(regs.data[i].flags) ;
		REICAST_S(regs.data[i].data32) ;
	}

	return true ;
}

bool register_unserialize(Array<RegisterStruct>& regs,void **data, unsigned int *total_size )
{
	int i = 0 ;
	u32 dummy = 0 ;

	for ( i = 0 ; i < regs.Size ; i++ )
	{
		REICAST_US(regs.data[i].flags) ;
		if ( ! (regs.data[i].flags & REG_RF) )
			REICAST_US(regs.data[i].data32) ;
		else
			REICAST_US(dummy) ;
	}
	return true ;
}

bool dc_serialize(void **data, unsigned int *total_size)
{
	int i = 0;
	int j = 0;
	serialize_version_enum version = V1 ;

	*total_size = 0 ;

	//dc not initialized yet
	if ( p_sh4rcb == NULL )
		return false ;

	REICAST_S(version) ;
	REICAST_S(aica_interr) ;
	REICAST_S(aica_reg_L) ;
	REICAST_S(e68k_out) ;
	REICAST_S(e68k_reg_L) ;
	REICAST_S(e68k_reg_M) ;

	REICAST_SA(arm_Reg,RN_ARM_REG_COUNT);
	REICAST_S(armIrqEnable);
	REICAST_S(armFiqEnable);
	REICAST_S(armMode);
	REICAST_S(Arm7Enabled);
	REICAST_SA(cpuBitsSet,256);
	REICAST_S(intState);
	REICAST_S(stopState);
	REICAST_S(holdState);

	REICAST_S(dsp);

	for ( i = 0 ; i < 3 ; i++)
	{
		REICAST_S(timers[i].c_step);
		REICAST_S(timers[i].m_step);
	}


	REICAST_SA(aica_ram.data,aica_ram.size) ;
	REICAST_S(VREG);
	REICAST_S(ARMRST);
	REICAST_S(rtc_EN);
	REICAST_S(dma_sched_id);

	REICAST_SA(aica_reg,0x8000);



	REICAST_SA(volume_lut,16);
	REICAST_SA(tl_lut,256 + 768);
	REICAST_SA(AEG_ATT_SPS,64);
	REICAST_SA(AEG_DSR_SPS,64);
	REICAST_S(pl);
	REICAST_S(pr);

	channel_serialize(data, total_size) ;

	REICAST_SA(cdda_sector,CDDA_SIZE);
	REICAST_S(cdda_index);
	REICAST_SA(mxlr,64);
	REICAST_S(samples_gen);


	register_serialize(sb_regs, data, total_size) ;
	REICAST_S(SB_ISTNRM);
	REICAST_S(SB_FFST_rc);
	REICAST_S(SB_FFST);



	//this is one-time init, no updates - don't need to serialize
	//extern RomChip sys_rom;
	REICAST_S(sys_nvmem.size);
	REICAST_S(sys_nvmem.mask);
#ifdef FLASH_SIZE
	REICAST_S(sys_nvmem.state);
#endif
	REICAST_SA(sys_nvmem.data,sys_nvmem.size);

	//this is one-time init, no updates - don't need to serialize
	//extern _vmem_handler area0_handler;




	REICAST_SA(reply_11,16) ;



	REICAST_S(gdrom_schid);
	REICAST_S(sns_asc);
	REICAST_S(sns_ascq);
	REICAST_S(sns_key);

	REICAST_S(packet_cmd);
	REICAST_S(set_mode_offset);
	REICAST_S(read_params);
	REICAST_S(packet_cmd);
	REICAST_S(read_buff);
	REICAST_S(pio_buff);
	REICAST_S(set_mode_offset);
	REICAST_S(ata_cmd);
	REICAST_S(cdda);
	REICAST_S(gd_state);
	REICAST_S(gd_disk_type);
	REICAST_S(data_write_mode);
	REICAST_S(DriveSel);
	REICAST_S(Error);
	REICAST_S(IntReason);
	REICAST_S(Features);
	REICAST_S(SecCount);
	REICAST_S(SecNumber);
	REICAST_S(GDStatus);
	REICAST_S(ByteCount);
	REICAST_S(i);	// GDROM_TICKS


	REICAST_SA(EEPROM,0x100);
	REICAST_S(EEPROM_loaded);
	REICAST_S(State);


	REICAST_S(maple_ddt_pending_reset);

	mcfg_SerializeDevices(data, total_size);

	REICAST_S(FrameCount);
	REICAST_S(pend_rend);


	REICAST_SA(YUV_tempdata,512/4);
	REICAST_S(YUV_dest);
	REICAST_S(YUV_blockcount);
	REICAST_S(YUV_x_curr);
	REICAST_S(YUV_y_curr);
	REICAST_S(YUV_x_size);
	REICAST_S(YUV_y_size);

	REICAST_SA(pvr_regs,pvr_RegSize);

	REICAST_S(in_vblank);
	REICAST_S(clc_pvr_scanline);
	REICAST_S(pvr_numscanlines);
	REICAST_S(prv_cur_scanline);
	REICAST_S(vblk_cnt);
	REICAST_S(Line_Cycles);
	REICAST_S(Frame_Cycles);
	REICAST_S(render_end_schid);
	REICAST_S(vblank_schid);
	REICAST_S(time_sync);
	REICAST_S(speed_load_mspdf);
	REICAST_S(mips_counter);
	REICAST_S(full_rps);
	REICAST_S(fskip);



	REICAST_SA(ta_type_lut,256);
	REICAST_SA(ta_fsm,2049);
	REICAST_S(ta_fsm_cl);

	REICAST_S(tileclip_val);
	REICAST_SA(f32_su8_tbl,65536);
	REICAST_SA(FaceBaseColor,4);
	REICAST_SA(FaceOffsColor,4);
	REICAST_S(SFaceBaseColor);
	REICAST_S(SFaceOffsColor);

	REICAST_SA(vram.data, vram.size);

	REICAST_SA(OnChipRAM.data,OnChipRAM_SIZE);

	register_serialize(CCN, data, total_size) ;
	register_serialize(UBC, data, total_size) ;
	register_serialize(BSC, data, total_size) ;
	register_serialize(DMAC, data, total_size) ;
	register_serialize(CPG, data, total_size) ;
	register_serialize(RTC, data, total_size) ;
	register_serialize(INTC, data, total_size) ;
	register_serialize(TMU, data, total_size) ;
	register_serialize(SCI, data, total_size) ;
	register_serialize(SCIF, data, total_size) ;

	REICAST_SA(mem_b.data, mem_b.size);



	REICAST_S(IRLPriority);
	REICAST_SA(InterruptEnvId,32);
	REICAST_SA(InterruptBit,32);
	REICAST_SA(InterruptLevelBit,16);
	REICAST_S(interrupt_vpend);
	REICAST_S(interrupt_vmask);
	REICAST_S(decoded_srimask);




	//default to nommu_full
	i = 3 ;
	if ( do_sqw_nommu == &do_sqw_nommu_area_3)
		i = 0 ;
	else if (do_sqw_nommu == &do_sqw_nommu_area_3_nonvmem)
		i = 1 ;
	else if (do_sqw_nommu==(sqw_fp*)&TAWriteSQ)
		i = 2 ;
	else if (do_sqw_nommu==&do_sqw_nommu_full)
		i = 3 ;

	REICAST_S(i) ;

	REICAST_SA((*p_sh4rcb).sq_buffer,64/8);

	//store these before unserializing and then restore after
	//void *getptr = &((*p_sh4rcb).cntx.sr.GetFull) ;
	//void *setptr = &((*p_sh4rcb).cntx.sr.SetFull) ;
	REICAST_S((*p_sh4rcb).cntx);

	REICAST_S(old_rm);
	REICAST_S(old_dn);




	REICAST_S(sh4_sched_ffb);
	REICAST_S(sh4_sched_intr);
	REICAST_S(sh4_sched_next_id);
	//this list is populated during initialization so the size will always be the same
	//extern vector<sched_list> list;
	for ( i = 0 ; i < list.size() ; i++ )
	{
		REICAST_S(list[i].tag) ;
		REICAST_S(list[i].start) ;
		REICAST_S(list[i].end) ;
	}



	REICAST_S(aica_schid);
	REICAST_S(rtc_schid);


	REICAST_S(SCIF_SCFSR2);
	REICAST_S(SCIF_SCFRDR2);
	REICAST_S(SCIF_SCFDR2);


	REICAST_S(BSC_PDTRA);




	REICAST_SA(tmu_shift,3);
	REICAST_SA(tmu_mask,3);
	REICAST_SA(tmu_mask64,3);
	REICAST_SA(old_mode,3);
	REICAST_SA(tmu_sched,3);
	REICAST_SA(tmu_ch_base,3);
	REICAST_SA(tmu_ch_base64,3);




	REICAST_SA(CCN_QACR_TR,2);




	REICAST_SA(UTLB,64);
	REICAST_SA(ITLB,4);
#if defined(NO_MMU)
	REICAST_SA(sq_remap,64);
#else
	REICAST_SA(ITLB_LRU_USE,64);
	REICAST_S(mmu_error_TT);
#endif



	REICAST_S(NullDriveDiscType);
	REICAST_SA(q_subchannel,96);

	REICAST_S(naomi_updates);
	REICAST_S(RomPioOffset);
	REICAST_S(DmaOffset);
	REICAST_S(DmaCount);
	REICAST_S(BoardID);
	REICAST_S(GSerialBuffer);
	REICAST_S(BSerialBuffer);
	REICAST_S(GBufPos);
	REICAST_S(BBufPos);
	REICAST_S(GState);
	REICAST_S(BState);
	REICAST_S(GOldClk);
	REICAST_S(BOldClk);
	REICAST_S(BControl);
	REICAST_S(BCmd);
	REICAST_S(BLastCmd);
	REICAST_S(GControl);
	REICAST_S(GCmd);
	REICAST_S(GLastCmd);
	REICAST_S(SerStep);
	REICAST_S(SerStep2);
	REICAST_SA(BSerial,69);
	REICAST_SA(GSerial,69);
	REICAST_S(reg_dimm_3c);
	REICAST_S(reg_dimm_40);
	REICAST_S(reg_dimm_44);
	REICAST_S(reg_dimm_48);
	REICAST_S(reg_dimm_4c);
	REICAST_S(NaomiDataRead);
	REICAST_S(NAOMI_ROM_OFFSETH);
	REICAST_S(NAOMI_ROM_OFFSETL);
	REICAST_S(NAOMI_ROM_DATA);
	REICAST_S(NAOMI_DMA_OFFSETH);
	REICAST_S(NAOMI_DMA_OFFSETL);
	REICAST_S(NAOMI_DMA_COUNT);
	REICAST_S(NAOMI_BOARDID_WRITE);
	REICAST_S(NAOMI_BOARDID_READ);
	REICAST_S(NAOMI_COMM_OFFSET);
	REICAST_S(NAOMI_COMM_DATA);

	REICAST_S(cycle_counter);
#if FEAT_SHREC == DYNAREC_CPP
	REICAST_S(idxnxx);
#else
	REICAST_S(i);
#endif

	REICAST_S(state);
	REICAST_S(div_som_reg1);
	REICAST_S(div_som_reg2);
	REICAST_S(div_som_reg3);



	REICAST_S(LastAddr);
	REICAST_S(LastAddr_min);
	REICAST_SA(block_hash,1024);


	REICAST_SA(RegisterWrite,sh4_reg_count);
	REICAST_SA(RegisterRead,sh4_reg_count);
	REICAST_S(fallback_blocks);
	REICAST_S(total_blocks);
	REICAST_S(REMOVED_OPS);




	REICAST_SA(kcode,4);
	REICAST_SA(rt,4);
	REICAST_SA(lt,4);
	REICAST_SA(vks,4);
	REICAST_SA(joyx,4);
	REICAST_SA(joyy,4);

	REICAST_S(settings.dreamcast.broadcast);
	REICAST_S(settings.dreamcast.cable);
	REICAST_S(settings.dreamcast.region);

	return true ;
}

bool dc_unserialize(void **data, unsigned int *total_size)
{
	int i = 0;
	int j = 0;
	serialize_version_enum version = V1 ;

	*total_size = 0 ;

	REICAST_US(version) ;
	REICAST_US(aica_interr) ;
	REICAST_US(aica_reg_L) ;
	REICAST_US(e68k_out) ;
	REICAST_US(e68k_reg_L) ;
	REICAST_US(e68k_reg_M) ;

	REICAST_USA(arm_Reg,RN_ARM_REG_COUNT);
	REICAST_US(armIrqEnable);
	REICAST_US(armFiqEnable);
	REICAST_US(armMode);
	REICAST_US(Arm7Enabled);
	REICAST_USA(cpuBitsSet,256);
	REICAST_US(intState);
	REICAST_US(stopState);
	REICAST_US(holdState);

	REICAST_US(dsp);

	for ( i = 0 ; i < 3 ; i++)
	{
		REICAST_US(timers[i].c_step);
		REICAST_US(timers[i].m_step);
	}


	REICAST_USA(aica_ram.data,aica_ram.size) ;
	REICAST_US(VREG);
	REICAST_US(ARMRST);
	REICAST_US(rtc_EN);
	REICAST_US(dma_sched_id);

	REICAST_USA(aica_reg,0x8000);



	REICAST_USA(volume_lut,16);
	REICAST_USA(tl_lut,256 + 768);
	REICAST_USA(AEG_ATT_SPS,64);
	REICAST_USA(AEG_DSR_SPS,64);
	REICAST_US(pl);
	REICAST_US(pr);

	channel_unserialize(data, total_size) ;

	REICAST_USA(cdda_sector,CDDA_SIZE);
	REICAST_US(cdda_index);
	REICAST_USA(mxlr,64);
	REICAST_US(samples_gen);


	register_unserialize(sb_regs, data, total_size) ;
	REICAST_US(SB_ISTNRM);
	REICAST_US(SB_FFST_rc);
	REICAST_US(SB_FFST);



	//this is one-time init, no updates - don't need to serialize
	//extern RomChip sys_rom;
	REICAST_US(sys_nvmem.size);
	REICAST_US(sys_nvmem.mask);
#ifdef FLASH_SIZE
	REICAST_US(sys_nvmem.state);
#endif
	REICAST_USA(sys_nvmem.data,sys_nvmem.size);


	//this is one-time init, no updates - don't need to serialize
	//extern _vmem_handler area0_handler;




	REICAST_USA(reply_11,16) ;



	REICAST_US(gdrom_schid);
	REICAST_US(sns_asc);
	REICAST_US(sns_ascq);
	REICAST_US(sns_key);

	REICAST_US(packet_cmd);
	REICAST_US(set_mode_offset);
	REICAST_US(read_params);
	REICAST_US(packet_cmd);
	REICAST_US(read_buff);
	REICAST_US(pio_buff);
	REICAST_US(set_mode_offset);
	REICAST_US(ata_cmd);
	REICAST_US(cdda);
	REICAST_US(gd_state);
	REICAST_US(gd_disk_type);
	REICAST_US(data_write_mode);
	REICAST_US(DriveSel);
	REICAST_US(Error);
	REICAST_US(IntReason);
	REICAST_US(Features);
	REICAST_US(SecCount);
	REICAST_US(SecNumber);
	REICAST_US(GDStatus);
	REICAST_US(ByteCount);
	REICAST_US(i);	// GDROM_TICKS


	REICAST_USA(EEPROM,0x100);
	REICAST_US(EEPROM_loaded);
	REICAST_US(State);


	REICAST_US(maple_ddt_pending_reset);

	mcfg_UnserializeDevices(data, total_size);

	REICAST_US(FrameCount);
	REICAST_US(pend_rend);


	REICAST_USA(YUV_tempdata,512/4);
	REICAST_US(YUV_dest);
	REICAST_US(YUV_blockcount);
	REICAST_US(YUV_x_curr);
	REICAST_US(YUV_y_curr);
	REICAST_US(YUV_x_size);
	REICAST_US(YUV_y_size);

	REICAST_USA(pvr_regs,pvr_RegSize);
	fog_needs_update = true ;

	REICAST_US(in_vblank);
	REICAST_US(clc_pvr_scanline);
	REICAST_US(pvr_numscanlines);
	REICAST_US(prv_cur_scanline);
	REICAST_US(vblk_cnt);
	REICAST_US(Line_Cycles);
	REICAST_US(Frame_Cycles);
	REICAST_US(render_end_schid);
	REICAST_US(vblank_schid);
	REICAST_US(time_sync);
	REICAST_US(speed_load_mspdf);
	REICAST_US(mips_counter);
	REICAST_US(full_rps);
	REICAST_US(fskip);

	REICAST_USA(ta_type_lut,256);
	REICAST_USA(ta_fsm,2049);
	REICAST_US(ta_fsm_cl);

	REICAST_US(tileclip_val);
	REICAST_USA(f32_su8_tbl,65536);
	REICAST_USA(FaceBaseColor,4);
	REICAST_USA(FaceOffsColor,4);
	REICAST_US(SFaceBaseColor);
	REICAST_US(SFaceOffsColor);

	pal_needs_update = true;

	REICAST_USA(vram.data, vram.size);

	REICAST_USA(OnChipRAM.data,OnChipRAM_SIZE);

	register_unserialize(CCN, data, total_size) ;
	register_unserialize(UBC, data, total_size) ;
	register_unserialize(BSC, data, total_size) ;
	register_unserialize(DMAC, data, total_size) ;
	register_unserialize(CPG, data, total_size) ;
	register_unserialize(RTC, data, total_size) ;
	register_unserialize(INTC, data, total_size) ;
	register_unserialize(TMU, data, total_size) ;
	register_unserialize(SCI, data, total_size) ;
	register_unserialize(SCIF, data, total_size) ;

	REICAST_USA(mem_b.data, mem_b.size);



	REICAST_US(IRLPriority);
	REICAST_USA(InterruptEnvId,32);
	REICAST_USA(InterruptBit,32);
	REICAST_USA(InterruptLevelBit,16);
	REICAST_US(interrupt_vpend);
	REICAST_US(interrupt_vmask);
	REICAST_US(decoded_srimask);




	REICAST_US(i) ;
	if ( i == 0 )
		do_sqw_nommu = &do_sqw_nommu_area_3 ;
	else if ( i == 1 )
		do_sqw_nommu = &do_sqw_nommu_area_3_nonvmem ;
	else if ( i == 2 )
		do_sqw_nommu = (sqw_fp*)&TAWriteSQ ;
	else if ( i == 3 )
		do_sqw_nommu = &do_sqw_nommu_full ;



	REICAST_USA((*p_sh4rcb).sq_buffer,64/8);

	//store these before unserializing and then restore after
	//void *getptr = &((*p_sh4rcb).cntx.sr.GetFull) ;
	//void *setptr = &((*p_sh4rcb).cntx.sr.SetFull) ;
	REICAST_US((*p_sh4rcb).cntx);
	//(*p_sh4rcb).cntx.sr.GetFull = getptr ;
	//(*p_sh4rcb).cntx.sr.SetFull = setptr ;

	REICAST_US(old_rm);
	REICAST_US(old_dn);




	REICAST_US(sh4_sched_ffb);
	REICAST_US(sh4_sched_intr);
	REICAST_US(sh4_sched_next_id);
	//this list is populated during initialization so the size will always be the same
	//extern vector<sched_list> list;
	for ( i = 0 ; i < list.size() ; i++ )
	{
		REICAST_US(list[i].tag) ;
		REICAST_US(list[i].start) ;
		REICAST_US(list[i].end) ;
	}



	REICAST_US(aica_schid);
	REICAST_US(rtc_schid);


	REICAST_US(SCIF_SCFSR2);
	REICAST_US(SCIF_SCFRDR2);
	REICAST_US(SCIF_SCFDR2);


	REICAST_US(BSC_PDTRA);




	REICAST_USA(tmu_shift,3);
	REICAST_USA(tmu_mask,3);
	REICAST_USA(tmu_mask64,3);
	REICAST_USA(old_mode,3);
	REICAST_USA(tmu_sched,3);
	REICAST_USA(tmu_ch_base,3);
	REICAST_USA(tmu_ch_base64,3);




	REICAST_USA(CCN_QACR_TR,2);




	REICAST_USA(UTLB,64);
	REICAST_USA(ITLB,4);
#if defined(NO_MMU)
	REICAST_USA(sq_remap,64);
#else
	REICAST_USA(ITLB_LRU_USE,64);
	REICAST_US(mmu_error_TT);
#endif




	REICAST_US(NullDriveDiscType);
	REICAST_USA(q_subchannel,96);

// FIXME
//	REICAST_US(i);	// FLASH_SIZE
//	REICAST_US(i);	// BBSRAM_SIZE
//	REICAST_US(i);	// BIOS_SIZE
//	REICAST_US(i);	// RAM_SIZE
//	REICAST_US(i);	// ARAM_SIZE
//	REICAST_US(i);	// VRAM_SIZE
//	REICAST_US(i);	// RAM_MASK
//	REICAST_US(i);	// ARAM_MASK
//	REICAST_US(i);	// VRAM_MASK



	REICAST_US(naomi_updates);
	REICAST_US(RomPioOffset);
	REICAST_US(DmaOffset);
	REICAST_US(DmaCount);
	REICAST_US(BoardID);
	REICAST_US(GSerialBuffer);
	REICAST_US(BSerialBuffer);
	REICAST_US(GBufPos);
	REICAST_US(BBufPos);
	REICAST_US(GState);
	REICAST_US(BState);
	REICAST_US(GOldClk);
	REICAST_US(BOldClk);
	REICAST_US(BControl);
	REICAST_US(BCmd);
	REICAST_US(BLastCmd);
	REICAST_US(GControl);
	REICAST_US(GCmd);
	REICAST_US(GLastCmd);
	REICAST_US(SerStep);
	REICAST_US(SerStep2);
	REICAST_USA(BSerial,69);
	REICAST_USA(GSerial,69);
	REICAST_US(reg_dimm_3c);
	REICAST_US(reg_dimm_40);
	REICAST_US(reg_dimm_44);
	REICAST_US(reg_dimm_48);
	REICAST_US(reg_dimm_4c);
	REICAST_US(NaomiDataRead);
	REICAST_US(NAOMI_ROM_OFFSETH);
	REICAST_US(NAOMI_ROM_OFFSETL);
	REICAST_US(NAOMI_ROM_DATA);
	REICAST_US(NAOMI_DMA_OFFSETH);
	REICAST_US(NAOMI_DMA_OFFSETL);
	REICAST_US(NAOMI_DMA_COUNT);
	REICAST_US(NAOMI_BOARDID_WRITE);
	REICAST_US(NAOMI_BOARDID_READ);
	REICAST_US(NAOMI_COMM_OFFSET);
	REICAST_US(NAOMI_COMM_DATA);

	REICAST_US(cycle_counter);
#if FEAT_SHREC == DYNAREC_CPP
	REICAST_US(idxnxx);
#else
	REICAST_US(i);
#endif

	REICAST_US(state);
	REICAST_US(div_som_reg1);
	REICAST_US(div_som_reg2);
	REICAST_US(div_som_reg3);




	//REICAST_USA(CodeCache,CODE_SIZE) ;
	//REICAST_USA(SH4_TCB,CODE_SIZE+4096);
	REICAST_US(LastAddr);
	REICAST_US(LastAddr_min);
	REICAST_USA(block_hash,1024);


	REICAST_USA(RegisterWrite,sh4_reg_count);
	REICAST_USA(RegisterRead,sh4_reg_count);
	REICAST_US(fallback_blocks);
	REICAST_US(total_blocks);
	REICAST_US(REMOVED_OPS);




	REICAST_USA(kcode,4);
	REICAST_USA(rt,4);
	REICAST_USA(lt,4);
	REICAST_USA(vks,4);
	REICAST_USA(joyx,4);
	REICAST_USA(joyy,4);


	REICAST_US(settings.dreamcast.broadcast);
	REICAST_US(settings.dreamcast.cable);
	REICAST_US(settings.dreamcast.region);

	return true ;
}
#endif
