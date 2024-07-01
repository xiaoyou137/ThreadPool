#!/bin/bash

DIR="build"
DIR2="bin"

if [ ! -d "$DIR" ]; then
    mkdir -p "$DIR"
fi 

if [ ! -d "$DIR2" ]; then
    mkdir -p "$DIR2"
fi

cd ./build && rm -f ./*
cmake ..
make