/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

   Authors: Daniele Lacamera
 *********************************************************************/


#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include "pico_device.h"
#include "pico_dev_tun.h"
#include "pico_stack.h"

#include <sys/poll.h>

struct pico_device_tun {
    struct pico_device dev;
    int fd;
};

#define TUN_MTU 2048

static int pico_tun_send(struct pico_device *dev, void *buf, int len)
{
    struct pico_device_tun *tun = (struct pico_device_tun *) dev;
    return (int)write(tun->fd, buf, (uint32_t)len);
}

static int pico_tun_poll(struct pico_device *dev, int loop_score)
{
    struct pico_device_tun *tun = (struct pico_device_tun *) dev;
    struct pollfd pfd;
    unsigned char buf[TUN_MTU];
    int len;
    pfd.fd = tun->fd;
    pfd.events = POLLIN;
    do  {
        if (poll(&pfd, 1, 0) <= 0)
            return loop_score;

        len = (int)read(tun->fd, buf, TUN_MTU);
        if (len > 0) {
            loop_score--;
            pico_stack_recv(dev, buf, (uint32_t)len);
        }
    } while(loop_score > 0);
    return 0;
}

/* Public interface: create/destroy. */

void pico_tun_destroy(struct pico_device *dev)
{
    struct pico_device_tun *tun = (struct pico_device_tun *) dev;
    if(tun->fd > 0)
        close(tun->fd);
}


static int tun_open(char *name)
{
    struct ifreq ifr;
    int tun_fd;
    if((tun_fd = open("/dev/net/tun", O_RDWR)) < 0) {
        return(-1);
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    strncpy(ifr.ifr_name, name, IFNAMSIZ);
    if(ioctl(tun_fd, TUNSETIFF, &ifr) < 0) {
        return(-1);
    }

    return tun_fd;
}



struct pico_device *pico_tun_create(char *name)
{
    struct pico_device_tun *tun = PICO_ZALLOC(sizeof(struct pico_device_tun));

    if (!tun)
        return NULL;

    if( 0 != pico_device_init((struct pico_device *)tun, name, NULL)) {
        dbg("Tun init failed.\n");
        pico_tun_destroy((struct pico_device *)tun);
        return NULL;
    }

    tun->dev.overhead = 0;
    tun->fd = tun_open(name);
    if (tun->fd < 0) {
        dbg("Tun creation failed.\n");
        pico_tun_destroy((struct pico_device *)tun);
        return NULL;
    }

    tun->dev.send = pico_tun_send;
    tun->dev.poll = pico_tun_poll;
    tun->dev.destroy = pico_tun_destroy;
    dbg("Device %s created.\n", tun->dev.name);
    return (struct pico_device *)tun;
}

