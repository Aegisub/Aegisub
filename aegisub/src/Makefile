include ../Makefile.inc

PROGRAM = $(AEGISUB_COMMAND)
PROGRAM_INSTALL = yes

PRECOMPILED_HEADER_NAME=agi_pre.h

AEGISUB_CFLAGS = -I. -I.. -Iinclude -I../libaegisub/include -I../build -DAEGISUB $(CFLAGS_PTHREAD) $(CFLAGS_FFTW3) $(CFLAGS_ICU)

CFLAGS += $(AEGISUB_CFLAGS)
CXXFLAGS += $(AEGISUB_CFLAGS) -D__STDC_FORMAT_MACROS $(CXXFLAGS_WX)

LIBS := -L../libaegisub -laegisub $(LIBS)
LIBS += $(LIBS_GL) $(LIBS_PTHREAD) $(LIBS_WX) $(LIBS_FREETYPE)
LIBS += $(LIBS_FONTCONFIG) $(LIBS_FFTW3) $(LIBS_UCHARDET) $(LIBS_BOOST)
LIBS += $(LIBS_ICU) $(LIBS_LUA)

ifeq (yes, $(BUILD_DARWIN))
SRC += osx_utils.mm osx_retina_helper.mm
endif

###############
# AUDIO PLAYERS
###############
ifeq (yes, $(HAVE_ALSA))
audio_player_alsa.o: CXXFLAGS += $(CFLAGS_ALSA)
LIBS += $(LIBS_ALSA)
SRC += audio_player_alsa.cpp
else
EXTRA_DIST += audio_player_alsa.cpp
endif

ifeq (yes, $(HAVE_PORTAUDIO))
audio_player_portaudio.o: CXXFLAGS += $(CFLAGS_PORTAUDIO)
LIBS += $(LIBS_PORTAUDIO)
SRC += audio_player_portaudio.cpp
else
EXTRA_DIST += audio_player_portaudio.cpp
endif

ifeq (yes, $(HAVE_LIBPULSE))
audio_player_pulse.o: CXXFLAGS += $(CFLAGS_LIBPULSE)
LIBS += $(LIBS_LIBPULSE)
SRC += audio_player_pulse.cpp
else
EXTRA_DIST += audio_player_pulse.cpp
endif

ifeq (yes, $(HAVE_OPENAL))
audio_player_openal.o: CXXFLAGS += $(CFLAGS_OPENAL)
LIBS += $(LIBS_OPENAL)
SRC += audio_player_openal.cpp
else
EXTRA_DIST += audio_player_openal.cpp
endif

ifeq (yes, $(HAVE_OSS))
audio_player_oss.o: CXXFLAGS += $(CFLAGS_OSS)
SRC += audio_player_oss.cpp
else
EXTRA_DIST += audio_player_oss.cpp
endif

#######################
# AUDIO / VIDEO SUPPORT
#######################
ifeq (yes, $(HAVE_FFMS2))
audio_provider_ffmpegsource.o video_provider_ffmpegsource.o ffmpegsource_common.o preferences.o: \
	CXXFLAGS += $(CFLAGS_FFMS2)
LIBS += $(LIBS_FFMS2)
SRC += audio_provider_ffmpegsource.cpp video_provider_ffmpegsource.cpp ffmpegsource_common.cpp
else
EXTRA_DIST += audio_provider_ffmpegsource.cpp video_provider_ffmpegsource.cpp ffmpegsource_common.cpp
endif

###########
# SUBTITLES
###########
subtitles_provider_libass.o: CXXFLAGS += $(CFLAGS_LIBASS) -Wno-c++11-narrowing
subtitles_provider.o: CXXFLAGS += $(CFLAGS_LIBASS)
LIBS += $(LIBS_LIBASS)
SRC += subtitles_provider_libass.cpp

##############
# MISCELLANOUS
##############
ifeq (yes, $(HAVE_HUNSPELL))
spellchecker_hunspell.o: CXXFLAGS += $(CFLAGS_HUNSPELL)
LIBS += $(LIBS_HUNSPELL)
SRC += spellchecker_hunspell.cpp
else
EXTRA_DIST += spellchecker_hunspell.cpp
endif

#####################
# SOURCE-LEVEL CFLAGS
#####################
audio_player.o:                CXXFLAGS += $(CFLAGS_ALSA) $(CFLAGS_PORTAUDIO) $(CFLAGS_LIBPULSE) $(CFLAGS_OPENAL)
audio_provider.o:              CXXFLAGS += $(CFLAGS_FFMS2)
auto4_base.o:                  CXXFLAGS += $(CFLAGS_FREETYPE)
charset_detect.o:              CXXFLAGS += -D_X86_
font_file_lister_fontconfig.o: CXXFLAGS += $(CFLAGS_FONTCONFIG)
text_file_reader.o:            CXXFLAGS += -D_X86_
video_provider_manager.o:      CXXFLAGS += $(CFLAGS_FFMS2)
auto4_lua.o auto4_lua_assfile.o auto4_lua_dialog.o  auto4_lua_progresssink.o auto4_lua_regex.o auto4_lua_scriptreader.o auto4_regex.o: CXXFLAGS += $(CFLAGS_LUA)
lpeg.o: CFLAGS += $(CFLAGS_LUA)

SRC += \
	MatroskaParser.c \
	aegisublocale.cpp \
	ass_attachment.cpp \
	ass_dialogue.cpp \
	ass_entry.cpp \
	ass_export_filter.cpp \
	ass_exporter.cpp \
	ass_file.cpp \
	ass_karaoke.cpp \
	ass_override.cpp \
	ass_parser.cpp \
	ass_style.cpp \
	ass_style_storage.cpp \
	ass_time.cpp \
	audio_box.cpp \
	audio_colorscheme.cpp \
	audio_controller.cpp \
	audio_display.cpp \
	audio_karaoke.cpp \
	audio_marker.cpp \
	audio_player.cpp \
	audio_provider.cpp \
	audio_provider_convert.cpp \
	audio_provider_dummy.cpp \
	audio_provider_hd.cpp \
	audio_provider_lock.cpp \
	audio_provider_pcm.cpp \
	audio_provider_ram.cpp \
	audio_renderer.cpp \
	audio_renderer_spectrum.cpp \
	audio_renderer_waveform.cpp \
	audio_timing_dialogue.cpp \
	audio_timing_karaoke.cpp \
	auto4_base.cpp \
	auto4_lua.cpp \
	auto4_lua_assfile.cpp \
	auto4_lua_dialog.cpp \
	auto4_lua_progresssink.cpp \
	auto4_lua_scriptreader.cpp \
	auto4_regex.cpp \
	avisynth_wrap.cpp \
	base_grid.cpp \
	charset_detect.cpp \
	colorspace.cpp \
	colour_button.cpp \
	compat.cpp \
	dialog_about.cpp \
	dialog_attachments.cpp \
	dialog_automation.cpp \
	dialog_autosave.cpp \
	dialog_colorpicker.cpp \
	dialog_detached_video.cpp \
	dialog_dummy_video.cpp \
	dialog_export.cpp \
	dialog_export_ebu3264.cpp \
	dialog_fonts_collector.cpp \
	dialog_jumpto.cpp \
	dialog_kara_timing_copy.cpp \
	dialog_log.cpp \
	dialog_paste_over.cpp \
	dialog_progress.cpp \
	dialog_properties.cpp \
	dialog_resample.cpp \
	dialog_search_replace.cpp \
	dialog_selected_choices.cpp \
	dialog_selection.cpp \
	dialog_shift_times.cpp \
	dialog_spellchecker.cpp \
	dialog_style_editor.cpp \
	dialog_style_manager.cpp \
	dialog_styling_assistant.cpp \
	dialog_text_import.cpp \
	dialog_timing_processor.cpp \
	dialog_translation.cpp \
	dialog_version_check.cpp \
	dialog_video_details.cpp \
	export_fixstyle.cpp \
	export_framerate.cpp \
	fft.cpp \
	font_file_lister.cpp \
	font_file_lister_fontconfig.cpp \
	frame_main.cpp \
	gl_text.cpp \
	gl_wrap.cpp \
	help_button.cpp \
	hotkey.cpp \
	hotkey_data_view_model.cpp \
	initial_line_state.cpp \
	lpeg.c \
	main.cpp \
	menu.cpp \
	mkv_wrap.cpp \
	pen.cpp \
	persist_location.cpp \
	plugin_manager.cpp \
	preferences.cpp \
	preferences_base.cpp \
	resolution_resampler.cpp \
	scintilla_text_ctrl.cpp \
	scintilla_text_selection_controller.cpp \
	search_replace_engine.cpp \
	spellchecker.cpp \
	spline.cpp \
	spline_curve.cpp \
	string_codec.cpp \
	subs_controller.cpp \
	subs_edit_box.cpp \
	subs_edit_ctrl.cpp \
	subs_preview.cpp \
	subtitle_format.cpp \
	subtitle_format_ass.cpp \
	subtitle_format_ebu3264.cpp \
	subtitle_format_encore.cpp \
	subtitle_format_microdvd.cpp \
	subtitle_format_mkv.cpp \
	subtitle_format_srt.cpp \
	subtitle_format_transtation.cpp \
	subtitle_format_ttxt.cpp \
	subtitle_format_txt.cpp \
	subtitles_provider.cpp \
	text_file_reader.cpp \
	text_file_writer.cpp \
	thesaurus.cpp \
	threaded_frame_source.cpp \
	timeedit_ctrl.cpp \
	toggle_bitmap.cpp \
	toolbar.cpp \
	tooltip_manager.cpp \
	utils.cpp \
	validators.cpp \
	vector2d.cpp \
	version.cpp \
	video_box.cpp \
	video_context.cpp \
	video_display.cpp \
	video_frame.cpp \
	video_out_gl.cpp \
	video_provider_cache.cpp \
	video_provider_dummy.cpp \
	video_provider_manager.cpp \
	video_provider_yuv4mpeg.cpp \
	video_slider.cpp \
	visual_feature.cpp \
	visual_tool.cpp \
	visual_tool_clip.cpp \
	visual_tool_cross.cpp \
	visual_tool_drag.cpp \
	visual_tool_rotatexy.cpp \
	visual_tool_rotatez.cpp \
	visual_tool_scale.cpp \
	visual_tool_vector_clip.cpp


HEADER += \
	*.h \
	command/*.h \
	config/*.h \
	include/aegisub/*.h \
	gl/*.h

EXTRA_DIST += \
	bitmaps/button/*.png \
	bitmaps/misc/*.png \
	bitmaps/windows/eyedropper.cur \
	bitmaps/windows/icon.ico \
	bitmaps/*.respack

EXTRA_DIST += \
	command/Makefile.inc \
	libresrc/Makefile.inc

include command/Makefile.inc
include libresrc/Makefile.inc

include ../Makefile.target
-include *.d
-include command/*.d
-include libresrc/*.d
