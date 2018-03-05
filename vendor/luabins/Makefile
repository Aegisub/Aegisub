include $(dir $(lastword $(MAKEFILE_LIST)))../../header.mk

luabins_OBJ := $(patsubst %.c,%.o,$(sort $(wildcard $(d)src/*.c)))
luabins_CPPFLAGS := $(CFLAGS_LUA)

LIB += luabins

include $(TOP)Makefile.target
