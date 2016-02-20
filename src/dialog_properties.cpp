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

#include "ass_file.h"
#include "async_video_provider.h"
#include "compat.h"
#include "help_button.h"
#include "include/aegisub/context.h"
#include "libresrc/libresrc.h"
#include "project.h"
#include "resolution_resampler.h"
#include "validators.h"

#include <boost/algorithm/string/predicate.hpp>
#include <vector>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace {
class DialogProperties {
	wxDialog d;
	agi::Context *c; ///< Project this dialog is adjusting the properties of

	/// Pairs of a script property and a text control for that property
	std::vector<std::pair<std::string, wxTextCtrl*>> properties;

	// Things that effect rendering
	wxComboBox *WrapStyle;   ///< Wrapping style for long lines
	wxTextCtrl *ResX;        ///< Script x resolution
	wxTextCtrl *ResY;        ///< Script y resolution
	wxCheckBox *ScaleBorder; ///< If script resolution != video resolution how should borders be handled
	wxComboBox *YCbCrMatrix;

	/// OK button handler
	void OnOK(wxCommandEvent &event);
	/// Set script resolution to video resolution button
	void OnSetFromVideo(wxCommandEvent &event);
	/// Set a script info field
	/// @param key Name of field
	/// @param value New value
	/// @return Did the value actually need to be changed?
	int SetInfoIfDifferent(std::string const& key, std::string const& value);

	/// Add a property with label and text box for updating the property
	/// @param sizer Sizer to add the label and control to
	/// @param label Label text to use
	/// @param property Script info property name
	void AddProperty(wxSizer *sizer, wxString const& label, std::string const& property);

public:
	/// Constructor
	/// @param c Project context
	DialogProperties(agi::Context *c);
	void ShowModal() { d.ShowModal(); }
};

DialogProperties::DialogProperties(agi::Context *c)
: d(c->parent, -1, _("Script Properties"))
, c(c)
{
	d.SetIcon(GETICON(properties_toolbutton_16));

	// Script details crap
	wxSizer *TopSizer = new wxStaticBoxSizer(wxHORIZONTAL,&d,_("Script"));
	auto TopSizerGrid = new wxFlexGridSizer(0,2,5,5);

	AddProperty(TopSizerGrid, _("Title:"), "Title");
	AddProperty(TopSizerGrid, _("Original script:"), "Original Script");
	AddProperty(TopSizerGrid, _("Translation:"), "Original Translation");
	AddProperty(TopSizerGrid, _("Editing:"), "Original Editing");
	AddProperty(TopSizerGrid, _("Timing:"), "Original Timing");
	AddProperty(TopSizerGrid, _("Synch point:"), "Synch Point");
	AddProperty(TopSizerGrid, _("Updated by:"), "Script Updated By");
	AddProperty(TopSizerGrid, _("Update details:"), "Update Details");

	TopSizerGrid->AddGrowableCol(1,1);
	TopSizer->Add(TopSizerGrid,1,wxALL | wxEXPAND,0);

	// Resolution box
	ResX = new wxTextCtrl(&d,-1,"",wxDefaultPosition,wxSize(50,-1),0,IntValidator(c->ass->GetScriptInfoAsInt("PlayResX")));
	ResY = new wxTextCtrl(&d,-1,"",wxDefaultPosition,wxSize(50,-1),0,IntValidator(c->ass->GetScriptInfoAsInt("PlayResY")));

	wxButton *FromVideo = new wxButton(&d,-1,_("From &video"));
	if (!c->project->VideoProvider())
		FromVideo->Enable(false);
	else
		FromVideo->Bind(wxEVT_BUTTON, &DialogProperties::OnSetFromVideo, this);

	auto res_sizer = new wxBoxSizer(wxHORIZONTAL);
	res_sizer->Add(ResX, 1, wxRIGHT | wxALIGN_CENTER_VERTICAL, 5);
	res_sizer->Add(new wxStaticText(&d, -1, "x"), 0, wxALIGN_CENTER | wxRIGHT, 5);
	res_sizer->Add(ResY, 1, wxRIGHT | wxALIGN_CENTER_VERTICAL, 5);
	res_sizer->Add(FromVideo, 1, 0, 0);

	YCbCrMatrix = new wxComboBox(&d, -1, to_wx(c->ass->GetScriptInfo("YCbCr Matrix")),
		 wxDefaultPosition, wxDefaultSize, to_wx(MatrixNames()), wxCB_READONLY);

	auto matrix_sizer = new wxBoxSizer(wxHORIZONTAL);
	matrix_sizer->Add(new wxStaticText(&d, -1, "YCbCr Matrix:"), wxSizerFlags().Center());
	matrix_sizer->Add(YCbCrMatrix, wxSizerFlags(1).Expand().Border(wxLEFT));

	auto res_box = new wxStaticBoxSizer(wxVERTICAL, &d, _("Resolution"));
	res_box->Add(res_sizer, wxSizerFlags().Expand());
	res_box->Add(matrix_sizer, wxSizerFlags().Border(wxTOP).Expand());

	// Options
	wxSizer *optionsBox = new wxStaticBoxSizer(wxHORIZONTAL,&d,_("Options"));
	auto optionsGrid = new wxFlexGridSizer(3,2,5,5);
	wxString wrap_opts[] = {
		_("0: Smart wrapping, top line is wider"),
		_("1: End-of-line word wrapping, only \\N breaks"),
		_("2: No word wrapping, both \\n and \\N break"),
		_("3: Smart wrapping, bottom line is wider")
	};
	WrapStyle = new wxComboBox(&d, -1, "", wxDefaultPosition, wxDefaultSize, 4, wrap_opts, wxCB_READONLY);
	WrapStyle->SetSelection(c->ass->GetScriptInfoAsInt("WrapStyle"));
	optionsGrid->Add(new wxStaticText(&d,-1,_("Wrap Style: ")),0,wxALIGN_CENTER_VERTICAL,0);
	optionsGrid->Add(WrapStyle,1,wxEXPAND,0);

	ScaleBorder = new wxCheckBox(&d,-1,_("Scale Border and Shadow"));
	ScaleBorder->SetToolTip(_("Scale border and shadow together with script/render resolution. If this is unchecked, relative border and shadow size will depend on renderer."));
	ScaleBorder->SetValue(boost::iequals(c->ass->GetScriptInfo("ScaledBorderAndShadow"), "yes"));
	optionsGrid->AddSpacer(0);
	optionsGrid->Add(ScaleBorder,1,wxEXPAND,0);
	optionsGrid->AddGrowableCol(1,1);
	optionsBox->Add(optionsGrid,1,wxEXPAND,0);

	// Button sizer
	auto ButtonSizer = d.CreateStdDialogButtonSizer(wxOK | wxCANCEL | wxHELP);
	d.Bind(wxEVT_BUTTON, &DialogProperties::OnOK, this, wxID_OK);
	d.Bind(wxEVT_BUTTON, std::bind(&HelpButton::OpenPage, "Properties"), wxID_HELP);

	// MainSizer
	wxSizer *MainSizer = new wxBoxSizer(wxVERTICAL);
	MainSizer->Add(TopSizer,0,wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND,5);
	MainSizer->Add(res_box,0,wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND,5);
	MainSizer->Add(optionsBox,0,wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND,5);
	MainSizer->Add(ButtonSizer,0,wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND,5);

	d.SetSizerAndFit(MainSizer);
	d.CenterOnParent();
}

void DialogProperties::AddProperty(wxSizer *sizer, wxString const& label, std::string const& property) {
	wxTextCtrl *ctrl = new wxTextCtrl(&d, -1, to_wx(c->ass->GetScriptInfo(property)), wxDefaultPosition, wxSize(200, -1));
	sizer->Add(new wxStaticText(&d, -1, label), wxSizerFlags().Center().Left());
	sizer->Add(ctrl, wxSizerFlags(1).Expand());
	properties.push_back({property, ctrl});
}

void DialogProperties::OnOK(wxCommandEvent &) {
	int count = 0;
	for (auto const& prop : properties)
		count += SetInfoIfDifferent(prop.first, from_wx(prop.second->GetValue()));

	count += SetInfoIfDifferent("PlayResX", from_wx(ResX->GetValue()));
	count += SetInfoIfDifferent("PlayResY", from_wx(ResY->GetValue()));
	count += SetInfoIfDifferent("WrapStyle", std::to_string(WrapStyle->GetSelection()));
	count += SetInfoIfDifferent("ScaledBorderAndShadow", ScaleBorder->GetValue() ? "yes" : "no");
	count += SetInfoIfDifferent("YCbCr Matrix", from_wx(YCbCrMatrix->GetValue()));

	if (count) c->ass->Commit(_("property changes"), AssFile::COMMIT_SCRIPTINFO);

	d.EndModal(!!count);
}

int DialogProperties::SetInfoIfDifferent(std::string const& key, std::string const&value) {
	if (c->ass->GetScriptInfo(key) != value) {
		c->ass->SetScriptInfo(key, value);
		return 1;
	}
	return 0;
}

void DialogProperties::OnSetFromVideo(wxCommandEvent &) {
	ResX->SetValue(std::to_wstring(c->project->VideoProvider()->GetWidth()));
	ResY->SetValue(std::to_wstring(c->project->VideoProvider()->GetHeight()));
}
}

void ShowPropertiesDialog(agi::Context *c) {
	DialogProperties(c).ShowModal();
}
