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

#pragma once

#include <string>

namespace agi {
class Time {
	/// Time in milliseconds
	int time = 0;

public:
	Time(int ms = 0);
	Time(std::string const& text);

	/// Get millisecond, rounded to centisecond precision
	// Always round up for 5ms because the range is [start, stop)
	operator int() const { return (time + 5) - (time + 5) % 10; }

	/// Return the time as a string
	/// @param ms Use milliseconds precision, for non-ASS formats
	std::string GetAssFormatted(bool ms=false) const;

	/// Return the time as a string
	std::string GetSrtFormatted() const;
};
}
