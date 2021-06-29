/*
	Sh4 register storage/functions/utilities
*/

#include "types.h"
#include "sh4_core.h"
#include "sh4_interrupts.h"


Sh4RCB* p_sh4rcb;
sh4_if  sh4_cpu;

static INLINE void ChangeGPR()
{
	std::swap((u32 (&)[8])r, r_bank);
}

static INLINE void ChangeFP()
{
	std::swap((f32 (&)[16])Sh4cntx.xffr, *(f32 (*)[16])&Sh4cntx.xffr[16]);
}

//called when sr is changed and we must check for reg banks etc.
//returns true if interrupt pending
bool UpdateSR()
{
	if (sr.MD)
	{
		if (old_sr.RB != sr.RB)
			ChangeGPR();//bank change
	}
	else
	{
		if (old_sr.RB)
			ChangeGPR();//switch
	}

	old_sr.status = sr.status;
	old_sr.RB &= sr.MD;

	return SRdecode();
}

// make host and sh4 rounding and denormal modes match
static u32 old_rm = 0xFF;
static u32 old_dn = 0xFF;

static void setHostRoundingMode()
{
	if ((old_rm!=fpscr.RM) || (old_dn!=fpscr.DN))
	{
		old_rm=fpscr.RM ;
		old_dn=fpscr.DN ;
        
        //Correct rounding is required by some games (SOTB, etc)
#ifdef _MSC_VER
        if (fpscr.RM == 1)  //if round to 0 , set the flag
            _controlfp(_RC_CHOP, _MCW_RC);
        else
            _controlfp(_RC_NEAR, _MCW_RC);
        
        if (fpscr.DN)     //denormals are considered 0
            _controlfp(_DN_FLUSH, _MCW_DN);
        else
            _controlfp(_DN_SAVE, _MCW_DN);
#else

    #if HOST_CPU==CPU_X86 || HOST_CPU==CPU_X64

            u32 temp=0x1f80;	//no flush to zero && round to nearest

			if (fpscr.RM==1)  //if round to 0 , set the flag
				temp|=(3<<13);

			if (fpscr.DN)     //denormals are considered 0
				temp|=(1<<15);
			asm("ldmxcsr %0" : : "m"(temp));
    #elif HOST_CPU==CPU_ARM
		static const unsigned int x = 0x04086060;
		unsigned int y = 0x02000000;
		if (fpscr.RM==1)  //if round to 0 , set the flag
			y|=3<<22;
	
		if (fpscr.DN)
			y|=1<<24;


		int raa;

		asm volatile
			(
				"fmrx   %0, fpscr   \n\t"
				"and    %0, %0, %1  \n\t"
				"orr    %0, %0, %2  \n\t"
				"fmxr   fpscr, %0   \n\t"
				: "=r"(raa)
				: "r"(x), "r"(y)
			);
	#elif HOST_CPU == CPU_ARM64
		static const unsigned long off_mask = 0x04080000;
        unsigned long on_mask = 0x02000000;    // DN=1 Any operation involving one or more NaNs returns the Default NaN

        if (fpscr.RM == 1)		// if round to 0, set the flag
        	on_mask |= 3 << 22;

        if (fpscr.DN)
        	on_mask |= 1 << 24;	// flush denormalized numbers to zero

        asm volatile
            (
                "MRS    x10, FPCR     \n\t"
                "AND    x10, x10, %0  \n\t"
                "ORR    x10, x10, %1  \n\t"
                "MSR    FPCR, x10     \n\t"
                :
                : "r"(off_mask), "r"(on_mask)
            );
    #else
	#error "SetFloatStatusReg: Unsupported platform"
    #endif
#endif

	}
}

//called when fpscr is changed and we must check for reg banks etc..
void UpdateFPSCR()
{
	if (fpscr.FR !=old_fpscr.FR)
		ChangeFP(); // FPU bank change

	old_fpscr=fpscr;
	setHostRoundingMode();
}

void RestoreHostRoundingMode()
{
	old_rm = 0xFF;
	old_dn = 0xFF;
	setHostRoundingMode();
}

static u32* Sh4_int_GetRegisterPtr(Sh4RegType reg)
{
	if ((reg>=reg_r0) && (reg<=reg_r15))
	{
		return &r[reg-reg_r0];
	}
	else if ((reg>=reg_r0_Bank) && (reg<=reg_r7_Bank))
	{
		return &r_bank[reg-reg_r0_Bank];
	}
	else if ((reg>=reg_fr_0) && (reg<=reg_fr_15))
	{
		return &fr_hex[reg-reg_fr_0];
	}
	else if ((reg>=reg_xf_0) && (reg<=reg_xf_15))
	{
		return &xf_hex[reg-reg_xf_0];
	}
	else
	{
		switch(reg)
		{
		case reg_gbr :
			return &gbr;
			break;
		case reg_vbr :
			return &vbr;
			break;

		case reg_ssr :
			return &ssr;
			break;

		case reg_spc :
			return &spc;
			break;

		case reg_sgr :
			return &sgr;
			break;

		case reg_dbr :
			return &dbr;
			break;

		case reg_mach :
			return &mac.h;
			break;

		case reg_macl :
			return &mac.l;
			break;

		case reg_pr :
			return &pr;
			break;

		case reg_fpul :
			return &fpul;
			break;


		case reg_nextpc :
			return &next_pc;
			break;

		case reg_old_sr_status :
			return &old_sr.status;
			break;

		case reg_sr_status :
			return &sr.status;
			break;

		case reg_sr_T :
			return &sr.T;
			break;

		case reg_old_fpscr :
			return &old_fpscr.full;
			break;

		case reg_fpscr :
			return &fpscr.full;
			break;

		case reg_pc_dyn:
			return &Sh4cntx.jdyn;

		case reg_temp:
			return &Sh4cntx.temp_reg;

		default:
			ERROR_LOG(SH4, "Unknown register ID %d", reg);
			die("Invalid reg");
			return 0;
			break;
		}
	}
}

u32* GetRegPtr(u32 reg)
{
	return Sh4_int_GetRegisterPtr((Sh4RegType)reg);
}
