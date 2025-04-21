@echo off

set commonCompilerFlags= -MTd -nologo -fp:fast -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4100 -wd4201 -wd4189 -wd4505 -wd4101 -DJUMPER_INTERNAL=1 -DJUMPER_SLOW=1 -DJUMPER_WIN32=1 -FC -Zi

set commonLinkerFlags= -incremental:no user32.lib gdi32.lib winmm.lib

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

echo WAITING FOR PDB > lock.tmp
cl %commonCompilerFlags% ..\code\jumper.cpp -Fmjumper.map /LD /link /EXPORT:GameUpdateAndRender /EXPORT:GameGetSoundData

del lock.tmp
cl %commonCompilerFlags% ..\code\win32_jumper.cpp -Fmwin32_jumper.map /link %commonLinkerFlags%
popd

