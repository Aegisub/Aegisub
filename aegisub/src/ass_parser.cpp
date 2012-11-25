// Copyright (c) 2012, Thomas Goyne <plorkyeran@aegisub.org>
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

#include "config.h"

#include "ass_parser.h"

#include "ass_attachment.h"
#include "ass_dialogue.h"
#include "ass_file.h"
#include "ass_style.h"
#include "subtitle_format.h"

#ifndef AGI_PRE
#include <algorithm>

#include <wx/log.h>
#endif

AssParser::AssParser(AssFile *target, int version)
: target(target)
, version(version)
, attach(nullptr)
, state(&AssParser::ParseScriptInfoLine)
{
	std::fill(begin(insertion_positions), end(insertion_positions), nullptr);
}

AssParser::~AssParser() {
}

void AssParser::ParseAttachmentLine(wxString const& data) {
	bool is_filename = data.StartsWith("fontname: ") || data.StartsWith("filename: ");

	bool valid_data = data.size() > 0 && data.size() <= 80;
	for (auto byte : data) {
		if (byte < 33 || byte >= 97) {
			valid_data = false;
			break;
		}
	}

	// Data is over, add attachment to the file
	if (!valid_data || is_filename) {
		attach->Finish();
		InsertLine(attach.release());
		AddLine(data);
	}
	else {
		attach->AddData(data);

		// Done building
		if (data.Length() < 80) {
			attach->Finish();
			InsertLine(attach.release());
		}
	}
}

void AssParser::ParseScriptInfoLine(wxString const& data) {
	if (data.StartsWith(";")) {
		// Skip stupid comments added by other programs
		// Of course, we'll add our own in place later... ;)
		return;
	}

	// If the first nonblank line isn't a header pretend it starts with [Script Info]
	if (target->Line.empty())
		target->Line.push_back(*new AssEntry("[Script Info]", "[Script Info]"));

	if (data.StartsWith("ScriptType:")) {
		wxString versionString = data.Mid(11).Trim(true).Trim(false).Lower();
		int trueVersion;
		if (versionString == "v4.00")
			trueVersion = 0;
		else if (versionString == "v4.00+")
			trueVersion = 1;
		else
			throw SubtitleFormatParseError("Unknown SSA file format version", 0);
		if (trueVersion != version) {
			wxLogMessage("Warning: File has the wrong extension.");
			version = trueVersion;
		}
	}

	InsertLine(new AssEntry(data));
}

void AssParser::ParseEventLine(wxString const& data) {
	if (data.StartsWith("Dialogue:") || data.StartsWith("Comment:"))
		InsertLine(new AssDialogue(data));
}

void AssParser::ParseStyleLine(wxString const& data) {
	if (data.StartsWith("Style:"))
		InsertLine(new AssStyle(data, version));
}

void AssParser::ParseFontLine(wxString const& data) {
	if (data.StartsWith("fontname: "))
		attach.reset(new AssAttachment(data.Mid(10), ENTRY_FONT));
}

void AssParser::ParseGraphicsLine(wxString const& data) {
	if (data.StartsWith("filename: "))
		attach.reset(new AssAttachment(data.Mid(10), ENTRY_GRAPHIC));
}

void AssParser::AddLine(wxString const& data) {
	// Special-case for attachments since a line could theoretically be both a
	// valid attachment data line and a valid section header, and if an
	// attachment is in progress it needs to be treated as that
	if (attach.get()) {
		ParseAttachmentLine(data);
		return;
	}

	if (data.empty()) return;

	// Section header
	if (data[0] == '[' && data.Last() == ']') {
		// Ugly hacks to allow intermixed v4 and v4+ style sections
		const wxString low = data.Lower();
		wxString header = data;
		if (low == "[v4 styles]") {
			header = "[V4+ Styles]";
			version = 0;
			state = &AssParser::ParseStyleLine;
		}
		else if (low == "[v4+ styles]") {
			header = "[V4+ Styles]";
			version = 1;
			state = &AssParser::ParseStyleLine;
		}
		else if (low == "[events]")
			state = &AssParser::ParseEventLine;
		else if (low == "[script info]")
			state = &AssParser::ParseScriptInfoLine;
		else if (low == "[graphics]")
			state = &AssParser::ParseGraphicsLine;
		else if (low == "[fonts]")
			state = &AssParser::ParseFontLine;
		else
			state = &AssParser::UnknownLine;
		return;
	}

	(this->*state)(data);
}

void AssParser::InsertLine(AssEntry *entry) {
	AssEntry *position = insertion_positions[entry->Group()];
	if (position)
		target->Line.insert(++target->Line.iterator_to(*position), *entry);
	else
		target->Line.push_back(*entry);
	insertion_positions[entry->Group()] = entry;
}
