AC_PREREQ(2.57)

###################
# Required packages
###################
m4_define([boost_required_version], [1.50.0])
m4_define([curl_required_version], [7.18.2])
m4_define([ffms2_required_version], [2.16])
m4_define([fftw3_required_version], [3.3])
m4_define([fontconfig_required_version], [2.4])
m4_define([freetype_required_version], [9.7.0])
m4_define([hunspell_required_version], [1.2.0])
m4_define([icu_required_version], [4.8.1.1])
m4_define([libass_required_version], [0.9.7])
m4_define([libpulse_required_version], [0.5])
m4_define([pkgconfig_required_version], [0.20])
m4_define([portaudio_required_version], [19])
m4_define([wx_required_version], [3.0.0])

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

AS_CASE([$host],
        [*-*-darwin*], [build_darwin="yes"],
        [*-*-linux*],  [build_linux="yes"])

AC_SUBST(build_darwin)

########################
# Configurable variables
########################
# Name of aegisub binary.
# This is tagged with the version on release branches.
AEGISUB_COMMAND="aegisub"
AC_SUBST(AEGISUB_COMMAND)
AC_DEFINE_UNQUOTED([AEGISUB_COMMAND], ["${AEGISUB_COMMAND}"], [Name of the Aegisub executable])

# Name of gettext catalog.
AEGISUB_CATALOG="aegisub"
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
. $srcdir/build/version.sh "$srcdir"
AC_MSG_RESULT([$BUILD_GIT_VERSION_STRING $VERSION_SOURCE])

AC_MSG_CHECKING([for build date])
BUILD_DATE=`date "+%Y-%m-%d %H:%M %Z"`
AC_MSG_RESULT($BUILD_DATE)

AC_SUBST([BUILD_GIT_VERSION_STRING])
AC_SUBST([BUILD_DATE])

###################################################
# Check for pthreads and setup variables / compiler
###################################################
AS_IF([test x$build_darwin != xyes], [
  ACX_PTHREAD([], [AC_MSG_FAILURE([You must have working pthreads.])])
  CC="$PTHREAD_CC"
])

######################
# Check compiler flags
######################
AC_ARG_ENABLE(compiler-flags, AS_HELP_STRING([--disable-compiler-flags],[Disable *all* additional compiler flags. [no]]))

AS_IF([test x$enable_compiler_flags != xno], [
  CFLAGS="$CFLAGS -Wall -Wextra -Wno-unused-parameter -std=gnu99 -pipe -g"
  CXXFLAGS="$CXXFLAGS -Wall -Wextra -Wno-unused-parameter -fno-strict-aliasing -pipe -g"
  AC_CXX_FLAG([-std=c++11])
  AC_CXX_FLAG([-Wno-c++11-narrowing])
  AC_C_FLAG([-Wno-unused-local-typedefs])
  AC_CXX_FLAG([-Wno-unused-local-typedefs])

  # -O* messes with debugging.
  AS_IF([test x$enable_debug = xyes], [
    CFLAGS="$CFLAGS -O0"
    CXXFLAGS="$CXXFLAGS -O0"
  ], [
    CFLAGS="$CFLAGS -O3"
    CXXFLAGS="$CXXFLAGS -O3"
  ])
])

AC_ARG_WITH([libc++], AS_HELP_STRING([--with-libc++],[Use libc++ rather than libstdc++. [auto]]))
AS_IF([test x$with_libc__ != xno], [
  AC_LANG_PUSH(C++)
  ac_ld_flag_save="$LDFLAGS"
  LDFLAGS="$LDFLAGS -stdlib=libc++"
  AC_MSG_CHECKING([[whether $CXX supports -stdlib=libc++]])
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[]])], [
    AC_MSG_RESULT([yes])
    CXXFLAGS="$CXXFLAGS -stdlib=libc++"
  ], [
    AS_IF([test x$with_libc__ = xyes], [
      AC_MSG_FAILURE([no])
    ], [
      LDFLAGS="$ac_ld_flag_save"
      AC_MSG_RESULT([no])
    ])
  ])
  AC_LANG_POP(C++)
])

######################################
# Check Headers / Features / Libraries
######################################
AC_CHECK_LIB([m], [main])
AC_CHECK_LIB([dl], [dlclose])
AC_CHECK_HEADERS([sys/time.h])

AC_ARG_ENABLE(sanity-checks,
              AS_HELP_STRING([--disable-sanity-checks],[Skip verifying that found libraries work.]))

##############################
# Program Support and Features
##############################
#########################
### Required Dependencies
#########################

AS_IF([test x$build_darwin != xyes], [
  AC_PATH_XTRA
  AS_IF([test x$no_x = xyes],
        [AC_MSG_FAILURE([You must have a working copy of X installed.])])

  AX_CHECK_GL
  AS_IF([test x$no_gl = xyes], [AC_MSG_FAILURE([Aegisub requires GL support.])])
], [
  AC_CHECK_HEADERS([OpenGL/gl.h])
])
AC_SUBST(GL_CFLAGS)
AC_SUBST(GL_LIBS)

PKG_CHECK_MODULES(FREETYPE, freetype2 >= freetype_required_version)

AS_IF([test x$build_darwin != xyes], [
  PKG_CHECK_MODULES(FONTCONFIG, fontconfig >= fontconfig_required_version)
  FONTCONFIG_CONF_DIR=$($PKG_CONFIG --variable=confdir fontconfig)
  AC_SUBST(FONTCONFIG_CONF_DIR)
])

PKG_CHECK_MODULES(LIBASS, libass >= libass_required_version)

AX_BOOST_BASE([boost_required_version])
AX_BOOST_CHRONO
AX_BOOST_FILESYSTEM
AX_BOOST_LOCALE
AX_BOOST_REGEX
AX_BOOST_SYSTEM
AX_BOOST_THREAD

cppflags_bak="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $BOOST_CPPFLAGS"
AC_CHECK_HEADERS([boost/spirit/home/phoenix/version.hpp])
CPPFLAGS=$cppflags_bak

AC_DEFINE(BOOST_SYSTEM_NO_DEPRECATED, 1,
          [Disable deprecated parts of boost.system])
AC_DEFINE(BOOST_MULTI_INDEX_DISABLE_SERIALIZATION, 1,
          [Disable boost.serialization support for boost.multi_index])

PKG_CHECK_MODULES(ICU_UC, icu-uc >= icu_required_version)
PKG_CHECK_MODULES(ICU_I18N, icu-i18n >= icu_required_version)

########
## boost
########
AS_IF([test x$enable_sanity_checks != xno], [
  AC_AGI_COMPILE([boost ICU support], [boost_icu], [$BOOST_CPPFLAGS],
                 [$BOOST_LDFLAGS $BOOST_REGEX_LIB $ICU_UC_LIBS $ICU_I18N_LIBS],[
  #include <boost/regex/icu.hpp>
  int main() {
    auto regex = boost::make_u32regex(".", boost::u32regex::perl);
    boost::smatch result;
    u32regex_search("a", result, regex, boost::match_default);
  } ])

  AS_IF([test x$agi_cv_with_boost_icu = xno],
        [AC_MSG_FAILURE([Aegisub requires that boost be built with ICU support.])])
])

########
## iconv
########
AC_ARG_VAR([ICONV_CFLAGS], [CFLAGS to use for iconv []])
AC_ARG_VAR([ICONV_LIBS], [LIBS to use for iconv [-liconv]])

AS_IF([test -z "$ICONV_LIBS"], AC_SEARCH_LIBS([iconv_open], [iconv]))

AC_AGI_COMPILE([iconv], [iconv], [$ICONV_CFLAGS], [$ICONV_LIBS],[
#include <iconv.h>
int main() {
  return !iconv_open("UTF-8", "UTF-8");
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

AGI_OPT_PKG(fftw3, [build without fftw support [auto]], [Enable fftw support])
AGI_OPT_PKG(hunspell, [build without hunspell spell-checking [auto]], [Enable Hunspell support])
AGI_OPT_PKG(uchardet, [build without automatic character set detection [auto]], [Enable uchardet support])

########
# LuaJIT
########
system_luajit="bundled"
AC_ARG_WITH(system-luajit, AS_HELP_STRING([--without-system-luajit], [Use built-in LuaJIT [auto]]))
# Check if it's available at all
AS_IF([test x$with_system_luajit = xno], [],
      [PKG_CHECK_MODULES(LUAJIT, luajit >= 2.0.0, [], [
       AS_IF([test x$with_system_luajit = xyes],
             [AC_MSG_FAILURE([--with-sytem-luajit was specified, but luajit could not be found])])
       with_system_luajit="no"])])

# Check if it's compiled in Lua 5.2 mode
# This check can be skipped by manually setting agi_cv_luajit_is_52
AS_IF([test x$with_system_luajit != xno], [
  aegisub_save_LIBS="$LIBS"
  aegisub_save_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS $LUAJIT_CFLAGS"
  LIBS="$LIBS $LUAJIT_LIBS"
  AC_CACHE_CHECK(
    [whether LuaJIT is compiled in 5.2 mode], [agi_cv_luajit_is_52],
    [AC_RUN_IFELSE([AC_LANG_PROGRAM(
     [[#include <lauxlib.h>]],
     [[lua_State *L = luaL_newstate();
       if (!L) return 1;
       // This is valid in lua 5.2, but a syntax error in 5.1
       const char testprogram[] = "function foo() while true do break return end end";
       return luaL_loadstring(L, testprogram) == LUA_ERRSYNTAX;
     ]])], [agi_cv_luajit_is_52="yes"], [agi_cv_luajit_is_52="no"])])
  CPPFLAGS="$aegisub_save_CPPFLAGS"
  LIBS="$aegisub_save_LIBS"

  if test $agi_cv_luajit_is_52 != yes; then
    AS_IF([test x$with_system_luajit = xyes],
          [AC_MSG_FAILURE([LuaJIT must be compiled in Lua 5.2 mode])])
    with_system_luajit="no"
  else
    with_system_luajit="yes"
    system_luajit="system"
  fi
])

AS_IF([test $with_system_luajit = no],
      [AC_SUBST([LUAJIT_CFLAGS], ['-I$(TOP)vendor/luajit/include'])
       AC_SUBST([LUAJIT_LIBS], ['$(TOP)vendor/luajit/src/libluajit.a'])])

AC_SUBST(with_system_luajit)

# We also need a Lua binary to run part of the build system
# Which version doesn't matter as the scripts are portable between them
AC_PATH_PROGS([LUA], [luajit lua lua5.3 lua-5.3 lua5.2 lua-5.2 lua5.1 lua-5.1])

# If the user doesn't have an installed copy of Lua, just use the one built
# as part of building LuaJIT
AS_IF([test -z $LUA], [LUA='$(TOP)vendor/luajit/src/host/minilua'])

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

AS_IF([test x$enable_sanity_checks != xno], [
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
      [AS_IF([$WX_CONFIG_PATH --debug],[AC_CXX_FLAG([-D_DEBUG])])])

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
packages/desktop/aegisub.desktop.template
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
*    * http://github.com/FFMS/ffms2
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

Audio Players
  ALSA:                  $with_alsa $alsa_disabled
  OpenAL:                $with_openal $openal_disabled
  OSS:                   $with_oss $oss_disabled
  PortAudio:             $with_portaudio $portaudio_disabled
  PulseAudio:            $with_libpulse $libpulse_disabled

Misc Packages
  uchardet:              $with_uchardet $uchardet_disabled
  Hunspell:              $with_hunspell $hunspell_disabled
  FFTW3:                 $with_fftw3 $fftw3_disabled
  LuaJIT:                $system_luajit
])
