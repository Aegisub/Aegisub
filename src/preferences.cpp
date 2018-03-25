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

/// @file preferences.cpp
/// @brief Preferences dialogue
/// @ingroup configuration_ui

#include "preferences.h"

#include "ass_style_storage.h"
#include "audio_provider_factory.h"
#include "audio_renderer_waveform.h"
#include "command/command.h"
#include "compat.h"
#include "help_button.h"
#include "hotkey_data_view_model.h"
#include "include/aegisub/audio_player.h"
#include "include/aegisub/hotkey.h"
#include "include/aegisub/subtitles_provider.h"
#include "libresrc/libresrc.h"
#include "options.h"
#include "preferences_base.h"
#include "video_provider_manager.h"

#ifdef WITH_PORTAUDIO
#include "audio_player_portaudio.h"
#endif

#ifdef WITH_FFMS2
#include <ffms.h>
#endif

#include <libaegisub/hotkey.h>

#include <unordered_set>

#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/event.h>
#include <wx/listctrl.h>
#include <wx/msgdlg.h>
#include <wx/srchctrl.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/treebook.h>

namespace {
/// General preferences page
void General(wxTreebook *book, Preferences *parent) {
	auto p = new OptionPage(book, parent, _("General"));

	auto general = p->PageSizer(_("General"));
	p->OptionAdd(general, _("Check for updates on startup"), "App/Auto/Check For Updates");
	p->OptionAdd(general, _("Show main toolbar"), "App/Show Toolbar");
	p->OptionAdd(general, _("Save UI state in subtitles files"), "App/Save UI State");
	p->CellSkip(general);

	p->OptionAdd(general, _("Toolbar Icon Size"), "App/Toolbar Icon Size");
	wxString autoload_modes[] = { _("Never"), _("Always"), _("Ask") };
	wxArrayString autoload_modes_arr(3, autoload_modes);
	p->OptionChoice(general, _("Automatically load linked files"), autoload_modes_arr, "App/Auto/Load Linked Files");
	p->OptionAdd(general, _("Undo Levels"), "Limits/Undo Levels", 2, 10000);

	auto recent = p->PageSizer(_("Recently Used Lists"));
	p->OptionAdd(recent, _("Files"), "Limits/MRU", 0, 16);
	p->OptionAdd(recent, _("Find/Replace"), "Limits/Find Replace");

	p->SetSizerAndFit(p->sizer);
}

void General_DefaultStyles(wxTreebook *book, Preferences *parent) {
	auto p = new OptionPage(book, parent, _("Default styles"), OptionPage::PAGE_SUB);

	auto staticbox = new wxStaticBoxSizer(wxVERTICAL, p, _("Default style catalogs"));
	p->sizer->Add(staticbox, 0, wxEXPAND, 5);
	p->sizer->AddSpacer(8);

	auto instructions = new wxStaticText(p, wxID_ANY, _("The chosen style catalogs will be loaded when you start a new file or import files in the various formats.\n\nYou can set up style catalogs in the Style Manager."));
	p->sizer->Fit(p);
	instructions->Wrap(400);
	staticbox->Add(instructions, 0, wxALL, 5);
	staticbox->AddSpacer(16);
	
	auto general = new wxFlexGridSizer(2, 5, 5);
	general->AddGrowableCol(0, 1);
	staticbox->Add(general, 1, wxEXPAND, 5);

	// Build a list of available style catalogs, and wished-available ones
	auto const& avail_catalogs = AssStyleStorage::GetCatalogs();
	std::unordered_set<std::string> catalogs_set(begin(avail_catalogs), end(avail_catalogs));
	// Always include one named "Default" even if it doesn't exist (ensure there is at least one on the list)
	catalogs_set.insert("Default");
	// Include all catalogs named in the existing configuration
	static const char *formats[] = { "ASS", "MicroDVD", "SRT", "TTXT", "TXT" };
	for (auto formatname : formats)
		catalogs_set.insert(OPT_GET("Subtitle Format/" + std::string(formatname) + "/Default Style Catalog")->GetString());
	// Sorted version
	wxArrayString catalogs;
	for (auto const& cn : catalogs_set)
		catalogs.Add(to_wx(cn));
	catalogs.Sort();

	p->OptionChoice(general, _("New files"), catalogs, "Subtitle Format/ASS/Default Style Catalog");
	p->OptionChoice(general, _("MicroDVD import"), catalogs, "Subtitle Format/MicroDVD/Default Style Catalog");
	p->OptionChoice(general, _("SRT import"), catalogs, "Subtitle Format/SRT/Default Style Catalog");
	p->OptionChoice(general, _("TTXT import"), catalogs, "Subtitle Format/TTXT/Default Style Catalog");
	p->OptionChoice(general, _("Plain text import"), catalogs, "Subtitle Format/TXT/Default Style Catalog");

	p->SetSizerAndFit(p->sizer);
}

/// Audio preferences page
void Audio(wxTreebook *book, Preferences *parent) {
	auto p = new OptionPage(book, parent, _("Audio"));

	auto general = p->PageSizer(_("Options"));
	p->OptionAdd(general, _("Default mouse wheel to zoom"), "Audio/Wheel Default to Zoom");
	p->OptionAdd(general, _("Lock scroll on cursor"), "Audio/Lock Scroll on Cursor");
	p->OptionAdd(general, _("Snap markers by default"), "Audio/Snap/Enable");
	p->OptionAdd(general, _("Auto-focus on mouse over"), "Audio/Auto/Focus");
	p->OptionAdd(general, _("Play audio when stepping in video"), "Audio/Plays When Stepping Video");
	p->OptionAdd(general, _("Left-click-drag moves end marker"), "Audio/Drag Timing");
	p->OptionAdd(general, _("Default timing length (ms)"), "Timing/Default Duration", 0, 36000);
	p->OptionAdd(general, _("Default lead-in length (ms)"), "Audio/Lead/IN", 0, 36000);
	p->OptionAdd(general, _("Default lead-out length (ms)"), "Audio/Lead/OUT", 0, 36000);

	p->OptionAdd(general, _("Marker drag-start sensitivity (px)"), "Audio/Start Drag Sensitivity", 1, 15);
	p->OptionAdd(general, _("Line boundary thickness (px)"), "Audio/Line Boundaries Thickness", 1, 5);
	p->OptionAdd(general, _("Maximum snap distance (px)"), "Audio/Snap/Distance", 0, 25);

	const wxString dtl_arr[] = { _("Don't show"), _("Show previous"), _("Show previous and next"), _("Show all") };
	wxArrayString choice_dtl(4, dtl_arr);
	p->OptionChoice(general, _("Show inactive lines"), choice_dtl, "Audio/Inactive Lines Display Mode");
	p->CellSkip(general);
	p->OptionAdd(general, _("Include commented inactive lines"), "Audio/Display/Draw/Inactive Comments");

	auto display = p->PageSizer(_("Display Visual Options"));
	p->OptionAdd(display, _("Keyframes in dialogue mode"), "Audio/Display/Draw/Keyframes in Dialogue Mode");
	p->OptionAdd(display, _("Keyframes in karaoke mode"), "Audio/Display/Draw/Keyframes in Karaoke Mode");
	p->OptionAdd(display, _("Cursor time"), "Audio/Display/Draw/Cursor Time");
	p->OptionAdd(display, _("Video position"), "Audio/Display/Draw/Video Position");
	p->OptionAdd(display, _("Seconds boundaries"), "Audio/Display/Draw/Seconds");
	p->CellSkip(display);
	p->OptionChoice(display, _("Waveform Style"), AudioWaveformRenderer::GetWaveformStyles(), "Audio/Display/Waveform Style");

	auto label = p->PageSizer(_("Audio labels"));
	p->OptionFont(label, "Audio/Karaoke/");

	p->SetSizerAndFit(p->sizer);
}

/// Video preferences page
void Video(wxTreebook *book, Preferences *parent) {
	auto p = new OptionPage(book, parent, _("Video"));

	auto general = p->PageSizer(_("Options"));
	p->OptionAdd(general, _("Show keyframes in slider"), "Video/Slider/Show Keyframes");
	p->CellSkip(general);
	p->OptionAdd(general, _("Only show visual tools when mouse is over video"), "Tool/Visual/Autohide");
	p->CellSkip(general);
	p->OptionAdd(general, _("Seek video to line start on selection change"), "Video/Subtitle Sync");
	p->CellSkip(general);
	p->OptionAdd(general, _("Automatically open audio when opening video"), "Video/Open Audio");
	p->CellSkip(general);

	const wxString czoom_arr[24] = { "12.5%", "25%", "37.5%", "50%", "62.5%", "75%", "87.5%", "100%", "112.5%", "125%", "137.5%", "150%", "162.5%", "175%", "187.5%", "200%", "212.5%", "225%", "237.5%", "250%", "262.5%", "275%", "287.5%", "300%" };
	wxArrayString choice_zoom(24, czoom_arr);
	p->OptionChoice(general, _("Default Zoom"), choice_zoom, "Video/Default Zoom");

	p->OptionAdd(general, _("Fast jump step in frames"), "Video/Slider/Fast Jump Step");

	const wxString cscr_arr[3] = { "?video", "?script", "." };
	wxArrayString scr_res(3, cscr_arr);
	p->OptionChoice(general, _("Screenshot save path"), scr_res, "Path/Screenshot");

	auto resolution = p->PageSizer(_("Script Resolution"));
	wxControl *autocb = p->OptionAdd(resolution, _("Use resolution of first video opened"), "Subtitle/Default Resolution/Auto");
	p->CellSkip(resolution);
	p->DisableIfChecked(autocb,
		p->OptionAdd(resolution, _("Default width"), "Subtitle/Default Resolution/Width"));
	p->DisableIfChecked(autocb,
		p->OptionAdd(resolution, _("Default height"), "Subtitle/Default Resolution/Height"));

	const wxString cres_arr[] = {_("Never"), _("Ask"), _("Always set"), _("Always resample")};
	wxArrayString choice_res(4, cres_arr);
	p->OptionChoice(resolution, _("Match video resolution on open"), choice_res, "Video/Script Resolution Mismatch");

	p->SetSizerAndFit(p->sizer);
}

/// Interface preferences page
void Interface(wxTreebook *book, Preferences *parent) {
	auto p = new OptionPage(book, parent, _("Interface"));

	auto edit_box = p->PageSizer(_("Edit Box"));
	p->OptionAdd(edit_box, _("Enable call tips"), "App/Call Tips");
	p->OptionAdd(edit_box, _("Overwrite in time boxes"), "Subtitle/Time Edit/Insert Mode");
	p->CellSkip(edit_box);
	p->OptionAdd(edit_box, _("Enable syntax highlighting"), "Subtitle/Highlight/Syntax");
	p->OptionBrowse(edit_box, _("Dictionaries path"), "Path/Dictionary");
	p->OptionFont(edit_box, "Subtitle/Edit Box/");

	auto character_count = p->PageSizer(_("Character Counter"));
	p->OptionAdd(character_count, _("Maximum characters per line"), "Subtitle/Character Limit", 0, 1000);
	p->OptionAdd(character_count, _("Characters Per Second Warning Threshold"), "Subtitle/Character Counter/CPS Warning Threshold", 0, 1000);
	p->OptionAdd(character_count, _("Characters Per Second Error Threshold"), "Subtitle/Character Counter/CPS Error Threshold", 0, 1000);
	p->OptionAdd(character_count, _("Ignore whitespace"), "Subtitle/Character Counter/Ignore Whitespace");
	p->OptionAdd(character_count, _("Ignore punctuation"), "Subtitle/Character Counter/Ignore Punctuation");

	auto grid = p->PageSizer(_("Grid"));
	p->OptionAdd(grid, _("Focus grid on click"), "Subtitle/Grid/Focus Allow");
	p->OptionAdd(grid, _("Highlight visible subtitles"), "Subtitle/Grid/Highlight Subtitles in Frame");
	p->OptionAdd(grid, _("Hide overrides symbol"), "Subtitle/Grid/Hide Overrides Char");
	p->OptionFont(grid, "Subtitle/Grid/");

	p->SetSizerAndFit(p->sizer);
}

/// Interface Colours preferences subpage
void Interface_Colours(wxTreebook *book, Preferences *parent) {
	auto p = new OptionPage(book, parent, _("Colors"), OptionPage::PAGE_SCROLL|OptionPage::PAGE_SUB);

	delete p->sizer;
	wxSizer *main_sizer = new wxBoxSizer(wxHORIZONTAL);

	p->sizer = new wxBoxSizer(wxVERTICAL);
	main_sizer->Add(p->sizer, wxEXPAND);

	auto audio = p->PageSizer(_("Audio Display"));
	p->OptionAdd(audio, _("Play cursor"), "Colour/Audio Display/Play Cursor");
	p->OptionAdd(audio, _("Line boundary start"), "Colour/Audio Display/Line boundary Start");
	p->OptionAdd(audio, _("Line boundary end"), "Colour/Audio Display/Line boundary End");
	p->OptionAdd(audio, _("Line boundary inactive line"), "Colour/Audio Display/Line Boundary Inactive Line");
	p->OptionAdd(audio, _("Syllable boundaries"), "Colour/Audio Display/Syllable Boundaries");
	p->OptionAdd(audio, _("Seconds boundaries"), "Colour/Audio Display/Seconds Line");

	auto syntax = p->PageSizer(_("Syntax Highlighting"));
	p->OptionAdd(syntax, _("Background"), "Colour/Subtitle/Background");
	p->OptionAdd(syntax, _("Normal"), "Colour/Subtitle/Syntax/Normal");
	p->OptionAdd(syntax, _("Comments"), "Colour/Subtitle/Syntax/Comment");
	p->OptionAdd(syntax, _("Drawings"), "Colour/Subtitle/Syntax/Drawing");
	p->OptionAdd(syntax, _("Brackets"), "Colour/Subtitle/Syntax/Brackets");
	p->OptionAdd(syntax, _("Slashes and Parentheses"), "Colour/Subtitle/Syntax/Slashes");
	p->OptionAdd(syntax, _("Tags"), "Colour/Subtitle/Syntax/Tags");
	p->OptionAdd(syntax, _("Parameters"), "Colour/Subtitle/Syntax/Parameters");
	p->OptionAdd(syntax, _("Error"), "Colour/Subtitle/Syntax/Error");
	p->OptionAdd(syntax, _("Error Background"), "Colour/Subtitle/Syntax/Background/Error");
	p->OptionAdd(syntax, _("Line Break"), "Colour/Subtitle/Syntax/Line Break");
	p->OptionAdd(syntax, _("Karaoke templates"), "Colour/Subtitle/Syntax/Karaoke Template");
	p->OptionAdd(syntax, _("Karaoke variables"), "Colour/Subtitle/Syntax/Karaoke Variable");

	p->sizer = new wxBoxSizer(wxVERTICAL);
	main_sizer->AddSpacer(5);
	main_sizer->Add(p->sizer, wxEXPAND);

	auto color_schemes = p->PageSizer(_("Audio Color Schemes"));
	wxArrayString schemes = to_wx(OPT_GET("Audio/Colour Schemes")->GetListString());
	p->OptionChoice(color_schemes, _("Spectrum"), schemes, "Colour/Audio Display/Spectrum");
	p->OptionChoice(color_schemes, _("Waveform"), schemes, "Colour/Audio Display/Waveform");

	auto grid = p->PageSizer(_("Subtitle Grid"));
	p->OptionAdd(grid, _("Standard foreground"), "Colour/Subtitle Grid/Standard");
	p->OptionAdd(grid, _("Standard background"), "Colour/Subtitle Grid/Background/Background");
	p->OptionAdd(grid, _("Selection foreground"), "Colour/Subtitle Grid/Selection");
	p->OptionAdd(grid, _("Selection background"), "Colour/Subtitle Grid/Background/Selection");
	p->OptionAdd(grid, _("Collision foreground"), "Colour/Subtitle Grid/Collision");
	p->OptionAdd(grid, _("In frame background"), "Colour/Subtitle Grid/Background/Inframe");
	p->OptionAdd(grid, _("Comment background"), "Colour/Subtitle Grid/Background/Comment");
	p->OptionAdd(grid, _("Selected comment background"), "Colour/Subtitle Grid/Background/Selected Comment");
	p->OptionAdd(grid, _("Header background"), "Colour/Subtitle Grid/Header");
	p->OptionAdd(grid, _("Left Column"), "Colour/Subtitle Grid/Left Column");
	p->OptionAdd(grid, _("Active Line Border"), "Colour/Subtitle Grid/Active Border");
	p->OptionAdd(grid, _("Lines"), "Colour/Subtitle Grid/Lines");
	p->OptionAdd(grid, _("CPS Error"), "Colour/Subtitle Grid/CPS Error");

	p->sizer = main_sizer;

	p->SetSizerAndFit(p->sizer);
}

/// Backup preferences page
void Backup(wxTreebook *book, Preferences *parent) {
	auto p = new OptionPage(book, parent, _("Backup"));

	auto save = p->PageSizer(_("Automatic Save"));
	wxControl *cb = p->OptionAdd(save, _("Enable"), "App/Auto/Save");
	p->CellSkip(save);
	p->EnableIfChecked(cb,
		p->OptionAdd(save, _("Interval in seconds"), "App/Auto/Save Every Seconds", 1));
	p->OptionBrowse(save, _("Path"), "Path/Auto/Save", cb, true);
	p->OptionAdd(save, _("Autosave after every change"), "App/Auto/Save on Every Change");

	auto backup = p->PageSizer(_("Automatic Backup"));
	cb = p->OptionAdd(backup, _("Enable"), "App/Auto/Backup");
	p->CellSkip(backup);
	p->OptionBrowse(backup, _("Path"), "Path/Auto/Backup", cb, true);

	p->SetSizerAndFit(p->sizer);
}

/// Automation preferences page
void Automation(wxTreebook *book, Preferences *parent) {
	auto p = new OptionPage(book, parent, _("Automation"));

	auto general = p->PageSizer(_("General"));

	p->OptionAdd(general, _("Base path"), "Path/Automation/Base");
	p->OptionAdd(general, _("Include path"), "Path/Automation/Include");
	p->OptionAdd(general, _("Auto-load path"), "Path/Automation/Autoload");

	const wxString tl_arr[6] = { _("0: Fatal"), _("1: Error"), _("2: Warning"), _("3: Hint"), _("4: Debug"), _("5: Trace") };
	wxArrayString tl_choice(6, tl_arr);
	p->OptionChoice(general, _("Trace level"), tl_choice, "Automation/Trace Level");

	const wxString ar_arr[4] = { _("No scripts"), _("Subtitle-local scripts"), _("Global autoload scripts"), _("All scripts") };
	wxArrayString ar_choice(4, ar_arr);
	p->OptionChoice(general, _("Autoreload on Export"), ar_choice, "Automation/Autoreload Mode");

	p->SetSizerAndFit(p->sizer);
}

/// Advanced preferences page
void Advanced(wxTreebook *book, Preferences *parent) {
	auto p = new OptionPage(book, parent, _("Advanced"));

	auto general = p->PageSizer(_("General"));

	auto warning = new wxStaticText(p, wxID_ANY ,_("Changing these settings might result in bugs and/or crashes.  Do not touch these unless you know what you're doing."));
	warning->SetFont(wxFont(12, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
	p->sizer->Fit(p);
	warning->Wrap(400);
	general->Add(warning, 0, wxALL, 5);

	p->SetSizerAndFit(p->sizer);
}

/// Advanced Audio preferences subpage
void Advanced_Audio(wxTreebook *book, Preferences *parent) {
	auto p = new OptionPage(book, parent, _("Audio"), OptionPage::PAGE_SUB);

	auto expert = p->PageSizer(_("Expert"));

	wxArrayString ap_choice = to_wx(GetAudioProviderNames());
	p->OptionChoice(expert, _("Audio provider"), ap_choice, "Audio/Provider");

	wxArrayString apl_choice = to_wx(AudioPlayerFactory::GetClasses());
	p->OptionChoice(expert, _("Audio player"), apl_choice, "Audio/Player");

	auto cache = p->PageSizer(_("Cache"));
	const wxString ct_arr[3] = { _("None (NOT RECOMMENDED)"), _("RAM"), _("Hard Disk") };
	wxArrayString ct_choice(3, ct_arr);
	p->OptionChoice(cache, _("Cache type"), ct_choice, "Audio/Cache/Type");
	p->OptionBrowse(cache, _("Path"), "Audio/Cache/HD/Location");

	auto spectrum = p->PageSizer(_("Spectrum"));

	const wxString sq_arr[4] = { _("Regular quality"), _("Better quality"), _("High quality"), _("Insane quality") };
	wxArrayString sq_choice(4, sq_arr);
	p->OptionChoice(spectrum, _("Quality"), sq_choice, "Audio/Renderer/Spectrum/Quality");

	p->OptionAdd(spectrum, _("Cache memory max (MB)"), "Audio/Renderer/Spectrum/Memory Max", 2, 1024);

#ifdef WITH_AVISYNTH
	auto avisynth = p->PageSizer("Avisynth");
	const wxString adm_arr[3] = { "ConvertToMono", "GetLeftChannel", "GetRightChannel" };
	wxArrayString adm_choice(3, adm_arr);
	p->OptionChoice(avisynth, _("Avisynth down-mixer"), adm_choice, "Audio/Downmixer");
	p->OptionAdd(avisynth, _("Force sample rate"), "Provider/Audio/AVS/Sample Rate");
#endif

#ifdef WITH_FFMS2
	auto ffms = p->PageSizer("FFmpegSource");

	const wxString error_modes[] = { _("Ignore"), _("Clear"), _("Stop"), _("Abort") };
	wxArrayString error_modes_choice(4, error_modes);
	p->OptionChoice(ffms, _("Audio indexing error handling mode"), error_modes_choice, "Provider/Audio/FFmpegSource/Decode Error Handling");

	p->OptionAdd(ffms, _("Always index all audio tracks"), "Provider/FFmpegSource/Index All Tracks");
#endif

#ifdef WITH_PORTAUDIO
	auto portaudio = p->PageSizer("Portaudio");
	p->OptionChoice(portaudio, _("Portaudio device"), PortAudioPlayer::GetOutputDevices(), "Player/Audio/PortAudio/Device Name");
#endif

#ifdef WITH_OSS
	auto oss = p->PageSizer("OSS");
	p->OptionBrowse(oss, _("OSS Device"), "Player/Audio/OSS/Device");
#endif

#ifdef WITH_DIRECTSOUND
	auto dsound = p->PageSizer("DirectSound");
	p->OptionAdd(dsound, _("Buffer latency"), "Player/Audio/DirectSound/Buffer Latency", 1, 1000);
	p->OptionAdd(dsound, _("Buffer length"), "Player/Audio/DirectSound/Buffer Length", 1, 100);
#endif

	p->SetSizerAndFit(p->sizer);
}

/// Advanced Video preferences subpage
void Advanced_Video(wxTreebook *book, Preferences *parent) {
	auto p = new OptionPage(book, parent, _("Video"), OptionPage::PAGE_SUB);

	auto expert = p->PageSizer(_("Expert"));

	wxArrayString vp_choice = to_wx(VideoProviderFactory::GetClasses());
	p->OptionChoice(expert, _("Video provider"), vp_choice, "Video/Provider");

	wxArrayString sp_choice = to_wx(SubtitlesProviderFactory::GetClasses());
	p->OptionChoice(expert, _("Subtitles provider"), sp_choice, "Subtitle/Provider");

#ifdef WITH_AVISYNTH
	auto avisynth = p->PageSizer("Avisynth");
	p->OptionAdd(avisynth, _("Allow pre-2.56a Avisynth"), "Provider/Avisynth/Allow Ancient");
	p->CellSkip(avisynth);
	p->OptionAdd(avisynth, _("Avisynth memory limit"), "Provider/Avisynth/Memory Max");
#endif

#ifdef WITH_FFMS2
	auto ffms = p->PageSizer("FFmpegSource");

	const wxString log_levels[] = { "Quiet", "Panic", "Fatal", "Error", "Warning", "Info", "Verbose", "Debug" };
	wxArrayString log_levels_choice(8, log_levels);
	p->OptionChoice(ffms, _("Debug log verbosity"), log_levels_choice, "Provider/FFmpegSource/Log Level");

	p->OptionAdd(ffms, _("Decoding threads"), "Provider/Video/FFmpegSource/Decoding Threads", -1);
	p->OptionAdd(ffms, _("Enable unsafe seeking"), "Provider/Video/FFmpegSource/Unsafe Seeking");
#endif

	p->SetSizerAndFit(p->sizer);
}

/// wxDataViewIconTextRenderer with command name autocompletion
class CommandRenderer final : public wxDataViewCustomRenderer {
	wxArrayString autocomplete;
	wxDataViewIconText value;
	static const int icon_width = 20;

public:
	CommandRenderer()
	: wxDataViewCustomRenderer("wxDataViewIconText", wxDATAVIEW_CELL_EDITABLE)
	, autocomplete(to_wx(cmd::get_registered_commands()))
	{
	}

	wxWindow *CreateEditorCtrl(wxWindow *parent, wxRect label_rect, wxVariant const& value) override {
		wxDataViewIconText iconText;
		iconText << value;

		wxString text = iconText.GetText();

		// adjust the label rect to take the width of the icon into account
		label_rect.x += icon_width;
		label_rect.width -= icon_width;

		wxTextCtrl* ctrl = new wxTextCtrl(parent, -1, text, label_rect.GetPosition(), label_rect.GetSize(), wxTE_PROCESS_ENTER);
		ctrl->SetInsertionPointEnd();
		ctrl->SelectAll();
		ctrl->AutoComplete(autocomplete);
		return ctrl;
	}

	bool SetValue(wxVariant const& var) override {
		value << var;
		return true;
	}

	bool Render(wxRect rect, wxDC *dc, int state) override {
		wxIcon const& icon = value.GetIcon();
		if (icon.IsOk())
			dc->DrawIcon(icon, rect.x, rect.y + (rect.height - icon.GetHeight()) / 2);

		RenderText(value.GetText(), icon_width, rect, dc, state);

		return true;
	}

	wxSize GetSize() const override {
		if (!value.GetText().empty()) {
			wxSize size = GetTextExtent(value.GetText());
			size.x += icon_width;
			return size;
		}
		return wxSize(80,20);
	}

	bool GetValueFromEditorCtrl(wxWindow* editor, wxVariant &var) override {
		wxTextCtrl *text = static_cast<wxTextCtrl*>(editor);
		wxDataViewIconText iconText(text->GetValue(), value.GetIcon());
		var << iconText;
		return true;
	}

	bool GetValue(wxVariant &) const override { return false; }
	bool HasEditorCtrl() const override { return true; }
};

class HotkeyRenderer final : public wxDataViewCustomRenderer {
	wxString value;
	wxTextCtrl *ctrl = nullptr;

public:
	HotkeyRenderer()
	: wxDataViewCustomRenderer("string", wxDATAVIEW_CELL_EDITABLE)
	{ }

	wxWindow *CreateEditorCtrl(wxWindow *parent, wxRect label_rect, wxVariant const& var) override {
		ctrl = new wxTextCtrl(parent, -1, var.GetString(), label_rect.GetPosition(), label_rect.GetSize(), wxTE_PROCESS_ENTER);
		ctrl->SetInsertionPointEnd();
		ctrl->SelectAll();
		ctrl->Bind(wxEVT_CHAR_HOOK, &HotkeyRenderer::OnKeyDown, this);
		return ctrl;
	}

	void OnKeyDown(wxKeyEvent &evt) {
		ctrl->ChangeValue(to_wx(hotkey::keypress_to_str(evt.GetKeyCode(), evt.GetModifiers())));
	}

	bool SetValue(wxVariant const& var) override {
		value = var.GetString();
		return true;
	}

	bool Render(wxRect rect, wxDC *dc, int state) override {
		RenderText(value, 0, rect, dc, state);
		return true;
	}

	bool GetValueFromEditorCtrl(wxWindow*, wxVariant &var) override {
		var = ctrl->GetValue();
		return true;
	}

	bool GetValue(wxVariant &) const override { return false; }
	wxSize GetSize() const override { return !value ? wxSize(80, 20) : GetTextExtent(value); }
	bool HasEditorCtrl() const override { return true; }
};

static void edit_item(wxDataViewCtrl *dvc, wxDataViewItem item) {
	dvc->EditItem(item, dvc->GetColumn(0));
}

class Interface_Hotkeys final : public OptionPage {
	wxDataViewCtrl *dvc;
	wxObjectDataPtr<HotkeyDataViewModel> model;
	wxSearchCtrl *quick_search;

	void OnNewButton(wxCommandEvent&);
	void OnUpdateFilter(wxCommandEvent&);
public:
	Interface_Hotkeys(wxTreebook *book, Preferences *parent);
};

/// Interface Hotkeys preferences subpage
Interface_Hotkeys::Interface_Hotkeys(wxTreebook *book, Preferences *parent)
: OptionPage(book, parent, _("Hotkeys"), OptionPage::PAGE_SUB)
, model(new HotkeyDataViewModel(parent))
{
	quick_search = new wxSearchCtrl(this, -1);
	auto new_button = new wxButton(this, -1, _("&New"));
	auto edit_button = new wxButton(this, -1, _("&Edit"));
	auto delete_button = new wxButton(this, -1, _("&Delete"));

	new_button->Bind(wxEVT_BUTTON, &Interface_Hotkeys::OnNewButton, this);
	edit_button->Bind(wxEVT_BUTTON, [=](wxCommandEvent&) { edit_item(dvc, dvc->GetSelection()); });
	delete_button->Bind(wxEVT_BUTTON, [=](wxCommandEvent&) { model->Delete(dvc->GetSelection()); });

	quick_search->Bind(wxEVT_TEXT, &Interface_Hotkeys::OnUpdateFilter, this);
	quick_search->Bind(wxEVT_SEARCHCTRL_CANCEL_BTN, [=](wxCommandEvent&) { quick_search->SetValue(""); });

	dvc = new wxDataViewCtrl(this, -1);
	dvc->AssociateModel(model.get());
#ifndef __APPLE__
	dvc->AppendColumn(new wxDataViewColumn("Hotkey", new HotkeyRenderer, 0, 125, wxALIGN_LEFT, wxCOL_SORTABLE | wxCOL_RESIZABLE));
	dvc->AppendColumn(new wxDataViewColumn("Command", new CommandRenderer, 1, 250, wxALIGN_LEFT, wxCOL_SORTABLE | wxCOL_RESIZABLE));
#else
	auto col = new wxDataViewColumn("Hotkey", new wxDataViewTextRenderer("string", wxDATAVIEW_CELL_EDITABLE), 0, 150, wxALIGN_LEFT, wxCOL_SORTABLE | wxCOL_RESIZABLE);
	col->SetMinWidth(150);
	dvc->AppendColumn(col);
	dvc->AppendColumn(new wxDataViewColumn("Command", new wxDataViewIconTextRenderer("wxDataViewIconText", wxDATAVIEW_CELL_EDITABLE), 1, 250, wxALIGN_LEFT, wxCOL_SORTABLE | wxCOL_RESIZABLE));
#endif
	dvc->AppendTextColumn("Description", 2, wxDATAVIEW_CELL_INERT, 300, wxALIGN_LEFT, wxCOL_SORTABLE | wxCOL_RESIZABLE);

	wxSizer *buttons = new wxBoxSizer(wxHORIZONTAL);
	buttons->Add(quick_search, wxSizerFlags().Border());
	buttons->AddStretchSpacer(1);
	buttons->Add(new_button, wxSizerFlags().Border());
	buttons->Add(edit_button, wxSizerFlags().Border());
	buttons->Add(delete_button, wxSizerFlags().Border());

	sizer->Add(buttons, wxSizerFlags().Expand());
	sizer->Add(dvc, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT));

	SetSizerAndFit(sizer);
}

void Interface_Hotkeys::OnNewButton(wxCommandEvent&) {
	wxDataViewItem sel = dvc->GetSelection();
	dvc->ExpandAncestors(sel);
	dvc->Expand(sel);

	wxDataViewItem new_item = model->New(sel);
	if (new_item.IsOk()) {
		dvc->Select(new_item);
		dvc->EnsureVisible(new_item);
		edit_item(dvc, new_item);
	}
}

void Interface_Hotkeys::OnUpdateFilter(wxCommandEvent&) {
	model->SetFilter(quick_search->GetValue());

	if (!quick_search->GetValue().empty()) {
		wxDataViewItemArray contexts;
		model->GetChildren(wxDataViewItem(nullptr), contexts);
		for (auto const& context : contexts)
			dvc->Expand(context);
	}
}
}

void Preferences::SetOption(std::unique_ptr<agi::OptionValue> new_value) {
	pending_changes[new_value->GetName()] = std::move(new_value);
	applyButton->Enable(true);
}

void Preferences::AddPendingChange(Thunk const& callback) {
	pending_callbacks.push_back(callback);
	applyButton->Enable(true);
}

void Preferences::AddChangeableOption(std::string const& name) {
	option_names.push_back(name);
}

void Preferences::OnOK(wxCommandEvent &event) {
	OnApply(event);
	EndModal(0);
}

void Preferences::OnApply(wxCommandEvent &) {
	for (auto const& change : pending_changes)
		OPT_SET(change.first)->Set(change.second.get());
	pending_changes.clear();

	for (auto const& thunk : pending_callbacks)
		thunk();
	pending_callbacks.clear();

	applyButton->Enable(false);
	config::opt->Flush();
}

void Preferences::OnResetDefault(wxCommandEvent&) {
	if (wxYES != wxMessageBox(_("Are you sure that you want to restore the defaults? All your settings will be overridden."), _("Restore defaults?"), wxYES_NO))
		return;

	for (auto const& opt_name : option_names) {
		agi::OptionValue *opt = OPT_SET(opt_name);
		if (!opt->IsDefault())
			opt->Reset();
	}
	config::opt->Flush();

	agi::hotkey::Hotkey def_hotkeys("", GET_DEFAULT_CONFIG(default_hotkey));
	hotkey::inst->SetHotkeyMap(def_hotkeys.GetHotkeyMap());

	// Close and reopen the dialog to update all the controls with the new values
	OPT_SET("Tool/Preferences/Page")->SetInt(book->GetSelection());
	EndModal(-1);
}

Preferences::Preferences(wxWindow *parent): wxDialog(parent, -1, _("Preferences"), wxDefaultPosition, wxSize(-1, -1), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
	SetIcon(GETICON(options_button_16));

	book = new wxTreebook(this, -1, wxDefaultPosition, wxDefaultSize);
	General(book, this);
	General_DefaultStyles(book, this);
	Audio(book, this);
	Video(book, this);
	Interface(book, this);
	Interface_Colours(book, this);
	new Interface_Hotkeys(book, this);
	Backup(book, this);
	Automation(book, this);
	Advanced(book, this);
	Advanced_Audio(book, this);
	Advanced_Video(book, this);

	book->Fit();

	book->ChangeSelection(OPT_GET("Tool/Preferences/Page")->GetInt());
	book->Bind(wxEVT_TREEBOOK_PAGE_CHANGED, [](wxBookCtrlEvent &evt) {
		OPT_SET("Tool/Preferences/Page")->SetInt(evt.GetSelection());
	});

	// Bottom Buttons
	auto stdButtonSizer = CreateStdDialogButtonSizer(wxOK | wxCANCEL | wxAPPLY | wxHELP);
	applyButton = stdButtonSizer->GetApplyButton();
	wxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
	auto defaultButton = new wxButton(this, -1, _("&Restore Defaults"));
	buttonSizer->Add(defaultButton, wxSizerFlags(0).Expand());
	buttonSizer->AddStretchSpacer(1);
	buttonSizer->Add(stdButtonSizer, wxSizerFlags(0).Expand());

	// Main Sizer
	wxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);
	mainSizer->Add(book, wxSizerFlags(1).Expand().Border());
	mainSizer->Add(buttonSizer, wxSizerFlags(0).Expand().Border(wxALL & ~wxTOP));

	SetSizerAndFit(mainSizer);
	CenterOnParent();

	applyButton->Enable(false);

	Bind(wxEVT_BUTTON, &Preferences::OnOK, this, wxID_OK);
	Bind(wxEVT_BUTTON, &Preferences::OnApply, this, wxID_APPLY);
	Bind(wxEVT_BUTTON, std::bind(&HelpButton::OpenPage, "Options"), wxID_HELP);
	defaultButton->Bind(wxEVT_BUTTON, &Preferences::OnResetDefault, this);
}

void ShowPreferences(wxWindow *parent) {
	while (Preferences(parent).ShowModal() < 0);
}
