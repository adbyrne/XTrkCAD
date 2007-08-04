/* AutoCAD DXF file to DKB Data File Converter */
/* Version 1.0 By Aaron A. Collins.  Written 8/13/90 */
/* This program is released to the public domain. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFSIZE	2048
#define FABS(x) ((x<0.0)?-x:x)

int getline (void);
void writeobj (void);
void finishobj (int color);
void lookupcolor (int color, float *red, float *green, float *blue);
int checkdegen(int a, int b, int c);

int groupcode;
char inname[80], outname[80];
char linbuf[BUFSIZE];
FILE *infile, *outfile;
long primitives = 0L, degenerates = 0L;
char curobj[80];
int curcolor;
float curthick;
float xcoords[10];
float ycoords[10];
float zcoords[10];
float floats[10];
float angles[10];
int ints[10];
float max_x, max_y, max_z, min_x, min_y, min_z;

main(argc, argv)
int argc;
char *argv[];
{
	char *index;

	printf("\n\nAutoCad DXF to DKB .Data file Translator\n");
	printf("Version 1.0 Written By Aaron A. Collins, 8/13/90\n");
	printf("This program is released to the public domain.\n\n");
	if (argc < 2 || argc > 3)
	{
		printf("Usage:  %s infile[.DXF] [outfile[.DAT]]\n", argv[0]);
		exit(1);
	}
	strcpy(inname, argv[1]);	/* make copy we can mess with */
	if (!strchr(inname, '.'))	/* no dot present in filename? */
		strcat(inname, ".DXF");
	if (!(infile = fopen(inname, "r")))
	{
		printf("Cannot open input file %s!\n", inname);
		exit(1);
	}
	if (argc < 3)			/* i.e. no 2nd name supplied... */
	{
		strcpy(outname, inname);
		index = strchr(outname, '.');	/* find the dot */
		strcpy(index, ".DAT");		/* make new ext. .DAT... */
	}
	else
	{
		strcpy(outname, argv[2]);
		if (!strchr(inname, '.'))	/* no dot in filename? */
			strcat(inname, ".DAT");	/* add .DAT extension */
	}
	if (!(outfile = fopen(outname, "w")))
	{
		printf("Cannot create output file %s!\n", outname);
		fclose(infile);
		exit(1);
	}

	printf("\nPlease wait; Processing...");
	
	fprintf(outfile, "{ %s: Converted from AutoCad DXF File: %s }\n\n", outname, inname);
	fprintf(outfile, "INCLUDE \"basicshapes.data\"\n\n");
	fprintf(outfile, "{ Put a VIEW_POINT here... }\n\n");
	fprintf(outfile, "{ Put a LIGHT_SOURCE here... }\n\n");
	fprintf(outfile, "COMPOSITE\n");

	curobj[0] = '\0';	/* not working on any object currently */
	curcolor = 7;		/* and it also doesn't have a color yet... */
	max_x = max_y = max_z = -10000000.0;	/* init bounding limits */
	min_x = min_y = min_z =  10000000.0;

find:	while (!feof(infile))	/* run file up to the "ENTITIES" section */
	{
		if (getline())		/* get a group code and a line */
			goto stopit;
		if (groupcode == 0)	/* file section mark */
		{
			if (strstr(linbuf, "EOF"))
				goto stopit;
			if (strstr(linbuf, "SECTION"))
			{
				if (getline())
					goto stopit;
				if (groupcode != 2)
					continue;
				if (strstr(linbuf, "ENTITIES"))
					break;
			}
		}
	}
	while (!feof(infile))		/* scan ENTITIES section */
	{
		if (getline())		/* get a group code and a line */
			break;
		if (groupcode < 10)	/* cardinal group codes */
		{
			switch(groupcode)
			{
				case 0: /* start of entity, table, file sep */
					if (strstr(linbuf, "EOF"))
					{
						writeobj(); /* dump object */
						goto stopit;
					}
					if (strstr(linbuf, "ENDSEC"))
					{
						writeobj(); /* dump object */
						goto find;
					}
					writeobj();	/* dump old object */
					curobj[0] = '\0'; /* reset object */
					curcolor = 7;
					strcpy(curobj, linbuf);	/* get new */
					break;
				case 1:	/* primary text value for entity (?)*/
					break;
				case 2: /* block name, attribute tag, etc */
				case 3:	/* other names */
				case 4:
					break;
				case 5:	/* entity handle (hex string) */
					break;
				case 6: /* line type name */
					break;
				case 7: /* text style name */
					break;
				case 8: /* layer name */
					break;
				case 9: /* variable name ID (only in header)*/
					break;
			}
		}
		else if (groupcode >= 10 && groupcode < 19) /* Some X coord */
		{
			sscanf(linbuf, "%f", &(xcoords[groupcode-10]));
			if (xcoords[groupcode-10] > max_x)
				max_x = xcoords[groupcode-10];
			if (xcoords[groupcode-10] < min_x)
				min_x = xcoords[groupcode-10];
		}
		else if (groupcode >= 20 && groupcode < 29) /* Some Y coord */
		{
			sscanf(linbuf, "%f", &(ycoords[groupcode-20]));
			if (ycoords[groupcode-20] > max_y)
				max_y = ycoords[groupcode-20];
			if (ycoords[groupcode-20] < min_y)
				min_y = ycoords[groupcode-20];
		}
		else if (groupcode >= 30 && groupcode < 38) /* Some Z coord */
		{
			sscanf(linbuf, "%f", &(zcoords[groupcode-30]));
			if (zcoords[groupcode-30] > max_z)
				max_z = zcoords[groupcode-30];
			if (zcoords[groupcode-30] < min_z)
				min_z = zcoords[groupcode-30];
		}
		else if (groupcode == 38) /* entity elevation if nonzero */
		{
		}
		else if (groupcode == 39) /* entity thickness if nonzero */
		{
		}
		else if (groupcode >= 40 && groupcode < 49) /* misc floats */
		{
			sscanf(linbuf, "%f", &(floats[groupcode-40]));
		}
		else if (groupcode == 49) /* repeated value groups */
		{
		}
		else if (groupcode >= 50 && groupcode < 59) /* misc angles */
		{
			sscanf(linbuf, "%f", &(angles[groupcode-50]));
		}
		else if (groupcode == 62) /* Color number */
		{
			sscanf(linbuf, "%6d", &curcolor);
		}
		else if (groupcode == 66) /* "entities follow" flag */
		{
		}
		else if (groupcode >= 70 && groupcode < 79) /* misc ints */
		{
			sscanf(linbuf, "%f", &(ints[groupcode-70]));
		}
		else if (groupcode == 210 || groupcode == 220 || groupcode == 230)
		{	/* X, Y, Z components of extrusion direction */
		}
	}
stopit: fprintf(outfile, "  BOUNDED_BY\n    INTERSECTION\n");
	fprintf(outfile, "      PLANE <1.0  0.0  0.0> %1.04f END_PLANE\n", FABS(max_x) * 1.01);
	fprintf(outfile, "      PLANE <-1.0 0.0  0.0> %1.04f END_PLANE\n", FABS(min_x) * 1.01);
	fprintf(outfile, "      PLANE <0.0  1.0  0.0> %1.04f END_PLANE\n", FABS(max_y) * 1.01);
	fprintf(outfile, "      PLANE <0.0 -1.0  0.0> %1.04f END_PLANE\n", FABS(min_y) * 1.01);
	fprintf(outfile, "      PLANE <0.0  0.0  1.0> %1.04f END_PLANE\n", FABS(max_z) * 1.01);
	fprintf(outfile, "      PLANE <0.0  0.0 -1.0> %1.04f END_PLANE\n", FABS(min_z) * 1.01);
	fprintf(outfile, "    END_INTERSECTION\n  END_BOUND\n   \nEND_COMPOSITE\n");
	fclose(infile);
	fflush(outfile);
	fclose(outfile);
	printf("Finished.\n\nTotal objects written to output file: %ld\n\n", primitives);
	printf("Total degenerate triangles removed from scene: %ld\n\n", degenerates);
	printf ("X bounding values range from %f to %f\n", min_x, max_x);
	printf ("Y bounding values range from %f to %f\n", min_y, max_y);
	printf ("Z bounding values range from %f to %f\n", min_z, max_z);
	exit(0);
}


int getline()		/* read a group code and the next line from infile */
{
	fgets(linbuf, BUFSIZE, infile);	/* get a line from .DXF */
	if (feof(infile))
		return(1);
	sscanf(linbuf, "%3d", &groupcode);  /* scan out group code */
	fgets(linbuf, BUFSIZE, infile);	/* get a line from .DXF */
	if (feof(infile))
		return(1);
	return(0);
}

void writeobj()	/* dump out current object we should have all info on */
{
	if (strstr(curobj, "LINE"))		/* a VERY skinny triangle! */
	{
		if (xcoords[0] == xcoords[1] && ycoords[0] == ycoords[1] && zcoords[0] == zcoords[1])
		{
			degenerates++;
			return;
		}

		fprintf(outfile, "OBJECT\n");
		fprintf(outfile, "  TRIANGLE <%1.06f %1.06f %1.06f> <%1.06f %1.06f %1.06f> <%1.06f %1.06f %1.06f> END_TRIANGLE\n", xcoords[0], ycoords[0], zcoords[0], xcoords[1], ycoords[1], zcoords[1], xcoords[1]+0.01, ycoords[1], zcoords[1]);
		finishobj(curcolor);

		return;
	}
	else if (strstr(curobj, "POINT"))	/* an itty, bitty sphere! */
	{
		fprintf(outfile, "OBJECT\n");
		fprintf(outfile, "  SPHERE <%1.06f %1.06f %1.06f> 0.1 END_SPHERE\n", xcoords[0], ycoords[0], zcoords[0]);
		finishobj(curcolor);
		return;
	}
	else if (strstr(curobj, "CIRCLE"))	/* a VERY short cylinder! */
	{
		fprintf(outfile, "OBJECT\n");
		fprintf(outfile, "  INTERSECTION\n");
		fprintf(outfile, "    QUADRIC Cylinder_Z SCALE <%1.06f %1.06f %1.06f> END_QUADRIC\n", floats[0], floats[0], floats[0]);
		fprintf(outfile, "    PLANE <0.0 0.0 1.0>  0.1 END_PLANE\n");
		fprintf(outfile, "    PLANE <0.0 0.0 -1.0> 0.1 END_PLANE\n");
		fprintf(outfile, "  END_INTERSECTION\n");
		fprintf(outfile, "  TRANSLATE <%1.06f %1.06f %1.06f>\n", xcoords[0], ycoords[0], zcoords[0]);
		finishobj(curcolor);
		return;
	}
	else if (strstr(curobj, "ARC"))		/* not implemented for now */
	{
		return;
	}
	else if (strstr(curobj, "TRACE"))	/* 2 back-to-back triangles */
	{
		if (checkdegen(0, 1, 2))
		{
			degenerates++;
			return;
		}

		fprintf(outfile, "OBJECT\n");
		fprintf(outfile, "  TRIANGLE <%1.06f %1.06f %1.06f> <%1.06f %1.06f %1.06f> <%1.06f %1.06f %1.06f> END_TRIANGLE\n", xcoords[0], ycoords[0], zcoords[0], xcoords[1], ycoords[1], zcoords[1], xcoords[2], ycoords[2], zcoords[2]);
		finishobj(curcolor);

		if (checkdegen(0, 3, 2))
		{
			degenerates++;
			return;
		}

		fprintf(outfile, "OBJECT\n");
		fprintf(outfile, "  TRIANGLE <%1.06f %1.06f %1.06f> <%1.06f %1.06f %1.06f> <%1.06f %1.06f %1.06f> END_TRIANGLE\n", xcoords[0], ycoords[0], zcoords[0], xcoords[3], ycoords[3], zcoords[3], xcoords[2], ycoords[2], zcoords[2]);
		finishobj(curcolor);

		return;
	}
	else if (strstr(curobj, "SOLID"))	/* 1 or 2 triangles */
	{
		if (checkdegen(0, 1, 2))
		{
			degenerates++;
			return;
		}

		fprintf(outfile, "OBJECT\n");
		fprintf(outfile, "  TRIANGLE <%1.06f %1.06f %1.06f> <%1.06f %1.06f %1.06f> <%1.06f %1.06f %1.06f> END_TRIANGLE\n", xcoords[0], ycoords[0], zcoords[0], xcoords[1], ycoords[1], zcoords[1], xcoords[2], ycoords[2], zcoords[2]);
		finishobj(curcolor);

		if (xcoords[2] == xcoords[3] && ycoords[2] == ycoords[3] && zcoords[2] == zcoords[3])
			return;	/* one triangle was enough... */

		if (checkdegen(0, 3, 2))
		{
			degenerates++;
			return;
		}

		fprintf(outfile, "OBJECT\n");
		fprintf(outfile, "  TRIANGLE <%1.06f %1.06f %1.06f> <%1.06f %1.06f %1.06f> <%1.06f %1.06f %1.06f> END_TRIANGLE\n", xcoords[0], ycoords[0], zcoords[0], xcoords[3], ycoords[3], zcoords[3], xcoords[2], ycoords[2], zcoords[2]);
		finishobj(curcolor);
		return;
	}
	else if (strstr(curobj, "TEXT"))	/* not implemented for now */
	{
		return;
	}
	else if (strstr(curobj, "SHAPE"))	/* these look very hard */
	{
		return;
	}
	else if (strstr(curobj, "BLOCK"))	/* these look very hard */
	{
		return;
	}
	else if (strstr(curobj, "ENDBLK"))	/* these look very hard */
	{
		return;
	}
	else if (strstr(curobj, "INSERT"))	/* these look very hard */
	{
		return;
	}
	else if (strstr(curobj, "ATTDEF"))	/* not implemented for now */
	{
		return;
	}
	else if (strstr(curobj, "ATTRIB"))	/* not implemented for now */
	{
		return;
	}
	else if (strstr(curobj, "POLYLINE"))	/* these look fairly hard */
	{
		return;
	}
	else if (strstr(curobj, "VERTEX"))	/* these look fairly hard */
	{
		return;
	}
	else if (strstr(curobj, "SEQEND"))	/* these look fairly hard */
	{
		return;
	}
	else if (strstr(curobj, "3DLINE"))	/* a VERY skinny triangle! */
	{
		if (xcoords[0] == xcoords[1] && ycoords[0] == ycoords[1] && zcoords[0] == zcoords[1])
		{
			degenerates++;
			return;
		}

		fprintf(outfile, "OBJECT\n");
		fprintf(outfile, "  TRIANGLE <%1.06f %1.06f %1.06f> <%1.06f %1.06f %1.06f> <%1.06f %1.06f %1.06f> END_TRIANGLE\n", xcoords[0], ycoords[0], zcoords[0], xcoords[1], ycoords[1], zcoords[1], xcoords[1]+0.1, ycoords[1], zcoords[1]);
		finishobj(curcolor);

		return;
	}
	else if (strstr(curobj, "3DFACE"))	/* 1 or 2 triangles */
	{
		if (checkdegen(0, 1, 2))
		{
			degenerates++;
			return;
		}

		fprintf(outfile, "OBJECT\n");
		fprintf(outfile, "  TRIANGLE <%1.06f %1.06f %1.06f> <%1.06f %1.06f %1.06f> <%1.06f %1.06f %1.06f> END_TRIANGLE\n", xcoords[0], ycoords[0], zcoords[0], xcoords[1], ycoords[1], zcoords[1], xcoords[2], ycoords[2], zcoords[2]);
		finishobj(curcolor);

		if (xcoords[2] == xcoords[3] && ycoords[2] == ycoords[3] && zcoords[2] == zcoords[3])
			return;	/* one triangle was enough... */

		if (checkdegen(0, 3, 2))
		{
			degenerates++;
			return;
		}

		fprintf(outfile, "OBJECT\n");
		fprintf(outfile, "  TRIANGLE <%1.06f %1.06f %1.06f> <%1.06f %1.06f %1.06f> <%1.06f %1.06f %1.06f> END_TRIANGLE\n", xcoords[0], ycoords[0], zcoords[0], xcoords[3], ycoords[3], zcoords[3], xcoords[2], ycoords[2], zcoords[2]);
		finishobj(curcolor);
		return;
	}
	else if (strstr(curobj, "DIMENSION"))	/* not implemented for now */
	{
		return;
	}
	return;	/* no current object defined... */
}

void finishobj(color)	/* conclude a DKB-style object definition */
int color;
{
	float red, green, blue;

	lookupcolor(color, &red, &green, &blue);
	fprintf(outfile, "\n  COLOR RED %1.06f GREEN %1.06f BLUE %1.06f\n", red, green, blue);
	fprintf(outfile, "  TEXTURE Dull END_TEXTURE\n");  /* default surf. */
	fprintf(outfile, "END_OBJECT\n\n");
	printf(".");		/* activity echo (happy dots) */
	primitives++;		/* count another output file primitive */
}

void lookupcolor(color, red, green, blue) /* basic AutoCAD 9-color pallette */
int color;
float *red, *green, *blue;
{
	switch (color)
	{
		case 0:	/* black */
			*red = *green = *blue = 0.0;
			break;
		case 1: /* red */
			*red = 1.0;
			*blue = *green = 0.0;
			break;
		case 2: /* yellow */
			*red = *green = 1.0;
			*blue = 0.0;
			break;
		case 3:	/* green */
			*green = 1.0;
			*red = *blue = 0.0;
			break;
		case 4: /* cyan */
			*blue = *green = 1.0;
			*red = 0.0;
			break;
		case 5: /* blue */
			*blue = 1.0;
			*red = *green = 0.0;
			break;
		case 6: /* magenta */
			*blue = *red = 1.0;
			*green = 0.0;
			break;
		case 8:	/* dk. grey */
			*red = *green = *blue = 0.5;
			break;
		case 9: /* lt. grey */
			*red = *green = *blue = 0.75;
			break;
		case 7: /* white */
		default: /* make anything else white (?) */
			*red = *green = *blue = 1.0;
	}
	return;
}

int checkdegen(a, b, c)		/* check for degenerate triangle structure */
int a, b, c;
{
	if (
	(xcoords[a] == xcoords[b] &&
	 ycoords[a] == ycoords[b] &&
	 zcoords[a] == zcoords[b]) || 
	(xcoords[b] == xcoords[c] &&
	 ycoords[b] == ycoords[c] &&
	 zcoords[b] == zcoords[c]) || 
	(xcoords[a] == xcoords[c] &&
	 ycoords[a] == ycoords[c] &&
	 zcoords[a] == zcoords[c]))
		return(1);
	return(0);
}

