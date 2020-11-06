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

#include "audiostream.h"

#if defined(__APPLE__)
#include "stdclass.h"

#include <atomic>

//#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>

static AudioUnit audioUnit;

// ~ 93 ms
#define BUFSIZE (4 * 1024 * 4)
static u8 samples_temp[BUFSIZE];

static std::atomic<int> samples_wptr;
static std::atomic<int> samples_rptr;
static cResetEvent bufferEmpty;

static OSStatus coreaudio_callback(void* ctx, AudioUnitRenderActionFlags* flags, const AudioTimeStamp* ts,
                            UInt32 bus, UInt32 frames, AudioBufferList* abl)
{
    verify(frames <= 1024);

    for (int i = 0; i < abl->mNumberBuffers; i++)
    {
        u32 buf_size = abl->mBuffers[i].mDataByteSize;
        u8* out_buffer = reinterpret_cast<u8*>(abl->mBuffers[i].mData);
        if ((samples_wptr - samples_rptr + BUFSIZE) % BUFSIZE < buf_size)
        {
            //printf("Core Audio: buffer underrun");
            memset(abl->mBuffers[i].mData, '\0', buf_size);
        }
        else
        {
            if (samples_rptr + buf_size > BUFSIZE)
            {
                // The data wraps around the buffer, so we need to do 2 copies
                int size1 = BUFSIZE - samples_rptr;
                int size2 = buf_size - size1;
                memcpy(out_buffer, samples_temp + samples_rptr, size1);
                memcpy(out_buffer + size1, samples_temp, size2);
            }
            else
            {
                memcpy(out_buffer, samples_temp + samples_rptr, buf_size);
            }
            
            // Increment the read pointer
            samples_rptr = (samples_rptr + buf_size) % BUFSIZE;

            // Set the mutex to allow writing
            bufferEmpty.Set();
        }
    }

    bufferEmpty.Set();

    return noErr;
}

// We're making these functions static - there's no need to pollute the global namespace
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
    //desc.componentSubType = kAudioUnitSubType_GenericOutput;
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

    err = AudioOutputUnitStart(audioUnit);

    verify(err == noErr);

    bufferEmpty.Set();
}

static u32 coreaudio_push(const void* frame, u32 samples, bool wait)
{
    int byte_size = samples * 4;
    while (true)
    {
        int space = (samples_rptr - samples_wptr + BUFSIZE) % BUFSIZE;
        if (space != 0 && byte_size > space - 1)
        {
            if (!wait)
                break;
            bufferEmpty.Wait();
            continue;
        }
        memcpy(&samples_temp[samples_wptr], frame, byte_size);
        samples_wptr = (samples_wptr + byte_size) % BUFSIZE;
        break;
    }

    return 1;
}

static void coreaudio_term()
{
    OSStatus err;

    err = AudioOutputUnitStop(audioUnit);
    verify(err == noErr);

    err = AudioUnitUninitialize(audioUnit);
    verify(err == noErr);

    err = AudioComponentInstanceDispose(audioUnit);
    verify(err == noErr);

    bufferEmpty.Set();
}

static audiobackend_t audiobackend_coreaudio = {
    "coreaudio", // Slug
    "Core Audio", // Name
    &coreaudio_init,
    &coreaudio_push,
    &coreaudio_term,
	NULL
};

static bool core = RegisterAudioBackend(&audiobackend_coreaudio);

#endif
