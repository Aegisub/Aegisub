// Copyright (c) 2010, Niels Martin Hansen
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

#include "ass_dialogue.h"
#include "ass_file.h"
#include "audio_controller.h"
#include "audio_marker.h"
#include "audio_rendering_style.h"
#include "audio_timing.h"
#include "command/command.h"
#include "include/aegisub/context.h"
#include "options.h"
#include "pen.h"
#include "selection_controller.h"
#include "utils.h"

#include <libaegisub/ass/time.h>
#include <libaegisub/make_unique.h>

#include <boost/range/algorithm.hpp>
#include <wx/pen.h>

namespace {
class TimeableLine;

/// @class DialogueTimingMarker
/// @brief AudioMarker implementation for AudioTimingControllerDialogue
///
/// Audio marker intended to live in pairs of two, taking styles depending
/// on which marker in the pair is to the left and which is to the right.
class DialogueTimingMarker final : public AudioMarker {
	/// Current ms position of this marker
	int position;

	/// Draw style for the marker
	const Pen *style;

	/// Feet style for the marker
	FeetStyle feet;

	/// Rendering style of the owning line, needed for sorting
	AudioRenderingStyle type;

	/// The line which owns this marker
	TimeableLine *line;

public:
	int       GetPosition() const override { return position; }
	wxPen     GetStyle()    const override { return *style; }
	FeetStyle GetFeet()     const override { return feet; }

	/// Move the marker to a new position
	/// @param new_position The position to move the marker to, in milliseconds
	///
	/// This notifies the owning line of the change, so that it can ensure that
	/// this marker has the appropriate rendering style.
	void SetPosition(int new_position);

	/// Constructor
	/// @param position Initial position of this marker
	/// @param style Rendering style of this marker
	/// @param feet Foot style of this marker
	/// @param type Type of this marker, used only for sorting
	/// @param line Line which this is a marker for
	DialogueTimingMarker(int position, const Pen *style, FeetStyle feet, AudioRenderingStyle type, TimeableLine *line)
	: position(position)
	, style(style)
	, feet(feet)
	, type(type)
	, line(line)
	{
	}

	DialogueTimingMarker(DialogueTimingMarker const& other, TimeableLine *line)
	: position(other.position)
	, style(other.style)
	, feet(other.feet)
	, type(other.type)
	, line(line)
	{
	}

	/// Get the line which this is a marker for
	TimeableLine *GetLine() const { return line; }

	/// Implicit decay to the position of the marker
	operator int() const { return position; }

	/// Comparison operator
	///
	/// Compares first on position, then on audio rendering style so that the
	/// markers for the active line end up after those for the inactive lines.
	bool operator<(DialogueTimingMarker const& other) const
	{
		if (position < other.position) return true;
		if (position > other.position) return false;
		return type < other.type;
	}

	/// Swap the rendering style of this marker with that of the passed marker
	void SwapStyles(DialogueTimingMarker &other)
	{
		std::swap(style, other.style);
		std::swap(feet, other.feet);
	}
};

/// A comparison predicate for pointers to dialogue markers and millisecond positions
struct marker_ptr_cmp
{
	bool operator()(const DialogueTimingMarker *lft, const DialogueTimingMarker *rgt) const
	{
		return *lft < *rgt;
	}

	bool operator()(const DialogueTimingMarker *lft, int rgt) const
	{
		return *lft < rgt;
	}

	bool operator()(int lft, const DialogueTimingMarker *rgt) const
	{
		return lft < *rgt;
	}
};

/// @class TimeableLine
/// @brief A single dialogue line which can be timed via AudioTimingControllerDialogue
///
/// This class provides markers and styling ranges for a single dialogue line,
/// both active and inactive. In addition, it can apply changes made via those
/// markers to the tracked dialogue line.
class TimeableLine {
	/// The current tracked dialogue line
	AssDialogue *line = nullptr;
	/// The rendering style of this line
	AudioRenderingStyle style;

	/// One of the markers. Initially the left marker, but the user may change this.
	DialogueTimingMarker marker1;
	/// One of the markers. Initially the right marker, but the user may change this.
	DialogueTimingMarker marker2;

	/// Pointer to whichever marker happens to be on the left
	DialogueTimingMarker *left_marker;
	/// Pointer to whichever marker happens to be on the right
	DialogueTimingMarker *right_marker;

public:
	/// Constructor
	/// @param style Rendering style to use for this line's time range
	/// @param style_left The rendering style for the start marker
	/// @param style_right The rendering style for the end marker
	TimeableLine(AudioRenderingStyle style, const Pen *style_left, const Pen *style_right)
	: style(style)
	, marker1(0, style_left, AudioMarker::Feet_Right, style, this)
	, marker2(0, style_right, AudioMarker::Feet_Left, style, this)
	, left_marker(&marker1)
	, right_marker(&marker2)
	{
	}

	/// Explicit copy constructor needed due to that the markers have a pointer to this
	TimeableLine(TimeableLine const& other)
	: line(other.line)
	, style(other.style)
	, marker1(*other.left_marker, this)
	, marker2(*other.right_marker, this)
	, left_marker(&marker1)
	, right_marker(&marker2)
	{
	}

	/// Get the tracked dialogue line
	AssDialogue *GetLine() const { return line; }

	/// Get the time range for this line
	operator TimeRange() const { return TimeRange(*left_marker, *right_marker); }

	/// Add this line's style to the style ranges
	void GetStyleRange(AudioRenderingStyleRanges *ranges) const
	{
		ranges->AddRange(*left_marker, *right_marker, style);
	}

	/// Get this line's markers
	/// @param c Vector to add the markers to
	template<typename Container>
	void GetMarkers(Container *c) const
	{
		c->push_back(left_marker);
		c->push_back(right_marker);
	}

	/// Get the leftmost of the markers
	DialogueTimingMarker *GetLeftMarker() { return left_marker; }
	const DialogueTimingMarker *GetLeftMarker() const { return left_marker; }

	/// Get the rightmost of the markers
	DialogueTimingMarker *GetRightMarker() { return right_marker; }
	const DialogueTimingMarker *GetRightMarker() const { return right_marker; }

	/// Does this line have a marker in the given range?
	bool ContainsMarker(TimeRange const& range) const
	{
		return range.contains(marker1) || range.contains(marker2);
	}

	/// Check if the markers have the correct styles, and correct them if needed
	void CheckMarkers()
	{
		if (*right_marker < *left_marker)
		{
			marker1.SwapStyles(marker2);
			std::swap(left_marker, right_marker);
		}
	}

	/// Apply any changes made here to the tracked dialogue line
	void Apply()
	{
		if (line)
		{
			line->Start = left_marker->GetPosition();
			line->End = right_marker->GetPosition();
		}
	}

	/// Set the dialogue line which this is tracking and reset the markers to
	/// the line's time range
	/// @return Were the markers actually set to the line's time?
	bool SetLine(AssDialogue *new_line)
	{
		if (!line || new_line->End > 0)
		{
			line = new_line;
			marker1.SetPosition(new_line->Start);
			marker2.SetPosition(new_line->End);
			return true;
		}
		else
		{
			line = new_line;
			return false;
		}
	}
};

void DialogueTimingMarker::SetPosition(int new_position) {
	position = new_position;
	line->CheckMarkers();
}

/// @class AudioTimingControllerDialogue
/// @brief Default timing mode for dialogue subtitles
///
/// Displays a start and end marker for an active subtitle line, and possibly
/// some of the inactive lines. The markers for the active line can be dragged,
/// updating the audio selection and the start/end time of that line. In
/// addition, any markers for inactive lines that start/end at the same time
/// as the active line starts/ends can optionally be dragged along with the
/// active line's markers, updating those lines as well.
class AudioTimingControllerDialogue final : public AudioTimingController {
	/// The rendering style for the active line's start marker
	Pen style_left{"Colour/Audio Display/Line boundary Start", "Audio/Line Boundaries Thickness"};
	/// The rendering style for the active line's end marker
	Pen style_right{"Colour/Audio Display/Line boundary End", "Audio/Line Boundaries Thickness"};
	/// The rendering style for the start and end markers of inactive lines
	Pen style_inactive{"Colour/Audio Display/Line Boundary Inactive Line", "Audio/Line Boundaries Thickness"};

	/// The currently active line
	TimeableLine active_line;

	/// Inactive lines which are currently modifiable
	std::list<TimeableLine> inactive_lines;

	/// Selected lines which are currently modifiable
	std::list<TimeableLine> selected_lines;

	/// All audio markers for active and inactive lines, sorted by position
	std::vector<DialogueTimingMarker*> markers;

	/// Marker provider for video keyframes
	AudioMarkerProviderKeyframes keyframes_provider;

	/// Marker provider for video playback position
	VideoPositionMarkerProvider video_position_provider;

	/// Marker provider for seconds lines
	SecondsMarkerProvider seconds_provider;

	/// The set of lines which have been modified and need to have their
	/// changes applied on commit
	std::set<TimeableLine*> modified_lines;

	/// Commit id for coalescing purposes when in auto commit mode
	int commit_id =-1;

	/// The owning project context
	agi::Context *context;

	/// The time which was clicked on for alt-dragging mode
	int clicked_ms;

	/// Index of marker serving as tap marker
	/// For AudioTimingControllerDialogue:
	/// - 0 is left marker
	/// - 1 is right marker
	size_t tap_marker_idx = 0;

	/// Autocommit option
	const agi::OptionValue *auto_commit = OPT_GET("Audio/Auto/Commit");
	const agi::OptionValue *inactive_line_mode = OPT_GET("Audio/Inactive Lines Display Mode");
	const agi::OptionValue *inactive_line_comments = OPT_GET("Audio/Display/Draw/Inactive Comments");
	const agi::OptionValue *drag_timing = OPT_GET("Audio/Drag Timing");

	agi::signal::Connection commit_connection;
	agi::signal::Connection audio_open_connection;
	agi::signal::Connection inactive_line_mode_connection;
	agi::signal::Connection inactive_line_comment_connection;
	agi::signal::Connection active_line_connection;
	agi::signal::Connection selection_connection;

	/// Update the audio controller's selection
	void UpdateSelection();

	/// Regenerate the list of timeable inactive lines
	void RegenerateInactiveLines();

	/// Regenerate the list of timeable selected lines
	void RegenerateSelectedLines();

	/// Add a line to the list of timeable inactive lines
	void AddInactiveLine(Selection const& sel, AssDialogue *diag);

	/// Regenerate the list of active and inactive line markers
	void RegenerateMarkers();

	/// Get the start markers for the active line and all selected lines
	std::vector<AudioMarker*> GetLeftMarkers();

	/// Get the end markers for the active line and all selected lines
	std::vector<AudioMarker*> GetRightMarkers();

	/// @brief Set the position of markers and announce the change to the world
	/// @param upd_markers Markers to move
	/// @param ms New position of the markers
	void SetMarkers(std::vector<AudioMarker*> const& upd_markers, int ms, int snap_range);

	/// Try to snap all of the active markers to any inactive markers
	/// @param snap_range Maximum distance to snap in milliseconds
	/// @param active     Markers which should be snapped
	/// @return The distance the markers were shifted by
	int SnapMarkers(int snap_range, std::vector<AudioMarker*> const& markers) const;

	/// Commit all pending changes to the file
	/// @param user_triggered Is this a user-initiated commit or an autocommit
	void DoCommit(bool user_triggered);

	void OnSelectedSetChanged();

	// AssFile events
	void OnFileChanged(int type);

public:
	// AudioMarkerProvider interface
	void GetMarkers(const TimeRange &range, AudioMarkerVector &out_markers) const override;
	int GetTapMarkerPosition() const override;
	size_t GetTapMarkerIndex() const override;

	// AudioTimingController interface
	void GetRenderingStyles(AudioRenderingStyleRanges &ranges) const override;
	void GetLabels(TimeRange const& range, std::vector<AudioLabel> &out) const override { }
	void Next(NextMode mode) override;
	void Prev() override;
	void Revert() override;
	void AddLeadIn() override;
	void AddLeadOut() override;
	void ModifyLength(int delta, bool shift_following) override;
	void ModifyStart(int delta) override;
	void MoveTapMarker(int ms) override;
	bool NextTapMarker() override;
	bool IsNearbyMarker(int ms, int sensitivity, bool alt_down) const override;
	std::vector<AudioMarker*> OnLeftClick(int ms, bool ctrl_down, bool alt_down, int sensitivity, int snap_range) override;
	std::vector<AudioMarker*> OnRightClick(int ms, bool ctrl_down, int sensitivity, int snap_range) override;
	void OnMarkerDrag(std::vector<AudioMarker*> const& markers, int new_position, int snap_range) override;

	// We have no warning messages currently, maybe add the old "Modified" message back later?
	wxString GetWarningMessage() const override { return wxString(); }
	TimeRange GetIdealVisibleTimeRange() const override { return active_line; }
	TimeRange GetPrimaryPlaybackRange() const override { return active_line; }
	TimeRange GetActiveLineRange() const override { return active_line; }
	void Commit() override { DoCommit(true); }

	/// Constructor
	/// @param c Project context
	AudioTimingControllerDialogue(agi::Context *c);
};

AudioTimingControllerDialogue::AudioTimingControllerDialogue(agi::Context *c)
: active_line(AudioStyle_Primary, &style_left, &style_right)
, keyframes_provider(c, "Audio/Display/Draw/Keyframes in Dialogue Mode")
, video_position_provider(c)
, context(c)
, commit_connection(c->ass->AddCommitListener(&AudioTimingControllerDialogue::OnFileChanged, this))
, inactive_line_mode_connection(OPT_SUB("Audio/Inactive Lines Display Mode", &AudioTimingControllerDialogue::RegenerateInactiveLines, this))
, inactive_line_comment_connection(OPT_SUB("Audio/Display/Draw/Inactive Comments", &AudioTimingControllerDialogue::RegenerateInactiveLines, this))
, active_line_connection(c->selectionController->AddActiveLineListener(&AudioTimingControllerDialogue::Revert, this))
, selection_connection(c->selectionController->AddSelectionListener(&AudioTimingControllerDialogue::OnSelectedSetChanged, this))
{
	keyframes_provider.AddMarkerMovedListener([=]{ AnnounceMarkerMoved(); });
	video_position_provider.AddMarkerMovedListener([=]{ AnnounceMarkerMoved(); });
	seconds_provider.AddMarkerMovedListener([=]{ AnnounceMarkerMoved(); });

	Revert();
}

void AudioTimingControllerDialogue::GetMarkers(const TimeRange &range, AudioMarkerVector &out_markers) const
{
	// The order matters here; later markers are painted on top of earlier
	// markers, so the markers that we want to end up on top need to appear last

	seconds_provider.GetMarkers(range, out_markers);

	// Copy inactive line markers in the range
	copy(
		boost::lower_bound(markers, range.begin(), marker_ptr_cmp()),
		boost::upper_bound(markers, range.end(), marker_ptr_cmp()),
		back_inserter(out_markers));

	keyframes_provider.GetMarkers(range, out_markers);
	video_position_provider.GetMarkers(range, out_markers);
}

int AudioTimingControllerDialogue::GetTapMarkerPosition() const
{
	assert(tap_marker_idx <= 1);

	if (tap_marker_idx == 0) {
		return *active_line.GetLeftMarker();
	}
	else {
		return *active_line.GetRightMarker();
	}
}

size_t AudioTimingControllerDialogue::GetTapMarkerIndex() const
{
	assert(tap_marker_idx <= 1);
	return tap_marker_idx;
}

void AudioTimingControllerDialogue::OnSelectedSetChanged()
{
	RegenerateSelectedLines();
	RegenerateInactiveLines();
}

void AudioTimingControllerDialogue::OnFileChanged(int type) {
	if (type & AssFile::COMMIT_DIAG_TIME)
		Revert();
	else if (type & AssFile::COMMIT_DIAG_ADDREM)
		RegenerateInactiveLines();
}

void AudioTimingControllerDialogue::GetRenderingStyles(AudioRenderingStyleRanges &ranges) const
{
	active_line.GetStyleRange(&ranges);
	for (auto const& line : selected_lines)
		line.GetStyleRange(&ranges);
	for (auto const& line : inactive_lines)
		line.GetStyleRange(&ranges);
}

void AudioTimingControllerDialogue::Next(NextMode mode)
{
	if (mode == TIMING_UNIT)
	{
		context->selectionController->NextLine();
		return;
	}

	int new_end_ms = *active_line.GetRightMarker();

	cmd::call("grid/line/next/create", context);

	if (mode == LINE_RESET_DEFAULT || active_line.GetLine()->End == 0) {
		const int default_duration = OPT_GET("Timing/Default Duration")->GetInt();
		// Setting right first here so that they don't get switched and the
		// same marker gets set twice
		active_line.GetRightMarker()->SetPosition(new_end_ms + default_duration);
		active_line.GetLeftMarker()->SetPosition(new_end_ms);
		boost::sort(markers, marker_ptr_cmp());
		modified_lines.insert(&active_line);
		UpdateSelection();
	}
}

void AudioTimingControllerDialogue::Prev()
{
	context->selectionController->PrevLine();
}

void AudioTimingControllerDialogue::DoCommit(bool user_triggered)
{
	// Store back new times
	if (modified_lines.size())
	{
		for (auto line : modified_lines)
			line->Apply();

		commit_connection.Block();
		if (user_triggered)
		{
			context->ass->Commit(_("timing"), AssFile::COMMIT_DIAG_TIME);
			commit_id = -1; // never coalesce with a manually triggered commit
		}
		else
		{
			AssDialogue *amend = modified_lines.size() == 1 ? (*modified_lines.begin())->GetLine() : nullptr;
			commit_id = context->ass->Commit(_("timing"), AssFile::COMMIT_DIAG_TIME, commit_id, amend);
		}

		commit_connection.Unblock();
		modified_lines.clear();
	}
}

void AudioTimingControllerDialogue::Revert()
{
	commit_id = -1;
	tap_marker_idx = 0;

	if (AssDialogue *line = context->selectionController->GetActiveLine())
	{
		modified_lines.clear();
		if (active_line.SetLine(line))
		{
			AnnounceUpdatedPrimaryRange();
			if (inactive_line_mode->GetInt() == 0)
				AnnounceUpdatedStyleRanges();
			AnnounceUpdatedTapMarker();
		}
		else
		{
			modified_lines.insert(&active_line);
		}
	}

	RegenerateInactiveLines();
	RegenerateSelectedLines();
}

void AudioTimingControllerDialogue::AddLeadIn()
{
	DialogueTimingMarker *m = active_line.GetLeftMarker();
	SetMarkers({ m }, *m - OPT_GET("Audio/Lead/IN")->GetInt(), 0);
}

void AudioTimingControllerDialogue::AddLeadOut()
{
	DialogueTimingMarker *m = active_line.GetRightMarker();
	SetMarkers({ m }, *m + OPT_GET("Audio/Lead/OUT")->GetInt(), 0);
}

void AudioTimingControllerDialogue::ModifyLength(int delta, bool) {
	DialogueTimingMarker *m = active_line.GetRightMarker();
	SetMarkers({ m },
		std::max<int>(*m + delta * 10, *active_line.GetLeftMarker()), 0);
}

void AudioTimingControllerDialogue::ModifyStart(int delta) {
	DialogueTimingMarker *m = active_line.GetLeftMarker();
	SetMarkers({ m },
		std::min<int>(*m + delta * 10, *active_line.GetRightMarker()), 0);
}

void AudioTimingControllerDialogue::MoveTapMarker(int ms) {
	// Fix rounding error
	ms = (ms + 5) / 10 * 10;

	DialogueTimingMarker *left = active_line.GetLeftMarker();
	DialogueTimingMarker *right = active_line.GetRightMarker();

	clicked_ms = INT_MIN;
	if (tap_marker_idx == 0) {
		// Moving left marker (start time of the line)
		if (ms > *right) SetMarkers({ right }, ms, 0);
		SetMarkers({ left }, ms, 0);
	}
	else {
		// Moving right marker (end time of the line)
		if (ms < *left) SetMarkers({ left }, ms, 0);
		SetMarkers({ right }, ms, 0);
	}
}

bool AudioTimingControllerDialogue::NextTapMarker() {
	if (tap_marker_idx == 0) {
		tap_marker_idx = 1;
		AnnounceUpdatedTapMarker();
		return true;
	}
	return false;
}

bool AudioTimingControllerDialogue::IsNearbyMarker(int ms, int sensitivity, bool alt_down) const
{
	assert(sensitivity >= 0);
	return alt_down || active_line.ContainsMarker(TimeRange(ms-sensitivity, ms+sensitivity));
}

std::vector<AudioMarker*> AudioTimingControllerDialogue::OnLeftClick(int ms, bool ctrl_down, bool alt_down, int sensitivity, int snap_range)
{
	assert(sensitivity >= 0);
	assert(snap_range >= 0);

	std::vector<AudioMarker*> ret;

	clicked_ms = INT_MIN;
	if (alt_down)
	{
		clicked_ms = ms;
		active_line.GetMarkers(&ret);
		for (auto const& line : selected_lines)
			line.GetMarkers(&ret);
		return ret;
	}

	DialogueTimingMarker *left = active_line.GetLeftMarker();
	DialogueTimingMarker *right = active_line.GetRightMarker();

	int dist_l = tabs(*left - ms);
	int dist_r = tabs(*right - ms);

	if (dist_l > sensitivity && dist_r > sensitivity)
	{
		// Clicked far from either marker:
		// Insta-set the left marker to the clicked position and return the
		// right as the dragged one, such that if the user does start dragging,
		// he will create a new selection from scratch
		std::vector<AudioMarker*> jump = GetLeftMarkers();
		ret = drag_timing->GetBool() ? GetRightMarkers() : jump;
		// Get ret before setting as setting may swap left/right
		SetMarkers(jump, ms, snap_range);
		// Also change tap marker to left marker
		tap_marker_idx = 0;
		return ret;
	}

	DialogueTimingMarker *clicked = dist_l <= dist_r ? left : right;

	if (ctrl_down)
	{
		// The use of GetPosition here is important, as otherwise it'll start
		// after lines ending at the same time as the active line begins
		auto it = boost::lower_bound(markers, clicked->GetPosition(), marker_ptr_cmp());
		for (; it != markers.end() && !(*clicked < **it); ++it)
			ret.push_back(*it);
	}
	else
		ret.push_back(clicked);

	// Left-click within drag range should still move the left marker to the
	// clicked position, but not the right marker
	if (clicked == left) {
		SetMarkers(ret, ms, snap_range);
	}

	// Also change tap marker
	if (clicked == left) {
		tap_marker_idx = 0;
	}
	else {
		tap_marker_idx = 1;
	}

	return ret;
}

std::vector<AudioMarker*> AudioTimingControllerDialogue::OnRightClick(int ms, bool ctrl_down, int sensitivity, int snap_range)
{
	if (ctrl_down) {
		// Ctrl-right-click: play audio
		context->audioController->PlayToEnd(ms);
		return {};
	}
	else {
		// Normal right-click: move right marker
		clicked_ms = INT_MIN;
		std::vector<AudioMarker*> ret = GetRightMarkers();
		SetMarkers(ret, ms, snap_range);
		tap_marker_idx = 1;
		return ret;
	}
}

void AudioTimingControllerDialogue::OnMarkerDrag(std::vector<AudioMarker*> const& markers, int new_position, int snap_range)
{
	SetMarkers(markers, new_position, snap_range);
}

void AudioTimingControllerDialogue::UpdateSelection()
{
	AnnounceUpdatedPrimaryRange();
	AnnounceUpdatedStyleRanges();
}

void AudioTimingControllerDialogue::SetMarkers(std::vector<AudioMarker*> const& upd_markers, int ms, int snap_range)
{
	if (upd_markers.empty()) return;

	int shift = clicked_ms != INT_MIN ? ms - clicked_ms : 0;
	if (shift) clicked_ms = ms;

	// Since we're moving markers, the sorted list of markers will need to be
	// resorted. To avoid resorting the entire thing, find the subrange that
	// is effected.
	int min_ms = ms;
	int max_ms = ms;
	for (AudioMarker *upd_marker : upd_markers)
	{
		auto marker = static_cast<DialogueTimingMarker*>(upd_marker);
		if (shift < 0) {
			min_ms = std::min<int>(*marker + shift, min_ms);
			max_ms = std::max<int>(*marker, max_ms);
		}
		else {
			min_ms = std::min<int>(*marker, min_ms);
			max_ms = std::max<int>(*marker + shift, max_ms);
		}
	}

	auto begin = boost::lower_bound(markers, min_ms, marker_ptr_cmp());
	auto end = upper_bound(begin, markers.end(), max_ms, marker_ptr_cmp());

	// Update the markers
	for (auto upd_marker : upd_markers)
	{
		auto marker = static_cast<DialogueTimingMarker*>(upd_marker);
		marker->SetPosition(clicked_ms != INT_MIN ? *marker + shift : ms);
		modified_lines.insert(marker->GetLine());
	}

	int snap = SnapMarkers(snap_range, upd_markers);
	if (clicked_ms != INT_MIN)
		clicked_ms += snap;

	// Resort the range
	sort(begin, end, marker_ptr_cmp());

	if (auto_commit->GetBool()) DoCommit(false);
	UpdateSelection();

	AnnounceMarkerMoved();
}

void AudioTimingControllerDialogue::RegenerateInactiveLines()
{
	using pred = bool(*)(AssDialogue const&);
	auto predicate = inactive_line_comments->GetBool()
		? static_cast<pred>([](AssDialogue const&) { return true; })
		: static_cast<pred>([](AssDialogue const& d) { return !d.Comment; });

	bool was_empty = inactive_lines.empty();
	inactive_lines.clear();

	auto const& sel = context->selectionController->GetSelectedSet();

	switch (int mode = inactive_line_mode->GetInt())
	{
	case 1: // Previous line only
	case 2: // Previous and next lines
		if (AssDialogue *line = context->selectionController->GetActiveLine())
		{
			auto current_line = context->ass->iterator_to(*line);
			if (current_line == context->ass->Events.end())
				break;

			if (current_line != context->ass->Events.begin())
			{
				auto prev = current_line;
				while (--prev != context->ass->Events.begin() && !predicate(*prev)) ;
				if (predicate(*prev))
					AddInactiveLine(sel, &*prev);
			}

			if (mode == 2)
			{
				auto next = std::find_if(++current_line, context->ass->Events.end(), predicate);
				if (next != context->ass->Events.end())
					AddInactiveLine(sel, &*next);
			}
		}
		break;
	case 3: // All inactive lines
	{
		AssDialogue *active_line = context->selectionController->GetActiveLine();
		for (auto& line : context->ass->Events)
		{
			if (&line != active_line && predicate(line))
				AddInactiveLine(sel, &line);
		}
		break;
	}
	default:
		if (was_empty)
		{
			RegenerateMarkers();
			return;
		}
	}

	AnnounceUpdatedStyleRanges();

	RegenerateMarkers();
}

void AudioTimingControllerDialogue::AddInactiveLine(Selection const& sel, AssDialogue *diag)
{
	if (sel.count(diag)) return;

	inactive_lines.emplace_back(AudioStyle_Inactive, &style_inactive, &style_inactive);
	inactive_lines.back().SetLine(diag);
}

void AudioTimingControllerDialogue::RegenerateSelectedLines()
{
	bool was_empty = selected_lines.empty();
	selected_lines.clear();

	AssDialogue *active = context->selectionController->GetActiveLine();
	for (auto line : context->selectionController->GetSelectedSet())
	{
		if (line == active) continue;

		selected_lines.emplace_back(AudioStyle_Selected, &style_inactive, &style_inactive);
		selected_lines.back().SetLine(line);
	}

	if (!selected_lines.empty() || !was_empty)
	{
		AnnounceUpdatedStyleRanges();
		RegenerateMarkers();
	}
}

void AudioTimingControllerDialogue::RegenerateMarkers()
{
	markers.clear();

	active_line.GetMarkers(&markers);
	for (auto const& line : selected_lines)
		line.GetMarkers(&markers);
	for (auto const& line : inactive_lines)
		line.GetMarkers(&markers);
	boost::sort(markers, marker_ptr_cmp());

	AnnounceMarkerMoved();
}

std::vector<AudioMarker*> AudioTimingControllerDialogue::GetLeftMarkers()
{
	std::vector<AudioMarker*> ret;
	ret.reserve(selected_lines.size() + 1);
	ret.push_back(active_line.GetLeftMarker());
	for (auto& line : selected_lines)
		ret.push_back(line.GetLeftMarker());
	return ret;
}

std::vector<AudioMarker*> AudioTimingControllerDialogue::GetRightMarkers()
{
	std::vector<AudioMarker*> ret;
	ret.reserve(selected_lines.size() + 1);
	ret.push_back(active_line.GetRightMarker());
	for (auto& line : selected_lines)
		ret.push_back(line.GetRightMarker());
	return ret;
}

int AudioTimingControllerDialogue::SnapMarkers(int snap_range, std::vector<AudioMarker*> const& active) const
{
	if (snap_range <= 0 || active.empty()) return 0;

	auto marker_range = [&] {
		int front = active.front()->GetPosition();
		int min = front;
		int max = front;
		for (auto m : active)
		{
			auto pos = m->GetPosition();
			if (pos < min) min = pos;
			if (pos > max) max = pos;
		}
		return TimeRange{min - snap_range, max + snap_range};
	}();

	std::vector<int> inactive_markers;
	inactive_markers.reserve(inactive_lines.size() * 2 + selected_lines.size() * 2 + 2 - active.size());

	// Add a marker to the set to check for snaps if it's in the right time
	// range, isn't at the same place as a marker already in the set, and isn't
	// one of the markers being moved
	auto add_inactive = [&](const DialogueTimingMarker *m, bool check)
	{
		if (!marker_range.contains(*m)) return;
		if (!inactive_markers.empty() && inactive_markers.back() == *m) return;
		if (check && boost::find(active, m) != end(active)) return;
		inactive_markers.push_back(*m);
	};

	bool moving_entire_selection = clicked_ms != INT_MIN;
	for (auto const& line : inactive_lines)
	{
		// If we're alt-dragging the entire selection, there can't be any
		// markers from inactive lines in the active set, so no need to check
		// for them
		add_inactive(line.GetLeftMarker(), !moving_entire_selection);
		add_inactive(line.GetRightMarker(), !moving_entire_selection);
	}

	// And similarly, there can't be any inactive markers from selected lines
	if (!moving_entire_selection)
	{
		for (auto const& line : selected_lines)
		{
			add_inactive(line.GetLeftMarker(), true);
			add_inactive(line.GetRightMarker(), true);
		}
		add_inactive(active_line.GetLeftMarker(), true);
		add_inactive(active_line.GetRightMarker(), true);
	}

	int snap_distance = INT_MAX;
	auto check = [&](int marker, int pos)
	{
		auto dist = marker - pos;
		if (tabs(dist) < tabs(snap_distance))
			snap_distance = dist;
	};

	int prev = -1;
	AudioMarkerVector snap_markers;
	for (const auto active_marker : active)
	{
		auto pos = active_marker->GetPosition();
		if (pos == prev) continue;

		snap_markers.clear();
		TimeRange range(pos - snap_range, pos + snap_range);
		keyframes_provider.GetMarkers(range, snap_markers);
		video_position_provider.GetMarkers(range, snap_markers);

		for (const auto marker : snap_markers)
		{
			check(marker->GetPosition(), pos);
			if (snap_distance == 0) return 0;
		}

		for (auto it = boost::lower_bound(inactive_markers, range.begin()); it != end(inactive_markers); ++it)
		{
			check(*it, pos);
			if (snap_distance == 0) return 0;
			if (*it > pos) break;
		}
	}

	if (tabs(snap_distance) > snap_range)
		return 0;

	for (auto m : active)
		static_cast<DialogueTimingMarker *>(m)->SetPosition(m->GetPosition() + snap_distance);
	return snap_distance;
}

} // namespace {

std::unique_ptr<AudioTimingController> CreateDialogueTimingController(agi::Context *c)
{
	return agi::make_unique<AudioTimingControllerDialogue>(c);
}
