##
##
##  Attract-Mode frontend
##  Copyright (C) 2013-2018 Andrew Mickelson
##
##  This file is part of Attract-Mode.
##
##  Attract-Mode is free software: you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation, either version 3 of the License, or
##  (at your option) any later version.
##
##  Attract-Mode is distributed in the hope that it will be useful,
##  but WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
##  GNU General Public License for more details.
##
##  You should have received a copy of the GNU General Public License
##  along with Attract-Mode.  If not, see <http://www.gnu.org/licenses/>.
##
##
###############################
#
# BUILD CONFIGURATION OPTIONS:
#
# Uncomment the next line to build the DRM/KMS version (alternative to X11)
#USE_DRM=1
#
# Uncomment next line to disable movie support (i.e. no FFmpeg).
#NO_MOVIE=1
#
# Uncomment the next line to enable the use of MMAL video decoder
#USE_MMAL=1
#
# By default, if FontConfig gets enabled we link against the system's expat
# library (because FontConfig uses expat too).  If FontConfig is not used
# then Attract-Mode is statically linked to its own version of expat.
# Uncomment next line to always link to Attract-Mode's version of expat.
#BUILD_EXPAT=1
#
# Uncomment next line for Windows static cross-compile build (mxe)
#WINDOWS_STATIC=1
#
# By default, Attract-Mode on Windows is built as a GUI application, which
# does not allow for command line interactions at the Windows console.
# Uncomment the next line to build a console version of Attract-Mode
# instead (Windows only)
#WINDOWS_CONSOLE=1
#
# Uncomment the next line(s) as appropriate to enable hardware video
# decoding on Linux (req FFmpeg >= 3.4)
#FE_HWACCEL_VAAPI=1
#FE_HWACCEL_VDPAU=1
#
# If you set this to 1, AM+ will link the system SFML version
# If left to blank, AM+ will build and link the extlib/SFML version
#USE_SYSTEM_SFML=1
###############################

#FE_DEBUG=1
#VERBOSE=1
#WINDOWS_XP=1

override FE_VERSION := v3.1.0

CC ?= gcc
CXX ?= g++
CFLAGS = $(EXTRA_CFLAGS)
STRIP ?= strip
PKG_CONFIG ?= pkg-config
AR ?= ar
ARFLAGS ?= rc
RM=rm -f
MD=mkdir -p
WINDRES=windres
CMAKE=cmake
LD=objcopy
B64FLAGS = -w0

CFLAGS += -DSQUSEDOUBLE
CFLAGS += -std=c++17

ifndef OPTIMIZE
OPTIMIZE=2
endif

STATIC ?= 1

ifndef VERBOSE
 SILENT=@
 CC_MSG = @echo Compiling $@...
 AR_MSG = @echo Archiving $@...
 EXE_MSG = @echo Creating executable: $@
 MAKEFLAGS += --no-print-directory
endif

ifneq ($(origin TOOLCHAIN),undefined)
override CC := $(TOOLCHAIN)-$(CC)
override CXX := $(TOOLCHAIN)-$(CXX)
override AR := $(TOOLCHAIN)-$(AR)
override CMAKE := $(TOOLCHAIN)-$(CMAKE)
override LD := $(TOOLCHAIN)-$(LD)
endif

ifneq ($(origin CROSS),undefined)
override STRIP := $(TOOLCHAIN)-$(STRIP)
override PKG_CONFIG := $(TOOLCHAIN)-$(PKG_CONFIG)
PKG_CONFIG_MXE=_$(subst .,_,$(TOOLCHAIN))
override PKG_CONFIG_MXE :=$(subst -,_,$(PKG_CONFIG_MXE))
override WINDRES := $(TOOLCHAIN)-$(WINDRES)
endif

# Debian packager doesn't set STRIP when crosscompiling
ifneq ($(DEB_HOST_GNU_TYPE),)
override STRIP := $(DEB_HOST_GNU_TYPE)-strip
endif

prefix ?= /usr/local
datarootdir=$(prefix)/share
datadir=$(datarootdir)
exec_prefix=$(prefix)
bindir=$(exec_prefix)/bin

DATA_PATH:=$(datadir)/attract/
EXE_BASE=attractplus
EXE_EXT=
OBJ_DIR=obj
SRC_DIR=src
EXTLIBS_DIR=extlibs
RES_DIR=resources
FE_FLAGS=
RES_SRC_DIR=resources
RES_DIR=$(OBJ_DIR)/$(RES_SRC_DIR)
RES_FONTS_DIR = $(RES_DIR)/fonts
RES_IMGS_DIR = $(RES_DIR)/images

_DEP =\
	fe_base.hpp \
	fe_util.hpp \
	fe_util_sq.hpp \
	fe_info.hpp \
	fe_input.hpp \
	fe_romlist.hpp \
	scraper_base.hpp \
	scraper_xml.hpp \
	fe_settings.hpp \
	fe_config.hpp \
	fe_presentable.hpp \
	fe_present.hpp \
	sprite.hpp \
	fe_image.hpp \
	fe_sound.hpp \
	fe_music.hpp \
	fe_shader.hpp \
	fe_overlay.hpp \
	fe_window.hpp \
	tp.hpp \
	fe_text.hpp \
	fe_listbox.hpp \
	rounded_rectangle_shape.hpp \
	fe_rectangle.hpp \
	fe_vm.hpp \
	fe_blend.hpp \
	fe_cache.hpp \
	path_cache.hpp \
	image_loader.hpp \
	base64.hpp \
	zip.hpp

_OBJ =\
	fe_base.o \
	fe_util.o \
	fe_util_sq.o \
	fe_cmdline.o \
	fe_info.o \
	fe_input.o \
	fe_romlist.o \
	fe_settings.o \
	scraper_base.o \
	scraper_xml.o \
	scraper_general.o \
	scraper_net.o \
	scraper_gamesdb.o \
	fe_config.o \
	fe_presentable.o \
	fe_present.o \
	sprite.o \
	fe_image.o \
	fe_sound.o \
	fe_music.o \
	fe_shader.o \
	fe_overlay.o \
	fe_window.o \
	tp.o \
	fe_text.o \
	fe_listbox.o \
	rounded_rectangle_shape.o \
	fe_rectangle.o \
	fe_vm.o \
	fe_blend.o \
	zip.o \
	fe_cache.o \
	path_cache.o \
	image_loader.o \
	base64.o \
	main.o

_RES =\
	resources/fonts/BarlowCJK.ttf \
	resources/fonts/Attract.ttf \
	resources/images/Logo.png

#
# Backward compatibility with WINDOWS_STATIC
#

ifeq ($(WINDOWS_STATIC),1)
  STATIC=1
  FE_WINDOWS_COMPILE=1
endif

ifneq ($(FE_WINDOWS_COMPILE),1)
 #
 # Test OS to set some defaults
 #
 ifeq ($(OS),Windows_NT)
  #
  # Windows
  #
  FE_WINDOWS_COMPILE=1
 else
  UNAME = $(shell uname -a)
  ifeq ($(firstword $(filter Darwin,$(UNAME))),Darwin)
   FE_MACOSX_COMPILE=1
  endif
  ifeq ($(FE_MACOSX_COMPILE),1)
   #
   # Mac OS X
   #
   _DEP += fe_util_osx.hpp
   _OBJ += fe_util_osx.o
   override B64FLAGS = -b 0 -i
   LIBS += -framework Cocoa -framework Carbon -framework IOKit -framework CoreVideo -framework OpenAL
  else
   ifeq ($(USE_DRM),1)
   else
    #
    # Test for Xlib and Xinerama...
    #
    ifeq ($(shell $(PKG_CONFIG) --exists x11 && echo "1" || echo "0"), 1)
     USE_XLIB=1
     ifeq ($(shell $(PKG_CONFIG) --exists xinerama && echo "1" || echo "0"), 1)
      USE_XINERAMA=1
     endif
    endif
   endif
  endif
 endif
endif

#
# Deal with SFML
#
ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
SFML_OBJ_DIR = $(OBJ_DIR)/sfml
SFML_LIB_DIR=$(SFML_OBJ_DIR)/install/lib/
SFML_PKG_CONFIG_PATH=$(ROOT_DIR)/$(SFML_OBJ_DIR)/install/lib/pkgconfig
LIBS += -L$(SFML_LIB_DIR)
SFML_PC="sfml-system sfml-window sfml-audio sfml-graphics"
SFML_TOKEN=$(SFML_OBJ_DIR)/.sfmlok


ifeq ($(FE_MACOSX_COMPILE),1)
  LIBS += -framework OpenGL
  LIBS += $(shell pkg-config --libs-only-L freetype2) -lfreetype
  LIBS += $(shell pkg-config --libs-only-L libjpeg) -ljpeg
else
  LIBS += -lfreetype
endif

ifneq ($(FE_WINDOWS_COMPILE),1)
 ifneq ($(FE_MACOSX_COMPILE),1)
  LIBS += -ldl -lGL -lpthread -lFLAC -logg -lvorbis -lvorbisfile -lvorbisenc -lopenal
 endif
else
 LIBS += -lopengl32 -lgdi32
 LIBS += -L$(EXTLIBS_DIR)/openal-soft
 LIBS += -lopengl32 -lFLAC -lvorbisfile -lopenal32-s -lwinmm
endif


ifeq ($(FE_WINDOWS_COMPILE),1)
 _DEP += attractplus.rc
 _OBJ += attractplus.res
 ifeq ($(WINDOWS_XP),1)
  FE_FLAGS += -DWINDOWS_XP
 else
  LIBS += -ldwmapi
 endif
 ifeq ($(WINDOWS_CONSOLE),1)
  CFLAGS += -mconsole
  FE_FLAGS += -DWINDOWS_CONSOLE
  EXE_BASE=attractplus-console
 else
  CFLAGS += -Wl,--subsystem,windows
 endif

 EXE_EXT = .exe
else
 CFLAGS += -DDATA_PATH=\"$(DATA_PATH)\"
endif

#
# Check whether optional libs should be enabled
#
ifeq ($(shell $(PKG_CONFIG) --exists libarchive && echo "1" || echo "0"), 1)
 USE_LIBARCHIVE=1
endif

ifeq ($(shell $(PKG_CONFIG) --exists libcurl && echo "1" || echo "0"), 1)
 USE_LIBCURL=1
endif

#
# Now process the various settings...
#
ifeq ($(FE_DEBUG),1)
 CFLAGS += -g -Wall
 FE_FLAGS += -DFE_DEBUG
else
 CFLAGS += -O$(OPTIMIZE) -DNDEBUG
endif

ifeq ($(USE_MMAL),1)
 FE_FLAGS += -DUSE_MMAL
endif

ifeq ($(USE_XLIB),1)
 FE_FLAGS += -DUSE_XLIB
 LIBS += -lX11 -lXi -lXrandr -lXcursor

ifeq ($(USE_XINERAMA),1)
  FE_FLAGS += -DUSE_XINERAMA
  LIBS += -lXinerama
 endif

endif

ifeq ($(USE_DRM),1)
 PKG_CONFIG_LIBS += libdrm gbm
 FE_FLAGS += -DUSE_DRM
 LIBS += -lEGL
endif

ifeq ($(FE_HWACCEL_VAAPI),1)
 FE_FLAGS += -DFE_HWACCEL_VAAPI
endif

ifeq ($(FE_HWACCEL_VDPAU),1)
 FE_FLAGS += -DFE_HWACCEL_VDPAU
endif

ifeq ($(USE_LIBARCHIVE),1)
 FE_FLAGS += -DUSE_LIBARCHIVE
 PKG_CONFIG_LIBS += libarchive
 LIBS += -lz
else
 CFLAGS += -I$(EXTLIBS_DIR)/miniz
endif

ifeq ($(USE_LIBCURL),1)
 FE_FLAGS += -DUSE_LIBCURL
 PKG_CONFIG_LIBS += libcurl
 _DEP += fe_net.hpp
 _OBJ += fe_net.o
endif

ifeq ($(NO_MOVIE),1)
 FE_FLAGS += -DNO_MOVIE
 ifeq ($(WINDOWS_STATIC),1)
  LIBS += -lsfml-audio-s
 else
  LIBS += -lsfml-audio
 endif
else
 PKG_CONFIG_LIBS += libavformat libavcodec libavutil libswscale libswresample
 _DEP += media.hpp
 _OBJ += media.o
endif

CFLAGS += -D__STDC_CONSTANT_MACROS -I$(RES_IMGS_DIR) -I$(RES_FONTS_DIR)

ifeq ($(shell $(PKG_CONFIG) --libs $(PKG_CONFIG_LIBS) && echo "1" || echo "0"), 0)
  $(error pkg-config couldn't find some libraries, aborting)
endif
LIBS := $(LIBS) $(shell $(PKG_CONFIG) --libs $(PKG_CONFIG_LIBS))
CFLAGS := $(CFLAGS) $(shell $(PKG_CONFIG) --cflags $(PKG_CONFIG_LIBS))

EXE = $(EXE_BASE)$(EXE_EXT)

ifeq ($(BUILD_EXPAT),1)
 CFLAGS += -I$(EXTLIBS_DIR)/expat
 EXPAT = $(OBJ_DIR)/libexpat.a
 ifneq ($(FE_WINDOWS_COMPILE),1)
  EXPAT_FLAGS = -DXML_DEV_URANDOM -DHAVE_MEMMOVE
 endif
else
 LIBS += -lexpat
 EXPAT =
endif

# Boost static linking
ifeq ($(FE_WINDOWS_COMPILE),1)
 LIBS += -lboost_system-mt -lboost_filesystem-mt
else ifeq ($(FE_MACOSX_COMPILE),1)
 PKG_CONFIG_LIBS += boost_system boost_filesystem
else
 LIBS += -l:libboost_filesystem.a -l:libboost_system.a
endif

CFLAGS += -I$(EXTLIBS_DIR)/squirrel/include -I$(EXTLIBS_DIR)/sqrat/include -I$(EXTLIBS_DIR)/nowide -I$(EXTLIBS_DIR)/nvapi -I$(EXTLIBS_DIR)/rapidjson/include -I$(EXTLIBS_DIR)/cereal
SQUIRREL = $(OBJ_DIR)/libsquirrel.a $(OBJ_DIR)/libsqstdlib.a

# Our nowide "lib" is only needed on Windows systems
ifeq ($(FE_WINDOWS_COMPILE),1)
 SQUIRREL += $(OBJ_DIR)/libnowide.a
endif

OBJ = $(patsubst %,$(OBJ_DIR)/%,$(_OBJ))
DEP = $(patsubst %,$(SRC_DIR)/%,$(_DEP))
RES = $(patsubst %,$(OBJ_DIR)/%.h,$(_RES))

VER_TEMP  = $(subst -, ,$(if $(FE_VERSION_NUM_PART),v$(FE_VERSION_NUM_PART),$(FE_VERSION)))
VER_PARTS = $(subst ., ,$(word 1,$(VER_TEMP)))
VER_MAJOR = $(subst v,,$(word 1,$(VER_PARTS)))
VER_MINOR = $(word 2,$(VER_PARTS))
ifneq ($(word 3,$(VER_PARTS)),)
 VER_POINT = $(word 3,$(VER_PARTS))
else
 VER_POINT = 0
endif

# version macros
FE_FLAGS += -DFE_VERSION_MAJOR=$(VER_MAJOR) -DFE_VERSION_MINOR=$(VER_MINOR) -DFE_VERSION_POINT=$(VER_POINT)
FE_FLAGS += -DFE_VERSION_D='"$(FE_VERSION)"' -DFE_VERSION_NUM=$(VER_MAJOR)$(VER_MINOR)$(VER_POINT)

BUILD_NR = 0
ifdef GITHUB_RUN_NUMBER
  BUILD_NR = $(GITHUB_RUN_NUMBER)
  FE_FLAGS += -DFE_BUILD_D='"( Build $(BUILD_NR) ) "'
else
  FE_FLAGS += -DFE_BUILD_D='""'
endif

$(OBJ_DIR)/%.res: $(SRC_DIR)/%.rc | $(OBJ_DIR)
	$(CC_MSG)
	$(SILENT)$(WINDRES) $(FE_FLAGS) $< -O coff -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(DEP) $(RES) | $(OBJ_DIR)
	$(CC_MSG)
	$(SILENT)$(CXX) -c -o $@ $< $(CFLAGS) $(FE_FLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.mm $(DEP) | $(OBJ_DIR)
	$(CC_MSG)
	$(SILENT)$(CC) -c -o $@ $< $(CFLAGS) $(FE_FLAGS)

.PRECIOUS: $(OBJ_DIR)/%.h
$(OBJ_DIR)/%.h: % | $(RES_FONTS_DIR) $(RES_IMGS_DIR)
	$(info Converting $< to $@ ...)
	$(shell (echo 'const char* _binary_$(subst .,_,$(subst /,_,$<)) = "' ; base64 $(B64FLAGS) $< ; echo '";') | tr -d '\n' > $@)


$(EXE): $(OBJ) $(EXPAT) $(SQUIRREL)
	$(EXE_MSG)
	$(SILENT)$(CXX) -o $@ $^ $(CFLAGS) $(FE_FLAGS) $(LIBS)
ifneq ($(FE_DEBUG),1)
	$(SILENT)$(STRIP) $@
endif

.PHONY: clean
.PHONY: install
.PHONY: sfml sfmlbuild
.PHONY: bin2h

SFML_FLAGS =
ifneq ($(USE_SYSTEM_SFML),1)
sfmlbuild:
ifneq ("$(wildcard $(SFML_TOKEN))","")
	$(info SFML is already built)
else
	$(info Building SFML...)
ifeq ($(STATIC),1)
	$(eval SFML_FLAGS += -DBUILD_SHARED_LIBS=FALSE)
ifeq ($(FE_WINDOWS_COMPILE),1)
	$(eval SFML_FLAGS += -DCMAKE_CXX_FLAGS="-DAL_LIBTYPE_STATIC=TRUE")
endif
else
	$(eval SFML_FLAGS += -DBUILD_SHARED_LIBS=TRUE)
endif
ifeq ($(USE_DRM),1)
	$(eval SFML_FLAGS += -DSFML_USE_DRM=1)
endif
ifeq ($(FE_MACOSX_COMPILE),1)
	$(eval SFML_FLAGS += -DSFML_USE_SYSTEM_DEPS=1)
endif
	$(SILENT)$(CMAKE) -S extlibs/SFML -B $(SFML_OBJ_DIR) -DCMAKE_INSTALL_PREFIX=$(SFML_OBJ_DIR)/install -DOpenGL_GL_PREFERENCE=GLVND -DSFML_INSTALL_PKGCONFIG_FILES=TRUE -DSFML_BUILD_NETWORK=FALSE $(SFML_FLAGS)
	+$(SILENT)$(CMAKE) --build obj/sfml --config Release --target install
	touch $(SFML_TOKEN)
endif
else
sfmlbuild:

endif

sfml: sfmlbuild
ifeq ($(STATIC),1)
	$(eval SFML_LIBS += $(shell PKG_CONFIG_PATH$(PKG_CONFIG_MXE)="$(SFML_PKG_CONFIG_PATH):${PKG_CONFIG_PATH}" $(PKG_CONFIG) --static --libs-only-L $(SFML_PC)))
	$(info Manually adding sfml libs as pkg-config has no --static version)
	$(eval SFML_LIBS += -lsfml-graphics-s -lsfml-window-s -lsfml-audio-s -lsfml-system-s)
	$(eval CFLAGS += -DSFML_STATIC $(shell PKG_CONFIG_PATH$(PKG_CONFIG_MXE)="$(SFML_PKG_CONFIG_PATH):${PKG_CONFIG_PATH}" $(PKG_CONFIG) --static --cflags $(SFML_PC)))
ifeq ($(FE_WINDOWS_COMPILE),1)
else ifeq ($(FE_MACOSX_COMPILE),1)
else
		$(eval SFML_LIBS += -lGL -lGLU -lm -lz -ludev -lrt)
endif
else ifneq ($(USE_SYSTEM_SFML), 1)
	# SFML may not generate .pc files, so manually add libs
	$(eval CFLAGS += $(shell PKG_CONFIG_PATH$(PKG_CONFIG_MXE)="$(SFML_PKG_CONFIG_PATH):${PKG_CONFIG_PATH}" $(PKG_CONFIG) --cflags $(SFML_PC)))
	$(eval SFML_LIBS += $(shell PKG_CONFIG_PATH$(PKG_CONFIG_MXE)="$(SFML_PKG_CONFIG_PATH):${PKG_CONFIG_PATH}" $(PKG_CONFIG) --libs $(SFML_PC)))
	#LIBS += -lsfml-graphics -lsfml-window -lsfml-system
else
	$(eval CFLAGS += $(shell $(PKG_CONFIG) --cflags $(SFML_PC)))
	$(eval SFML_LIBS += $(shell $(PKG_CONFIG) --libs $(SFML_PC)))
	$(info SFML_lIBS=$(SFML_LIBS))
endif
	$(eval override LIBS = $(SFML_LIBS) $(LIBS))

# .WAIT is supported from make 4.4, not yet standard sadly. So the all target appreared
#$(OBJ_DIR) : sfml .WAIT headerinfo
$(OBJ_DIR): headerinfo
	$(MD) $@

$(RES_FONTS_DIR): $(OBJ_DIR)
	$(MD) $@

$(RES_IMGS_DIR): $(OBJ_DIR)
	$(MD) $@

headerinfo: sfml
	$(info flags: $(CFLAGS) $(FE_FLAGS))
	$(info libs: $(LIBS))

#
# Expat Library
#
EXPAT_OBJ_DIR = $(OBJ_DIR)/expat

EXPATOBJS = \
	$(EXPAT_OBJ_DIR)/xmlparse.o \
	$(EXPAT_OBJ_DIR)/xmlrole.o \
	$(EXPAT_OBJ_DIR)/xmltok.o

$(OBJ_DIR)/libexpat.a: $(EXPATOBJS) | $(EXPAT_OBJ_DIR)
	$(AR_MSG)
	$(SILENT)$(AR) $(ARFLAGS) $@ $(EXPATOBJS)

$(EXPAT_OBJ_DIR)/%.o: $(EXTLIBS_DIR)/expat/%.c | $(EXPAT_OBJ_DIR)
	$(CC_MSG)
	$(SILENT)$(CC) -c $< -o $@ $(CFLAGS) $(EXPAT_FLAGS)

$(EXPAT_OBJ_DIR):
	$(MD) $@

#
# Squirrel Library
#
SQUIRREL_FLAGS = -fno-exceptions -fno-rtti -fno-strict-aliasing
SQUIRREL_OBJ_DIR = $(OBJ_DIR)/squirrel

SQUIRRELOBJS= \
	$(SQUIRREL_OBJ_DIR)/sqapi.o \
	$(SQUIRREL_OBJ_DIR)/sqbaselib.o \
	$(SQUIRREL_OBJ_DIR)/sqfuncstate.o \
	$(SQUIRREL_OBJ_DIR)/sqdebug.o \
	$(SQUIRREL_OBJ_DIR)/sqlexer.o \
	$(SQUIRREL_OBJ_DIR)/sqobject.o \
	$(SQUIRREL_OBJ_DIR)/sqcompiler.o \
	$(SQUIRREL_OBJ_DIR)/sqstate.o \
	$(SQUIRREL_OBJ_DIR)/sqtable.o \
	$(SQUIRREL_OBJ_DIR)/sqmem.o \
	$(SQUIRREL_OBJ_DIR)/sqvm.o \
	$(SQUIRREL_OBJ_DIR)/sqclass.o

$(OBJ_DIR)/libsquirrel.a: $(SQUIRRELOBJS) | $(SQUIRREL_OBJ_DIR)
	$(AR_MSG)
	$(SILENT)$(AR) $(ARFLAGS) $@ $(SQUIRRELOBJS)

$(SQUIRREL_OBJ_DIR)/%.o: $(EXTLIBS_DIR)/squirrel/squirrel/%.cpp | $(SQUIRREL_OBJ_DIR)
	$(CC_MSG)
	$(SILENT)$(CXX) -c $< -o $@ $(CFLAGS) $(SQUIRREL_FLAGS)

$(SQUIRREL_OBJ_DIR):
	$(MD) $@

#
# Squirrel libsqstdlib
#
SQSTDLIB_OBJ_DIR = $(OBJ_DIR)/sqstdlib

SQSTDLIBOBJS= \
	$(SQSTDLIB_OBJ_DIR)/sqstdblob.o \
	$(SQSTDLIB_OBJ_DIR)/sqstdio.o \
	$(SQSTDLIB_OBJ_DIR)/sqstdstream.o \
	$(SQSTDLIB_OBJ_DIR)/sqstdmath.o \
	$(SQSTDLIB_OBJ_DIR)/sqstdstring.o \
	$(SQSTDLIB_OBJ_DIR)/sqstdaux.o \
	$(SQSTDLIB_OBJ_DIR)/sqstdsystem.o \
	$(SQSTDLIB_OBJ_DIR)/sqstdrex.o

$(OBJ_DIR)/libsqstdlib.a: $(SQSTDLIBOBJS) | $(SQSTDLIB_OBJ_DIR)
	$(AR_MSG)
	$(SILENT)$(AR) $(ARFLAGS) $@ $(SQSTDLIBOBJS)

$(SQSTDLIB_OBJ_DIR)/%.o: $(EXTLIBS_DIR)/squirrel/sqstdlib/%.cpp | $(SQSTDLIB_OBJ_DIR)
	$(CC_MSG)
	$(SILENT)$(CXX) -c $< -o $@ $(CFLAGS) $(SQUIRREL_FLAGS)

$(SQSTDLIB_OBJ_DIR): sfml
	$(MD) $@

#
# Nowide
#
NOWIDE_OBJ_DIR = $(OBJ_DIR)/nowidelib

NOWIDEOBJS= \
	$(NOWIDE_OBJ_DIR)/iostream.o \
	$(NOWIDE_OBJ_DIR)/filebuf.o \
	$(NOWIDE_OBJ_DIR)/cstdio.o \
	$(NOWIDE_OBJ_DIR)/cstdlib.o \
	$(NOWIDE_OBJ_DIR)/console_buffer.o \
	$(NOWIDE_OBJ_DIR)/stat.o

$(OBJ_DIR)/libnowide.a: $(NOWIDEOBJS) | $(NOWIDE_OBJ_DIR)
	$(AR_MSG)
	$(SILENT)$(AR) $(ARFLAGS) $@ $(NOWIDEOBJS)

$(NOWIDE_OBJ_DIR)/%.o: $(EXTLIBS_DIR)/nowide/%.cpp | $(NOWIDE_OBJ_DIR)
	$(CC_MSG)
	$(SILENT)$(CXX) -c $< -o $@ $(CFLAGS)

$(NOWIDE_OBJ_DIR):
	$(MD) $@

$(DATA_PATH):
	$(MD) -p $(DESTDIR)$@

install: $(EXE) $(DATA_PATH)
	install -D $(EXE) $(DESTDIR)$(bindir)/$(EXE)
	mkdir -p $(DESTDIR)$(DATA_PATH)
	cp -r config/* $(DESTDIR)$(DATA_PATH)

smallclean:
	-$(RM) $(OBJ_DIR)/*.o *~ core $(RES_FONTS_DIR)/*.h $(RES_IMGS_DIR)/*.h

clean:
	-$(RM) -r $(OBJ_DIR)/*.o $(EXPAT_OBJ_DIR)/*.o $(SQUIRREL_OBJ_DIR)/*.o $(SQSTDLIB_OBJ_DIR)/*.o $(AUDIO_OBJ_DIR)/*.o $(NOWIDE_OBJ_DIR)/*.o $(OBJ_DIR)/*.a $(OBJ_DIR)/*.res $(SFML_OBJ_DIR)/* $(SFML_TOKEN) $(RES_FONTS_DIR)/*.h $(RES_IMGS_DIR)/*.h *~ core
