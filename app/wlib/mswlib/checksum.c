#include "mswchksm.c"
#include <stdlib.h>

int main( int argc, char *argv[] )
{
	int set;
	FILE * fp;
	long FileSize;
	unsigned short sum16computed, sum16stored = 0xb8dd;
	unsigned long sum32computed, sum32stored = 0xa25ce7ac;
	long sum32off;
	if (argc < 2) {
		fprintf( stderr, "Usage: %s [-s] file.exe\n", argv[0] );
		exit(1);
	}
	if (argc > 2) {
		set = 1;
		fp = openfile( argv[2], "r+b", &FileSize );
	} else {
		set = 0;
		fp = openfile( argv[1], "rb", &FileSize );
	}
	if (fp == NULL)
		exit(1);
	
	fprintf( stderr, "File Size = %ld (%lx)\n", FileSize, FileSize );
	sum16computed = mswCheck16( fp, FileSize, &sum16stored );
	if (!mswCheck32( fp, FileSize, &sum32off, &sum32computed, &sum32stored ))
		fprintf( stderr, "mswCheck32 error\n" );
	fprintf( stderr, "sum16: stored = %x, computed = %x, sum = %x, expected FFFF\n", sum16stored, sum16computed, sum16stored+sum16computed );
	fprintf( stderr, "sum32: stored = %lx, computed = %lx, expected %lx\n", sum32stored, sum32computed, sum32stored );
	if (set) {
		fseek( fp, 0x12, SEEK_SET );
		sum16computed = 0xFFFF - sum16computed;
		fwrite( &sum16computed, sizeof sum16computed, 1, fp );
		fseek( fp, sum32off, SEEK_SET );
		/*fwrite( &sum32computed, sizeof sum32computed, 1, fp );*/
		fflush( fp );
	}
	fclose(fp);
	exit(0);
}
