/*
	Copyright 2021 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "audiostream.h"
#ifdef USE_OBOE
#include <oboe/Oboe.h>
#include <vector>
#include <algorithm>
#include <atomic>
#include <memory>
#include "stdclass.h"

static RingBuffer ringBuffer;
static cResetEvent pushWait;

static std::shared_ptr<oboe::AudioStream> stream;
static std::shared_ptr<oboe::AudioStream> recordStream;

static void audio_init();
static void audio_term();

class AudioCallback : public oboe::AudioStreamDataCallback
{
public:
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames) override
    {
    	if (!ringBuffer.read((u8 *)audioData, numFrames * 4))
    		// underrun
    		memset(audioData, 0, numFrames * 4);
   		pushWait.Set();

        return oboe::DataCallbackResult::Continue;
    }
};
static AudioCallback audioCallback;

class AudioErrorCallback : public oboe::AudioStreamErrorCallback
{
public:
	void onErrorAfterClose(oboe::AudioStream *stream, oboe::Result error) override {
		WARN_LOG(AUDIO, "Audio device lost. Attempting to reopen the audio stream");
		audio_term();
		audio_init();
	}
};
static AudioErrorCallback errorCallback;

static void audio_init()
{
	// Actual capacity is size-1 to avoid overrun so add one buffer
	ringBuffer.setCapacity((config::AudioBufferSize + SAMPLE_COUNT) * 4);

	oboe::AudioStreamBuilder builder;
	oboe::Result result = builder.setDirection(oboe::Direction::Output)
			->setPerformanceMode(oboe::PerformanceMode::LowLatency)
			->setSharingMode(oboe::SharingMode::Exclusive)
			->setFormat(oboe::AudioFormat::I16)
			->setChannelCount(oboe::ChannelCount::Stereo)
			->setSampleRate(44100)
			->setFramesPerCallback(SAMPLE_COUNT)
			->setDataCallback(&audioCallback)
			->setErrorCallback(&errorCallback)
			->setUsage(oboe::Usage::Game)
			->openStream(stream);
	if (result != oboe::Result::OK)
	{
		ERROR_LOG(AUDIO, "Oboe open stream failed: %s", oboe::convertToText(result));
		return;
	}

	if (stream->getAudioApi() == oboe::AudioApi::AAudio && config::AudioBufferSize < 1764)
	{
		// Reduce internal buffer for low latency (< 40 ms)
		int bufSize = stream->getBufferSizeInFrames();
		int burst = stream->getFramesPerBurst();
		if (bufSize - burst > SAMPLE_COUNT)
		{
			while (bufSize - burst > SAMPLE_COUNT)
				bufSize -= burst;
			stream->setBufferSizeInFrames(bufSize);
		}
	}

	stream->requestStart();
	NOTICE_LOG(AUDIO, "Oboe driver started. stream capacity: %d frames, size: %d frames, frames/callback: %d, frames/burst: %d",
			stream->getBufferCapacityInFrames(), stream->getBufferSizeInFrames(),
			stream->getFramesPerCallback(), stream->getFramesPerBurst());
}

static void audio_term()
{
	NOTICE_LOG(AUDIO, "Oboe driver stopping");
	if (stream != nullptr)
	{
		stream->stop();
		stream->close();
		stream.reset();
	}
}

static u32 audio_push(const void* frame, u32 samples, bool wait) {
	while (!ringBuffer.write((const u8 *)frame, samples * 4) && wait)
		pushWait.Wait();

	return 1;
}

static void term_record()
{
	if (recordStream != nullptr)
	{
		recordStream->stop();
		recordStream->close();
		recordStream.reset();
	}
	NOTICE_LOG(AUDIO, "Oboe recorder stopped");
}

static bool init_record(u32 sampling_freq)
{
	oboe::AudioStreamBuilder builder;
	oboe::Result result = builder.setDirection(oboe::Direction::Input)
		->setPerformanceMode(oboe::PerformanceMode::None)
		->setSharingMode(oboe::SharingMode::Exclusive)
		->setFormat(oboe::AudioFormat::I16)
		->setChannelCount(oboe::ChannelCount::Mono)
		->setSampleRate(sampling_freq)
		->openStream(recordStream);
	if (result != oboe::Result::OK)
	{
		ERROR_LOG(AUDIO, "Oboe open record stream failed: %s", oboe::convertToText(result));
		return false;
	}
	recordStream->requestStart();
	NOTICE_LOG(AUDIO, "Oboe recorder started. stream capacity: %d frames",
			stream->getBufferCapacityInFrames());

	return true;
}

static u32 record(void *data, u32 samples)
{
	if (recordStream == nullptr)
		return 0;
	oboe::ResultWithValue<int32_t> result = recordStream->read(data, samples, 0);
	if (result == oboe::Result::ErrorDisconnected)
	{
		WARN_LOG(AUDIO, "Recording device lost. Attempting to reopen the audio stream");
		u32 sampleRate = recordStream->getSampleRate();
		term_record();
		init_record(sampleRate);
	}
	return std::max(0, result.value());
}

static audiobackend_t audiobackend_oboe = {
		"Oboe",		// Slug
		"Automatic AAudio / OpenSL selection",	// Name
		&audio_init,
		&audio_push,
		&audio_term,
		NULL,
		&init_record,
		&record,
		&term_record
};

static bool oboebe = RegisterAudioBackend(&audiobackend_oboe);

#endif
