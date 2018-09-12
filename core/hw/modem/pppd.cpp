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
#include "pppd.h"

static int pppd_pid;
static int inpipe = -1;
static int outpipe = -1;

static u8 in_buffer[128];
static int in_bufsize;
static int in_bufindex;

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

void write_pppd(u8 b)
{
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

