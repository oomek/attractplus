# <img src="https://github.com/oomek/attractplus/blob/master/resources/images/Logo.png?raw=true" width="40" align="left"> Attract-Mode Plus Frontend

> -  Attract-Mode Plus is a fork of [Attract-Mode](https://github.com/mickelson/attract) with various community improvements and fixes.
> -  New releases can be found on the [Releases](https://github.com/oomek/attractplus/releases) page.
> -  Work-in-progress binaries can be found in the [Actions](https://github.com/oomek/attractplus/actions) tab (requires login).
> -  The Layout Coding Reference can be found in the [Layouts.md](./Layouts.md) file.
> -  Features unique to Attract-Mode Plus are marked with a 🔶 symbol.
> -  Join our [Discord](https://discord.gg/86bB9dD) Server and share your creations.

## Contents

-  [Overview](#overview)
-  [Quick Start](#quick-start)
-  [Basic Organization](#basic-organization)
-  [Further Customization](#further-customization)
   -  [Controls](#controls)
   -  [Filters](#filters)
   -  [Sound](#sound)
   -  [Artwork](#artwork)
   -  [Layouts](#layouts)
   -  [Plug-ins](#plug-ins)
   -  [Romlists](#romlists)
-  [Romlist Generation](#romlist-generation)
-  [Command Line Options](#command-line-options)

---

## Overview

Attract-Mode is a graphical frontend for command line emulators such as `MAME`, `MESS`, and `Nestopia`. It hides the underlying operating system and is intended to be controlled with a joystick, gamepad or spin dial, making it ideal for use in arcade cabinet setups.

Attract-Mode was originally developed for Linux. It is known to work on Linux (x86, x86-64, ARM, Raspberry Pi), Mac OS X and Windows based-systems.

Attract-Mode is licensed under the terms of the GNU General Public License, version 3 or later.

Visit <http://attractmode.org> for more information.

See [Compile.md](./Compile.md) if you intend to compile Attract-Mode from source.

---

## Quick Start

1. Run Attract-Mode. By default, Attract-Mode will look for its configuration files in the `$HOME/.attract` directory on Linux and Mac OS X, and in the current working directory on Windows. If you want to use a different location for the Attract-Mode configuration then you need to specify it at the command line as follows:
   ```sh
   attract --config /my/config/location
   ```
2. If you are running Attract-Mode for the first time, you will be prompted to select the language to use and then the frontend will load directly to its configuration mode. If you do not start in config mode, pressing the `TAB` key will get you there. By default, config mode can be navigated using the up/ down arrows, enter to select an option, and escape to go back.

3. Select the `Emulators` option, where you will be presented with the option of editing the configuration for the emulators Attract-Mode auto-detected (if any). `Edit` an existing emulator or select `Add` to add a new emulator configuration. Default configuration templates are provided for various popular emulators to help you get started, however some settings will likely have to be customized for your system (file locations etc).

4. Once you have an emulator configured correctly for your system, select the `Generate Collection/Rom List` option from the emulator's configuration menu. Attract-Mode will use the configured emulator settings to generate a list of available games for the emulator. Next select the `Scrape Artwork` option if you want to have Attract-Mode go and automatically download artwork images for the emulator from the web.

5. Exit config mode by selecting the `Back` option a few times. You should now have a usable front-end!

---

## Basic Organization

At its most basic level, Attract-Mode is organized into `Displays`, of which you can configure one or more. Each `Display` is a grouping of the following:

**Collection / Rom List**

-  A Collection/Rom List is the listing of available games/roms to show. Romlists are text files located in the `romlists` directory. They are created by Attract-Mode using the `Generate Collection/Rom List` option available when configuring an emulator, or by using the `--build-romlist` or `--import-romlist` command line options.

**Layout**

-  A Layout is the visual theme/skin to use for the Display. Layouts are located in the `layouts` directory, with each layout either in its own subdirectory or contained in a `.zip` file in the `layouts` directory.

**Global Filter**

-  The global filter is a set of filter rules that always get applied to a Collection/Rom List. This filter can be used to remove entries you will never want to see

**Filters**

-  Filters are a set of rules for which games to display and how to sort them. Filters can be created to categorize games based attributes such as their orientation, category, manufacturer, year, times played, favourite status, file availability, etc. Filters can be cycled through using the `Previous Filter` and `Next Filter` controls, and can also be selected from the `Filters Menu`.

---

## Further Customization

### Controls

The inputs used to control Attract-Mode can be configured from from the `Controls` menu in config mode. Attract-Mode actions can be mapped to most keyboard, mouse and joystick inputs, including combinations. Here is a list of the default control mappings:

| Keyboard          | Joystick          | Action                  |
| ----------------- | ----------------- | ----------------------- |
| Up                | Up                | Previous Entry          |
| Down              | Down              | Next Entry              |
| Left              | Left              | Previous Display        |
| Right             | Right             | Next Display            |
| Enter or LControl | Button A          | Select                  |
| Escape            | Button B          | Back/Exit               |
| LControl+Up       | Button A+Up       | Jump to Previous Letter |
| LControl+Down     | Button A+Down     | Jump to Next Letter     |
| LControl+Left     | Button A+Left     | Show Filters Menu       |
| LControl+Right    | Button A+Right    | Next Filter             |
| LControl+Escape   | Button A+Button B | Toggle Favourite        |
| Tab or Escape+Up  | Button B+Up       | Configure               |
| Escape+Down       | Button B+Down     | Edit Game               |

### Filters

Filters are used to control what games are shown, how they are sorted, and how many are displayed. They can be added to a Display in `Configure > Displays > Display Edit > Add Filter`. Filters use list of `rules` and `exceptions` that the frontend steps through, in order, to determine whether or not to list a game. If a game does not match a `rule`, then it is not shown. If a game matches to an `exception`, then it gets listed no matter what (ignoring the rest of the rules in the filter). In other words, in order to be listed, a game has to match _all_ the rules or _just one_ of the exceptions configured for the filter.

For example, you might want to have a filter that only shows 1980's multiplayer sports games. This would be achieved by creating a filter with three rules:

1. The year is in the 1980s - `Year equals 198.`
2. The number of players in not 1 - `Players not_equals 1`
3. The category contains "Sports" - `Category contains Sports`

Filters use regular expressions, which allow for powerful text matching capabilities. From the example above, the `198.` will match any four letter word that starts with `198`, with the `.` matching _any_ character thereafter.

Filters are stored in `attract.cfg` and may be edited directly. Keep in mind that changes to this file are only loaded when AM first starts, and the file is overwritten when AM exits. Rules are in the format `target comparison value`.

-  `target` - Can be one of the Rule Targets listed below.
-  `comparison` - Can be one of the following:
   -  `equals` - The target must equal the value.
   -  `not_equals` - The target must not equal the value.
   -  `contains` - The target must contain the value.
   -  `not_contains` - The target must not contain the value.
-  `value` - A regular expression.

**Rule Targets**

- Any field from the romlist: `Name`, `Title`, `Emulator`, `CloneOf`, `Year`, `Manufacturer`, `Category`, `Players`, `Rotation`, `Control`, `Status`, `DisplayCount`, `DisplayType`, `AltRomname`, `AltTitle`, `Extra`, `Buttons`, `Series`, `Language`, `Region`, `Rating`
- Tags: `Favourite`, `Tags`
- Stats: `PlayedCount`, `PlayedTime`, `PlayedLast`
- Other: `FileIsAvailable`, `Shuffle`

### Sound

To configure sounds, place the sound file in the `sounds` subdirectory of your Attract-Mode config directory. The sound file can then be selected from the `Sound` menu when in config mode and mapped to an action or event. Attract-Mode should support any sound format supported by FFmpeg (`MP3`, etc).

### Artwork

Attract-Mode supports `PNG`, `JPEG`, `GIF`, `BMP` and `TGA` image formats. For video formats, Attract-Mode should support any video format supported by FFmpeg (`MP4`, `FLV`, `AVI`, etc). When deciding what file to use for a particular artwork type, Attract-Mode will use the artwork selection order set out below.

The location of artwork resources is configured on a per-emulator basis in the `Emulators` configuration menu. Attract-Mode's default artworks are:

-  `"marquee"` - For cabinet marquee images.
-  `"snap"` - For attract-mode videos and game screen shots.
-  `"flyer"` - For game flyer/box art.
-  `"wheel"` - For Hyperspin wheel art.

You can add others as needed in the `Emulators` configuration menu. Multiple paths can be specified for each artwork, in which case Attract-Mode will check each path in the order they are specified before moving to the next check in the selection order.

**Artwork Selection Order**

-  From the artwork paths configured for the emulator (if any) and the previously scraped artworks (if any):
   -  `[Name].\*` - Video, i.e. "pacman.mp4"
   -  `[CloneOf].\*` - Video, i.e. "puckman.mp4"
   -  `[Name].\*` - Image, i.e. "pacman.jpg"
   -  `[CloneOf].\*` - Image, i.e. "puckman.png"
   -  `[Emulator].\*` - Video, i.e. "mame.avi"
   -  `[Emulator].\*` - Image, i.e. "mame.gif"
-  From the layout path for the current layout (layouts are located in the `layouts` subdirectory):
   -  `[Emulator]-[ArtLabel].\*` - Video, i.e. "mame-marquee.mp4"
   -  `[Emulator]-[ArtLabel].\*` - Image, i.e. "mame-marquee.png"
   -  `[ArtLabel].\*` - Video, i.e. "marquee.avi"
   -  `[ArtLabel].\*` - Image, i.e. "marquee.png"
-  When looking for artwork for the `Displays Menu`, artwork is loaded from the `menu-art` subdirectory.
   -  Artwork matching the Display's name or the Display's romlist name are matched from the corresponding artwork directories located there.
-  If no file match is found, Attract-Mode will check for a subdirectory that meets the match criteria.
   -  If a subdirectory is found (for example a `pacman` directory in the configured artwork path) then Attract-Mode will then pick a random video or image from that directory.
-  If no files are found matching the above rules, then the artwork is not drawn.

### Layouts

Attract-Mode's layouts are located in the `layouts` directory. Each layout has to be in its own subdirectory or contained in a `.zip` file. Attract-Mode's native layouts are made up of a squirrel script (a `.nut` file) and related resources. See [Layouts.md](./Layouts.md) for more information on Attract-Mode layouts.

Attract-Mode can also display layouts made for other frontends, including `MaLa` and `Hyperspin`. This feature is experimental, and certain features from these other frontends might not be fully implemented.

-  `MaLa` - Copy the layout and related resources into a new subdirectory of the `layouts` directory, or place a `zip`, `7z` or `rar` file containing these things into the `layouts` directory. After doing this, you hould be able to select the layout when configuring a Display in Attract-Mode, just as you would for a native Attract-Mode layout.
-  `Hyperspin` - Copy the Hyperspin `Media` directory into its own directory in the Attract-Mode layouts directory. Create a Display in Attract-Mode and configure the layout to be the directory you copied `Media` into. The Display's name needs to match the name of one of the system subdirectories in the Hyperspin `Media` directory. This will allow Attract-Mode to find the Hyperspin themes and graphics to use. So for example, naming the Display `MAME` will cause it to match Hyperspin's `MAME/Themes/_` for themes, `MAME/Images/Artwork1/_` for artwork1, etc. Note that the wheel images are located using Attract-Mode's built-in wheel artwork. Wheel images located in the Hyperspin directories are ignored.

### Plug-ins

Plug-ins are squirrel scripts that need to be placed in the `plugins` subdirectory of your Attract-Mode config directory. Available plugins can be enabled/disabled and configured from the `Plug-Ins` menu when in config mode. See [Layouts.md](./Layouts.md) for a description of Attract-Mode's Plug-in API.

### Romlists

Collection/Rom lists are saved in the `romlist` subdirectory of your Attract-Mode config directory. Each list is a semi-colon delimited text file that can be edited by most common spreadsheet programs (be sure to load it as `Text CSV`). The file has one game entry per line, with the very first line of the file specifying what each column represents.

---

## Romlist Generation

In addition to the romlist generation function available in config mode, Attract-Mode can generate a single romlist containing roms from multiple emulators using the command line.

```sh
attract --build-romlist <emu>...
```

You can also import romlists from other frontends. Supported files include:
- `*.txt` - Attract-Mode lists
- `*.xml` - Mame listxml, listsoftware, and HyperSpin lists
- `*.lst` - MameWah, Wah!Cade

```sh
attract --import-romlist <file> [emu]
```

The `--build-romlist` and `--import-romlist` options can be chained together to generate combined Attract-Mode romlists. The following example combines the entries from `mame.lst` and `nintendo.lst` (located in the current directory) into a single Attract-Mode romlist. The `mame` emulator will be used for the `mame.lst` games, while the `nestopia` emulator will be used for the `nintendo.lst` games.

```sh
attract --import-romlist mame.lst --import-romlist nintendo.lst nestopia
```

Filter rules can be applied when importing or building a romlist using the `--filter <rule>` option. The `<rule>` argument uses the same format as the rules in the `attract.cfg` file.

```sh
attract --build-romlist mame --filter "Year equals 198."
```

If you wish to specify the name of the created romlist at the command line, you can do so with the `--output <name>` option. Beware that this will overwrite any existing Attract-Mode romlist with the specified name.

---

## Command Line Options

A complete list of command line options can be found by running:

```sh
attract --help
```

Attract-Mode by default will print log messages to the console window (stdout). To suppress these messages, run with the following option:

```sh
attract --loglevel silent
```

Alternatively, more verbose debug log messages can be enabled by running:

```sh
attract --loglevel debug
```
