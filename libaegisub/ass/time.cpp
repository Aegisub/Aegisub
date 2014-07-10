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
#include <libaegisub/util.h>

#include <algorithm>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/range/adaptor/filtered.hpp>

namespace agi {
Time::Time(int time) : time(util::mid(0, time, 10 * 60 * 60 * 1000 - 1)) { }

Time::Time(std::string const& text) {
	int after_decimal = -1;
	int current = 0;
	for (char c : text | boost::adaptors::filtered(boost::is_any_of(",.0123456789:"))) {
		if (c == ':') {
			time = time * 60 + current;
			current = 0;
		}
		else if (c == '.' || c == ',') {
			time = (time * 60 + current) * 1000;
			current = 0;
			after_decimal = 100;
		}
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
	time = util::mid(0, time, 10 * 60 * 60 * 1000 - 1);
}

std::string Time::GetAssFormatted(bool msPrecision) const {
	const char ass_timestamp_c[] = {
		'0' + GetTimeHours(),
		':',
		'0' + time % (60 * 60 * 1000) / (60 * 1000 * 10),
		'0' + time % (10 * 60 * 1000) / (60 * 1000),
		':',
		'0' + time % (60 * 1000) / (1000 * 10),
		'0' + time % (10 * 1000) / 1000,
		'.',
		'0' + time % 1000 / 100,
		'0' + time % 100 / 10,
		msPrecision ? '0' + time % 10 : '\0',
		'\0'
	};
	return ass_timestamp_c;
}

int Time::GetTimeHours() const { return time / 3600000; }
int Time::GetTimeMinutes() const { return (time % 3600000) / 60000; }
int Time::GetTimeSeconds() const { return (time % 60000) / 1000; }
int Time::GetTimeMiliseconds() const { return (time % 1000); }
int Time::GetTimeCentiseconds() const { return (time % 1000) / 10; }

SmpteFormatter::SmpteFormatter(vfr::Framerate fps, std::string sep)
: fps(std::move(fps))
, sep(std::move(sep))
{
}

std::string SmpteFormatter::ToSMPTE(Time time) const {
	int h=0, m=0, s=0, f=0;
	fps.SmpteAtTime(time, &h, &m, &s, &f);
	return format("%02d%s%02d%s%02d%s%02d", h, sep, m, sep, s, sep, f);
}

Time SmpteFormatter::FromSMPTE(std::string const& str) const {
	std::vector<std::string> toks;
	boost::split(toks, str, boost::is_any_of(sep));
	if (toks.size() != 4) return 0;

	int h, m, s, f;
	util::try_parse(toks[0], &h);
	util::try_parse(toks[1], &m);
	util::try_parse(toks[2], &s);
	util::try_parse(toks[3], &f);
	return fps.TimeAtSmpte(h, m, s, f);
}
}
