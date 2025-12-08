# <img src="https://github.com/oomek/attractplus/blob/master/resources/images/Logo.png?raw=true" width="40" align="left"> Attract-Mode Plus Coding Reference

> Get the [_AM+ Squirrel_][extension] extension for VS Code
> <br><sup><sub>A suite of support tools to enhance your AM+ development experience. Code completions, highlighting, linting, formatting, and more!</sub></sup>

[extension]: https://marketplace.visualstudio.com/items?itemName=chadnaut.am-squirrel

## Contents

> Features unique to Attract-Mode Plus are marked with a 🔶 symbol.

-  [Overview](#overview)
   -  [Layout](#layout)
   -  [Intro](#intro)
   -  [Screensaver](#screensaver)
   -  [Plugin](#plugin)
   -  [Module](#module)
   -  [Squirrel Language](#squirrel-language)
-  [Common Structures](#common-structures)
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
   -  [`fe.compile_shader()`](#fecompile_shader-) 🔶
   -  [`fe.add_sound()`](#feadd_sound)
   -  [`fe.add_music()`](#feadd_music-) 🔶
   -  [`fe.add_ticks_callback()`](#feadd_ticks_callback)
   -  [`fe.add_transition_callback()`](#feadd_transition_callback)
   -  [`fe.get_game_info()`](#feget_game_info)
   -  [`fe.set_game_info()`](#feset_game_info) 🔶
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
   -  [`fe.get_input_mappings()`](#feget_input_mappings-) 🔶
   -  [`fe.get_general_config()`](#feget_general_config-) 🔶
   -  [`fe.get_config()`](#feget_config)
   -  [`fe.set_config()`](#feset_config) 🔶
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
-  [File System Functions](#file-system-functions)
   -  [`fs.path_expand()`](#fspath_expand)
   -  [`fs.path_test()`](#fspath_test)
   -  [`fs.get_file_mtime()`](#fsget_file_mtime-) 🔶
   -  [`fs.set_file_mtime()`](#fsset_file_mtime-) 🔶
   -  [`fs.get_dir()`](#fsget_dir-) 🔶
   -  [`fs.make_dir()`](#fsmake_dir-) 🔶
   -  [`fs.copy_file()`](#fscopy_file-) 🔶
-  [Constants](#constants)
-  [Language Extensions](#language-extensions)
   -  [Math](#math-) 🔶
   -  [Vector](#vector-) 🔶
   -  [Easing](#easing-) 🔶

---

## Overview

### Layout

Layouts define what gets displayed to the user. They consist of a `layout.nut` file and a collection of related resources (images, other scripts, etc.). Layouts are located in the `layouts/` directory, each in its own separate subdirectory or [archive](#language-extensions).

Layouts can have one or more `layout*.nut` files, which may be cycled between using the "Toggle Layout" command.

```squirrel
local flw = fe.layout.width
local flh = fe.layout.height

// Background movie artwork
local img = fe.add_artwork( "snap", 0, 0, flw, flh )
img.alpha = 70

// Game listbox
local lb = fe.add_listbox( 0, 0, flw, flh )
```

> _The default Layout - it doesn't get much simpler than this!_

### Intro

The `Intro` is a Layout that gets run when the frontend first starts, and exits as soon as any action is triggered, such as pressing the select button. The Intro script is located at `intro/intro.nut`.

### Screensaver

The `Screensaver` is a Layout that gets loaded after a user-configured period of inactivity. The Screensaver script is located at `screensaver/screensaver.nut`.

### Plugin

Plugins are Layouts that provide additional features to _every_ Display. Plugins are located in the `plugins/` directory, in a single `.nut` file, or in their own subdirectory (or archive) with a file named `plugin.nut`.

### Module

Modules are shared libraries that provide functionality to any Layout that requires it using [`fe.load_module()`](#feload_module). Modules are located in the `modules/` directory, in a single `.nut` file, or in their own subdirectory.

---

## Squirrel Language

Attract-Mode Plus Layouts are written in [Squirrel](http://www.squirrel-lang.org/), a high level imperative, object-oriented programming language. Squirrel's syntax is similar to C/C++/Java, but the language has a very dynamic nature like Python/Lua. For more information on programming in Squirrel consult the following resources:

-  [Squirrel 3.0 Reference Manual](http://www.squirrel-lang.org/doc/squirrel3.html)
-  [Squirrel 3.0 Standard Library Manual](https://web.archive.org/web/20210730072847/http://www.squirrel-lang.org/doc/sqstdlib3.html)
-  [Introduction to Squirrel Programming](https://github.com/mickelson/attract/wiki/Introduction-to-Squirrel-Programming)
-  [AM+ Squirrel VSCode Extension][extension]

---

### Magic Tokens

[`fe.Image`](#feimage) names, as well as the messages displayed by [`fe.Text`](#fetext) and [`fe.Listbox`](#felistbox) objects, can contain one or more _Magic Tokens_. _Magic Tokens_ are enclosed in square brackets, and the frontend automatically updates them as the user navigates the layout. For example, a Text message set to `"[Title]"` will be automatically updated with the appropriate game's Title.

```squirrel
// A Text element that displays the current game's Title as well as it's List position
fe.add_text( "[Title] - [ListEntry] of [ListSize]", 0, 0, 400, 20 )
```

The following _Magic Tokens_ are supported:

-  List
   -  `[DisplayName]` - The name of the Display
   -  `[FilterName]` - The name of the Filter
   -  `[ListSize]` - The number of items in the list
   -  `[SortName]` - The attribute the list was sorted by
   -  `[Search]` - The search rule applied to the list
-  Game
   -  `[Name]` - The short name of the game
   -  `[Title]` - The formatted name of the game
   -  `[TitleLetter]` - The capitalised first letter of the formatted name of the game
   -  `[TitleFull]` - The full pre-formatted name of the game
   -  `[Emulator]` - The emulator to use for the game
   -  `[CloneOf]` - The short name of the game's parent
   -  `[Year]` - The year for the game
   -  `[Manufacturer]` - The manufacturer of the game
   -  `[Category]` - The category for the game
   -  `[Players]` - The number of players for the game
   -  `[Rotation]` - The display rotation for the game
   -  `[Control]` - The primary control for the game
   -  `[Status]` - The emulation status for the game
   -  `[DisplayCount]` - The number of displays used by the game
   -  `[DisplayType]` - The display type for the game
   -  `[AltRomname]` - The alternative rom name for the game
   -  `[AltTitle]` - The alternative title for the game
   -  `[Extra]` - The extra information for the game
   -  `[Buttons]` - The number of buttons used by the game
   -  `[Series]` - The series the game belongs to
   -  `[Language]` - The language used by the game
   -  `[Region]` - The region the game belongs to
   -  `[Rating]` - The age rating for the game
   -  `[Overview]` - The overview description for the game
   -  `[System]` - The first System name for the game's emulator
   -  `[SystemN]` - The last System name for the game's emulator
   -  `[ListEntry]` - The index of the game within the list
   -  `[SortValue]` - The value used to sort the game in the list
-  Tags
   -  `[Favourite]` - The string `1` if the game has the favourite status
   -  `[FavouriteStar]` - The `★` icon if the game has the favourite status
   -  `[FavouriteStarAlt]` - The `☆` icon if the game has the favourite status
   -  `[FavouriteHeart]` - The `♥` icon if the game has the favourite status
   -  `[FavouriteHeartAlt]` - The `♡` icon if the game has the favourite status
   -  `[Tags]` - The tags attached to the game, delimited by `;` semicolons
   -  `[TagList]` - The tags attached to the game, formatted with `🏷` icons
-  Stats
   -  `[PlayedCount]` - The number of times the game has been played
   -  `[PlayedTime]` - The number of seconds the game has been played
   -  `[PlayedLast]` - The timestamp the game was last played
   -  `[PlayedAgo]` - The last played date formatted relative to now, for example: `5 Minutes Ago`
   -  `[Score]` - The user score for the game. Range is `[0.0...5.0]`
   -  `[ScoreStar]` - The score displayed as a number of stars, for example: `★★★`
   -  `[ScoreStarAlt]` - The score displayed as a number of stars plus empty slots, for example: `★★★☆☆`
   -  `[Votes]` - The total number of user votes for the game

#### Custom Magic Token Functions

_Magic Tokens_ can also run user-defined functions in the form `[!token_function]`. When used, Attract-Mode Plus will run the corresponding `token_function()` defined in the Squirrel "root table". The function may contain up to two parameters, and must return a string value to replace the _Magic Token_.

-  `index_offset` - The offset (from the current selection) of the game.
-  `filter_offset` - The offset (from the current filter) of the filter.

```squirrel
// Return the first word in the Manufacturer name
function manufacturer_name()
{
  local m = fe.get_game_info( Info.Manufacturer )
  return split( m, " " )[0]
}

// An Image element based on the Manufacturer name (ie: "Atari.png", "Nintendo.png")
fe.add_image( "[!manufacturer_name].png", 0, 0 )
```

```squirrel
// Return a copyright message if both Manufacturer and Year exist
// Otherwise just return the Manufacturer's name
function copyright( index_offset, filter_offset )
{
  local m = fe.get_game_info( Info.Manufacturer, index_offset, filter_offset )
  local y = fe.get_game_info( Info.Year, index_offset, filter_offset )

  if (( m.len() > 0 ) && ( y.len() > 0 ))
  {
	 return "Copyright " + y + ", " + m
  }

  return m
}

// A Text element displaying the copyright
fe.add_text( "[!copyright]", 0, 0, 400, 20 )
```

---

### User Config

Configuration settings can be added to a layout/plugin/screensaver/intro to provide users with customization options.

Configurations are defined by a `UserConfig` class at the top of your script, where each property is an individual setting. Properties should be prefixed with an `</ attribute />` that describes how they are displayed, and their values can be retrieved using [`fe.get_config()`](#feget_config) or updated using [`fe.set_config()`](#feset_config-).

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
-  `is_function` - [boolean] Call the function named by the property, see example below.
-  `is_info` - [boolean] A readonly setting used for headings or separators.
-  If none of the above are used a text input for keyboard entry will be displayed.

**Callback**

-  The `is_function` callback should be in the following form:

```squirrel
class UserConfig </ help="Description" /> {
	</ label="String", order=1 /> val = "Default"
	</ label="Action", order=2, is_function=true /> func = "callback"
}

// The parameter contains the fe.get_config() table
function callback( config )
{
	// The config may be updated
	config.val = ( config.val == "Default" ) ? "Changed" : "Default"

	// The returned string is displayed in the help section
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

Adds an image or video to the end of the draw list.

The default `blend_mode` for images is `BlendMode.Alpha`

**Parameters**

-  `name` - The name of an image/video file to show. If a relative path is provided (i.e. `"bg.png"`) it is assumed to be relative to the current layout directory (or the plugin directory, if called from a plugin script). If a relative path is provided and the layout/plugin is contained in an archive, Attract-Mode Plus will open the corresponding file stored inside of the archive. Supported image formats are: `PNG`, `JPEG`, `GIF`, `BMP` and `TGA`. Videos can be in any format supported by FFmpeg. One or more [_Magic Tokens_](#magic-tokens) can be used in the name, in which case Attract-Mode Plus will automatically update the image/video file appropriately in response to user navigation. For example `"man/[Manufacturer]"` will load the file corresponding to the manufacturer's name from the man subdirectory of the layout/plugin (example: `"man/Konami.png"`). When Magic Tokens are used, the file extension specified in `name` is ignored (if present) and Attract-Mode Plus will load any supported media file that matches the Magic Token.
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

Add an artwork to the end of the draw list. The image/video displayed in an artwork is updated automatically whenever the user changes the game selection.

The default `blend_mode` for artwork is `BlendMode.Alpha`

**Parameters**

-  `label` - The label of the artwork to display. This should correspond to an artwork configured in Attract-Mode Plus (artworks are configured per emulator in the config menu) or scraped using the scraper. Standard artwork labels are: `"snap"`, `"marquee"`, `"flyer"`, `"wheel"`, and `"fanart"`.
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

Add a surface to the end of the draw list. A surface is an off-screen texture upon which you can draw other [`Image`](#feadd_image), [`Artwork`](#feadd_artwork), [`Text`](#feadd_text), [`Listbox`](#feadd_listbox) and [`Surface`](#feadd_surface) objects. The resulting texture is treated as a static image by Attract-Mode Plus which can in turn have image effects applied to it (`scale`, `position`, `pinch`, `skew`, `shaders`, etc) when it is drawn.

A surface's texture size is fixed upon creation. Later changes to `width` or `height` will not change the texture's dimensions.

The default `blend_mode` for surfaces is `BlendMode.Premultiplied`

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

Clone an image, artwork or surface object and add the clone to the back of the draw list. The texture pixel data of the original and clone is shared as a result.

**Parameters**

-  `img` - The image, artwork or surface object to clone. This needs to be an instance of the class [`fe.Image`](#feimage).

**Return Value**

-  An instance of the class [`fe.Image`](#feimage) which can be used to interact with the added clone.

---

### `fe.add_text()`

```squirrel
fe.add_text( msg, x, y, w, h )
```

Add a text label to the end of the draw list.

**Parameters**

-  `msg` - The text to display. [_Magic Tokens_](#magic-tokens) can be used here, in which case Attract-Mode Plus will dynamically update the msg in response to navigation.
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

Add a listbox to the end of the draw list.

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

Add a rectangle to the end of the draw list.

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

Compile a GLSL shader from the given file(s) for use in the layout. Also see [`fe.compile_shader()`](#fecompile_shader-).

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

**Notes**

Windows caches compiled shaders for future recall, and since every change results in a new file this cache will continue to grow over time. When it reaches a limit (`2GB` by default, ie: *thousands* of files) new shaders will not be cached, causing slowdowns in your Layout.

To clear this cache close the program and delete the contents of the cache folders.

- AMD: `C:\Users\<name>\AppData\Local\AMD\GLCache\`
- nVidia: `C:\Users\<name>\AppData\Local\NVIDIA\GLCache\`

---

### `fe.add_sound()`

```squirrel
fe.add_sound( name )
```

Add a sound file that can then be played by Attract-Mode Plus. For short sounds, stored in RAM. For playing long audio tracks use [`fe.add_music()`](#feadd_music-).

**Parameters**

-  `name` - The name of the sound file. If a relative path is provided, it is treated as relative to the directory for the layout/plugin that called this function.

**Return Value**

-  An instance of the class [`fe.Sound`](#fesound) which can be used to interact with the sound.

---

### `fe.add_music()` 🔶

```squirrel
fe.add_music( name )
```

Add an audio track that can then be played by Attract-Mode Plus. For long audio tracks, streamed from disk. For playing short sounds use [`fe.add_sound()`](#feadd_sound).

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

The transition function must return a boolean value. It should return `true` if a redraw is required, in which case Attract-Mode Plus will redraw the screen and immediately call the transition function again with an updated `transition_time`.

**_The transition function must eventually return `false` to notify Attract-Mode Plus that the transition effect is done, allowing the normal operation of the frontend to proceed._**

---

### `fe.compile_shader()` 🔶

```squirrel
fe.compile_shader( type, shader1, shader2 )
fe.compile_shader( type, shader1 )
fe.compile_shader( type )
```

Compile a GLSL shader from the given shader code for use in the layout. Also see [`fe.add_shader()`](#feadd_shader).

**Parameters**

-  `type` - The type of shader to add. Can be one of the following values:
   -  `Shader.VertexAndFragment` - Add a combined vertex and fragment shader
   -  `Shader.Vertex` - Add a vertex shader
   -  `Shader.Fragment` - Add a fragment shader
   -  `Shader.Empty` - Add an empty shader. An object's shader property can be set to an empty shader to stop using a shader on that object where one was set previously.
-  `shader1` - A string containing shader code. For the VertexAndFragment type, this should be the vertex shader.
-  `shader2` - This parameter is only used with the VertexAndFragment type, and should be a string containing the fragment shader code.

**Return Value**

-  An instance of the class [`fe.Shader`](#feshader) which can be used to interact with the added shader.

**Notes**

You should avoid "baking" variables into compiled shaders and use `uniforms` instead. While compiling dynamic shaders can avoid costly code branches, using this method to spawn *thousands* of unique shaders should be avoided. See the notes in [`fe.add_shader()`](#feadd_shader).

---

### `fe.get_game_info()`

```squirrel
fe.get_game_info( id )
fe.get_game_info( id, index_offset )
fe.get_game_info( id, index_offset, filter_offset )
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
   -  `Info.Buttons`
   -  `Info.Series`
   -  `Info.Language`
   -  `Info.Region`
   -  `Info.Rating`
   -  `Info.Favourite`
   -  `Info.Tags`
   -  `Info.PlayedCount`
   -  `Info.PlayedTime`
   -  `Info.PlayedLast`
   -  `Info.Score`
   -  `Info.Votes`
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

### `fe.set_game_info()`

```squirrel
fe.set_game_info( id, value )
fe.set_game_info( id, value, index_offset )
fe.set_game_info( id, value, index_offset, filter_offset )
```

Set information about the selected game.

**Parameters**

-  `id` - Id of the information attribute to set. Can be one of the following values:
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
   -  `Info.Buttons`
   -  `Info.Series`
   -  `Info.Language`
   -  `Info.Region`
   -  `Info.Rating`
   -  `Info.Favourite`
   -  `Info.Tags`
   -  `Info.PlayedCount`
   -  `Info.PlayedTime`
   -  `Info.PlayedLast`
   -  `Info.Score`
   -  `Info.Votes`
-  `value` - The value to set.
-  `index_offset` - The offset (from the current selection) of the game to update info for. i.e. `-1` = previous game, `0` = current game, `1` = next game, and so on. Default value is `0`.
-  `filter_offset` - The offset (from the current filter) of the filter containing the selection to update info for. i.e. `-1` = previous filter, `0` = current filter. Default value is `0`.

**Return Value**

-  True if the value was successfully set.

**Notes**

-  Modifying game information does not reload the current Filter, or fire any related Transitions. If the change is expected to modify the list then `fe.signal("reload_config")` should be called afterward.

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

-  `label` - The label of the artwork to retrieve. This should correspond to an artwork configured in Attract-Mode Plus (artworks are configured per emulator in the config menu) or scraped using the scraper. Standard artwork labels are: `"snap"`, `"marquee"`, `"flyer"`, `"wheel"`, and `"fanart"`.
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
   -  `"reload_layout"`
   -  `"reload_config"`

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

-  `true` if the file was loaded, `false` if it was not found.

---

### `fe.load_module()`

```squirrel
fe.load_module( name )
```

Loads a module (a "library" Squirrel script).

**Parameters**

-  `name` - The name of the library module to load. This should correspond to a script file in the "modules" subdirectory of your Attract-Mode Plus configuration (without the file extension).

**Return Value**

-  `true` if the module was loaded, `false` if it was not found.

---

### `fe.plugin_command()`

```squirrel
fe.plugin_command( executable, arg_string )
fe.plugin_command( executable, arg_string, environment, callback_function )
fe.plugin_command( executable, arg_string, callback_function )
```

Execute a command and wait until the command is done.

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

Execute a command in the background and return immediately.

**Parameters**

-  `executable` - The name of the executable to run.
-  `arg_string` - The arguments to pass when running the executable.

**Return Value**

-  None.

---

### `fe.get_input_mappings()` 🔶

```squirrel
fe.get_input_mappings()
```

Get the Attract-Mode Plus Control input mappings.

**Parameters**

-  None.

**Return Value**

-  A table containing the Attract-Mode Plus `control` settings, in the format:

```squirrel
{
	signal = ["input1", "input2+input3"]
}
```

---

### `fe.get_general_config()` 🔶

```squirrel
fe.get_general_config()
```

Get the Attract-Mode Plus general configuration settings.

**Parameters**

-  None.

**Return Value**

-  A table containing the Attract-Mode Plus `general` configuration settings.

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

### `fe.set_config()` 🔶

```squirrel
fe.set_config( config )
```

Set the user configured settings for this layout/plugin/screensaver/intro.

**Parameters**

-  A table containing the [User Config](#user-config) settings.

**Return Value**

-  None.

---

### `fe.get_text()`

```squirrel
fe.get_text( text )
fe.get_text( text, index_offset )
fe.get_text( text, index_offset, filter_offset )
```

Translate the given `text` into the user's language, then perform [_Magic Tokens_](#magic-tokens) replacement. Available translations are limited to the content found within the GUI, and if no matches are found the given text will be used.

**Parameters**

-  `text` - The text string to translate.
-  `index_offset` - The offset (from the current selection) of the game to use for _Magic Tokens_. i.e. `-1` = previous game, `0` = current game, `1` = next game, and so on. Default value is `0`.
-  `filter_offset` - The offset (from the current filter) of the filter containing the selection to use for _Magic Tokens_. i.e. `-1` = previous filter, `0` = current filter. Default value is `0`.

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

An instance of the [`fe.ImageCache`](#feimagecache) class and provides script access to the internal image cache.

### `fe.overlay`

An instance of the [`fe.Overlay`](#feoverlay-1) class and is where overlay functionality may be accessed.

### `fe.obj`

Contains the Attract-Mode Plus draw list. It is an array of [`fe.Image`](#feimage), [`fe.Text`](#fetext), [`fe.ListBox`](#felistbox), and [`fe.Rectangle`](#ferectangle-) instances.

### `fe.displays`

Contains information on the available displays. It is an array of [`fe.Display`](#fedisplay) instances.

### `fe.filters`

Contains information on the available filters. It is an array of [`fe.Filter`](#fefilter) instances.

### `fe.monitors`

An array of [`fe.Monitor`](#femonitor) instances, and provides the mechanism for interacting with the various monitors in a multi-monitor setup. There will always be at least one entry in this list, and the first entry will always be the _primary_ monitor.

### `fe.script_dir`

When Attract-Mode Plus runs a layout or plugin script, `fe.script_dir` is set to the layout or plugin's directory.

### `fe.script_file`

When Attract-Mode Plus runs a layout or plugin script, `fe.script_file` is set to the name of the layout or plugin script file.

### `fe.module_dir`

When Attract-Mode Plus runs a module script, `fe.module_dir` is set to the module's directory.

### `fe.nv`

A table that can be used for persistent data storage, `fe.nv` is accessible to all Layouts, Modules and Plugins. The table is automatically loaded on startup, and saved to disk on shutdown, layout change, and after control inputs. All basic types can be stored: `boolean`, `integer`, `float`, `string`, `array` and `table`.

---

## Classes

### `fe.LayoutGlobals`

This class is a container for global layout settings. The instance of this class is the [`fe.layout`](#felayout) object. This class cannot be otherwise instantiated in a script.

**Properties**

-  `width` - Get/set the layout width. Default value is `ScreenWidth`.
-  `height` - Get/set the layout height. Default value is `ScreenHeight`.
-  `font` - Get/set the filename of the font which will be used for text and listbox objects in this layout.
-  `base_rotation` - Get the base orientation of Attract-Mode Plus which is in Settings. This property cannot be set from the script. This can be one of the following values:
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
-  `nv` 🔶 - A table that can be used for persistent data storage, much like [`fe.nv`](#fenv) - but only available to the current Layout.

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
-  `tags` 🔶 - Returns array containing the available tags for the current list.

---

### `fe.ImageCache`

This class is a container for the internal image cache. The instance of this class is the [`fe.image_cache`](#feimage_cache) object. This class cannot be otherwise instantiated in a script.

**Properties**

-  `count` - Get the number of images currently in the cache.
-  `size` - Get the current size of the image cache (in bytes).
-  `max_size` - Get the (user configured) maximum size of the image cache (in bytes).
-  `bg_load` - Get/set whether images are to be loaded on a background thread. Setting to `true` might make Attract-Mode Plus animations smoother, but can cause a slight flicker as images get loaded. Default value is `false`.

**Member Functions**

-  `add_image( filename )` - Add `filename` image to the internal cache and/or flag it as "most recently used". Least recently used images are cleared from the cache first when space is needed. If filename is contained in an archive, the parameter should be formatted: `"<archive_name>|<filename>"`
-  `name_at( pos )` - Return the name of the image at position `pos` in the internal cache. `pos` can be an integer between `0` and `fe.image_cache.count - 1`.
-  `size_at( pos )` - Return the size (in bytes) of the image at position `pos` in the internal cache. `pos` can be an integer between `0` and `fe.image_cache.count - 1`

---

### `fe.Overlay`

This class is a container for overlay functionality. The instance of this class is the [`fe.overlay`](#feoverlay) object. This class cannot be otherwise instantiated in a script.

**Properties**

-  `is_up` - Get whether the overlay is currently being displayed (i.e. config mode, etc).
-  `list_index` - Returns the index of the overlay option that is currently selected.
-  `list_size` - Get the size of the overlay options list.

**Member Functions**

-  `set_custom_controls( caption_text, options_listbox )`
-  `set_custom_controls( caption_text )`
-  `set_custom_controls()` - Customise the overlay used for Display, Filter, Tag and Exit menus. The `caption_text` parameter is the [`fe.Text`](#fetext) object used to display the caption (i.e. `"Exit Attract-Mode Plus?"`). The `options_listbox` parameter is the [`fe.Listbox`](#felistbox) object used to display the overlay options. The `Configure > Settings > Custom Overlay` setting must be enabled to allow custom controls.
-  `clear_custom_controls()` - Tell the frontend that the layout will NOT do any custom control handling for overlay menus. This will result in the frontend using its built-in default menus instead for overlays.
-  `list_dialog( options, title, default_sel, cancel_sel )`
-  `list_dialog( options, title, default_sel )`
-  `list_dialog( options, title )`
-  `list_dialog( options )` - The list_dialog function prompts the user with a menu containing a list of options, returning the index of the selection. The `options` parameter is an array of strings that are the menu options to display in the list. The `title` parameter is a caption for the list. `default_sel` is the index of the entry to be selected initially (default is `0`). `cancel_sel` is the index to return if the user cancels (default is `-1`). The return value is the index selected by the user.
-  `edit_dialog( msg, text )` - Prompt the user to input/edit text. The `msg` parameter is the prompt caption. `text` is the initial text to be edited. The return value a the string of text as edited by the user.
-  `splash_message( msg, footer_msg )`
-  `splash_message( msg )` - Immediately provide text feedback to the user. This could be useful during computationally-intensive operations. The `footer_msg` text is displayed in the footer.

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
   -  `Info.Buttons`
   -  `Info.Series`
   -  `Info.Language`
   -  `Info.Region`
   -  `Info.Rating`
   -  `Info.Favourite`
   -  `Info.Tags`
   -  `Info.PlayedCount`
   -  `Info.PlayedTime`
   -  `Info.PlayedLast`
   -  `Info.Score`
   -  `Info.Votes`
   -  `Info.FileIsAvailable`
-  `reverse_order` - [boolean] Will be equal to `true` if the list order has been reversed.
-  `list_limit` - Get the value of the list limit applied to the filter game list.

---

### `fe.Monitor`

This class represents a monitor in Attract-Mode Plus, and provides the interface to the extra monitors in a multi-monitor setup. Instances of this class are contained in the [`fe.monitors`](#femonitors) array. This class cannot otherwise be instantiated in a script.

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

-  As of this writing, multiple monitor support has not been implemented for the `OS X` version of Attract-Mode Plus.
-  The first entry in the [`fe.monitors`](#femonitors) array is always the _primary_ display for the system.

---

### `fe.Image`

The class representing an image in Attract-Mode Plus. Instances of this class are returned by the [`fe.add_image()`](#feadd_image), [`fe.add_artwork()`](#feadd_artwork), [`fe.add_surface()`](#feadd_surface) and [`fe.add_clone()`](#feadd_clone) functions and also appear in the [`fe.obj`](#feobj) array (the Attract-Mode Plus draw list). This class cannot be otherwise instantiated in a script.

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
-  `fit` 🔶 - Get/set the texture fitting method. Can be set to one of the following modes:
   -  `Fit.None` - The texture remains unscaled.
   -  `Fit.Fill` (default) - The texture is stretched to fill the entire image.
   -  `Fit.Contain` - The texture is resized to fit within the image while maintaining its aspect-ratio. Letter-boxing may occur.
   -  `Fit.Cover` - The texture is resized to completely cover the image while maintaining its aspect-ratio. Cropping may occur where the texture overlaps.
-  `fit_anchor` 🔶 - Set the anchor for textures that don't fill the image, usually `Fit.Contain` and `Fit.Cover`. See the available modes for `anchor` below. Default is `Anchor.Centre`.
-  `fit_anchor_x` 🔶 - Get/set the x position of the texture anchor. Range is `[0.0...1.0]`. Default value is `0.5`.
-  `fit_anchor_y` 🔶 - Get/set the y position of the texture anchor. Range is `[0.0...1.0]`. Default value is `0.5`.
-  `fit_x` 🔶 - Get the rendered texture x position in pixels, relative to the image.
-  `fit_y` 🔶 - Get the rendered texture y position in pixels, relative to the image.
-  `fit_width` 🔶 - Get the rendered texture width in pixels.
-  `fit_height` 🔶 - Get the rendered texture height in pixels.
-  `crop` 🔶 - [boolean] Get/set whether textures using `Fit.Cover` have their overlap clipped. Default value is `true`.
-  `sample_aspect_ratio` - Get the [pixel aspect ratio](https://en.wikipedia.org/wiki/Pixel_aspect_ratio) of the texture, which is the width of a pixel divided by its height. Commonly found in CRT video sources, as they usually contain non-square pixels.
-  `force_aspect_ratio` 🔶 - Get/set the aspect ratio to use for the texture. Forces a texture to display at the given aspect ratio. Default is `0`, which disables it.
-  `preserve_aspect_ratio` - Get/set whether the `sample_aspect_ratio` or `forced_aspect_ratio` is to be preserved. Default value is `false`.
-  `origin_x` - (deprecated) Get/set the x origin in pixels for position, scale, and rotation. Other anchor and origin properties will be offset by this value. Default is `( 0, 0 )` (top-left corner).
-  `origin_y` - (deprecated) Get/set the y origin in pixels for position, scale, and rotation. Other anchor and origin properties will be offset by this value. Default is `( 0, 0 )` (top-left corner).
-  `transform_origin` 🔶 - Get/set the origin for position, scale and rotation. Can be set to one of the following modes:
   -  `Origin.Left`
   -  `Origin.Centre`
   -  `Origin.Right`
   -  `Origin.Top`
   -  `Origin.Bottom`
   -  `Origin.TopLeft` (default)
   -  `Origin.TopRight`
   -  `Origin.BottomLeft`
   -  `Origin.BottomRight`
-  `anchor` 🔶 - Get/set the midpoint for position and scale. Can be set to one of the following modes:
   -  `Anchor.Left`
   -  `Anchor.Centre`
   -  `Anchor.Right`
   -  `Anchor.Top`
   -  `Anchor.Bottom`
   -  `Anchor.TopLeft` (default)
   -  `Anchor.TopRight`
   -  `Anchor.BottomLeft`
   -  `Anchor.BottomRight`
-  `rotation_origin` 🔶 - Set the midpoint for rotation. Can be set to one of the following modes:
   -  `Origin.Left`
   -  `Origin.Centre`
   -  `Origin.Right`
   -  `Origin.Top`
   -  `Origin.Bottom`
   -  `Origin.TopLeft` (default)
   -  `Origin.TopRight`
   -  `Origin.BottomLeft`
   -  `Origin.BottomRight`
-  `transform_origin_x` 🔶 - Get/set the x position of the midpoint for position, scale, and rotation. This value will become invalid if `anchor_x` or `rotation_origin_x` is manually changed afterward. Range is `[0.0...1.0]`. Default value is `0.0`, centre is `0.5`
-  `transform_origin_y` 🔶 - Get/set the y position of the midpoint for position, scale, and rotation. This value will become invalid if `anchor_y` or `rotation_origin_y` is manually changed afterward. Range is `[0.0...1.0]`. Default value is `0.0`, centre is `0.5`
-  `anchor_x` 🔶 - Get/set the x position of the midpoint for position and scale. Range is `[0.0...1.0]`. Default value is `0.0`, centre is `0.5`
-  `anchor_y` 🔶 - Get/set the y position of the midpoint for position and scale. Range is `[0.0...1.0]`. Default value is `0.0`, centre is `0.5`
-  `rotation_origin_x` 🔶 - Get/set the x position of the midpoint for rotation. Range is `[0.0...1.0]`. Default value is `0.0`, centre is `0.5`
-  `rotation_origin_y` 🔶 - Get/set the y position of the midpoint for rotation. Range is `[0.0...1.0]`. Default value is `0.0`, centre is `0.5`
-  `video_flags` - _[image & artwork only]_ Get/set video flags for this object. These flags allow you to override the default video playback behaviour. Can be set to any combination of none or more of the following (i.e. `Vid.NoAudio | Vid.NoLoop`):
   -  `Vid.Default`
   -  `Vid.ImagesOnly` (disable video playback, display images instead)
   -  `Vid.NoAudio` (silence the audio track)
   -  `Vid.NoAutoStart` (don't automatically start video playback)
   -  `Vid.NoLoop` (don't loop video playback)
-  `video_playing` - _[image & artwork only]_ [boolean] Get/set whether video is currently playing in this artwork.
-  `video_duration` - Get the video duration (in milliseconds).
-  `video_time` - Get the time that the video is current at (in milliseconds).
-  `file_name` - _[image & artwork only]_ Get/set the name of the image/video file being shown. If you set this on an artwork or a dynamic image object it will get reset the next time the user changes the game selection. If file_name is contained in an archive, this string should be formatted: "<archive_name>|<filename>".
-  `shader` - Get/set the GLSL shader for this image. This can only be set to an instance of the class [`fe.Shader`](#feshader).
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
-  `pan` 🔶 - Get/set the audio panning, which positions the sound within the stereo field from left to right. Default value is `0.0`, which is centre. Range is `[-1.0...1.0]`.
-  `vu` 🔶 - _[video only]_ Get the current VU meter value in mono. Range is `[0.0...1.0]`.
-  `vu_left` 🔶 - _[video only]_ Get the current VU meter value for the left audio channel. Range is `[0.0...1.0]`.
-  `vu_right` 🔶 - _[video only]_ Get the current VU meter value for the right audio channel. Range is `[0.0...1.0]`.
-  `fft` 🔶 - _[video only]_ Get the [Fast Fourier Transform](https://en.wikipedia.org/wiki/Fast_Fourier_transform) data for mono audio as an array of float values. Range is `[0.0...1.0]`. Size of the array is defined by `fft_bands`.
-  `fft_left` 🔶 - _[video only]_ Get the Fast Fourier Transform data for the left audio channel as an array of float values. Range is `[0.0...1.0]`. Size of the array is defined by `fft_bands`.
-  `fft_right` 🔶 - _[video only]_ Get the Fast Fourier Transform data for the right audio channel as an array of float values. Range is `[0.0...1.0]`. Size of the array is defined by `fft_bands`.
-  `fft_bands` 🔶 - _[video only]_ Get/set the Fast Fourier Transform band count. Range is `[2...128]` Default value is `32`.
-  `repeat` 🔶 - Enables texture repeat when set to `true`. Default value is `false`. To see the effect `subimg_width/height` must be set larger than `texture_width/height`
-  `border_scale` 🔶 - Get/set the scaling factor of the border defined by `set_border()`. Default value is `1.0`.
-  `clear` 🔶 - _[surface only]_ When set to `false` surface is not cleared before the next frame. This can be used for various accumulative effects.
-  `redraw` 🔶 - _[surface only]_ When set to `false` surface's content is not redrawn which gives optimization opportunity for hidden surfaces. This in conjunction with `clear = false` can be used to freeze surface's content.
-  `border_left` 🔶 - Get/set the left 9-slice region size in pixels. Default is `0`.
-  `border_top` 🔶 - Get/set the top 9-slice region size in pixels. Default is `0`.
-  `border_right` 🔶 - Get/set the right 9-slice region size in pixels. Default is `0`.
-  `border_bottom` 🔶 - Get/set the bottom 9-slice region size in pixels. Default is `0`.
-  `padding_left` 🔶 - Get/set the left padding offset in pixels. Extends the texture beyond the image boundary. Default is `0`.
-  `padding_top` 🔶 - Get/set the top padding offset in pixels. Extends the texture beyond the image boundary. Default is `0`.
-  `padding_right` 🔶 - Get/set the right padding offset in pixels. Extends the texture beyond the image boundary. Default is `0`.
-  `padding_bottom` 🔶 - Get/set the bottom padding offset in pixels. Extends the texture beyond the image boundary. Default is `0`.

**Member Functions**

-  `set_rgb( r, g, b )` - Set the red, green and blue colour values for the image. Range is `[0...255]`.
-  `set_pos( x, y )` - Set the image position (in layout coordinates).
-  `set_pos( x, y, width, height )` - Set the image position and size (in layout coordinates).
-  `set_fit_anchor( x, y )` 🔶 - Set the texture anchor, x and y are in `[0.0...1.0]` range, centre is `( 0.5, 0.5 )`
-  `set_transform_origin( x, y )` 🔶 - Set the midpoint for position, scale, and rotation, x and y are in `[0.0...1.0]` range, centre is `( 0.5, 0.5 )`
-  `set_anchor( x, y )` 🔶 - Set the midpoint for position and scale, x and y are in `[0.0...1.0]` range, centre is `( 0.5, 0.5 )`
-  `set_rotation_origin( x, y )` 🔶 - Set the midpoint for rotation, x and y are in `[0.0...1.0]` range, centre is `( 0.5, 0.5 )`
-  `swap( other_img )` - Swap the texture contents of this object (and all of its clones) with the contents of `other_img` (and all of its clones). If an image or artwork is swapped, its video attributes (`video_flags` and `video_playing`) will be swapped as well.
-  `set_border( left, top, right, bottom )` 🔶 - Set the border size in pixels for [9-slice scaling](https://en.wikipedia.org/wiki/9-slice_scaling). The borders define constrained regions at the edges of the image, while the centre region scales normally. Note that pinch has no effect when borders are set.
-  `set_padding( left, top, right, bottom )` 🔶 - Set the padding offsets in pixels that extend the texture beyond its original dimensions.
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
-  Attract-Mode Plus defers the loading of artwork and dynamic images (images with [_Magic Tokens_](#magic-tokens)) until after all layout and plugin scripts have completed running. This means that the `texture_width`, `texture_height` and `file_name` attributes are not available when a layout or plugin script first adds the image. These attributes become available during transitions such as `Transition.FromOldSelection` and `Transition.ToNewList`:

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
- Pinch is achieved by subdividing the texture polygon into a triangle-strip, and scaling it towards one end. This results in a distorted image, and "ripple" artifacts on large pinches. For nicer results try the `perspective` module - see its Readme for more information.

---

### `fe.Text`

The class representing a text label in Attract-Mode Plus. Instances of this class are returned by the [`fe.add_text()`](#feadd_text) functions and also appear in the [`fe.obj`](#feobj) array (the Attract-Mode Plus draw list). This class cannot be otherwise instantiated in a script.

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
-  `char_size` - Get/set the forced character size. If this is `<= 0` then Attract-Mode Plus will auto-size based on `height`. Default value is `-1`.
-  `glyph_size` - Get the height in pixels of the capital letter. Useful if you want to set the textbox height to match the letter height.
-  `char_spacing` - Get/set the spacing factor between letters. Default value is `1.0`.
-  `line_spacing` - Get/set the spacing factor between lines. Default value is `1.0` At values `0.75` or lower letters start to overlap. For uppercase texts it's around `0.5` It's advised to use this property with the new align modes.
-  `outline` 🔶 - Get/set the thickness of the outline applied to the text. Default value is `0.0`.
-  `bg_outline` 🔶 - Get/set the thickness of the outline applied to the background. Default value is `0.0`
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
-  `shader` - Get/set the GLSL shader for this text. This can only be set to an instance of the class [`fe.Shader`](#feshader).
-  `zorder` - Get/set the Text's order in the applicable draw list. Objects with a lower zorder are drawn first, so that when objects overlap, the one with the higher zorder is drawn on top. Default value is `0`.

**Member Functions**

-  `get_cursor_pos( index )` - Return the cursor `x` position relative to to the object for the given index. Does not work when `word_wrap` is `true`. Range is `[0...msg.len()]`
-  `set_rgb( r, g, b )` - Set the red, green and blue colour values for the text. Range is `[0...255]`.
-  `set_bg_rgb( r, g, b )` - Set the red, green and blue colour values for the text background. Range is `[0...255]`.
-  `set_outline_rgb( r, g, b )` 🔶 - Set the red, green and blue colour values for the text outline. Range is `[0...255]`.
-  `set_bg_outline_rgb( r, g, b )` 🔶 - Set the red, green and blue colour values for the outline of the text background. Range is `[0...255]`.
-  `set_pos( x, y )` - Set the text position (in layout coordinates).
-  `set_pos( x, y, width, height )` - Set the text position and size (in layout coordinates).

---

### `fe.ListBox`

The class representing the listbox in Attract-Mode Plus. Instances of this class are returned by the [`fe.add_listbox()`](#feadd_listbox) functions and also appear in the [`fe.obj`](#feobj) array (the Attract-Mode Plus draw list). This class cannot be otherwise instantiated in a script.

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
-  `list_align` - Get/set the row alignment for the `Selection.Moving` mode. Only affects listbox's where `list_size` is less than the number of `rows`.
   -  `ListAlign.Top` (default) - Aligns options to the top.
   -  `ListAlign.Middle` - Aligns options to the middle.
   -  `ListAlign.Bottom` - Aligns options to the bottom.
   -  `ListAlign.Selection` - Aligns options to keep `sel_row` in-place during list change (with respect to `sel_margin` and `list_size`).
-  `list_size` - Get the size of the list shown by the listbox. When the listbox is assigned as an overlay custom control this property will return the number of options available in the overlay dialog. This property is updated during `Transition.ShowOverlay`
-  `char_size` - Get/set the forced character size. If this is `<= 0` then Attract-Mode Plus will auto-size based on the value of `height`/`rows`. Default value is `-1`.
-  `glyph_size` - Get the height in pixels of the capital letter.
-  `char_spacing` - Get/set the spacing factor between letters. Default value is `1.0`.
-  `outline` 🔶 - Get/set the thickness of the outline applied to the text. Default value is `0.0`.
-  `sel_outline` 🔶 - Get/set the thickness of the outline applied to the selection text. Default value is `0.0`.
-  `style` - Get/set the text style. Can be a combination of one or more of the following (i.e. `Style.Bold | Style.Italic`):
   -  `Style.Regular` (default)
   -  `Style.Bold`
   -  `Style.Italic`
   -  `Style.Underlined`
   -  `Style.StrikeThrough` 🔶
-  `sel_style` - Get/set the selection text style. Can be a combination of one or more of the following (i.e. `Style.Bold | Style.Italic`):
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
-  `sel_mode` 🔶 - Get/set the selection mode. Controls how the listbox behaves when navigating. Can be one of the following values:
   -  `Selection.Static` (default) - The selection remains fixed at the `sel_row` (middle by default) while the list scrolls.
   -  `Selection.Moving` - The selection moves freely, and the list scrolls when the `sel_margin` is reached. When the list edge is reached the selection enters the margin.
   -  `Selection.Bounded` - Identical to `Selection.Moving` except the selection is always bounded by the margin.
   -  `Selection.Paged` - The selection moves freely until it reaches the `sel_margin`, at which point the list scrolls _one page_ and the selection resets to the opposite margin.
-  `sel_margin` 🔶 - Get/set the selection margin for `Selection.Moving` and `Selection.Paged` modes. The list will scroll when the selection is this many rows away from the edges.
-  `sel_row` 🔶 - Get/set the index of the row that is currently selected, with respect to `sel_margin` and `list_size`. Has no effect in `Selection.Paged` mode. Defaults to the middle row.
-  `font` - Get/set the filename of the font used for this listbox. If not set default font is used.
-  `margin` - Get/set the margin spacing in pixels to sides of the text. Default value is `-1` which calculates the margin based on the .char_size.
-  `format_string` - Get/set the format for the text to display in each list entry. [_Magic Tokens_](#magic-tokens) can be used here. If empty, game titles will be displayed (i.e. the same behaviour as if set to `"[Title]"`). Default is an empty value.
-  `shader` - Get/set the GLSL shader for this listbox. This can only be set to an instance of the class [`fe.Shader`](#feshader).
-  `zorder` - Get/set the listbox's order in the applicable draw list. Objects with a lower zorder are drawn first, so that when objects overlap, the one with the higher zorder is drawn on top. Default value is `0`.

**Member Functions**

-  `set_rgb( r, g, b )` - Set the red, green and blue colour values for the text. Range is `[0...255]`.
-  `set_bg_rgb( r, g, b )` - Set the red, green and blue colour values for the text background. Range is `[0...255]`.
-  `set_sel_rgb( r, g, b )` - Set the red, green and blue colour values for the selection text. Range is `[0...255]`.
-  `set_selbg_rgb( r, g, b )` - Set the red, green and blue colour values for the selection background. Range is `[0...255]`.
-  `set_outline_rgb( r, g, b )` 🔶 - Set the red, green and blue colour values for the text outline. Range is `[0...255]`.
-  `set_sel_outline_rgb( r, g, b )` 🔶 - Set the red, green and blue colour values for the selection text outline. Range is `[0...255]`.
-  `set_pos( x, y )` - Set the listbox position (in layout coordinates).
-  `set_pos( x, y, width, height )` - Set the listbox position and size (in layout coordinates).

---

### `fe.Rectangle` 🔶

The class representing a rectangle in Attract-Mode Plus. Instances of this class are returned by the [`fe.add_rectangle()`](#feadd_rectangle-) function and also appear in the [`fe.obj`](#feobj) array (the Attract-Mode Plus draw list). This class cannot be otherwise instantiated in a script.

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
-  `outline` - Get/set the thickness of the outline applied to the rectangle. Default value is `0.0`
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
-  `shader` - Get/set the GLSL shader for this rectangle. This can only be set to an instance of the class [`fe.Shader`](#feshader).
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
-  `pan` 🔶 - Get/set the audio panning, which positions the sound within the stereo field from left to right. Default value is `0.0`, which is centre. Range is `[-1.0...1.0]`.
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
-  `pan` - Get/set the audio panning, which positions the sound within the stereo field from left to right. Default value is `0.0`, which is centre. Range is `[-1.0...1.0]`.
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

The class representing a GLSL shader. Instances of this class are returned by the [`fe.add_shader()`](#feadd_shader) and [`fe.compile_shader()`](#fecompile_shader-) functions. This class cannot be otherwise instantiated in a script.

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

## File System Functions

### `fs.path_expand()`

```squirrel
fs.path_expand( path )
```

Expand the given path name. A leading `~` or `$HOME` token will be become the user's home directory. On Windows systems, a leading `%SYSTEMROOT%` token will become the path to the Windows directory and a leading `%PROGRAMFILES%` or `%PROGRAMFILES(X86)%` will become the path to the applicable Windows `Program Files` directory. For full list of Windows environment variables follow this [link](https://learn.microsoft.com/en-us/windows/deployment/usmt/usmt-recognized-environment-variables)

**Parameters**

-  `path` - The path string to expand.

**Return Value**

-  The expansion of path.

---

### `fs.path_test()`

```squirrel
fs.path_test( path, flag )
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

### `fs.get_file_mtime()` 🔶

```squirrel
fs.get_file_mtime( filename )
```

Returns the modified time of the given file.

**Parameters**

-  `filename` - The file to get the modified time of.

**Return Value**

-  An integer containing the GMT timestamp.

---

### `fs.set_file_mtime()` 🔶

```squirrel
fs.set_file_mtime( filename )
```

Sets the modified time of the given file.

**Parameters**

-  `filename` - The file to set the modified time of.

**Return Value**

-  `true` if succeeded, `false` otherwise.

---

### `fs.get_dir()` 🔶

```squirrel
fs.get_dir( folder )
```

Return an array of the filenames contained in `folder`.

**Parameters**

-  `folder` - Path of the folder to be read.

**Return Value**

-  An array of the filenames contained in the folder.

---

### `fs.make_dir()` 🔶

```squirrel
fs.make_dir( folder )
```

Create a folder in the given location.

**Parameters**

-  `folder` - Path of the folder to be created.

**Return Value**

-  `true` if succeeded, `false` otherwise.

---

### `fs.copy_file()` 🔶

```squirrel
fs.copy_file( src_file, dst_file )
```

Copy file from source to destination.

**Parameters**

-  `src_file` - Path of the source file to be copied.
-  `dst_file` - Path of the destination file including the filename.

**Return Value**

-  `true` if succeeded, `false` otherwise.

---

## Constants

-  `FeVersion` - [string] The current Attract-Mode Plus version.
-  `FeVersionNum` - [int] The current Attract-Mode Plus version.
-  `FeLogLevel` - [string] The current loglevel, will be one of: `"silent"`, `"info"`, or `"debug"`.
-  `FeConfigDirectory` - [string] The path to the config directory.
-  `IntroActive` - [boolean] `true` if the intro is active, `false` otherwise.
-  `Language` - [string] The configured language.
-  `OS` - [string] The Operating System that Attract-Mode Plus is running under, will be one of: `"Windows"`, `"OSX"`, `"FreeBSD"`, `"Linux"`, or `"Unknown"`.
-  `ScreenWidth` - [int] The screen width in pixels.
-  `ScreenHeight` - [int] The screen height in pixels.
-  `ScreenRefreshRate` - [int] The refresh rate of the main screen in Hz.
-  `ScreenSaverActive` - [boolean] `true` if the screen saver is active, `false` otherwise.
-  `ShadersAvailable` - [boolean] `true` if GLSL shaders are available on this system, `false` otherwise.

---

## Language Extensions

Attract-Mode Plus includes all [Squirrel Standard Library](https://web.archive.org/web/20210730072847/http://www.squirrel-lang.org/doc/sqstdlib3.html) methods, as well as the following custom extensions:

-  `zip_extract_archive( zipfile, filename )` - Open a specified `zipfile` archive file and extract `filename` from it, returning the contents as a squirrel blob. Supported formats are: `.zip` `.7z` `.rar` `.tar.gz` `.tar.bz2` `.tar`
-  `zip_get_dir( zipfile )` - Return an array of the filenames contained in the `zipfile` archive file.
-  `regexp2( pattern, flags )` 🔶 - A class which evaluates regular expressions using the C++ regular expression engine. Recommended over the standard `regexp` class as it contains considerable improvements. Flags are optional, accepts `"i"` for case-insensitive matches.
-  `join( arr, delim )` 🔶 - Returns a string containing concatenated array values separated with the given delimiter.
-  `get_clipboard()` 🔶 - Returns the contents of the OS clipboard.
-  `set_clipboard( value )` 🔶 - Sets the contents of the OS clipboard.

---

### Math 🔶

In addition to the standard `Math` library, the following methods are included:

-  `ceil2( x )` - Ceils `x` to the nearest even integer
-  `clamp( x, min, max )` - Clamps `x` between `min` and `max` (inclusive)
-  `degrees( r )` - Converts `r` from radians to degrees
-  `exp2( x )` - Return `2` raised to the power of `x` (more performant than `pow( 2, x )`)
-  `floor2( x )` - Floors `x` to the nearest even integer
-  `fract( x )` - Returns a fractional part of `x`
-  `hypot( x, y )` - Returns the hypotenuse of `x, y`
-  `log2( x )` - Returns the base `2` logarithm of a number
-  `max( a, b )` - Returns the largest `a` or `b`
-  `min( a, b )` - Returns the smallest `a` or `b`
-  `mix( a, b, x )` - Returns a blend between `a` and `b`, using a mixing ratio `x`
-  `mix_short( a, b, m, x )` - Returns a blend of the shortest _wrapped_ distance between `a` and `b`, using a mixing ratio `x` and wrapping modulo `m`
-  `modulo( v, m )` - Modulo of `v` with correct handling of negative numbers
-  `radians( d )` - Converts `d` from degrees to radians
-  `random( min, max )` - Returns a random integer in a range defined by `min` and `max` (inclusive)
-  `randomf( min, max )` - Returns a random float in a range defined by `min` and `max` (inclusive)
-  `round( x )` - Rounds `x` to the nearest integer
-  `round2( x )` - Rounds `x` to the nearest even integer
-  `short( a, b, m )` - Returns the shortest _wrapped_ distance between `a` and `b`, using wrapping modulo `m`
-  `round2( x )` - Rounds `x` to the nearest even integer
-  `sign( x )` - Returns `1` when `x > 0`, returns `-1` when `x < 0`, returns `0` when `x == 0`

---

### Vector 🔶

```squirrel
Vec2()
Vec2( x, y )
```

All standard operators work with 2D Vectors.

```squirrel
local a = Vec2( 1, 2 )
local b = Vec2( 3, 4 )

local c = a + b
fe.log( c ) // 4.000, 6.000

local d = c * 2.0
fe.log( d ) // 8.000, 12.000
```

-  `x` - Get/set the Vectors x coordinate (contains length for polar Vector)
-  `y` - Get/set the Vectors y coordinate (contains angle for polar Vector)
-  `len` - Get/set the length of the Vector
-  `angle` - Get/set the angle of the Vector in radians
-  `componentMul( v )` - Return the Vector by multiplied by the components of the given vector
-  `componentDiv( v )` - Return the Vector by divided by the components of the given vector
-  `polar()` - Return the Vector represented in polar coordinates
-  `cartesian()` - Return the Vector represented in cartesian coordinates
-  `normalize()` - Return the normalized Vector
-  `perpendicular()` - Return a perpendicular Vector
-  `projectedOnto( v )` - Return the Vector projected onto the given Vector
-  `mix( v, x )` - Return a mix of the Vector with the given Vector
-  `lengthSquared()` - Return the Vector length squared, more performant than `len` (used for comparisons)
-  `angleTo( v )` - Return the angle between the given Vector in radians
-  `distance( v )` - Return the distance to the given Vector
-  `dot( v )` - Return the dot product with the given Vector
-  `cross( v )` - Return the cross product with the given Vector

---

### Easing 🔶

[Easing functions](https://easings.net/) specify the rate of change of a parameter over time. Easing in animation is a transition method that modifies motion to make it less pronounced and jarring. All [Penner Easings](http://robertpenner.com/easing/) are included, plus a few common extras.

**Methods**

The easing methods belong to a global object named `ease`, such as `ease.out_cubic( t, b, c, d )`

| In            | Out            | InOut             | OutIn             | Params                       |
| ------------- | -------------- | ----------------- | ----------------- | ---------------------------- |
| `in_quad`     | `out_quad`     | `in_out_quad`     | `out_in_quad`     | `t, b, c, d`                 |
| `in_cubic`    | `out_cubic`    | `in_out_cubic`    | `out_in_cubic`    | `t, b, c, d`                 |
| `in_quart`    | `out_quart`    | `in_out_quart`    | `out_in_quart`    | `t, b, c, d`                 |
| `in_quint`    | `out_quint`    | `in_out_quint`    | `out_in_quint`    | `t, b, c, d`                 |
| `in_sine`     | `out_sine`     | `in_out_sine`     | `out_in_sine`     | `t, b, c, d`                 |
| `in_expo`     | `out_expo`     | `in_out_expo`     | `out_in_expo`     | `t, b, c, d`                 |
| `in_expo2`    | `out_expo2`    | `in_out_expo2`    | `out_in_expo2`    | `t, b, c, d`                 |
| `in_circ`     | `out_circ`     | `in_out_circ`     | `out_in_circ`     | `t, b, c, d`                 |
| `in_bounce`   | `out_bounce`   | `in_out_bounce`   | `out_in_bounce`   | `t, b, c, d`                 |
| `in_bounce2`  | `out_bounce2`  | `in_out_bounce2`  | `out_in_bounce2`  | `t, b, c, d, p`              |
| `in_back`     | `out_back`     | `in_out_back`     | `out_in_back`     | `t, b, c, d, s`              |
| `in_back2`    | `out_back2`    | `in_out_back2`    | `out_in_back2`    | `t, b, c, d`                 |
| `in_elastic`  | `out_elastic`  | `in_out_elastic`  | `out_in_elastic`  | `t, b, c, d, a, p`           |
| `in_elastic2` | `out_elastic2` | `in_out_elastic2` | `out_in_elastic2` | `t, b, c, d, p`              |
| `linear`      |                |                   |                   | `t, b, c, d`                 |
| `bezier`      |                |                   |                   | `t, b, c, d, x1, y1, x2, y2` |
| `steps`       |                |                   |                   | `t, b, c, d, s, j`           |

**Parameters**

The following parameters are common to all easing functions:

-  `t` - Current time, where `t` is in the range `[0...d]`
-  `b` - Beginning value, when `t == 0` the method returns `b`
-  `c` - Change in value, when `t == d` the method returns `b + c`
-  `d` - Duration, the maximum value of `t`

**Extra Parameters**

-  `bounce2`
   -  `p` - _(optional)_ Period of the bounce. Default is `0.5`
-  `back`
   -  `s` - _(optional)_ Strength of the overshoot. Default is `1.70158`
-  `elastic`
   -  `a` - _(optional)_ Amplitude of the wave. Default is `0.0`
   -  `p` - _(optional)_ Period of the wave. Default is `d * 0.3`
-  `elastic2`
   -  `p` - _(optional)_ Period of the wave. Default is `0.3`
-  `bezier`
   -  `x1, y1` - The first [control point](https://cubic-bezier.com), in the range `[0.0...1.0], [0.0...1.0]`
   -  `x2, y2` - The second control point, in the range `[0.0...1.0], [0.0...1.0]`
   -  Additional control points exist at `0.0, 0.0` and `1.0, 1.0` to complete the ease
-  `steps`
   -  `s` - [int] Number of steps
   -  `j` - _(optional)_ [Step position](https://developer.mozilla.org/en-US/docs/Web/CSS/Reference/Values/easing-function/steps#description). Defaults to `Jump.End`. May be one of the following:
      -  `Jump.Start` - The first step happens when the ease begins
      -  `Jump.End` - The last step happens when the ease ends
      -  `Jump.None` - Neither start nor end jumps occur
      -  `Jump.Both` - Both start and end jumps occur

**Example**

```squirrel
local d = 10
for (local t=0; t<=d; t++)
{
	fe.log(t + " = " + ease.out_cubic(t, 0, 1, d))
}

// 0 = 0
// 1 = 0.271
// 2 = 0.488
// 3 = 0.657
// 4 = 0.784
// 5 = 0.875
// 6 = 0.936
// 7 = 0.973
// 8 = 0.992
// 9 = 0.999
// 10 = 1
```

The results show the beginning value `b = 0` changing by `c = 1`, using a `cubic` algorithm to decelerate the change as `t` approaches `d`.
