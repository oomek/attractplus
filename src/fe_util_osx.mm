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

void osx_take_focus()
{
	[NSApp activateIgnoringOtherApps:YES];
}

bool osx_get_capslock()
{
	NSUInteger flags = [NSEvent modifierFlags];
	return ( flags & NSAlphaShiftKeyMask );
}
