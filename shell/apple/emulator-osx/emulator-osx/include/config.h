#ifndef HAD_CONFIG_H
#define HAD_CONFIG_H
#ifndef _HAD_ZIPCONF_H
#include "zipconf.h"
#endif
/* BEGIN DEFINES */
/* #undef HAVE___PROGNAME */
/* #undef HAVE__CLOSE */
/* #undef HAVE__DUP */
/* #undef HAVE__FDOPEN */
/* #undef HAVE__FILENO */
/* #undef HAVE__SETMODE */
/* #undef HAVE__SNPRINTF */
/* #undef HAVE__STRDUP */
/* #undef HAVE__STRICMP */
/* #undef HAVE__STRTOI64 */
/* #undef HAVE__STRTOUI64 */
/* #undef HAVE__UMASK */
/* #undef HAVE__UNLINK */
#define HAVE_ARC4RANDOM
#define HAVE_CLONEFILE
/* #undef HAVE_COMMONCRYPTO */
/* #undef HAVE_CRYPTO */
/* #undef HAVE_FICLONERANGE */
#define HAVE_FILENO
#define HAVE_FSEEKO
#define HAVE_FTELLO
#define HAVE_GETPROGNAME
/* #undef HAVE_GNUTLS */
/* #undef HAVE_LIBBZ2 */
/* #undef HAVE_LIBLZMA */
/* #undef HAVE_LIBZSTD */
#define HAVE_LOCALTIME_R
/* #undef HAVE_MBEDTLS */
/* #undef HAVE_MKSTEMP */
#define HAVE_NULLABLE
/* #undef HAVE_OPENSSL */
#define HAVE_SETMODE
#define HAVE_STRCASECMP
#define HAVE_STRDUP
/* #undef HAVE_STRICMP */
#define HAVE_STRTOLL
#define HAVE_STRTOULL
/* #undef HAVE_STRUCT_TM_TM_ZONE */
#define HAVE_STDBOOL_H
#define HAVE_STRINGS_H
#define HAVE_UNISTD_H
/* #undef HAVE_WINDOWS_CRYPTO */
#define SIZEOF_OFF_T 8
#define SIZEOF_SIZE_T 8
/* #undef HAVE_DIRENT_H */
#define HAVE_FTS_H
/* #undef HAVE_NDIR_H */
/* #undef HAVE_SYS_DIR_H */
/* #undef HAVE_SYS_NDIR_H */
/* #undef WORDS_BIGENDIAN */
/* #undef HAVE_SHARED */
/* END DEFINES */
#define PACKAGE "flycast"
#define VERSION ""

// FIXME FLAC config
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
