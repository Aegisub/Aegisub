// Copyright (c) 2005-2006, Rodrigo Braz Monteiro
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

#include "utils.h"

#include "compat.h"
#include "format.h"
#include "options.h"
#include "retina_helper.h"

#include <libaegisub/dispatch.h>
#include <libaegisub/fs.h>
#include <libaegisub/log.h>

#ifdef __UNIX__
#include <unistd.h>
#endif
#include <boost/filesystem/path.hpp>
#include <map>
#include <unicode/locid.h>
#include <unicode/unistr.h>
#include <wx/clipbrd.h>
#include <wx/filedlg.h>
#include <wx/stdpaths.h>
#include <wx/window.h>

#ifdef __APPLE__
#include <libaegisub/util_osx.h>
#include <CoreText/CTFont.h>
#endif

using namespace icu;

/// @brief There shall be no kiB, MiB stuff here Pretty reading of size
wxString PrettySize(int bytes) {
	const char *suffix[] = { "", "kB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB" };

	// Set size
	size_t i = 0;
	double size = bytes;
	while (size > 1024 && i + 1 < sizeof(suffix) / sizeof(suffix[0])) {
		size /= 1024.0;
		i++;
	}

	// Set number of decimal places
	const char *fmt = "%.0f";
	if (size < 10)
		fmt = "%.2f";
	else if (size < 100)
		fmt = "%1.f";
	return agi::wxformat(fmt, size) + " " + suffix[i];
}

std::string float_to_string(double val) {
	std::string s = agi::format("%.3f", val);
	size_t pos = s.find_last_not_of("0");
	if (pos != s.find(".")) ++pos;
	s.erase(begin(s) + pos, end(s));
	return s;
}

int SmallestPowerOf2(int x) {
	x--;
	x |= (x >> 1);
	x |= (x >> 2);
	x |= (x >> 4);
	x |= (x >> 8);
	x |= (x >> 16);
	x++;
	return x;
}

#ifndef __WXMAC__
void RestartAegisub() {
	config::opt->Flush();

#if defined(__WXMSW__)
	wxExecute("\"" + wxStandardPaths::Get().GetExecutablePath() + "\"");
#else
	wxExecute(wxStandardPaths::Get().GetExecutablePath());
#endif
}
#endif

bool ForwardMouseWheelEvent(wxWindow *source, wxMouseEvent &evt) {
	wxWindow *target = wxFindWindowAtPoint(wxGetMousePosition());
	if (!target || target == source) return true;

	// If the mouse is over a parent of the source window just pretend it's
	// over the source window, so that the mouse wheel works on borders and such
	wxWindow *parent = source->GetParent();
	while (parent && parent != target) parent = parent->GetParent();
	if (parent == target) return true;

	// Otherwise send it to the new target
	target->GetEventHandler()->ProcessEvent(evt);
	evt.Skip(false);
	return false;
}

std::string GetClipboard() {
	wxString data;
	wxClipboard *cb = wxClipboard::Get();
	if (cb->Open()) {
		if (cb->IsSupported(wxDF_TEXT) || cb->IsSupported(wxDF_UNICODETEXT)) {
			wxTextDataObject raw_data;
			cb->GetData(raw_data);
			data = raw_data.GetText();
		}
		cb->Close();
	}
	return from_wx(data);
}

void SetClipboard(std::string const& new_data) {
	wxClipboard *cb = wxClipboard::Get();
	if (cb->Open()) {
		cb->SetData(new wxTextDataObject(to_wx(new_data)));
		cb->Flush();
		cb->Close();
	}
}

void SetClipboard(wxBitmap const& new_data) {
	wxClipboard *cb = wxClipboard::Get();
	if (cb->Open()) {
		cb->SetData(new wxBitmapDataObject(new_data));
		cb->Flush();
		cb->Close();
	}
}

void CleanCache(agi::fs::path const& directory, std::string const& file_type, uint64_t max_size, uint64_t max_files) {
	static std::unique_ptr<agi::dispatch::Queue> queue;
	if (!queue)
		queue = agi::dispatch::Create();

	max_size <<= 20;
	if (max_files == 0)
		max_files = std::numeric_limits<uint64_t>::max();
	queue->Async([=]{
		LOG_D("utils/clean_cache") << "cleaning " << directory/file_type;
		uint64_t total_size = 0;
		using cache_item = std::pair<int64_t, agi::fs::path>;
		std::vector<cache_item> cachefiles;
		for (auto const& file : agi::fs::DirectoryIterator(directory, file_type)) {
			agi::fs::path path = directory/file;
			cachefiles.push_back({agi::fs::ModifiedTime(path), path});
			total_size += agi::fs::Size(path);
		}

		if (cachefiles.size() <= max_files && total_size <= max_size) {
			LOG_D("utils/clean_cache") << agi::format("cache does not need cleaning (maxsize=%d, cursize=%d, maxfiles=%d, numfiles=%d), exiting"
				, max_size, total_size, max_files, cachefiles.size());
			return;
		}

		sort(begin(cachefiles), end(cachefiles), [](cache_item const& a, cache_item const& b) {
			return a.first < b.first;
		});

		int deleted = 0;
		for (auto const& i : cachefiles) {
			// stop cleaning?
			if ((total_size <= max_size && cachefiles.size() - deleted <= max_files) || cachefiles.size() - deleted < 2)
				break;

			uint64_t size = agi::fs::Size(i.second);
			try {
				agi::fs::Remove(i.second);
				LOG_D("utils/clean_cache") << "deleted " << i.second;
			}
			catch  (agi::Exception const& e) {
				LOG_D("utils/clean_cache") << "failed to delete file " << i.second << ": " << e.GetMessage();
				continue;
			}

			total_size -= size;
			++deleted;
		}

		LOG_D("utils/clean_cache") << "deleted " << deleted << " files, exiting";
	});
}

#ifndef __WXOSX_COCOA__
// OS X implementation in osx_utils.mm
void AddFullScreenButton(wxWindow *) { }
void SetFloatOnParent(wxWindow *) { }

// OS X implementation in retina_helper.mm
RetinaHelper::RetinaHelper(wxWindow *) { }
RetinaHelper::~RetinaHelper() { }
int RetinaHelper::GetScaleFactor() const { return 1; }

// OS X implementation in scintilla_ime.mm
namespace osx { namespace ime {
	void inject(wxStyledTextCtrl *) { }
	void invalidate(wxStyledTextCtrl *) { }
	bool process_key_event(wxStyledTextCtrl *, wxKeyEvent&) { return false; }
} }
#endif

wxString FontFace(std::string opt_prefix) {
	opt_prefix += "/Font Face";
	auto value = OPT_GET(opt_prefix)->GetString();
#ifdef __WXOSX_COCOA__
	if (value.empty()) {
		auto default_font = CTFontCreateUIFontForLanguage(kCTFontUserFontType, 0, nullptr);
		auto default_font_name = CTFontCopyPostScriptName(default_font);
		CFRelease(default_font);

		auto utf8_str = CFStringGetCStringPtr(default_font_name, kCFStringEncodingUTF8);
		if (utf8_str)
			value = utf8_str;
		else {
			char buffer[1024];
			CFStringGetCString(default_font_name, buffer, sizeof(buffer), kCFStringEncodingUTF8);
			buffer[1023] = '\0';
			value = buffer;
		}

		CFRelease(default_font_name);
	}
#endif
	return to_wx(value);
}

static agi::fs::path FileSelector(wxString const& message, std::string const& option_name, std::string const& default_filename, std::string const& default_extension, std::string const& wildcard, int flags, wxWindow *parent) {
	wxString path;
	if (!option_name.empty())
		path = to_wx(OPT_GET(option_name)->GetString());
	agi::fs::path filename = wxFileSelector(message, path, to_wx(default_filename), to_wx(default_extension), to_wx(wildcard), flags, parent).wx_str();
	if (!filename.empty() && !option_name.empty())
		OPT_SET(option_name)->SetString(filename.parent_path().string());
	return filename;
}

agi::fs::path OpenFileSelector(wxString const& message, std::string const& option_name, std::string const& default_filename, std::string const& default_extension, std::string const& wildcard, wxWindow *parent) {
	return FileSelector(message, option_name, default_filename, default_extension, wildcard, wxFD_OPEN | wxFD_FILE_MUST_EXIST, parent);
}

agi::fs::path SaveFileSelector(wxString const& message, std::string const& option_name, std::string const& default_filename, std::string const& default_extension, std::string const& wildcard, wxWindow *parent) {
	return FileSelector(message, option_name, default_filename, default_extension, wildcard, wxFD_SAVE | wxFD_OVERWRITE_PROMPT, parent);
}

wxString LocalizedLanguageName(wxString const& lang) {
	icu::Locale iculoc(lang.c_str());
	if (!iculoc.isBogus()) {
		icu::UnicodeString ustr;
		iculoc.getDisplayName(iculoc, ustr);
#ifdef _MSC_VER
		return wxString(ustr.getBuffer());
#else
		std::string utf8;
		ustr.toUTF8String(utf8);
		return to_wx(utf8);
#endif
	}

	if (auto info = wxLocale::FindLanguageInfo(lang))
		return info->Description;
	return lang;
}
