#!/bin/sh 

# a script to get the cvs version number of a particular file 
# used by doxygen to identify the source of the doxygen-produced  documentation
# $1 contains the full pathname of the file of interest. cvs expects a pathname
# relative to the directory from which doxygen is invoked.
# Note that the cvs status command must consult the server (sourceforge.net)
# and is therefore quite slow.

WD=`pwd`
FN=`echo "$1" | sed "s|${WD}/||" `
cvs status ${FN} | sed -n 's/^[ \]*Working revision:[ \t]*\([0-9][0-9\.]*\).*/\1/p'