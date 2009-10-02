/* 
BIN2C V1.0 CODED BY CHRISTIAN PADOVANO ON 17-MAY-1995     
this little utility translates a binary file in a useful C structure
that can be included in a C source.
to contact me write to EMAIL: [[Email Removed]]
*/


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define BUF_LEN  1024
#define LINE     12

/* Tell u the file size in bytes */

long int filesize( FILE *fp )
{
    long int save_pos, size_of_file;      
	 
	 save_pos = ftell( fp );     
	 fseek( fp,0L, SEEK_END );     
	 size_of_file = ftell( fp );     
	 fseek( fp, save_pos, SEEK_SET );
	 return( size_of_file );
}


/* lower chars --> upper chars */

void Upper_chars(char *buffer)
{
 unsigned int c;   
 
 for (c=0; c <= strlen(buffer)-1; c++) 
 	*(buffer+c)=toupper( *( buffer+c) );
}


int main( int argc, char **argv )
{
    FILE *source,*dest;     
	 char Dummy[BUF_LEN];     
	 int buffer;
	 int c;      
	 
	 if ( (argc < 4) )
    {

    	if (  ( argc == 2 ) && ( strcmp(argv[1],"-h")==0 )  )
      {
      	puts(" - <<< BIN2C V1.0 >>> by Christian Padovano - \n");
			puts("USAGE: bin2C  <BINARY file name> <TARGET file name> <STRUCT name>");       
			puts("\n <STRUCT > = name of the C structure in the destination file name.\n"); 
			puts(" <TARGET > = without extension '.h' it will be added by program."); 
			return EXIT_SUCCESS;
		}
      else
      {
      	puts("Bad arguments !!! You must give me all the parameters !!!!\n"
         	  "Type 'bin2c -h' to read the help !!!! ");       
			return EXIT_SUCCESS;
	   }

    }

    if( (source=fopen( argv[1], "rb" )) == NULL )
    {
      printf("ERROR : I can't find source file   %s\n",argv[1]);       
		return EXIT_FAILURE;
    }

    strcpy(Dummy,argv[2]);     
	 strcat(Dummy,".h");               /* add suffix .h to target name */

    if( (dest=fopen( Dummy, "wb+" )) == NULL )
    {
      printf("ERROR : I can't open destination file   %s\n",Dummy);
		return EXIT_FAILURE;
	 }


    strcpy(Dummy,argv[3]);     
	 Upper_chars(Dummy);    /* lower to upper chars */
    strcat(Dummy,"_LEN");  /* add the suffix _LEN to the struct name */
                           /* for the #define stantment              */


    /* It writes the header information */
    fprintf( dest, "\n#define %s %ld\n\n", Dummy, filesize(source) );     
	 fprintf( dest, "static unsigned char %s[] = {\n  ", argv[3] );
	 
	 if( ferror( dest ))
    {
    	printf( "ERROR writing on target file:  %s\n",argv[2] ); 
		return EXIT_FAILURE;
	 }


	 c = 0;
	 buffer = fgetc( source );			
	
    while( buffer != EOF )
    {
		fprintf(dest,"0x%02x", buffer);	 	
			
		buffer = fgetc( source );	
		if( !feof(source))
			fputc(',', dest);
		
		c++; 	
		if(c == LINE )	{
	 	   fprintf(dest,"\n  ");   
			c = 0;
		}	

	 }
     
    fprintf(dest,"\n};\n\n");
	 
	return EXIT_SUCCESS;
}


