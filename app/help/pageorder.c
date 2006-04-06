
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
#if defined (__sun) && defined (__SVR4)
#include <stdlib.h>
#endif

int main ( int argc, char * argv[] ) {

int pagecnt, start, end, count ;
pagecnt = atoi( argv[1] );

if ( (pagecnt+2)%4 != 0 ) {
	fprintf( stderr, "pagecnt+2 % 4 != 0\n" );
	exit(1);
}

printf( "%d-,%d\n", pagecnt, pagecnt-1 );
start = 1;
end = pagecnt-2;
count=5;
while ( start < end ) {
	printf( "%d,%d,%d,%d%s", end,start,start+1,end-1, count>0?",":"\n" );
	start += 2;
	end -= 2;
	count--;
	if ( count < 0 )
		count = 5;
}
return 0;
}
