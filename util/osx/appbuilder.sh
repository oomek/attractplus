#!/bin/bash

echo STEP 1 - PREPARE BUNDLE FOLDERS

#CALL WITH "artifacts" as buildpath for CI
buildpath=${1:-"artifacts"}

echo $buildpath

# Define folder path variables
bundlehome="$buildpath/Attract Mode Plus.app"
bundlecontent="$bundlehome"/Contents
bundlelibs="$bundlecontent"/libs

rm -Rf "$bundlehome"
mkdir "$bundlehome"
mkdir "$bundlecontent"
mkdir "$bundlelibs"
mkdir "$bundlecontent"/MacOS
mkdir "$bundlecontent"/Resources
mkdir "$bundlecontent"/share
mkdir "$bundlecontent"/share/attract

#CALL WITH "am" AS PARAMETER 2
basedir=${2:-"am"}
attractname="$basedir/attractplus"

echo STEP 2 - COLLECT AND FIX LINKED LIBRARIES

# Define library fixing pairs
#-- Installing: /Users/djhan/buildattract/attractplus/obj/sfml/install/lib/freetype.framework/Versions/A/freetype

fr_lib+=("@rpath/libsfml")
to_lib+=("$basedir/obj/sfml/install/lib/libsfml")

checklib=$(brew --prefix)
fr_lib+=("/opt/homebrew/Cellar")
to_lib+=("$checklib/opt")

checklib=$(brew --prefix)
fr_lib+=("@loader_path/../../../../opt")
to_lib+=("$checklib/opt")

checklib=$(pkg-config --libs-only-L libsharpyuv)
checklib="${checklib:2}"
fr_lib+=("@rpath/libsharpyuv")
to_lib+=("$checklib/libsharpyuv")

checklib=$(pkg-config --libs-only-L libwebp)
checklib="${checklib:2}"
fr_lib+=("@rpath/libwebp")
to_lib+=("$checklib/libwebp")

checklib=$(pkg-config --libs-only-L libjxl_cms)
checklib="${checklib:2}"
fr_lib+=("@rpath/libjxl_cms")
to_lib+=("$checklib/libjxl_cms")

checklib=$(pkg-config --libs-only-L libavcodec)
checklib="${checklib:2}"
fr_lib+=("@loader_path/libavcodec")
to_lib+=("$checklib/libavcodec")

checklib=$(pkg-config --libs-only-L libswresample)
checklib="${checklib:2}"
fr_lib+=("@loader_path/libswresample")
to_lib+=("$checklib/libswresample")

checklib=$(pkg-config --libs-only-L libavutil)
checklib="${checklib:2}"
fr_lib+=("@loader_path/libavutil")
to_lib+=("$checklib/libavutil")

checklib=$(pkg-config --libs-only-L libcrypto)
checklib="${checklib:2}"
fr_lib+=("@loader_path/libcrypto")
to_lib+=("$checklib/libcrypto")

checklib=$(pkg-config --libs-only-L libbrotlicommon)
checklib="${checklib:2}"
fr_lib+=("@loader_path/libbrotlicommon")
to_lib+=("$checklib/libbrotlicommon")

#fr_lib+=("@rpath/libsharpyuv")
#to_lib+=("/usr/local/opt/webp/lib/libsharpyuv")

#fr_lib=("@rpath/../Frameworks")
#to_lib=("$basedir/obj/sfml/install/lib")

#"${myString:1}"

#fr_lib+=("@rpath/libwebp")
#to_lib+=("/usr/local/opt/webp/lib/libwebp")

# Build commands for processing
commands=("")
for enum in ${!fr_lib[@]}; do
	commands+=(s/$(sed 's/\//\\\//g' <<< "${fr_lib[enum]}")/$(sed 's/\//\\\//g' <<< "${to_lib[enum]}")/g)
done

# Populate fullarray with L0 paths
# This is the array of entries as they are in the actual binaries
fullarray=( $(otool -L $attractname | tail -n +2 | grep '@loader_path\|@loader_path/../../../../opt\|/usr/local\|/opt/homebrew\|@rpath' | awk -F' ' '{print $1}') )

echo
echo $( basename "$attractname" )
echo "  pre"
for val in ${fullarray[@]}; do
   echo "   $val"
done

# Build fullarray and updatearray with filtered paths
# This is the array with the correct names of the libraries as they appear in the paths of the build
for commandline in ${commands[@]}; do
	fullarray=($(sed "$commandline" <<< "${fullarray[@]}"))
done

echo "  post"
for val in ${fullarray[@]}; do
   echo "   $val"
done

# Updatearray is the list of libraries that need to be changed, it is used to copy and gather the correct libraries
# therefore it must use the fullarray data which carries the correct paths
updatearray=(${fullarray[@]})

# Iterative scan of linked libraries to build library array
iter=0
while [ ${#updatearray[@]} != 1 ] #repeat until there are no more sublibraries
do
   iter=$(($iter + 1))
	echo 
   echo check iteration $iter
	# Sublevelarray is the list of all libraries in this sublevel
	sublevelarray=("")
	# Updatearray contains the libraries from fullarray, that is the actual correct library paths,
	# they are scanned one by one to gather sublibraries for each. Each library is scanned to build the subarray
   for strlib in ${updatearray[@]}; do
		subarray=( $(otool -L $strlib | tail -n +2 | grep '@loader_path\|@loader_path/../../../../opt\|/usr/local\|/opt/homebrew\|@rpath' | awk -F' ' '{print $1}') )
		echo $( basename "$strlib" ) 
		echo "  pre"
		for val in ${subarray[@]}; do
			echo "   $val"
		done
		for commandline in ${commands[@]}; do
			subarray=($(sed "$commandline" <<< "${subarray[@]}"))
		done
		echo "  post"
		for val in ${subarray[@]}; do
			echo "   $val"
		done
		# as before, sublevelarray is built by post entries, with correct path
      sublevelarray+=("${subarray[@]}")
   done

	# Updatearray is cleaned so that only new entries can be added for future iterations
	# Build an array of unique library entries to pass to the next iteration
   updatearray=("")
	echo
   for val in ${sublevelarray[@]}; do
      new=1
      for i in ${fullarray[@]}; do
         if [[ $i == $val ]]
         then
            new=0
         fi
      done
		# If the library is not in fullarray, then it can be added to updatearray to be scanned in next level, and to fullarray
      if [[ $new == "1" ]]
      then
         echo L$iter $val
         updatearray+=($val)
         fullarray+=($val) #add the current unique non repeated libraries to the global array
      fi
   done
done

# Copy linked libraries to bundle folder, using fullarray that has the whole list of paths
for str in ${fullarray[@]}; do
   echo copying $str
   cp -n $str "$bundlelibs"/
done

# Change paths for all copied libraries
libsarray=( $(ls "$bundlecontent"/libs) )
for str in ${libsarray[@]}; do
   echo fixing $str
   subarray=( $(otool -L "$bundlelibs"/$str | tail -n +2 | grep '@loader_path\|@loader_path/../../../../opt\|/usr/local\|/opt/homebrew\|@rpath' | awk -F' ' '{print $1}') )
   subarray_fix=( $(otool -L "$bundlelibs"/$str | tail -n +2 | grep '@loader_path\|@loader_path/../../../../opt\|/usr/local\|/opt/homebrew\|@rpath' | awk -F' ' '{print $1}') )

	#Apply correction filters to all libraries
	for commandline in ${commands[@]}; do
		subarray_fix=($(sed "$commandline" <<< "${subarray_fix[@]}"))
	done

	for enum in ${!subarray[@]}; do
      str3=$( basename "${subarray_fix[enum]}" )
      str2="${subarray[enum]}"
      install_name_tool -change $str2 @loader_path/../libs/$str3 "$bundlelibs"/$str
   done
   install_name_tool -id @loader_path/../libs/$str "$bundlelibs"/$str
	#codesign --force -s - "$bundlelibs"/$str
done

echo STEP 3 - POPULATE BUNDLE FOLDER

# Copy assets to bundle folder
# cp -r $basedir/config "$bundlecontent"/
cp -a $basedir/config/ "$bundlecontent"/share/attract
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

echo STEP 4 - FIX ATTRACTPLUS EXECUTABLE

# Update rpath for attractplus
install_name_tool -add_rpath "@executable_path/../libs/" "$bundlecontent"/MacOS/attractplus

# List libraries linked in attractplus
attractlibs=( $(otool -L $attractname | tail -n +2 | grep '@loader_path\|@loader_path/../../../../opt\|/usr/local\|/opt/homebrew\|@rpath' | awk -F' ' '{print $1}') )

# Apply new links to libraries
for str in ${attractlibs[@]}; do
   str2=$( basename "$str" )
   install_name_tool -change $str @loader_path/../libs/$str2 "$bundlecontent"/MacOS/attractplus
done
#codesign --force -s - "$bundlecontent"/MacOS/attractplus
echo STEP 5 - RENAME ARTIFACT TO v${SHORTVERSION}

newappname="$buildpath/Attract-Mode Plus v${SHORTVERSION}.app"
mv "$bundlehome" "$newappname"

signapp=${3:-"no"}

if [[ $signapp == "yes" ]]
then
	echo STEP 6 - AD HOC SIGNING
	libsarray=( $(ls "$newappname/Contents/libs") )
	for str in ${libsarray[@]}; do
		codesign --force -s - "$newappname/Contents/libs/$str"
	done
	codesign --force -s - "$newappname/Contents/MacOS/attractplus"
	codesign --force -s - "$newappname"
fi

echo ALL DONE
