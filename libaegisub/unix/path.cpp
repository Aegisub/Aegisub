// Copyright (c) 2013, Thomas Goyne <plorkyeran@aegisub.org>
//
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//
// Aegisub Project http://www.aegisub.org/

#include <libaegisub/path.h>

#include <libaegisub/exception.h>
#include <libaegisub/util_osx.h>

#include <boost/filesystem/operations.hpp>
#include <pwd.h>

namespace {
#ifndef __APPLE__
std::string home_dir() {
	const char *env = getenv("HOME");
	if (env && *env) return env;

	if ((env = getenv("USER")) || (env = getenv("LOGNAME"))) {
		if (passwd *user_info = getpwnam(env)) {
			const char *pw_dir = user_info->pw_dir;
			if (pw_dir && *pw_dir)
				return user_info->pw_dir;
		}
	}

	throw agi::EnvironmentError("Could not get home directory. Make sure HOME is set.");
}

std::string xdg_config_dir() {
	const char *env = getenv("XDG_CONFIG_HOME");
	if (env && *env) return env;

	agi::fs::path home = home_dir();
	return (home/".config").string();
}

std::string xdg_data_dir() {
	const char *env = getenv("XDG_DATA_HOME");
	if (env && *env) return env;

	agi::fs::path home = home_dir();
	return (home/".local/share").string();
}

std::string xdg_cache_dir() {
	const char *env = getenv("XDG_CACHE_HOME");
	if (env && *env) return env;

	agi::fs::path home = home_dir();
	return (home/".cache").string();
}
#endif
}

namespace agi {
void Path::FillPlatformSpecificPaths() {
#ifndef __APPLE__
	std::string dir = "aegisub";
#if defined(XDG_DIRS) && XDG_DIRS >= 1
	agi::fs::path xdg_config = xdg_config_dir();
	SetToken("?user", xdg_config/dir);
#if XDG_DIRS >= 2
	agi::fs::path xdg_data = xdg_data_dir();
	SetToken("?local", xdg_data/dir);
#else
	SetToken("?local", xdg_config/dir);
#endif
#else
    dir = "." + dir;
	agi::fs::path home = home_dir();
	SetToken("?user", home/dir);
	SetToken("?local", home/dir);
#endif
	SetToken("?data", P_DATA);
#ifdef P_DICTIONARY
	SetToken("?dictionary", P_DICTIONARY);
#else
	SetToken("?dictionary", "/usr/share/hunspell");
#endif
#else
	std::string dir = "Aegisub";
	agi::fs::path app_support = agi::util::GetApplicationSupportDirectory();
	SetToken("?user", app_support/dir);
	SetToken("?local", app_support/dir);
	agi::fs::path shared_support = agi::util::GetBundleSharedSupportDirectory();
	SetToken("?data", shared_support);
	SetToken("?dictionary", shared_support/"dictionaries");
#endif
#if !defined(__APPLE__) && defined(XDG_DIRS) && XDG_DIRS >= 3
	agi::fs::path xdg_cache = xdg_cache_dir();
	SetToken("?temp", xdg_cache);
#else
	SetToken("?temp", boost::filesystem::temp_directory_path());
#endif
}

}
