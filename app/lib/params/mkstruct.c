#include <stdio.h>
#include <stdlib.h>
#ifndef WINDOWS
#include <string.h>
#endif

int main ( int argc, char * argv [] )
{
	long color = 0xFF00FF;
	double x;
	double y;
	int cm = 0;

	while (argc > 1 && argv[1][0] == '-' ) {
	    if ( strcmp( argv[1], "-cm" ) == 0 ) {
		cm++;
		argv++;
		argc--;
	    } else if ( strncmp( argv[1], "-color=", strlen( "-color=" ) ) == 0 ) {
		color = strtol( argv[1]+strlen( "-color=" ), NULL, 16 );
		argv++;
		argc--;
	    } else {
		fprintf( stderr, "Unrecognized option: %s\n", argv[1] );
		exit(1);
	    }
	}

	if ( argc != 5 ) {
		fprintf( stderr, "Usage: mkstruct [-cm] SCALE NAME X Y\n" );
		exit(1);
	}

	x = atof( argv[4] );
	y = atof( argv[3] );
	if (cm) {
		x /= 2.54;
		y /= 2.54;
	}

	printf("STRUCTURE %s \"%s\"\n", argv[1], argv[2] );
	printf("\tL %ld 0 %0.6f %0.6f %0.6f %0.6f\n", color, 0.0, 0.0, 0.0, x );
	printf("\tL %ld 0 %0.6f %0.6f %0.6f %0.6f\n", color, 0.0, x, y, x );
	printf("\tL %ld 0 %0.6f %0.6f %0.6f %0.6f\n", color, y, x, y, 0.0 );
	printf("\tL %ld 0 %0.6f %0.6f %0.6f %0.6f\n", color, y, 0.0, 0.0, 0.0 );
	printf("\tEND\n");

	exit(0);
}
