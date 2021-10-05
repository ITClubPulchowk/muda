#!/bin/bash

SOURCEFILES=../main.c
OUTPUTFILE=main.out
GCCFLAGS="-g -Od"
CLANGFLAGS="-gcodeview -Od"

if [ ! -d "./bin" ]; then
    mkdir bin
fi

if [ "$1" == "optimize" ]; then
    GCCFLAGS="-O2"
    CLANGFLAGS="-O2 -gcodeview"
fi

echo ------------------------------
echo Building With GCC
echo ------------------------------
if command -v gcc &> /dev/null
then
    pushd bin
    gcc -Wno-switch -Wno-pointer-sign -Wno-enum-conversion $SOURCEFILES $COMPILERFLAGS -o $OUTPUTFILE
    popd
    exit
else
    echo GCC Not Found
    echo ------------------------------
fi

echo ------------------------------
echo Building With Clang
echo ------------------------------
if command -v clang &> /dev/null
then
    pushd bin
    clang -Wno-switch -Wno-pointer-sign -Wno-enum-conversion -D_CRT_SECURE_NO_WARNINGS $SOURCEFILES $COMPILERFLAGS -o $OUTPUTFILE
    popd
    exit
else
    echo Clang Not Found
    echo ------------------------------
fi
