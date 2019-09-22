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

#include "libaegisub/path.h"

#include "libaegisub/fs.h"

#include <iostream>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/range/distance.hpp>

namespace {
static const char *tokens[] = {
	"?audio",
	"?data",
	"?dictionary",
	"?local",
	"?script",
	"?temp",
	"?user",
	"?video"
};

int find_token(const char *str, size_t len) {
	if (len < 5 || str[0] != '?') return -1;
	int idx;
	switch (str[1] + str[4]) {
	case 'a' + 'i': idx = 0; break;
	case 'd' + 'a': idx = 1; break;
	case 'd' + 't': idx = 2; break;
	case 'l' + 'a': idx = 3; break;
	case 's' + 'i': idx = 4; break;
	case 't' + 'p': idx = 5; break;
	case 'u' + 'r': idx = 6; break;
	case 'v' + 'e': idx = 7; break;
	default: return -1;
	}

	return strncmp(str, tokens[idx], strlen(tokens[idx])) == 0 ? idx : -1;
}
}

namespace agi {

Path::Path() {
	static_assert(sizeof(paths) / sizeof(paths[0]) == sizeof(tokens) / sizeof(tokens[0]),
		"Token and path arrays need to be the same size");
	FillPlatformSpecificPaths();
	std::cout << "Paths:" << std::endl;
	std::cout << "?user: " << Decode("?user") << std::endl;
	std::cout << "?local: " << Decode("?local") << std::endl;
	std::cout << "?data: " << Decode("?data") << std::endl;
	std::cout << "?dictionary: " << Decode("?dictionary") << std::endl;
	std::cout << "?temp: " << Decode("?temp") << std::endl;
}

fs::path Path::Decode(std::string const& path) const {
	int idx = find_token(path.c_str(), path.size());
	if (idx == -1 || paths[idx].empty())
		return fs::path(path).make_preferred();
	return (paths[idx]/path.substr(strlen(tokens[idx]))).make_preferred();
}

fs::path Path::MakeRelative(fs::path const& path, std::string const& token) const {
	int idx = find_token(token.c_str(), token.size());
	if (idx == -1) throw agi::InternalError("Bad token: " + token);

	return MakeRelative(path, paths[idx]);
}

fs::path Path::MakeRelative(fs::path const& path, fs::path const& base) const {
	if (path.empty() || base.empty()) return path;

	const auto str = path.string();
	if (boost::starts_with(str, "?dummy") || boost::starts_with(str, "dummy-audio:"))
		return path;

	// Paths on different volumes can't be made relative to each other
	if (path.has_root_name() && path.root_name() != base.root_name())
		return path.string();

	auto path_it = path.begin();
	auto ref_it = base.begin();
	for (; path_it != path.end() && ref_it != base.end() && *path_it == *ref_it; ++path_it, ++ref_it) ;

	agi::fs::path result;
	for (; ref_it != base.end(); ++ref_it)
		result /= "..";
	for (; path_it != path.end(); ++path_it)
		result /= *path_it;

	return result;
}

fs::path Path::MakeAbsolute(fs::path path, std::string const& token) const {
	if (path.empty()) return path;
	int idx = find_token(token.c_str(), token.size());
	if (idx == -1) throw agi::InternalError("Bad token: " + token);

	path.make_preferred();
	const auto str = path.string();
	if (boost::starts_with(str, "?dummy") || boost::starts_with(str, "dummy-audio:"))
		return path;
	return (paths[idx].empty() || path.is_absolute()) ? path : paths[idx]/path;
}

std::string Path::Encode(fs::path const& path) const {
	// Find the shortest encoding of path made relative to each token
	std::string shortest = path.string();
	size_t length = boost::distance(path);
	for (size_t i = 0; i < paths.size(); ++i) {
		if (paths[i].empty()) continue;

		const auto p = MakeRelative(path, tokens[i]);
		const size_t d = boost::distance(p);
		if (d < length) {
			length = d;
			shortest = (tokens[i]/p).string();
		}
	}

	return shortest;
}

void Path::SetToken(const char *token_name, fs::path const& token_value) {
	int idx = find_token(token_name, strlen(token_name));
	if (idx == -1) throw agi::InternalError("Bad token: " + std::string(token_name));

	if (token_value.empty())
		paths[idx] = token_value;
	else if (!token_value.is_absolute())
		paths[idx].clear();
	else {
		paths[idx] = token_value;
		paths[idx].make_preferred();
		if (fs::FileExists(paths[idx]))
			paths[idx] = paths[idx].parent_path();
	}
}

}
