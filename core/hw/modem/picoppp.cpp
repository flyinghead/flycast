#include <queue>

extern "C" {
#include <pico_stack.h>
#include <pico_dev_ppp.h>
#include <pico_dev_tap.h>
#include <pico_arp.h>
#include <pico_dev_tun.h>
#ifdef DHCP
#include <pico_dhcp_client.h>
#endif
}

#define NETWORK_TUN

#include "types.h"
#include "cfg/cfg.h"
#include "picoppp.h"

static struct pico_device *ppp;
struct pico_device* tap;
struct pico_device* tun;

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
    struct pico_ip4 ipaddr, netmask, zero = {
    	    0
    	};

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
    pico_string_to_ipv4(dc_ip.c_str(), &ipaddr.addr);
    pico_ppp_set_peer_ip(ppp, ipaddr);
    pico_string_to_ipv4("192.168.167.1", &ipaddr.addr);
    pico_ppp_set_ip(ppp, ipaddr);

    string dns_ip = cfgLoadStr("network", "DNS", "46.101.91.123");
    pico_string_to_ipv4(dns_ip.c_str(), &ipaddr.addr);
    pico_ppp_set_dns1(ppp, ipaddr);

#ifdef NETWORK_TAP
    // TAP
    // tap config:
    // # ip tuntap add mode tap user joe
    // # ifconfig tap0 192.168.5.5
    // # ip link set tap0 up
    // # ip route add <IP>/32 dev tap0		# where <IP> is the value of network:IP in emu.cfg./. This also allows proxy arp
    // # echo '1' >/proc/sys/net/ipv4/conf/all/proxy_arp
    // (or ...conf/tap0/proxy_arp and ...conf/eth0/proxy_arp only)
    tap = pico_tap_create("tap0");
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
}
