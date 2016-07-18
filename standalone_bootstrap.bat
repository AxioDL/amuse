@echo off
git clone https://github.com/AxioDL/boo.git
pushd boo
git submodule update --recursive --init
popd

git clone https://github.com/libAthena/athena.git
pushd athena
git submodule update --recursive --init
popd
