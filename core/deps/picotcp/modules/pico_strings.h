/*********************************************************************
   PicoTCP. Copyright (c) 2015-2017 Altran ISY BeNeLux. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

   .

   Author: Michele Di Pede
 *********************************************************************/

#ifndef PICO_STRINGS_H
#define PICO_STRINGS_H
#include <stddef.h>
#include <stdint.h>

char *get_string_terminator_position(char *const block, size_t len);
int pico_strncasecmp(const char *const str1, const char *const str2, size_t n);
size_t pico_strnlen(const char *str, size_t n);

int num2string(int32_t num, char *buf, int len);

#endif
