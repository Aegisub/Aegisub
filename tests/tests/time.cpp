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

#include <main.h>

#include <libaegisub/ass/smpte.h>
#include <libaegisub/ass/time.h>

using agi::Time;

TEST(lagi_time, out_of_range_times) {
	EXPECT_EQ(0, (int)Time(-1));
	EXPECT_EQ(10 * 60 * 60 * 1000 - 10, (int)Time(10 * 60 * 60 * 1000));
}

TEST(lagi_time, rounds_to_cs) {
	EXPECT_EQ(10, (int)Time(14));
}

TEST(lagi_time, cs_formatting) {
	EXPECT_STREQ("1:23:45.67", Time((((1 * 60) + 23) * 60 + 45) * 1000 + 670).GetAssFormatted().c_str());
}

TEST(lagi_time, ms_formatting) {
	EXPECT_STREQ("1:23:45.678", Time((((1 * 60) + 23) * 60 + 45) * 1000 + 678).GetAssFormatted(true).c_str());
}

TEST(lagi_time, well_formed_ass_time_parse) {
	EXPECT_STREQ("1:23:45.67", Time("1:23:45.67").GetAssFormatted().c_str());
}

TEST(lagi_time, missing_components) {
	EXPECT_STREQ("0:23:45.67", Time("23:45.67").GetAssFormatted().c_str());
	EXPECT_STREQ("0:00:45.67", Time("45.67").GetAssFormatted().c_str());
	EXPECT_STREQ("0:00:45.60", Time("45.6").GetAssFormatted().c_str());
	EXPECT_STREQ("0:00:45.00", Time("45").GetAssFormatted().c_str());
}

TEST(lagi_time, out_of_range_compontents) {
	EXPECT_STREQ("1:23:45.67", Time("0:83:45.67").GetAssFormatted().c_str());
	EXPECT_STREQ("0:01:40.00", Time("100").GetAssFormatted().c_str());
}

TEST(lagi_time, comma_decimal) {
	EXPECT_STREQ("1:23:45.67", Time("1:23:45,67").GetAssFormatted().c_str());
}

TEST(lagi_time, extra_garbage_is_ignored) {
	EXPECT_STREQ("1:23:45.67", Time("1a:b2c3d:e4f5g.!6&7").GetAssFormatted().c_str());
}

TEST(lagi_time, srt_time) {
	EXPECT_STREQ("1:23:45.678", Time("1:23:45,678").GetAssFormatted(true).c_str());
	EXPECT_STREQ("01:23:45,678", Time("1:23:45,678").GetSrtFormatted().c_str());
}

TEST(lagi_time, smpte_parse_valid) {
	EXPECT_STREQ("1:23:45.44", agi::SmpteFormatter(25).FromSMPTE("1:23:45:11").GetAssFormatted().c_str());
}

TEST(lagi_time, smpte_parse_invalid) {
	EXPECT_EQ(0, (int)agi::SmpteFormatter(25).FromSMPTE("1:23:45.11"));
}

TEST(lagi_time, to_smpte) {
	EXPECT_STREQ("01:23:45:11", agi::SmpteFormatter(25).ToSMPTE(Time("1:23:45.44")).c_str());
}
