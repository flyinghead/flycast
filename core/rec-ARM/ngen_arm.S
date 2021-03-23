@@

#include "build.h"

.arm
.align 8

.equ SH4_TIMESLICE, 448

#if defined(__APPLE__)
#define CSYM(n) _##n
#define HIDDEN(n)
#else
#define CSYM(n) n
#define HIDDEN(n) .hidden CSYM(n)
#endif

@@@@@@@@@@ some helpers @@@@@@@@@@

.global CSYM(do_sqw_nommu_area_3)
HIDDEN(do_sqw_nommu_area_3)
@r0: addr
@r1: sq_both
CSYM(do_sqw_nommu_area_3):
add r3,r1,#0x0C000000	@ get ram ptr from r1, part 1
and r2,r0,#0x20			@ SQ# selection, isolate
ubfx r0,r0,#5,#20		@ get ram offset
add r1,r2				@ SQ# selection, add to SQ ptr
add r3,#512				@ get ram ptr from r1, part 2
add r3,r0,lsl #5		@ ram + offset
vldm r1,{d0-d3}
vstm r3,{d0-d3}
bx lr

#if FEAT_SHREC != DYNAREC_NONE

@@@@@@@@@@ ngen_LinkBlock_*****_stub @@@@@@@@@@

.global CSYM(ngen_LinkBlock_Generic_stub)
HIDDEN(ngen_LinkBlock_Generic_stub)
CSYM(ngen_LinkBlock_Generic_stub):

    mov r1,r4	@ djump/pc -> in case we need it ..
	b CSYM(ngen_LinkBlock_Shared_stub)


.global CSYM(ngen_LinkBlock_cond_Branch_stub)
HIDDEN(ngen_LinkBlock_cond_Branch_stub)
CSYM(ngen_LinkBlock_cond_Branch_stub):

	mov r1,#1
	b CSYM(ngen_LinkBlock_Shared_stub)
    
.global CSYM(ngen_LinkBlock_cond_Next_stub)
HIDDEN(ngen_LinkBlock_cond_Next_stub)
CSYM(ngen_LinkBlock_cond_Next_stub):

	mov r1,#0
	b CSYM(ngen_LinkBlock_Shared_stub)


.global CSYM(ngen_LinkBlock_Shared_stub)
HIDDEN(ngen_LinkBlock_Shared_stub)
CSYM(ngen_LinkBlock_Shared_stub):

	mov r0,lr
    sub r0,#4                  @go before the call
    bl CSYM(rdv_LinkBlock)
    bx r0

@@@@@@@@@@ ngen_FailedToFindBlock_ @@@@@@@@@@


.global CSYM(ngen_FailedToFindBlock_)
HIDDEN(ngen_FailedToFindBlock_)
CSYM(ngen_FailedToFindBlock_):
	mov r0,r4
    bl CSYM(rdv_FailedToFindBlock)
    bx r0

@@@@@@@@@@ ngen_blockcheckfail @@@@@@@@@@

.global CSYM(ngen_blockcheckfail)
HIDDEN(ngen_blockcheckfail)
CSYM(ngen_blockcheckfail):
    bl CSYM(rdv_BlockCheckFail)
    bx r0


@@@@@@@@@@ ngen_mainloop @@@@@@@@@@

@ you can load the address of the sh4 reg struct on the mainloop init
@ using (u8*)regptr-(u8*)Sh4cntx
@ all registers are < 1024 bytes from that
@ so you can use reg+imm forms for it



.global CSYM(ngen_mainloop)
HIDDEN(ngen_mainloop)
CSYM(ngen_mainloop):

push { r4-r12,lr }


#if defined(__APPLE__)
	mov r11, #SH4_TIMESLICE @ load cycle counter
#else
	mov r9, #SH4_TIMESLICE @ load cycle counter
#endif

    mov r8, r0			@Load context
	ldr r4, [r8,#-184]	@load pc
	
	b CSYM(no_update)			@Go to mainloop !


	@this code is here for fall-through behavior of do_iter
	.global CSYM(intc_sched)
	HIDDEN(intc_sched)
CSYM(intc_sched):        @ next_pc _MUST_ be on ram
#if defined(__APPLE__)
    add r11,r11,#SH4_TIMESLICE
#else
    add r9,r9,#SH4_TIMESLICE
#endif
	mov r4,lr
    bl CSYM(UpdateSystem)
	mov lr,r4
	cmp r0,#0
	bne CSYM(do_iter)
	ldr r0,[r8,#-156]         @load CpuRunning
	cmp r0,#0
	bxne lr

HIDDEN(do_iter)
CSYM(do_iter):
	mov r0,r4
	bl CSYM(rdv_DoInterrupts)
	mov r4,r0

.global CSYM(no_update)
HIDDEN(no_update)
CSYM(no_update):              @ next_pc _MUST_ be on r4 *R4 NOT R0 anymore*

	ldr r0,[r8,#-156]         @load CpuRunning
	cmp r0,#0
	beq CSYM(cleanup)

#if RAM_SIZE_MAX == 33554432
	sub r2,r8,#0x4100000
	ubfx r1,r4,#1,#24			@ 24+1 bits: 32 MB
								@ RAM wraps around so if actual RAM size is 16MB, we won't overflow
#elif RAM_SIZE_MAX == 16777216
	sub r2,r8,#0x2100000
	ubfx r1,r4,#1,#23			@ 23+1 bits: 16 MB
#else
#error "Define RAM_SIZE_MAX"
#endif
	ldr pc,[r2,r1,lsl #2]

	@bic r1,r4,#0xFF000000
	@ldr pc,[r2,r1,lsl #1]

HIDDEN(cleanup)
CSYM(cleanup):

pop {r4-r12,lr}
bx lr

end_ngen_mainloop:
@@@@@@@@@@ ngen_mainloop @@@@@@@@@@

#endif
