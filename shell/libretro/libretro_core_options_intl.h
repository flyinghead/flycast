/*
    This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef LIBRETRO_CORE_OPTIONS_INTL_H__
#define LIBRETRO_CORE_OPTIONS_INTL_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#pragma warning(disable:4566)
#endif

#include <libretro.h>

#include "libretro_core_option_defines.h"

/*
 ********************************
 * VERSION: 1.3
 ********************************
 *
 * - 1.3: Move translations to libretro_core_options_intl.h
 *        - libretro_core_options_intl.h includes BOM and utf-8
 *          fix for MSVC 2010-2013
 *        - Added HAVE_NO_LANGEXTRA flag to disable translations
 *          on platforms/compilers without BOM support
 * - 1.2: Use core options v1 interface when
 *        RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION is >= 1
 *        (previously required RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION == 1)
 * - 1.1: Support generation of core options v0 retro_core_option_value
 *        arrays containing options with a single value
 * - 1.0: First commit
*/

#ifdef __cplusplus
extern "C" {
#endif

/*
 ********************************
 * Core Option Definitions
 ********************************
*/

/* RETRO_LANGUAGE_JAPANESE */

/* RETRO_LANGUAGE_FRENCH */

/* RETRO_LANGUAGE_SPANISH */

/* RETRO_LANGUAGE_GERMAN */

/* RETRO_LANGUAGE_ITALIAN */

/* RETRO_LANGUAGE_DUTCH */

/* RETRO_LANGUAGE_PORTUGUESE_BRAZIL */

/* RETRO_LANGUAGE_PORTUGUESE_PORTUGAL */

/* RETRO_LANGUAGE_RUSSIAN */

/* RETRO_LANGUAGE_KOREAN */

/* RETRO_LANGUAGE_CHINESE_TRADITIONAL */

/* RETRO_LANGUAGE_CHINESE_SIMPLIFIED */

/* RETRO_LANGUAGE_ESPERANTO */

/* RETRO_LANGUAGE_POLISH */

/* RETRO_LANGUAGE_VIETNAMESE */

/* RETRO_LANGUAGE_ARABIC */

/* RETRO_LANGUAGE_GREEK */

/* RETRO_LANGUAGE_TURKISH */

#define COLORS_STRING_TR \
      { "BLACK 02",          "Siyah" }, \
      { "BLUE 03",           "Mavi" }, \
      { "LIGHT_BLUE 04",     "Açık Mavi" }, \
      { "GREEN 05",          "Yeşil" }, \
      { "CYAN 06",           "Camgöbeği" }, \
      { "CYAN_BLUE 07",      "Camgöbeği Mavi" }, \
      { "LIGHT_GREEN 08",    "Açık Yeşil" }, \
      { "CYAN_GREEN 09",     "Camgöbeği Yeşil" }, \
      { "LIGHT_CYAN 10",     "Açık Camgöbeği" }, \
      { "RED 11",            "Kırmızı" }, \
      { "PURPLE 12",         "Mor" }, \
      { "LIGHT_PURPLE 13",   "Açık Mor" }, \
      { "YELLOW 14",         "Sarı" }, \
      { "GRAY 15",           "Gri" }, \
      { "LIGHT_PURPLE_2 16", "Açık Mor (2)" }, \
      { "LIGHT_GREEN_2 17",  "Açık Yeşil (2)" }, \
      { "LIGHT_GREEN_3 18",  "Açık Yeşil (3)" }, \
      { "LIGHT_CYAN_2 19",   "Açık Cyan (2)" }, \
      { "LIGHT_RED_2 20",    "Açık Kırmızı (2)" }, \
      { "MAGENTA 21",        "Eflatun" }, \
      { "LIGHT_PURPLE_2 22", "Açık Mor (2)" }, \
      { "LIGHT_ORANGE 23",   "Açık Turuncu" }, \
      { "ORANGE 24",         "Turuncu" }, \
      { "LIGHT_PURPLE_3 25", "Açık Mor (3)" }, \
      { "LIGHT_YELLOW 26",   "Açık Sarı" }, \
      { "LIGHT_YELLOW_2 27", "Açık Sarı (2)" }, \
      { "WHITE 28",          "Beyaz" }, \
      { NULL, NULL },

#define VMU_SCREEN_PARAMS_TR(num) \
{ \
   CORE_OPTION_NAME "_vmu" #num "_screen_display", \
   "VMU Screen " #num " Görsel", \
   "", \
   { \
      { NULL, NULL }, \
   }, \
   NULL, \
}, \
{ \
   CORE_OPTION_NAME "_vmu" #num "_screen_position", \
   "VMU Screen " #num " Pozisyon", \
   "", \
   { \
      { "Upper Left",  "Sol Üst" }, \
      { "Upper Right", "Sağ Üst" }, \
      { "Lower Left",  "Sol Alt" }, \
      { "Lower Right", "Sağ Alt" }, \
      { NULL, NULL }, \
   }, \
   NULL, \
}, \
{ \
   CORE_OPTION_NAME "_vmu" #num "_screen_size_mult", \
   "VMU Screen " #num " Boyut", \
   "", \
   { \
      { NULL, NULL }, \
   }, \
   NULL, \
}, \
{ \
   CORE_OPTION_NAME "_vmu" #num "_pixel_on_color", \
   "VMU Screen " #num " Piksel Varken Renk", \
   "", \
   { \
      { "DEFAULT_ON 00",  "Varsayılan AÇIK" }, \
      { "DEFAULT_OFF 01", "Varsayılan KAPALI" }, \
      COLORS_STRING_TR \
   }, \
   NULL, \
}, \
{ \
   CORE_OPTION_NAME "_vmu" #num "_pixel_off_color", \
   "VMU Screen " #num " Piksel Yokken Renk", \
   "", \
   { \
      { "DEFAULT_OFF 01", "Varsayılan KAPALI" }, \
      { "DEFAULT_ON 00",  "Varsayılan AÇIK" }, \
      COLORS_STRING_TR \
   }, \
   NULL, \
}, \
{ \
   CORE_OPTION_NAME "_vmu" #num "_screen_opacity", \
   "VMU Screen " #num " Opaklık", \
   "", \
   { \
      { NULL,   NULL }, \
   }, \
   NULL, \
},

#define LIGHTGUN_PARAMS_TR(num) \
{ \
   CORE_OPTION_NAME "_lightgun" #num "_crosshair", \
   "Gun Crosshair " #num " Görsel", \
   "", \
   { \
      { "disabled", NULL }, \
      { "White",    NULL }, \
      { "Red",      NULL }, \
      { "Green",    NULL }, \
      { "Blue",     NULL }, \
      { NULL,       NULL }, \
   }, \
   NULL, \
},

struct retro_core_option_definition option_defs_tr[] = {
   {
      CORE_OPTION_NAME "_boot_to_bios",
      "BIOS'a önyükleme (Yeniden Başlatma Gerektirir)",
      "Doğrudan Dreamcast BIOS menüsüne önyükleme yapın.",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_hle_bios",
      "HLE BIOS",
      "Üst düzey öykünmüş BIOS(HLE) kullanımını zorla.",
      {
         { NULL, NULL},
      },
      NULL,
   },
#ifdef HAVE_OIT
   {
      CORE_OPTION_NAME "_oit_abuffer_size",
      "Birikim Piksel Arabellek Boyutu (Yeniden Başlatma Gerektirir)",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
#endif
   {
      CORE_OPTION_NAME "_internal_resolution",
      "Dahili Çözünürlük",
      "Render çözünürlüğünü değiştirin.",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_screen_rotation",
      "Ekran Yönü",
      "",
      {
         { "horizontal", "Yatay" },
         { "vertical",   "Dikey" },
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_alpha_sorting",
      "Alfa Sıralama",
      "",
      {
         { "per-strip (fast, least accurate)", "Şerit Başına (hızlı, en az doğru)" },
         { "per-triangle (normal)",            "Üçgen Başına (normal)" },
#if defined(HAVE_OIT) || defined(HAVE_VULKAN)
         { "per-pixel (accurate)",             "Piksel Başına (doğru, ancak en yavaş)" },
#endif
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_gdrom_fast_loading",
      "GDROM Hızlı Yükleme (kusurlu)",
      "GD-ROM yüklemesini hızlandırır.",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_mipmapping",
      "Mipmapping",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_fog",
      "Fog Effects",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_volume_modifier_enable",
      "Hacim Değiştirici",
      "Nesne gölgeleri çizmek için genellikle oyunlar tarafından kullanılan bir Dreamcast GPU özelliği. Bu normalde etkinleştirilmelidir - performansın etkisi ihmal edilebilir düzeyde genellikle minimum düzeydedir.",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_widescreen_hack",
      "Geniş ekran kesmesi",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_widescreen_cheats",
      "Widescreen Cheats (Restart)",
      "Activates cheats that allow certain games to display in widescreen format.",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_cable_type",
      "Kablo Tipi",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_broadcast",
      "Yayın",
      "",
      {
         { "NTSC",    NULL },
         { "PAL",     "PAL (World)" },
         { "PAL_N",   "PAL-N (Argentina, Paraguay, Uruguay)" },
         { "PAL_M",   "PAL-M (Brazil)" },
         { "Default", "Varsayılan" },
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_region",
      "Bölge",
      "",
      {
         { "Japan",   NULL },
         { "USA",     NULL },
         { "Europe",  NULL },
         { "Default", "Varsayılan" },
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_language",
      "Dil",
      "",
      {
         { "Japanese", NULL },
         { "English",  NULL },
         { "German",   NULL },
         { "French",   NULL },
         { "Spanish",  NULL },
         { "Italian",  NULL },
         { "Default",  "Varsayılan" },
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_force_wince",
      "Force Windows CE Mode",
      "Enable full MMU emulation and other settings for Windows CE games",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_analog_stick_deadzone",
      "Analog Çubuğu Ölü Bölge",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_trigger_deadzone",
      "Tetik Ölü Bölge",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_digital_triggers",
      "Dijital Tetikleyiciler",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_enable_dsp",
      "DSP'yi Etkinleştir",
      "Dreamcast'in ses DSP'sinin (dijital sinyal işlemcisi) öykünmesini etkinleştirin. Üretilen sesin doğruluğunu arttırır, ancak performans gereksinimlerini artırır.",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_anisotropic_filtering",
      "Anisotropic Filtering",
      "Enhance the quality of textures on surfaces that are at oblique viewing angles with respect to the camera.",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_pvr2_filtering",
      "PowerVR2 Post-processing Filter",
      "Post-process the rendered image to simulate effects specific to the PowerVR2 GPU and analog video signals.",
      {
         { NULL, NULL },
      },
      NULL,
   },
#ifdef HAVE_TEXUPSCALE
   {
      CORE_OPTION_NAME "_texupscale",
      "Doku Büyütme (xBRZ)",
      "",
      {
         { "off", "Devre Dışı" },
         { "2x",  NULL },
         { "4x",  NULL },
         { "6x",  NULL },
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_texupscale_max_filtered_texture_size",
      "Doku Yükseltme Maks. Filtre boyutu",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
#endif
   {
      CORE_OPTION_NAME "_enable_rttb",
      "RTT'yi etkinleştirme (Dokuya Render'i) ara belleği",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_render_to_texture_upscaling",
      "Doku Yükseltme İşlemine Render",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_threaded_rendering",
      "İşlem Parçacığı Renderlama (Yeniden Başlatma Gerektirir)",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_delay_frame_swapping",
      "Delay Frame Swapping",
      "Useful to avoid flashing screens or glitchy videos. Not recommended on slow platforms.",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_auto_skip_frame",
      "Auto Skip Frame",
      "Automatically skip frames when the emulator is running slow. Note: This setting only applies when 'Threaded Rendering' is enabled.",
      {
         { NULL, NULL },
      },
      NULL
   },
   {
      CORE_OPTION_NAME "_frame_skipping",
      "Kare Atlama",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_enable_purupuru",
      "Purupuru Paketi / Titreşim Paketi",
      "Denetleyici geri bildirimini etkinleştirir.",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_allow_service_buttons",
      "Allow NAOMI Service Buttons",
      "Kabin ayarlarına girmek için NAOMI'nin SERVİS düğmesini etkinleştirir.",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_custom_textures",
      "Özel Dokular Yükle",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_dump_textures",
      "Dokuları Göm",
      "",
      {
         { NULL, NULL },
      },
      NULL,
   },
   {
      CORE_OPTION_NAME "_per_content_vmus",
      "Oyun Başına VMU'lar",
      "Devre dışı bırakıldığında, tüm oyunlar RetroArch'ın sistem dizininde bulunan 4 VMU kaydetme dosyasını (A1, B1, C1, D1) paylaşır. 'VMU A1' ayarı, RetroArch'ın başlattığı her oyun için kaydetme dizininde benzersiz bir VMU 'A1' dosyası oluşturur. 'Tüm VMU'lar' ayarı, başlatılan her oyun için 4 benzersiz VMU dosyası (A1, B1, C1, D1) oluşturur.",
      {
         { "disabled", "Devre Dışı" },
         { "VMU A1",   NULL },
         { "All VMUs", "Tüm VMU'lar" },
         { NULL, NULL},
      },
      NULL,
   },
   VMU_SCREEN_PARAMS_TR(1)
   VMU_SCREEN_PARAMS_TR(2)
   VMU_SCREEN_PARAMS_TR(3)
   VMU_SCREEN_PARAMS_TR(4)
   LIGHTGUN_PARAMS_TR(1)
   LIGHTGUN_PARAMS_TR(2)
   LIGHTGUN_PARAMS_TR(3)
   LIGHTGUN_PARAMS_TR(4)
   { NULL, NULL, NULL, {{0}}, NULL },
};

#ifdef __cplusplus
}
#endif

#endif
