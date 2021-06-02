@echo off

IF NOT EXIST build mkdir build
pushd build
cl -MT -nologo -GR- -Gm- -Oi -EHa- -WX -W4 -wd4201 -wd4100 -Fmwin32_game.map -DGAME_INTERNAL=1 -DGAME_SLOW=1 -FC -Zi  ../src/main.cpp user32.lib Gdi32.lib
popd
