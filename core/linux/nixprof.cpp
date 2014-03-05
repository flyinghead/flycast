#include "types.h"
#include "cfg/cfg.h"


#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <memory.h>
#include <signal.h>
//#include <sys/ucontext.h>
#include <stdio.h>
#include <signal.h>
//#include <execinfo.h>
#include <sys/syscall.h>
#include <sys/stat.h>

#include <poll.h>
#include <termios.h>
//#include <curses.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/time.h>
#include "hw/sh4/dyna/blockmanager.h"
#include <set>
#include "deps/libelf/elf.h"

#if defined(_ANDROID)
#include <asm/sigcontext.h>

typedef struct ucontext_t
{
	unsigned long uc_flags;
	struct ucontext_t *uc_link;
	struct
	{
		void *p;
		int flags;
		size_t size;
	} sstack_data;

	struct sigcontext uc_mcontext;
	/* some 2.6.x kernel has fp data here after a few other fields
	 * we don't use them for now...
	 */
} ucontext_t;

#include <android/log.h> 
#endif

#if HOST_CPU == CPU_ARM
#define GET_PC_FROM_CONTEXT(c) (((ucontext_t *)(c))->uc_mcontext.arm_pc)
#elif HOST_CPU == CPU_MIPS
#ifdef _ANDROID
#define GET_PC_FROM_CONTEXT(c) (((ucontext_t *)(c))->uc_mcontext.sc_pc)
#else
#define GET_PC_FROM_CONTEXT(c) (((ucontext_t *)(c))->uc_mcontext.pc)
#endif
#elif HOST_CPU == CPU_X86
#define GET_PC_FROM_CONTEXT(c) (((ucontext_t *)(c))->uc_mcontext.eip)
#else
#error fix ->pc support
#endif

/** 
@file      CallStack_Android.h 
@brief     Getting the callstack under Android 
@author    Peter Holtwick 
*/ 
#include <unwind.h> 
#include <stdio.h> 
#include <string.h> 

int tick_count=0;
pthread_t proft;
pthread_t thread[2];
void*     prof_address[2];
u32 prof_wait;

u8* syms_ptr;
int syms_len;

void sample_Syms(u8* data,u32 len)
{
	syms_ptr=data;
	syms_len=len;
}

void prof_handler (int sn, siginfo_t * si, void *ctxr)
{
	ucontext_t* ctx=(ucontext_t*)ctxr;
	int thd=-1;
	if (pthread_self()==thread[0]) thd=0;
	else if (pthread_self()==thread[1]) thd=1;
	else return;

	prof_address[thd]=(void*)GET_PC_FROM_CONTEXT(ctx);
}



void install_prof_handler(int id)
{
	struct sigaction act, segv_oact;
	memset(&act, 0, sizeof(act));
	act.sa_sigaction = prof_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO | SA_RESTART;
	sigaction(SIGPROF, &act, &segv_oact);

	thread[id]=pthread_self();
}

void prof_head(FILE* out, const char* type, const char* name)
{
	fprintf(out,"==xx==xx==\n%s:%s\n",type,name);
}
void prof_head(FILE* out, const char* type, int d)
{
	fprintf(out,"==xx==xx==\n%s:%d\n",type,d);
}

void elf_syms(FILE* out,const char* libfile)
{
	struct stat statbuf;

	printf("LIBFILE \"%s\"\n", libfile);
	int fd = open(libfile, O_RDONLY, 0);

	if (!fd)
	{
		printf("Failed to open file \"%s\"\n", libfile);
		return;
	}
	if (fstat(fd, &statbuf) < 0)
	{
		printf("Failed to fstat file \"%s\"\n", libfile);
		return;
	}

	{
		void* data = mmap(0, statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
		close(fd);

		if (data == (void*)-1)
		{
			printf("Failed to mmap file \"%s\"\n", libfile);
			return;
		}

		//printf("MMap: %08p, %08X\n",data,statbuf.st_size);

		int dynsym=-1;
		int dynstr=-1;

		/*
			Section: 2 -> .dynsym
			Section: 3 -> .dynstr
		*/
		if (elf_checkFile(data)>=0)
		{
			int scnt=elf_getNumSections(data);

			for (int si=0;si<scnt;si++)
			{
				if (strcmp(".dynsym",elf_getSectionName(data,si))==0)
					dynsym=si;

			
				if (strcmp(".dynstr",elf_getSectionName(data,si))==0)
					dynstr=si;
			}
		}
		else
		{
			printf("Invalid elf file\n");
		}

		if (dynsym >= 0)
		{
			prof_head(out,"libsym",libfile);
		//	printf("Found dymsym %d, and dynstr %d!\n",dynsym,dynstr);
			elf_symbol* sym=(elf_symbol*)elf_getSection(data,dynsym);
			int symcnt=elf_getSectionSize(data,dynsym)/sizeof(elf_symbol);

			for (int i=0;i<symcnt;i++)
			{
				if (sym[i].st_value && sym[i].st_name && sym[i].st_shndx)
				{
					char* name=(char*)elf_getSection(data,dynstr);// sym[i].st_shndx

				//	printf("Symbol %d: %s, %08X, %d bytes\n",i,name+sym[i].st_name,sym[i].st_value,sym[i].st_size);
					fprintf(out,"%08X %d %s\n",sym[i].st_value,sym[i].st_size,name+sym[i].st_name);
				}
			}
		}
		else
		{
			printf("No dynsym\n");
		}

		munmap(data,statbuf.st_size);
	}
}



volatile bool prof_run;

int str_ends_with(const char * str, const char * suffix)
{
	if (str == NULL || suffix == NULL)
		return 0;

	size_t str_len = strlen(str);
	size_t suffix_len = strlen(suffix);

	if (suffix_len > str_len)
		return 0;

	return 0 == strncmp(str + str_len - suffix_len, suffix, suffix_len);
}

void sh4_jitsym(FILE* out);

void* prof(void *ptr)
{
	FILE* prof_out;
	char line[512];

	sprintf(line, "/%d.reprof", tick_count);

		string logfile=GetPath(line);


		printf("Profiler thread logging to -> %s\n", logfile.c_str());

		prof_out = fopen(logfile.c_str(), "wb");
		if (!prof_out)
		{
			printf("Failed to open profiler file\n");
			return 0;
		}

		set<string> libs;

		prof_head(prof_out, "vaddr", "");
		FILE* maps = fopen("/proc/self/maps", "r");
		while (!feof(maps))
		{
			fgets(line, 512, maps);
			fputs(line, prof_out);

			if (strstr(line, ".so"))
			{
				char file[512];
				file[0] = 0;
				sscanf(line, "%*x-%*x %*s %*x %*x:%*x %*d %s\n", file);
				if (strlen(file))
					libs.insert(file);
			}
		}

		//Write map file
		prof_head(prof_out, ".map", "");
		fwrite(syms_ptr, 1, syms_len, prof_out);

		//write exports from .so's
		for (set<string>::iterator it = libs.begin(); it != libs.end(); it++)
		{
			elf_syms(prof_out, it->c_str());
		}

		//Write shrec syms file !
		prof_head(prof_out, "jitsym", "SH4");
		
		#if !defined(HOST_NO_REC)
		sh4_jitsym(prof_out);
		#endif

		//Write arm7rec syms file ! -> to do
		//prof_head(prof_out,"jitsym","ARM7");

		prof_head(prof_out, "samples", prof_wait);

		do
		{
			tick_count++;
			//printf("Sending SIGPROF %08X %08X\n",thread[0],thread[1]);
			for (int i = 0; i < 2; i++) pthread_kill(thread[i], SIGPROF);
			//printf("Sent SIGPROF\n");
			usleep(prof_wait);
			//fwrite(&prof_address[0],1,sizeof(prof_address[0])*2,prof_out);
			fprintf(prof_out, "%p %p\n", prof_address[0], prof_address[1]);

			if (!(tick_count % 10000))
			{
				printf("Profiler: %d ticks, flusing ..\n", tick_count);
				fflush(prof_out);
			}
		} while (prof_run);

		fclose(maps);
		fclose(prof_out);
}

void sample_Start(int freq)
{
	prof_wait = 1000000 / freq;
	printf("sampling profiler: starting %d hz %d wait\n", freq, prof_wait);
	prof_run = true;
	pthread_create(&proft, NULL, prof, 0);
}

void sample_Stop()
{
	if (prof_run)
	{
		prof_run = false;
		pthread_join(proft, NULL);
	}
	printf("sampling profiler: stopped\n");
}
