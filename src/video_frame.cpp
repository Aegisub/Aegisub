// Copyright (c) 2013, Thomas Goyne <plorkyeran@aegisub.org>
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

#include "video_frame.h"

#include <boost/gil.hpp>
#include <wx/image.h>

namespace {
	// We actually have bgr_, not bgra, so we need a custom converter which ignores the alpha channel
	struct color_converter {
		template <typename P1, typename P2>
		void operator()(P1 const& src, P2& dst) const {
			using namespace boost::gil;
			dst = rgb8_pixel_t(
				get_color(src, red_t()),
				get_color(src, green_t()),
				get_color(src, blue_t()));
		}
	};
}

wxImage GetImage(VideoFrame const& frame) {
	using namespace boost::gil;

	wxImage img(frame.width, frame.height);
	auto src = interleaved_view(frame.width, frame.height, (bgra8_pixel_t*)frame.data.data(), frame.pitch);
	auto dst = interleaved_view(frame.width, frame.height, (rgb8_pixel_t*)img.GetData(), 3 * frame.width);
	if (frame.flipped)
		src = flipped_up_down_view(src);
	copy_and_convert_pixels(src, dst, color_converter());
	return img;
}
