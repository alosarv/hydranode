#!/bin/sh
# This utility script is an all-in-one hydranode build script for Mac OS X.
# Get the boost libraries and etxract.

if [ ! -d "boost_1_32_0" ]; then
if [ ! -f "boost_1_32_0.tar.gz"]; then
	echo "Downloading boost libraries, get some coffee come back in 5-10 mins..."
	curl http://internap.dl.sourceforge.net/sourceforge/boost/boost_1_32_0.tar.gz --progress-bar --output boost_1_32_0.tar.gz
fi
fi

#if [ ! -f "boost_1_32_0.tar.gz"]; then
#   	echo"Boost archive not found!"
#  	 exit 1
#fi

if [ ! -d "boost_1_32_0" ]; then
	gunzip	boost_1_32_0.tar.gz
	tar -xf boost_1_32_0.tar
fi

# Checkout the hydranode source.
echo "Checking out hydranode source..."
rm -rf hydranode
svn co svn://hydranode.com/hydranode/hydranode

# If we are 10.4.x we need to make sure we are using gcc 3.3 for now.
mac_version=`sw_vers | grep ProductVersion | cut -f 2 | cut -c 4`
if [ $mac_version == 4 ]; then
        sudo gcc_select 3.3
fi

if [ -d "hydranode" ]; then
	cd hydranode
	./autogen.sh
	./configure
else
# Exit failure
	echo "Failed!"
	exit 1
fi

# Start our build.
echo "Building..."
make

# Strip the binaries and copy them into a release folder.
echo "Stripping binaries..."
strip -x src/hydranode
strip -x modules/ed2k/libed2k.so
strip -x modules/hnsh/libhnsh.so

if [ ! -d "release" ]; then
	mkdir release
fi

echo "Copying binaries to release dir..."
cp src/hydranode release/hydranode
cp modules/ed2k/libed2k.so release/libed2k.so
cp modules/hnsh/libhnsh.so release/libhnsh.so

# Set gcc back to 4.0, if needed.
if [ $mac_version == 4 ]; then
	sudo gcc_select 4.0
fi

# All done, exit ok
echo "Done!"
exit 0
