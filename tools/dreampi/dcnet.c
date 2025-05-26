/*
	Copyright 2025 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <termios.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdlib.h>
#include <poll.h>

#define DEFAULT_TTY "/dev/ttyACM0"
#define DEFAULT_HOST "dcnet.flyca.st"
#define DEFAULT_PORT 7654

char ttyName[512] = DEFAULT_TTY;
char hostName[64] = DEFAULT_HOST;
uint16_t port = DEFAULT_PORT;
struct termios tbufsave;

int setNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		flags = 0;
	flags |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) != 0) {
		perror("fcntl(O_NONBLOCK)");
		return -1;
	}
	return 0;
}

int configureTty(int fd, int local)
{
	if (tcgetattr(fd, &tbufsave) == -1)
	        perror("tcgetattr");
	struct termios tbuf;
	memcpy(&tbuf, &tbufsave, sizeof(tbuf));
	/* 8-bit, one stop bit, no parity, carrier detect, no hang up on close,
	   disable RTS/CTS flow control.
	 */
	tbuf.c_cflag &= ~(CSIZE | CSTOPB | PARENB | CLOCAL | HUPCL | CRTSCTS);
	tbuf.c_cflag |= CS8 | CREAD;
	if (local)
		// ignore CD
		tbuf.c_cflag |= CLOCAL;

	/* don't translate NL to CR or CR to NL on input, get all 8 bits of input
	    disable xon/xoff flow control on output, no interrupt on break signal,
	    ignore parity, ignore break
	 */
        tbuf.c_iflag = IGNBRK | IGNPAR;
        /* disable all output processing */
        tbuf.c_oflag = 0;
        /* non-canonical, ignore signals and no echoing on output */
        tbuf.c_lflag = 0;
        tbuf.c_cc[VMIN] = 1;
        tbuf.c_cc[VTIME] = 0;
        /* set the parameters associated with the terminal port */
	if (tcsetattr(fd, TCSANOW, &tbuf) == -1) {
		perror("tcsetattr");
		return 1;
	}
	return 0;
}

struct sockaddr_in *resolve(const char *hostName)
{
	static struct sockaddr_in resolved;
	struct addrinfo hints, *result;
	memset(&hints, 0, sizeof (hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	int errcode = getaddrinfo(hostName, NULL, &hints, &result);
	if (errcode != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(errcode));
		return NULL;
	}
	if (result == NULL) {
		fprintf(stderr, "%s: host not found\n", hostName);
		return NULL;
	}
	memcpy(&resolved, (struct sockaddr_in *)result->ai_addr, sizeof(resolved));
	freeaddrinfo(result);
	return &resolved;
}

const uint32_t MAGIC = 0xDC15C001;

typedef struct
{
	struct sockaddr_in address;
	char name[256];
	int ping;
	int count;
} AccessPoint;
AccessPoint accessPoints[16];
int apCount;

uint64_t getTimeMs()
{
	struct timeval start;
	gettimeofday(&start, NULL);
	return (uint64_t)start.tv_sec * 1000 + (uint64_t)start.tv_usec / 1000;
}

void sendPing(int sock, const struct sockaddr_in *dest)
{
	uint8_t buf[13];
	memcpy(&buf[0], &MAGIC, sizeof(MAGIC));
	buf[4] = 1; // ping
	uint64_t now = getTimeMs();
	memcpy(&buf[5], &now, sizeof(uint64_t));
	sendto(sock, buf, sizeof(buf), 0, (const struct sockaddr *)dest, sizeof(*dest));
}

struct sockaddr_in *findBestAccessPoint()
{
	struct sockaddr_in *discoaddr = resolve("dcnet.flyca.st");
	if (discoaddr == NULL)
		return NULL;
	discoaddr->sin_port = htons(7655);
	// create UDP socket
	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		perror("socket");
		return NULL;
	}
	// bind it
	struct sockaddr_in serveraddr = {};
	serveraddr.sin_family = AF_INET;
	if (bind(sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
	{
		perror("bind");
		close(sock);
		return NULL;
	}
	// set non-blocking
	fcntl(sock, F_SETFL, O_NONBLOCK);

	// discover access points
	uint8_t buf[512];
	memcpy(&buf[0], &MAGIC, sizeof(MAGIC));
	buf[4] = 3;	// discover access points
	if (sendto(sock, buf, 5, 0, (const struct sockaddr *)discoaddr, sizeof(struct sockaddr_in)) < 5)
	{
		perror("sendto");
		close(sock);
		return NULL;
	}

	uint64_t start = getTimeMs();
	int timeout = 500;
	int done = 0;

	while (!done)
	{
		struct pollfd pfd = { sock, POLLIN };
		int rc = poll(&pfd, 1, timeout);
		if (rc < 0) {
			perror("poll");
			break;
		}
		if (pfd.revents & (POLLERR|POLLHUP|POLLNVAL))
			// error
			break;

		const uint64_t now = getTimeMs();
		timeout = (int)(start + 1000u - now);
		if (timeout <= 0)
			// done
			break;
		if (timeout < 500)
		{
			// Re-ping access points that didn't answer after 500 ms
			for (int i = 0; i < apCount; i++) {
				if (accessPoints[i].count == 0)
					sendPing(sock, &accessPoints[i].address);
			}
		}
		else {
			timeout -= 500;
		}
		if (rc == 0)
			continue;

		struct sockaddr_in peeraddr;
		socklen_t sz = sizeof(peeraddr);
		ssize_t recvlen = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&peeraddr, &sz);
		if (recvlen <= 0)
		{
			if (recvlen < 0)
				perror("recvfrom");
			break;
		}
		if (recvlen < 5 || memcmp(&buf[0], &MAGIC, sizeof(MAGIC))) {
			fprintf(stderr, "Invalid packet received (size %zd)\n", recvlen);
			continue;
		}
		switch (buf[4])
		{
		case 2:	// pong
			if (recvlen != 13) {
				fprintf(stderr, "Invalid ping received (size %zd)\n", recvlen);
				break;
			}
			for (int i = 0; i < apCount; i++)
			{
				if (accessPoints[i].address.sin_addr.s_addr == peeraddr.sin_addr.s_addr)
				{
					uint64_t sent;
					memcpy(&sent, &buf[5], sizeof(sent));
					accessPoints[i].ping += (int)(now - sent);
					accessPoints[i].count++;
					if (accessPoints[i].count == 3)
						done = 1;
					else
						sendPing(sock, &accessPoints[i].address);
					break;
				}
			}
			break;
		case 3: // discover
			{
				apCount = 0;
				const uint8_t *p = &buf[5];
				while (p - &buf[0] < (int)recvlen)
				{
					accessPoints[apCount].address.sin_family = AF_INET;
					accessPoints[apCount].address.sin_port = htons(7655);
					memcpy(&accessPoints[apCount].address.sin_addr.s_addr, p, sizeof(uint32_t));
					p += 4;
					size_t l = *p++;
					memcpy(accessPoints[apCount].name, p, l);
					accessPoints[apCount].name[l] = '\0';
					p += l;
					apCount++;
				}
				if (apCount == 1) {
					done = 1;
				}
				else {
					for (int i = 0; i < apCount; i++)
						sendPing(sock, &accessPoints[i].address);
				}
				break;
			}
		}
	}
	close(sock);
	int bestAP = 0;
	int bestPing = 1000000;
	for (int i = 0; i < apCount; i++)
	{
		const AccessPoint *ap = &accessPoints[i];
		if (ap->count != 0)
		{
			int ping = ap->ping / ap->count;
			printf("%s: %d ms\n", ap->name, ping);
			if (ping < bestPing) {
				bestAP = i;
				bestPing = ping;
			}
		}
		else {
			printf("%s: no answer\n", ap->name);
		}
	}
	return bestAP < apCount ? &accessPoints[bestAP].address : NULL;
}

void usage(const char *arg0)
{
	fprintf(stderr, "Usage: %s [-t <tty>] [-h <server name] [-p <server port>]\n", arg0);
	fprintf(stderr, "Default tty is %s. Default host:port is %s:%d\n", DEFAULT_TTY, DEFAULT_HOST, DEFAULT_PORT);
	exit(1);
}

int main(int argc, char *argv[])
{
	int forceHost = 0;
	int opt;
	while ((opt = getopt(argc, argv, "t:h:p:")) != -1)
	{
		switch (opt)
		{
		case 't':
			strcpy(ttyName, optarg);
			break;
		case 'h':
			forceHost = 1;
			strcpy(hostName, optarg);
			break;
		case 'p':
			port = (uint16_t)atoi(optarg);
			break;
		default:
			usage(argv[0]);
		}
	}
	if (optind != argc)
		usage(argv[0]);

	fprintf(stderr, "DCNet starting\n");
	struct sockaddr_in *serverAddress = NULL;
	if (!forceHost)
		serverAddress = findBestAccessPoint();

	if (serverAddress == NULL)
	{
		serverAddress = resolve(hostName);
		if (serverAddress == NULL)
			return -1;
		char s[100];
		inet_ntop(AF_INET, &serverAddress->sin_addr, s, 100);
		printf("%s is %s\n", hostName, s);
	}

	/* TTY */
	int ttyfd = open(ttyName, O_RDWR);
	if (ttyfd == -1) {
		perror("Can't open tty");
		return 1;
	}
	if (configureTty(ttyfd, 0))
		return 1;

	/* SOCKET */
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		fprintf(stderr, "socket() failed: %d\n", errno);
		return 1;
	}
	serverAddress->sin_port = htons(port);
	if (connect(sockfd, (struct sockaddr *)serverAddress, sizeof(*serverAddress)) != 0) {
		fprintf(stderr, "connect() failed: %d\n", errno);
		return 1;
	}

	if (setNonBlocking(sockfd) || setNonBlocking(ttyfd))
		return -1;

	char outbuf[1504];
	ssize_t outbuflen = 0;
	char inbuf[1504];
	ssize_t inbuflen = 0;
	for (;;)
	{
#ifdef DEBUG
		ssize_t old_olen = outbuflen;
		ssize_t old_ilen = inbuflen;
#endif
		fd_set readfds;
		FD_ZERO(&readfds);
		if (inbuflen < sizeof(inbuf))
			FD_SET(sockfd, &readfds);
		if (outbuflen < sizeof(outbuf))
			FD_SET(ttyfd, &readfds);

		fd_set writefds;
		FD_ZERO(&writefds);
		ssize_t outbufReady = 0;
		if (outbuflen > 0)
		{
			outbufReady = outbuflen;
			/*
			if (outbuf[0] != 0x7e) {
				outbufReady = outbuflen;
			}
			else {
				for (int i = 1; i < outbuflen; i++)
				{
					if (outbuf[i] == 0x7e) {
						outbufReady = i + 1;
						break;
					}
				}
			}
			*/
			if (outbufReady != 0)
				FD_SET(sockfd, &writefds);
		}
		if (inbuflen > 0)
			FD_SET(ttyfd, &writefds);
		int nfds = (sockfd > ttyfd ? sockfd : ttyfd) + 1;
		if (select(nfds, &readfds, &writefds, NULL, NULL) == -1)
		{
			if (errno == EINTR)
				continue;
			fprintf(stderr, "select() failed: %d\n", errno);
			close(sockfd);
			return 1;
		}
		if (FD_ISSET(ttyfd, &readfds))
		{
			ssize_t ret = read(ttyfd, outbuf + outbuflen, sizeof(outbuf) - (size_t)outbuflen); 
			if (ret < 0) {
				if (errno != EINTR && errno != EWOULDBLOCK) {
					fprintf(stderr, "read from tty failed: %d\n", errno);
					break;
				}
				ret = 0;
			}
			else if (ret == 0) {
				fprintf(stderr, "modem hang up\n");
				break;
			}
			if (ret > 0) {
				outbuflen += ret;
				FD_SET(sockfd, &writefds);
			}
		}
		if (FD_ISSET(sockfd, &readfds))
		{
			ssize_t ret = read(sockfd, inbuf + inbuflen, sizeof(inbuf) - (size_t)inbuflen); 
			if (ret < 0) {
				if (errno != EINTR && errno != EWOULDBLOCK) {
					fprintf(stderr, "read from socket failed: %d\n", errno);
					break;
				}
				ret = 0;
			}
			else if (ret == 0) {
				fprintf(stderr, "socket read EOF\n");
				break;
			}
			if (ret > 0) {
				inbuflen += ret;
				FD_SET(ttyfd, &writefds);
			}
		}
		if (FD_ISSET(ttyfd, &writefds))
		{
			ssize_t ret = write(ttyfd, inbuf, (size_t)inbuflen);
			if (ret < 0) {
				if (errno != EINTR && errno != EWOULDBLOCK) {
					fprintf(stderr, "write to tty failed: %d\n", errno);
					break;
				}
				ret = 0;
			}
			if (ret > 0)
			{
				inbuflen -= ret;
				if (inbuflen > 0)
					memmove(inbuf, inbuf + ret, (size_t)inbuflen);
			}
		}
		if (FD_ISSET(sockfd, &writefds))
		{
			ssize_t ret = write(sockfd, outbuf, (size_t)outbufReady);
			if (ret < 0) {
				if (errno == EINTR && errno != EWOULDBLOCK) {
					fprintf(stderr, "write to socket failed: %d\n", errno);
					break;
				}
				ret = 0;
			}
			if (ret > 0)
			{
				outbuflen -= ret;
				if (outbuflen > 0)
					memmove(outbuf, outbuf + ret, (size_t)outbuflen);
			}
		}
#ifdef DEBUG
		printf("OUT %03zd%c IN %03zd%c\r",
			outbuflen, outbuflen > old_olen ? '+' : outbuflen < old_olen ? '-' : ' ',
			inbuflen, inbuflen > old_ilen ? '+' : inbuflen < old_ilen ? '-' : ' ');
		fflush(stdout);
#endif
	}
	close(ttyfd);
	ttyfd = open(ttyName, O_RDWR);
	if (ttyfd == -1) {
		perror("Can't reopen tty");
	}
	else {
		tcsetattr(ttyfd, TCSANOW, &tbufsave);
		close(ttyfd);
	}
	close(sockfd);
	fprintf(stderr, "DCNet stopped\n");
	return 0;
}

