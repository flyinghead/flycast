#include "build.h"
#if defined(_WIN32) && !defined(TARGET_UWP)
#include "audiostream.h"
#include <initguid.h>
#include <dsound.h>
#include <vector>
#include <atomic>
#include <thread>
#include "stdclass.h"
#include "windows/comptr.h"

HWND getNativeHwnd();
#define verifyc(x) verify(!FAILED(x))

#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IDirectSoundBuffer8, 0x6825A449,0x7524,0x4D82,0x92,0x0F,0x50,0xE3,0x6A,0xB3,0xAB,0x1E);
__CRT_UUID_DECL(IDirectSoundNotify,	0xB0210783,0x89cd,0x11d0,0xAF,0x08,0x00,0xA0,0xC9,0x25,0xCD,0x16);
__CRT_UUID_DECL(IDirectSoundCaptureBuffer8, 0x00990DF4,0x0DBB,0x4872,0x83,0x3E,0x6D,0x30,0x3E,0x80,0xAE,0xB6);
#else
struct __declspec(uuid("{6825A449-7524-4D82-920F-50E36AB3AB1E}")) IDirectSoundBuffer8;
struct __declspec(uuid("{B0210783-89cd-11d0-AF08-00A0C925CD16}")) IDirectSoundNotify;
struct __declspec(uuid("{00990DF4-0DBB-4872-833E-6D303E80AEB6}")) IDirectSoundCaptureBuffer8;
#endif

static ComPtr<IDirectSound8> dsound;
static ComPtr<IDirectSoundBuffer8> buffer;
static std::vector<HANDLE> notificationEvents;

static ComPtr<IDirectSoundCapture8> dcapture;
static ComPtr<IDirectSoundCaptureBuffer8> capture_buffer;

static std::atomic_bool audioThreadRunning;
static std::thread audioThread;
static cResetEvent pushWait;

constexpr u32 SAMPLE_BYTES = SAMPLE_COUNT * 4;

static RingBuffer ringBuffer;

static u32 notificationOffset(int index) {
	return index * SAMPLE_BYTES;
}

static void audioThreadMain()
{
	audioThreadRunning = true;
	while (true)
	{
		u32 rv = WaitForMultipleObjects(notificationEvents.size(), &notificationEvents[0], false, 100);

		if (!audioThreadRunning)
			break;
		if (rv == WAIT_TIMEOUT || rv == WAIT_FAILED)
			continue;
		rv -= WAIT_OBJECT_0;

		void *p1, *p2;
		DWORD sz1, sz2;

		if (SUCCEEDED(buffer->Lock(notificationOffset(rv), SAMPLE_BYTES, &p1, &sz1, &p2, &sz2, 0)))
		{
			if (!ringBuffer.read((u8*)p1, sz1))
				memset(p1, 0, sz1);
			if (sz2 != 0)
			{
				if (!ringBuffer.read((u8*)p2, sz2))
					memset(p2, 0, sz2);
			}
			buffer->Unlock(p1, sz1, p2, sz2);
			pushWait.Set();
		}
	}
}

static void directsound_init()
{
	verifyc(DirectSoundCreate8(NULL, &dsound.get(), NULL));
	verifyc(dsound->SetCooperativeLevel(getNativeHwnd(), DSSCL_PRIORITY));

	// Set up WAV format structure.
	WAVEFORMATEX wfx;
	memset(&wfx, 0, sizeof(WAVEFORMATEX));
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = 2;
	wfx.nSamplesPerSec = 44100;
	wfx.nBlockAlign = 4;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
	wfx.wBitsPerSample = 16;

	// Set up DSBUFFERDESC structure.
	DSBUFFERDESC desc;
	memset(&desc, 0, sizeof(DSBUFFERDESC));
	desc.dwSize = sizeof(DSBUFFERDESC);
	desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GLOBALFOCUS;
	desc.dwBufferBytes = SAMPLE_BYTES * 2;
	desc.lpwfxFormat = &wfx;

	// Create the buffer
	ComPtr<IDirectSoundBuffer> buffer_;
	verifyc(dsound->CreateSoundBuffer(&desc, &buffer_.get(), 0));
	verifyc(buffer_.as(buffer));

	// Set up notifications
	ComPtr<IDirectSoundNotify> bufferNotify;
	verifyc(buffer.as(bufferNotify));
	notificationEvents.clear();
	std::vector<DSBPOSITIONNOTIFY> posNotify;
	for (int i = 0; notificationOffset(i) < desc.dwBufferBytes; i++)
	{
		notificationEvents.push_back(CreateEvent(nullptr, false, false, nullptr));
		posNotify.push_back({ notificationOffset(i), notificationEvents.back() });
	}
	bufferNotify->SetNotificationPositions(posNotify.size(), &posNotify[0]);

	// Clear the buffers
	void *p1, *p2;
	DWORD sz1, sz2;
	verifyc(buffer->Lock(0, desc.dwBufferBytes, &p1, &sz1, &p2, &sz2, 0));
	verify(p2 == nullptr);
	memset(p1, 0, sz1);
	verifyc(buffer->Unlock(p1, sz1, p2, sz2));
	ringBuffer.setCapacity(config::AudioBufferSize * 4);

	// Start the thread
	audioThread = std::thread(audioThreadMain);

	// Play the buffer !
	verifyc(buffer->Play(0, 0, DSBPLAY_LOOPING));
	INFO_LOG(AUDIO, "DirectSound playback started");
}

static u32 directsound_push(const void* frame, u32 samples, bool wait)
{
	while (!ringBuffer.write((const u8 *)frame, samples * 4) && wait)
		pushWait.Wait();

	return 1;
}

static void directsound_term()
{
	audioThreadRunning = false;
	audioThread.join();
	buffer->Stop();

	for (HANDLE event : notificationEvents)
		CloseHandle(event);
	buffer.reset();
	dsound.reset();
	INFO_LOG(AUDIO, "DirectSound playback stopped");
}

static bool directsound_init_record(u32 sampling_freq)
{
	if (FAILED(DirectSoundCaptureCreate8(&DSDEVID_DefaultVoiceCapture, &dcapture.get(), NULL)))
	{
		INFO_LOG(AUDIO, "DirectSound capture device creation failed");
		return false;
	}
	HRESULT hr;
	WAVEFORMATEX wfx =
	{ WAVE_FORMAT_PCM, 1, sampling_freq, sampling_freq * 2, 2, 16, 0 };
	// wFormatTag, nChannels, nSamplesPerSec, nAvgBytesPerSec,
	// nBlockAlign, wBitsPerSample, cbSize

	DSCBUFFERDESC dscbd;
	dscbd.dwSize = sizeof(DSCBUFFERDESC);
	dscbd.dwFlags = 0;
	dscbd.dwBufferBytes = 480 * 2;
	dscbd.dwReserved = 0;
	dscbd.lpwfxFormat = &wfx;
	dscbd.dwFXCount = 0;
	dscbd.lpDSCFXDesc = NULL;

	ComPtr<IDirectSoundCaptureBuffer> pDSCB;
	if (FAILED(hr = dcapture->CreateCaptureBuffer(&dscbd, &pDSCB.get(), NULL)))
	{
		INFO_LOG(AUDIO, "DirectSound capture buffer creation failed");
		dcapture.reset();
		return false;
	}
	pDSCB.as(capture_buffer);
	capture_buffer->Start(DSCBSTART_LOOPING);
	INFO_LOG(AUDIO, "DirectSound capture device and buffer created");

	return true;
}

static u32 directsound_record(void *buffer, u32 samples)
{
	DWORD readPos;
	capture_buffer->GetCurrentPosition(NULL, &readPos);
	void *p1, *p2;
	DWORD p1bytes, p2bytes;
	capture_buffer->Lock(readPos, samples * 2, &p1, &p1bytes, &p2, &p2bytes, 0);
	memcpy(buffer, p1, p1bytes);
	if (p2bytes > 0)
		memcpy((u8 *)buffer + p1bytes, p2, p2bytes);
	capture_buffer->Unlock(p1, p1bytes, p2, p2bytes);
	return (p1bytes + p2bytes) / 2;
}

static void directsound_term_record()
{
	if (!dcapture)
		return;
	capture_buffer->Stop();
	capture_buffer.reset();
	dcapture.reset();
}

static audiobackend_t audiobackend_directsound = {
	"directsound", // Slug
	"Microsoft DirectSound", // Name
	&directsound_init,
	&directsound_push,
	&directsound_term,
	NULL,
	&directsound_init_record,
	&directsound_record,
	&directsound_term_record
};

static bool ds = RegisterAudioBackend(&audiobackend_directsound);
#endif
