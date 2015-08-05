#!/bin/bash

echo "make mkapkg..."

cd source/mkapkg/src
qmake -r
make clean
make
cp -a mkapkg ../out

echo "make mkfw..."
cd ../../mkfw/src
qmake -r
make clean
make
cp -a mkfw ../out

cd ../../..

