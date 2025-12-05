<p align="center" width="100%">
   <picture>
      <source srcset="https://github.com/oomek/attractplus/blob/master/resources/images/Logo_Full_White.png?raw=true" media="(prefers-color-scheme: dark)">
      <source srcset="https://github.com/oomek/attractplus/blob/master/resources/images/Logo_Full_Black.png?raw=true" media="(prefers-color-scheme: light)">
      <img width="50%" src="https://github.com/oomek/attractplus/blob/master/resources/images/Logo.png?raw=true" alt="Logo">
   </picture>
</p>

Attract-Mode Plus is a graphical frontend for launching command-line emulators. It offers robust list filtering capabilities, and employs a versatile scripting language to showcase an extensive range of image, video, and audio formats. Compatible with Windows, Linux (x86, x86-64, ARM, Raspberry Pi), and Mac OS X.

[Releases](https://github.com/oomek/attractplus/releases)
• [WIP](https://github.com/oomek/attractplus/actions)
• [Coding Reference](./Layouts.md)
• [Compiling](./Compile.md)
• [Info](./Info.md)
• [Discord](https://discord.gg/86bB9dD)

> Attract-Mode Plus is a fork of [Attract-Mode](https://github.com/mickelson/attract), enhanced by numerous community-driven improvements, fixes, and features. Licensed under the terms of the GNU General Public License, version 3 or later.

---

# ⏱️ Quickstart + Mame + Windows

Attract-Mode Plus is a blank canvas that requires external assets to operate effectively.

> Although `Snaps` are fundamental, `Videos` energise and elevate the frontend experience.

-  👾 Application - _Emulators and Roms to launch_
   -  [Attract-Mode Plus](https://github.com/oomek/attractplus/releases) - extract to `C:\attract-mode-plus`
   -  [Mame](https://www.mamedev.org/release.html) - extract to `C:\mame`
   -  [Roms](https://www.mamedev.org/roms/) - place in `C:\mame\roms`
-  🎨 Artwork - _Images and Videos to show_
   -  [Snaps](https://www.progettosnaps.net/snapshots/) - extract to `C:\mame\snap`
   -  [Videos](https://emumovies.com/files/file/2214-mame-arcade-video-snaps-pack-3862) - extract to `C:\mame\video` - _Standard Quality_
   -  [Logos](https://emumovies.com/files/file/1969-mame-game-logo-pack/) - extract to `C:\mame\wheel`
   -  [Marquees](https://emumovies.com/files/file/551-mame-marquees-pack/) - extract to `C:\mame\marquee`
-  📚 Additional - _Category, Series, Languages, and Players information_
   -  [catver.ini](https://www.progettosnaps.net/catver/) - place in `C:\mame\folders`
   -  [series.ini](https://www.progettosnaps.net/series/) - place in `C:\mame\folders`
   -  [languages.ini](https://www.progettosnaps.net/languages/) - place in `C:\mame\folders`
   -  [nplayers.ini](https://nplayers.arcadebelgium.be/) - place in `C:\mame\folders`

> [!TIP]\
> Select - `Up/Down`\
> Accept/Back - `Return/Escape`\
> Configure - `Tab`\
> Fullscreen/Window - `F11`

-  🚀 Launch `C:\attract-mode-plus\attractplus.exe`
-  🌏 Select your language
-  📡 Auto-detect emulators > `Yes`
-  💾 Found 1 emulator > `Import`
-  🎉 **_Attract-Mode Plus is ready to play!_**

---

# ⚙️ Basic Configuration

[Controls](#️-controls)
• [Emulators](#️-emulators)
• [Romlists](#-romlists)
• [Artwork](#-artwork)
• [Displays](#️-displays)
• [Filters](#-filters)
• [Global Filters](#-global-filters)
• [Favourites](#-favourites)
• [Tags](#️-tags)
• [Layouts](#️-layouts)
• [Plugins](#-plugins)
• [Modules](#-modules)
• [Displays Menu](#️-displays-menu)
• [Clones](#-clones)
• [Files](#-files)
• [Updates](#-updates)

> Fine-tune Attract-Mode Plus to suit your needs using its extensive range of settings.

## 🕹️ Controls

Configure the input bindings to suit your device - navigation is a good place to start.

-  Configure > Controls
   -  Previous / Next Page - _Scroll by an entire page_
   -  Previous / Next Letter - _Skip to the next letter_
   -  Previous / Next Filter - _Switch between Filters_
   -  Previous / Next Display - _Switch between Displays_
   -  Displays Menu - _Toggle the Displays Menu_

> Hold multiple keys to create combos.
>
> 🔍 `C:\attract-mode-plus\attract.cfg`

## 🛠️ Emulators

Emulators specify how Executables run Roms.

-  Configure > Emulators > `mame`
   -  Executable > `C:\mame\mame.exe`
   -  Command Arguments > `[name]`
   -  Working Directory > `C:\mame`

> Launches `C:\mame\mame.exe [name]` where `[name]` is substituted with the selected Romlist entry name.
>
> 🔍 `C:\attract-mode-plus\emulators\mame.cfg`

## 📜 Romlists

Romlists are created by scanning `Rom Paths` for files matching `Rom Extensions`.

-  Configure > Emulators > `mame`
   -  Rom Path(s) > `C:\mame\roms`
   -  Rom Extension(s) > `.zip;.7z`
   -  Info Source/Scraper > `listxml`
   -  Additional Import File(s) > _`your;ini;files;here`_
   -  Generate Collection/Romlist - _Rebuild the Romlist_

> `Additional Import Files` and the Mame-only `listxml` scraper augment the Romlist to support advanced Filtering.
>
> 🔍 `C:\attract-mode-plus\romlists\mame.txt`

## 🎨 Artwork

Artwork is associated with Romlist entries that have matching names.

-  Configure > Emulators > `mame`
   -  snap > `C:\mame\snap;C:\mame\video`
   -  marquee > `C:\mame\marquee`
   -  wheel > `C:\mame\wheel`

> `Configure > Emulators > mame > Scrape Artwork` can import some artwork, but with reduced control over the results.
>
> 🔍 `C:\attract-mode-plus\scraper\mame`

## 📽️ Displays

Displays determine what the frontend presents to the user.

-  Configure > Displays > `mame`
   -  Layout > `BasicPlus` - _The Theme_
   -  Collection/Romlist > `mame` - _The Roms_
   -  Filter > `All` - _The Lists_

> Multiple Displays can share the same Romlist, each with its own Layout and Filters.
>
> 🔍 `C:\attract-mode-plus\displays.cfg`

## 🔬 Filters

Filters are rule-sets that organise Romlists into concise, structured Lists.

-  Configure > Displays > mame
   -  Add Filter > `The 90's`
      -  Add Rule
         -  Target > `Year`
         -  Comparison > `contains`
         -  Filter Value > `199`

> `Filter Value` is a [regular expression](https://en.wikipedia.org/wiki/Regular_expression) that matches Romlist fields.
>
> 🔍 `C:\attract-mode-plus\displays.cfg`

## 🔭 Global Filters

Global Filters refine the Romlist for the _entire_ Display.

-  Configure > Displays > mame
   -  Global Filter
      -  Add Rule
         -  Target > `Category`
         -  Comparison > `does not contain`
         -  Filter Value > `Mahjong`

> Use to exclude Roms from all Filters within the Display.
>
> 🔍 `C:\attract-mode-plus\displays.cfg`

## ⭐ Favourites

Displays are created with two Filters, one of which is a "Favourites" List.

-  `All` - _Show all Roms in the Romlist_
-  `Favourites` - _Show favourite Roms in the Romlist_

> Use the `Add/Remove Favourites` control to toggle a Rom's "Favourite" status.
>
> 🔍 `C:\attract-mode-plus\romlists\mame.tag`

## 🏷️ Tags

Custom Tags can be added to Roms, allowing Filters to generate personalised Lists.

-  Configure > Displays > mame
   -  Add Filter > `Party Games`
      -  Add Rule
         -  Target > `Tags`
         -  Comparison > `contains`
         -  Filter Value > `party`

> Use the `Add/Remove Tags` control to add a Tag named `party` to your Roms.
>
> 🔍 `C:\attract-mode-plus\romlists\mame\party.tag`

## 🎞️ Layouts

Layouts are themes that shape the appearance and behaviour of a Display.

-  Configure > Displays > mame
   -  Layout > _`Select a Layout`_
   -  Layout Options > _`Varies by Layout`_

> Layout Options belong to the Layout - they are common to all Displays that use it.

A multitude of Layouts can be found on [Discord](https://discord.com/channels/373969602784526336/1145679917808353370) or the [Forum](http://forum.attractmode.org/index.php?board=6.0).

-  While some explore [technical possibilities](https://discord.com/channels/373969602784526336/1145679917808353370/1379329222782484500), others stand as [magnum opuses](https://discord.com/channels/373969602784526336/1145679917808353370/1145684611863674930).
-  A select few are [absolute behemoths](https://discord.com/channels/373969602784526336/1194713309430173766/1357592010462335046), featuring hand-crafted Layouts for individual Roms.
-  With some basic coding you can even [create your own!](./Layouts.md)

> As community-generated content, Layouts can vary in quality.
>
> 🔍 `C:\attract-mode-plus\layouts`

## 🔌 Plugins

Plugins run _atop_ the Layout, providing added features across all Displays.

-  Configure > Plugins > SearchPlus
   -  Enabled > `Yes`
   -  Toggle > `Custom 1` - _The control that shows the Keyboard_

> Use the `Plugin Options` control to configure the last viewed Plugin.
>
> 🔍 `C:\attract-mode-plus\plugins`

## 📦 Modules

Modules are shared libraries that provide functionality to Layouts. Popular Modules such as `inertia`, `animate`, and `wheel` are bundled with Attract-Mode Plus.

> Modules do not require enabling. If a Layout isn't working, run `attractplus-console.exe` to inspect console warnings. Refer to the Layout's Readme for information about required files.
>
> 🔍 `C:\attract-mode-plus\modules`

## 🏟️ Displays Menu

The `Displays Menu` can be configured to use a Layout. Setting it to show on startup establishes it as the "root" Menu to which other Displays return.

-  Configure > Displays > Displays Menu Options
   -  Layout > _`Select a Layout`_
-  Configure > Settings >
   -  Startup Mode > `Show Displays Menu`

> Artwork for the `Displays Menu` must match the `Display` or `Romlist` name, located in the `menu-art/<resource>` subfolder.
>
> 🔍 `C:\attract-mode-plus\menu-art\snap\mame.png`

## 🐑 Clones

Rom clones are grouped into sub-menus, which are opened by selecting the Rom.

-  Configure > Settings >
   -  General > Group Clones > `Yes`

> Use the `Back` control to exit the clone sub-menu.

## 📁 Files

Configuration files can be edited while Attract-Mode Plus is running.

> Close the menu to avoid changes being overwritten by the frontend. Preserve the file's formatting to ensure it remains readable.
>
> Use the `Reload Config` control to reload all configuration files.

## 📬 Updates

A notification will appear at startup whenever an update is available. Visit the [Releases](https://github.com/oomek/attractplus/releases) page to download it.

> Extract the new version over your existing files - your configuration will remain intact.
