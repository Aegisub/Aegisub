d := $(abspath $(dir $(lastword $(filter-out $(lastword $(MAKEFILE_LIST)),$(MAKEFILE_LIST)))))/

ifndef TOP
TOP := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))/
include $(TOP)Makefile.inc

subdirs := \
	automation \
	libaegisub \
	packages/desktop \
	po \
	src \
	tests \
	tools \
	vendor/luabins

ifeq (no, $(SYSTEM_LUAJIT))
subdirs += vendor/luajit
endif

subdirs := $(addprefix $(TOP),$(addsuffix /Makefile,$(subdirs)))

INCLUDING_CHILD_MAKEFILES=yes
d_save := $d
$(foreach dir,$(filter-out $(abspath $(MAKEFILE_LIST)),$(subdirs)), $(eval include $(dir)))
d := $(d_save)
INCLUDING_CHILD_MAKEFILES=no

DISTCLEANFILES += \
	$(TOP)acconf.h \
	$(TOP)configure \
	$(TOP)acconf.h.in~ \
	$(TOP)build/git_version.h \
	$(TOP)Makefile.inc \
	$(TOP)config.log \
	$(TOP)acconf.h.in \
	$(TOP)config.status \
	$(TOP)autom4te.cache \
	$(TOP)aclocal.m4 \

define MKDIR_INSTALL
@$(BIN_MKDIR_P) $(dir $@)
$(BIN_INSTALL) -m644 $< $@
endef

endif
