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
#include "../ass_file.h"
#include "../compat.h"
#include "../dialog_search_replace.h"
#include "../dialogs.h"
#include "../frame_main.h"
#include "../include/aegisub/context.h"
#include "../libresrc/libresrc.h"
#include "../main.h"
#include "../options.h"
#include "../project.h"
#include "../search_replace_engine.h"
#include "../selection_controller.h"
#include "../subs_controller.h"
#include "../subtitle_format.h"
#include "../utils.h"
#include "../video_controller.h"

#include <libaegisub/address_of_adaptor.h>
#include <libaegisub/charset_conv.h>
#include <libaegisub/make_unique.h>

#include <boost/range/algorithm/copy.hpp>
#include <wx/msgdlg.h>
#include <wx/choicdlg.h>

namespace {
	using cmd::Command;

struct validate_nonempty_selection : public Command {
	CMD_TYPE(COMMAND_VALIDATE)
	bool Validate(const agi::Context *c) override {
		return !c->selectionController->GetSelectedSet().empty();
	}
};

struct validate_nonempty_selection_video_loaded : public Command {
	CMD_TYPE(COMMAND_VALIDATE)
	bool Validate(const agi::Context *c) override {
		return c->project->VideoProvider() && !c->selectionController->GetSelectedSet().empty();
	}
};

struct subtitle_attachment final : public Command {
	CMD_NAME("subtitle/attachment")
	CMD_ICON(attach_button)
	STR_MENU("A&ttachments...")
	STR_DISP("Attachments")
	STR_HELP("Open the attachment manager dialog")

	void operator()(agi::Context *c) override {
		c->videoController->Stop();
		ShowAttachmentsDialog(c->parent, c->ass.get());
	}
};

struct subtitle_find final : public Command {
	CMD_NAME("subtitle/find")
	CMD_ICON(find_button)
	STR_MENU("&Find...")
	STR_DISP("Find")
	STR_HELP("Search for text in the subtitles")

	void operator()(agi::Context *c) override {
		c->videoController->Stop();
		DialogSearchReplace::Show(c, false);
	}
};

struct subtitle_find_next final : public Command {
	CMD_NAME("subtitle/find/next")
	CMD_ICON(find_next_menu)
	STR_MENU("Find &Next")
	STR_DISP("Find Next")
	STR_HELP("Find next match of last search")

	void operator()(agi::Context *c) override {
		c->videoController->Stop();
		if (!c->search->FindNext())
			DialogSearchReplace::Show(c, false);
	}
};

static void insert_subtitle_at_video(agi::Context *c, bool after) {
	auto def = new AssDialogue;
	int video_ms = c->videoController->TimeAtFrame(c->videoController->GetFrameN(), agi::vfr::START);
	def->Start = video_ms;
	def->End = video_ms + OPT_GET("Timing/Default Duration")->GetInt();
	def->Style = c->selectionController->GetActiveLine()->Style;

	auto pos = c->ass->iterator_to(*c->selectionController->GetActiveLine());
	if (after) ++pos;

	c->ass->Events.insert(pos, *def);
	c->ass->Commit(_("line insertion"), AssFile::COMMIT_DIAG_ADDREM);

	c->selectionController->SetSelectionAndActive({ def }, def);
}

struct subtitle_insert_after final : public validate_nonempty_selection {
	CMD_NAME("subtitle/insert/after")
	STR_MENU("&After Current")
	STR_DISP("After Current")
	STR_HELP("Insert a new line after the current one")

	void operator()(agi::Context *c) override {
		AssDialogue *active_line = c->selectionController->GetActiveLine();

		auto new_line = new AssDialogue;
		new_line->Style = active_line->Style;
		new_line->Start = active_line->End;
		new_line->End = new_line->Start + OPT_GET("Timing/Default Duration")->GetInt();

		for (auto it = c->ass->Events.begin(); it != c->ass->Events.end(); ++it) {
			AssDialogue *diag = &*it;

			// Limit the line to the available time
			if (diag->Start >= new_line->Start)
				new_line->End = std::min(new_line->End, diag->Start);

			// If we just hit the active line, insert the new line after it
			if (diag == active_line) {
				++it;
				c->ass->Events.insert(it, *new_line);
				--it;
			}
		}

		c->ass->Commit(_("line insertion"), AssFile::COMMIT_DIAG_ADDREM);
		c->selectionController->SetSelectionAndActive({ new_line }, new_line);
	}
};

struct subtitle_insert_after_videotime final : public validate_nonempty_selection_video_loaded {
	CMD_NAME("subtitle/insert/after/videotime")
	STR_MENU("After Current, at Video Time")
	STR_DISP("After Current, at Video Time")
	STR_HELP("Insert a new line after the current one, starting at video time")

	void operator()(agi::Context *c) override {
		insert_subtitle_at_video(c, true);
	}
};

struct subtitle_insert_before final : public validate_nonempty_selection {
	CMD_NAME("subtitle/insert/before")
	STR_MENU("&Before Current")
	STR_DISP("Before Current")
	STR_HELP("Insert a new line before the current one")

	void operator()(agi::Context *c) override {
		AssDialogue *active_line = c->selectionController->GetActiveLine();

		auto new_line = new AssDialogue;
		new_line->Style = active_line->Style;
		new_line->End = active_line->Start;
		new_line->Start = new_line->End - OPT_GET("Timing/Default Duration")->GetInt();

		for (auto it = c->ass->Events.begin(); it != c->ass->Events.end(); ++it) {
			auto diag = &*it;

			// Limit the line to the available time
			if (diag->End <= new_line->End)
				new_line->Start = std::max(new_line->Start, diag->End);

			// If we just hit the active line, insert the new line before it
			if (diag == active_line)
				c->ass->Events.insert(it, *new_line);
		}

		c->ass->Commit(_("line insertion"), AssFile::COMMIT_DIAG_ADDREM);
		c->selectionController->SetSelectionAndActive({ new_line }, new_line);
	}
};

struct subtitle_insert_before_videotime final : public validate_nonempty_selection_video_loaded {
	CMD_NAME("subtitle/insert/before/videotime")
	STR_MENU("Before Current, at Video Time")
	STR_DISP("Before Current, at Video Time")
	STR_HELP("Insert a new line before the current one, starting at video time")

	void operator()(agi::Context *c) override {
		insert_subtitle_at_video(c, false);
	}
};

bool is_okay_to_close_subtitles(agi::Context *c) {
#ifdef __APPLE__
	return true;
#else
	return c->subsController->TryToClose() != wxCANCEL;
#endif
}

void load_subtitles(agi::Context *c, agi::fs::path const& path, std::string const& encoding="") {
#ifdef __APPLE__
	wxGetApp().EmptyProjectContext().project->LoadSubtitles(path, encoding);
#else
	c->project->LoadSubtitles(path, encoding);
#endif
}

struct subtitle_new final : public Command {
	CMD_NAME("subtitle/new")
	CMD_ICON(new_toolbutton)
	STR_MENU("&New Subtitles")
	STR_DISP("New Subtitles")
	STR_HELP("New subtitles")

	void operator()(agi::Context *c) override {
#ifdef __APPLE__
		wxGetApp().NewProjectContext();
#else
		if (is_okay_to_close_subtitles(c))
			c->project->CloseSubtitles();
#endif
	}
};

struct subtitle_close final : public Command {
	CMD_NAME("subtitle/close")
	CMD_ICON(new_toolbutton)
	STR_MENU("Close")
	STR_DISP("Close")
	STR_HELP("Close")

	void operator()(agi::Context *c) override {
		c->frame->Close();
	}
};

struct subtitle_open final : public Command {
	CMD_NAME("subtitle/open")
	CMD_ICON(open_toolbutton)
	STR_MENU("&Open Subtitles...")
	STR_DISP("Open Subtitles")
	STR_HELP("Open a subtitles file")

	void operator()(agi::Context *c) override {
		if (!is_okay_to_close_subtitles(c)) return;

		auto filename = OpenFileSelector(_("Open subtitles file"), "Path/Last/Subtitles", "","", SubtitleFormat::GetWildcards(0), c->parent);
		if (!filename.empty())
			load_subtitles(c, filename);
	}
};

struct subtitle_open_autosave final : public Command {
	CMD_NAME("subtitle/open/autosave")
	STR_MENU("Open A&utosaved Subtitles...")
	STR_DISP("Open Autosaved Subtitles")
	STR_HELP("Open a previous version of a file which was autosaved by Aegisub")

	void operator()(agi::Context *c) override {
		if (!is_okay_to_close_subtitles(c)) return;
		auto filename = PickAutosaveFile(c->parent);
		if (!filename.empty())
			load_subtitles(c, filename);
	}
};

struct subtitle_open_charset final : public Command {
	CMD_NAME("subtitle/open/charset")
	CMD_ICON(open_with_toolbutton)
	STR_MENU("Open Subtitles with &Charset...")
	STR_DISP("Open Subtitles with Charset")
	STR_HELP("Open a subtitles file with a specific file encoding")

	void operator()(agi::Context *c) override {
		if (!is_okay_to_close_subtitles(c)) return;

		auto filename = OpenFileSelector(_("Open subtitles file"), "Path/Last/Subtitles", "","", SubtitleFormat::GetWildcards(0), c->parent);
		if (filename.empty()) return;

		wxString charset = wxGetSingleChoice(_("Choose charset code:"), _("Charset"), agi::charset::GetEncodingsList<wxArrayString>(), c->parent, -1, -1, true, 250, 200);
		if (charset.empty()) return;

		load_subtitles(c, filename, from_wx(charset));
	}
};

struct subtitle_open_video final : public Command {
	CMD_NAME("subtitle/open/video")
	STR_MENU("Open Subtitles from &Video")
	STR_DISP("Open Subtitles from Video")
	STR_HELP("Open the subtitles from the current video file")
	CMD_TYPE(COMMAND_VALIDATE)

	void operator()(agi::Context *c) override {
		if (c->subsController->TryToClose() == wxCANCEL) return;
		c->project->LoadSubtitles(c->project->VideoName(), "binary", false);
	}

	bool Validate(const agi::Context *c) override {
		return c->project->CanLoadSubtitlesFromVideo();
	}
};

struct subtitle_properties final : public Command {
	CMD_NAME("subtitle/properties")
	CMD_ICON(properties_toolbutton)
	STR_MENU("&Properties...")
	STR_DISP("Properties")
	STR_HELP("Open script properties window")

	void operator()(agi::Context *c) override {
		c->videoController->Stop();
		ShowPropertiesDialog(c);
	}
};

static void save_subtitles(agi::Context *c, agi::fs::path filename) {
	if (filename.empty()) {
		c->videoController->Stop();
		filename = SaveFileSelector(_("Save subtitles file"), "Path/Last/Subtitles",
			c->subsController->Filename().stem().string() + ".ass", "ass",
			"Advanced Substation Alpha (*.ass)|*.ass", c->parent);
		if (filename.empty()) return;
	}

	try {
		c->subsController->Save(filename);
	}
	catch (const agi::Exception& err) {
		wxMessageBox(to_wx(err.GetMessage()), "Error", wxOK | wxICON_ERROR | wxCENTER, c->parent);
	}
	catch (...) {
		wxMessageBox("Unknown error", "Error", wxOK | wxICON_ERROR | wxCENTER, c->parent);
	}
}

struct subtitle_save final : public Command {
	CMD_NAME("subtitle/save")
	CMD_ICON(save_toolbutton)
	STR_MENU("&Save Subtitles")
	STR_DISP("Save Subtitles")
	STR_HELP("Save the current subtitles")
	CMD_TYPE(COMMAND_VALIDATE)

	void operator()(agi::Context *c) override {
		save_subtitles(c, c->subsController->CanSave() ? c->subsController->Filename() : "");
	}

	bool Validate(const agi::Context *c) override {
		return c->subsController->IsModified();
	}
};

struct subtitle_save_as final : public Command {
	CMD_NAME("subtitle/save/as")
	CMD_ICON(save_as_toolbutton)
	STR_MENU("Save Subtitles &as...")
	STR_DISP("Save Subtitles as")
	STR_HELP("Save subtitles with another name")

	void operator()(agi::Context *c) override {
		save_subtitles(c, "");
	}
};

struct subtitle_select_all final : public Command {
	CMD_NAME("subtitle/select/all")
	STR_MENU("Select &All")
	STR_DISP("Select All")
	STR_HELP("Select all dialogue lines")

	void operator()(agi::Context *c) override {
		Selection sel;
		boost::copy(c->ass->Events | agi::address_of, inserter(sel, sel.end()));
		c->selectionController->SetSelectedSet(std::move(sel));
	}
};

struct subtitle_select_visible final : public Command {
	CMD_NAME("subtitle/select/visible")
	CMD_ICON(select_visible_button)
	STR_MENU("Select Visible")
	STR_DISP("Select Visible")
	STR_HELP("Select all dialogue lines that are visible on the current video frame")
	CMD_TYPE(COMMAND_VALIDATE)

	void operator()(agi::Context *c) override {
		c->videoController->Stop();

		Selection new_selection;
		int frame = c->videoController->GetFrameN();

		for (auto& diag : c->ass->Events) {
			if (c->videoController->FrameAtTime(diag.Start, agi::vfr::START) <= frame &&
				c->videoController->FrameAtTime(diag.End, agi::vfr::END) >= frame)
			{
				if (new_selection.empty())
					c->selectionController->SetActiveLine(&diag);
				new_selection.insert(&diag);
			}
		}

		c->selectionController->SetSelectedSet(std::move(new_selection));
	}

	bool Validate(const agi::Context *c) override {
		return !!c->project->VideoProvider();
	}
};

struct subtitle_spellcheck final : public Command {
	CMD_NAME("subtitle/spellcheck")
	CMD_ICON(spellcheck_toolbutton)
	STR_MENU("Spell &Checker...")
	STR_DISP("Spell Checker")
	STR_HELP("Open spell checker")

	void operator()(agi::Context *c) override {
		c->videoController->Stop();
		ShowSpellcheckerDialog(c);
	}
};
}

namespace cmd {
	void init_subtitle() {
		reg(agi::make_unique<subtitle_attachment>());
		reg(agi::make_unique<subtitle_find>());
		reg(agi::make_unique<subtitle_find_next>());
		reg(agi::make_unique<subtitle_insert_after>());
		reg(agi::make_unique<subtitle_insert_after_videotime>());
		reg(agi::make_unique<subtitle_insert_before>());
		reg(agi::make_unique<subtitle_insert_before_videotime>());
		reg(agi::make_unique<subtitle_new>());
		reg(agi::make_unique<subtitle_close>());
		reg(agi::make_unique<subtitle_open>());
		reg(agi::make_unique<subtitle_open_autosave>());
		reg(agi::make_unique<subtitle_open_charset>());
		reg(agi::make_unique<subtitle_open_video>());
		reg(agi::make_unique<subtitle_properties>());
		reg(agi::make_unique<subtitle_save>());
		reg(agi::make_unique<subtitle_save_as>());
		reg(agi::make_unique<subtitle_select_all>());
		reg(agi::make_unique<subtitle_select_visible>());
		reg(agi::make_unique<subtitle_spellcheck>());
	}
}
