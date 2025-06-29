#!/bin/bash

rm -rf build/
mkdir build/

pushd build/

gcc -o x11_handmade ../x11_handmade.c -g -lX11

popd


