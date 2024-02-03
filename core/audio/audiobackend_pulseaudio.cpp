#ifdef USE_PULSEAUDIO
#include "audiostream.h"
#include "cfg/option.h"
#include <pulse/pulseaudio.h>

class PulseAudioBackend : public AudioBackend
{
	pa_threaded_mainloop *mainloop = nullptr;
	pa_context *context = nullptr;
	pa_stream *stream = nullptr;
	pa_stream *record_stream = nullptr;

	static void context_state_cb(pa_context *c, void *userdata)
	{
		PulseAudioBackend *backend = (PulseAudioBackend *)userdata;
		switch (pa_context_get_state(c))
		{
		case PA_CONTEXT_READY:
		case PA_CONTEXT_TERMINATED:
		case PA_CONTEXT_FAILED:
			pa_threaded_mainloop_signal(backend->mainloop, 0);
			break;
		default:
			break;
		}
	}

	static void stream_request_cb(pa_stream *s, size_t length, void *userdata)
	{
		PulseAudioBackend *backend = (PulseAudioBackend *)userdata;
		pa_threaded_mainloop_signal(backend->mainloop, 0);
	}

public:
	PulseAudioBackend()
		: AudioBackend("pulse", "PulseAudio") {}

	bool init() override
	{
		mainloop = pa_threaded_mainloop_new();
		if (!mainloop)
		{
			WARN_LOG(AUDIO, "PulseAudio: pa_threaded_mainloop_new failed");
			return false;
		}
		context = pa_context_new(pa_threaded_mainloop_get_api(mainloop), "flycast");
		if (!context)
		{
			WARN_LOG(AUDIO, "PulseAudio: pa_context_new failed");
			term();
			return false;
		}
		pa_context_set_state_callback(context, context_state_cb, this);

		if (pa_context_connect(context, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0)
		{
			WARN_LOG(AUDIO, "PulseAudio: pa_context_connect failed");
			term();
			return false;
		}

		pa_threaded_mainloop_lock(mainloop);
		if (pa_threaded_mainloop_start(mainloop) < 0)
		{
			WARN_LOG(AUDIO, "PulseAudio: pa_threaded_mainloop_start failed");
			pa_threaded_mainloop_unlock(mainloop);
			term();
			return false;
		}
		pa_threaded_mainloop_wait(mainloop);

		if (pa_context_get_state(context) != PA_CONTEXT_READY)
		{
			WARN_LOG(AUDIO, "PulseAudio: context isn't ready");
			pa_threaded_mainloop_unlock(mainloop);
			term();
			return false;
		}
		pa_sample_spec spec{ PA_SAMPLE_S16LE, 44100, 2 };

		stream = pa_stream_new(context, "audio", &spec, NULL);
		if (!stream)
		{
			WARN_LOG(AUDIO, "PulseAudio: pa_stream_new failed");
			pa_threaded_mainloop_unlock(mainloop);
			term();
			return false;
		}

		//pa_stream_set_state_callback(stream, stream_state_cb, this);
		pa_stream_set_write_callback(stream, stream_request_cb, this);
		//pa_stream_set_latency_update_callback(stream, stream_latency_update_cb, this);
		//pa_stream_set_underflow_callback(stream, underrun_update_cb, this);
		//pa_stream_set_buffer_attr_callback(stream, buffer_attr_cb, this);

		pa_buffer_attr buffer_attr;
		buffer_attr.maxlength = -1;
		buffer_attr.tlength = pa_usec_to_bytes(config::AudioBufferSize * PA_USEC_PER_SEC / 44100, &spec);
		buffer_attr.prebuf = -1;
		buffer_attr.minreq = -1;
		buffer_attr.fragsize = -1;

		if (pa_stream_connect_playback(stream, nullptr, &buffer_attr, PA_STREAM_ADJUST_LATENCY, nullptr, nullptr) < 0)
		{
			WARN_LOG(AUDIO, "PulseAudio: pa_stream_connect_playback failed");
			pa_threaded_mainloop_unlock(mainloop);
			term();
			return false;
		}

		pa_threaded_mainloop_wait(mainloop);

		if (pa_stream_get_state(stream) != PA_STREAM_READY)
		{
			WARN_LOG(AUDIO, "PulseAudio: stream isn't ready");
			pa_threaded_mainloop_unlock(mainloop);
			term();
			return false;
		}

		const pa_buffer_attr *server_attr = pa_stream_get_buffer_attr(stream);
		if (server_attr)
			DEBUG_LOG(AUDIO, "PulseAudio: requested %d samples buffer, got %d", buffer_attr.tlength / 4, server_attr->tlength / 4);

		pa_threaded_mainloop_unlock(mainloop);

		return true;
	}

	u32 push(const void* frame, u32 samples, bool wait) override
	{
		const u8 *buf = (const u8 *)frame;
		size_t size = samples * 4;

		pa_threaded_mainloop_lock(mainloop);
		while (size)
		{
			size_t writable = std::min(size, pa_stream_writable_size(stream));

			if (writable)
			{
				pa_stream_write(stream, buf, writable, NULL, 0, PA_SEEK_RELATIVE);
				buf += writable;
				size -= writable;
			}
			else if (wait)
				pa_threaded_mainloop_wait(mainloop);
			else
				break;
		}
		pa_threaded_mainloop_unlock(mainloop);

		return 0;
	}

	void term() override
	{
		if (mainloop)
			pa_threaded_mainloop_stop(mainloop);

		if (stream)
		{
			pa_stream_disconnect(stream);
			pa_stream_unref(stream);
			stream = nullptr;
		}
		if (context)
		{
			pa_context_disconnect(context);
			pa_context_unref(context);
			context = nullptr;
		}

		if (mainloop)
			pa_threaded_mainloop_free(mainloop);
		mainloop = nullptr;
	}

	bool initRecord(u32 sampling_freq) override
	{
		pa_sample_spec spec{ PA_SAMPLE_S16LE, sampling_freq, 1 };

		record_stream = pa_stream_new(context, "record", &spec, NULL);
		if (!record_stream)
		{
			INFO_LOG(AUDIO, "PulseAudio: pa_stream_new failed");
			return false;
		}

		pa_threaded_mainloop_lock(mainloop);

		pa_buffer_attr buffer_attr;
		buffer_attr.fragsize = 240 * 2;
		buffer_attr.maxlength = buffer_attr.fragsize * 2;

		if (pa_stream_connect_record(record_stream, nullptr, &buffer_attr, PA_STREAM_NOFLAGS) < 0)
		{
			INFO_LOG(AUDIO, "PulseAudio: pa_stream_connect_record failed");
			pa_stream_unref(record_stream);
			record_stream = nullptr;
			return false;
		}
		pa_threaded_mainloop_unlock(mainloop);
		INFO_LOG(AUDIO, "PulseAudio: Successfully initialized capture device");

		return true;
	}

	void termRecord() override
	{
		if (record_stream != nullptr)
		{
			pa_threaded_mainloop_lock(mainloop);
			pa_stream_disconnect(record_stream);
			pa_stream_unref(record_stream);
			record_stream = nullptr;
			pa_threaded_mainloop_unlock(mainloop);
		}
	}

	u32 record(void *buffer, u32 samples) override
	{
		if (record_stream == nullptr)
			return 0;
		pa_threaded_mainloop_lock(mainloop);
		const void *data;
		size_t size;
		if (pa_stream_peek(record_stream, &data, &size) < 0)
		{
			pa_threaded_mainloop_unlock(mainloop);
			DEBUG_LOG(AUDIO, "PulseAudio: pa_stream_peek error");
			return 0;
		}
		if (size == 0)
		{
			pa_threaded_mainloop_unlock(mainloop);
			return 0;
		}
		size = std::min((size_t)samples * 2, size);
		if (data != nullptr)
			memcpy(buffer, data, size);
		else
			memset(buffer, 0, size);
		pa_stream_drop(record_stream);
		pa_threaded_mainloop_unlock(mainloop);

		return size / 2;
	}
};
static PulseAudioBackend pulseAudioBackend;

#endif
