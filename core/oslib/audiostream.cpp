#include "audiostream_rif.h"
#include "stdclass.h"
#include "oslib/oslib.h"

//cResetEvent speed_limit(true,true);

struct SoundFrame { s16 l;s16 r; };
#define SAMPLE_COUNT 512

SoundFrame RingBuffer[SAMPLE_COUNT];
const u32 RingBufferByteSize = sizeof(RingBuffer);
const u32 RingBufferSampleCount = SAMPLE_COUNT;

volatile u32 WritePtr;  //last WRITEN sample
volatile u32 ReadPtr;   //next sample to read

u32 gen_samples=0;

double time_diff, time_last;
#ifdef LOG_SOUND
WaveWriter rawout("d:\\aica_out.wav");
#endif


u32 os_Push(void* frame, u32 amt, bool wait);
//void os_Pull(u32 level)

u32 asRingUsedCount()
{
	if (WritePtr>ReadPtr)
		return WritePtr-ReadPtr;
	else
		return RingBufferSampleCount-(ReadPtr-WritePtr);
	//s32 sz=(WritePtr+1)%RingBufferSampleCount-ReadPtr;
	//return sz<0?sz+RingBufferSampleCount:sz;
}
u32 asRingFreeCount()
{
	return RingBufferSampleCount-asRingUsedCount();
}

void WriteSample(s16 r, s16 l)
{

#if 0
	#if HOST_OS==OS_WINDOWS
		#ifdef LOG_SOUND
		rawout.Write(l,r);
		#endif

		if (!asRingFreeCount())
		{
			//printf("Buffer overrun\n");
			if (settings.aica.LimitFPS)
			{
				//speed_limit.Wait();
			}
			else
				return;
		}

		gen_samples++;
		//while limit on, 128 samples done, there is a buffer ready to be service AND speed is too fast then wait
		if (settings.aica.LimitFPS==1 && gen_samples>128)
		{
			for(;asRingUsedCount()>BufferSampleCount && (os_GetSeconds()-time_last)<=time_diff;)
				;
			gen_samples=0;
			time_last=os_GetSeconds();
		}
	#else
		if (!asRingFreeCount())
		{
			if (settings.aica.LimitFPS)
			{
				while(!asRingFreeCount()) ;
			}
			else
			{
				return;
			}
		}
	#endif
#endif


	const u32 ptr=(WritePtr+1)%RingBufferSampleCount;
	RingBuffer[ptr].r=r;
	RingBuffer[ptr].l=l;
	WritePtr=ptr;

	if (WritePtr==(SAMPLE_COUNT-1))
	{
		os_Push(RingBuffer,SAMPLE_COUNT,settings.aica.LimitFPS);
	}
}

void InitAudio()
{
	time_diff=128/44100.0;
}

void TermAudio()
{
	
}
