--[[
 Copyright (c) 2005-2010, Niels Martin Hansen, Rodrigo Braz Monteiro
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

_G.util = require 'aegisub.util'

_G.table.copy = _G.util.copy
_G.copy_line = _G.util.copy
_G.table.copy_deep = _G.util.deep_copy
_G.ass_color = _G.util.ass_color
_G.ass_alpha = _G.util.ass_alpha
_G.ass_style_color = _G.util.ass_style_color
_G.extract_color = _G.util.extract_color
_G.alpha_from_style = _G.util.alpha_from_style
_G.color_from_style = _G.util.color_from_style
_G.HSV_to_RGB = _G.util.HSV_to_RGB
_G.HSL_to_RGB = _G.util.HSL_to_RGB
_G.clamp = _G.util.clamp
_G.interpolate = _G.util.interpolate
_G.interpolate_color = _G.util.interpolate_color
_G.interpolate_alpha = _G.util.interpolate_alpha
_G.string.headtail = _G.util.headtail
_G.string.trim = _G.util.trim
_G.string.words = _G.util.words

return _G.util