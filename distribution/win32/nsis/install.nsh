;
; This file is included from the CMake generated NSIS file during install.
;

CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\XTrkCad Help.lnk" "$INSTDIR\share\xtrkcad\xtrkcad.chm" "" "" 0
CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\XTrkCad ReadMe.lnk" "notepad.exe" "$INSTDIR\share\xtrkcad\ReadMe.txt" 	
  

; create the directory in the user profile
SetShellVarContext current

; Does an INI file exist?
;  If yes, the program has been run before, so leave it alone
;  If no, try to find an INI file in the former locations and copy it to the new place ( application settings)
;
  IfFileExists $APPDATA\XTrackCad\xtrkcad.ini NoOldIni 0
  
; INI file does not exist at this point, test whether the directory exists and create if necessary
;  	
	IfFileExists $APPDATA\XTrackCad\. DirExists 0
		CreateDirectory $APPDATA\XTrackCad

DirExists:		  
; find the old INI file location
;
; the default location is the Windows directory, start there 
;
    StrCpy $1 $WINDIR

; the user can select the directory select by configuring an xtrkcad0.ini, so look for this next 
;
    IfFileExists $INSTDIR\xtrkcad0.ini 0 NoIni
	  ReadINIStr $1 $INSTDIR\xtrkcad0.ini workdir path
	
NoIni:	

;  In case an ini file exists in this location, copy it to the new location
;
    IfFileExists $1\xtrkcad.ini 0 NoOldIni	
	  CopyFiles $1\xtrkcad.ini $APPDATA\XTrackCad
	
	  IfFileExists $1\xtrkcad.cus 0 +2 
		CopyFiles $1\xtrkcad.cus $APPDATA\XTrackCad
	
	  IfFileExists $1\xtrkcad.cu1 0 +2 
		CopyFiles $1\xtrkcad.cu1 $APPDATA\XTrackCad
	
	  IfFileExists $1\xtrkcad.ckp 0 +2 
		CopyFiles $1\xtrkcad.ckp $APPDATA\XTrackCad

; at this point, the ini file does exist in the user's roaming profile directory
; continue installation with setting up the menu short cuts
;

NoOldIni:
;	
;  create file association
;
; back up old value of .opt
  !define Index "Line${__LINE__}"
  ReadRegStr $1 HKCR ".xtc" ""
  StrCmp $1 "" "${Index}-NoBackup"
    StrCmp $1 "XTrackCAD.Design" "${Index}-NoBackup"
      WriteRegStr HKCR ".xtc" "backup_val" $1
  "${Index}-NoBackup:"
  
; create the new association  
    WriteRegStr HKCR ".xtc" "" "XTrackCAD.Design"
    ReadRegStr $0 HKCR "XTrackCAD.Design" ""
    StrCmp $0 "" 0 "${Index}-Skip"
      WriteRegStr HKCR "XTrackCAD.Design" "" "XTrackCAD Layout Design"
      WriteRegStr HKCR "XTrackCAD.Design\shell" "" "open"
      WriteRegStr HKCR "XTrackCAD.Design\DefaultIcon" "" "$INSTDIR\bin\xtrkcad.exe,0"
    "${Index}-Skip:"
    WriteRegStr HKCR "XTrackCAD.Design\shell\open\command" "" '$INSTDIR\bin\xtrkcad.exe "%1"'
	
  System::Call 'Shell32::SHChangeNotify(i 0x8000000, i 0, i 0, i 0)'
  	
  !undef Index