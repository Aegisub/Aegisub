// Copyright (c) 2005, Rodrigo Braz Monteiro
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

#include <memory>
#include <wx/frame.h>
#include <wx/timer.h>

class AegisubApp;
class AsyncVideoProvider;
class AudioBox;
class AudioProvider;
class VideoBox;
namespace agi { struct Context; class OptionValue; }

class FrameMain : public wxFrame {
	friend class AegisubApp;

	std::unique_ptr<agi::Context> context;

    // XXX: Make Freeze()/Thaw() noops on GTK, this seems to be buggy
#ifdef __WXGTK__
    void Freeze(void) {}
    void Thaw(void) {}
#endif

	bool showVideo = true; ///< Is the video display shown?
	bool showAudio = true; ///< Is the audio display shown?
	wxTimer StatusClear;   ///< Status bar timeout timer

	void InitContents();

	void UpdateTitle();

	void OnKeyDown(wxKeyEvent &event);
	void OnMouseWheel(wxMouseEvent &evt);
	void OnMove(wxMoveEvent &evt);

	void OnStatusClear(wxTimerEvent &event);
	void OnCloseWindow (wxCloseEvent &event);

	void OnAudioOpen(AudioProvider *provider);
	void OnVideoOpen(AsyncVideoProvider *provider);
	void OnVideoDetach(agi::OptionValue const& opt);
	void OnSubtitlesOpen();

	void EnableToolBar(agi::OptionValue const& opt);

	AudioBox *audioBox;      ///< The audio area
	VideoBox *videoBox;      ///< The video area

	wxSizer *MainSizer;  ///< Arranges things from top to bottom in the window
	wxSizer *TopSizer;   ///< Arranges video box and tool box from left to right
	wxSizer *ToolsSizer; ///< Arranges audio and editing areas top to bottom

public:
	FrameMain();
	~FrameMain();

	/// Set the status bar text
	/// @param text New status bar text
	/// @param ms Time in milliseconds that the message should be visible
	void StatusTimeout(wxString text,int ms=10000);

	/// @brief Set the video and audio display visibility
	/// @param video -1: leave unchanged; 0: hide; 1: show
	/// @param audio -1: leave unchanged; 0: hide; 1: show
	void SetDisplayMode(int showVid,int showAudio);

	bool IsVideoShown() const { return showVideo; }
	bool IsAudioShown() const { return showAudio; }

	DECLARE_EVENT_TABLE()
};
