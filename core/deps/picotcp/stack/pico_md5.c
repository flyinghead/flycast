/*********************************************************************
 *    PicoTCP. Copyright (c) 2015-2017 Altran Intelligent Systems. Some rights reserved.
 *    See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.
 *
 *    Authors: Daniele Lacamera
 *    *********************************************************************/


#include <pico_md5.h>

#if defined (PICO_SUPPORT_CYASSL)
#include <cyassl/ctaocrypt/md5.h>

void pico_md5sum(uint8_t *dst, const uint8_t *src, size_t len)
{
    Md5 md5;
    InitMd5(&md5);
    Md5Update(&md5, src, len);
    Md5Final(&md5, dst);
}

#elif defined (PICO_SUPPORT_POLARSSL)
#include <polarssl/md5.h>

void pico_md5sum(uint8_t *dst, const uint8_t *src, size_t len)
{
    md5(src, len, dst);
}

#else
static void (*do_pico_md5sum)(uint8_t *dst, const uint8_t *src, size_t len);
void pico_md5sum(uint8_t *dst, const uint8_t *src, size_t len)
{
    if (do_pico_md5sum) {
        do_pico_md5sum(dst, src, len);
    }
}

void pico_register_md5sum(void (*md5)(uint8_t *, const uint8_t *, size_t))
{
    do_pico_md5sum = md5;
}
#endif
