#!/bin/sh

test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.

olddir=`pwd`

cd $srcdir

AUTORECONF=$(which autoreconf 2>/dev/null)
GTKDOCIZE=$(which gtkdocize 2>/dev/null)

set -x

if test -z $GTKDOCIZE; then
	echo "*** No gtkdocize found, please install gtk-doc ***"
	exit 1
else
        ${GTKDOCIZE} || exit $?
fi

if test -z $AUTORECONF; then
        echo "*** No autoreconf found, please install autoconf ***"
        exit 1
else
        ${AUTORECONF} --force --install --verbose || exit $?
fi

if test -z "$NOCONFIGURE"; then
        if test -z "$*"; then
                echo "*** I am going to run ./configure with no arguments - if you wish"
                echo "*** to pass any to it, please specify them on the $0 command line."
        fi
fi

cd "$olddir"
test -n "$NOCONFIGURE" || "$srcdir/configure" "$@"

{ set +x; } 2>/dev/null
