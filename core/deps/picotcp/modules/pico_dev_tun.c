/*********************************************************************
   PicoTCP. Copyright (c) 2012-2017 Altran Intelligent Systems. Some rights reserved.
   See COPYING, LICENSE.GPLv2 and LICENSE.GPLv3 for usage.

   Authors: Daniele Lacamera
 *********************************************************************/

#ifndef _WIN32
#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#ifdef __APPLE__
#include <sys/sys_domain.h>
#include <net/if_utun.h>                // UTUN_CONTROL_NAME
#include <sys/kern_control.h>   // struct socketaddr_ctl
#else
#include <linux/if_tun.h>
#endif
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
#ifdef __APPLE__
	// Add the protocol (IP) before the packet (4 bytes)
	uint8_t *p = (uint8_t *)malloc(len + 4);
	*(uint32_t *)p = 2;
	memcpy(p + 4, buf, len);
	int rc = (int)write(tun->fd, p, len + 4);
	free(p);
	return rc;
#endif
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
#ifdef __APPLE__
			pico_stack_recv(dev, buf + 4, (uint32_t)len - 4);
#else
            pico_stack_recv(dev, buf, (uint32_t)len);
#endif
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

#ifdef IFF_TUN  // Linux

static int tun_open(const char *name)
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

#else   // BSD, OS X, ...

#ifdef __APPLE__
static int tun_open(const char *name)
{
	struct ctl_info ctlInfo;
	strlcpy(ctlInfo.ctl_name, UTUN_CONTROL_NAME, sizeof(ctlInfo.ctl_name));
	int fd = -1;
	
	for (int unit = 0; unit < 256 && fd == -1; unit++)@
	{
		fd = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
		if (fd < 0) {
			perror("socket");
			continue;
		}
		
		struct sockaddr_ctl sc;
		
		if (ioctl(fd, CTLIOCGINFO, &ctlInfo) == -1) {
			close(fd);
			perror("ioctl");
			fd = -1;
			continue;
		}
		printf("ctl_info: {ctl_id: %ud, ctl_name: %s}\n",
			   ctlInfo.ctl_id, ctlInfo.ctl_name);
		
		sc.sc_id = ctlInfo.ctl_id;
		sc.sc_len = sizeof(sc);
		sc.sc_family = AF_SYSTEM;
		sc.ss_sysaddr = AF_SYS_CONTROL;
		sc.sc_unit = unit;
		
		if (connect(fd, (struct sockaddr *)&sc, sizeof(sc)) < 0) {
			perror("connect");
			close(fd);
			fd = -1;
			continue;
		}
		printf("Opened tunnel utun%d\n", unit);
		
		// set_nonblock (fd);
		fcntl (fd, F_SETFL, O_NONBLOCK);
		
		// set_cloexec (fd);
		/*
		 int s = socket(PF_ROUTE, SOCK_RAW, 0);
		 af = AF_INET;
		 aflen = sizeof(struct sockaddr_in);
		 flags |= RTF_UP;
		 flags |= RTF_HOST;
		 if ((ret = rtmsg(*cmd, flags)) == 0)
		 break;
		 */
	}
	
	return fd;
}

#elif defined(SIOCIFCREATE)
static int tun_open(const char *name)
{
	int fd;
	int s;
	struct ifreq ifr;
	
	fd = open(name, O_RDWR);
	if (fd == -1)
	{
		s = socket(AF_INET, SOCK_DGRAM, 0);
		if (s < 0)
			return -1;
		
		memset(&ifr, 0, sizeof(ifr));
		strncpy(ifr.ifr_name, name + 5, sizeof(ifr.ifr_name) - 1);
		if (!ioctl(s, SIOCIFCREATE, &ifr))
			fd = open(name, O_RDWR);
		
		close(s);
	}
	
	return fd;
}
#else
#define tun_open(tun_name) open(tun_name, O_RDWR)
#endif
#endif

struct pico_device *pico_tun_create(const char *name)
{
    struct pico_device_tun *tun = PICO_ZALLOC(sizeof(struct pico_device_tun));

    if (!tun)
        return NULL;

    if( 0 != pico_device_init((struct pico_device *)tun, name, NULL)) {
        printf("Tun init failed.\n");
        pico_tun_destroy((struct pico_device *)tun);
        return NULL;
    }

    tun->dev.overhead = 0;
    tun->fd = tun_open(name);
    if (tun->fd < 0) {
        printf("Tun creation failed.\n");
        pico_tun_destroy((struct pico_device *)tun);
        return NULL;
    }

    tun->dev.send = pico_tun_send;
    tun->dev.poll = pico_tun_poll;
    tun->dev.destroy = pico_tun_destroy;
    dbg("Device %s created.\n", tun->dev.name);
    return (struct pico_device *)tun;
}

#endif
