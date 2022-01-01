/*
    Simple Core Audio backend for osx (and maybe ios?)
    Based off various audio core samples and dolphin's code

    This is part of the Reicast project, please consult the
    LICENSE file for licensing & related information

    This could do with some locking logic to avoid
    race conditions, and some variable length buffer
    logic to support chunk sizes other than 512 bytes

    It does work on my macmini though
 */
#if defined(__APPLE__)
#include "audiostream.h"
#include "stdclass.h"

#include <atomic>

#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioQueue.h>

static AudioUnit audioUnit;

static u32 BUFSIZE;
static u8 *samples_temp;

static std::atomic<int> samples_wptr;
static std::atomic<int> samples_rptr;
static cResetEvent bufferEmpty;

// input buffer and indexes
static u8 samples_input[2400];
constexpr size_t InputBufSize = sizeof(samples_input);
static std::atomic<int> input_wptr;
static std::atomic<int> input_rptr;
AudioQueueRef recordQueue;

static OSStatus coreaudio_callback(void* ctx, AudioUnitRenderActionFlags* flags, const AudioTimeStamp* ts,
                            UInt32 bus, UInt32 frames, AudioBufferList* abl)
{
	for (int i = 0; i < abl->mNumberBuffers; i++)
    {
		int size = abl->mBuffers[i].mDataByteSize;
		u8 *outBuffer = (u8 *)abl->mBuffers[i].mData;
		while (size != 0)
		{
			int avail = (samples_wptr - samples_rptr + BUFSIZE) % BUFSIZE;
			if (avail == 0)
			{
				//printf("Core Audio: buffer underrun %d bytes (%d)", size, abl->mBuffers[i].mDataByteSize);
				memset(outBuffer, '\0', size);
				return noErr;
			}
			avail = std::min(avail, size);
			avail = std::min(avail, (int)BUFSIZE - samples_rptr);
			memcpy(outBuffer, samples_temp + samples_rptr, avail);
			samples_rptr = (samples_rptr + avail) % BUFSIZE;
			size -= avail;
			outBuffer += avail;
			// Set the mutex to allow writing
			bufferEmpty.Set();
		}
    }

    return noErr;
}

static void coreaudio_init()
{
    OSStatus err;
    AURenderCallbackStruct callback_struct;
    AudioStreamBasicDescription format;
    AudioComponentDescription desc;
    AudioComponent component;

    desc.componentType = kAudioUnitType_Output;
#if !defined(TARGET_IPHONE)
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
#else
    desc.componentSubType = kAudioUnitSubType_RemoteIO;
#endif
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    component = AudioComponentFindNext(nullptr, &desc);

    verify(component != nullptr);

    err = AudioComponentInstanceNew(component, &audioUnit);
    verify(err == noErr);

    FillOutASBDForLPCM(format, 44100,
                       2, 16, 16, false, false, false);
    err = AudioUnitSetProperty(audioUnit,
                               kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Input, 0, &format,
                               sizeof(AudioStreamBasicDescription));
    verify(err == noErr);

    callback_struct.inputProc = coreaudio_callback;
    callback_struct.inputProcRefCon = 0;
    err = AudioUnitSetProperty(audioUnit,
                               kAudioUnitProperty_SetRenderCallback,
                               kAudioUnitScope_Input, 0, &callback_struct,
                               sizeof callback_struct);
    verify(err == noErr);

    /*
    err = AudioUnitSetParameter(audioUnit,
                                kHALOutputParam_Volume,
                                kAudioUnitParameterFlag_Output, 0,
                                1, 0);
    verify(err == noErr);
    */

    err = AudioUnitInitialize(audioUnit);
    verify(err == noErr);

	BUFSIZE = config::AudioBufferSize * 4;
	samples_temp = new u8[BUFSIZE]();
	samples_rptr = 0;
	samples_wptr = 0;

    err = AudioOutputUnitStart(audioUnit);
    verify(err == noErr);
	
    bufferEmpty.Set();
}

static u32 coreaudio_push(const void* frame, u32 samples, bool wait)
{
    int size = samples * 4;
    while (size != 0)
    {
        int avail = (samples_rptr - samples_wptr - 4 + BUFSIZE) % BUFSIZE;
        if (avail == 0)
        {
            if (!wait)
                break;
            bufferEmpty.Wait();
            continue;
        }
		avail = std::min(avail, size);
		avail = std::min(avail, (int)BUFSIZE - samples_wptr);
        memcpy(&samples_temp[samples_wptr], frame, avail);
        samples_wptr = (samples_wptr + avail) % BUFSIZE;
		frame = (u8 *)frame + avail;
		size -= avail;
    }

    return 1;
}

static void coreaudio_term()
{
	AudioOutputUnitStop(audioUnit);
	AudioUnitUninitialize(audioUnit);
	AudioComponentInstanceDispose(audioUnit);
	bufferEmpty.Set();
	delete [] samples_temp;
}

static void coreaudio_record_callback(void *inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer, const AudioTimeStamp *inStartTime, UInt32 frameSize, const AudioStreamPacketDescription *dataFormat)
{
	//DEBUG_LOG(AUDIO, "AudioQueue callback: wptr %d rptr %d bytes %d", (int)input_wptr, (int)input_rptr, inBuffer->mAudioDataByteSize);
	UInt32 size = inBuffer->mAudioDataByteSize;
	UInt32 freeSpace = (input_rptr - input_wptr - 2 + InputBufSize) % InputBufSize;
	if (size > freeSpace)
	{
		DEBUG_LOG(AUDIO, "coreaudio: record overrun %d bytes", size - freeSpace);
		size = freeSpace;
	}
	while (size != 0)
	{
		UInt32 chunk = std::min(size, (UInt32)(InputBufSize - input_wptr));
		memcpy(samples_input + input_wptr, inBuffer->mAudioData, chunk);
		input_wptr = (input_wptr + chunk) % InputBufSize;
		size -= chunk;
	}
	AudioQueueEnqueueBuffer(recordQueue, inBuffer, 0, nullptr);
}

static void coreaudio_term_record()
{
	if (recordQueue != nullptr)
	{
		AudioQueueStop(recordQueue, true);
		AudioQueueDispose(recordQueue, true);
		recordQueue = nullptr;
	}
}

static bool coreaudio_init_record(u32 sampling_freq)
{
	AudioStreamBasicDescription desc{};
	desc.mFormatID = kAudioFormatLinearPCM;
	desc.mSampleRate = (double)sampling_freq;
	desc.mChannelsPerFrame = 1;
	desc.mBitsPerChannel = 16;
	desc.mBytesPerPacket = desc.mBytesPerFrame = 2;
	desc.mFramesPerPacket = 1;
	desc.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
	desc.mReserved = 0;

	OSStatus err = AudioQueueNewInput(&desc,
					   coreaudio_record_callback,
					   nullptr,
					   nullptr,
					   kCFRunLoopCommonModes,
					   0,
					   &recordQueue);
	if (err != noErr)
	{
		ERROR_LOG(AUDIO, "AudioQueueNewInput failed: %d", err);
		return false;
	}
	
	AudioQueueBufferRef buffers[2];
	for (UInt32 i = 0; i < ARRAY_SIZE(buffers) && err == noErr; i++)
	{
		err = AudioQueueAllocateBuffer(recordQueue, 480, &buffers[i]);
		if (err == noErr)
			err = AudioQueueEnqueueBuffer(recordQueue, buffers[i], 0, nullptr);
    }
	input_wptr = 0;
	input_rptr = 0;
	if (err == noErr)
		err = AudioQueueStart(recordQueue, nullptr);
	if (err != noErr)
	{
		ERROR_LOG(AUDIO, "AudioQueue init failed: %d", err);
		coreaudio_term_record();
		return false;
	}
	INFO_LOG(AUDIO, "AudioQueue initialized - sample rate %f", desc.mSampleRate);

	return true;
}

static u32 coreaudio_record(void* frame, u32 samples)
{
//	DEBUG_LOG(AUDIO, "coreaudio_record: wptr %d rptr %d", (int)input_wptr, (int)input_rptr);
    u32 size = samples * 2;
    while (size != 0)
    {
        u32 avail = (input_wptr - input_rptr + InputBufSize) % InputBufSize;
		if (avail == 0)
		{
			DEBUG_LOG(AUDIO, "coreaudio: record underrun %d bytes", size);
			break;
		}
		avail = std::min(avail, size);
		avail = std::min(avail, (u32)(InputBufSize - input_rptr));

		memcpy(frame, &samples_input[input_rptr], avail);
		frame = (u8 *)frame + avail;
        input_rptr = (input_rptr + avail) % InputBufSize;
		size -= avail;
    }

	return samples - size / 2;
}

static audiobackend_t audiobackend_coreaudio = {
    "coreaudio", // Slug
    "Core Audio", // Name
    &coreaudio_init,
    &coreaudio_push,
    &coreaudio_term,
	nullptr,
	&coreaudio_init_record,
	&coreaudio_record,
	&coreaudio_term_record
};

static bool core = RegisterAudioBackend(&audiobackend_coreaudio);

#endif
