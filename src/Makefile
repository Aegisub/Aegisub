include $(dir $(lastword $(MAKEFILE_LIST)))../header.mk

PROGRAM += $(d)src

src_CPPFLAGS := -I$(d) -I.. -I$(d)include -I$(TOP)libaegisub/include -I$(TOP)build \
	$(CFLAGS_PTHREAD) $(CFLAGS_FFTW3) $(CFLAGS_ICU) $(CPPFLAGS_BOOST)
src_CXXFLAGS := $(CXXFLAGS_WX)
src_LIBS := $(LIBS_GL) $(LIBS_PTHREAD) $(LIBS_WX) $(LIBS_FREETYPE) \
	$(LIBS_LIBASS) $(LIBS_FONTCONFIG) $(LIBS_FFTW3)  $(LIBS_BOOST) $(LIBS_ICU)
src_PCH := $(d)agi_pre.h
src_INSTALLNAME := $(AEGISUB_COMMAND)

src_OBJ := \
	$(patsubst %.cpp,%.o,$(sort $(wildcard $(d)command/*.cpp))) \
	$(patsubst %.cpp,%.o,$(sort $(wildcard $(d)dialog_*.cpp))) \
	$(patsubst %.cpp,%.o,$(sort $(wildcard $(d)subtitle_format*.cpp))) \
	$(patsubst %.cpp,%.o,$(sort $(wildcard $(d)visual_tool*.cpp))) \
	$(d)MatroskaParser.o \
	$(d)aegisublocale.o \
	$(d)ass_attachment.o \
	$(d)ass_dialogue.o \
	$(d)ass_entry.o \
	$(d)ass_export_filter.o \
	$(d)ass_exporter.o \
	$(d)ass_file.o \
	$(d)ass_karaoke.o \
	$(d)ass_override.o \
	$(d)ass_parser.o \
	$(d)ass_style.o \
	$(d)ass_style_storage.o \
	$(d)async_video_provider.o \
	$(d)audio_box.o \
	$(d)audio_colorscheme.o \
	$(d)audio_controller.o \
	$(d)audio_display.o \
	$(d)audio_karaoke.o \
	$(d)audio_marker.o \
	$(d)audio_player.o \
	$(d)audio_provider_factory.o \
	$(d)audio_renderer.o \
	$(d)audio_renderer_spectrum.o \
	$(d)audio_renderer_waveform.o \
	$(d)audio_timing_dialogue.o \
	$(d)audio_timing_karaoke.o \
	$(d)auto4_base.o \
	$(d)auto4_lua.o \
	$(d)auto4_lua_assfile.o \
	$(d)auto4_lua_dialog.o \
	$(d)auto4_lua_progresssink.o \
	$(d)avisynth_wrap.o \
	$(d)base_grid.o \
	$(d)charset_detect.o \
	$(d)colorspace.o \
	$(d)colour_button.o \
	$(d)compat.o \
	$(d)context.o \
	$(d)crash_writer.o \
	$(d)export_fixstyle.o \
	$(d)export_framerate.o \
	$(d)fft.o \
	$(d)font_file_lister.o \
	$(d)frame_main.o \
	$(d)gl_text.o \
	$(d)gl_wrap.o \
	$(d)grid_column.o \
	$(d)help_button.o \
	$(d)hotkey.o \
	$(d)hotkey_data_view_model.o \
	$(d)initial_line_state.o \
	$(d)main.o \
	$(d)menu.o \
	$(d)mkv_wrap.o \
	$(d)pen.o \
	$(d)persist_location.o \
	$(d)preferences.o \
	$(d)preferences_base.o \
	$(d)project.o \
	$(d)resolution_resampler.o \
	$(d)search_replace_engine.o \
	$(d)selection_controller.o \
	$(d)spellchecker.o \
	$(d)spline.o \
	$(d)spline_curve.o \
	$(d)string_codec.o \
	$(d)subs_controller.o \
	$(d)subs_edit_box.o \
	$(d)subs_edit_ctrl.o \
	$(d)subs_preview.o \
	$(d)subtitles_provider.o \
	$(d)subtitles_provider_libass.o \
	$(d)text_file_reader.o \
	$(d)text_file_writer.o \
	$(d)text_selection_controller.o \
	$(d)thesaurus.o \
	$(d)timeedit_ctrl.o \
	$(d)toggle_bitmap.o \
	$(d)toolbar.o \
	$(d)tooltip_manager.o \
	$(d)utils.o \
	$(d)validators.o \
	$(d)vector2d.o \
	$(d)version.o \
	$(d)video_box.o \
	$(d)video_controller.o \
	$(d)video_display.o \
	$(d)video_frame.o \
	$(d)video_out_gl.o \
	$(d)video_provider_cache.o \
	$(d)video_provider_dummy.o \
	$(d)video_provider_manager.o \
	$(d)video_provider_yuv4mpeg.o \
	$(d)video_slider.o \
	$(d)visual_feature.o \
	$(LIBS_LUA) \
	$(TOP)lib/libaegisub.a \
	$(TOP)lib/libluabins.a \
	$(TOP)lib/libresrc.a \

ifeq (yes, $(BUILD_DARWIN))
src_OBJ += $(d)font_file_lister_coretext.o
src_OBJ += $(patsubst %.mm,%.o,$(sort $(wildcard $(d)osx/*.mm)))
$(d)font_file_lister_coretext.o_FLAGS := -fobjc-arc
else
src_OBJ += $(d)font_file_lister_fontconfig.o
endif

###############
# AUDIO PLAYERS
###############
$(d)audio_player_alsa.o_FLAGS      := $(CFLAGS_ALSA)
$(d)audio_player_portaudio.o_FLAGS := $(CFLAGS_PORTAUDIO)
$(d)audio_player_pulse.o_FLAGS     := $(CFLAGS_LIBPULSE)
$(d)audio_player_openal.o_FLAGS    := $(CFLAGS_OPENAL)
$(d)audio_player_oss.o_FLAGS       := $(CFLAGS_OSS)

ifeq (yes, $(HAVE_ALSA))
src_LIBS += $(LIBS_ALSA)
src_OBJ += $(d)audio_player_alsa.o
endif

ifeq (yes, $(HAVE_PORTAUDIO))
src_LIBS += $(LIBS_PORTAUDIO)
src_OBJ += $(d)audio_player_portaudio.o
endif

ifeq (yes, $(HAVE_LIBPULSE))
src_LIBS += $(LIBS_LIBPULSE)
src_OBJ += $(d)audio_player_pulse.o
endif

ifeq (yes, $(HAVE_OPENAL))
src_LIBS += $(LIBS_OPENAL)
src_OBJ += $(d)audio_player_openal.o
endif

ifeq (yes, $(HAVE_OSS))
src_OBJ += $(d)audio_player_oss.o
endif

#######################
# AUDIO / VIDEO SUPPORT
#######################
ifeq (yes, $(HAVE_FFMS2))
$(d)audio_provider_ffmpegsource.o_FLAGS := $(CFLAGS_FFMS2)
$(d)video_provider_ffmpegsource.o_FLAGS := $(CFLAGS_FFMS2)
$(d)ffmpegsource_common.o_FLAGS         := $(CFLAGS_FFMS2)
$(d)preferences.o_FLAGS                 := $(CFLAGS_FFMS2)
src_LIBS += $(LIBS_FFMS2)
src_OBJ += \
	$(d)audio_provider_ffmpegsource.o \
	$(d)video_provider_ffmpegsource.o \
	$(d)ffmpegsource_common.o
endif

##############
# MISCELLANOUS
##############
ifeq (yes, $(HAVE_HUNSPELL))
$(d)spellchecker_hunspell.o_FLAGS := $(CFLAGS_HUNSPELL)
src_LIBS += $(LIBS_HUNSPELL)
src_OBJ += $(d)spellchecker_hunspell.o
endif

ifeq (yes, $(HAVE_UCHARDET))
src_LIBS += $(LIBS_UCHARDET)
endif

#####################
# SOURCE-LEVEL CFLAGS
#####################
$(d)MatroskaParser.o_FLAGS              := -Wno-sometimes-uninitialized
$(d)audio_player.o_FLAGS                := $(CFLAGS_ALSA) $(CFLAGS_PORTAUDIO) $(CFLAGS_LIBPULSE) $(CFLAGS_OPENAL)
$(d)audio_provider_factory.o_FLAGS      := $(CFLAGS_FFMS2)
$(d)auto4_base.o_FLAGS                  := $(CFLAGS_FREETYPE)
$(d)charset_detect.o_FLAGS              := -D_X86_
$(d)font_file_lister_fontconfig.o_FLAGS := $(CFLAGS_FONTCONFIG)
$(d)subtitles_provider.o_FLAGS          := $(CFLAGS_LIBASS)
$(d)subtitles_provider_libass.o_FLAGS   := $(CFLAGS_LIBASS) -Wno-c++11-narrowing
$(d)text_file_reader.o_FLAGS            := -D_X86_
$(d)video_provider_manager.o_FLAGS      := $(CFLAGS_FFMS2)
$(d)auto4_lua.o_FLAGS                   := $(CFLAGS_LUA)
$(d)auto4_lua_assfile.o_FLAGS           := $(CFLAGS_LUA)
$(d)auto4_lua_dialog.o_FLAGS            := $(CFLAGS_LUA)
$(d)auto4_lua_progresssink.o_FLAGS      := $(CFLAGS_LUA)
$(d)aegisublocale.o_FLAGS               := -DP_LOCALE=\"$(P_LOCALE)\"

$(src_OBJ): $(d)libresrc/bitmap.h $(d)libresrc/default_config.h

include $(d)libresrc/Makefile
