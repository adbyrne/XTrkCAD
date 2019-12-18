#ifndef READXTP_H
	#define READXTP_H

	#define CONTENTSCOMMAND "CONTENTS"

	#ifndef WIN32
	#define stricmp strcasecmp
	#define strnicmp strncasecmp
	#endif // !WIN32

	char *GetParameterFileContent(char *file);
#endif
