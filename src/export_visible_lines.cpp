// Copyright (c) 2005, Niels Martin Hansen
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
//
// $Id$

/// @file export_visible_lines.cpp
/// @brief Limit to Visible Lines export filter
/// @ingroup export
///


///////////
// Headers
#include "config.h"

#include "ass_dialogue.h"
#include "ass_file.h"
#include "ass_override.h"
#include "export_visible_lines.h"
#include "vfr.h"


/// @brief Constructor 
///
AssLimitToVisibleFilter::AssLimitToVisibleFilter() {
	initialized = false;
	frame = -1;
}



/// @brief Init 
/// @return 
///
void AssLimitToVisibleFilter::Init() {
	if (initialized) return;
	initialized = true;
	autoExporter = true;
	hidden = false;
	Register(_("Limit to Visible Lines"),1000000);
	description = _("Limit to Visible Lines");
}



/// @brief Process 
/// @param subs          
/// @param export_dialog 
/// @return 
///
void AssLimitToVisibleFilter::ProcessSubs(AssFile *subs, wxWindow *export_dialog) {
	// Nothing to do
	if (frame == -1) return;

	// Process
	using std::list;
	AssDialogue *diag;
	entryIter cur, next = subs->Line.begin();
	while (next != subs->Line.end()) {
		// Set next
		cur = next++;

		// Is dialogue?
		diag = dynamic_cast<AssDialogue*>(*cur);
		if (diag) {
			int f1 = VFR_Output.GetFrameAtTime(diag->Start.GetMS(),true);
			int f2 = VFR_Output.GetFrameAtTime(diag->End.GetMS(),false);

			// Invisible, remove frame
			if (f1 > frame || f2 < frame) {
				subs->Line.erase(cur);
			}
		}
	}
}



/// @brief Set limitation time 
/// @param _frame 
///
void AssLimitToVisibleFilter::SetFrame(int _frame) {
	instance.frame = _frame;
}



/// DOCME
AssLimitToVisibleFilter AssLimitToVisibleFilter::instance;


