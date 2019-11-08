// Copyright (c) 2007, Niels Martin Hansen
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Aegisub Group nor the names of its contributors
//     may be used to endorse or promote products derived from this software
//     without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Aegisub Project http://www.aegisub.org/

/// @file audio_player_pulse.cpp
/// @brief PulseAudio-based audio output
/// @ingroup audio_output
///

#ifdef WITH_LIBPULSE
#include "include/aegisub/audio_player.h"

#include "audio_controller.h"
#include "utils.h"

#include <libaegisub/audio/provider.h>
#include <libaegisub/log.h>
#include <libaegisub/make_unique.h>

#include <cstdio>
#include <pulse/pulseaudio.h>
#include <wx/thread.h>

namespace {
class PulseAudioPlayer final : public AudioPlayer {
	float volume = 1.f;
	bool is_playing = false;

	volatile unsigned long start_frame = 0;
	volatile unsigned long cur_frame = 0;
	volatile unsigned long end_frame = 0;

	unsigned long bpf = 0; // bytes per frame

	wxSemaphore context_notify{0, 1};
	wxSemaphore stream_notify{0, 1};
	wxSemaphore stream_success{0, 1};
	volatile int stream_success_val;

	pa_threaded_mainloop *mainloop = nullptr; // pulseaudio mainloop handle
	pa_context *context = nullptr; // connection context
	volatile pa_context_state_t cstate;

	pa_stream *stream = nullptr;
	volatile pa_stream_state_t sstate;

	volatile pa_usec_t play_start_time; // timestamp when playback was started

	int paerror = 0;

	/// Called by PA to notify about other context-related stuff
	static void pa_context_notify(pa_context *c, PulseAudioPlayer *thread);
	/// Called by PA when a stream operation completes
	static void pa_stream_success(pa_stream *p, int success, PulseAudioPlayer *thread);
	/// Called by PA to request more data written to stream
	static void pa_stream_write(pa_stream *p, size_t length, PulseAudioPlayer *thread);
	/// Called by PA to notify about other stream-related stuff
	static void pa_stream_notify(pa_stream *p, PulseAudioPlayer *thread);

public:
	PulseAudioPlayer(agi::AudioProvider *provider);
	~PulseAudioPlayer();

	void init();
	void uninit();
	void Play(int64_t start,int64_t count);
	void Stop();
	bool IsPlaying() { return is_playing; }

	int64_t GetEndPosition() { return end_frame; }
	int64_t GetCurrentPosition();
	void SetEndPosition(int64_t pos);

	void SetVolume(double vol) { volume = vol; }
};

PulseAudioPlayer::PulseAudioPlayer(agi::AudioProvider *provider) : AudioPlayer(provider) {
	init();
}

PulseAudioPlayer::~PulseAudioPlayer()
{
	uninit();
}

void PulseAudioPlayer::init()
{
	bool locked = false;
    
	// Initialise a mainloop
	mainloop = pa_threaded_mainloop_new();
	if (!mainloop)
	{
		throw AudioPlayerOpenError("Failed to initialise PulseAudio threaded mainloop object");
		uninit();
		return;
	}
		
	if (pa_threaded_mainloop_start(mainloop) < 0)
	{
		throw AudioPlayerOpenError("Failed to start PulseAudio threaded mainloop object");
		uninit();
		return;
	}

	pa_threaded_mainloop_lock(mainloop);
    locked = true;

	if (!(context = pa_context_new(pa_threaded_mainloop_get_api(mainloop), "Aegisub")))
    {
		if (locked)
			pa_threaded_mainloop_unlock(mainloop);

		if (context) 
		{
			pa_threaded_mainloop_lock(mainloop);
			
			if (!(pa_context_errno(context) == PA_ERR_CONNECTIONREFUSED))
				throw AudioPlayerOpenError("PulseAudio init failed");
			
			pa_threaded_mainloop_unlock(mainloop);
		}
        
        throw AudioPlayerOpenError("Failed to create PulseAudio context");
        uninit();
		return;
    }
	
	pa_context_set_state_callback(context, (pa_context_notify_cb_t)pa_context_notify, this);
    
	// Connect the context
	if (pa_context_connect(context, nullptr, PA_CONTEXT_NOAUTOSPAWN, nullptr) < 0)
	{
		if (locked)
			pa_threaded_mainloop_unlock(mainloop);

		if (context) 
		{
			pa_threaded_mainloop_lock(mainloop);
			
			if (!(pa_context_errno(context) == PA_ERR_CONNECTIONREFUSED))
				throw AudioPlayerOpenError("PulseAudio init failed");
			
			pa_threaded_mainloop_unlock(mainloop);
		}
		
		throw AudioPlayerOpenError("Failed to connect to PulseAudio context");
		uninit();
		return;
	}
    
    while (true) 
	{   
        if (cstate == PA_CONTEXT_READY)
            break;
        
        if (!PA_CONTEXT_IS_GOOD(cstate))
        {
			if (locked)
				pa_threaded_mainloop_unlock(mainloop);

			if (context) 
			{
				pa_threaded_mainloop_lock(mainloop);
				
				if (!(pa_context_errno(context) == PA_ERR_CONNECTIONREFUSED))
				{
					throw AudioPlayerOpenError("PulseAudio init failed");
					uninit();
					return;
				}
				
				pa_threaded_mainloop_unlock(mainloop);
			}
		}
        
        pa_threaded_mainloop_wait(mainloop);
    }

    pa_threaded_mainloop_unlock(mainloop);

	// Set up stream
	bpf = provider->GetChannels() * provider->GetBytesPerSample();
	pa_sample_spec ss;
	ss.format = PA_SAMPLE_S16LE; // FIXME
	ss.rate = provider->GetSampleRate();
	ss.channels = provider->GetChannels();
	pa_channel_map map;
	pa_channel_map_init_auto(&map, ss.channels, PA_CHANNEL_MAP_DEFAULT);

	stream = pa_stream_new(context, "Sound", &ss, &map);
	if (!stream) {
		// argh!
		pa_context_disconnect(context);
		pa_context_unref(context);
		pa_threaded_mainloop_stop(mainloop);
		pa_threaded_mainloop_free(mainloop);
		throw AudioPlayerOpenError("PulseAudio could not create stream");
	}
	pa_stream_set_state_callback(stream, (pa_stream_notify_cb_t)pa_stream_notify, this);
	pa_stream_set_write_callback(stream, (pa_stream_request_cb_t)pa_stream_write, this);

	// Connect stream
	paerror = pa_stream_connect_playback(stream, nullptr, nullptr, (pa_stream_flags_t)(PA_STREAM_INTERPOLATE_TIMING|PA_STREAM_NOT_MONOTONOUS|PA_STREAM_AUTO_TIMING_UPDATE), nullptr, nullptr);
	if (paerror) {
		LOG_E("audio/player/pulse") << "Stream connection failed: " << pa_strerror(paerror) << "(" << paerror << ")";
		throw AudioPlayerOpenError(std::string("PulseAudio reported error: ") + pa_strerror(paerror));
	}
	while (true) {
		stream_notify.Wait();
		if (sstate == PA_STREAM_READY) {
			break;
		} else if (sstate == PA_STREAM_FAILED) {
			paerror = pa_context_errno(context);
			LOG_E("audio/player/pulse") << "Stream connection failed: " << pa_strerror(paerror) << "(" << paerror << ")";
			throw AudioPlayerOpenError("PulseAudio player: Something went wrong connecting the stream");
		}
	}
}

void PulseAudioPlayer::uninit()
{
	if (is_playing) Stop();

    if (mainloop)
        pa_threaded_mainloop_stop(mainloop);

    if (stream) {
        pa_stream_disconnect(stream);
        pa_stream_unref(stream);
        stream = nullptr;
    }

    if (context) {
        pa_context_disconnect(context);
        pa_context_unref(context);
        context = nullptr;
    }

    if (mainloop) {
        pa_threaded_mainloop_free(mainloop);
        mainloop = nullptr;
    }
}

void PulseAudioPlayer::Play(int64_t start,int64_t count)
{
	if (is_playing) {
		// If we're already playing, do a quick "reset"
		is_playing = false;

		pa_threaded_mainloop_lock(mainloop);
		pa_operation *op = pa_stream_flush(stream, (pa_stream_success_cb_t)pa_stream_success, this);
		pa_threaded_mainloop_unlock(mainloop);
		stream_success.Wait();
		pa_operation_unref(op);
		if (!stream_success_val) {
			paerror = pa_context_errno(context);
			LOG_E("audio/player/pulse") << "Error flushing stream: " << pa_strerror(paerror) << "(" << paerror << ")";
		}
	}

	start_frame = start;
	cur_frame = start;
	end_frame = start + count;

	is_playing = true;

	play_start_time = 0;
	pa_threaded_mainloop_lock(mainloop);
	paerror = pa_stream_get_time(stream, (pa_usec_t*) &play_start_time);
	pa_threaded_mainloop_unlock(mainloop);
	if (paerror)
		LOG_E("audio/player/pulse") << "Error getting stream time: " << pa_strerror(paerror) << "(" << paerror << ")";

	PulseAudioPlayer::pa_stream_write(stream, pa_stream_writable_size(stream), this);

	pa_threaded_mainloop_lock(mainloop);
	pa_operation *op = pa_stream_trigger(stream, (pa_stream_success_cb_t)pa_stream_success, this);
	pa_threaded_mainloop_unlock(mainloop);
	stream_success.Wait();
	pa_operation_unref(op);
	if (!stream_success_val) {
		paerror = pa_context_errno(context);
		LOG_E("audio/player/pulse") << "Error triggering stream: " << pa_strerror(paerror) << "(" << paerror << ")";
	}
}

void PulseAudioPlayer::Stop()
{
	if (!is_playing) return;

	is_playing = false;

	start_frame = 0;
	cur_frame = 0;
	end_frame = 0;

	// Flush the stream of data
	pa_threaded_mainloop_lock(mainloop);
	pa_operation *op = pa_stream_flush(stream, (pa_stream_success_cb_t)pa_stream_success, this);
	pa_threaded_mainloop_unlock(mainloop);
	stream_success.Wait();
	pa_operation_unref(op);
	if (!stream_success_val) {
		paerror = pa_context_errno(context);
		LOG_E("audio/player/pulse") << "Error flushing stream: " << pa_strerror(paerror) << "(" << paerror << ")";
	}
}

void PulseAudioPlayer::SetEndPosition(int64_t pos)
{
	end_frame = pos;
}

int64_t PulseAudioPlayer::GetCurrentPosition()
{
	if (!is_playing) return 0;

	// FIXME: this should be based on not duration played but actual sample being heard
	// (during vidoeo playback, cur_frame might get changed to resync)

	// Calculation duration we have played, in microseconds
	pa_usec_t play_cur_time;
	pa_stream_get_time(stream, &play_cur_time);
	pa_usec_t playtime = play_cur_time - play_start_time;

	return start_frame + playtime * provider->GetSampleRate() / (1000*1000);
}

/// @brief Called by PA to notify about other context-related stuff
void PulseAudioPlayer::pa_context_notify(pa_context *c, PulseAudioPlayer *thread)
{
    switch (pa_context_get_state(c)) {
    case PA_CONTEXT_READY:
    case PA_CONTEXT_TERMINATED:
    case PA_CONTEXT_FAILED:
        pa_threaded_mainloop_signal(thread->mainloop, 0);
        break;
    }
	thread->stream_notify.Post();
}

/// @brief Called by PA when an operation completes
void PulseAudioPlayer::pa_stream_success(pa_stream *p, int success, PulseAudioPlayer *thread)
{
	thread->stream_success_val = success;
	thread->stream_success.Post();
}

/// @brief Called by PA to request more data (and other things?)
void PulseAudioPlayer::pa_stream_write(pa_stream *p, size_t length, PulseAudioPlayer *thread)
{
	if (!thread->is_playing) return;

	if (thread->cur_frame >= thread->end_frame + thread->provider->GetSampleRate()) {
		// More than a second past end of stream
		thread->is_playing = false;
		pa_operation *op = pa_stream_drain(p, nullptr, nullptr);
		pa_operation_unref(op);
		return;

	} else if (thread->cur_frame >= thread->end_frame) {
		// Past end of stream, but not a full second, add some silence
		void *buf = calloc(length, 1);
		::pa_stream_write(p, buf, length, free, 0, PA_SEEK_RELATIVE);
		thread->cur_frame += length / thread->bpf;
		return;
	}

	unsigned long bpf = thread->bpf;
	unsigned long frames = length / thread->bpf;
	unsigned long maxframes = thread->end_frame - thread->cur_frame;
	if (frames > maxframes) frames = maxframes;
	void *buf = malloc(frames * bpf);
	thread->provider->GetAudioWithVolume(buf, thread->cur_frame, frames, thread->volume);
	::pa_stream_write(p, buf, frames*bpf, free, 0, PA_SEEK_RELATIVE);
	thread->cur_frame += frames;
}

/// @brief Called by PA to notify about other stuff
void PulseAudioPlayer::pa_stream_notify(pa_stream *p, PulseAudioPlayer *thread)
{
	thread->sstate = pa_stream_get_state(thread->stream);
	thread->stream_notify.Post();
}
}

std::unique_ptr<AudioPlayer> CreatePulseAudioPlayer(agi::AudioProvider *provider, wxWindow *) {
	return agi::make_unique<PulseAudioPlayer>(provider);
}
#endif // WITH_LIBPULSE
