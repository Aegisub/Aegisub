include $(dir $(lastword $(MAKEFILE_LIST)))../header.mk

aegisub_OBJ := \
	$(d)common/parser.o \
	$(d)ass/dialogue_parser.o \
	$(d)ass/time.o \
	$(d)ass/uuencode.o \
	$(subst .cpp,.o,$(sort $(wildcard $(d)audio/*.cpp))) \
	$(subst .cpp,.o,$(sort $(wildcard $(d)common/cajun/*.cpp))) \
	$(subst .cpp,.o,$(sort $(wildcard $(d)lua/modules/*.cpp))) \
	$(subst .c,.o,$(sort $(wildcard $(d)lua/modules/*.c))) \
	$(subst .cpp,.o,$(sort $(wildcard $(d)lua/*.cpp))) \
	$(subst .cpp,.o,$(sort $(wildcard $(d)unix/*.cpp))) \
	$(d)common/calltip_provider.o \
	$(d)common/character_count.o \
	$(d)common/charset.o \
	$(d)common/charset_6937.o \
	$(d)common/charset_conv.o \
	$(d)common/color.o \
	$(d)common/file_mapping.o \
	$(d)common/format.o \
	$(d)common/fs.o \
	$(d)common/hotkey.o \
	$(d)common/io.o \
	$(d)common/json.o \
	$(d)common/kana_table.o \
	$(d)common/karaoke_matcher.o \
	$(d)common/keyframe.o \
	$(d)common/line_iterator.o \
	$(d)common/log.o \
	$(d)common/mru.o \
	$(d)common/option.o \
	$(d)common/option_value.o \
	$(d)common/path.o \
	$(d)common/thesaurus.o \
	$(d)common/util.o \
	$(d)common/vfr.o \
	$(d)common/ycbcr_conv.o

ifeq (yes, $(BUILD_DARWIN))
aegisub_OBJ += $(subst .mm,.o,$(sort $(wildcard $(d)osx/*.mm)))
else
aegisub_OBJ += $(d)common/dispatch.o
endif

aegisub_PCH := $(d)lagi_pre.h
aegisub_CPPFLAGS := -I$(d)include -I$(TOP) $(CPPFLAGS_BOOST) $(CFLAGS_LUA) $(CFLAGS_PTHREAD)

$(d)common/charset.o_FLAGS := $(CFLAGS_UCHARDET)
$(d)common/charset_conv.o_FLAGS := $(CFLAGS_ICONV)
$(d)common/parser.o_FLAGS := -ftemplate-depth=256
$(d)unix/path.o_FLAGS := -DP_DATA=\"$(P_DATA)\"

LIB += aegisub

include $(TOP)Makefile.target
