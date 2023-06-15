#!/bin/bash

if [[ -n "$1" ]]
then
    branch="-b $1"
else
    branch=""
fi

export PKG_CONFIG_PATH=/usr/local/pkgconfig

echo Creating Folders
rm -Rf $HOME/buildattract
mkdir $HOME/buildattract

echo Cloning attractplus
git clone $branch http://github.com/zpaolo11x/attractplus-macOS $HOME/buildattract/attractplus

cd $HOME/buildattract/attractplus

LASTTAG=$(git describe --tag --abbrev=0)
VERSION=$(git describe --tag | sed 's/-[^-]\{8\}$//')
BUNDLEVERSION=${VERSION//[v-]/.}; BUNDLEVERSION=${BUNDLEVERSION#"."}
SHORTVERSION=${LASTTAG//v/}

NPROC=$(getconf _NPROCESSORS_ONLN)

echo Building attractplus
make clean
eval make -j${NPROC} STATIC=0 VERBOSE=1 prefix=..


bash util/osx/appbuilder.sh $HOME/buildattract $HOME/buildattract/attractplus
