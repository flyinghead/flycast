#ifndef HAD_CONFIG_H
#define HAD_CONFIG_H
#ifndef _HAD_ZIPCONF_H
#include "zipconf.h"
#endif
/* BEGIN DEFINES */
/* #undef HAVE___PROGNAME */
#define HAVE__CLOSE
#define HAVE__DUP
#define HAVE__FDOPEN
#define HAVE__FILENO
#define HAVE__SETMODE
/* #undef HAVE__SNPRINTF */
#define HAVE__STRDUP
#define HAVE__STRICMP
#define HAVE__STRTOI64
#define HAVE__STRTOUI64
/* #undef HAVE__UMASK */
#define HAVE__UNLINK
/* #undef HAVE_ARC4RANDOM */
/* #undef HAVE_CLONEFILE */
/* #undef HAVE_COMMONCRYPTO */
/* #undef HAVE_CRYPTO */
/* #undef HAVE_FICLONERANGE */
#define HAVE_FILENO
#define HAVE_FSEEKO
#define HAVE_FTELLO
/* #undef HAVE_GETPROGNAME */
/* #undef HAVE_GNUTLS */
/* #undef HAVE_LIBBZ2 */
/* #undef HAVE_LIBLZMA */
/* #undef HAVE_LIBZSTD */
/* #undef HAVE_LOCALTIME_R */
/* #undef HAVE_MBEDTLS */
/* #undef HAVE_MKSTEMP */
/* #undef HAVE_NULLABLE */
/* #undef HAVE_OPENSSL */
#define HAVE_SETMODE
/* #undef HAVE_STRCASECMP */
#define HAVE_STRDUP
#define HAVE_STRICMP
#define HAVE_STRTOLL
#define HAVE_STRTOULL
/* #undef HAVE_STRUCT_TM_TM_ZONE */
#define HAVE_STDBOOL_H
/* #undef HAVE_STRINGS_H */
/* #undef HAVE_UNISTD_H */
/* #undef HAVE_WINDOWS_CRYPTO */
#define SIZEOF_OFF_T 4
#define SIZEOF_SIZE_T 8
/* #undef HAVE_DIRENT_H */
/* #undef HAVE_FTS_H */
/* #undef HAVE_NDIR_H */
/* #undef HAVE_SYS_DIR_H */
/* #undef HAVE_SYS_NDIR_H */
/* #undef WORDS_BIGENDIAN */
/* #undef HAVE_SHARED */
/* END DEFINES */
#define PACKAGE "flycast"
#define VERSION ""

/* FIXME This is for FLAC */
#define PACKAGE_VERSION "1.3.2"
#define FLAC__HAS_OGG 0
#define FLAC__NO_DLL 1
#define HAVE_LROUND 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#ifndef _MSC_VER
#define HAVE_SYS_PARAM_H 1
#endif

#endif /* HAD_CONFIG_H */
