/*********************************************************************
   PicoTCP. Copyright (c) 2014-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

   Authors: Maxime Vincent
            Based on the OpenVPN tun.c driver, under GPL

   NOTES: This is the Windows-only driver, a Linux-equivalent is available, too
          You need to have an OpenVPN TUN/TAP network adapter installed, first
          This driver is barely working:
 * Only TAP-mode is supported (TUN is not)
 * it will simply open the first TAP device it can find
 * there is memory being allocated that's never freed
 * there is no destroy function, yet
 * it has only been tested on a Windows 7 machine
 *********************************************************************/

#ifndef __PICO_DEV_TAP_WINDOWS_PRIVATE_H
#define __PICO_DEV_TAP_WINDOWS_PRIVATE_H

/* Extra defines (vnz) */
#define TAP_WIN_COMPONENT_ID  "tap0901"
#define TAP_WIN_MIN_MAJOR     9
#define TAP_WIN_MIN_MINOR     9
#define PACKAGE_NAME          "PicoTCP WinTAP"

/* Extra structs */
struct tap_reg
{
    const char *guid;
    struct tap_reg *next;
};

struct panel_reg
{
    const char *name;
    const char *guid;
    struct panel_reg *next;
};


/*
 * =============
 * TAP IOCTLs
 * =============
 */

#define TAP_WIN_CONTROL_CODE(request, method) \
    CTL_CODE (FILE_DEVICE_UNKNOWN, request, method, FILE_ANY_ACCESS)

/* Present in 8.1 */

#define TAP_WIN_IOCTL_GET_MAC               TAP_WIN_CONTROL_CODE (1, METHOD_BUFFERED)
#define TAP_WIN_IOCTL_GET_VERSION           TAP_WIN_CONTROL_CODE (2, METHOD_BUFFERED)
#define TAP_WIN_IOCTL_GET_MTU               TAP_WIN_CONTROL_CODE (3, METHOD_BUFFERED)
#define TAP_WIN_IOCTL_GET_INFO              TAP_WIN_CONTROL_CODE (4, METHOD_BUFFERED)
#define TAP_WIN_IOCTL_CONFIG_POINT_TO_POINT TAP_WIN_CONTROL_CODE (5, METHOD_BUFFERED)
#define TAP_WIN_IOCTL_SET_MEDIA_STATUS      TAP_WIN_CONTROL_CODE (6, METHOD_BUFFERED)
#define TAP_WIN_IOCTL_CONFIG_DHCP_MASQ      TAP_WIN_CONTROL_CODE (7, METHOD_BUFFERED)
#define TAP_WIN_IOCTL_GET_LOG_LINE          TAP_WIN_CONTROL_CODE (8, METHOD_BUFFERED)
#define TAP_WIN_IOCTL_CONFIG_DHCP_SET_OPT   TAP_WIN_CONTROL_CODE (9, METHOD_BUFFERED)

/* Added in 8.2 */

/* obsoletes TAP_WIN_IOCTL_CONFIG_POINT_TO_POINT */
#define TAP_WIN_IOCTL_CONFIG_TUN            TAP_WIN_CONTROL_CODE (10, METHOD_BUFFERED)

/*
 * =================
 * Registry keys
 * =================
 */

#define ADAPTER_KEY "SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}"

#define NETWORK_CONNECTIONS_KEY "SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}"

/*
 * ======================
 * Filesystem prefixes
 * ======================
 */

#define USERMODEDEVICEDIR "\\\\.\\Global\\"
#define SYSDEVICEDIR      "\\Device\\"
#define USERDEVICEDIR     "\\DosDevices\\Global\\"
#define TAP_WIN_SUFFIX    ".tap"

#endif
