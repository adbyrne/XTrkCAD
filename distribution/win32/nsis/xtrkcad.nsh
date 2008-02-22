
SetShellVarContext current

IfFileExists "$APPDATA\XTrackCad\." +3 0
	CreateDirectory "$APPDATA\XTrackCad\"
	MessageBox MB_OK "$APPDATA\XTrackCad created" 
	
	
