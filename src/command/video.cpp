// Copyright (c) 2005-2010, Niels Martin Hansen
// Copyright (c) 2005-2010, Rodrigo Braz Monteiro
// Copyright (c) 2010, Amar Takhar
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//	 this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//	 this list of conditions and the following disclaimer in the documentation
//	 and/or other materials provided with the distribution.
//   * Neither the name of the Aegisub Group nor the names of its contributors
//	 may be used to endorse or promote products derived from this software
//	 without specific prior written permission.
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

#include "command.h"

#include "../ass_dialogue.h"
#include "../async_video_provider.h"
#include "../compat.h"
#include "../dialog_detached_video.h"
#include "../dialog_manager.h"
#include "../dialogs.h"
#include "../format.h"
#include "../frame_main.h"
#include "../include/aegisub/context.h"
#include "../include/aegisub/subtitles_provider.h"
#include "../libresrc/libresrc.h"
#include "../options.h"
#include "../project.h"
#include "../selection_controller.h"
#include "../utils.h"
#include "../video_controller.h"
#include "../video_display.h"
#include "../video_frame.h"

#include <libaegisub/ass/time.h>
#include <libaegisub/fs.h>
#include <libaegisub/path.h>
#include <libaegisub/make_unique.h>
#include <libaegisub/util.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <wx/msgdlg.h>
#include <wx/textdlg.h>

namespace {
	using cmd::Command;

struct validator_video_loaded : public Command {
	CMD_TYPE(COMMAND_VALIDATE)
	bool Validate(const agi::Context *c) override {
		return !!c->project->VideoProvider();
	}
};

struct validator_video_attached : public Command {
	CMD_TYPE(COMMAND_VALIDATE)
	bool Validate(const agi::Context *c) override {
		return !!c->project->VideoProvider() && !c->dialog->Get<DialogDetachedVideo>();
	}
};

struct video_aspect_cinematic final : public validator_video_loaded {
	CMD_NAME("video/aspect/cinematic")
	STR_MENU("&Cinematic (2.35)")
	STR_DISP("Cinematic (2.35)")
	STR_HELP("Force video to 2.35 aspect ratio")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_RADIO)

	bool IsActive(const agi::Context *c) override {
		return c->videoController->GetAspectRatioType() == AspectRatio::Cinematic;
	}

	void operator()(agi::Context *c) override {
		c->videoController->Stop();
		c->videoController->SetAspectRatio(AspectRatio::Cinematic);
		c->frame->SetDisplayMode(1,-1);
	}
};

struct video_aspect_custom final : public validator_video_loaded {
	CMD_NAME("video/aspect/custom")
	STR_MENU("C&ustom...")
	STR_DISP("Custom")
	STR_HELP("Force video to a custom aspect ratio")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_RADIO)

	bool IsActive(const agi::Context *c) override {
		return c->videoController->GetAspectRatioType() == AspectRatio::Custom;
	}

	void operator()(agi::Context *c) override {
		c->videoController->Stop();

		std::string value = from_wx(wxGetTextFromUser(
			_("Enter aspect ratio in either:\n  decimal (e.g. 2.35)\n  fractional (e.g. 16:9)\n  specific resolution (e.g. 853x480)"),
			_("Enter aspect ratio"),
			std::to_wstring(c->videoController->GetAspectRatioValue())));
		if (value.empty()) return;

		double numval = 0;
		if (agi::util::try_parse(value, &numval)) {
			//Nothing to see here, move along
		}
		else {
			std::vector<std::string> chunks;
			split(chunks, value, boost::is_any_of(":/xX"));
			if (chunks.size() == 2) {
				double num, den;
				if (agi::util::try_parse(chunks[0], &num) && agi::util::try_parse(chunks[1], &den))
					numval = num / den;
			}
		}

		if (numval < 0.5 || numval > 5.0)
			wxMessageBox(_("Invalid value! Aspect ratio must be between 0.5 and 5.0."),_("Invalid Aspect Ratio"),wxOK | wxICON_ERROR | wxCENTER);
		else {
			c->videoController->SetAspectRatio(numval);
			c->frame->SetDisplayMode(1,-1);
		}
	}
};

struct video_aspect_default final : public validator_video_loaded {
	CMD_NAME("video/aspect/default")
	STR_MENU("&Default")
	STR_DISP("Default")
	STR_HELP("Use video's original aspect ratio")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_RADIO)

	bool IsActive(const agi::Context *c) override {
		return c->videoController->GetAspectRatioType() == AspectRatio::Default;
	}

	void operator()(agi::Context *c) override {
		c->videoController->Stop();
		c->videoController->SetAspectRatio(AspectRatio::Default);
		c->frame->SetDisplayMode(1,-1);
	}
};

struct video_aspect_full final : public validator_video_loaded {
	CMD_NAME("video/aspect/full")
	STR_MENU("&Fullscreen (4:3)")
	STR_DISP("Fullscreen (4:3)")
	STR_HELP("Force video to 4:3 aspect ratio")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_RADIO)

	bool IsActive(const agi::Context *c) override {
		return c->videoController->GetAspectRatioType() == AspectRatio::Fullscreen;
	}

	void operator()(agi::Context *c) override {
		c->videoController->Stop();
		c->videoController->SetAspectRatio(AspectRatio::Fullscreen);
		c->frame->SetDisplayMode(1,-1);
	}
};

struct video_aspect_wide final : public validator_video_loaded {
	CMD_NAME("video/aspect/wide")
	STR_MENU("&Widescreen (16:9)")
	STR_DISP("Widescreen (16:9)")
	STR_HELP("Force video to 16:9 aspect ratio")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_RADIO)

	bool IsActive(const agi::Context *c) override {
		return c->videoController->GetAspectRatioType() == AspectRatio::Widescreen;
	}

	void operator()(agi::Context *c) override {
		c->videoController->Stop();
		c->videoController->SetAspectRatio(AspectRatio::Widescreen);
		c->frame->SetDisplayMode(1,-1);
	}
};

struct video_close final : public validator_video_loaded {
	CMD_NAME("video/close")
	CMD_ICON(close_video_menu)
	STR_MENU("&Close Video")
	STR_DISP("Close Video")
	STR_HELP("Close the currently open video file")

	void operator()(agi::Context *c) override {
		c->project->CloseVideo();
	}
};

struct video_copy_coordinates final : public validator_video_loaded {
	CMD_NAME("video/copy_coordinates")
	STR_MENU("Copy coordinates to Clipboard")
	STR_DISP("Copy coordinates to Clipboard")
	STR_HELP("Copy the current coordinates of the mouse over the video to the clipboard")

	void operator()(agi::Context *c) override {
		SetClipboard(c->videoDisplay->GetMousePosition().Str());
	}
};

struct video_cycle_subtitles_provider final : public cmd::Command {
	CMD_NAME("video/subtitles_provider/cycle")
	STR_MENU("Cycle active subtitles provider")
	STR_DISP("Cycle active subtitles provider")
	STR_HELP("Cycle through the available subtitles providers")

	void operator()(agi::Context *c) override {
		auto providers = SubtitlesProviderFactory::GetClasses();
		if (providers.empty()) return;

		auto it = find(begin(providers), end(providers), OPT_GET("Subtitle/Provider")->GetString());
		if (it != end(providers)) ++it;
		if (it == end(providers)) it = begin(providers);

		OPT_SET("Subtitle/Provider")->SetString(*it);
		c->frame->StatusTimeout(fmt_tl("Subtitles provider set to %s", *it), 5000);
	}
};

struct video_detach final : public validator_video_loaded {
	CMD_NAME("video/detach")
	CMD_ICON(detach_video_menu)
	STR_MENU("&Detach Video")
	STR_DISP("Detach Video")
	STR_HELP("Detach the video display from the main window, displaying it in a separate Window")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_TOGGLE)

	bool IsActive(const agi::Context *c) override {
		return !!c->dialog->Get<DialogDetachedVideo>();
	}

	void operator()(agi::Context *c) override {
		if (DialogDetachedVideo *d = c->dialog->Get<DialogDetachedVideo>())
			d->Close();
		else
			c->dialog->Show<DialogDetachedVideo>(c);
	}
};

struct video_details final : public validator_video_loaded {
	CMD_NAME("video/details")
	CMD_ICON(show_video_details_menu)
	STR_MENU("Show &Video Details")
	STR_DISP("Show Video Details")
	STR_HELP("Show video details")

	void operator()(agi::Context *c) override {
		c->videoController->Stop();
		ShowVideoDetailsDialog(c);
	}
};

struct video_focus_seek final : public validator_video_loaded {
	CMD_NAME("video/focus_seek")
	STR_MENU("Toggle video slider focus")
	STR_DISP("Toggle video slider focus")
	STR_HELP("Toggle focus between the video slider and the previous thing to have focus")

	void operator()(agi::Context *c) override {
		wxWindow *curFocus = wxWindow::FindFocus();
		if (curFocus == c->videoSlider) {
			if (c->previousFocus) c->previousFocus->SetFocus();
		}
		else {
			c->previousFocus = curFocus;
			c->videoSlider->SetFocus();
		}
	}
};

wxImage get_image(agi::Context *c, bool raw) {
	auto frame = c->videoController->GetFrameN();
	return GetImage(*c->project->VideoProvider()->GetFrame(frame, c->project->Timecodes().TimeAtFrame(frame), raw));
}

struct video_frame_copy final : public validator_video_loaded {
	CMD_NAME("video/frame/copy")
	STR_MENU("Copy image to Clipboard")
	STR_DISP("Copy image to Clipboard")
	STR_HELP("Copy the currently displayed frame to the clipboard")

	void operator()(agi::Context *c) override {
		SetClipboard(wxBitmap(get_image(c, false), 24));
	}
};

struct video_frame_copy_raw final : public validator_video_loaded {
	CMD_NAME("video/frame/copy/raw")
	STR_MENU("Copy image to Clipboard (no subtitles)")
	STR_DISP("Copy image to Clipboard (no subtitles)")
	STR_HELP("Copy the currently displayed frame to the clipboard, without the subtitles")

	void operator()(agi::Context *c) override {
		SetClipboard(wxBitmap(get_image(c, true), 24));
	}
};

struct video_frame_next final : public validator_video_loaded {
	CMD_NAME("video/frame/next")
	STR_MENU("Next Frame")
	STR_DISP("Next Frame")
	STR_HELP("Seek to the next frame")

	void operator()(agi::Context *c) override {
		c->videoController->NextFrame();
	}
};

struct video_frame_next_boundary final : public validator_video_loaded {
	CMD_NAME("video/frame/next/boundary")
	STR_MENU("Next Boundary")
	STR_DISP("Next Boundary")
	STR_HELP("Seek to the next beginning or end of a subtitle")

	void operator()(agi::Context *c) override {
		AssDialogue *active_line = c->selectionController->GetActiveLine();
		if (!active_line) return;

		int target = c->videoController->FrameAtTime(active_line->Start, agi::vfr::START);
		if (target > c->videoController->GetFrameN()) {
			c->videoController->JumpToFrame(target);
			return;
		}

		target = c->videoController->FrameAtTime(active_line->End, agi::vfr::END);
		if (target > c->videoController->GetFrameN()) {
			c->videoController->JumpToFrame(target);
			return;
		}

		c->selectionController->NextLine();
		AssDialogue *new_line = c->selectionController->GetActiveLine();
		if (new_line != active_line)
		c->videoController->JumpToTime(new_line->Start);
	}
};

struct video_frame_next_keyframe final : public validator_video_loaded {
	CMD_NAME("video/frame/next/keyframe")
	STR_MENU("Next Keyframe")
	STR_DISP("Next Keyframe")
	STR_HELP("Seek to the next keyframe")

	void operator()(agi::Context *c) override {
		auto const& kf = c->project->Keyframes();
		auto pos = lower_bound(kf.begin(), kf.end(), c->videoController->GetFrameN() + 1);

		c->videoController->JumpToFrame(pos == kf.end() ? c->project->VideoProvider()->GetFrameCount() - 1 : *pos);
	}
};

struct video_frame_next_large final : public validator_video_loaded {
	CMD_NAME("video/frame/next/large")
	STR_MENU("Fast jump forward")
	STR_DISP("Fast jump forward")
	STR_HELP("Fast jump forward")

	void operator()(agi::Context *c) override {
		c->videoController->JumpToFrame(
			c->videoController->GetFrameN() +
			OPT_GET("Video/Slider/Fast Jump Step")->GetInt());
	}
};

struct video_frame_prev final : public validator_video_loaded {
	CMD_NAME("video/frame/prev")
	STR_MENU("Previous Frame")
	STR_DISP("Previous Frame")
	STR_HELP("Seek to the previous frame")

	void operator()(agi::Context *c) override {
		c->videoController->PrevFrame();
	}
};

struct video_frame_prev_boundary final : public validator_video_loaded {
	CMD_NAME("video/frame/prev/boundary")
	STR_MENU("Previous Boundary")
	STR_DISP("Previous Boundary")
	STR_HELP("Seek to the previous beginning or end of a subtitle")

	void operator()(agi::Context *c) override {
		AssDialogue *active_line = c->selectionController->GetActiveLine();
		if (!active_line) return;

		int target = c->videoController->FrameAtTime(active_line->End, agi::vfr::END);
		if (target < c->videoController->GetFrameN()) {
			c->videoController->JumpToFrame(target);
			return;
		}

		target = c->videoController->FrameAtTime(active_line->Start, agi::vfr::START);
		if (target < c->videoController->GetFrameN()) {
			c->videoController->JumpToFrame(target);
			return;
		}

		c->selectionController->PrevLine();
		AssDialogue *new_line = c->selectionController->GetActiveLine();
		if (new_line != active_line)
			c->videoController->JumpToTime(new_line->End, agi::vfr::END);
	}
};

struct video_frame_prev_keyframe final : public validator_video_loaded {
	CMD_NAME("video/frame/prev/keyframe")
	STR_MENU("Previous Keyframe")
	STR_DISP("Previous Keyframe")
	STR_HELP("Seek to the previous keyframe")

	void operator()(agi::Context *c) override {
		auto const& kf = c->project->Keyframes();
		if (kf.empty()) {
			c->videoController->JumpToFrame(0);
			return;
		}

		auto pos = lower_bound(kf.begin(), kf.end(), c->videoController->GetFrameN());

		if (pos != kf.begin())
			--pos;

		c->videoController->JumpToFrame(*pos);
	}
};

struct video_frame_prev_large final : public validator_video_loaded {
	CMD_NAME("video/frame/prev/large")
	STR_MENU("Fast jump backwards")
	STR_DISP("Fast jump backwards")
	STR_HELP("Fast jump backwards")

	void operator()(agi::Context *c) override {
		c->videoController->JumpToFrame(
			c->videoController->GetFrameN() -
			OPT_GET("Video/Slider/Fast Jump Step")->GetInt());
	}
};

static void save_snapshot(agi::Context *c, bool raw) {
	auto option = OPT_GET("Path/Screenshot")->GetString();
	agi::fs::path basepath;

	auto videoname = c->project->VideoName();
	bool is_dummy = boost::starts_with(videoname.string(), "?dummy");

	// Is it a path specifier and not an actual fixed path?
	if (option[0] == '?') {
		// If dummy video is loaded, we can't save to the video location
		if (boost::starts_with(option, "?video") && is_dummy) {
			// So try the script location instead
			option = "?script";
		}
		// Find out where the ?specifier points to
		basepath = c->path->Decode(option);
		// If where ever that is isn't defined, we can't save there
		if ((basepath == "\\") || (basepath == "/")) {
			// So save to the current user's home dir instead
			basepath = wxGetHomeDir().c_str();
		}
	}
	// Actual fixed (possibly relative) path, decode it
	else
		basepath = c->path->MakeAbsolute(option, "?user/");

	basepath /= is_dummy ? "dummy" : videoname.stem();

	// Get full path
	int session_shot_count = 1;
	std::string path;
	do {
		path = agi::format("%s_%03d_%d.png", basepath.string(), session_shot_count++, c->videoController->GetFrameN());
	} while (agi::fs::FileExists(path));

	get_image(c, raw).SaveFile(to_wx(path), wxBITMAP_TYPE_PNG);
}

struct video_frame_save final : public validator_video_loaded {
	CMD_NAME("video/frame/save")
	STR_MENU("Save PNG snapshot")
	STR_DISP("Save PNG snapshot")
	STR_HELP("Save the currently displayed frame to a PNG file in the video's directory")

	void operator()(agi::Context *c) override {
		save_snapshot(c, false);
	}
};

struct video_frame_save_raw final : public validator_video_loaded {
	CMD_NAME("video/frame/save/raw")
	STR_MENU("Save PNG snapshot (no subtitles)")
	STR_DISP("Save PNG snapshot (no subtitles)")
	STR_HELP("Save the currently displayed frame without the subtitles to a PNG file in the video's directory")

	void operator()(agi::Context *c) override {
		save_snapshot(c, true);
	}
};

struct video_jump final : public validator_video_loaded {
	CMD_NAME("video/jump")
	CMD_ICON(jumpto_button)
	STR_MENU("&Jump to...")
	STR_DISP("Jump to")
	STR_HELP("Jump to frame or time")

	void operator()(agi::Context *c) override {
		c->videoController->Stop();
		ShowJumpToDialog(c);
		c->videoSlider->SetFocus();
	}
};

struct video_jump_end final : public validator_video_loaded {
	CMD_NAME("video/jump/end")
	CMD_ICON(video_to_subend)
	STR_MENU("Jump Video to &End")
	STR_DISP("Jump Video to End")
	STR_HELP("Jump the video to the end frame of current subtitle")

	void operator()(agi::Context *c) override {
		if (auto active_line = c->selectionController->GetActiveLine())
			c->videoController->JumpToTime(active_line->End, agi::vfr::END);
	}
};

struct video_jump_start final : public validator_video_loaded {
	CMD_NAME("video/jump/start")
	CMD_ICON(video_to_substart)
	STR_MENU("Jump Video to &Start")
	STR_DISP("Jump Video to Start")
	STR_HELP("Jump the video to the start frame of current subtitle")

	void operator()(agi::Context *c) override {
		if (auto active_line = c->selectionController->GetActiveLine())
			c->videoController->JumpToTime(active_line->Start);
	}
};

struct video_open final : public Command {
	CMD_NAME("video/open")
	CMD_ICON(open_video_menu)
	STR_MENU("&Open Video...")
	STR_DISP("Open Video")
	STR_HELP("Open a video file")

	void operator()(agi::Context *c) override {
		auto str = from_wx(_("Video Formats") + " (*.asf,*.avi,*.avs,*.d2v,*.h264,*.hevc,*.m2ts,*.m4v,*.mkv,*.mov,*.mp4,*.mpeg,*.mpg,*.ogm,*.webm,*.wmv,*.ts,*.y4m,*.yuv)|*.asf;*.avi;*.avs;*.d2v;*.h264;*.hevc;*.m2ts;*.m4v;*.mkv;*.mov;*.mp4;*.mpeg;*.mpg;*.ogm;*.webm;*.wmv;*.ts;*.y4m;*.yuv|"
		         + _("All Files") + " (*.*)|*.*");
		auto filename = OpenFileSelector(_("Open video file"), "Path/Last/Video", "", "", str, c->parent);
		if (!filename.empty())
			c->project->LoadVideo(filename);
	}
};

struct video_open_dummy final : public Command {
	CMD_NAME("video/open/dummy")
	CMD_ICON(use_dummy_video_menu)
	STR_MENU("&Use Dummy Video...")
	STR_DISP("Use Dummy Video")
	STR_HELP("Open a placeholder video clip with solid color")

	void operator()(agi::Context *c) override {
		std::string fn = CreateDummyVideo(c->parent);
		if (!fn.empty())
			c->project->LoadVideo(fn);
	}
};

struct video_opt_autoscroll final : public Command {
	CMD_NAME("video/opt/autoscroll")
	CMD_ICON(toggle_video_autoscroll)
	STR_MENU("Toggle autoscroll of video")
	STR_DISP("Toggle autoscroll of video")
	STR_HELP("Toggle automatically seeking video to the start time of selected lines")
	CMD_TYPE(COMMAND_TOGGLE)

	bool IsActive(const agi::Context *) override {
		return OPT_GET("Video/Subtitle Sync")->GetBool();
	}

	void operator()(agi::Context *) override {
		OPT_SET("Video/Subtitle Sync")->SetBool(!OPT_GET("Video/Subtitle Sync")->GetBool());
	}
};

struct video_pan_reset final : public validator_video_loaded {
	CMD_NAME("video/pan_reset")
	STR_MENU("Reset video pan")
	STR_DISP("Reset video pan")
	STR_HELP("Reset the video pan to the original value")

	void operator()(agi::Context *c) override {
		c->videoDisplay->ResetPan();
	}
};

struct video_play final : public validator_video_loaded {
	CMD_NAME("video/play")
	CMD_ICON(button_play)
	STR_MENU("Play")
	STR_DISP("Play")
	STR_HELP("Play video starting on this position")

	void operator()(agi::Context *c) override {
		c->videoController->Play();
	}
};

struct video_play_line final : public validator_video_loaded {
	CMD_NAME("video/play/line")
	CMD_ICON(button_playline)
	STR_MENU("Play line")
	STR_DISP("Play line")
	STR_HELP("Play current line")

	void operator()(agi::Context *c) override {
		c->videoController->PlayLine();
	}
};

struct video_show_overscan final : public validator_video_loaded {
	CMD_NAME("video/show_overscan")
	STR_MENU("Show &Overscan Mask")
	STR_DISP("Show Overscan Mask")
	STR_HELP("Show a mask over the video, indicating areas that might get cropped off by overscan on televisions")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_TOGGLE)

	bool IsActive(const agi::Context *) override {
		return OPT_GET("Video/Overscan Mask")->GetBool();
	}

	void operator()(agi::Context *c) override {
		OPT_SET("Video/Overscan Mask")->SetBool(!OPT_GET("Video/Overscan Mask")->GetBool());
		c->videoDisplay->Render();
	}
};

class video_zoom_100: public validator_video_attached {
public:
	CMD_NAME("video/zoom/100")
	STR_MENU("&100%")
	STR_DISP("100%")
	STR_HELP("Set zoom to 100%")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_RADIO)

	bool IsActive(const agi::Context *c) override {
		return c->videoDisplay->GetZoom() == 1.;
	}

	void operator()(agi::Context *c) override {
		c->videoController->Stop();
		c->videoDisplay->SetZoom(1.);
	}
};

class video_stop: public validator_video_loaded {
public:
	CMD_NAME("video/stop")
	CMD_ICON(button_pause)
	STR_MENU("Stop video")
	STR_DISP("Stop video")
	STR_HELP("Stop video playback")

	void operator()(agi::Context *c) override {
		c->videoController->Stop();
	}
};

class video_zoom_200: public validator_video_attached {
public:
	CMD_NAME("video/zoom/200")
	STR_MENU("&200%")
	STR_DISP("200%")
	STR_HELP("Set zoom to 200%")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_RADIO)

	bool IsActive(const agi::Context *c) override {
		return c->videoDisplay->GetZoom() == 2.;
	}

	void operator()(agi::Context *c) override {
		c->videoController->Stop();
		c->videoDisplay->SetZoom(2.);
	}
};

class video_zoom_50: public validator_video_attached {
public:
	CMD_NAME("video/zoom/50")
	STR_MENU("&50%")
	STR_DISP("50%")
	STR_HELP("Set zoom to 50%")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_RADIO)

	bool IsActive(const agi::Context *c) override {
		return c->videoDisplay->GetZoom() == .5;
	}

	void operator()(agi::Context *c) override {
		c->videoController->Stop();
		c->videoDisplay->SetZoom(.5);
	}
};

struct video_zoom_in final : public validator_video_attached {
	CMD_NAME("video/zoom/in")
	CMD_ICON(zoom_in_button)
	STR_MENU("Zoom In")
	STR_DISP("Zoom In")
	STR_HELP("Zoom video in")

	void operator()(agi::Context *c) override {
		c->videoDisplay->SetZoom(c->videoDisplay->GetZoom() + .125);
	}
};

struct video_zoom_out final : public validator_video_attached {
	CMD_NAME("video/zoom/out")
	CMD_ICON(zoom_out_button)
	STR_MENU("Zoom Out")
	STR_DISP("Zoom Out")
	STR_HELP("Zoom video out")

	void operator()(agi::Context *c) override {
		c->videoDisplay->SetZoom(c->videoDisplay->GetZoom() - .125);
	}
};
}

namespace cmd {
	void init_video() {
		reg(agi::make_unique<video_aspect_cinematic>());
		reg(agi::make_unique<video_aspect_custom>());
		reg(agi::make_unique<video_aspect_default>());
		reg(agi::make_unique<video_aspect_full>());
		reg(agi::make_unique<video_aspect_wide>());
		reg(agi::make_unique<video_close>());
		reg(agi::make_unique<video_copy_coordinates>());
		reg(agi::make_unique<video_cycle_subtitles_provider>());
		reg(agi::make_unique<video_detach>());
		reg(agi::make_unique<video_details>());
		reg(agi::make_unique<video_focus_seek>());
		reg(agi::make_unique<video_frame_copy>());
		reg(agi::make_unique<video_frame_copy_raw>());
		reg(agi::make_unique<video_frame_next>());
		reg(agi::make_unique<video_frame_next_boundary>());
		reg(agi::make_unique<video_frame_next_keyframe>());
		reg(agi::make_unique<video_frame_next_large>());
		reg(agi::make_unique<video_frame_prev>());
		reg(agi::make_unique<video_frame_prev_boundary>());
		reg(agi::make_unique<video_frame_prev_keyframe>());
		reg(agi::make_unique<video_frame_prev_large>());
		reg(agi::make_unique<video_frame_save>());
		reg(agi::make_unique<video_frame_save_raw>());
		reg(agi::make_unique<video_jump>());
		reg(agi::make_unique<video_jump_end>());
		reg(agi::make_unique<video_jump_start>());
		reg(agi::make_unique<video_open>());
		reg(agi::make_unique<video_open_dummy>());
		reg(agi::make_unique<video_opt_autoscroll>());
		reg(agi::make_unique<video_pan_reset>());
		reg(agi::make_unique<video_play>());
		reg(agi::make_unique<video_play_line>());
		reg(agi::make_unique<video_show_overscan>());
		reg(agi::make_unique<video_stop>());
		reg(agi::make_unique<video_zoom_100>());
		reg(agi::make_unique<video_zoom_200>());
		reg(agi::make_unique<video_zoom_50>());
		reg(agi::make_unique<video_zoom_in>());
		reg(agi::make_unique<video_zoom_out>());
	}
}
