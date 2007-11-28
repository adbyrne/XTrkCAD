@echo off

if .%1 == . goto parametererror

mkdir .\xtrkcad-%1
mkdir .\xtrkcad-%1\demos
mkdir .\xtrkcad-%1\examples
mkdir .\xtrkcad-%1\params
copy .\bin\Release\xtrkcad.exe xtrkcad-%1
copy .\lib\logo.bmp xtrkcad-%1
tools\addcrlf -d .\COPYING xtrkcad-%1\COPYING

echo Copying the parameter files
setlocal
cd .\lib\params
for %%f in (*.xtp) do call ..\..\tools\addcrlf -d %%f ..\..\xtrkcad-%1\params\%%f
endlocal

echo Copying the demo files
setlocal
cd .\lib\demos
for %%f in (*.xtr) do call ..\..\tools\addcrlf -d %%f ..\..\xtrkcad-%1\demos\%%f
endlocal

echo Copying the example files
setlocal
cd .\lib\examples
for %%f in (*.xtc) do call ..\..\tools\addcrlf -d %%f ..\..\xtrkcad-%1\examples\%%f
endlocal

echo Copying misc files
setlocal
cd .\lib
..\tools\addcrlf -d aareadme.txt ..\xtrkcad-%1\ReadMe.txt
endlocal

echo Copying support files
setlocal
cd .\lib
..\tools\addcrlf -d xtrkcad.xtq ..\xtrkcad-%1\xtrkcad.xtq
..\tools\addcrlf -d xtrkcad.enh ..\xtrkcad-%1\xtrkcad.enh
..\tools\addcrlf -d xtrkcad.upd ..\xtrkcad-%1\xtrkcad.upd
..\tools\addcrlf -d xtrkcad.ini ..\xtrkcad-%1\xtrkcad.ini
endlocal

setlocal
cd .\help
..\tools\addcrlf -d xtrkcad.tip ..\xtrkcad-%1\xtrkcad.tip
endlocal

setlocal
cd .\doc
copy .\xtrkcad.chm ..\xtrkcad-%1\xtrkcad.chm
endlocal 

setlocal
cd .\bin
..\tools\addcrlf -d ChangeLog ..\xtrkcad-%1\ChangeLog
endlocal

setlocal
cd xtrkcad-%1
makensis /Drel=%1 /NOCD ../tools/xtrkcad.nsi
copy xtcinst.exe xtc%1.exe
del xtcinst.exe

del /q examples\*.*
rmdir examples
del /q demos\*.*
rmdir demos
del /q params\*.*
rmdir params
del COPYING
del xtrkcad.exe
del xtrkcad.xtq 
del xtrkcad.enh 
del xtrkcad.ini 
del xtrkcad.chm
del xtrkcad.tip
del logo.bmp
del ReadMe.txt
del ChangeLog
endlocal

goto end

:parametererror
echo.
echo ERROR: Execute with %0 rel
echo.
echo Example: %0 401
echo where <401> is the release of XTrkCad to be packaged. 
echo During execution of this script, the directory xtrkcad-<rel> is 
echo created and all necessary files are collected into that directory.
echo After that NSIS is run to build the install package. After successful
echo build, the files are deleted again.
echo.
echo Execute from directory xtrkcad\app
echo.

:end