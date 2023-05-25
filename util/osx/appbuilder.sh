#!/bin/bash

echo STEP 1 - PREPARE BUNDLE FOLDERS

# Define folder path variables
bundlehome="artifacts/Attract Mode Plus.app"
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

basedir="am"
attractname="$basedir/attractplus"

echo STEP 2 - COLLECT AND FIX LINKED LIBRARIES

# Define library fixing pairs

fr_lib=("@rpath/../Frameworks/freetype.framework/Versions/A/freetype")
to_lib=("/usr/local/opt/freetype/lib/libfreetype.6.dylib")

fr_lib+=("@rpath/libsfml")
to_lib+=("am/obj/sfml/install/lib/libsfml")

fr_lib+=("@rpath/libsharpyuv")
to_lib+=("/usr/local/opt/webp/lib/libsharpyuv")

fr_lib+=("@rpath/libwebp")
to_lib+=("/usr/local/opt/webp/lib/libwebp")

# Build commands for processing
commands=("")
for enum in ${!fr_lib[@]}; do
	commands+=(s/$(sed 's/\//\\\//g' <<< "${fr_lib[enum]}")/$(sed 's/\//\\\//g' <<< "${to_lib[enum]}")/g)
done

# Populate startarray with L0 paths
fullarray=( $(otool -L $attractname | tail -n +2 | grep '/usr/local\|@rpath' | awk -F' ' '{print $1}') )

echo fullarray
for val in ${fullarray[@]}; do
   echo $val
done

# Build fullarray and updatearray with filtered paths
for commandline in ${commands[@]}; do
	fullarray=($(sed "$commandline" <<< "${fullarray[@]}"))
done

echo fullarray post
for val in ${fullarray[@]}; do
   echo $val
done

updatearray=(${fullarray[@]})


for val in ${updatearray[@]}; do
   echo L0 $val
done

# Iterative scan of linked libraries to build library array
iter=0
while [ ${#updatearray[@]} != 1 ] #repeat until there are no more sublibraries
do
   iter=$(($iter + 1))
   echo check iteration $iter
	# Reset array of all libraries in this sublevel
	sublevelarray=("")
	# For each library in the updatearray build a subarray
   for strlib in ${updatearray[@]}; do
		subarray=( $(otool -L $strlib | tail -n +2 | grep '/usr/local\|@rpath' | awk -F' ' '{print $1}') )
		echo subarray pre
		for val in ${subarray[@]}; do
			echo $val
		done
		for commandline in ${commands[@]}; do
			subarray=($(sed "$commandline" <<< "${subarray[@]}"))
		done
		echo subarray post
		for val in ${subarray[@]}; do
			echo $val
		done
      sublevelarray+=("${subarray[@]}")
   done
	

	# Build an array of unique library entries to pass to the next iteration
   updatearray=("")
   for val in ${sublevelarray[@]}; do
      new=1
      for i in ${fullarray[@]}; do
         if [[ $i == $val ]]
         then
            new=0
         fi
      done
      if [[ $new == "1" ]]
      then
         echo L$iter $val
         updatearray+=($val)
         fullarray+=($val) #add the current unique non repeated libraries to the global array
      fi   
   done
done

# Copy linked libraries to bundle folder
for str in ${fullarray[@]}; do
   echo copying $str
   cp -n $str "$bundlelibs"/
done

# Change paths for all copied libraries
libsarray=( $(ls "$bundlecontent"/libs) )
for str in ${libsarray[@]}; do
   echo fixing $str
   subarray=( $(otool -L "$bundlelibs"/$str | tail -n +2 | grep '/usr/local\|@rpath' | awk -F' ' '{print $1}') )
   for str2 in ${subarray[@]}; do
      str3=$( basename "$str2" )
      install_name_tool -change $str2 @loader_path/../libs/$str3 "$bundlelibs"/$str
   done
   install_name_tool -id @loader_path/../libs/$str "$bundlelibs"/$str
done

echo STEP 3 - POPULATE BUNDLE FOLDER

# Copy assets to bundle folder
# cp -r $basedir/config "$bundlecontent"/
cp -a $basedir/config/ "$bundlecontent"/share/attract
cp -a $basedir/attractplus "$bundlecontent"/MacOS/
cp -a $basedir/util/osx/attractplus.icns "$bundlecontent"/Resources/
cp -a $basedir/util/osx/launch.sh "$bundlecontent"/MacOS/

# Prepare plist file
LASTTAG=$(git -C am/ describe --tag --abbrev=0)
VERSION=$(git -C am/ describe --tag | sed 's/-[^-]\{8\}$//')
BUNDLEVERSION=${VERSION//[v-]/.}; BUNDLEVERSION=${BUNDLEVERSION#"."}
SHORTVERSION=${LASTTAG//v/}

sed -e 's/%%SHORTVERSION%%/'${SHORTVERSION}'/' -e 's/%%BUNDLEVERSION%%/'${BUNDLEVERSION}'/' $basedir/util/osx/Info.plist > "$bundlecontent"/Info.plist

echo STEP 4 - FIX ATTRACTPLUS EXECUTABLE

# Update rpath for attractplus
install_name_tool -add_rpath "@executable_path/../libs/" "$bundlecontent"/MacOS/attractplus

# List libraries linked in attractplus
attractlibs=( $(otool -L $attractname | tail -n +2 | grep '/usr/local\|@rpath' | awk -F' ' '{print $1}') )  

# Apply new links to libraries
for str in ${attractlibs[@]}; do
   str2=$( basename "$str" )
   install_name_tool -change $str @loader_path/../libs/$str2 "$bundlecontent"/MacOS/attractplus
done
