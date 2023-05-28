/*******************************************************************************
 *
 * Copyright (c) 2017, snickerbockers <chimerasaurusrex@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

#ifndef SH4ASM_H_
#define SH4ASM_H_

#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

// this must match the signature of emit_bin_handler_func in sh4_bin_emit.h
typedef void(*sh4asm_emit_func)(uint16_t);

typedef void(*sh4asm_error_func)(char const*, va_list)
    __attribute__((__noreturn__));

void sh4asm_reset(void);
void sh4asm_set_emitter(sh4asm_emit_func emit);
void sh4asm_set_error_handler(sh4asm_error_func handler);
void sh4asm_error(char const *fmt, ...) __attribute__((__noreturn__));
void sh4asm_input_char(char ch);
void sh4asm_input_string(char const *txt);

// assembly with printf-style substitutions.  Only the %u substitution pattern
// is supported at this time.
void sh4asm_printf(char const *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
