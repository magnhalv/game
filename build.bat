@echo off

IF NOT EXIST build mkdir build
pushd build
cl -DGAME_INTERNAL=1 -DGAME_SLOW=1 -FC -Zi  ../src/main.cpp user32.lib Gdi32.lib
popd
