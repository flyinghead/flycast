.hidden context_switch_aarch64
.globl context_switch_aarch64
context_switch_aarch64:
    // Restore FPU regs
    ldp d0, d1, [x0, #0x00]
    ldp d2, d3, [x0, #0x10]
    ldp d4, d5, [x0, #0x20]
    ldp d6, d7, [x0, #0x30]
    ldp d8, d9, [x0, #0x40]
    ldp d10, d11, [x0, #0x50]
    ldp d12, d13, [x0, #0x60]
    ldp d14, d15, [x0, #0x70]
    ldp d16, d17, [x0, #0x80]
    ldp d18, d19, [x0, #0x90]
    ldp d20, d21, [x0, #0xa0]
    ldp d22, d23, [x0, #0xb0]
    ldp d24, d25, [x0, #0xc0]
    ldp d26, d27, [x0, #0xd0]
    ldp d28, d29, [x0, #0xe0]
    ldp d30, d31, [x0, #0xf0]

    // Restore GPRS regs
    ldr x1, [x0, #0x108]
    ldp x2, x3, [x0, #0x110]
    ldp x4, x5, [x0, #0x120]
    ldp x6, x7, [x0, #0x130]
    ldp x8, x9, [x0, #0x140]
    ldp x10, x11, [x0, #0x150]
    ldp x12, x13, [x0, #0x160]
    ldp x14, x15, [x0, #0x170]
    ldp x16, x17, [x0, #0x180]
    ldp x18, x19, [x0, #0x190]
    ldp x20, x21, [x0, #0x1a0]
    ldp x22, x23, [x0, #0x1b0]
    ldp x24, x25, [x0, #0x1c0]
    ldp x26, x27, [x0, #0x1d0]
    ldp x28, x29, [x0, #0x1e0]	// x29 is FP
    // Restore special regs and jump
    // x16 and x17 are safe to use?
    ldp x30, x17, [x0, #0x1f0]	// LR, pstate
    msr nzcv, x17
    ldr x17, [x0, #0x200]		// SP
    mov sp, x17
    ldr x17, [x0, #0x208]		// PC
    ldr x0, [x0, #0x100]
    br x17
