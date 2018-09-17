#include <queue>

extern "C" {
#include <pico_stack.h>
#include <pico_dev_ppp.h>
#ifdef _WIN32
#include <pico_dev_tap_windows.h>
#else
#include <pico_dev_tap.h>
#endif
#include <pico_arp.h>
#include <pico_dev_tun.h>
#ifdef DHCP
#include <pico_dhcp_client.h>
#endif
}

#ifdef _WIN32
#include <iphlpapi.h>
#define NETWORK_TAP
#else
#define NETWORK_TUN
#endif

#include "types.h"
#include "cfg/cfg.h"
#include "picoppp.h"

static struct pico_device *ppp;
struct pico_device* tap;
struct pico_device* tun;
u8 virtual_mac[] = { 0x76, 0x6D, 0x61, 0x63, 0x30, 0x31 };

static std::queue<u8> in_buffer;
static std::queue<u8> out_buffer;

static int modem_read(struct pico_device *dev, void *data, int len)
{
	u8 *p = (u8 *)data;

	int count = 0;
	while (!out_buffer.empty() && count < len)
	{
		*p++ = out_buffer.front();
		out_buffer.pop();
		count++;
	}

    return count;
}

static int modem_write(struct pico_device *dev, const void *data, int len)
{
	u8 *p = (u8 *)data;

	while (len > 0)
	{
		in_buffer.push(*p++);
		len--;
	}

    return len;
}

void write_pico(u8 b)
{
	out_buffer.push(b);
}

int read_pico()
{
	pico_stack_tick();

	if (in_buffer.empty())
		return -1;
	else
	{
		u32 b = in_buffer.front();
		in_buffer.pop();
		return b;
	}
}

static int modem_set_speed(struct pico_device *dev, uint32_t speed)
{
    return 0;
}

#ifdef DHCP
static uint32_t dhcpclient_xid;
static struct pico_ip4 dchp_address = { 0 };

void callback_dhcpclient(void *arg, int code)
{
    char s_address[16] = { }, s_gateway[16] = { };

    printf("DHCP client: callback happened with code %d!\n", code);
    if (code == PICO_DHCP_SUCCESS)
    {
    	dchp_address = pico_dhcp_get_address(arg);
        //gateway = pico_dhcp_get_gateway(arg);
        pico_ipv4_to_string(s_address, dchp_address.addr);
        //pico_ipv4_to_string(s_gateway, gateway.addr);
        printf("DHCP client: got IP %s assigned with cli %p\n", s_address, arg);
    }
}
#endif

static bool pico_stack_inited;

bool start_pico()
{
    struct pico_ip4 ipaddr, dcaddr, dnsaddr, netmask, zero = {
    	    0
    	};

#ifdef _WIN32
    // No de-init on Windows yet
    if (pico_stack_inited)
	return;
#endif

    if (!pico_stack_inited)
    {
    	pico_stack_init();
    	pico_stack_inited = true;
    }

    // PPP
    ppp = pico_ppp_create();
    if (!ppp)
        return false;
    string dc_ip = cfgLoadStr("network", "IP", "");
    if (dc_ip.length() == 0)
    {
    	printf("No IP address set for Netplay. Set IP= in the [network] section\n");
    	return false;
    }
    pico_string_to_ipv4(dc_ip.c_str(), &dcaddr.addr);
    pico_ppp_set_peer_ip(ppp, dcaddr);
    pico_string_to_ipv4("192.168.167.1", &ipaddr.addr);
    pico_ppp_set_ip(ppp, ipaddr);

    string dns_ip = cfgLoadStr("network", "DNS", "46.101.91.123");
    pico_string_to_ipv4(dns_ip.c_str(), &dnsaddr.addr);
    pico_ppp_set_dns1(ppp, dnsaddr);

#ifdef NETWORK_TAP
    // TAP
    // tap config:
    // # ip tuntap add mode tap user joe
    // # ifconfig tap0 192.168.5.5
    // # ip link set tap0 up
    // # ip route add <IP>/32 dev tap0		# where <IP> is the value of network:IP in emu.cfg./. This also allows proxy arp
    // # echo '1' >/proc/sys/net/ipv4/conf/all/proxy_arp
    // (or ...conf/tap0/proxy_arp and ...conf/eth0/proxy_arp only)
#ifdef _WIN32
    tap = pico_tap_create("tap0", virtual_mac);
#else
    tap = pico_tap_create("tap0");
#endif
    if (!tap)
    {
    	stop_pico();
    	return false;
    }

    pico_string_to_ipv4("192.168.166.2", &ipaddr.addr);
    pico_string_to_ipv4("255.255.255.0", &netmask.addr);
    pico_ipv4_link_add(tap, ipaddr, netmask);
    // Proxy ARP
    pico_arp_create_entry(tap->eth->mac.addr, ipaddr, ppp);

#ifdef _WIN32
    int err;

    // Enable routing
    OVERLAPPED overlapped;
    memset(&overlapped, 0, sizeof(overlapped));
    overlapped.hEvent = overlapped.hEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
    if (overlapped.hEvent == NULL)
	printf("CreateEvent failed with error %d\n", GetLastError());
    else
    {
	HANDLE handle;
	err = EnableRouter(&handle, &overlapped);
	if (err != ERROR_IO_PENDING)
	    printf("EnableRouter failed with error %d\n", err);
	else
	    printf("Windows router enabled\n");
	CloseHandle(overlapped.hEvent);
    }

    // Get the LAN interface index
    DWORD idx = -1;
    err = GetBestInterface(dnsaddr.addr, &idx);
    if (err != NO_ERROR)
	printf("GetBestInterface failed error %d\n", err);

    // Create a Proxy ARP entry for the DC on the local LAN
    if (idx != -1)
    {
	err = CreateProxyArpEntry(dcaddr.addr, 0xffffffff, idx);
	if (err == ERROR_OBJECT_ALREADY_EXISTS)
		printf("Proxy ARP entry already exists\n");
	else if (err != NO_ERROR)
		printf("CreateProxyArpEntry failed error %d\n", err);
    }

	// Get the TAP interface index
	unsigned long size = sizeof(IP_INTERFACE_INFO);
	IP_INTERFACE_INFO *infos = (IP_INTERFACE_INFO *)malloc(size);
	err = GetInterfaceInfo(infos, &size);
	if (err == ERROR_INSUFFICIENT_BUFFER)
	{
	    free(infos);
	    infos = (IP_INTERFACE_INFO *)malloc(size);
	    err = GetInterfaceInfo(infos, &size);
	    if (err != NO_ERROR)
	    {
		printf("GetInterfaceInfo failed error %d\n", err);
		infos->NumAdapters = 0;
	    }
	}

	const char *tap_guid = pico_tap_get_guid(tap);
	wchar_t wtap_guid[40];
	MultiByteToWideChar(CP_UTF8, 0, tap_guid, strlen(tap_guid), &wtap_guid[0], 40);
	DWORD tap_idx = -1; // 11;
	for (int i = 0; i < infos->NumAdapters; i++)
	{
	    printf("Found interface %ls index %d\n", infos->Adapter[i].Name, infos->Adapter[i].Index);
	    if (wcsstr(infos->Adapter[i].Name, wtap_guid) != NULL)
	    {
		tap_idx = infos->Adapter[i].Index;
		break;
	    }
	}
	free(infos);

	// Set the TAP interface IP address
	pico_string_to_ipv4("192.168.166.1", &ipaddr.addr);
	pico_string_to_ipv4("255.255.255.0", &netmask.addr);
	unsigned long nte_context, nte_instance;
	err = AddIPAddress(ipaddr.addr, netmask.addr, tap_idx, &nte_context, &nte_instance);
	if (err == ERROR_OBJECT_ALREADY_EXISTS)
		printf("TAP IP address already set\n");
	else if (err != NO_ERROR)
		printf("AddIpAddress failed with error %d\n", err);
	else
		printf("TAP IP address set\n");

	// Create a route to the DC through the TAP interface
	if (tap_idx != -1)
	{
		MIB_IPFORWARDROW fwd;
		memset(&fwd, 0, sizeof(fwd));
		fwd.dwForwardDest = dcaddr.addr;
		fwd.dwForwardMask = 0xffffffff;
		fwd.dwForwardIfIndex = tap_idx;
		fwd.dwForwardProto = MIB_IPPROTO_NETMGMT;
		fwd.dwForwardAge = INFINITE;
		fwd.dwForwardMetric1 = 500;
		err = CreateIpForwardEntry(&fwd);
		if (err == ERROR_OBJECT_ALREADY_EXISTS)
			printf("IP forward entry already exists\n");
		else if (err != NO_ERROR)
			printf("CreateIpForwardEntry failed with error %d\n", err);
		else
			printf("IP forward entry created\n");
	}

#endif
#endif

#ifdef NETWORK_TUN
    // TUN
    // tun config:
    // # ip tuntap add mode tun user joe
    // # ip link set tun0 up
    // # ip route add <IP>/32 dev tun0		# where <IP> is the value of network:IP in emu.cfg./. This also allows proxy arp
    // # echo '1' >/proc/sys/net/ipv4/conf/all/proxy_arp
    // (or ...conf/tun0/proxy_arp and ...conf/eth0/proxy_arp only)
    tun = pico_tun_create("tun0");
    if (!tun)
    {
    	stop_pico();
    	return false;
    }

    pico_string_to_ipv4("192.168.166.1", &ipaddr.addr);
    pico_string_to_ipv4("255.255.255.255", &netmask.addr);
    pico_ipv4_link_add(tun, ipaddr, netmask);
#endif

    // Default route
    pico_string_to_ipv4("192.168.166.1", &ipaddr.addr);
    pico_ipv4_route_add(zero, zero, ipaddr, 1, NULL);

    pico_ppp_set_serial_read(ppp, modem_read);
    pico_ppp_set_serial_write(ppp, modem_write);
    pico_ppp_set_serial_set_speed(ppp, modem_set_speed);

    pico_ppp_connect(ppp);

#ifdef DHCP
    if (pico_dhcp_initiate_negotiation(tap, &callback_dhcpclient, &dhcpclient_xid) < 0)
    {
        printf("%s: error initiating negotiation: %s\n", __FUNCTION__, strerror(pico_err));
        return false;
    }
#endif

    return true;
}

void stop_pico()
{
#ifndef _WIN32
	if (ppp)
	{
		pico_ppp_destroy(ppp);
		ppp = NULL;
	}
	if (tap)
	{
		pico_device_destroy(tap);
		tap = NULL;
	}
	if (tun)
	{
		pico_device_destroy(tun);
		tun = NULL;
	}
#endif
}
