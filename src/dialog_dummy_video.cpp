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

#include "colour_button.h"
#include "format.h"
#include "help_button.h"
#include "libresrc/libresrc.h"
#include "options.h"
#include "validators.h"
#include "video_provider_dummy.h"

#include <libaegisub/ass/time.h>
#include <libaegisub/color.h>

#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/valgen.h>

namespace {
struct DialogDummyVideo {
	wxDialog d;

	double fps       = OPT_GET("Video/Dummy/FPS")->GetDouble();
	int width        = OPT_GET("Video/Dummy/Last/Width")->GetInt();
	int height       = OPT_GET("Video/Dummy/Last/Height")->GetInt();
	int length       = OPT_GET("Video/Dummy/Last/Length")->GetInt();
	agi::Color color = OPT_GET("Colour/Video Dummy/Last Colour")->GetColor();
	bool pattern     = OPT_GET("Video/Dummy/Pattern")->GetBool();

	wxStaticText *length_display;
	wxFlexGridSizer *sizer;

	template<typename T>
	void AddCtrl(wxString const& label, T *ctrl);

	void OnResolutionShortcut(wxCommandEvent &evt);
	void UpdateLengthDisplay();

	DialogDummyVideo(wxWindow *parent);
};

struct ResolutionShortcut {
	const char *name;
	int width;
	int height;
};

static ResolutionShortcut resolutions[] = {
	{"640x480 (SD fullscreen)", 640, 480},
	{"704x480 (SD anamorphic)", 704, 480},
	{"640x360 (SD widescreen)", 640, 360},
	{"704x396 (SD widescreen)", 704, 396},
	{"640x352 (SD widescreen MOD16)", 640, 352},
	{"704x400 (SD widescreen MOD16)", 704, 400},
	{"1280x720 (HD 720p)", 1280, 720},
	{"1920x1080 (HD 1080p)", 1920, 1080},
	{"1024x576 (SuperPAL widescreen)", 1024, 576}
};

wxSpinCtrl *spin_ctrl(wxWindow *parent, int min, int max, int *value) {
	auto ctrl = new wxSpinCtrl(parent, -1, "", wxDefaultPosition, wxSize(100, -1), wxSP_ARROW_KEYS, min, max, *value);
	ctrl->SetValidator(wxGenericValidator(value));
	return ctrl;
}

wxControl *spin_ctrl(wxWindow *parent, double min, double max, double *value) {
	return new wxTextCtrl(parent, -1, "", wxDefaultPosition, wxSize(100, -1), 0, DoubleValidator(value, min, max));
}

wxComboBox *resolution_shortcuts(wxWindow *parent, int width, int height) {
	wxComboBox *ctrl = new wxComboBox(parent, -1, "", wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY);

	for (auto const& res : resolutions) {
		ctrl->Append(res.name);
		if (res.width == width && res.height == height)
			ctrl->SetSelection(ctrl->GetCount() - 1);
	}

	return ctrl;
}

DialogDummyVideo::DialogDummyVideo(wxWindow *parent)
: d(parent, -1, _("Dummy video options"))
{
	d.SetIcon(GETICON(use_dummy_video_menu_16));

	auto res_sizer = new wxBoxSizer(wxHORIZONTAL);
	res_sizer->Add(spin_ctrl(&d, 1, 10000, &width), wxSizerFlags(1).Expand());
	res_sizer->Add(new wxStaticText(&d, -1, " x "), wxSizerFlags().Center());
	res_sizer->Add(spin_ctrl(&d, 1, 10000, &height), wxSizerFlags(1).Expand());

	auto color_sizer = new wxBoxSizer(wxHORIZONTAL);
	auto color_btn = new ColourButton(&d, wxSize(30, 17), false, color);
	color_sizer->Add(color_btn, wxSizerFlags().DoubleBorder(wxRIGHT));
	color_sizer->Add(new wxCheckBox(&d, -1, _("Checkerboard &pattern"), wxDefaultPosition, wxDefaultSize, 0, wxGenericValidator(&pattern)), wxSizerFlags(1).Center());

	sizer = new wxFlexGridSizer(2, 5, 5);
	AddCtrl(_("Video resolution:"), resolution_shortcuts(&d, width, height));
	AddCtrl("", res_sizer);
	AddCtrl(_("Color:"), color_sizer);
	AddCtrl(_("Frame rate (fps):"), spin_ctrl(&d, .1, 1000.0, &fps));
	AddCtrl(_("Duration (frames):"), spin_ctrl(&d, 2, 36000000, &length)); // Ten hours of 1k FPS
	AddCtrl("", length_display = new wxStaticText(&d, -1, ""));

	auto btn_sizer = d.CreateStdDialogButtonSizer(wxOK | wxCANCEL | wxHELP);
	btn_sizer->GetHelpButton()->Bind(wxEVT_BUTTON, std::bind(&HelpButton::OpenPage, "Dummy Video"));

	auto main_sizer = new wxBoxSizer(wxVERTICAL);
	main_sizer->Add(sizer, wxSizerFlags(1).Border().Expand());
	main_sizer->Add(new wxStaticLine(&d, wxHORIZONTAL), wxSizerFlags().HorzBorder().Expand());
	main_sizer->Add(btn_sizer, wxSizerFlags().Expand().Border());

	UpdateLengthDisplay();

	d.SetSizerAndFit(main_sizer);
	d.CenterOnParent();

	d.Bind(wxEVT_COMBOBOX, &DialogDummyVideo::OnResolutionShortcut, this);
	color_btn->Bind(EVT_COLOR, [=](ValueEvent<agi::Color>& e) { color = e.Get(); });
	d.Bind(wxEVT_SPINCTRL, [&](wxCommandEvent&) {
		d.TransferDataFromWindow();
		UpdateLengthDisplay();
	});
}

static void add_label(wxWindow *parent, wxSizer *sizer, wxString const& label) {
	if (!label)
		sizer->AddStretchSpacer();
	else
		sizer->Add(new wxStaticText(parent, -1, label), wxSizerFlags().Center().Left());
}

template<typename T>
void DialogDummyVideo::AddCtrl(wxString const& label, T *ctrl) {
	add_label(&d, sizer, label);
	sizer->Add(ctrl, wxSizerFlags().Expand().Center().Left());
}

void DialogDummyVideo::OnResolutionShortcut(wxCommandEvent &e) {
	d.TransferDataFromWindow();
	int rs = e.GetSelection();
	width = resolutions[rs].width;
	height = resolutions[rs].height;
	d.TransferDataToWindow();
}

void DialogDummyVideo::UpdateLengthDisplay() {
	length_display->SetLabel(fmt_tl("Resulting duration: %s", agi::Time(length / fps * 1000).GetAssFormatted(true)));
}
}

std::string CreateDummyVideo(wxWindow *parent) {
	DialogDummyVideo dlg(parent);
	if (dlg.d.ShowModal() != wxID_OK)
		return "";

	OPT_SET("Video/Dummy/FPS")->SetDouble(dlg.fps);
	OPT_SET("Video/Dummy/Last/Width")->SetInt(dlg.width);
	OPT_SET("Video/Dummy/Last/Height")->SetInt(dlg.height);
	OPT_SET("Video/Dummy/Last/Length")->SetInt(dlg.length);
	OPT_SET("Colour/Video Dummy/Last Colour")->SetColor(dlg.color);
	OPT_SET("Video/Dummy/Pattern")->SetBool(dlg.pattern);

	return DummyVideoProvider::MakeFilename(dlg.fps, dlg.length, dlg.width, dlg.height, dlg.color, dlg.pattern);
}
