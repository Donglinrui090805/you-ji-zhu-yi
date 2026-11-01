@echo off
chcp 65001 >nul
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cl /nologo /EHsc /utf-8 /O2 youjizhuyi.cpp /I D:\storygame\include /link /LIBPATH:D:\storygame\x64\vc16\lib opencv_world481.lib gdiplus.lib user32.lib /out:story.exe
echo Build finished. story.exe updated.
pause
