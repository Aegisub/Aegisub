// Copyright (c) 2005, Niels Martin Hansen
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

#include "colorspace.h"
#include "compat.h"
#include "help_button.h"
#include "libresrc/libresrc.h"
#include "options.h"
#include "persist_location.h"
#include "utils.h"
#include "value_event.h"

#include <libaegisub/scoped_ptr.h>
#include <libaegisub/make_unique.h>

#include <memory>
#include <vector>

#include <wx/bitmap.h>
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/dcclient.h>
#include <wx/dcmemory.h>
#include <wx/dcscreen.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/image.h>
#include <wx/rawbmp.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statbmp.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#ifdef __WXMAC__
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace {

enum class PickerDirection {
	HorzVert,
	Horz,
	Vert
};

static const int spectrum_horz_vert_arrow_size = 4;

wxDEFINE_EVENT(EVT_SPECTRUM_CHANGE, wxCommandEvent);

class ColorPickerSpectrum final : public wxControl {
	int x;
	int y;

	wxBitmap *background;
	PickerDirection direction;

	void OnPaint(wxPaintEvent &evt) {
		if (!background) return;

		int height = background->GetHeight();
		int width = background->GetWidth();
		wxPaintDC dc(this);

		wxMemoryDC memdc;
		memdc.SelectObject(*background);
		dc.Blit(1, 1, width, height, &memdc, 0, 0);

		wxPoint arrow[3];
		wxRect arrow_box;

		wxPen invpen(*wxWHITE, 3);
		invpen.SetCap(wxCAP_BUTT);
		dc.SetLogicalFunction(wxXOR);
		dc.SetPen(invpen);

		switch (direction) {
			case PickerDirection::HorzVert:
				// Make a little cross
				dc.DrawLine(x-4, y+1, x+7, y+1);
				dc.DrawLine(x+1, y-4, x+1, y+7);
				break;
			case PickerDirection::Horz:
				// Make a vertical line stretching all the way across
				dc.DrawLine(x+1, 1, x+1, height+1);
				// Points for arrow
				arrow[0] = wxPoint(x+1, height+2);
				arrow[1] = wxPoint(x+1-spectrum_horz_vert_arrow_size, height+2+spectrum_horz_vert_arrow_size);
				arrow[2] = wxPoint(x+1+spectrum_horz_vert_arrow_size, height+2+spectrum_horz_vert_arrow_size);

				arrow_box.SetLeft(0);
				arrow_box.SetTop(height + 2);
				arrow_box.SetRight(width + 1 + spectrum_horz_vert_arrow_size);
				arrow_box.SetBottom(height + 2 + spectrum_horz_vert_arrow_size);
				break;
			case PickerDirection::Vert:
				// Make a horizontal line stretching all the way across
				dc.DrawLine(1, y+1, width+1, y+1);
				// Points for arrow
				arrow[0] = wxPoint(width+2, y+1);
				arrow[1] = wxPoint(width+2+spectrum_horz_vert_arrow_size, y+1-spectrum_horz_vert_arrow_size);
				arrow[2] = wxPoint(width+2+spectrum_horz_vert_arrow_size, y+1+spectrum_horz_vert_arrow_size);

				arrow_box.SetLeft(width + 2);
				arrow_box.SetTop(0);
				arrow_box.SetRight(width + 2 + spectrum_horz_vert_arrow_size);
				arrow_box.SetBottom(height + 1 + spectrum_horz_vert_arrow_size);
				break;
		}

		if (direction == PickerDirection::Horz || direction == PickerDirection::Vert) {
			wxBrush bgBrush;
			bgBrush.SetColour(GetBackgroundColour());
			dc.SetLogicalFunction(wxCOPY);
			dc.SetPen(*wxTRANSPARENT_PEN);
			dc.SetBrush(bgBrush);
			dc.DrawRectangle(arrow_box);

			// Arrow pointing at current point
			dc.SetBrush(*wxBLACK_BRUSH);
			dc.DrawPolygon(3, arrow);
		}

		// Border around the spectrum
		wxPen blkpen(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT), 1);
		blkpen.SetCap(wxCAP_BUTT);

		dc.SetLogicalFunction(wxCOPY);
		dc.SetPen(blkpen);
		dc.SetBrush(*wxTRANSPARENT_BRUSH);
		dc.DrawRectangle(0, 0, background->GetWidth()+2, background->GetHeight()+2);
	}

	void OnMouse(wxMouseEvent &evt) {
		evt.Skip();

		// We only care about mouse move events during a drag
		if (evt.Moving())
			return;

		if (evt.LeftDown()) {
			CaptureMouse();
			SetCursor(wxCursor(wxCURSOR_BLANK));
		}
		else if (evt.LeftUp() && HasCapture()) {
			ReleaseMouse();
			SetCursor(wxNullCursor);
		}

		if (evt.LeftDown() || (HasCapture() && evt.LeftIsDown())) {
			// Adjust for the 1px black border around the control
			int newx = mid(0, evt.GetX() - 1, GetClientSize().x - 3);
			int newy = mid(0, evt.GetY() - 1, GetClientSize().y - 3);
			SetXY(newx, newy);
			wxCommandEvent evt2(EVT_SPECTRUM_CHANGE, GetId());
			AddPendingEvent(evt2);
		}
	}

	bool AcceptsFocusFromKeyboard() const override { return false; }

public:
	ColorPickerSpectrum(wxWindow *parent, PickerDirection direction, wxSize size)
	: wxControl(parent, -1, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
	, x(-1)
	, y(-1)
	, background(nullptr)
	, direction(direction)
	{
		size.x += 2;
		size.y += 2;

		if (direction == PickerDirection::Vert) size.x += spectrum_horz_vert_arrow_size + 1;
		if (direction == PickerDirection::Horz) size.y += spectrum_horz_vert_arrow_size + 1;

		SetClientSize(size);
		SetMinSize(GetSize());

		Bind(wxEVT_LEFT_DOWN, &ColorPickerSpectrum::OnMouse, this);
		Bind(wxEVT_LEFT_UP, &ColorPickerSpectrum::OnMouse, this);
		Bind(wxEVT_MOTION, &ColorPickerSpectrum::OnMouse, this);
		Bind(wxEVT_PAINT, &ColorPickerSpectrum::OnPaint, this);
	}

	int GetX() const { return x; }
	int GetY() const { return y; }

	void SetXY(int xx, int yy) {
		if (x != xx || y != yy) {
			x = xx;
			y = yy;
			Refresh(false);
		}
	}

	/// @brief Set the background image for this spectrum
	/// @param new_background New background image
	/// @param force Repaint even if it appears to be the same image
	void SetBackground(wxBitmap *new_background, bool force = false) {
		if (background == new_background && !force) return;
		background = new_background;
		Refresh(false);
	}
};

#ifdef WIN32
#define STATIC_BORDER_FLAG wxSTATIC_BORDER
#else
#define STATIC_BORDER_FLAG wxSIMPLE_BORDER
#endif

wxDEFINE_EVENT(EVT_RECENT_SELECT, ValueEvent<agi::Color>);

/// @class ColorPickerRecent
/// @brief A grid of recently used colors which can be selected by clicking on them
class ColorPickerRecent final : public wxStaticBitmap {
	int rows;     ///< Number of rows of colors
	int cols;     ///< Number of cols of colors
	int cellsize; ///< Width/Height of each cell

	/// The colors currently displayed in the control
	std::vector<agi::Color> colors;

	void OnClick(wxMouseEvent &evt) {
		wxSize cs = GetClientSize();
		int cx = evt.GetX() * cols / cs.x;
		int cy = evt.GetY() * rows / cs.y;
		if (cx < 0 || cx > cols || cy < 0 || cy > rows) return;
		int i = cols*cy + cx;

		if (i >= 0 && i < (int)colors.size())
			AddPendingEvent(ValueEvent<agi::Color>(EVT_RECENT_SELECT, GetId(), colors[i]));
	}

	void UpdateBitmap() {
		wxSize sz = GetClientSize();

		wxBitmap background(sz.x, sz.y);
		wxMemoryDC dc(background);

		dc.SetPen(*wxTRANSPARENT_PEN);

		for (int cy = 0; cy < rows; cy++) {
			for (int cx = 0; cx < cols; cx++) {
				int x = cx * cellsize;
				int y = cy * cellsize;

				dc.SetBrush(wxBrush(to_wx(colors[cy * cols + cx])));
				dc.DrawRectangle(x, y, x+cellsize, y+cellsize);
			}
		}

		{
			wxEventBlocker blocker(this);
			SetBitmap(background);
		}

		Refresh(false);
	}

	bool AcceptsFocusFromKeyboard() const override { return false; }

public:
	ColorPickerRecent(wxWindow *parent, int cols, int rows, int cellsize)
	: wxStaticBitmap(parent, -1, wxBitmap(), wxDefaultPosition, wxDefaultSize, STATIC_BORDER_FLAG)
	, rows(rows)
	, cols(cols)
	, cellsize(cellsize)
	{
		colors.resize(rows * cols);
		SetClientSize(cols*cellsize, rows*cellsize);
		SetMinSize(GetSize());
		SetMaxSize(GetSize());
		SetCursor(*wxCROSS_CURSOR);

		Bind(wxEVT_LEFT_DOWN, &ColorPickerRecent::OnClick, this);
		Bind(wxEVT_SIZE, [=](wxSizeEvent&) { UpdateBitmap(); });
	}

	/// Load the colors to show
	void Load(std::vector<agi::Color> const& recent_colors) {
		colors = recent_colors;
		colors.resize(rows * cols);
		UpdateBitmap();
	}

	/// Get the list of recent colors
	std::vector<agi::Color> Save() const { return colors; }

	/// Add a color to the beginning of the recent list
	void AddColor(agi::Color color) {
		auto existing = find(colors.begin(), colors.end(), color);
		if (existing != colors.end())
			rotate(colors.begin(), existing, existing + 1);
		else {
			colors.insert(colors.begin(), color);
			colors.pop_back();
		}

		UpdateBitmap();
	}
};

wxDEFINE_EVENT(EVT_DROPPER_SELECT, ValueEvent<agi::Color>);

class ColorPickerScreenDropper final : public wxControl {
	wxBitmap capture;

	int resx, resy;
	int magnification;

	void OnMouse(wxMouseEvent &evt) {
		int x = evt.GetX();
		int y = evt.GetY();

		if (x >= 0 && x < capture.GetWidth() && y >= 0 && y < capture.GetHeight()) {
			wxNativePixelData pd(capture, wxRect(x, y, 1, 1));
			wxNativePixelData::Iterator pdi(pd.GetPixels());
			agi::Color color(pdi.Red(), pdi.Green(), pdi.Blue(), 0);

			wxThreadEvent evnt(EVT_DROPPER_SELECT, GetId());
			AddPendingEvent(ValueEvent<agi::Color>(EVT_DROPPER_SELECT, GetId(), color));
		}
	}

	void OnPaint(wxPaintEvent &evt) {
		wxPaintDC(this).DrawBitmap(capture, 0, 0);
	}

	bool AcceptsFocusFromKeyboard() const override { return false; }

public:
	ColorPickerScreenDropper(wxWindow *parent, int resx, int resy, int magnification)
	: wxControl(parent, -1, wxDefaultPosition, wxDefaultSize, STATIC_BORDER_FLAG)
	, capture(resx * magnification, resy * magnification, wxNativePixelFormat::BitsPerPixel)
	, resx(resx)
	, resy(resy)
	, magnification(magnification)
	{
		SetClientSize(resx*magnification, resy*magnification);
		SetMinSize(GetSize());
		SetMaxSize(GetSize());
		SetCursor(*wxCROSS_CURSOR);

		wxMemoryDC capdc(capture);
		capdc.SetPen(*wxTRANSPARENT_PEN);
		capdc.SetBrush(*wxWHITE_BRUSH);
		capdc.DrawRectangle(0, 0, capture.GetWidth(), capture.GetHeight());

		Bind(wxEVT_PAINT, &ColorPickerScreenDropper::OnPaint, this);
		Bind(wxEVT_LEFT_DOWN, &ColorPickerScreenDropper::OnMouse, this);
	}

	void DropFromScreenXY(int x, int y);
};

void ColorPickerScreenDropper::DropFromScreenXY(int x, int y) {
	wxMemoryDC capdc(capture);
	capdc.SetPen(*wxTRANSPARENT_PEN);
#ifndef __WXMAC__
	wxScreenDC screen;
	capdc.StretchBlit(0, 0, resx * magnification, resy * magnification,
		&screen, x - resx / 2, y - resy / 2, resx, resy);
#else
	// wxScreenDC doesn't work on recent versions of OS X so do it manually

	// Doesn't bother handling the case where the rect overlaps two monitors
	CGDirectDisplayID display_id;
	uint32_t display_count;
	CGGetDisplaysWithPoint(CGPointMake(x, y), 1, &display_id, &display_count);

	agi::scoped_holder<CGImageRef> img(CGDisplayCreateImageForRect(display_id, CGRectMake(x - resx / 2, y - resy / 2, resx, resy)), CGImageRelease);
	NSUInteger width = CGImageGetWidth(img);
	NSUInteger height = CGImageGetHeight(img);
	std::vector<uint8_t> imgdata(height * width * 4);

	agi::scoped_holder<CGColorSpaceRef> colorspace(CGColorSpaceCreateDeviceRGB(), CGColorSpaceRelease);
	agi::scoped_holder<CGContextRef> bmp_context(CGBitmapContextCreate(&imgdata[0], width, height, 8, 4 * width, colorspace, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big), CGContextRelease);

	CGContextDrawImage(bmp_context, CGRectMake(0, 0, width, height), img);

	for (int x = 0; x < resx; x++) {
		for (int y = 0; y < resy; y++) {
			uint8_t *pixel = &imgdata[y * width * 4 + x * 4];
			capdc.SetBrush(wxBrush(wxColour(pixel[0], pixel[1], pixel[2])));
			capdc.DrawRectangle(x * magnification, y * magnification, magnification, magnification);
		}
	}
#endif

	Refresh(false);
}

class DialogColorPicker final : public wxDialog {
	std::unique_ptr<PersistLocation> persist;

	agi::Color cur_color; ///< Currently selected colour

	bool spectrum_dirty; ///< Does the spectrum image need to be regenerated?
	ColorPickerSpectrum *spectrum; ///< The 2D color spectrum
	ColorPickerSpectrum *slider; ///< The 1D slider for the color component not in the slider
	ColorPickerSpectrum *alpha_slider;

	wxChoice *colorspace_choice; ///< The dropdown list to select colorspaces

	wxSpinCtrl *rgb_input[3];
	wxBitmap rgb_spectrum[3]; ///< x/y spectrum bitmap where color "i" is excluded from
	wxBitmap rgb_slider[3];   ///< z spectrum for color "i"

	wxSpinCtrl *hsl_input[3];
	wxBitmap hsl_spectrum; ///< h/s spectrum
	wxBitmap hsl_slider;   ///< l spectrum

	wxSpinCtrl *hsv_input[3];
	wxBitmap hsv_spectrum; ///< s/v spectrum
	wxBitmap hsv_slider;   ///< h spectrum
	wxBitmap alpha_slider_img;

	wxTextCtrl *ass_input;
	wxTextCtrl *html_input;
	wxSpinCtrl *alpha_input;

	/// The eyedropper is set to a blank icon when it's clicked, so store its normal bitmap
	wxBitmap eyedropper_bitmap;

	/// The point where the eyedropper was click, used to make it possible to either
	/// click the eyedropper or drag the eyedropper
	wxPoint eyedropper_grab_point;

	bool eyedropper_is_grabbed;

	wxStaticBitmap *preview_box; ///< A box which simply shows the current color
	ColorPickerRecent *recent_box; ///< A grid of recently used colors

	ColorPickerScreenDropper *screen_dropper;

	wxStaticBitmap *screen_dropper_icon;

	/// Update all other controls as a result of modifying an RGB control
	void UpdateFromRGB(bool dirty = true);
	/// Update all other controls as a result of modifying an HSL control
	void UpdateFromHSL(bool dirty = true);
	/// Update all other controls as a result of modifying an HSV control
	void UpdateFromHSV(bool dirty = true);
	/// Update all other controls as a result of modifying the ASS format control
	void UpdateFromAss();
	/// Update all other controls as a result of modifying the HTML format control
	void UpdateFromHTML();
	void UpdateFromAlpha();

	void SetRGB(agi::Color new_color);
	void SetHSL(unsigned char r, unsigned char g, unsigned char b);
	void SetHSV(unsigned char r, unsigned char g, unsigned char b);

	/// Redraw the spectrum display
	void UpdateSpectrumDisplay();

	wxBitmap *MakeGBSpectrum();
	wxBitmap *MakeRBSpectrum();
	wxBitmap *MakeRGSpectrum();
	wxBitmap *MakeHSSpectrum();
	wxBitmap *MakeSVSpectrum();

	/// Constructor helper function for making the color input box sizers
	template<int N, class Control>
	wxSizer *MakeColorInputSizer(wxString (&labels)[N], Control *(&inputs)[N]);

	void OnChangeMode(wxCommandEvent &evt);
	void OnSpectrumChange(wxCommandEvent &evt);
	void OnSliderChange(wxCommandEvent &evt);
	void OnAlphaSliderChange(wxCommandEvent &evt);
	void OnRecentSelect(ValueEvent<agi::Color>& evt); // also handles dropper pick
	void OnDropperMouse(wxMouseEvent &evt);
	void OnMouse(wxMouseEvent &evt);
	void OnCaptureLost(wxMouseCaptureLostEvent&);

	std::function<void (agi::Color)> callback;

public:
	DialogColorPicker(wxWindow *parent, agi::Color initial_color, std::function<void (agi::Color)> callback, bool alpha);
	~DialogColorPicker();

	void SetColor(agi::Color new_color);
	void AddColorToRecent();
};

static const int slider_width = 10; ///< width in pixels of the color slider control
static const int alpha_box_size = 5;

template<typename Func>
static wxBitmap make_slider_img(Func func) {
	unsigned char *slid = (unsigned char *)calloc(slider_width * 256 * 3, 1);
	func(slid);
	wxImage img(slider_width, 256, slid);
	return wxBitmap(img);
}

template<typename Func>
static wxBitmap make_slider(Func func) {
	return make_slider_img([&](unsigned char *slid) {
		for (int y = 0; y < 256; ++y) {
			unsigned char rgb[3];
			func(y, rgb);
			for (int x = 0; x < slider_width; ++x)
				memcpy(slid + y * slider_width * 3 + x * 3, rgb, 3);
		}
	});
}

DialogColorPicker::DialogColorPicker(wxWindow *parent, agi::Color initial_color, std::function<void (agi::Color)> callback, bool alpha)
: wxDialog(parent, -1, _("Select Color"))
, callback(std::move(callback))
{
	// generate spectrum slider bar images
	for (int i = 0; i < 3; ++i) {
		rgb_slider[i] = make_slider([=](int y, unsigned char *rgb) {
			memset(rgb, 0, 3);
			rgb[i] = y;
		});
	}
	hsl_slider = make_slider([](int y, unsigned char *rgb) { memset(rgb, y, 3); });
	hsv_slider = make_slider([](int y, unsigned char *rgb) { hsv_to_rgb(y, 255, 255, rgb, rgb + 1, rgb + 2); });

	// Create the controls for the dialog
	wxSizer *spectrum_box = new wxStaticBoxSizer(wxVERTICAL, this, _("Color spectrum"));
	spectrum = new ColorPickerSpectrum(this, PickerDirection::HorzVert, wxSize(256, 256));
	slider = new ColorPickerSpectrum(this, PickerDirection::Vert, wxSize(slider_width, 256));
	alpha_slider = new ColorPickerSpectrum(this, PickerDirection::Vert, wxSize(slider_width, 256));
	wxString modes[] = { _("RGB/R"), _("RGB/G"), _("RGB/B"), _("HSL/L"), _("HSV/H") };
	colorspace_choice = new wxChoice(this, -1, wxDefaultPosition, wxDefaultSize, 5, modes);

	wxSize colorinput_size = GetTextExtent(" [&H10117B&] ");
	colorinput_size.SetHeight(-1);
	wxSize colorinput_labelsize(40, -1);

	wxSizer *rgb_box = new wxStaticBoxSizer(wxHORIZONTAL, this, _("RGB color"));
	wxSizer *hsl_box = new wxStaticBoxSizer(wxVERTICAL, this, _("HSL color"));
	wxSizer *hsv_box = new wxStaticBoxSizer(wxVERTICAL, this, _("HSV color"));

	for (auto& elem : rgb_input)
		elem = new wxSpinCtrl(this, -1, "", wxDefaultPosition, colorinput_size, wxSP_ARROW_KEYS, 0, 255);

	ass_input = new wxTextCtrl(this, -1, "", wxDefaultPosition, colorinput_size);
	html_input = new wxTextCtrl(this, -1, "", wxDefaultPosition, colorinput_size);
	alpha_input = new wxSpinCtrl(this, -1, "", wxDefaultPosition, colorinput_size, wxSP_ARROW_KEYS, 0, 255);

	for (auto& elem : hsl_input)
		elem = new wxSpinCtrl(this, -1, "", wxDefaultPosition, colorinput_size, wxSP_ARROW_KEYS, 0, 255);

	for (auto& elem : hsv_input)
		elem = new wxSpinCtrl(this, -1, "", wxDefaultPosition, colorinput_size, wxSP_ARROW_KEYS, 0, 255);

	preview_box = new wxStaticBitmap(this, -1, wxBitmap(40, 40, 24), wxDefaultPosition, wxSize(40, 40), STATIC_BORDER_FLAG);
	recent_box = new ColorPickerRecent(this, 8, 4, 16);

	eyedropper_bitmap = GETIMAGE(eyedropper_tool_24);
	eyedropper_bitmap.SetMask(new wxMask(eyedropper_bitmap, wxColour(255, 0, 255)));
	screen_dropper_icon = new wxStaticBitmap(this, -1, eyedropper_bitmap, wxDefaultPosition, wxDefaultSize, wxRAISED_BORDER);
	screen_dropper = new ColorPickerScreenDropper(this, 7, 7, 8);

	// Arrange the controls in a nice way
	wxSizer *spectop_sizer = new wxBoxSizer(wxHORIZONTAL);
	spectop_sizer->Add(new wxStaticText(this, -1, _("Spectrum mode:")), 0, wxALIGN_CENTER_VERTICAL|wxALIGN_LEFT|wxRIGHT, 5);
	spectop_sizer->Add(colorspace_choice, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_LEFT);
	spectop_sizer->Add(5, 5, 1, wxEXPAND);
	spectop_sizer->Add(preview_box, 0, wxALIGN_CENTER_VERTICAL);

	wxSizer *spectrum_sizer = new wxFlexGridSizer(3, 5, 5);
	spectrum_sizer->Add(spectop_sizer, wxEXPAND);
	spectrum_sizer->AddStretchSpacer(1);
	spectrum_sizer->AddStretchSpacer(1);
	spectrum_sizer->Add(spectrum);
	spectrum_sizer->Add(slider);
	spectrum_sizer->Add(alpha_slider);
	if (!alpha)
		spectrum_sizer->Hide(alpha_slider);

	spectrum_box->Add(spectrum_sizer, 0, wxALL, 3);

	wxString rgb_labels[] = { _("Red:"), _("Green:"), _("Blue:") };
	rgb_box->Add(MakeColorInputSizer(rgb_labels, rgb_input), 1, wxALL|wxEXPAND, 3);

	wxString ass_labels[] = { "ASS:", "HTML:", _("Alpha:") };
	wxControl *ass_ctrls[] = { ass_input, html_input, alpha_input };
	auto ass_colors_sizer = MakeColorInputSizer(ass_labels, ass_ctrls);
	if (!alpha)
		ass_colors_sizer->Hide(alpha_input);
	rgb_box->Add(ass_colors_sizer, 0, wxALL|wxCENTER|wxEXPAND, 3);

	wxString hsl_labels[] = { _("Hue:"), _("Sat.:"), _("Lum.:") };
	hsl_box->Add(MakeColorInputSizer(hsl_labels, hsl_input), 0, wxALL|wxEXPAND, 3);

	wxString hsv_labels[] = { _("Hue:"), _("Sat.:"), _("Value:") };
	hsv_box->Add(MakeColorInputSizer(hsv_labels, hsv_input), 0, wxALL|wxEXPAND, 3);

	wxSizer *hsx_sizer = new wxBoxSizer(wxHORIZONTAL);
	hsx_sizer->Add(hsl_box);
	hsx_sizer->AddSpacer(5);
	hsx_sizer->Add(hsv_box);

	wxSizer *picker_sizer = new wxBoxSizer(wxHORIZONTAL);
	picker_sizer->AddStretchSpacer();
	picker_sizer->Add(screen_dropper_icon, 0, wxALIGN_CENTER|wxRIGHT, 5);
	picker_sizer->Add(screen_dropper, 0, wxALIGN_CENTER);
	picker_sizer->AddStretchSpacer();
	picker_sizer->Add(recent_box, 0, wxALIGN_CENTER);
	picker_sizer->AddStretchSpacer();

	wxStdDialogButtonSizer *button_sizer = CreateStdDialogButtonSizer(wxOK | wxCANCEL | wxHELP);

	wxSizer *input_sizer = new wxBoxSizer(wxVERTICAL);
	input_sizer->Add(rgb_box, 0, wxEXPAND);
	input_sizer->AddSpacer(5);
	input_sizer->Add(hsx_sizer, 0, wxEXPAND);
	input_sizer->AddStretchSpacer(1);
	input_sizer->Add(picker_sizer, 0, wxEXPAND);
	input_sizer->AddStretchSpacer(2);
	input_sizer->Add(button_sizer, 0, wxALIGN_RIGHT);

	wxSizer *main_sizer = new wxBoxSizer(wxHORIZONTAL);
	main_sizer->Add(spectrum_box, 1, wxALL | wxEXPAND, 5);
	main_sizer->Add(input_sizer, 0, (wxALL&~wxLEFT)|wxEXPAND, 5);

	SetSizerAndFit(main_sizer);

	persist = agi::make_unique<PersistLocation>(this, "Tool/Colour Picker");

	// Fill the controls
	int mode = OPT_GET("Tool/Colour Picker/Mode")->GetInt();
	if (mode < 0 || mode > 4) mode = 3; // HSL default
	colorspace_choice->SetSelection(mode);
	SetColor(initial_color);
	recent_box->Load(OPT_GET("Tool/Colour Picker/Recent Colours")->GetListColor());

	using std::bind;
	for (int i = 0; i < 3; ++i) {
		rgb_input[i]->Bind(wxEVT_SPINCTRL, bind(&DialogColorPicker::UpdateFromRGB, this, true));
		rgb_input[i]->Bind(wxEVT_TEXT, bind(&DialogColorPicker::UpdateFromRGB, this, true));
		hsl_input[i]->Bind(wxEVT_SPINCTRL, bind(&DialogColorPicker::UpdateFromHSL, this, true));
		hsl_input[i]->Bind(wxEVT_TEXT, bind(&DialogColorPicker::UpdateFromHSL, this, true));
		hsv_input[i]->Bind(wxEVT_SPINCTRL, bind(&DialogColorPicker::UpdateFromHSV, this, true));
		hsv_input[i]->Bind(wxEVT_TEXT, bind(&DialogColorPicker::UpdateFromHSV, this, true));
	}
	ass_input->Bind(wxEVT_TEXT, bind(&DialogColorPicker::UpdateFromAss, this));
	html_input->Bind(wxEVT_TEXT, bind(&DialogColorPicker::UpdateFromHTML, this));
	alpha_input->Bind(wxEVT_SPINCTRL, bind(&DialogColorPicker::UpdateFromAlpha, this));
	alpha_input->Bind(wxEVT_TEXT, bind(&DialogColorPicker::UpdateFromAlpha, this));

	screen_dropper_icon->Bind(wxEVT_MOTION, &DialogColorPicker::OnDropperMouse, this);
	screen_dropper_icon->Bind(wxEVT_LEFT_DOWN, &DialogColorPicker::OnDropperMouse, this);
	screen_dropper_icon->Bind(wxEVT_LEFT_UP, &DialogColorPicker::OnDropperMouse, this);
	screen_dropper_icon->Bind(wxEVT_MOUSE_CAPTURE_LOST, &DialogColorPicker::OnCaptureLost, this);
	Bind(wxEVT_MOTION, &DialogColorPicker::OnMouse, this);
	Bind(wxEVT_LEFT_DOWN, &DialogColorPicker::OnMouse, this);
	Bind(wxEVT_LEFT_UP, &DialogColorPicker::OnMouse, this);

	spectrum->Bind(EVT_SPECTRUM_CHANGE, &DialogColorPicker::OnSpectrumChange, this);
	slider->Bind(EVT_SPECTRUM_CHANGE, &DialogColorPicker::OnSliderChange, this);
	alpha_slider->Bind(EVT_SPECTRUM_CHANGE, &DialogColorPicker::OnAlphaSliderChange, this);
	recent_box->Bind(EVT_RECENT_SELECT, &DialogColorPicker::OnRecentSelect, this);
	screen_dropper->Bind(EVT_DROPPER_SELECT, &DialogColorPicker::OnRecentSelect, this);

	colorspace_choice->Bind(wxEVT_CHOICE, &DialogColorPicker::OnChangeMode, this);

	button_sizer->GetHelpButton()->Bind(wxEVT_BUTTON, bind(&HelpButton::OpenPage, "Colour Picker"));
}

template<int N, class Control>
wxSizer *DialogColorPicker::MakeColorInputSizer(wxString (&labels)[N], Control *(&inputs)[N]) {
	auto sizer = new wxFlexGridSizer(2, 5, 5);
	for (int i = 0; i < N; ++i) {
		sizer->Add(new wxStaticText(this, -1, labels[i]), wxSizerFlags(1).Center().Left());
		sizer->Add(inputs[i]);
	}
	sizer->AddGrowableCol(0,1);
	return sizer;
}

DialogColorPicker::~DialogColorPicker() {
	if (screen_dropper_icon->HasCapture()) screen_dropper_icon->ReleaseMouse();
}

static void change_value(wxSpinCtrl *ctrl, int value) {
	wxEventBlocker blocker(ctrl);
	ctrl->SetValue(value);
}

void DialogColorPicker::SetColor(agi::Color new_color) {
	change_value(alpha_input, new_color.a);
	alpha_slider->SetXY(0, new_color.a);
	cur_color.a = new_color.a;

	SetRGB(new_color);
	spectrum_dirty = true;
	UpdateFromRGB();
}

void DialogColorPicker::AddColorToRecent() {
	recent_box->AddColor(cur_color);
	OPT_SET("Tool/Colour Picker/Recent Colours")->SetListColor(recent_box->Save());
}

void DialogColorPicker::SetRGB(agi::Color new_color) {
	change_value(rgb_input[0], new_color.r);
	change_value(rgb_input[1], new_color.g);
	change_value(rgb_input[2], new_color.b);
	new_color.a = cur_color.a;
	cur_color = new_color;
}

void DialogColorPicker::SetHSL(unsigned char r, unsigned char g, unsigned char b) {
	unsigned char h, s, l;
	rgb_to_hsl(r, g, b, &h, &s, &l);
	change_value(hsl_input[0], h);
	change_value(hsl_input[1], s);
	change_value(hsl_input[2], l);
}

void DialogColorPicker::SetHSV(unsigned char r, unsigned char g, unsigned char b) {
	unsigned char h, s, v;
	rgb_to_hsv(r, g, b, &h, &s, &v);
	change_value(hsv_input[0], h);
	change_value(hsv_input[1], s);
	change_value(hsv_input[2], v);
}

void DialogColorPicker::UpdateFromRGB(bool dirty) {
	unsigned char r = rgb_input[0]->GetValue();
	unsigned char g = rgb_input[1]->GetValue();
	unsigned char b = rgb_input[2]->GetValue();
	SetHSL(r, g, b);
	SetHSV(r, g, b);
	cur_color = agi::Color(r, g, b, cur_color.a);
	ass_input->ChangeValue(to_wx(cur_color.GetAssOverrideFormatted()));
	html_input->ChangeValue(to_wx(cur_color.GetHexFormatted()));

	if (dirty)
		spectrum_dirty = true;
	UpdateSpectrumDisplay();
}

void DialogColorPicker::UpdateFromHSL(bool dirty) {
	unsigned char r, g, b, h, s, l;
	h = hsl_input[0]->GetValue();
	s = hsl_input[1]->GetValue();
	l = hsl_input[2]->GetValue();
	hsl_to_rgb(h, s, l, &r, &g, &b);
	SetRGB(agi::Color(r, g, b));
	SetHSV(r, g, b);

	ass_input->ChangeValue(to_wx(cur_color.GetAssOverrideFormatted()));
	html_input->ChangeValue(to_wx(cur_color.GetHexFormatted()));

	if (dirty)
		spectrum_dirty = true;
	UpdateSpectrumDisplay();
}

void DialogColorPicker::UpdateFromHSV(bool dirty) {
	unsigned char r, g, b, h, s, v;
	h = hsv_input[0]->GetValue();
	s = hsv_input[1]->GetValue();
	v = hsv_input[2]->GetValue();
	hsv_to_rgb(h, s, v, &r, &g, &b);
	SetRGB(agi::Color(r, g, b));
	SetHSL(r, g, b);
	ass_input->ChangeValue(to_wx(cur_color.GetAssOverrideFormatted()));
	html_input->ChangeValue(to_wx(cur_color.GetHexFormatted()));

	if (dirty)
		spectrum_dirty = true;
	UpdateSpectrumDisplay();
}

void DialogColorPicker::UpdateFromAss() {
	agi::Color color(from_wx(ass_input->GetValue()));
	SetRGB(color);
	SetHSL(color.r, color.g, color.b);
	SetHSV(color.r, color.g, color.b);
	html_input->ChangeValue(to_wx(cur_color.GetHexFormatted()));

	spectrum_dirty = true;
	UpdateSpectrumDisplay();
}

void DialogColorPicker::UpdateFromHTML() {
	agi::Color color(from_wx(html_input->GetValue()));
	SetRGB(color);
	SetHSL(color.r, color.g, color.b);
	SetHSV(color.r, color.g, color.b);
	ass_input->ChangeValue(to_wx(cur_color.GetAssOverrideFormatted()));

	spectrum_dirty = true;
	UpdateSpectrumDisplay();
}

void DialogColorPicker::UpdateFromAlpha() {
	cur_color.a = alpha_input->GetValue();
	alpha_slider->SetXY(0, cur_color.a);
	callback(cur_color);
}

void DialogColorPicker::UpdateSpectrumDisplay() {
	int i = colorspace_choice->GetSelection();
	if (spectrum_dirty) {
		switch (i) {
			case 0: spectrum->SetBackground(MakeGBSpectrum(), true); break;
			case 1: spectrum->SetBackground(MakeRBSpectrum(), true); break;
			case 2: spectrum->SetBackground(MakeRGSpectrum(), true); break;
			case 3: spectrum->SetBackground(MakeHSSpectrum(), true); break;
			case 4: spectrum->SetBackground(MakeSVSpectrum(), true); break;
		}
	}

	switch (i) {
		case 0: case 1: case 2:
			slider->SetBackground(&rgb_slider[i]);
			slider->SetXY(0, rgb_input[i]->GetValue());
			spectrum->SetXY(rgb_input[2 - (i == 2)]->GetValue(), rgb_input[i == 0]->GetValue());
			break;
		case 3:
			slider->SetBackground(&hsl_slider);
			slider->SetXY(0, hsl_input[2]->GetValue());
			spectrum->SetXY(hsl_input[1]->GetValue(), hsl_input[0]->GetValue());
			break;
		case 4:
			slider->SetBackground(&hsv_slider);
			slider->SetXY(0, hsv_input[0]->GetValue());
			spectrum->SetXY(hsv_input[1]->GetValue(), hsv_input[2]->GetValue());
			break;
	}
	spectrum_dirty = false;

	wxBitmap tempBmp = preview_box->GetBitmap();
	{
		wxMemoryDC previewdc;
		previewdc.SelectObject(tempBmp);
		previewdc.SetPen(*wxTRANSPARENT_PEN);
		previewdc.SetBrush(wxBrush(to_wx(cur_color)));
		previewdc.DrawRectangle(0, 0, 40, 40);
	}
	preview_box->SetBitmap(tempBmp);

	alpha_slider_img = make_slider_img([=](unsigned char *slid) {
		static_assert(slider_width % alpha_box_size == 0, "Slider width must be a multiple of alpha box width");

		for (int y = 0; y < 256; ++y) {
			unsigned char inv_y = 0xFF - y;

			unsigned char box_colors[] = {
				static_cast<unsigned char>(0x66 - inv_y * 0x66 / 0xFF),
				static_cast<unsigned char>(0x99 - inv_y * 0x99 / 0xFF)
			};

			if ((y / alpha_box_size) & 1)
				std::swap(box_colors[0], box_colors[1]);

			unsigned char colors[2][3];
			for (int i = 0; i < 2; ++i) {
				colors[i][0] = cur_color.r * inv_y / 0xFF + box_colors[i];
				colors[i][1] = cur_color.g * inv_y / 0xFF + box_colors[i];
				colors[i][2] = cur_color.b * inv_y / 0xFF + box_colors[i];
			}

			for (int x = 0; x < slider_width; ++x) {
				*slid++ = colors[x / alpha_box_size][0];
				*slid++ = colors[x / alpha_box_size][1];
				*slid++ = colors[x / alpha_box_size][2];
			}
		}
	});
	alpha_slider->SetBackground(&alpha_slider_img, true);

	callback(cur_color);
}

template<typename Func>
static wxBitmap *make_spectrum(wxBitmap *bitmap, Func func) {
	wxImage spectrum_image(256, 256);
	func(spectrum_image.GetData());
	*bitmap = wxBitmap(spectrum_image);
	return bitmap;
}

wxBitmap *DialogColorPicker::MakeGBSpectrum() {
	return make_spectrum(&rgb_spectrum[0], [=](unsigned char *spec) {
		for (int g = 0; g < 256; g++) {
			for (int b = 0; b < 256; b++) {
				*spec++ = cur_color.r;
				*spec++ = g;
				*spec++ = b;
			}
		}
	});
}

wxBitmap *DialogColorPicker::MakeRBSpectrum() {
	return make_spectrum(&rgb_spectrum[1], [=](unsigned char *spec) {
		for (int r = 0; r < 256; r++) {
			for (int b = 0; b < 256; b++) {
				*spec++ = r;
				*spec++ = cur_color.g;
				*spec++ = b;
			}
		}
	});
}

wxBitmap *DialogColorPicker::MakeRGSpectrum() {
	return make_spectrum(&rgb_spectrum[2], [=](unsigned char *spec) {
		for (int r = 0; r < 256; r++) {
			for (int g = 0; g < 256; g++) {
				*spec++ = r;
				*spec++ = g;
				*spec++ = cur_color.b;
			}
		}
	});
}

wxBitmap *DialogColorPicker::MakeHSSpectrum() {
	int l = hsl_input[2]->GetValue();
	return make_spectrum(&hsl_spectrum, [=](unsigned char *spec) {
		for (int h = 0; h < 256; h++) {
			unsigned char maxr, maxg, maxb;
			hsl_to_rgb(h, 255, l, &maxr, &maxg, &maxb);

			for (int s = 0; s < 256; s++) {
				*spec++ = maxr * s / 256 + (255-s) * l / 256;
				*spec++ = maxg * s / 256 + (255-s) * l / 256;
				*spec++ = maxb * s / 256 + (255-s) * l / 256;
			}
		}
	});
}

wxBitmap *DialogColorPicker::MakeSVSpectrum() {
	int h = hsv_input[0]->GetValue();
	unsigned char maxr, maxg, maxb;
	hsv_to_rgb(h, 255, 255, &maxr, &maxg, &maxb);

	return make_spectrum(&hsv_spectrum, [=](unsigned char *spec) {
		for (int v = 0; v < 256; v++) {
			int rr = (255-maxr) * v / 256;
			int rg = (255-maxg) * v / 256;
			int rb = (255-maxb) * v / 256;
			for (int s = 0; s < 256; s++) {
				*spec++ = 255 - rr * s / 256 - (255-v);
				*spec++ = 255 - rg * s / 256 - (255-v);
				*spec++ = 255 - rb * s / 256 - (255-v);
			}
		}
	});
}

void DialogColorPicker::OnChangeMode(wxCommandEvent &) {
	spectrum_dirty = true;
	OPT_SET("Tool/Colour Picker/Mode")->SetInt(colorspace_choice->GetSelection());
	UpdateSpectrumDisplay();
}

void DialogColorPicker::OnSpectrumChange(wxCommandEvent &) {
	int i = colorspace_choice->GetSelection();
	switch (i) {
		case 0: case 1: case 2:
			change_value(rgb_input[2 - (i == 2)], spectrum->GetX());
			change_value(rgb_input[i == 0], spectrum->GetY());
			break;
		case 3:
			change_value(hsl_input[1], spectrum->GetX());
			change_value(hsl_input[0], spectrum->GetY());
			break;
		case 4:
			change_value(hsv_input[1], spectrum->GetX());
			change_value(hsv_input[2], spectrum->GetY());
			break;
	}

	switch (i) {
		case 0: case 1: case 2:
			UpdateFromRGB(false);
			break;
		case 3:
			UpdateFromHSL(false);
			break;
		case 4:
			UpdateFromHSV(false);
			break;
	}
}

void DialogColorPicker::OnSliderChange(wxCommandEvent &) {
	spectrum_dirty = true;
	int i = colorspace_choice->GetSelection();
	switch (i) {
		case 0: case 1: case 2:
			change_value(rgb_input[i], slider->GetY());
			UpdateFromRGB(false);
			break;
		case 3:
			change_value(hsl_input[2], slider->GetY());
			UpdateFromHSL(false);
			break;
		case 4:
			change_value(hsv_input[0], slider->GetY());
			UpdateFromHSV(false);
			break;
	}
}

void DialogColorPicker::OnAlphaSliderChange(wxCommandEvent &) {
	change_value(alpha_input, alpha_slider->GetY());
	cur_color.a = alpha_slider->GetY();
	callback(cur_color);
}

void DialogColorPicker::OnRecentSelect(ValueEvent<agi::Color> &evt) {
	agi::Color new_color = evt.Get();
	new_color.a = cur_color.a;
	SetColor(new_color);
}

void DialogColorPicker::OnDropperMouse(wxMouseEvent &evt) {
	if (evt.LeftDown() && !screen_dropper_icon->HasCapture()) {
#ifdef WIN32
		screen_dropper_icon->SetCursor(wxCursor("eyedropper_cursor"));
#else
		screen_dropper_icon->SetCursor(*wxCROSS_CURSOR);
#endif
		screen_dropper_icon->SetBitmap(wxNullBitmap);
		screen_dropper_icon->CaptureMouse();
		eyedropper_grab_point = evt.GetPosition();
		eyedropper_is_grabbed = false;
	}

	if (evt.LeftUp()) {
		wxPoint ptdiff = evt.GetPosition() - eyedropper_grab_point;
		bool release_now = eyedropper_is_grabbed || abs(ptdiff.x) + abs(ptdiff.y) > 7;
		if (release_now) {
			screen_dropper_icon->ReleaseMouse();
			eyedropper_is_grabbed = false;
			screen_dropper_icon->SetCursor(wxNullCursor);
			screen_dropper_icon->SetBitmap(eyedropper_bitmap);
		}
		else
			eyedropper_is_grabbed = true;
	}

	if (screen_dropper_icon->HasCapture()) {
		wxPoint scrpos = screen_dropper_icon->ClientToScreen(evt.GetPosition());
		screen_dropper->DropFromScreenXY(scrpos.x, scrpos.y);
	}
}

/// @brief Hack to redirect events to the screen dropper icon
void DialogColorPicker::OnMouse(wxMouseEvent &evt) {
	if (!screen_dropper_icon->HasCapture()) {
		evt.Skip();
		return;
	}

	wxPoint dropper_pos = screen_dropper_icon->ScreenToClient(ClientToScreen(evt.GetPosition()));
	evt.m_x = dropper_pos.x;
	evt.m_y = dropper_pos.y;
	screen_dropper_icon->GetEventHandler()->ProcessEvent(evt);
}

void DialogColorPicker::OnCaptureLost(wxMouseCaptureLostEvent&) {
	eyedropper_is_grabbed = false;
	screen_dropper_icon->SetCursor(wxNullCursor);
	screen_dropper_icon->SetBitmap(eyedropper_bitmap);
}

}

bool GetColorFromUser(wxWindow* parent, agi::Color original, bool alpha, std::function<void (agi::Color)> callback) {
	DialogColorPicker dialog(parent, original, callback, alpha);
	bool ok = dialog.ShowModal() == wxID_OK;
	if (!ok)
		callback(original);
	else
		dialog.AddColorToRecent();
	return ok;
}
