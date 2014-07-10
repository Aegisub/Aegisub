--[[ 
"Clean Tags" -- An Auto4 LUA script for cleaning up ASS subtitle lines of badly-formed override 
blocks and redundant/duplicate tags
* Designed to work for Aegisub 2.0 and above (only pre-release version was available at the time of 
writing) @ http://www.malakith.net/aegiwiki
* Requires cleantags.lua to be available in automation's include folder
* The changes performed on your subtitles are guaranteed to be undo-able provided that Aegisub's undo 
mechanism works. Even so, I am not resposible if it damages your subtitles permanently, so please 
back up your subtitle file before applying the cleaning up

Copyright (c) 2007-2009 Muhammad Lukman Nasaruddin (aka ai-chan, Aegisub's forum member)

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and 
associated documentation files (the "Software"), to deal in the Software without restriction, 
including without limitation the rights to use, copy, modify, merge, publish, distribute, 
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is 
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial 
portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT 
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES 
OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN 
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
]]

local tr = aegisub.gettext

script_name = tr"Clean Tags"
script_description = tr"Clean subtitle lines by re-arranging ASS tags and override blocks within the lines"
script_author = "Muhammad Lukman Nasaruddin (ai-chan)"
script_version = "1.20"
script_modified = "25 February 2009"

include("cleantags.lua")

function cleantags_subs(subtitles)
	local linescleaned = 0
	for i = 1, #subtitles do
		aegisub.progress.set(i * 100 / #subtitles)
		local line = subtitles[i]
		if line.class == "dialogue" and not line.comment and line.text ~= "" then
			line.text = cleantags(line.text)
			subtitles[i] = line
			linescleaned = linescleaned + 1
			aegisub.progress.task(linescleaned .. " lines cleaned")
		end
	end
end

function cleantags_macro(subtitles, selected_lines, active_line)
	cleantags_subs(subtitles)
	aegisub.set_undo_point(script_name)
end

function cleantags_filter(subtitles, config)
	cleantags_subs(subtitles)
end

aegisub.register_macro(script_name, script_description, cleantags_macro)
aegisub.register_filter(script_name, script_description, 0, cleantags_filter)
