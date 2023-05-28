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

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "sh4asm.h"

static sh4asm_error_func error_func;

void sh4asm_set_emitter(sh4asm_emit_func emit) {
    sh4asm_parser_set_emitter(emit);
}

void sh4asm_input_char(char ch) {
    sh4asm_lexer_input_char(ch, sh4asm_parser_input_token);
}

// preprocessor state
enum pp_state {
    // normal operation
    PP_STATE_NORM,

    // previous char was %-character
    PP_STATE_PERCENT
};

static enum pp_state state = PP_STATE_NORM;

static char const *unsigned_to_str(unsigned uval) {
#define CONVBUF_LEN 16
    static char buf[CONVBUF_LEN];
    unsigned place = 1000000000;
    unsigned idx = 0;
    unsigned len = 0;
    bool leading_zero = true;

    // special case since the leading_zero won't return anything otherwise
    if (!uval) {
        buf[0] = '0';
        buf[1] = '\0';
        return buf;
    }

    memset(buf, 0, sizeof(buf));

    while (place && idx < (CONVBUF_LEN - 1)) {
        unsigned digit = uval / place;
        if (leading_zero && digit == 0) {
            place /= 10;
            continue;
        } else {
            leading_zero = false;
        }

        buf[idx++] = digit + '0';
        uval -= digit * place;
        place /= 10;
        len++;
    }

    return buf;
}

void sh4asm_printf(char const *fmt, ...) {
    char ch;
    va_list arg;
    unsigned uval;
    char const *txtp;

    va_start(arg, fmt);

    while ((ch = *fmt++)) {
        switch (state) {
        case PP_STATE_NORM:
            if (ch == '%')
                state = PP_STATE_PERCENT;
            else
                sh4asm_lexer_input_char(ch, sh4asm_parser_input_token);
            break;
        case PP_STATE_PERCENT:
            switch (ch) {
            case '%':
                sh4asm_lexer_input_char('%', sh4asm_parser_input_token);
                break;
            case 'u':
                uval = va_arg(arg, unsigned);
                txtp = unsigned_to_str(uval);
                while (*txtp)
                    sh4asm_lexer_input_char(*txtp++, sh4asm_parser_input_token);
                break;
            default:
                printf("ERROR: only the %%-character is allowed for printf-style "
                       "substitutions\n");
            }
            state = PP_STATE_NORM;
            break;
        default:
            // this ought to be impossible
            printf("error - %s - state is %d\n", __func__, state);
            break;
        }
    }
    va_end(arg);
}

void sh4asm_input_string(char const *txt) {
    while (*txt)
        sh4asm_input_char(*txt++);
}

void sh4asm_set_error_handler(sh4asm_error_func handler) {
    error_func = handler;
}

__attribute__((__noreturn__)) void sh4asm_error(char const *fmt, ...) {
    va_list arg;

    va_start(arg, fmt);
    error_func(fmt, arg);
    va_end(arg);
}

void sh4asm_reset(void) {
    state = PP_STATE_NORM;
    sh4asm_parser_reset();
    sh4asm_lexer_reset();
}
