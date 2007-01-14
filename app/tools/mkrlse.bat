rem @echo off
if x%1 == x goto usage
set SRCROOT=q:\xtrkcad
set SRCROOT=%CD%
set BINROOT=c:\xtrkcad
if x%2 == x goto bincheck
set SRCROOT=%2
:bincheck
if x%3 == x goto doit
set BINROOT=%3

:doit

mkdir %BINROOT%\%1\release
mkdir %BINROOT%\%1\release\demos
mkdir %BINROOT%\%1\release\examples
mkdir %BINROOT%\%1\release\params
cd %SRCROOT%\%1\lib\params
for %%f in (*.xtp) do call addcr %%f %BINROOT%\%1\release\params\%%f
cd %SRCROOT%\%1\lib\demos
for %%f in (dm*.xtr) do call addcr %%f %BINROOT%\%1\release\demos\%%f
cd %SRCROOT%\%1\lib\examples
for %%f in (*.xtc) do call addcr %%f %BINROOT%\%1\release\examples\%%f
cd %SRCROOT%\%1\lib
copy register.* %BINROOT%\%1\release
for %%f in (xtrkcad.xtq xtrkcad.bug xtrkcad.fix xtrkcad.enh aareadme.txt) do call addcr %%f %BINROOT%\%1\release\%%f
copy %SRCROOT%\%1\help\xtrkcad.hlp %BINROOT%\%1\release
copy %BINROOT%\%1\msw\xtrkcad.exe %BINROOT%\%1\release
set SRCROOT=
set BINROOT=
goto end
:usage
@echo Usage:  mkrlse.bat version [ [source root] [bin root] ]
:end
