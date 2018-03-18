// Copyright (c) 2012, Thomas Goyne <plorkyeran@aegisub.org>
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

/// @file osx_utils.mm
/// @see utils.h
/// @ingroup utils
///

#include <wx/frame.h>
#include <wx/menu.h>
#include <wx/osx/core/cfstring.h>

#include <boost/filesystem.hpp>
#include <libaegisub/fs_fwd.h>

#import <AppKit/AppKit.h>

void AddFullScreenButton(wxWindow *window) {
    NSWindow *nsWindow = [window->GetHandle() window];
    if (![nsWindow respondsToSelector:@selector(toggleFullScreen:)])
        return;

    NSWindowCollectionBehavior collectionBehavior = [nsWindow collectionBehavior];
    collectionBehavior |= NSWindowCollectionBehaviorFullScreenPrimary;
    [nsWindow setCollectionBehavior:collectionBehavior];
}

void SetFloatOnParent(wxWindow *window) {
    __block NSWindow *nsWindow = [window->GetHandle() window];
    [nsWindow setLevel:NSFloatingWindowLevel];

    if (!([nsWindow collectionBehavior] & NSWindowCollectionBehaviorFullScreenPrimary))
        return;

    NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
    [nc addObserverForName:@"NSWindowWillEnterFullScreenNotification"
                    object:nsWindow
                     queue:nil
                usingBlock:^(NSNotification *) { [nsWindow setLevel:NSNormalWindowLevel]; }];
    [nc addObserverForName:@"NSWindowWillExitFullScreenNotification"
                    object:nsWindow
                     queue:nil
                usingBlock:^(NSNotification *) { [nsWindow setLevel:NSFloatingWindowLevel]; }];
}

void SetPlaceholderText(wxWindow *window, wxString const& placeholder) {
    id nsWindow = window->GetHandle();
    if ([nsWindow respondsToSelector:@selector(cell)]) {
        NSTextFieldCell *cell = [nsWindow cell];
        cell.placeholderString = wxCFStringRef(placeholder).AsNSString();
    }
}

void RestartAegisub() {
    auto helperPath = [NSBundle.mainBundle pathForAuxiliaryExecutable:@"restart-helper"];
    if (helperPath)
        [NSTask launchedTaskWithLaunchPath:helperPath
                                 arguments:@[NSBundle.mainBundle.executablePath]];
}

void SetTitleWithFilename(wxWindow *window, agi::fs::path const& name) {
    __block NSWindow *nsWindow = [window->GetHandle() window];
    [nsWindow setTitleWithRepresentedFilename:[NSString stringWithUTF8String:name.c_str()]];
}

namespace osx {
void make_windows_menu(wxMenu* wxmenu) {
	NSApp.windowsMenu = wxmenu->GetHMenu();
}

bool activate_top_window_other_than(wxFrame *wx) {
	NSWindow *w = wx->GetWXWindow();
	for (NSNumber *windowNumber in [NSWindow windowNumbersWithOptions:0]) {
		NSWindow *window = [NSApp windowWithWindowNumber:windowNumber.integerValue];
		if (window && window.canBecomeMainWindow && window != w) {
			[window makeMainWindow];
			return true;
		}
	}
	return false;
}

void bring_to_front() {
	[NSApp arrangeInFront:nil];
}
}
