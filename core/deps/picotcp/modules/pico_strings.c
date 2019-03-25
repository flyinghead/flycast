/*********************************************************************
   PicoTCP. Copyright (c) 2015-2017 Altran ISY BeNeLux. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.



   Author: Michele Di Pede
 *********************************************************************/

#include <ctype.h>
#include <stdlib.h>
#include "pico_strings.h"

char *get_string_terminator_position(char *const block, size_t len)
{
    size_t length = pico_strnlen(block, len);

    return (len != length) ? (block + length) : 0;
}

int pico_strncasecmp(const char *const str1, const char *const str2, size_t n)
{
    int ch1;
    int ch2;
    size_t i;

    for (i = 0; i < n; ++i) {
        ch1 = toupper(*(str1 + i));
        ch2 = toupper(*(str2 + i));
        if (ch1 < ch2)
            return -1;

        if (ch1 > ch2)
            return 1;

        if ((!ch1) && (!ch2))
            return 0;
    }
    return 0;
}

size_t pico_strnlen(const char *str, size_t n)
{
    size_t len = 0;

    if (!str)
        return 0;

    for (; len < n && *(str + len); ++len)
        ; /* TICS require this empty statement here */

    return len;
}

static inline int num2string_validate(int32_t num, char *buf, int len)
{
    if (num < 0)
        return -1;

    if (!buf)
        return -2;

    if (len < 2)
        return -3;

    return 0;
}

static inline int revert_and_shift(char *buf, int len, int pos)
{
    int i;

    len -= pos;
    for (i = 0; i < len; ++i)
        buf[i] = buf[i + pos];
    return len;
}

int num2string(int32_t num, char *buf, int len)
{
    ldiv_t res;
    int pos = 0;

    if (num2string_validate(num, buf, len))
        return -1;

    pos = len;
    buf[--pos] = '\0';

    res.quot = (long)num;

    do {
        if (!pos)
            return -3;

        res = ldiv(res.quot, 10);
        buf[--pos] = (char)((res.rem + '0') & 0xFF);
    } while (res.quot);

    return revert_and_shift(buf, len, pos);
}
