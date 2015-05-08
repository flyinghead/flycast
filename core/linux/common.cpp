#include "types.h"
#include "cfg/cfg.h"

#if HOST_OS==OS_LINUX
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
#include <unistd.h>
#include "hw/sh4/dyna/blockmanager.h"

#if defined(_ANDROID)
#include <asm/sigcontext.h>
#if 0
typedef struct ucontext_t {
unsigned long uc_flags;
struct ucontext_t *uc_link;
struct {
void *p;
int flags;
size_t size;
} sstack_data;
struct sigcontext uc_mcontext;
/* some 2.6.x kernel has fp data here after a few other fields
* we don't use them for now...
*/
} ucontext_t;
#endif
#endif

#if HOST_CPU == CPU_ARM
#define GET_PC_FROM_CONTEXT(c) (((ucontext_t *)(c))->uc_mcontext.arm_pc)
#elif HOST_CPU == CPU_MIPS
#if 0 && _ANDROID
#define GET_PC_FROM_CONTEXT(c) (((ucontext_t *)(c))->uc_mcontext.sc_pc)
#else
#define GET_PC_FROM_CONTEXT(c) (((ucontext_t *)(c))->uc_mcontext.pc)
#endif
#elif HOST_CPU == CPU_X86
#if 0 && _ANDROID
#define GET_PC_FROM_CONTEXT(c) (((ucontext_t *)(c))->uc_mcontext.eip)
#else
#define GET_PC_FROM_CONTEXT(c) (((ucontext_t *)(c))->uc_mcontext.gregs[REG_EIP])
#define GET_ESP_FROM_CONTEXT(c) (((ucontext_t *)(c))->uc_mcontext.gregs[REG_ESP])
#define GET_EAX_FROM_CONTEXT(c) (((ucontext_t *)(c))->uc_mcontext.gregs[REG_EAX])
#define GET_ECX_FROM_CONTEXT(c) (((ucontext_t *)(c))->uc_mcontext.gregs[REG_ECX])
#endif
#else
#error fix ->pc support
#endif

#include "hw/sh4/dyna/ngen.h"

bool ngen_Rewrite(unat& addr,unat retadr,unat acc);
u32* ngen_readm_fail_v2(u32* ptr,u32* regs,u32 saddr);
bool VramLockedWrite(u8* address);
bool BM_LockedWrite(u8* address);

void fault_handler (int sn, siginfo_t * si, void *ctxr)
{
	bool dyna_cde=((u32)GET_PC_FROM_CONTEXT(ctxr)>(u32)CodeCache) && ((u32)GET_PC_FROM_CONTEXT(ctxr)<(u32)(CodeCache+CODE_SIZE));

	ucontext_t* ctx=(ucontext_t*)ctxr;
	//printf("mprot hit @ ptr 0x%08X @@ code: %08X, %d\n",si->si_addr,ctx->uc_mcontext.arm_pc,dyna_cde);

	
	if (VramLockedWrite((u8*)si->si_addr) || BM_LockedWrite((u8*)si->si_addr))
		return;
	#if !defined(HOST_NO_REC)
		#if HOST_CPU==CPU_ARM
			else if (dyna_cde)
			{
				GET_PC_FROM_CONTEXT(ctxr)=(u32)ngen_readm_fail_v2((u32*)GET_PC_FROM_CONTEXT(ctxr),(u32*)&(ctx->uc_mcontext.arm_r0),(unat)si->si_addr);
			}
		#elif HOST_CPU==CPU_X86
			else if ( ngen_Rewrite((unat&)GET_PC_FROM_CONTEXT(ctxr),*(unat*)GET_ESP_FROM_CONTEXT(ctxr),GET_EAX_FROM_CONTEXT(ctxr)) )
			{
				//remove the call from call stack
				GET_ESP_FROM_CONTEXT(ctxr)+=4;
				//restore the addr from eax to ecx so its valid again
				GET_ECX_FROM_CONTEXT(ctxr)=GET_EAX_FROM_CONTEXT(ctxr);
			}
		#else
			#error JIT: Not supported arch
		#endif
	#endif
	else
	{
		printf("SIGSEGV @ fault_handler+0x%08X ... %08X -> was not in vram\n",GET_PC_FROM_CONTEXT(ctxr)-(u32)fault_handler,si->si_addr);
		die("segfault");
//		asm volatile("bkpt 0x0001\n\t");
		signal(SIGSEGV, SIG_DFL);
	}
}

void install_fault_handler (void)
{
	struct sigaction act, segv_oact;
	memset(&act, 0, sizeof(act));
	act.sa_sigaction = fault_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;
	sigaction(SIGSEGV, &act, &segv_oact);
}


//Thread class
cThread::cThread(ThreadEntryFP* function,void* prm)
{
	Entry=function;
	param=prm;
}

void cThread::Start()
{
		pthread_create( (pthread_t*)&hThread, NULL, Entry, param);
}

void cThread::WaitToEnd()
{
	pthread_join((pthread_t)hThread,0);
}

//End thread class

//cResetEvent Calss
cResetEvent::cResetEvent(bool State,bool Auto)
{
	//sem_init((sem_t*)hEvent, 0, State?1:0);
	verify(State==false&&Auto==true);
	mutx = PTHREAD_MUTEX_INITIALIZER;
	cond = PTHREAD_COND_INITIALIZER;
}
cResetEvent::~cResetEvent()
{
	//Destroy the event object ?

}
void cResetEvent::Set()//Signal
{
	pthread_mutex_lock( &mutx );
	state=true;
    pthread_cond_signal( &cond);
	pthread_mutex_unlock( &mutx );
}
void cResetEvent::Reset()//reset
{
	pthread_mutex_lock( &mutx );
	state=false;
	pthread_mutex_unlock( &mutx );
}
void cResetEvent::Wait(u32 msec)//Wait for signal , then reset
{
	verify(false);
}
void cResetEvent::Wait()//Wait for signal , then reset
{
	pthread_mutex_lock( &mutx );
	if (!state)
	{
		pthread_cond_wait( &cond, &mutx );
	}
	state=false;
	pthread_mutex_unlock( &mutx );
}

//End AutoResetEvent

#include <errno.h>

void VArray2::LockRegion(u32 offset,u32 size)
{
  u32 inpage=offset & PAGE_MASK;
	u32 rv=mprotect (data+offset-inpage, size+inpage, PROT_READ );
	if (rv!=0)
	{
		printf("mprotect(%08X,%08X,R) failed: %d | %d\n",data+offset-inpage,size+inpage,rv,errno);
		die("mprotect  failed ..\n");
	}
}

void print_mem_addr()
{
    FILE *ifp, *ofp;
    char *mode = "r";
    char outputFilename[] = "/data/data/com.reicast.emulator/files/mem_alloc.txt";

    ifp = fopen("/proc/self/maps", mode);

    if (ifp == NULL) {
        fprintf(stderr, "Can't open input file /proc/self/maps!\n");
        exit(1);
    }

    ofp = fopen(outputFilename, "w");

    if (ofp == NULL) {
        fprintf(stderr, "Can't open output file %s!\n",
                outputFilename);
        exit(1);
    }

    char line [ 512 ];
    while (fgets(line, sizeof line, ifp) != NULL) {
        fprintf(ofp, "%s", line);
    }

    fclose(ifp);
    fclose(ofp);
}

void VArray2::UnLockRegion(u32 offset,u32 size)
{
  u32 inpage=offset & PAGE_MASK;
	u32 rv=mprotect (data+offset-inpage, size+inpage, PROT_READ | PROT_WRITE);
	if (rv!=0)
	{
        print_mem_addr();
		printf("mprotect(%08X,%08X,RW) failed: %d | %d\n",data+offset-inpage,size+inpage,rv,errno);
		die("mprotect  failed ..\n");
	}
}
double os_GetSeconds()
{
	timeval a;
	gettimeofday (&a,0);
	static u64 tvs_base=a.tv_sec;
	return a.tv_sec-tvs_base+a.tv_usec/1000000.0;
}


void enable_runfast()
{
	#if HOST_CPU==CPU_ARM && !defined(ARMCC)
	static const unsigned int x = 0x04086060;
	static const unsigned int y = 0x03000000;
	int r;
	asm volatile (
		"fmrx	%0, fpscr			\n\t"	//r0 = FPSCR
		"and	%0, %0, %1			\n\t"	//r0 = r0 & 0x04086060
		"orr	%0, %0, %2			\n\t"	//r0 = r0 | 0x03000000
		"fmxr	fpscr, %0			\n\t"	//FPSCR = r0
		: "=r"(r)
		: "r"(x), "r"(y)
	);

	printf("ARM VFP-Run Fast (NFP) enabled !\n");
	#endif
}

void common_linux_setup()
{
	enable_runfast();
	install_fault_handler();
	signal(SIGINT, exit);
	
	settings.profile.run_counts=0;
	
	printf("Linux paging: %08X %08X %08X\n",sysconf(_SC_PAGESIZE),PAGE_SIZE,PAGE_MASK);
	verify(PAGE_MASK==(sysconf(_SC_PAGESIZE)-1));
}
#endif
