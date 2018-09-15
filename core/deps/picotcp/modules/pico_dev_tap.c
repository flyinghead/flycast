/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

   Authors: Daniele Lacamera
 *********************************************************************/


#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <signal.h>
#include "pico_device.h"
#include "pico_dev_tap.h"
#include "pico_stack.h"

#ifndef __FreeBSD__
#include <linux/if_tun.h>
#endif

#include <sys/poll.h>

struct pico_device_tap {
    struct pico_device dev;
    int fd;
};

#define TUN_MTU 2048

/* We only support one global link state - we only have two USR signals, we */
/* can't spread these out over an arbitrary amount of devices. When you unplug */
/* one tap, you unplug all of them. */

static int tapdev_link_state = 0;

static void sig_handler(int signo)
{
    if (signo == SIGUSR1) {
        tapdev_link_state = 0;
    }

    if (signo == SIGUSR2) {
        tapdev_link_state = 1;
    }
}

static int tap_link_state(__attribute__((unused)) struct pico_device *self)
{
    return tapdev_link_state;
}


static int pico_tap_send(struct pico_device *dev, void *buf, int len)
{
    struct pico_device_tap *tap = (struct pico_device_tap *) dev;
    return (int)write(tap->fd, buf, (uint32_t)len);
}

static int pico_tap_poll(struct pico_device *dev, int loop_score)
{
    struct pico_device_tap *tap = (struct pico_device_tap *) dev;
    struct pollfd pfd;
    unsigned char buf[TUN_MTU];
    int len;
    pfd.fd = tap->fd;
    pfd.events = POLLIN;
    do  {
        if (poll(&pfd, 1, 0) <= 0) {
            return loop_score;
        }

        len = (int)read(tap->fd, buf, TUN_MTU);
        if (len > 0) {
            loop_score--;
            pico_stack_recv(dev, buf, (uint32_t)len);
        }
    } while(loop_score > 0);
    return 0;
}

/* Public interface: create/destroy. */

void pico_tap_destroy(struct pico_device *dev)
{
    struct pico_device_tap *tap = (struct pico_device_tap *) dev;
    if(tap->fd > 0) {
        close(tap->fd);
    }
}

#ifndef __FreeBSD__
static int tap_open(char *name)
{
    struct ifreq ifr;
    int tap_fd;
    if((tap_fd = open("/dev/net/tun", O_RDWR)) < 0) {
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    strncpy(ifr.ifr_name, name, IFNAMSIZ);
    if(ioctl(tap_fd, TUNSETIFF, &ifr) < 0) {
        return -1;
    }

    return tap_fd;
}
#else
static int tap_open(char *name)
{
    int tap_fd;
    (void)name;
    tap_fd = open("/dev/tap0", O_RDWR);
    return tap_fd;
}
#endif


#ifndef __FreeBSD__
static int tap_get_mac(char *name, uint8_t *mac)
{
    int sck;
    struct ifreq eth;
    int retval = -1;

    sck = socket(AF_INET, SOCK_DGRAM, 0);
    if(sck < 0) {
        return retval;
    }

    memset(&eth, 0, sizeof(struct ifreq));
    strcpy(eth.ifr_name, name);
    /* call the IOCTL */
    if (ioctl(sck, SIOCGIFHWADDR, &eth) < 0) {
        perror("ioctl(SIOCGIFHWADDR)");
        return -1;
    }

    memcpy (mac, &eth.ifr_hwaddr.sa_data, 6);
    close(sck);
    return 0;

}
#else
#include <net/if_dl.h>
#include <ifaddrs.h>
#include <net/if_types.h>
static int tap_get_mac(char *name, uint8_t *mac)
{
    struct sockaddr_dl *sdl;
    struct ifaddrs *ifap, *root;
    if (getifaddrs(&ifap) != 0)
        return -1;

    root = ifap;
    while(ifap) {
        if (strcmp(name, ifap->ifa_name) == 0) {
            sdl = (struct sockaddr_dl *) ifap->ifa_addr;
        }

        if (sdl->sdl_type == IFT_ETHER) {
            memcpy(mac, LLADDR(sdl), 6);
            freeifaddrs(root);
            return 0;
        }

        ifap = ifap->ifa_next;
    }
    freeifaddrs(root);
    return 0;
}
#endif

struct pico_device *pico_tap_create(char *name)
{
    struct pico_device_tap *tap = PICO_ZALLOC(sizeof(struct pico_device_tap));
    uint8_t mac[6] = {};
    struct sigaction sa;

    if (!tap) {
        return NULL;
    }

    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = sig_handler;

    if ((sigaction(SIGUSR1, &sa, NULL) == 0) &&
        (sigaction(SIGUSR2, &sa, NULL) == 0)) {
        tap->dev.link_state = &tap_link_state;
    }

    tap->dev.overhead = 0;
    tap->fd = tap_open(name);
    if (tap->fd < 0) {
        dbg("Tap creation failed.\n");
        pico_tap_destroy((struct pico_device *)tap);
        return NULL;
    }

    /* Host's mac address is generated * by the host kernel and is
     * retrieved via tap_get_mac().
     */
    if (tap_get_mac(name, mac) < 0) {
        dbg("Tap mac query failed.\n");
        pico_tap_destroy((struct pico_device *)tap);
        return NULL;
    }

    /* To act as a second endpoint in the same subnet, the picoTCP
     * app using the tap device must have a different mac address.
     * For simplicity, we just add 1 to the last byte of the linux
     * endpoint so the two addresses are consecutive.
     */
    mac[5]++;

    if( 0 != pico_device_init((struct pico_device *)tap, name, mac)) {
        dbg("Tap init failed.\n");
        pico_tap_destroy((struct pico_device *)tap);
        return NULL;
    }

    tap->dev.send = pico_tap_send;
    tap->dev.poll = pico_tap_poll;
    tap->dev.destroy = pico_tap_destroy;
    dbg("Device %s created.\n", tap->dev.name);
    return (struct pico_device *)tap;
}

