// Copyright (c) 2005, Rodrigo Braz Monteiro
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

#include "subs_edit_ctrl.h"

#include "ass_dialogue.h"
#include "command/command.h"
#include "compat.h"
#include "format.h"
#include "options.h"
#include "include/aegisub/context.h"
#include "include/aegisub/spellchecker.h"
#include "selection_controller.h"
#include "text_selection_controller.h"
#include "thesaurus.h"
#include "utils.h"

#include <libaegisub/ass/dialogue_parser.h>
#include <libaegisub/calltip_provider.h>
#include <libaegisub/character_count.h>
#include <libaegisub/make_unique.h>
#include <libaegisub/spellchecker.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <functional>

#include <wx/clipbrd.h>
#include <wx/intl.h>
#include <wx/menu.h>
#include <wx/settings.h>

// Maximum number of languages (locales)
// It should be above 100 (at least 242) and probably not more than 1000
#define LANGS_MAX 1000

/// Event ids
enum {
	EDIT_MENU_SPLIT_PRESERVE = 1400,
	EDIT_MENU_SPLIT_ESTIMATE,
	EDIT_MENU_SPLIT_VIDEO,
	EDIT_MENU_CUT,
	EDIT_MENU_COPY,
	EDIT_MENU_PASTE,
	EDIT_MENU_SELECT_ALL,
	EDIT_MENU_ADD_TO_DICT,
	EDIT_MENU_REMOVE_FROM_DICT,
	EDIT_MENU_SUGGESTION,
	EDIT_MENU_SUGGESTIONS,
	EDIT_MENU_THESAURUS = 1450,
	EDIT_MENU_THESAURUS_SUGS,
	EDIT_MENU_DIC_LANGUAGE = 1600,
	EDIT_MENU_DIC_LANGS,
	EDIT_MENU_THES_LANGUAGE = EDIT_MENU_DIC_LANGUAGE + LANGS_MAX,
	EDIT_MENU_THES_LANGS
};

SubsTextEditCtrl::SubsTextEditCtrl(wxWindow* parent, wxSize wsize, long style, agi::Context *context)
: wxStyledTextCtrl(parent, -1, wxDefaultPosition, wsize, style)
, spellchecker(SpellCheckerFactory::GetSpellChecker())
, thesaurus(agi::make_unique<Thesaurus>())
, context(context)
{
	osx::ime::inject(this);

	// Set properties
	SetWrapMode(wxSTC_WRAP_WORD);
	SetMarginWidth(1,0);
	UsePopUp(false);
	SetStyles();

	// Set hotkeys
	CmdKeyClear(wxSTC_KEY_RETURN,wxSTC_SCMOD_CTRL);
	CmdKeyClear(wxSTC_KEY_RETURN,wxSTC_SCMOD_SHIFT);
	CmdKeyClear(wxSTC_KEY_RETURN,wxSTC_SCMOD_NORM);
	CmdKeyClear(wxSTC_KEY_TAB,wxSTC_SCMOD_NORM);
	CmdKeyClear(wxSTC_KEY_TAB,wxSTC_SCMOD_SHIFT);
	CmdKeyClear('D',wxSTC_SCMOD_CTRL);
	CmdKeyClear('L',wxSTC_SCMOD_CTRL);
	CmdKeyClear('L',wxSTC_SCMOD_CTRL | wxSTC_SCMOD_SHIFT);
	CmdKeyClear('T',wxSTC_SCMOD_CTRL);
	CmdKeyClear('T',wxSTC_SCMOD_CTRL | wxSTC_SCMOD_SHIFT);
	CmdKeyClear('U',wxSTC_SCMOD_CTRL);

	using std::bind;

	Bind(wxEVT_CHAR_HOOK, &SubsTextEditCtrl::OnKeyDown, this);

	Bind(wxEVT_MENU, bind(&SubsTextEditCtrl::Cut, this), EDIT_MENU_CUT);
	Bind(wxEVT_MENU, bind(&SubsTextEditCtrl::Copy, this), EDIT_MENU_COPY);
	Bind(wxEVT_MENU, bind(&SubsTextEditCtrl::Paste, this), EDIT_MENU_PASTE);
	Bind(wxEVT_MENU, bind(&SubsTextEditCtrl::SelectAll, this), EDIT_MENU_SELECT_ALL);

	if (context) {
		Bind(wxEVT_MENU, bind(&cmd::call, "edit/line/split/preserve", context), EDIT_MENU_SPLIT_PRESERVE);
		Bind(wxEVT_MENU, bind(&cmd::call, "edit/line/split/estimate", context), EDIT_MENU_SPLIT_ESTIMATE);
		Bind(wxEVT_MENU, bind(&cmd::call, "edit/line/split/video", context), EDIT_MENU_SPLIT_VIDEO);
	}

	Bind(wxEVT_CONTEXT_MENU, &SubsTextEditCtrl::OnContextMenu, this);
	Bind(wxEVT_IDLE, std::bind(&SubsTextEditCtrl::UpdateCallTip, this));
	Bind(wxEVT_STC_DOUBLECLICK, &SubsTextEditCtrl::OnDoubleClick, this);
	Bind(wxEVT_STC_STYLENEEDED, [=](wxStyledTextEvent&) {
		{
			std::string text = GetTextRaw().data();
			if (text == line_text) return;
			line_text = move(text);
		}

		UpdateStyle();
	});

	OPT_SUB("Subtitle/Edit Box/Font Face", &SubsTextEditCtrl::SetStyles, this);
	OPT_SUB("Subtitle/Edit Box/Font Size", &SubsTextEditCtrl::SetStyles, this);
	Subscribe("Normal");
	Subscribe("Comment");
	Subscribe("Drawing");
	Subscribe("Brackets");
	Subscribe("Slashes");
	Subscribe("Tags");
	Subscribe("Error");
	Subscribe("Parameters");
	Subscribe("Line Break");
	Subscribe("Karaoke Template");
	Subscribe("Karaoke Variable");

	OPT_SUB("Colour/Subtitle/Background", &SubsTextEditCtrl::SetStyles, this);
	OPT_SUB("Subtitle/Highlight/Syntax", &SubsTextEditCtrl::UpdateStyle, this);
	OPT_SUB("App/Call Tips", &SubsTextEditCtrl::UpdateCallTip, this);

	Bind(wxEVT_MENU, [=](wxCommandEvent&) {
		if (spellchecker) spellchecker->AddWord(currentWord);
		UpdateStyle();
		SetFocus();
	}, EDIT_MENU_ADD_TO_DICT);

	Bind(wxEVT_MENU, [=](wxCommandEvent&) {
		if (spellchecker) spellchecker->RemoveWord(currentWord);
		UpdateStyle();
		SetFocus();
	}, EDIT_MENU_REMOVE_FROM_DICT);
}

SubsTextEditCtrl::~SubsTextEditCtrl() {
}

void SubsTextEditCtrl::Subscribe(std::string const& name) {
	OPT_SUB("Colour/Subtitle/Syntax/" + name, &SubsTextEditCtrl::SetStyles, this);
	OPT_SUB("Colour/Subtitle/Syntax/Background/" + name, &SubsTextEditCtrl::SetStyles, this);
	OPT_SUB("Colour/Subtitle/Syntax/Bold/" + name, &SubsTextEditCtrl::SetStyles, this);
}

BEGIN_EVENT_TABLE(SubsTextEditCtrl,wxStyledTextCtrl)
	EVT_KILL_FOCUS(SubsTextEditCtrl::OnLoseFocus)

	EVT_MENU_RANGE(EDIT_MENU_SUGGESTIONS,EDIT_MENU_THESAURUS-1,SubsTextEditCtrl::OnUseSuggestion)
	EVT_MENU_RANGE(EDIT_MENU_THESAURUS_SUGS,EDIT_MENU_DIC_LANGUAGE-1,SubsTextEditCtrl::OnUseSuggestion)
	EVT_MENU_RANGE(EDIT_MENU_DIC_LANGS,EDIT_MENU_THES_LANGUAGE-1,SubsTextEditCtrl::OnSetDicLanguage)
	EVT_MENU_RANGE(EDIT_MENU_THES_LANGS,EDIT_MENU_THES_LANGS+LANGS_MAX,SubsTextEditCtrl::OnSetThesLanguage)
END_EVENT_TABLE()

void SubsTextEditCtrl::OnLoseFocus(wxFocusEvent &event) {
	CallTipCancel();
	event.Skip();
}

void SubsTextEditCtrl::OnKeyDown(wxKeyEvent &event) {
	if (osx::ime::process_key_event(this, event)) return;
	event.Skip();

	// Workaround for wxSTC eating tabs.
	if (event.GetKeyCode() == WXK_TAB)
		Navigate(event.ShiftDown() ? wxNavigationKeyEvent::IsBackward : wxNavigationKeyEvent::IsForward);
	else if (event.GetKeyCode() == WXK_RETURN && event.GetModifiers() == wxMOD_SHIFT) {
		auto sel_start = GetSelectionStart(), sel_end = GetSelectionEnd();
		wxCharBuffer old = GetTextRaw();
		std::string data(old.data(), sel_start);
		data.append("\\N");
		data.append(old.data() + sel_end, old.length() - sel_end);
		SetTextRaw(data.c_str());

		SetSelection(sel_start + 2, sel_start + 2);
		event.Skip(false);
	}
}

void SubsTextEditCtrl::SetSyntaxStyle(int id, wxFont &font, std::string const& name, wxColor const& default_background) {
	StyleSetFont(id, font);
	StyleSetBold(id, OPT_GET("Colour/Subtitle/Syntax/Bold/" + name)->GetBool());
	StyleSetForeground(id, to_wx(OPT_GET("Colour/Subtitle/Syntax/" + name)->GetColor()));
	const agi::OptionValue *background = OPT_GET("Colour/Subtitle/Syntax/Background/" + name);
	if (background->GetType() == agi::OptionType::Color)
		StyleSetBackground(id, to_wx(background->GetColor()));
	else
		StyleSetBackground(id, default_background);
}

void SubsTextEditCtrl::SetStyles() {
	wxFont font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
	font.SetEncoding(wxFONTENCODING_DEFAULT); // this solves problems with some fonts not working properly
	wxString fontname = FontFace("Subtitle/Edit Box");
	if (!fontname.empty()) font.SetFaceName(fontname);
	font.SetPointSize(OPT_GET("Subtitle/Edit Box/Font Size")->GetInt());

	auto default_background = to_wx(OPT_GET("Colour/Subtitle/Background")->GetColor());

	namespace ss = agi::ass::SyntaxStyle;
	SetSyntaxStyle(ss::NORMAL, font, "Normal", default_background);
	SetSyntaxStyle(ss::COMMENT, font, "Comment", default_background);
	SetSyntaxStyle(ss::DRAWING, font, "Drawing", default_background);
	SetSyntaxStyle(ss::OVERRIDE, font, "Brackets", default_background);
	SetSyntaxStyle(ss::PUNCTUATION, font, "Slashes", default_background);
	SetSyntaxStyle(ss::TAG, font, "Tags", default_background);
	SetSyntaxStyle(ss::ERROR, font, "Error", default_background);
	SetSyntaxStyle(ss::PARAMETER, font, "Parameters", default_background);
	SetSyntaxStyle(ss::LINE_BREAK, font, "Line Break", default_background);
	SetSyntaxStyle(ss::KARAOKE_TEMPLATE, font, "Karaoke Template", default_background);
	SetSyntaxStyle(ss::KARAOKE_VARIABLE, font, "Karaoke Variable", default_background);

	SetCaretForeground(StyleGetForeground(ss::NORMAL));
	StyleSetBackground(wxSTC_STYLE_DEFAULT, default_background);

	// Misspelling indicator
	IndicatorSetStyle(0,wxSTC_INDIC_SQUIGGLE);
	IndicatorSetForeground(0,wxColour(255,0,0));

	// IME pending text indicator
	IndicatorSetStyle(1, wxSTC_INDIC_PLAIN);
	IndicatorSetUnder(1, true);
}

void SubsTextEditCtrl::UpdateStyle() {
	AssDialogue *diag = context ? context->selectionController->GetActiveLine() : nullptr;
	bool template_line = diag && diag->Comment && boost::istarts_with(diag->Effect.get(), "template");

	tokenized_line = agi::ass::TokenizeDialogueBody(line_text, template_line);
	agi::ass::SplitWords(line_text, tokenized_line);

	cursor_pos = -1;
	UpdateCallTip();

	#if wxCHECK_VERSION(3, 1, 0)
	StartStyling(0);
	#else
	StartStyling(0,255);
	#endif

	if (!OPT_GET("Subtitle/Highlight/Syntax")->GetBool()) {
		SetStyling(line_text.size(), 0);
		return;
	}

	if (line_text.empty()) return;

	SetIndicatorCurrent(0);
	size_t pos = 0;
	for (auto const& style_range : agi::ass::SyntaxHighlight(line_text, tokenized_line, spellchecker.get())) {
		if (style_range.type == agi::ass::SyntaxStyle::SPELLING) {
			SetStyling(style_range.length, agi::ass::SyntaxStyle::NORMAL);
			IndicatorFillRange(pos, style_range.length);
		}
		else {
			SetStyling(style_range.length, style_range.type);
			IndicatorClearRange(pos, style_range.length);
		}
		pos += style_range.length;
	}
}

void SubsTextEditCtrl::UpdateCallTip() {
	if (!OPT_GET("App/Call Tips")->GetBool()) return;

	int pos = GetCurrentPos();
	if (pos == cursor_pos) return;
	cursor_pos = pos;

	agi::Calltip new_calltip = agi::GetCalltip(tokenized_line, line_text, pos);

	if (!new_calltip.text) {
		CallTipCancel();
		return;
	}

	if (!CallTipActive() || calltip_position != new_calltip.tag_position || calltip_text != new_calltip.text)
		CallTipShow(new_calltip.tag_position, wxString::FromUTF8Unchecked(new_calltip.text));

	calltip_position = new_calltip.tag_position;
	calltip_text = new_calltip.text;

	CallTipSetHighlight(new_calltip.highlight_start, new_calltip.highlight_end);
}

void SubsTextEditCtrl::SetTextTo(std::string const& text) {
	osx::ime::invalidate(this);
	SetEvtHandlerEnabled(false);
	Freeze();

	auto insertion_point = GetInsertionPoint();
	if (static_cast<size_t>(insertion_point) > line_text.size())
		line_text = GetTextRaw().data();
	auto old_pos = agi::CharacterCount(line_text.begin(), line_text.begin() + insertion_point, 0);
	line_text.clear();

	if (context) {
		context->textSelectionController->SetSelection(0, 0);
		SetTextRaw(text.c_str());
		auto pos = agi::IndexOfCharacter(text, old_pos);
		context->textSelectionController->SetSelection(pos, pos);
	}
	else {
		SetSelection(0, 0);
		SetTextRaw(text.c_str());
		auto pos = agi::IndexOfCharacter(text, old_pos);
		SetSelection(pos, pos);
	}

	SetEvtHandlerEnabled(true);
	Thaw();
}

void SubsTextEditCtrl::Paste() {
	std::string data = GetClipboard();

	boost::replace_all(data, "\r\n", "\\N");
	boost::replace_all(data, "\n", "\\N");
	boost::replace_all(data, "\r", "\\N");

	wxCharBuffer old = GetTextRaw();
	data.insert(0, old.data(), GetSelectionStart());
	int sel_start = data.size();
	data.append(old.data() + GetSelectionEnd());

	SetTextRaw(data.c_str());

	SetSelectionStart(sel_start);
	SetSelectionEnd(sel_start);
}

void SubsTextEditCtrl::OnContextMenu(wxContextMenuEvent &event) {
	wxPoint pos = event.GetPosition();
	int activePos;
	if (pos == wxDefaultPosition)
		activePos = GetCurrentPos();
	else
		activePos = PositionFromPoint(ScreenToClient(pos));

	currentWordPos = GetBoundsOfWordAtPosition(activePos);
	currentWord = line_text.substr(currentWordPos.first, currentWordPos.second);

	wxMenu menu;
	if (spellchecker) {
		AddSpellCheckerEntries(menu);

		// Append language list
		menu.Append(-1, _("Spell checker language"), GetLanguagesMenu(
			EDIT_MENU_DIC_LANGS,
			to_wx(OPT_GET("Tool/Spell Checker/Language")->GetString()),
			to_wx(spellchecker->GetLanguageList())));
		menu.AppendSeparator();
	}

	AddThesaurusEntries(menu);

	// Standard actions
	menu.Append(EDIT_MENU_CUT,_("Cu&t"))->Enable(GetSelectionStart()-GetSelectionEnd() != 0);
	menu.Append(EDIT_MENU_COPY,_("&Copy"))->Enable(GetSelectionStart()-GetSelectionEnd() != 0);
	menu.Append(EDIT_MENU_PASTE,_("&Paste"))->Enable(CanPaste());
	menu.AppendSeparator();
	menu.Append(EDIT_MENU_SELECT_ALL,_("Select &All"));

	// Split
	if (context) {
		menu.AppendSeparator();
		menu.Append(EDIT_MENU_SPLIT_PRESERVE, _("Split at cursor (preserve times)"));
		menu.Append(EDIT_MENU_SPLIT_ESTIMATE, _("Split at cursor (estimate times)"));
		cmd::Command *split_video = cmd::get("edit/line/split/video");
		menu.Append(EDIT_MENU_SPLIT_VIDEO, split_video->StrMenu(context))->Enable(split_video->Validate(context));
	}

	PopupMenu(&menu);
}

void SubsTextEditCtrl::OnDoubleClick(wxStyledTextEvent &evt) {
	int pos = evt.GetPosition();
	if (pos == -1 && !tokenized_line.empty()) {
		auto tok = tokenized_line.back();
		SetSelection(line_text.size() - tok.length, line_text.size());
	}
	else {
		auto bounds = GetBoundsOfWordAtPosition(evt.GetPosition());
		if (bounds.second != 0)
			SetSelection(bounds.first, bounds.first + bounds.second);
		else
			evt.Skip();
	}
}

void SubsTextEditCtrl::AddSpellCheckerEntries(wxMenu &menu) {
	if (currentWord.empty()) return;

	if (spellchecker->CanRemoveWord(currentWord))
		menu.Append(EDIT_MENU_REMOVE_FROM_DICT, fmt_tl("Remove \"%s\" from dictionary", currentWord));

	sugs = spellchecker->GetSuggestions(currentWord);
	if (spellchecker->CheckWord(currentWord)) {
		if (sugs.empty())
			menu.Append(EDIT_MENU_SUGGESTION,_("No spell checker suggestions"))->Enable(false);
		else {
			auto subMenu = new wxMenu;
			for (size_t i = 0; i < sugs.size(); ++i)
				subMenu->Append(EDIT_MENU_SUGGESTIONS+i, to_wx(sugs[i]));

			menu.Append(-1, fmt_tl("Spell checker suggestions for \"%s\"", currentWord), subMenu);
		}
	}
	else {
		if (sugs.empty())
			menu.Append(EDIT_MENU_SUGGESTION,_("No correction suggestions"))->Enable(false);

		for (size_t i = 0; i < sugs.size(); ++i)
			menu.Append(EDIT_MENU_SUGGESTIONS+i, to_wx(sugs[i]));

		// Append "add word"
		menu.Append(EDIT_MENU_ADD_TO_DICT, fmt_tl("Add \"%s\" to dictionary", currentWord))->Enable(spellchecker->CanAddWord(currentWord));
	}
}

void SubsTextEditCtrl::AddThesaurusEntries(wxMenu &menu) {
	if (currentWord.empty()) return;

	auto results = thesaurus->Lookup(currentWord);

	thesSugs.clear();

	if (results.size()) {
		auto thesMenu = new wxMenu;

		int curThesEntry = 0;
		for (auto const& result : results) {
			// Single word, insert directly
			if (result.second.empty()) {
				thesMenu->Append(EDIT_MENU_THESAURUS_SUGS+curThesEntry, to_wx(result.first));
				thesSugs.push_back(result.first);
				++curThesEntry;
			}
			// Multiple, create submenu
			else {
				auto subMenu = new wxMenu;
				for (auto const& sug : result.second) {
					subMenu->Append(EDIT_MENU_THESAURUS_SUGS+curThesEntry, to_wx(sug));
					thesSugs.push_back(sug);
					++curThesEntry;
				}

				thesMenu->Append(-1, to_wx(result.first), subMenu);
			}
		}

		menu.Append(-1, fmt_tl("Thesaurus suggestions for \"%s\"", currentWord), thesMenu);
	}
	else
		menu.Append(EDIT_MENU_THESAURUS,_("No thesaurus suggestions"))->Enable(false);

	// Append language list
	menu.Append(-1,_("Thesaurus language"), GetLanguagesMenu(
		EDIT_MENU_THES_LANGS,
		to_wx(OPT_GET("Tool/Thesaurus/Language")->GetString()),
		to_wx(thesaurus->GetLanguageList())));
	menu.AppendSeparator();
}

wxMenu *SubsTextEditCtrl::GetLanguagesMenu(int base_id, wxString const& curLang, wxArrayString const& langs) {
	auto languageMenu = new wxMenu;
	languageMenu->AppendRadioItem(base_id, _("Disable"))->Check(curLang.empty());

	for (size_t i = 0; i < langs.size(); ++i)
		languageMenu->AppendRadioItem(base_id + i + 1, LocalizedLanguageName(langs[i]))->Check(langs[i] == curLang);

	return languageMenu;
}

void SubsTextEditCtrl::OnUseSuggestion(wxCommandEvent &event) {
	std::string suggestion;
	int sugIdx = event.GetId() - EDIT_MENU_THESAURUS_SUGS;
	if (sugIdx >= 0)
		suggestion = thesSugs[sugIdx];
	else
		suggestion = sugs[event.GetId() - EDIT_MENU_SUGGESTIONS];

	size_t pos;
	while ((pos = suggestion.rfind('(')) != std::string::npos) {
		// If there's only one suggestion for a word it'll be in the form "(noun) word",
		// so we need to trim the "(noun) " part
		if (pos == 0) {
			pos = suggestion.find(')');
			if (pos != std::string::npos) {
				if (pos + 1< suggestion.size() && suggestion[pos + 1] == ' ') ++pos;
				suggestion.erase(0, pos + 1);
			}
			break;
		}

		// Some replacements have notes about their usage after the word in the
		// form "word (generic term)" that we need to remove (plus the leading space)
		suggestion.resize(pos - 1);
	}

	// line_text needs to get cleared before SetTextRaw to ensure it gets reparsed
	std::string new_text;
	swap(line_text, new_text);
	SetTextRaw(new_text.replace(currentWordPos.first, currentWordPos.second, suggestion).c_str());

	SetSelection(currentWordPos.first, currentWordPos.first + suggestion.size());
	SetFocus();
}

void SubsTextEditCtrl::OnSetDicLanguage(wxCommandEvent &event) {
	std::vector<std::string> langs = spellchecker->GetLanguageList();

	int index = event.GetId() - EDIT_MENU_DIC_LANGS - 1;
	std::string lang;
	if (index >= 0)
		lang = langs[index];

	OPT_SET("Tool/Spell Checker/Language")->SetString(lang);

	UpdateStyle();
}

void SubsTextEditCtrl::OnSetThesLanguage(wxCommandEvent &event) {
	if (!thesaurus) return;

	std::vector<std::string> langs = thesaurus->GetLanguageList();

	int index = event.GetId() - EDIT_MENU_THES_LANGS - 1;
	std::string lang;
	if (index >= 0) lang = langs[index];
	OPT_SET("Tool/Thesaurus/Language")->SetString(lang);

	UpdateStyle();
}

std::pair<int, int> SubsTextEditCtrl::GetBoundsOfWordAtPosition(int pos) {
	int len = 0;
	for (auto const& tok : tokenized_line) {
		if (len + (int)tok.length > pos) {
			if (tok.type == agi::ass::DialogueTokenType::WORD)
				return {len, tok.length};
			return {0, 0};
		}
		len += tok.length;
	}

	return {0, 0};
}
