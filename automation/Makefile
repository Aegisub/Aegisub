include $(dir $(lastword $(MAKEFILE_LIST)))../header.mk

DATA_AUTOMATION := \
	$(sort $(wildcard $(d)autoload/*)) \
	$(sort $(wildcard $(d)demos/*)) \
	$(sort $(wildcard $(d)include/*.lua)) \
	$(sort $(wildcard $(d)include/aegisub/*))

DATA_AUTOMATION_REL := $(subst $(d),,$(DATA_AUTOMATION))
DATA_AUTOMATION_INSTALLED = $(addprefix $(DESTDIR)$(P_DATA)/automation/, $(DATA_AUTOMATION_REL))

$(DESTDIR)$(P_DATA)/automation/%: $(d)%
	$(MKDIR_INSTALL)

aegisub-lua_OBJ := $(d)tests/aegisub.o $(TOP)lib/libaegisub.a $(TOP)lib/libluabins.a $(LIBS_LUA)
aegisub-lua_CPPFLAGS := $(CPPFLAGS_BOOST) $(CFLAGS_LUA) -I$(TOP)libaegisub/include -I$(TOP)src $(CXXFLAGS_WX)
aegisub-lua_LIBS := $(LIBS_WX) $(LIBS_BOOST) $(LIBS_ICU) $(LIBS_PTHREAD)

PROGRAM += $(d)aegisub-lua

test-automation: $(PROGRAM)
	eval `luarocks path`; cd $(TOP)automation; ./aegisub-lua tests/busted.lua -p 'moon' tests/modules

test: test-automation

install: $(DATA_AUTOMATION_INSTALLED)

include $(TOP)Makefile.target
