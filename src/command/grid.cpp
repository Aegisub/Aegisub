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
#include "../audio_controller.h"
#include "../audio_timing.h"
#include "../frame_main.h"
#include "../include/aegisub/context.h"
#include "../libresrc/libresrc.h"
#include "../options.h"
#include "../selection_controller.h"

#include <libaegisub/make_unique.h>

#include "../video_controller.h"
#include "../video_display.h"

namespace {
	using cmd::Command;

struct grid_line_next final : public Command {
	CMD_NAME("grid/line/next")
	STR_MENU("Next Line")
	STR_DISP("Next Line")
	STR_HELP("Move to the next subtitle line")

	void operator()(agi::Context *c) override {
		c->selectionController->NextLine();
	}
};

struct grid_line_next_play final : public Command {
	CMD_NAME("grid/line/next/play")
	STR_MENU("Next Line")
	STR_DISP("Next Line")
	STR_HELP("Move to the next subtitle line and play current line")

	void operator()(agi::Context *c) override {
		c->selectionController->NextLine();
		c->videoController->PlayLine();
	}
};

struct grid_line_next2_play final : public Command {
	CMD_NAME("grid/line/next2/play")
	STR_MENU("Next Line Twice")
	STR_DISP("Next Line Twice")
	STR_HELP("Move to the next twice subtitle line and play current line")

	void operator()(agi::Context *c) override {
		c->selectionController->NextLine();
		c->selectionController->NextLine();
		c->videoController->PlayLine();
	}
};

struct grid_line_next_create final : public Command {
	CMD_NAME("grid/line/next/create")
	CMD_ICON(button_audio_commit)
	STR_MENU("Next Line")
	STR_DISP("Next Line")
	STR_HELP("Move to the next subtitle line, creating a new one if needed")

	void operator()(agi::Context *c) override {
		AudioTimingController *tc = c->audioController->GetTimingController();
		if (tc)
			tc->Commit();

		AssDialogue *cur = c->selectionController->GetActiveLine();
		c->selectionController->NextLine();
		if (cur == c->selectionController->GetActiveLine()) {
			auto newline = new AssDialogue;
			newline->Start = cur->End;
			newline->End = cur->End + OPT_GET("Timing/Default Duration")->GetInt();
			newline->Style = cur->Style;

			auto pos = c->ass->iterator_to(*cur);
			c->ass->Events.insert(++pos, *newline);
			c->ass->Commit(_("line insertion"), AssFile::COMMIT_DIAG_ADDREM);
			c->selectionController->NextLine();
		}
	}
};

struct grid_line_prev final : public Command {
	CMD_NAME("grid/line/prev")
	STR_MENU("Previous Line")
	STR_DISP("Previous Line")
	STR_HELP("Move to the previous line")

	void operator()(agi::Context *c) override {
		c->selectionController->PrevLine();
	}
};

struct grid_sort_actor final : public Command {
	CMD_NAME("grid/sort/actor")
	STR_MENU("&Actor Name")
	STR_DISP("Actor Name")
	STR_HELP("Sort all subtitles by their actor names")

	void operator()(agi::Context *c) override {
		c->ass->Sort(AssFile::CompActor);
		c->ass->Commit(_("sort"), AssFile::COMMIT_ORDER);
	}
};

struct validate_sel_multiple : public Command {
	CMD_TYPE(COMMAND_VALIDATE)

	bool Validate(const agi::Context *c) override {
		return c->selectionController->GetSelectedSet().size() > 1;
	}
};

struct grid_sort_actor_selected final : public validate_sel_multiple {
	CMD_NAME("grid/sort/actor/selected")
	STR_MENU("&Actor Name")
	STR_DISP("Actor Name")
	STR_HELP("Sort selected subtitles by their actor names")

	void operator()(agi::Context *c) override {
		c->ass->Sort(AssFile::CompActor, c->selectionController->GetSelectedSet());
		c->ass->Commit(_("sort"), AssFile::COMMIT_ORDER);
	}
};

struct grid_sort_effect final : public Command {
	CMD_NAME("grid/sort/effect")
	STR_MENU("&Effect")
	STR_DISP("Effect")
	STR_HELP("Sort all subtitles by their effects")

	void operator()(agi::Context *c) override {
		c->ass->Sort(AssFile::CompEffect);
		c->ass->Commit(_("sort"), AssFile::COMMIT_ORDER);
	}
};

struct grid_sort_effect_selected final : public validate_sel_multiple {
	CMD_NAME("grid/sort/effect/selected")
	STR_MENU("&Effect")
	STR_DISP("Effect")
	STR_HELP("Sort selected subtitles by their effects")

	void operator()(agi::Context *c) override {
		c->ass->Sort(AssFile::CompEffect, c->selectionController->GetSelectedSet());
		c->ass->Commit(_("sort"), AssFile::COMMIT_ORDER);
	}
};

struct grid_sort_end final : public Command {
	CMD_NAME("grid/sort/end")
	STR_MENU("&End Time")
	STR_DISP("End Time")
	STR_HELP("Sort all subtitles by their end times")

	void operator()(agi::Context *c) override {
		c->ass->Sort(AssFile::CompEnd);
		c->ass->Commit(_("sort"), AssFile::COMMIT_ORDER);
	}
};

struct grid_sort_end_selected final : public validate_sel_multiple {
	CMD_NAME("grid/sort/end/selected")
	STR_MENU("&End Time")
	STR_DISP("End Time")
	STR_HELP("Sort selected subtitles by their end times")

	void operator()(agi::Context *c) override {
		c->ass->Sort(AssFile::CompEnd, c->selectionController->GetSelectedSet());
		c->ass->Commit(_("sort"), AssFile::COMMIT_ORDER);
	}
};

struct grid_sort_layer final : public Command {
	CMD_NAME("grid/sort/layer")
	STR_MENU("&Layer")
	STR_DISP("Layer")
	STR_HELP("Sort all subtitles by their layer number")

	void operator()(agi::Context *c) override {
		c->ass->Sort(AssFile::CompLayer);
		c->ass->Commit(_("sort"), AssFile::COMMIT_ORDER);
	}
};

struct grid_sort_layer_selected final : public validate_sel_multiple {
	CMD_NAME("grid/sort/layer/selected")
	STR_MENU("&Layer")
	STR_DISP("Layer")
	STR_HELP("Sort selected subtitles by their layer number")

	void operator()(agi::Context *c) override {
		c->ass->Sort(AssFile::CompLayer, c->selectionController->GetSelectedSet());
		c->ass->Commit(_("sort"), AssFile::COMMIT_ORDER);
	}
};

struct grid_sort_start final : public Command {
	CMD_NAME("grid/sort/start")
	STR_MENU("&Start Time")
	STR_DISP("Start Time")
	STR_HELP("Sort all subtitles by their start times")

	void operator()(agi::Context *c) override {
		c->ass->Sort();
		c->ass->Commit(_("sort"), AssFile::COMMIT_ORDER);
	}
};

struct grid_sort_start_selected final : public validate_sel_multiple {
	CMD_NAME("grid/sort/start/selected")
	STR_MENU("&Start Time")
	STR_DISP("Start Time")
	STR_HELP("Sort selected subtitles by their start times")

	void operator()(agi::Context *c) override {
		c->ass->Sort(AssFile::CompStart, c->selectionController->GetSelectedSet());
		c->ass->Commit(_("sort"), AssFile::COMMIT_ORDER);
	}
};

struct grid_sort_style final : public Command {
	CMD_NAME("grid/sort/style")
	STR_MENU("St&yle Name")
	STR_DISP("Style Name")
	STR_HELP("Sort all subtitles by their style names")

	void operator()(agi::Context *c) override {
		c->ass->Sort(AssFile::CompStyle);
		c->ass->Commit(_("sort"), AssFile::COMMIT_ORDER);
	}
};

struct grid_sort_style_selected final : public validate_sel_multiple {
	CMD_NAME("grid/sort/style/selected")
	STR_MENU("St&yle Name")
	STR_DISP("Style Name")
	STR_HELP("Sort selected subtitles by their style names")

	void operator()(agi::Context *c) override {
		c->ass->Sort(AssFile::CompStyle, c->selectionController->GetSelectedSet());
		c->ass->Commit(_("sort"), AssFile::COMMIT_ORDER);
	}
};

struct grid_tag_cycle_hiding final : public Command {
	CMD_NAME("grid/tag/cycle_hiding")
	CMD_ICON(toggle_tag_hiding)
	STR_MENU("Cycle Tag Hiding Mode")
	STR_DISP("Cycle Tag Hiding Mode")
	STR_HELP("Cycle through tag hiding modes")

	void operator()(agi::Context *c) override {
		int tagMode = OPT_GET("Subtitle/Grid/Hide Overrides")->GetInt();

		// Cycle to next
		tagMode = (tagMode+1)%3;

		// Show on status bar
		wxString message;
		if (tagMode == 0) message = _("ASS Override Tag mode set to show full tags.");
		if (tagMode == 1) message = _("ASS Override Tag mode set to simplify tags.");
		if (tagMode == 2) message = _("ASS Override Tag mode set to hide tags.");
		c->frame->StatusTimeout(message,10000);

		// Set option
		OPT_SET("Subtitle/Grid/Hide Overrides")->SetInt(tagMode);
	}
};

struct grid_tags_hide final : public Command {
	CMD_NAME("grid/tags/hide")
	STR_MENU("&Hide Tags")
	STR_DISP("Hide Tags")
	STR_HELP("Hide override tags in the subtitle grid")
	CMD_TYPE(COMMAND_RADIO)

	bool IsActive(const agi::Context *) override {
		return OPT_GET("Subtitle/Grid/Hide Overrides")->GetInt() == 2;
	}

	void operator()(agi::Context *) override {
		OPT_SET("Subtitle/Grid/Hide Overrides")->SetInt(2);
	}
};

struct grid_tags_show final : public Command {
	CMD_NAME("grid/tags/show")
	STR_MENU("Sh&ow Tags")
	STR_DISP("Show Tags")
	STR_HELP("Show full override tags in the subtitle grid")
	CMD_TYPE(COMMAND_RADIO)

	bool IsActive(const agi::Context *) override {
		return OPT_GET("Subtitle/Grid/Hide Overrides")->GetInt() == 0;
	}

	void operator()(agi::Context *) override {
		OPT_SET("Subtitle/Grid/Hide Overrides")->SetInt(0);
	}
};

struct grid_tags_simplify final : public Command {
	CMD_NAME("grid/tags/simplify")
	STR_MENU("S&implify Tags")
	STR_DISP("Simplify Tags")
	STR_HELP("Replace override tags in the subtitle grid with a simplified placeholder")
	CMD_TYPE(COMMAND_RADIO)

	bool IsActive(const agi::Context *) override {
		return OPT_GET("Subtitle/Grid/Hide Overrides")->GetInt() == 1;
	}

	void operator()(agi::Context *) override {
		OPT_SET("Subtitle/Grid/Hide Overrides")->SetInt(1);
	}
};

template<class T, class U>
static bool move_one(T begin, T end, U const& to_move, bool swap) {
	size_t move_count = 0;
	auto prev = end;
	for (auto it = begin; it != end; ++it) {
		auto cur = &*it;
		if (!to_move.count(cur))
			prev = it;
		else if (prev != end) {
			it->swap_nodes(*prev);
			if (swap)
				std::swap(prev, it);
			else
				prev = it;
			if (++move_count == to_move.size())
				break;
		}
	}

	return move_count > 0;
}

struct grid_move_up final : public Command {
	CMD_NAME("grid/move/up")
	STR_MENU("Move line up")
	STR_DISP("Move line up")
	STR_HELP("Move the selected lines up one row")
	CMD_TYPE(COMMAND_VALIDATE)

	bool Validate(const agi::Context *c) override {
		return c->selectionController->GetSelectedSet().size() != 0;
	}

	void operator()(agi::Context *c) override {
		if (move_one(c->ass->Events.begin(), c->ass->Events.end(), c->selectionController->GetSelectedSet(), false))
			c->ass->Commit(_("move lines"), AssFile::COMMIT_ORDER);
	}
};

struct grid_move_down final : public Command {
	CMD_NAME("grid/move/down")
	STR_MENU("Move line down")
	STR_DISP("Move line down")
	STR_HELP("Move the selected lines down one row")
	CMD_TYPE(COMMAND_VALIDATE)

	bool Validate(const agi::Context *c) override {
		return c->selectionController->GetSelectedSet().size() != 0;
	}

	void operator()(agi::Context *c) override {
		if (move_one(c->ass->Events.rbegin(), c->ass->Events.rend(), c->selectionController->GetSelectedSet(), true))
			c->ass->Commit(_("move lines"), AssFile::COMMIT_ORDER);
	}
};

struct grid_swap final : public Command {
	CMD_NAME("grid/swap")
	CMD_ICON(arrow_sort)
	STR_MENU("Swap Lines")
	STR_DISP("Swap Lines")
	STR_HELP("Swap the two selected lines")
	CMD_TYPE(COMMAND_VALIDATE)

	bool Validate(const agi::Context *c) override {
		return c->selectionController->GetSelectedSet().size() == 2;
	}

	void operator()(agi::Context *c) override {
		auto const& sel = c->selectionController->GetSelectedSet();
		if (sel.size() == 2) {
			(*sel.begin())->swap_nodes(**sel.rbegin());
			c->ass->Commit(_("swap lines"), AssFile::COMMIT_ORDER);
		}
	}
};

}

namespace cmd {
	void init_grid() {
		reg(agi::make_unique<grid_line_next>());
		reg(agi::make_unique<grid_line_next_create>());
		reg(agi::make_unique<grid_line_prev>());
		reg(agi::make_unique<grid_sort_actor>());
		reg(agi::make_unique<grid_sort_effect>());
		reg(agi::make_unique<grid_sort_end>());
		reg(agi::make_unique<grid_sort_layer>());
		reg(agi::make_unique<grid_sort_start>());
		reg(agi::make_unique<grid_sort_style>());
		reg(agi::make_unique<grid_sort_actor_selected>());
		reg(agi::make_unique<grid_sort_effect_selected>());
		reg(agi::make_unique<grid_sort_end_selected>());
		reg(agi::make_unique<grid_sort_layer_selected>());
		reg(agi::make_unique<grid_sort_start_selected>());
		reg(agi::make_unique<grid_sort_style_selected>());
		reg(agi::make_unique<grid_move_down>());
		reg(agi::make_unique<grid_move_up>());
		reg(agi::make_unique<grid_swap>());
		reg(agi::make_unique<grid_tag_cycle_hiding>());
		reg(agi::make_unique<grid_tags_hide>());
		reg(agi::make_unique<grid_tags_show>());
		reg(agi::make_unique<grid_tags_simplify>());
		reg(agi::make_unique<grid_line_next_play>());
		reg(agi::make_unique<grid_line_next2_play>());

	}
}
