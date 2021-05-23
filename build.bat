@echo off

IF NOT EXIST build mkdir build
pushd build
cl -FC -Zi  ../src/main.cpp user32.lib Gdi32.lib
popd
