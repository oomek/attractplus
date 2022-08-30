#!/bin/bash

echo STEP 1 - PREPARE BUNDLE FOLDERS

bundlehome="artifacts/Attract Mode Plus.app"
bundlecontent="$bundlehome"/Contents
bundlelibs="$bundlecontent"/libs

rm -Rf "$bundlehome"
mkdir "$bundlehome"
mkdir "$bundlecontent"
mkdir "$bundlelibs"
mkdir "$bundlecontent"/MacOS
mkdir "$bundlecontent"/Resources

basedir="am"
attractname="$basedir/attractplus"

echo STEP 2 - COLLECT AND FIX LINKED LIBRARIES

# Array of libraries linked by attractplus
fullarray=( $(otool -L $attractname | tail -n +2 | grep '/usr/local\|@rpath' | awk -F' ' '{print $1}' | sed 's/@rpath/sfml\/install\/lib/') )  
updatearray=( $(otool -L $attractname | tail -n +2 | grep '/usr/local\|@rpath' | awk -F' ' '{print $1}' | sed 's/@rpath/sfml\/install\/lib/') )  

for val in ${updatearray[@]}; do
   echo L0 $val
done

# Iterative scan of linked libraries to build library array
iter=0
while [ ${#updatearray[@]} != 1 ]
do
   iter=$(($iter + 1))
   echo check iteration $iter
   level1=("")
   for strlib in ${updatearray[@]}; do
      level1_T=( $(otool -L $strlib | tail -n +2 | grep '/usr/local\|@rpath' | awk -F' ' '{print $1}' | sed 's/@rpath/sfml\/install\/lib/') )  
      level1=("${level1[@]}" "${level1_T[@]}")
   done
   updatearray=("")
   for val in ${level1[@]}; do
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
         fullarray+=($val)
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
cp -r $basedir/config "$bundlecontent"/
cp -a $basedir/attractplus "$bundlecontent"/MacOS/
cp -a $basedir/util/osx/attractplus.icns "$bundlecontent"/Resources/
cp -a $basedir/util/osx/launch.sh "$bundlecontent"/MacOS/

# Prepare plist file
LASTTAG=$(git -C am/ describe --tag --abbrev=0)
VERSION=$(git -C am/ describe --tag | sed 's/-[^-]\{8\}$//')
BUNDLEVERSION=${VERSION//[v-]/.}; BUNDLEVERSION=${BUNDLEVERSION#"."}
SHORTVERSION=${LASTTAG//v/}

cat $basedir/util/osx/Info.plist | sed -e 's/%%SHORTVERSION%%/'${SHORTVERSION}'/' -e 's/%%BUNDLEVERSION%%/'${BUNDLEVERSION}'/' > "$bundlecontent"/Info.plist

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
