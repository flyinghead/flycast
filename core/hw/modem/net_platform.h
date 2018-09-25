#ifndef _NET_PLATFORM_H
#define _NET_PLATFORM_H

#ifndef _WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#else
#include <ws2tcpip.h>
#endif

#ifndef _WIN32
#define closesocket close
typedef int sock_t;
#define VALID(s) ((s) >= 0)
#define L_EWOULDBLOCK EWOULDBLOCK
#define L_EAGAIN EAGAIN
#define get_last_error() (errno)
#define INVALID_SOCKET (-1)
#else
typedef SOCKET sock_t;
#define VALID(s) ((s) != INVALID_SOCKET)
#define L_EWOULDBLOCK WSAEWOULDBLOCK
#define L_EAGAIN WSAEWOULDBLOCK
#define get_last_error() (WSAGetLastError())
#define perror(s) do { if (s) printf("%s: ", s); printf("Winsock error: %d\n", WSAGetLastError()); } while (false)
#endif

#endif
