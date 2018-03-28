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

#include "resolution_resampler.h"

#include "ass_dialogue.h"
#include "ass_file.h"
#include "ass_style.h"
#include "utils.h"

#include <libaegisub/exception.h>
#include <libaegisub/of_type_adaptor.h>
#include <libaegisub/split.h>
#include <libaegisub/util.h>
#include <libaegisub/ycbcr_conv.h>

#include <algorithm>
#include <boost/algorithm/string/predicate.hpp>
#include <cmath>
#include <wx/intl.h>

enum {
	LEFT = 0,
	RIGHT = 1,
	TOP = 2,
	BOTTOM = 3
};

static const std::string names[] = {
	"None",
	"TV.601", "PC.601",
	"TV.709", "PC.709",
	"TV.FCC", "PC.FCC",
	"TV.240M", "PC.240M"
};

YCbCrMatrix MatrixFromString(std::string const& str) {
	if (str.empty()) return YCbCrMatrix::tv_709;
	auto pos = std::find(std::begin(names), std::end(names), str);
	if (pos == std::end(names))
		return YCbCrMatrix::rgb;
	return static_cast<YCbCrMatrix>(std::distance(std::begin(names), pos));
}

std::string MatrixToString(YCbCrMatrix mat) {
	return names[static_cast<int>(mat)];
}

std::vector<std::string> MatrixNames() {
	return {std::begin(names), std::end(names)};
}

namespace {
	std::string transform_drawing(std::string const& drawing, int shift_x, int shift_y, double scale_x, double scale_y) {
		bool is_x = true;
		std::string final;
		final.reserve(drawing.size());

		for (auto const& cur : agi::Split(drawing, ' ')) {
			double val;
			if (agi::util::try_parse(agi::str(cur), &val)) {
				if (is_x)
					val = (val + shift_x) * scale_x;
				else
					val = (val + shift_y) * scale_y;
				val = round(val * 8) / 8.0; // round to eighth-pixels
				final += float_to_string(val);
				final += ' ';
				is_x = !is_x;
			}
			else if (cur.size() == 1) {
				char c = tolower(cur[0]);
				if (c == 'm' || c == 'n' || c == 'l' || c == 'b' || c == 's' || c == 'p' || c == 'c') {
					is_x = true;
					final += c;
					final += ' ';
				}
			}
		}

		if (final.size())
			final.pop_back();
		return final;
	}

	struct resample_state {
		const int *margin;
		double rx;
		double ry;
		double ar;
		agi::ycbcr_converter conv;
		bool convert_colors;
	};

	void resample_tags(std::string const& name, AssOverrideParameter *cur, void *ud) {
		resample_state *state = static_cast<resample_state *>(ud);

		double resizer = 1.0;
		int shift = 0;

		switch (cur->classification) {
			case AssParameterClass::ABSOLUTE_SIZE:
				resizer = state->ry;
				break;

			case AssParameterClass::ABSOLUTE_POS_X:
				resizer = state->rx;
				shift = state->margin[LEFT];
				break;

			case AssParameterClass::ABSOLUTE_POS_Y:
				resizer = state->ry;
				shift = state->margin[TOP];
				break;

			case AssParameterClass::RELATIVE_SIZE_X:
				resizer = state->ar;
				break;

			case AssParameterClass::RELATIVE_SIZE_Y:
				break;

			case AssParameterClass::DRAWING: {
				cur->Set(transform_drawing(
					cur->Get<std::string>(),
					state->margin[LEFT], state->margin[TOP], state->rx, state->ry));
				return;
			}

			case AssParameterClass::COLOR:
				if (state->convert_colors)
					cur->Set<std::string>(state->conv.rgb_to_rgb(agi::Color{cur->Get<std::string>()}).GetAssOverrideFormatted());
				return;

			default:
				return;
		}

		VariableDataType curType = cur->GetType();
		if (curType == VariableDataType::FLOAT)
			cur->Set((cur->Get<double>() + shift) * resizer);
		else if (curType == VariableDataType::INT)
			cur->Set<int>((cur->Get<int>() + shift) * resizer + 0.5);
	}

	void resample_line(resample_state *state, AssDialogue &diag) {
		if (diag.Comment && (boost::starts_with(diag.Effect.get(), "template") || boost::starts_with(diag.Effect.get(), "code")))
			return;

		auto blocks = diag.ParseTags();

		for (auto block : blocks | agi::of_type<AssDialogueBlockOverride>())
			block->ProcessParameters(resample_tags, state);

		for (auto drawing : blocks | agi::of_type<AssDialogueBlockDrawing>())
			drawing->text = transform_drawing(drawing->text, 0, 0, state->rx / state->ar, state->ry);

		for (size_t i = 0; i < 3; ++i) {
			if (diag.Margin[i])
				diag.Margin[i] = int((diag.Margin[i] + state->margin[i]) * (i < 2 ? state->rx : state->ry) + 0.5);
		}

		diag.UpdateText(blocks);
	}

	void resample_style(resample_state *state, AssStyle &style) {
		style.fontsize = int(style.fontsize * state->ry + 0.5);
		style.outline_w *= state->ry;
		style.shadow_w *= state->ry;
		style.spacing *= state->rx;
		style.scalex *= state->ar;
		for (int i = 0; i < 3; i++)
			style.Margin[i] = int((style.Margin[i] + state->margin[i]) * (i < 2 ? state->rx : state->ry) + 0.5);
		if (state->convert_colors) {
			style.primary = state->conv.rgb_to_rgb(style.primary);
			style.secondary = state->conv.rgb_to_rgb(style.secondary);
			style.outline = state->conv.rgb_to_rgb(style.outline);
			style.shadow = state->conv.rgb_to_rgb(style.shadow);
		}
		style.UpdateData();
	}

	agi::ycbcr_matrix matrix(YCbCrMatrix mat) {
		switch (mat) {
			case YCbCrMatrix::rgb: return agi::ycbcr_matrix::bt601;
			case YCbCrMatrix::tv_601:  case YCbCrMatrix::pc_601:  return agi::ycbcr_matrix::bt601;
			case YCbCrMatrix::tv_709:  case YCbCrMatrix::pc_709:  return agi::ycbcr_matrix::bt709;
			case YCbCrMatrix::tv_fcc:  case YCbCrMatrix::pc_fcc:  return agi::ycbcr_matrix::fcc;
			case YCbCrMatrix::tv_240m: case YCbCrMatrix::pc_240m: return agi::ycbcr_matrix::smpte_240m;
		}
		throw agi::InternalError("Invalid matrix");
	}

	agi::ycbcr_range range(YCbCrMatrix mat) {
		switch (mat) {
			case YCbCrMatrix::rgb:
			case YCbCrMatrix::tv_601:
			case YCbCrMatrix::tv_709:
			case YCbCrMatrix::tv_fcc:
			case YCbCrMatrix::tv_240m:
				return agi::ycbcr_range::tv;
			case YCbCrMatrix::pc_601:
			case YCbCrMatrix::pc_709:
			case YCbCrMatrix::pc_fcc:
			case YCbCrMatrix::pc_240m:
				return agi::ycbcr_range::pc;
		}
		throw agi::InternalError("Invalid matrix");
	}
}

void ResampleResolution(AssFile *ass, ResampleSettings settings) {
	auto horizontal_stretch = 1.0;
	auto old_ar = double(settings.source_x) / settings.source_y;
	auto new_ar = double(settings.dest_x) / settings.dest_y;
	bool border_horizontally = new_ar > old_ar;
	// Don't convert aspect ratio if it's very close to correct
	// (for reference, 848x480 <-> 1280x720 is .006)
	if (std::abs(old_ar - new_ar) / new_ar > .01) {
		switch (settings.ar_mode) {
		case ResampleARMode::RemoveBorder:
			border_horizontally = !border_horizontally;
		case ResampleARMode::AddBorder:
			if (border_horizontally) // Wider/Shorter
				settings.margin[LEFT] = settings.margin[RIGHT] = (settings.source_y * new_ar - settings.source_x) / 2;
			else // Taller/Narrower
				settings.margin[TOP] = settings.margin[BOTTOM] = (settings.source_x / new_ar - settings.source_y) / 2;
			break;
		case ResampleARMode::Stretch:
			horizontal_stretch = new_ar / old_ar;
			break;
		case ResampleARMode::Manual:
			old_ar =
				double(settings.source_x + settings.margin[LEFT] + settings.margin[RIGHT]) /
				double(settings.source_y + settings.margin[TOP] + settings.margin[BOTTOM]);

			if (std::abs(old_ar - new_ar) / new_ar > .01)
				horizontal_stretch = new_ar / old_ar;
			break;
		}
	}

	// Add margins to original resolution
	settings.source_x += settings.margin[LEFT] + settings.margin[RIGHT];
	settings.source_y += settings.margin[TOP] + settings.margin[BOTTOM];

	bool resample_colors =
		settings.source_matrix != settings.dest_matrix &&
		settings.source_matrix != YCbCrMatrix::rgb &&
		settings.dest_matrix != YCbCrMatrix::rgb;

	resample_state state = {
		settings.margin,
		double(settings.dest_x) / double(settings.source_x),
		double(settings.dest_y) / double(settings.source_y),
		horizontal_stretch,
		agi::ycbcr_converter{
			matrix(settings.source_matrix),
			range(settings.source_matrix),
			matrix(settings.dest_matrix),
			range(settings.dest_matrix),
		},
		resample_colors
	};

	for (auto& line : ass->Styles)
		resample_style(&state, line);
	for (auto& line : ass->Events)
		resample_line(&state, line);

	ass->SetScriptInfo("PlayResX", std::to_string(settings.dest_x));
	ass->SetScriptInfo("PlayResY", std::to_string(settings.dest_y));
	if (resample_colors)
		ass->SetScriptInfo("YCbCr Matrix", MatrixToString(settings.dest_matrix));

	ass->Commit(_("resolution resampling"), AssFile::COMMIT_SCRIPTINFO | AssFile::COMMIT_DIAG_FULL);
}
