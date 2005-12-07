/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis
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
#ifdef WINDOWS
#include <stdlib.h>
#endif


typedef struct helpMsg_t * helpMsg_p;
typedef struct helpMsg_t {
	char * key;
	char * title;
	char * help;
	} helpMsg_t;

helpMsg_t helpMsgs[200];
int helpMsgCnt = 0;

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


void dumpHelp( void )
{
	FILE * hlpsrcF;
	int inx;
	qsort( helpMsgs, helpMsgCnt, sizeof helpMsgs[0], cmpHelpMsg );
	hlpsrcF = fopen( "messages.hlpsrc", "w" );
	fprintf( hlpsrcF, "?LS\n" );
	for ( inx=0; inx<helpMsgCnt; inx++ ) {
		fprintf( hlpsrcF, "?LI\n$B" );
		unescapeString( hlpsrcF, helpMsgs[inx].title );
		fprintf( hlpsrcF, "$\n?H" );
		unescapeString ( hlpsrcF, helpMsgs[inx].key );
		fprintf( hlpsrcF, "\n\n" );
		unescapeString ( hlpsrcF, helpMsgs[inx].help );
		fprintf( hlpsrcF, "\n\n?H****\n" );
	}
	fprintf( hlpsrcF, "?LE\n" );
	fclose( hlpsrcF );
}


int main( int argc, char * argv[] )
{
	FILE * hdrF;
	char buff[256];
	char * cp;
	enum {m_init, m_title, m_alt, m_help } mode = m_init;
	char msgName[256];
	char msgAlt[256];
	char msgTitle[1024];
	char msgTitle1[1024];
	char msgHelp[4096];
	int flags;

	hdrF = fopen( "messages.h", "w" );
	while ( fgets( buff, sizeof buff, stdin ) ) {
		if ( buff[0] == '#' )
			continue;
		cp = buff+strlen(buff)-1;
		if (*cp=='\n') *cp = 0;
		if ( strncmp( buff, "MESSAGE ", 8 ) == 0 ) {
			cp = strchr( buff+8, ' ' );
			if (cp)
				while (*cp == ' ') *cp++ = 0;
			if ( cp && *cp ) {
				flags = atoi(cp);
			}
			strcpy( msgName, buff+8 );
			msgAlt[0] = 0;
			msgTitle[0] = 0;
			msgTitle1[0] = 0;
			msgHelp[0] = 0;
			mode = m_title;
		} else if ( strncmp( buff, "ALT", 3 ) == 0 ) {
			mode = m_alt;
		} else if ( strncmp( buff, "HELP", 4 ) == 0 ) {
			mode = m_help;
		} else if ( strncmp( buff, "END", 3 ) == 0 ) {
			if (msgHelp[0]==0) {
				fprintf( hdrF, "#define %s \"%s\"\n", msgName, msgTitle );
			} else if (msgAlt[0]) {
				fprintf( hdrF, "#define %s \"%s\\t%s\\t%s\"\n", msgName, msgName, msgAlt, msgTitle );
			} else {
				fprintf( hdrF, "#define %s \"%s\\t%s\"\n", msgName, msgName, msgTitle );
			}
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
			if (mode == m_title) {
				if (msgTitle[0]) {
					if (msgAlt[0] == 0) {
						strcpy( msgAlt, msgTitle );
						strcat( msgAlt, "..." );
					}
					strcat( msgTitle, "\\n" );
				}
				strcat( msgTitle, buff );
			} else if (mode == m_alt) {
				strcpy( msgAlt, buff );
			} else if (mode == m_help) {
				strcat( msgHelp, buff );
				strcat( msgHelp, "\n" );
			}
		}
	}
	fclose( hdrF );
	dumpHelp();
	printf( "%d messages\n", helpMsgCnt );
	return 0;
}
