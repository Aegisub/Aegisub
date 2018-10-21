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

#include <libaegisub/signal.h>

#include "ass_dialogue.h"
#include "ass_file.h"
#include "ass_karaoke.h"
#include "audio_controller.h"
#include "audio_marker.h"
#include "audio_rendering_style.h"
#include "audio_timing.h"
#include "compat.h"
#include "include/aegisub/context.h"
#include "options.h"
#include "pen.h"
#include "selection_controller.h"
#include "utils.h"

#include <libaegisub/make_unique.h>

#include <boost/range/algorithm/copy.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/sliced.hpp>
#include <wx/intl.h>

/// @class KaraokeMarker
/// @brief AudioMarker implementation for AudioTimingControllerKaraoke
class KaraokeMarker final : public AudioMarker {
	int position;
	Pen *pen = nullptr;
	FeetStyle style = Feet_None;
public:

	int GetPosition() const override { return position; }
	wxPen GetStyle() const override { return *pen; }
	FeetStyle GetFeet() const override { return style; }

	void Move(int new_pos) { position = new_pos; }

	KaraokeMarker(int position) : position(position) { }

	KaraokeMarker(int position, Pen *pen, FeetStyle style)
	: position(position)
	, pen(pen)
	, style(style)
	{
	}

	operator int() const { return position; }
};

/// @class AudioTimingControllerKaraoke
/// @brief Karaoke timing mode for timing subtitles
///
/// Displays the active line with draggable markers between each pair of
/// adjacent syllables, along with the text of each syllable.
///
/// This does not support \kt, as it inherently requires that the end time of
/// one syllable be the same as the start time of the next one.
class AudioTimingControllerKaraoke final : public AudioTimingController {
	std::vector<agi::signal::Connection> connections;
	agi::signal::Connection& file_changed_slot;

	agi::Context *c;          ///< Project context
	AssDialogue *active_line; ///< Currently active line
	AssKaraoke *kara;         ///< Parsed karaoke model provided by karaoke controller

	size_t cur_syl = 0; ///< Index of currently selected syllable in the line

	/// Pen used for the mid-syllable markers
	Pen separator_pen{"Colour/Audio Display/Syllable Boundaries", "Audio/Line Boundaries Thickness", wxPENSTYLE_DOT};
	/// Pen used for the start-of-line marker
	Pen start_pen{"Colour/Audio Display/Line boundary Start", "Audio/Line Boundaries Thickness"};
	/// Pen used for the end-of-line marker
	Pen end_pen{"Colour/Audio Display/Line boundary End", "Audio/Line Boundaries Thickness"};

	/// Immobile marker for the beginning of the line
	KaraokeMarker start_marker;
	/// Immobile marker for the end of the line
	KaraokeMarker end_marker;
	/// Mobile markers between each pair of syllables
	std::vector<KaraokeMarker> markers;

	/// Marker provider for video keyframes
	AudioMarkerProviderKeyframes keyframes_provider;

	/// Marker provider for video playback position
	VideoPositionMarkerProvider video_position_provider;

	/// Labels containing the stripped text of each syllable
	std::vector<AudioLabel> labels;

	 /// Should changes be automatically commited?
	bool auto_commit = OPT_GET("Audio/Auto/Commit")->GetBool();
	int commit_id = -1;   ///< Last commit id used for an autocommit
	bool pending_changes; ///< Are there any pending changes to be committed?

	void DoCommit();
	void ApplyLead(bool announce_primary);
	int MoveMarker(KaraokeMarker *marker, int new_position);
	void AnnounceChanges(int syl);

public:
	// AudioTimingController implementation
	void GetMarkers(const TimeRange &range, AudioMarkerVector &out_markers) const override;
	wxString GetWarningMessage() const override { return ""; }
	TimeRange GetIdealVisibleTimeRange() const override;
	void GetRenderingStyles(AudioRenderingStyleRanges &ranges) const override;
	TimeRange GetPrimaryPlaybackRange() const override;
	TimeRange GetActiveLineRange() const override;
	void GetLabels(const TimeRange &range, std::vector<AudioLabel> &out_labels) const override;
	void Next(NextMode mode) override;
	void Prev() override;
	void Commit() override;
	void Revert() override;
	void AddLeadIn() override;
	void AddLeadOut() override;
	void ModifyLength(int delta, bool shift_following) override;
	void ModifyStart(int delta) override;
	bool IsNearbyMarker(int ms, int sensitivity, bool) const override;
	std::vector<AudioMarker*> OnLeftClick(int ms, bool, bool, int sensitivity, int) override;
	std::vector<AudioMarker*> OnRightClick(int ms, bool, int, int) override;
	void OnMarkerDrag(std::vector<AudioMarker*> const& marker, int new_position, int) override;

	AudioTimingControllerKaraoke(agi::Context *c, AssKaraoke *kara, agi::signal::Connection& file_changed);
};

std::unique_ptr<AudioTimingController> CreateKaraokeTimingController(agi::Context *c, AssKaraoke *kara, agi::signal::Connection& file_changed)
{
	return agi::make_unique<AudioTimingControllerKaraoke>(c, kara, file_changed);
}

AudioTimingControllerKaraoke::AudioTimingControllerKaraoke(agi::Context *c, AssKaraoke *kara, agi::signal::Connection& file_changed)
: file_changed_slot(file_changed)
, c(c)
, active_line(c->selectionController->GetActiveLine())
, kara(kara)
, start_marker(active_line->Start, &start_pen, AudioMarker::Feet_Right)
, end_marker(active_line->End, &end_pen, AudioMarker::Feet_Left)
, keyframes_provider(c, "Audio/Display/Draw/Keyframes in Karaoke Mode")
, video_position_provider(c)
{
	connections.push_back(kara->AddSyllablesChangedListener(&AudioTimingControllerKaraoke::Revert, this));
	connections.push_back(OPT_SUB("Audio/Auto/Commit", [=](agi::OptionValue const& opt) { auto_commit = opt.GetBool(); }));

	keyframes_provider.AddMarkerMovedListener([=]{ AnnounceMarkerMoved(); });
	video_position_provider.AddMarkerMovedListener([=]{ AnnounceMarkerMoved(); });

	Revert();
}

void AudioTimingControllerKaraoke::Next(NextMode mode) {
	// Don't create new lines since it's almost never useful to k-time a line
	// before dialogue timing it
	if (mode != TIMING_UNIT)
		cur_syl = markers.size();

	++cur_syl;
	if (cur_syl > markers.size()) {
		--cur_syl;
		c->selectionController->NextLine();
	}
	else {
		AnnounceUpdatedPrimaryRange();
		AnnounceUpdatedStyleRanges();
	}

	c->audioController->PlayPrimaryRange();
}

void AudioTimingControllerKaraoke::Prev() {
	if (cur_syl == 0) {
		AssDialogue *old_line = active_line;
		c->selectionController->PrevLine();
		if (old_line != active_line) {
			cur_syl = markers.size();
			AnnounceUpdatedPrimaryRange();
			AnnounceUpdatedStyleRanges();
		}
	}
	else {
		--cur_syl;
		AnnounceUpdatedPrimaryRange();
		AnnounceUpdatedStyleRanges();
	}

	c->audioController->PlayPrimaryRange();
}

void AudioTimingControllerKaraoke::GetRenderingStyles(AudioRenderingStyleRanges &ranges) const
{
	TimeRange sr = GetPrimaryPlaybackRange();
	ranges.AddRange(sr.begin(), sr.end(), AudioStyle_Primary);
	ranges.AddRange(start_marker, end_marker, AudioStyle_Selected);
}

TimeRange AudioTimingControllerKaraoke::GetPrimaryPlaybackRange() const {
	return TimeRange(
		cur_syl > 0 ? markers[cur_syl - 1] : start_marker,
		cur_syl < markers.size() ? markers[cur_syl] : end_marker);
}

TimeRange AudioTimingControllerKaraoke::GetActiveLineRange() const {
	return TimeRange(start_marker, end_marker);
}

TimeRange AudioTimingControllerKaraoke::GetIdealVisibleTimeRange() const {
	return GetActiveLineRange();
}

void AudioTimingControllerKaraoke::GetMarkers(TimeRange const& range, AudioMarkerVector &out) const {
	size_t i;
	for (i = 0; i < markers.size() && markers[i] < range.begin(); ++i) ;
	for (; i < markers.size() && markers[i] < range.end(); ++i)
		out.push_back(&markers[i]);

	if (range.contains(start_marker)) out.push_back(&start_marker);
	if (range.contains(end_marker)) out.push_back(&end_marker);

	keyframes_provider.GetMarkers(range, out);
	video_position_provider.GetMarkers(range, out);
}

void AudioTimingControllerKaraoke::DoCommit() {
	active_line->Text = kara->GetText();
	file_changed_slot.Block();
	commit_id = c->ass->Commit(_("karaoke timing"), AssFile::COMMIT_DIAG_TEXT, commit_id, active_line);
	file_changed_slot.Unblock();
	pending_changes = false;
}

void AudioTimingControllerKaraoke::Commit() {
	if (!auto_commit && pending_changes)
		DoCommit();
}

void AudioTimingControllerKaraoke::Revert() {
	active_line = c->selectionController->GetActiveLine();

	cur_syl = 0;
	commit_id = -1;
	pending_changes = false;

	start_marker.Move(active_line->Start);
	end_marker.Move(active_line->End);

	markers.clear();
	labels.clear();

	markers.reserve(kara->size());
	labels.reserve(kara->size());

	for (auto it = kara->begin(); it != kara->end(); ++it) {
		if (it != kara->begin())
			markers.emplace_back(it->start_time, &separator_pen, AudioMarker::Feet_None);
		labels.push_back(AudioLabel{to_wx(it->text), TimeRange(it->start_time, it->start_time + it->duration)});
	}

	AnnounceUpdatedPrimaryRange();
	AnnounceUpdatedStyleRanges();
	AnnounceMarkerMoved();
}

void AudioTimingControllerKaraoke::AddLeadIn() {
	start_marker.Move(start_marker - OPT_GET("Audio/Lead/IN")->GetInt());
	labels.front().range = TimeRange(start_marker, labels.front().range.end());
	ApplyLead(cur_syl == 0);
}

void AudioTimingControllerKaraoke::AddLeadOut() {
	end_marker.Move(end_marker + OPT_GET("Audio/Lead/OUT")->GetInt());
	labels.back().range = TimeRange(labels.back().range.begin(), end_marker);
	ApplyLead(cur_syl == markers.size());
}

void AudioTimingControllerKaraoke::ApplyLead(bool announce_primary) {
	active_line->Start = (int)start_marker;
	active_line->End = (int)end_marker;
	kara->SetLineTimes(start_marker, end_marker);
	if (!announce_primary)
		AnnounceUpdatedStyleRanges();
	AnnounceChanges(announce_primary ? cur_syl : cur_syl + 2);
}

void AudioTimingControllerKaraoke::ModifyLength(int delta, bool shift_following) {
	if (cur_syl == markers.size()) return;

	int cur, end, step;
	if (delta < 0) {
		cur = cur_syl;
		end = shift_following ? markers.size() : cur_syl + 1;
		step = 1;
	}
	else {
		cur = shift_following ? markers.size() - 1 : cur_syl;
		end = cur_syl - 1;
		step = -1;
	}

	for (; cur != end; cur += step) {
		MoveMarker(&markers[cur], markers[cur] + delta * 10);
	}
	AnnounceChanges(cur_syl);
}

void AudioTimingControllerKaraoke::ModifyStart(int delta) {
	if (cur_syl == 0) return;
	MoveMarker(&markers[cur_syl - 1], markers[cur_syl - 1] + delta * 10);
	AnnounceChanges(cur_syl);
}

bool AudioTimingControllerKaraoke::IsNearbyMarker(int ms, int sensitivity, bool) const {
	TimeRange range(ms - sensitivity, ms + sensitivity);
	return any_of(markers.begin(), markers.end(), [&](KaraokeMarker const& km) {
		return range.contains(km);
	});
}

template<typename Out, typename In>
static std::vector<Out *> copy_ptrs(In &vec, size_t start, size_t end) {
	std::vector<Out *> ret;
	ret.reserve(end - start);
	for (; start < end; ++start)
		ret.push_back(&vec[start]);
	return ret;
}

std::vector<AudioMarker*> AudioTimingControllerKaraoke::OnLeftClick(int ms, bool ctrl_down, bool, int sensitivity, int) {
	TimeRange range(ms - sensitivity, ms + sensitivity);

	size_t syl = distance(markers.begin(), lower_bound(markers.begin(), markers.end(), ms));
	if (syl < markers.size() && range.contains(markers[syl]))
		return copy_ptrs<AudioMarker>(markers, syl, ctrl_down ? markers.size() : syl + 1);
	if (syl > 0 && range.contains(markers[syl - 1]))
		return copy_ptrs<AudioMarker>(markers, syl - 1, ctrl_down ? markers.size() : syl);

	cur_syl = syl;

	AnnounceUpdatedPrimaryRange();
	AnnounceUpdatedStyleRanges();

	return {};
}

std::vector<AudioMarker*> AudioTimingControllerKaraoke::OnRightClick(int ms, bool, int, int) {
	cur_syl = distance(markers.begin(), lower_bound(markers.begin(), markers.end(), ms));

	AnnounceUpdatedPrimaryRange();
	AnnounceUpdatedStyleRanges();
	c->audioController->PlayPrimaryRange();

	return {};
}

int AudioTimingControllerKaraoke::MoveMarker(KaraokeMarker *marker, int new_position) {
	// No rearranging of syllables allowed
	new_position = mid(
		marker == &markers.front() ? start_marker.GetPosition() : (marker - 1)->GetPosition(),
		new_position,
		marker == &markers.back() ? end_marker.GetPosition() : (marker + 1)->GetPosition());

	if (new_position == marker->GetPosition())
		return -1;

	marker->Move(new_position);

	size_t syl = marker - &markers.front() + 1;
	kara->SetStartTime(syl, new_position);

	labels[syl - 1].range = TimeRange(labels[syl - 1].range.begin(), new_position);
	labels[syl].range = TimeRange(new_position, labels[syl].range.end());

	return syl;
}

void AudioTimingControllerKaraoke::AnnounceChanges(int syl) {
	if (syl < 0) return;

	if (syl == cur_syl || syl == cur_syl + 1) {
		AnnounceUpdatedPrimaryRange();
		AnnounceUpdatedStyleRanges();
	}
	AnnounceMarkerMoved();
	AnnounceLabelChanged();

	if (auto_commit)
		DoCommit();
	else {
		pending_changes = true;
		commit_id = -1;
	}
}

void AudioTimingControllerKaraoke::OnMarkerDrag(std::vector<AudioMarker*> const& m, int new_position, int) {
	// Fix rounding error
	new_position = (new_position + 5) / 10 * 10;
	int old_position = m[0]->GetPosition();
	int syl = MoveMarker(static_cast<KaraokeMarker *>(m[0]), new_position);
	if (syl < 0) return;

	if (m.size() > 1) {
		int delta = m[0]->GetPosition() - old_position;
		for (AudioMarker *marker : m | boost::adaptors::sliced(1, m.size()))
			MoveMarker(static_cast<KaraokeMarker *>(marker), marker->GetPosition() + delta);
		syl = cur_syl;
	}

	AnnounceChanges(syl);
}

void AudioTimingControllerKaraoke::GetLabels(TimeRange const& range, std::vector<AudioLabel> &out) const {
	copy(labels | boost::adaptors::filtered([&](AudioLabel const& l) {
		return range.overlaps(l.range);
	}), back_inserter(out));
}
