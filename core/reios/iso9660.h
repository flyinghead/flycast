/*
    Copyright (C) 2003-2008, 2012-2013
                  Rocky Bernstein <rocky@gnu.org>
    Copyright (C) 2000 Herbert Valerio Riedel <hvr@gnu.org>

    See also iso9660.h by Eric Youngdale (1993).

    Copyright 1993 Yggdrasil Computing, Incorporated

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CDIO_ISO9660_H_
#define CDIO_ISO9660_H_

#include <time.h>

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
#define GNUC_PACKED                             \
  __attribute__((packed))
#else   /* !__GNUC__ */
#define GNUC_PACKED
#endif /* !__GNUC__ */

#if defined(__MINGW32__)
#  define PRAGMA_BEGIN_PACKED _Pragma("pack(push)") \
                              _Pragma("pack(1)")
#  define PRAGMA_END_PACKED   _Pragma("pack(pop)")
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)
     /* should work with most EDG-frontend based compilers */
#    define PRAGMA_BEGIN_PACKED _Pragma("pack(1)")
#    define PRAGMA_END_PACKED   _Pragma("pack()")
#elif defined(_MSC_VER)
#  define PRAGMA_BEGIN_PACKED __pragma(pack(push, 1))
#  define PRAGMA_END_PACKED   __pragma(pack(pop))
#else /* neither gcc nor _Pragma() available... */
   /* ...so let's be naive and hope the regression testsuite is run... */
#  define PRAGMA_BEGIN_PACKED
#  define PRAGMA_END_PACKED
#endif


/** \brief ISO 9660 Integer and Character types

These are described in the section 7 of the ISO 9660 (or ECMA 119)
specification.
*/

typedef uint8_t  iso711_t; /*! See section 7.1.1 */
typedef int8_t   iso712_t; /*! See section 7.1.2 */
typedef uint16_t iso721_t; /*! See section 7.2.1 */
typedef uint16_t iso722_t; /*! See section 7.2.2 */
typedef uint32_t iso723_t; /*! See section 7.2.3 */
typedef uint32_t iso731_t; /*! See section 7.3.1 */
typedef uint32_t iso732_t; /*! See section 7.3.2 */
typedef uint64_t iso733_t; /*! See section 7.3.3 */

typedef char     achar_t;  /*! See section 7.4.1 */
typedef char     dchar_t;  /*! See section 7.4.1 */

#ifndef EMPTY_ARRAY_SIZE
#define EMPTY_ARRAY_SIZE 0
#endif

#ifdef ISODCL
#undef ISODCL
#endif
/* This part borrowed from the bsd386 isofs */
#define ISODCL(from, to)        ((to) - (from) + 1)

#define MIN_TRACK_SIZE 4*75
#define MIN_ISO_SIZE MIN_TRACK_SIZE

/*! The below isn't really an enumeration one would really use in a
    program; things are done this way so that in a debugger one can to
    refer to the enumeration value names such as in a debugger
    expression and get something. With the more common a \#define
    mechanism, the name/value assocation is lost at run time.
  */
extern enum iso_enum1_s {
  ISO_PVD_SECTOR      =   16, /**< Sector of Primary Volume Descriptor. */
  ISO_EVD_SECTOR      =   17, /**< Sector of End Volume Descriptor. */
  LEN_ISONAME         =   31, /**< Size in bytes of the filename
                                 portion + null byte. */
  ISO_MAX_SYSTEM_ID   =   32, /**< Maximum number of characters in a system
                                 id. */
  MAX_ISONAME         =   37, /**< Size in bytes of the filename
                                 portion + null byte. */
  ISO_MAX_PREPARER_ID =  128, /**< Maximum number of characters in a
                                 preparer id. */
  MAX_ISOPATHNAME     =  255, /**< Maximum number of characters in the
                                 entire ISO 9660 filename. */
  ISO_BLOCKSIZE       = 2048  /**< Number of bytes in an ISO 9660 block. */

} iso_enums1;

/*! An enumeration for some of the ISO_* \#defines below. This isn't
    really an enumeration one would really use in a program it is here
    to be helpful in debuggers where wants just to refer to the
    ISO_*_ names and get something.
  */

/*! ISO 9660 directory flags. */
extern enum iso_flag_enum_s {
  ISO_FILE            =   0,   /**<  Not really a flag...                */
  ISO_EXISTENCE       =   1,   /**< Do not make existence known (hidden) */
  ISO_DIRECTORY       =   2,   /**< This file is a directory             */
  ISO_ASSOCIATED      =   4,   /**< This file is an associated file      */
  ISO_RECORD          =   8,   /**< Record format in extended attr. != 0 */
  ISO_PROTECTION      =  16,   /**< No read/execute perm. in ext. attr.  */
  ISO_DRESERVED1      =  32,   /**<, Reserved bit 5                      */
  ISO_DRESERVED2      =  64,   /**<, Reserved bit 6                      */
  ISO_MULTIEXTENT     = 128,   /**< Not final entry of a mult. ext. file */
} iso_flag_enums;

/*! Volume descriptor types */
extern enum iso_vd_enum_s {
  ISO_VD_BOOT_RECORD   =  0,  /**< CD is bootable */
  ISO_VD_PRIMARY       =  1,  /**< Is in any ISO-9660 */
  ISO_VD_SUPPLEMENTARY =  2,  /**< Used by Joliet, for example */
  ISO_VD_PARITION      =  3,  /**< Indicates a partition of a CD */
  ISO_VD_END           = 255
} iso_vd_enums;


/*!
   An ISO filename is:
   <em>abcd</em>.<em>eee</em> ->
   <em>filename</em>.<em>ext</em>;<em>version#</em>

    For ISO-9660 Level 1, the maximum needed string length is:

@code
         30 chars (filename + ext)
    +     2 chars ('.' + ';')
    +     5 chars (strlen("32767"))
    +     1 null byte
   ================================
    =    38 chars
@endcode

*/

/*! \brief Maximum number of characters in a publisher id. */
#define ISO_MAX_PUBLISHER_ID 128

/*! \brief Maximum number of characters in an application id. */
#define ISO_MAX_APPLICATION_ID 128

/*! \brief Maximum number of characters in a volume id. */
#define ISO_MAX_VOLUME_ID 32

/*! \brief Maximum number of characters in a volume-set id. */
#define ISO_MAX_VOLUMESET_ID 128

/*! String inside frame which identifies an ISO 9660 filesystem. This
    string is the "id" field of an iso9660_pvd_t or an iso9660_svd_t.
*/
extern const char ISO_STANDARD_ID[sizeof("CD001")-1];

#define ISO_STANDARD_ID      "CD001"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum strncpy_pad_check {
  ISO9660_NOCHECK = 0,
  ISO9660_7BIT,
  ISO9660_ACHARS,
  ISO9660_DCHARS
} strncpy_pad_check_t;

PRAGMA_BEGIN_PACKED

/*!
  \brief ISO-9660 shorter-format time structure. See ECMA 9.1.5.

  @see iso9660_dtime
 */
struct  iso9660_dtime_s {
  iso711_t      dt_year;   /**< Number of years since 1900 */
  iso711_t      dt_month;  /**< Has value in range 1..12. Note starts
                              at 1, not 0 like a tm struct. */
  iso711_t      dt_day;    /**< Day of the month from 1 to 31 */
  iso711_t      dt_hour;   /**< Hour of the day from 0 to 23 */
  iso711_t      dt_minute; /**< Minute of the hour from 0 to 59 */
  iso711_t      dt_second; /**< Second of the minute from 0 to 59 */
  iso712_t      dt_gmtoff; /**< GMT values -48 .. + 52 in 15 minute
                              intervals */
} GNUC_PACKED;

typedef struct iso9660_dtime_s  iso9660_dtime_t;

/*!
  \brief ISO-9660 longer-format time structure.

  Section 8.4.26.1 of ECMA 119. All values are encoded as character
  arrays, eg. '1', '9', '5', '5' for the year 1955 (no null terminated
  byte).

  @see iso9660_ltime
 */
struct  iso9660_ltime_s {
  char   lt_year        [ISODCL(   1,   4)];   /**< Add 1900 to value
                                                    for the Julian
                                                    year */
  char   lt_month       [ISODCL(   5,   6)];   /**< Has value in range
                                                  1..12. Note starts
                                                  at 1, not 0 like a
                                                  tm struct. */
  char   lt_day         [ISODCL(   7,   8)];   /**< Day of month: 1..31 */
  char   lt_hour        [ISODCL(   9,   10)];  /**< hour: 0..23 */
  char   lt_minute      [ISODCL(  11,   12)];  /**< minute: 0..59 */
  char   lt_second      [ISODCL(  13,   14)];  /**< second: 0..59 */
  char   lt_hsecond     [ISODCL(  15,   16)];  /**< The value is in
                                                  units of 1/100's of
                                                  a second */
  iso712_t lt_gmtoff;  /**< Offset from Greenwich Mean Time in number
                          of 15 min intervals from -48 (West) to +52
                          (East) recorded according to 7.1.2 numerical
                          value */
} GNUC_PACKED;

typedef struct iso9660_ltime_s  iso9660_ltime_t;
typedef struct iso9660_dir_s    iso9660_dir_t;
typedef struct iso9660_stat_s   iso9660_stat_t;

/*! \brief Format of an ISO-9660 directory record

 Section 9.1 of ECMA 119.

 This structure may have an odd length depending on how many
 characters there are in the filename!  Some compilers (e.g. on
 Sun3/mc68020) pad the structures to an even length.  For this reason,
 we cannot use sizeof (struct iso_path_table) or sizeof (struct
 iso_directory_record) to compute on disk sizes.  Instead, we use
 offsetof(..., name) and add the name size.  See mkisofs.h of the
 cdrtools package.

  @see iso9660_stat
*/
struct iso9660_dir_s {
  iso711_t         length;            /*! Length of Directory record (9.1.1) */
  iso711_t         xa_length;         /*! XA length if XA is used. Otherwise
                                          zero. (9.1.2)  */
  iso733_t         extent;            /*! LBA of first local block allocated
                                          to the extent */
  iso733_t         size;              /*! data length of File Section. This
                                          does not include the length of
                                          any XA Records. (9.1.2) */
  iso9660_dtime_t  recording_time;    /*! Recording date and time (9.1.3) */
  uint8_t          file_flags;        /*! If no XA then zero. If a directory,
                                        then bits 2,3 and 7 are zero.
                                        (9.1.6) */
  iso711_t         file_unit_size;    /*! File Unit size for the File
                                        Section if the File Section
                                        is recorded in interleaved
                                        mode. Otherwise zero. (9.1.7) */
  iso711_t         interleave_gap;    /*! Interleave Gap size for the
                                        File Section if the File
                                        Section is interleaved. Otherwise
                                        zero. (9.1.8) */
  iso723_t volume_sequence_number;    /*! Ordinal number of the volume
                                          in the Volume Set on which
                                          the Extent described by this
                                          Directory Record is
                                          recorded. (9.1.9) */
/*! MSVC compilers cannot handle a zero sized array in the middle
    of a struct, and iso9660_dir_s is reused within iso9660_pvd_s.
    Therefore, instead of defining:
       iso711_t filename_len;
       char     filename[];
    we leverage the fact that iso711_t and char are the same size
    and use an union. The only gotcha is that the actual string
    payload of filename.str[] starts at 1, not 0. */
  union {
    iso711_t        len;
    char            str[1];
  } filename;
} GNUC_PACKED;

/*!
  \brief ISO-9660 Primary Volume Descriptor.
 */
struct iso9660_pvd_s {
  iso711_t         type;                         /**< ISO_VD_PRIMARY - 1 */
  char             id[5];                        /**< ISO_STANDARD_ID "CD001"
                                                  */
  iso711_t         version;                      /**< value 1 for ECMA 119 */
  char             unused1[1];                   /**< unused - value 0 */
  achar_t          system_id[ISO_MAX_SYSTEM_ID]; /**< each char is an achar */
  dchar_t          volume_id[ISO_MAX_VOLUME_ID]; /**< each char is a dchar */
  uint8_t          unused2[8];                   /**< unused - value 0 */
  iso733_t         volume_space_size;            /**< total number of
                                                    sectors */
  uint8_t          unused3[32];                  /**< unused - value 0 */
  iso723_t         volume_set_size;              /**< often 1 */
  iso723_t         volume_sequence_number;       /**< often 1 */
  iso723_t         logical_block_size;           /**< sector size, e.g. 2048 */
  iso733_t         path_table_size;              /**< bytes in path table */
  iso731_t         type_l_path_table;            /**< first sector of L Path
                                                      Table */
  iso731_t         opt_type_l_path_table;        /**< first sector of optional
                                                    L Path Table */
  iso732_t         type_m_path_table;            /**< first sector of M Path
                                                    table */
  iso732_t         opt_type_m_path_table;        /**< first sector of optional
                                                    M Path table */
  iso9660_dir_t    root_directory_record;        /**< See 8.4.18 and
                                                    section 9.1 of
                                                    ISO 9660 spec. */
  char             root_directory_filename;      /**< Is '\\0' or root
                                                  directory. Also pads previous
                                                  field to 34 bytes */
  dchar_t          volume_set_id[ISO_MAX_VOLUMESET_ID]; /**< Volume Set of
                                                           which the volume is
                                                           a member. See
                                                        section 8.4.19 */
  achar_t          publisher_id[ISO_MAX_PUBLISHER_ID];  /**< Publisher of
                                                         volume. If the first
                                                         character is '_' 0x5F,
                                                         the remaining bytes
                                                         specify a file
                                                         containing the user.
                                                         If all bytes are " "
                                                         (0x20) no publisher
                                                         is specified. See
                                                         section 8.4.20 of
                                                         ECMA 119 */
  achar_t          preparer_id[ISO_MAX_PREPARER_ID]; /**< preparer of
                                                         volume. If the first
                                                         character is '_' 0x5F,
                                                         the remaining bytes
                                                         specify a file
                                                         containing the user.
                                                         If all bytes are " "
                                                         (0x20) no preparer
                                                         is specified.
                                                         See section 8.4.21
                                                         of ECMA 119 */
  achar_t          application_id[ISO_MAX_APPLICATION_ID]; /**< application
                                                         use to create the
                                                         volume. If the first
                                                         character is '_' 0x5F,
                                                         the remaining bytes
                                                         specify a file
                                                         containing the user.
                                                         If all bytes are " "
                                                         (0x20) no application
                                                         is specified.
                                                         See section of 8.4.22
                                                         of ECMA 119 */
  dchar_t          copyright_file_id[37];     /**< Name of file for
                                                 copyright info. If
                                                 all bytes are " "
                                                 (0x20), then no file
                                                 is identified.  See
                                                 section 8.4.23 of ECMA 119
                                                 9660 spec. */
  dchar_t          abstract_file_id[37];      /**< See section 8.4.24 of
                                                 ECMA 119. */
  dchar_t          bibliographic_file_id[37]; /**< See section 7.5 of
                                                 ISO 9660 spec. */
  iso9660_ltime_t  creation_date;             /**< date and time of volume
                                                 creation. See section 8.4.26.1
                                                 of the ISO 9660 spec. */
  iso9660_ltime_t  modification_date;         /**< date and time of the most
                                                 recent modification.
                                                 See section 8.4.27 of the
                                                 ISO 9660 spec. */
  iso9660_ltime_t  expiration_date;           /**< date and time when volume
                                                 expires. See section 8.4.28
                                                 of the ISO 9660 spec. */
  iso9660_ltime_t  effective_date;            /**< date and time when volume
                                                 is effective. See section
                                                 8.4.29 of the ISO 9660
                                                 spec. */
  iso711_t         file_structure_version;    /**< value 1 for ECMA 119 */
  uint8_t           unused4[1];                /**< unused - value 0 */
  char             application_data[512];     /**< Application can put
                                                 whatever it wants here. */
  uint8_t          unused5[653];              /**< Unused - value 0 */
} GNUC_PACKED;

typedef struct iso9660_pvd_s  iso9660_pvd_t;

/*!
  \brief ISO-9660 Supplementary Volume Descriptor.

  This is used for Joliet Extentions and is almost the same as the
  the primary descriptor but two unused fields, "unused1" and "unused3
  become "flags and "escape_sequences" respectively.
*/
struct iso9660_svd_s {
  iso711_t         type;                         /**< ISO_VD_SUPPLEMENTARY - 2
                                                  */
  char             id[5];                        /**< ISO_STANDARD_ID "CD001"
                                                  */
  iso711_t         version;                      /**< value 1 */
  char             flags;                        /**< Section 8.5.3 */
  achar_t          system_id[ISO_MAX_SYSTEM_ID]; /**< Section 8.5.4; each char
                                                    is an achar */
  dchar_t          volume_id[ISO_MAX_VOLUME_ID]; /**< Section 8.5.5; each char
                                                    is a dchar */
  char             unused2[8];
  iso733_t         volume_space_size;            /**< total number of
                                                    sectors */
  char             escape_sequences[32];         /**< Section 8.5.6 */
  iso723_t         volume_set_size;              /**< often 1 */
  iso723_t         volume_sequence_number;       /**< often 1 */
  iso723_t         logical_block_size;           /**< sector size, e.g. 2048 */
  iso733_t         path_table_size;              /**< 8.5.7; bytes in path
                                                    table */
  iso731_t         type_l_path_table;            /**< 8.5.8; first sector of
                                                    little-endian path table */
  iso731_t         opt_type_l_path_table;        /**< 8.5.9; first sector of
                                                    optional little-endian
                                                    path table */
  iso732_t         type_m_path_table;            /**< 8.5.10; first sector of
                                                    big-endian path table */
  iso732_t         opt_type_m_path_table;        /**< 8.5.11; first sector of
                                                    optional big-endian path
                                                    table */
  iso9660_dir_t    root_directory_record;        /**< See section 8.5.12 and
                                                    9.1 of ISO 9660 spec. */
  char             root_directory_filename;      /**< Is '\\0' or root
                                                  directory. Also pads previous
                                                  field to 34 bytes */
  dchar_t          volume_set_id[ISO_MAX_VOLUMESET_ID];    /**< 8.5.13;
                                                              dchars */
  achar_t          publisher_id[ISO_MAX_PUBLISHER_ID]; /**<
                                                          Publisher of volume.
                                                          If the first char-
                                                          aracter is '_' 0x5F,
                                                          the remaining bytes
                                                          specify a file
                                                          containing the user.
                                                          If all bytes are " "
                                                          (0x20) no publisher
                                                          is specified. See
                                                          section 8.5.14 of
                                                          ECMA 119 */
  achar_t          preparer_id[ISO_MAX_PREPARER_ID]; /**<
                                                        Data preparer of
                                                        volume. If the first
                                                        character is '_' 0x5F,
                                                        the remaining bytes
                                                        specify a file
                                                        containing the user.
                                                        If all bytes are " "
                                                        (0x20) no preparer
                                                        is specified.
                                                        See section 8.5.15
                                                        of ECMA 119 */
  achar_t          application_id[ISO_MAX_APPLICATION_ID]; /**< application
                                                         use to create the
                                                         volume. If the first
                                                         character is '_' 0x5F,
                                                         the remaining bytes
                                                         specify a file
                                                         containing the user.
                                                         If all bytes are " "
                                                         (0x20) no application
                                                         is specified.
                                                         See section of 8.5.16
                                                         of ECMA 119 */
  dchar_t          copyright_file_id[37];     /**< Name of file for
                                                 copyright info. If
                                                 all bytes are " "
                                                 (0x20), then no file
                                                 is identified.  See
                                                 section 8.5.17 of ECMA 119
                                                 9660 spec. */
  dchar_t          abstract_file_id[37];      /**< See section 8.5.18 of
                                                 ECMA 119. */
  dchar_t          bibliographic_file_id[37]; /**< See section 8.5.19 of
                                                 ECMA 119. */
  iso9660_ltime_t  creation_date;             /**< date and time of volume
                                                 creation. See section 8.4.26.1
                                                 of the ECMA 119 spec. */
  iso9660_ltime_t  modification_date;         /**< date and time of the most
                                                 recent modification.
                                                 See section 8.4.27 of the
                                                 ECMA 119 spec. */
  iso9660_ltime_t  expiration_date;           /**< date and time when volume
                                                 expires. See section 8.4.28
                                                 of the ECMA 119 spec. */
  iso9660_ltime_t  effective_date;            /**< date and time when volume
                                                 is effective. See section
                                                 8.4.29 of the ECMA 119
                                                 spec. */
  iso711_t         file_structure_version;    /**< value 1 for ECMA 119 */
  uint8_t           unused4[1];                /**< unused - value 0 */
  char             application_data[512];     /**< 8.5.20 Application can put
                                                 whatever it wants here. */
  uint8_t          unused5[653];              /**< Unused - value 0 */
} GNUC_PACKED;

typedef struct iso9660_svd_s  iso9660_svd_t;

PRAGMA_END_PACKED

/** A mask used in iso9660_ifs_read_vd which allows what kinds
    of extensions we allow, eg. Joliet, Rock Ridge, etc. */
typedef uint8_t iso_extension_mask_t;

/*! An enumeration for some of the ISO_EXTENSION_* \#defines below. This isn't
    really an enumeration one would really use in a program it is here
    to be helpful in debuggers where wants just to refer to the
    ISO_EXTENSION_*_ names and get something.
  */
extern enum iso_extension_enum_s {
  ISO_EXTENSION_JOLIET_LEVEL1 = 0x01,
  ISO_EXTENSION_JOLIET_LEVEL2 = 0x02,
  ISO_EXTENSION_JOLIET_LEVEL3 = 0x04,
  ISO_EXTENSION_ROCK_RIDGE    = 0x08,
  ISO_EXTENSION_HIGH_SIERRA   = 0x10
} iso_extension_enums;


#define ISO_EXTENSION_ALL           0xFF
#define ISO_EXTENSION_NONE          0x00
#define ISO_EXTENSION_JOLIET     \
  (ISO_EXTENSION_JOLIET_LEVEL1 | \
   ISO_EXTENSION_JOLIET_LEVEL2 | \
   ISO_EXTENSION_JOLIET_LEVEL3 )


/** This is an opaque structure. */
typedef struct _iso9660_s iso9660_t;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#undef ISODCL
#endif /* CDIO_ISO9660_H_ */

/*
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
