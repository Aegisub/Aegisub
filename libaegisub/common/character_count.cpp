// Copyright (c) 2014, Thomas Goyne <plorkyeran@aegisub.org>
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

#include "libaegisub/character_count.h"

#include "libaegisub/ass/dialogue_parser.h"
#include "libaegisub/exception.h"

#include <unicode/uchar.h>
#include <unicode/utf8.h>

#include <mutex>
#include <unicode/brkiter.h>

namespace {
struct utext_deleter {
	void operator()(UText *ut) { if (ut) utext_close(ut); }
};
using utext_ptr = std::unique_ptr<UText, utext_deleter>;

UChar32 ass_special_chars[] = {'n', 'N', 'h'};

icu::BreakIterator& get_break_iterator(const char *ptr, size_t len) {
	static std::unique_ptr<icu::BreakIterator> bi;
	static std::once_flag token;
	std::call_once(token, [&] {
		UErrorCode status = U_ZERO_ERROR;
		bi.reset(BreakIterator::createCharacterInstance(Locale::getDefault(), status));
		if (U_FAILURE(status)) throw agi::InternalError("Failed to create character iterator");
	});

	UErrorCode err = U_ZERO_ERROR;
	utext_ptr ut(utext_openUTF8(nullptr, ptr, len, &err));
	if (U_FAILURE(err)) throw agi::InternalError("Failed to open utext");

	bi->setText(ut.get(), err);
	if (U_FAILURE(err)) throw agi::InternalError("Failed to set break iterator text");

	return *bi;
}

template <typename Iterator>
size_t count_in_range(Iterator begin, Iterator end, int mask) {
	if (begin == end) return 0;

	auto& character_bi = get_break_iterator(&*begin, end - begin);

	size_t count = 0;
	auto pos = character_bi.first();
	for (auto end = character_bi.next(); end != BreakIterator::DONE; pos = end, end = character_bi.next()) {
		if (!mask)
			++count;
		else {
			UChar32 c;
			int i = 0;
			U8_NEXT_UNSAFE(begin + pos, i, c);
			if ((U_GET_GC_MASK(c) & mask) == 0) {
				if (mask & U_GC_Z_MASK) {
					UChar32 *result = std::find(std::begin(ass_special_chars), std::end(ass_special_chars), c);
					if (result != std::end(ass_special_chars)) {
						UChar32 c2;
						i = 0;
						U8_PREV_UNSAFE(begin + pos, i, c2);
						if (c2 != (UChar32) '\\')
							++count;
						else if (!(mask & U_GC_P_MASK))
							--count;
					}
					else
						++count;

				}
				else
					++count;
			}
		}

	}
	return count;
}

int ignore_mask_to_icu_mask(int mask) {
	int ret = 0;
	if (mask & agi::IGNORE_PUNCTUATION)
		ret |= U_GC_P_MASK;
	if (mask & agi::IGNORE_WHITESPACE)
		ret |= U_GC_Z_MASK;
	return ret;
}
}

namespace agi {
size_t CharacterCount(std::string::const_iterator begin, std::string::const_iterator end, int ignore) {
	int mask = ignore_mask_to_icu_mask(ignore);
	if ((ignore & agi::IGNORE_BLOCKS) == 0)
		return count_in_range(begin, end, mask);

	size_t characters = 0;
	auto pos = begin;
	do {
		auto it = std::find(pos, end, '{');
		characters += count_in_range(pos, it, mask);
		if (it == end) break;

		pos = std::find(pos, end, '}');
		if (pos == end) {
			characters += count_in_range(it, pos, mask);
			break;
		}
	} while (++pos != end);

	return characters;
}

size_t CharacterCount(std::string const& str, int mask) {
	return CharacterCount(begin(str), end(str), mask);
}

size_t MaxLineLength(std::string const& text, int mask) {
	mask = ignore_mask_to_icu_mask(mask);
	auto tokens = agi::ass::TokenizeDialogueBody(text);
	agi::ass::MarkDrawings(text, tokens);

	size_t pos = 0;
	size_t max_line_length = 0;
	size_t current_line_length = 0;
	for (auto token : tokens) {
		if (token.type == agi::ass::DialogueTokenType::LINE_BREAK) {
			if (text[pos + 1] == 'h') {
				if (!(mask & U_GC_Z_MASK))
					current_line_length += 1;
			}
			else { // N or n
				max_line_length = std::max(max_line_length, current_line_length);
				current_line_length = 0;
			}
		}
		else if (token.type == agi::ass::DialogueTokenType::TEXT)
			current_line_length += count_in_range(begin(text) + pos, begin(text) + pos + token.length, mask);

		pos += token.length;
	}

	return std::max(max_line_length, current_line_length);
}

size_t IndexOfCharacter(std::string const& str, size_t n) {
	if (str.empty() || n == 0) return 0;
	auto& bi = get_break_iterator(&str[0], str.size());

	for (auto pos = bi.first(), end = bi.next(); ; --n, pos = end, end = bi.next()) {
		if (end == BreakIterator::DONE)
			return str.size();
		if (n == 0)
			return pos;
	}
}
}
