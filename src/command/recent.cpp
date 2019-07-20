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

#include "../include/aegisub/context.h"
#include "../libresrc/libresrc.h"
#include "../main.h"
#include "../options.h"
#include "../project.h"
#include "../subs_controller.h"

#include <libaegisub/make_unique.h>

namespace {
	using cmd::Command;

COMMAND_GROUP(recent_audio,     "recent/audio",     _("Recent"), _("Recent"), _("Open recent audio"));
COMMAND_GROUP(recent_keyframes, "recent/keyframe",  _("Recent"), _("Recent"), _("Open recent keyframes"));
COMMAND_GROUP(recent_subtitle,  "recent/subtitle",  _("Recent"), _("Recent"), _("Open recent subtitles"));
COMMAND_GROUP(recent_timecodes, "recent/timecodes", _("Recent"), _("Recent"), _("Open recent timecodes"));
COMMAND_GROUP(recent_video,     "recent/video",     _("Recent"), _("Recent"), _("Open recent video"));

struct recent_audio_entry : public Command {
	CMD_NAME("recent/audio/")
	STR_MENU("Recent")
	STR_DISP("Recent")
	STR_HELP("Open recent audio")

	void operator()(agi::Context *c, int id) {
		c->project->LoadAudio(config::mru->GetEntry("Audio", id));
	}
};

struct recent_keyframes_entry : public Command {
	CMD_NAME("recent/keyframes/")
	STR_MENU("Recent")
	STR_DISP("Recent")
	STR_HELP("Open recent keyframes")

	void operator()(agi::Context *c, int id) {
		c->project->LoadKeyframes(config::mru->GetEntry("Keyframes", id));
	}
};

struct recent_subtitle_entry : public Command {
	CMD_NAME("recent/subtitle/")
	STR_MENU("Recent")
	STR_DISP("Recent")
	STR_HELP("Open recent subtitles")

	void operator()(agi::Context *c, int id) {
#ifdef __APPLE__
		wxGetApp().EmptyProjectContext().project->LoadSubtitles(config::mru->GetEntry("Subtitle", id));
#else
		if (c->subsController->TryToClose() == wxCANCEL) return;
		c->project->LoadSubtitles(config::mru->GetEntry("Subtitle", id));
#endif
	}
};

struct recent_timecodes_entry : public Command {
	CMD_NAME("recent/timecodes/")
	STR_MENU("Recent")
	STR_DISP("Recent")
	STR_HELP("Open recent timecodes")

	void operator()(agi::Context *c, int id) {
		c->project->LoadTimecodes(config::mru->GetEntry("Timecodes", id));
	}
};

struct recent_video_entry : public Command {
	CMD_NAME("recent/video/")
	STR_MENU("Recent")
	STR_DISP("Recent")
	STR_HELP("Open recent videos")

	void operator()(agi::Context *c, int id) {
		c->project->LoadVideo(config::mru->GetEntry("Video", id));
	}
};

template<class T>
class mru_wrapper final : public T {
	int id;
	std::string full_name;
public:
	const char *name() const { return full_name.c_str(); }
	void operator()(agi::Context *c) {
		T::operator()(c, id);
	}
	mru_wrapper(int id) : id(id) , full_name(T::name() + std::to_string(id)) { }
};
}

namespace cmd {
	void init_recent() {
		reg(agi::make_unique<recent_audio>());
		reg(agi::make_unique<recent_keyframes>());
		reg(agi::make_unique<recent_subtitle>());
		reg(agi::make_unique<recent_timecodes>());
		reg(agi::make_unique<recent_video>());

		for (int i = 0; i < 16; ++i) {
			reg(agi::make_unique<mru_wrapper<recent_audio_entry>>(i));
			reg(agi::make_unique<mru_wrapper<recent_keyframes_entry>>(i));
			reg(agi::make_unique<mru_wrapper<recent_subtitle_entry>>(i));
			reg(agi::make_unique<mru_wrapper<recent_timecodes_entry>>(i));
			reg(agi::make_unique<mru_wrapper<recent_video_entry>>(i));
		}
	}
}
