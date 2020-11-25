// Copyright (c) 2011, Thomas Goyne <plorkyeran@aegisub.org>
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

/// @file visual_tool_cross.cpp
/// @brief Crosshair double-click-to-position visual typesetting tool
/// @ingroup visual_ts

#include "visual_tool_cross.h"

#include "gl_text.h"
#include "include/aegisub/context.h"
#include "selection_controller.h"
#include "video_display.h"

#include <libaegisub/color.h>
#include <libaegisub/format.h>
#include <libaegisub/make_unique.h>

VisualToolCross::VisualToolCross(VideoDisplay *parent, agi::Context *context)
: VisualTool<VisualDraggableFeature>(parent, context)
, gl_text(agi::make_unique<OpenGLText>())
{
	parent->SetCursor(wxCursor(wxCURSOR_BLANK));
}

VisualToolCross::~VisualToolCross() {
	parent->SetCursor(wxNullCursor);
}

void VisualToolCross::OnDoubleClick() {
	Vector2D d = ToScriptCoords(mouse_pos) - GetLinePosition(active_line);

	for (auto line : c->selectionController->GetSelectedSet()) {
		Vector2D p1, p2;
		int t1, t2;
		if (GetLineMove(line, p1, p2, t1, t2)) {
			if (t1 > 0 || t2 > 0)
				SetOverride(line, "\\move", agi::format("(%s,%s,%d,%d)", Text(p1 + d), Text(p2 + d), t1, t2));
			else
				SetOverride(line, "\\move", agi::format("(%s,%s)", Text(p1 + d), Text(p2 + d)));
		}
		else
			SetOverride(line, "\\pos", "(" + Text(GetLinePosition(line) + d) + ")");

		if (Vector2D org = GetLineOrigin(line))
			SetOverride(line, "\\org", "(" + Text(org + d) + ")");
	}

	Commit(_("positioning"));
}

void VisualToolCross::Draw() {
	if (!mouse_pos) return;

	// Draw cross
	gl.SetInvert();
	gl.SetLineColour(*wxWHITE, 1.0, 1);
	float lines[] = {
		0.f, mouse_pos.Y(),
		client_size.X(), mouse_pos.Y(),
		mouse_pos.X(), 0.f,
		mouse_pos.X(), client_size.Y(),
	};
	gl.DrawLines(2, lines, 4);
	gl.ClearInvert();

	std::string mouse_text = Text(ToScriptCoords(shift_down ? video_res - mouse_pos : mouse_pos));

	int tw, th;
	gl_text->SetFont("Verdana", 12, true, false);
	gl_text->SetColour(agi::Color(255, 255, 255, 255));
	gl_text->GetExtent(mouse_text, tw, th);

	// Place the text in the corner of the cross closest to the center of the video
	int dx = mouse_pos.X();
	int dy = mouse_pos.Y();
	if (dx > video_res.X() / 2)
		dx -= tw + 4;
	else
		dx += 4;

	if (dy < video_res.Y() / 2)
		dy += 3;
	else
		dy -= th + 3;

	gl_text->Print(mouse_text, dx, dy);
}

std::string VisualToolCross::Text(Vector2D v) {
	return video_res.X() > script_res.X() ? v.Str() : v.DStr();
}
