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

/// @file visual_tool.h
/// @see visual_tool.cpp
/// @ingroup visual_ts

#pragma once

#include "gl_wrap.h"
#include "vector2d.h"
#include "options.h"

#include <libaegisub/owning_intrusive_list.h>
#include <libaegisub/signal.h>

#include <set>

class AssDialogue;
class VideoDisplay;
class wxMouseCaptureLostEvent;
class wxMouseEvent;
class wxToolBar;
namespace agi {
	struct Context;
	class OptionValue;
}

/// @class VisualToolBase
/// @brief Base class for visual tools containing all functionality that doesn't interact with features
///
/// This is required so that visual tools can be used polymorphically, as
/// different VisualTool<T>s are unrelated types otherwise. In addition, as much
/// functionality as possible is implemented here to avoid having four copies
/// of each method for no good reason (and four times as many error messages)
class VisualToolBase {
	void OnCommit(int type);
	void OnSeek(int new_frame);

	void OnMouseCaptureLost(wxMouseCaptureLostEvent &);

	/// @brief Get the dialogue line currently in the edit box
	/// @return nullptr if the line is not active on the current frame
	AssDialogue *GetActiveDialogueLine();

	// SubtitleSelectionListener implementation
	void OnActiveLineChanged(AssDialogue *new_line);

	// Below here are the virtuals that must be implemented

	/// Called when the script, video or screen resolutions change
	virtual void OnCoordinateSystemsChanged() { DoRefresh(); }

	/// Called when the file is changed by something other than a visual tool
	virtual void OnFileChanged() { DoRefresh(); }

	/// Called when the frame number changes
	virtual void OnFrameChanged() { }

	/// Called when the active line changes
	virtual void OnLineChanged() { DoRefresh(); }

	/// Generic refresh to simplify tools which have no interesting state and
	/// can simply do do the same thing for any external change (i.e. most of
	/// them). Called only by the above virtual methods.
	virtual void DoRefresh() { }

protected:
	std::vector<agi::signal::Connection> connections;

	OpenGLWrapper gl;

	/// Called when the user double-clicks
	virtual void OnDoubleClick() { }

	agi::Context *c;
	VideoDisplay *parent;

	bool holding = false; ///< Is a hold currently in progress?
	AssDialogue *active_line = nullptr; ///< Active dialogue line; nullptr if it is not visible on the current frame
	bool dragging = false; ///< Is a drag currently in progress?

	int frame_number; ///< Current frame number

	bool shift_down = false; ///< Is shift down?
	bool ctrl_down = false; ///< Is ctrl down?
	bool alt_down = false; ///< Is alt down?

	Vector2D mouse_pos; ///< Last seen mouse position
	Vector2D drag_start; ///< Mouse position at the beginning of the last drag
	Vector2D script_res; ///< Script resolution
	Vector2D video_pos; ///< Top-left corner of the video in the display area
	Vector2D video_res; ///< Video resolution
	Vector2D client_size; ///< The size of the display area

	const agi::OptionValue *highlight_color_primary_opt;
	const agi::OptionValue *highlight_color_secondary_opt;
	const agi::OptionValue *line_color_primary_opt;
	const agi::OptionValue *line_color_secondary_opt;
	const agi::OptionValue *shaded_area_alpha_opt;

	agi::signal::Connection file_changed_connection;
	int commit_id = -1; ///< Last used commit id for coalescing

	/// @brief Commit the current file state
	/// @param message Description of changes for undo
	virtual void Commit(wxString message = wxString());
	bool IsDisplayed(AssDialogue *line) const;

	/// Get the line's position if it's set, or it's default based on style if not
	Vector2D GetLinePosition(AssDialogue *diag);
	/// Get the line's origin if it's set, or Vector2D::Bad() if not
	Vector2D GetLineOrigin(AssDialogue *diag);
	bool GetLineMove(AssDialogue *diag, Vector2D &p1, Vector2D &p2, int &t1, int &t2);
	void GetLineRotation(AssDialogue *diag, float &rx, float &ry, float &rz);
	void GetLineShear(AssDialogue *diag, float& fax, float& fay);
	void GetLineScale(AssDialogue *diag, Vector2D &scale);
	void GetLineClip(AssDialogue *diag, Vector2D &p1, Vector2D &p2, bool &inverse);
	std::string GetLineVectorClip(AssDialogue *diag, int &scale, bool &inverse);

	void SetOverride(AssDialogue* line, std::string const& tag, std::string const& value);
	void SetSelectedOverride(std::string const& tag, std::string const& value);

	VisualToolBase(VideoDisplay *parent, agi::Context *context);

public:
	/// Convert a point from video to script coordinates
	Vector2D ToScriptCoords(Vector2D point) const;
	/// Convert a point from script to video coordinates
	Vector2D FromScriptCoords(Vector2D point) const;

	// Stuff called by VideoDisplay
	virtual void OnMouseEvent(wxMouseEvent &event)=0;
	virtual void Draw()=0;
	virtual void SetClientSize(int w, int h);
	virtual void SetDisplayArea(int x, int y, int w, int h);
	virtual void SetToolbar(wxToolBar *) { }
	virtual ~VisualToolBase() = default;
};

/// Visual tool base class containing all common feature-related functionality
template<class FeatureType>
class VisualTool : public VisualToolBase {
protected:
	typedef FeatureType Feature;
	typedef agi::owning_intrusive_list<FeatureType> feature_list;

private:
	bool sel_changed = false; /// Has the selection already been changed in the current click?

	/// @brief Called when a hold is begun
	/// @return Should the hold actually happen?
	virtual bool InitializeHold() { return false; }
	/// @brief Called on every mouse event during a hold
	virtual void UpdateHold() { }

	/// @brief Called at the beginning of a drag
	/// @param feature The visual feature clicked on
	/// @return Should the drag happen?
	virtual bool InitializeDrag(FeatureType *feature) { return true; }
	/// @brief Called on every mouse event during a drag
	/// @param feature The current feature to process; not necessarily the one clicked on
	virtual void UpdateDrag(FeatureType *feature) { }

protected:
	std::set<FeatureType *> sel_features; ///< Currently selected visual features

	/// Topmost feature under the mouse; generally only valid during a drag
	FeatureType *active_feature = nullptr;
	/// List of features which are drawn and can be clicked on
	/// List is used here for the iterator invalidation properties
	feature_list features;

	/// Draw all of the features in the list
	void DrawAllFeatures();

	/// @brief Remove a feature from the selection
	/// @param i Index in the feature list
	/// Also deselects lines if all features for that line have been deselected
	void RemoveSelection(FeatureType *feat);

	/// @brief Set the selection to a single feature, deselecting everything else
	/// @param i Index in the feature list
	void SetSelection(FeatureType *feat, bool clear);

public:
	/// @brief Handler for all mouse events
	/// @param event Shockingly enough, the mouse event
	void OnMouseEvent(wxMouseEvent &event) override;

	/// @brief Constructor
	/// @param parent The VideoDisplay to use for coordinate conversion
	/// @param video Video and mouse information passing blob
	VisualTool(VideoDisplay *parent, agi::Context *context);
};
