// Copyright (c) 2005-2010, Niels Martin Hansen
// Copyright (c) 2005-2010, Rodrigo Braz Monteiro
// Copyright (c) 2010, Amar Takhar
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

#include "command.h"

#include "../ass_dialogue.h"
#include "../async_video_provider.h"
#include "../audio_box.h"
#include "../audio_controller.h"
#include "../audio_karaoke.h"
#include "../audio_timing.h"
#include "../compat.h"
#include "../include/aegisub/context.h"
#include "../libresrc/libresrc.h"
#include "../options.h"
#include "../project.h"
#include "../selection_controller.h"
#include "../utils.h"
#include "../video_controller.h"

#include <libaegisub/audio/provider.h>
#include <libaegisub/make_unique.h>
#include <libaegisub/io.h>

namespace {
	using cmd::Command;

	struct validate_audio_open : public Command {
		CMD_TYPE(COMMAND_VALIDATE)
		bool Validate(const agi::Context *c) override {
			return !!c->project->AudioProvider();
		}
	};

struct audio_close final : public validate_audio_open {
	CMD_NAME("audio/close")
	CMD_ICON(close_audio_menu)
	STR_MENU("&Close Audio")
	STR_DISP("Close Audio")
	STR_HELP("Close the currently open audio file")

	void operator()(agi::Context *c) override {
		c->project->CloseAudio();
	}
};

struct audio_open final : public Command {
	CMD_NAME("audio/open")
	CMD_ICON(open_audio_menu)
	STR_MENU("&Open Audio File...")
	STR_DISP("Open Audio File")
	STR_HELP("Open an audio file")

	void operator()(agi::Context *c) override {
		auto str = from_wx(_("Audio Formats") + " (*.aac,*.ac3,*.ape,*.dts,*.flac,*.m4a,*.mka,*.mp3,*.mp4,*.ogg,*.w64,*.wav,*.wma)|*.aac;*.ac3;*.ape;*.dts;*.flac;*.m4a;*.mka;*.mp3;*.mp4;*.ogg;*.w64;*.wav;*.wma|"
					+ _("Video Formats") + " (*.asf,*.avi,*.avs,*.d2v,*.m2ts,*.m4v,*.mkv,*.mov,*.mp4,*.mpeg,*.mpg,*.ogm,*.webm,*.wmv,*.ts)|*.asf;*.avi;*.avs;*.d2v;*.m2ts;*.m4v;*.mkv;*.mov;*.mp4;*.mpeg;*.mpg;*.ogm;*.webm;*.wmv;*.ts|"
					+ _("All Files") + " (*.*)|*.*");
		auto filename = OpenFileSelector(_("Open Audio File"), "Path/Last/Audio", "", "", str, c->parent);
		if (!filename.empty())
			c->project->LoadAudio(filename);
	}
};

struct audio_open_blank final : public Command {
	CMD_NAME("audio/open/blank")
	STR_MENU("Open 2h30 Blank Audio")
	STR_DISP("Open 2h30 Blank Audio")
	STR_HELP("Open a 150 minutes blank audio clip, for debugging")

	void operator()(agi::Context *c) override {
		c->project->LoadAudio("dummy-audio:silence?sr=44100&bd=16&ch=1&ln=396900000");
	}
};

struct audio_open_noise final : public Command {
	CMD_NAME("audio/open/noise")
	STR_MENU("Open 2h30 Noise Audio")
	STR_DISP("Open 2h30 Noise Audio")
	STR_HELP("Open a 150 minutes noise-filled audio clip, for debugging")

	void operator()(agi::Context *c) override {
		c->project->LoadAudio("dummy-audio:noise?sr=44100&bd=16&ch=1&ln=396900000");
	}
};

struct audio_open_video final : public Command {
	CMD_NAME("audio/open/video")
	CMD_ICON(open_audio_from_video_menu)
	STR_MENU("Open Audio from &Video")
	STR_DISP("Open Audio from Video")
	STR_HELP("Open the audio from the current video file")
	CMD_TYPE(COMMAND_VALIDATE)

	bool Validate(const agi::Context *c) override {
		return c->project->VideoProvider() && c->project->VideoProvider()->HasAudio();
	}

	void operator()(agi::Context *c) override {
		c->project->LoadAudio(c->project->VideoName());
	}
};

struct audio_view_spectrum final : public Command {
	CMD_NAME("audio/view/spectrum")
	STR_MENU("&Spectrum Display")
	STR_DISP("Spectrum Display")
	STR_HELP("Display audio as a frequency-power spectrograph")
	CMD_TYPE(COMMAND_RADIO)

	bool IsActive(const agi::Context *) override {
		return OPT_GET("Audio/Spectrum")->GetBool();
	}

	void operator()(agi::Context *) override {
		OPT_SET("Audio/Spectrum")->SetBool(true);
	}
};

struct audio_view_waveform final : public Command {
	CMD_NAME("audio/view/waveform")
	STR_MENU("&Waveform Display")
	STR_DISP("Waveform Display")
	STR_HELP("Display audio as a linear amplitude graph")
	CMD_TYPE(COMMAND_RADIO)

	bool IsActive(const agi::Context *) override {
		return !OPT_GET("Audio/Spectrum")->GetBool();
	}

	void operator()(agi::Context *) override {
		OPT_SET("Audio/Spectrum")->SetBool(false);
	}
};

struct audio_save_clip final : public Command {
	CMD_NAME("audio/save/clip")
	STR_MENU("Create audio clip")
	STR_DISP("Create audio clip")
	STR_HELP("Save an audio clip of the selected line")
	CMD_TYPE(COMMAND_VALIDATE)

	bool Validate(const agi::Context *c) override {
		return c->project->AudioProvider() && !c->selectionController->GetSelectedSet().empty();
	}

	void operator()(agi::Context *c) override {
		auto const& sel = c->selectionController->GetSelectedSet();
		if (sel.empty()) return;

		auto filename = SaveFileSelector(_("Save audio clip"), "", "", "wav", "", c->parent);
		if (filename.empty()) return;

		agi::Time start = INT_MAX, end = 0;
		for (auto line : sel) {
			start = std::min(start, line->Start);
			end = std::max(end, line->End);
		}

		agi::SaveAudioClip(*c->project->AudioProvider(), filename, start, end);
	}
};

struct audio_play_current_selection final : public validate_audio_open {
	CMD_NAME("audio/play/current")
	STR_MENU("Play current audio selection")
	STR_DISP("Play current audio selection")
	STR_HELP("Play the current audio selection, ignoring changes made while playing")

	void operator()(agi::Context *c) override {
		if (c->videoController->HasVideo() && OPT_GET("Audio/Sync Video to Audio")->GetBool()) {
			c->videoController->PlayRange(c->audioController->GetPrimaryPlaybackRange());
		} else {
			c->videoController->Stop();
			c->audioController->PlayRange(c->audioController->GetPrimaryPlaybackRange());
		}
	}
};

struct audio_play_current_line final : public validate_audio_open {
	CMD_NAME("audio/play/line")
	CMD_ICON(button_playline)
	STR_MENU("Play current line")
	STR_DISP("Play current line")
	STR_HELP("Play the audio for the current line")

	void operator()(agi::Context *c) override {
		AudioTimingController *tc = c->audioController->GetTimingController();
		c->videoController->Stop();
		if(tc) {
			if (c->videoController->HasVideo() && OPT_GET("Audio/Sync Video to Audio")->GetBool()) {
				c->videoController->PlayRange(tc->GetActiveLineRange());
			} else {
				c->audioController->PlayRange(tc->GetActiveLineRange());
			}
		}
	}
};

struct audio_play_selection final : public validate_audio_open {
	CMD_NAME("audio/play/selection")
	CMD_ICON(button_playsel)
	STR_MENU("Play audio selection")
	STR_DISP("Play audio selection")
	STR_HELP("Play audio until the end of the selection is reached")

	void operator()(agi::Context *c) override {
		if (c->videoController->HasVideo() && OPT_GET("Audio/Sync Video to Audio")->GetBool()) {
			c->videoController->PlayRange(c->audioController->GetPrimaryPlaybackRange());
		} else {
			c->videoController->Stop();
			c->audioController->PlayPrimaryRange();
		}
	}
};

struct audio_play_toggle final : public validate_audio_open {
	CMD_NAME("audio/play/toggle")
	STR_MENU("Play audio selection or stop")
	STR_DISP("Play audio selection or stop")
	STR_HELP("Play selection, or stop playback if it's already playing")

	void operator()(agi::Context *c) override {
		if (c->videoController->HasVideo() && OPT_GET("Audio/Sync Video to Audio")->GetBool()) {
			if (c->audioController->IsPlaying() || c->videoController->IsPlaying()) {
				c->audioController->Stop();
				c->videoController->Stop();
			} else {
				c->videoController->PlayPrimaryRange(c->audioController->GetPrimaryPlaybackRange());
			}
		} else {
			if (c->audioController->IsPlaying())
				c->audioController->Stop();
			else {
				c->videoController->Stop();
				c->audioController->PlayPrimaryRange();
			}
		}
	}
};

struct audio_stop final : public Command {
	CMD_NAME("audio/stop")
	CMD_ICON(button_stop)
	STR_MENU("Stop playing")
	STR_DISP("Stop playing")
	STR_HELP("Stop audio and video playback")
	CMD_TYPE(COMMAND_VALIDATE)

	bool Validate(const agi::Context *c) override {
		return c->audioController->IsPlaying();
	}

	void operator()(agi::Context *c) override {
		c->audioController->Stop();
		c->videoController->Stop();
	}
};

struct audio_play_before final : public validate_audio_open {
	CMD_NAME("audio/play/selection/before")
	CMD_ICON(button_playfivehbefore)
	STR_MENU("Play 500 ms before selection")
	STR_DISP("Play 500 ms before selection")
	STR_HELP("Play 500 ms before selection")

	void operator()(agi::Context *c) override {
		int begin = c->audioController->GetPrimaryPlaybackRange().begin();
		if (c->videoController->HasVideo() && OPT_GET("Audio/Sync Video to Audio")->GetBool()) {
			c->videoController->PlayRange(TimeRange(begin - 500, begin));
		} else {
			c->videoController->Stop();
			c->audioController->PlayRange(TimeRange(begin - 500, begin));
		}
	}
};

struct audio_play_after final : public validate_audio_open {
	CMD_NAME("audio/play/selection/after")
	CMD_ICON(button_playfivehafter)
	STR_MENU("Play 500 ms after selection")
	STR_DISP("Play 500 ms after selection")
	STR_HELP("Play 500 ms after selection")

	void operator()(agi::Context *c) override {
		int end = c->audioController->GetPrimaryPlaybackRange().end();
		if (c->videoController->HasVideo() && OPT_GET("Audio/Sync Video to Audio")->GetBool()) {
			c->videoController->PlayRange(TimeRange(end, end + 500));
		} else {
			c->videoController->Stop();
			c->audioController->PlayRange(TimeRange(end, end + 500));
		}
	}
};

struct audio_play_end final : public validate_audio_open {
	CMD_NAME("audio/play/selection/end")
	CMD_ICON(button_playlastfiveh)
	STR_MENU("Play last 500 ms of selection")
	STR_DISP("Play last 500 ms of selection")
	STR_HELP("Play last 500 ms of selection")

	void operator()(agi::Context *c) override {
		TimeRange times(c->audioController->GetPrimaryPlaybackRange());
		if (c->videoController->HasVideo() && OPT_GET("Audio/Sync Video to Audio")->GetBool()) {
			c->videoController->PlayToEndOfPrimary(TimeRange(times.end() - std::min(500, times.length()), c->audioController->GetPrimaryPlaybackRange().end()));
		} else {
			c->videoController->Stop();
			c->audioController->PlayToEndOfPrimary(times.end() - std::min(500, times.length()));
		}
	}
};

struct audio_play_begin final : public validate_audio_open {
	CMD_NAME("audio/play/selection/begin")
	CMD_ICON(button_playfirstfiveh)
	STR_MENU("Play first 500 ms of selection")
	STR_DISP("Play first 500 ms of selection")
	STR_HELP("Play first 500 ms of selection")

	void operator()(agi::Context *c) override {
		TimeRange times(c->audioController->GetPrimaryPlaybackRange());
		if (c->videoController->HasVideo() && OPT_GET("Audio/Sync Video to Audio")->GetBool()) {
			c->videoController->PlayRange(TimeRange(
				times.begin(),
				times.begin() + std::min(500, times.length())));
		} else {
			c->videoController->Stop();
			c->audioController->PlayRange(TimeRange(
				times.begin(),
				times.begin() + std::min(500, times.length())));
		}
	}
};

struct audio_play_to_end final : public validate_audio_open {
	CMD_NAME("audio/play/to_end")
	CMD_ICON(button_playtoend)
	STR_MENU("Play from selection start to end of file")
	STR_DISP("Play from selection start to end of file")
	STR_HELP("Play from selection start to end of file")

	void operator()(agi::Context *c) override {
		if (c->videoController->HasVideo() && OPT_GET("Audio/Sync Video to Audio")->GetBool()) {
			c->videoController->PlayToEnd(c->audioController->GetPrimaryPlaybackRange().begin());
		} else {
			c->videoController->Stop();
			c->audioController->PlayToEnd(c->audioController->GetPrimaryPlaybackRange().begin());
		}
	}
};

struct audio_commit final : public validate_audio_open {
	CMD_NAME("audio/commit")
	CMD_ICON(button_audio_commit)
	STR_MENU("Commit")
	STR_DISP("Commit")
	STR_HELP("Commit any pending audio timing changes")

	void operator()(agi::Context *c) override {
		AudioTimingController *tc = c->audioController->GetTimingController();
		if (tc) {
			tc->Commit();
			if(OPT_GET("Audio/Next Line on Commit")->GetBool())
				tc->Next(AudioTimingController::LINE);
		}
	}
};

struct audio_commit_default final : public validate_audio_open {
	CMD_NAME("audio/commit/default")
	STR_MENU("Commit and use default timing for next line")
	STR_DISP("Commit and use default timing for next line")
	STR_HELP("Commit any pending audio timing changes and reset the next line's times to the default")

	void operator()(agi::Context *c) override {
		AudioTimingController *tc = c->audioController->GetTimingController();
		if (tc) {
			tc->Commit();
			tc->Next(AudioTimingController::LINE_RESET_DEFAULT);
		}
	}
};

struct audio_commit_next final : public validate_audio_open {
	CMD_NAME("audio/commit/next")
	STR_MENU("Commit and move to next line")
	STR_DISP("Commit and move to next line")
	STR_HELP("Commit any pending audio timing changes and move to the next line")

	void operator()(agi::Context *c) override {
		AudioTimingController *tc = c->audioController->GetTimingController();
		if (tc) {
			tc->Commit();
			tc->Next(AudioTimingController::LINE);
		}
	}
};

struct audio_commit_stay final : public validate_audio_open {
	CMD_NAME("audio/commit/stay")
	STR_MENU("Commit and stay on current line")
	STR_DISP("Commit and stay on current line")
	STR_HELP("Commit any pending audio timing changes and stay on the current line")

	void operator()(agi::Context *c) override {
		AudioTimingController *tc = c->audioController->GetTimingController();
		if (tc) tc->Commit();
	}
};

struct audio_go_to final : public validate_audio_open {
	CMD_NAME("audio/go_to")
	CMD_ICON(button_audio_goto)
	STR_MENU("Go to selection")
	STR_DISP("Go to selection")
	STR_HELP("Scroll the audio display to center on the current audio selection")

	void operator()(agi::Context *c) override {
		c->audioBox->ScrollToActiveLine();
	}
};

struct audio_scroll_left final : public validate_audio_open {
	CMD_NAME("audio/scroll/left")
		STR_MENU("Scroll left")
		STR_DISP("Scroll left")
		STR_HELP("Scroll the audio display left")

		void operator()(agi::Context *c) override {
			c->audioBox->ScrollAudioBy(-128);
	}
};

struct audio_scroll_right final : public validate_audio_open {
	CMD_NAME("audio/scroll/right")
		STR_MENU("Scroll right")
		STR_DISP("Scroll right")
		STR_HELP("Scroll the audio display right")

		void operator()(agi::Context *c) override {
			c->audioBox->ScrollAudioBy(128);
	}
};

static inline void toggle(const char *opt) {
	OPT_SET(opt)->SetBool(!OPT_GET(opt)->GetBool());
}

struct audio_autoscroll final : public Command {
	CMD_NAME("audio/opt/autoscroll")
	CMD_ICON(toggle_audio_autoscroll)
	STR_MENU("Auto scroll audio display to selected line")
	STR_DISP("Auto scroll audio display to selected line")
	STR_HELP("Auto scroll audio display to selected line")
	CMD_TYPE(COMMAND_TOGGLE)

	bool IsActive(const agi::Context *) override {
		return OPT_GET("Audio/Auto/Scroll")->GetBool();
	}

	void operator()(agi::Context *) override {
		toggle("Audio/Auto/Scroll");
	}
};

struct audio_autocommit final : public Command {
	CMD_NAME("audio/opt/autocommit")
	CMD_ICON(toggle_audio_autocommit)
	STR_MENU("Automatically commit all changes")
	STR_DISP("Automatically commit all changes")
	STR_HELP("Automatically commit all changes")
	CMD_TYPE(COMMAND_TOGGLE)

	bool IsActive(const agi::Context *) override {
		return OPT_GET("Audio/Auto/Commit")->GetBool();
	}

	void operator()(agi::Context *) override {
		toggle("Audio/Auto/Commit");
	}
};

struct audio_autonext final : public Command {
	CMD_NAME("audio/opt/autonext")
	CMD_ICON(toggle_audio_nextcommit)
	STR_MENU("Auto go to next line on commit")
	STR_DISP("Auto go to next line on commit")
	STR_HELP("Automatically go to next line on commit")
	CMD_TYPE(COMMAND_TOGGLE)

	bool IsActive(const agi::Context *) override {
		return OPT_GET("Audio/Next Line on Commit")->GetBool();
	}

	void operator()(agi::Context *) override {
		toggle("Audio/Next Line on Commit");
	}
};

struct audio_toggle_spectrum final : public Command {
	CMD_NAME("audio/opt/spectrum")
	CMD_ICON(toggle_audio_spectrum)
	STR_MENU("Spectrum analyzer mode")
	STR_DISP("Spectrum analyzer mode")
	STR_HELP("Spectrum analyzer mode")
	CMD_TYPE(COMMAND_TOGGLE)

	bool IsActive(const agi::Context *) override {
		return OPT_GET("Audio/Spectrum")->GetBool();
	}

	void operator()(agi::Context *) override {
		toggle("Audio/Spectrum");
	}
};

struct audio_toggle_video_audio_sync final : public Command {
	CMD_NAME("audio/opt/video_audio_sync")
	CMD_ICON(toggle_video_audio_sync)
	STR_MENU("Sync video to audio")
	STR_DISP("Sync video to audio")
	STR_HELP("Sync video to audio")
	CMD_TYPE(COMMAND_TOGGLE)

	bool IsActive(const agi::Context *) override {
		return OPT_GET("Audio/Sync Video to Audio")->GetBool();
	}

	void operator()(agi::Context *) override {
		toggle("Audio/Sync Video to Audio");
	}
};

struct audio_vertical_link final : public Command {
	CMD_NAME("audio/opt/vertical_link")
	CMD_ICON(toggle_audio_link)
	STR_MENU("Link vertical zoom and volume sliders")
	STR_DISP("Link vertical zoom and volume sliders")
	STR_HELP("Link vertical zoom and volume sliders")
	CMD_TYPE(COMMAND_TOGGLE)

	bool IsActive(const agi::Context *) override {
		return OPT_GET("Audio/Link")->GetBool();
	}

	void operator()(agi::Context *) override {
		toggle("Audio/Link");
	}
};

struct audio_karaoke final : public Command {
	CMD_NAME("audio/karaoke")
	CMD_ICON(kara_mode)
	STR_MENU("Toggle karaoke mode")
	STR_DISP("Toggle karaoke mode")
	STR_HELP("Toggle karaoke mode")
	CMD_TYPE(COMMAND_TOGGLE)

	bool IsActive(const agi::Context *c) override {
		return c->karaoke->IsEnabled();
	}
	void operator()(agi::Context *c) override {
		c->karaoke->SetEnabled(!c->karaoke->IsEnabled());
	}
};

}

namespace cmd {
	void init_audio() {
		reg(agi::make_unique<audio_autocommit>());
		reg(agi::make_unique<audio_autonext>());
		reg(agi::make_unique<audio_autoscroll>());
		reg(agi::make_unique<audio_close>());
		reg(agi::make_unique<audio_commit>());
		reg(agi::make_unique<audio_commit_default>());
		reg(agi::make_unique<audio_commit_next>());
		reg(agi::make_unique<audio_commit_stay>());
		reg(agi::make_unique<audio_go_to>());
		reg(agi::make_unique<audio_karaoke>());
		reg(agi::make_unique<audio_open>());
		reg(agi::make_unique<audio_open_blank>());
		reg(agi::make_unique<audio_open_noise>());
		reg(agi::make_unique<audio_open_video>());
		reg(agi::make_unique<audio_play_after>());
		reg(agi::make_unique<audio_play_before>());
		reg(agi::make_unique<audio_play_begin>());
		reg(agi::make_unique<audio_play_end>());
		reg(agi::make_unique<audio_play_current_selection>());
		reg(agi::make_unique<audio_play_current_line>());
		reg(agi::make_unique<audio_play_selection>());
		reg(agi::make_unique<audio_play_to_end>());
		reg(agi::make_unique<audio_play_toggle>());
		reg(agi::make_unique<audio_save_clip>());
		reg(agi::make_unique<audio_scroll_left>());
		reg(agi::make_unique<audio_scroll_right>());
		reg(agi::make_unique<audio_stop>());
		reg(agi::make_unique<audio_toggle_spectrum>());
		reg(agi::make_unique<audio_toggle_video_audio_sync>());
		reg(agi::make_unique<audio_vertical_link>());
		reg(agi::make_unique<audio_view_spectrum>());
		reg(agi::make_unique<audio_view_waveform>());
	}
}
