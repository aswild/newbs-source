#!/bin/sh -xe
aclocal
autoconf
automake --add-missing

./configure "$@"
make
