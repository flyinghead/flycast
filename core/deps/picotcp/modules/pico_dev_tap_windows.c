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

#include "pico_device.h"
#include "pico_dev_null.h"
#include "pico_stack.h"
#include "pico_dev_tap_windows.h"

#include <Windows.h>
#include <Winreg.h>
#include <winioctl.h>
#include "pico_dev_tap_windows_private.h"

/*
 * Debugging info
 */
#ifdef DEBUG_TAP_GENERAL
#define dbg_tap                dbg    /* first level debug */
#else
#define dbg_tap(...)           do{} while(0)
#endif

#ifdef DEBUG_TAP_INFO
#define dbg_tap_info           dbg    /* tap info messages */
#else
#define dbg_tap_info(...)      do{} while(0)
#endif

#ifdef DEBUG_TAP_WIN
#define dbg_tap_win32          dbg    /* second level detailed win32 debug */
#else
#define dbg_tap_win32(...)     do{} while(0)
#endif

#ifdef DEBUG_TAP_REG
#define dbg_tap_reg            dbg    /* third level: registry debug */
#else
#define dbg_tap_reg(...)       do{} while(0)
#endif

/*
 * Tunnel types
 */
#define DEV_TYPE_UNDEF 0
#define DEV_TYPE_NULL  1
#define DEV_TYPE_TUN   2    /* point-to-point IP tunnel */
#define DEV_TYPE_TAP   3    /* ethernet (802.3) tunnel */


/*
 * We try to do all Win32 I/O using overlapped
 * (i.e. asynchronous) I/O for a performance win.
 */
struct overlapped_io {
# define IOSTATE_INITIAL          0
# define IOSTATE_QUEUED           1 /* overlapped I/O has been queued */
# define IOSTATE_IMMEDIATE_RETURN 2 /* I/O function returned immediately without queueing */
    int iostate;
    OVERLAPPED overlapped;
    DWORD size;
    DWORD flags;
    int status;
    int addr_defined;
    uint8_t *buf_init;
    uint32_t buf_init_len;
    uint8_t *buf;
    uint32_t buf_len;
};

struct rw_handle {
    HANDLE read;
    HANDLE write;
};

struct tuntap
{
    int type; /* DEV_TYPE_x as defined in proto.h */
    int ipv6;
    int persistent_if;      /* if existed before, keep on program end */
    char *actual_name; /* actual name of TUN/TAP dev, usually including unit number */
    int post_open_mtu;
    uint8_t mac[6];

    /* Windows stuff */
    DWORD adapter_index; /*adapter index for TAP-Windows adapter, ~0 if undefined */
    HANDLE hand;
    struct overlapped_io reads; /* for overlapped IO */
    struct overlapped_io writes;
    struct rw_handle rw_handle;

};


struct pico_device_tap {
    struct pico_device dev;
    int statistics_frames_out;
    struct tuntap *tt;
};


/*
 * Private function prototypes
 */
const struct tap_reg *get_tap_reg (void);
const struct panel_reg *get_panel_reg (void);


/*
 * Private functions
 */

/* Get TAP info from Windows registry */
const struct tap_reg *get_tap_reg (void)
{
    HKEY adapter_key;
    LONG status;
    DWORD len;
    struct tap_reg *first = NULL;
    struct tap_reg *last = NULL;
    int i = 0;

    status = RegOpenKeyEx(
        HKEY_LOCAL_MACHINE,
        ADAPTER_KEY,
        0,
        KEY_READ,
        &adapter_key);

    if (status != ERROR_SUCCESS)
    {
        dbg_tap_reg("Error opening registry key: %s\n", ADAPTER_KEY);
        return NULL;
    }

    while (1)
    {
        char enum_name[256];
        char unit_string[256];
        HKEY unit_key;
        char component_id_string[] = "ComponentId";
        char component_id[256];
        char net_cfg_instance_id_string[] = "NetCfgInstanceId";
        char net_cfg_instance_id[256];
        DWORD data_type;

        len = sizeof (enum_name);
        status = RegEnumKeyEx(
            adapter_key,
            i,
            enum_name,
            &len,
            NULL,
            NULL,
            NULL,
            NULL);
        if (status == ERROR_NO_MORE_ITEMS)
            break;
        else if (status != ERROR_SUCCESS)
            dbg_tap_reg("Error enumerating registry subkeys of key: %s.\n", ADAPTER_KEY);

        snprintf (unit_string, sizeof(unit_string), "%s\\%s",
                  ADAPTER_KEY, enum_name);

        status = RegOpenKeyEx(
            HKEY_LOCAL_MACHINE,
            unit_string,
            0,
            KEY_READ,
            &unit_key);

        if (status != ERROR_SUCCESS)
        {
            dbg_tap_reg("Error opening registry key: %s\n", unit_string);
        }
        else
        {
            len = sizeof (component_id);
            status = RegQueryValueEx(
                unit_key,
                component_id_string,
                NULL,
                &data_type,
                (LPBYTE)component_id,
                &len);

            if (status != ERROR_SUCCESS || data_type != REG_SZ)
            {
                dbg_tap_reg("Error opening registry key: %s\\%s\n", unit_string, component_id_string);
            }
            else
            {
                len = sizeof (net_cfg_instance_id);
                status = RegQueryValueEx(
                    unit_key,
                    net_cfg_instance_id_string,
                    NULL,
                    &data_type,
                    (LPBYTE)net_cfg_instance_id,
                    &len);

                if (status == ERROR_SUCCESS && data_type == REG_SZ)
                {
                    if (!strcmp (component_id, TAP_WIN_COMPONENT_ID))
                    {
                        struct tap_reg *reg;
                        reg = PICO_ZALLOC(sizeof(struct tap_reg), 1);
                        /* ALLOC_OBJ_CLEAR_GC (reg, struct tap_reg, gc); */
                        if (!reg)
                            return NULL;

                        /* reg->guid = string_alloc (net_cfg_instance_id, gc); */
                        reg->guid = PICO_ZALLOC (strlen(net_cfg_instance_id) + 1, 1);
                        if (!(reg->guid))
                        {
                            PICO_FREE(reg);
                            return NULL;
                        }

                        strcpy((char *)reg->guid, net_cfg_instance_id);
                        /* link into return list */
                        if (!first)
                            first = reg;

                        if (last)
                            last->next = reg;

                        last = reg;
                    }
                }
            }

            RegCloseKey (unit_key);
        }

        ++i;
    }
    RegCloseKey (adapter_key);
    return first;
}

/* Get Panel info from Windows registry */
const struct panel_reg *get_panel_reg (void)
{
    LONG status;
    HKEY network_connections_key;
    DWORD len;
    struct panel_reg *first = NULL;
    struct panel_reg *last = NULL;
    int i = 0;

    status = RegOpenKeyEx(
        HKEY_LOCAL_MACHINE,
        NETWORK_CONNECTIONS_KEY,
        0,
        KEY_READ,
        &network_connections_key);

    if (status != ERROR_SUCCESS)
    {
        dbg_tap_reg("Error opening registry key: %s\n", NETWORK_CONNECTIONS_KEY);
        return NULL;
    }

    while (1)
    {
        char enum_name[256];
        char connection_string[256];
        HKEY connection_key;
        WCHAR name_data[256];
        DWORD name_type;
        const WCHAR name_string[] = L"Name";

        len = sizeof (enum_name);
        status = RegEnumKeyEx(
            network_connections_key,
            i,
            enum_name,
            &len,
            NULL,
            NULL,
            NULL,
            NULL);
        if (status == ERROR_NO_MORE_ITEMS)
            break;
        else if (status != ERROR_SUCCESS)
            dbg_tap_reg("Error enumerating registry subkeys of key: %s.\n", NETWORK_CONNECTIONS_KEY);

        snprintf (connection_string, sizeof(connection_string), "%s\\%s\\Connection", NETWORK_CONNECTIONS_KEY, enum_name);

        status = RegOpenKeyEx(
            HKEY_LOCAL_MACHINE,
            connection_string,
            0,
            KEY_READ,
            &connection_key);
        if (status != ERROR_SUCCESS)
            dbg_tap_reg("Error opening registry key: %s\n", connection_string);
        else
        {
            len = sizeof (name_data);
            status = RegQueryValueExW(
                connection_key,
                name_string,
                NULL,
                &name_type,
                (LPBYTE) name_data,
                &len);

            if (status != ERROR_SUCCESS || name_type != REG_SZ)
                dbg_tap_reg("Error opening registry key: %s\\%s\\%S\n", NETWORK_CONNECTIONS_KEY, connection_string, name_string);
            else
            {
                int n;
                LPSTR name;
                struct panel_reg *reg;

                /* ALLOC_OBJ_CLEAR_GC (reg, struct panel_reg, gc); */
                reg = PICO_ZALLOC(sizeof(struct panel_reg), 1);
                if (!reg)
                    return NULL;

                n = WideCharToMultiByte (CP_UTF8, 0, name_data, -1, NULL, 0, NULL, NULL);
                /* name = gc_malloc (n, false, gc); */
                name = PICO_ZALLOC(n, 1);
                if (!name)
                {
                    PICO_FREE(reg);
                    return NULL;
                }

                WideCharToMultiByte (CP_UTF8, 0, name_data, -1, name, n, NULL, NULL);
                reg->name = name;
                /* reg->guid = string_alloc (enum_name, gc); */
                reg->guid = PICO_ZALLOC(strlen(enum_name) + 1, 1);
                if (!reg->guid)
                {
                    PICO_FREE((void *)reg->name);
                    PICO_FREE((void *)reg);
                    return NULL;
                }

                strcpy((char *)reg->guid, enum_name);

                /* link into return list */
                if (!first)
                    first = reg;

                if (last)
                    last->next = reg;

                last = reg;
            }

            RegCloseKey (connection_key);
        }

        ++i;
    }
    RegCloseKey (network_connections_key);

    return first;
}


void show_tap_win_adapters (void)
{
    int warn_panel_null = 0;
    int warn_panel_dup = 0;
    int warn_tap_dup = 0;

    int links;

    const struct tap_reg *tr;
    const struct tap_reg *tr1;
    const struct panel_reg *pr;

    const struct tap_reg *tap_reg = get_tap_reg ();
    const struct panel_reg *panel_reg = get_panel_reg ();

    if (!(tap_reg && panel_reg))
        return;

    dbg_tap_info("Available TAP-WIN32 adapters [name, GUID]:\n");

    /* loop through each TAP-Windows adapter registry entry */
    for (tr = tap_reg; tr != NULL; tr = tr->next)
    {
        links = 0;

        /* loop through each network connections entry in the control panel */
        for (pr = panel_reg; pr != NULL; pr = pr->next)
        {
            if (!strcmp (tr->guid, pr->guid))
            {
                dbg_tap_info("\t>> '%s' %s\n", pr->name, tr->guid);
                ++links;
            }
        }
        if (links > 1)
        {
            warn_panel_dup = 1;
        }
        else if (links == 0)
        {
            /* a TAP adapter exists without a link from the network
               connections control panel */
            warn_panel_null = 1;
            dbg_tap_info("\t>> [NULL] %s\n", tr->guid);
        }
    }
    /* check for TAP-Windows adapter duplicated GUIDs */
    for (tr = tap_reg; tr != NULL; tr = tr->next)
    {
        for (tr1 = tap_reg; tr1 != NULL; tr1 = tr1->next)
        {
            if (tr != tr1 && !strcmp (tr->guid, tr1->guid))
                warn_tap_dup = 1;
        }
    }
    /* warn on registry inconsistencies */
    if (warn_tap_dup)
        dbg_tap_info("WARNING: Some TAP-Windows adapters have duplicate GUIDs\n");

    if (warn_panel_dup)
        dbg_tap_info("WARNING: Some TAP-Windows adapters have duplicate links from the Network Connections control panel\n");

    if (warn_panel_null)
        dbg_tap_info("WARNING: Some TAP-Windows adapters have no link from the Network Connections control panel\n");
}


/* Get the GUID of the first TAP device found */
const char *get_first_device_guid(const struct tap_reg *tap_reg, const struct panel_reg *panel_reg, char *name)
{
    const struct tap_reg *tr;
    const struct panel_reg *pr;
    /* loop through each TAP-Windows adapter registry entry */
    for (tr = tap_reg; tr != NULL; tr = tr->next)
    {
        /* loop through each network connections entry in the control panel */
        for (pr = panel_reg; pr != NULL; pr = pr->next)
        {
            if (!strcmp (tr->guid, pr->guid))
            {
                dbg_tap_info("Using first TAP device: '%s' %s\n", pr->name, tr->guid);
                if (name)
                    strcpy(name, pr->name);

                return tr->guid;
            }
        }
    }
    return NULL;
}



int open_tun (const char *dev, const char *dev_type, const char *dev_node, struct tuntap *tt)
{
    char device_path[256];
    const char *device_guid = NULL;
    DWORD len;

    dbg_tap_info("open_tun, tt->ipv6=%d\n", tt->ipv6 );

    if (!(tt->type == DEV_TYPE_TAP || tt->type == DEV_TYPE_TUN))
    {
        dbg_tap_info("Unknown virtual device type: '%s'\n", dev);
        return -1;
    }

    /*
     * Lookup the device name in the registry, using the --dev-node high level name.
     */
    {
        const struct tap_reg *tap_reg = get_tap_reg();
        const struct panel_reg *panel_reg = get_panel_reg();
        char name[256];

        if (!(tap_reg && panel_reg))
        {
            dbg_tap_info("No TUN/TAP devices found\n");
            return -1;
        }

        /* Get the device GUID for the device specified with --dev-node. */
        device_guid = get_first_device_guid (tap_reg, panel_reg, name);

        if (!device_guid)
            dbg_tap_info("TAP-Windows adapter '%s' not found\n", dev_node);

        /* Open Windows TAP-Windows adapter */
        snprintf (device_path, sizeof(device_path), "%s%s%s",
                  USERMODEDEVICEDIR,
                  device_guid,
                  TAP_WIN_SUFFIX);

        tt->hand = CreateFile (
            device_path,
            GENERIC_READ | GENERIC_WRITE,
            0,     /* was: FILE_SHARE_READ */
            0,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED,
            0
            );

        if (tt->hand == INVALID_HANDLE_VALUE)
            dbg_tap_info("CreateFile failed on TAP device: %s\n", device_path);

        /* translate high-level device name into a device instance
           GUID using the registry */
        tt->actual_name = PICO_ZALLOC(strlen(name) + 1);
        if (tt->actual_name)
            strcpy(tt->actual_name, name);
    }

    dbg_tap_info("TAP-WIN32 device [%s] opened: %s\n", tt->actual_name, device_path);
    /* TODO TODO TODO */
    /* tt->adapter_index = get_adapter_index (device_guid); */

    /* get driver version info */
    {
        ULONG info[3];
        /* TODO TODO TODO */
        /* CLEAR (info); */
        if (DeviceIoControl (tt->hand, TAP_WIN_IOCTL_GET_VERSION, &info, sizeof (info), &info, sizeof (info), &len, NULL))
        {
            dbg_tap_info ("TAP-Windows Driver Version %d.%d %s\n",
                          (int) info[0],
                          (int) info[1],
                          (info[2] ? "(DEBUG)" : ""));

        }

        if (!(info[0] == TAP_WIN_MIN_MAJOR && info[1] >= TAP_WIN_MIN_MINOR))
            dbg_tap_info ("ERROR:  This version of " PACKAGE_NAME " requires a TAP-Windows driver that is at least version %d.%d  \
                    -- If you recently upgraded your " PACKAGE_NAME " distribution,                             \
                    a reboot is probably required at this point to get Windows to see the new driver.\n",
                          TAP_WIN_MIN_MAJOR,
                          TAP_WIN_MIN_MINOR);

        /* usage of numeric constants is ugly, but this is really tied to
         * *this* version of the driver
         */
        if ( tt->ipv6 && tt->type == DEV_TYPE_TUN && info[0] == 9 && info[1] < 8)
        {
            dbg_tap_info("WARNING:  Tap-Win32 driver version %d.%d does not support IPv6 in TUN mode.  IPv6 will be disabled. \
                    Upgrade to Tap-Win32 9.8 (2.2-beta3 release or later) or use TAP mode to get IPv6\n", (int) info[0], (int) info[1] );
            tt->ipv6 = 0;
        }

        /* tap driver 9.8 (2.2.0 and 2.2.1 release) is buggy
         */
        if ( tt->type == DEV_TYPE_TUN && info[0] == 9 && info[1] == 8)
        {
            dbg_tap_info("ERROR:  Tap-Win32 driver version %d.%d is buggy regarding small IPv4 packets in TUN mode.  Upgrade to Tap-Win32 9.9 (2.2.2 release or later) or use TAP mode\n", (int) info[0], (int) info[1] );
        }
    }

    /* get driver MTU */
    {
        ULONG mtu;
        if (DeviceIoControl (tt->hand, TAP_WIN_IOCTL_GET_MTU,
                             &mtu, sizeof (mtu),
                             &mtu, sizeof (mtu), &len, NULL))
        {
            tt->post_open_mtu = (int) mtu;
            dbg_tap_info("TAP-Windows MTU=%d\n", (int) mtu);
        }
    }


    /* get driver MAC */
    {
        uint8_t mac[6] = {
            0, 0, 0, 0, 0, 0
        };
        if (DeviceIoControl (tt->hand, TAP_WIN_IOCTL_GET_MAC,
                             mac, sizeof (mac),
                             mac, sizeof (mac), &len, NULL))
        {
            dbg_tap_info("TAP-Windows MAC=[%x,%x,%x,%x,%x,%x]\n", mac[0], mac[1], mac[2],
                         mac[2], mac[4], mac[5]);
            memcpy(tt->mac, mac, sizeof(mac));
        }
    }

    /* set point-to-point mode if TUN device */

    if (tt->type == DEV_TYPE_TUN)
    {
        dbg_tap_info("TUN type not supported for now...\n");
        return -1;
    }
    else if (tt->type == DEV_TYPE_TAP)
    { /* TAP DEVICE */
        dbg_tap_info("TODO: Set Point-to-point through DeviceIoControl\n");
    }

    /* set driver media status to 'connected' */
    {
        ULONG status = TRUE;
        if (!DeviceIoControl (tt->hand, TAP_WIN_IOCTL_SET_MEDIA_STATUS,
                              &status, sizeof (status),
                              &status, sizeof (status), &len, NULL))
            dbg_tap_info("WARNING: The TAP-Windows driver rejected a TAP_WIN_IOCTL_SET_MEDIA_STATUS DeviceIoControl call.");
    }

    return 0;
}




/* TODO: Closing a TUN device is currently not implemented */
/*
   void close_tun (struct tuntap *tt)
   {
    (void)tt;
   }
 */


int tap_win_getinfo (const struct tuntap *tt, char *buf, int bufsize)
{
    if (tt && tt->hand != NULL && buf != NULL)
    {
        DWORD len;
        if (DeviceIoControl (tt->hand, TAP_WIN_IOCTL_GET_INFO,
                             buf, bufsize,
                             buf, bufsize,
                             &len, NULL))
        {
            return 0;
        }
    }

    return -1;
}

void tun_show_debug (struct tuntap *tt, char *buf, int bufsize)
{
    if (tt && tt->hand != NULL && buf != NULL)
    {
        DWORD len;
        while (DeviceIoControl (tt->hand, TAP_WIN_IOCTL_GET_LOG_LINE,
                                buf, bufsize,
                                buf, bufsize,
                                &len, NULL))
        {
            dbg_tap_info("TAP-Windows: %s\n", buf);
        }
    }
}


/* returns the state */
int tun_read_queue (struct tuntap *tt, uint8_t *buffer, int maxsize)
{
    if (tt->reads.iostate == IOSTATE_INITIAL)
    {
        DWORD len = 1500;
        BOOL status;
        int err;

        /* reset buf to its initial state */
        tt->reads.buf = tt->reads.buf_init;
        tt->reads.buf_len = tt->reads.buf_init_len;

        len = maxsize ? maxsize : (tt->reads.buf_len);
        if (len > (tt->reads.buf_len)) /* clip to buffer len */
            len = tt->reads.buf_len;

        /* the overlapped read will signal this event on I/O completion */
        if (!ResetEvent (tt->reads.overlapped.hEvent))
            dbg_tap("ResetEvent failed\n");

        status = ReadFile(
            tt->hand,
            buffer,
            len,
            &tt->reads.size,
            &tt->reads.overlapped
            );

        if (status) /* operation completed immediately? */
        {
            /* since we got an immediate return, we must signal the event object ourselves */
            /* ASSERT (SetEvent (tt->reads.overlapped.hEvent)); */
            if (!SetEvent (tt->reads.overlapped.hEvent))
                dbg_tap("SetEvent failed\n");

            tt->reads.iostate = IOSTATE_IMMEDIATE_RETURN;
            tt->reads.status = 0;

            dbg_tap_win32 ("WIN32 I/O: TAP Read immediate return [%d,%d]\n",
                       (int) len,
                       (int) tt->reads.size);
        }
        else
        {
            err = GetLastError ();
            if (err == ERROR_IO_PENDING) /* operation queued? */
            {
                tt->reads.iostate = IOSTATE_QUEUED;
                tt->reads.status = err;
                dbg_tap_win32 ("WIN32 I/O: TAP Read queued [%d]\n", (int) len);
            }
            else /* error occurred */
            {
                if (!SetEvent (tt->reads.overlapped.hEvent))
                    dbg_tap("SetEvent failed\n");

                tt->reads.iostate = IOSTATE_IMMEDIATE_RETURN;
                tt->reads.status = err;
                dbg_tap ("WIN32 I/O: TAP Read error [%d] : %d\n", (int) len, (int) err);
            }
        }
    }

    return tt->reads.iostate;
}

/* Finalize any pending overlapped IO's */
int tun_finalize(HANDLE h, struct overlapped_io *io, uint8_t **buf, uint32_t *buf_len)
{
    int ret = -1;
    BOOL status;

    switch (io->iostate)
    {
    case IOSTATE_QUEUED:
        status = GetOverlappedResult(
            h,
            &io->overlapped,
            &io->size,
            0u
            );
        if (status)
        {
            /* successful return for a queued operation */
            if (buf)
            {
                *buf = io->buf;
                *buf_len = io->buf_len;
            }

            ret = io->size;
            io->iostate = IOSTATE_INITIAL;

            if (!ResetEvent (io->overlapped.hEvent))
                dbg_tap("ResetEvent in finalize failed!\n");

            dbg_tap_win32 ("WIN32 I/O: TAP Completion success: QUEUED! [%d]\n", ret);
        }
        else
        {
            /* error during a queued operation */
            /* error, or just not completed? */
            ret = 0;
            if (GetLastError() != ERROR_IO_INCOMPLETE)
            {
                /* if no error (i.e. just not finished yet),
                   then DON'T execute this code */
                io->iostate = IOSTATE_INITIAL;
                if (!ResetEvent (io->overlapped.hEvent))
                    dbg_tap("ResetEvent in finalize failed!\n");

                dbg_tap("WIN32 I/O: TAP Completion error\n");
                ret = -1;     /* There actually was an error */
            }
        }

        break;

    case IOSTATE_IMMEDIATE_RETURN:
        io->iostate = IOSTATE_INITIAL;
        if (!ResetEvent (io->overlapped.hEvent))
            dbg_tap("ResetEvent in finalize failed!\n");

        if (io->status)
        {
            /* error return for a non-queued operation */
            SetLastError (io->status);
            ret = -1;
            dbg_tap("WIN32 I/O: TAP Completion non-queued error\n");
        }
        else
        {
            /* successful return for a non-queued operation */
            if (buf)
                *buf = io->buf;

            ret = io->size;
            dbg_tap_win32 ("WIN32 I/O: TAP Completion non-queued success [%d]\n", ret);
        }

        break;

    case IOSTATE_INITIAL:     /* were we called without proper queueing? */
        SetLastError (ERROR_INVALID_FUNCTION);
        ret = -1;
        dbg_tap ("WIN32 I/O: TAP Completion BAD STATE\n");
        break;

    default:
        dbg_tap ("Some weird case happened..\n");
    }

    if (buf)
        *buf_len = ret;

    return ret;
}



/* returns the amount of bytes written */
int tun_write_queue (struct tuntap *tt, uint8_t *buf, uint32_t buf_len)
{
    if (tt->writes.iostate == IOSTATE_INITIAL)
    {
        BOOL status;
        int err;

        /* make a private copy of buf */
        tt->writes.buf = tt->writes.buf_init;
        tt->writes.buf_len = buf_len;
        memcpy(tt->writes.buf, buf, buf_len);

        /* the overlapped write will signal this event on I/O completion */
        if (!ResetEvent (tt->writes.overlapped.hEvent))
            dbg_tap("ResetEvent in write_queue failed!\n");

        status = WriteFile(
            tt->hand,
            tt->writes.buf,
            tt->writes.buf_len,
            &tt->writes.size,
            &tt->writes.overlapped
            );

        if (status) /* operation completed immediately? */
        {
            tt->writes.iostate = IOSTATE_IMMEDIATE_RETURN;

            /* since we got an immediate return, we must signal the event object ourselves */
            if (!SetEvent (tt->writes.overlapped.hEvent))
                dbg_tap("SetEvent in write_queue failed!\n");

            tt->writes.status = 0;

            dbg_tap_win32 ("WIN32 I/O: TAP Write immediate return [%d,%d]\n",
                       (int)(tt->writes.buf_len),
                       (int)tt->writes.size);
        }
        else
        {
            err = GetLastError ();
            if (err == ERROR_IO_PENDING) /* operation queued? */
            {
                tt->writes.iostate = IOSTATE_QUEUED;
                tt->writes.status = err;
                dbg_tap_win32("WIN32 I/O: TAP Write queued [%d]\n",
                          (tt->writes.buf_len));
            }
            else /* error occurred */
            {
                if (!SetEvent (tt->writes.overlapped.hEvent))
                    dbg_tap("SetEvent in write_queue failed!\n");

                tt->writes.iostate = IOSTATE_IMMEDIATE_RETURN;
                tt->writes.status = err;
                dbg_tap ("WIN32 I/O: TAP Write error [%d] : %d\n", (int) &tt->writes.buf_len, (int) err);
            }
        }
    }

    return tt->writes.iostate;
}

static inline int overlapped_io_active (struct overlapped_io *o)
{
    return o->iostate == IOSTATE_QUEUED || o->iostate == IOSTATE_IMMEDIATE_RETURN;
}

/* if >= 0: returns the amount of bytes read, otherwise error! */
static int tun_write_win32 (struct tuntap *tt, uint8_t *buf, uint32_t buf_len)
{
    int err = 0;
    int status = 0;
    if (overlapped_io_active (&tt->writes))
    {
        status = tun_finalize (tt->hand, &tt->writes, NULL, 0);
        if (status == 0)
        {
            /* busy, just wait, do not schedule a new write */
            return 0;
        }

        if (status < 0)
            err = GetLastError ();
    }

    /* the overlapped IO is done, now we can schedule a new write */
    tun_write_queue (tt, buf, buf_len);
    if (status < 0)
    {
        SetLastError (err);
        return status;
    }
    else
        return buf_len;
}


/* if >= 0: returns the amount of bytes read, otherwise error! */
static int tun_read_win32 (struct tuntap *tt, uint8_t *buf, uint32_t buf_len)
{
    int err = 0;
    int status = 0;


    /* First, finish possible pending IOs */
    if (overlapped_io_active (&tt->reads))
    {
        status = tun_finalize (tt->hand, &tt->reads, &buf, &buf_len);
        if (status == 0)
        {
            /* busy, just wait, do not schedule a new read */
            return 0;
        }

        if (status < 0)
        {
            dbg_tap ("tun_finalize status < 0: %d\n", status);
            err = GetLastError ();
        }

        if (status > 0)
        {
            return buf_len;
        }
    }

    /* If no pending IOs, schedule a new read */
    /* queue, or immediate return */
    if (IOSTATE_IMMEDIATE_RETURN == tun_read_queue(tt, buf, buf_len))
    {
        return tt->reads.size;
    }

    /* If the pending IOs gave an error, report it */
    if (status < 0)
    {
        SetLastError (err);
        return status;
    }
    else
    {
        /* no errors, but the newly scheduled read is now pending */
        return 0;
    }
}


static int read_tun_buffered(struct tuntap *tt, uint8_t *buf, uint32_t buf_len)
{
    return tun_read_win32 (tt, buf, buf_len);
}

static int write_tun_buffered(struct tuntap *tt, uint8_t *buf, uint32_t buf_len)
{
    return tun_write_win32 (tt, buf, buf_len);
}


static int pico_tap_send(struct pico_device *dev, void *buf, int len)
{
    uint32_t bytes_sent = 0;
    struct pico_device_tap *tap = (struct pico_device_tap *) dev;

    /* Increase the statistic count */
    tap->statistics_frames_out++;

    bytes_sent = write_tun_buffered (tap->tt, buf, len);
    dbg_tap("TX> sent %d bytes\n", bytes_sent);

    /* Discard the frame content silently. */
    return bytes_sent;
}

uint8_t recv_buffer[1500];

static int pico_tap_poll(struct pico_device *dev, int loop_score)
{
    struct pico_device_tap *tap = (struct pico_device_tap *) dev;
    while (loop_score)
    {
        int bytes_read = read_tun_buffered(tap->tt, recv_buffer, 1500);
        loop_score--;
        if (bytes_read > 0)
        {
            dbg_tap("RX< recvd: %d bytes\n", bytes_read);
            pico_stack_recv(dev, recv_buffer, bytes_read);
            /* break; */
        }
        else
            break;
    }
    return loop_score;
}



#define CLEAR(x) memset(&(x), 0, sizeof(x))

void overlapped_io_init (struct overlapped_io *o, int event_state)
{
    CLEAR (*o);

    /* manual reset event, initially set according to event_state */
    o->overlapped.hEvent = CreateEvent (NULL, TRUE, event_state, NULL);
    if (o->overlapped.hEvent == NULL)
        dbg_tap ("Error: overlapped_io_init: CreateEvent failed\n");

    /* allocate buffer for overlapped I/O */
    o->buf_init = PICO_ZALLOC(1500); /* XXX: MTU */
    o->buf_init_len = 1500; /* XXX: MTU */
    if (!(o->buf_init))
        dbg_tap("buffer alloc failed!\n"); /* XXX: return -1 or so? */
    else
        dbg_tap("overlapped_io_init buffer allocated!\n");
}

void init_tun_post (struct tuntap *tt)
{
    dbg_tap("TUN post init (for overlapped io)\n");
    overlapped_io_init (&tt->reads, FALSE);
    overlapped_io_init (&tt->writes, TRUE);
    tt->rw_handle.read = tt->reads.overlapped.hEvent;
    tt->rw_handle.write = tt->writes.overlapped.hEvent;
}


/*
 * Public interface: pico_tap_create
 * TODO: pico_tap_destroy
 */

struct pico_device *pico_tap_create(char *name, uint8_t *mac)
{
    struct pico_device_tap *tap = PICO_ZALLOC(sizeof(struct pico_device_tap));
    struct tuntap *tt = PICO_ZALLOC(sizeof(struct tuntap), 1);

    if (!(tap) || !(tt))
        return NULL;

    tap->dev.overhead = 0;
    tap->statistics_frames_out = 0;
    tap->dev.send = pico_tap_send;
    tap->dev.poll = pico_tap_poll;

    show_tap_win_adapters();

    tt->type = DEV_TYPE_TAP;
    if (open_tun(NULL, NULL, "tap0", tt))
    {
        dbg_tap("Failed to create TAP device!\n");
        PICO_FREE(tt);
        PICO_FREE(tap);
        return NULL;
    }

    tap->tt = tt;

    if( 0 != pico_device_init((struct pico_device *)tap, name, mac)) {
        return NULL;
    }

    init_tun_post(tt); /* init overlapped io */

    dbg_tap("Device %s created.\n", tap->dev.name);

    return (struct pico_device *)tap;
}
