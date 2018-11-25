// Copyright (c) 2005, Rodrigo Braz Monteiro
// Copyright (c) 2009-2010, Niels Martin Hansen
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

#include "audio_display.h"

#include "audio_controller.h"
#include "audio_renderer.h"
#include "audio_renderer_spectrum.h"
#include "audio_renderer_waveform.h"
#include "audio_timing.h"
#include "compat.h"
#include "format.h"
#include "include/aegisub/context.h"
#include "include/aegisub/hotkey.h"
#include "options.h"
#include "project.h"
#include "utils.h"
#include "video_controller.h"

#include <libaegisub/ass/time.h>
#include <libaegisub/audio/provider.h>
#include <libaegisub/make_unique.h>

#include <algorithm>

#include <wx/dcbuffer.h>
#include <wx/mousestate.h>

/// @class AudioDisplayInteractionObject
/// @brief Interface for objects on the audio display that can respond to mouse events
class AudioDisplayInteractionObject {
public:
	/// @brief The user is interacting with the object using the mouse
	/// @param event Mouse event data
	/// @return True to take mouse capture, false to release mouse capture
	///
	/// Assuming no object has the mouse capture, the audio display uses other methods
	/// in the object implementing this interface to determine whether a mouse event
	/// should go to the object. If the mouse event goes to the object, this method
	/// is called.
	///
	/// If this method returns true, the audio display takes the mouse capture and
	/// stores a pointer to the AudioDisplayInteractionObject interface for the object
	/// and redirects the next mouse event to that object.
	///
	/// If the object that has the mouse capture returns false from this method, the
	/// capture is released and regular processing is done for the next event.
	///
	/// If the object does not have mouse capture and returns false from this method,
	/// no capture is taken or released and regular processing is done for the next
	/// mouse event.
	virtual bool OnMouseEvent(wxMouseEvent &event) = 0;

	/// @brief Destructor
	///
	/// Empty virtual destructor for the cases that need it.
	virtual ~AudioDisplayInteractionObject() = default;
};

namespace {
/// @brief Colourscheme-based UI colour provider
///
/// This class provides UI colours corresponding to the supplied audio colour
/// scheme.
///
/// SetColourScheme must be called to set the active colour scheme before
/// colours can be retrieved
class UIColours {
	wxColour light_colour;         ///< Light unfocused colour from the colour scheme
	wxColour dark_colour;          ///< Dark unfocused colour from the colour scheme
	wxColour sel_colour;           ///< Selection unfocused colour from the colour scheme
	wxColour light_focused_colour; ///< Light focused colour from the colour scheme
	wxColour dark_focused_colour;  ///< Dark focused colour from the colour scheme
	wxColour sel_focused_colour;   ///< Selection focused colour from the colour scheme

	bool focused = false; ///< Use the focused colours?
public:
	/// Set the colour scheme to load colours from
	/// @param name Name of the colour scheme
	void SetColourScheme(std::string const& name)
	{
		std::string opt_prefix = "Colour/Schemes/" + name + "/UI/";
		light_colour = to_wx(OPT_GET(opt_prefix + "Light")->GetColor());
		dark_colour = to_wx(OPT_GET(opt_prefix + "Dark")->GetColor());
		sel_colour = to_wx(OPT_GET(opt_prefix + "Selection")->GetColor());

		opt_prefix = "Colour/Schemes/" + name + "/UI Focused/";
		light_focused_colour = to_wx(OPT_GET(opt_prefix + "Light")->GetColor());
		dark_focused_colour = to_wx(OPT_GET(opt_prefix + "Dark")->GetColor());
		sel_focused_colour = to_wx(OPT_GET(opt_prefix + "Selection")->GetColor());
	}

	/// Set whether to use the focused or unfocused colours
	/// @param focused If true, focused colours will be returned
	void SetFocused(bool focused) { this->focused = focused; }

	/// Get the current Light colour
	wxColour Light() const { return focused ? light_focused_colour : light_colour; }
	/// Get the current Dark colour
	wxColour Dark() const { return focused ? dark_focused_colour : dark_colour; }
	/// Get the current Selection colour
	wxColour Selection() const { return focused ? sel_focused_colour : sel_colour; }
};

class AudioDisplayScrollbar final : public AudioDisplayInteractionObject {
	static const int height = 15;
	static const int min_width = 10;

	wxRect bounds;
	wxRect thumb;

	bool dragging = false;   ///< user is dragging with the primary mouse button

	int data_length = 1; ///< total amount of data in control
	int page_length = 1; ///< amount of data in one page
	int position    = 0; ///< first item displayed

	int sel_start  = -1; ///< first data item in selection
	int sel_length = 0;  ///< number of data items in selection

	UIColours colours; ///< Colour provider

	/// Containing display to send scroll events to
	AudioDisplay *display;

	// Recalculate thumb bounds from position and length data
	void RecalculateThumb()
	{
		thumb.width = std::max<int>(min_width, (int64_t)bounds.width * page_length / data_length);
		thumb.height = height;
		thumb.x = int((int64_t)bounds.width * position / data_length);
		thumb.y = bounds.y;
	}

public:
	AudioDisplayScrollbar(AudioDisplay *display)
	: display(display)
	{
	}

	/// The audio display has changed size
	void SetDisplaySize(const wxSize &display_size)
	{
		bounds.x = 0;
		bounds.y = display_size.y - height;
		bounds.width = display_size.x;
		bounds.height = height;
		page_length = display_size.x;

		RecalculateThumb();
	}

	void SetColourScheme(std::string const& name)
	{
		colours.SetColourScheme(name);
	}

	const wxRect & GetBounds() const { return bounds; }
	int GetPosition() const { return position; }

	int SetPosition(int new_position)
	{
		// These two conditionals can't be swapped, otherwise the position can become
		// negative if the entire data is shorter than one page.
		if (new_position + page_length >= data_length)
			new_position = data_length - page_length - 1;
		if (new_position < 0)
			new_position = 0;

		position = new_position;
		RecalculateThumb();

		return position;
	}

	void SetSelection(int new_start, int new_length)
	{
		sel_start = (int64_t)new_start * bounds.width / data_length;
		sel_length = (int64_t)new_length * bounds.width / data_length;
	}

	void ChangeLengths(int new_data_length, int new_page_length)
	{
		data_length = new_data_length;
		page_length = new_page_length;

		RecalculateThumb();
	}

	bool OnMouseEvent(wxMouseEvent &event) override
	{
		if (event.LeftIsDown())
		{
			const int thumb_left = event.GetPosition().x - thumb.width/2;
			const int data_length_less_page = data_length - page_length;
			const int shaft_length_less_thumb = bounds.width - thumb.width;

			display->ScrollPixelToLeft((int64_t)data_length_less_page * thumb_left / shaft_length_less_thumb);

			dragging = true;
		}
		else if (event.LeftUp())
		{
			dragging = false;
		}

		return dragging;
	}

	void Paint(wxDC &dc, bool has_focus, int load_progress)
	{
		colours.SetFocused(has_focus);

		dc.SetPen(wxPen(colours.Light()));
		dc.SetBrush(wxBrush(colours.Dark()));
		dc.DrawRectangle(bounds);

		if (sel_length > 0 && sel_start >= 0)
		{
			dc.SetPen(wxPen(colours.Selection()));
			dc.SetBrush(wxBrush(colours.Selection()));
			dc.DrawRectangle(wxRect(sel_start, bounds.y, sel_length, bounds.height));
		}

		dc.SetPen(wxPen(colours.Light()));
		dc.SetBrush(*wxTRANSPARENT_BRUSH);
		dc.DrawRectangle(bounds);

		if (load_progress > 0 && load_progress < data_length)
		{
			wxRect marker(
				(int64_t)bounds.width * load_progress / data_length - 25, bounds.y + 1,
				25, bounds.height - 2);
			dc.GradientFillLinear(marker, colours.Dark(), colours.Light());
		}

		dc.SetPen(wxPen(colours.Light()));
		dc.SetBrush(wxBrush(colours.Light()));
		dc.DrawRectangle(thumb);
	}
};

const int AudioDisplayScrollbar::min_width;

class AudioDisplayTimeline final : public AudioDisplayInteractionObject {
	int duration = 0;          ///< Total duration in ms
	double ms_per_pixel = 1.0; ///< Milliseconds per pixel
	int pixel_left = 0;        ///< Leftmost visible pixel (i.e. scroll position)

	wxRect bounds;

	wxPoint drag_lastpos;
	bool dragging = false;

	enum Scale {
		Sc_Millisecond,
		Sc_Centisecond,
		Sc_Decisecond,
		Sc_Second,
		Sc_Decasecond,
		Sc_Minute,
		Sc_Decaminute,
		Sc_Hour,
		Sc_Decahour, // If anyone needs this they should reconsider their project
		Sc_MAX = Sc_Decahour
	};
	Scale scale_minor;
	int scale_major_modulo; ///< If minor_scale_mark_index % scale_major_modulo == 0 the mark is a major mark
	double scale_minor_divisor; ///< Absolute scale-mark index multiplied by this number gives sample index for scale mark

	AudioDisplay *display; ///< Containing audio display

	UIColours colours; ///< Colour provider

public:
	AudioDisplayTimeline(AudioDisplay *display)
	: display(display)
	{
		int width, height;
		display->GetTextExtent("0123456789:.", &width, &height);
		bounds.height = height + 4;
	}

	void SetColourScheme(std::string const& name)
	{
		colours.SetColourScheme(name);
	}

	void SetDisplaySize(const wxSize &display_size)
	{
		// The size is without anything that goes below the timeline (like scrollbar)
		bounds.width = display_size.x;
		bounds.x = 0;
		bounds.y = 0;
	}

	int GetHeight() const { return bounds.height; }
	const wxRect & GetBounds() const { return bounds; }

	void ChangeAudio(int new_duration)
	{
		duration = new_duration;
	}

	void ChangeZoom(double new_ms_per_pixel)
	{
		ms_per_pixel = new_ms_per_pixel;

		double px_sec = 1000.0 / ms_per_pixel;

		if (px_sec > 3000) {
			scale_minor = Sc_Millisecond;
			scale_minor_divisor = 1.0;
			scale_major_modulo = 10;
		} else if (px_sec > 300) {
			scale_minor = Sc_Centisecond;
			scale_minor_divisor = 10.0;
			scale_major_modulo = 10;
		} else if (px_sec > 30) {
			scale_minor = Sc_Decisecond;
			scale_minor_divisor = 100.0;
			scale_major_modulo = 10;
		} else if (px_sec > 3) {
			scale_minor = Sc_Second;
			scale_minor_divisor = 1000.0;
			scale_major_modulo = 10;
		} else if (px_sec > 1.0/3.0) {
			scale_minor = Sc_Decasecond;
			scale_minor_divisor = 10000.0;
			scale_major_modulo = 6;
		} else if (px_sec > 1.0/9.0) {
			scale_minor = Sc_Minute;
			scale_minor_divisor = 60000.0;
			scale_major_modulo = 10;
		} else if (px_sec > 1.0/90.0) {
			scale_minor = Sc_Decaminute;
			scale_minor_divisor = 600000.0;
			scale_major_modulo = 6;
		} else {
			scale_minor = Sc_Hour;
			scale_minor_divisor = 3600000.0;
			scale_major_modulo = 10;
		}
	}

	void SetPosition(int new_pixel_left)
	{
		pixel_left = std::max(new_pixel_left, 0);
	}

	bool OnMouseEvent(wxMouseEvent &event) override
	{
		if (event.LeftDown())
		{
			drag_lastpos = event.GetPosition();
			dragging = true;
		}
		else if (event.LeftIsDown())
		{
			display->ScrollPixelToLeft(pixel_left - event.GetPosition().x + drag_lastpos.x);

			drag_lastpos = event.GetPosition();
			dragging = true;
		}
		else if (event.LeftUp())
		{
			dragging = false;
		}

		return dragging;
	}

	void Paint(wxDC &dc)
	{
		int bottom = bounds.y + bounds.height;

		// Background
		dc.SetPen(wxPen(colours.Dark()));
		dc.SetBrush(wxBrush(colours.Dark()));
		dc.DrawRectangle(bounds);

		// Top line
		dc.SetPen(wxPen(colours.Light()));
		dc.DrawLine(bounds.x, bottom-1, bounds.x+bounds.width, bottom-1);

		// Prepare for writing text
		dc.SetTextBackground(colours.Dark());
		dc.SetTextForeground(colours.Light());

		// Figure out the first scale mark to show
		int ms_left = int(pixel_left * ms_per_pixel);
		int next_scale_mark = int(ms_left / scale_minor_divisor);
		if (next_scale_mark * scale_minor_divisor < ms_left)
			next_scale_mark += 1;
		assert(next_scale_mark * scale_minor_divisor >= ms_left);

		// Draw scale marks
		int next_scale_mark_pos;
		int last_text_right = -1;
		int last_hour = -1, last_minute = -1;
		if (duration < 3600) last_hour = 0; // Trick to only show hours if audio is longer than 1 hour
		do {
			next_scale_mark_pos = int(next_scale_mark * scale_minor_divisor / ms_per_pixel) - pixel_left;
			bool mark_is_major = next_scale_mark % scale_major_modulo == 0;

			if (mark_is_major)
				dc.DrawLine(next_scale_mark_pos, bottom-6, next_scale_mark_pos, bottom-1);
			else
				dc.DrawLine(next_scale_mark_pos, bottom-4, next_scale_mark_pos, bottom-1);

			// Print time labels on major scale marks
			if (mark_is_major && next_scale_mark_pos > last_text_right)
			{
				double mark_time = next_scale_mark * scale_minor_divisor / 1000.0;
				int mark_hour = (int)(mark_time / 3600);
				int mark_minute = (int)(mark_time / 60) % 60;
				double mark_second = mark_time - mark_hour*3600.0 - mark_minute*60.0;

				wxString time_string;
				bool changed_hour = mark_hour != last_hour;
				bool changed_minute = mark_minute != last_minute;

				if (changed_hour)
				{
					time_string = fmt_wx("%d:%02d:", mark_hour, mark_minute);
					last_hour = mark_hour;
					last_minute = mark_minute;
				}
				else if (changed_minute)
				{
					time_string = fmt_wx("%d:", mark_minute);
					last_minute = mark_minute;
				}
				if (scale_minor >= Sc_Decisecond)
					time_string += fmt_wx("%02d", mark_second);
				else if (scale_minor == Sc_Centisecond)
					time_string += fmt_wx("%02.1f", mark_second);
				else
					time_string += fmt_wx("%02.2f", mark_second);

				int tw, th;
				dc.GetTextExtent(time_string, &tw, &th);
				last_text_right = next_scale_mark_pos + tw;

				dc.DrawText(time_string, next_scale_mark_pos, 0);
			}

			next_scale_mark += 1;

		} while (next_scale_mark_pos < bounds.width);
	}
};

class AudioStyleRangeMerger final : public AudioRenderingStyleRanges {
	typedef std::map<int, AudioRenderingStyle> style_map;
public:
	typedef style_map::iterator iterator;

private:
	style_map points;

	void Split(int point)
	{
		auto it = points.lower_bound(point);
		if (it == points.end() || it->first != point)
		{
			assert(it != points.begin());
			points[point] = (--it)->second;
		}
	}

	void Restyle(int start, int end, AudioRenderingStyle style)
	{
		assert(points.lower_bound(end) != points.end());
		for (auto pt = points.lower_bound(start); pt->first < end; ++pt)
		{
			if (style > pt->second)
				pt->second = style;
		}
	}

public:
	AudioStyleRangeMerger()
	{
		points[0] = AudioStyle_Normal;
	}

	void AddRange(int start, int end, AudioRenderingStyle style) override
	{

		if (start < 0) start = 0;
		if (end < start) return;

		Split(start);
		Split(end);
		Restyle(start, end, style);
	}

	iterator begin() { return points.begin(); }
	iterator end() { return points.end(); }
};

}

class AudioMarkerInteractionObject final : public AudioDisplayInteractionObject {
	// Object-pair being interacted with
	std::vector<AudioMarker*> markers;
	AudioTimingController *timing_controller;
	// Audio display drag is happening on
	AudioDisplay *display;
	// Mouse button used to initiate the drag
	wxMouseButton button_used;
	// Default to snapping to snappable markers
	bool default_snap = OPT_GET("Audio/Snap/Enable")->GetBool();
	// Range in pixels to snap at
	int snap_range = OPT_GET("Audio/Snap/Distance")->GetInt();

public:
	AudioMarkerInteractionObject(std::vector<AudioMarker*> markers, AudioTimingController *timing_controller, AudioDisplay *display, wxMouseButton button_used)
	: markers(std::move(markers))
	, timing_controller(timing_controller)
	, display(display)
	, button_used(button_used)
	{
	}

	bool OnMouseEvent(wxMouseEvent &event) override
	{
		if (event.Dragging())
		{
			timing_controller->OnMarkerDrag(
				markers,
				display->TimeFromRelativeX(event.GetPosition().x),
				default_snap != event.ShiftDown() ? display->TimeFromAbsoluteX(snap_range) : 0);
		}

		// We lose the marker drag if the button used to initiate it goes up
		return !event.ButtonUp(button_used);
	}

	/// Get the position in milliseconds of this group of markers
	int GetPosition() const { return markers.front()->GetPosition(); }
};

AudioDisplay::AudioDisplay(wxWindow *parent, AudioController *controller, agi::Context *context)
: wxWindow(parent, -1, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS|wxBORDER_SIMPLE)
, audio_open_connection(context->project->AddAudioProviderListener(&AudioDisplay::OnAudioOpen, this))
, context(context)
, audio_renderer(agi::make_unique<AudioRenderer>())
, controller(controller)
, scrollbar(agi::make_unique<AudioDisplayScrollbar>(this))
, timeline(agi::make_unique<AudioDisplayTimeline>(this))
, style_ranges({{0, 0}})
{
	audio_renderer->SetAmplitudeScale(scale_amplitude);
	SetZoomLevel(0);

	SetMinClientSize(wxSize(-1, 70));
	SetBackgroundStyle(wxBG_STYLE_PAINT);
	SetThemeEnabled(false);

	Bind(wxEVT_LEFT_DOWN, &AudioDisplay::OnMouseEvent, this);
	Bind(wxEVT_MIDDLE_DOWN, &AudioDisplay::OnMouseEvent, this);
	Bind(wxEVT_RIGHT_DOWN, &AudioDisplay::OnMouseEvent, this);
	Bind(wxEVT_LEFT_UP, &AudioDisplay::OnMouseEvent, this);
	Bind(wxEVT_MIDDLE_UP, &AudioDisplay::OnMouseEvent, this);
	Bind(wxEVT_RIGHT_UP, &AudioDisplay::OnMouseEvent, this);
	Bind(wxEVT_MOTION, &AudioDisplay::OnMouseEvent, this);
	Bind(wxEVT_ENTER_WINDOW, &AudioDisplay::OnMouseEnter, this);
	Bind(wxEVT_LEAVE_WINDOW, &AudioDisplay::OnMouseLeave, this);
	Bind(wxEVT_PAINT, &AudioDisplay::OnPaint, this);
	Bind(wxEVT_SIZE, &AudioDisplay::OnSize, this);
	Bind(wxEVT_KILL_FOCUS, &AudioDisplay::OnFocus, this);
	Bind(wxEVT_SET_FOCUS, &AudioDisplay::OnFocus, this);
	Bind(wxEVT_CHAR_HOOK, &AudioDisplay::OnKeyDown, this);
	Bind(wxEVT_KEY_DOWN, &AudioDisplay::OnKeyDown, this);
	scroll_timer.Bind(wxEVT_TIMER, &AudioDisplay::OnScrollTimer, this);
	load_timer.Bind(wxEVT_TIMER, &AudioDisplay::OnLoadTimer, this);
}

AudioDisplay::~AudioDisplay()
{
}

void AudioDisplay::ScrollBy(int pixel_amount)
{
	ScrollPixelToLeft(scroll_left + pixel_amount);
}

void AudioDisplay::ScrollPixelToLeft(int pixel_position)
{
	const int client_width = GetClientRect().GetWidth();

	if (pixel_position + client_width >= pixel_audio_width)
		pixel_position = pixel_audio_width - client_width;
	if (pixel_position < 0)
		pixel_position = 0;

	scroll_left = pixel_position;
	scrollbar->SetPosition(scroll_left);
	timeline->SetPosition(scroll_left);
	Refresh();
}

void AudioDisplay::ScrollTimeRangeInView(const TimeRange &range)
{
	int client_width = GetClientRect().GetWidth();
	int range_begin = AbsoluteXFromTime(range.begin());
	int range_end = AbsoluteXFromTime(range.end());
	int range_len = range_end - range_begin;

	// Remove 5 % from each side of the client area.
	int leftadjust = client_width / 20;
	int client_left = scroll_left + leftadjust;
	client_width = client_width * 9 / 10;

	// Is everything already in view?
	if (range_begin >= client_left && range_end <= client_left+client_width)
		return;

	// The entire range can fit inside the view, center it
	if (range_len < client_width)
	{
		ScrollPixelToLeft(range_begin - (client_width-range_len)/2 - leftadjust);
	}

	// Range doesn't fit in view and we're viewing a middle part of it, just leave it alone
	else if (range_begin < client_left && range_end > client_left+client_width)
	{
		// nothing
	}

	// Right edge is in view, scroll it as far to the right as possible
	else if (range_end >= client_left && range_end < client_left+client_width)
	{
		ScrollPixelToLeft(range_end - client_width - leftadjust);
	}

	// Nothing is in view or the left edge is in view, scroll left edge as far to the left as possible
	else
	{
		ScrollPixelToLeft(range_begin - leftadjust);
	}
}

void AudioDisplay::SetZoomLevel(int new_zoom_level)
{
	zoom_level = new_zoom_level;

	const int factor = GetZoomLevelFactor(zoom_level);
	const int base_pixels_per_second = 50; /// @todo Make this customisable
	const double base_ms_per_pixel = 1000.0 / base_pixels_per_second;
	const double new_ms_per_pixel = 100.0 * base_ms_per_pixel / factor;

	if (ms_per_pixel == new_ms_per_pixel) return;

	int client_width = GetClientSize().GetWidth();
	double cursor_pos = track_cursor_pos >= 0 ? track_cursor_pos - scroll_left : client_width / 2.0;
	double cursor_time = (scroll_left + cursor_pos) * ms_per_pixel;

	ms_per_pixel = new_ms_per_pixel;
	pixel_audio_width = std::max(1, int(GetDuration() / ms_per_pixel));

	audio_renderer->SetMillisecondsPerPixel(ms_per_pixel);
	scrollbar->ChangeLengths(pixel_audio_width, client_width);
	timeline->ChangeZoom(ms_per_pixel);

	ScrollPixelToLeft(AbsoluteXFromTime(cursor_time) - cursor_pos);
	if (track_cursor_pos >= 0)
		track_cursor_pos = AbsoluteXFromTime(cursor_time);
	Refresh();
}

wxString AudioDisplay::GetZoomLevelDescription(int level) const
{
	const int factor = GetZoomLevelFactor(level);
	const int base_pixels_per_second = 50; /// @todo Make this customisable along with the above
	const int second_pixels = 100 * base_pixels_per_second / factor;

	return fmt_tl("%d%%, %d pixel/second", factor, second_pixels);
}

int AudioDisplay::GetZoomLevelFactor(int level)
{
	int factor = 100;

	if (level > 0)
	{
		factor += 25 * level;
	}
	else if (level < 0)
	{
		if (level >= -5)
			factor += 10 * level;
		else if (level >= -11)
			factor = 50 + (level+5) * 5;
		else
			factor = 20 + level + 11;
		if (factor <= 0)
			factor = 1;
	}

	return factor;
}

void AudioDisplay::SetAmplitudeScale(float scale)
{
	audio_renderer->SetAmplitudeScale(scale);
	Refresh();
}

void AudioDisplay::ReloadRenderingSettings()
{
	std::string colour_scheme_name;

	if (OPT_GET("Audio/Spectrum")->GetBool())
	{
		colour_scheme_name = OPT_GET("Colour/Audio Display/Spectrum")->GetString();
		auto audio_spectrum_renderer = agi::make_unique<AudioSpectrumRenderer>(colour_scheme_name);

		int64_t spectrum_quality = OPT_GET("Audio/Renderer/Spectrum/Quality")->GetInt();
#ifdef WITH_FFTW3
		// FFTW is so fast we can afford to upgrade quality by two levels
		spectrum_quality += 2;
#endif
		spectrum_quality = mid<int64_t>(0, spectrum_quality, 5);

		// Quality indexes:        0  1  2  3   4   5
		int spectrum_width[]    = {8, 9, 9, 9, 10, 11};
		int spectrum_distance[] = {8, 8, 7, 6,  6,  5};

		audio_spectrum_renderer->SetResolution(
			spectrum_width[spectrum_quality],
			spectrum_distance[spectrum_quality]);

		audio_renderer_provider = std::move(audio_spectrum_renderer);
	}
	else
	{
		colour_scheme_name = OPT_GET("Colour/Audio Display/Waveform")->GetString();
		audio_renderer_provider = agi::make_unique<AudioWaveformRenderer>(colour_scheme_name);
	}

	audio_renderer->SetRenderer(audio_renderer_provider.get());
	scrollbar->SetColourScheme(colour_scheme_name);
	timeline->SetColourScheme(colour_scheme_name);

	Refresh();
}

void AudioDisplay::OnLoadTimer(wxTimerEvent&)
{
	using namespace std::chrono;
	if (provider)
	{
		const auto now = steady_clock::now();
		const auto elapsed = duration_cast<milliseconds>(now - audio_load_start_time).count();
		if (elapsed == 0) return;

		const int64_t new_decoded_count = provider->GetDecodedSamples();
		if (new_decoded_count != last_sample_decoded)
			audio_load_speed = (audio_load_speed + (double)new_decoded_count / elapsed) / 2;
		if (audio_load_speed == 0) return;

		int new_pos = AbsoluteXFromTime(elapsed * audio_load_speed * 1000.0 / provider->GetSampleRate());
		if (new_pos > audio_load_position)
			audio_load_position = new_pos;

		const double left = last_sample_decoded * 1000.0 / provider->GetSampleRate() / ms_per_pixel;
		const double right = new_decoded_count * 1000.0 / provider->GetSampleRate() / ms_per_pixel;

		if (left < scroll_left + pixel_audio_width && right >= scroll_left)
			Refresh();
		else
			RefreshRect(scrollbar->GetBounds());
		last_sample_decoded = new_decoded_count;
	}

	if (!provider || last_sample_decoded == provider->GetNumSamples()) {
		load_timer.Stop();
		audio_load_position = -1;
	}
}

void AudioDisplay::OnPaint(wxPaintEvent&)
{
	if (!audio_renderer_provider || !provider) return;

	wxAutoBufferedPaintDC dc(this);

	wxRect audio_bounds(0, audio_top, GetClientSize().GetWidth(), audio_height);
	bool redraw_scrollbar = false;
	bool redraw_timeline = false;

	for (wxRegionIterator region(GetUpdateRegion()); region; ++region)
	{
		wxRect updrect = region.GetRect();

		redraw_scrollbar |= scrollbar->GetBounds().Intersects(updrect);
		redraw_timeline |= timeline->GetBounds().Intersects(updrect);

		if (audio_bounds.Intersects(updrect))
		{
			TimeRange updtime(
				std::max(0, TimeFromRelativeX(updrect.x - foot_size)),
				std::max(0, TimeFromRelativeX(updrect.x + updrect.width + foot_size)));

			PaintAudio(dc, updtime, updrect);
			PaintMarkers(dc, updtime);
			PaintLabels(dc, updtime);
		}
	}

	if (track_cursor_pos >= 0)
		PaintTrackCursor(dc);

	if (redraw_scrollbar)
		scrollbar->Paint(dc, HasFocus(), audio_load_position);
	if (redraw_timeline)
		timeline->Paint(dc);
}

void AudioDisplay::PaintAudio(wxDC &dc, const TimeRange updtime, const wxRect updrect)
{
	auto pt = begin(style_ranges), pe = end(style_ranges);
	while (pt != pe && pt + 1 != pe && (pt + 1)->first < updtime.begin()) ++pt;

	while (pt != pe && pt->first < updtime.end())
	{
		const auto range_style = static_cast<AudioRenderingStyle>(pt->second);
		const int range_x1 = std::max(updrect.x, RelativeXFromTime(pt->first));
		int range_x2 = updrect.x + updrect.width;
		if (++pt != pe)
			range_x2 = std::min(range_x2, RelativeXFromTime(pt->first));

		if (range_x2 > range_x1)
			audio_renderer->Render(dc, wxPoint(range_x1, audio_top),
				range_x1 + scroll_left, range_x2 - range_x1, range_style);
	}
}

void AudioDisplay::PaintMarkers(wxDC &dc, TimeRange updtime)
{
	AudioMarkerVector markers;
	controller->GetTimingController()->GetMarkers(updtime, markers);
	if (markers.empty()) return;

	wxDCPenChanger pen_retainer(dc, wxPen());
	wxDCBrushChanger brush_retainer(dc, wxBrush());
	for (const auto marker : markers)
	{
		int marker_x = RelativeXFromTime(marker->GetPosition());

		dc.SetPen(marker->GetStyle());
		dc.DrawLine(marker_x, audio_top, marker_x, audio_top+audio_height);

		if (marker->GetFeet() == AudioMarker::Feet_None) continue;

		dc.SetBrush(wxBrush(marker->GetStyle().GetColour()));
		dc.SetPen(*wxTRANSPARENT_PEN);

		if (marker->GetFeet() & AudioMarker::Feet_Left)
			PaintFoot(dc, marker_x, -1);
		if (marker->GetFeet() & AudioMarker::Feet_Right)
			PaintFoot(dc, marker_x, 1);
	}


	if (OPT_GET("Timing/Tap To Time")->GetBool()) {
		dc.SetBrush(wxBrush(*wxGREEN));
		dc.SetPen(*wxTRANSPARENT_PEN);
		int marker_x = RelativeXFromTime(controller->GetTimingController()->GetTapMarkerPosition());
		PaintTapMarker(dc, marker_x);
	}
}

void AudioDisplay::PaintFoot(wxDC &dc, int marker_x, int dir)
{
	wxPoint foot_top[3] = { wxPoint(foot_size * dir, 0), wxPoint(0, 0), wxPoint(0, foot_size) };
	wxPoint foot_bot[3] = { wxPoint(foot_size * dir, 0), wxPoint(0, -foot_size), wxPoint(0, 0) };
	dc.DrawPolygon(3, foot_top, marker_x, audio_top);
	dc.DrawPolygon(3, foot_bot, marker_x, audio_top+audio_height);
}

void AudioDisplay::PaintTapMarker(wxDC &dc, int marker_x)
{
	wxPoint arrow[3] = { wxPoint(-foot_size * 2, 0), wxPoint(0, -foot_size * 2), wxPoint(foot_size * 2, 0) };
	dc.DrawPolygon(3, arrow, marker_x, audio_top+audio_height);
}

void AudioDisplay::PaintLabels(wxDC &dc, TimeRange updtime)
{
	std::vector<AudioLabelProvider::AudioLabel> labels;
	controller->GetTimingController()->GetLabels(updtime, labels);
	if (labels.empty()) return;

	wxDCFontChanger fc(dc);
	wxFont font = dc.GetFont();
	font.SetWeight(wxFONTWEIGHT_BOLD);
	fc.Set(font);
	dc.SetTextForeground(*wxWHITE);
	for (auto const& label : labels)
	{
		wxSize extent = dc.GetTextExtent(label.text);
		int left = RelativeXFromTime(label.range.begin());
		int width = AbsoluteXFromTime(label.range.length());

		// If it doesn't fit, truncate
		if (width < extent.GetWidth())
		{
			dc.SetClippingRegion(left, audio_top + 4, width, extent.GetHeight());
			dc.DrawText(label.text, left, audio_top + 4);
			dc.DestroyClippingRegion();
		}
		// Otherwise center in the range
		else
		{
			dc.DrawText(label.text, left + (width - extent.GetWidth()) / 2, audio_top + 4);
		}
	}
}

void AudioDisplay::PaintTrackCursor(wxDC &dc) {
	wxDCPenChanger penchanger(dc, wxPen(*wxWHITE));
	dc.DrawLine(track_cursor_pos-scroll_left, audio_top, track_cursor_pos-scroll_left, audio_top+audio_height);

	if (track_cursor_label.empty()) return;

	wxDCFontChanger fc(dc);
	wxFont font = dc.GetFont();
	wxString face_name = FontFace("Audio/Track Cursor");
	if (!face_name.empty())
		font.SetFaceName(face_name);
	font.SetWeight(wxFONTWEIGHT_BOLD);
	fc.Set(font);

	wxSize label_size(dc.GetTextExtent(track_cursor_label));
	wxPoint label_pos(track_cursor_pos - scroll_left - label_size.x/2, audio_top + 2);
	label_pos.x = mid(2, label_pos.x, GetClientSize().GetWidth() - label_size.x - 2);

	int old_bg_mode = dc.GetBackgroundMode();
	dc.SetBackgroundMode(wxTRANSPARENT);

	// Draw border
	dc.SetTextForeground(wxColour(64, 64, 64));
	dc.DrawText(track_cursor_label, label_pos.x+1, label_pos.y+1);
	dc.DrawText(track_cursor_label, label_pos.x+1, label_pos.y-1);
	dc.DrawText(track_cursor_label, label_pos.x-1, label_pos.y+1);
	dc.DrawText(track_cursor_label, label_pos.x-1, label_pos.y-1);

	// Draw fill
	dc.SetTextForeground(*wxWHITE);
	dc.DrawText(track_cursor_label, label_pos.x, label_pos.y);
	dc.SetBackgroundMode(old_bg_mode);

	label_pos.x -= 2;
	label_pos.y -= 2;
	label_size.IncBy(4, 4);
	// If the rendered text changes size we have to draw it an extra time to make sure the entire thing was drawn
	bool need_extra_redraw = track_cursor_label_rect.GetSize() != label_size;
	track_cursor_label_rect.SetPosition(label_pos);
	track_cursor_label_rect.SetSize(label_size);
	if (need_extra_redraw)
		RefreshRect(track_cursor_label_rect, false);
}

void AudioDisplay::SetDraggedObject(AudioDisplayInteractionObject *new_obj)
{
	dragged_object = new_obj;

	if (dragged_object && !HasCapture())
		CaptureMouse();
	else if (!dragged_object && HasCapture())
		ReleaseMouse();

	if (!dragged_object)
		audio_marker.reset();
}

void AudioDisplay::SetTrackCursor(int new_pos, bool show_time)
{
	if (new_pos == track_cursor_pos) return;

	int old_pos = track_cursor_pos;
	track_cursor_pos = new_pos;

	RefreshRect(wxRect(old_pos - scroll_left - 1, audio_top, 2, audio_height - 1), false);
	RefreshRect(wxRect(new_pos - scroll_left - 1, audio_top, 2, audio_height - 1), false);

	// Make sure the old label gets cleared away
	RefreshRect(track_cursor_label_rect, false);

	if (show_time)
	{
		agi::Time new_label_time = TimeFromAbsoluteX(track_cursor_pos);
		track_cursor_label = to_wx(new_label_time.GetAssFormatted());
		track_cursor_label_rect.x += new_pos - old_pos;
		RefreshRect(track_cursor_label_rect, false);
	}
	else
	{
		track_cursor_label_rect.SetSize(wxSize(0,0));
		track_cursor_label.Clear();
	}
}

void AudioDisplay::RemoveTrackCursor()
{
	SetTrackCursor(-1, false);
}

void AudioDisplay::OnMouseEnter(wxMouseEvent&)
{
	if (OPT_GET("Audio/Auto/Focus")->GetBool())
		SetFocus();
}

void AudioDisplay::OnMouseLeave(wxMouseEvent&)
{
	if (!controller->IsPlaying())
		RemoveTrackCursor();
}

void AudioDisplay::OnMouseEvent(wxMouseEvent& event)
{
	// If we have focus, we get mouse move events on Mac even when the mouse is
	// outside our client rectangle, we don't want those.
	if (event.Moving() && !GetClientRect().Contains(event.GetPosition()))
	{
		event.Skip();
		return;
	}

	if (event.IsButton())
		SetFocus();

	const int mouse_x = event.GetPosition().x;

	// Scroll the display after a mouse-up near one of the edges
	if ((event.LeftUp() || event.RightUp()) && OPT_GET("Audio/Auto/Scroll")->GetBool())
	{
		const int width = GetClientSize().GetWidth();
		if (mouse_x < width / 20) {
			ScrollBy(-width / 3);
		}
		else if (width - mouse_x < width / 20) {
			ScrollBy(width / 3);
		}
	}

	if (ForwardMouseEvent(event))
		return;

	if (event.MiddleIsDown())
	{
		context->videoController->JumpToTime(TimeFromRelativeX(mouse_x), agi::vfr::EXACT);
		return;
	}

	if (event.Moving() && !controller->IsPlaying())
	{
		SetTrackCursor(scroll_left + mouse_x, OPT_GET("Audio/Display/Draw/Cursor Time")->GetBool());
	}

	AudioTimingController *timing = controller->GetTimingController();
	if (!timing) return;
	const int drag_sensitivity = int(OPT_GET("Audio/Start Drag Sensitivity")->GetInt() * ms_per_pixel);
	const int snap_sensitivity = OPT_GET("Audio/Snap/Enable")->GetBool() != event.ShiftDown() ? int(OPT_GET("Audio/Snap/Distance")->GetInt() * ms_per_pixel) : 0;

	// Not scrollbar, not timeline, no button action
	if (event.Moving())
	{
		const int timepos = TimeFromRelativeX(mouse_x);

		if (timing->IsNearbyMarker(timepos, drag_sensitivity, event.AltDown()))
			SetCursor(wxCursor(wxCURSOR_SIZEWE));
		else
			SetCursor(wxNullCursor);
		return;
	}

	const int old_scroll_pos = scroll_left;
	if (event.LeftDown() || event.RightDown())
	{
		const int timepos = TimeFromRelativeX(mouse_x);
		std::vector<AudioMarker*> markers = event.LeftDown()
			? timing->OnLeftClick(timepos, event.CmdDown(), event.AltDown(), drag_sensitivity, snap_sensitivity)
			: timing->OnRightClick(timepos, event.CmdDown(), drag_sensitivity, snap_sensitivity);

		// Clicking should never result in the audio display scrolling
		ScrollPixelToLeft(old_scroll_pos);

		if (markers.size())
		{
			RemoveTrackCursor();
			audio_marker = agi::make_unique<AudioMarkerInteractionObject>(markers, timing, this, (wxMouseButton)event.GetButton());
			SetDraggedObject(audio_marker.get());
			return;
		}
	}
}

bool AudioDisplay::ForwardMouseEvent(wxMouseEvent &event) {
	// Handle any ongoing drag
	if (dragged_object && HasCapture())
	{
		if (!dragged_object->OnMouseEvent(event))
		{
			scroll_timer.Stop();
			SetDraggedObject(nullptr);
			SetCursor(wxNullCursor);
		}
		return true;
	}
	else
	{
		// Something is wrong, we might have lost capture somehow.
		// Fix state and pretend it didn't happen.
		SetDraggedObject(nullptr);
		SetCursor(wxNullCursor);
	}

	const wxPoint mousepos = event.GetPosition();
	AudioDisplayInteractionObject *new_obj = nullptr;
	// Check for scrollbar action
	if (scrollbar->GetBounds().Contains(mousepos))
	{
		new_obj = scrollbar.get();
	}
	// Check for timeline action
	else if (timeline->GetBounds().Contains(mousepos))
	{
		SetCursor(wxCursor(wxCURSOR_SIZEWE));
		new_obj = timeline.get();
	}
	else
	{
		return false;
	}

	if (!controller->IsPlaying())
		RemoveTrackCursor();
	if (new_obj->OnMouseEvent(event))
		SetDraggedObject(new_obj);
	return true;
}

void AudioDisplay::OnKeyDown(wxKeyEvent& event)
{
	hotkey::check("Audio", context, event);
}

void AudioDisplay::OnSize(wxSizeEvent &)
{
	// We changed size, update the sub-controls' internal data and redraw
	wxSize size = GetClientSize();

	timeline->SetDisplaySize(wxSize(size.x, scrollbar->GetBounds().y));
	scrollbar->SetDisplaySize(size);

	if (controller->GetTimingController())
	{
		TimeRange sel(controller->GetTimingController()->GetPrimaryPlaybackRange());
		scrollbar->SetSelection(AbsoluteXFromTime(sel.begin()), AbsoluteXFromTime(sel.length()));
	}

	audio_height = size.GetHeight();
	audio_height -= scrollbar->GetBounds().GetHeight();
	audio_height -= timeline->GetHeight();
	audio_renderer->SetHeight(audio_height);

	audio_top = timeline->GetHeight();

	Refresh();
}

void AudioDisplay::OnFocus(wxFocusEvent &)
{
	// The scrollbar indicates focus so repaint that
	RefreshRect(scrollbar->GetBounds(), false);
}

int AudioDisplay::GetDuration() const
{
	if (!provider) return 0;
	return (provider->GetNumSamples() * 1000 + provider->GetSampleRate() - 1) / provider->GetSampleRate();
}

void AudioDisplay::OnAudioOpen(agi::AudioProvider *provider)
{
	this->provider = provider;

	if (!audio_renderer_provider)
		ReloadRenderingSettings();

	audio_renderer->SetAudioProvider(provider);
	audio_renderer->SetCacheMaxSize(OPT_GET("Audio/Renderer/Spectrum/Memory Max")->GetInt() * 1024 * 1024);

	timeline->ChangeAudio(GetDuration());

	ms_per_pixel = 0;
	SetZoomLevel(zoom_level);

	Refresh();

	if (provider)
	{
		if (connections.empty())
		{
			connections = agi::signal::make_vector({
				controller->AddPlaybackPositionListener(&AudioDisplay::OnPlaybackPosition, this),
				controller->AddPlaybackStopListener(&AudioDisplay::RemoveTrackCursor, this),
				controller->AddTimingControllerListener(&AudioDisplay::OnTimingController, this),
				OPT_SUB("Audio/Spectrum", &AudioDisplay::ReloadRenderingSettings, this),
				OPT_SUB("Audio/Display/Waveform Style", &AudioDisplay::ReloadRenderingSettings, this),
				OPT_SUB("Colour/Audio Display/Spectrum", &AudioDisplay::ReloadRenderingSettings, this),
				OPT_SUB("Colour/Audio Display/Waveform", &AudioDisplay::ReloadRenderingSettings, this),
				OPT_SUB("Audio/Renderer/Spectrum/Quality", &AudioDisplay::ReloadRenderingSettings, this),
				OPT_SUB("Timing/Tap To Time", &AudioDisplay::OnTapMarkerChanged, this),
			});
			OnTimingController();
		}

		last_sample_decoded = provider->GetDecodedSamples();
		audio_load_position = -1;
		audio_load_speed = 0;
		audio_load_start_time = std::chrono::steady_clock::now();
		if (last_sample_decoded != provider->GetNumSamples())
			load_timer.Start(100);
	}
	else
	{
		connections.clear();
	}
}

void AudioDisplay::OnTimingController()
{
	AudioTimingController *timing_controller = controller->GetTimingController();
	if (timing_controller)
	{
		timing_controller->AddMarkerMovedListener(&AudioDisplay::OnMarkerMoved, this);
		timing_controller->AddUpdatedPrimaryRangeListener(&AudioDisplay::OnSelectionChanged, this);
		timing_controller->AddUpdatedStyleRangesListener(&AudioDisplay::OnStyleRangesChanged, this);
		timing_controller->AddUpdatedTapMarkerListener(&AudioDisplay::OnTapMarkerChanged, this);

		OnStyleRangesChanged();
		OnMarkerMoved();
		OnSelectionChanged();
		OnTapMarkerChanged();
	}
}

void AudioDisplay::OnPlaybackPosition(int ms)
{
	int pixel_position = AbsoluteXFromTime(ms);
	SetTrackCursor(pixel_position, false);

	if (OPT_GET("Audio/Lock Scroll on Cursor")->GetBool())
	{
		int client_width = GetClientSize().GetWidth();
		int edge_size = client_width / 20;
		if (scroll_left > 0 && pixel_position < scroll_left + edge_size)
		{
			ScrollPixelToLeft(std::max(pixel_position - edge_size, 0));
		}
		else if (scroll_left + client_width < std::min(pixel_audio_width - 1, pixel_position + edge_size))
		{
			ScrollPixelToLeft(std::min(pixel_position - client_width + edge_size, pixel_audio_width - client_width - 1));
		}
	}
}

void AudioDisplay::OnSelectionChanged()
{
	TimeRange sel(controller->GetPrimaryPlaybackRange());
	scrollbar->SetSelection(AbsoluteXFromTime(sel.begin()), AbsoluteXFromTime(sel.length()));

	if (audio_marker)
	{
		if (!scroll_timer.IsRunning())
		{
			// If the dragged object is outside the visible area, start the
			// scroll timer to shift it back into view
			int rel_x = RelativeXFromTime(audio_marker->GetPosition());
			if (rel_x < 0 || rel_x >= GetClientSize().GetWidth())
			{
				// 50ms is the default for this on Windows (hardcoded since
				// wxSystemSettings doesn't expose DragScrollDelay etc.)
				scroll_timer.Start(50, true);
			}
		}
	}
	else if (OPT_GET("Audio/Auto/Scroll")->GetBool() && sel.end() != 0)
	{
		ScrollTimeRangeInView(sel);
	}

	RefreshRect(scrollbar->GetBounds(), false);
}

void AudioDisplay::OnScrollTimer(wxTimerEvent &event)
{
	if (!audio_marker) return;

	int rel_x = RelativeXFromTime(audio_marker->GetPosition());
	int width = GetClientSize().GetWidth();

	// If the dragged object is outside the visible area, scroll it into
	// view with a 5% margin
	if (rel_x < 0)
	{
		ScrollBy(rel_x - width / 20);
	}
	else if (rel_x >= width)
	{
		ScrollBy(rel_x - width + width / 20);
	}
}

void AudioDisplay::OnStyleRangesChanged()
{
	if (!controller->GetTimingController()) return;

	AudioStyleRangeMerger asrm;
	controller->GetTimingController()->GetRenderingStyles(asrm);

	style_ranges.clear();
	for (auto pair : asrm) style_ranges.push_back(pair);

	RefreshRect(wxRect(0, audio_top, GetClientSize().GetWidth(), audio_height), false);
}

void AudioDisplay::OnTapMarkerChanged()
{
	RefreshRect(wxRect(0, audio_top, GetClientSize().GetWidth(), audio_height), false);
}


void AudioDisplay::OnMarkerMoved()
{
	RefreshRect(wxRect(0, audio_top, GetClientSize().GetWidth(), audio_height), false);
}
