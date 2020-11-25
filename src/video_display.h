// Copyright (c) 2005-2010, Rodrigo Braz Monteiro
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

/// @file video_display.h
/// @see video_display.cpp
/// @ingroup video main_ui
///

#include <libaegisub/signal.h>

#include "vector2d.h"

#include <memory>
#include <typeinfo>
#include <vector>
#include <wx/glcanvas.h>

// Prototypes
class RetinaHelper;
class VideoController;
class VideoOutGL;
class VisualToolBase;
class wxComboBox;
class wxTextCtrl;
class wxToolBar;
struct FrameReadyEvent;
struct VideoFrame;

namespace agi {
	struct Context;
	class OptionValue;
}

class VideoDisplay final : public wxGLCanvas {
	/// Signals the display is connected to
	std::vector<agi::signal::Connection> connections;

	const agi::OptionValue* autohideTools;

	agi::Context *con;

	std::unique_ptr<wxMenu> context_menu;

	/// The size of the video in screen at the current zoom level, which may not
	/// be the same as the actual client size of the display
	wxSize videoSize;

	Vector2D last_mouse_pos, mouse_pos;

	/// Screen pixels between the left of the canvas and the left of the video
	int viewport_left = 0;
	/// The width of the video in screen pixels
	int viewport_width = 0;
	/// Screen pixels between the bottom of the canvas and the bottom of the video; used for glViewport
	int viewport_bottom = 0;
	/// The REAL bottom of the viewport; used only for glViewport
	int viewport_bottom_end = 0;
	/// Screen pixels between the bottom of the canvas and the top of the video; used for coordinate space conversion
	int viewport_top = 0;
	/// The height of the video in screen pixels
	int viewport_height = 0;

	/// The current zoom level, where 1.0 = 100%
	double zoomValue;

	/// The last position of the mouse, when dragging
	Vector2D pan_last_pos;
	/// True if middle mouse button is down, and we should update pan_{x,y}
	bool panning = false;
	/// The current video pan offset width
	int pan_x = 0;
	/// The current video pan offset height
	int pan_y = 0;

	/// The video renderer
	std::unique_ptr<VideoOutGL> videoOut;

	/// The active visual typesetting tool
	std::unique_ptr<VisualToolBase> tool;
	/// The toolbar used by individual typesetting tools
	wxToolBar* toolBar;

	/// The OpenGL context for this display
	std::unique_ptr<wxGLContext> glContext;

	/// The dropdown box for selecting zoom levels
	wxComboBox *zoomBox;

	/// Whether the display can be freely resized by the user
	bool freeSize;

	/// Frame which will replace the currently visible frame on the next render
	std::shared_ptr<VideoFrame> pending_frame;

	std::unique_ptr<RetinaHelper> retina_helper;
	int scale_factor;
	agi::signal::Connection scale_factor_connection;

	/// @brief Draw an overscan mask
	/// @param horizontal_percent The percent of the video reserved horizontally
	/// @param vertical_percent The percent of the video reserved vertically
	void DrawOverscanMask(float horizontal_percent, float vertical_percent) const;

	/// Upload the image for the current frame to the video card
	void UploadFrameData(FrameReadyEvent&);

	/// @brief Initialize the gl context and set the active context to this one
	/// @return Could the context be set?
	bool InitContext();

	/// @brief Set the size of the display based on the current zoom and video resolution
	void UpdateSize();
	void PositionVideo();
	/// Set the zoom level to that indicated by the dropdown
	void SetZoomFromBox(wxCommandEvent&);
	/// Set the zoom level to that indicated by the text
	void SetZoomFromBoxText(wxCommandEvent&);

	/// @brief Key event handler
	void OnKeyDown(wxKeyEvent &event);
	/// @brief Mouse event handler
	void OnMouseEvent(wxMouseEvent& event);
	void OnMouseWheel(wxMouseEvent& event);
	void OnMouseLeave(wxMouseEvent& event);
	/// @brief Recalculate video positioning and scaling when the available area or zoom changes
	void OnSizeEvent(wxSizeEvent &event);
	void OnContextMenu(wxContextMenuEvent&);

public:
	/// @brief Constructor
	VideoDisplay(
		wxToolBar *visualSubToolBar,
		bool isDetached,
		wxComboBox *zoomBox,
		wxWindow* parent,
		agi::Context *context);
	~VideoDisplay();

	/// @brief Render the currently visible frame
	void Render();

	/// @brief Set the zoom level
	/// @param value The new zoom level
	void SetZoom(double value);
	/// @brief Get the current zoom level
	double GetZoom() const { return zoomValue; }

	/// @brief Reset the video pan
	void ResetPan();

	/// Get the last seen position of the mouse in script coordinates
	Vector2D GetMousePosition() const;

	void SetTool(std::unique_ptr<VisualToolBase> new_tool);

	bool ToolIsType(std::type_info const& type) const;

	/// Discard all OpenGL state
	void Unload();
};
