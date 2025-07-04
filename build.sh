#!/bin/bash

echo "Building..."

rm -rf build/
mkdir build/

pushd build/ 1> /dev/null 2> /dev/null

echo "Compiling..."
gcc -o x11_handmade ../x11_handmade.c -g -lX11 -lXext -ffast-math -msse -msse2 -O2 -Wall -Wextra -pedantic -fsanitize=address

popd 1> /dev/null 2> /dev/null

echo "Done build"

