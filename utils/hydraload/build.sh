#!/bin/sh
test -d build || mkdir build
test -d build || mkdir build/chrome
cd chrome
zip hydraload.jar `find . | grep -v .svn` -r -0
mv hydraload.jar ../build/chrome
cd ../
cp install.* build
cd build
zip hydraload.xpi `find . | grep -v .svn` -r -9
mv hydraload.xpi ../
cd ../
