include $(dir $(lastword $(MAKEFILE_LIST)))../header.mk

GTEST_ROOT ?= $(TOP)vendor/googletest
GTEST_FILE := ${GTEST_ROOT}/src/gtest-all

run_PCH := $(d)support/tests_pre.h
run_CPPFLAGS := -I$(TOP)libaegisub/include -I$(TOP) -I$(d)support \
	-I$(GTEST_ROOT) -I$(GTEST_ROOT)/include $(CPPFLAGS_BOOST) $(CFLAGS_LUA)
run_CXXFLAGS := -Wno-unused-value -Wno-sign-compare
run_LIBS := $(LIBS_BOOST) $(LIBS_ICU) $(LIBS_UCHARDET) $(LIBS_PTHREAD)
run_OBJ := \
	$(patsubst %.cpp,%.o,$(wildcard $(d)tests/*.cpp)) \
	$(d)support/main.o \
	$(d)support/util.o \
	$(TOP)lib/libaegisub.a \
	$(GTEST_FILE).o

# This bit of goofiness is to make it only try to build the tests if google
# test can be found and silently skip it if not, by using $(wildcard) to check
# for file existence
PROGRAM += $(subst $(GTEST_FILE).cc,$(d)run,$(wildcard $(GTEST_FILE).cc))

ifeq (yes, $(BUILD_DARWIN))
run_LIBS += -framework ApplicationServices -framework Foundation
endif

$(d)data: $(d)setup.sh
	cd $(TOP)tests; ./setup.sh

gtest_filter ?= *
test-libaegisub: $(d)run $(d)data
	cd $(TOP)tests; ./run --gtest_filter="$(gtest_filter)"

test: $(subst $(GTEST_FILE).cc,test-libaegisub,$(wildcard $(GTEST_FILE).cc))

include $(TOP)Makefile.target
