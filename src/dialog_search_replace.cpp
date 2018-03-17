// Copyright (c) 2013, Thomas Goyne <plorkyeran@aegisub.org>
//
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//
// Aegisub Project http://www.aegisub.org/

/// @file dialog_search_replace.cpp
/// @brief Find and Search/replace dialogue box and logic
/// @ingroup secondary_ui
///

#include "dialog_search_replace.h"

#include "compat.h"
#include "include/aegisub/context.h"
#include "options.h"
#include "search_replace_engine.h"
#include "utils.h"
#include "validators.h"

#include <libaegisub/make_unique.h>

#include <functional>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/radiobox.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/valgen.h>

DialogSearchReplace::DialogSearchReplace(agi::Context* c, bool replace)
: wxDialog(c->parent, -1, replace ? _("Replace") : _("Find"))
, c(c)
, settings(agi::make_unique<SearchReplaceSettings>())
, has_replace(replace)
{
	auto recent_find(lagi_MRU_wxAS("Find"));
	auto recent_replace(lagi_MRU_wxAS("Replace"));

	settings->field = static_cast<SearchReplaceSettings::Field>(OPT_GET("Tool/Search Replace/Field")->GetInt());
	settings->limit_to = static_cast<SearchReplaceSettings::Limit>(OPT_GET("Tool/Search Replace/Affect")->GetInt());
	settings->find = recent_find.empty() ? std::string() : from_wx(recent_find.front());
	settings->replace_with = recent_replace.empty() ? std::string() : from_wx(recent_replace.front());
	settings->match_case = OPT_GET("Tool/Search Replace/Match Case")->GetBool();
	settings->use_regex = OPT_GET("Tool/Search Replace/RegExp")->GetBool();
	settings->ignore_comments = OPT_GET("Tool/Search Replace/Skip Comments")->GetBool();
	settings->skip_tags = OPT_GET("Tool/Search Replace/Skip Tags")->GetBool();
	settings->exact_match = false;

	auto find_sizer = new wxFlexGridSizer(2, 2, 5, 15);
	find_edit = new wxComboBox(this, -1, "", wxDefaultPosition, wxSize(300, -1), recent_find, wxCB_DROPDOWN | wxTE_PROCESS_ENTER, StringBinder(&settings->find));
	find_edit->SetMaxLength(0);
	find_sizer->Add(new wxStaticText(this, -1, _("Find what:")), wxSizerFlags().Center().Left());
	find_sizer->Add(find_edit);

	if (has_replace) {
		replace_edit = new wxComboBox(this, -1, "", wxDefaultPosition, wxSize(300, -1), lagi_MRU_wxAS("Replace"), wxCB_DROPDOWN | wxTE_PROCESS_ENTER, StringBinder(&settings->replace_with));
		replace_edit->SetMaxLength(0);
		find_sizer->Add(new wxStaticText(this, -1, _("Replace with:")), wxSizerFlags().Center().Left());
		find_sizer->Add(replace_edit);
	}

	auto options_sizer = new wxBoxSizer(wxVERTICAL);
	options_sizer->Add(new wxCheckBox(this, -1, _("&Match case"), wxDefaultPosition, wxDefaultSize, 0, wxGenericValidator(&settings->match_case)), wxSizerFlags().Border(wxBOTTOM));
	options_sizer->Add(new wxCheckBox(this, -1, _("&Use regular expressions"), wxDefaultPosition, wxDefaultSize, 0, wxGenericValidator(&settings->use_regex)), wxSizerFlags().Border(wxBOTTOM));
	options_sizer->Add(new wxCheckBox(this, -1, _("&Skip Comments"), wxDefaultPosition, wxDefaultSize, 0, wxGenericValidator(&settings->ignore_comments)), wxSizerFlags().Border(wxBOTTOM));
	options_sizer->Add(new wxCheckBox(this, -1, _("S&kip Override Tags"), wxDefaultPosition, wxDefaultSize, 0, wxGenericValidator(&settings->skip_tags)));

	auto left_sizer = new wxBoxSizer(wxVERTICAL);
	left_sizer->Add(find_sizer, wxSizerFlags().DoubleBorder(wxBOTTOM));
	left_sizer->Add(options_sizer);

	wxString field[] = { _("&Text"), _("St&yle"), _("A&ctor"), _("&Effect") };
	wxString affect[] = { _("A&ll rows"), _("Selected &rows") };
	auto limit_sizer = new wxBoxSizer(wxHORIZONTAL);
	limit_sizer->Add(new wxRadioBox(this, -1, _("In Field"), wxDefaultPosition, wxDefaultSize, countof(field), field, 0, wxRA_SPECIFY_COLS, MakeEnumBinder(&settings->field)), wxSizerFlags().Border(wxRIGHT));
	limit_sizer->Add(new wxRadioBox(this, -1, _("Limit to"), wxDefaultPosition, wxDefaultSize, countof(affect), affect, 0, wxRA_SPECIFY_COLS, MakeEnumBinder(&settings->limit_to)));

	auto find_next = new wxButton(this, -1, _("&Find next"));
	auto replace_next = new wxButton(this, -1, _("Replace &next"));
	auto replace_all = new wxButton(this, -1, _("Replace &all"));
	find_next->SetDefault();

	auto button_sizer = new wxBoxSizer(wxVERTICAL);
	button_sizer->Add(find_next, wxSizerFlags().Border(wxBOTTOM));
	button_sizer->Add(replace_next, wxSizerFlags().Border(wxBOTTOM));
	button_sizer->Add(replace_all, wxSizerFlags().Border(wxBOTTOM));
	button_sizer->Add(new wxButton(this, wxID_CANCEL));

	if (!has_replace) {
		button_sizer->Hide(replace_next);
		button_sizer->Hide(replace_all);
	}

	auto top_sizer = new wxBoxSizer(wxHORIZONTAL);
	top_sizer->Add(left_sizer, wxSizerFlags().Border());
	top_sizer->Add(button_sizer, wxSizerFlags().Border());

	auto main_sizer = new wxBoxSizer(wxVERTICAL);
	main_sizer->Add(top_sizer);
	main_sizer->Add(limit_sizer, wxSizerFlags().Border());
	SetSizerAndFit(main_sizer);
	CenterOnParent();

	TransferDataToWindow();
	find_edit->SetFocus();
	find_edit->SelectAll();

	find_edit->Bind(wxEVT_TEXT_ENTER, std::bind(&DialogSearchReplace::FindReplace, this, &SearchReplaceEngine::FindNext));
	if (has_replace)
	  replace_edit->Bind(wxEVT_TEXT_ENTER, std::bind(&DialogSearchReplace::FindReplace, this, &SearchReplaceEngine::ReplaceNext));
	find_next->Bind(wxEVT_BUTTON, std::bind(&DialogSearchReplace::FindReplace, this, &SearchReplaceEngine::FindNext));
	replace_next->Bind(wxEVT_BUTTON, std::bind(&DialogSearchReplace::FindReplace, this, &SearchReplaceEngine::ReplaceNext));
	replace_all->Bind(wxEVT_BUTTON, std::bind(&DialogSearchReplace::FindReplace, this, &SearchReplaceEngine::ReplaceAll));
}

DialogSearchReplace::~DialogSearchReplace() {
}

void DialogSearchReplace::FindReplace(bool (SearchReplaceEngine::*func)()) {
	TransferDataFromWindow();

	if (settings->find.empty())
		return;

	c->search->Configure(*settings);
	try {
		((*c->search).*func)();
	}
	catch (std::exception const& e) {
		wxMessageBox(to_wx(e.what()), "Error", wxOK | wxICON_ERROR | wxCENTER, this);
		return;
	}

	config::mru->Add("Find", settings->find);
	if (has_replace)
		config::mru->Add("Replace", settings->replace_with);

	OPT_SET("Tool/Search Replace/Match Case")->SetBool(settings->match_case);
	OPT_SET("Tool/Search Replace/RegExp")->SetBool(settings->use_regex);
	OPT_SET("Tool/Search Replace/Skip Comments")->SetBool(settings->ignore_comments);
	OPT_SET("Tool/Search Replace/Skip Tags")->SetBool(settings->skip_tags);
	OPT_SET("Tool/Search Replace/Field")->SetInt(static_cast<int>(settings->field));
	OPT_SET("Tool/Search Replace/Affect")->SetInt(static_cast<int>(settings->limit_to));

	UpdateDropDowns();
}

static void update_mru(wxComboBox *cb, const char *mru_name) {
	cb->Freeze();
	cb->Clear();
	cb->Append(lagi_MRU_wxAS(mru_name));
	if (!cb->IsListEmpty())
		cb->SetSelection(0);
	cb->Thaw();
}

void DialogSearchReplace::UpdateDropDowns() {
	update_mru(find_edit, "Find");

	if (has_replace)
		update_mru(replace_edit, "Replace");
}

void DialogSearchReplace::Show(agi::Context *context, bool replace) {
	static DialogSearchReplace *diag = nullptr;

	if (diag && (replace != diag->has_replace || diag->c != context)) {
		// Already opened, but wrong type - destroy and create the right one
		diag->Destroy();
		diag = nullptr;
	}

	if (!diag)
		diag = new DialogSearchReplace(context, replace);

	diag->find_edit->SetFocus();
	diag->find_edit->SelectAll();
	diag->wxDialog::Show();
	diag->Raise();
}
