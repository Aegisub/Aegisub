--[[
 Copyright (c) 2007, 2010, Niels Martin Hansen, Rodrigo Braz Monteiro
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

include("utils.lua")
include("unicode.lua")

-- Make sure karaskel table exists
if not karaskel then
	karaskel = {}
end

-- Collect styles and metadata from the subs
function karaskel.collect_head(subs, generate_furigana)
	local meta = {
		-- X and Y script resolution
		res_x = 0, res_y = 0,
		-- Aspect ratio correction factor for video/script resolution mismatch
		video_x_correct_factor = 1.0
	}
	local styles = { n = 0 }
	local toinsert = {}
	local first_style_line = nil
	
	if not karaskel.furigana_scale then
		karaskel.furigana_scale = 0.5
	end
	
	-- First pass: collect all existing styles and get resolution info
	for i = 1, #subs do
		if aegisub.progress.is_cancelled() then error("User cancelled") end
		local l = subs[i]
		
		if l.class == "style" then
			if not first_style_line then first_style_line = i end
			-- Store styles into the style table
			styles.n = styles.n + 1
			styles[styles.n] = l
			styles[l.name] = l
			l.margin_v = l.margin_t -- convenience
			
			-- And also generate furigana styles if wanted
			if generate_furigana and not l.name:match("furigana") then
				aegisub.debug.out(5, "Creating furigana style for style: " .. l.name .. "\n")
				local fs = table.copy(l)
				fs.fontsize = l.fontsize * karaskel.furigana_scale
				fs.outline = l.outline * karaskel.furigana_scale
				fs.shadow = l.shadow * karaskel.furigana_scale
				fs.name = l.name .. "-furigana"
				
				table.insert(toinsert, fs) -- queue to insert in file
			end
			
		elseif l.class == "info" then
			-- Also look for script resolution
			local k = l.key:lower()
			meta[k] = l.value
		end
	end
	
	-- Second pass: insert all toinsert styles that don't already exist
	for i = 1, #toinsert do
		if not styles[toinsert[i].name] then
			-- Insert into styles table
			styles.n = styles.n + 1
			styles[styles.n] = toinsert[i]
			styles[toinsert[i].name] = toinsert[i]
			-- And subtitle file
			subs[-first_style_line] = toinsert[i]
		end
	end
	
	-- Fix resolution data
	if meta.playresx then
		meta.res_x = math.floor(meta.playresx)
	end
	if meta.playresy then
		meta.res_y = math.floor(meta.playresy)
	end
	if meta.res_x == 0 and meta_res_y == 0 then
		meta.res_x = 384
		meta.res_y = 288
	elseif meta.res_x == 0 then
		-- This is braindead, but it's how TextSub does things...
		if meta.res_y == 1024 then
			meta.res_x = 1280
		else
			meta.res_x = meta.res_y / 3 * 4
		end
	elseif meta.res_y == 0 then
		-- As if 1280x960 didn't exist
		if meta.res_x == 1280 then
			meta.res_y = 1024
		else
			meta.res_y = meta.res_x * 3 / 4
		end
	end
	
	local video_x, video_y = aegisub.video_size()
	if video_y then
		-- Correction factor for TextSub weirdness when render resolution does
		-- not match script resolution. Text pixels are considered square in
		-- render resolution rather than in script resolution, which is
		-- logically inconsistent. Correct for that.
		meta.video_x_correct_factor =
			(video_y / video_x) / (meta.res_y / meta.res_x)
	end
	aegisub.debug.out(4, "Karaskel: Video X correction factor = %f\n\n", meta.video_x_correct_factor)
	
	return meta, styles
end


-- Pre-process line, determining stripped text, karaoke data and splitting off furigana data
-- Modifies the object passed for line
function karaskel.preproc_line_text(meta, styles, line)
	-- Assume line is class=dialogue
	local kara = aegisub.parse_karaoke_data(line)
	line.kara = { n = 0 }
	line.furi = { n = 0 }
	
	line.text_stripped = ""
	line.duration = line.end_time - line.start_time
	
	local worksyl = { highlights = {n=0}, furi = {n=0} }
	local cur_inline_fx = ""
	for i = 0, #kara do
		local syl = kara[i]
		
		-- Detect any inline-fx tags
		local inline_fx = syl.text:match("%{.*\\%-([^}\\]+)")
		if inline_fx then
			cur_inline_fx = inline_fx
		end
		
		-- Strip spaces (only basic ones, no fullwidth etc.)
		local prespace, syltext, postspace = syl.text_stripped:match("^([ \t]*)(.-)([ \t]*)$")
		
		-- See if we've broken a (possible) multi-hl stretch
		-- If we did it's time for a new worksyl (though never for the zero'th syllable)
		local prefix = syltext:sub(1,unicode.charwidth(syltext,1))
		if prefix ~= "#" and prefix ~= "＃" and i > 0 then
			line.kara[line.kara.n] = worksyl
			line.kara.n = line.kara.n + 1
			worksyl = { highlights = {n=0}, furi = {n=0} }
		end

		-- Add highlight data
		local hl = {
			start_time = syl.start_time,
			end_time = syl.end_time,
			duration = syl.duration
		}
		worksyl.highlights.n = worksyl.highlights.n + 1
		worksyl.highlights[worksyl.highlights.n] = hl
		
		-- Detect furigana (both regular and fullwidth pipes work)
		-- Furigana is stored independantly from syllables
		if syltext:find("|") or syltext:find("｜") then
			-- Replace fullwidth pipes, they aren't regex friendly
			syltext = syltext:gsub("｜", "|")
			-- Get before/after pipe text
			local maintext, furitext = syltext:match("^(.-)|(.-)$")
			syltext = maintext
			
			local furi = { }
			furi.syl = worksyl
			
			-- Magic happens here
			-- isbreak = Don't join this furi visually with previous furi, even if their main texts are adjacent
			-- spillback = Allow this furi text to spill over the left edge of the main text
			-- (Furi is always allowed to spill over the right edge of main text.)
			local prefix = furitext:sub(1,unicode.charwidth(furitext,1))
			if prefix == "!" or prefix == "！" then
				furi.isbreak = true
				furi.spillback = false
			elseif prefix == "<" or prefix == "＜" then
				furi.isbreak = true
				furi.spillback = true
			else
				furi.isbreak = false
				furi.spillback = false
			end
			-- Remove the prefix character from furitext, if there was one
			if furi.isbreak then
				furitext = furitext:sub(unicode.charwidth(furitext,1)+1)
			end
			
			-- Some of these may seem superflous, but a furi should ideally have the same "interface" as a syllable
			furi.start_time = syl.start_time
			furi.end_time = syl.end_time
			furi.duration = syl.duration
			furi.kdur = syl.duration / 10
			furi.text = furitext
			furi.text_stripped = furitext
			furi.text_spacestripped = furitext
			furi.line = line
			furi.tag = syl.tag
			furi.inline_fx = cur_inline_fx
			furi.i = line.kara.n
			furi.prespace = ""
			furi.postspace = ""
			furi.highlights = { n=1, [1]=hl }
			furi.isfuri = true
			
			line.furi.n = line.furi.n + 1
			line.furi[line.furi.n] = furi
			worksyl.furi.n = worksyl.furi.n + 1
			worksyl.furi[worksyl.furi.n] = furi
		end
		
		-- Syllables that aren't part of a multi-highlight generate a new output-syllable
		if not worksyl.text or (prefix ~= "#" and prefix ~= "＃") then
			-- Update stripped line-text
			line.text_stripped = line.text_stripped .. prespace .. syltext .. postspace
			
			-- Copy data from syl to worksyl
			worksyl.text = syl.text
			worksyl.duration = syl.duration
			worksyl.kdur = syl.duration / 10
			worksyl.start_time = syl.start_time
			worksyl.end_time = syl.end_time
			worksyl.tag = syl.tag
			worksyl.line = line
			
			-- And add new data to worksyl
			worksyl.i = line.kara.n
			worksyl.text_stripped = prespace .. syltext .. postspace -- be sure to include the spaces so the original line can be built from text_stripped
			worksyl.inline_fx = cur_inline_fx
			worksyl.text_spacestripped = syltext
			worksyl.prespace = prespace
			worksyl.postspace = postspace
		else
			-- This is just an extra highlight
			worksyl.duration = worksyl.duration + syl.duration
			worksyl.kdur = worksyl.kdur + syl.duration / 10
			worksyl.end_time = syl.end_time
		end
	end

	-- Add the last syllable
	line.kara[line.kara.n] = worksyl
	-- But don't increment n here, n should be the highest syllable index! (The zero'th syllable doesn't count.)
end


-- Pre-calculate sizing information for the given line, no layouting is done
-- Modifies the object passed for line
function karaskel.preproc_line_size(meta, styles, line)
	if not line.kara then
		karaskel.preproc_line_text(meta, styles, line)
	end
	
	-- Add style information
	if styles[line.style] then
		line.styleref = styles[line.style]
	else
		aegisub.debug.out(2, "WARNING: Style not found: " .. line.style .. "\n")
		line.styleref = styles[1]
	end
	
	-- Calculate whole line sizing
	line.width, line.height, line.descent, line.extlead = aegisub.text_extents(line.styleref, line.text_stripped)
	line.width = line.width * meta.video_x_correct_factor

	-- Calculate syllable sizing
	for s = 0, line.kara.n do
		local syl = line.kara[s]
		syl.style = line.styleref
		syl.width, syl.height = aegisub.text_extents(syl.style, syl.text_spacestripped)
		syl.width = syl.width * meta.video_x_correct_factor
		syl.prespacewidth = aegisub.text_extents(syl.style, syl.prespace) * meta.video_x_correct_factor
		syl.postspacewidth = aegisub.text_extents(syl.style, syl.postspace) * meta.video_x_correct_factor
	end
	
	-- Calculate furigana sizing
	if styles[line.style .. "-furigana"] then
		line.furistyle = styles[line.style .. "-furigana"]
	else
		aegisub.debug.out(4, "No furigana style defined for style '%s'\n", line.style)
		line.furistyle = false
	end
	if line.furistyle then
		for f = 1, line.furi.n do
			local furi = line.furi[f]
			furi.style = line.furistyle
			furi.width, furi.height = aegisub.text_extents(furi.style, furi.text)
			furi.width = furi.width * meta.video_x_correct_factor
			furi.prespacewidth = 0
			furi.postspacewidth = 0
		end
	end
end


-- Layout a line, including furigana layout
-- Modifies the object passed for line
function karaskel.preproc_line_pos(meta, styles, line)
	if not line.styleref then
		karaskel.preproc_line_size(meta, styles, line)
	end
	
	-- Syllable layouting must be done before the rest, since furigana layout may change the total width of the line
	if line.furistyle then
		karaskel.do_furigana_layout(meta, styles, line)
	else
		karaskel.do_basic_layout(meta, styles, line)
	end

	-- Effective margins
	line.margin_v = line.margin_t
	line.eff_margin_l = ((line.margin_l > 0) and line.margin_l) or line.styleref.margin_l
	line.eff_margin_r = ((line.margin_r > 0) and line.margin_r) or line.styleref.margin_r
	line.eff_margin_t = ((line.margin_t > 0) and line.margin_t) or line.styleref.margin_t
	line.eff_margin_b = ((line.margin_b > 0) and line.margin_b) or line.styleref.margin_b
	line.eff_margin_v = ((line.margin_v > 0) and line.margin_v) or line.styleref.margin_v
	-- And positioning
	if line.styleref.align == 1 or line.styleref.align == 4 or line.styleref.align == 7 then
		-- Left aligned
		line.left = line.eff_margin_l
		line.center = line.left + line.width / 2
		line.right = line.left + line.width
		line.x = line.left
		line.halign = "left"
	elseif line.styleref.align == 2 or line.styleref.align == 5 or line.styleref.align == 8 then
		-- Centered
		line.left = (meta.res_x - line.eff_margin_l - line.eff_margin_r - line.width) / 2 + line.eff_margin_l
		line.center = line.left + line.width / 2
		line.right = line.left + line.width
		line.x = line.center
		line.halign = "center"
	elseif line.styleref.align == 3 or line.styleref.align == 6 or line.styleref.align == 9 then
		-- Right aligned
		line.left = meta.res_x - line.eff_margin_r - line.width
		line.center = line.left + line.width / 2
		line.right = line.left + line.width
		line.x = line.right
		line.halign = "right"
	end
	line.hcenter = line.center
	if line.styleref.align >=1 and line.styleref.align <= 3 then
		-- Bottom aligned
		line.bottom = meta.res_y - line.eff_margin_b
		line.middle = line.bottom - line.height / 2
		line.top = line.bottom - line.height
		line.y = line.bottom
		line.valign = "bottom"
	elseif line.styleref.align >= 4 and line.styleref.align <= 6 then
		-- Mid aligned
		line.top = (meta.res_y - line.eff_margin_t - line.eff_margin_b - line.height) / 2 + line.eff_margin_t
		line.middle = line.top + line.height / 2
		line.bottom = line.top + line.height
		line.y = line.middle
		line.valign = "middle"
	elseif line.styleref.align >= 7 and line.styleref.align <= 9 then
		-- Top aligned
		line.top = line.eff_margin_t
		line.middle = line.top + line.height / 2
		line.bottom = line.top + line.height
		line.y = line.top
		line.valign = "top"
	end
	line.vcenter = line.middle
end


-- Do simple syllable layouting (no furigana)
function karaskel.do_basic_layout(meta, styles, line)
	local curx = 0
	for i = 0, line.kara.n do
		local syl = line.kara[i]
		syl.left = curx + syl.prespacewidth
		syl.center = syl.left + syl.width / 2
		syl.right = syl.left + syl.width
		curx = curx + syl.prespacewidth + syl.width + syl.postspacewidth
	end
end


-- Do advanced furigana layout algorithm
function karaskel.do_furigana_layout(meta, styles, line)
	-- Start by building layout groups
	-- Two neighboring syllables with furigana that join together are part of the same layout group
	-- A forced split creates a new layout group
	local lgroups = {}
	-- Start-sentinel
	local lgsentinel = {basewidth=0, furiwidth=0, syls={}, furi={}, spillback=false, left=0, right=0}
	table.insert(lgroups, lgsentinel)
	-- Create groups
	local last_had_furi = false
	local lg = { basewidth=0, furiwidth=0, syls={}, furi={}, spillback=false }
	for s = 0, line.kara.n do
		local syl = line.kara[s]
		-- Furigana-less syllables always generate a new layout group
		-- So do furigana-endowed syllables that are marked as split
		-- But if current lg has no width (usually only first) don't create a new
		aegisub.debug.out(5, "syl.furi.n=%d, isbreak=%s, last_had_furi=%s, lg.basewidth=%d\n", syl.furi.n, syl.furi.n > 0 and syl.furi[1].isbreak and "y" or "n", last_had_furi and "y" or "n", lg.basewidth)
		if (syl.furi.n == 0 or syl.furi[1].isbreak or not last_had_furi) and lg.basewidth > 0 then
			aegisub.debug.out(5, "Inserting layout group, basewidth=%d, furiwidth=%d, isbreak=%s\n", lg.basewidth, lg.furiwidth, syl.furi.n > 0 and syl.furi[1].isbreak and "y" or "n")
			table.insert(lgroups, lg)
			lg = { basewidth=0, furiwidth=0, syls={}, furi={}, spillback=false }
			last_had_furi = false
		end
		
		-- Add this syllable to lg
		lg.basewidth = lg.basewidth + syl.prespacewidth + syl.width + syl.postspacewidth
		table.insert(lg.syls, syl)
		aegisub.debug.out(5, "\tAdding syllable to layout group: '%s', width=%d, isbreak=%s\n", syl.text_stripped, syl.width, syl.furi.n > 0 and syl.furi[1].isbreak and "y" or "n")
		
		-- Add this syllable's furi to lg
		for f = 1, syl.furi.n do
			local furi = syl.furi[f]
			lg.furiwidth = lg.furiwidth + furi.width
			lg.spillback = lg.spillback or furi.spillback
			table.insert(lg.furi, furi)
			aegisub.debug.out(5, "\tAdding furigana to layout group: %s (width=%d)\n", furi.text, furi.width)
			last_had_furi = true
		end
	end
	-- Insert last lg
	aegisub.debug.out(5, "Inserting layout group, basewidth=%d, furiwidth=%d\n", lg.basewidth, lg.furiwidth)
	table.insert(lgroups, lg)
	-- And end-sentinel
	table.insert(lgroups, lgsentinel)

	aegisub.debug.out(5, "\nProducing layout from %d layout groups\n", #lgroups-1)
	-- Layout the groups at macro-level
	-- Skip sentinel at ends in loop
	local curx = 0
	for i = 2, #lgroups-1 do
		local lg = lgroups[i]
		local prev = lgroups[i-1]
		aegisub.debug.out(5, "Layout group, nsyls=%d, nfuri=%d, syl1text='%s', basewidth=%f furiwidth=%f, ", #lg.syls, #lg.furi, lg.syls[1] and lg.syls[1].text or "", lg.basewidth, lg.furiwidth)
		
		-- Three cases: No furigana, furigana smaller than base and furigana larger than base
		if lg.furiwidth == 0 then
			-- Here wa can basically just place the base text
			lg.left = curx
			lg.right = lg.left + lg.basewidth
			-- If there was any spillover from a previous group, add it to here
			if prev.rightspill  and prev.rightspill > 0 then
				aegisub.debug.out(5, "eat rightspill=%f, ", prev.rightspill)
				lg.leftspill = 0
				lg.rightspill = prev.rightspill - lg.basewidth
				prev.rightspill = 0
			end
			curx = curx + lg.basewidth
		elseif lg.furiwidth <= lg.basewidth then
			-- If there was any rightspill from previous group, we have to stay 100% clear of that
			if prev.rightspill and prev.rightspill > 0 then
				aegisub.debug.out(5, "skip rightspill=%f, ", prev.rightspill)
				curx = curx + prev.rightspill
				prev.rightspill = 0
			end
			lg.left = curx
			lg.right = lg.left + lg.basewidth
			curx = curx + lg.basewidth
			-- Negative spill here
			lg.leftspill = (lg.furiwidth - lg.basewidth) / 2
			lg.rightspill = lg.leftspill
		else
			-- Furigana is wider than base, we'll have to spill in some direction
			if prev.rightspill and prev.rightspill > 0 then
				aegisub.debug.out(5, "skip rightspill=%f, ", prev.rightspill)
				curx = curx + prev.rightspill
				prev.rightspill = 0
			end
			-- Do we spill only to the right or in both directions?
			if lg.spillback then
				-- Both directions
				lg.leftspill = (lg.furiwidth - lg.basewidth) / 2
				lg.rightspill = lg.leftspill
				aegisub.debug.out(5, "spill left=%f right=%f, ", lg.leftspill, lg.rightspill)
				-- If there was any furigana or spill on previous syllable we can't overlap it
				if prev.rightspill then
					lg.left = curx + lg.leftspill
				else
					lg.left = curx
				end
			else
				-- Only to the right
				lg.leftspill = 0
				lg.rightspill = lg.furiwidth - lg.basewidth
				aegisub.debug.out(5, "spill right=%f, ", lg.rightspill)
				lg.left = curx
			end
			lg.right = lg.left + lg.basewidth
			curx = lg.right
		end
		aegisub.debug.out(5, "left=%f, right=%f\n", lg.left, lg.right)
	end
	
	-- Now the groups are layouted, so place the individual syllables/furigana
	for i, lg in ipairs(lgroups) do
		local basecenter = (lg.left + lg.right) / 2 -- centered furi is centered over this
		local curx = lg.left -- base text is placed from here on
		-- Place base syllables
		for s, syl in ipairs(lg.syls) do
			syl.left = curx + syl.prespacewidth
			syl.center = syl.left + syl.width/2
			syl.right = syl.left + syl.width
			curx = syl.right + syl.postspacewidth
		end
		if curx > line.width then line.width = curx end
		-- Place furigana
		if lg.furiwidth < lg.basewidth or lg.spillback then
			-- Center over group
			curx = lg.left + (lg.basewidth - lg.furiwidth) / 2
		else
			-- Left aligned
			curx = lg.left
		end
		for f, furi in ipairs(lg.furi) do
			furi.left = curx
			furi.center = furi.left + furi.width/2
			furi.right = furi.left + furi.width
			curx = furi.right
		end
	end
end


-- Precalc some info on a line
-- Modifies the line parameter
function karaskel.preproc_line(subs, meta, styles, line)
	-- subs parameter is never used and probably won't ever be
	-- (it wouldn't be fun if some lines suddenly changed index here)
	-- pass whatever you want, but be careful calling preproc_line_pos directly, that interface might change
	karaskel.preproc_line_pos(meta, styles, line)
end


-- An actual "skeleton" function
-- Parses the first word out of the Effect field of each dialogue line and runs "fx_"..effect on that line
-- Lines with empty Effect field run fx_none
-- Lines with unimplemented effects are left alone
-- If the effect function returns true, the original line is kept in output,
-- otherwise the original line is converted to a comment
-- General prototype of an fx function: function(subs, meta, styles, line, fxdata)
-- fxdata are extra data after the effect name in the Effect field
local fx_library_registered = false
function karaskel.use_fx_library_furi(use_furigana, macrotoo)
	local function fx_library_main(subs)
		aegisub.progress.task("Collecting header info")
		meta, styles = karaskel.collect_head(subs, use_furigana)
		
		aegisub.progress.task("Processing subs")
		local i, maxi = 1, #subs
		while i <= maxi do
			aegisub.progress.set(i/maxi*100)
			local l = subs[i]
			
			if l.class == "dialogue" then
				aegisub.progress.task(l.text)
				karaskel.preproc_line(subs, meta, styles, l)
				local keep = true
				local fx, fxdata = string.headtail(l.effect)
				if fx == "" then fx = "none" end
				if _G["fx_" .. fx] then
					-- note to casual readers: _G is a special global variable that points to the global environment
					-- specifically, _G["_G"] == _G
					keep = _G["fx_" .. fx](subs, meta, styles, l, fxdata)
				end
				if not keep then
					l = subs[i]
					l.comment = true
					subs[i] = l
				end
			end
			
			i = i + 1
		end
	end

	if fx_library_registered then return end
	aegisub.register_filter(script_name or "fx_library", script_description or "Apply karaoke effects (fx_library skeleton)", 2000, fx_library_main)
	
	if macrotoo then
		local function fxlibmacro(subs)
			fx_library_main(subs)
			aegisub.set_undo_point(script_name or "karaoke effect")
		end
		aegisub.register_macro(script_name or "fx_library", script_description or "Apply karaoke effects (fx_library skeleton)", fxlibmacro)
	end
end
function karaskel.use_fx_library(macrotoo)
	return karaskel.use_fx_library_furi(false, macrotoo)
end


-- A skeleton that approximately simulates the Auto3 "advanced" one.
-- Build a Auto3-like list of dialogue lines and also add linked list refs to lines.
-- Call user-defined do_line function for each line, if it exists.
-- The default do_line function will call the do_syllable function for each line,
-- if the function exists.
-- The function names called are constant.
local classic_adv_registered = false
function karaskel.use_classic_adv(use_furigana, macrotoo)
	local function classic_adv_main(subs)
		
		local function default_do_syllable(subs, meta, styles, lines, line, syl)
			-- do nothing
		end
	
		local sylfunc = (type(_G.do_syllable)=="function" and _G.do_syllable) or default_do_syllable
		local furifunc = (type(_G.do_furigana)=="function" and _G.do_furigana) or default_do_syllable

		local function default_do_line(subs, meta, styles, lines, line)
			for i = 0, line.kara.n do
				sylfunc(subs, meta, styles, lines, line, line.kara[i])
			end
			if use_furigana then
				for i = 0, line.furi.n do
					furifunc(subs, meta, styles, lines, line, line.furi[i])
				end
			end
		end
		
		aegisub.progress.task("Collecting header info")
		local meta, styles = karaskel.collect_head(subs, use_furigana)
		
		-- Collect lines
		aegisub.progress.task("Collecting subtitle lines")
		local lines = { n=0 }
		local prevline = nil
		local i = 1
		local curorgline, maxorglines = 1, #subs
		while i <= #subs do
			aegisub.progress.set(curorgline/maxorglines*100)
			local l = subs[i]
			if l.class == "dialogue" then
				-- Link prev of this one
				karaskel.preproc_line(subs, meta, styles, l)
				l.prev = prevline
				l.next = nil
				-- Line next of prev one
				if prevline then
					prevline.next = l
				end
				-- Insert into array
				lines.n = lines.n + 1
				lines[lines.n] = l
				-- Update prev
				prevline = l
				-- Delete from file
				subs[i] = nil
			else
				-- Only increase for non-dialogue lines
				-- (Dialogue lines are deleted, so every other lines moves one down)
				 i = i + 1
			end
			curorgline = curorgline + 1
		end
		
		aegisub.progress.task("Processing subtitles")
		local linefunc = default_do_line
		if type(_G.do_line)=="function" then
			linefunc = function(subs, meta, styles, lines, line)
				return _G.do_line(subs, meta, styles, lines, line, default_do_line)
			end
		end
		for i = 1, lines.n do
			aegisub.progress.set(i/lines.n*100)
			linefunc(subs, meta, styles, lines, lines[i])
		end
		
		aegisub.progress.task("Finished")
		aegisub.progress.set(100)
	end
	
	if classic_adv_registered then return end
	aegisub.register_filter(script_name or "classic_adv", script_description or "Apply karaoke effects (classic_adv skeleton)", 2000, classic_adv_main)
	
	if macrotoo then
		local function classic_adv_macro(subs)
			classic_adv_main(subs)
			aegisub.set_undo_point(script_name or "karaoke effect")
		end
		aegisub.register_macro(script_name or "classic_adv", script_description or "Apply karaoke effects (classic_adv skeleton)", classic_adv_macro)
	end
end
