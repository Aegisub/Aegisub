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
//
#include <libaegisub/signal.h>

#include <chrono>
#include <cstdint>
#include <memory>

#include <wx/gdicmn.h>
#include <wx/string.h>
#include <wx/timer.h>
#include <wx/window.h>

namespace agi { class AudioProvider; }
namespace agi { struct Context; }

class AudioController;
class AudioRenderer;
class AudioRendererBitmapProvider;
class TimeRange;

// Helper classes used in implementation of the audio display
namespace {
	class AudioDisplayScrollbar;
	class AudioDisplayTimeline;
}
class AudioDisplayInteractionObject;
class AudioMarkerInteractionObject;

/// @class AudioDisplay
/// @brief Primary view/UI for interaction with audio timing
///
/// The audio display is the common view that allows the user to interact with the active
/// timing controller. The audio display also renders audio according to the audio controller
/// and the timing controller, using an audio renderer instance.
class AudioDisplay: public wxWindow {
	agi::signal::Connection audio_open_connection;

	std::vector<agi::signal::Connection> connections;
	agi::Context *context;

	/// The audio renderer manager
	std::unique_ptr<AudioRenderer> audio_renderer;

	/// The current audio renderer
	std::unique_ptr<AudioRendererBitmapProvider> audio_renderer_provider;

	/// The controller managing us
	AudioController *controller = nullptr;

	agi::AudioProvider *provider = nullptr;

	/// Scrollbar helper object
	std::unique_ptr<AudioDisplayScrollbar> scrollbar;

	/// Timeline helper object
	std::unique_ptr<AudioDisplayTimeline> timeline;

	/// The interaction object for the last-dragged audio marker
	std::unique_ptr<AudioMarkerInteractionObject> audio_marker;


	/// Current object on display being dragged, if any
	AudioDisplayInteractionObject *dragged_object = nullptr;
	/// Change the dragged object and update mouse capture
	void SetDraggedObject(AudioDisplayInteractionObject *new_obj);


	/// Timer for scrolling when markers are dragged out of the displayed area
	wxTimer scroll_timer;

	wxTimer load_timer;
	int64_t last_sample_decoded = 0;
	/// Time at which audio loading began, for calculating loading speed
	std::chrono::steady_clock::time_point audio_load_start_time;
	/// Estimated speed of audio decoding in samples per ms
	double audio_load_speed = 0.0;
	/// Current position of the audio loading progress in absolute pixels
	int audio_load_position = 0;

	/// Leftmost pixel in the virtual audio image being displayed
	int scroll_left = 0;

	/// Total width of the audio in pixels
	int pixel_audio_width = 0;

	/// Horizontal zoom measured in millisecond per pixels
	double ms_per_pixel = 0.;

	/// Amplitude scaling ("vertical zoom") as a factor, 1.0 is neutral
	float scale_amplitude = 1.f;

	/// Top of the main audio area in pixels
	int audio_top = 0;

	/// Height of main audio area in pixels
	int audio_height = 0;

	/// Width of the audio marker feet in pixels
	static const int foot_size = 6;

	/// Zoom level given as a number, see SetZoomLevel for details
	int zoom_level;

	/// Absolute pixel position of the tracking cursor (mouse or playback)
	int track_cursor_pos = -1;
	/// Label to show by track cursor
	wxString track_cursor_label;
	/// Bounding rectangle last drawn track cursor label
	wxRect track_cursor_label_rect;
	/// @brief Move the tracking cursor
	/// @param new_pos   New absolute pixel position of the tracking cursor
	/// @param show_time Display timestamp by the tracking cursor?
	void SetTrackCursor(int new_pos, bool show_time);
	/// @brief Remove the tracking cursor from the display
	void RemoveTrackCursor();

	/// Previous style ranges for optimizing redraw when ranges change
	std::vector<std::pair<int, int>> style_ranges;

	/// @brief Reload all rendering settings from Options and reset caches
	///
	/// This can be called if some rendering quality settings have been changed
	/// in Options and need to be reloaded to take effect.
	void ReloadRenderingSettings();

	/// Paint the audio data for a time range
	/// @param dc DC to paint to
	/// @param updtime Time range to repaint
	/// @param updrect Pixel range to repaint
	void PaintAudio(wxDC &dc, TimeRange updtime, wxRect updrect);

	/// Paint the markers in a time range
	/// @param dc DC to paint to
	/// @param updtime Time range to repaint
	void PaintMarkers(wxDC &dc, TimeRange updtime);

	/// Draw a single foot for a marker
	/// @param dc DC to paint to
	/// @param marker_x Position of the marker whose foot is being painted in pixels
	/// @param dir -1 for left, 1 for right
	void PaintFoot(wxDC &dc, int marker_x, int dir);

	/// Draw an indicator for the tap marker
	/// @param dc DC to paint to
	/// @param marker_x Position of the tap marker
	void PaintTapMarker(wxDC &dc, int marker_x);

	/// Paint the labels in a time range
	/// @param dc DC to paint to
	/// @param updtime Time range to repaint
	void PaintLabels(wxDC &dc, TimeRange updtime);

	/// Paint the track cursor
	/// @param dc DC to paint to
	void PaintTrackCursor(wxDC &dc);

	/// Forward the mouse event to the appropriate child control, if any
	/// @return Was the mouse event forwarded somewhere?
	bool ForwardMouseEvent(wxMouseEvent &event);

	/// wxWidgets paint event
	void OnPaint(wxPaintEvent &event);
	/// wxWidgets mouse input event
	void OnMouseEvent(wxMouseEvent &event);
	/// wxWidgets control size changed event
	void OnSize(wxSizeEvent &event);
	/// wxWidgets input focus changed event
	void OnFocus(wxFocusEvent &event);
	/// wxWidgets keypress event
	void OnKeyDown(wxKeyEvent& event);
	void OnScrollTimer(wxTimerEvent &event);
	void OnLoadTimer(wxTimerEvent &);
	void OnMouseEnter(wxMouseEvent&);
	void OnMouseLeave(wxMouseEvent&);

	int GetDuration() const;

	void OnAudioOpen(agi::AudioProvider *provider);
	void OnPlaybackPosition(int ms_position);
	void OnSelectionChanged();
	void OnStyleRangesChanged();
	void OnTimingController();
	void OnMarkerMoved();
	void OnTapMarkerChanged();

public:
	AudioDisplay(wxWindow *parent, AudioController *controller, agi::Context *context);
	~AudioDisplay();

	/// @brief Scroll the audio display
	/// @param pixel_amount Number of pixels to scroll the view
	///
	/// A positive amount moves the display to the right, making later parts of the audio visible.
	void ScrollBy(int pixel_amount);

	/// @brief Scroll the audio display
	/// @param pixel_position Absolute pixel to put at left edge of the audio display
	void ScrollPixelToLeft(int pixel_position);

	/// @brief Scroll the audio display
	/// @param range Time range to ensure is in view
	///
	/// If the entire range is already visible inside the display, nothing is
	/// scrolled. If just one of the two endpoints is visible, the display is
	/// scrolled such that the visible endpoint stays in view but more of the
	/// rest of the range becomes visible.
	///
	/// If the entire range fits inside the display, the display is centered
	/// over the range.  For this calculation, the display is considered
	/// smaller by some margins, see below.
	///
	/// If the range does not fit within the display with margins subtracted,
	/// the start of the range is ensured visible and as much of the rest of
	/// the range is brought into view.
	///
	/// For the purpose of this function, a 5 percent margin is assumed at each
	/// end of the audio display such that a range endpoint that is ensured to
	/// be in view never gets closer to the edge of the display than the
	/// margin. The edge that is not ensured to be in view might be outside of
	/// view or might be closer to the display edge than the
	/// margin.
	void ScrollTimeRangeInView(const TimeRange &range);


	/// @brief Change the zoom level
	/// @param new_zoom_level The new zoom level to use
	///
	/// A zoom level of 0 is the default zoom level, all other levels are based
	/// on this. Negative zoom levels zoom out, positive zoom in.
	///
	/// The zoom levels generally go from +30 to -30. It is possible to zoom in
	/// more than +30.
	void SetZoomLevel(int new_zoom_level);

	/// @brief Get the zoom level
	/// @return The zoom level
	///
	/// See SetZoomLevel for a description of zoom levels.
	int GetZoomLevel() const { return zoom_level; }

	/// @brief Get a textual description of a zoom level
	/// @param level The zoom level to describe
	/// @return A translated string describing a zoom level
	///
	/// The zoom level description can tell the user details about how much audio is
	/// actually displayed.
	wxString GetZoomLevelDescription(int level) const;

	/// @brief Get the zoom factor in percent for a zoom level
	/// @param level The zoom level to get the factor of
	/// @return The zoom factor in percent
	///
	/// Positive: 125, 150, 175, 200, 225, ...
	///
	/// Negative: 90, 80, 70, 60, 50, 45, 40, 35, 30, 25, 20, 19, 18, 17, ..., 1
	///
	/// Too negative numbers get clamped.
	static int GetZoomLevelFactor(int level);

	/// @brief Set amplitude scale factor
	/// @param scale New amplitude scale factor, 1.0 is no scaling
	void SetAmplitudeScale(float scale);

	/// Get a time in milliseconds from an X coordinate relative to current scroll
	int TimeFromRelativeX(int x) const { return int((scroll_left + x) * ms_per_pixel); }
	/// Get a time in milliseconds from an absolute X coordinate
	int TimeFromAbsoluteX(int x) const { return int(x * ms_per_pixel); }
	/// Get an X coordinate relative to the current scroll from a time in milliseconds
	int RelativeXFromTime(int ms) const { return int(ms / ms_per_pixel) - scroll_left; }
	/// Get an absolute X coordinate from a time in milliseconds
	int AbsoluteXFromTime(int ms) const { return int(ms / ms_per_pixel); }
};
