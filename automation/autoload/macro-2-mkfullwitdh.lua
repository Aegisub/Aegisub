-- Automation 4 demo script
-- Converts halfwidth (ASCII) Latin letters to fullwidth JIS Latin letters

local tr = aegisub.gettext

script_name = tr"Make text fullwidth"
script_description = tr"Shows how to use the unicode include to iterate over characters and a lookup table to convert those characters to something else."
script_author = "Niels Martin Hansen"
script_version = "1"

include("unicode.lua")


lookup = {
	['!'] = '！', ['"'] = '”', ['#'] = '＃', ['$'] = '＄', 
	['%'] = '％', ['&'] = '＆', ["'"] = '’', ['('] = '（', 
	[')'] = '）', ['*'] = '＊', ['+'] = '＋', [','] = '，', 
	['-'] = '－', ['.'] = '．', ['/'] = '／',
	['1'] = '１', ['2'] = '２', ['3'] = '３', ['4'] = '４', 
	['5'] = '５', ['6'] = '６', ['7'] = '７', ['8'] = '８', 
	['9'] = '９', ['0'] = '０',
	[':'] = '：', [';'] = '；', ['<'] = '＜', ['='] = '＝', 
	['>'] = '＞', ['?'] = '？', ['@'] = '＠',
	['A'] = 'Ａ', ['B'] = 'Ｂ', ['C'] = 'Ｃ', ['D'] = 'Ｄ', 
	['E'] = 'Ｅ', ['F'] = 'Ｆ', ['G'] = 'Ｇ', ['H'] = 'Ｈ', 
	['I'] = 'Ｉ', ['J'] = 'Ｊ', ['K'] = 'Ｋ', ['L'] = 'Ｌ', 
	['M'] = 'Ｍ', ['N'] = 'Ｎ', ['O'] = 'Ｏ', ['P'] = 'Ｐ', 
	['Q'] = 'Ｑ', ['R'] = 'Ｒ', ['S'] = 'Ｓ', ['T'] = 'Ｔ', 
	['U'] = 'Ｕ', ['V'] = 'Ｖ', ['W'] = 'Ｗ', ['X'] = 'Ｘ', 
	['Y'] = 'Ｙ', ['Z'] = 'Ｚ',
	['['] = '［', ['\\'] = '＼', [']'] = '］', ['^'] = '＾', 
	['a'] = 'ａ', ['b'] = 'ｂ', ['c'] = 'ｃ', ['d'] = 'ｄ', 
	['e'] = 'ｅ', ['f'] = 'ｆ', ['g'] = 'ｇ', ['h'] = 'ｈ', 
	['i'] = 'ｉ', ['j'] = 'ｊ', ['k'] = 'ｋ', ['l'] = 'ｌ', 
	['m'] = 'ｍ', ['n'] = 'ｎ', ['o'] = 'ｏ', ['p'] = 'ｐ', 
	['q'] = 'ｑ', ['r'] = 'ｒ', ['s'] = 'ｓ', ['t'] = 'ｔ', 
	['u'] = 'ｕ', ['v'] = 'ｖ', ['w'] = 'ｗ', ['x'] = 'ｘ', 
	['y'] = 'ｙ', ['z'] = 'ｚ',
	['_'] = '＿', ['`'] = '‘',
	['{'] = '｛', ['|'] = '｜', ['}'] = '｝', ['~'] = '～', 
}

function make_fullwidth(subtitles, selected_lines, active_line)
	for z, i in ipairs(selected_lines) do
		local l = subtitles[i]
		
		aegisub.debug.out(string.format('Processing line %d: "%s"\n', i, l.text))
		aegisub.debug.out("Chars: \n")
		
		local in_tags = false
		local newtext = ""
		for c in unicode.chars(l.text) do
			aegisub.debug.out(c .. ' -> ')
			if c == "{" then
				in_tags = true
			end
			if in_tags then
				aegisub.debug.out(c .. " (ignored, in tags)\n")
				newtext = newtext .. c
			else
				if lookup[c] then
					aegisub.debug.out(lookup[c] .. " (converted)\n")
					newtext = newtext .. lookup[c]
				else
					aegisub.debug.out(c .. " (not found in lookup)\n")
					newtext = newtext .. c
				end
			end
			if c == "}" then
				in_tags = false
			end
		end
		
		l.text = newtext
		subtitles[i] = l
	end
	aegisub.set_undo_point(tr"Make fullwidth")
end

aegisub.register_macro(tr"Make fullwidth", tr"Convert Latin letters to SJIS fullwidth letters", make_fullwidth)
