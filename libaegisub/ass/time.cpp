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

#include <libaegisub/ass/time.h>
#include <libaegisub/ass/smpte.h>

#include <libaegisub/format.h>
#include <libaegisub/split.h>
#include <libaegisub/util.h>

#include <algorithm>

namespace agi {
Time::Time(int time) : time(util::mid(0, time, 10 * 60 * 60 * 1000 - 6)) { }

Time::Time(std::string const& text) {
	int after_decimal = -1;
	int current = 0;
	for (char c : text) {
		if (c == ':') {
			time = time * 60 + current;
			current = 0;
		}
		else if (c == '.' || c == ',') {
			time = (time * 60 + current) * 1000;
			current = 0;
			after_decimal = 100;
		}
		else if (c < '0' || c > '9')
			continue;
		else if (after_decimal < 0) {
			current *= 10;
			current += c - '0';
		}
		else {
			time += (c - '0') * after_decimal;
			after_decimal /= 10;
		}
	}

	// Never saw a decimal, so convert now to ms
	if (after_decimal < 0)
		time = (time * 60 + current) * 1000;

	// Limit to the valid range
	time = util::mid(0, time, 10 * 60 * 60 * 1000 - 6);
}

std::string Time::GetAssFormatted(bool msPrecision) const {
	int ass_time = msPrecision ? time : int(*this);
	std::string ret(10 + msPrecision, ':');
	ret[0] = '0' + ass_time / 3600000;
	ret[2] = '0' + (ass_time % (60 * 60 * 1000)) / (60 * 1000 * 10);
	ret[3] = '0' + (ass_time % (10 * 60 * 1000)) / (60 * 1000);
	ret[5] = '0' + (ass_time % (60 * 1000)) / (1000 * 10);
	ret[6] = '0' + (ass_time % (10 * 1000)) / 1000;
	ret[7] = '.';
	ret[8] = '0' + (ass_time % 1000) / 100;
	ret[9] = '0' + (ass_time % 100) / 10;
	if (msPrecision)
		ret[10] = '0' + ass_time % 10;
	return ret;
}

std::string Time::GetSrtFormatted() const {
	std::string ret(12, ':');
	ret[0] = '0';
	ret[1] = '0' + time / 3600000;
	ret[3] = '0' + (time % (60 * 60 * 1000)) / (60 * 1000 * 10);
	ret[4] = '0' + (time % (10 * 60 * 1000)) / (60 * 1000);
	ret[6] = '0' + (time % (60 * 1000)) / (1000 * 10);
	ret[7] = '0' + (time % (10 * 1000)) / 1000;
	ret[8] = ',';
	ret[9] = '0' + (time % 1000) / 100;
	ret[10] = '0' + (time % 100) / 10;
	ret[11] = '0' + time % 10;
	return ret;
}

SmpteFormatter::SmpteFormatter(vfr::Framerate fps, char sep)
: fps(std::move(fps))
, sep(sep)
{
}

std::string SmpteFormatter::ToSMPTE(Time time) const {
	int h=0, m=0, s=0, f=0;
	fps.SmpteAtTime(time, &h, &m, &s, &f);
	return format("%02d%c%02d%c%02d%c%02d", h, sep, m, sep, s, sep, f);
}

Time SmpteFormatter::FromSMPTE(std::string const& str) const {
	std::vector<std::string> toks;
	Split(toks, str, sep);
	if (toks.size() != 4) return 0;

	int h, m, s, f;
	util::try_parse(toks[0], &h);
	util::try_parse(toks[1], &m);
	util::try_parse(toks[2], &s);
	util::try_parse(toks[3], &f);
	return fps.TimeAtSmpte(h, m, s, f);
}
}
