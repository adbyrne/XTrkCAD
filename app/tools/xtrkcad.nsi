; xtrkcad.nsi
;
; This script is based on example1.nsi, but it remembers the directory, 
; has uninstall support and (optionally) installs start menu shortcuts.
;
; It will install xtrkcad into a directory that the user selects,

;--------------------------------
;Include Modern UI

  !include "MUI.nsh"

;--------------------------------
; The name of the installer

Name "XTrkCad"

; The file to write

OutFile "xtcinst.exe"

; The default installation directory
InstallDir "$PROGRAMFILES\XTrkCAD4"

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\XTrkCAD" "Install_Dir"

;--------------------------------
;Interface Settings

  !define MUI_ABORTWARNING

;--------------------------------

; Pages

  !insertmacro MUI_PAGE_WELCOME
;  !insertmacro MUI_PAGE_LICENSE "release\COPYING.txt"
;  !insertmacro MUI_PAGE_COMPONENTS
  !insertmacro MUI_PAGE_DIRECTORY
  !insertmacro MUI_PAGE_INSTFILES
  
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES


;--------------------------------
;Languages
 
  !insertmacro MUI_LANGUAGE "English"

;--------------------------------

; The stuff to install
Section "XTrkCAD (required)" program

  SectionIn RO
  
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  
  File "xtrkcad.exe"
  File "COPYING"
  File "ReadMe.txt"
  File "xtrkcad.xtq"
  File "xtrkcad.tip"	
  File "xtrkcad.chm"
  File "logo.bmp"	
  File "ChangeLog"	
  
  ; remove the older help file if it does exist
  IfFileExists $INSTDIR\xtrkcad.hlp 0 +2
	Delete $INSTDIR\xtrkcad.hlp
  
  ; add the parameter files	
  SetOutPath "$INSTDIR\params"
  File "params\*.*"		

  ; add the demo files	
  SetOutPath "$INSTDIR\demos"
  File "demos\*.*"		

  ; add the example files	
  SetOutPath "$INSTDIR\examples"
  File "examples\*.*"		

  ; add the ini file
;
; adding and customizing the ini file will be done later - some changes
; in XTC needed as well (use user's directories instead of Windows system dir)
;
;  SetOutPath "$DOCUMENTS"
;
;  File "/oname=xtrkcad.new" "release\xtrkcad.ini" 
;
;  IfFileExists "$DOCUMENTS\xtrkcad.ini" +2 0
;    Rename "$DOCUMENTS\xtrkcad.new" "$DOCUMENTS\xtrkcad.ini"
;
;  Delete "$DOCUMENTS\xtrkcad.new"
;
; 
;  WriteINIStr "$DOCUMENTS\xtrkcad.ini"	file paramdir "$INSTDIR\params\"
;
; Does an INI file exist?
;  If yes, the program has been run before, so leave it alone
;  If no, try to find an INI file in the former locations and copy it to the new place ( application settings)
;
  IfFileExists $APPDATA\xtrkcad\xtrkcad.ini NoOldIni 0
     
; find the INI file location
;
; the default location is the Windows directory
    StrCpy $1 $WINDIR
;
; the user can select this select by configuring an xtrkcad0.ini, so look for this next 
    IfFileExists $INSTDIR\xtrkcad0.ini 0 NoIni
	  ReadINIStr $1 $INSTDIR\xtrkcad0.ini workdir path
	
NoIni:	
;  In case an ini file exists in this location, copy it to the new location
    IfFileExists $1\xtrkcad.ini 0 NoOldIni	
	  CopyFiles $1\xtrkcad.ini $APPDATA\xtrkcad
	
	  IfFileExists $1\xtrkcad.cus 0 +2 
		CopyFiles $1\xtrkcad.cus $APPDATA\xtrkcad
	
	  IfFileExists $1\xtrkcad.cu1 0 +2 
		CopyFiles $1\xtrkcad.cu1 $APPDATA\xtrkcad
	
	  IfFileExists $1\xtrkcad.ckp 0 +2 
		CopyFiles $1\xtrkcad.ckp $APPDATA\xtrkcad	

NoOldIni:
  CreateDirectory "$SMPROGRAMS\XTrkCAD4"
  CreateShortCut "$SMPROGRAMS\XTrkCAD4\XTrkCad.lnk" "$INSTDIR\xtrkcad.exe" "" "$INSTDIR\xtrkcad.exe" 0
  CreateShortCut "$SMPROGRAMS\XTrkCAD4\XTrkCad Help.lnk" "$INSTDIR\xtrkcad.chm" "" "" 0
  CreateShortCut "$SMPROGRAMS\XTrkCAD4\XTrkCad ReadMe.lnk" "notepad.exe" "$INSTDIR\ReadMe.txt" 	
  CreateShortCut "$SMPROGRAMS\XTrkCAD4\XTrkCad Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  
  ; Write the installation path into the registry
  WriteRegStr HKLM SOFTWARE\XTrkCAD "Install_Dir" "$INSTDIR"
  
  ; Write the uninstall keys for Windows

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\XTrkCAD" "DisplayName" "XTrkCAD Model Railroad Design Software"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\XTrkCAD" "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\XTrkCAD" "DisplayIcon" "$INSTDIR\uninstall.exe"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\XTrkCAD" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\XTrkCAD" "NoRepair" 1
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\XTrkCAD" "URLInfoAbout" "http://www.xtrkcad.org/" 
;  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\XTrkCAD" "DisplayVersion" $(REL)
;    * VersionMajor (DWORD) - Major version number of the application
;    * VersionMinor (DWORD) - Minor version number of the application 


; create file association

; back up old value of .opt
  !define Index "Line${__LINE__}"
  ReadRegStr $1 HKCR ".xtc" ""
  StrCmp $1 "" "${Index}-NoBackup"
    StrCmp $1 "XTrkCAD.Design" "${Index}-NoBackup"
      WriteRegStr HKCR ".xtc" "backup_val" $1
  "${Index}-NoBackup:"
    WriteRegStr HKCR ".xtc" "" "XTrkCAD.Design"
    ReadRegStr $0 HKCR "XTrkCAD.Design" ""
    StrCmp $0 "" 0 "${Index}-Skip"
      WriteRegStr HKCR "XTrkCAD.Design" "" "XTrkCAD Layout Design"
      WriteRegStr HKCR "XTrkCAD.Design\shell" "" "open"
      WriteRegStr HKCR "XTrkCAD.Design\DefaultIcon" "" "$INSTDIR\xtrkcad.exe,0"
    "${Index}-Skip:"
    WriteRegStr HKCR "XTrkCAD.Design\shell\open\command" "" '$INSTDIR\xtrkcad.exe "%1"'
;    WriteRegStr HKCR "XTrkCAD.Design\shell\edit" "" "Edit Options File"
;    WriteRegStr HKCR "XTrkCAD.Design\shell\edit\command" "" '$INSTDIR\xtrkcad.exe "%1"'
 
  System::Call 'Shell32::SHChangeNotify(i 0x8000000, i 0, i 0, i 0)'
  
  !undef Index

  WriteUninstaller "uninstall.exe"
  
SectionEnd

;--------------------------------

; Uninstaller

Section "Uninstall"

  ; Remove file association
  !define Index "Line${__LINE__}"
  ReadRegStr $1 HKCR ".xtc" ""
  StrCmp $1 "XTrkCAD.Design" 0 "${Index}-NoOwn" ; only do this if we own it
    ReadRegStr $1 HKCR ".xtc" "backup_val"
    StrCmp $1 "" 0 "${Index}-Restore" ; if backup="" then delete the whole key
      DeleteRegKey HKCR ".xtc"
      Goto "${Index}-NoOwn"
    "${Index}-Restore:"
      WriteRegStr HKCR ".xtc" "" $1
      DeleteRegValue HKCR ".xtc" "backup_val"
   
  DeleteRegKey HKCR "XTrkCAD.Design" ;Delete key with association settings
 
  System::Call 'Shell32::SHChangeNotify(i 0x8000000, i 0, i 0, i 0)'
  "${Index}-NoOwn:"
  !undef Index

  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\XTrkCAD"
  DeleteRegKey HKLM SOFTWARE\XTrkCAD

  ; Remove files and uninstaller
  Delete $INSTDIR\xtrkcad.exe
  Delete $INSTDIR\uninstall.exe
  Delete $INSTDIR\COPYING.txt
  Delete $INSTDIR\ReadMe.txt
  Delete $INSTDIR\xtrkcad.xtq
  Delete $INSTDIR\xtrkcad.tip"	
  Delete $INSTDIR\xtrkcad.chm"
  Delete $INSTDIR\ChangeLog"	
  Delete $INSTDIR\xtrkcad.gid

  ; Remove the parameter files
  Delete "$INSTDIR\params\*.*"

  ; Remove the demo files
  Delete "$INSTDIR\demos\*.*"

  ; Remove the example files
  Delete "$INSTDIR\examples\*.*"
			
  ; Remove shortcuts, if any
  Delete "$SMPROGRAMS\XTrkCAD4\*.*"

  ; Remove directories used
  RMDir "$SMPROGRAMS\XTrkCAD4"
  RMDir "$INSTDIR\params"	
  RMDir "$INSTDIR\demos"	
  RMDir "$INSTDIR\examples"	
  RMDir "$INSTDIR"

SectionEnd


LangString DESC_program ${LANG_ENGLISH} "This selection installs all required components."

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${program} $(DESC_program)
!insertmacro MUI_FUNCTION_DESCRIPTION_END