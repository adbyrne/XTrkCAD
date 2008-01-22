/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2008 Mikko Nissinen
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
 * Input file types:
 *  XTR = Marco file. Parse MESSAGE...END blocks. 
 *  XTQ = Configuration file. Parse DEMOGROUP and DEMO lines.
 *  TIP = Tip of the day file.
 */
typedef enum { MODE_XTR, MODE_XTQ, MODE_TIP } mode_e;


/* Process the given input file. */
void process( mode_e mode, FILE * inFile )
{
	char line[4096];
	char * cp;
	int len;
	int offset;
	int i;

	while ( fgets( line, sizeof(line), inFile ) != NULL )
	{
		offset = 0;

		switch (mode)
		{
		case MODE_XTR:
			if (strncmp( line, "MESSAGE", 7 ) == 0)
			{
				while ( ( fgets( line, sizeof(line), inFile ) ) != NULL ) {
					if ( strncmp(line, "END", 3) == 0)
						/* End of message block */
						break;
					else if (strncmp(line, "__________", 10) == 0
							|| strncmp(line, "==========", 10) == 0)
						/* Skip */
						continue;

					len = (int)strlen( line );
					if (len > 0 && line[len-1] == '\n' ) len--;
					if (len > 0 && line[len-1] == '\r' ) len--;
					line[len] = '\0';
					if (len > 0)
					{
						if (strchr(line, '"'))
						{
							printf("N_(\"");
							for (i = 0; i < len; i++)
							{
								/* Escape double quotation marks */
								if (line[i] == '"')
									putchar('\\');
								putchar(line[i]);
							}
							printf("\\n\");\n");
						}
						else
						{
							printf("N_(\"%s\\n\");\n", line);
						}
					}
				} // while (in msg block)
			}
			break; // case MODE_XTR:

		case MODE_XTQ:
			if ( strncmp( line, "DEMOGROUP ", 10 ) == 0 )
			{
				offset = 10;
			}
			else if ( strncmp( line, "DEMO ", 5 ) == 0 )
			{
				offset = 6;
				if (line[5] != '"')
					break;
				cp = line+offset;
				while (*cp && *cp != '"') cp++;
				if ( !*cp )
					break;
				*cp++ = '\0';
				while (*cp && *cp == ' ') cp++;
				if ( strlen(cp)==0 )
					break;
			}
			if (offset > 0)
			{
				len = (int)strlen( line );
				if (line[len-1] == '\n' ) len--;
				if (line[len-1] == '\r' ) len--;
				line[len] = '\0';
				if (len == 0)
					break;
				printf("N_(\"%s\");\n", line+offset);
			}
			break; // case MODE_XTQ:

		case MODE_TIP:
			/* lines starting with hash sign are ignored (comments) */
			if (line[0] == '#')
				continue;

			/* remove CRs and LFs at end of line */				
			cp = line+strlen(line)-1;
			if (*cp=='\n') cp--;
			if (*cp=='\r') cp--;

			/* get next line if the line was empty */
			if (cp < line)
				continue;

			cp[1] = '\0';

			/* if line ended with a continuation sign, get the rest */
			while (*cp=='\\') {
				*cp++ = '\\';
				*cp++ = 'n';

				/* read a line */
				if (!fgets( cp, (sizeof(line)) - (cp-line), inFile )) {
					return;
				}

				/* lines starting with hash sign are ignored (comments) */
				if (*cp=='#')
					continue;

				/* remove CRs and LFs at end of line */				
				cp += strlen(cp)-1;
				if (*cp=='\n') cp--;
				if (*cp=='\r') cp--;
				cp[1] = '\0';
			}

			if (strchr(line, '"'))
			{
				printf("N_(\"");
				len = strlen(line);
				for (i = 0; i < len; i++)
				{
					/* Escape double quotation marks */
					if (line[i] == '"')
						putchar('\\');
					putchar(line[i]);
				}
				printf("\");\n");
			}
			else
			{
				printf("N_(\"%s\");\n", line);
			}
			break; // case MODE_TIP:
		} // switch (mode)
	} // while (...)
}


int main ( int argc, char * argv[] )
{
	FILE * inFile;
	mode_e mode;
	char *ch;
	int i;
	int files = 0;
	int xtrFiles = 0;
	int xtqFiles = 0;
	int tipFiles = 0;
	int errors = 0;

	if ( argc < 2 ) {
		fprintf( stderr,
				"Usage: %s files ...\n"
				"       Where \'files\' is a list of files to be parsed. Program\n"
				"       automatically detects the file type from the file extension.\n"
				"       Supported file types are:\n"
				"          .xtr, .xtq, .tip\n",
				argv[0] );
		exit(1);
	}

	/* Print header info */
	printf("/* ----------------------------------------------------------*\n"
		   " * These strings are generated from the XTrkCad macro and\n"
		   " * Tip of the day files by %s. The strings are\n"
		   " * formatted so that the xgettext can extract them into\n"
		   " * .pot file for translation.\n"
		   " * ----------------------------------------------------------*/\n",
		   argv[0]);

	for (i = 1; i < argc; i++)
	{
		/* Set operating mode according to the file name extension */
		ch = strrchr(argv[i], '.');
		if (ch == NULL)
		{
			errors++;
			fprintf( stderr, "WARNING: No file name extension in file \"%s\"\n", argv[i]);
			continue;
		}
		ch++;
		if ( strcmp( ch, "xtq" ) == 0 )
			mode = MODE_XTQ;
		else if ( strcmp( ch, "xtr" ) == 0 )
			mode = MODE_XTR;
		else if ( strcmp( ch, "tip" ) == 0 )
			mode = MODE_TIP;
		else
		{
			errors++;
			fprintf( stderr, "WARNING: Unknown file name extension in file \"%s\"\n", argv[i]);
			continue;
		}

		/* Open file */
		inFile = fopen( argv[i], "r" );
		if (inFile == NULL) {
			errors++;
			perror( argv[i] );
			continue;
		}

		/* Process file */
		process( mode, inFile );

		/* Close  file */
		files++;
		switch (mode)
		{
		case MODE_XTQ:
			xtqFiles++;
			break;
		case MODE_XTR:
			xtrFiles++;
			break;
		case MODE_TIP:
			tipFiles++;
			break;
		}
		fclose(inFile);
		inFile = NULL;
	}

	/* Print out the results */
	printf("/* ----------------------------------------------------------*\n"
		   " * Input files:   %d\n"
		   " * Files handled: %d (xtq: %d, xtr: %d, tip: %d)\n"
		   " * Errors:        %d\n"
		   " * ----------------------------------------------------------*/\n",
		   argc-1,
		   files, xtqFiles, xtrFiles, tipFiles,
		   errors);

	exit(0);
}
