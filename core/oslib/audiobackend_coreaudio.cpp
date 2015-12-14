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

#include "oslib/audiobackend_coreaudio.h"

#if HOST_OS == OS_DARWIN

//#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>

AudioUnit audioUnit;

u8 samples_temp[1024 * 4];

volatile int samples_ptr = 0;

OSStatus coreaudio_callback(void* ctx, AudioUnitRenderActionFlags* flags, const AudioTimeStamp* ts,
                            UInt32 bus, UInt32 frames, AudioBufferList* abl)
{
    verify(frames <= 1024);
    
    u8* src = samples_temp;
    
    for (int i = 0; i < abl->mNumberBuffers; i++) {
        memcpy(abl->mBuffers[i].mData, src, abl->mBuffers[i].mDataByteSize);
        src += abl->mBuffers[i].mDataByteSize;
    }
    
    samples_ptr -= frames * 2 * 2;
    
    if (samples_ptr < 0)
        samples_ptr = 0;
    
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
}

static u32 coreaudio_push(void* frame, u32 samples, bool wait)
{
    /* Yeah, right */
    while (samples_ptr != 0 && wait) ;
    
    if (samples_ptr == 0) {
        memcpy(&samples_temp[samples_ptr], frame, samples * 4);
        samples_ptr += samples * 4;
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
}

audiobackend_t audiobackend_coreaudio = {
    "coreaudio", // Slug
    "Core Audio", // Name
    &coreaudio_init,
    &coreaudio_push,
    &coreaudio_term
};
#endif
