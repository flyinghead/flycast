#include "audiostream.h"
#ifdef USE_PULSEAUDIO
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/unistd.h>

#ifdef PULSEAUDIO_SIMPLE
#include <pulse/simple.h>
#include <pulse/error.h>

static pa_simple *pulse_stream;

static void pulseaudio_simple_init()
{
	pa_sample_spec ss;
	ss.format = PA_SAMPLE_S16LE;
	ss.channels = 2;
	ss.rate = 44100;

	/* Create a new playback stream */
	int error;
	pulse_stream = pa_simple_new(NULL, "flycast", PA_STREAM_PLAYBACK, NULL, "flycast", &ss, NULL, NULL, &error);
	if (!pulse_stream)
		WARN_LOG(AUDIO, "PulseAudio: pa_simple_new failed: %s", pa_strerror(error));
}

static u32 pulseaudio_simple_push(const void* frame, u32 samples, bool wait)
{
	if (pa_simple_write(pulse_stream, frame, (size_t)samples * 4, NULL) < 0)
		WARN_LOG(AUDIO, "PulseAudio: pa_simple_write() failed!");

	return 0;
}

static void pulseaudio_simple_term()
{
	if (pulse_stream != NULL)
	{
		// Make sure that every single sample was played
		if (pa_simple_drain(pulse_stream, NULL) < 0)
			WARN_LOG(AUDIO, "PulseAudio: pa_simple_drain() failed!");
		pa_simple_free(pulse_stream);
	}
}

static audiobackend_t audiobackend_pulseaudio = {
		"pulse", // Slug
		"PulseAudio", // Name
		&pulseaudio_simple_init,
		&pulseaudio_simple_push,
		&pulseaudio_simple_term,
		NULL
};

#else	// !PULSEAUDIO_SIMPLE

#include <pulse/pulseaudio.h>

pa_threaded_mainloop *mainloop;
pa_context *context;
pa_stream *stream;

static void context_state_cb(pa_context *c, void *userdata)
{
	switch (pa_context_get_state(c))
	{
	case PA_CONTEXT_READY:
	case PA_CONTEXT_TERMINATED:
	case PA_CONTEXT_FAILED:
		pa_threaded_mainloop_signal(mainloop, 0);
		break;
	default:
		break;
	}
}

#if 0
static void stream_state_cb(pa_stream *s, void *data)
{
	switch (pa_stream_get_state(s))
	{
	case PA_STREAM_READY:
	case PA_STREAM_FAILED:
	case PA_STREAM_TERMINATED:
		pa_threaded_mainloop_signal(mainloop, 0);
		break;
	default:
		break;
	}
}

static void stream_latency_update_cb(pa_stream *s, void *data)
{
	pa_threaded_mainloop_signal(mainloop, 0);
}

static void underrun_update_cb(pa_stream *s, void *data)
{
	//DEBUG_LOG(AUDIO, "PulseAudio: buffer underrun");
}

static void buffer_attr_cb(pa_stream *s, void *data)
{
	const pa_buffer_attr *server_attr = pa_stream_get_buffer_attr(s);
	if (server_attr)
	{
		u32 buffer_size = server_attr->tlength;
		DEBUG_LOG(AUDIO, "PulseAudio: new buffer size %d", buffer_size);
	}
}
#endif

static void stream_request_cb(pa_stream *s, size_t length, void *data)
{
	pa_threaded_mainloop_signal(mainloop, 0);
}

static void pulseaudio_init()
{
	mainloop = pa_threaded_mainloop_new();
	if (!mainloop)
	{
		WARN_LOG(AUDIO, "PulseAudio: pa_threaded_mainloop_new failed");
		return;
	}
	context = pa_context_new(pa_threaded_mainloop_get_api(mainloop), "flycast");
	if (!context)
	{
		WARN_LOG(AUDIO, "PulseAudio: pa_context_new failed");
		return;
	}
	pa_context_set_state_callback(context, context_state_cb, nullptr);

	if (pa_context_connect(context, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0)
	{
		WARN_LOG(AUDIO, "PulseAudio: pa_context_connect failed");
		return;
	}

	pa_threaded_mainloop_lock(mainloop);
	if (pa_threaded_mainloop_start(mainloop) < 0)
	{
		WARN_LOG(AUDIO, "PulseAudio: pa_threaded_mainloop_start failed");
		return;
	}
	pa_threaded_mainloop_wait(mainloop);

	if (pa_context_get_state(context) != PA_CONTEXT_READY)
	{
		WARN_LOG(AUDIO, "PulseAudio: context isn't ready");
		return;
	}
	pa_sample_spec spec{ PA_SAMPLE_S16LE, 44100, 2 };

	stream = pa_stream_new(context, "audio", &spec, NULL);
	if (!stream)
	{
		WARN_LOG(AUDIO, "PulseAudio: pa_stream_new failed");
		return;
	}

	//pa_stream_set_state_callback(stream, stream_state_cb, nullptr);
	pa_stream_set_write_callback(stream, stream_request_cb, nullptr);
	//pa_stream_set_latency_update_callback(stream, stream_latency_update_cb, nullptr);
	//pa_stream_set_underflow_callback(stream, underrun_update_cb, nullptr);
	//pa_stream_set_buffer_attr_callback(stream, buffer_attr_cb, nullptr);

	pa_buffer_attr buffer_attr;
	buffer_attr.maxlength = -1;
	buffer_attr.tlength = pa_usec_to_bytes(settings.aica.BufferSize * PA_USEC_PER_SEC / 44100, &spec);
	buffer_attr.prebuf = -1;
	buffer_attr.minreq = -1;
	buffer_attr.fragsize = -1;

	if (pa_stream_connect_playback(stream, nullptr, &buffer_attr, PA_STREAM_ADJUST_LATENCY, nullptr, nullptr) < 0)
	{
		WARN_LOG(AUDIO, "PulseAudio: pa_stream_connect_playback failed");
		return;
	}

	pa_threaded_mainloop_wait(mainloop);

	if (pa_stream_get_state(stream) != PA_STREAM_READY)
	{
		WARN_LOG(AUDIO, "PulseAudio: stream isn't ready");
		return;
	}

	const pa_buffer_attr *server_attr = pa_stream_get_buffer_attr(stream);
	if (server_attr)
		DEBUG_LOG(AUDIO, "PulseAudio: requested %d samples buffer, got %d", buffer_attr.tlength / 4, server_attr->tlength / 4);

	pa_threaded_mainloop_unlock(mainloop);
}

static u32 pulseaudio_push(const void* frame, u32 samples, bool wait)
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

static void pulseaudio_term()
{
	if (mainloop)
		pa_threaded_mainloop_stop(mainloop);

	if (stream)
	{
		pa_stream_disconnect(stream);
		pa_stream_unref(stream);
	}
	if (context)
	{
		pa_context_disconnect(context);
		pa_context_unref(context);
	}

	if (mainloop)
		pa_threaded_mainloop_free(mainloop);
}

static audiobackend_t audiobackend_pulseaudio = {
		"pulse", // Slug
		"PulseAudio", // Name
		&pulseaudio_init,
		&pulseaudio_push,
		&pulseaudio_term,
		NULL
};
#endif	// !PULSEAUDIO_SIMPLE

static bool pulse = RegisterAudioBackend(&audiobackend_pulseaudio);
#endif
