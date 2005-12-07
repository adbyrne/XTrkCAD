# Microsoft Developer Studio Project File - Name="xtrkcad" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=xtrkcad - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "xtrkcad.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "xtrkcad.mak" CFG="xtrkcad - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "xtrkcad - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "xtrkcad - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "xtrkcad - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "wlib\include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "WINDOWS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=wlib\mswlib\Release\checksum -s Release\xtrkcad.exe
# End Special Build Tool

!ELSEIF  "$(CFG)" == "xtrkcad - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "wlib\include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "WINDOWS" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# SUBTRACT LINK32 /pdb:none
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=wlib\mswlib\Debug\checksum -s Debug\xtrkcad.exe
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "xtrkcad - Win32 Release"
# Name "xtrkcad - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\bllnhlp.c
# End Source File
# Begin Source File

SOURCE=..\bin\ccurve.c
# End Source File
# Begin Source File

SOURCE=..\bin\cdraw.c
# End Source File
# Begin Source File

SOURCE=..\bin\celev.c
# End Source File
# Begin Source File

SOURCE=..\bin\cgroup.c
# End Source File
# Begin Source File

SOURCE=..\bin\chndldto.c
# End Source File
# Begin Source File

SOURCE=..\bin\chotbar.c
# End Source File
# Begin Source File

SOURCE=..\bin\cjoin.c
# End Source File
# Begin Source File

SOURCE=..\bin\cmisc.c
# End Source File
# Begin Source File

SOURCE=..\bin\cmisc2.c
# End Source File
# Begin Source File

SOURCE=..\bin\cmodify.c
# End Source File
# Begin Source File

SOURCE=..\bin\cnote.c
# End Source File
# Begin Source File

SOURCE=..\bin\compound.c
# End Source File
# Begin Source File

SOURCE=..\bin\cparalle.c
# End Source File
# Begin Source File

SOURCE=..\bin\cprint.c
# End Source File
# Begin Source File

SOURCE=..\bin\cprofile.c
# End Source File
# Begin Source File

SOURCE=..\bin\cpull.c
# End Source File
# Begin Source File

SOURCE=..\bin\cruler.c
# End Source File
# Begin Source File

SOURCE=..\bin\cselect.c
# End Source File
# Begin Source File

SOURCE=..\bin\csnap.c
# End Source File
# Begin Source File

SOURCE=..\bin\csplit.c
# End Source File
# Begin Source File

SOURCE=..\bin\cstraigh.c
# End Source File
# Begin Source File

SOURCE=..\bin\cstruct.c
# End Source File
# Begin Source File

SOURCE=..\bin\ctext.c
# End Source File
# Begin Source File

SOURCE=..\bin\ctodesgn.c
# End Source File
# Begin Source File

SOURCE=..\bin\ctrain.c
# End Source File
# Begin Source File

SOURCE=..\bin\cturnout.c
# End Source File
# Begin Source File

SOURCE=..\bin\cturntbl.c
# End Source File
# Begin Source File

SOURCE=..\bin\cundo.c
# End Source File
# Begin Source File

SOURCE=..\bin\custom.c
# End Source File
# Begin Source File

SOURCE=..\bin\dbench.c
# End Source File
# Begin Source File

SOURCE=..\bin\dbitmap.c
# End Source File
# Begin Source File

SOURCE=..\bin\dcar.c
# End Source File
# Begin Source File

SOURCE=..\bin\dcmpnd.c
# End Source File
# Begin Source File

SOURCE=..\bin\dcustmgm.c
# End Source File
# Begin Source File

SOURCE=..\bin\dease.c
# End Source File
# Begin Source File

SOURCE=..\bin\denum.c
# End Source File
# Begin Source File

SOURCE=..\bin\dlayer.c
# End Source File
# Begin Source File

SOURCE=..\bin\doption.c
# End Source File
# Begin Source File

SOURCE=..\bin\dpricels.c
# End Source File
# Begin Source File

SOURCE=..\bin\dprmfile.c
# End Source File
# Begin Source File

SOURCE=..\bin\draw.c
# End Source File
# Begin Source File

SOURCE=..\bin\drawgeom.c
# End Source File
# Begin Source File

SOURCE=..\bin\elev.c
# End Source File
# Begin Source File

SOURCE=..\bin\fileio.c
# End Source File
# Begin Source File

SOURCE=..\bin\lprintf.c
# End Source File
# Begin Source File

SOURCE=..\bin\macro.c
# End Source File
# Begin Source File

SOURCE=..\bin\misc.c
# End Source File
# Begin Source File

SOURCE=..\bin\misc2.c
# End Source File
# Begin Source File

SOURCE=..\bin\param.c
# End Source File
# Begin Source File

SOURCE=..\bin\shrtpath.c
# End Source File
# Begin Source File

SOURCE=..\bin\tcurve.c
# End Source File
# Begin Source File

SOURCE=..\bin\tease.c
# End Source File
# Begin Source File

SOURCE=..\bin\track.c
# End Source File
# Begin Source File

SOURCE=..\bin\trkseg.c
# End Source File
# Begin Source File

SOURCE=..\bin\tstraigh.c
# End Source File
# Begin Source File

SOURCE=..\bin\utility.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\bin\acclkeys.h
# End Source File
# Begin Source File

SOURCE=..\bin\ccurve.h
# End Source File
# Begin Source File

SOURCE=..\bin\cjoin.h
# End Source File
# Begin Source File

SOURCE=..\bin\common.h
# End Source File
# Begin Source File

SOURCE=..\bin\compound.h
# End Source File
# Begin Source File

SOURCE=..\bin\cselect.h
# End Source File
# Begin Source File

SOURCE=..\bin\cstraigh.h
# End Source File
# Begin Source File

SOURCE=..\bin\ctrain.h
# End Source File
# Begin Source File

SOURCE=..\bin\cundo.h
# End Source File
# Begin Source File

SOURCE=..\bin\custom.h
# End Source File
# Begin Source File

SOURCE=..\bin\draw.h
# End Source File
# Begin Source File

SOURCE=..\bin\drawgeom.h
# End Source File
# Begin Source File

SOURCE=..\bin\fileio.h
# End Source File
# Begin Source File

SOURCE=..\bin\misc.h
# End Source File
# Begin Source File

SOURCE=..\bin\misc2.h
# End Source File
# Begin Source File

SOURCE=..\bin\param.h
# End Source File
# Begin Source File

SOURCE=..\bin\regmac.h
# End Source File
# Begin Source File

SOURCE=..\bin\shrtpath.h
# End Source File
# Begin Source File

SOURCE=..\bin\track.h
# End Source File
# Begin Source File

SOURCE=..\bin\trackx.h
# End Source File
# Begin Source File

SOURCE=..\bin\utility.h
# End Source File
# Begin Source File

SOURCE=..\bin\version.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\xtrkcad.rc

!IF  "$(CFG)" == "xtrkcad - Win32 Release"

# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409 /i "wlib\include"

!ELSEIF  "$(CFG)" == "xtrkcad - Win32 Debug"

# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409 /i "wlib\include"

!ENDIF 

# End Source File
# End Group
# End Target
# End Project
