// Copyright (c) 2012, Thomas Goyne <plorkyeran@aegisub.org>
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

#include "ass_file.h"
#include "async_video_provider.h"
#include "compat.h"
#include "help_button.h"
#include "include/aegisub/context.h"
#include "libresrc/libresrc.h"
#include "project.h"
#include "resolution_resampler.h"
#include "validators.h"

#include <boost/range/size.hpp>
#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/valgen.h>

namespace {
/// @class DialogResample
/// @brief Configuration dialog for resolution resampling
///
/// Populate a ResampleSettings structure with data from the user
struct DialogResample {
	wxDialog d;
	agi::Context *c; ///< Project context

	int script_w;
	int script_h;
	YCbCrMatrix script_mat;
	int video_w = 0;
	int video_h = 0;
	YCbCrMatrix video_mat;

	wxSpinCtrl *source_x;
	wxSpinCtrl *source_y;
	wxSpinCtrl *dest_x;
	wxSpinCtrl *dest_y;
	wxComboBox *source_matrix;
	wxComboBox *dest_matrix;
	wxCheckBox *symmetrical;
	wxRadioBox *ar_mode;
	wxSpinCtrl *margin_ctrl[4];

	wxButton *from_script;
	wxButton *from_video;

	void SetSourceFromScript(wxCommandEvent &);
	/// Set the destination resolution to the video's resolution
	void SetDestFromVideo(wxCommandEvent &);
	/// Symmetrical checkbox toggle handler
	void OnSymmetrical(wxCommandEvent &);
	/// Copy margin values over if symmetrical is enabled
	void OnMarginChange(wxSpinCtrl *src, wxSpinCtrl *dst);
	void UpdateButtons();

public:
	/// Constructor
	/// @param context Project context
	/// @param[out] settings Settings struct to populate
	DialogResample(agi::Context *context, ResampleSettings &settings);
};

enum {
	LEFT = 0,
	RIGHT = 1,
	TOP = 2,
	BOTTOM = 3
};

DialogResample::DialogResample(agi::Context *c, ResampleSettings &settings)
: d(c->parent, -1, _("Resample Resolution"))
, c(c)
{
	d.SetIcon(GETICON(resample_toolbutton_16));

	memset(&settings, 0, sizeof(settings));
	c->ass->GetResolution(script_w, script_h);
	settings.source_x = script_w;
	settings.source_y = script_h;
	settings.source_matrix = script_mat = MatrixFromString(c->ass->GetScriptInfo("YCbCr Matrix"));

	if (auto provider = c->project->VideoProvider()) {
		settings.dest_x = video_w = provider->GetWidth();
		settings.dest_y = video_h = provider->GetHeight();
		settings.dest_matrix = video_mat = MatrixFromString(provider->GetRealColorSpace());
	}
	else {
		settings.dest_x = script_w;
		settings.dest_y = script_h;
		settings.dest_matrix = script_mat;
		video_mat = YCbCrMatrix::rgb;
	}

	// Create all controls and set validators
	for (size_t i = 0; i < 4; ++i) {
		margin_ctrl[i] = new wxSpinCtrl(&d, -1, "0", wxDefaultPosition, wxSize(100, -1), wxSP_ARROW_KEYS, -9999, 9999, 0);
		margin_ctrl[i]->SetValidator(wxGenericValidator(&settings.margin[i]));
	}

	symmetrical = new wxCheckBox(&d, -1, _("&Symmetrical"));
	symmetrical->SetValue(true);

	margin_ctrl[RIGHT]->Enable(false);
	margin_ctrl[BOTTOM]->Enable(false);

	source_x = new wxSpinCtrl(&d, -1, "", wxDefaultPosition, wxSize(100, -1), wxSP_ARROW_KEYS, 1, INT_MAX);
	source_y = new wxSpinCtrl(&d, -1, "", wxDefaultPosition, wxSize(100, -1), wxSP_ARROW_KEYS, 1, INT_MAX);
	source_matrix = new wxComboBox(&d, -1, "", wxDefaultPosition,
		wxDefaultSize, to_wx(MatrixNames()), wxCB_READONLY);
	dest_x = new wxSpinCtrl(&d, -1, "", wxDefaultPosition, wxSize(100, -1), wxSP_ARROW_KEYS, 1, INT_MAX);
	dest_y = new wxSpinCtrl(&d, -1, "", wxDefaultPosition, wxSize(100, -1), wxSP_ARROW_KEYS, 1, INT_MAX);
	dest_matrix = new wxComboBox(&d, -1, "", wxDefaultPosition, wxDefaultSize,
		to_wx(MatrixNames()), wxCB_READONLY);

	source_x->SetValidator(wxGenericValidator(&settings.source_x));
	source_y->SetValidator(wxGenericValidator(&settings.source_y));
	source_matrix->SetValidator(MakeEnumBinder(&settings.source_matrix));
	dest_x->SetValidator(wxGenericValidator(&settings.dest_x));
	dest_y->SetValidator(wxGenericValidator(&settings.dest_y));
	dest_matrix->SetValidator(MakeEnumBinder(&settings.dest_matrix));

	from_video = new wxButton(&d, -1, _("From &video"));
	from_video->Enable(false);
	from_script = new wxButton(&d, -1, _("From s&cript"));
	from_script->Enable(false);

	wxString ar_modes[] = {_("Stretch"), _("Add borders"), _("Remove borders"), _("Manual")};
	ar_mode = new wxRadioBox(&d, -1, _("Aspect Ratio Handling"), wxDefaultPosition,
		wxDefaultSize, boost::size(ar_modes), ar_modes, 1, 4, MakeEnumBinder(&settings.ar_mode));

	// Position the controls
	auto margin_sizer = new wxGridSizer(3, 3, 5, 5);
	margin_sizer->AddSpacer(1);
	margin_sizer->Add(margin_ctrl[TOP], wxSizerFlags(1).Expand());
	margin_sizer->AddSpacer(1);
	margin_sizer->Add(margin_ctrl[LEFT], wxSizerFlags(1).Expand());
	margin_sizer->Add(symmetrical, wxSizerFlags(1).Expand());
	margin_sizer->Add(margin_ctrl[RIGHT], wxSizerFlags(1).Expand());
	margin_sizer->AddSpacer(1);
	margin_sizer->Add(margin_ctrl[BOTTOM], wxSizerFlags(1).Expand());
	margin_sizer->AddSpacer(1);

	auto margin_box = new wxStaticBoxSizer(wxVERTICAL, &d, _("Margin offset"));
	margin_box->Add(margin_sizer, wxSizerFlags(1).Expand().Border(wxBOTTOM));

	auto source_res_sizer = new wxBoxSizer(wxHORIZONTAL);
	source_res_sizer->Add(source_x, wxSizerFlags(1).Border(wxRIGHT).Align(wxALIGN_CENTER_VERTICAL));
	source_res_sizer->Add(new wxStaticText(&d, -1, _("x")), wxSizerFlags().Center().Border(wxRIGHT));
	source_res_sizer->Add(source_y, wxSizerFlags(1).Border(wxRIGHT).Align(wxALIGN_CENTER_VERTICAL));
	source_res_sizer->Add(from_script, wxSizerFlags(1));

	auto source_matrix_sizer = new wxBoxSizer(wxHORIZONTAL);
	source_matrix_sizer->Add(new wxStaticText(&d, -1, _("YCbCr Matrix:")), wxSizerFlags().Border(wxRIGHT).Center());
	source_matrix_sizer->Add(source_matrix, wxSizerFlags(1).Center().Right());

	auto source_res_box = new wxStaticBoxSizer(wxVERTICAL, &d, _("Source Resolution"));
	source_res_box->Add(source_res_sizer, wxSizerFlags(1).Expand().Border(wxBOTTOM));
	source_res_box->Add(source_matrix_sizer, wxSizerFlags(1).Expand());

	auto dest_res_sizer = new wxBoxSizer(wxHORIZONTAL);
	dest_res_sizer->Add(dest_x, wxSizerFlags(1).Border(wxRIGHT).Align(wxALIGN_CENTER_VERTICAL));
	dest_res_sizer->Add(new wxStaticText(&d, -1, _("x")), wxSizerFlags().Center().Border(wxRIGHT));
	dest_res_sizer->Add(dest_y, wxSizerFlags(1).Border(wxRIGHT).Align(wxALIGN_CENTER_VERTICAL));
	dest_res_sizer->Add(from_video, wxSizerFlags(1));

	auto dest_matrix_sizer = new wxBoxSizer(wxHORIZONTAL);
	dest_matrix_sizer->Add(new wxStaticText(&d, -1, _("YCbCr Matrix:")), wxSizerFlags().Border(wxRIGHT).Center());
	dest_matrix_sizer->Add(dest_matrix, wxSizerFlags(1).Center().Right());

	auto dest_res_box = new wxStaticBoxSizer(wxVERTICAL, &d, _("Destination Resolution"));
	dest_res_box->Add(dest_res_sizer, wxSizerFlags(1).Expand().Border(wxBOTTOM));
	dest_res_box->Add(dest_matrix_sizer, wxSizerFlags(1).Expand());

	auto main_sizer = new wxBoxSizer(wxVERTICAL);
	main_sizer->Add(source_res_box, wxSizerFlags().Expand().Border());
	main_sizer->Add(dest_res_box, wxSizerFlags().Expand().Border());
	main_sizer->Add(ar_mode, wxSizerFlags().Expand().Border());
	main_sizer->Add(margin_box, wxSizerFlags(1).Expand().Border());
	main_sizer->Add(d.CreateStdDialogButtonSizer(wxOK | wxCANCEL | wxHELP), wxSizerFlags().Expand().Border(wxALL & ~wxTOP));
	d.SetSizerAndFit(main_sizer);
	d.CenterOnParent();

	d.TransferDataToWindow();
	UpdateButtons();

	// Bind events
	using std::bind;
	d.Bind(wxEVT_BUTTON, bind(&HelpButton::OpenPage, "Resample resolution"), wxID_HELP);
	d.Bind(wxEVT_SPINCTRL, [=](wxCommandEvent&) { UpdateButtons(); });
	d.Bind(wxEVT_RADIOBOX, [=](wxCommandEvent&) { UpdateButtons(); });
	from_video->Bind(wxEVT_BUTTON, &DialogResample::SetDestFromVideo, this);
	from_script->Bind(wxEVT_BUTTON, &DialogResample::SetSourceFromScript, this);
	symmetrical->Bind(wxEVT_CHECKBOX, &DialogResample::OnSymmetrical, this);
	margin_ctrl[LEFT]->Bind(wxEVT_SPINCTRL, bind(&DialogResample::OnMarginChange, this, margin_ctrl[LEFT], margin_ctrl[RIGHT]));
	margin_ctrl[TOP]->Bind(wxEVT_SPINCTRL, bind(&DialogResample::OnMarginChange, this, margin_ctrl[TOP], margin_ctrl[BOTTOM]));
}

void DialogResample::SetDestFromVideo(wxCommandEvent &) {
	dest_x->SetValue(video_w);
	dest_y->SetValue(video_h);
	dest_matrix->SetSelection((int)video_mat);
}

void DialogResample::SetSourceFromScript(wxCommandEvent&) {
	source_x->SetValue(script_w);
	source_y->SetValue(script_h);
	source_matrix->SetSelection((int)script_mat);
}

void DialogResample::UpdateButtons() {
	from_video->Enable(c->project->VideoProvider() &&
		(dest_x->GetValue() != video_w || dest_y->GetValue() != video_h));
	from_script->Enable(source_x->GetValue() != script_w || source_y->GetValue() != script_h);

	auto source_ar = double(source_x->GetValue()) / source_y->GetValue();
	auto dest_ar = double(dest_x->GetValue()) / dest_y->GetValue();
	bool ar_changed = std::abs(source_ar - dest_ar) / dest_ar > .01;

	ar_mode->Enable(ar_changed);

	bool margins = ar_changed && ar_mode->GetSelection() == (int)ResampleARMode::Manual;
	symmetrical->Enable(margins);
	margin_ctrl[LEFT]->Enable(margins);
	margin_ctrl[TOP]->Enable(margins);
	margin_ctrl[RIGHT]->Enable(margins && !symmetrical->GetValue());
	margin_ctrl[BOTTOM]->Enable(margins && !symmetrical->GetValue());
}

void DialogResample::OnSymmetrical(wxCommandEvent &) {
	bool state = !symmetrical->IsChecked();

	margin_ctrl[RIGHT]->Enable(state);
	margin_ctrl[BOTTOM]->Enable(state);

	if (!state) {
		margin_ctrl[RIGHT]->SetValue(margin_ctrl[LEFT]->GetValue());
		margin_ctrl[BOTTOM]->SetValue(margin_ctrl[TOP]->GetValue());
	}
}

void DialogResample::OnMarginChange(wxSpinCtrl *src, wxSpinCtrl *dst) {
	if (symmetrical->IsChecked())
		dst->SetValue(src->GetValue());
}
}

bool PromptForResampleSettings(agi::Context *c, ResampleSettings &settings) {
	return DialogResample(c, settings).d.ShowModal() == wxID_OK;
}
