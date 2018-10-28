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

/// @file audio_karaoke.cpp
/// @brief Karaoke table UI in audio box (not in audio display)
/// @ingroup audio_ui
///

#include "audio_karaoke.h"

#include "include/aegisub/context.h"

#include "ass_dialogue.h"
#include "ass_file.h"
#include "ass_karaoke.h"
#include "audio_box.h"
#include "audio_controller.h"
#include "audio_timing.h"
#include "compat.h"
#include "libresrc/libresrc.h"
#include "options.h"
#include "project.h"
#include "selection_controller.h"
#include "utils.h"

#include <libaegisub/make_unique.h>

#include <algorithm>
#include <boost/locale/boundary.hpp>
#include <wx/bmpbuttn.h>
#include <wx/button.h>
#include <wx/dcclient.h>
#include <wx/dcmemory.h>
#include <wx/menu.h>
#include <wx/panel.h>
#include <wx/settings.h>
#include <wx/sizer.h>

template<class Container, class Value>
static inline size_t last_lt_or_eq(Container const& c, Value const& v) {
	auto it = lower_bound(c.begin(), c.end(), v);
	// lower_bound gives first >=
	if (it == c.end() || *it > v)
		--it;
	return distance(c.begin(), it);
}

AudioKaraoke::AudioKaraoke(wxWindow *parent, agi::Context *c)
: wxWindow(parent, -1, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxBORDER_SUNKEN)
, c(c)
, file_changed(c->ass->AddCommitListener(&AudioKaraoke::OnFileChanged, this))
, audio_opened(c->project->AddAudioProviderListener(&AudioKaraoke::OnAudioOpened, this))
, active_line_changed(c->selectionController->AddActiveLineListener(&AudioKaraoke::OnActiveLineChanged, this))
, tap_to_time_toggled(OPT_SUB("Timing/Tap To Time", &AudioKaraoke::OnTapMarkerChanged, this))
, kara(agi::make_unique<AssKaraoke>())
{
	using std::bind;

	cancel_button = new wxBitmapButton(this, -1, GETIMAGE(kara_split_cancel_16));
	cancel_button->SetToolTip(_("Discard all uncommitted splits"));
	cancel_button->Bind(wxEVT_BUTTON, bind(&AudioKaraoke::CancelSplit, this));

	accept_button = new wxBitmapButton(this, -1, GETIMAGE(kara_split_accept_16));
	accept_button->SetToolTip(_("Commit splits"));
	accept_button->Bind(wxEVT_BUTTON, bind(&AudioKaraoke::AcceptSplit, this));

	split_area = new wxPanel(this);

	wxSizer *main_sizer = new wxBoxSizer(wxHORIZONTAL);
	main_sizer->Add(cancel_button);
	main_sizer->Add(accept_button);
	main_sizer->Add(split_area, wxSizerFlags(1).Expand());
	SetSizerAndFit(main_sizer);

	/// @todo subscribe
	split_font.SetFaceName(FontFace("Audio/Karaoke"));
	split_font.SetPointSize(OPT_GET("Audio/Karaoke/Font Size")->GetInt());

	split_area->Bind(wxEVT_SIZE, &AudioKaraoke::OnSize, this);
	split_area->Bind(wxEVT_PAINT, &AudioKaraoke::OnPaint, this);
	split_area->Bind(wxEVT_LEFT_DOWN, &AudioKaraoke::OnMouse, this);
	split_area->Bind(wxEVT_LEFT_UP, &AudioKaraoke::OnMouse, this);
	split_area->Bind(wxEVT_MOTION, &AudioKaraoke::OnMouse, this);
	split_area->Bind(wxEVT_LEAVE_WINDOW, &AudioKaraoke::OnMouse, this);
	split_area->Bind(wxEVT_CONTEXT_MENU, &AudioKaraoke::OnContextMenu, this);
	scroll_timer.Bind(wxEVT_TIMER, &AudioKaraoke::OnScrollTimer, this);

	accept_button->Enable(false);
	cancel_button->Enable(false);
	enabled = false;
}

AudioKaraoke::~AudioKaraoke() {
}

void AudioKaraoke::OnActiveLineChanged(AssDialogue *new_line) {
	active_line = new_line;
	if (enabled) {
		LoadFromLine();
		split_area->Refresh(false);
	}
}

void AudioKaraoke::OnFileChanged(int type, const AssDialogue *changed) {
	if (enabled && (type & AssFile::COMMIT_DIAG_FULL) && (!changed || changed == active_line)) {
		LoadFromLine();
		split_area->Refresh(false);
	}
}

void AudioKaraoke::OnAudioOpened(agi::AudioProvider *provider) {
	if (provider)
		SetEnabled(enabled);
	else
		c->audioController->SetTimingController(nullptr);
}

void AudioKaraoke::SetEnabled(bool en) {
	enabled = en;

	c->audioBox->ShowKaraokeBar(enabled);
	if (enabled) {
		LoadFromLine();
		c->audioController->SetTimingController(CreateKaraokeTimingController(c, kara.get(), file_changed));
		c->audioController->GetTimingController()->AddUpdatedTapMarkerListener(&AudioKaraoke::OnTapMarkerChanged, this);
		Refresh(false);
	}
	else {
		c->audioController->SetTimingController(CreateDialogueTimingController(c));
	}
}

void AudioKaraoke::OnSize(wxSizeEvent &evt) {
	RenderText();
	Refresh(false);
}

void AudioKaraoke::OnPaint(wxPaintEvent &) {
	int w, h;
	split_area->GetClientSize(&w, &h);

	wxPaintDC dc(split_area);
	wxMemoryDC bmp_dc(rendered_line);

	// Draw the text and split lines
	dc.Blit(-scroll_x, 0, rendered_line.GetWidth(), h, &bmp_dc, 0, 0);

	// Draw the split line under the mouse
	if (click_will_remove_split)
		dc.SetPen(*wxRED);
	else
		dc.SetPen(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT));
	dc.DrawLine(mouse_pos, 0, mouse_pos, h);

	dc.SetPen(*wxTRANSPARENT_PEN);

	int width_past_bmp = w + scroll_x - rendered_line.GetWidth();
	dc.SetBrush(*wxWHITE_BRUSH);
	if (width_past_bmp > 0)
		dc.DrawRectangle(w - width_past_bmp, 0, width_past_bmp, h);

	// Draw scroll arrows if needed
	if (scroll_x > 0) {
		dc.DrawRectangle(0, 0, 20, h);

		wxPoint triangle[] = {
			wxPoint(10, h / 2 - 6),
			wxPoint(4, h / 2),
			wxPoint(10, h / 2 + 6)
		};
		dc.SetBrush(*wxBLACK_BRUSH);
		dc.DrawPolygon(3, triangle);
	}

	if (rendered_line.GetWidth() - scroll_x > w) {
		dc.SetBrush(*wxWHITE_BRUSH);
		dc.DrawRectangle(w - 20, 0, 20, h);

		wxPoint triangle[] = {
			wxPoint(w - 10, h / 2 - 6),
			wxPoint(w - 4, h / 2),
			wxPoint(w - 10, h / 2 + 6)
		};
		dc.SetBrush(*wxBLACK_BRUSH);
		dc.DrawPolygon(3, triangle);
	}
}

void AudioKaraoke::RenderText() {
	wxSize bmp_size = split_area->GetClientSize();
	int line_width = spaced_text.size() * char_width + 5;
	if (line_width > bmp_size.GetWidth())
		bmp_size.SetWidth(line_width);

	if (!rendered_line.IsOk() || bmp_size != rendered_line.GetSize())
		rendered_line = wxBitmap(bmp_size);

	wxMemoryDC dc(rendered_line);

	// Draw background
	dc.SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)));
	dc.SetPen(*wxTRANSPARENT_PEN);
	dc.DrawRectangle(wxPoint(), bmp_size);

	dc.SetFont(split_font);
	dc.SetTextForeground(wxColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT)));

	// Draw each character in the line
	int y = (bmp_size.GetHeight() - char_height) / 2;
	for (size_t i = 0; i < spaced_text.size(); ++i) {
		if (!(tap_syl_start <= i && i < tap_syl_end)) {
			// Only draw with normal color if _not_ the tap syllable
			dc.DrawText(spaced_text[i], char_x[i], y);
		}
	}

	// Draw marked syllable
	dc.SetTextForeground(*wxGREEN);
	for (size_t i = tap_syl_start; i < tap_syl_end; ++i)
		dc.DrawText(spaced_text[i], char_x[i], y);

	// Draw the lines between each syllable
	dc.SetPen(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT));
	for (auto syl_line : syl_lines)
		dc.DrawLine(syl_line, 0, syl_line, bmp_size.GetHeight());
}

void AudioKaraoke::AddMenuItem(wxMenu &menu, std::string const& tag, wxString const& help, std::string const& selected) {
	wxMenuItem *item = menu.AppendCheckItem(-1, to_wx(tag), help);
	menu.Bind(wxEVT_MENU, std::bind(&AudioKaraoke::SetTagType, this, tag), item->GetId());
	item->Check(tag == selected);
}

void AudioKaraoke::OnContextMenu(wxContextMenuEvent&) {
	if (!enabled) return;

	wxMenu context_menu(_("Karaoke tag"));
	std::string type = kara->GetTagType();

	AddMenuItem(context_menu, "\\k", _("Change karaoke tag to \\k"), type);
	AddMenuItem(context_menu, "\\kf", _("Change karaoke tag to \\kf"), type);
	AddMenuItem(context_menu, "\\ko", _("Change karaoke tag to \\ko"), type);

	PopupMenu(&context_menu);
}

void AudioKaraoke::OnMouse(wxMouseEvent &event) {
	if (!enabled) return;

	mouse_pos = event.GetX();

	if (event.Leaving()) {
		mouse_pos = -1;
		split_area->Refresh(false);
		return;
	}

	if (scroll_timer.IsRunning() || split_area->HasCapture()) {
		if (event.LeftUp()) {
			scroll_timer.Stop();
			split_area->ReleaseMouse();
		}
		return;
	}

	// Check if the mouse is over a scroll arrow
	int client_width = split_area->GetClientSize().GetWidth();
	if (scroll_x > 0 && mouse_pos < 20)
		scroll_dir = -1;
	else if (scroll_x + client_width < rendered_line.GetWidth() && mouse_pos > client_width - 20)
		scroll_dir = 1;
	else
		scroll_dir = 0;

	if (scroll_dir) {
		mouse_pos = -1;
		if (event.LeftDown()) {
			split_area->Refresh(false);
			scroll_timer.Start(50);
			split_area->CaptureMouse();
			wxTimerEvent evt;
			OnScrollTimer(evt);
		}
		return;
	}

	int shifted_pos = mouse_pos + scroll_x;

	// Character to insert the new split point before
	int split_pos = std::min<int>((shifted_pos + char_width / 2) / char_width, spaced_text.size());

	// Syllable this character is in
	int syl = last_lt_or_eq(syl_start_points, split_pos);

	// If the click is sufficiently close to a line of a syllable split,
	// remove that split rather than adding a new one
	bool click_right = (syl > 0 && shifted_pos <= syl_lines[syl - 1] + 3);
	bool click_left = (syl < (int)syl_lines.size() && shifted_pos >= syl_lines[syl] - 3);
	click_will_remove_split = click_left || click_right;

	if (!event.LeftDown()) {
		// Erase the old line and draw the new one
		split_area->Refresh(false);
		return;
	}

	if (click_will_remove_split)
		kara->RemoveSplit(syl + (click_left && !click_right));
	else
		kara->AddSplit(syl, char_to_byte[split_pos] - 1);

	SetDisplayText();
	accept_button->Enable(true);
	cancel_button->Enable(true);
	split_area->Refresh(false);
}

void AudioKaraoke::OnScrollTimer(wxTimerEvent &) {
	scroll_x += scroll_dir * char_width * 3;

	int max_scroll = rendered_line.GetWidth() + 20 - split_area->GetClientSize().GetWidth();
	if (scroll_x < 0 || scroll_x > max_scroll) {
		scroll_x = mid(0, scroll_x, max_scroll);
		scroll_timer.Stop();
	}

	split_area->Refresh(false);
}

void AudioKaraoke::OnTapMarkerChanged() {
	tap_syl_start = 0;
	tap_syl_end = 0;

	if (OPT_GET("Timing/Tap To Time")->GetBool() && kara->size() > 0) {
		const AudioTimingController *tc = c->audioController->GetTimingController();
		const size_t marker_idx = tc->GetTapMarkerIndex();
		if (marker_idx > 0) {
			tap_syl_start = syl_start_points[marker_idx - 1];
			tap_syl_end =
				(marker_idx < syl_start_points.size() ?
				syl_start_points[marker_idx] :
				spaced_text.size());
		}
	}

	RenderText();
	Refresh(false);
}

void AudioKaraoke::LoadFromLine() {
	scroll_x = 0;
	scroll_timer.Stop();
	kara->SetLine(active_line, true);
	SetDisplayText();
	accept_button->Enable(kara->GetText() != active_line->Text);
	cancel_button->Enable(false);
}

void AudioKaraoke::SetDisplayText() {
	using namespace boost::locale::boundary;

	wxMemoryDC dc;
	dc.SetFont(split_font);

	auto get_char_width = [&](std::string const& character) -> int {
		const auto it = char_widths.find(character);
		if (it != end(char_widths))
			return it->second;

		const auto size = dc.GetTextExtent(to_wx(character));
		char_height = std::max(char_height, size.GetHeight());
		char_widths[character] = size.GetWidth();
		return size.GetWidth();
	};

	char_width = get_char_width(" ");

	// Width in pixels of each character in this string
	std::vector<int> str_char_widths;

	spaced_text.clear();
	char_to_byte.clear();
	syl_start_points.clear();
	for (auto const& syl : *kara) {
		// The last (and only the last) syllable needs the width of the final
		// character in the syllable, so we unconditionally add it at the end
		// of this loop, then remove the extra ones here
		if (!char_to_byte.empty())
			char_to_byte.pop_back();

		syl_start_points.push_back(spaced_text.size());

		// Add a space between each syllable to avoid crowding
		spaced_text.emplace_back(wxS(" "));
		str_char_widths.push_back(char_width);
		char_to_byte.push_back(1);

		size_t syl_idx = 1;
		const ssegment_index characters(character, begin(syl.text), end(syl.text));
		for (auto chr : characters) {
			// Calculate the width in pixels of this character
			const std::string character = chr.str();
			const int width = get_char_width(character);
			char_width = std::max(char_width, width);
			str_char_widths.push_back(width);

			spaced_text.emplace_back(to_wx(character));
			char_to_byte.push_back(syl_idx);
			syl_idx += character.size();
		}

		char_to_byte.push_back(syl_idx);
	}

	// Center each character within the space available to it
	char_x.resize(str_char_widths.size());
	for (size_t i = 0; i < str_char_widths.size(); ++i)
		char_x[i] =  i * char_width + (char_width - str_char_widths[i]) / 2;

	// Calculate the positions of the syllable divider lines
	syl_lines.resize(syl_start_points.size() - 1);
	for (size_t i = 1; i < syl_start_points.size(); ++i)
		syl_lines[i - 1] = syl_start_points[i] * char_width + char_width / 2;

	RenderText();
}

void AudioKaraoke::CancelSplit() {
	LoadFromLine();
	split_area->Refresh(false);
}

void AudioKaraoke::AcceptSplit() {
	active_line->Text = kara->GetText();
	file_changed.Block();
	c->ass->Commit(_("karaoke split"), AssFile::COMMIT_DIAG_TEXT);
	file_changed.Unblock();

	accept_button->Enable(false);
	cancel_button->Enable(false);
}

void AudioKaraoke::SetTagType(std::string const& new_tag) {
	kara->SetTagType(new_tag);
	AcceptSplit();
}
