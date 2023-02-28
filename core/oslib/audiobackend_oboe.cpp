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
#ifdef USE_OBOE
#include "audiostream.h"
#include "cfg/option.h"
#include <oboe/Oboe.h>
#include <vector>
#include <algorithm>
#include <atomic>
#include <memory>
#include "stdclass.h"

class OboeBackend : AudioBackend
{
	RingBuffer ringBuffer;
	cResetEvent pushWait;

	std::shared_ptr<oboe::AudioStream> stream;
	std::shared_ptr<oboe::AudioStream> recordStream;

	class AudioCallback : public oboe::AudioStreamDataCallback
	{
	public:
		AudioCallback(OboeBackend *backend) : backend(backend) {}

		oboe::DataCallbackResult onAudioReady(oboe::AudioStream *audioStream, void *audioData, int32_t numFrames) override
		{
			if (!backend->ringBuffer.read((u8 *)audioData, numFrames * 4))
				// underrun
				memset(audioData, 0, numFrames * 4);
			backend->pushWait.Set();

			return oboe::DataCallbackResult::Continue;
		}

		OboeBackend *backend;
	};
	AudioCallback audioCallback;

	class AudioErrorCallback : public oboe::AudioStreamErrorCallback
	{
	public:
		AudioErrorCallback(OboeBackend *backend) : backend(backend) {}

		void onErrorAfterClose(oboe::AudioStream *stream, oboe::Result error) override
		{
			// Only attempt to recover if init was successful
			if (backend->stream != nullptr)
			{
				WARN_LOG(AUDIO, "Audio device lost. Attempting to reopen the audio stream");
				// the oboe stream is already closed so make sure we don't close it twice
				backend->stream.reset();
				backend->term();
				backend->init();
			}
		}

		OboeBackend *backend;
	};
	AudioErrorCallback errorCallback;

public:
	OboeBackend()
		: AudioBackend("Oboe", "Automatic AAudio / OpenSL selection"), audioCallback(this), errorCallback(this) {}

	bool init() override
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
			return false;
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

		return true;
	}

	void term() override
	{
		NOTICE_LOG(AUDIO, "Oboe driver stopping");
		if (stream != nullptr)
		{
			// Don't let the AudioErrorCallback term/reinit while we are stopping
			// This won't prevent shit to hit the fan if it's already in the process
			// of doing so but this is a pretty rare event and happens on devices
			// that have audio issues already.
			auto localStream = stream;
			stream.reset();
			localStream->stop();
			localStream->close();
		}
	}

	u32 push(const void* frame, u32 samples, bool wait) override
	{
		while (!ringBuffer.write((const u8 *)frame, samples * 4) && wait)
			pushWait.Wait();

		return 1;
	}

	void termRecord() override
	{
		if (recordStream != nullptr)
		{
			recordStream->stop();
			recordStream->close();
			recordStream.reset();
		}
		NOTICE_LOG(AUDIO, "Oboe recorder stopped");
	}

	bool initRecord(u32 sampling_freq) override
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

	u32 record(void *data, u32 samples) override
	{
		if (recordStream == nullptr)
			return 0;
		oboe::ResultWithValue<int32_t> result = recordStream->read(data, samples, 0);
		if (result == oboe::Result::ErrorDisconnected)
		{
			WARN_LOG(AUDIO, "Recording device lost. Attempting to reopen the audio stream");
			u32 sampleRate = recordStream->getSampleRate();
			termRecord();
			initRecord(sampleRate);
		}
		return std::max(0, result.value());
	}
};
static OboeBackend oboeBackend;

#endif
