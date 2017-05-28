// Copyright (c) 2006, 2007, Niels Martin Hansen
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

/// @file auto4_lua.cpp
/// @brief Lua 5.1-based scripting engine
/// @ingroup scripting
///

#include "auto4_lua.h"

#include "ass_dialogue.h"
#include "ass_file.h"
#include "ass_info.h"
#include "ass_style.h"
#include "async_video_provider.h"
#include "auto4_lua_factory.h"
#include "audio_controller.h"
#include "audio_timing.h"
#include "command/command.h"
#include "compat.h"
#include "include/aegisub/context.h"
#include "options.h"
#include "project.h"
#include "selection_controller.h"
#include "subs_controller.h"
#include "video_controller.h"
#include "utils.h"

#include <libaegisub/format.h>
#include <libaegisub/lua/ffi.h>
#include <libaegisub/lua/modules.h>
#include <libaegisub/lua/script_reader.h>
#include <libaegisub/lua/utils.h>
#include <libaegisub/make_unique.h>
#include <libaegisub/path.h>

#include <algorithm>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/scope_exit.hpp>
#include <cassert>
#include <mutex>
#include <wx/clipbrd.h>
#include <wx/log.h>
#include <wx/msgdlg.h>

using namespace agi::lua;
using namespace Automation4;

namespace {
	wxString get_wxstring(lua_State *L, int idx)
	{
		return wxString::FromUTF8(lua_tostring(L, idx));
	}

	wxString check_wxstring(lua_State *L, int idx)
	{
		return to_wx(check_string(L, idx));
	}

	void set_context(lua_State *L, const agi::Context *c)
	{
		// Explicit cast is needed to discard the const
		push_value(L, (void *)c);
		lua_setfield(L, LUA_REGISTRYINDEX, "project_context");
	}

	const agi::Context *get_context(lua_State *L)
	{
		lua_getfield(L, LUA_REGISTRYINDEX, "project_context");
		if (!lua_islightuserdata(L, -1)) {
			lua_pop(L, 1);
			return nullptr;
		}
		const agi::Context * c = static_cast<const agi::Context *>(lua_touserdata(L, -1));
		lua_pop(L, 1);
		return c;
	}

	int get_file_name(lua_State *L)
	{
		const agi::Context *c = get_context(L);
		if (c && !c->subsController->Filename().empty())
			push_value(L, c->subsController->Filename().filename());
		else
			lua_pushnil(L);
		return 1;
	}

	int get_translation(lua_State *L)
	{
		wxString str(check_wxstring(L, 1));
		push_value(L, _(str).utf8_str());
		return 1;
	}

	const char *clipboard_get()
	{
		std::string data = GetClipboard();
		if (data.empty())
			return nullptr;
		return strndup(data);
	}

	bool clipboard_set(const char *str)
	{
		bool succeeded = false;

#if wxUSE_OLE
		// OLE needs to be initialized on each thread that wants to write to
		// the clipboard, which wx does not handle automatically
		wxClipboard cb;
#else
		wxClipboard &cb = *wxTheClipboard;
#endif
		if (cb.Open()) {
			succeeded = cb.SetData(new wxTextDataObject(wxString::FromUTF8(str)));
			cb.Close();
			cb.Flush();
		}

		return succeeded;
	}

	int clipboard_init(lua_State *L)
	{
		agi::lua::register_lib_table(L, {}, "get", clipboard_get, "set", clipboard_set);
		return 1;
	}

	int frame_from_ms(lua_State *L)
	{
		const agi::Context *c = get_context(L);
		int ms = lua_tointeger(L, -1);
		lua_pop(L, 1);
		if (c && c->project->Timecodes().IsLoaded())
			push_value(L, c->videoController->FrameAtTime(ms, agi::vfr::START));
		else
			lua_pushnil(L);

		return 1;
	}

	int ms_from_frame(lua_State *L)
	{
		const agi::Context *c = get_context(L);
		int frame = lua_tointeger(L, -1);
		lua_pop(L, 1);
		if (c && c->project->Timecodes().IsLoaded())
			push_value(L, c->videoController->TimeAtFrame(frame, agi::vfr::START));
		else
			lua_pushnil(L);
		return 1;
	}

	int video_size(lua_State *L)
	{
		const agi::Context *c = get_context(L);
		if (c && c->project->VideoProvider()) {
			auto provider = c->project->VideoProvider();
			push_value(L, provider->GetWidth());
			push_value(L, provider->GetHeight());
			push_value(L, c->videoController->GetAspectRatioValue());
			push_value(L, (int)c->videoController->GetAspectRatioType());
			return 4;
		}
		else {
			lua_pushnil(L);
			return 1;
		}
	}

	int get_keyframes(lua_State *L)
	{
		if (const agi::Context *c = get_context(L))
			push_value(L, c->project->Keyframes());
		else
			lua_pushnil(L);
		return 1;
	}

	int decode_path(lua_State *L)
	{
		std::string path = check_string(L, 1);
		lua_pop(L, 1);
		if (const agi::Context *c = get_context(L))
			push_value(L, c->path->Decode(path));
		else
			push_value(L, config::path->Decode(path));
		return 1;
	}

	int cancel_script(lua_State *L)
	{
		lua_pushnil(L);
		throw error_tag();
	}

	int lua_text_textents(lua_State *L)
	{
		argcheck(L, !!lua_istable(L, 1), 1, "");
		argcheck(L, !!lua_isstring(L, 2), 2, "");

		// have to check that it looks like a style table before actually converting
		// if it's a dialogue table then an active AssFile object is required
		{
			lua_getfield(L, 1, "class");
			std::string actual_class{lua_tostring(L, -1)};
			boost::to_lower(actual_class);
			if (actual_class != "style")
				return error(L, "Not a style entry");
			lua_pop(L, 1);
		}

		lua_pushvalue(L, 1);
		std::unique_ptr<AssEntry> et(Automation4::LuaAssFile::LuaToAssEntry(L));
		lua_pop(L, 1);
		if (typeid(*et) != typeid(AssStyle))
			return error(L, "Not a style entry");

		double width, height, descent, extlead;
		if (!Automation4::CalculateTextExtents(static_cast<AssStyle*>(et.get()),
				check_string(L, 2), width, height, descent, extlead))
			return error(L, "Some internal error occurred calculating text_extents");

		push_value(L, width);
		push_value(L, height);
		push_value(L, descent);
		push_value(L, extlead);
		return 4;
	}

	int lua_get_audio_selection(lua_State *L)
	{
		const agi::Context *c = get_context(L);
		if (!c || !c->audioController || !c->audioController->GetTimingController()) {
			lua_pushnil(L);
			return 1;
		}
		const TimeRange range = c->audioController->GetTimingController()->GetActiveLineRange();
		push_value(L, range.begin());
		push_value(L, range.end());
		return 2;
	}

	int project_properties(lua_State *L)
	{
		const agi::Context *c = get_context(L);
		if (!c)
			lua_pushnil(L);
		else {
			lua_createtable(L, 0, 14);
#define PUSH_FIELD(name) set_field(L, #name, c->ass->Properties.name)
			PUSH_FIELD(automation_scripts);
			PUSH_FIELD(export_filters);
			PUSH_FIELD(export_encoding);
			PUSH_FIELD(style_storage);
			PUSH_FIELD(video_zoom);
			PUSH_FIELD(ar_value);
			PUSH_FIELD(scroll_position);
			PUSH_FIELD(active_row);
			PUSH_FIELD(ar_mode);
			PUSH_FIELD(video_position);
#undef PUSH_FIELD
			set_field(L, "audio_file", c->path->MakeAbsolute(c->ass->Properties.audio_file, "?script"));
			set_field(L, "video_file", c->path->MakeAbsolute(c->ass->Properties.video_file, "?script"));
			set_field(L, "timecodes_file", c->path->MakeAbsolute(c->ass->Properties.timecodes_file, "?script"));
			set_field(L, "keyframes_file", c->path->MakeAbsolute(c->ass->Properties.keyframes_file, "?script"));
		}
		return 1;
	}

	class LuaFeature {
		int myid = 0;
	protected:
		lua_State *L;

		void RegisterFeature();
		void UnregisterFeature();

		void GetFeatureFunction(const char *function) const;

		LuaFeature(lua_State *L) : L(L) { }
	};

	/// Run a lua function on a background thread
	/// @param L Lua state
	/// @param nargs Number of arguments the function takes
	/// @param nresults Number of values the function returns
	/// @param title Title to use for the progress dialog
	/// @param parent Parent window for the progress dialog
	/// @param can_open_config Can the function open its own dialogs?
	/// @throws agi::UserCancelException if the function fails to run to completion (either due to cancelling or errors)
	void LuaThreadedCall(lua_State *L, int nargs, int nresults, std::string const& title, wxWindow *parent, bool can_open_config);

	class LuaCommand final : public cmd::Command, private LuaFeature {
		std::string cmd_name;
		wxString display;
		wxString help;
		int cmd_type;

	public:
		LuaCommand(lua_State *L);
		~LuaCommand();

		const char* name() const override { return cmd_name.c_str(); }
		wxString StrMenu(const agi::Context *) const override { return display; }
		wxString StrDisplay(const agi::Context *) const override { return display; }
		wxString StrHelp() const override { return help; }

		int Type() const override { return cmd_type; }

		void operator()(agi::Context *c) override;
		bool Validate(const agi::Context *c) override;
		virtual bool IsActive(const agi::Context *c) override;

		static int LuaRegister(lua_State *L);
	};

	class LuaExportFilter final : public ExportFilter, private LuaFeature {
		bool has_config;
		LuaDialog *config_dialog;

	protected:
		std::unique_ptr<ScriptDialog> GenerateConfigDialog(wxWindow *parent, agi::Context *c) override;

	public:
		LuaExportFilter(lua_State *L);
		static int LuaRegister(lua_State *L);

		void ProcessSubs(AssFile *subs, wxWindow *export_dialog) override;
	};
	class LuaScript final : public Script {
		lua_State *L = nullptr;

		std::string name;
		std::string description;
		std::string author;
		std::string version;

		std::vector<cmd::Command*> macros;
		std::vector<std::unique_ptr<ExportFilter>> filters;

		/// load script and create internal structures etc.
		void Create();
		/// destroy internal structures, unreg features and delete environment
		void Destroy();

		static int LuaInclude(lua_State *L);

	public:
		LuaScript(agi::fs::path const& filename);
		~LuaScript() { Destroy(); }

		void RegisterCommand(LuaCommand *command);
		void UnregisterCommand(LuaCommand *command);
		void RegisterFilter(LuaExportFilter *filter);

		static LuaScript* GetScriptObject(lua_State *L);

		// Script implementation
		void Reload() override { Create(); }

		std::string GetName() const override { return name; }
		std::string GetDescription() const override { return description; }
		std::string GetAuthor() const override { return author; }
		std::string GetVersion() const override { return version; }
		bool GetLoadedState() const override { return L != nullptr; }

		std::vector<cmd::Command*> GetMacros() const override { return macros; }
		std::vector<ExportFilter*> GetFilters() const override;
	};

	LuaScript::LuaScript(agi::fs::path const& filename)
	: Script(filename)
	{
		Create();
	}

	void LuaScript::Create()
	{
		Destroy();

		name = GetPrettyFilename().string();

		// create lua environment
		L = luaL_newstate();
		if (!L) {
			description = "Could not initialize Lua state";
			return;
		}

		bool loaded = false;
		BOOST_SCOPE_EXIT_ALL(&) { if (!loaded) Destroy(); };
		LuaStackcheck stackcheck(L);

		// register standard libs
		preload_modules(L);
		stackcheck.check_stack(0);

		// dofile and loadfile are replaced with include
		lua_pushnil(L);
		lua_setglobal(L, "dofile");
		lua_pushnil(L);
		lua_setglobal(L, "loadfile");
		push_value(L, exception_wrapper<LuaInclude>);
		lua_setglobal(L, "include");

		// Replace the default lua module loader with our unicode compatible
		// one and set the module search path
		if (!Install(L, include_path)) {
			description = get_string_or_default(L, 1);
			lua_pop(L, 1);
			return;
		}
		stackcheck.check_stack(0);

		// prepare stuff in the registry

		// store the script's filename
		push_value(L, GetFilename().stem());
		lua_setfield(L, LUA_REGISTRYINDEX, "filename");
		stackcheck.check_stack(0);

		// reference to the script object
		push_value(L, this);
		lua_setfield(L, LUA_REGISTRYINDEX, "aegisub");
		stackcheck.check_stack(0);

		// make "aegisub" table
		lua_pushstring(L, "aegisub");
		lua_createtable(L, 0, 13);

		set_field<LuaCommand::LuaRegister>(L, "register_macro");
		set_field<LuaExportFilter::LuaRegister>(L, "register_filter");
		set_field<lua_text_textents>(L, "text_extents");
		set_field<frame_from_ms>(L, "frame_from_ms");
		set_field<ms_from_frame>(L, "ms_from_frame");
		set_field<video_size>(L, "video_size");
		set_field<get_keyframes>(L, "keyframes");
		set_field<decode_path>(L, "decode_path");
		set_field<cancel_script>(L, "cancel");
		set_field(L, "lua_automation_version", 4);
		set_field<clipboard_init>(L, "__init_clipboard");
		set_field<get_file_name>(L, "file_name");
		set_field<get_translation>(L, "gettext");
		set_field<project_properties>(L, "project_properties");
		set_field<lua_get_audio_selection>(L, "get_audio_selection");

		// store aegisub table to globals
		lua_settable(L, LUA_GLOBALSINDEX);
		stackcheck.check_stack(0);

		// load user script
		if (!LoadFile(L, GetFilename())) {
			description = get_string_or_default(L, 1);
			lua_pop(L, 1);
			return;
		}
		stackcheck.check_stack(1);

		// Insert our error handler under the user's script
		lua_pushcclosure(L, add_stack_trace, 0);
		lua_insert(L, -2);

		// and execute it
		// this is where features are registered
		if (lua_pcall(L, 0, 0, -2)) {
			// error occurred, assumed to be on top of Lua stack
			description = agi::format("Error initialising Lua script \"%s\":\n\n%s", GetPrettyFilename().string(), get_string_or_default(L, -1));
			lua_pop(L, 2); // error + error handler
			return;
		}
		lua_pop(L, 1); // error handler
		stackcheck.check_stack(0);

		lua_getglobal(L, "version");
		if (lua_isnumber(L, -1) && lua_tointeger(L, -1) == 3) {
			lua_pop(L, 1); // just to avoid tripping the stackcheck in debug
			description = "Attempted to load an Automation 3 script as an Automation 4 Lua script. Automation 3 is no longer supported.";
			return;
		}

		name = get_global_string(L, "script_name");
		description = get_global_string(L, "script_description");
		author = get_global_string(L, "script_author");
		version = get_global_string(L, "script_version");

		if (name.empty())
			name = GetPrettyFilename().string();

		lua_pop(L, 1);
		// if we got this far, the script should be ready
		loaded = true;
	}

	void LuaScript::Destroy()
	{
		// Assume the script object is clean if there's no Lua state
		if (!L) return;

		// loops backwards because commands remove themselves from macros when
		// they're unregistered
		for (int i = macros.size() - 1; i >= 0; --i)
			cmd::unreg(macros[i]->name());

		filters.clear();

		lua_close(L);
		L = nullptr;
	}

	std::vector<ExportFilter*> LuaScript::GetFilters() const
	{
		std::vector<ExportFilter *> ret;
		ret.reserve(filters.size());
		for (auto& filter : filters) ret.push_back(filter.get());
		return ret;
	}

	void LuaScript::RegisterCommand(LuaCommand *command)
	{
		for (auto macro : macros) {
			if (macro->name() == command->name()) {
				error(L, "A macro named '%s' is already defined in script '%s'",
					command->StrDisplay(nullptr).utf8_str().data(), name.c_str());
			}
		}
		macros.push_back(command);
	}

	void LuaScript::UnregisterCommand(LuaCommand *command)
	{
		macros.erase(remove(macros.begin(), macros.end(), command), macros.end());
	}

	void LuaScript::RegisterFilter(LuaExportFilter *filter)
	{
		filters.emplace_back(filter);
	}

	LuaScript* LuaScript::GetScriptObject(lua_State *L)
	{
		lua_getfield(L, LUA_REGISTRYINDEX, "aegisub");
		void *ptr = lua_touserdata(L, -1);
		lua_pop(L, 1);
		return (LuaScript*)ptr;
	}


	int LuaScript::LuaInclude(lua_State *L)
	{
		const LuaScript *s = GetScriptObject(L);

		const std::string filename(check_string(L, 1));
		agi::fs::path filepath;

		// Relative or absolute path
		if (!boost::all(filename, !boost::is_any_of("/\\")))
			filepath = s->GetFilename().parent_path()/filename;
		else { // Plain filename
			for (auto const& dir : s->include_path) {
				filepath = dir/filename;
				if (agi::fs::FileExists(filepath))
					break;
			}
		}

		if (!agi::fs::FileExists(filepath))
			return error(L, "Lua include not found: %s", filename.c_str());

		if (!LoadFile(L, filepath))
			return error(L, "Error loading Lua include \"%s\":\n%s", filename.c_str(), check_string(L, 1).c_str());

		int pretop = lua_gettop(L) - 1; // don't count the function value itself
		lua_call(L, 0, LUA_MULTRET);
		return lua_gettop(L) - pretop;
	}

	void LuaThreadedCall(lua_State *L, int nargs, int nresults, std::string const& title, wxWindow *parent, bool can_open_config)
	{
		bool failed = false;
		BackgroundScriptRunner bsr(parent, title);
		bsr.Run([&](ProgressSink *ps) {
			LuaProgressSink lps(L, ps, can_open_config);

			// Insert our error handler under the function to call
			lua_pushcclosure(L, add_stack_trace, 0);
			lua_insert(L, -nargs - 2);

			if (lua_pcall(L, nargs, nresults, -nargs - 2)) {
				if (!lua_isnil(L, -1)) {
					// if the call failed, log the error here
					ps->Log("\n\nLua reported a runtime error:\n");
					ps->Log(get_string_or_default(L, -1));
				}
				lua_pop(L, 2);
				failed = true;
			}
			else
				lua_remove(L, -nresults - 1);

			lua_gc(L, LUA_GCCOLLECT, 0);
		});
		if (failed)
			throw agi::UserCancelException("Script threw an error");
	}

	// LuaFeature
	void LuaFeature::RegisterFeature()
	{
		myid = luaL_ref(L, LUA_REGISTRYINDEX);
	}

	void LuaFeature::UnregisterFeature()
	{
		luaL_unref(L, LUA_REGISTRYINDEX, myid);
	}

	void LuaFeature::GetFeatureFunction(const char *function) const
	{
		// get this feature's function pointers
		lua_rawgeti(L, LUA_REGISTRYINDEX, myid);
		// get pointer for validation function
		push_value(L, function);
		lua_rawget(L, -2);
		// remove the function table
		lua_remove(L, -2);
		assert(lua_isfunction(L, -1));
	}

	// LuaFeatureMacro
	int LuaCommand::LuaRegister(lua_State *L)
	{
		static std::mutex mutex;
		auto command = agi::make_unique<LuaCommand>(L);
		{
			std::lock_guard<std::mutex> lock(mutex);
			cmd::reg(std::move(command));
		}
		return 0;
	}

	LuaCommand::LuaCommand(lua_State *L)
	: LuaFeature(L)
	, display(check_wxstring(L, 1))
	, help(get_wxstring(L, 2))
	, cmd_type(cmd::COMMAND_NORMAL)
	{
		lua_getfield(L, LUA_REGISTRYINDEX, "filename");
		cmd_name = agi::format("automation/lua/%s/%s", check_string(L, -1), check_string(L, 1));

		if (!lua_isfunction(L, 3))
			error(L, "The macro processing function must be a function");

		if (lua_isfunction(L, 4))
			cmd_type |= cmd::COMMAND_VALIDATE;

		if (lua_isfunction(L, 5))
			cmd_type |= cmd::COMMAND_TOGGLE;

		// new table for containing the functions for this feature
		lua_createtable(L, 0, 3);

		// store processing function
		push_value(L, "run");
		lua_pushvalue(L, 3);
		lua_rawset(L, -3);

		// store validation function
		push_value(L, "validate");
		lua_pushvalue(L, 4);
		lua_rawset(L, -3);

		// store active function
		push_value(L, "isactive");
		lua_pushvalue(L, 5);
		lua_rawset(L, -3);

		// store the table in the registry
		RegisterFeature();

		LuaScript::GetScriptObject(L)->RegisterCommand(this);
	}

	LuaCommand::~LuaCommand()
	{
		UnregisterFeature();
		LuaScript::GetScriptObject(L)->UnregisterCommand(this);
	}

	static std::vector<int> selected_rows(const agi::Context *c)
	{
		auto const& sel = c->selectionController->GetSelectedSet();
		int offset = c->ass->Info.size() + c->ass->Styles.size();
		std::vector<int> rows;
		rows.reserve(sel.size());
		for (auto line : sel)
			rows.push_back(line->Row + offset + 1);
		sort(begin(rows), end(rows));
		return rows;
	}

	bool LuaCommand::Validate(const agi::Context *c)
	{
		if (!(cmd_type & cmd::COMMAND_VALIDATE)) return true;

		set_context(L, c);

		// Error handler goes under the function to call
		lua_pushcclosure(L, add_stack_trace, 0);

		GetFeatureFunction("validate");
		auto subsobj = new LuaAssFile(L, c->ass.get());

		push_value(L, selected_rows(c));
		if (auto active_line = c->selectionController->GetActiveLine())
			push_value(L, active_line->Row + c->ass->Info.size() + c->ass->Styles.size() + 1);
		else
			lua_pushnil(L);

		int err = lua_pcall(L, 3, 2, -5 /* three args, function, error handler */);
		subsobj->ProcessingComplete();

		if (err) {
			wxLogWarning("Runtime error in Lua macro validation function:\n%s", get_wxstring(L, -1));
			lua_pop(L, 2);
			return false;
		}

		bool result = !!lua_toboolean(L, -2);

		wxString new_help_string(get_wxstring(L, -1));
		if (new_help_string.size()) {
			help = new_help_string;
			cmd_type |= cmd::COMMAND_DYNAMIC_HELP;
		}

		lua_pop(L, 3); // two return values and error handler

		return result;
	}

	void LuaCommand::operator()(agi::Context *c)
	{
		LuaStackcheck stackcheck(L);
		set_context(L, c);
		stackcheck.check_stack(0);

		GetFeatureFunction("run");
		auto subsobj = new LuaAssFile(L, c->ass.get(), true, true);

		int original_offset = c->ass->Info.size() + c->ass->Styles.size() + 1;
		auto original_sel = selected_rows(c);
		int original_active = 0;
		if (auto active_line = c->selectionController->GetActiveLine())
			original_active = active_line->Row + original_offset;

		push_value(L, original_sel);
		push_value(L, original_active);

		try {
			LuaThreadedCall(L, 3, 2, from_wx(StrDisplay(c)), c->parent, true);
		}
		catch (agi::UserCancelException const&) {
			subsobj->Cancel();
			stackcheck.check_stack(0);
			return;
		}

		auto lines = subsobj->ProcessingComplete(StrDisplay(c));

		AssDialogue *active_line = nullptr;
		int active_idx = original_active;

		// Check for a new active row
		if (lua_isnumber(L, -1)) {
			active_idx = lua_tointeger(L, -1);
			if (active_idx < 1 || active_idx > (int)lines.size()) {
				wxLogError("Active row %d is out of bounds (must be 1-%u)", active_idx, lines.size());
				active_idx = original_active;
			}
		}

		stackcheck.check_stack(2);
		lua_pop(L, 1);

		// top of stack will be selected lines array, if any was returned
		if (lua_istable(L, -1)) {
			std::set<AssDialogue*> sel;
			lua_for_each(L, [&] {
				if (!lua_isnumber(L, -1))
					return;
				int cur = lua_tointeger(L, -1);
				if (cur < 1 || cur > (int)lines.size()) {
					wxLogError("Selected row %d is out of bounds (must be 1-%u)", cur, lines.size());
					throw LuaForEachBreak();
				}

				if (typeid(*lines[cur - 1]) != typeid(AssDialogue)) {
					wxLogError("Selected row %d is not a dialogue line", cur);
					throw LuaForEachBreak();
				}

				auto diag = static_cast<AssDialogue*>(lines[cur - 1]);
				sel.insert(diag);
				if (!active_line || active_idx == cur)
					active_line = diag;
			});

			AssDialogue *new_active = c->selectionController->GetActiveLine();
			if (active_line && (active_idx > 0 || !sel.count(new_active)))
				new_active = active_line;
			if (sel.empty())
				sel.insert(new_active);
			c->selectionController->SetSelectionAndActive(std::move(sel), new_active);
		}
		else {
			lua_pop(L, 1);

			Selection new_sel;
			AssDialogue *new_active = nullptr;

			int prev = original_offset;
			auto it = c->ass->Events.begin();
			for (int row : original_sel) {
				while (row > prev && it != c->ass->Events.end()) {
					++prev;
					++it;
				}
				if (it == c->ass->Events.end()) break;
				new_sel.insert(&*it);
				if (row == original_active)
					new_active = &*it;
			}

			if (new_sel.empty() && !c->ass->Events.empty())
				new_sel.insert(&c->ass->Events.front());
			if (!new_sel.count(new_active))
				new_active = *new_sel.begin();
			c->selectionController->SetSelectionAndActive(std::move(new_sel), new_active);
		}

		stackcheck.check_stack(0);
	}

	bool LuaCommand::IsActive(const agi::Context *c)
	{
		if (!(cmd_type & cmd::COMMAND_TOGGLE)) return false;

		LuaStackcheck stackcheck(L);

		set_context(L, c);
		stackcheck.check_stack(0);

		GetFeatureFunction("isactive");
		auto subsobj = new LuaAssFile(L, c->ass.get());
		push_value(L, selected_rows(c));
		if (auto active_line = c->selectionController->GetActiveLine())
			push_value(L, active_line->Row + c->ass->Info.size() + c->ass->Styles.size() + 1);

		int err = lua_pcall(L, 3, 1, 0);
		subsobj->ProcessingComplete();

		bool result = false;
		if (err)
			wxLogWarning("Runtime error in Lua macro IsActive function:\n%s", get_wxstring(L, -1));
		else
			result = !!lua_toboolean(L, -1);

		// clean up stack (result or error message)
		stackcheck.check_stack(1);
		lua_pop(L, 1);

		return result;
	}

	// LuaFeatureFilter
	LuaExportFilter::LuaExportFilter(lua_State *L)
	: ExportFilter(check_string(L, 1), lua_tostring(L, 2), lua_tointeger(L, 3))
	, LuaFeature(L)
	{
		if (!lua_isfunction(L, 4))
			error(L, "The filter processing function must be a function");

		// new table for containing the functions for this feature
		lua_createtable(L, 0, 2);

		// store processing function
		push_value(L, "run");
		lua_pushvalue(L, 4);
		lua_rawset(L, -3);

		// store config function
		push_value(L, "config");
		lua_pushvalue(L, 5);
		has_config = lua_isfunction(L, -1);
		lua_rawset(L, -3);

		// store the table in the registry
		RegisterFeature();

		LuaScript::GetScriptObject(L)->RegisterFilter(this);
	}

	int LuaExportFilter::LuaRegister(lua_State *L)
	{
		static std::mutex mutex;
		auto filter = agi::make_unique<LuaExportFilter>(L);
		{
			std::lock_guard<std::mutex> lock(mutex);
			AssExportFilterChain::Register(std::move(filter));
		}
		return 0;
	}

	void LuaExportFilter::ProcessSubs(AssFile *subs, wxWindow *export_dialog)
	{
		LuaStackcheck stackcheck(L);

		GetFeatureFunction("run");
		stackcheck.check_stack(1);

		// The entire point of an export filter is to modify the file, but
		// setting undo points makes no sense
		auto subsobj = new LuaAssFile(L, subs, true);
		assert(lua_isuserdata(L, -1));
		stackcheck.check_stack(2);

		// config
		if (has_config && config_dialog) {
			int results_produced = config_dialog->LuaReadBack(L);
			assert(results_produced == 1);
			(void) results_produced;	// avoid warning on release builds
			// TODO, write back stored options here
		} else {
			// no config so put an empty table instead
			lua_newtable(L);
		}
		assert(lua_istable(L, -1));
		stackcheck.check_stack(3);

		try {
			LuaThreadedCall(L, 2, 0, GetName(), export_dialog, false);
			stackcheck.check_stack(0);
			subsobj->ProcessingComplete();
		}
		catch (agi::UserCancelException const&) {
			subsobj->Cancel();
			throw;
		}
	}

	std::unique_ptr<ScriptDialog> LuaExportFilter::GenerateConfigDialog(wxWindow *parent, agi::Context *c)
	{
		if (!has_config)
			return nullptr;

		set_context(L, c);

		GetFeatureFunction("config");

		// prepare function call
		auto subsobj = new LuaAssFile(L, c->ass.get());
		// stored options
		lua_newtable(L); // TODO, nothing for now

		// do call
		int err = lua_pcall(L, 2, 1, 0);
		subsobj->ProcessingComplete();

		if (err) {
			wxLogWarning("Runtime error in Lua config dialog function:\n%s", get_wxstring(L, -1));
			lua_pop(L, 1); // remove error message
		} else {
			// Create config dialogue from table on top of stack
			config_dialog = new LuaDialog(L, false);
		}

		return std::unique_ptr<ScriptDialog>{config_dialog};
	}
}

namespace Automation4 {
	LuaScriptFactory::LuaScriptFactory()
	: ScriptFactory("Lua", "*.lua,*.moon")
	{
	}

	std::unique_ptr<Script> LuaScriptFactory::Produce(agi::fs::path const& filename) const
	{
		if (agi::fs::HasExtension(filename, "lua") || agi::fs::HasExtension(filename, "moon"))
			return agi::make_unique<LuaScript>(filename);
		return nullptr;
	}
}
