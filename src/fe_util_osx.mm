/*
 *
 *  Attract-Mode frontend
 *  Copyright (C) 2014-15 Andrew Mickelson
 *
 *  This file is part of Attract-Mode.
 *
 *  Attract-Mode is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Attract-Mode is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Attract-Mode.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "fe_util_osx.hpp"
#include <Cocoa/Cocoa.h>
#include <SFML/System/Utf.hpp>

namespace
{
	class cocoa_ar_pool_class
	{
	public:
		cocoa_ar_pool_class()
		{
			pool = [[NSAutoreleasePool alloc] init];
		}

		~cocoa_ar_pool_class()
		{
			[pool release];
		}

	private:
		NSAutoreleasePool* pool;
	};
};

void osx_hide_menu_bar()
{
	[NSMenu setMenuBarVisible:NO];
}

std::string osx_clipboard_get_content()
{
	cocoa_ar_pool_class pool;

	NSPasteboard* pboard = [NSPasteboard generalPasteboard];
	NSString* nstext = [pboard stringForType:NSPasteboardTypeString];
	return std::string( [nstext UTF8String] );
}

void osx_clipboard_set_content( const std::string &value )
{
	cocoa_ar_pool_class pool;

	NSString *nstext = [NSString stringWithCString:value.c_str() encoding:NSUTF8StringEncoding];
	NSPasteboard* pboard = [NSPasteboard generalPasteboard];
	[pboard clearContents];
	[pboard setString:nstext forType:NSPasteboardTypeString];
}

void osx_take_focus()
{
	[NSApp activateIgnoringOtherApps:YES];
}

bool osx_get_capslock()
{
	CGEventFlags flags = CGEventSourceFlagsState(kCGEventSourceStatePrivate);
	return ((flags & kCGEventFlagMaskAlphaShift) == kCGEventFlagMaskAlphaShift);
}
