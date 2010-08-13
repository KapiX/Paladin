#!/bin/sh

# ----------------------------------------------------------------------------
# SETUP
# ----------------------------------------------------------------------------
if [ "$1" == "clean" ]
then
	MAKECLEAN=1
else
	MAKECLEAN=0
fi

PLATFORM=`uname -o`

# the R5 version of uname lacks a -o switch
if [ "$?" -ne 0 ]
then
	PLATFORM="beos"
fi
echo "Platform: $PLATFORM"

# This function creates a temporary project from which we make
# sure that the target is being built without debugging enabled
BuildNoDebug ()
{
	if [ -z "$1" ]
	then
		echo "Function BuildNoDebug called without parameter\n"
		exit -1
	fi

	PROJEXT=".pld"
	TEMPEXT=".nodebug.pld"
	PROJNAME=$1$PROJEXT
	TMPPROJ=$1$TEMPEXT
	
	echo "Building temporary project $TMPPROJ"
	OBJDIRNAME='(Objects.'$1'.nodebug)'
	
	# do build here
	sed 's/CCDEBUG=yes//' "$PROJNAME" | sed 's/CCOPLEVEL=[0-9]/CCOPLEVEL=3/' > "$TMPPROJ"
	
	Paladin -b "$TMPPROJ"
	SUCCESS=$?
	
	if [ "$MAKECLEAN" == 1 ]
	then
		rm "$TMPPROJ"
		if [ -d "$OBJDIRNAME" ]
		then
			rm -r "$OBJDIRNAME"
		fi
	fi
	
	if [ "$SUCCESS" -ne 0 ]
	then
		echo "Failed to build $TMPPROJ. Error code $SUCCESS" 
		exit 2;
	else
		echo "$PROJNAME has been built"
	fi
}

# ----------------------------------------------------------------------------
# BUILD
# ----------------------------------------------------------------------------

# Attempt to build Paladin
cd Paladin
rm -f Paladin Paladin.new
echo "Building Paladin"
if [ "$PLATFORM" == "Haiku" ]
then
	if [ -e /boot/system/lib/libsupc++.so ]
	then
		buildhaikugcc4.sh
	else
		buildhaikugcc2.sh
	fi
else
	build.sh
fi
if [ ! -e Paladin ]
then
	echo "Paladin build failed."
	exit 1
fi

# Make sure there's a link to make calling it easier
if [ ! -e /boot/home/config/bin/Paladin ]
then
	ln -s --target-directory=/boot/home/config/bin/ `pwd`"/Paladin"
fi

cd ../

# Now attempt ccache
echo "\nBuilding ccache"
cd ccache
BuildNoDebug ccache
cd ..

# fastdep doesn't use a project file to build itself
echo "\nBuilding fastdep"
cd fastdep-0.16
if [ "$MAKECLEAN" -eq 1 ]
then
	configure
	make clean
fi
make
cd ..

# PalEdit
cd PalEdit
if [ "$MAKECLEAN" == 1 ]
then
	jam clean
fi
jam -q
cd ..

cd PSfx
BuildNoDebug PSfx

# Make sure there's a link to make calling it easier
if [ ! -e /boot/home/config/bin/PSfx ]
then
	ln -s --target-directory=/boot/home/config/bin/ `pwd`"/PSfx"
fi
cd ..

cd SymbolFinder
BuildNoDebug SymbolFinder

# ----------------------------------------------------------------------------
# PACKAGE
# ----------------------------------------------------------------------------
