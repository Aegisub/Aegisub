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

/// @file audio_timing.h
/// @brief Construction-functions for timing controller objects
/// @ingroup audio_ui

class AssDialogue;
class AssFile;
class AssKaraoke;
class AudioRenderingStyleRanges;
namespace agi { struct Context; }

#include "audio_marker.h"

/// @class AudioTimingController
/// @brief Base class for objects controlling audio timing
///
/// There is just one active audio timing controller at a time per audio
/// controller. The timing controller manages the timing mode and supplies
/// markers that can be manipulated to the audio display, as well as the
/// current selection.
///
/// The timing controller must then be sent the marker drag events as well as
/// clicks in empty areas of the audio display.
class AudioTimingController : public AudioMarkerProvider, public AudioLabelProvider {
protected:
	/// The primary playback range has changed, usually as a result of user interaction.
	agi::signal::Signal<> AnnounceUpdatedPrimaryRange;

	/// One or more rendering style ranges have changed in the timing controller.
	agi::signal::Signal<> AnnounceUpdatedStyleRanges;

	/// The tap marker has changed in the timing controller.
	agi::signal::Signal<> AnnounceUpdatedTapMarker;

public:
	/// @brief Get any warning message to show in the audio display
	/// @return The warning message to show, may be empty if there is none
	virtual wxString GetWarningMessage() const = 0;

	/// @brief Get the time range the user is most likely to want to see for the current state
	/// @return A time range
	///
	/// This is used for "bring working area into view" operations.
	virtual TimeRange GetIdealVisibleTimeRange() const = 0;

	/// @brief Get the primary playback range
	/// @return A time range
	///
	/// Get the time range the user is most likely to want to play back
	/// currently.
	virtual TimeRange GetPrimaryPlaybackRange() const = 0;

	/// @brief Get the active line's time
	/// @return A time range
	///
	/// Get the time range which the active line would have if any pending
	/// modifications were committed.
	virtual TimeRange GetActiveLineRange() const = 0;

	/// @brief Get all rendering style ranges
	/// @param[out] ranges Rendering ranges will be added to this
	virtual void GetRenderingStyles(AudioRenderingStyleRanges &ranges) const = 0;

	/// @brief Return the position of the tap marker
	virtual int GetTapMarkerPosition() const = 0;

	/// @brief Return the index of the tap marker
	virtual size_t GetTapMarkerIndex() const = 0;

	enum NextMode {
		/// Advance to the next timing unit, whether it's a line or a sub-part
		/// of a line such as a karaoke syllable
		TIMING_UNIT = 0,

		/// @brief Advance to the next line
		///
		/// This may create a new line if there are no more lines in the file,
		/// but should never modify existing lines
		LINE,

		/// @brief Advance to the next line using default timing
		///
		/// This may create new lines when needed, and should discard any
		/// existing timing data in favor of the defaults
		LINE_RESET_DEFAULT
	};

	/// @brief Go to next timing unit
	/// @param mode What sort of timing unit should be advanced to
	virtual void Next(NextMode mode) = 0;

	/// @brief Go to the previous timing unit
	///
	/// Rewinds the timing controller to the previous timing unit.
	virtual void Prev() = 0;

	/// @brief Commit all changes
	///
	/// Stores all changes permanently.
	virtual void Commit() = 0;

	/// @brief Revert all changes
	///
	/// Revert all changes to the last committed state.
	virtual void Revert() = 0;

	/// Add lead-in time to the current timing unit
	virtual void AddLeadIn() = 0;

	/// Add lead-out time to the current timing unit
	virtual void AddLeadOut() = 0;

	/// Modify the length of the current and possibly following timing units
	/// @param delta Amount to add in centiseconds
	/// @param shift_following Should the following things be shifted by delta?
	virtual void ModifyLength(int delta, bool shift_following) = 0;

	/// Modify the start time of the current timing unit
	/// @param delta Amount to add in centiseconds
	virtual void ModifyStart(int delta) = 0;

	/// Move tap marker position to given position
	/// @param position to move marker to
	virtual void MoveTapMarker(int ms) = 0;

	/// Go to next tap marker
	/// @return True if moved to the next marker, False if tap marker is already
	///         the last marker of the line
	virtual bool NextTapMarker() = 0;

	/// @brief Determine if a position is close to a draggable marker
	/// @param ms          The time in milliseconds to test
	/// @param sensitivity Distance in milliseconds to consider markers as nearby
	/// @return True if a marker is close by the given time, as defined by sensitivity
	///
	/// This is solely for hit-testing against draggable markers, for
	/// controlling the mouse cursor.
	virtual bool IsNearbyMarker(int ms, int sensitivity, bool alt_down) const = 0;

	/// @brief The user pressed the left mouse button on the audio
	/// @param ms          The time in milliseconds the user clicked
	/// @param ctrl_down   Is the user currently holding the ctrl key down?
	/// @param alt_down    Is the user currently holding the alt key down?
	/// @param sensitivity Distance in milliseconds to consider existing markers
	/// @param snap_range  Maximum snapping range in milliseconds
	/// @return All audio markers at the clicked position which are eligible
	///         to be dragged, if any.
	virtual std::vector<AudioMarker*> OnLeftClick(int ms, bool ctrl_down, bool alt_down, int sensitivity, int snap_range) = 0;

	/// @brief The user pressed the right mouse button on the audio
	/// @param ms          The time in milliseconds the user clicked
	/// @param ctrl_down   Is the user currently holding the ctrl key down?
	/// @param sensitivity Distance in milliseconds to consider existing markers
	/// @param snap_range  Maximum snapping range in milliseconds
	/// @return All audio markers at the clicked position which are eligible
	///         to be dragged, if any.
	virtual std::vector<AudioMarker*> OnRightClick(int ms, bool ctrl_down, int sensitivity, int snap_range) = 0;

	/// @brief The user dragged one or more timing markers
	/// @param marker       The markers being dragged. This is guaranteed to be
	///                     a vector returned from OnLeftClick or OnRightClick.
	/// @param new_position Time position the marker was dragged to
	/// @param snap_range   Maximum snapping range in milliseconds
	virtual void OnMarkerDrag(std::vector<AudioMarker*> const& marker, int new_position, int snap_range) = 0;

	/// @brief Destructor
	virtual ~AudioTimingController() = default;

	DEFINE_SIGNAL_ADDERS(AnnounceUpdatedPrimaryRange, AddUpdatedPrimaryRangeListener)
	DEFINE_SIGNAL_ADDERS(AnnounceUpdatedStyleRanges, AddUpdatedStyleRangesListener)
	DEFINE_SIGNAL_ADDERS(AnnounceUpdatedTapMarker, AddUpdatedTapMarkerListener)
};

/// @brief Create a standard dialogue audio timing controller
/// @param c Project context
std::unique_ptr<AudioTimingController> CreateDialogueTimingController(agi::Context *c);

/// @brief Create a karaoke audio timing controller
/// @param c Project context
/// @param kara Karaoke model
std::unique_ptr<AudioTimingController> CreateKaraokeTimingController(agi::Context *c, AssKaraoke *kara, agi::signal::Connection& file_changed);
