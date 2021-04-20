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

/// @file video_display.cpp
/// @brief Control displaying a video frame obtained from the video context
/// @ingroup video main_ui
///

#include "video_display.h"

#include "ass_file.h"
#include "async_video_provider.h"
#include "command/command.h"
#include "compat.h"
#include "format.h"
#include "include/aegisub/context.h"
#include "include/aegisub/hotkey.h"
#include "include/aegisub/menu.h"
#include "options.h"
#include "project.h"
#include "retina_helper.h"
#include "spline_curve.h"
#include "utils.h"
#include "video_out_gl.h"
#include "video_controller.h"
#include "visual_tool.h"

#include <libaegisub/make_unique.h>

#include <algorithm>
#include <wx/combobox.h>
#include <wx/menu.h>
#include <wx/textctrl.h>
#include <wx/toolbar.h>

#ifdef HAVE_OPENGL_GL_H
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

/// Attribute list for gl canvases; set the canvases to doublebuffered rgba with an 8 bit stencil buffer
int attribList[] = { WX_GL_RGBA , WX_GL_DOUBLEBUFFER, WX_GL_STENCIL_SIZE, 8, 0 };

/// An OpenGL error occurred while uploading or displaying a frame
class OpenGlException final : public agi::Exception {
public:
	OpenGlException(const char *func, int err)
	: agi::Exception(agi::format("%s failed with error code %d", func, err))
	{ }
};

#define E(cmd) cmd; if (GLenum err = glGetError()) throw OpenGlException(#cmd, err)

VideoDisplay::VideoDisplay(wxToolBar *toolbar, bool freeSize, wxComboBox *zoomBox, wxWindow *parent, agi::Context *c)
: wxGLCanvas(parent, -1, attribList)
, autohideTools(OPT_GET("Tool/Visual/Autohide"))
, con(c)
, windowZoomValue(OPT_GET("Video/Default Zoom")->GetInt() * .125 + .125)
, videoZoomValue(1)
, toolBar(toolbar)
, zoomBox(zoomBox)
, freeSize(freeSize)
, retina_helper(agi::make_unique<RetinaHelper>(this))
, scale_factor(retina_helper->GetScaleFactor())
, scale_factor_connection(retina_helper->AddScaleFactorListener([=](int new_scale_factor) {
	double new_zoom = windowZoomValue * new_scale_factor / scale_factor;
	scale_factor = new_scale_factor;
	SetWindowZoom(new_zoom);
}))
{
	zoomBox->SetValue(fmt_wx("%g%%", windowZoomValue * 100.));
	zoomBox->Bind(wxEVT_COMBOBOX, &VideoDisplay::SetZoomFromBox, this);
	zoomBox->Bind(wxEVT_TEXT_ENTER, &VideoDisplay::SetZoomFromBoxText, this);

	con->videoController->Bind(EVT_FRAME_READY, &VideoDisplay::UploadFrameData, this);
	connections = agi::signal::make_vector({
		con->project->AddVideoProviderListener(&VideoDisplay::UpdateSize, this),
		con->videoController->AddARChangeListener(&VideoDisplay::UpdateSize, this),
	});

	Bind(wxEVT_PAINT, std::bind(&VideoDisplay::Render, this));
	Bind(wxEVT_SIZE, &VideoDisplay::OnSizeEvent, this);
	Bind(wxEVT_CONTEXT_MENU, &VideoDisplay::OnContextMenu, this);
	Bind(wxEVT_ENTER_WINDOW, &VideoDisplay::OnMouseEvent, this);
	Bind(wxEVT_CHAR_HOOK, &VideoDisplay::OnKeyDown, this);
	Bind(wxEVT_LEAVE_WINDOW, &VideoDisplay::OnMouseLeave, this);
	Bind(wxEVT_LEFT_DCLICK, &VideoDisplay::OnMouseEvent, this);
	Bind(wxEVT_LEFT_DOWN, &VideoDisplay::OnMouseEvent, this);
	Bind(wxEVT_LEFT_UP, &VideoDisplay::OnMouseEvent, this);
	Bind(wxEVT_MIDDLE_DOWN, &VideoDisplay::OnMouseEvent, this);
	Bind(wxEVT_MIDDLE_UP, &VideoDisplay::OnMouseEvent, this);
	Bind(wxEVT_MOTION, &VideoDisplay::OnMouseEvent, this);
	Bind(wxEVT_MOUSEWHEEL, &VideoDisplay::OnMouseWheel, this);

	SetCursor(wxNullCursor);

	c->videoDisplay = this;

	con->videoController->JumpToFrame(con->videoController->GetFrameN());

	SetLayoutDirection(wxLayout_LeftToRight);
}

VideoDisplay::~VideoDisplay () {
	Unload();
	con->videoController->Unbind(EVT_FRAME_READY, &VideoDisplay::UploadFrameData, this);
}

bool VideoDisplay::InitContext() {
	if (!IsShownOnScreen())
		return false;

	// If this display is in a minimized detached dialog IsShownOnScreen will
	// return true, but the client size is guaranteed to be 0
	if (GetClientSize() == wxSize(0, 0))
		return false;

	if (!glContext)
		glContext = agi::make_unique<wxGLContext>(this);

	SetCurrent(*glContext);
	return true;
}

void VideoDisplay::UploadFrameData(FrameReadyEvent &evt) {
	pending_frame = evt.frame;
	Render();
}

void VideoDisplay::Render() try {
	if (!con->project->VideoProvider() || !InitContext() || (!videoOut && !pending_frame))
		return;

	if (!videoOut)
		videoOut = agi::make_unique<VideoOutGL>();

	if (!tool)
		cmd::call("video/tool/cross", con);

	try {
		if (pending_frame) {
			videoOut->UploadFrameData(*pending_frame);
			pending_frame.reset();
		}
	}
	catch (const VideoOutInitException& err) {
		wxLogError(
			"Failed to initialize video display. Closing other running "
			"programs and updating your video card drivers may fix this.\n"
			"Error message reported: %s",
			err.GetMessage());
		con->project->CloseVideo();
		return;
	}
	catch (const VideoOutRenderException& err) {
		wxLogError(
			"Could not upload video frame to graphics card.\n"
			"Error message reported: %s",
			err.GetMessage());
		return;
	}

	if (videoSize.GetWidth() == 0) videoSize.SetWidth(1);
	if (videoSize.GetHeight() == 0) videoSize.SetHeight(1);

	if (!viewport_height || !viewport_width)
		PositionVideo();

	videoOut->Render(viewport_left, viewport_bottom, viewport_width, viewport_height);

	int client_w, client_h;
	GetClientSize(&client_w, &client_h);
	E(glViewport(0, 0, client_w, client_h));

	E(glMatrixMode(GL_PROJECTION));
	E(glLoadIdentity());
	E(glOrtho(0.0f, client_w / scale_factor, client_h / scale_factor, 0.0f, -1000.0f, 1000.0f));

	if (OPT_GET("Video/Overscan Mask")->GetBool()) {
		double ar = con->videoController->GetAspectRatioValue();

		// Based on BBC's guidelines: http://www.bbc.co.uk/guidelines/dq/pdf/tv/tv_standards_london.pdf
		// 16:9 or wider
		if (ar > 1.75) {
			DrawOverscanMask(.1f, .05f);
			DrawOverscanMask(0.035f, 0.035f);
		}
		// Less wide than 16:9 (use 4:3 standard)
		else {
			DrawOverscanMask(.067f, .05f);
			DrawOverscanMask(0.033f, 0.035f);
		}
	}

	if ((mouse_pos || !autohideTools->GetBool()) && tool)
		tool->Draw();

	SwapBuffers();
}
catch (const agi::Exception &err) {
	wxLogError(
		"An error occurred trying to render the video frame on the screen.\n"
		"Error message reported: %s",
		err.GetMessage());
	con->project->CloseVideo();
}

void VideoDisplay::DrawOverscanMask(float horizontal_percent, float vertical_percent) const {
	Vector2D v(viewport_width, viewport_height);
	Vector2D size = Vector2D(horizontal_percent, vertical_percent) / 2 * v;

	// Clockwise from top-left
	Vector2D corners[] = {
		size,
		Vector2D(viewport_width - size.X(), size),
		v - size,
		Vector2D(size, viewport_height - size.Y())
	};

	// Shift to compensate for black bars
	Vector2D pos(viewport_left, viewport_top);
	for (auto& corner : corners)
		corner = corner + pos;

	int count = 0;
	std::vector<float> points;
	for (size_t i = 0; i < 4; ++i) {
		size_t prev = (i + 3) % 4;
		size_t next = (i + 1) % 4;
		count += SplineCurve(
				(corners[prev] + corners[i] * 4) / 5,
				corners[i], corners[i],
				(corners[next] + corners[i] * 4) / 5)
			.GetPoints(points);
	}

	OpenGLWrapper gl;
	gl.SetFillColour(wxColor(30, 70, 200), .5f);
	gl.SetLineColour(*wxBLACK, 0, 1);

	std::vector<int> vstart(1, 0);
	std::vector<int> vcount(1, count);
	gl.DrawMultiPolygon(points, vstart, vcount, Vector2D(viewport_left, viewport_top), Vector2D(viewport_width, viewport_height), true);
}

void VideoDisplay::PositionVideo() {
	auto provider = con->project->VideoProvider();
	if (!provider || !IsShownOnScreen()) return;

	int client_w, client_h;
	GetClientSize(&client_w, &client_h);

	viewport_left = 0;
	viewport_bottom = client_h * scale_factor - videoSize.GetHeight();
	viewport_top = 0;
	viewport_width = videoSize.GetWidth();
	viewport_height = videoSize.GetHeight();

	if (freeSize) {
		int vidW = provider->GetWidth();
		int vidH = provider->GetHeight();

		AspectRatio arType = con->videoController->GetAspectRatioType();
		double displayAr = double(client_w) / client_h;
		double videoAr = arType == AspectRatio::Default ? double(vidW) / vidH : con->videoController->GetAspectRatioValue();

		// Window is wider than video, blackbox left/right
		if (displayAr - videoAr > 0.01) {
			int delta = client_w - videoAr * client_h;
			viewport_left = delta / 2;
		}
		// Video is wider than window, blackbox top/bottom
		else if (videoAr - displayAr > 0.01) {
			int delta = client_h - client_w / videoAr;
			viewport_top += delta / 2;
			viewport_bottom += delta / 2;
			viewport_height -= delta;
			viewport_width = viewport_height * videoAr;
		}
	}

	viewport_left += pan_x;
	viewport_top += pan_y;
	viewport_bottom -= pan_y;

	if (tool) {
		tool->SetClientSize(client_w * scale_factor, client_h * scale_factor);
		tool->SetDisplayArea(viewport_left / scale_factor, viewport_top / scale_factor,
		                     viewport_width / scale_factor, viewport_height / scale_factor);
	}
	Render();
}

void VideoDisplay::UpdateSize() {
	auto provider = con->project->VideoProvider();
	if (!provider || !IsShownOnScreen()) return;

	videoSize.Set(provider->GetWidth(), provider->GetHeight());
	videoSize *= videoZoomValue * windowZoomValue;

	wxEventBlocker blocker(this);
	if (freeSize) {
		wxWindow *top = GetParent();
		while (!top->IsTopLevel()) top = top->GetParent();

		wxSize cs = GetClientSize();
		float csAr = (float)cs.GetWidth() / (float)cs.GetHeight();
		wxSize oldSize = top->GetSize();
		top->SetSize(top->GetSize() + wxSize(provider->GetHeight() * csAr, provider->GetHeight()) * windowZoomValue / scale_factor - cs);
		SetClientSize(cs + top->GetSize() - oldSize);
	}
	else {
		wxSize newSize = wxSize(provider->GetWidth(), provider->GetHeight()) * windowZoomValue / scale_factor;
		SetMinClientSize(newSize);
		SetMaxClientSize(newSize);

		GetGrandParent()->Layout();
	}

	PositionVideo();
}

void VideoDisplay::OnSizeEvent(wxSizeEvent &event) {
	if (freeSize) {
		/* If the video is not moved */
		if (videoZoomValue == 1.0f && pan_x == 0 && pan_y == 0)
			videoSize = GetClientSize() * scale_factor;
		/* If the video is moving, we only need to update the size in this case */
		else if (videoSize.GetWidth() == 0 && videoSize.GetHeight() == 0)
			videoSize = GetClientSize() * videoZoomValue * scale_factor;
		windowZoomValue = double(GetClientSize().GetHeight()) / con->project->VideoProvider()->GetHeight();
		zoomBox->ChangeValue(fmt_wx("%g%%", windowZoomValue * 100.));
		con->ass->Properties.video_zoom = windowZoomValue;
		UpdateSize();
	}
	else {
		PositionVideo();
	}
}

void VideoDisplay::OnMouseEvent(wxMouseEvent& event) {
	if (event.ButtonDown())
		SetFocus();

	last_mouse_pos = mouse_pos = event.GetPosition();

	if (event.GetButton() == wxMOUSE_BTN_MIDDLE) {
		if ((panning = event.ButtonDown()))
			pan_last_pos = event.GetPosition();
	}
	if (panning && event.Dragging()) {
		pan_x += event.GetX() - pan_last_pos.X();
		pan_y += event.GetY() - pan_last_pos.Y();
		pan_last_pos = event.GetPosition();

		PositionVideo();
	}

	if (tool)
		tool->OnMouseEvent(event);
}

void VideoDisplay::OnMouseLeave(wxMouseEvent& event) {
	mouse_pos = Vector2D();
	if (tool)
		tool->OnMouseEvent(event);
}

void VideoDisplay::OnMouseWheel(wxMouseEvent& event) {
	if (int wheel = event.GetWheelRotation()) {
		if (ForwardMouseWheelEvent(this, event))
			if (event.ControlDown())
				SetWindowZoom(windowZoomValue + .125 * (wheel / event.GetWheelDelta()));
			else
				SetVideoZoom(wheel / event.GetWheelDelta());
	}
}

void VideoDisplay::OnContextMenu(wxContextMenuEvent&) {
	if (!context_menu) context_menu = menu::GetMenu("video_context", con);
	SetCursor(wxNullCursor);
	menu::OpenPopupMenu(context_menu.get(), this);
}

void VideoDisplay::OnKeyDown(wxKeyEvent &event) {
	hotkey::check("Video", con, event);
}

void VideoDisplay::ResetPan() {
	pan_x = pan_y = 0;
	videoZoomValue = 1;
	UpdateSize();
	PositionVideo();
}

void VideoDisplay::SetWindowZoom(double value) {
	if (value == 0) return;
	value = std::max(value, .125);
	pan_x *= value / windowZoomValue;
	pan_y *= value / windowZoomValue;
	windowZoomValue = value;
	size_t selIndex = windowZoomValue / .125 - 1;
	if (selIndex < zoomBox->GetCount())
		zoomBox->SetSelection(selIndex);
	zoomBox->ChangeValue(fmt_wx("%g%%", windowZoomValue * 100.));
	con->ass->Properties.video_zoom = windowZoomValue;
	UpdateSize();
}

void VideoDisplay::SetVideoZoom(int step) {
	if (step == 0) return;
	double newVideoZoom = videoZoomValue + (.125 * step) * videoZoomValue;
	if (newVideoZoom < 0.125 || newVideoZoom > 10.0)
		return;

	// With the current blackbox algorithm in PositionVideo(), viewport_{width,height} could go negative. Stop that here
	wxSize cs = GetClientSize();
	wxSize videoNewSize = videoSize * (newVideoZoom / videoZoomValue);
	float windowAR = (float)cs.GetWidth() / cs.GetHeight();
	float videoAR = (float)videoNewSize.GetWidth() / videoNewSize.GetHeight();
	if (windowAR < videoAR) {
		int delta = cs.GetHeight() - cs.GetWidth() / videoAR;
		if (videoNewSize.GetHeight() - delta < 0)
			return;
	}

	// Mouse coordinates, relative to the video, at the current zoom level
	Vector2D mp = GetMousePosition() * videoZoomValue * windowZoomValue;

	// The video size will change by this many pixels
	int pixelChangeW = std::lround(videoSize.GetWidth() * (newVideoZoom / videoZoomValue - 1.0));
	int pixelChangeH = std::lround(videoSize.GetHeight() * (newVideoZoom / videoZoomValue - 1.0));

	pan_x -= pixelChangeW * (mp.X() / videoSize.GetWidth());
	pan_y -= pixelChangeH * (mp.Y() / videoSize.GetHeight());

	videoZoomValue = newVideoZoom;
	UpdateSize();
}

void VideoDisplay::SetZoomFromBox(wxCommandEvent &) {
	int sel = zoomBox->GetSelection();
	if (sel != wxNOT_FOUND) {
		windowZoomValue = (sel + 1) * .125;
		con->ass->Properties.video_zoom = windowZoomValue;
		UpdateSize();
	}
}

void VideoDisplay::SetZoomFromBoxText(wxCommandEvent &) {
	wxString strValue = zoomBox->GetValue();
	if (strValue.EndsWith("%"))
		strValue.RemoveLast();

	double value;
	if (strValue.ToDouble(&value))
		SetWindowZoom(value / 100.);
}

void VideoDisplay::SetTool(std::unique_ptr<VisualToolBase> new_tool) {
	// Set the tool first to prevent repeated initialization from VideoDisplay::Render
	tool = std::move(new_tool);

	// Hide the tool bar first to eliminate unecessary size changes
	toolBar->Show(false);
	toolBar->ClearTools();
	toolBar->AddSeparator();
	tool->SetToolbar(toolBar);

	// Update size as the new typesetting tool may have changed the subtoolbar size
	if (!freeSize)
		UpdateSize();
	else {
		// UpdateSize fits the window to the video, which we don't want to do
		GetGrandParent()->Layout();
		tool->SetDisplayArea(viewport_left / scale_factor, viewport_top / scale_factor,
		                     viewport_width / scale_factor, viewport_height / scale_factor);
	}
}

bool VideoDisplay::ToolIsType(std::type_info const& type) const {
	return tool && typeid(*tool) == type;
}

Vector2D VideoDisplay::GetMousePosition() const {
	return last_mouse_pos ? tool->ToScriptCoords(last_mouse_pos) : last_mouse_pos;
}

void VideoDisplay::Unload() {
	if (glContext) {
		SetCurrent(*glContext);
	}
	videoOut.reset();
	tool.reset();
	glContext.reset();
	pending_frame.reset();
}
