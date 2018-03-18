// Copyright (c) 2005-2009, Rodrigo Braz Monteiro, Niels Martin Hansen
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

#pragma once

#include <libaegisub/fs_fwd.h>

#include <cstdint>
#include <string>

#include <wx/bitmap.h>
#include <wx/string.h>

class wxKeyEvent;
class wxMouseEvent;
class wxStyledTextCtrl;
class wxWindow;

wxString PrettySize(int bytes);

std::string float_to_string(double val);

/// @brief Get the smallest power of two that is greater or equal to x
///
/// Algorithm from http://bob.allegronetwork.com/prog/tricks.html
int SmallestPowerOf2(int x);

/// @brief Launch a new copy of Aegisub.
///
/// Contrary to what the name suggests, this does not close the currently
/// running process.
void RestartAegisub();

/// Add the OS X 10.7+ full-screen button to a window
void AddFullScreenButton(wxWindow *window);

void SetFloatOnParent(wxWindow *window);

// Set a title reflecting a file path
void SetTitleWithFilename(wxWindow *window, agi::fs::path const& name);


/// Forward a mouse wheel event to the window under the mouse if needed
/// @param source The initial target of the wheel event
/// @param evt The event
/// @return Should the calling code process the event?
bool ForwardMouseWheelEvent(wxWindow *source, wxMouseEvent &evt);

/// Clean up the given cache directory, limiting the size to max_size
/// @param directory Directory to clean
/// @param file_type Wildcard pattern for files to clean up
/// @param max_size Maximum size of directory in MB
/// @param max_files Maximum number of files
void CleanCache(agi::fs::path const& directory, std::string const& file_type, uint64_t max_size, uint64_t max_files = -1);

/// @brief Templated abs() function
template <typename T> T tabs(T x) { return x < 0 ? -x : x; }

/// Get the middle value of a, b, and c (i.e. clamp b to [a,c])
/// @precondition a <= c
template<typename T> inline T mid(T a, T b, T c) {
	return a > b ? a : (b > c ? c : b);
}

/// Get the text contents of the clipboard, or empty string on failure
std::string GetClipboard();
/// Try to set the clipboard to the given string
void SetClipboard(std::string const& new_value);
void SetClipboard(wxBitmap const& new_value);

#define countof(array) (sizeof(array) / sizeof(array[0]))

wxString FontFace(std::string opt_prefix);

agi::fs::path OpenFileSelector(wxString const& message, std::string const& option_name, std::string const& default_filename, std::string const& default_extension, std::string const& wildcard, wxWindow *parent);
agi::fs::path SaveFileSelector(wxString const& message, std::string const& option_name, std::string const& default_filename, std::string const& default_extension, std::string const& wildcard, wxWindow *parent);

wxString LocalizedLanguageName(wxString const& lang);

namespace osx {
	/// Make the given menu the OS X Window menu
	void make_windows_menu(wxMenu *wxmenu);
	/// Activate a top-level document window other than the given one
	bool activate_top_window_other_than(wxFrame *wx);
	// Bring all windows to the front, maintaining relative z-order
	void bring_to_front();

namespace ime {
	/// Inject the IME helper into the given wxSTC
	void inject(wxStyledTextCtrl *ctrl);
	/// Invalidate any pending text from the IME
	void invalidate(wxStyledTextCtrl *ctrl);
	/// Give the IME a chance to process a key event and return whether it did
	bool process_key_event(wxStyledTextCtrl *ctrl, wxKeyEvent &);
}
}
