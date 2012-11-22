AC_PREREQ(2.57)

###################
# Required packages
###################
m4_define([boost_required_version], [1.49.0])
m4_define([curl_required_version], [7.18.2])
m4_define([ffms2_required_version], [2.16])
m4_define([fftw3_required_version], [3.3])
m4_define([fontconfig_required_version], [2.4])
m4_define([freetype_required_version], [9.7.0])
m4_define([hunspell_required_version], [1.2.0])
m4_define([libass_required_version], [0.9.7])
m4_define([libpulse_required_version], [0.5])
m4_define([lua_auto4_required_version], [5.1])
m4_define([pkgconfig_required_version], [0.20])
m4_define([portaudio_required_version], [19])
m4_define([wx_required_version], [2.9.3])

#######
# Setup
#######
AC_INIT([Aegisub], [aegisub_version],, [aegisub])
: ${CFLAGS=""}
: ${CXXFLAGS=""}
AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_HEADER([acconf.h])
AC_GNU_SOURCE
AC_CANONICAL_HOST

###########################
# Check host architecture
###########################
build_darwin="no"
build_linux="no"
build_bsd="no"
build_default="no"

AS_CASE([$host],
        [*-*-darwin*], [build_darwin="yes"],
        [*-*-linux*],  [build_linux="yes"],
        [*-*-*bsd*],   [build_bsd="yes"],
        [build_default="yes"])

# Used for universalchardet.
AC_AGI_MDCPUCFG($host)

AC_SUBST(build_bsd)
AC_SUBST(build_linux)
AC_SUBST(build_darwin)
AC_SUBST(build_default)

# Handle endianess.
AC_C_BIGENDIAN(
  AC_DEFINE([HAVE_BIG_ENDIAN], [1], [Big Endian]),
  AC_DEFINE([HAVE_LITTLE_ENDIAN], [1], [Little Endian]),
  AC_DEFINE([HAVE_DYNAMIC_ENDIAN], [1], [Unknown endian]),
  AC_DEFINE([HAVE_UNIVERSAL_ENDIAN], [1], [Universal Endian (MAC Universal PPC/Intel Binaries, uses HAVE_DYNAMIC_ENDIAN)])
)

########################
# Configurable variables
########################
# Name of aegisub binary.
# On OS X everything goes in an app bundle so no point in versioning the executable
AS_IF([test x$build_darwin = xyes],
      [AEGISUB_COMMAND="aegisub"],
      [AEGISUB_COMMAND="aegisub-3.1"])
AC_SUBST(AEGISUB_COMMAND)
AC_DEFINE_UNQUOTED([AEGISUB_COMMAND], ["${AEGISUB_COMMAND}"], [Name of the Aegisub executable])

# Name of gettext catalog.
# On OS X everything goes in an app bundle so no point in versioning the executable
AS_IF([test x$build_darwin = xyes],
      [AEGISUB_CATALOG="aegisub"],
      [AEGISUB_CATALOG="aegisub-31"])
AC_SUBST(AEGISUB_CATALOG)
AC_DEFINE_UNQUOTED([AEGISUB_CATALOG], ["${AEGISUB_CATALOG}"], [Name of the Aegisub gettext catalog])

# Handle location of desktop files: http://freedesktop.org/wiki/Specifications/desktop-entry-spec
AC_ARG_WITH(desktop-dir,
            AS_HELP_STRING([--with-desktop-dir=PATH],[desktop file locations [PREFIX/share/applications]]))

P_DESKTOP=${with_desktop_dir:-$datarootdir/applications}
AC_SUBST(P_DESKTOP)

# Handle location of icons: http://standards.freedesktop.org/icon-theme-spec/icon-theme-spec-latest.html
AC_ARG_WITH(icon-dir,
            AS_HELP_STRING([--with-icon-dir=PATH],[icon locations [PREFIX/share/icons]]))

P_ICON=${with_icon_dir:-$datarootdir/icons}
AC_SUBST(P_ICON)

# Install prefix
# If a user doesn't supply --prefix then it is set to NONE so we
# set it to $ac_default_prefix if it hasn't been supplied.
AS_CASE([x$prefix], [xNONE | x], [prefix="$ac_default_prefix"], [])

# Install prefix used by wxStandardPaths::SetInstallPrefix.
AC_DEFINE_UNQUOTED([INSTALL_PREFIX], ["$prefix"], [Default install prefix, or --prefix.])

# Build credit
AC_ARG_WITH(build-credit,
            AS_HELP_STRING([--with-build-credit=NAME],[Build credit shown in the program title.]),
            [use_build_credit="yes"])

AC_MSG_CHECKING([whether BUILD_CREDIT has been set])
AS_IF([test x$use_build_credit = xyes],
  AS_IF([test x$with_build_credit = xyes || test -z "$with_build_credit"], [
    AC_MSG_FAILURE([You must set a value eg --with-build-credit=<username>])
  ], [
    AC_MSG_RESULT([yes ($with_build_credit)])
    AC_DEFINE_UNQUOTED([BUILD_CREDIT], ["$with_build_credit"], [Build credit supplied in application title using --with-build-credit=])
  ]),
  [AC_MSG_RESULT([no])])

####################
# Check for programs
####################
CHECK_GNU_MAKE
AC_PROG_CC
AC_PROG_CXX
AC_LANG(C++)
AM_PROG_CC_C_O
# This is reported to be required on Ubuntu, I haven't had time to check.
define([AC_LIBTOOL_LANG_F77_CONFIG], [:])
AC_PROG_INSTALL
AC_PROG_RANLIB
PKG_PROG_PKG_CONFIG([pkgconfig_required_version])

#################
# Developers only
#################
AC_MSG_CHECKING([for version])
. $srcdir/build/version.sh "$srcdir/.."
AC_MSG_RESULT([$BUILD_GIT_VERSION_STRING $VERSION_SOURCE])

AC_MSG_CHECKING([for build date])
BUILD_DATE=`date "+%Y-%m-%d %H:%M %Z"`
AC_MSG_RESULT($BUILD_DATE)

AC_SUBST([BUILD_GIT_VERSION_STRING])
AC_SUBST([BUILD_DATE])

###################################################
# Check for pthreads and setup variables / compiler
###################################################
ACX_PTHREAD([], [AC_MSG_FAILURE([You must have working pthreads.])])
CC="$PTHREAD_CC"

######################
# Check compiler flags
######################
AC_ARG_ENABLE(compiler-flags, AS_HELP_STRING([--disable-compiler-flags],[Disable *all* additional compiler flags. [no]]))

AS_IF([test x$enable_compiler_flags != xno], [
  AC_C_FLAG([-Wall])
  AC_C_FLAG([-Wextra],[AC_C_FLAG([-W])])
  AC_C_FLAG([-Wno-unused-parameter])
  AC_C_FLAG([-std=gnu99])
  AC_C_FLAG([-pipe])
  AC_C_FLAG([-g])
  AC_CXX_FLAG([-Wall])
  AC_CXX_FLAG([-Wextra],[AC_CXX_FLAG([-W])])
  AC_CXX_FLAG([-Wno-unused-parameter])
  AC_CXX_FLAG([-Wno-long-long])
  AC_CXX_FLAG([-fno-strict-aliasing])
  AC_CXX_FLAG([-pipe])
  AC_CXX_FLAG([-g])
  AC_CXX_FLAG([-std=c++11])
  AC_LD_FLAG([-stdlib=libc++])

  # -O* messes with debugging.
  AS_IF([test x$enable_debug = xyes], [
    AC_C_FLAG([-O0])
    AC_CXX_FLAG([-O0])
  ], [
    AC_C_FLAG([-O3])
    AC_CXX_FLAG([-O3])
  ])
])

######################################
# Check Headers / Features / Libraries
######################################
AC_HEADER_STDC
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE
AC_CHECK_LIB([m], [main])

AC_AGI_COMPILE([pragma once], [pragma_once], [], [],[
#pragma once
int main(void) {}
])

AS_IF([test x$agi_cv_pragma_once = xno],
      [AC_MSG_FAILURE([Aegisub requires #pragma once to work correctly.])])

AC_CHECK_HEADER([wchar.h],,[AC_MSG_FAILURE([aegisub requires wide character support])])

AC_CHECK_HEADERS([sys/time.h])

##############################
# Program Support and Features
##############################
#########################
### Required Dependencies
#########################

AC_PATH_XTRA
AS_IF([test x$no_x = xyes && test x$build_darwin != xyes],
      [AC_MSG_FAILURE([You must have a working copy of X installed.])])

AX_CHECK_GL
AS_IF([test x$no_gl = xyes], [AC_MSG_FAILURE([Aegisub requires GL support.])])
AC_SUBST(GL_CFLAGS)
AC_SUBST(GL_LIBS)

PKG_CHECK_MODULES(FREETYPE, freetype2 >= freetype_required_version,
                  AC_DEFINE(WITH_FREETYPE2, 1, [Enable FreeType 2 support.]),
                  [AC_MSG_FAILURE([Aegisub requires >= FreeType2 freetype_required_version])])

PKG_CHECK_MODULES(FONTCONFIG, fontconfig >= fontconfig_required_version,
                  [AC_DEFINE(WITH_FONTCONFIG, 1, [Enable FontConfig support.])],
                  [AC_MSG_FAILURE([Aegisub requires fontconfig >= fontconfig_required_version])])

BOOST_REQUIRE([boost_required_version])

########
## iconv
########
AC_ARG_VAR([ICONV_CFLAGS], [CFLAGS to use for iconv []])
AC_ARG_VAR([ICONV_LIBS], [LIBS to use for iconv [-liconv]])

AS_IF([test -z "$ICONV_LIBS"], AC_SEARCH_LIBS([iconv_open], [iconv]))

AC_AGI_COMPILE([iconv], [iconv], [$ICONV_CFLAGS], [$ICONV_LIBS],[
#include <iconv.h>
int main(void) {
  iconv_t ic;
  ic = iconv_open ("UTF-8", "UTF-8");
  if (!ic) return 1;
  return 0;
} ])

AC_SUBST(ICONV_LIBS)
AC_SUBST(ICONV_CFLAGS)

AS_IF([test x$agi_cv_with_iconv = xno], [AC_MSG_FAILURE([Please install a working iconv library.])])

AC_AGI_COMPILE([iconv (const)], [iconv_const], [$ICONV_CFLAGS], [$ICONV_LIBS],[
#include <iconv.h>
int main(void) {
  iconv_t cd = iconv_open("UTF-16", "UTF-8");
  const char *in = "in";
  char *out = new char();
  size_t res, inbytesleft, outbytesleft;
  res = iconv(cd, &in, &inbytesleft, &out, &outbytesleft);
  return 0;
} ])

AS_IF([test x$agi_cv_with_iconv_const = xyes],
      [AC_DEFINE(AGI_ICONV_CONST, 1, [Enable if iconv expects the in argument to be const])])

###############
# Audio Players
###############
#######
## ALSA
#######
AC_ARG_WITH(alsa,
            AS_HELP_STRING([--without-alsa],
                           [build without ALSA audio player [auto]]))

AC_ARG_VAR([ALSA_CFLAGS], [CFLAGS to use for ALSA []])
AC_ARG_VAR([ALSA_LIBS], [LIBS to use for ALSA [-lasound -lrt]])

ALSA_LIBS=${ALSA_LIBS:--lasound -lrt}

AS_IF([test x$with_alsa = xno],
      [alsa_disabled="(disabled)"],
      [AC_CHECK_LIB([asound], [snd_pcm_open], [with_alsa="yes"], [with_alsa="no"], $ALSA_LIBS)])

AS_IF([test x$with_alsa = xyes], AC_DEFINE(WITH_ALSA, 1, [Enable ALSA Support]))

AC_SUBST(ALSA_CFLAGS)
AC_SUBST(ALSA_LIBS)
AC_SUBST(with_alsa)

#########
## OpenAL
#########
AC_ARG_WITH(openal,
            AS_HELP_STRING([--without-openal],
                           [build without OpenAL audio player [auto]]))

AS_IF([test x$build_darwin = xyes], [
  OPENAL_LIBS="-framework OpenAL"
  OPENAL_CFLAGS=""
  AC_SUBST(OPENAL_LIBS)
  AC_SUBST(OPENAL_CFLAGS)
  with_openal="yes"
],
  [test x$with_openal = xno], [openal_disabled="(disabled)"],
  [PKG_CHECK_MODULES(OPENAL, openal >= 0.0.8, [with_openal=yes], [with_openal=no])])

AS_IF([test x$with_openal != xno], [
  AC_AGI_COMPILE([OpenAL], [openal], [$OPENAL_CFLAGS], [$OPENAL_LIBS],[
#if defined(__APPLE__)
#include <OpenAL/AL.h>
#include <OpenAL/ALC.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif
int main(void) {
  ALCdevice *device = alcOpenDevice(0);
  if (!device) return 1;
  ALCcontext *context = alcCreateContext(device, 0);
  if (!context) return 1;
  return 0;
} ])
])

AS_IF([test x$agi_cv_with_openal = xno && test x$with_openal = xyes], [
  AC_MSG_WARN([OpenAL detected, but it doesn''t work...])
  with_openal="no"
])

AS_IF([test x$agi_cv_with_openal = xyes && test x$with_openal = xyes],
      AC_DEFINE(WITH_OPENAL, 1, [Enable OpenAL support]))

AC_SUBST(with_openal)

######
## OSS
######
AC_ARG_VAR([OSS_CFLAGS], [CFLAGS to use for OSS [auto]])
AC_ARG_WITH(oss,
            AS_HELP_STRING([--without-oss],
                           [build without OSS audio player [auto]]))

AS_IF([test x$with_oss = xno], [oss_disabled="(disabled)"], [
  AS_IF([test -z "$OSS_CFLAGS" && test -f "/etc/oss.conf"], [
    . /etc/oss.conf
    OSS_CFLAGS="-I${OSSLIBDIR}/include/sys"
  ])
  AC_CHECK_HEADERS([soundcard.h sys/soundcard.h], [with_oss="yes"], [with_oss="no"])
])

AS_IF([test x$with_oss = xyes], AC_DEFINE(WITH_OSS, 1, [Enable OSS support]))

AC_SUBST(with_oss)

###################
# Simple pkg-config stuff
###################
AGI_OPT_PKG(libpulse, [build without PulseAudio audio player [auto]], [Enable PulseAudio support])
AGI_OPT_PKG(portaudio-2.0, [build without PortAudio v19 audio player [auto]], [Enable PortAudio v19 support])

AGI_OPT_PKG(ffms2, [build without ffms2 A/V provider [auto]], [Enable FFMS2 support])

AGI_OPT_PKG(libass, [build without libass subtitle provider [auto]], [Enable libass support])

AGI_OPT_PKG(fftw3, [build without fftw support [auto]], [Enable fftw support])
AGI_OPT_PKG(hunspell, [build without hunspell spell-checking [auto]], [Enable Hunspell support])

############
# Automation
############
AC_ARG_WITH(lua,
            AS_HELP_STRING([--without-lua],
                           [build without lua 5.1 (auto4)]))

AS_IF([test x$with_lua = xno], [$with_auto4_lua="no"; lua_disabled="(disabled)"], [
  # First try pkg-config with several names
  PKG_CHECK_MODULES(LUA, lua     >= 5.1, [with_auto4_lua="yes"], [
  PKG_CHECK_MODULES(LUA, lua5.1  >= 5.1, [with_auto4_lua="yes"], [
  PKG_CHECK_MODULES(LUA, lua-5.1 >= 5.1, [with_auto4_lua="yes"], [
    # Still not found so try to autodetect
    AS_IF([test -z "$LUA_CFLAGS"],
      [AGI_FIND_HEADER([LUA], [lua.hpp],
                       [/usr/include/lua5.1 /usr/include/lua51 /usr/local/include/lua51 /usr/local/include/lua5.1 /usr/include /usr/local/include])])

    AS_IF([test -z "$LUA_LIBS"],
      [AGI_FIND_LIB([LUA], [lua51 lua-5.1 lua5.1 lua], [])])

    AC_AGI_COMPILE([Lua 5.1 (auto4)], [lua], [$LUA_CFLAGS], [$LUA_LIBS],[
#include <lua.hpp>

int main () {
  lua_State *L = luaL_newstate();
  lua_pushstring(L, "test");
  int res = lua_objlen(L, -1) == 4;
  lua_close(L);
  return !res;
}])
    AS_IF([test x$agi_cv_with_lua = xno && test x$with_lua = xyes],
          [AC_MSG_FAILURE([--with-lua was specified, but lua could not be found. Try setting LUA_CLFAGS and LUA_LIBS manually])])
    with_auto4_lua=$agi_cv_with_lua
  ])])])
])

AS_IF([test x$with_auto4_lua = xyes], AC_DEFINE(WITH_AUTO4_LUA, 1, [Enable Automation (auto4), requires lua]))

AC_SUBST(with_auto4_lua)
AC_SUBST(LUA_CFLAGS)
AC_SUBST(LUA_LIBS)

######################################################
# Debugging support
######################################################
AC_MSG_CHECKING([whether to turn on debugging])
AC_ARG_ENABLE(debug, AS_HELP_STRING([--enable-debug],[enable debugging [no]]))
AC_MSG_RESULT([${enable_debug:=no}])

AC_MSG_CHECKING([whether to use exception handling in debug mode])
AC_ARG_ENABLE(debug-exceptions, AS_HELP_STRING([--enable-debug-exceptions],[enable exception handling in debug mode [no]]))
AC_MSG_RESULT([${enabled_debug_exceptions:=no}])

AS_IF([test x$enable_debug_exceptions = xyes],
      [AC_DEFINE(WITH_EXCEPTIONS, 1,
       [Enable exception handling in debug mode. (--enable-debug)  This is always enabled when debug mode is off.])])

################
# Widget support
################
WX_CONFIG_OPTIONS
WX_STANDARD_OPTIONS([debug])
WX_DEBUG=$DEBUG
WX_UNICODE=$UNICODE
WX_CONFIG_CHECK([wx_required_version],,,[std,gl,stc],[$WXCONFIG_FLAGS])
AC_SUBST(WX_CONFIG_PATH)

AS_IF([test x$WX_VERSION = x],
  [AC_MSG_FAILURE([wxWidgets detection failed, please set --with-wx* or add the libraries to your LIBS, CXX/CFLAGS.])])

AC_ARG_ENABLE(slow-wx-checks,
              AS_HELP_STRING([--disable-slow-wx-checks],[Disable slow-running checks for wx components.]))

AS_IF([test x$enable_slow_wx_checks != xno], [
  AC_AGI_LINK([wxWidgets OpenGL support], [wxopengl], [wx/glcanvas.h], [$GL_CFLAGS $WX_CFLAGS], [$GL_LIBS $WX_LIBS],[
#include <wx/glcanvas.h>
int main(void) {
  wxGLCanvas *canvas;
  wxGLContext *context;
  return 0;
} ])
  AC_AGI_LINK([wxWidgets StyledTextCtrl support], [wxstc], [wx/stc/stc.h], [$WX_CFLAGS], [$WX_LIBS],[
#include <wx/stc/stc.h>
int main(void) {
  wxStyledTextCtrl *canvas;
  return 0;
} ])
])

AS_IF([test x$with_agi_cv_wxopengl = xno], AC_MSG_FAILURE([wxWidgets OpenGL support missing]))
AS_IF([test x$with_agi_cv_wxstc = xno], AC_MSG_FAILURE([wxWidgets StyledTextCtrl support missing]))

# This turns on some internal (to aegisub) debugging features.
# A debug version of wxWidgets is required.
AS_IF([test x$enable_debug = xyes],
      [PACKAGE_DEBUG="-debug"
       AS_IF([$WX_CONFIG_PATH --debug],[AC_CXX_FLAG([-D_DEBUG])])])

AC_SUBST(PACKAGE_DEBUG)

############################
# Precompiled Header Support
# Only works with gcc! (and clang)
############################
AC_MSG_CHECKING([whether to use precompiled headers])
AC_ARG_ENABLE(gcc-prec, AS_HELP_STRING([--disable-gcc-prec],[disable GCC precompiled headers]))
AC_MSG_RESULT([${enable_gcc_prec:=yes}])
AC_SUBST(enable_gcc_prec)
AC_PCH_FLAG([-Winvalid-pch])
AC_PCH_FLAG([-fpch-deps])
AC_PCH_FLAG([-fpch-preprocess])

###########
# Profiling
###########
AC_ARG_VAR([PROFILE_FLAGS], [Profile flag(s) to use with --enable-profile [-pg]])

AC_MSG_CHECKING([whether to turn on profiling])
AC_ARG_ENABLE(profile, AS_HELP_STRING([--enable-profile],[enable profiling [no]]))
AS_IF([test x$enable_profile = xyes], [
  PROFILE_FLAGS=${PROFILE_FLAGS:--pg}
  AC_MSG_RESULT([yes ($PROFILE_FLAGS)])
  CFLAGS="$PROFILE_FLAGS $CFLAGS"
], [
  enable_profile="no"
  AC_MSG_RESULT([no])
])


######
# gcov
######
AC_ARG_VAR([GCOV_CFLAGS], [gcov cflag(s) to use with --enable-gcov [-ftest-coverage -fprofile-arcs]])
AC_ARG_VAR([GCOV_LIBS], [gcov ldflag(s) to use with --enable-gcov [-lgcov]])

AC_MSG_CHECKING([whether to turn on gcov])
AC_ARG_ENABLE(gcov, AS_HELP_STRING([--enable-gcov],[enable gcov [no]]))

AS_IF([test x$enable_gcov = xyes], [
  GCOV_CFLAGS=${GCOV_CLFAGS:--fprofile-arcs -ftest-coverage}
  GCOV_LIBS=${GCOV_LIBS:--lgcov}

  AC_MSG_RESULT([yes ($GCOV_CFLAGS) ($GCOV_LIBS)])
  CFLAGS="$CFLAGS $GCOV_CFLAGS"
  LIBS="$LIBS $GCOV_LIBS"
], [
  enable_gcov="no"
  AC_MSG_RESULT([no])
])

################
# Electric Fence
################
AC_ARG_VAR([EFENCE_LIBS], [Electric Fence ldflag(s) to use with --enable-profile [-lefence]])

AC_MSG_CHECKING([whether to turn on Electric Fence])
AC_ARG_ENABLE(efence, AS_HELP_STRING([--enable-efence],[enable Electric Fence [no]]))
AS_IF([test x$enable_efence = xyes], [
  EFENCE_LIBS=${EFENCE_LIBS:--lefence}
  AC_MSG_RESULT([yes ($EFENCE_LIBS)])
], [
  enable_efence="no"
  AC_MSG_RESULT([no])
])
AC_SUBST(EFENCE_LIBS)

##########
# ccmalloc
##########
AC_ARG_VAR([CCMALLOC_LIBS], [ccmalloc libs(s) to use with --enable-ccmalloc [$prefix/lib/ccmalloc-c++.o]])
AC_ARG_VAR([CCMALLOC_LDFLAGS], [ccmalloc ldflag(s) to use with --enable-ccmalloc [-lccmalloc]])

AC_MSG_CHECKING([whether to turn on ccmalloc])
AC_ARG_ENABLE(ccmalloc, AS_HELP_STRING([--enable-ccmalloc],[enable ccmalloc [no]]))
AS_IF([test x$enable_ccmalloc = xyes], [
  CCMALLOC_LIBS=${CCMALLOC_LIBS:-$prefix/lib/ccmalloc-c++.o}
  CCMALLOC_LDFLAGS=${CCMALLOC_LDFLAGS:--lccmalloc}
  AC_MSG_RESULT([yes ($CCMALLOC_LIBS $CCMALLOC_LDFLAGS)])
], [
  enable_ccmalloc="no"
  AC_MSG_RESULT([no])
])
AC_SUBST(CCMALLOC_LDFLAGS)
AC_SUBST(CCMALLOC_LIBS)

AS_IF([test x$enable_efence = xyes && test x$enable_ccmalloc = xyes],
      [AC_MSG_FAILURE([Please enable Electric Fence _or_ ccmalloc otherwise strange things will happen.])])

########
# ccache
########
AC_MSG_CHECKING([whether to enable ccache])
AC_ARG_ENABLE(ccache, AS_HELP_STRING([--enable-ccache],[enable ccache [no]]))
AC_MSG_RESULT(${enable_ccache:=no})
AC_SUBST(enable_ccache)

##############################
# Internationalisation support
##############################
AM_GNU_GETTEXT([external])

################
# Update checker
################
AC_MSG_CHECKING([whether to enable the update checker])
AC_ARG_ENABLE(update-checker,
              AS_HELP_STRING([--disable-update-checker], [disable the update checker [no]]))
AC_MSG_RESULT(${enable_update_checker:=yes})
AS_IF([test "x$enable_update_checker" != "xno"],
      [AC_DEFINE([WITH_UPDATE_CHECKER], [],
                 [Whether to enable the update checker])])

AC_MSG_CHECKING([for update checker server])
AC_ARG_WITH(update-server,
            AS_HELP_STRING([--with-update-server=HOSTNAME],
                           [Server to use for the update checker
                            [updates.aegisub.org]]))
AC_MSG_RESULT(${with_update_server:=updates.aegisub.org})
AC_DEFINE_UNQUOTED([UPDATE_CHECKER_SERVER], ["$with_update_server"],
                   [Server for the update checker])

AC_MSG_CHECKING([for update checker base URL])
AC_ARG_WITH(update-url,
            AS_HELP_STRING([--with-update-url=HOSTNAME],
                           [Base path to use for the update checker [/trunk]]))
AC_MSG_RESULT(${with_update_url:=/trunk})
AC_DEFINE_UNQUOTED([UPDATE_CHECKER_BASE_URL], ["$with_update_url"],
                   [Base path for the update checker])

################
# Crash reporter
################
AC_MSG_CHECKING([whether to enable the crash reporter])
AC_ARG_ENABLE(crash-reporter,
              AS_HELP_STRING([--enable-crash-reporter], [enable the crash reporter [no]]))
AC_MSG_RESULT(${enable_crash_reporter:=no})
AS_IF([test "x$enable_crash_reporter" = "xyes"],
      [PKG_CHECK_MODULES(LIBCURL, libcurl >= curl_required_version,,
        [AC_MSG_FAILURE([the crash repoter requires >= cURL curl_required_version])])])
ENABLE_REPORTER="$enable_crash_reporter"
AC_SUBST(ENABLE_REPORTER)

####################################################################
# Default settings for Providers/Players
# * This is done at the end to preserve sanity rather than littering
#   it above.
####################################################################
AC_ARG_WITH(player-audio,
            AS_HELP_STRING([--with-player-audio=(ALSA|OpenAL|PortAudio|PulseAudio|OSS)],
                           [Default Audio Player [Linux/ALSA, Darwin/OpenAL, 1:*/OSS, 2:*/PortAudio]]))

# Default audio player.
AS_IF([test -z "$with_player_audio"], [
  AS_IF([test x$build_linux = xyes && test x$with_alsa = xyes],    [DEFAULT_PLAYER_AUDIO="ALSA"],
        [test x$build_darwin = xyes && test x$with_openal = xyes], [DEFAULT_PLAYER_AUDIO="OpenAL"],
        [test x$with_portaudio = xyes],                            [DEFAULT_PLAYER_AUDIO="PortAudio"],
        [test x$with_oss = xyes],                                  [DEFAULT_PLAYER_AUDIO="OSS"],
        [test x$with_alsa = xyes],                                 [DEFAULT_PLAYER_AUDIO="ALSA"],
        [test x$with_openal = xyes],                               [DEFAULT_PLAYER_AUDIO="OpenAL"],
        [test x$with_libpulse = xyes],                             [DEFAULT_PLAYER_AUDIO="PulseAudio"])],
  [DEFAULT_PLAYER_AUDIO="$with_player_audio"])
AC_SUBST(DEFAULT_PLAYER_AUDIO)

# Set some friendly strings if some of the above aren't detected.
DEFAULT_PLAYER_AUDIO=${DEFAULT_PLAYER_AUDIO:-NONE}

###############
# Misc settings
###############
# Files that need substitution.
AC_CONFIG_FILES([
desktop/aegisub.desktop
src/libresrc/default_config_platform.json
tools/osx-bundle.sed
Makefile.inc
])

AC_OUTPUT

AS_IF([test x"$DEFAULT_PLAYER_AUDIO" = xNONE], AC_MSG_NOTICE([

***********************************************************************
* No supported audio player interface was found on your system.
* If you want audio support in Aegisub you need to install one of
* these libraries:
*  - PulseAudio
*    * http://pulseaudio.org/
*  - ALSA (Linux only)
*    * http://www.alsa-project.org/
*  - PortAudio (version 19 only)
*    * http://www.portaudio.com/
***********************************************************************
]))

AS_IF([test x$with_ffms2 != xyes], [AC_MSG_NOTICE([

***********************************************************************
* No supported video/audio reader interface was found on your system.
* You will not be able to open any video or audio files in Aegisub
* unless you install a supported video/audio provider.
* You will however still be able to open "dummy" video, ie. a blank,
* virtual video clip with subtitles overlaid.
* Currently we only support one video/audio provider on non-Windows
* systems:
*  - FFMS2
*    * http://code.google.com/p/ffmpegsource/
***********************************************************************
])])

AC_MSG_RESULT([
Configure settings
  Install prefix:        $prefix
  Revision:              $BUILD_GIT_VERSION_STRING
  Debug                  $enable_debug
  CFLAGS                 $CFLAGS
  CXXFLAGS               $CXXFLAGS
  CPPFLAGS               $CPPFLAGS
  LDFLAGS                $LDFLAGS
  LIBS                   $LIBS

Default Settings
  Audio Player:          $DEFAULT_PLAYER_AUDIO

Scripting Engines
  auto4 Lua:             $with_auto4_lua $lua51_disabled

Audio Players
  ALSA:                  $with_alsa $alsa_disabled
  OpenAL:                $with_openal $openal_disabled
  OSS:                   $with_oss $oss_disabled
  PortAudio:             $with_portaudio $portaudio_disabled
  PulseAudio:            $with_libpulse $libpulse_disabled

A/V Providers
  FFMS2:                 $with_ffms2 $ffms2_disabled

Subtitle Providers:
  libass                 $with_libass $libass_disabled

Misc Packages
  Hunspell:              $with_hunspell $hunspell_disabled
  FFTW3:                 $with_fftw3 $fftw3_disabled
])
