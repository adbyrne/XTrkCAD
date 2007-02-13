/**
 * \file addcrlf.c Convert text between DOS, UNIX and MAC
 *
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/tools/addcrlf.c,v 1.7 2007-02-13 19:46:25 m_fischer Exp $
 *
 * This is heavily based on flip by Craig Stuart Sapp <craig@ccrma.stanford.edu>
 * Web Address:   http://www-ccrma.stanford.edu/~craig/utility/flip/flip.cpp 
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) Marin Fischer 2006
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

void exitUsage( char *cmd );
void translateToDos( char *infile, char *outfile );
void translateToUnix(char* infilename, char *outfilename);
void determineType(char* filename);

int 
main( int argc, char ** argv)
{
	char option;
	
	if( argc < 2 ) 
		exitUsage(argv[0]);

   if( argv[1][0] != '-' &&argv[1][0] != '/' ) 
   	exitUsage(argv[0]);
		
   option = argv[1][1];
	
   if (!(option == 'u' || option == 'd' || option == 't' || option =='h' )) {
      exitUsage(argv[0]);
   }

	if( !((option == 't' && argc == 3)||(option !='t' && argc==4)||( option =='h' ))) {
      exitUsage(argv[0]);
	}
   switch( option )
	{
		case 'd':
			translateToDos(argv[2], argv[3]);
			break;
		case 'u':	
			translateToUnix(argv[2], argv[3]);
			break;
		case 'm':	
/*			translateToMacintosh(argv[i+2]); */
			break;
		case 'h':
			exitUsage(argv[0]);
			break;
	   default:
			determineType(argv[2]); 
			break;
   }
   return 0;	
}



void 
translateToDos(char* infilename, char *outfilename)
{
	FILE *in, *out;
   char ch, lastch;
   int peekch;
	
	in = fopen( infilename, "rb" );
   if (!in) {
      printf( "Error: cannot find file: %s\n", infilename );
      return;
   }

	out = fopen( outfilename, "wb" );
   if (!out) {
      printf( "Error: cannot open file: %s\n", outfilename );
      return;
   }

   ch = getc( in );
   lastch = ch;

   while( !feof( in ))
	{
      if (ch == 0x0a && lastch != 0x0d) {
		   // convert newline from Unix to MS-DOS
         putc( (char)0x0d, out );
         putc( ch, out );
         lastch = ch;
      } else {
			if (ch == 0x0d) {             // convert newline from Mac to MS-DOS
         	peekch = getc( in );			// lookahead for following character
				ungetc( peekch, in );

         	if (peekch != 0x0a) {
            	putc( ch, out );
            	putc( (char)0x0a, out );
            	lastch = 0x0a;
         	} else {
            	lastch = 0x0d;
            	// Bug fix here reported by Shelley Adams: running -d
            	// twice in a row was generating Unix style newlines
            	// without the following statement:
            	putc( 0x0d, out );
         	}

      	} else {
         	putc( ch, out );
         	lastch = ch;
			}	
      }
      ch = getc( in );
   }
     
   fclose( in );
	fclose( out );
}

void 
translateToUnix(char* infilename, char *outfilename)
{
	FILE *in, *out;
   char ch, lastch;
   	
	in = fopen( infilename, "rb" );
   if (!in) {
      printf( "Error: cannot find file: %s\n", infilename );
      return;
   }

	out = fopen( outfilename, "wb" );
   if (!out) {
      printf( "Error: cannot open file: %s\n", outfilename );
      return;
   }

   ch = getc( in );
   lastch = ch;

   while( !feof( in ))
	{
		if (ch == 0x0d) {
         putc( (char)0x0a, out );
      } else {
			if (ch == 0x0a) {
         	if (lastch == 0x0d) {
            	// do nothing: already converted MSDOS newline to Unix form
	         } else {
   		      putc( (char)0x0a, out );
	         }
   	   } else {
      	   putc( ch, out );
	      }
		}	
      lastch = ch;
      ch = getc( in );      			
   }
     
   fclose( in );
	fclose( out );
}

void 
determineType(char* filename) 
{
	FILE *in;
   int ch;
	int crcount = 0;
	int lfcount = 0;
	
	in = fopen( filename, "rb" );
   if (!in) {
      printf( "Error: cannot find file: %s\n", filename );
      return;
   }

   ch = getc( in );
	if( ch == EOF ) {
		printf( "Error: file could not be read: %s\n", filename );
		return;
	}	

   while( !feof( in ))
	{
		if (ch == 0x0d) {
         crcount ++;
      } else {
			if (ch == 0x0a) {
				lfcount++;
   	   } 
		}	
      ch = getc( in );      			
   }
     
   fclose( in );

   if ((lfcount == crcount) && (crcount != 0)) {
      printf("%s : DOS\n", filename );
   } else if ((lfcount > 0) && (crcount == 0)) {
      printf("%s : UNIX\n", filename );
   } else if ((lfcount == 0) && (crcount > 0)) {
      printf("%s : MAC\n", filename );
   } else if ((lfcount > 0) && (crcount > 0)) {
      printf("%s : MIXED\n", filename );
   } else {
      printf("%s : UNKNOWN\n", filename );
   }
}

void 
exitUsage( char* commandName ) 
{
   printf( "\nUsage: %s [-h] | [-t infile] | [[-u|-d|-m] infile outfile]\n"
           "   Converts an ASCII file between Unix, MS-DOS/Windows, or Macintosh newline formats\n\n"
           "   Options: \n"
           "      -u  =  convert file to Unix newline format (newline)\n"
           "      -d  =  convert file to MS-DOS/Windows newline format (linefeed + newline)\n"
           "      -m  =  convert file to Macintosh newline format (linefeed)\n"
           "      -t  =  display current file type, no file modifications\n"
	   "      -h  =  show this help\n", 
	   commandName );

   exit(1);
}
