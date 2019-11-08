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

#include "video_controller.h"

#include "ass_dialogue.h"
#include "ass_file.h"
#include "audio_controller.h"
#include "compat.h"
#include "include/aegisub/context.h"
#include "options.h"
#include "project.h"
#include "selection_controller.h"
#include "time_range.h"
#include "async_video_provider.h"
#include "utils.h"

#include <libaegisub/ass/time.h>

#include <wx/log.h>

VideoController::VideoController(agi::Context *c)
: context(c)
, playAudioOnStep(OPT_GET("Audio/Plays When Stepping Video"))
, connections(agi::signal::make_vector({
	context->ass->AddCommitListener(&VideoController::OnSubtitlesCommit, this),
	context->project->AddVideoProviderListener(&VideoController::OnNewVideoProvider, this),
	context->selectionController->AddActiveLineListener(&VideoController::OnActiveLineChanged, this),
}))
{
	Bind(EVT_VIDEO_ERROR, &VideoController::OnVideoError, this);
	Bind(EVT_SUBTITLES_ERROR, &VideoController::OnSubtitlesError, this);
	playback.Bind(wxEVT_TIMER, &VideoController::OnPlayTimer, this);
}

void VideoController::OnNewVideoProvider(AsyncVideoProvider *new_provider) {
	Stop();
	provider = new_provider;
	color_matrix = provider ? provider->GetColorSpace() : "";
}

void VideoController::OnSubtitlesCommit(int type, const AssDialogue *changed) {
	if (!provider) return;

	if ((type & AssFile::COMMIT_SCRIPTINFO) || type == AssFile::COMMIT_NEW) {
		auto new_matrix = context->ass->GetScriptInfo("YCbCr Matrix");
		if (!new_matrix.empty() && new_matrix != color_matrix) {
			color_matrix = new_matrix;
			provider->SetColorSpace(new_matrix);
		}
	}

	if (!changed)
		provider->LoadSubtitles(context->ass.get());
	else
		provider->UpdateSubtitles(context->ass.get(), changed);
}

void VideoController::OnActiveLineChanged(AssDialogue *line) {
	if (line && provider && OPT_GET("Video/Subtitle Sync")->GetBool()) {
		Stop();
		JumpToTime(line->Start);
	}
}

void VideoController::RequestFrame() {
	context->ass->Properties.video_position = frame_n;
	provider->RequestFrame(frame_n, TimeAtFrame(frame_n));
}

void VideoController::JumpToFrame(int n) {
	if (!provider) return;

	bool was_playing = IsPlaying();
	if (was_playing)
		Stop();

	frame_n = mid(0, n, provider->GetFrameCount() - 1);
	RequestFrame();
	Seek(frame_n);

	if (was_playing)
		Play();
}

void VideoController::JumpToTime(int ms, agi::vfr::Time end) {
	JumpToFrame(FrameAtTime(ms, end));
}

void VideoController::NextFrame() {
	if (!provider || IsPlaying() || frame_n == provider->GetFrameCount())
		return;

	JumpToFrame(frame_n + 1);
	if (playAudioOnStep->GetBool())
		context->audioController->PlayRange(TimeRange(TimeAtFrame(frame_n - 1), TimeAtFrame(frame_n)));
}

void VideoController::PrevFrame() {
	if (!provider || IsPlaying() || frame_n == 0)
		return;

	JumpToFrame(frame_n - 1);
	if (playAudioOnStep->GetBool())
		context->audioController->PlayRange(TimeRange(TimeAtFrame(frame_n), TimeAtFrame(frame_n + 1)));
}

void VideoController::Play() {
	if (IsPlaying()) {
		Stop();
		return;
	}

	if (!provider) return;

	playback_mode = PM_ToEnd;

	start_ms = TimeAtFrame(frame_n);
	end_frame = provider->GetFrameCount() - 1;

	context->audioController->PlayToEnd(start_ms);

	playback_start_time = std::chrono::steady_clock::now();
	playback.Start(10);
}

void VideoController::PlayLine() {
	Stop();

	AssDialogue *curline = context->selectionController->GetActiveLine();
	if (!curline) return;

	context->audioController->PlayRange(TimeRange(curline->Start, curline->End));

	// Round-trip conversion to convert start to exact
	int startFrame = FrameAtTime(context->selectionController->GetActiveLine()->Start, agi::vfr::START);
	start_ms = TimeAtFrame(startFrame);
	end_frame = FrameAtTime(context->selectionController->GetActiveLine()->End, agi::vfr::END) + 1;

	JumpToFrame(startFrame);

	playback_start_time = std::chrono::steady_clock::now();
	playback.Start(10);
}

void VideoController::PlayRange(const TimeRange &range) {
	Stop();

	context->audioController->PlayRange(range);

	start_ms = range.begin();
	// Round-trip conversion to convert start to exact
	int startFrame = FrameAtTime(range.begin(), agi::vfr::START);
	end_frame = FrameAtTime(range.end(), agi::vfr::END) + 1;

	playback_mode = PM_Range;
	JumpToFrame(startFrame);

	playback_start_time = std::chrono::steady_clock::now();
	playback.Start(10);
}

void VideoController::PlayPrimaryRange(const TimeRange &range) {
	Stop();

	context->audioController->PlayPrimaryRange();

	start_ms = range.begin();
	// Round-trip conversion to convert start to exact
	int startFrame = FrameAtTime(range.begin(), agi::vfr::START);
	end_frame = FrameAtTime(range.end(), agi::vfr::END) + 1;

	JumpToFrame(startFrame);

	playback_start_time = std::chrono::steady_clock::now();
	playback.Start(10);

	playback_mode = PM_PrimaryRange;
}

void VideoController::PlayToEndOfPrimary(const TimeRange &range) {
	Stop();

	context->audioController->PlayToEndOfPrimary(range.begin());

	start_ms = range.begin();
	// Round-trip conversion to convert start to exact
	int startFrame = FrameAtTime(range.begin(), agi::vfr::START);
	end_frame = FrameAtTime(range.end(), agi::vfr::END) + 1;

	JumpToFrame(startFrame);

	playback_start_time = std::chrono::steady_clock::now();
	playback.Start(10);

	playback_mode = PM_PrimaryRange;
}


void VideoController::PlayToEnd(int start_ms) {
	Stop();

	context->audioController->PlayToEnd(start_ms);

	this->start_ms = start_ms;
	playback_mode = PM_ToEnd;
	end_frame = provider->GetFrameCount() - 1;

	int start_frame = FrameAtTime(start_ms);

	JumpToFrame(start_frame);

	playback_start_time = std::chrono::steady_clock::now();
	playback.Start(10);
}

void VideoController::Stop() {
	if (IsPlaying()) {
		playback.Stop();
		context->audioController->Stop();
	}
	playback_mode = PM_NotPlaying;
}

void VideoController::OnPlayTimer(wxTimerEvent &) {
	using namespace std::chrono;
	int next_frame = FrameAtTime(start_ms + duration_cast<milliseconds>(steady_clock::now() - playback_start_time).count());
	if (next_frame == frame_n) return;

	if (next_frame >= end_frame)
		Stop();
	else {
		if (!IsPlaying() ||
			(playback_mode != PM_ToEnd && next_frame >= end_frame + 1)) {
			// The +1 is to allow the player to end the sound output cleanly,
			// otherwise a popping artifact can sometimes be heard.
			Stop();
		} else {
			frame_n = next_frame;
			RequestFrame();
			Seek(frame_n);
		}
	}
}

double VideoController::GetARFromType(AspectRatio type) const {
	switch (type) {
		case AspectRatio::Default:    return (double)provider->GetWidth()/provider->GetHeight();
		case AspectRatio::Fullscreen: return 4.0/3.0;
		case AspectRatio::Widescreen: return 16.0/9.0;
		case AspectRatio::Cinematic:  return 2.35;
        default: throw agi::InternalError("Bad AR type");
	}
}

void VideoController::SetAspectRatio(double value) {
	ar_type = AspectRatio::Custom;
	ar_value = mid(.5, value, 5.);
	context->ass->Properties.ar_mode = (int)ar_type;
	context->ass->Properties.ar_value = ar_value;
	ARChange(ar_type, ar_value);
}

void VideoController::SetAspectRatio(AspectRatio type) {
	ar_value = mid(.5, GetARFromType(type), 5.);
	ar_type = type;
	context->ass->Properties.ar_mode = (int)ar_type;
	context->ass->Properties.ar_value = ar_value;
	ARChange(ar_type, ar_value);
}

int VideoController::TimeAtFrame(int frame, agi::vfr::Time type) const {
	return context->project->Timecodes().TimeAtFrame(frame, type);
}

int VideoController::FrameAtTime(int time, agi::vfr::Time type) const {
	return context->project->Timecodes().FrameAtTime(time, type);
}

void VideoController::OnVideoError(VideoProviderErrorEvent const& err) {
	wxLogError(
		"Failed seeking video. The video file may be corrupt or incomplete.\n"
		"Error message reported: %s",
		to_wx(err.GetMessage()));
}

void VideoController::OnSubtitlesError(SubtitlesProviderErrorEvent const& err) {
	wxLogError(
		"Failed rendering subtitles. Error message reported: %s",
		to_wx(err.GetMessage()));
}
