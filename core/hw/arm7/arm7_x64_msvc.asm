_TEXT    SEGMENT

;SH4_TIMESLICE = 448
;CPU_RUNNING = 135266148
;PC = 135266120

;EXTERN bm_GetCodeByVAddr: PROC
;EXTERN UpdateSystem_INTC: PROC
;EXTERN setjmp: PROC
;EXTERN cycle_counter: dword
;EXTERN p_sh4rcb: qword
;EXTERN jmp_env: qword
EXTERN CompileCode: PROC
EXTERN CPUFiq: PROC
EXTERN arm_Reg: PTR DWORD
EXTERN entry_points: QWORD

PUBLIC arm_mainloop
arm_mainloop PROC FRAME			; arm_mainloop(regs, entry points)
	push rdi
	.pushreg rdi
	push rsi
	.pushreg rsi
	push r12
	.pushreg r12
	push r13
	.pushreg r13
	push r14
	.pushreg r14
	push r15
	.pushreg r15
	push rbx
	.pushreg rbx
	push rbp
	.pushreg rbp
	sub rsp, 40					; 32-byte shadow space + 16-byte stack alignment
	.allocstack 40
	.endprolog

	mov qword ptr [entry_points], rdx

PUBLIC arm_dispatch
arm_dispatch::
	mov rdx, qword ptr [entry_points]
	mov ecx, dword ptr [arm_Reg + 184]		; R15_ARM_NEXT
	mov eax, dword ptr [arm_Reg + 188]		; INTR_PEND
	cmp dword ptr [arm_Reg + 192], 0
	jle arm_exit							; timeslice is over
	test eax, eax
	jne arm_dofiq							; if interrupt pending, handle it

	and ecx, 7FFFFCh
	jmp qword ptr [rdx + rcx * 2]

arm_dofiq:
	call CPUFiq
	jmp arm_dispatch

arm_exit:
	add rsp, 40
	pop rbp
	pop rbx
	pop r15
	pop r14
	pop r13
	pop r12
	pop rsi
	pop rdi
	ret
arm_mainloop ENDP

PUBLIC arm_compilecode
arm_compilecode PROC
	call CompileCode
	jmp arm_dispatch
arm_compilecode ENDP

_TEXT    ENDS
END
