// Copyright (c) 2005, Niels Martin Hansen
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Aegisub Group nor the names of its contributors
//     may be used to endorse or promote products derived from this software
//     without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Aegisub Project http://www.aegisub.org/

/// @file version.cpp
/// @brief Derive and return various information about the build and version at runtime
/// @ingroup main
///

#include "version.h"
#include "git_version.h"

#ifdef _DEBUG
	#define DEBUG_SUFFIX " [DEBUG VERSION]"
#else
	#define DEBUG_SUFFIX ""
#endif

#if defined(BUILD_CREDIT) && !TAGGED_RELEASE
	#define BUILD_CREDIT_SUFFIX ", " BUILD_CREDIT
#else
	#define BUILD_CREDIT_SUFFIX ""
#endif

const char *GetAegisubLongVersionString() {
	return BUILD_GIT_VERSION_STRING BUILD_CREDIT_SUFFIX DEBUG_SUFFIX;
}

const char *GetAegisubShortVersionString() {
	return BUILD_GIT_VERSION_STRING DEBUG_SUFFIX;
}

#ifdef BUILD_CREDIT
const char *GetAegisubBuildTime() {
	return __DATE__ " " __TIME__;
}

const char *GetAegisubBuildCredit() {
	return BUILD_CREDIT;
	return "";
}
#endif

bool GetIsOfficialRelease() {
	return false;
}

const char *GetVersionNumber() {
	return BUILD_GIT_VERSION_STRING;
}

int GetSVNRevision() {
#ifdef BUILD_GIT_VERSION_NUMBER
	return BUILD_GIT_VERSION_NUMBER;
#else
	return 0;
#endif
}
