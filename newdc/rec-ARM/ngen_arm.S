@@

.arm
.align 8

.equ SH4_TIMESLICE, 448
.equ BM_BLOCKLIST_MASK, 65532 @FFFC
.equ CPU_RATIO, 5


@@@@@@@@@@ some helpers @@@@@@@@@@

.global do_sqw_nommu_area_3
@r0: addr
@r1: sq_both
do_sqw_nommu_area_3:
add r3,r1,#0x0C000000	@ get ram ptr from r1, part 1
and r2,r0,#0x20			@ SQ# selection, isolate
ubfx r0,r0,#5,#19		@ get ram offset
add r1,r2				@ SQ# selection, add to SQ ptr
add r3,#512				@ get ram ptr from r1, part 2
add r3,r0,lsl #5		@ ram + offset
vldm r1,{d0-d3}
vstm r3,{d0-d3}
bx lr

.global TAWriteSQ
@r0: addr
@r1: sq_both
TAWriteSQ:
BIC     R3, R0, #0xFE000000		@clear unused bits
AND     R0, R0, #0x20			@SQ#, isolate
CMP     R3, #0x800000			@TA write?
ADD     R0, R1, R0				@SQ#, add to SQ ptr
BCC     _Z13ta_vtx_data32Pv	@TA write?

TAWriteSQ_yuv:
CMP     R3, #0x1000000	@Yuv write ?
BCS     TAWriteSQ_vram
MOV     R1, #1
B       _Z8YUV_dataPjj	

TAWriteSQ_vram:				@vram write ..
bkpt
ubfx r0,r3,#5,#18		@ get vram offset
add r3,r1,#0x04000000	@ get vram ptr from r1, part 1
add r3,#512				@ get ram ptr from r1, part 2
add r3,r0,lsl #5		@ ram + offset
vldm r1,{d0-d3}
vstm r3,{d0-d3}
bx lr

@@@@@@@@@@ ngen_LinkBlock_*****_stub @@@@@@@@@@

.global ngen_LinkBlock_Generic_stub
ngen_LinkBlock_Generic_stub:

    mov r1,r4	@ djump/pc -> in case we need it ..
	b ngen_LinkBlock_Shared_stub


.global ngen_LinkBlock_cond_Branch_stub
ngen_LinkBlock_cond_Branch_stub:

	mov r1,#1
	b ngen_LinkBlock_Shared_stub
    
.global ngen_LinkBlock_cond_Next_stub    
ngen_LinkBlock_cond_Next_stub:

	mov r1,#0
	b ngen_LinkBlock_Shared_stub


.global ngen_LinkBlock_Shared_stub
ngen_LinkBlock_Shared_stub:

	mov r0,lr
    sub r0,#4                  @go before the call
    bl rdv_LinkBlock
    bx r0

@@@@@@@@@@ ngen_FailedToFindBlock_ @@@@@@@@@@


.global ngen_FailedToFindBlock_
ngen_FailedToFindBlock_:
	mov r0,r4
    bl rdv_FailedToFindBlock
    bx r0

@@@@@@@@@@ ngen_blockcheckfail @@@@@@@@@@

.global ngen_blockcheckfail
ngen_blockcheckfail:
    bl rdv_BlockCheckFail
    bx r0


@@@@@@@@@@ ngen_mainloop @@@@@@@@@@

@ you can load the address of the sh4 reg struct on the mainloop init
@ using (u8*)regptr-(u8*)Sh4cntx
@ all registers are < 1024 bytes from that
@ so you can use reg+imm forms for it



.global ngen_mainloop
ngen_mainloop:

push { r4-r12,lr }


	mov r9, #SH4_TIMESLICE @ load cycle counter

    mov r8, r0			@Load context
	ldr r4, [r8,#-184]	@load pc
	
	b no_update			@Go to mainloop !


	@this code is here for fall-through behavior of do_iter
	.global intc_sched
intc_sched:        @ next_pc _MUST_ be on ram
    add r9,r9,#SH4_TIMESLICE
	mov r4,lr
    bl UpdateSystem
	mov lr,r4
	cmp r0,#0
	bxeq lr			@faster than bxeq r4 (as it should, call stack cache)

do_iter:
	mov r0,r4
	bl rdv_DoInterrupts
	mov r4,r0

.global no_update
no_update:              @ next_pc _MUST_ be on r4 *R4 NOT R0 anymore*

	sub r2,r8,#33816576
	ubfx r1,r4,#1,#23
	ldr pc,[r2,r1,lsl #2]

	@bic r1,r4,#0xFF000000
	@ldr pc,[r2,r1,lsl #1]


pop {r4-r12,lr}
bx lr

end_ngen_mainloop:
@@@@@@@@@@ ngen_mainloop @@@@@@@@@@

.global arm_compilecode
arm_compilecode:
bl CompileCode
b arm_dispatch

.global arm_mainloop
arm_mainloop: @(cntx,lookup_base,cycles)

push {r4,r5,r8,r9,lr}

	ldr r8,=arm_Reg			@load cntx
	ldr r4,=EntryPoints		@load lookup base

	ldr r5,[r8,#192]	@load cycle count
	add r5,r0			@add cycles for this timeslice

	b arm_dispatch

.global arm_dispatch
arm_dispatch:
	ldrd r0,[r8,#184]		@load: Next PC, interrupt
	ubfx r2,r0,#2,#19
	cmp r1,#0
	bne arm_dofiq

	ldr pc,[r4,r2,lsl #2]
	
arm_dofiq:
	bl CPUFiq
	b arm_dispatch

.global arm_exit
arm_exit:
	str r5,[r8,#192]		@if timeslice is over, save remaining cycles
	pop {r4,r5,r8,r9,pc}	

@@@@@@
@matrix mul
#ifndef _ANDROID
.global ftrv_asm
ftrv_asm:

@r0=dst,r1=vec,r2=mtx

@3x vld1.32 might be faster
vldm r2,{d16-d24}
vldm r1, {d0-d1} 

VMUL.F32 Q2,Q8,d0[0]
VMLA.F32 Q2,Q9,d0[1]
VMLA.F32 Q2,Q10,d1[0]
VMLA.F32 Q2,Q11,d1[1]

vstm r0,{d4,d5}

bx lr

.global fipr_asm
fipr_asm:

@ vdot
@		idp=fr[n+0]*fr[m+0];
@		idp+=fr[n+1]*fr[m+1];
@		idp+=fr[n+2]*fr[m+2];
@		idp+=fr[n+3]*fr[m+3];


vldm r0, {d0,d1}
vldm r1, {d2,d3}

vmul.f32 q0,q1
@NEON is quite nice actually ! if only its performance was good enough ...
vpadd.f32 d0,d0,d1 @d0={d0[0]+d0[1], d1[0]+d1[1]}
vpadd.f32 d0,d0,d0 @d0={d0[0]+d0[1]+d1[0]+d1[1], d0[0]+d0[1]+d1[0]+d1[1]}

@store to ret ..
vmov r0,s0
bx lr

#endif
