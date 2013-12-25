#include "types.h"

#if HOST_OS==OS_WINDOWS
#include <initguid.h>
#include <dsound.h>
#include "audiostream_rif.h"
#include "oslib.h"

void* SoundThread(void* param);
#define V2_BUFFERSZ (16*1024)

IDirectSound8* dsound;
IDirectSoundBuffer8* buffer;

u32 ds_ring_size;

void os_InitAudio()
{
	verifyc(DirectSoundCreate8(NULL,&dsound,NULL));

	verifyc(dsound->SetCooperativeLevel((HWND)libPvr_GetRenderTarget(),DSSCL_PRIORITY));
	IDirectSoundBuffer* buffer_;

	WAVEFORMATEX wfx; 
	DSBUFFERDESC desc; 

	// Set up WAV format structure. 

	memset(&wfx, 0, sizeof(WAVEFORMATEX)); 
	wfx.wFormatTag = WAVE_FORMAT_PCM; 
	wfx.nChannels = 2; 
	wfx.nSamplesPerSec = 44100; 
	wfx.nBlockAlign = 4; 
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign; 
	wfx.wBitsPerSample = 16; 

	// Set up DSBUFFERDESC structure. 

	ds_ring_size=8192*wfx.nBlockAlign;

	memset(&desc, 0, sizeof(DSBUFFERDESC)); 
	desc.dwSize = sizeof(DSBUFFERDESC); 
	desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLPOSITIONNOTIFY;// _CTRLPAN | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY; 
	
	desc.dwBufferBytes = ds_ring_size; 
	desc.lpwfxFormat = &wfx; 

	

	if (settings.aica.HW_mixing==0)
	{
		desc.dwFlags |=DSBCAPS_LOCSOFTWARE;
	}
	else if (settings.aica.HW_mixing==1)
	{
		desc.dwFlags |=DSBCAPS_LOCHARDWARE;
	}
	else if (settings.aica.HW_mixing==2)
	{
		//auto
	}
	else
	{
		die("settings.HW_mixing: Invalid value");
	}

	if (settings.aica.GlobalFocus)
		desc.dwFlags|=DSBCAPS_GLOBALFOCUS;

	verifyc(dsound->CreateSoundBuffer(&desc,&buffer_,0));
	verifyc(buffer_->QueryInterface(IID_IDirectSoundBuffer8,(void**)&buffer));
	buffer_->Release();

	//Play the buffer !
	verifyc(buffer->Play(0,0,DSBPLAY_LOOPING));
	
}


DWORD wc=0;


int os_getfreesz()
{
	DWORD pc,wch;

	buffer->GetCurrentPosition(&pc,&wch);

	int fsz=0;
	if (wc>=pc)
		fsz=ds_ring_size-wc+pc;
	else
		fsz=pc-wc;

	fsz-=32;
	return fsz;
}

int os_getusedSamples()
{
	return (ds_ring_size-os_getfreesz())/4;
}


bool os_IsAudioBuffered()
{
	return os_getusedSamples()>=3000;
}


bool os_IsAudioBufferedLots()
{
	u32 xxx=os_getusedSamples();
	return xxx>=4096;
}

u32 os_Push_nw(void* frame, u32 samplesb)
{
	DWORD pc,wch;

	u32 bytes=samplesb*4;

	buffer->GetCurrentPosition(&pc,&wch);

	int fsz=0;
	if (wc>=pc)
		fsz=ds_ring_size-wc+pc;
	else
		fsz=pc-wc;

	fsz-=32;

	//printf("%d: r:%d w:%d (f:%d wh:%d)\n",fsz>bytes,pc,wc,fsz,wch);

	if (fsz>bytes)
	{
		void* ptr1,* ptr2;
		DWORD ptr1sz,ptr2sz;

		u8* data=(u8*)frame;

		buffer->Lock(wc,bytes,&ptr1,&ptr1sz,&ptr2,&ptr2sz,0);
		memcpy(ptr1,data,ptr1sz);
		if (ptr2sz)
		{
			data+=ptr1sz;
			memcpy(ptr2,data,ptr2sz);
		}

		buffer->Unlock(ptr1,ptr1sz,ptr2,ptr2sz);
		wc=(wc+bytes)%ds_ring_size;
		return 1;
	}
	return 0;
	//ds_ring_size
}

u32 os_Push(void* frame, u32 samples, bool wait)
{

	u16* f=(u16*)frame;

	bool w=false;

	for (u32 i = 0; i < samples*2; i++)
	{
		if (f[i])
		{
			w = true;
			break;
		}
	}

	wait &= w;

	int ffs=1;
	
	while (os_IsAudioBufferedLots() && wait)
		if (ffs == 0)
			ffs = printf("AUD WAIT %d\n",os_getusedSamples());

	while(!os_Push_nw(frame,samples) && wait)
		printf("FAILED waiting on audio FAILED %d\n",os_getusedSamples());

	return 1;
}
void os_TermAudio()
{
	buffer->Stop();
	
	buffer->Release();
	dsound->Release();
}

#endif