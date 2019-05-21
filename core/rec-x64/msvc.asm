_TEXT    SEGMENT

SH4_TIMESLICE = 448
CPU_RUNNING = 135266148
PC = 135266120

EXTERN bm_GetCode2: PROC
EXTERN UpdateSystem_INTC: PROC
EXTERN cycle_counter: dword
EXTERN p_sh4rcb: qword

PUBLIC ngen_mainloop
ngen_mainloop PROC

	push rbx
	push rbp
	push rdi
	push rsi
	push r12
	push r13
	push r14
	push r15
	sub rsp, 40                 ; 32-byte shadow space + 8 for stack 16-byte alignment

	mov dword ptr [cycle_counter], SH4_TIMESLICE

run_loop:
	mov rax, qword ptr [p_sh4rcb]
	mov edx, dword ptr[CPU_RUNNING + rax]
	test edx, edx
	je end_run_loop

slice_loop:
	mov rax, qword ptr [p_sh4rcb]
	mov ecx, dword ptr[PC + rax]
	call bm_GetCode2
	call rax
	mov ecx, dword ptr [cycle_counter]
	test ecx, ecx
	jg slice_loop

	add ecx, SH4_TIMESLICE
	mov dword ptr [cycle_counter], ecx
	call UpdateSystem_INTC
	jmp run_loop

end_run_loop:

	add rsp, 40
	pop r15
	pop r14
	pop r13
	pop r12
	pop rsi
	pop rdi
	pop rbp
	pop rbx
	ret
ngen_mainloop ENDP
_TEXT    ENDS
END
