/*
	Created on: Sep 24, 2018

	Copyright 2018 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "types.h"
#include "net_platform.h"

#include <cstdio>
#include <cerrno>
#include <vector>

extern "C" {
#include <pico_stack.h>
#include <pico_ipv4.h>
#include <pico_dns_common.h>
#ifdef _MSC_VER
#pragma pack(pop)
#endif
}

u32 makeDnsQueryPacket(void *buf, const char *host);
pico_ip4 parseDnsResponsePacket(const void *buf, size_t len);

static sock_t sock_fd = INVALID_SOCKET;
static unsigned short qid = PICO_TIME_MS();
static int qname_len;

void get_host_by_name(const char *host, struct pico_ip4 dnsaddr)
{
	DEBUG_LOG(NETWORK, "get_host_by_name: %s", host);
    if (!VALID(sock_fd))
    {
    	sock_fd = socket(AF_INET , SOCK_DGRAM , IPPROTO_UDP);
    	set_non_blocking(sock_fd);
    }

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(53);
    dest.sin_addr.s_addr = dnsaddr.addr;

    // DNS Packet header
	char buf[1024];
	u32 len = makeDnsQueryPacket(buf, host);

    if (sendto(sock_fd, buf, len, 0, (struct sockaddr *)&dest, sizeof(dest)) < 0)
    	perror("DNS sendto failed");
}

u32 makeDnsQueryPacket(void *buf, const char *host)
{
    pico_dns_packet *dns = (pico_dns_packet *)buf;

    dns->id = qid++;
    dns->qr = PICO_DNS_QR_QUERY;
    dns->opcode = PICO_DNS_OPCODE_QUERY;
    dns->aa = PICO_DNS_AA_NO_AUTHORITY;
    dns->tc = PICO_DNS_TC_NO_TRUNCATION;
    dns->rd = PICO_DNS_RD_IS_DESIRED;
    dns->ra = PICO_DNS_RA_NO_SUPPORT;
    dns->z = 0;
    dns->rcode = PICO_DNS_RCODE_NO_ERROR;
    dns->qdcount = htons(1);	// One question
    dns->ancount = 0;
    dns->nscount = 0;
    dns->arcount = 0;

    char *qname = (char *)buf + sizeof(pico_dns_packet);

    strcpy(qname + 1, host);
    pico_dns_name_to_dns_notation(qname, 128);
	qname_len = strlen(qname) + 1;

	pico_dns_question_suffix *qinfo = (pico_dns_question_suffix *)(qname + qname_len); //fill it
    qinfo->qtype = htons(PICO_DNS_TYPE_A);		// Address record
    qinfo->qclass = htons(PICO_DNS_CLASS_IN);

    return sizeof(pico_dns_packet) + qname_len + sizeof(pico_dns_question_suffix);
}

static bool skipDnsName(const u8 *&reader, const u8 *end)
{
	if (reader == end)
		return false;
	if ((*reader & 0xC0) != 0)
	{
		if ((*reader & 0xC0) != 0xC0 || end - reader < 2)
			return false;
		reader += 2;
		return true;
	}
	while (reader != end)
	{
		u8 labelLength = *reader++;
		if (labelLength == 0)
			return true;
		if (labelLength > 63 || labelLength > end - reader)
			return false;
		reader += labelLength;
	}
	return false;
}

int get_dns_answer(struct pico_ip4 *address, struct pico_ip4 dnsaddr)
{
	struct sockaddr_in peer;
	socklen_t peer_len = sizeof(peer);
    char buf[1024];

    int r = recvfrom(sock_fd, buf, sizeof(buf), 0, (struct sockaddr*)&peer , &peer_len);
    if (r < 0)
    {
    	if (get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK)
    		perror("DNS recvfrom failed");
    	return -1;
    }
    if (peer.sin_addr.s_addr != dnsaddr.addr)
    	return -1;

    pico_ip4 addr = parseDnsResponsePacket(buf, r);
    if (addr.addr == ~0u)
    	return -1;
    address->addr = addr.addr;

    return 0;
}

pico_ip4 parseDnsResponsePacket(const void *buf, size_t len)
{
	const u8 *reader = (const u8 *)buf;
	const u8 *end = reader + len;
	if (len < sizeof(pico_dns_packet))
		return { ~0u };
	const pico_dns_packet *dns = (const pico_dns_packet *)buf;
	if (dns->qr != PICO_DNS_QR_RESPONSE || ntohs(dns->qdcount) != 1)
		return { ~0u };

	// move to the first answer
	reader += sizeof(pico_dns_packet);
	if (!skipDnsName(reader, end)
			|| (size_t)(end - reader) < sizeof(pico_dns_question_suffix))
		return { ~0u };
	reader += sizeof(pico_dns_question_suffix);

	for (int i = 0; i < ntohs(dns->ancount); i++)
	{
		if (!skipDnsName(reader, end)
				|| (size_t)(end - reader) < sizeof(pico_dns_record_suffix))
			return { ~0u };
		const pico_dns_record_suffix *record = (const pico_dns_record_suffix *)reader;
		reader += sizeof(pico_dns_record_suffix);
		u16 dataLength = ntohs(record->rdlength);
		if (dataLength > (size_t)(end - reader))
			return { ~0u };

        if (ntohs(record->rtype) == PICO_DNS_TYPE_A) // Address record
        {
			if (dataLength != sizeof(pico_ip4))
				return { ~0u };
        	pico_ip4 address;
            memcpy(&address.addr, reader, 4);

            return address;
        }
		reader += dataLength;
	}
	return { ~0u };
}

#if !defined(_WIN32) && !defined(__SWITCH__)
#include <ifaddrs.h>
#include <net/if.h>
#endif

#ifdef __ANDROID__
extern "C" {
int getifaddrs(struct ifaddrs **ifap);
void freeifaddrs(struct ifaddrs *ifa);
};
#endif

static std::vector<u32> localAddresses;

bool is_local_address(u32 addr)
{
	if (localAddresses.empty())
	{
#ifdef _WIN32
		SOCKET sd = WSASocket(AF_INET, SOCK_DGRAM, 0, 0, 0, 0);
		if (sd == INVALID_SOCKET)
		{
			WARN_LOG(NETWORK, "WSASocket failed");
			return false;
		}
		INTERFACE_INFO ifList[20];
		unsigned long size;
		if (WSAIoctl(sd, SIO_GET_INTERFACE_LIST, 0, 0, &ifList,
				sizeof(ifList), &size, 0, 0) == SOCKET_ERROR)
		{
			WARN_LOG(NETWORK, "WSAIoctl failed");
			closesocket(sd);
			return false;
		}

		int count = size / sizeof(INTERFACE_INFO);
		for (int i = 0; i < count; i++)
		{
			if ((ifList[i].iiFlags & IFF_UP) == 0)
				continue;
			if (ifList[i].iiAddress.Address.sa_family != AF_INET)
				continue;
			sockaddr_in *pAddress = (sockaddr_in *)&ifList[i].iiAddress;
			localAddresses.push_back(pAddress->sin_addr.s_addr);
		}
		closesocket(sd);

#elif defined(__SWITCH__)
		// TODO
#elif defined(__HAIKU__)
		// TODO
#else // !_WIN32 && !__SWITCH__ && !__HAIKU__

		ifaddrs *myaddrs;
		if (getifaddrs(&myaddrs) != 0)
		{
			WARN_LOG(NETWORK, "getifaddrs failed");
			return false;
		}
		for (ifaddrs *ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next)
		{
			if (ifa->ifa_addr == NULL)
				continue;
			if (!(ifa->ifa_flags & IFF_UP))
				continue;
			if (ifa->ifa_addr->sa_family != AF_INET)
				continue;

			sockaddr_in *sa = (sockaddr_in *)ifa->ifa_addr;
			localAddresses.push_back(sa->sin_addr.s_addr);
		}
		freeifaddrs(myaddrs);
#endif
	}

	for (auto a : localAddresses)
		if (a == addr)
			return true;

	return false;
}
