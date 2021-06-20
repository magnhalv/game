@echo off

set CommonCompilerFlags=-MT -nologo -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -DGAME_INTERNAL=1 -DGAME_SLOW=1 -FC -Z7 /std:c++latest
set CommonLinkerFlags= -incremental:no -opt:ref user32.lib Gdi32.lib winmm.lib

set CurrentDateTime=%DATE:~10,4%%DATE:~4,2%%DATE:~7,2%_%time:~0,2%%time:~3,2%%time:~6,2%
set CurrentDateTime=%CurrentDateTime: =0%

IF NOT EXIST build mkdir build
pushd build
del *.pdb
cl %CommonCompilerFlags%  ../src/game.cpp -Fmwin32_game.map /LD /link /EXPORT:game_update_and_render_imp /EXPORT:game_get_sound_samples_imp /PDB:game_%CurrentDateTime%.pdb
cl %CommonCompilerFlags%  ../src/win32_game.cpp -Fmwin32_game.map user32.lib Gdi32.lib winmm.lib
popd
