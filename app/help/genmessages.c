/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis
 *						2007 Martin Fischer
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#ifdef WINDOWS
	#if _MSC_VER >=1400
		#define strdup _strdup
	#endif
#endif

#define I18NHEADERFILE "i18n.h"

typedef struct helpMsg_t * helpMsg_p;
typedef struct helpMsg_t {
	char * key;
	char * title;
	char * help;
	} helpMsg_t;

helpMsg_t helpMsgs[200];
int helpMsgCnt = 0;

struct transTbl {
      char *inChar;
      char *outChar[];
};

/* ATTENTION: make sure that the characters are in the same order as the equivalent escape sequences below */

/* translation table for unicode sequences understood by Halibut */
struct transTbl toUnicode = {
       "\xB0\0",
      { "\\u00B0",
		  "\\0"  }
};

/* translation table for escape sequences understood by C compiler */

struct transTbl toC = {
      "\n\\\"\0", 
      { "\\n",		
		"\\\\",
      "\\\"",
		"\\0" }
};


char *
TranslateString( char *srcString, struct transTbl *trTbl )
{
	char *destString;
   char *cp;
   char *cp2;
   size_t bufLen = strlen( srcString ) + 1;
   char *idx;

   /* calculate the expected result length */
   for( cp = srcString; *cp; cp++ )
   	if( idx = strchr( trTbl->inChar, *cp ))         /* does character need translation ? */
      	bufLen += strlen( (trTbl->outChar)[idx - trTbl->inChar ] ) - 1; /* yes, extend buffer accordingly */

   /* allocate memory for result */
   destString = malloc( bufLen );

   if( destString ) {
		/* copy and translate characters as needed */
      cp2 = destString;
      for( cp = srcString; *cp; cp++ ) {
      	if( idx = strchr( trTbl->inChar, *cp )) {       /* does character need translation ? */
         	strcpy( cp2, (trTbl->outChar)[idx - trTbl->inChar ] );   /* yes, copy the escaped character sequence */
            cp2 += strlen((trTbl->outChar)[idx - trTbl->inChar ] );
         } else {
            *cp2++ = *cp;                       /* no, just copy the character */
         }
		}
   	/* terminate string */
   	*cp2 = '\0';
   } else {
   	/* memory allocation failed */
      exit(1);
   }

   return( destString );
}


int cmpHelpMsg( const void * a, const void * b )
{
	helpMsg_p aa = (helpMsg_p)a;
	helpMsg_p bb = (helpMsg_p)b;
	return strcmp( aa->title, bb->title );
}

void unescapeString( FILE * f, char * str ) 
{
	while (*str) {
		if (*str != '\\')
			fputc( *str, f );
		str++;
	}
}

/**
 * Generate the file in help source format ( ie. the BUT file )
 */

void dumpHelp( FILE *hlpsrcF )
{
	int inx;
	char *transStr;
	
	fputs( "\\#\n * DO NOT EDIT! This file has been automatically created by genmessages.\n * Changes to this file will be overwritten.\n", hlpsrcF );

	fprintf( hlpsrcF, "\n\n\\H{messageList} Message Explanations\n\n" );

	/* sort in alphabetical order */
	qsort( helpMsgs, helpMsgCnt, sizeof helpMsgs[0], cmpHelpMsg );

	/* now save all the help messages */
	for ( inx=0; inx<helpMsgCnt; inx++ ) {
	
		transStr = TranslateString( helpMsgs[inx].title, &toUnicode );
		fprintf( hlpsrcF, "\\S{%s} %s\n\n", helpMsgs[inx].key, transStr );
		free( transStr );
		
		transStr = TranslateString( helpMsgs[inx].help, &toUnicode );
		fprintf( hlpsrcF, "%s\n\n", transStr );
		free( transStr );			

		fprintf( hlpsrcF, "\n\n\\rule\n\n" );
	}
	
}


int main( int argc, char * argv[] )
{
	FILE * hdrF;
	FILE *inF;
	FILE *outF;

	char buff[ 4096 ];
	char * cp;
	int inFileIdx;
	enum {m_init, m_title, m_alt, m_help } mode = m_init;
	char msgName[256];
	char msgAlt[256];
	char msgTitle[1024];
	char msgTitle1[1024];
	char msgHelp[4096];
	char *tName, *tAlt, *tTitle;
#ifndef FLAGS	
	int flags; 
#endif
	int i18n = 0;

	/* check argument count */
	if ( argc < 3 || argc > 4 ) {
		fprintf( stderr, "Usage: %s [-i18n] INFILE OUTFILE\n\n", argv[0] );
		fprintf( stderr, "       -i18n is used to generate a include file with gettext support.\n\n" );
		exit(1);
	}

	/* check options */
	if( argc == 4 ) {
		if( !strcmp(argv[ 1 ], "-i18n")){
			i18n = 1;
			inFileIdx = 2;	/* second argument is input file */
		}
		/* inFileIdx = 2;  skip over option argument */
	} else {
		inFileIdx = 1;	/* first argument is input file */
	}	

	/* open the file for reading */
	inF = fopen( argv[ inFileIdx ], "r" );
	if( !inF ) {
			fprintf( stderr, "Could not open %s for reading!\n", argv[ inFileIdx ] );
			exit( 1 );
	}		
			
	/* open the include file to generate */		
	hdrF = fopen( "messages.h", "w" );
	if( !hdrF ) {
			fprintf( stderr, "Could not open messages.h for writing!\n" );
			exit( 1 );
	}		

	fputs( "/*\n * DO NOT EDIT! This file has been automatically created by genmessages.\n * Changes to this file will be overwritten.\n */\n", hdrF );
	
	/* open the help file to generate */
	outF = fopen( argv[ inFileIdx + 1 ], "w" );
	if( !inF ) {
			fprintf( stderr, "Could not open %s for writing!\n", argv[ inFileIdx ] );
			exit( 1 );
	}		

	/* Include i18n header, if needed */
	if (i18n)
		fprintf( hdrF, "#include \"" I18NHEADERFILE "\"\n\n" );

	while ( fgets( buff, sizeof buff, inF ) ) {
	
		/* skip comment lines */
		if ( buff[0] == '#' )
			continue;

		/* remove trailing whitespaces */	
		cp = buff+strlen(buff)-1;
		while( cp >= buff && isspace(*cp)) {
			*cp = '\0';
			cp--;
		}

		if ( strncmp( buff, "MESSAGE ", 8 ) == 0 ) {

			/* skip any spaces */
			cp = strchr( buff+8, ' ' );
			if (cp)
				while (*cp == ' ') *cp++ = 0;
#ifndef FLAGS		
			if ( cp && *cp ) {
				flags = atoi(cp);
			} 
#endif			
			/* save the name of the message */
			strcpy( msgName, buff + 8 );
			msgAlt[0] = 0;
			msgTitle[0] = 0;
			msgTitle1[0] = 0;
			msgHelp[0] = 0;
			mode = m_title;
		} else if ( strncmp( buff, "ALT", 3 ) == 0 ) {
			mode = m_alt;
			msgAlt[0] = 0;			
		} else if ( strncmp( buff, "HELP", 4 ) == 0 ) {
			mode = m_help;
		} else if ( strncmp( buff, "END", 3 ) == 0 ) {
			/* the whole message has been read */

			/* create escape sequences */
			tName = TranslateString( msgName, &toC );
			tTitle = TranslateString( msgTitle, &toC );	
			tAlt = TranslateString( msgAlt, &toC );
			
			if (msgHelp[0]==0) {
				/* no help text is included */
				if (i18n)
					fprintf( hdrF, "#define %s N_(\"%s\")\n", tName, tTitle );
				else
					fprintf( hdrF, "#define %s \"%s\"\n", tName, tTitle );
			} else if (msgAlt[0]) {
				/* a help text and an alternate description are included */
				if (i18n)
					fprintf( hdrF, "#define %s N_(\"%s\\t%s\\t%s\")\n", tName, tName, tAlt, tTitle );
				else
					fprintf( hdrF, "#define %s \"%s\\t%s\\t%s\"\n", tName, tName, tAlt, tTitle );
			} else {
				/* a help text but no alternate description are included */
				if (i18n)
					fprintf( hdrF, "#define %s N_(\"%s\\t%s\")\n", tName, tName, tTitle );
				else
					fprintf( hdrF, "#define %s \"%s\\t%s\"\n", tName, tName, tTitle );
			}
			
			/*free temp stzrings */
			free( tName );
			free( tTitle );
			free( tAlt );
			
			/* save the help text for later use */
			if (msgHelp[0]) {
				helpMsgs[helpMsgCnt].key = strdup(msgName);
				if ( msgAlt[0] )
				    helpMsgs[helpMsgCnt].title = strdup(msgAlt);
				else
				    helpMsgs[helpMsgCnt].title = strdup(msgTitle);
				helpMsgs[helpMsgCnt].help = strdup(msgHelp);
				helpMsgCnt++;
			}
			mode = 0;
		} else {
			/* are we currently reading the message text? */
			if (mode == m_title) {
				/* yes, is the message text split over two lines ? */
				if (msgTitle[0]) {
					/* if yes, keep the first part as the short text */			
					if (msgAlt[0] == 0) {
						strcpy( msgAlt, msgTitle );
						strcat( msgAlt, "..." );
					}
					/* add a newline to the first part */
					strcat( msgTitle, "\n" );
				}
				/* now save the buffer into the message title */
				strcat( msgTitle, buff );
			} else if (mode == m_alt) {
				/* an alternate text was explicitly specified, save */
				if( msgAlt[ 0 ] ) {
					strcat( msgAlt, " " );
					strcat( msgAlt, buff );
				} else {					
					strcpy( msgAlt, buff );
				}	
			} else if (mode == m_help) {
				/* we are reading the help text, save in buffer */
				strcat( msgHelp, buff );
				strcat( msgHelp, "\n" );
			}
		}
	}
	dumpHelp( outF );

	fclose( hdrF );
	fclose( inF );
	fclose( outF );
	
	printf( "%d messages\n", helpMsgCnt );
	return 0;
}
