# <img src="https://github.com/oomek/attractplus/blob/master/resources/images/Logo.png?raw=true" width="40" align="left"> Attract-Mode Plus Compiling

Build instructions are set out by operating system below.

## Contents

-  [Linux, FreeBSD](#linux-freebsd)
-  [OS X](#os-x)
-  [Windows (cross-compile)](#windows-cross-compile)
-  [Windows (native compile)](#windows-native-compile)

---

## Linux, FreeBSD

These instructions assume that you have the GNU C/C++ compilers and basic build utilities (make, pkg-config, ar) on your system. This means the "build-essential" and "pkg-config" packages on Debian or Ubuntu-based distributions. Other distributions should have similar packages available.

1. Install the following _development_ libraries on your system:

   -  Required:
      -  SFML SDK version 2.x http://sfml-dev.org
      -  OpenAL
      -  Zlib
      -  FreeType 2
      -  The following FFmpeg libraries (required for videos):
         -  avformat
         -  avcodec
         -  swscale
         -  avutil
         -  swresample
      -  OpenGL and GLU (or OpenGLES for GLES version)
      -  JPEG library
      -  Make and Package Config
      -  Xrandr
   -  Optional:
      -  Xinerama (for multiple monitor support).
      -  libarchive (for .7z, .rar, .tar.gz and .tar.bz2 archive support).
      -  Libcurl (for network info/artwork scraping).

2. Extract the Attract-Mode source to your system.

3. From the directory you extracted the source into, run:

   ```sh
   make
   ```

   OpenGL ES support is dropped in Attract-Mode Plus, if you are building on a Raspberry Pi, O-Droid, or another embedded SBC, you can build the OpenGL version with the following if your system supports DRM/KMS:

   ```sh
   make USE_DRM=1
   ```

   This step will create the "attract" executable file.

4. The final step is to actually copy the Attract-Mode executable and data to a location where they can be used. To install on a system-wide basis you should run:

   ```sh
   sudo make install
   ```

   This will copy the "attract" executable to `/usr/local/bin/` and default data to `/usr/local/share/attract/`.

   For a single user install on Linux or FreeBSD, you can complete this step by copying the contents of the "config" directory from the Attract-Mode source directory to the location that you will use as your Attract-Mode config directory. By default, this config directory is located in `$HOME/.attract` on Linux/FreeBSD systems.

   NOTE: The Attract-Mode makefile tries to follow the GNU standards for specifying installation directories: https://www.gnu.org/prep/standards/html_node/Directory-Variables.html. If you want to change the location where Attract-Mode looks for its default data from `/usr/local/share/attract` you should change these values appropriately before running the `make` and `make install` commands.

---

## OS X

These instructions assume that you have X Code installed.

1. Install Homebrew http://brew.sh. This can be done by running the following command at a terminal command prompt:

   ```sh
   ruby -e "$(curl -fsSL https://raw.github.com/Homebrew/homebrew/go/install)"
   ```

2. Install the "pkg-config", "ffmpeg", "sfml" and "libarchive" homebrew recipes:

   ```sh
   brew update
   brew install pkg-config ffmpeg sfml libarchive
   ```

3. Extract the Attract-Mode source to your system.

4. From the directory you extracted the Attract-Mode source into, run:

   ```sh
   make
   ```

   This step will create the "attract" executable file.

5. The final step is to actually copy the Attract-Mode executable and data to the location where they will be used. You can run:

   ```sh
   sudo make install
   ```

   to install on a system-wide basis. This will copy the 'attract' executable to `/usr/local/bin/` and data to `/usr/local/share/attract/`

   If you prefer to do a single user install, you can complete this step by copying the contents of the "config" directory from the Attract-Mode source directory to the location that you will use as your Attract-Mode config directory. By default, this config directory is `$HOME/.attract` on OS X.

---

## Windows (cross-compile)

The recommended way to build Windows binaries for Attract-Mode is to cross compile on an OS that supports MXE http://mxe.cc such as Linux, FreeBSD or OS X.

1. Follow the steps in the mxe tutorial to set up mxe on your system: http://mxe.cc/#tutorial

2. Make mxe's sfml, ffmpeg, libarchive and curl packages:

   ```sh
   make ffmpeg sfml libarchive curl
   ```

   the above command will make 32-bit versions of ffmpeg, sfml, libarchive and curl (and anything else that they depend on). To make the 64-bit version use the following:

   ```sh
   make MXE_TARGETS='x86_64-w64-mingw32.static' ffmpeg sfml libarchive curl
   ```

3. Extract the Attract-Mode source to your system.

4. From the directory you extracted the source into, run the following:

   ```sh
   make CROSS=1 TOOLCHAIN=i686-w64-mingw32.static WINDOWS_STATIC=1
   ```

   to build the 32-bit version of Attract-Mode. To build 64-bit, run:

   ```sh
   make CROSS=1 TOOLCHAIN=x86_64-w64-mingw32.static WINDOWS_STATIC=1
   ```

   This step will create the "attract.exe" executable file.

5. Copy the contents of the config directory from the Attract-Mode source directory and the executable you just built into the same directory on your Windows-based system, and you should be ready to go!

---

## Windows (native compile)

1. Install MSYS2 https://msys2.github.io/

2. Launch the MSYS2 shell and update the system:

   ```sh
   pacman --needed -Sy bash pacman pacman-mirrors msys2-runtime
   ```

3. Close MSYS2 Shell, run it again and run the following command:

   ```sh
   pacman -Syu
   ```

4. Install required packages. (optionally use the mingw-w64-i686-toolchain instead for 32-bit windows architectures), install "all" (by default) :

   ```sh
   pacman -S git mingw-w64-x86_64-toolchain msys/make mingw64/mingw-w64-x86_64-sfml mingw64/mingw-w64-x86_64-ffmpeg mingw64/mingw-w64-x86_64-libarchive
   ```

5. Clone and make Attract-Mode

   ```sh
   git clone https://github.com/mickelson/attract attract
   cd attract
   make
   ```

   This builds a version of Attract-Mode with various .dll dependencies. To run the program, you will need to add `c:\msys64\mingw64\bin` to your path (for 64-bit systems) or copy the dependent .dlls from that directory into the same directory you will run Attract-Mode from.
