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

#include <wx/msgdlg.h>

#include "command.h"

#include <libaegisub/log.h>
#include <libaegisub/make_unique.h>

#include "../compat.h"
#include "../dialog_detached_video.h"
#include "../dialog_manager.h"
#include "../dialogs.h"
#include "../frame_main.h"
#include "../include/aegisub/context.h"
#include "../libresrc/libresrc.h"
#include "../main.h"
#include "../options.h"
#include "../project.h"
#include "../utils.h"

namespace {
	using cmd::Command;

struct app_about final : public Command {
	CMD_NAME("app/about")
	CMD_ICON(about_menu)
	STR_MENU("&About")
	STR_DISP("About")
	STR_HELP("About Aegisub")

	void operator()(agi::Context *c) override {
		ShowAboutDialog(c->parent);
	}
};

struct app_display_audio_subs final : public Command {
	CMD_NAME("app/display/audio_subs")
	STR_MENU("&Audio+Subs View")
	STR_DISP("Audio+Subs View")
	STR_HELP("Display audio and the subtitles grid only")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_RADIO)

	void operator()(agi::Context *c) override {
		c->frame->SetDisplayMode(0,1);
	}

	bool Validate(const agi::Context *c) override {
		return !!c->project->AudioProvider();
	}

	bool IsActive(const agi::Context *c) override {
		return c->frame->IsAudioShown() && !c->frame->IsVideoShown();
	}
};

struct app_display_full final : public Command {
	CMD_NAME("app/display/full")
	STR_MENU("&Full view")
	STR_DISP("Full view")
	STR_HELP("Display audio, video and then subtitles grid")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_RADIO)

	void operator()(agi::Context *c) override {
		c->frame->SetDisplayMode(1,1);
	}

	bool Validate(const agi::Context *c) override {
		return c->project->AudioProvider() && c->project->VideoProvider() && !c->dialog->Get<DialogDetachedVideo>();
	}

	bool IsActive(const agi::Context *c) override {
		return c->frame->IsAudioShown() && c->frame->IsVideoShown();
	}
};

struct app_display_subs final : public Command {
	CMD_NAME("app/display/subs")
	STR_MENU("S&ubs Only View")
	STR_DISP("Subs Only View")
	STR_HELP("Display the subtitles grid only")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_RADIO)

	void operator()(agi::Context *c) override {
		c->frame->SetDisplayMode(0, 0);
	}

	bool IsActive(const agi::Context *c) override {
		return !c->frame->IsAudioShown() && !c->frame->IsVideoShown();
	}
};

struct app_display_video_subs final : public Command {
	CMD_NAME("app/display/video_subs")
	STR_MENU("&Video+Subs View")
	STR_DISP("Video+Subs View")
	STR_HELP("Display video and the subtitles grid only")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_RADIO)

	void operator()(agi::Context *c) override {
		c->frame->SetDisplayMode(1, 0);
	}

	bool Validate(const agi::Context *c) override {
		return c->project->VideoProvider() && !c->dialog->Get<DialogDetachedVideo>();
	}

	bool IsActive(const agi::Context *c) override {
		return !c->frame->IsAudioShown() && c->frame->IsVideoShown();
	}
};

struct app_exit final : public Command {
	CMD_NAME("app/exit")
	STR_MENU("E&xit")
	STR_DISP("Exit")
	STR_HELP("Exit the application")

	void operator()(agi::Context *) override {
		wxGetApp().Quit();
	}
};

struct app_language final : public Command {
	CMD_NAME("app/language")
	CMD_ICON(languages_menu)
	STR_MENU("&Language...")
	STR_DISP("Language")
	STR_HELP("Select Aegisub interface language")

	void operator()(agi::Context *c) override {
		// Get language
		auto new_language = wxGetApp().locale.PickLanguage();
		if (new_language.empty()) return;

		OPT_SET("App/Language")->SetString(new_language);

		// Ask to restart program
		int result = wxMessageBox("Aegisub needs to be restarted so that the new language can be applied. Restart now?", "Restart Aegisub?", wxYES_NO | wxICON_QUESTION |  wxCENTER);
		if (result == wxYES) {
			// Restart Aegisub
			if (c->frame->Close()) {
				RestartAegisub();
			}
		}
	}
};

struct app_log final : public Command {
	CMD_NAME("app/log")
	CMD_ICON(about_menu)
	STR_MENU("&Log window")
	STR_DISP("Log window")
	STR_HELP("View the event log")

	void operator()(agi::Context *c) override {
		ShowLogWindow(c);
	}
};

struct app_new_window final : public Command {
	CMD_NAME("app/new_window")
	CMD_ICON(new_window_menu)
	STR_MENU("New &Window")
	STR_DISP("New Window")
	STR_HELP("Open a new application window")

	void operator()(agi::Context *) override {
		wxGetApp().NewProjectContext();
	}
};

struct app_options final : public Command {
	CMD_NAME("app/options")
	CMD_ICON(options_button)
	STR_MENU("&Options...")
	STR_DISP("Options")
	STR_HELP("Configure Aegisub")

	void operator()(agi::Context *c) override {
		try {
			ShowPreferences(c->parent);
		} catch (agi::Exception& e) {
			LOG_E("config/init") << "Caught exception: " << e.GetMessage();
		}
	}
};

struct app_toggle_global_hotkeys final : public Command {
	CMD_NAME("app/toggle/global_hotkeys")
	CMD_ICON(toggle_audio_medusa)
	STR_MENU("Toggle global hotkey overrides")
	STR_DISP("Toggle global hotkey overrides")
	STR_HELP("Toggle global hotkey overrides (Medusa Mode)")
	CMD_TYPE(COMMAND_TOGGLE)

	bool IsActive(const agi::Context *c) override {
		return OPT_GET("Audio/Medusa Timing Hotkeys")->GetBool();
	}

	void operator()(agi::Context *c) override {
		agi::OptionValue *opt = OPT_SET("Audio/Medusa Timing Hotkeys");
		opt->SetBool(!opt->GetBool());
	}
};

struct app_toggle_toolbar final : public Command {
	CMD_NAME("app/toggle/toolbar")
	STR_HELP("Toggle the main toolbar")
	CMD_TYPE(COMMAND_DYNAMIC_NAME)

	wxString StrMenu(const agi::Context *c) const override {
		return OPT_GET("App/Show Toolbar")->GetBool() ?
			_("Hide Toolbar") :
			_("Show Toolbar");
	}

	wxString StrDisplay(const agi::Context *c) const override {
		return StrMenu(nullptr);
	}

	void operator()(agi::Context *c) override {
		agi::OptionValue *opt = OPT_SET("App/Show Toolbar");
		opt->SetBool(!opt->GetBool());
	}
};

struct app_updates final : public Command {
	CMD_NAME("app/updates")
	STR_MENU("&Check for Updates...")
	STR_DISP("Check for Updates")
	STR_HELP("Check to see if there is a new version of Aegisub available")

	void operator()(agi::Context *c) override {
		PerformVersionCheck(true);
	}
};

#ifdef __WXMAC__
struct app_minimize final : public Command {
	CMD_NAME("app/minimize")
	STR_MENU("Minimize")
	STR_DISP("Minimize")
	STR_HELP("Minimize the active window")

	void operator()(agi::Context *c) override {
		c->frame->Iconize();
	}
};

struct app_maximize final : public Command {
	CMD_NAME("app/maximize")
	STR_MENU("Zoom")
	STR_DISP("Zoom")
	STR_HELP("Maximize the active window")

	void operator()(agi::Context *c) override {
		c->frame->Maximize(!c->frame->IsMaximized());
	}
};

struct app_bring_to_front final : public Command {
	CMD_NAME("app/bring_to_front")
	STR_MENU("Bring All to Front")
	STR_DISP("Bring All to Front")
	STR_HELP("Bring forward all open documents to the front")

	void operator()(agi::Context *) override {
		osx::bring_to_front();
	}
};
#endif

}

namespace cmd {
	void init_app() {
		reg(agi::make_unique<app_about>());
		reg(agi::make_unique<app_display_audio_subs>());
		reg(agi::make_unique<app_display_full>());
		reg(agi::make_unique<app_display_subs>());
		reg(agi::make_unique<app_display_video_subs>());
		reg(agi::make_unique<app_exit>());
		reg(agi::make_unique<app_language>());
		reg(agi::make_unique<app_log>());
		reg(agi::make_unique<app_new_window>());
		reg(agi::make_unique<app_options>());
		reg(agi::make_unique<app_toggle_global_hotkeys>());
		reg(agi::make_unique<app_toggle_toolbar>());
#ifdef __WXMAC__
		reg(agi::make_unique<app_minimize>());
		reg(agi::make_unique<app_maximize>());
		reg(agi::make_unique<app_bring_to_front>());
#endif
#ifdef WITH_UPDATE_CHECKER
		reg(agi::make_unique<app_updates>());
#endif
	}
}
