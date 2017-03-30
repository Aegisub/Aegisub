include $(dir $(lastword $(MAKEFILE_LIST)))../../header.mk
DESKTOP_SRC := $(d)

ICONS = $(sort $(wildcard $(d)*.png)) $(sort $(wildcard $(d)*.svg))
ICONS_INSTALLED = $(DESTDIR)$(P_ICON)/hicolor/%/apps/aegisub.

DESKTOP_FILE := $(d)aegisub.desktop
DESKTOP_FILE_PO := $(d)../../po
DESKTOP_FILE_INSTALLED = $(DESTDIR)$(P_DESKTOP)/$(notdir $(DESKTOP_FILE))

DISTCLEANFILES += $(DESKTOP_FILE)

%.desktop: %.desktop.template $(DESKTOP_FILE_PO)
	intltool-merge --quiet --desktop-style $(DESKTOP_FILE_PO) $< $@

$(ICONS_INSTALLED)png: $(d)%.png	; $(MKDIR_INSTALL)
$(ICONS_INSTALLED)svg: $(d)%.svg	; $(MKDIR_INSTALL)
$(DESKTOP_FILE_INSTALLED): $(DESKTOP_FILE)	; $(MKDIR_INSTALL)

ifneq (yes, $(BUILD_DARWIN))
install: \
	$(DESKTOP_FILE_INSTALLED) \
	$(patsubst %.png, $(ICONS_INSTALLED)png, $(patsubst %.svg, $(ICONS_INSTALLED)svg, $(notdir $(ICONS))))
endif

include $(TOP)Makefile.target
