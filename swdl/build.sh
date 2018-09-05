#!/bin/bash -e
(
    set -x
    aclocal
    autoconf
    automake --add-missing
)

if [[ "$1" == "sanitize" ]]; then
    configargs=('--disable-lto' 'CC=clang' 'CXX=clang++'
                'CFLAGS=-g -O2 -fsanitize=address -fsanitize=undefined'
                'CXXFLAGS=-g -O2 -fsanitize=address -fsanitize=undefined')
else
    configargs=("$@")
fi

set -x
./configure "${configargs[@]}"
make
