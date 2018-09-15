# picoTCP

---------------

Welcome to the one and only <font color=ff00f0>picoTCP repository</font>. 

picoTCP is a small-footprint, modular TCP/IP stack designed for embedded systems and the Internet of Things. It's actively being developed by *[Altran Intelligent Systems](http://intelligent-systems.altran.com/)*.

This code is released under the terms of GNU GPL v2 and GNU GPL v3. Some rights reserved.
Other licenses may apply at the sole discretion of the copyright holders.

Learn how to use picoTCP in your project by going through the **Getting Started guide** on our [GitHub wiki](https://github.com/tass-belgium/picotcp/wiki).

For more information send us an email or contact us on [Twitter](https://twitter.com/picotcp), [Facebook](https://www.facebook.com/picoTCP) or [Reddit](http://www.reddit.com/r/picotcp/).

Wondering about picoTCP's code quality? Check [our TiCS score](http://162.13.112.57:42506/tiobeweb/TICS/TqiDashboard.html#axes=Project()&metric=tqi&sel=Project(PicoTCP_rel))

---------------

## Continuous integration

Functional tests: 
[![Jenkins autotest](http://162.13.84.104:8080/buildStatus/icon?job=picoTCP_Rel/PicoTCP_rel_autotest)](http://162.13.84.104:8080/job/picoTCP_Rel/job/PicoTCP_rel_autotest) -
Unit tests      : 
[![Jenkins unit tests](http://162.13.84.104:8080/buildStatus/icon?job=picoTCP_Rel/PicoTCP_rel_unit_tests)](http://162.13.84.104:8080/job/picoTCP_Rel/job/PicoTCP_rel_unit_tests) -
RFC compliance  :
[![Jenkins RFC Compliance](http://162.13.84.104:8080/buildStatus/icon?job=picoTCP_Rel/PicoTCP_rel_RF_mbed)](http://162.13.84.104:8080/job/picoTCP_Rel/job/PicoTCP_rel_RF_mbed) -
TICS quality    :
[![Jenkins TICS](http://162.13.84.104:8080/buildStatus/icon?job=picoTCP_Rel/PicoTCP_rel_TICS)](http://162.13.84.104:8080/job/picoTCP_Rel/job/PicoTCP_rel_TICS/)
Coverity Scan Build status:
[![Coverity Scan Build Status](https://scan.coverity.com/projects/7944/badge.svg)](https://scan.coverity.com/projects/7944)

---------------

## It runs on (pretty much) everything

By keeping interfaces simple, the porting effort to new platforms and OSses are very low. To give you an indication: porting to a new platform can be done in 3 days or less, a new OS in a single day and if you really go crazy, you can do an initial port in a single evening. Different platforms, mean different compilers, that’s why we continuously compile our stack with a bunch of them. The following list shows some of the currently supported platforms, device drivers and compilers.

###  PicoTCP has been used with

**Platforms picoTCP runs on**:
ARM Cortex-M series (ST Micro STM, NXP LPC, TI Stellaris, Freescale K64F),
ARM ARM9-series (ST Micro STR9),
Texas Instruments (MSP430),
Microchip (PIC24, PIC32),
Atmel (AVR 8bit),
Linux (User space (TUN/TAP), Kernel space),
Windows (User space (TAP))

**Network devices picoTCP has worked with**:
BCM43362 (IEEE 802.11), MRF24WG (IEEE 802.11), LPC Ethernet ENET/EMAC (IEEE 802.3), Stellaris Ethernet (IEEE 802.3), STM32 Ethernet (IEEE 802.3), Wiznet W5100 (IEEE 802.3), USB CDC-ECM (CDC1.2), PPP, Virtual drivers ( TUN/TAP, VDE, Libpcap)

**(RT)OSes picoTCP has been integrated into**:
No OS / Bare metal, FreeRTOS, mbed-RTOS, Frosted, linux / POSIX, MS DOS, MS Windows

**Libraries picoTCP has been integrated with**:
wolfSSL, mbedTLS, Mongoose RESTful library, MicroPython

**Compilers picoTCP compiles under**:
GCC, Clang, TCC, ARM-RCVT, IAR, XC-16, XC-32, MSP-GCC, AVR-GCC

Unfortunately we can't release all the code, a.o. because some parts depend on code or binaries that aren't GPL compatible, some parts were developed under a commercial contract, and some consist of very rough proof-of-concept code.
If you want to know more about the availability under the commercial license, or the possibility of using our expert services for porting or driver development, feel free to contact us at picotcp@altran.com.

Your favorite not in the list? Check out the wiki for information and examples on how to port picoTCP to a new platform!

---------------

## Highly configurable and modular design

Features are developed as modules in picoTCP, allowing you to pick the features you want in your application. This results in the smallest possible stack that remains compliant with the internet standards. The schematic below provides an overview of all implemented protocols.

![modular](https://s1.postimg.org/139xbnv7lb/image.png)

---------------


## Simple example

### Preparations
This example uses Ubuntu 14.04. It works on other linux distibutions as well, though you may need to change some package names. See [setting up the environment](https://github.com/tass-belgium/picotcp/wiki/Setting-up-the-environment#prerequisite-packages) for some more info.

```bash
sudo apt-get install git check vde2 libvdeplug2-dev libpcap0.8-dev openvpn wireshark
git clone https://github.com/tass-belgium/picotcp
cd picotcp
make TAP=1
cd ..
```

### The code

Then make a new directory, e.g. `example`, and create a file with the following content : 
[//]: # (The code below is pulled through our CI - please leave the code extractor comments intact!)
[//]: # (code extractor start)
```C
#include <time.h>
#include <pico_stack.h>
#include <pico_ipv4.h>
#include <pico_icmp4.h>
#include <pico_dev_tap.h>

#define NUM_PING 10

static int finished = 0;

/* gets called when the ping receives a reply, or encounters a problem */
void cb_ping(struct pico_icmp4_stats *s)
{
    char host[30];
    pico_ipv4_to_string(host, s->dst.addr);
    if (s->err == 0) {
        /* if all is well, print some pretty info */
        printf("%lu bytes from %s: icmp_req=%lu ttl=%lu time=%lu ms\n", s->size,
                host, s->seq, s->ttl, (long unsigned int)s->time);
        if (s->seq >= NUM_PING)
            finished = 1;
    } else {
        /* if something went wrong, print it and signal we want to stop */
        printf("PING %lu to %s: Error %d\n", s->seq, host, s->err);
        finished = 1;
    }
}


int main(void){
    int id;
    struct pico_ip4 ipaddr, netmask;
    struct pico_device* dev;

    /* initialise the stack. Super important if you don't want ugly stuff like
     * segfaults and such! */
    pico_stack_init();

    /* create the tap device */
    dev = pico_tap_create("tap0");
    if (!dev)
        return -1;

    /* assign the IP address to the tap interface */
    pico_string_to_ipv4("192.168.5.4", &ipaddr.addr);
    pico_string_to_ipv4("255.255.255.0", &netmask.addr);
    pico_ipv4_link_add(dev, ipaddr, netmask);

    printf("starting ping\n");
    id = pico_icmp4_ping("192.168.5.5", NUM_PING, 1000, 10000, 64, cb_ping);

    if (id == -1)
        return -1;

    /* keep running stack ticks to have picoTCP do its network magic. Note that
     * you can do other stuff here as well, or sleep a little. This will impact
     * your network performance, but everything should keep working (provided
     * you don't go overboard with the delays). */
    while (finished != 1)
    {
        usleep(1000);
        pico_stack_tick();
    }

    printf("finished !\n");
    return 0;
}

```

[//]: # (code extractor stop)

### Building and running

Now we can compile this and link it, by running 
```bash
gcc -c -o main.o -I../picotcp/build/include main.c
gcc -o main.elf main.o ../picotcp/build/lib/libpicotcp.a
```

Next we'll create a persistent tap device - a virtual network port. You don't need to repeat this each time, the device will exist until you reboot, or until you go `sudo tunctl -d tap0`
```bash
sudo tunctl -u <username>
sudo ifconfig tap0 192.168.5.5
```

Now, you should be able to run `./main.elf`, and see output like 
```
Protocol ethernet registered (layer: 2).
Protocol ipv4 registered (layer: 3).
Protocol ipv6 registered (layer: 3).
Protocol icmp4 registered (layer: 4).
Protocol icmp6 registered (layer: 4).
Protocol igmp registered (layer: 4).
Protocol udp registered (layer: 4).
Protocol tcp registered (layer: 4).
Device tap0 created.
Assigned ipv4 192.168.5.4 to device tap0
starting ping
64 bytes from 192.168.5.5: icmp_req=1 ttl=64 time=5 ms
64 bytes from 192.168.5.5: icmp_req=2 ttl=64 time=0 ms
64 bytes from 192.168.5.5: icmp_req=3 ttl=64 time=0 ms
64 bytes from 192.168.5.5: icmp_req=4 ttl=64 time=0 ms
64 bytes from 192.168.5.5: icmp_req=5 ttl=64 time=0 ms
64 bytes from 192.168.5.5: icmp_req=6 ttl=64 time=0 ms
64 bytes from 192.168.5.5: icmp_req=7 ttl=64 time=0 ms
64 bytes from 192.168.5.5: icmp_req=8 ttl=64 time=0 ms
64 bytes from 192.168.5.5: icmp_req=9 ttl=64 time=0 ms
64 bytes from 192.168.5.5: icmp_req=10 ttl=64 time=0 ms
finished !
```

While the application is running, you can also run
```
ping 192.168.5.4
```
to send pings in the other direction.

### Investigating what happened

Run wireshark, and sniff the tap0 interface. Then run the `./main.elf` again, and see what happens. You should see an ARP request from picoTCP to Linux, and a reply. After that you should see the ping requests and replies going back and forth.

Note, sometimes you may see lots of other stuff, IPv6 router sollicitations, various broadcasts, mDNS, DNS-SD, etc - this is your when your Linux notices the new network interface is up, and starts all sorts of discoveries. With the persistent TAP device, this usually only happens the first time you start the application. Start a new wireshark capture, and start the application again, it should be much cleaner now.

Now you could make some changes to the `main.c` file, and experiment a bit! Keep some statistics of your pings (max, min, avg time). Open a UDP socket, send some stuff to a netcat instance on your linux. Or build a rudimentary port scanner, see what ports are open on your machine.


This is just a very quick overview, more info can be found in our [wiki](https://github.com/tass-belgium/picotcp/wiki).

---------------

## Contributors

Contributors are very welcome. Report a bug, suggest a way to improve our documentation, or write some new code.

Note however that, before accepting your code, we would ask you to sign our [Contributors License Agreement](https://docs.google.com/forms/d/1-z6lsT75l6ZIrgHGEWrWdHylJ6xxpjc7FwGfL2ilDFU/viewform). Your code remains under your copyright, and will always be available under GPLv2 and GPLv3. However, this CLA enables us to use picoTCP (including code from external contributors like you) under other licenses, including our commercial license. By doing commercial projects, we can keep investing in the quality and features of picoTCP.


