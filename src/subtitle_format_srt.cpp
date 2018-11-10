// Copyright (c) 2006, Rodrigo Braz Monteiro
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

/// @file subtitle_format_srt.cpp
/// @brief Reading/writing SubRip format subtitles (.SRT)
/// @ingroup subtitle_io
///

#include "subtitle_format_srt.h"

#include "ass_attachment.h"
#include "ass_dialogue.h"
#include "ass_file.h"
#include "options.h"
#include "text_file_reader.h"
#include "text_file_writer.h"

#include <libaegisub/format.h>
#include <libaegisub/of_type_adaptor.h>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/regex.hpp>

DEFINE_EXCEPTION(SRTParseError, SubtitleFormatParseError);

namespace {
enum class TagType {
	UNKNOWN,
	BOLD_OPEN,
	BOLD_CLOSE,
	ITALICS_OPEN,
	ITALICS_CLOSE,
	UNDERLINE_OPEN,
	UNDERLINE_CLOSE,
	STRIKEOUT_OPEN,
	STRIKEOUT_CLOSE,
	FONT_OPEN,
	FONT_CLOSE
};

TagType type_from_name(std::string const& tag) {
	switch (tag.size()) {
		case 1:
			switch (tag[0]) {
				case 'b': return TagType::BOLD_OPEN;
				case 'i': return TagType::ITALICS_OPEN;
				case 'u': return TagType::UNDERLINE_OPEN;
				case 's': return TagType::STRIKEOUT_OPEN;
				default: return TagType::UNKNOWN;
			}
		case 2:
			if (tag[0] != '/') return TagType::UNKNOWN;
			switch (tag[1]) {
				case 'b': return TagType::BOLD_CLOSE;
				case 'i': return TagType::ITALICS_CLOSE;
				case 'u': return TagType::UNDERLINE_CLOSE;
				case 's': return TagType::STRIKEOUT_CLOSE;
				default: return TagType::UNKNOWN;
			}
		default:
			if (tag == "font") return TagType::FONT_OPEN;
			if (tag == "/font") return TagType::FONT_CLOSE;
			return TagType::UNKNOWN;
	}
}

struct ToggleTag {
	char tag;
	int level = 0;

	ToggleTag(char tag) : tag(tag) { }

	void Open(std::string& out) {
		if (level == 0) {
			out += "{\\";
			out += tag;
			out += "1}";
		}
		++level;
	}

	void Close(std::string& out) {
		if (level == 1) {
			out += "{\\";
			out += tag;
			out += '}';
		}
		if (level > 0)
			--level;
	}
};

class SrtTagParser {
	struct FontAttribs {
		std::string face;
		std::string size;
		std::string color;
	};

	const boost::regex tag_matcher;
	const boost::regex attrib_matcher;
	const boost::regex is_quoted;

public:
	SrtTagParser()
	: tag_matcher("^(.*?)<(/?b|/?i|/?u|/?s|/?font)([^>]*)>(.*)$", boost::regex::icase)
	, attrib_matcher(R"(^[[:space:]]+(face|size|color)=('[^']*'|"[^"]*"|[^[:space:]]+))", boost::regex::icase)
	, is_quoted(R"(^(['"]).*\1$)")
	{
	}

	std::string ToAss(std::string srt)
	{
		ToggleTag bold('b');
		ToggleTag italic('i');
		ToggleTag underline('u');
		ToggleTag strikeout('s');
		std::vector<FontAttribs> font_stack;

		std::string ass; // result to be built

		while (!srt.empty())
		{
			boost::smatch result;
			if (!regex_match(srt, result, tag_matcher))
			{
				// no more tags could be matched, end of string
				ass.append(srt);
				break;
			}

			// we found a tag, translate it
			std::string pre_text  = result.str(1);
			std::string tag_name  = result.str(2);
			std::string tag_attrs = result.str(3);
			std::string post_text = result.str(4);

			// the text before the tag goes through unchanged
			ass.append(pre_text);
			// the text after the tag is the input for next iteration
			srt = post_text;

			boost::to_lower(tag_name);
			switch (type_from_name(tag_name))
			{
			case TagType::BOLD_OPEN:       bold.Open(ass);       break;
			case TagType::BOLD_CLOSE:      bold.Close(ass);      break;
			case TagType::ITALICS_OPEN:    italic.Open(ass);     break;
			case TagType::ITALICS_CLOSE:   italic.Close(ass);    break;
			case TagType::UNDERLINE_OPEN:  underline.Open(ass);  break;
			case TagType::UNDERLINE_CLOSE: underline.Close(ass); break;
			case TagType::STRIKEOUT_OPEN:  strikeout.Open(ass);  break;
			case TagType::STRIKEOUT_CLOSE: strikeout.Close(ass); break;
			case TagType::FONT_OPEN:
				{
					// new attributes to fill in
					FontAttribs new_attribs;
					FontAttribs old_attribs;
					// start out with any previous ones on stack
					if (font_stack.size() > 0)
						old_attribs = font_stack.back();
					new_attribs = old_attribs;
					// now find all attributes on this font tag
					boost::smatch result;
					while (regex_search(tag_attrs, result, attrib_matcher))
					{
						// get attribute name and values
						std::string attr_name = result.str(1);
						std::string attr_value = result.str(2);

						// clean them
						boost::to_lower(attr_name);
						if (regex_match(attr_value, is_quoted))
							attr_value = attr_value.substr(1, attr_value.size() - 2);

						// handle the attributes
						if (attr_name == "face")
							new_attribs.face = agi::format("{\\fn%s}", attr_value);
						else if (attr_name == "size")
							new_attribs.size = agi::format("{\\fs%s}", attr_value);
						else if (attr_name == "color")
							new_attribs.color = agi::format("{\\c%s}", agi::Color(attr_value).GetAssOverrideFormatted());

						// remove this attribute to prepare for the next
						tag_attrs = result.suffix().str();
					}

					// the attributes changed from old are then written out
					if (new_attribs.face != old_attribs.face)
						ass.append(new_attribs.face);
					if (new_attribs.size != old_attribs.size)
						ass.append(new_attribs.size);
					if (new_attribs.color != old_attribs.color)
						ass.append(new_attribs.color);

					// lastly dump the new attributes state onto the stack
					font_stack.push_back(new_attribs);
				}
				break;
			case TagType::FONT_CLOSE:
				{
					// this requires a font stack entry
					if (font_stack.empty())
						break;
					// get the current attribs
					FontAttribs cur_attribs = font_stack.back();
					// remove them from the stack
					font_stack.pop_back();
					// grab the old attributes if there are any
					FontAttribs old_attribs;
					if (font_stack.size() > 0)
						old_attribs = font_stack.back();
					// then restore the attributes to previous settings
					if (cur_attribs.face != old_attribs.face)
					{
						if (old_attribs.face.empty())
							ass.append("{\\fn}");
						else
							ass.append(old_attribs.face);
					}
					if (cur_attribs.size != old_attribs.size)
					{
						if (old_attribs.size.empty())
							ass.append("{\\fs}");
						else
							ass.append(old_attribs.size);
					}
					if (cur_attribs.color != old_attribs.color)
					{
						if (old_attribs.color.empty())
							ass.append("{\\c}");
						else
							ass.append(old_attribs.color);
					}
				}
				break;
			default:
				// unknown tag, replicate it in the output
				ass.append("<").append(tag_name).append(tag_attrs).append(">");
				break;
			}
		}

		// make it a little prettier, join tag groups
		boost::replace_all(ass, "}{", "");

		return ass;
	}
};

std::string WriteSRTTime(agi::Time const& ts)
{
	return ts.GetSrtFormatted();
}

}

SRTSubtitleFormat::SRTSubtitleFormat()
: SubtitleFormat("SubRip")
{
}

std::vector<std::string> SRTSubtitleFormat::GetReadWildcards() const {
	return {"srt"};
}

std::vector<std::string> SRTSubtitleFormat::GetWriteWildcards() const {
	return GetReadWildcards();
}

enum class ParseState {
	INITIAL,
	TIMESTAMP,
	FIRST_LINE_OF_BODY,
	REST_OF_BODY,
	LAST_WAS_BLANK
};

void SRTSubtitleFormat::ReadFile(AssFile *target, agi::fs::path const& filename, agi::vfr::Framerate const& fps, std::string const& encoding) const {
	using namespace std;

	TextFileReader file(filename, encoding);
	target->LoadDefault(false, OPT_GET("Subtitle Format/SRT/Default Style Catalog")->GetString());

	// See parsing algorithm at <http://devel.aegisub.org/wiki/SubtitleFormats/SRT>

	// "hh:mm:ss,fff --> hh:mm:ss,fff" (e.g. "00:00:04,070 --> 00:00:10,04")
	const boost::regex timestamp_regex("^([0-9]{1,2}:[0-9]{1,2}:[0-9]{1,2},[0-9]{1,}) --> ([0-9]{1,2}:[0-9]{1,2}:[0-9]{1,2},[0-9]{1,})");

	SrtTagParser tag_parser;

	ParseState state = ParseState::INITIAL;
	int line_num = 0;
	int linebreak_debt = 0;
	AssDialogue *line = nullptr;
	std::string text;
	while (file.HasMoreLines()) {
		std::string text_line = file.ReadLineFromFile();
		++line_num;
		boost::trim(text_line);

		boost::smatch timestamp_match;
		bool found_timestamps = false;
		switch (state) {
			case ParseState::INITIAL:
				// ignore leading blank lines
				if (text_line.empty()) break;
				if (all(text_line, boost::is_digit())) {
					// found the line number, throw it away and hope for timestamps
					state = ParseState::TIMESTAMP;
					break;
				}
				if (regex_search(text_line, timestamp_match, timestamp_regex)) {
					found_timestamps = true;
					break;
				}

				throw SRTParseError(agi::format("Parsing SRT: Expected subtitle index at line %d", line_num));

			case ParseState::TIMESTAMP:
				if (!regex_search(text_line, timestamp_match, timestamp_regex))
					throw SRTParseError(agi::format("Parsing SRT: Expected timestamp pair at line %d", line_num));

				found_timestamps = true;
				break;

			case ParseState::FIRST_LINE_OF_BODY:
				if (text_line.empty()) {
					// that's not very interesting... blank subtitle?
					state = ParseState::LAST_WAS_BLANK;
					// no previous line that needs a line break after
					linebreak_debt = 0;
					break;
				}
				text.append(text_line);
				state = ParseState::REST_OF_BODY;
				break;

			case ParseState::REST_OF_BODY:
				if (text_line.empty()) {
					// Might be either the gap between two subtitles or just a
					// blank line in the middle of a subtitle, so defer adding
					// the line break until we check what's on the next line
					state = ParseState::LAST_WAS_BLANK;
					linebreak_debt = 1;
					break;
				}
				text.append("\\N");
				text.append(text_line);
				break;

			case ParseState::LAST_WAS_BLANK:
				++linebreak_debt;
				if (text_line.empty()) break;
				if (all(text_line, boost::is_digit())) {
					// Hopefully it's the start of a new subtitle, and the
					// previous blank line(s) were the gap between subtitles
					state = ParseState::TIMESTAMP;
					break;
				}
				if (regex_search(text_line, timestamp_match, timestamp_regex)) {
					found_timestamps = true;
					break;
				}

				// assume it's a continuation of the subtitle text
				// resolve our line break debt and append the line text
				while (linebreak_debt-- > 0)
					text.append("\\N");
				text.append(text_line);
				state = ParseState::REST_OF_BODY;
				break;
		}
		if (found_timestamps) {
			if (line) {
				// finalize active line
				line->Text = tag_parser.ToAss(text);
				text.clear();
			}

			// create new subtitle
			line = new AssDialogue;
			line->Start = timestamp_match.str(1);
			line->End = timestamp_match.str(2);
			// store pointer to subtitle, we'll continue working on it
			target->Events.push_back(*line);
			// next we're reading the text
			state = ParseState::FIRST_LINE_OF_BODY;
		}
	}

	if (state == ParseState::TIMESTAMP || state == ParseState::FIRST_LINE_OF_BODY)
		throw SRTParseError("Parsing SRT: Incomplete file");

	if (line) // an unfinalized line
		line->Text = tag_parser.ToAss(text);
}

void SRTSubtitleFormat::WriteFile(const AssFile *src, agi::fs::path const& filename, agi::vfr::Framerate const& fps, std::string const& encoding) const {
	TextFileWriter file(filename, encoding);

	// Convert to SRT
	AssFile copy(*src);
	copy.Sort();
	StripComments(copy);
	RecombineOverlaps(copy);
	MergeIdentical(copy);
#ifdef _WIN32
	ConvertNewlines(copy, "\r\n", false);
#else
	ConvertNewlines(copy, "\n", false);
#endif

	// Write lines
	int i=0;
	for (auto const& current : copy.Events) {
		file.WriteLineToFile(std::to_string(++i));
		file.WriteLineToFile(WriteSRTTime(current.Start) + " --> " + WriteSRTTime(current.End));
		file.WriteLineToFile(ConvertTags(&current));
		file.WriteLineToFile("");
	}
}

bool SRTSubtitleFormat::CanSave(const AssFile *file) const {
	if (!file->Attachments.empty())
		return false;

	auto def = boost::flyweight<std::string>("Default");
	for (auto const& line : file->Events) {
		if (line.Style != def)
			return false;

		auto blocks = line.ParseTags();
		for (auto ovr : blocks | agi::of_type<AssDialogueBlockOverride>()) {
			// Verify that all overrides used are supported
			for (auto const& tag : ovr->Tags) {
				if (tag.Name.size() != 2)
					return false;
				if (!strchr("bisu", tag.Name[1]))
					return false;
			}
		}
	}

	return true;
}

std::string SRTSubtitleFormat::ConvertTags(const AssDialogue *diag) const {
	struct tag_state { char tag; bool value; };
	tag_state tag_states[] = {
		{'b', false},
		{'i', false},
		{'s', false},
		{'u', false}
	};

	std::string final;
	for (auto& block : diag->ParseTags()) {
		switch (block->GetType()) {
		case AssBlockType::OVERRIDE:
			for (auto const& tag : static_cast<AssDialogueBlockOverride&>(*block).Tags) {
				if (!tag.IsValid() || tag.Name.size() != 2)
					continue;
				for (auto& state : tag_states) {
					if (state.tag != tag.Name[1]) continue;

					bool temp = tag.Params[0].Get(false);
					if (temp && !state.value)
						final += agi::format("<%c>", state.tag);
					if (!temp && state.value)
						final += agi::format("</%c>", state.tag);
					state.value = temp;
				}
			}
			break;
		case AssBlockType::PLAIN:
			final += block->GetText();
			break;
		case AssBlockType::DRAWING:
		case AssBlockType::COMMENT:
			break;
		}
	}

	// Ensure all tags are closed
	// Otherwise unclosed overrides might affect lines they shouldn't, see bug #809 for example
	for (auto state : tag_states) {
		if (state.value)
			final += agi::format("</%c>", state.tag);
	}

	return final;
}
