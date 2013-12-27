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

#include "config.h"

#include "video_display.h"

#include "ass_file.h"
#include "command/command.h"
#include "compat.h"
#include "include/aegisub/context.h"
#include "include/aegisub/hotkey.h"
#include "include/aegisub/menu.h"
#include "options.h"
#include "spline_curve.h"
#include "retina_helper.h"
#include "subs_controller.h"
#include "threaded_frame_source.h"
#include "utils.h"
#include "video_out_gl.h"
#include "video_context.h"
#include "video_frame.h"
#include "visual_tool.h"

#include <libaegisub/util.h>

#include <algorithm>

#include <wx/combobox.h>
#include <wx/dataobj.h>
#include <wx/dcclient.h>
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

/// @class VideoOutRenderException
/// @extends VideoOutException
/// @brief An OpenGL error occurred while uploading or displaying a frame
class OpenGlException : public agi::Exception {
public:
	OpenGlException(const char *func, int err)
	: agi::Exception(from_wx(wxString::Format("%s failed with error code %d", func, err)))
	{ }
	const char * GetName() const override { return "video/opengl"; }
	Exception * Copy() const override { return new OpenGlException(*this); }
};

#define E(cmd) cmd; if (GLenum err = glGetError()) throw OpenGlException(#cmd, err)

VideoDisplay::VideoDisplay(
	wxToolBar *visualSubToolBar,
	bool freeSize,
	wxComboBox *zoomBox,
	wxWindow* parent,
	agi::Context *c)
: wxGLCanvas(parent, -1, attribList)
, autohideTools(OPT_GET("Tool/Visual/Autohide"))
, con(c)
, zoomValue(OPT_GET("Video/Default Zoom")->GetInt() * .125 + .125)
, toolBar(visualSubToolBar)
, zoomBox(zoomBox)
, freeSize(freeSize)
{
	zoomBox->SetValue(wxString::Format("%g%%", zoomValue * 100.));
	zoomBox->Bind(wxEVT_COMBOBOX, &VideoDisplay::SetZoomFromBox, this);
	zoomBox->Bind(wxEVT_TEXT_ENTER, &VideoDisplay::SetZoomFromBoxText, this);

	con->videoController->Bind(EVT_FRAME_READY, &VideoDisplay::UploadFrameData, this);
	slots.push_back(con->videoController->AddVideoOpenListener(&VideoDisplay::UpdateSize, this));
	slots.push_back(con->videoController->AddARChangeListener(&VideoDisplay::UpdateSize, this));

	slots.push_back(con->subsController->AddFileSaveListener(&VideoDisplay::OnSubtitlesSave, this));

	Bind(wxEVT_PAINT, std::bind(&VideoDisplay::Render, this));
	Bind(wxEVT_SIZE, &VideoDisplay::OnSizeEvent, this);
	Bind(wxEVT_CONTEXT_MENU, &VideoDisplay::OnContextMenu, this);
	Bind(wxEVT_ENTER_WINDOW, &VideoDisplay::OnMouseEvent, this);
	Bind(wxEVT_CHAR_HOOK, &VideoDisplay::OnKeyDown, this);
	Bind(wxEVT_LEAVE_WINDOW, &VideoDisplay::OnMouseLeave, this);
	Bind(wxEVT_LEFT_DCLICK, &VideoDisplay::OnMouseEvent, this);
	Bind(wxEVT_LEFT_DOWN, &VideoDisplay::OnMouseEvent, this);
	Bind(wxEVT_LEFT_UP, &VideoDisplay::OnMouseEvent, this);
	Bind(wxEVT_MOTION, &VideoDisplay::OnMouseEvent, this);
	Bind(wxEVT_MOUSEWHEEL, &VideoDisplay::OnMouseWheel, this);

	SetCursor(wxNullCursor);

	retinaHelper = agi::util::make_unique<RetinaHelper>(this);
	retinaHelper->setViewWantsBestResolutionOpenGLSurface(true);
	scaleFactor = retinaHelper->getBackingScaleFactor();

	c->videoDisplay = this;

	if (con->videoController->IsLoaded())
		con->videoController->JumpToFrame(con->videoController->GetFrameN());
}

VideoDisplay::~VideoDisplay () {
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
		glContext = agi::util::make_unique<wxGLContext>(this);

	SetCurrent(*glContext);
	return true;
}

void VideoDisplay::UploadFrameData(FrameReadyEvent &evt) {
	pending_frame = evt.frame;
	Render();
}

void VideoDisplay::Render() try {
	if (!con->videoController->IsLoaded() || !InitContext() || (!videoOut && !pending_frame))
		return;

	if (!videoOut)
		videoOut = agi::util::make_unique<VideoOutGL>();

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
		con->videoController->SetVideo("");
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
	E(glViewport(0, std::min(viewport_bottom, 0), videoSize.GetWidth(), videoSize.GetHeight()));

	E(glMatrixMode(GL_PROJECTION));
	E(glLoadIdentity());
	E(glOrtho(0.0f, videoSize.GetWidth(), videoSize.GetHeight(), 0.0f, -1000.0f, 1000.0f));

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
		err.GetChainedMessage());
	con->videoController->SetVideo("");
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
	if (!con->videoController->IsLoaded() || !IsShownOnScreen()) return;

	viewport_left = 0;
	viewport_bottom = GetClientSize().GetHeight() * scaleFactor - videoSize.GetHeight();
	viewport_top = 0;
	viewport_width = videoSize.GetWidth();
	viewport_height = videoSize.GetHeight();

	if (freeSize) {
		int vidW = con->videoController->GetWidth();
		int vidH = con->videoController->GetHeight();

		AspectRatio arType = con->videoController->GetAspectRatioType();
		double displayAr = double(viewport_width) / viewport_height;
		double videoAr = arType == AspectRatio::Default ? double(vidW) / vidH : con->videoController->GetAspectRatioValue();

		// Window is wider than video, blackbox left/right
		if (displayAr - videoAr > 0.01f) {
			int delta = viewport_width - videoAr * viewport_height;
			viewport_left = delta / 2;
			viewport_width -= delta;
		}
		// Video is wider than window, blackbox top/bottom
		else if (videoAr - displayAr > 0.01f) {
			int delta = viewport_height - viewport_width / videoAr;
			viewport_top = viewport_bottom = delta / 2;
			viewport_height -= delta;
		}
	}

	if (tool)
		tool->SetDisplayArea(viewport_left, viewport_top, viewport_width, viewport_height);

	Render();
}

void VideoDisplay::UpdateSize() {
	scaleFactor = retinaHelper->getBackingScaleFactor();

	if (!con->videoController->IsLoaded() || !IsShownOnScreen()) return;

	videoSize.Set(con->videoController->GetWidth(), con->videoController->GetHeight());
	videoSize *= zoomValue;
	if (con->videoController->GetAspectRatioType() != AspectRatio::Default)
		videoSize.SetWidth(videoSize.GetHeight() * con->videoController->GetAspectRatioValue());

	wxEventBlocker blocker(this);
	if (freeSize) {
		wxWindow *top = GetParent();
		while (!top->IsTopLevel()) top = top->GetParent();

		wxSize cs = GetClientSize();
		wxSize oldSize = top->GetSize();
		top->SetSize(top->GetSize() + videoSize * (1.0f / scaleFactor) - cs);
		SetClientSize(cs + top->GetSize() - oldSize);
	}
	else {
		auto scaledSize = wxSize(videoSize.GetWidth() / scaleFactor, videoSize.GetHeight() / scaleFactor);
		SetMinClientSize(scaledSize);
		SetMaxClientSize(scaledSize);

		GetGrandParent()->Layout();
	}

	PositionVideo();
}

void VideoDisplay::OnSizeEvent(wxSizeEvent &event) {
	scaleFactor = retinaHelper->getBackingScaleFactor();
	
	if(tool)
		tool->SetScaleFactor(scaleFactor);
	
	if (freeSize) {
		videoSize = GetClientSize();
		PositionVideo();
		zoomValue = double(viewport_height) / con->videoController->GetHeight() * scaleFactor;
		zoomBox->ChangeValue(wxString::Format("%g%%", zoomValue * 100.));
	}
	else {
		PositionVideo();
	}
}

void VideoDisplay::OnMouseEvent(wxMouseEvent& event) {
	if (event.ButtonDown())
		SetFocus();

	last_mouse_pos = mouse_pos = event.GetPosition();

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
			SetZoom(zoomValue + .125 * (wheel / event.GetWheelDelta()));
	}
}

void VideoDisplay::OnContextMenu(wxContextMenuEvent&) {
	if (!context_menu.get()) context_menu = menu::GetMenu("video_context", con);
	SetCursor(wxNullCursor);
	menu::OpenPopupMenu(context_menu.get(), this);
}

void VideoDisplay::OnKeyDown(wxKeyEvent &event) {
	hotkey::check("Video", con, event);
}

void VideoDisplay::SetZoom(double value) {
	zoomValue = std::max(value, .125);
	size_t selIndex = zoomValue / .125 - 1;
	if (selIndex < zoomBox->GetCount())
		zoomBox->SetSelection(selIndex);
	zoomBox->ChangeValue(wxString::Format("%g%%", zoomValue * 100.));
	UpdateSize();
}

void VideoDisplay::SetZoomFromBox(wxCommandEvent &) {
	int sel = zoomBox->GetSelection();
	if (sel != wxNOT_FOUND) {
		zoomValue = (sel + 1) * .125;
		UpdateSize();
	}
}

void VideoDisplay::SetZoomFromBoxText(wxCommandEvent &) {
	wxString strValue = zoomBox->GetValue();
	if (strValue.EndsWith("%"))
		strValue.RemoveLast();

	double value;
	if (strValue.ToDouble(&value))
		SetZoom(value / 100.);
}

void VideoDisplay::SetTool(std::unique_ptr<VisualToolBase> new_tool) {
	toolBar->ClearTools();
	toolBar->Realize();
	toolBar->Show(false);

	tool = std::move(new_tool);
	tool->SetToolbar(toolBar);

	tool->SetScaleFactor(scaleFactor);

	// Update size as the new typesetting tool may have changed the subtoolbar size
	if (!freeSize)
		UpdateSize();
	else {
		// UpdateSize fits the window to the video, which we don't want to do
		GetGrandParent()->Layout();
		tool->SetDisplayArea(viewport_left, viewport_top, viewport_width, viewport_height);
	}
}

bool VideoDisplay::ToolIsType(std::type_info const& type) const {
	return tool && typeid(*tool) == type;
}

Vector2D VideoDisplay::GetMousePosition() const {
	return last_mouse_pos ? tool->ToScriptCoords(last_mouse_pos) : last_mouse_pos;
}

void VideoDisplay::Unload() {
	glContext.reset();
	videoOut.reset();
	tool.reset();
	pending_frame.reset();
}

void VideoDisplay::OnSubtitlesSave() {
	con->ass->SaveUIState("Video Zoom Percent", std::to_string(zoomValue));
}
