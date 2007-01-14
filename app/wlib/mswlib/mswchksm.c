#include <stdio.h>
#include <sys/stat.h>
#include "../include/wlib.h"
#ifdef WINDOWS
#include <windows.h>
#include "mswint.h"
#endif

#define HEWHDROFFSET	(0x3C)

static FILE * openfile( const char * fn, const char * mode, long * fileSize )
{
	unsigned short PageCnt;
	long FileSize;
	FILE *fp;
	struct stat Stat;
	fp = fopen( fn, mode );
	if (fp == NULL) {
		perror( "fopen" );
		return NULL;
	}
	fread( &PageCnt, sizeof(PageCnt), 1, fp ); /* Read past signature */
	fread( &PageCnt, sizeof(PageCnt), 1, fp ); /* Read past pagesize */
	FileSize = PageCnt;
	fread( &PageCnt, sizeof(PageCnt), 1, fp ); /* Read past pagesize */
	if ( FileSize == 0L )
		FileSize = PageCnt * 512L;
	else
		FileSize += (PageCnt - 1) * 512L;
	*fileSize = FileSize;
	stat( fn, &Stat );
	*fileSize = (long)Stat.st_size;
	fprintf( stderr, "size1 = %ld, size2 = %ld\n", FileSize, (long)Stat.st_size );
	return fp;
}


static unsigned short mswCheck16( FILE * fp, long FileSize, unsigned short * sum16stored )
{
	unsigned short int sum16, NxtInt;
	long x;
	unsigned char NxtChar;
	sum16 = 0;
	fseek(fp, 0, SEEK_SET);

	for (x=0L; x<FileSize/2L; x++) {
		fread( &NxtInt, sizeof NxtInt, 1, fp );
		if (x == 9)
			*sum16stored = NxtInt;
		else
			sum16 += NxtInt;
	}
	if (FileSize%2) {
		fread( &NxtChar, sizeof NxtChar, 1, fp );
		sum16 += (unsigned int)NxtChar;
	}
	return sum16;
}


static int mswCheck32( FILE * fp, long FileSize, long * sum32off, unsigned long * sum32computed, unsigned long * sum32stored )
{
	unsigned long sum32, NxtLong;
	long x;
	long NewHdrOffset;
	unsigned char NxtByte, y;

	fseek( fp, HEWHDROFFSET, SEEK_SET );
	fread( &NewHdrOffset, sizeof NewHdrOffset, 1, fp );
	if (NewHdrOffset == 0) {
		fprintf( stderr, "NewHdrOffset == 0\n" );
		return 0;
	}
	NewHdrOffset = (NewHdrOffset/4)*4;
	*sum32off = NewHdrOffset + 8;
	sum32 = 0L;
	fseek( fp, 0, SEEK_SET );
	for (x = ( NewHdrOffset + 8 ) / 4; x; x-- ) {
		fread( &NxtLong, sizeof NxtLong, 1, fp );
		sum32 += NxtLong;
	}
	fread( sum32stored, sizeof sum32stored, 1, fp );

	for (x=0; x<(FileSize-NewHdrOffset - 12)/4; x++) {
		fread( &NxtLong, sizeof NxtLong, 1, fp );
		sum32 += NxtLong;
	}
	if ( 0L != (x=FileSize%4L) ) {
		NxtLong = 0L;
		for (y=0; y<x; y++ ) {
			fread( &NxtByte, sizeof NxtByte, 1, fp );
			NxtLong += (unsigned long)NxtByte << (8*y);
		}
		sum32 += NxtLong;
	}
	*sum32computed = sum32;
	return 1;
}


#ifdef WINDOWS
wBool_t wCheckExecutable( void )
{
	char fileName[1024];
	FILE * fp;
	long FileSize;
	GetModuleFileName( mswHInst, fileName, sizeof fileName );
	fp = openfile( fileName, "rb", &FileSize );
#ifdef LATER
	{
		unsigned long int sum32offset, sum32computed, sum32stored;
		if ( ! mswCheck32( fp, FileSize, &sum32offset, &sum32computed, &sum32stored ) )
			return FALSE;
		return sum32computed == sum32stored;
	}
#else
	{
		unsigned short int sum16computed, sum16stored;
		sum16computed = mswCheck16( fp, FileSize, &sum16stored );
		sum16computed += sum16stored;
		return sum16computed == 0xFFFF;
	}
#endif
}
#endif
