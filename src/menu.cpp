// Copyright (c) 2011, Thomas Goyne <plorkyeran@aegisub.org>
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

/// @file menu.cpp
/// @brief Dynamic menu and toolbar generator.
/// @ingroup menu

#include "include/aegisub/menu.h"

#include "include/aegisub/context.h"
#include "include/aegisub/hotkey.h"

#include "auto4_base.h"
#include "command/command.h"
#include "compat.h"
#include "format.h"
#include "libresrc/libresrc.h"
#include "options.h"
#include "utils.h"

#include <libaegisub/cajun/reader.h>
#include <libaegisub/hotkey.h>
#include <libaegisub/json.h>
#include <libaegisub/log.h>
#include <libaegisub/make_unique.h>
#include <libaegisub/path.h>
#include <libaegisub/split.h>

#include <algorithm>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/range/algorithm_ext/push_back.hpp>
#include <boost/locale/collator.hpp>
#include <vector>
#include <wx/frame.h>
#include <wx/menu.h>
#include <wx/menuitem.h>

#ifdef __WXMAC__
#include <wx/app.h>
#endif

namespace {
/// Window ID of first menu item
static const int MENU_ID_BASE = 10000;

class MruMenu final : public wxMenu {
	std::string type;
	std::vector<wxMenuItem *> items;
	std::vector<std::string> *cmds;

	void Resize(size_t new_size) {
		for (size_t i = GetMenuItemCount(); i > new_size; --i) {
			Remove(FindItemByPosition(i - 1));
		}

		for (size_t i = GetMenuItemCount(); i < new_size; ++i) {
			if (i >= items.size()) {
				items.push_back(new wxMenuItem(this, MENU_ID_BASE + cmds->size(), "_"));
				cmds->push_back(agi::format("recent/%s/%d", boost::to_lower_copy(type), i));
			}
			Append(items[i]);
		}
	}

public:
	MruMenu(std::string type, std::vector<std::string> *cmds)
	: type(std::move(type))
	, cmds(cmds)
	{
	}

	~MruMenu() {
		// Append all items to ensure that they're all cleaned up
		Resize(items.size());
	}

	void Update() {
		const auto mru = config::mru->Get(type.c_str());

		Resize(mru->size());

		if (mru->empty()) {
			Resize(1);
			items[0]->Enable(false);
			items[0]->SetItemLabel(_("Empty"));
			return;
		}

		int i = 0;
		for (auto it = mru->begin(); it != mru->end(); ++it, ++i) {
			wxString name = it->wstring();
			if (!name.StartsWith("?"))
				name = it->filename().wstring();
			items[i]->SetItemLabel(fmt_wx("%s%d %s",
				i <= 9 ? "&" : "", i + 1,
				name));
			items[i]->Enable(true);
		}
	}
};

/// @class CommandManager
/// @brief Event dispatcher to update menus on open and handle click events
///
/// Some of what the class does could be dumped off on wx, but wxEVT_MENU_OPEN
/// is super buggy (GetMenu() often returns nullptr and it outright doesn't trigger
/// on submenus in many cases, and registering large numbers of wxEVT_UPDATE_UI
/// handlers makes everything involves events unusably slow.
class CommandManager {
	/// Menu items which need to do something on menu open
	std::vector<std::pair<std::string, wxMenuItem*>> dynamic_items;
	/// Menu items which need to be updated only when hotkeys change
	std::vector<std::pair<std::string, wxMenuItem*>> static_items;
	/// window id -> command map
	std::vector<std::string> items;
	/// MRU menus which need to be updated on menu open
	std::vector<MruMenu*> mru;

	/// Project context
	agi::Context *context;

	/// Connection for hotkey change signal
	agi::signal::Connection hotkeys_changed;

	/// Update a single dynamic menu item
	void UpdateItem(std::pair<std::string, wxMenuItem*> const& item) {
		cmd::Command *c = cmd::get(item.first);
		int flags = c->Type();
		if (flags & cmd::COMMAND_VALIDATE) {
			item.second->Enable(c->Validate(context));
			flags = c->Type();
		}
		if (flags & cmd::COMMAND_DYNAMIC_NAME)
			UpdateItemName(item);
		if (flags & cmd::COMMAND_DYNAMIC_HELP)
			item.second->SetHelp(c->StrHelp());
		if (flags & cmd::COMMAND_RADIO || flags & cmd::COMMAND_TOGGLE) {
			bool check = c->IsActive(context);
			// Don't call Check(false) on radio items as this causes wxGtk to
			// send a menu clicked event, and it should be a no-op anyway
			if (check || flags & cmd::COMMAND_TOGGLE)
				item.second->Check(check);
		}
	}

	void UpdateItemName(std::pair<std::string, wxMenuItem*> const& item) {
		cmd::Command *c = cmd::get(item.first);
		wxString text;
		if (c->Type() & cmd::COMMAND_DYNAMIC_NAME)
			text = c->StrMenu(context);
		else
			text = item.second->GetItemLabel().BeforeFirst('\t');
		item.second->SetItemLabel(text + to_wx("\t" + hotkey::get_hotkey_str_first("Default", c->name())));
	}

public:
	CommandManager(agi::Context *context)
	: context(context)
	, hotkeys_changed(hotkey::inst->AddHotkeyChangeListener(&CommandManager::OnHotkeysChanged, this))
	{
	}

	void SetContext(agi::Context *c) {
		context = c;
	}

	int AddCommand(cmd::Command *co, wxMenu *parent, std::string const& text = "") {
		return AddCommand(co, parent, text.empty() ? co->StrMenu(context) : _(to_wx(text)));
	}

	// because wxString doesn't have a move constructor
	int AddCommand(cmd::Command *co, wxMenu *parent, wxString const& menu_text) {
		return AddCommand(co, parent, wxString(menu_text));
	}

	/// Append a command to a menu and register the needed handlers
	int AddCommand(cmd::Command *co, wxMenu *parent, wxString&& menu_text) {
		int flags = co->Type();
		wxItemKind kind =
			flags & cmd::COMMAND_RADIO ? wxITEM_RADIO :
			flags & cmd::COMMAND_TOGGLE ? wxITEM_CHECK :
			wxITEM_NORMAL;

		menu_text += to_wx("\t" + hotkey::get_hotkey_str_first("Default", co->name()));

		wxMenuItem *item = new wxMenuItem(parent, MENU_ID_BASE + items.size(), menu_text, co->StrHelp(), kind);
#ifndef __WXMAC__
		/// @todo Maybe make this a configuration option instead?
		if (kind == wxITEM_NORMAL)
			item->SetBitmap(co->Icon(16));
#endif
		parent->Append(item);
		items.push_back(co->name());

		if (flags != cmd::COMMAND_NORMAL)
			dynamic_items.emplace_back(co->name(), item);
		else
			static_items.emplace_back(co->name(), item);

		return item->GetId();
	}

	/// Unregister a dynamic menu item
	void Remove(wxMenuItem *item) {
		auto pred = [=](std::pair<std::string, wxMenuItem*> const& o) {
			return o.second == item;
		};

		auto it = find_if(dynamic_items.begin(), dynamic_items.end(), pred);
		if (it != dynamic_items.end())
			dynamic_items.erase(it);
		it = find_if(static_items.begin(), static_items.end(), pred);
		if (it != static_items.end())
			static_items.erase(it);
	}

	/// Create a MRU menu and register the needed handlers
	/// @param name MRU type
	/// @param parent Menu to append the new MRU menu to
	void AddRecent(std::string const& name, wxMenu *parent) {
		mru.push_back(new MruMenu(name, &items));
		parent->AppendSubMenu(mru.back(), _("&Recent"));
	}

	void OnMenuOpen(wxMenuEvent &) {
		if (!context)
			return;
		for (auto const& item : dynamic_items) UpdateItem(item);
		for (auto item : mru) item->Update();
	}

	void OnMenuClick(wxCommandEvent &evt) {
		// This also gets clicks on unrelated things such as the toolbar, so
		// the window ID ranges really need to be unique
		size_t id = static_cast<size_t>(evt.GetId() - MENU_ID_BASE);
		if (id < items.size() && context)
			cmd::call(items[id], context);

#ifdef __WXMAC__
		else {
			switch (evt.GetId()) {
				case wxID_ABOUT:
					cmd::call("app/about", context);
					break;
				case wxID_PREFERENCES:
					cmd::call("app/options", context);
					break;
				case wxID_EXIT:
					cmd::call("app/exit", context);
					break;
				default:
					break;
			}
		}
#endif
	}

	/// Update the hotkeys for all menu items
	void OnHotkeysChanged() {
		for (auto const& item : dynamic_items) UpdateItemName(item);
		for (auto const& item : static_items) UpdateItemName(item);
	}
};

/// Wrapper for wxMenu to add a command manager
struct CommandMenu final : public wxMenu {
	CommandManager cm;
	CommandMenu(agi::Context *c) : cm(c) { }
};

/// Wrapper for wxMenuBar to add a command manager
struct CommandMenuBar final : public wxMenuBar {
	CommandManager cm;
	CommandMenuBar(agi::Context *c) : cm(c) { }
};

/// Read a string from a json object
/// @param obj Object to read from
/// @param name Index to read from
/// @param[out] value Output value to write to
/// @return Was the requested index found
bool read_entry(json::Object const& obj, const char *name, std::string *value) {
	auto it = obj.find(name);
	if (it == obj.end()) return false;
	*value = static_cast<json::String const&>(it->second);
	return true;
}

/// Get the root object of the menu configuration
json::Object const& get_menus_root() {
	static json::Object root;
	if (!root.empty()) return root;

	try {
		root = std::move(static_cast<json::Object&>(agi::json_util::file(config::path->Decode("?user/menu.json"), GET_DEFAULT_CONFIG(default_menu))));
		return root;
	}
	catch (json::Reader::ParseException const& e) {
		LOG_E("menu/parse") << "json::ParseException: " << e.what() << ", Line/offset: " << e.m_locTokenBegin.m_nLine + 1 << '/' << e.m_locTokenBegin.m_nLineOffset + 1;
		throw;
	}
	catch (std::exception const& e) {
		LOG_E("menu/parse") << e.what();
		throw;
	}
}

/// Get the menu with the specified name
/// @param name Name of menu to get
/// @return Array of menu items
json::Array const& get_menu(std::string const& name) {
	auto const& root = get_menus_root();

	auto it = root.find(name);
	if (it == root.end()) throw menu::UnknownMenu("Menu named " + name + " not found");
	return it->second;
}

wxMenu *build_menu(std::string const& name, agi::Context *c, CommandManager *cm, wxMenu *menu = nullptr);

/// Recursively process a single entry in the menu json
/// @param parent Menu to add the item(s) from this entry to
/// @param c Project context to bind the menu to
/// @param ele json object to process
/// @param cm Command manager for this menu
void process_menu_item(wxMenu *parent, agi::Context *c, json::Object const& ele, CommandManager *cm) {
	if (ele.empty()) {
		parent->AppendSeparator();
		return;
	}

	std::string submenu, recent, command, text, special;
	read_entry(ele, "special", &special);

#ifdef __WXMAC__
	if (special == "window")
		osx::make_windows_menu(parent);
#endif

	if (read_entry(ele, "submenu", &submenu) && read_entry(ele, "text", &text)) {
		wxString tl_text = _(to_wx(text));
		parent->AppendSubMenu(build_menu(submenu, c, cm), tl_text);
#ifdef __WXMAC__
		if (special == "help")
			wxApp::s_macHelpMenuTitleName = tl_text;
#endif
		return;
	}

	if (read_entry(ele, "recent", &recent)) {
		cm->AddRecent(recent, parent);
		return;
	}

	if (!read_entry(ele, "command", &command))
		return;

	read_entry(ele, "text", &text);

	try {
		int id = cm->AddCommand(cmd::get(command), parent, text);
#ifdef __WXMAC__
		if (!special.empty()) {
			if (special == "about")
				wxApp::s_macAboutMenuItemId = id;
			else if (special == "exit")
				wxApp::s_macExitMenuItemId = id;
			else if (special == "options")
				wxApp::s_macPreferencesMenuItemId = id;
		}
#else
		(void)id;
#endif
	}
	catch (agi::Exception const& e) {
#ifdef _DEBUG
		parent->Append(-1, to_wx(e.GetMessage()))->Enable(false);
#endif
		LOG_W("menu/command/not_found") << "Skipping command " << command << ": " << e.GetMessage();
	}
}

/// Build the menu with the given name
/// @param name Name of the menu
/// @param c Project context to bind the menu to
wxMenu *build_menu(std::string const& name, agi::Context *c, CommandManager *cm, wxMenu *menu) {
	if (!menu) menu = new wxMenu;
	for (auto const& item : get_menu(name))
		process_menu_item(menu, c, item, cm);
	return menu;
}

class AutomationMenu final : public wxMenu {
	agi::Context *c;
	CommandManager *cm;
	agi::signal::Connection global_slot;
	agi::signal::Connection local_slot;
	std::vector<wxMenuItem *> all_items;

	struct WorkItem {
		std::string displayname;
		cmd::Command *command;
		std::vector<WorkItem> subitems;

		WorkItem(std::string const &displayname, cmd::Command *command = nullptr)
		: displayname(displayname), command(command) { }

		WorkItem *FindOrMakeSubitem(std::string const &name) {
			auto sub = std::find_if(subitems.begin(), subitems.end(), [&](WorkItem const &item) { return item.displayname == name; });
			if (sub != subitems.end()) return &*sub;

			subitems.emplace_back(name);
			return &subitems.back();
		}

		void Sort() {
			if (command) return;
			for (auto &sub : subitems)
				sub.Sort();
			auto comp = boost::locale::comparator<std::string::value_type>();
			std::sort(subitems.begin(), subitems.end(), [&](WorkItem const &a, WorkItem const &b){
				return comp(a.displayname, b.displayname);
			});
		}

		void GenerateMenu(wxMenu *parent, AutomationMenu *am) {
			for (auto item : subitems) {
				if (item.command) {
					am->cm->AddCommand(item.command, parent, item.displayname);
					am->all_items.push_back(parent->GetMenuItems().back());
				}
				else {
					auto submenu = new wxMenu;
					parent->AppendSubMenu(submenu, to_wx(item.displayname));
					item.GenerateMenu(submenu, am);
				}
			}
		}
	};

	void Regenerate() {
		for (auto item : all_items)
			cm->Remove(item);

		wxMenuItemList &items = GetMenuItems();
		// Remove everything but automation manager and the separator
		for (size_t i = items.size() - 1; i >= 2; --i)
			Delete(items[i]);

		auto macros = config::global_scripts->GetMacros();
		boost::push_back(macros, c->local_scripts->GetMacros());
		if (macros.empty()) {
			Append(-1, _("No Automation macros loaded"))->Enable(false);
			return;
		}

		WorkItem top("");
		for (auto macro : macros) {
			const auto name = from_wx(macro->StrMenu(c));
			WorkItem *parent = &top;
			for (auto section : agi::Split(name, wxS('/'))) {
				std::string sectionname(section.begin(), section.end());

				if (section.end() == name.end()) {
					parent->subitems.emplace_back(sectionname, macro);
				}
				else {
					parent = parent->FindOrMakeSubitem(sectionname);
				}
			}
		}
		top.Sort();
		top.GenerateMenu(this, this);
	}
public:
	AutomationMenu(agi::Context *c, CommandManager *cm)
	: c(c)
	, cm(cm)
	, global_slot(config::global_scripts->AddScriptChangeListener(&AutomationMenu::Regenerate, this))
	, local_slot(c->local_scripts->AddScriptChangeListener(&AutomationMenu::Regenerate, this))
	{
		cm->AddCommand(cmd::get("am/meta"), this);
		AppendSeparator();
		Regenerate();
	}
};
}

namespace menu {
	void GetMenuBar(std::string const& name, wxFrame *window, agi::Context *c) {
#ifdef __WXMAC__
		auto bind_events = [&](CommandMenuBar *menu) {
			window->Bind(wxEVT_ACTIVATE, [=](wxActivateEvent&) { menu->cm.SetContext(c); });
			window->Bind(wxEVT_DESTROY, [=](wxWindowDestroyEvent&) {
				if (!osx::activate_top_window_other_than(window))
					menu->cm.SetContext(nullptr);
			});
		};

		if (wxMenuBar *menu = wxMenuBar::MacGetCommonMenuBar()) {
			bind_events(static_cast<CommandMenuBar *>(menu));
			return;
		}
#endif

		auto menu = agi::make_unique<CommandMenuBar>(c);
		for (auto const& item : get_menu(name)) {
			std::string submenu, disp;
			read_entry(item, "submenu", &submenu);
			read_entry(item, "text", &disp);
			if (!submenu.empty()) {
				menu->Append(build_menu(submenu, c, &menu->cm), _(to_wx(disp)));
			}
			else {
				read_entry(item, "special", &submenu);
				if (submenu == "automation")
					menu->Append(new AutomationMenu(c, &menu->cm), _(to_wx(disp)));
			}
		}

#ifdef __WXMAC__
		menu->Bind(wxEVT_MENU_OPEN, &CommandManager::OnMenuOpen, &menu->cm);
		menu->Bind(wxEVT_MENU, &CommandManager::OnMenuClick, &menu->cm);
#else
		window->Bind(wxEVT_MENU_OPEN, &CommandManager::OnMenuOpen, &menu->cm);
		window->Bind(wxEVT_MENU, &CommandManager::OnMenuClick, &menu->cm);
#endif

#ifdef __WXMAC__
		bind_events(menu.get());
		wxMenuBar::MacSetCommonMenuBar(menu.get());
#else
		window->SetMenuBar(menu.get());
#endif

		menu.release();
	}

	std::unique_ptr<wxMenu> GetMenu(std::string const& name, agi::Context *c) {
		auto menu = agi::make_unique<CommandMenu>(c);
		build_menu(name, c, &menu->cm, menu.get());
		menu->Bind(wxEVT_MENU_OPEN, &CommandManager::OnMenuOpen, &menu->cm);
		menu->Bind(wxEVT_MENU, &CommandManager::OnMenuClick, &menu->cm);
		return std::unique_ptr<wxMenu>(menu.release());
	}

	void OpenPopupMenu(wxMenu *menu, wxWindow *parent_window) {
		wxMenuEvent evt;
		evt.SetEventType(wxEVT_MENU_OPEN);
		menu->ProcessEvent(evt);
		parent_window->PopupMenu(menu);
	}
}
