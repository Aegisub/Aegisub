// Copyright (c) 2010, Amar Takhar <verm@aegisub.org>
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

/// @file charset.cpp
/// @brief Character set detection and manipulation utilities.
/// @ingroup libaegisub

#include "libaegisub/charset.h"

#include "libaegisub/file_mapping.h"
#include "libaegisub/scoped_ptr.h"

#ifdef WITH_UCHARDET
#include <uchardet/uchardet.h>
#endif

namespace agi { namespace charset {
std::string Detect(agi::fs::path const& file) {
	agi::read_file_mapping fp(file);

	// FIXME: It is an empty file. Treat as ascii
	if (fp.size() == 0) return "ascii";

	// FIXME: Dirty hack for Matroska. These 4 bytes are the magic
	// number of EBML which is used by mkv and webm
	if (fp.size() >= 4) {
		const char* buf = fp.read(0, 4);
		if (!strncmp(buf, "\x1a\x45\xdf\xa3", 4))
			return "binary";
	}

#ifdef WITH_UCHARDET
	agi::scoped_holder<uchardet_t> ud(uchardet_new(), uchardet_delete);
	for (uint64_t offset = 0; offset < fp.size(); ) {
		auto read = std::min<uint64_t>(65536, fp.size() - offset);
		auto buf = fp.read(offset, read);
		uchardet_handle_data(ud, buf, read);
		offset += read;
	}
	uchardet_data_end(ud);
	std::string encoding = uchardet_get_charset(ud);
	return encoding.empty() ? "binary" : encoding;
#else

	// Look for utf-8 BOM
	if (fp.size() >= 3) {
		const char* buf = fp.read(0, 3);
		if (!strncmp(buf, "\xef\xbb\xbf", 3))
			return "utf-8";
	}

	// If it's over 100 MB it's either binary or big enough that we won't
	// be able to do anything useful with it anyway
	if (fp.size() > 100 * 1024 * 1024)
		return "binary";

	uint64_t binaryish = 0;
	auto read = std::min<uint64_t>(65536, fp.size());
	auto buf = fp.read(0, read);
	for (size_t i = 0; i < read; ++i) {
		if ((unsigned char)buf[i] < 32 && (buf[i] != '\r' && buf[i] != '\n' && buf[i] != '\t'))
			++binaryish;
	}

	if (binaryish > read / 8)
		return "binary";
	return "utf-8";
#endif
}
} }
