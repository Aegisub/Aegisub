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
#include "../ass_karaoke.h"
#include "../ass_style.h"
#include "../compat.h"
#include "../dialog_search_replace.h"
#include "../dialogs.h"
#include "../format.h"
#include "../include/aegisub/context.h"
#include "../initial_line_state.h"
#include "../libresrc/libresrc.h"
#include "../options.h"
#include "../project.h"
#include "../selection_controller.h"
#include "../subs_controller.h"
#include "../text_selection_controller.h"
#include "../utils.h"
#include "../video_controller.h"

#include <libaegisub/address_of_adaptor.h>
#include <libaegisub/of_type_adaptor.h>
#include <libaegisub/make_unique.h>

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/indirected.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/range/adaptor/sliced.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/regex.hpp>
#include <boost/tokenizer.hpp>

#include <wx/clipbrd.h>
#include <wx/fontdlg.h>
#include <wx/textentry.h>

namespace {
	using namespace boost::adaptors;
	using cmd::Command;

struct validate_sel_nonempty : public Command {
	CMD_TYPE(COMMAND_VALIDATE)
	bool Validate(const agi::Context *c) override {
		return c->selectionController->GetSelectedSet().size() > 0;
	}
};

struct validate_video_and_sel_nonempty : public Command {
	CMD_TYPE(COMMAND_VALIDATE)
	bool Validate(const agi::Context *c) override {
		return c->project->VideoProvider() && !c->selectionController->GetSelectedSet().empty();
	}
};

struct validate_sel_multiple : public Command {
	CMD_TYPE(COMMAND_VALIDATE)
	bool Validate(const agi::Context *c) override {
		return c->selectionController->GetSelectedSet().size() > 1;
	}
};

template<typename String>
AssDialogue *get_dialogue(String data) {
	boost::trim(data);
	try {
		// Try to interpret the line as an ASS line
		return new AssDialogue(data);
	}
	catch (...) {
		// Line didn't parse correctly, assume it's plain text that
		// should be pasted in the Text field only
		auto d = new AssDialogue;
		d->End = 0;
		d->Text = data;
		return d;
	}
}

template<typename Paster>
void paste_lines(agi::Context *c, bool paste_over, Paster&& paste_line) {
	std::string data = GetClipboard();
	if (data.empty()) return;

	AssDialogue *first = nullptr;
	Selection newsel;

	boost::char_separator<char> sep("\r\n");
	for (auto curdata : boost::tokenizer<boost::char_separator<char>>(data, sep)) {
		AssDialogue *inserted = paste_line(get_dialogue(curdata));
		if (!inserted)
			break;

		newsel.insert(inserted);
		if (!first)
			first = inserted;
	}

	if (first) {
		c->ass->Commit(_("paste"), paste_over ? AssFile::COMMIT_DIAG_FULL : AssFile::COMMIT_DIAG_ADDREM);

		if (!paste_over)
			c->selectionController->SetSelectionAndActive(std::move(newsel), first);
	}
}

AssDialogue *paste_over(wxWindow *parent, std::vector<bool>& pasteOverOptions, AssDialogue *new_line, AssDialogue *old_line) {
	if (pasteOverOptions.empty()) {
		if (!ShowPasteOverDialog(parent)) return nullptr;
		pasteOverOptions = OPT_GET("Tool/Paste Lines Over/Fields")->GetListBool();
	}

	if (pasteOverOptions[0])  old_line->Comment   = new_line->Comment;
	if (pasteOverOptions[1])  old_line->Layer     = new_line->Layer;
	if (pasteOverOptions[2])  old_line->Start     = new_line->Start;
	if (pasteOverOptions[3])  old_line->End       = new_line->End;
	if (pasteOverOptions[4])  old_line->Style     = new_line->Style;
	if (pasteOverOptions[5])  old_line->Actor     = new_line->Actor;
	if (pasteOverOptions[6])  old_line->Margin[0] = new_line->Margin[0];
	if (pasteOverOptions[7])  old_line->Margin[1] = new_line->Margin[1];
	if (pasteOverOptions[8])  old_line->Margin[2] = new_line->Margin[2];
	if (pasteOverOptions[9])  old_line->Effect    = new_line->Effect;
	if (pasteOverOptions[10]) old_line->Text      = new_line->Text;

	return old_line;
}

struct parsed_line {
	AssDialogue *line;
	std::vector<std::unique_ptr<AssDialogueBlock>> blocks;

	parsed_line(AssDialogue *line) : line(line), blocks(line->ParseTags()) { }
	parsed_line(parsed_line&& r) = default;

	const AssOverrideTag *find_tag(int blockn, std::string const& tag_name, std::string const& alt) const {
		for (auto ovr : blocks | sliced(0, blockn + 1) | reversed | agi::of_type<AssDialogueBlockOverride>()) {
			for (auto const& tag : ovr->Tags | reversed) {
				if (tag.Name == tag_name || tag.Name == alt)
					return &tag;
			}
		}
		return nullptr;
	}

	template<typename T>
	T get_value(int blockn, T initial, std::string const& tag_name, std::string const& alt = "") const {
		auto tag = find_tag(blockn, tag_name, alt);
		if (tag)
			return tag->Params[0].template Get<T>(initial);
		return initial;
	}

	int block_at_pos(int pos) const {
		auto const& text = line->Text.get();
		int n = 0;
		int max = text.size() - 1;
		bool in_block = false;

		for (int i = 0; i <= max; ++i) {
			if (text[i] == '{') {
				if (!in_block && i > 0 && pos >= 0)
					++n;
				in_block = true;
			}
			else if (text[i] == '}' && in_block) {
				in_block = false;
				if (pos > 0 && (i + 1 == max || text[i + 1] != '{'))
					n++;
			}
			else if (!in_block) {
				if (--pos == 0)
					return n + (i < max && text[i + 1] == '{');
			}
		}

		return n - in_block;
	}

	int set_tag(std::string const& tag, std::string const& value, int norm_pos, int orig_pos) {
		int blockn = block_at_pos(norm_pos);

		AssDialogueBlockPlain *plain = nullptr;
		AssDialogueBlockOverride *ovr = nullptr;
		while (blockn >= 0 && !plain && !ovr) {
			AssDialogueBlock *block = blocks[blockn].get();
			switch (block->GetType()) {
			case AssBlockType::PLAIN:
				plain = static_cast<AssDialogueBlockPlain *>(block);
				break;
			case AssBlockType::DRAWING:
				--blockn;
				break;
			case AssBlockType::COMMENT:
				--blockn;
				orig_pos = line->Text.get().rfind('{', orig_pos);
				break;
			case AssBlockType::OVERRIDE:
				ovr = static_cast<AssDialogueBlockOverride*>(block);
				break;
			}
		}

		// If we didn't hit a suitable block for inserting the override just put
		// it at the beginning of the line
		if (blockn < 0)
			orig_pos = 0;

		std::string insert(tag + value);
		int shift = insert.size();
		if (plain || blockn < 0) {
			line->Text = line->Text.get().substr(0, orig_pos) + "{" + insert + "}" + line->Text.get().substr(orig_pos);
			shift += 2;
			blocks = line->ParseTags();
		}
		else if (ovr) {
			std::string alt;
			if (tag == "\\c") alt = "\\1c";
			// Remove old of same
			bool found = false;
			for (size_t i = 0; i < ovr->Tags.size(); i++) {
				std::string const& name = ovr->Tags[i].Name;
				if (tag == name || alt == name) {
					shift -= ((std::string)ovr->Tags[i]).size();
					if (found) {
						ovr->Tags.erase(ovr->Tags.begin() + i);
						i--;
					}
					else {
						ovr->Tags[i].Params[0].Set(value);
						found = true;
					}
				}
			}
			if (!found)
				ovr->AddTag(insert);

			line->UpdateText(blocks);
		}
		else
			assert(false);

		return shift;
	}
};

int normalize_pos(std::string const& text, int pos) {
	int plain_len = 0;
	bool in_block = false;

	for (int i = 0, max = text.size() - 1; i < pos && i <= max; ++i) {
		if (text[i] == '{')
			in_block = true;
		if (!in_block)
			++plain_len;
		if (text[i] == '}' && in_block)
			in_block = false;
	}

	return plain_len;
}

template<typename Func>
void update_lines(const agi::Context *c, wxString const& undo_msg, Func&& f) {
	const auto active_line = c->selectionController->GetActiveLine();
	const int sel_start = c->textSelectionController->GetSelectionStart();
	const int sel_end = c->textSelectionController->GetSelectionEnd();
	const int norm_sel_start = normalize_pos(active_line->Text, sel_start);
	const int norm_sel_end = normalize_pos(active_line->Text, sel_end);
	int active_sel_shift = 0;

	for (const auto line : c->selectionController->GetSelectedSet()) {
		int shift = f(line, sel_start, sel_end, norm_sel_start, norm_sel_end);
		if (line == active_line)
			active_sel_shift = shift;
	}

	auto const& sel = c->selectionController->GetSelectedSet();
	c->ass->Commit(undo_msg, AssFile::COMMIT_DIAG_TEXT, -1, sel.size() == 1 ? *sel.begin() : nullptr);
	if (active_sel_shift != 0)
		c->textSelectionController->SetSelection(sel_start + active_sel_shift, sel_end + active_sel_shift);
}

void toggle_override_tag(const agi::Context *c, bool (AssStyle::*field), const char *tag, wxString const& undo_msg) {
	update_lines(c, undo_msg, [&](AssDialogue *line, int sel_start, int sel_end, int norm_sel_start, int norm_sel_end) {
		AssStyle const* const style = c->ass->GetStyle(line->Style);
		bool state = style ? style->*field : AssStyle().*field;

		parsed_line parsed(line);
		int blockn = parsed.block_at_pos(norm_sel_start);

		state = parsed.get_value(blockn, state, tag);

		int shift = parsed.set_tag(tag, state ? "0" : "1", norm_sel_start, sel_start);
		if (sel_start != sel_end)
			parsed.set_tag(tag, state ? "1" : "0", norm_sel_end, sel_end + shift);
		return shift;
	});
}

void show_color_picker(const agi::Context *c, agi::Color (AssStyle::*field), const char *tag, const char *alt, const char *alpha) {
	agi::Color initial_color;
	const auto active_line = c->selectionController->GetActiveLine();
	const int sel_start = c->textSelectionController->GetSelectionStart();
	const int sel_end = c->textSelectionController->GetSelectionStart();
	const int norm_sel_start = normalize_pos(active_line->Text, sel_start);

	auto const& sel = c->selectionController->GetSelectedSet();
	using line_info = std::pair<agi::Color, parsed_line>;
	std::vector<line_info> lines;
	for (auto line : sel) {
		AssStyle const* const style = c->ass->GetStyle(line->Style);
		agi::Color color = (style ? style->*field : AssStyle().*field);

		parsed_line parsed(line);
		int blockn = parsed.block_at_pos(norm_sel_start);

		int a = parsed.get_value(blockn, (int)color.a, alpha, "\\alpha");
		color = parsed.get_value(blockn, color, tag, alt);
		color.a = a;

		if (line == active_line)
			initial_color = color;

		lines.emplace_back(color, std::move(parsed));
	}

	int active_shift = 0;
	int commit_id = -1;
	bool ok = GetColorFromUser(c->parent, initial_color, true, [&](agi::Color new_color) {
		for (auto& line : lines) {
			int shift = line.second.set_tag(tag, new_color.GetAssOverrideFormatted(), norm_sel_start, sel_start);
			if (new_color.a != line.first.a) {
				shift += line.second.set_tag(alpha, agi::format("&H%02X&", (int)new_color.a), norm_sel_start, sel_start + shift);
				line.first.a = new_color.a;
			}

			if (line.second.line == active_line)
				active_shift = shift;
		}

		commit_id = c->ass->Commit(_("set color"), AssFile::COMMIT_DIAG_TEXT, commit_id, sel.size() == 1 ? *sel.begin() : nullptr);
		if (active_shift)
			c->textSelectionController->SetSelection(sel_start + active_shift, sel_start + active_shift);
	});

	if (!ok && commit_id != -1) {
		c->subsController->Undo();
		c->textSelectionController->SetSelection(sel_start, sel_end);
	}
}

struct edit_color_primary final : public Command {
	CMD_NAME("edit/color/primary")
	CMD_ICON(button_color_one)
	STR_MENU("Primary Color...")
	STR_DISP("Primary Color")
	STR_HELP("Set the primary fill color (\\c) at the cursor position")

	void operator()(agi::Context *c) override {
		show_color_picker(c, &AssStyle::primary, "\\c", "\\1c", "\\1a");
	}
};

struct edit_color_secondary final : public Command {
	CMD_NAME("edit/color/secondary")
	CMD_ICON(button_color_two)
	STR_MENU("Secondary Color...")
	STR_DISP("Secondary Color")
	STR_HELP("Set the secondary (karaoke) fill color (\\2c) at the cursor position")

	void operator()(agi::Context *c) override {
		show_color_picker(c, &AssStyle::secondary, "\\2c", "", "\\2a");
	}
};

struct edit_color_outline final : public Command {
	CMD_NAME("edit/color/outline")
	CMD_ICON(button_color_three)
	STR_MENU("Outline Color...")
	STR_DISP("Outline Color")
	STR_HELP("Set the outline color (\\3c) at the cursor position")

	void operator()(agi::Context *c) override {
		show_color_picker(c, &AssStyle::outline, "\\3c", "", "\\3a");
	}
};

struct edit_color_shadow final : public Command {
	CMD_NAME("edit/color/shadow")
	CMD_ICON(button_color_four)
	STR_MENU("Shadow Color...")
	STR_DISP("Shadow Color")
	STR_HELP("Set the shadow color (\\4c) at the cursor position")

	void operator()(agi::Context *c) override {
		show_color_picker(c, &AssStyle::shadow, "\\4c", "", "\\4a");
	}
};

struct edit_style_bold final : public Command {
	CMD_NAME("edit/style/bold")
	CMD_ICON(button_bold)
	STR_MENU("Toggle Bold")
	STR_DISP("Toggle Bold")
	STR_HELP("Toggle bold (\\b) for the current selection or at the current cursor position")

	void operator()(agi::Context *c) override {
		toggle_override_tag(c, &AssStyle::bold, "\\b", _("toggle bold"));
	}
};

struct edit_style_italic final : public Command {
	CMD_NAME("edit/style/italic")
	CMD_ICON(button_italics)
	STR_MENU("Toggle Italics")
	STR_DISP("Toggle Italics")
	STR_HELP("Toggle italics (\\i) for the current selection or at the current cursor position")

	void operator()(agi::Context *c) override {
		toggle_override_tag(c, &AssStyle::italic, "\\i", _("toggle italic"));
	}
};

struct edit_style_underline final : public Command {
	CMD_NAME("edit/style/underline")
	CMD_ICON(button_underline)
	STR_MENU("Toggle Underline")
	STR_DISP("Toggle Underline")
	STR_HELP("Toggle underline (\\u) for the current selection or at the current cursor position")

	void operator()(agi::Context *c) override {
		toggle_override_tag(c, &AssStyle::underline, "\\u", _("toggle underline"));
	}
};

struct edit_style_strikeout final : public Command {
	CMD_NAME("edit/style/strikeout")
	CMD_ICON(button_strikeout)
	STR_MENU("Toggle Strikeout")
	STR_DISP("Toggle Strikeout")
	STR_HELP("Toggle strikeout (\\s) for the current selection or at the current cursor position")

	void operator()(agi::Context *c) override {
		toggle_override_tag(c, &AssStyle::strikeout, "\\s", _("toggle strikeout"));
	}
};

struct edit_font final : public Command {
	CMD_NAME("edit/font")
	CMD_ICON(button_fontname)
	STR_MENU("Font Face...")
	STR_DISP("Font Face")
	STR_HELP("Select a font face and size")

	void operator()(agi::Context *c) override {
		const parsed_line active(c->selectionController->GetActiveLine());
		const int insertion_point = normalize_pos(active.line->Text, c->textSelectionController->GetInsertionPoint());

		auto font_for_line = [&](parsed_line const& line) -> wxFont {
			const int blockn = line.block_at_pos(insertion_point);

			const AssStyle *style = c->ass->GetStyle(line.line->Style);
			const AssStyle default_style;
			if (!style)
				style = &default_style;

			return wxFont(
				line.get_value(blockn, (int)style->fontsize, "\\fs"),
				wxFONTFAMILY_DEFAULT,
				line.get_value(blockn, style->italic, "\\i") ? wxFONTSTYLE_ITALIC : wxFONTSTYLE_NORMAL,
				line.get_value(blockn, style->bold, "\\b") ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL,
				line.get_value(blockn, style->underline, "\\u"),
				to_wx(line.get_value(blockn, style->font, "\\fn")));
		};

		const wxFont initial = font_for_line(active);
		const wxFont font = wxGetFontFromUser(c->parent, initial);
		if (!font.Ok() || font == initial) return;

		update_lines(c, _("set font"), [&](AssDialogue *line, int sel_start, int sel_end, int norm_sel_start, int norm_sel_end) {
			parsed_line parsed(line);
			const wxFont startfont = font_for_line(parsed);
			int shift = 0;
			auto do_set_tag = [&](const char *tag_name, std::string const& value) {
				shift += parsed.set_tag(tag_name, value, norm_sel_start, sel_start + shift);
			};

			if (font.GetFaceName() != startfont.GetFaceName())
				do_set_tag("\\fn", from_wx(font.GetFaceName()));
			if (font.GetPointSize() != startfont.GetPointSize())
				do_set_tag("\\fs", std::to_string(font.GetPointSize()));
			if (font.GetWeight() != startfont.GetWeight())
				do_set_tag("\\b", std::to_string(font.GetWeight() == wxFONTWEIGHT_BOLD));
			if (font.GetStyle() != startfont.GetStyle())
				do_set_tag("\\i", std::to_string(font.GetStyle() == wxFONTSTYLE_ITALIC));
			if (font.GetUnderlined() != startfont.GetUnderlined())
				do_set_tag("\\i", std::to_string(font.GetUnderlined()));

			return shift;
		});
	}
};

struct edit_find_replace final : public Command {
	CMD_NAME("edit/find_replace")
	CMD_ICON(find_replace_menu)
	STR_MENU("Find and R&eplace...")
	STR_DISP("Find and Replace")
	STR_HELP("Find and replace words in subtitles")

	void operator()(agi::Context *c) override {
		c->videoController->Stop();
		DialogSearchReplace::Show(c, true);
	}
};

static void copy_lines(agi::Context *c) {
	SetClipboard(join(c->selectionController->GetSortedSelection()
		| transformed(static_cast<std::string(*)(AssDialogue*)>([](AssDialogue *d) { return d->GetEntryData(); })),
		"\r\n"));
}

static void delete_lines(agi::Context *c, wxString const& commit_message) {
	auto const& sel = c->selectionController->GetSelectedSet();

	// Find a line near the active line not being deleted to make the new active line
	AssDialogue *pre_sel = nullptr;
	AssDialogue *post_sel = nullptr;
	bool hit_selection = false;

	for (auto& diag : c->ass->Events) {
		if (sel.count(&diag))
			hit_selection = true;
		else if (hit_selection && !post_sel) {
			post_sel = &diag;
			break;
		}
		else
			pre_sel = &diag;
	}

	// Remove the selected lines, but defer the deletion until after we select
	// different lines. We can't just change the selection first because we may
	// need to create a new dialogue line for it, and we can't select dialogue
	// lines until after they're committed.
	std::vector<std::unique_ptr<AssDialogue>> to_delete;
	c->ass->Events.remove_and_dispose_if([&sel](AssDialogue const& e) {
		return sel.count(const_cast<AssDialogue *>(&e));
	}, [&](AssDialogue *e) {
		to_delete.emplace_back(e);
	});

	AssDialogue *new_active = post_sel;
	if (!new_active)
		new_active = pre_sel;
	// If we didn't get a new active line then we just deleted all the dialogue
	// lines, so make a new one
	if (!new_active) {
		new_active = new AssDialogue;
		c->ass->Events.push_back(*new_active);
	}

	c->ass->Commit(commit_message, AssFile::COMMIT_DIAG_ADDREM);
	c->selectionController->SetSelectionAndActive({ new_active }, new_active);
}

struct edit_line_copy final : public validate_sel_nonempty {
	CMD_NAME("edit/line/copy")
	CMD_ICON(copy_button)
	STR_MENU("&Copy Lines")
	STR_DISP("Copy Lines")
	STR_HELP("Copy subtitles to the clipboard")

	void operator()(agi::Context *c) override {
		// Ideally we'd let the control's keydown handler run and only deal
		// with the events not processed by it, but that doesn't seem to be
		// possible with how wx implements key event handling - the native
		// platform processing is evoked only if the wx event is unprocessed,
		// and there's no way to do something if the native platform code leaves
		// it unprocessed

		if (wxTextEntryBase *ctrl = dynamic_cast<wxTextEntryBase*>(c->parent->FindFocus()))
			ctrl->Copy();
		else {
			copy_lines(c);
		}
	}
};

struct edit_line_cut: public validate_sel_nonempty {
	CMD_NAME("edit/line/cut")
	CMD_ICON(cut_button)
	STR_MENU("Cu&t Lines")
	STR_DISP("Cut Lines")
	STR_HELP("Cut subtitles")

	void operator()(agi::Context *c) override {
		if (wxTextEntryBase *ctrl = dynamic_cast<wxTextEntryBase*>(c->parent->FindFocus()))
			ctrl->Cut();
		else {
			copy_lines(c);
			delete_lines(c, _("cut lines"));
		}
	}
};

struct edit_line_delete final : public validate_sel_nonempty {
	CMD_NAME("edit/line/delete")
	CMD_ICON(delete_button)
	STR_MENU("De&lete Lines")
	STR_DISP("Delete Lines")
	STR_HELP("Delete currently selected lines")

	void operator()(agi::Context *c) override {
		delete_lines(c, _("delete lines"));
	}
};

static void duplicate_lines(agi::Context *c, int shift) {
	auto const& sel = c->selectionController->GetSelectedSet();
	auto in_selection = [&](AssDialogue const& d) { return sel.count(const_cast<AssDialogue *>(&d)); };

	Selection new_sel;
	AssDialogue *new_active = nullptr;

	auto start = c->ass->Events.begin();
	auto end = c->ass->Events.end();
	while (start != end) {
		// Find the first line in the selection
		start = std::find_if(start, end, in_selection);
		if (start == end) break;

		// And the last line in this contiguous selection
		auto insert_pos = std::find_if_not(start, end, in_selection);
		auto last = std::prev(insert_pos);

		// Duplicate each of the selected lines, inserting them in a block
		// after the selected block
		do {
			auto old_diag = &*start;
			auto  new_diag = new AssDialogue(*old_diag);

			c->ass->Events.insert(insert_pos, *new_diag);
			new_sel.insert(new_diag);
			if (!new_active)
				new_active = new_diag;

			if (shift) {
				int cur_frame = c->videoController->GetFrameN();
				int old_start = c->videoController->FrameAtTime(new_diag->Start, agi::vfr::START);
				int old_end = c->videoController->FrameAtTime(new_diag->End, agi::vfr::END);

				// If the current frame isn't within the range of the line then
				// splitting doesn't make any sense, so instead just duplicate
				// the line and set the new one to just this frame
				if (cur_frame < old_start || cur_frame > old_end) {
					new_diag->Start = c->videoController->TimeAtFrame(cur_frame, agi::vfr::START);
					new_diag->End = c->videoController->TimeAtFrame(cur_frame, agi::vfr::END);
				}
				/// @todo This does dumb things when old_start == old_end
				else if (shift < 0) {
					old_diag->End = c->videoController->TimeAtFrame(cur_frame - 1, agi::vfr::END);
					new_diag->Start = c->videoController->TimeAtFrame(cur_frame, agi::vfr::START);
				}
				else {
					old_diag->Start = c->videoController->TimeAtFrame(cur_frame + 1, agi::vfr::START);
					new_diag->End = c->videoController->TimeAtFrame(cur_frame, agi::vfr::END);
				}

				/// @todo also split \t and \move?
			}
		} while (start++ != last);

		// Skip over the lines we just made
		start = insert_pos;
	}

	if (new_sel.empty()) return;

	c->ass->Commit(shift ? _("split") : _("duplicate lines"), AssFile::COMMIT_DIAG_ADDREM);

	c->selectionController->SetSelectionAndActive(std::move(new_sel), new_active);
}

struct edit_line_duplicate final : public validate_sel_nonempty {
	CMD_NAME("edit/line/duplicate")
	STR_MENU("&Duplicate Lines")
	STR_DISP("Duplicate Lines")
	STR_HELP("Duplicate the selected lines")

	void operator()(agi::Context *c) override {
		duplicate_lines(c, 0);
	}
};

struct edit_line_duplicate_shift final : public validate_video_and_sel_nonempty {
	CMD_NAME("edit/line/split/after")
	STR_MENU("Split lines after current frame")
	STR_DISP("Split lines after current frame")
	STR_HELP("Split the current line into a line which ends on the current frame and a line which starts on the next frame")
	CMD_TYPE(COMMAND_VALIDATE)

	void operator()(agi::Context *c) override {
		duplicate_lines(c, 1);
	}
};

struct edit_line_duplicate_shift_back final : public validate_video_and_sel_nonempty {
	CMD_NAME("edit/line/split/before")
	STR_MENU("Split lines before current frame")
	STR_DISP("Split lines before current frame")
	STR_HELP("Split the current line into a line which ends on the previous frame and a line which starts on the current frame")
	CMD_TYPE(COMMAND_VALIDATE)

	void operator()(agi::Context *c) override {
		duplicate_lines(c, -1);
	}
};

static void combine_lines(agi::Context *c, void (*combiner)(AssDialogue *, AssDialogue *), wxString const& message) {
	auto sel = c->selectionController->GetSortedSelection();

	AssDialogue *first = sel[0];
	combiner(first, nullptr);
	for (size_t i = 1; i < sel.size(); ++i) {
		combiner(first, sel[i]);
		first->End = std::max(first->End, sel[i]->End);
		delete sel[i];
	}

	c->selectionController->SetSelectionAndActive({first}, first);

	c->ass->Commit(message, AssFile::COMMIT_DIAG_ADDREM | AssFile::COMMIT_DIAG_FULL);
}

static void combine_karaoke(AssDialogue *first, AssDialogue *second) {
	if (second)
		first->Text = first->Text.get() + "{\\k" + std::to_string((second->End - second->Start) / 10) + "}" + second->Text.get();
	else
		first->Text = "{\\k" + std::to_string((first->End - first->Start) / 10) + "}" + first->Text.get();
}

static void combine_concat(AssDialogue *first, AssDialogue *second) {
	if (second)
		first->Text = first->Text.get() + " " + second->Text.get();
}

static void combine_drop(AssDialogue *, AssDialogue *) { }

struct edit_line_join_as_karaoke final : public validate_sel_multiple {
	CMD_NAME("edit/line/join/as_karaoke")
	STR_MENU("As &Karaoke")
	STR_DISP("As Karaoke")
	STR_HELP("Join selected lines in a single one, as karaoke")

	void operator()(agi::Context *c) override {
		combine_lines(c, combine_karaoke, _("join as karaoke"));
	}
};

struct edit_line_join_concatenate final : public validate_sel_multiple {
	CMD_NAME("edit/line/join/concatenate")
	STR_MENU("&Concatenate")
	STR_DISP("Concatenate")
	STR_HELP("Join selected lines in a single one, concatenating text together")

	void operator()(agi::Context *c) override {
		combine_lines(c, combine_concat, _("join lines"));
	}
};

struct edit_line_join_keep_first final : public validate_sel_multiple {
	CMD_NAME("edit/line/join/keep_first")
	STR_MENU("Keep &First")
	STR_DISP("Keep First")
	STR_HELP("Join selected lines in a single one, keeping text of first and discarding remaining")

	void operator()(agi::Context *c) override {
		combine_lines(c, combine_drop, _("join lines"));
	}
};

static bool try_paste_lines(agi::Context *c) {
	std::string data = GetClipboard();
	boost::trim_left(data);
	if (!boost::starts_with(data, "Dialogue:")) return false;

	EntryList<AssDialogue> parsed;
	boost::char_separator<char> sep("\r\n");
	for (auto curdata : boost::tokenizer<boost::char_separator<char>>(data, sep)) {
		boost::trim(curdata);
		try {
			parsed.push_back(*new AssDialogue(curdata));
		}
		catch (...) {
			parsed.clear_and_dispose([](AssDialogue *e) { delete e; });
			return false;
		}
	}

	AssDialogue *new_active = &*parsed.begin();
	Selection new_selection;
	for (auto& line : parsed)
		new_selection.insert(&line);

	auto pos = c->ass->iterator_to(*c->selectionController->GetActiveLine());
	c->ass->Events.splice(pos, parsed, parsed.begin(), parsed.end());
	c->ass->Commit(_("paste"), AssFile::COMMIT_DIAG_ADDREM);
	c->selectionController->SetSelectionAndActive(std::move(new_selection), new_active);

	return true;
}

struct edit_line_paste final : public Command {
	CMD_NAME("edit/line/paste")
	CMD_ICON(paste_button)
	STR_MENU("&Paste Lines")
	STR_DISP("Paste Lines")
	STR_HELP("Paste subtitles")
	CMD_TYPE(COMMAND_VALIDATE)

	bool Validate(const agi::Context *) override {
		bool can_paste = false;
		if (wxTheClipboard->Open()) {
			can_paste = wxTheClipboard->IsSupported(wxDF_TEXT) || wxTheClipboard->IsSupported(wxDF_UNICODETEXT);
			wxTheClipboard->Close();
		}
		return can_paste;
	}

	void operator()(agi::Context *c) override {
		if (wxTextEntryBase *ctrl = dynamic_cast<wxTextEntryBase*>(c->parent->FindFocus())) {
			if (!try_paste_lines(c))
				ctrl->Paste();
		}
		else {
			auto pos = c->ass->iterator_to(*c->selectionController->GetActiveLine());
			paste_lines(c, false, [=](AssDialogue *new_line) -> AssDialogue * {
				c->ass->Events.insert(pos, *new_line);
				return new_line;
			});
		}
	}
};

struct edit_line_paste_over final : public Command {
	CMD_NAME("edit/line/paste/over")
	STR_MENU("Paste Lines &Over...")
	STR_DISP("Paste Lines Over")
	STR_HELP("Paste subtitles over others")
	CMD_TYPE(COMMAND_VALIDATE)

	bool Validate(const agi::Context *c) override {
		bool can_paste = !c->selectionController->GetSelectedSet().empty();
		if (can_paste && wxTheClipboard->Open()) {
			can_paste = wxTheClipboard->IsSupported(wxDF_TEXT) || wxTheClipboard->IsSupported(wxDF_UNICODETEXT);
			wxTheClipboard->Close();
		}
		return can_paste;
	}

	void operator()(agi::Context *c) override {
		auto const& sel = c->selectionController->GetSelectedSet();
		std::vector<bool> pasteOverOptions;

		// Only one line selected, so paste over downwards from the active line
		if (sel.size() < 2) {
			auto pos = c->ass->iterator_to(*c->selectionController->GetActiveLine());

			paste_lines(c, true, [&](AssDialogue *new_line) -> AssDialogue * {
				std::unique_ptr<AssDialogue> deleter(new_line);
				if (pos == c->ass->Events.end()) return nullptr;

				AssDialogue *ret = paste_over(c->parent, pasteOverOptions, new_line, &*pos);
				if (ret)
					++pos;
				return ret;
			});
		}
		else {
			// Multiple lines selected, so paste over the selection
			auto sorted_selection = c->selectionController->GetSortedSelection();
			auto pos = begin(sorted_selection);
			paste_lines(c, true, [&](AssDialogue *new_line) -> AssDialogue * {
				std::unique_ptr<AssDialogue> deleter(new_line);
				if (pos == end(sorted_selection)) return nullptr;

				AssDialogue *ret = paste_over(c->parent, pasteOverOptions, new_line, *pos);
				if (ret) ++pos;
				return ret;
			});
		}
	}
};

namespace {
std::string trim_text(std::string text) {
	boost::regex start(R"(^( |	|\\[nNh])+)");
	boost::regex end(R"(( |	|\\[nNh])+$)");

	text = regex_replace(text, start, "", boost::format_first_only);
	text = regex_replace(text, end, "", boost::format_first_only);
	return text;
}

void expand_times(AssDialogue *src, AssDialogue *dst) {
	dst->Start = std::min(dst->Start, src->Start);
	dst->End = std::max(dst->End, src->End);
}

bool check_start(AssDialogue *d1, AssDialogue *d2) {
	if (boost::starts_with(d1->Text.get(), d2->Text.get())) {
		d1->Text = trim_text(d1->Text.get().substr(d2->Text.get().size()));
		expand_times(d1, d2);
		return true;
	}
	return false;
}

bool check_end(AssDialogue *d1, AssDialogue *d2) {
	if (boost::ends_with(d1->Text.get(), d2->Text.get())) {
		d1->Text = trim_text(d1->Text.get().substr(0, d1->Text.get().size() - d2->Text.get().size()));
		expand_times(d1, d2);
		return true;
	}
	return false;
}

}

struct edit_line_recombine final : public validate_sel_multiple {
	CMD_NAME("edit/line/recombine")
	STR_MENU("Recom&bine Lines")
	STR_DISP("Recombine Lines")
	STR_HELP("Recombine subtitles which have been split and merged")

	void operator()(agi::Context *c) override {
		auto const& sel_set = c->selectionController->GetSelectedSet();
		if (sel_set.size() < 2) return;

		auto active_line = c->selectionController->GetActiveLine();

		std::vector<AssDialogue*> sel(sel_set.begin(), sel_set.end());
		boost::sort(sel, [](const AssDialogue *a, const AssDialogue *b) {
			return a->Start < b->Start;
		});

		for (auto &diag : sel)
			diag->Text = trim_text(diag->Text);

		auto end = sel.end() - 1;
		for (auto cur = sel.begin(); cur != end; ++cur) {
			auto d1 = *cur;
			auto d2 = cur + 1;

			// 1, 1+2 (or 2+1), 2 gets turned into 1, 2, 2 so kill the duplicate
			if (d1->Text == (*d2)->Text) {
				expand_times(d1, *d2);
				delete d1;
				continue;
			}

			// 1, 1+2, 1 turns into 1, 2, [empty]
			if (d1->Text.get().empty()) {
				delete d1;
				continue;
			}

			// If d2 is the last line in the selection it'll never hit the above test
			if (d2 == end && (*d2)->Text.get().empty()) {
				delete *d2;
				continue;
			}

			// 1, 1+2
			while (d2 <= end && check_start(*d2, d1))
				++d2;

			// 1, 2+1
			while (d2 <= end && check_end(*d2, d1))
				++d2;

			// 1+2, 2
			while (d2 <= end && check_end(d1, *d2))
				++d2;

			// 2+1, 2
			while (d2 <= end && check_start(d1, *d2))
				++d2;
		}

		// Remove now non-existent lines from the selection
		Selection lines, new_sel;
		boost::copy(c->ass->Events | agi::address_of, inserter(lines, lines.begin()));
		boost::set_intersection(lines, sel_set, inserter(new_sel, new_sel.begin()));

		if (new_sel.empty())
			new_sel.insert(*lines.begin());

		// Restore selection
		if (!new_sel.count(active_line))
			active_line = *new_sel.begin();
		c->selectionController->SetSelectionAndActive(std::move(new_sel), active_line);

		c->ass->Commit(_("combining"), AssFile::COMMIT_DIAG_ADDREM | AssFile::COMMIT_DIAG_FULL);
	}
};

struct edit_line_split_by_karaoke final : public validate_sel_nonempty {
	CMD_NAME("edit/line/split/by_karaoke")
	STR_MENU("Split Lines (by karaoke)")
	STR_DISP("Split Lines (by karaoke)")
	STR_HELP("Use karaoke timing to split line into multiple smaller lines")

	void operator()(agi::Context *c) override {
		auto sel = c->selectionController->GetSortedSelection();
		if (sel.empty()) return;

		Selection new_sel;
		AssKaraoke kara;

		std::vector<std::unique_ptr<AssDialogue>> to_delete;
		for (auto line : sel) {
			kara.SetLine(line);

			// If there aren't at least two tags there's nothing to split
			if (kara.size() < 2) continue;

			for (auto const& syl : kara) {
				auto new_line = new AssDialogue(*line);

				new_line->Start = syl.start_time;
				new_line->End = syl.start_time + syl.duration;
				new_line->Text = syl.GetText(false);

				c->ass->Events.insert(c->ass->iterator_to(*line), *new_line);

				new_sel.insert(new_line);
			}

			c->ass->Events.erase(c->ass->iterator_to(*line));
			to_delete.emplace_back(line);
		}

		if (to_delete.empty()) return;

		c->ass->Commit(_("splitting"), AssFile::COMMIT_DIAG_ADDREM | AssFile::COMMIT_DIAG_FULL);

		AssDialogue *new_active = c->selectionController->GetActiveLine();
		if (!new_sel.count(c->selectionController->GetActiveLine()))
			new_active = *new_sel.begin();
		c->selectionController->SetSelectionAndActive(std::move(new_sel), new_active);
	}
};

void split_lines(agi::Context *c, AssDialogue *&n1, AssDialogue *&n2) {
	int pos = c->textSelectionController->GetSelectionStart();

	n1 = c->selectionController->GetActiveLine();
	n2 = new AssDialogue(*n1);
	c->ass->Events.insert(++c->ass->iterator_to(*n1), *n2);

	std::string orig = n1->Text;
	n1->Text = boost::trim_right_copy(orig.substr(0, pos));
	n2->Text = boost::trim_left_copy(orig.substr(pos));
}

template<typename Func>
void split_lines(agi::Context *c, Func&& set_time) {
	AssDialogue *n1, *n2;
	split_lines(c, n1, n2);
	set_time(n1, n2);

	c->ass->Commit(_("split"), AssFile::COMMIT_DIAG_ADDREM | AssFile::COMMIT_DIAG_FULL);
}

struct edit_line_split_estimate final : public validate_video_and_sel_nonempty {
	CMD_NAME("edit/line/split/estimate")
	STR_MENU("Split at cursor (estimate times)")
	STR_DISP("Split at cursor (estimate times)")
	STR_HELP("Split the current line at the cursor, dividing the original line's duration between the new ones")

	void operator()(agi::Context *c) override {
		split_lines(c, [](AssDialogue *n1, AssDialogue *n2) {
			size_t len = n1->Text.get().size() + n2->Text.get().size();
			if (!len) return;
			double splitPos = double(n1->Text.get().size()) / len;
			n2->Start = n1->End = (int)((n1->End - n1->Start) * splitPos) + n1->Start;
		});
	}
};

struct edit_line_split_preserve final : public validate_sel_nonempty {
	CMD_NAME("edit/line/split/preserve")
	STR_MENU("Split at cursor (preserve times)")
	STR_DISP("Split at cursor (preserve times)")
	STR_HELP("Split the current line at the cursor, setting both lines to the original line's times")

	void operator()(agi::Context *c) override {
		split_lines(c, [](AssDialogue *, AssDialogue *) { });
	}
};

struct edit_line_split_video final : public validate_video_and_sel_nonempty {
	CMD_NAME("edit/line/split/video")
	STR_MENU("Split at cursor (at video frame)")
	STR_DISP("Split at cursor (at video frame)")
	STR_HELP("Split the current line at the cursor, dividing the line's duration at the current video frame")

	void operator()(agi::Context *c) override {
		split_lines(c, [&](AssDialogue *n1, AssDialogue *n2) {
			int cur_frame = mid(
				c->videoController->FrameAtTime(n1->Start, agi::vfr::START),
				c->videoController->GetFrameN(),
				c->videoController->FrameAtTime(n1->End, agi::vfr::END));
			n1->End = n2->Start = c->videoController->TimeAtFrame(cur_frame, agi::vfr::END);
		});
	}
};

struct edit_redo final : public Command {
	CMD_NAME("edit/redo")
	CMD_ICON(redo_button)
	STR_HELP("Redo last undone action")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_DYNAMIC_NAME)

	wxString StrMenu(const agi::Context *c) const override {
		return c->subsController->IsRedoStackEmpty() ?
			_("Nothing to &redo") :
			fmt_tl("&Redo %s", c->subsController->GetRedoDescription());
	}
	wxString StrDisplay(const agi::Context *c) const override {
		return c->subsController->IsRedoStackEmpty() ?
			_("Nothing to redo") :
			fmt_tl("Redo %s", c->subsController->GetRedoDescription());
	}

	bool Validate(const agi::Context *c) override {
		return !c->subsController->IsRedoStackEmpty();
	}

	void operator()(agi::Context *c) override {
		c->subsController->Redo();
	}
};

struct edit_undo final : public Command {
	CMD_NAME("edit/undo")
	CMD_ICON(undo_button)
	STR_HELP("Undo last action")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_DYNAMIC_NAME)

	wxString StrMenu(const agi::Context *c) const override {
		return c->subsController->IsUndoStackEmpty() ?
			_("Nothing to &undo") :
			fmt_tl("&Undo %s", c->subsController->GetUndoDescription());
	}
	wxString StrDisplay(const agi::Context *c) const override {
		return c->subsController->IsUndoStackEmpty() ?
			_("Nothing to undo") :
			fmt_tl("Undo %s", c->subsController->GetUndoDescription());
	}

	bool Validate(const agi::Context *c) override {
		return !c->subsController->IsUndoStackEmpty();
	}

	void operator()(agi::Context *c) override {
		c->subsController->Undo();
	}
};

struct edit_revert final : public Command {
	CMD_NAME("edit/revert")
	STR_DISP("Revert")
	STR_MENU("Revert")
	STR_HELP("Revert the active line to its initial state (shown in the upper editor)")

	void operator()(agi::Context *c) override {
		AssDialogue *line = c->selectionController->GetActiveLine();
		line->Text = c->initialLineState->GetInitialText();
		c->ass->Commit(_("revert line"), AssFile::COMMIT_DIAG_TEXT, -1, line);
	}
};

struct edit_clear final : public Command {
	CMD_NAME("edit/clear")
	STR_DISP("Clear")
	STR_MENU("Clear")
	STR_HELP("Clear the current line's text")

	void operator()(agi::Context *c) override {
		AssDialogue *line = c->selectionController->GetActiveLine();
		line->Text = "";
		c->ass->Commit(_("clear line"), AssFile::COMMIT_DIAG_TEXT, -1, line);
	}
};

std::string get_text(AssDialogueBlock &d) { return d.GetText(); }
struct edit_clear_text final : public Command {
	CMD_NAME("edit/clear/text")
	STR_DISP("Clear Text")
	STR_MENU("Clear Text")
	STR_HELP("Clear the current line's text, leaving override tags")

	void operator()(agi::Context *c) override {
		AssDialogue *line = c->selectionController->GetActiveLine();
		auto blocks = line->ParseTags();
		line->Text = join(blocks
			| indirected
			| filtered([](AssDialogueBlock const& b) { return b.GetType() != AssBlockType::PLAIN; })
			| transformed(get_text),
			"");
		c->ass->Commit(_("clear line"), AssFile::COMMIT_DIAG_TEXT, -1, line);
	}
};

struct edit_insert_original final : public Command {
	CMD_NAME("edit/insert_original")
	STR_DISP("Insert Original")
	STR_MENU("Insert Original")
	STR_HELP("Insert the original line text at the cursor")

	void operator()(agi::Context *c) override {
		AssDialogue *line = c->selectionController->GetActiveLine();
		int sel_start = c->textSelectionController->GetSelectionStart();
		int sel_end = c->textSelectionController->GetSelectionEnd();

		line->Text = line->Text.get().substr(0, sel_start) + c->initialLineState->GetInitialText() + line->Text.get().substr(sel_end);
		c->ass->Commit(_("insert original"), AssFile::COMMIT_DIAG_TEXT, -1, line);
	}
};

}

namespace cmd {
	void init_edit() {
		reg(agi::make_unique<edit_color_primary>());
		reg(agi::make_unique<edit_color_secondary>());
		reg(agi::make_unique<edit_color_outline>());
		reg(agi::make_unique<edit_color_shadow>());
		reg(agi::make_unique<edit_font>());
		reg(agi::make_unique<edit_find_replace>());
		reg(agi::make_unique<edit_line_copy>());
		reg(agi::make_unique<edit_line_cut>());
		reg(agi::make_unique<edit_line_delete>());
		reg(agi::make_unique<edit_line_duplicate>());
		reg(agi::make_unique<edit_line_duplicate_shift>());
		reg(agi::make_unique<edit_line_duplicate_shift_back>());
		reg(agi::make_unique<edit_line_join_as_karaoke>());
		reg(agi::make_unique<edit_line_join_concatenate>());
		reg(agi::make_unique<edit_line_join_keep_first>());
		reg(agi::make_unique<edit_line_paste>());
		reg(agi::make_unique<edit_line_paste_over>());
		reg(agi::make_unique<edit_line_recombine>());
		reg(agi::make_unique<edit_line_split_by_karaoke>());
		reg(agi::make_unique<edit_line_split_estimate>());
		reg(agi::make_unique<edit_line_split_preserve>());
		reg(agi::make_unique<edit_line_split_video>());
		reg(agi::make_unique<edit_style_bold>());
		reg(agi::make_unique<edit_style_italic>());
		reg(agi::make_unique<edit_style_underline>());
		reg(agi::make_unique<edit_style_strikeout>());
		reg(agi::make_unique<edit_redo>());
		reg(agi::make_unique<edit_undo>());
		reg(agi::make_unique<edit_revert>());
		reg(agi::make_unique<edit_insert_original>());
		reg(agi::make_unique<edit_clear>());
		reg(agi::make_unique<edit_clear_text>());
	}
}
