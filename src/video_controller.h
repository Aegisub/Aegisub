// Copyright (c) 2005-2007, Rodrigo Braz Monteiro
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

#include <libaegisub/signal.h>
#include <libaegisub/vfr.h>

#include <chrono>
#include <set>

#include <wx/timer.h>


class TimeRange;
class AssDialogue;
class AsyncVideoProvider;
struct SubtitlesProviderErrorEvent;
struct VideoProviderErrorEvent;

namespace agi {
	struct Context;
	class OptionValue;
}

enum class AspectRatio {
	Default = 0,
	Fullscreen,
	Widescreen,
	Cinematic,
	Custom
};

/// Manage stuff related to video playback
class VideoController final : public wxEvtHandler {
	/// Current frame number changed (new frame number)
	agi::signal::Signal<int> Seek;
	/// Aspect ratio was changed (type, value)
	agi::signal::Signal<AspectRatio, double> ARChange;

	agi::Context *context;

	/// The video provider owned by the threaded frame source, or nullptr if no
	/// video is open
	AsyncVideoProvider *provider = nullptr;

	/// Last seen script color matrix
	std::string color_matrix;

	/// Playback timer used to periodically check if we should go to the next
	/// frame while playing video
	wxTimer playback;

	/// Time when playback was last started
	std::chrono::steady_clock::time_point playback_start_time;

	/// The start time of the first frame of the current playback; undefined if
	/// video is not currently playing
	int start_ms = 0;

	/// The last frame to play if video is currently playing
	int end_frame = 0;

	/// The frame number which was last requested from the video provider,
	/// which may not be the same thing as the currently displayed frame
	int frame_n = 0;

	enum PlaybackMode {
		PM_NotPlaying,
		PM_Range,
		PM_PrimaryRange,
		PM_ToEnd
	};
	/// The current playback mode
	PlaybackMode playback_mode = PM_NotPlaying;

	/// The picture aspect ratio of the video if the aspect ratio has been
	/// overridden by the user
	double ar_value = 1.;

	/// The current AR type
	AspectRatio ar_type = AspectRatio::Default;

	/// Cached option for audio playing when frame stepping
	const agi::OptionValue* playAudioOnStep;

	std::vector<agi::signal::Connection> connections;

	void OnPlayTimer(wxTimerEvent &event);

	void OnVideoError(VideoProviderErrorEvent const& err);
	void OnSubtitlesError(SubtitlesProviderErrorEvent const& err);

	void OnSubtitlesCommit(int type, const AssDialogue *changed);
	void OnNewVideoProvider(AsyncVideoProvider *provider);
	void OnActiveLineChanged(AssDialogue *line);

	void RequestFrame();

public:
	VideoController(agi::Context *context);

	/// Is the video currently playing?
	bool IsPlaying() const { return playback.IsRunning(); }

	/// Is video loaded?
	bool HasVideo() const { return provider != nullptr; }

	/// Get the current frame number
	int GetFrameN() const { return frame_n; }

	/// Get the actual aspect ratio from a predefined AR type
	double GetARFromType(AspectRatio type) const;

	/// Override the aspect ratio of the currently loaded video
	void SetAspectRatio(double value);

	/// Override the aspect ratio of the currently loaded video
	/// @param type Predefined type to set the AR to. Must not be Custom.
	void SetAspectRatio(AspectRatio type);

	/// Get the current AR type
	AspectRatio GetAspectRatioType() const { return ar_type; }

	/// Get the current aspect ratio of the video
	double GetAspectRatioValue() const { return ar_value; }

	/// @brief Jump to the beginning of a frame
	/// @param n Frame number to jump to
	void JumpToFrame(int n);
	/// @brief Jump to a time
	/// @param ms Time to jump to in milliseconds
	/// @param end Type of time
	void JumpToTime(int ms, agi::vfr::Time end = agi::vfr::START);

	/// Starting playing the video
	void Play();
	/// Play the next frame then stop
	void NextFrame();
	/// Play the previous frame then stop
	void PrevFrame();
	/// Seek to the beginning of the current line, then play to the end of it
	void PlayLine();
	/// Play a time range
	void PlayRange(const TimeRange &range);
	/// Play primary range
	void PlayPrimaryRange(const TimeRange &range);
	/// Play to end of primary range
	void PlayToEndOfPrimary(const TimeRange &range);
	/// Play to end
	void PlayToEnd(int start_ms);
	/// Stop playing
	void Stop();

	DEFINE_SIGNAL_ADDERS(Seek, AddSeekListener)
	DEFINE_SIGNAL_ADDERS(ARChange, AddARChangeListener)

	int TimeAtFrame(int frame, agi::vfr::Time type = agi::vfr::EXACT) const;
	int FrameAtTime(int time, agi::vfr::Time type = agi::vfr::EXACT) const;
};
