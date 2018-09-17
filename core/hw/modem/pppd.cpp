/*
	pppd.cpp

	Created on: Sep 10, 2018

	Copyright 2018 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "types.h"

#if HOST_OS == OS_LINUX

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#include <bitset>
#include <string>
#include "oslib/oslib.h"
#include "pppd.h"

static int pppd_pid;
static int inpipe = -1;
static int outpipe = -1;

static u8 in_buffer[128];
static int in_bufsize;
static int in_bufindex;
static bool v42_negotiate = false;
static u32 v42_odp_idx;
static u32 v42_odp_count;
const u8 v42_odp[] { 0x45, 0xfc, 0x17, 0xf9, 0x5f, 0xc4, 0x7f, 0x91, 0xff };
double last_adp_sent;

void start_pppd()
{
	int inpipefd[2];
	int outpipefd[2];

	if (pipe(inpipefd) || pipe(outpipefd))
	{
		perror("pipe");
		return;
	}
	pppd_pid = fork();
	if (pppd_pid < 0)
	{
		perror("fork");
		close(inpipefd[0]);
		close(inpipefd[1]);
		close(outpipefd[0]);
		close(outpipefd[1]);
		return;
	}

	if (pppd_pid == 0)
	{
		// Child
		close(inpipefd[0]);
		dup2(inpipefd[1], 1);
		close(outpipefd[1]);
		dup2(outpipefd[0], 0);
		execl("/usr/sbin/pppd", "pppd", "notty", NULL);
		perror("execl");
		exit(1);
	}
	// Parent
	inpipe = inpipefd[0];
	outpipe = outpipefd[1];
	fcntl(inpipe, F_SETFL, fcntl(inpipe, F_GETFL) | O_NONBLOCK);
	in_bufindex = 0;
	in_bufsize = 1;
	in_buffer[0] = 0;
}

void stop_pppd()
{
	if (pppd_pid > 0)
	{
		kill(SIGTERM, pppd_pid);
		// FIXME wait / waitpid
		pppd_pid = 0;
	}
	if (inpipe >= 0)
	{
		close(inpipe);
		inpipe = -1;
	}
	if (outpipe >= 0)
	{
		close(outpipe);
		outpipe = -1;
	}
}

static u8 reverse_bits(u8 x)
{
	x = ((x * 0x0802 & 0x22110) | (x * 0x8020 & 0x88440)) * 0x10101 >> 16;

	return x & 0xFF;
}

static void send_v42_adp()
{
	in_bufsize = 0;
	in_bufindex = 0;

	//  V.42 disabled E, 8-16 ones, NULL, 8-16 ones // FIXME leading one is a test
	const std::string adp_nov42("1" "0101000101" "11111111" "0000000001" "11111111");
	const std::string adp_v42("1" "0101000101" "11111111" "0110000101" "11111111");
	const std::string& adp = adp_nov42;
	const std::string ones("1111111111111111");

	int adp_idx = 0;
	for (int i = 0; i < 10; )
	{
		int end = min(adp_idx + 8, (int)adp.length());
		std::string binary = adp.substr(adp_idx, end - adp_idx);
		adp_idx += 8;

		if (adp_idx > end)
		{
			adp_idx -= adp.length();
			if (i == 9)
				binary += ones.substr(0, adp_idx);
			else
				binary += adp.substr(0, adp_idx);
			i++;
		}
		else
		{
			if (adp_idx >= adp.length())
			{
				adp_idx = 0;
				i++;
			}
		}
		std::bitset<8> bs(binary);
		in_buffer[in_bufsize++] = reverse_bits(bs.to_ulong());
		if (in_bufsize == sizeof(in_buffer))
		{
			printf("PPPD in buffer overflow\n");
			return;
		}
	}
	v42_negotiate = false;
}

void write_pppd(u8 b)
{
	if (v42_negotiate)
	{
		if (b == v42_odp[v42_odp_idx])
		{
			v42_odp_idx++;
			if (v42_odp_idx >= sizeof(v42_odp))
			{
				v42_odp_count++;
				if (v42_odp_count >= 2)
				{
					if (last_adp_sent == 0 || os_GetSeconds() - last_adp_sent > 0.75)	// T400
					{
						printf("PPPD V42 2 ODP received. Sending ADP\n");
						last_adp_sent = os_GetSeconds();
						send_v42_adp();
					}
					v42_odp_count = 0;
				}
				v42_odp_idx = 0;
			}
		}
		else
		{
			v42_odp_idx = 0;
			printf("PPPD ignored byte %02x\n", b);
		}
		return;
	}
	int rc = write(outpipe, &b, 1);
	if (rc < 0)
		perror("write outpipe");
	else if (rc == 0)
		printf("pppd EOF on outpipe\n");
}

int read_pppd()
{
	if (in_bufindex == in_bufsize)
	{
		in_bufindex = 0;
		in_bufsize = 0;
		if (v42_negotiate)
			return -1;

		int rc = read(inpipe, in_buffer, sizeof(in_buffer));
		if (rc < 0)
		{
			if (errno != EWOULDBLOCK)
				perror("read inpipe");
			return rc;
		}
		else if (rc == 0)
		{
			printf("pppd EOF on inpipe\n");
			return -1;
		}
		in_bufsize = rc;
		in_bufindex = 0;
	}
	return in_buffer[in_bufindex++];
}

int avail_pppd()
{
	return in_bufsize - in_bufindex;
}

#else	// not Linux

void start_pppd()
{
}

void stop_pppd()
{
}

void write_pppd(u8 b)
{
}

int read_pppd()
{
	return -1;
}

int avail_pppd() {
	return 0;
}

#endif

