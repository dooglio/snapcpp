#!/bin/bash
#
# This script uploads a source package to the lp servers, using "ppa:snapcpp/ppa".
#
# The parameter to the script is the package name which will be built. It is assumed that
# the debian subfolder is one level down, in the folder name provided. Package will
# be determined using dpkg-parsechangelog.
#

# Handle command line
#
set -e
if [ -z "$1" ]
then
    echo "usage: $0 distribution"
    exit 1
fi

if [ -n "$2" ]
then
    # The MAKEFLAGS do not make it through the chroot, but I keep this
    # as a reminder
    #
    #export MAKEFLAGS=$2

    # Extract the -j<count> option if any and pass it through the
    # DEB_BUILD_OPTIONS parameter (Note: this can also be done through
    # the .pbuilderrc file)
    #
    case "$2" in
    *-j[0-9]*)
        count=`echo "$2" | sed -e 's/.*-j\([[:digit:]]\)\+.*/\1/'`
        export DEB_BUILD_OPTIONS="parallel=$count"
        ;;
    esac
fi

if [ ! -e debian/changelog ]
then
	echo "No debian changelog found!"
	exit 1
fi

NAME=`dpkg-parsechangelog --show-field Source`
VERSION=`dpkg-parsechangelog --show-field Version`

if ! pbuilder-dist $1 build ../${NAME}_${VERSION}.dsc
then
    # once in a while we get an error, try again immediately because in
    # most cases the second attempt is more than likely to succeed just
    # fine; i.e. if we get an error such as:
    # https://bugs.launchpad.net/pbuilderjenkins/+bug/1233775
    # if the problem is with the source, it will just fail again...
    pbuilder-dist $1 build ../${NAME}_${VERSION}.dsc
fi

# vim: ts=4 sw=4 et
