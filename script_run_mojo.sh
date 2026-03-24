#!/bin/bash

rm -rf build
cmake -S src -B build
cmake --build build
./build/mojoraw_example
