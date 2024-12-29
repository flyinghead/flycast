/*
	Sh4 register storage/functions/utilities
*/

#include "types.h"
#include "sh4_core.h"
#include "sh4_interrupts.h"
#if defined(__ANDROID__) && HOST_CPU == CPU_ARM
#include <fenv.h>
#endif

Sh4RCB* p_sh4rcb;

static void ChangeGPR()
{
	std::swap((u32 (&)[8])Sh4cntx.r, Sh4cntx.r_bank);
}

//called when sr is changed and we must check for reg banks etc.
//returns true if interrupt pending
bool UpdateSR()
{
	if (Sh4cntx.sr.MD)
	{
		if (Sh4cntx.old_sr.RB != Sh4cntx.sr.RB)
			ChangeGPR();//bank change
	}
	else
	{
		if (Sh4cntx.old_sr.RB)
			ChangeGPR();//switch
	}

	Sh4cntx.old_sr.status = Sh4cntx.sr.status;
	Sh4cntx.old_sr.RB &= Sh4cntx.sr.MD;

	return SRdecode();
}

// make host and sh4 rounding and denormal modes match
static u32 old_rm = 0xFF;
static u32 old_dn = 0xFF;

static void setHostRoundingMode(u32 roundingMode, u32 denorm2zero)
{
	if (old_rm != roundingMode || old_dn != denorm2zero)
	{
		old_rm = roundingMode;
		old_dn = denorm2zero;
        
        //Correct rounding is required by some games (SOTB, etc)
#ifdef _MSC_VER
        if (roundingMode == 1)	// if round to 0 , set the flag
            _controlfp(_RC_CHOP, _MCW_RC);
        else
            _controlfp(_RC_NEAR, _MCW_RC);
        
        if (denorm2zero == 1)	// denormals are considered 0
            _controlfp(_DN_FLUSH, _MCW_DN);
        else
            _controlfp(_DN_SAVE, _MCW_DN);
#else

    #if HOST_CPU==CPU_X86 || HOST_CPU==CPU_X64

            u32 temp=0x1f80;	//no flush to zero && round to nearest

			if (roundingMode==1)	// if round to 0 , set the flag
				temp|=(3<<13);

			if (denorm2zero == 1)	// denormals are considered 0
				temp|=(1<<15);
			asm("ldmxcsr %0" : : "m"(temp));
    #elif HOST_CPU==CPU_ARM
		static const unsigned int offMask = 0x04086060;
		unsigned int onMask = 0x02000000;

		if (roundingMode == 1)
			onMask |= 3 << 22;

		if (denorm2zero == 1)
			onMask |= 1 << 24;

		#ifdef __ANDROID__
			fenv_t fenv;
			fegetenv(&fenv);
			fenv &= offMask;
			fenv |= onMask;
			fesetenv(&fenv);
		#else
			int raa;
	
			asm volatile
				(
					"fmrx   %0, fpscr   \n\t"
					"and    %0, %0, %1  \n\t"
					"orr    %0, %0, %2  \n\t"
					"fmxr   fpscr, %0   \n\t"
					: "=r"(raa)
					: "r"(offMask), "r"(onMask)
				);
		#endif
	#elif HOST_CPU == CPU_ARM64
		static const unsigned long off_mask = 0x04080000;
        unsigned long on_mask = 0x02000000;    // DN=1 Any operation involving one or more NaNs returns the Default NaN

        if (roundingMode == 1)
        	on_mask |= 3 << 22;

        if (denorm2zero == 1)
        	on_mask |= 1 << 24;	// flush denormalized numbers to zero

        asm volatile
            (
                "MRS    x10, FPCR     \n\t"
                "AND    x10, x10, %0  \n\t"
                "ORR    x10, x10, %1  \n\t"
                "MSR    FPCR, x10     \n\t"
                :
                : "r"(off_mask), "r"(on_mask)
				: "x10"
            );
    #else
	#error "SetFloatStatusReg: Unsupported platform"
    #endif
#endif

	}
}

//called when fpscr is changed and we must check for reg banks etc..
void DYNACALL Sh4Context::UpdateFPSCR(Sh4Context *ctx)
{
	if (ctx->fpscr.FR != ctx->old_fpscr.FR)
		// FPU bank change
		std::swap(ctx->xf, ctx->fr);

	ctx->old_fpscr = ctx->fpscr;
	setHostRoundingMode(ctx->fpscr.RM, ctx->fpscr.DN);
}

void Sh4Context::restoreHostRoundingMode()
{
	old_rm = 0xFF;
	old_dn = 0xFF;
	setHostRoundingMode(fpscr.RM, fpscr.DN);
}

void setDefaultRoundingMode()
{
	setHostRoundingMode(0, 0);
}
