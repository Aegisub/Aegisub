// Copyright (c) 2012, Thomas Goyne <plorkyeran@aegisub.org>
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

#include "font_file_lister.h"

#include "compat.h"
#include "dialog_manager.h"
#include "format.h"
#include "help_button.h"
#include "include/aegisub/context.h"
#include "libresrc/libresrc.h"
#include "options.h"
#include "utils.h"
#include "value_event.h"

#include <libaegisub/dispatch.h>
#include <libaegisub/format_path.h>
#include <libaegisub/fs.h>
#include <libaegisub/path.h>
#include <libaegisub/make_unique.h>

#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/radiobox.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/stc/stc.h>
#include <wx/textctrl.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace {
enum class FcMode {
	CheckFontsOnly = 0,
	CopyToFolder = 1,
	CopyToScriptFolder = 2,
	CopyToZip = 3,
	SymlinkToFolder = 4
};

class DialogFontsCollector final : public wxDialog {
	AssFile *subs;
	agi::Path &path;
	FcMode mode = FcMode::CheckFontsOnly;

	wxStyledTextCtrl *collection_log;
	wxButton *close_btn;
	wxButton *dest_browse_button;
	wxButton *start_btn;
	wxRadioBox *collection_mode;
	wxStaticText *dest_label;
	wxTextCtrl *dest_ctrl;

	void OnStart(wxCommandEvent &);
	void OnBrowse(wxCommandEvent &);
	void OnRadio(wxCommandEvent &e);

	/// Append text to log message from worker thread
	void OnAddText(ValueEvent<std::pair<int, wxString>>& event);
	/// Collection complete notification from the worker thread to reenable buttons
	void OnCollectionComplete(wxThreadEvent &);

	void UpdateControls();

public:
	DialogFontsCollector(agi::Context *c);
};

using color_str_pair = std::pair<int, wxString>;
wxDEFINE_EVENT(EVT_ADD_TEXT, ValueEvent<color_str_pair>);
wxDEFINE_EVENT(EVT_COLLECTION_DONE, wxThreadEvent);

void FontsCollectorThread(AssFile *subs, agi::fs::path const& destination, FcMode oper, wxEvtHandler *collector) {
	agi::dispatch::Background().Async([=]{
		auto AppendText = [&](wxString text, int colour) {
			collector->AddPendingEvent(ValueEvent<color_str_pair>(EVT_ADD_TEXT, -1, {colour, text.Clone()}));
		};

		auto paths = FontCollector(AppendText).GetFontPaths(subs);
		if (paths.empty()) {
			collector->AddPendingEvent(wxThreadEvent(EVT_COLLECTION_DONE));
			return;
		}

		// Copy fonts
		switch (oper) {
			case FcMode::CheckFontsOnly:
				collector->AddPendingEvent(wxThreadEvent(EVT_COLLECTION_DONE));
				return;
			case FcMode::SymlinkToFolder:
				AppendText(_("Symlinking fonts to folder...\n"), 0);
				break;
			case FcMode::CopyToScriptFolder:
			case FcMode::CopyToFolder:
				AppendText(_("Copying fonts to folder...\n"), 0);
				break;
			case FcMode::CopyToZip:
				AppendText(_("Copying fonts to archive...\n"), 0);
				break;
		}

		// Open zip stream if saving to compressed archive
		std::unique_ptr<wxFFileOutputStream> out;
		std::unique_ptr<wxZipOutputStream> zip;
		if (oper == FcMode::CopyToZip) {
			try {
				agi::fs::CreateDirectory(destination.parent_path());
			}
			catch (agi::fs::FileSystemError const& e) {
				AppendText(fmt_tl("* Failed to create directory '%s': %s.\n",
					destination.parent_path().wstring(), to_wx(e.GetMessage())), 2);
				collector->AddPendingEvent(wxThreadEvent(EVT_COLLECTION_DONE));
				return;
			}

			out = agi::make_unique<wxFFileOutputStream>(destination.wstring());
			if (out->IsOk())
				zip = agi::make_unique<wxZipOutputStream>(*out);

			if (!out->IsOk() || !zip || !zip->IsOk()) {
				AppendText(fmt_tl("* Failed to open %s.\n", destination), 2);
				collector->AddPendingEvent(wxThreadEvent(EVT_COLLECTION_DONE));
				return;
			}
		}

		int64_t total_size = 0;
		bool allOk = true;
		for (auto path : paths) {
			path.make_preferred();

			int ret = 0;
			total_size += agi::fs::Size(path);

			switch (oper) {
				case FcMode::SymlinkToFolder:
				case FcMode::CopyToScriptFolder:
				case FcMode::CopyToFolder: {
					auto dest = destination/path.filename();
					if (agi::fs::FileExists(dest))
						ret = 2;
#ifndef _WIN32
					else if (oper == FcMode::SymlinkToFolder) {
						// returns 0 on success, -1 on error...
						if (symlink(path.c_str(), dest.c_str()))
							ret = 0;
						else
							ret = 3;
					}
#endif
					else {
						try {
							agi::fs::Copy(path, dest);
							ret = true;
						}
						catch (...) {
							ret = false;
						}
					}
				}
				break;

				case FcMode::CopyToZip: {
					wxFFileInputStream in(path.wstring());
					if (!in.IsOk())
						ret = false;
					else {
						ret = zip->PutNextEntry(path.filename().wstring());
						zip->Write(in);
					}
				}
				default: break;
			}

			if (ret == 1)
				AppendText(fmt_tl("* Copied %s.\n", path), 1);
			else if (ret == 2)
				AppendText(fmt_tl("* %s already exists on destination.\n", path.filename()), 3);
			else if (ret == 3)
				AppendText(fmt_tl("* Symlinked %s.\n", path), 1);
			else {
				AppendText(fmt_tl("* Failed to copy %s.\n", path), 2);
				allOk = false;
			}
		}

		if (allOk)
			AppendText(_("Done. All fonts copied."), 1);
		else
			AppendText(_("Done. Some fonts could not be copied."), 2);

		if (total_size > 32 * 1024 * 1024)
			AppendText(_("\nOver 32 MB of fonts were copied. Some of the fonts may not be loaded by the player if they are all attached to a Matroska file."), 2);

		AppendText("\n", 0);

		collector->AddPendingEvent(wxThreadEvent(EVT_COLLECTION_DONE));
	});
}

DialogFontsCollector::DialogFontsCollector(agi::Context *c)
: wxDialog(c->parent, -1, _("Fonts Collector"))
, subs(c->ass.get())
, path(*c->path)
{
	SetIcon(GETICON(font_collector_button_16));

	wxString modes[] = {
		 _("Check fonts for availability")
		,_("Copy fonts to folder")
		,_("Copy fonts to subtitle file's folder")
		,_("Copy fonts to zipped archive")
#ifndef _WIN32
		,_("Symlink fonts to folder")
#endif
	};

	mode = static_cast<FcMode>(mid<int>(0, OPT_GET("Tool/Fonts Collector/Action")->GetInt(), countof(modes)));
	collection_mode = new wxRadioBox(this, -1, _("Action"), wxDefaultPosition, wxDefaultSize, countof(modes), modes, 1);
	collection_mode->SetSelection(static_cast<int>(mode));

	if (c->path->Decode("?script") == "?script")
		collection_mode->Enable(2, false);

	wxStaticBoxSizer *destination_box = new wxStaticBoxSizer(wxVERTICAL, this, _("Destination"));

	dest_label = new wxStaticText(this, -1, " ");
	dest_ctrl = new wxTextCtrl(this, -1, c->path->Decode(OPT_GET("Path/Fonts Collector Destination")->GetString()).wstring());
	dest_browse_button = new wxButton(this, -1, _("&Browse..."));

	wxSizer *dest_browse_sizer = new wxBoxSizer(wxHORIZONTAL);
	dest_browse_sizer->Add(dest_ctrl, wxSizerFlags(1).Border(wxRIGHT).Align(wxALIGN_CENTER_VERTICAL));
	dest_browse_sizer->Add(dest_browse_button, wxSizerFlags());

	destination_box->Add(dest_label, wxSizerFlags().Border(wxBOTTOM));
	destination_box->Add(dest_browse_sizer, wxSizerFlags().Expand());

	wxStaticBoxSizer *log_box = new wxStaticBoxSizer(wxVERTICAL, this, _("Log"));
	collection_log = new wxStyledTextCtrl(this, -1, wxDefaultPosition, wxSize(600, 300));
	collection_log->SetWrapMode(wxSTC_WRAP_WORD);
	collection_log->SetMarginWidth(1, 0);
	collection_log->SetReadOnly(true);
	collection_log->StyleSetForeground(1, wxColour(0, 200, 0));
	collection_log->StyleSetForeground(2, wxColour(200, 0, 0));
	collection_log->StyleSetForeground(3, wxColour(200, 100, 0));
	log_box->Add(collection_log, wxSizerFlags().Border());

	wxStdDialogButtonSizer *button_sizer = CreateStdDialogButtonSizer(wxOK | wxCANCEL | wxHELP);
	start_btn = button_sizer->GetAffirmativeButton();
	close_btn = button_sizer->GetCancelButton();
	start_btn->SetLabel(_("&Start!"));
	start_btn->SetDefault();

	wxSizer *main_sizer = new wxBoxSizer(wxVERTICAL);
	main_sizer->Add(collection_mode, wxSizerFlags().Expand().Border());
	main_sizer->Add(destination_box, wxSizerFlags().Expand().Border(wxALL & ~wxTOP));
	main_sizer->Add(log_box, wxSizerFlags().Border(wxALL & ~wxTOP));
	main_sizer->Add(button_sizer, wxSizerFlags().Right().Border(wxALL & ~wxTOP));

	SetSizerAndFit(main_sizer);
	CenterOnParent();

	// Update the browse button and label
	UpdateControls();

	start_btn->Bind(wxEVT_BUTTON, &DialogFontsCollector::OnStart, this);
	dest_browse_button->Bind(wxEVT_BUTTON, &DialogFontsCollector::OnBrowse, this);
	collection_mode->Bind(wxEVT_RADIOBOX, &DialogFontsCollector::OnRadio, this);
	button_sizer->GetHelpButton()->Bind(wxEVT_BUTTON, std::bind(&HelpButton::OpenPage, "Fonts Collector"));
	Bind(EVT_ADD_TEXT, &DialogFontsCollector::OnAddText, this);
	Bind(EVT_COLLECTION_DONE, &DialogFontsCollector::OnCollectionComplete, this);
}

void DialogFontsCollector::OnStart(wxCommandEvent &) {
	collection_log->SetReadOnly(false);
	collection_log->ClearAll();
	collection_log->SetReadOnly(true);

	agi::fs::path dest;
	if (mode != FcMode::CheckFontsOnly) {
		dest = path.Decode(mode == FcMode::CopyToScriptFolder ? "?script/" : from_wx(dest_ctrl->GetValue()));

		if (mode != FcMode::CopyToZip) {
			if (agi::fs::FileExists(dest))
				wxMessageBox(_("Invalid destination."), _("Error"), wxOK | wxICON_ERROR | wxCENTER, this);
			try {
				agi::fs::CreateDirectory(dest);
			}
			catch (agi::Exception const&) {
				wxMessageBox(_("Could not create destination folder."), _("Error"), wxOK | wxICON_ERROR | wxCENTER, this);
				return;
			}
		}
		else if (agi::fs::DirectoryExists(dest) || dest.filename().empty()) {
			wxMessageBox(_("Invalid path for .zip file."), _("Error"), wxOK | wxICON_ERROR | wxCENTER, this);
			return;
		}
	}

	if (mode != FcMode::CheckFontsOnly)
		OPT_SET("Path/Fonts Collector Destination")->SetString(dest.string());

	// Disable the UI while it runs as we don't support canceling
	EnableCloseButton(false);
	start_btn->Enable(false);
	dest_browse_button->Enable(false);
	dest_ctrl->Enable(false);
	close_btn->Enable(false);
	collection_mode->Enable(false);
	dest_label->Enable(false);

	FontsCollectorThread(subs, dest, mode, GetEventHandler());
}

void DialogFontsCollector::OnBrowse(wxCommandEvent &) {
	wxString dest;
	if (mode == FcMode::CopyToZip) {
		dest = wxFileSelector(
			_("Select archive file name"),
			dest_ctrl->GetValue(),
			wxFileName(dest_ctrl->GetValue()).GetFullName(),
			".zip", "Zip Archives (*.zip)|*.zip",
			wxFD_SAVE|wxFD_OVERWRITE_PROMPT);
	}
	else
		dest = wxDirSelector(_("Select folder to save fonts on"), dest_ctrl->GetValue(), 0);

	if (!dest.empty())
		dest_ctrl->SetValue(dest);
}

void DialogFontsCollector::OnRadio(wxCommandEvent &evt) {
	OPT_SET("Tool/Fonts Collector/Action")->SetInt(evt.GetInt());
	mode = static_cast<FcMode>(evt.GetInt());
	UpdateControls();
}

void DialogFontsCollector::UpdateControls() {
	wxString dst = dest_ctrl->GetValue();

	if (mode == FcMode::CheckFontsOnly || mode == FcMode::CopyToScriptFolder) {
		dest_ctrl->Enable(false);
		dest_browse_button->Enable(false);
		dest_label->Enable(false);
		dest_label->SetLabel(_("N/A"));
	}
	else {
		dest_ctrl->Enable(true);
		dest_browse_button->Enable(true);
		dest_label->Enable(true);

		if (mode == FcMode::CopyToFolder || mode == FcMode::SymlinkToFolder) {
			dest_label->SetLabel(_("Choose the folder where the fonts will be collected to. It will be created if it doesn't exist."));

			// Remove filename from browse box
			if (dst.Right(4) == ".zip")
				dest_ctrl->SetValue(wxFileName(dst).GetPath());
		}
		else {
			dest_label->SetLabel(_("Enter the name of the destination zip file to collect the fonts to. If a folder is entered, a default name will be used."));

			// Add filename to browse box
			if (!dst.EndsWith(".zip")) {
				wxFileName fn(dst + "//");
				fn.SetFullName("fonts.zip");
				dest_ctrl->SetValue(fn.GetFullPath());
			}
		}
	}

#ifdef __APPLE__
	// wxStaticText auto-wraps everywhere but OS X
	dest_label->Wrap(dest_label->GetParent()->GetSize().GetWidth() - 20);
	Layout();
#endif
}

void DialogFontsCollector::OnAddText(ValueEvent<color_str_pair> &event) {
	auto const& str = event.Get();
	collection_log->SetReadOnly(false);
	int pos = collection_log->GetLength();
	auto const& utf8 = str.second.utf8_str();
	collection_log->AppendTextRaw(utf8.data(), utf8.length());
	if (str.first) {
		#if wxCHECK_VERSION(3, 1, 0)
		collection_log->StartStyling(pos);
		#else
		collection_log->StartStyling(pos, 31);
		#endif
		collection_log->SetStyling(utf8.length(), str.first);
	}
	collection_log->GotoPos(pos + utf8.length());
	collection_log->SetReadOnly(true);
}

void DialogFontsCollector::OnCollectionComplete(wxThreadEvent &) {
	EnableCloseButton(true);
	start_btn->Enable();
	close_btn->Enable();
	collection_mode->Enable();
	if (path.Decode("?script") == "?script")
		collection_mode->Enable(2, false);

	UpdateControls();
}
}

void ShowFontsCollectorDialog(agi::Context *c) {
	c->dialog->Show<DialogFontsCollector>(c);
}
