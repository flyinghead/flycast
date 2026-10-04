#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <ifaddrs.h>
#include <switch.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/inet.h>

// Seems to be missing in newlib, dumb stub (file permissions is not a thing on fat32 anyways)
mode_t umask(mode_t mask)
{
    return mask;
}

int pause()
{
	sleep(0xffffffff);
	return -1;
}

// FIXME always failing stub
int pthread_sigmask(int how, const sigset_t *set, sigset_t *oset)
{
  switch (how)
    {
    case SIG_BLOCK:
    case SIG_UNBLOCK:
    case SIG_SETMASK:
      break;
    default:
      errno = EINVAL;
      return -1;
    }
  errno = ENOSYS;
  return -1;
}

// Map an interface index into its name.
char *if_indextoname(unsigned ifindex, char *ifname)
{
	errno = ENXIO;
	return NULL;
}

int getifaddrs(struct ifaddrs **ifap)
{
	uint32_t addr, subnet, gateway, dns1, dns2;
	Result r = nifmGetCurrentIpConfigInfo(&addr, &subnet, &gateway, &dns1, &dns2);
	if (R_FAILED(r)) {
		errno = EIO; // not sure what to use here
		return -1;
	}

	struct ifaddrs *ifa = (struct ifaddrs *)malloc(sizeof(struct ifaddrs));
	if (ifa == NULL) {
		errno = ENOMEM;
		return -1;
	}
	memset(ifa, 0, sizeof(*ifa));
	bool ethon = false;
	nifmIsEthernetCommunicationEnabled(&ethon);
	if (ethon)
		ifa->ifa_name = "en0";
	else
		ifa->ifa_name = "wifi0";
	ifa->ifa_flags = IFF_UP | IFF_BROADCAST;

	struct sockaddr_in *ifa_addr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
	if (ifa_addr == NULL) {
		free(ifa);
		errno = ENOMEM;
		return -1;
	}
	ifa_addr->sin_family = AF_INET;
	ifa_addr->sin_port = 0;
	ifa_addr->sin_addr.s_addr = addr;
	ifa->ifa_addr = (struct sockaddr *)ifa_addr;

	struct sockaddr_in *ifa_netmask = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
	if (ifa_netmask == NULL)
	{
		free(ifa_addr);
		free(ifa);
		errno = ENOMEM;
		return -1;
	}
	ifa_netmask->sin_family = AF_INET;
	ifa_netmask->sin_port = 0;
	ifa_netmask->sin_addr.s_addr = subnet;
	ifa->ifa_netmask = (struct sockaddr *)ifa_netmask;

	struct sockaddr_in *broadaddr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
	if (broadaddr == NULL)
	{
		free(ifa_netmask);
		free(ifa_addr);
		free(ifa);
		errno = ENOMEM;
		return -1;
	}
	broadaddr->sin_family = AF_INET;
	broadaddr->sin_port = 0;
	broadaddr->sin_addr.s_addr = addr | (0xffffffff & ~subnet);
	ifa->ifa_broadaddr = (struct sockaddr *)broadaddr;

	*ifap = ifa;

	return 0;
}

void freeifaddrs(struct ifaddrs *ifa)
{
	if (ifa == NULL)
		return;
	free(ifa->ifa_addr);
	free(ifa->ifa_netmask);
	free(ifa->ifa_broadaddr);
	free(ifa);
}

#undef getnameinfo

int getnameinfo_fixed(const struct sockaddr *sa, socklen_t salen,
                char *host, socklen_t hostlen,
                char *serv, socklen_t servlen,
                int flags)
{
	if (flags & (NI_NUMERICHOST | NI_NUMERICSERV))
	{
		if (host != NULL)
			inet_ntop(AF_INET, &((const struct sockaddr_in *)sa)->sin_addr.s_addr, host, hostlen);
		if (serv != NULL && servlen != 0)
			*serv = '\0';
		return 0;
	}
	return getnameinfo(sa, salen, host, hostlen, serv, servlen, flags);
}
