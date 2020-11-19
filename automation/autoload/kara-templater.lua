--[[
 Copyright (c) 2007, Niels Martin Hansen, Rodrigo Braz Monteiro
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

   * Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   * Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   * Neither the name of the Aegisub Group nor the names of its contributors
     may be used to endorse or promote products derived from this software
     without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
]]

-- Aegisub Automation 4 Lua karaoke templater tool
-- Parse and apply a karaoke effect written in ASS karaoke template language
-- See help file and wiki for more information on this

local tr = aegisub.gettext

script_name = tr"Karaoke Templater"
script_description = tr"Macro and export filter to apply karaoke effects using the template language"
script_author = "Niels Martin Hansen"
script_version = "2.1.7"


include("karaskel.lua")


-- Find and parse/prepare all karaoke template lines
function parse_templates(meta, styles, subs)
	local templates = { once = {}, line = {}, syl = {}, char = {}, furi = {}, styles = {} }
	local i = 1
	while i <= #subs do
		aegisub.progress.set((i-1) / #subs * 100)
		local l = subs[i]
		i = i + 1
		if l.class == "dialogue" and l.comment then
			local fx, mods = string.headtail(l.effect)
			fx = fx:lower()
			if fx == "code" then
				parse_code(meta, styles, l, templates, mods)
			elseif fx == "template" then
				parse_template(meta, styles, l, templates, mods)
			end
			templates.styles[l.style] = true
		elseif l.class == "dialogue" and l.effect == "fx" then
			-- this is a previously generated effect line, remove it
			i = i - 1
			subs.delete(i)
		end
	end
	aegisub.progress.set(100)
	return templates
end

function parse_code(meta, styles, line, templates, mods)
	local template = {
		code = line.text,
		loops = 1,
		style = line.style
	}
	local inserted = false

	local rest = mods
	while rest ~= "" do
		local m, t = string.headtail(rest)
		rest = t
		m = m:lower()
		if m == "once" then
			aegisub.debug.out(5, "Found run-once code line: %s\n", line.text)
			table.insert(templates.once, template)
			inserted = true
		elseif m == "line" then
			aegisub.debug.out(5, "Found per-line code line: %s\n", line.text)
			table.insert(templates.line, template)
			inserted = true
		elseif m == "syl" then
			aegisub.debug.out(5, "Found per-syl code line: %s\n", line.text)
			table.insert(templates.syl, template)
			inserted = true
		elseif m == "furi" then
			aegisub.debug.out(5, "Found per-syl code line: %s\n", line.text)
			table.insert(templates.furi, template)
			inserted = true
		elseif m == "all" then
			template.style = nil
		elseif m == "noblank" then
			template.noblank = true
		elseif m == "repeat" or m == "loop" then
			local times, t = string.headtail(rest)
			template.loops = tonumber(times)
			if not template.loops then
				aegisub.debug.out(3, "Failed reading this repeat-count to a number: %s\nIn template code line: %s\nEffect field: %s\n\n", times, line.text, line.effect)
				template.loops = 1
			else
				rest = t
			end
		else
			aegisub.debug.out(3, "Unknown modifier in code template: %s\nIn template code line: %s\nEffect field: %s\n\n", m, line.text, line.effect)
		end
	end

	if not inserted then
		aegisub.debug.out(5, "Found implicit run-once code line: %s\n", line.text)
		table.insert(templates.once, template)
	end
end

-- List of reserved words that can't be used as "line" template identifiers
template_modifiers = {
	"pre-line", "line", "syl", "furi", "char", "all", "repeat", "loop",
	"notext", "keeptags", "noblank", "multi", "fx", "fxgroup"
}

function parse_template(meta, styles, line, templates, mods)
	local template = {
		t = "",
		pre = "",
		style = line.style,
		loops = 1,
		layer = line.layer,
		addtext = true,
		keeptags = false,
		fxgroup = nil,
		fx = nil,
		multi = false,
		isline = false,
		perchar = false,
		noblank = false
	}
	local inserted = false

	local rest = mods
	while rest ~= "" do
		local m, t = string.headtail(rest)
		rest = t
		m = m:lower()
		if (m == "pre-line" or m == "line") and not inserted then
			aegisub.debug.out(5, "Found line template '%s'\n", line.text)
			-- should really fail if already inserted
			local id, t = string.headtail(rest)
			id = id:lower()
			-- check that it really is an identifier and not a keyword
			for _, kw in pairs(template_modifiers) do
				if id == kw then
					id = nil
					break
				end
			end
			if id == "" then
				id = nil
			end
			if id then
				rest = t
			end
			-- get old template if there is one
			if id and templates.line[id] then
				template = templates.line[id]
			elseif id then
				template.id = id
				templates.line[id] = template
			else
				table.insert(templates.line, template)
			end
			inserted = true
			template.isline = true
			-- apply text to correct string
			if m == "line" then
				template.t = template.t .. line.text
			else -- must be pre-line
				template.pre = template.pre .. line.text
			end
		elseif m == "syl" and not template.isline then
			table.insert(templates.syl, template)
			inserted = true
		elseif m == "furi" and not template.isline then
			table.insert(templates.furi, template)
			inserted = true
		elseif (m == "pre-line" or m == "line") and inserted then
			aegisub.debug.out(2, "Unable to combine %s class templates with other template classes\n\n", m)
		elseif (m == "syl" or m == "furi") and template.isline then
			aegisub.debug.out(2, "Unable to combine %s class template lines with line or pre-line classes\n\n", m)
		elseif m == "all" then
			template.style = nil
		elseif m == "repeat" or m == "loop" then
			local times, t = string.headtail(rest)
			template.loops = tonumber(times)
			if not template.loops then
				aegisub.debug.out(3, "Failed reading this repeat-count to a number: %s\nIn template line: %s\nEffect field: %s\n\n", times, line.text, line.effect)
				template.loops = 1
			else
				rest = t
			end
		elseif m == "notext" then
			template.addtext = false
		elseif m == "keeptags" then
			template.keeptags = true
		elseif m == "multi" then
			template.multi = true
		elseif m == "char" then
			template.perchar = true
		elseif m == "noblank" then
			template.noblank = true
		elseif m == "fx" then
			local fx, t = string.headtail(rest)
			if fx ~= "" then
				template.fx = fx
				rest = t
			else
				aegisub.debug.out(3, "No fx name following fx modifier\nIn template line: %s\nEffect field: %s\n\n", line.text, line.effect)
				template.fx = nil
			end
		elseif m == "fxgroup" then
			local fx, t = string.headtail(rest)
			if fx ~= "" then
				template.fxgroup = fx
				rest = t
			else
				aegisub.debug.out(3, "No fxgroup name following fxgroup modifier\nIn template linee: %s\nEffect field: %s\n\n", line.text, line.effect)
				template.fxgroup = nil
			end
		else
			aegisub.debug.out(3, "Unknown modifier in template: %s\nIn template line: %s\nEffect field: %s\n\n", m, line.text, line.effect)
		end
	end

	if not inserted then
		table.insert(templates.syl, template)
	end
	if not template.isline then
		template.t = line.text
	end
end

-- Iterator function, return all templates that apply to the given line
function matching_templates(templates, line, tenv)
	local lastkey = nil
	local function test_next()
		local k, t = next(templates, lastkey)
		lastkey = k
		if not t then
			return nil
		elseif (t.style == line.style or not t.style) and
				(not t.fxgroup or
				(t.fxgroup and tenv.fxgroup[t.fxgroup] ~= false)) then
			return t
		else
			return test_next()
		end
	end
	return test_next
end

-- Iterator function, run a loop using tenv.j and tenv.maxj as loop controllers
function template_loop(tenv, initmaxj)
	local oldmaxj = initmaxj
	tenv.maxj = initmaxj
	tenv.j = 0
	local function itor()
		if tenv.j >= tenv.maxj or aegisub.progress.is_cancelled() then
			return nil
		else
			tenv.j = tenv.j + 1
			if oldmaxj ~= tenv.maxj then
				aegisub.debug.out(5, "Number of loop iterations changed from %d to %d\n", oldmaxj, tenv.maxj)
				oldmaxj = tenv.maxj
			end
			return tenv.j, tenv.maxj
		end
	end
	return itor
end


-- Apply the templates
function apply_templates(meta, styles, subs, templates)
	-- the environment the templates will run in
	local tenv = {
		meta = meta,
		-- put in some standard libs
		string = string,
		math = math,
		_G = _G
	}
	tenv.tenv = tenv

	-- Define helper functions in tenv

	tenv.retime = function(mode, addstart, addend)
		local line, syl = tenv.line, tenv.syl
		local newstart, newend = line.start_time, line.end_time
		addstart = addstart or 0
		addend = addend or 0
		if mode == "syl" then
			newstart = line.start_time + syl.start_time + addstart
			newend = line.start_time + syl.end_time + addend
		elseif mode == "presyl" then
			newstart = line.start_time + syl.start_time + addstart
			newend = line.start_time + syl.start_time + addend
		elseif mode == "postsyl" then
			newstart = line.start_time + syl.end_time + addstart
			newend = line.start_time + syl.end_time + addend
		elseif mode == "line" then
			newstart = line.start_time + addstart
			newend = line.end_time + addend
		elseif mode == "preline" then
			newstart = line.start_time + addstart
			newend = line.start_time + addend
		elseif mode == "postline" then
			newstart = line.end_time + addstart
			newend = line.end_time + addend
		elseif mode == "start2syl" then
			newstart = line.start_time + addstart
			newend = line.start_time + syl.start_time + addend
		elseif mode == "syl2end" then
			newstart = line.start_time + syl.end_time + addstart
			newend = line.end_time + addend
		elseif mode == "set" or mode == "abs" then
			newstart = addstart
			newend = addend
		elseif mode == "sylpct" then
			newstart = line.start_time + syl.start_time + addstart*syl.duration/100
			newend = line.start_time + syl.start_time + addend*syl.duration/100
		-- wishlist: something for fade-over effects,
		-- "time between previous line and this" and
		-- "time between this line and next"
		end
		line.start_time = newstart
		line.end_time = newend
		line.duration = newend - newstart
		return ""
	end

	tenv.fxgroup = {}

	tenv.relayer = function(layer)
		tenv.line.layer = layer
		return ""
	end

	tenv.restyle = function(style)
		tenv.line.style = style
		tenv.line.styleref = styles[style]
		return ""
	end

	tenv.maxloop = function(newmaxj)
		tenv.maxj = newmaxj
		return ""
	end
	tenv.maxloops = tenv.maxloop
	tenv.loopctl = function(newj, newmaxj)
		tenv.j = newj
		tenv.maxj = newmaxj
		return ""
	end

	tenv.recall = {}
	setmetatable(tenv.recall, {
		decorators = {},
		__call = function(tab, name, default)
			local decorator = getmetatable(tab).decorators[name]
			if decorator then
				name = decorator(tostring(name))
			end
			aegisub.debug.out(5, "Recalling '%s'\n", name)
			return tab[name] or default
		end,
		decorator_line = function(name)
			return string.format("_%s_%s", tostring(tenv.orgline), name)
		end,
		decorator_syl = function(name)
			return string.format("_%s_%s", tostring(tenv.syl), name)
		end,
		decorator_basesyl = function(name)
			return string.format("_%s_%s", tostring(tenv.basesyl), name)
		end
	})
	tenv.remember = function(name, value, decorator)
		getmetatable(tenv.recall).decorators[name] = decorator
		if decorator then
			name = decorator(tostring(name))
		end
		aegisub.debug.out(5, "Remembering '%s' as '%s'\n", name, tostring(value))
		tenv.recall[name] = value
		return value
	end
	tenv.remember_line = function(name, value)
		return tenv.remember(name, value, getmetatable(tenv.recall).decorator_line)
	end
	tenv.remember_syl = function(name, value)
		return tenv.remember(name, value, getmetatable(tenv.recall).decorator_syl)
	end
	tenv.remember_basesyl = function(name, value)
		return tenv.remember(name, value, getmetatable(tenv.recall).decorator_basesyl)
	end
	tenv.remember_if = function(name, value, condition, decorator)
		if condition then
			return tenv.remember(name, value, decorator)
		end
		return value
	end

	-- run all run-once code snippets
	for k, t in pairs(templates.once) do
		assert(t.code, "WTF, a 'once' template without code?")
		run_code_template(t, tenv)
	end

	-- start processing lines
	local i, n = 0, #subs
	while i < n do
		aegisub.progress.set(i/n*100)
		i = i + 1
		local l = subs[i]
		if l.class == "dialogue" and ((l.effect == "" and not l.comment) or l.effect:match("[Kk]araoke")) then
			l.i = i
			l.comment = false
			karaskel.preproc_line(subs, meta, styles, l)
			if apply_line(meta, styles, subs, l, templates, tenv) then
				-- Some templates were applied to this line, make a karaoke timing line of it
				l.comment = true
				l.effect = "karaoke"
				subs[i] = l
			end
		end
	end
end

function set_ctx_syl(varctx, line, syl)
	varctx.sstart = syl.start_time
	varctx.send = syl.end_time
	varctx.sdur = syl.duration
	varctx.skdur = syl.duration / 10
	varctx.smid = syl.start_time + syl.duration / 2
	varctx["start"] = varctx.sstart
	varctx["end"] = varctx.send
	varctx.dur = varctx.sdur
	varctx.kdur = varctx.skdur
	varctx.mid = varctx.smid
	varctx.si = syl.i
	varctx.i = varctx.si
	local slack = syl.slack * ({left = 0, center = 0.5, right = 1})[line.halign]
	varctx.sleft = math.floor(line.left + slack + syl.left + 0.5)
	varctx.scenter = math.floor(line.left + slack + syl.center + 0.5)
	varctx.sright = math.floor(line.left + slack + syl.right + 0.5)
	varctx.swidth = math.floor(syl.width + 0.5)
	varctx.stop = math.floor(line.top + syl.top + 0.5)
	varctx.smiddle = math.floor(line.top + syl.middle + 0.5)
	varctx.sbottom = math.floor(line.top + syl.bottom + 0.5)
	varctx.sheight = math.floor(syl.height + 0.5)
	if line.halign == "left" then
		varctx.sx = varctx.sleft
	elseif line.halign == "center" then
		varctx.sx = varctx.scenter
	elseif line.halign == "right" then
		varctx.sx = varctx.sright
	end
	if line.valign == "top" then
		varctx.sy = varctx.stop
	elseif line.valign == "middle" then
		varctx.sy = varctx.smiddle
	elseif line.valign == "bottom" then
		varctx.sy = varctx.sbottom
	end
	varctx.left = varctx.sleft
	varctx.center = varctx.scenter
	varctx.right = varctx.sright
	varctx.width = varctx.swidth
	varctx.top = varctx.stop
	varctx.middle = varctx.smiddle
	varctx.bottom = varctx.sbottom
	varctx.height = varctx.sheight
	varctx.x = varctx.sx
	varctx.y = varctx.sy
end

function apply_line(meta, styles, subs, line, templates, tenv)
	-- Tell whether any templates were applied to this line, needed to know whether the original line should be removed from input
	local applied_templates = false

	-- General variable replacement context
	local varctx = {
		layer = line.layer,
		lstart = line.start_time,
		lend = line.end_time,
		ldur = line.duration,
		lmid = line.start_time + line.duration/2,
		style = line.style,
		actor = line.actor,
		margin_l = ((line.margin_l > 0) and line.margin_l) or line.styleref.margin_l,
		margin_r = ((line.margin_r > 0) and line.margin_r) or line.styleref.margin_r,
		margin_t = ((line.margin_t > 0) and line.margin_t) or line.styleref.margin_t,
		margin_b = ((line.margin_b > 0) and line.margin_b) or line.styleref.margin_b,
		margin_v = ((line.margin_t > 0) and line.margin_t) or line.styleref.margin_t,
		syln = line.kara.n,
		li = line.i,
		lleft = math.floor(line.left+0.5),
		lcenter = math.floor(line.left + line.width/2 + 0.5),
		lright = math.floor(line.left + line.width + 0.5),
		lwidth = math.floor(line.width + 0.5),
		ltop = math.floor(line.top + 0.5),
		lmiddle = math.floor(line.middle + 0.5),
		lbottom = math.floor(line.bottom + 0.5),
		lheight = math.floor(line.height + 0.5),
		lx = math.floor(line.x+0.5),
		ly = math.floor(line.y+0.5)
	}

	tenv.orgline = line
	tenv.line = nil
	tenv.syl = nil
	tenv.basesyl = nil

	-- Apply all line templates
	aegisub.debug.out(5, "Running line templates\n")
	for t in matching_templates(templates.line, line, tenv) do
		if aegisub.progress.is_cancelled() then break end

		-- Set varctx for per-line variables
		varctx["start"] = varctx.lstart
		varctx["end"] = varctx.lend
		varctx.dur = varctx.ldur
		varctx.kdur = math.floor(varctx.dur / 10)
		varctx.mid = varctx.lmid
		varctx.i = varctx.li
		varctx.left = varctx.lleft
		varctx.center = varctx.lcenter
		varctx.right = varctx.lright
		varctx.width = varctx.lwidth
		varctx.top = varctx.ltop
		varctx.middle = varctx.lmiddle
		varctx.bottom = varctx.lbottom
		varctx.height = varctx.lheight
		varctx.x = varctx.lx
		varctx.y = varctx.ly

		for j, maxj in template_loop(tenv, t.loops) do
			if t.code then
				aegisub.debug.out(5, "Code template, %s\n", t.code)
				tenv.line = line
				-- Although run_code_template also performs template looping this works
				-- by "luck", since by the time the first loop of this outer loop completes
				-- the one run by run_code_template has already performed all iterations
				-- and has tenv.j and tenv.maxj in a loop-ending state, causing the outer
				-- loop to only ever run once.
				run_code_template(t, tenv)
			else
				aegisub.debug.out(5, "Line template, pre = '%s', t = '%s'\n", t.pre, t.t)
				applied_templates = true
				local newline = table.copy(line)
				tenv.line = newline
				newline.layer = t.layer
				newline.text = ""
				if t.pre ~= "" then
					newline.text = newline.text .. run_text_template(t.pre, tenv, varctx)
				end
				if t.t ~= "" then
					for i = 1, line.kara.n do
						local syl = line.kara[i]
						tenv.syl = syl
						tenv.basesyl = syl
						set_ctx_syl(varctx, line, syl)
						newline.text = newline.text .. run_text_template(t.t, tenv, varctx)
						if t.addtext then
							if t.keeptags then
								newline.text = newline.text .. syl.text
							else
								newline.text = newline.text .. syl.text_stripped
							end
						end
					end
				else
					-- hmm, no main template for the line... put original text in
					if t.keeptags then
						newline.text = newline.text .. line.text
					else
						newline.text = newline.text .. line.text_stripped
					end
				end
				newline.effect = "fx"
				subs.append(newline)
			end
		end
	end
	aegisub.debug.out(5, "Done running line templates\n\n")

	-- Loop over syllables
	for i = 0, line.kara.n do
		if aegisub.progress.is_cancelled() then break end
		local syl = line.kara[i]

		aegisub.debug.out(5, "Applying templates to syllable: %s\n", syl.text)
		if apply_syllable_templates(syl, line, templates.syl, tenv, varctx, subs) then
			applied_templates = true
		end
	end

	-- Loop over furigana
	for i = 1, line.furi.n do
		if aegisub.progress.is_cancelled() then break end
		local furi = line.furi[i]

		aegisub.debug.out(5, "Applying templates to furigana: %s\n", furi.text)
		if apply_syllable_templates(furi, line, templates.furi, tenv, varctx, subs) then
			applied_templates = true
		end
	end

	return applied_templates
end

function run_code_template(template, tenv)
	local f, err = loadstring(template.code, "template code")
	if not f then
		aegisub.debug.out(2, "Failed to parse Lua code: %s\nCode that failed to parse: %s\n\n", err, template.code)
		aegisub.cancel()
	else
		local pcall = pcall
		setfenv(f, tenv)
		for j, maxj in template_loop(tenv, template.loops) do
			local res, err = pcall(f)
			if not res then
				aegisub.debug.out(2, "Runtime error in template code: %s\nCode producing error: %s\n\n", err, template.code)
				aegisub.cancel()
			end
		end
	end
end

function run_text_template(template, tenv, varctx)
	local res = template
	aegisub.debug.out(5, "Running text template '%s'\n", res)

	-- Replace the variables in the string (this is probably faster than using a custom function, but doesn't provide error reporting)
	if varctx then
		aegisub.debug.out(5, "Has varctx, replacing variables\n")
		local function var_replacer(varname)
			varname = string.lower(varname)
			aegisub.debug.out(5, "Found variable named '%s', ", varname)
			if varctx[varname] ~= nil then
				aegisub.debug.out(5, "it exists, value is '%s'\n", varctx[varname])
				return varctx[varname]
			else
				aegisub.debug.out(5, "doesn't exist\n")
				aegisub.debug.out(2, "Unknown variable name: %s\nIn karaoke template: %s\n\n", varname, template)
				return "$" .. varname
			end
		end
		res = string.gsub(res, "$([%a_]+)", var_replacer)
		aegisub.debug.out(5, "Done replacing variables, new template string is '%s'\n", res)
	end

	-- Function for evaluating expressions
	local function expression_evaluator(expression)
		f, err = loadstring(string.format("return (%s)", expression))
		if (err) ~= nil then
			aegisub.debug.out(2, "Error parsing expression: %s\nExpression producing error: %s\nTemplate with expression: %s\n\n", err, expression, template)
			aegisub.cancel()
		else
			setfenv(f, tenv)
			local res, val = pcall(f)
			if res then
				return val
			else
				aegisub.debug.out(2, "Runtime error in template expression: %s\nExpression producing error: %s\nTemplate with expression: %s\n\n", val, expression, template)
				aegisub.cancel()
			end
		end
	end
	-- Find and evaluate expressions
	aegisub.debug.out(5, "Now evaluating expressions\n")
	res = string.gsub(res , "!(.-)!", expression_evaluator)
	aegisub.debug.out(5, "After evaluation: %s\nDone handling template\n\n", res)

	return res
end

function apply_syllable_templates(syl, line, templates, tenv, varctx, subs)
	local applied = 0

	-- Loop over all templates matching the line style
	for t in matching_templates(templates, line, tenv) do
		if aegisub.progress.is_cancelled() then break end

		tenv.syl = syl
		tenv.basesyl = syl
		set_ctx_syl(varctx, line, syl)

		applied = applied + apply_one_syllable_template(syl, line, t, tenv, varctx, subs, false, false)
	end

	return applied > 0
end

function is_syl_blank(syl)
	if syl.duration <= 0 then
		return true
	end

	-- try to remove common spacing characters
	local t = syl.text_stripped
	if t:len() <= 0 then return true end
	t = t:gsub("[ \t\n\r]", "") -- regular ASCII space characters
	t = t:gsub("　", "") -- fullwidth space
	return t:len() <= 0
end

function apply_one_syllable_template(syl, line, template, tenv, varctx, subs, skip_perchar, skip_multi)
	if aegisub.progress.is_cancelled() then return 0 end
	local t = template
	local applied = 0

	aegisub.debug.out(5, "Applying template to one syllable with text: %s\n", syl.text)

	-- Check for right inline_fx
	if t.fx and t.fx ~= syl.inline_fx then
		aegisub.debug.out(5, "Syllable has wrong inline-fx (wanted '%s', got '%s'), skipping.\n", t.fx, syl.inline_fx)
		return 0
	end

	if t.noblank and is_syl_blank(syl) then
		aegisub.debug.out(5, "Syllable is blank, skipping.\n")
		return 0
	end

	-- Recurse to per-char if required
	if not skip_perchar and t.perchar then
		aegisub.debug.out(5, "Doing per-character effects...\n")
		local charsyl = table.copy(syl)
		tenv.syl = charsyl

		local left, width = syl.left, 0
		for c in unicode.chars(syl.text_stripped) do
			charsyl.text = c
			charsyl.text_stripped = c
			charsyl.text_spacestripped = c
			charsyl.prespace, charsyl.postspace = "", "" -- for whatever anyone might use these for
			width = aegisub.text_extents(syl.style, c)
			charsyl.left = left
			charsyl.center = left + width/2
			charsyl.right = left + width
			charsyl.prespacewidth, charsyl.postspacewidth = 0, 0 -- whatever...
			left = left + width
			set_ctx_syl(varctx, line, charsyl)

			applied = applied + apply_one_syllable_template(charsyl, line, t, tenv, varctx, subs, true, false)
		end

		return applied
	end

	-- Recurse to multi-hl if required
	if not skip_multi and t.multi then
		aegisub.debug.out(5, "Doing multi-highlight effects...\n")
		local hlsyl = table.copy(syl)
		tenv.syl = hlsyl

		for hl = 1, syl.highlights.n do
			local hldata = syl.highlights[hl]
			hlsyl.start_time = hldata.start_time
			hlsyl.end_time = hldata.end_time
			hlsyl.duration = hldata.duration
			set_ctx_syl(varctx, line, hlsyl)

			applied = applied + apply_one_syllable_template(hlsyl, line, t, tenv, varctx, subs, true, true)
		end

		return applied
	end

	-- Regular processing
	if t.code then
		aegisub.debug.out(5, "Running code line\n")
		tenv.line = line
		run_code_template(t, tenv)
	else
		aegisub.debug.out(5, "Running %d effect loops\n", t.loops)
		for j, maxj in template_loop(tenv, t.loops) do
			local newline = table.copy(line)
			newline.styleref = syl.style
			newline.style = syl.style.name
			newline.layer = t.layer
			tenv.line = newline
			newline.text = run_text_template(t.t, tenv, varctx)
			if t.keeptags then
				newline.text = newline.text .. syl.text
			elseif t.addtext then
				newline.text = newline.text .. syl.text_stripped
			end
			newline.effect = "fx"
			aegisub.debug.out(5, "Generated line with text: %s\n", newline.text)
			subs.append(newline)
			applied = applied + 1
		end
	end

	return applied
end


-- Main function to do the templating
function filter_apply_templates(subs, config)
	aegisub.progress.task("Collecting header data...")
	local meta, styles = karaskel.collect_head(subs, true)

	aegisub.progress.task("Parsing templates...")
	local templates = parse_templates(meta, styles, subs)

	aegisub.progress.task("Applying templates...")
	apply_templates(meta, styles, subs, templates)
end

function macro_apply_templates(subs, sel)
	filter_apply_templates(subs, {ismacro=true, sel=sel})
	aegisub.set_undo_point("apply karaoke template")
end

function macro_can_template(subs)
	-- check if this file has templates in it, don't allow running the macro if it hasn't
	local num_dia = 0
	for i = 1, #subs do
		local l = subs[i]
		if l.class == "dialogue" then
			num_dia = num_dia + 1
			-- test if the line is a template
			if (string.headtail(l.effect)):lower() == "template" then
				return true
			end
			-- don't try forever, this has to be fast
			if num_dia > 50 then
				return false
			end
		end
	end
	return false
end

aegisub.register_macro(tr"Apply karaoke template", tr"Applies karaoke effects from templates", macro_apply_templates, macro_can_template)
aegisub.register_filter(tr"Karaoke template", tr"Apply karaoke effect templates to the subtitles.\n\nSee the help file for information on how to use this.", 2000, filter_apply_templates)
