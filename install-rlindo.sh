#!/bin/sh
echo
echo --------------------------------------------------
echo rLindo Installation script for Linux and OSX
echo         Lindo Systems, Inc.
echo --------------------------------------------------

if [ "${LINDOAPI_HOME}" = "" ]; then
	echo Error: LINDOAPI_HOME variable is not set.
	echo
	exit
fi

if [ ! -f $LINDOAPI_HOME/include/lsversion.sh ]; then
	echo Error: \'$LINDOAPI_HOME/include/lsversion.sh\' file was not found
	echo
	exit
fi
. $LINDOAPI_HOME/include/lsversion.sh
echo Looking for source package rLindo_$LS_MAJOR.$LS_MINOR.tar.gz
if [ -f rLindo_$LS_MAJOR.$LS_MINOR.tar.gz ]; then
	echo Ok.. Installing...
else
	echo Error: source package rLindo_$LS_MAJOR.$LS_MINOR.tar.gz not found
	exit
fi

command -v R --help>/dev/null 2>&1 || { 
	echo >&2 "Error: R is not installed or is not on path."; 
	exit 1; 
}

R CMD INSTALL rLindo_$LS_MAJOR.$LS_MINOR.tar.gz --no-test-load


if [ -n "${R_HOME+x}" -o "$R_HOME" = "" ]; then
	R_HOME=`R RHOME`
fi	
#echo R_HOME is ${R_HOME}
LIBRARY=library
if [ -d "${R_HOME}/site-library" ]; then
	LIBRARY=site-library
fi	
OSYS=`uname -s`
MACHINE=`uname -m`
if [ ${OSYS} = "Darwin" ]; then	
	if [ -f "${R_HOME}/$LIBRARY/rLindo/libs/rLindo.so" ]; then
		echo changing the install name of rLindo package
		if [ ${MACHINE} = "x86_64" ]; then
			install_name_tool -change @loader_path/liblindo64.dylib \
							${LINDOAPI_HOME}/bin/osx64x86/liblindo64.$LS_MAJOR.$LS_MINOR.dylib \
							${R_HOME}/$LIBRARY/rLindo/libs/rLindo.so
		else
			install_name_tool -change @loader_path/liblindo.dylib \
							${LINDOAPI_HOME}/bin/osx32x86/liblindo.$LS_MAJOR.$LS_MINOR.dylib \
							${R_HOME}/$LIBRARY/rLindo/libs/rLindo.so
		fi
	else
		echo >&2 "Error: ${R_HOME}/$LIBRARY/rLindo/libs/rLindo.so is not found"
	fi
fi
echo Finished

