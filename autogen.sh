#!/bin/sh

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

test -f "configure.ac" || {
	echo "You must run this script in the top-level gom directory"
	exit 1
}

autoreconf -v --install || exit $?

test -n "$NOCONFIGURE" ||
(
	"$srcdir/configure" "$@" &&
	echo "Now type 'make' to compile gom"
)
