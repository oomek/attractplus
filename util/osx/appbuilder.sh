#!/bin/bash

echo "STEP 1 - PREPARE BUNDLE FOLDERS"

# Default to "artifacts" if not provided, as buildpath
buildpath=${1:-"artifacts"}

echo "Build Path: $buildpath"

# Define folder path variables
bundlehome="$buildpath/Attract Mode Plus.app"
bundlecontent="$bundlehome"/Contents
bundlelibs="$bundlecontent"/libs

# Clean up previous builds
rm -Rf "$bundlehome"

# Create the app bundle folders
mkdir -p "$bundlehome" "$bundlecontent" "$bundlelibs"
mkdir -p "$bundlecontent"/MacOS "$bundlecontent"/Resources "$bundlecontent"/share "$bundlecontent"/share/attract

echo "STEP 2 - EXECUTABLE AND LIBRARY HANDLING"

# Use the passed or default 'basedir' as the executable path
basedir=${2:-"am"}
attractname="$basedir/attractplus"  # Executable path

# Check if the executable exists
if [ ! -f "$attractname" ]; then
  echo "Error: Executable $attractname does not exist!"
  exit 1
fi

# Initialize arrays to track libraries
VISITED=()
RESOLVED=()

# Function to recursively resolve library dependencies
resolve_links() {
  local file="$1"
  [[ ! -f "$file" ]] && return
  [[ " ${VISITED[*]} " =~ " ${file} " ]] && return

  VISITED+=("$file")

  local links
  links=$(otool -L "$file" | tail -n +2 | awk '{print $1}')

  while IFS= read -r lib; do
    local resolved=""

    # --- Special case: @rpath/libsfml* -> am/obj/sfml/install/lib ---
    if [[ "$lib" == @rpath/libsfml* ]]; then
      local libfile="${lib#@rpath/}"
      local sfml_candidate="am/obj/sfml/install/lib/$libfile"
      if [[ -f "$sfml_candidate" ]]; then
        resolved="$sfml_candidate"
        echo "Resolved SFML override: $lib -> $resolved"
      fi
    fi

    # --- Try pkg-config if still unresolved and lib is @rpath/... ---
    if [[ -z "$resolved" && "$lib" == @rpath/* ]]; then
      local libfile="${lib#@rpath/}"
      local base="${libfile%%.dylib*}"         # e.g. libwebp.7 or libwebp.0.1
      base="${base%%.*}"                       # extract just the base, e.g. libwebp
      echo "Running pkg-config --libs-only-L for: $base"
      local pkg_lib
      pkg_lib=$(pkg-config --libs-only-L "$base" 2>/dev/null)
      echo "pkg-config result for $base: $pkg_lib"

      if [[ -n "$pkg_lib" ]]; then
        local pkg_dir="${pkg_lib#-L}"
        local candidate="$pkg_dir/$libfile"
        if [[ -f "$candidate" ]]; then
          resolved="$candidate"
        fi
      fi
    fi

    # --- Try absolute path as-is ---
    if [[ -z "$resolved" && -f "$lib" ]]; then
      resolved="$lib"
    fi

    # --- Try rpath entries in binary ---
    if [[ -z "$resolved" && "$lib" == @rpath/* ]]; then
      local rpaths
      rpaths=$(otool -l "$file" | awk '
        $1 == "cmd" && $2 == "LC_RPATH" {r=1}
        r && $1 == "path" {print $2; r=0}
      ')
      for rpath in $rpaths; do
        local candidate="$rpath/${lib#@rpath/}"
        if [[ -f "$candidate" ]]; then
          resolved="$candidate"
          break
        fi
      done
    fi

    # --- Store and recurse ---
    if [[ -n "$resolved" && ! " ${RESOLVED[*]} " =~ " ${resolved} " ]]; then
      RESOLVED+=("$resolved")
      resolve_links "$resolved"
    fi
  done <<< "$links"
}

# Start resolving libraries from the executable
resolve_links "$attractname"

echo "STEP 3 - COPYING LIBRARIES TO BUNDLE"

for lib in "${RESOLVED[@]}"; do
  lib_name=$(basename "$lib")
  if [[ ! -f "$bundlelibs/$lib_name" ]]; then
    echo "Copying $lib to $bundlelibs/$lib_name"
    cp -v "$lib" "$bundlelibs/$lib_name"
  fi
done

echo "STEP 4 - UPDATING LIBRARY PATHS"

# Update library paths in the executable and all resolved libraries
# Change paths for all copied libraries
libsarray=( $(ls "$bundlecontent"/libs) )
for str in ${libsarray[@]}; do
   echo fixing $str
   subarray=( $(otool -L "$bundlelibs"/$str | tail -n +2 | grep '/opt/homebrew\|@rpath' | awk -F' ' '{print $1}') )
   subarray_fix=( $(otool -L "$bundlelibs"/$str | tail -n +2 | grep '/opt/homebrew\|@rpath' | awk -F' ' '{print $1}') )

	for enum in ${!subarray[@]}; do
      str3=$( basename "${subarray[enum]}" )
      str2="${subarray[enum]}"
      install_name_tool -change $str2 @loader_path/../libs/$str3 "$bundlelibs"/$str 2>/dev/null
   done
   install_name_tool -id @loader_path/../libs/$str "$bundlelibs"/$str 2>/dev/null
done

echo "Library paths updated successfully!"

echo STEP 5 - POPULATE BUNDLE FOLDER

# Copy assets to bundle folder
# cp -r $basedir/config "$bundlecontent"/
cp -a $basedir/config/ "$bundlecontent"/share/attractplus
cp -a $basedir/attractplus "$bundlecontent"/MacOS/
cp -a $basedir/util/osx/attractplus.icns "$bundlecontent"/Resources/
cp -a $basedir/util/osx/launch.sh "$bundlecontent"/MacOS/
#cp "$bundlelibs"/libfreetype.6.dylib "$bundlelibs"/freetype

# Prepare plist file
LASTTAG=$(git -C $basedir/ describe --tag --abbrev=0)
VERSION=$(git -C $basedir/ describe --tag | sed 's/-[^-]\{8\}$//')
BUNDLEVERSION=${VERSION//[v-]/.}; BUNDLEVERSION=${BUNDLEVERSION#"."}
SHORTVERSION=${LASTTAG//v/}

sed -e 's/%%SHORTVERSION%%/'${SHORTVERSION}'/' -e 's/%%BUNDLEVERSION%%/'${BUNDLEVERSION}'/' $basedir/util/osx/Info.plist > "$bundlecontent"/Info.plist

echo STEP 6 - FIX ATTRACTPLUS EXECUTABLE

# Update rpath in the executable to point to the new libs folder inside the app bundle
install_name_tool -add_rpath "@executable_path/../libs" "$attractname"

# List libraries linked in attractplus
attractlibs=( $(otool -L $attractname | tail -n +2 | grep '/opt/homebrew\|@rpath' | awk -F' ' '{print $1}') )

# Apply new links to libraries
for str in ${attractlibs[@]}; do
   str2=$( basename "$str" )
   install_name_tool -change $str @loader_path/../libs/$str2 "$bundlecontent"/MacOS/attractplus
done
#codesign --force -s - "$bundlecontent"/MacOS/attractplus

echo STEP 7 - RENAME ARTIFACT TO v${SHORTVERSION}

newappname="$buildpath/Attract-Mode Plus v${SHORTVERSION}.app"
mv "$bundlehome" "$newappname"

signapp=${3:-"no"}

if [[ $signapp == "yes" ]]
then
	echo STEP 8 - AD HOC SIGNING
	libsarray=( $(ls "$newappname/Contents/libs") )
	for str in ${libsarray[@]}; do
		codesign --force -s - "$newappname/Contents/libs/$str"
	done
	codesign --force -s - "$newappname/Contents/MacOS/attractplus"
	codesign --force -s - "$newappname"
fi

echo ALL DONE
