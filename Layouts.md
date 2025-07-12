# <img src="https://github.com/oomek/attractplus/blob/master/resources/images/Logo.png?raw=true" width="40" align="left"> Attract-Mode Plus Coding Reference

Functions and parameters unique to Attract-Mode Plus are marked with a 🔶 symbol.

## Contents

-  [Overview](#overview)
   -  [Squirrel Language](#squirrel-language)
   -  [Language Extensions](#language-extensions)
   -  [Frontend Binding](#frontend-binding)
   -  [Magic Tokens](#magic-tokens)
   -  [User Config](#user-config)
-  [Functions](#functions)
   -  [`fe.add_image()`](#feadd_image)
   -  [`fe.add_artwork()`](#feadd_artwork)
   -  [`fe.add_surface()`](#feadd_surface)
   -  [`fe.add_clone()`](#feadd_clone)
   -  [`fe.add_text()`](#feadd_text)
   -  [`fe.add_listbox()`](#feadd_listbox)
   -  [`fe.add_rectangle()`](#feadd_rectangle-) 🔶
   -  [`fe.add_shader()`](#feadd_shader)
   -  [`fe.add_sound()`](#feadd_sound)
   -  [`fe.add_music()`](#feadd_music-) 🔶
   -  [`fe.add_ticks_callback()`](#feadd_ticks_callback)
   -  [`fe.add_transition_callback()`](#feadd_transition_callback)
   -  [`fe.game_info()`](#fegame_info)
   -  [`fe.get_art()`](#feget_art)
   -  [`fe.get_input_state()`](#feget_input_state)
   -  [`fe.get_input_pos()`](#feget_input_pos)
   -  [`fe.signal()`](#fesignal)
   -  [`fe.set_display()`](#feset_display)
   -  [`fe.add_signal_handler()`](#feadd_signal_handler)
   -  [`fe.remove_signal_handler()`](#feremove_signal_handler)
   -  [`fe.do_nut()`](#fedo_nut)
   -  [`fe.load_module()`](#feload_module)
   -  [`fe.plugin_command()`](#feplugin_command)
   -  [`fe.plugin_command_bg()`](#feplugin_command_bg)
   -  [`fe.path_expand()`](#fepath_expand)
   -  [`fe.path_test()`](#fepath_test)
   -  [`fe.get_file_mtime()`](#feget_file_mtime-) 🔶
   -  [`fe.get_config()`](#feget_config)
   -  [`fe.get_text()`](#feget_text)
   -  [`fe.get_url()`](#feget_url-) 🔶
   -  [`fe.log()`](#felog-) 🔶
-  [Objects and Variables](#objects-and-variables)
   -  [`fe.ambient_sound`](#feambient_sound)
   -  [`fe.layout`](#felayout)
   -  [`fe.list`](#felist)
   -  [`fe.image_cache`](#feimage_cache)
   -  [`fe.overlay`](#feoverlay)
   -  [`fe.obj`](#feobj)
   -  [`fe.displays`](#fedisplays)
   -  [`fe.filters`](#fefilters)
   -  [`fe.monitors`](#femonitors)
   -  [`fe.script_dir`](#fescript_dir)
   -  [`fe.script_file`](#fescript_file)
   -  [`fe.module_dir`](#femodule_dir)
   -  [`fe.nv`](#fenv)
-  [Classes](#classes)
   -  [`fe.LayoutGlobals`](#felayoutglobals)
   -  [`fe.CurrentList`](#fecurrentlist)
   -  [`fe.ImageCache`](#feimagecache)
   -  [`fe.Overlay`](#feoverlay-1)
   -  [`fe.Display`](#fedisplay)
   -  [`fe.Filter`](#fefilter)
   -  [`fe.Monitor`](#femonitor)
   -  [`fe.Image`](#feimage)
   -  [`fe.Text`](#fetext)
   -  [`fe.ListBox`](#felistbox)
   -  [`fe.Rectangle`](#ferectangle-) 🔶
   -  [`fe.Sound`](#fesound)
   -  [`fe.Music`](#femusic-) 🔶
   -  [`fe.Shader`](#feshader)
-  [Constants](#constants)

---

## Overview

The Attract-mode layout sets out what gets displayed to the user. Layouts consist of a `layout.nut` script file and a collection of related resources (images, other scripts, etc.) used by the script.

Layouts are stored under the "layouts" subdirectory of the Attract-Mode config directory. Each layout is stored in its own separate subdirectory or archive file (Attract-Mode can read layouts and plugins directly from `.zip`, `.7z`, `.rar`, `.tar.gz`, `.tar.bz2` and `.tar` files).

Each layout can have one or more `layout*.nut` script files. The "Toggle Layout" command in Attract-Mode allows users to cycle between each of the `layout*.nut` script files located in the layout's directory. Attract-Mode remembers the last layout file toggled to for each layout and will go back to that same file the next time the layout is loaded. This allows for variations of a particular layout to be implemented and easily selected by the user (for example, a layout could provide a `layout.nut` for horizontal monitor orientations and a `layout-vert.nut` for vertical).

The Attract-Mode screen saver and intro modes are really just special case layouts. The screensaver gets loaded after a user-configured period of inactivity, while the intro mode gets run when the frontend first starts and exits as soon as any action is triggered (for example if the user hits the select button). The screen saver script is located in the `screensaver.nut` file stored in the "screensaver" subdirectory. The intro script is located in the `intro.nut` file stored in the "intro" subdirectory.

Plug-ins are similar to layouts in that they consist of at least one squirrel script file and a collection of related resources. Plug-ins are stored in the "plugins" subdirectory of the Attract-Mode config directory. Plug-ins can be a single ".nut" file stored in this subdirectory. They can also have their own separate subdirectory or archive file (in which case the script itself needs to be in a file called `plugin.nut`).

---

### Squirrel Language

Attract-Mode's layouts are scripts written in the Squirrel programming language. Squirrel's standard `Blob`, `IO`, `Math`, `String` and `System` library functions are available for use in a script. For more information on programming in Squirrel and using its standard libraries, consult the Squirrel manuals:

-  [Squirrel 3.0 Reference Manual](http://www.squirrel-lang.org/doc/squirrel3.html)
-  [Squirrel 3.0 Standard Library Manual](http://www.squirrel-lang.org/doc/sqstdlib3.html)

Also check out the Introduction to Squirrel on the Attract-Mode wiki:

-  https://github.com/mickelson/attract/wiki/Introduction-to-Squirrel-Programming

---

### Language Extensions

Attract-Mode includes the following home-brewed extensions to the squirrel language and standard libraries:

-  A `zip_extract_archive( zipfile, filename )` function that will open a specified `zipfile` archive file and extract `filename` from it, returning the contents as a squirrel blob.
-  A `zip_get_dir( zipfile )` function that will return an array of the filenames contained in the `zipfile` archive file.

Supported archive formats are: `.zip`, `.7z`, `.rar`, `.tar.gz`, `.tar.bz2` and `.tar`.

---

### Frontend Binding

All of the functions, objects and classes that Attract-Mode exposes to Squirrel are arranged under the `fe` table, which is bound to Squirrel's root table.

```squirrel
fe.add_image( "bg.png", 0, 0 )
local marquee = fe.add_artwork( "marquee", 256, 20, 512, 256 )
marquee.set_rgb( 100, 100, 100 )
```

The remainder of this document describes the functions, objects, classes and constants that are exposed to layout and plug-in scripts.

---

### Magic Tokens

Image names, as well as the messages displayed by Text and Listbox objects, can all contain one or more _Magic Tokens_. _Magic Tokens_ are enclosed in square brackets, and the frontend automatically updates them accordingly as the user navigates the frontend. So for example, a Text message set to `"[Manufacturer]"` will be automatically updated with the appropriate Manufacturer's name. There are more examples below.

The following _Magic Tokens_ are currently supported:

-  `[DisplayName]` - The name of the current display
-  `[ListSize]` - The number of items in the game list
-  `[ListEntry]` - The number of the current selection in the game list
-  `[FilterName]` - The name of the filter
-  `[Search]` - The search rule currently applied to the game list
-  `[SortName]` - The attribute that the list was sorted by
-  `[Name]` - The short name of the selected game
-  `[Title]` - The full name of the selected game
-  `[Emulator]` - The emulator to use for the selected game
-  `[CloneOf]` - The short name of the game that the selection is a clone of
-  `[Year]` - The year for the selected game
-  `[Manufacturer]` - The manufacturer for the selected game
-  `[Category]` - The category for the selected game
-  `[Players]` - The number of players for the selected game
-  `[Rotation]` - The rotation for the selected game
-  `[Control]` - The primary control for the selected game
-  `[Status]` - The emulation status for the selected game
-  `[DisplayCount]` - The number of displays for the selected game
-  `[DisplayType]` - The display type for the selected game
-  `[AltRomname]` - The alternative Romname for the selected game
-  `[AltTitle]` - The alternative title for the selected game
-  `[PlayedCount]` - The number of times the selected game has been played
-  `[PlayedTime]` - The amount of time the selected game has been played
-  `[PlayedLast]` - The date and time the selected game was last played
-  `[PlayedAgo]` - The last played date relative to now, for example: "5 Minutes Ago"
-  `[SortValue]` - The value used to order the selected game in the list
-  `[System]` - The first "System" name configured for the selected game's emulator
-  `[SystemN]` - The last "System" name configured for the selected game's emulator
-  `[Overview]` - The overview description for the selected game

_Magic Tokens_ can also be used to run a function defined in your layout or plugin's squirrel script to obtain the desired text. These tokens are in the form `[!<function_name>]`. When used, Attract-Mode will run the corresponding function (defined in the squirrel "root table"). This function should then return the string value that you wish to have replace the _Magic Token_. The function defined in squirrel can optionally have up to two parameters passed to it. If it is defined with a first parameter, Attract-Mode will supply the appropriate index_offset in that parameter when it calls the function. If a second parameter is present as well, the appropriate filter_offset is supplied.

```squirrel
// Add a text that displays the filter name and list location
//
fe.add_text( "[FilterName] [[ListEntry]/[ListSize]]", 0, 0, 400, 20 )

// Add an image that will match to the first word in the
// Manufacturer name (i.e. "Atari.png", "Nintendo.jpg")
//
function strip_man( ioffset )
{
  local m = fe.game_info(Info.Manufacturer, ioffset)
  return split( m, " " )[0]
}
fe.add_image( "[!strip_man]", 0, 0 )

// Add a text that will display a copyright message if both
// the manufacturer name and a year are present.  Otherwise,
// just show the Manufacturer name.
//
function well_formatted()
{
  local m = fe.game_info( Info.Manufacturer )
  local y = fe.game_info( Info.Year )

  if (( m.len() > 0 ) && ( y.len() > 0 ))
  {
    return "Copyright " + y + ", " + m
  }

  return m
}
fe.add_text( "[!well_formatted]", 0, 0 )
```

---

### User Config

Configuration settings can be added to a layout/plugin/screensaver/intro to provide users with customization options.

Configurations are defined by a `UserConfig` class at the top of your script, where each property is an individual setting. Properties should be prefixed with an `</ attribute />` that describes how they are displayed, and their values can be retrieved using [`fe.get_config()`](#feget_config).

```squirrel
class UserConfig </ help="Description" /> {
  </ label="Choice", order=1, options="Yes,No" /> opt = "Yes"
  </ label="String", order=2 /> val = "Default"
}

local config = fe.get_config()
// config = { opt = "Yes", val = "Default" }
```

**Properties**

-  `label` - [string] Text for the setting list item, if omitted the property id is used.
-  `help` - [string] The message to display in the footer when the setting is selected.
-  `order` - [integer] The list order of the setting.
-  `per_display` - [boolean] When `true` the setting value will be unique to each display.

The attribute may use **one** of the following properties to define its type:

-  `options` - [string] Present the user with a choice, for example: `"Yes,No"`.
-  `is_input` - [boolean] Prompt the user to press a key.
-  `is_function` - [boolean] Call the function named by the property, for example: `func = "callback"`.
-  `is_info` - [boolean] A readonly setting used for headings or separators.
-  (None of the above) - A text input for keyboard entry.

**Callback**

-  The function should be in the following form:

   ```squirrel
   // The config parameter contains the fe.get_config() table
   function callback( config )
   {
     // The returned string is displayed in the footer
     return "Success"
   }
   ```

---

## Functions

### `fe.add_image()`

```squirrel
fe.add_image( name )
fe.add_image( name, x, y )
fe.add_image( name, x, y, w, h )
```

Adds an image or video to the end of Attract-Mode's draw list.

The default blend mode for images is `BlendMode.Alpha`

**Parameters**

-  `name` - The name of an image/video file to show. If a relative path is provided (i.e. `"bg.png"`) it is assumed to be relative to the current layout directory (or the plugin directory, if called from a plugin script). If a relative path is provided and the layout/plugin is contained in an archive, Attract-Mode will open the corresponding file stored inside of the archive. Supported image formats are: `PNG`, `JPEG`, `GIF`, `BMP` and `TGA`. Videos can be in any format supported by FFmpeg. One or more [_Magic Tokens_](#magic-tokens) can be used in the name, in which case Attract-Mode will automatically update the image/video file appropriately in response to user navigation. For example `"man/[Manufacturer]"` will load the file corresponding to the manufacturer's name from the man subdirectory of the layout/plugin (example: `"man/Konami.png"`). When Magic Tokens are used, the file extension specified in `name` is ignored (if present) and Attract-Mode will load any supported media file that matches the Magic Token.
-  `x` - The x position of the image (in layout coordinates).
-  `y` - The y position of the image (in layout coordinates).
-  `w` - The width of the image (in layout coordinates). Default value is `0`, which enables `auto_width`.
-  `h` - The height of the image (in layout coordinates). Default value is `0`, which enables `auto_height`.

**Return Value**

-  An instance of the class [`fe.Image`](#feimage) which can be used to interact with the added image/video.

---

### `fe.add_artwork()`

```squirrel
fe.add_artwork( label )
fe.add_artwork( label, x, y )
fe.add_artwork( label, x, y, w, h )
```

Add an artwork to the end of Attract-Mode's draw list. The image/video displayed in an artwork is updated automatically whenever the user changes the game selection.

The default blend mode for artwork is `BlendMode.Alpha`

**Parameters**

-  `label` - The label of the artwork to display. This should correspond to an artwork configured in Attract-Mode (artworks are configured per emulator in the config menu) or scraped using the scraper. Attract-Mode's standard artwork labels are: `"snap"`, `"marquee"`, `"flyer"`, `"wheel"`, and `"fanart"`.
-  `x` - The x position of the artwork (in layout coordinates).
-  `y` - The y position of the artwork (in layout coordinates).
-  `w` - The width of the artwork (in layout coordinates). Default value is `0`, which enables `auto_width`.
-  `h` - The height of the artwork (in layout coordinates). Default value is `0`, which enables `auto_height`.

**Return Value**

-  An instance of the class [`fe.Image`](#feimage) which can be used to interact with the added artwork.

---

### `fe.add_surface()`

```squirrel
fe.add_surface( w, h )
fe.add_surface( x, y, w, h ) 🔶
```

Add a surface to the end of Attract-Mode's draw list. A surface is an off-screen texture upon which you can draw other [`fe.Image`](#feadd_image), [`fe.Artwork`](#feadd_artwork), [`fe.Text`](#feadd_text), [`fe.Listbox`](#feadd_listbox) and [`fe.Surface`](#feadd_surface) objects. The resulting texture is treated as a static image by Attract-Mode which can in turn have image effects applied to it (`scale`, `position`, `pinch`, `skew`, `shaders`, etc) when it is drawn.

The default blend mode for surfaces is `BlendMode.Premultiplied`

**Parameters**

-  `x` - The x coordinate of the top left corner of the surface (in layout coordinates).
-  `y` - The y coordinate of the top left corner of the surface (in layout coordinates).
-  `w` - The width of the surface texture (in pixels).
-  `h` - The height of the surface texture (in pixels).

**Return Value**

-  An instance of the class [`fe.Image`](#feimage) which can be used to interact with the added surface.

---

### `fe.add_clone()`

```squirrel
fe.add_clone( img )
```

Clone an image, artwork or surface object and add the clone to the back of Attract-Mode's draw list. The texture pixel data of the original and clone is shared as a result.

**Parameters**

-  `img` - The image, artwork or surface object to clone. This needs to be an instance of the class [`fe.Image`](#feimage).

**Return Value**

-  An instance of the class [`fe.Image`](#feimage) which can be used to interact with the added clone.

---

### `fe.add_text()`

```squirrel
fe.add_text( msg, x, y, w, h )
```

Add a text label to the end of Attract-Mode's draw list.

**Parameters**

-  `msg` - The text to display. [_Magic Tokens_](#magic-tokens) can be used here, in which case Attract-Mode will dynamically update the msg in response to navigation.
-  `x` - The x coordinate of the top left corner of the text (in layout coordinates).
-  `y` - The y coordinate of the top left corner of the text (in layout coordinates).
-  `w` - The width of the text (in layout coordinates).
-  `h` - The height of the text (in layout coordinates).

**Return Value**

-  An instance of the class [`fe.Text`](#fetext) which can be used to interact with the added text.

---

### `fe.add_listbox()`

```squirrel
fe.add_listbox( x, y, w, h )
```

Add a listbox to the end of Attract-Mode's draw list.

**Parameters**

-  `x` - The x coordinate of the top left corner of the listbox (in layout coordinates).
-  `y` - The y coordinate of the top left corner of the listbox (in layout coordinates).
-  `w` - The width of the listbox (in layout coordinates).
-  `h` - The height of the listbox (in layout coordinates).

**Return Value**

-  An instance of the class [`fe.ListBox`](#felistbox) which can be used to
   interact with the added text.

---

### `fe.add_rectangle()` 🔶

```squirrel
fe.add_rectangle( x, y, w, h )
```

Add a rectangle to the end of Attract-Mode's draw list.

**Parameters**

-  `x` - The x coordinate of the top left corner of the rectangle (in layout coordinates).
-  `y` - The y coordinate of the top left corner of the rectangle (in layout coordinates).
-  `w` - The width of the rectangle (in layout coordinates).
-  `h` - The height of the rectangle (in layout coordinates).

**Return Value**

-  An instance of the class [`fe.Rectangle`](#ferectangle-) which can be used to interact with the added rectangle.

---

### `fe.add_shader()`

```squirrel
fe.add_shader( type, file1, file2 )
fe.add_shader( type, file1 )
fe.add_shader( type )
```

Add a GLSL shader (vertex and/or fragment) for use in the layout.

**Parameters**

-  `type` - The type of shader to add. Can be one of the following values:
   -  `Shader.VertexAndFragment` - Add a combined vertex and fragment shader
   -  `Shader.Vertex` - Add a vertex shader
   -  `Shader.Fragment` - Add a fragment shader
   -  `Shader.Empty` - Add an empty shader. An object's shader property can be set to an empty shader to stop using a shader on that object where one was set previously.
-  `file1` - The name of the shader file located in the layout/plugin directory. For the VertexAndFragment type, this should be the vertex shader.
-  `file2` - This parameter is only used with the VertexAndFragment type, and should be the name of the fragment shader file located in the layout/plugin directory.

**Return Value**

-  An instance of the class [`fe.Shader`](#feshader) which can be used to interact with the added shader.

**GLSL Shaders**

-  Shaders are implemented using the SFML API. For more information please see http://www.sfml-dev.org/tutorials/2.1/graphics-shader.php
-  The minimal vertex shader expected is as follows:

   ```glsl
   void main()
   {
     // transform the vertex position
     gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;

     // transform the texture coordinates
     gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;

     // forward the vertex color
     gl_FrontColor = gl_Color;
   }
   ```

-  The minimal fragment shader expected is as follows:

   ```glsl
   uniform sampler2D texture;

   void main()
   {
     // lookup the pixel in the texture
     vec4 pixel = texture2D(texture, gl_TexCoord[0].xy);

     // multiply it by the color
     gl_FragColor = gl_Color * pixel;
   }
   ```

---

### `fe.add_sound()`

```squirrel
fe.add_sound( name )
```

Add a sound file that can then be played by Attract-Mode. For short sounds, stored in RAM. For playing long audio tracks use [`fe.add_music()`](#feadd_music-).

**Parameters**

-  `name` - The name of the sound file. If a relative path is provided, it is treated as relative to the directory for the layout/plugin that called this function.

**Return Value**

-  An instance of the class [`fe.Sound`](#fesound) which can be used to interact with the sound.

---

### `fe.add_music()` 🔶

```squirrel
fe.add_music( name )
```

Add an audio track that can then be played by Attract-Mode. For long audio tracks, streamed from disk. For playing short sounds use [`fe.add_sound()`](#feadd_sound).

**Parameters**

-  `name` - The name of the audio track file. If a relative path is provided, it is treated as relative to the directory for the layout/plugin that called this function.

**Return Value**

-  An instance of the class [`fe.Music`](#femusic-) which can be used to interact with the sound.

---

### `fe.add_ticks_callback()`

```squirrel
fe.add_ticks_callback( environment, function_name )
fe.add_ticks_callback( function_name )
```

The single parameter passed to the tick function is the amount of time (in milliseconds) since the layout began. Register a function in your script to get "tick" callbacks. Tick callbacks occur continuously during the running of the frontend.

**Parameters**

-  `environment` - The squirrel object that the function is associated with (default value: the root table of the squirrel vm)
-  `function_name` - A string naming the function to be called.

**Return Value**

-  None.

**Callback**

-  The function that is registered should be in the following form:

   ```squirrel
   function tick( tick_time )
   {
     // do stuff...
   }
   ```

---

### `fe.add_transition_callback()`

```squirrel
fe.add_transition_callback( environment, function_name )
fe.add_transition_callback( function_name )
```

Register a function in your script to get transition callbacks. Transition callbacks are triggered by certain events in the frontend.

**Parameters**

-  `environment` - The squirrel object that the function is associated with (default value: the root table of the squirrel vm)
-  `function_name` - A string naming the function to be called.

**Return Value**

-  None.

**Callback**

-  The function that is registered should be in the following form:

   ```squirrel
   function transition( ttype, var, transition_time )
   {
     local redraw_needed = false

     // do stuff...

     if ( redraw_needed )
     {
       return true
     }

     return false
   }
   ```

The `ttype` parameter passed to the transition function indicates what is happening. It will have one of the following values:

-  `Transition.StartLayout`
-  `Transition.EndLayout`
-  `Transition.ToNewSelection`
-  `Transition.FromOldSelection`
-  `Transition.ToGame`
-  `Transition.FromGame`
-  `Transition.ToNewList`
-  `Transition.EndNavigation`
-  `Transition.ShowOverlay`
-  `Transition.HideOverlay`
-  `Transition.NewSelOverlay`
-  `Transition.ChangedTag`

The value of the `var` parameter passed to the transition function depends upon the value of `ttype`:

-  `Transition.ToNewSelection`, `var` will be:
   -  The index offset of the selection being transitioned to (i.e. `-1` when moving back one position in the list, `1` when moving forward one position, `2` when moving forward two positions, etc.)
-  `Transition.FromOldSelection`, `var` will be:
   -  The index offset of the selection being transitioned from (i.e. `1` after moving back one position in the list, `-1` after moving forward one position, `-2` after moving forward two positions, etc.)
-  `Transition.StartLayout`, `var` will be:
   -  `FromTo.Frontend` - If the frontend is just starting,
   -  `FromTo.ScreenSaver` - If the layout is starting (or the list is being loaded) because the built-in screen saver has stopped, or
   -  `FromTo.NoValue` - Otherwise.
-  `Transition.EndLayout`, `var` will be:
   -  `FromTo.Frontend` - If the frontend is shutting down,
   -  `FromTo.ScreenSaver` - If the layout is stopping because the built-in screen saver is starting, or
   -  `FromTo.NoValue` - Otherwise.
-  `Transition.ToNewList`, `var` will be:
   -  The filter index offset of the filter being transitioned to (i.e. `-1` when moving back one filter, `1` when moving forward) if known, otherwise `var` is `0`.
-  `Transition.ShowOverlay`, var will be:
   -  `Overlay.Custom` - If a script generated overlay is being shown.
   -  `Overlay.Exit` - If the exit menu is being shown.
   -  `Overlay.Favourite` - If the add/remove favourite menu is being shown.
   -  `Overlay.Displays` - If the displays menu is being shown.
   -  `Overlay.Filters` - If the filters menu is being shown.
   -  `Overlay.Tags` - If the tags menu is being shown.
-  `Transition.NewSelOverlay`, `var` will be:
   -  The index of the new selection in the Overlay menu.
-  `Transition.ChangedTag`, `var` will be:
   -  `Info.Favourite` if the favourite status of the current game was changed.
   -  `Info.Tags` if a tag for the current game was changed.
-  `Transition.ToGame`, `Transition.FromGame`, `Transition.EndNavigation`, or `Transition.HideOverlay`, `var` will be:
   -  `FromTo.NoValue`.

The `transition_time` parameter passed to the transition function is the amount of time (in milliseconds) since the transition began.

The transition function must return a boolean value. It should return `true` if a redraw is required, in which case Attract-Mode will redraw the screen and immediately call the transition function again with an updated `transition_time`.

**_The transition function must eventually return `false` to notify Attract-Mode that the transition effect is done, allowing the normal operation of the frontend to proceed._**

---

### `fe.game_info()`

```squirrel
fe.game_info( id )
fe.game_info( id, index_offset )
fe.game_info( id, index_offset, filter_offset )
```

Get information about the selected game.

**Parameters**

-  `id` - Id of the information attribute to get. Can be one of the following values:
   -  `Info.Name`
   -  `Info.Title`
   -  `Info.Emulator`
   -  `Info.CloneOf`
   -  `Info.Year`
   -  `Info.Manufacturer`
   -  `Info.Category`
   -  `Info.Players`
   -  `Info.Rotation`
   -  `Info.Control`
   -  `Info.Status`
   -  `Info.DisplayCount`
   -  `Info.DisplayType`
   -  `Info.AltRomname`
   -  `Info.AltTitle`
   -  `Info.Extra`
   -  `Info.Favourite`
   -  `Info.Tags`
   -  `Info.PlayedCount`
   -  `Info.PlayedTime`
   -  `Info.PlayedLast`
   -  `Info.FileIsAvailable`
   -  `Info.System`
   -  `Info.Overview`
   -  `Info.IsPaused`
   -  `Info.SortValue`
-  `index_offset` - The offset (from the current selection) of the game to retrieve info on. i.e. `-1` = previous game, `0` = current game, `1` = next game, and so on. Default value is `0`.
-  `filter_offset` - The offset (from the current filter) of the filter containing the selection to retrieve info on. i.e. `-1` = previous filter, `0` = current filter. Default value is `0`.

**Return Value**

-  A string containing the requested information.

**Notes**

-  The `Info.IsPaused` attribute is `1` if the game is currently paused by the frontend, and an empty string if it is not.

---

### `fe.get_art()`

```squirrel
fe.get_art( label )
fe.get_art( label, index_offset )
fe.get_art( label, index_offset, filter_offset )
fe.get_art( label, index_offset, filter_offset, flags )
```

Get the filename of an artwork for the selected game.

**Parameters**

-  `label` - The label of the artwork to retrieve. This should correspond to an artwork configured in Attract-Mode (artworks are configured per emulator in the config menu) or scraped using the scraper. Attract-Mode's standard artwork labels are: `"snap"`, `"marquee"`, `"flyer"`, `"wheel"`, and `"fanart"`.
-  `index_offset` - The offset (from the current selection) of the game to retrieve the filename for. i.e. `-1` = previous game, `0` = current game, `1` = next game, and so on. Default value is `0`.
-  `filter_offset` - The offset (from the current filter) of the filter containing the selection to retrieve the filename for. i.e. `-1` = previous filter, `0` = current filter. Default value is `0`.
-  `flags` - Flags to control the filename that gets returned. Can be set
   to any combination of none or more of the following (i.e. `Art.ImagesOnly | Art.FullList`):
   -  `Art.Default` - Return single match, video or image
   -  `Art.ImagesOnly` - Only return an image match (no video)
   -  `Art.FullList` - Return a full list of the matches made (if multiples available). Names are returned in a single string, semicolon separated

**Return Value**

-  A string containing the filename of the requested artwork. If no file is found, an empty string is returned. If the artwork is contained in an archive, then both the archive path and the internal path are returned, separated by a pipe `|` character: `"<archive_path>|<content_path>"`

---

### `fe.get_input_state()`

```squirrel
fe.get_input_state( input_id )
```

Check if a specific keyboard key, mouse button, joystick button or joystick direction is currently pressed, or check if any input mapped to a particular frontend action is pressed.

**Parameters**

-  `input_id` - [string] the input to test. This can be a string in the same format as used in the attract.cfg file for input mappings. For example, `"LControl"` will check the left control key, `"Joy0 Up"` will check the up direction on the first joystick, `"Mouse MiddleButton"` will check the middle mouse button, and `"select"` will check for any input mapped to the game select button.

**Return Value**

-  `true` if input is pressed, `false` otherwise.

---

### `fe.get_input_pos()`

```squirrel
fe.get_input_pos( input_id )
```

Return the current position for the specified joystick axis.

**Parameters**

-  `input_id` - [string] the input to test. The format of this string is the same as that used in the `attract.cfg` file. For example, `"Joy0 Up"` is the up direction on the first joystick, `"Mouse Up"` is the y position of the mouse, and `"Mouse WheelUp"` is the delta of an upward mouse wheel scroll.

**Return Value**

-  Current position of the specified axis, in range `[0...100]`.

---

### `fe.signal()`

```squirrel
fe.signal( signal_str )
```

Signal that a particular frontend action should occur.

**Parameters**

-  `signal_str` - The action to signal for. Can be one of the following strings:
   -  `"back"`
   -  `"up"`
   -  `"down"`
   -  `"left"`
   -  `"right"`
   -  `"select"`
   -  `"prev_game"`
   -  `"next_game"`
   -  `"prev_page"`
   -  `"next_page"`
   -  `"prev_display"`
   -  `"next_display"`
   -  `"displays_menu"`
   -  `"prev_filter"`
   -  `"next_filter"`
   -  `"filters_menu"`
   -  `"toggle_layout"`
   -  `"toggle_movie"`
   -  `"toggle_mute"`
   -  `"toggle_rotate_right"`
   -  `"toggle_flip"`
   -  `"toggle_rotate_left"`
   -  `"exit"`
   -  `"exit_to_desktop"`
   -  `"screenshot"`
   -  `"configure"`
   -  `"random_game"`
   -  `"replay_last_game"`
   -  `"add_favourite"`
   -  `"prev_favourite"`
   -  `"next_favourite"`
   -  `"add_tags"`
   -  `"screen_saver"`
   -  `"prev_letter"`
   -  `"next_letter"`
   -  `"intro"`
   -  `"custom1"`
   -  `"custom2"`
   -  `"custom3"`
   -  `"custom4"`
   -  `"custom5"`
   -  `"custom6"`
   -  `"custom7"`
   -  `"custom8"`
   -  `"custom9"`
   -  `"custom10"`
   -  `"reset_window"`
   -  `"reload"`

**Return Value**

-  None.

---

### `fe.set_display()`

```squirrel
fe.set_display( index, stack_previous, reload ) 🔶
fe.set_display( index, stack_previous )
fe.set_display( index )
```

Change to the display at the specified index. This should align with the index of the [`fe.displays`](#fedisplays) array that contains the intended display.

**Parameters**

-  `index` - The index of the display to change to. This should correspond to the index in the [`fe.displays`](#fedisplays) array of the intended new display. The index for the current display is stored in [`fe.list.display_index`](#fecurrentlist).
-  `stack_previous` - [boolean] if set to `true`, the new display is stacked on the current one, so that when the user selects the `"Back"` UI button the frontend will navigate back to the earlier display. Default value is `false`.
-  `reload` 🔶 - [boolean] if set to `false` and the current display shares the same layout file the layout is not reloaded. Default value is `true`.

**Return Value**

-  None.

---

### `fe.add_signal_handler()`

```squirrel
fe.add_signal_handler( environment, function_name )
fe.add_signal_handler( function_name )
```

Register a function in your script to handle signals. Signals are sent whenever a mapped control is used by the user or whenever a layout or plugin script uses the [`fe.signal()`](#fesignal) function.

**Parameters**

-  `environment` - The squirrel object that the function is associated with (default value: the root table of the squirrel vm)
-  `function_name` - A string naming the signal handler function to be added.

**Return Value**

-  None.

**Callback**

-  The function that is registered should be in the following form:

   ```squirrel
   function handler( signal_str )
   {
     local no_more_processing = false

     // do stuff...

     if ( no_more_processing )
     {
       return true
     }

     return false
   }
   ```

The `signal_str` parameter passed to the handler function is a string that identifies the signal that has been given. This string will correspond to the `signal_str` parameter values of [`fe.signal()`](#fesignal)

The signal handler function should return a boolean value. It should return `true` if no more processing should be done on this signal. It should return `false` if signal processing is to continue, in which case this signal will be dealt with in the default manner by the frontend.

---

### `fe.remove_signal_handler()`

```squirrel
fe.remove_signal_handler( environment, function_name )
fe.remove_signal_handler( function_name )
```

Remove a signal handler that has been added with the [`fe.add_signal_handler()`](#feadd_signal_handler) function.

**Parameters**

-  `environment` - The squirrel object that the signal handler function is associated with (default value: the root table of the squirrel vm)
-  `function_name` - A string naming the signal handler function to remove.

**Return Value**

-  None.

---

### `fe.do_nut()`

```squirrel
fe.do_nut( name )
```

Execute another Squirrel script.

**Parameters**

-  `name` - The name of the script file. If a relative path is provided, it is treated as relative to the directory for the layout/plugin that called this function.

**Return Value**

-  None.

---

### `fe.load_module()`

```squirrel
fe.load_module( name )
```

Loads a module (a "library" Squirrel script).

**Parameters**

-  `name` - The name of the library module to load. This should correspond to a script file in the "modules" subdirectory of your Attract-Mode configuration (without the file extension).

**Return Value**

-  `true` if the module was loaded, `false` if it was not found.

---

### `fe.plugin_command()`

```squirrel
fe.plugin_command( executable, arg_string )
fe.plugin_command( executable, arg_string, environment, callback_function )
fe.plugin_command( executable, arg_string, callback_function )
```

Execute a plug-in command and wait until the command is done.

**Parameters**

-  `executable` - The name of the executable to run.
-  `arg_string` - The arguments to pass when running the executable.
-  `environment` - The squirrel object that the callback function is associated with.
-  `callback_function` - A string containing the name of the function in Squirrel to call with any output that the executable provides on stdout.

**Return Value**

-  None.

**Callback**

-  The function should be in the following form:

   ```squirrel
   function callback_function( op )
   {
     // do stuff...
   }
   ```

   If provided, this function will get called repeatedly with chunks of the command output in `op`.

**Notes**

-  `op` is not necessarily aligned with the start and the end of the lines of output from the command. In any one call `op` may contain data from multiple lines and that may begin or end in the middle of a line.

---

### `fe.plugin_command_bg()`

```squirrel
fe.plugin_command_bg( executable, arg_string )
```

Execute a plug-in command in the background and return immediately.

**Parameters**

-  `executable` - The name of the executable to run.
-  `arg_string` - The arguments to pass when running the executable.

**Return Value**

-  None.

---

### `fe.path_expand()`

```squirrel
fe.path_expand( path )
```

Expand the given path name. A leading `~` or `$HOME` token will be become the user's home directory. On Windows systems, a leading `%SYSTEMROOT%` token will become the path to the Windows directory and a leading `%PROGRAMFILES%` or `%PROGRAMFILES(X86)%` will become the path to the applicable Windows `Program Files` directory. For full list of Windows environment variables follow this [link](https://learn.microsoft.com/en-us/windows/deployment/usmt/usmt-recognized-environment-variables)

**Parameters**

-  `path` - The path string to expand.

**Return Value**

-  The expansion of path.

---

### `fe.path_test()`

```squirrel
fe.path_test( path, flag )
```

Check whether the specified path has the status indicated by `flag`.

**Parameters**

-  `path` - The path to test.
-  `flag` - What to test for. Can be one of the following values:
   -  `PathTest.IsFileOrDirectory`
   -  `PathTest.IsFile`
   -  `PathTest.IsDirectory`
   -  `PathTest.IsRelativePath`
   -  `PathTest.IsSupportedArchive`
   -  `PathTest.IsSupportedMedia`

**Return Value**

-  (boolean) result.

---

### `fe.get_file_mtime()` 🔶

```squirrel
fe.get_file_mtime( filename )
```

Returns the modified time of the given file.

**Parameters**

-  `filename` - The file to get the modified time of.

**Return Value**

-  An integer containing the GMT timestamp.

---

### `fe.get_config()`

```squirrel
fe.get_config()
```

Get the user configured settings for this layout/plugin/screensaver/intro.

**Parameters**

-  None.

**Return Value**

-  A table containing the [User Config](#user-config) settings.

**Notes**

-  This function will _not_ return valid settings when called from a callback function registered with [`fe.add_ticks_callback()`](#feadd_ticks_callback), [`fe.add_transition_callback()`](#feadd_transition_callback) or [`fe.add_signal_handler()`](#feadd_signal_handler).

---

### `fe.get_text()`

```squirrel
fe.get_text( text )
```

Translate the specified text into the user's language. If no translation is found, then return the contents of `text`.

**Parameters**

-  `text` - The text string to translate.

**Return Value**

-  A string containing the translated text.

---

### `fe.get_url()` 🔶

```squirrel
fe.get_url( url, file_path )
```

Download a file from provided url address. When `file_path` is relative the file will be saved to the layout's folder.

**Parameters**

-  `url` - An internet address of the file to download.
-  `file_path` - A destination folder set as an absolute path, or a relative path inside the layout's folder.

---

### `fe.log()` 🔶

```squirrel
fe.log( text )
```

Print a string into the console. It's an alternative to Squirrel `print()` function, which does not always show up immediately.

**Parameters**

-  `text` - A string of text to print in the console

---

## Objects and Variables

### `fe.ambient_sound`

An instance of the [`fe.Music`](#femusic-) class and can be used to control the ambient audio track.

### `fe.layout`

An instance of the [`fe.LayoutGlobals`](#felayoutglobals) class and is where global layout settings are stored.

### `fe.list`

An instance of the [`fe.CurrentList`](#fecurrentlist) class and is where current display settings are stored.

### `fe.image_cache`

An instance of the [`fe.ImageCache`](#feimagecache) class and provides script access to Attract-Mode's internal image cache.

### `fe.overlay`

An instance of the [`fe.Overlay`](#feoverlay-1) class and is where overlay functionality may be accessed.

### `fe.obj`

Contains the Attract-Mode draw list. It is an array of [`fe.Image`](#feimage), [`fe.Text`](#fetext), [`fe.ListBox`](#felistbox), and [`fe.Rectangle`](#ferectangle-) instances.

### `fe.displays`

Contains information on the available displays. It is an array of [`fe.Display`](#fedisplay) instances.

### `fe.filters`

Contains information on the available filters. It is an array of [`fe.Filter`](#fefilter) instances.

### `fe.monitors`

An array of [`fe.Monitor`](#femonitor) instances, and provides the mechanism for interacting with the various monitors in a multi-monitor setup. There will always be at least one entry in this list, and the first entry will always be the _primary_ monitor.

### `fe.script_dir`

When Attract-Mode runs a layout or plug-in script, `fe.script_dir` is set to the layout or plug-in's directory.

### `fe.script_file`

When Attract-Mode runs a layout or plug-in script, `fe.script_file` is set to the name of the layout or plug-in script file.

### `fe.module_dir`

When Attract-Mode runs a module script, `fe.module_dir` is set to the module's directory.

### `fe.nv`

The `fe.nv` table can be used by layouts and plugins to store persistent values. The values in this table get saved by Attract-Mode whenever the layout changes and are saved to disk when Attract-Mode is shut down. `boolean`, `integer`, `float`, `string`, `array` and `table` values can be stored in this table.

---

## Classes

### `fe.LayoutGlobals`

This class is a container for global layout settings. The instance of this class is the [`fe.layout`](#felayout) object. This class cannot be otherwise instantiated in a script.

**Properties**

-  `width` - Get/set the layout width. Default value is `ScreenWidth`.
-  `height` - Get/set the layout height. Default value is `ScreenHeight`.
-  `font` - Get/set the filename of the font which will be used for text and listbox objects in this layout.
-  `base_rotation` - Get the base orientation of Attract Mode which is set in General Settings. This property cannot be set from the script. This can be one of the following values:
   -  `RotateScreen.None` (default)
   -  `RotateScreen.Right`
   -  `RotateScreen.Flip`
   -  `RotateScreen.Left`
-  `toggle_rotation` - Get/set the "toggle" orientation of the layout. The toggle rotation is added to the rotation set in general settings to determine what the actual rotation is at any given time. The user can change this value using the Rotation Toggle inputs. This can be one of the following values:
   -  `RotateScreen.None` (default)
   -  `RotateScreen.Right`
   -  `RotateScreen.Flip`
   -  `RotateScreen.Left`
-  `page_size` - Get/set the number of entries to jump each time the `"Next Page"` or `"Previous Page"` button is pressed.
-  `preserve_aspect_ratio` - Get/set whether the overall layout aspect ratio should be preserved by the frontend. Default value is `false`.
-  `time` - Get the number of milliseconds that the layout has been showing.
-  `mouse_pointer` 🔶 - When set to `true` mouse pointer will be visible.

**Member Functions**

-  `redraw()` 🔶 - Adds the ability to process `tick()` and redraw the screen during computationally intensive loops in transition and signal callbacks. DO NOT call this function inside `tick()` callback. It will result in an infinite loop and the frontend will crash.

**Notes**

-  The actual rotation of the layout can be determined using the following equation: `( fe.layout.base_rotation + fe.layout.toggle_rotation ) % 4`

---

### `fe.CurrentList`

This class is a container for status information regarding the current display. The instance of this class is the [`fe.list`](#felist) object. This class cannot be otherwise instantiated in a script.

**Properties**

-  `name` - Get the name of the current display.
-  `display_index` - Get the index of the current display. Use the [`fe.set_display()`](#feset_display) function if you want to change the current display. If this value is less than `0`, then the "Displays Menu" (with a custom layout) is currently showing.
-  `filter_index` - Get/set the index of the currently selected filter, see [`fe.filters`](#fefilters) for the list of available filters.
-  `index` - Get/set the index of the currently selected game.
-  `search_rule` - Get/set the search rule applied to the current game list. If you set this and the resulting search finds no results, then the current game list remains displayed in its entirety. If there are results, then those results are shown instead, until search_rule is cleared or the user navigates away from the display/filter.
-  `size` - Get the size of the current game list. If a search rule has been applied, this will be the number of matches found (if any)
-  `clones_list` 🔶 - Returns `true` if the current list contains game clones.

---

### `fe.ImageCache`

This class is a container for Attract-Mode's internal image cache. The instance of this class is the [`fe.image_cache`](#feimage_cache) object. This class cannot be otherwise instantiated in a script.

**Properties**

-  `count` - Get the number of images currently in the cache.
-  `size` - Get the current size of the image cache (in bytes).
-  `max_size` - Get the (user configured) maximum size of the image cache (in bytes).
-  `bg_load` - Get/set whether images are to be loaded on a background thread. Setting to `true` might make Attract-Mode animations smoother, but can cause a slight flicker as images get loaded. Default value is `false`.

**Member Functions**

-  `add_image( filename )` - Add `filename` image to the internal cache and/or flag it as "most recently used". Least recently used images are cleared from the cache first when space is needed. If filename is contained in an archive, the parameter should be formatted: `"<archive_name>|<filename>"`
-  `name_at( pos )` - Return the name of the image at position `pos` in the internal cache. `pos` can be an integer between `0` and `fe.image_cache.count - 1`.
-  `size_at( pos )` - Return the size (in bytes) of the image at position `pos` in the internal cache. `pos` can be an integer between `0` and `fe.image_cache.count - 1`

---

### `fe.Overlay`

This class is a container for overlay functionality. The instance of this class is the [`fe.overlay`](#feoverlay) object. This class cannot be otherwise instantiated in a script.

**Properties**

-  `is_up` - Get whether the overlay is currently being displayed (i.e. config mode, etc).

**Member Functions**

-  `set_custom_controls( caption_text, options_listbox )`
-  `set_custom_controls( caption_text )`
-  `set_custom_controls()` - Tells the frontend that the layout will provide custom controls for displaying overlay menus such as the exit dialog, displays menu, etc. The `caption_text` parameter is the FeText object that the frontend end should use to display the overlay caption (i.e. `"Exit Attract-Mode?"`). The `options_listbox` parameter is the FeListBox object that the frontend should use to display the overlay options.
-  `clear_custom_controls()` - Tell the frontend that the layout will NOT do any custom control handling for overlay menus. This will result in the frontend using its built-in default menus instead for overlays.
-  `list_dialog( options, title, default_sel, cancel_sel )`
-  `list_dialog( options, title, default_sel )`
-  `list_dialog( options, title )`
-  `list_dialog( options )` - The list_dialog function prompts the user with a menu containing a list of options, returning the index of the selection. The `options` parameter is an array of strings that are the menu options to display in the list. The `title` parameter is a caption for the list. `default_sel` is the index of the entry to be selected initially (default is `0`). `cancel_sel` is the index to return if the user cancels (default is `-1`). The return value is the index selected by the user.
-  `edit_dialog( msg, text )` - Prompt the user to input/edit text. The `msg` parameter is the prompt caption. `text` is the initial text to be edited. The return value a the string of text as edited by the user.
-  `splash_message( msg, replace, footer_msg )`
-  `splash_message( msg, replace )`
-  `splash_message( msg )` - Immediately provide text feedback to the user. This could be useful during computationally-intensive operations. The `msg` parameter may contain a `$1` placeholder that gets replaced by `replace`. The `footer_msg` text is displayed in the footer.

---

### `fe.Display`

This class is a container for information about the available displays. Instances of this class are contained in the [`fe.displays`](#fedisplays) array. This class cannot otherwise be instantiated in a script.

**Properties**

-  `name` - Get the name of the display.
-  `layout` - Get the layout used by this display.
-  `romlist` - Get the romlist used by this display.
-  `in_cycle` - Get whether the display is shown in the prev display/next display cycle.
-  `in_menu` - Get whether the display is shown in the "Displays Menu"

---

### `fe.Filter`

This class is a container for information about the available filters. Instances of this class are contained in the [`fe.filters`](#fefilters) array. This class cannot otherwise be instantiated in a script.

**Properties**

-  `name` - Get the filter name.
-  `index` - Get the index of the currently selected game in this filter.
-  `size` - Get the size of the game list in this filter.
-  `sort_by` - Get the attribute that the game list has been sorted by. Will be equal to one of the following values:
   -  `Info.NoSort`
   -  `Info.Name`
   -  `Info.Title`
   -  `Info.Emulator`
   -  `Info.CloneOf`
   -  `Info.Year`
   -  `Info.Manufacturer`
   -  `Info.Category`
   -  `Info.Players`
   -  `Info.Rotation`
   -  `Info.Control`
   -  `Info.Status`
   -  `Info.DisplayCount`
   -  `Info.DisplayType`
   -  `Info.AltRomname`
   -  `Info.AltTitle`
   -  `Info.Extra`
   -  `Info.Favourite`
   -  `Info.Tags`
   -  `Info.PlayedCount`
   -  `Info.PlayedTime`
   -  `Info.PlayedLast`
   -  `Info.FileIsAvailable`
-  `reverse_order` - [boolean] Will be equal to `true` if the list order has been reversed.
-  `list_limit` - Get the value of the list limit applied to the filter game list.

---

### `fe.Monitor`

This class represents a monitor in Attract-Mode, and provides the interface to the extra monitors in a multi-monitor setup. Instances of this class are contained in the [`fe.monitors`](#femonitors) array. This class cannot otherwise be instantiated in a script.

**Properties**

-  `num` - Get the monitor number.
-  `width` - Get the monitor width in pixels.
-  `height` - Get the monitor height in pixels.

**Member Functions**

-  `add_image()` - Add an image to the end of this monitor's draw list, see [`fe.add_image()`](#feadd_image).
-  `add_artwork()` - Add an artwork to the end of this monitor's draw list, see [`fe.add_artwork()`](#feadd_artwork).
-  `add_clone()` - Add a clone to the end of this monitor's draw list, see [`fe.add_clone()`](#feadd_clone).
-  `add_text()` - Add a text to the end of this monitor's draw list, see [`fe.add_text()`](#feadd_text).
-  `add_listbox()` - Add a listbox to the end of this monitor's draw list, see [`fe.add_listbox()`](#feadd_listbox).
-  `add_surface()` - Add a surface to the end of this monitor's draw list, see [`fe.add_surface()`](#feadd_surface).
-  `add_rectangle()` - Add a rectangle to the end of this monitor's draw list, see [`fe.add_rectangle()`](#feadd_rectangle-).

**Notes**

-  As of this writing, multiple monitor support has not been implemented for the `OS X` version of Attract-Mode.
-  The first entry in the [`fe.monitors`](#femonitors) array is always the _primary_ display for the system.

---

### `fe.Image`

The class representing an image in Attract-Mode. Instances of this class are returned by the [`fe.add_image()`](#feadd_image), [`fe.add_artwork()`](#feadd_artwork), [`fe.add_surface()`](#feadd_surface) and [`fe.add_clone()`](#feadd_clone) functions and also appear in the [`fe.obj`](#feobj) array (the Attract-Mode draw list). This class cannot be otherwise instantiated in a script.

**Properties**

-  `x` - Get/set the x position of the image (in layout coordinates).
-  `y` - Get/set the y position of the image (in layout coordinates).
-  `width` - Get/set the width of the image (in layout coordinates). Setting this property will set `auto_width` to `false`. See [Notes](#artwork-notes).
-  `height` - Get/set the height of the image (in layout coordinates). Setting this property will set `auto_height` to `false`. See [Notes](#artwork-notes).
-  `auto_width` 🔶 - Get/set if using automatic width, which updates `width` to match the current texture. Default is `true`.
-  `auto_height` 🔶 - Get/set if using automatic height, which updates `height` to match the current texture. Default is `true`.
-  `visible` - Get/set whether image is visible (boolean). Default value is `true`.
-  `rotation` - Get/set rotation of image around its rotation origin. Range is `[0...360]`. Default value is `0`.
-  `red` - Get/set red colour level for image. Range is `[0...255]`. Default value is `255`.
-  `green` - Get/set green colour level for image. Range is `[0...255]`. Default value is `255`.
-  `blue` - Get/set blue colour level for image. Range is `[0...255]`. Default value is `255`.
-  `alpha` - Get/set alpha level for image. Range is `[0...255]`. Default value is `255`.
-  `index_offset` - Get/set offset from current selection for the artwork/ dynamic image to display. For example, set to `-1` for the image corresponding to the previous list entry, or `1` for the next list entry, etc. Default value is `0`.
-  `filter_offset` - Get/set filter offset from current filter for the artwork/dynamic image to display. For example, set to `-1` for an image indexed in the previous filter, or `1` for the next filter, etc. Default value is `0`.
-  `skew_x` - Get/set the amount of x-direction image skew (in layout coordinates). Default value is `0`. Use a negative value to skew the image to the left instead.
-  `skew_y` - Get/set the amount of y-direction image skew (in layout coordinates). Default value is `0`. Use a negative value to skew the image up instead.
-  `pinch_x` - Get/set the amount of x-direction image pinch (in layout coordinates). Default value is `0`. Use a negative value to expand towards the bottom instead.
-  `pinch_y` - Get/set the amount of y-direction image pinch (in layout coordinates). Default value is `0`. Use a negative value to expand towards the right instead.
-  `texture_width` - Get the width of the image texture (in pixels), see [Notes](#artwork-notes).
-  `texture_height` - Get the height of the image texture (in pixels), see [Notes](#artwork-notes).
-  `subimg_x` - Get/set the x position of top left corner of the image texture sub-rectangle to display. Default value is `0`.
-  `subimg_y` - Get/set the y position of top left corner of the image texture sub-rectangle to display. Default value is `0`.
-  `subimg_width` - Get/set the width of the image texture sub-rectangle to display. Default value is `texture_width`.
-  `subimg_height` - Get/set the height of the image texture sub-rectangle to display. Default value is `texture_height`.
-  `sample_aspect_ratio` - Get the "sample aspect ratio", which is the width of a pixel divided by the height of the pixel.
-  `origin_x` - (deprecated) Get/set the x position of the local origin for the image. The origin defines the centre point for any positioning or rotation of the image. Default origin is `( 0, 0 )` (top-left corner).
-  `origin_y` - (deprecated) Get/set the y position of the local origin for the image. The origin defines the centre point for any positioning or rotation of the image. Default origin is `( 0, 0 )` (top-left corner).
-  `anchor` 🔶 - Set the midpoint for position and scale. Can be set to one of the following modes:
   -  `Anchor.Left`
   -  `Anchor.Centre`
   -  `Anchor.Right`
   -  `Anchor.Top`
   -  `Anchor.Bottom`
   -  `Anchor.TopLeft` (default)
   -  `Anchor.TopRight`
   -  `Anchor.BottomLeft`
   -  `Anchor.BottomRight`
-  `rotation_origin` 🔶 - Set the midpoint for rotation Can be set to one of the following modes:
   -  `Origin.Left`
   -  `Origin.Centre`
   -  `Origin.Right`
   -  `Origin.Top`
   -  `Origin.Bottom`
   -  `Origin.TopLeft` (default)
   -  `Origin.TopRight`
   -  `Origin.BottomLeft`
   -  `Origin.BottomRight`
-  `anchor_x` 🔶 - Get/set the x position of the midpoint for position and scale. Range is `[0.0...1.0]`. Default value is `0.0`, centre is `0.5`
-  `anchor_y` 🔶 - Get/set the y position of the midpoint for position and scale. Range is `[0.0...1.0]`. Default value is `0.0`, centre is `0.5`
-  `rotation_origin_x` 🔶 - Get/set the x position of the midpoint for rotation. Range is `[0.0...1.0]`. Default value is `0.0`, centre is `0.5`
-  `rotation_origin_y` 🔶 - Get/set the y position of the midpoint for rotation. Range is `[0.0...1.0]`. Default value is `0.0`, centre is `0.5`
-  `video_flags` - _[image & artwork only]_ Get/set video flags for this object. These flags allow you to override Attract-Mode's default video playback behaviour. Can be set to any combination of none or more of the following (i.e. `Vid.NoAudio | Vid.NoLoop`):
   -  `Vid.Default`
   -  `Vid.ImagesOnly` (disable video playback, display images instead)
   -  `Vid.NoAudio` (silence the audio track)
   -  `Vid.NoAutoStart` (don't automatically start video playback)
   -  `Vid.NoLoop` (don't loop video playback)
-  `video_playing` - _[image & artwork only]_ [boolean] Get/set whether video is currently playing in this artwork.
-  `video_duration` - Get the video duration (in milliseconds).
-  `video_time` - Get the time that the video is current at (in milliseconds).
-  `preserve_aspect_ratio` - Get/set whether the aspect ratio from the source image is to be preserved. Default value is `false`.
-  `file_name` - _[image & artwork only]_ Get/set the name of the image/video file being shown. If you set this on an artwork or a dynamic image object it will get reset the next time the user changes the game selection. If file_name is contained in an archive, this string should be formatted: "<archive_name>|<filename>".
-  `shader` - Get/set the GLSL shader for this image. This can only be set to an instance of the class [`fe.Shader`](#feshader), see [`fe.add_shader()`](#feadd_shader).
-  `trigger` - Get/set the transition that triggers updates of this artwork/ dynamic image. Can be set to `Transition.ToNewSelection` or `Transition.EndNavigation`. Default value is `Transition.ToNewSelection`.
-  `smooth` - Get/set whether the image is to be smoothed. Default value can be configured in `attract.cfg`.
-  `zorder` - Get/set the Image's order in the applicable draw list. Objects with a lower zorder are drawn first, so that when objects overlap, the one with the higher zorder is drawn on top. Default value is `0`.
-  `blend_mode` - Get/set the blend mode for this image. Can have one of the following values:
   -  `BlendMode.Alpha` (default for images and artwork)
   -  `BlendMode.Add`
   -  `BlendMode.Screen`
   -  `BlendMode.Multiply`
   -  `BlendMode.Overlay`
   -  `BlendMode.Premultiplied` (default for surfaces)
   -  `BlendMode.None`
-  `mipmap` - Get/set the automatic generation of mipmap for the image/artwork/video. Setting this to `true` greatly improves the quality of scaled down images. The default value is `false`. It's advised to force anisotropic filtering in the display driver settings if the Image with auto generated mipmap is scaled by the ratio that is not isotropic.
-  `volume` 🔶 - Get/set the volume of played video. Range is `[0...100]`
-  `vu` 🔶 - _[video only]_ Get the current VU meter value in mono. Range is `[0.0...1.0]`.
-  `vu_left` 🔶 - _[video only]_ Get the current VU meter value for the left audio channel. Range is `[0.0...1.0]`.
-  `vu_right` 🔶 - _[video only]_ Get the current VU meter value for the right audio channel. Range is `[0.0...1.0]`.
-  `fft` 🔶 - _[video only]_ Get the Fast Fourier Transform data for mono audio as an array of float values. Range is `[0.0...1.0]`. Size of the array is defined by `fft_bands`.
-  `fft_left` 🔶 - _[video only]_ Get the Fast Fourier Transform data for the left audio channel as an array of float values. Range is `[0.0...1.0]`. Size of the array is defined by `fft_bands`.
-  `fft_right` 🔶 - _[video only]_ Get the Fast Fourier Transform data for the right audio channel as an array of float values. Range is `[0.0...1.0]`. Size of the array is defined by `fft_bands`.
-  `fft_bands` 🔶 - _[video only]_ Get/set the Fast Fourier Transform band count. Range is `[2...128]` Default value is `32`.
-  `repeat` 🔶 - Enables texture repeat when set to `true`. Default value is `false`. To see the effect `subimg_width/height` must be set larger than `texture_width/height`
-  `border_scale` 🔶 - Get/set the scaling factor of the border defined by `set_border()`. Default value is `1.0`.
-  `clear` 🔶 - _[surface only]_ When set to `false` surface is not cleared before the next frame. This can be used for various accumulative effects.
-  `redraw` 🔶 - _[surface only]_ When set to `false` surface's content is not redrawn which gives optimization opportunity for hidden surfaces. This in conjunction with `clear = false` can be used to freeze surface's content.

**Member Functions**

-  `set_rgb( r, g, b )` - Set the red, green and blue colour values for the image. Range is `[0...255]`.
-  `set_pos( x, y )` - Set the image position (in layout coordinates).
-  `set_pos( x, y, width, height )` - Set the image position and size (in layout coordinates).
-  `set_anchor( x, y )` 🔶 - Set the midpoint for position and scale x and y are in `[0.0...1.0]` range, centre is `( 0.5, 0.5 )`
-  `set_rotation_origin( x, y )` 🔶 - Set the midpoint for rotation x and y are in `[0.0...1.0]` range, centre is `( 0.5, 0.5 )`
-  `swap( other_img )` - Swap the texture contents of this object (and all of its clones) with the contents of `other_img` (and all of its clones). If an image or artwork is swapped, its video attributes (`video_flags` and `video_playing`) will be swapped as well.
-  `set_border( left, top, right, bottom )` :🔶 - Define border dimensions for 9-slice image. All parameters are in pixels. The borders define constrained regions at the edges of the image, while the centre region scales normally. Follow this link for more information [9-slice](https://en.wikipedia.org/wiki/9-slice_scaling)
-  `set_padding( left, top, right, bottom )` 🔶 - Define padding offsets that extend the sprite beyond its original dimensions. Padding creates additional space around the 9-slice borders. Positive values extend the sprite outward, while negative values bring the edges inward, effectively cropping the border regions. Used only with `set_padding()`
-  `fix_masked_image()` - Takes the colour of the top left pixel in the image and makes all the pixels in the image with that colour transparent.
-  `add_image()` - _[surface only]_ Add an image to the end of this surface's draw list, see [`fe.add_image()`](#feadd_image).
-  `add_artwork()` - _[surface only]_ Add an artwork to the end of this surface's draw list, see [`fe.add_artwork()`](#feadd_artwork).
-  `add_clone()` - _[surface only]_ Add a clone to the end of this surface's draw list, see [`fe.add_clone()`](#feadd_clone).
-  `add_text()` - _[surface only]_ Add a text to the end of this surface's draw list, see [`fe.add_text()`](#feadd_text).
-  `add_listbox()` - _[surface only]_ Add a listbox to the end of this surface's draw list, see [`fe.add_listbox()`](#feadd_listbox).
-  `add_surface()` - _[surface only]_ Add a surface to the end of this surface's draw list, see [`fe.add_surface()`](#feadd_surface).
-  `add_rectangle()` - _[surface only]_ Add a rectangle to the end of this surface's draw list, see [`fe.add_rectangle()`](#feadd_rectangle-).

<a name="artwork-notes">**Notes**</a>

-  Using a negative `width` or `height` will flip the image about its anchor point. To flip an image in-place `subimg` properties can be used:
   ```squirrel
   // flip img vertically
   local img = fe.add_image( "my_image.png" )
   img.subimg_height = img.texture_height * -1
   img.subimg_y = img.texture_height
   ```
-  Attract-Mode defers the loading of artwork and dynamic images (images with [_Magic Tokens_](#magic-tokens)) until after all layout and plug-in scripts have completed running. This means that the `texture_width`, `texture_height` and `file_name` attributes are not available when a layout or plug-in script first adds the image. These attributes become available during transitions such as `Transition.FromOldSelection` and `Transition.ToNewList`:
   ```squirrel
   local art = fe.add_artwork( "snap" )
   // dynamic art texture_width and texture_height are not yet available

   fe.add_transition_callback( "artwork_transition" )
   function artwork_transition( ttype, var, ttime )
   {
     if (( ttype == Transition.FromOldSelection ) || ( ttype == Transition.ToNewList ))
     {
       // dynamic art texture_width and texture_height are now available
       art.subimg_height = texture_height * -1
       art.subimg_y = texture_height
     }
   }
   ```

---

### `fe.Text`

The class representing a text label in Attract-Mode. Instances of this class are returned by the [`fe.add_text()`](#feadd_text) functions and also appear in the [`fe.obj`](#feobj) array (the Attract-Mode draw list). This class cannot be otherwise instantiated in a script.

**Properties**

-  `msg` - Get/set the text label's message. [_Magic Tokens_](#magic-tokens) can be used here.
-  `msg_wrapped` - Get the text label's message after word wrapping.
-  `x` - Get/set x position of top left corner (in layout coordinates).
-  `y` - Get/set y position of top left corner (in layout coordinates).
-  `width` - Get/set width of text (in layout coordinates).
-  `height` - Get/set height of text (in layout coordinates).
-  `visible` - Get/set whether text is visible (boolean). Default value is `true`.
-  `rotation` - Get/set rotation of text. Range is `[0...360]`. Default value is `0`.
-  `red` - Get/set red colour level for text. Range is `[0...255]`. Default value is `255`.
-  `green` - Get/set green colour level for text. Range is `[0...255]`. Default value is `255`.
-  `blue` - Get/set blue colour level for text. Range is `[0...255]`. Default value is `255`.
-  `alpha` - Get/set alpha level for text. Range is `[0...255]`. Default value is `255`.
-  `index_offset` - Get/set offset from current game selection for text info to display. For example, set to `-1` to show text info for the previous list entry, or `1` for the next list entry. Default value is `0`.
-  `filter_offset` - Get/set filter offset from current filter for the text info to display. For example, set to `-1` to show text info for a selection in the previous filter, or `1` for the next filter, etc. Default value is `0`.
-  `bg_red` - Get/set red colour level for text background. Range is `[0...255]`. Default value is `0`.
-  `bg_green` - Get/set green colour level for text background. Range is `[0...255]`. Default value is `0`.
-  `bg_blue` - Get/set blue colour level for text background. Range is `[0...255]`. Default value is `0`.
-  `bg_alpha` - Get/set alpha level for text background. Range is `[0...255]`. Default value is `0` (transparent).
-  `char_size` - Get/set the forced character size. If this is `<= 0` then Attract-Mode will auto-size based on `height`. Default value is `-1`.
-  `glyph_size` - Get the height in pixels of the capital letter. Useful if you want to set the textbox height to match the letter height.
-  `char_spacing` - Get/set the spacing factor between letters. Default value is `1.0`.
-  `line_spacing` - Get/set the spacing factor between lines. Default value is `1.0` At values `0.75` or lower letters start to overlap. For uppercase texts it's around `0.5` It's advised to use this property with the new align modes.
-  `outline` 🔶 - Get/set the thickness of the outline applied to text. Value is set in pixels and can be fractional. Default value is `0.0`
-  `bg_outline` 🔶 - Get/set the thickness of the outline applied to the background. Value is set in pixels and can be fractional. Default value is `0.0`
-  `style` - Get/set the text style. Can be a combination of one or more of the following (i.e. `Style.Bold | Style.Italic`):
   -  `Style.Regular` (default)
   -  `Style.Bold`
   -  `Style.Italic`
   -  `Style.Underlined`
   -  `Style.StrikeThrough` 🔶
-  `justify` 🔶 - Get/set the text justification. Can be one of the following values:
   -  `Justify.None` - No justification (default)
   -  `Justify.Word` - Increase space between words to fill line.
   -  `Justify.Character` - Increase space between characters to fill line.
-  `align` - Get/set the text alignment. Can be one of the following values:
   -  ~~`Align.Centre`~~ (default)
   -  ~~`Align.Left`~~
   -  ~~`Align.Right`~~
   -  `Align.TopCentre`
   -  `Align.TopLeft`
   -  `Align.TopRight`
   -  `Align.BottomCentre`
   -  `Align.BottomLeft`
   -  `Align.BottomRight`
   -  `Align.MiddleCentre`
   -  `Align.MiddleLeft`
   -  `Align.MiddleRight`
   -  The last 3 alignment modes have the same function as the first 3, but they are more accurate. The first 3 modes are preserved for compatibility.
-  `word_wrap` - Get/set whether word wrapping is enabled in this text (boolean). Default is `false`.
-  `msg_width` - Get the width of the text message, in layout coordinates.
-  `msg_height` 🔶 - Get the height of the text message, in layout coordinates.
-  `lines` 🔶 - Get the maximum line count that can be fitted inside the text box.
-  `lines_total` 🔶 - Get the total line count of the formatted text message.
-  `line_height` 🔶 - Get the distance between two lines of text in layout coordinates.
-  `first_line_hint` 🔶 - Get/set the line in the formatted text that is shown as first line in the text object
-  `font` - Get/set the filename of the font used for this text. If not set default font is used.
-  `margin` - Get/set the margin spacing in pixels to sides of the text. Default value is `-1` which calculates the margin based on the `char_size`.
-  `shader` - Get/set the GLSL shader for this text. This can only be set to an instance of the class [`fe.Shader`](#feshader), see [`fe.add_shader()`](#feadd_shader).
-  `zorder` - Get/set the Text's order in the applicable draw list. Objects with a lower zorder are drawn first, so that when objects overlap, the one with the higher zorder is drawn on top. Default value is `0`.

**Member Functions**

-  `set_rgb( r, g, b )` - Set the red, green and blue colour values for the text. Range is `[0...255]`.
-  `set_bg_rgb( r, g, b )` - Set the red, green and blue colour values for the text background. Range is `[0...255]`.
-  `set_outline_rgb( r, g, b )` 🔶 - Set the red, green and blue colour values for the text outline. Range is `[0...255]`.
-  `set_bg_outline_rgb()` 🔶 - Set the red, green and blue colour values for the outline of the text background. Range is `[0...255]`.
-  `set_pos( x, y )` - Set the text position (in layout coordinates).
-  `set_pos( x, y, width, height )` - Set the text position and size (in layout coordinates).

---

### `fe.ListBox`

The class representing the listbox in Attract-Mode. Instances of this class are returned by the [`fe.add_listbox()`](#feadd_listbox) functions and also appear in the [`fe.obj`](#feobj) array (the Attract-Mode draw list). This class cannot be otherwise instantiated in a script.

**Properties**

-  `x` - Get/set x position of top left corner (in layout coordinates).
-  `y` - Get/set y position of top left corner (in layout coordinates).
-  `width` - Get/set width of listbox (in layout coordinates).
-  `height` - Get/set height of listbox (in layout coordinates).
-  `visible` - Get/set whether listbox is visible (boolean). Default value is `true`.
-  `rotation` - Get/set rotation of listbox. Range is `[0...360]`. Default value is `0`.
-  `red` - Get/set red colour level for text. Range is `[0...255]`. Default value is `255`.
-  `green` - Get/set green colour level for text. Range is `[0...255]`. Default value is `255`.
-  `blue` - Get/set blue colour level for text. Range is `[0...255]`. Default value is `255`.
-  `alpha` - Get/set alpha level for text. Range is `[0...255]`. Default value is `255`.
-  `index_offset` - Not used.
-  `filter_offset` - Get/set filter offset from current filter for the text info to display. For example, set to `-1` to show info for the previous filter, or `1` for the next filter, etc. Default value is `0`.
-  `bg_red` - Get/set red colour level for background. Range is `[0...255]`. Default value is `0`.
-  `bg_green` - Get/set green colour level for background. Range is `[0...255]`. Default value is `0`.
-  `bg_blue` - Get/set blue colour level for background. Range is `[0...255]`. Default value is `0`.
-  `bg_alpha` - Get/set alpha level for background. Range is `[0...255]`. Default value is `0` (transparent).
-  `sel_red` - Get/set red colour level for selection text. Range is `[0...255]`. Default value is `255`.
-  `sel_green` - Get/set green colour level for selection text. Range is `[0...255]`. Default value is `255`.
-  `sel_blue` - Get/set blue colour level for selection text. Range is `[0...255]`. Default value is `0`.
-  `sel_alpha` - Get/set alpha level for selection text. Range is `[0...255]`. Default value is `255`.
-  `selbg_red` - Get/set red colour level for selection background. Range is `[0...255]`. Default value is `0`.
-  `selbg_green` - Get/set green colour level for selection background. Range is `[0...255]`. Default value is `0`.
-  `selbg_blue` - Get/set blue colour level for selection background. Range is `[0...255]`. Default value is `255`.
-  `selbg_alpha` - Get/set alpha level for selection background. Range is `[0...255]`. Default value is `255`.
-  `rows` - Get/set the number of listbox rows. Default value is `11`.
-  `list_size` - Get the size of the list shown by listbox. When listbox is assigned as an overlay custom control this property will return the number of options available in the overlay dialog. This property is updated during `Transition.ShowOverlay`
-  `char_size` - Get/set the forced character size. If this is `<= 0` then Attract-Mode will auto-size based on the value of `height`/`rows`. Default value is `-1`.
-  `glyph_size` - Get the height in pixels of the capital letter.
-  `char_spacing` - Get/set the spacing factor between letters. Default value is `1.0`.
-  `style` - Get/set the text style. Can be a combination of one or more of the following (i.e. `Style.Bold | Style.Italic`):
   -  `Style.Regular` (default)
   -  `Style.Bold`
   -  `Style.Italic`
   -  `Style.Underlined`
   -  `Style.StrikeThrough` 🔶
-  `justify` 🔶 - Get/set the text justification. Can be one of the following values:
   -  `Justify.None` - No justification (default)
   -  `Justify.Word` - Increase space between words to fill line.
   -  `Justify.Character` - Increase space between characters to fill line.
-  `align` - Get/set the text alignment. Can be one of the following values:
   -  ~~`Align.Centre`~~ (default)
   -  ~~`Align.Left`~~
   -  ~~`Align.Right`~~
   -  `Align.TopCentre`
   -  `Align.TopLeft`
   -  `Align.TopRight`
   -  `Align.BottomCentre`
   -  `Align.BottomLeft`
   -  `Align.BottomRight`
   -  `Align.MiddleCentre`
   -  `Align.MiddleLeft`
   -  `Align.MiddleRight`
   -  The last 3 alignment modes have the same function as the first 3, but they are more accurate. The first 3 modes are preserved for compatibility.
-  `sel_style` - Get/set the selection text style. Can be a combination of one or more of the following (i.e. `Style.Bold | Style.Italic`):
   -  `Style.Regular` (default)
   -  `Style.Bold`
   -  `Style.Italic`
   -  `Style.Underlined`
   -  `Style.StrikeThrough` 🔶
-  `sel_mode` - Get/set the selection mode. Controls how the ListBox behaves when navigating. Can be one of the following values:
   -  `Selection.Static` (default) - The selection stays in the centre of the ListBox. The list scrolls
   -  `Selection.Moving` - The selection moves and the list scrolls when margin is reached. Margin can be adjusted with `sel_margin`
   -  `Selection.Paged` - The selection moves and the list scrolls in pages
-  `sel_margin` - Get/set the selection margin in rows. When using `Selection.Moving` mode, the list will scroll to keep the selection at least this many rows away from the edges.
-  `sel_row` - Returns the index of the row that is currently selected within the visible rows of the ListBox.
-  `font` - Get/set the filename of the font used for this listbox. If not set default font is used.
-  `margin` - Get/set the margin spacing in pixels to sides of the text. Default value is `-1` which calculates the margin based on the .char_size.
-  `format_string` - Get/set the format for the text to display in each list entry. [_Magic Tokens_](#magic-tokens) can be used here. If empty, game titles will be displayed (i.e. the same behaviour as if set to `"[Title]"`). Default is an empty value.
-  `shader` - Get/set the GLSL shader for this listbox. This can only be set to an instance of the class [`fe.Shader`](#feshader), see [`fe.add_shader()`](#feadd_shader).
-  `zorder` - Get/set the Listbox's order in the applicable draw list. Objects with a lower zorder are drawn first, so that when objects overlap, the one with the higher zorder is drawn on top. Default value is `0`.

**Member Functions**

-  `set_rgb( r, g, b )` - Set the red, green and blue colour values for the text. Range is `[0...255]`.
-  `set_bg_rgb( r, g, b )` - Set the red, green and blue colour values for the text background. Range is `[0...255]`.
-  `set_sel_rgb( r, g, b )` - Set the red, green and blue colour values for the selection text. Range is `[0...255]`.
-  `set_selbg_rgb( r, g, b )` - Set the red, green and blue colour values for the selection background. Range is `[0...255]`.
-  `set_pos( x, y )` - Set the listbox position (in layout coordinates).
-  `set_pos( x, y, width, height )` - Set the listbox position and size (in layout coordinates).

---

### `fe.Rectangle` 🔶

The class representing a rectangle in Attract-Mode. Instances of this class are returned by the [`fe.add_rectangle()`](#feadd_rectangle-) function and also appear in the [`fe.obj`](#feobj) array (the Attract-Mode draw list). This class cannot be otherwise instantiated in a script.

**Properties**

-  `x` - Get/set the x position of the rectangle (in layout coordinates).
-  `y` - Get/set the y position of the rectangle (in layout coordinates).
-  `width` - Get/set the width of the rectangle (in layout coordinates).
-  `height` - Get/set the height of the rectangle (in layout coordinates).
-  `visible` - Get/set whether the rectangle is visible (boolean). Default value is `true`.
-  `rotation` - Get/set rotation of the rectangle around its origin. Range is `[0...360]`. Default value is `0`.
-  `red` - Get/set red colour level for the rectangle. Range is `[0...255]`. Default value is `255`.
-  `green` - Get/set green colour level for the rectangle. Range is `[0...255]`. Default value is `255`.
-  `blue` - Get/set blue colour level for the rectangle. Range is `[0...255]`. Default value is `255`.
-  `alpha` - Get/set alpha level for the rectangle. Range is `[0...255]`. Default value is `255`.
-  `outline` Get/set the thickness of the outline applied to the rectangle. Value is set in pixels and can be fractional. Default value is `0.0`
-  `outline_red` - Get/set red colour level for the rectangle's outline. Range is `[0...255]`. Default value is `255`.
-  `outline_green` - Get/set green colour level for the rectangle's outline. Range is `[0...255]`. Default value is `255`.
-  `outline_blue` - Get/set blue colour level for the rectangle's outline. Range is `[0...255]`. Default value is `255`.
-  `outline_alpha` - Get/set alpha level for the rectangle's outline. Range is `[0...255]`. Default value is `255`.
-  `origin_x` - (deprecated) Get/set the x position of the local origin for the rectangle. The origin defines the centre point for any positioning or rotation of the rectangle. Default origin in `( 0, 0 )` (top-left corner).
-  `origin_y` - (deprecated) Get/set the y position of the local origin for the rectangle. The origin defines the centre point for any positioning or rotation of the rectangle. Default origin is `( 0, 0 )` (top-left corner).
-  `anchor` Set the midpoint for position and scale. Can be set to one of the following modes:
   -  `Anchor.Left`
   -  `Anchor.Centre`
   -  `Anchor.Right`
   -  `Anchor.Top`
   -  `Anchor.Bottom`
   -  `Anchor.TopLeft` (default)
   -  `Anchor.TopRight`
   -  `Anchor.BottomLeft`
   -  `Anchor.BottomRight`
-  `rotation_origin` Set the midpoint for rotation Can be set to one of the following modes:
   -  `Origin.Left`
   -  `Origin.Centre`
   -  `Origin.Right`
   -  `Origin.Top`
   -  `Origin.Bottom`
   -  `Origin.TopLeft` (default)
   -  `Origin.TopRight`
   -  `Origin.BottomLeft`
   -  `Origin.BottomRight`
-  `anchor_x` - Get/set the x position of the midpoint for position and scale. Range is `[0.0...1.0]`. Default value is `0.0`, centre is `0.5`
-  `anchor_y` - Get/set the y position of the midpoint for position and scale. Range is `[0.0...1.0]`. Default value is `0.0`, centre is `0.5`
-  `rotation_origin_x` - Get/set the x position of the midpoint for rotation. Range is `[0.0...1.0]`. Default value is `0.0`, centre is `0.5`
-  `rotation_origin_y` - Get/set the y position of the midpoint for rotation. Range is `[0.0...1.0]`. Default value is `0.0`, centre is `0.5`
-  `corner_radius` - Get/set the corner radius (in layout coordinates). This property will adjust the radius to preserve corner roundness when set. Default value is `0.0`.
-  `corner_radius_x` - Get/set the corner x radius (in layout coordinates). Default value is `0.0`.
-  `corner_radius_y` - Get/set the corner y radius (in layout coordinates). Default value is `0.0`.
-  `corner_ratio` - Get/set the corner radius as a fraction of the smallest side. This property will adjust the radius to preserve corner roundness when set. Range is `[0.0...0.5]`. Default value is `0.0`.
-  `corner_ratio_x` - Get/set the corner x radius as a fraction of the width. Range is `[0.0...0.5]`. Default value is `0.0`.
-  `corner_ratio_y` - Get/set the corner y radius as a fraction of the height. Range is `[0.0...0.5]`. Default value is `0.0`.
-  `corner_points` - Get/set the number of points used to draw the corner radius. More points produce smooth curves, while fewer points result in flat bevels. Range is `[1...32]`. Default value is `12`.
-  `shader` - Get/set the GLSL shader for this rectangle. This can only be set to an instance of the class [`fe.Shader`](#feshader), see [`fe.add_shader()`](#feadd_shader).
-  `zorder` - Get/set the rectangles's order in the applicable draw list. Objects with a lower zorder are drawn first, so that when objects overlap, the one with the higher zorder is drawn on top. Default value is `0`.
-  `blend_mode` - Get/set the blend mode for this rectangle. Can have one of the following values:
   -  `BlendMode.Alpha`
   -  `BlendMode.Add`
   -  `BlendMode.Screen`
   -  `BlendMode.Multiply`
   -  `BlendMode.Overlay`
   -  `BlendMode.Premultiplied`
   -  `BlendMode.None`

**Member Functions**

-  `set_rgb( r, g, b )` - Set the red, green and blue colour values for the rectangle. Range is `[0...255]`.
-  `set_pos( x, y )` - Set the rectangle position (in layout coordinates).
-  `set_pos( x, y, width, height )` - Set the rectangle position and size (in layout coordinates).
-  `set_outline_rgb( r, g, b )` - Set the red, green and blue colour values for the rectangle outline. Range is `[0...255]`.
-  `set_anchor( x, y )` - Set the midpoint for position and scale x and y are in `[0.0...1.0]` range, centre is `( 0.5, 0.5 )`.
-  `set_rotation_origin( x, y )` - Set the midpoint for rotation x and y are in `[0.0...1.0]` range, centre is `( 0.5, 0.5 )`.
-  `set_corner_radius( x, y )` - Set the corner x and y radius (in layout coordinates).
-  `set_corner_ratio( x, y )` - Set the corner x and y radius as a fraction of the width and height. Range is `[0.0...0.5]`.

---

### `fe.Sound`

The class representing a sound object. Instances of this class are returned by the [`fe.add_sound()`](#feadd_sound) function. This class cannot be otherwise instantiated in a script.

**Properties**

-  `file_name` - Get/set the sound filename.
-  `volume` 🔶 - Get/set the volume of played sound. Range is `[0...100]`.
-  `playing` - Get/set whether the sound is currently playing (boolean).
-  `loop` - Get/set whether the sound should be looped (boolean).
-  `pitch` - Get/set the sound pitch (float). Default value is `1`.
-  `x` - Get/set the x position of the sound. Default value is `0`.
-  `y` - Get/set the y position of the sound. Default value is `0`.
-  `z` - Get/set the z position of the sound. Default value is `0`.
-  `duration` - Get the sound duration (in milliseconds).
-  `time` - Get the time that the sound is current at (in milliseconds).

---

### `fe.Music` 🔶

The class representing an audio track. Instances of this class are returned by the [`fe.add_music()`](#feadd_music-) function. This is also the class for the `fe.ambient_sound` object. Object of this class cannot be otherwise instantiated in a script.

**Properties**

-  `file_name` - Get/set the audio track filename.
-  `volume` - Get/set the volume of played audio track. Range is `[0...100]`
-  `playing` - Get/set whether the audio track is currently playing (boolean).
-  `loop` - Get/set whether the audio track should be looped (boolean).
-  `pitch` - Get/set the audio track pitch (float). Default value is `1`.
-  `x` - Get/set the x position of the audio track. Default value is `0`.
-  `y` - Get/set the y position of the audio track. Default value is `0`.
-  `z` - Get/set the z position of the audio track. Default value is `0`.
-  `duration` - Get the audio track duration (in milliseconds).
-  `time` - Get the time that the audio track is current at (in milliseconds).
-  `vu` - Get the current VU meter value in mono. Range is `[0.0...1.0]`.
-  `vu_left` - Get the current VU meter value for the left audio channel. Range is `[0.0...1.0]`.
-  `vu_right` - Get the current VU meter value for the right audio channel. Range is `[0.0...1.0]`.
-  `fft` - Get the Fast Fourier Transform data for mono audio as an array of float values. Range is `[0.0...1.0]`. Size of the array is defined by `fft_bands`.
-  `fft_left` - Get the Fast Fourier Transform data for the left audio channel as an array of float values. Range is `[0.0...1.0]`. Size of the array is defined by `fft_bands`.
-  `fft_right` - Get the Fast Fourier Transform data for the right audio channel as an array of float values. Range is `[0.0...1.0]`. Size of the array is defined by `fft_bands`.
-  `fft_bands` - Get/set the Fast Fourier Transform band count. Range is `[2...128]` Default value is `32`.

**Member Functions**

-  `get_metadata( tag )` - Get the meta data (if available in the source file) that corresponds to the specified tag (i.e. `"artist"`, `"album"`, etc.)

---

### `fe.Shader`

The class representing a GLSL shader. Instances of this class are returned by the [`fe.add_shader()`](#feadd_shader) function. This class cannot be otherwise instantiated in a script.

**Properties**

-  `type` - Get the shader type. Can be one of the following values:
   -  `Shader.VertexAndFragment`
   -  `Shader.Vertex`
   -  `Shader.Fragment`
   -  `Shader.Empty`

**Member Functions**

-  `set_param( name, f )` - Set the float variable (float GLSL type) with the specified name to the value of `f`.
-  `set_param( name, f1, f2 )` - Set the 2-component vector variable (vec2 GLSL type) with the specified name to `(f1, f2)`.
-  `set_param( name, f1, f2, f3 )` - Set the 3-component vector variable (vec3 GLSL type) with the specified name to `(f1, f2, f3)`.
-  `set_param( name, f1, f2, f3, f4 )` - Set the 4-component vector variable (vec4 GLSL type) with the specified name to `(f1, f2, f3, f4)`.
-  `set_texture_param( name )` - Set the texture variable (sampler2D GLSL type) with the specified `name`. The texture used will be the texture for whatever object ([`fe.Image`](#feimage), [`fe.Text`](#fetext), [`fe.Listbox`](#felistbox)) the shader is drawing.
-  `set_texture_param( name, image )` - Set the texture variable (sampler2D GLSL type) with the specified `name` to the texture contained in `image`. `image` must be an instance of the [`fe.Image`](#feimage) class.

---

## Constants

-  `FeVersion` - [string] The current Attract-Mode version.
-  `FeVersionNum` - [int] The current Attract-Mode version.
-  `FeConfigDirectory` - [string] The path to Attract-Mode's config directory.
-  `IntroActive` - [boolean] `true` if the intro is active, `false` otherwise.
-  `Language` - [string] The configured language.
-  `OS` - [string] The Operating System that Attract-Mode is running under, will be one of: `"Windows"`, `"OSX"`, `"FreeBSD"`, `"Linux"`, or `"Unknown"`.
-  `ScreenWidth` - [int] The screen width in pixels.
-  `ScreenHeight` - [int] The screen height in pixels.
-  `ScreenRefreshRate` - [int] The refresh rate of the main screen in Hz.
-  `ScreenSaverActive` - [boolean] `true` if the screen saver is active, `false` otherwise.
-  `ShadersAvailable` - [boolean] `true` if GLSL shaders are available on this system, `false` otherwise.
