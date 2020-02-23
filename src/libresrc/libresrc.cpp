// Copyright (c) 2009, Amar Takhar <verm@aegisub.org>
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

#include "libresrc.h"

#include <wx/bitmap.h>
#include <wx/icon.h>
#include <wx/image.h>
#include <wx/intl.h>
#include <wx/mstream.h>

wxBitmap libresrc_getimage(const unsigned char *buff, size_t size, double scale, int dir) {
	wxMemoryInputStream mem(buff, size);
#if wxCHECK_VERSION(3, 1, 0)
	// Since wxWidgets 3.1.0, there is an undocumented third parameter in the ctor of wxBitmap from wxImage
	// This "scale" parameter sets the logical scale factor of the created wxBitmap
	if (dir != wxLayout_RightToLeft)
		return wxBitmap(wxImage(mem), wxBITMAP_SCREEN_DEPTH, scale);
	return wxBitmap(wxImage(mem).Mirror(), wxBITMAP_SCREEN_DEPTH, scale);
#else
	if (dir != wxLayout_RightToLeft)
		return wxBitmap(wxImage(mem));
	return wxBitmap(wxImage(mem).Mirror());
#endif
}

wxIcon libresrc_geticon(const unsigned char *buff, size_t size) {
	wxMemoryInputStream mem(buff, size);
	wxIcon icon;
	icon.CopyFromBitmap(wxBitmap(wxImage(mem)));
	return icon;
}
