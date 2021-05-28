#!/bin/sh
git clone https://github.com/AxioDL/boo.git
cd boo
git submodule update --recursive --init
cd ..

git clone https://github.com/libAthena/athena.git
cd athena
git submodule update --recursive --init
cd ..

