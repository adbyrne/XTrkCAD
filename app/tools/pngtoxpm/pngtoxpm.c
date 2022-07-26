
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <math.h>
#include <string.h>

#include <assert.h>

#include <FreeImage.h>

// #define DEBUGPRINT

//#if defined(WIN32) || defined(_WIN32) 
//#define PATH_SEPARATOR '\\' 
//#else 
//#define PATH_SEPARATOR '/'
//#endif 

#define CHATTY 0 /* 1 = Enable progress messages */
#define ALPHATHRESH  96
#define BACKGROUND   208
#define BGMASK       0xF8
#define NUMPALETTE   40


FIBITMAP    *image;

/* Note that percent does not print so omit that char from the list! */
char palette[76] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz~!@#$^&=+-<>/";

char xpmBuff[10000];

/**********************************************************************
C Implementation of Wu's Color Quantizer (v. 2)
(see Graphics Gems vol. II, pp. 126-133)
Author:	Xiaolin Wu
Dept. of Computer Science
Univ. of Western Ontario
London, Ontario N6A 5B7
wu@csd.uwo.ca
Algorithm: Greedy orthogonal bipartition of RGB space for variance
minimization aided by inclusion-exclusion tricks.
For speed no nearest neighbor search is done. Slightly
better performance can be expected by more sophisticated
but more expensive versions.
The author thanks Tom Lane at Tom_Lane@G.GP.CS.CMU.EDU for much of
additional documentation and a cure to a previous bug.

Free to distribute, comments and suggestions are appreciated.

Code from https://gist.github.com/bert/1192520
**********************************************************************/	

#define MAXCOLOR	256
#define	RED	    2
#define	GREEN	1
#define BLUE	0

static float		m2[33][33][33];
static long int	wt[33][33][33], mr[33][33][33],	mg[33][33][33],	mb[33][33][33];
unsigned char   *Ir, *Ig, *Ib, *Ia;
int	        size;    /*image size*/
int		    K;       /*color look-up table size*/
unsigned short int *Qadd;
unsigned char	lut_r[MAXCOLOR], lut_g[MAXCOLOR], lut_b[MAXCOLOR];

struct box {
	int r0;			 /* min value, exclusive */
	int r1;			 /* max value, inclusive */
	int g0;  
	int g1;  
	int b0;  
	int b1;
	int vol;
};

/* Histogram is in elements 1..HISTSIZE along each axis,
* element 0 is for base or marginal value
* NB: these must start out 0!
*/

/* build 3-D color histogram of counts, r/g/b, c^2 */
void Hist3d(long *vwt, long *vmr, long *vmg, long *vmb, float *m2) 
{
	register int ind, r, g, b;
	int	     inr, ing, inb, table[256];
	register long int i;

	for(i=0; i<256; ++i) table[i]=i*i;
	Qadd = (unsigned short int *)malloc(sizeof(short int)*size);
	if (Qadd==NULL) {fprintf(stderr, "Not enough space\n"); exit( 1 ); }
	for(i=0; i<size; ++i){
		r = Ir[i]; g = Ig[i]; b = Ib[i];
		inr=(r>>3)+1; 
		ing=(g>>3)+1; 
		inb=(b>>3)+1; 
		Qadd[i]=ind=(inr<<10)+(inr<<6)+inr+(ing<<5)+ing+inb;
		/*[inr][ing][inb]*/
		++vwt[ind];
		vmr[ind] += r;
		vmg[ind] += g;
		vmb[ind] += b;
		m2[ind] += (float)(table[r]+table[g]+table[b]);
	}
}

/* At conclusion of the histogram step, we can interpret
*   wt[r][g][b] = sum over voxel of P(c)
*   mr[r][g][b] = sum over voxel of r*P(c)  ,  similarly for mg, mb
*   m2[r][g][b] = sum over voxel of c^2*P(c)
* Actually each of these should be divided by 'size' to give the usual
* interpretation of P() as ranging from 0 to 1, but we needn't do that here.
*/

/* We now convert histogram into moments so that we can rapidly calculate
* the sums of the above quantities over any desired box.
*/

/* compute cumulative moments. */
void M3d(long *vwt, long *vmr, long *vmg, long *vmb, float *m2) 
{
	register unsigned short int ind1, ind2;
	register unsigned char i, r, g, b;
	long int line, line_r, line_g, line_b,
		area[33], area_r[33], area_g[33], area_b[33];
	float    line2, area2[33];

	for(r=1; r<=32; ++r){
		for(i=0; i<=32; ++i) 
			area2[i]=area[i]=area_r[i]=area_g[i]=area_b[i]=0;
		for(g=1; g<=32; ++g){
			line2 = line = line_r = line_g = line_b = 0;
			for(b=1; b<=32; ++b){
				ind1 = (r<<10) + (r<<6) + r + (g<<5) + g + b; /* [r][g][b] */
				line += vwt[ind1];
				line_r += vmr[ind1]; 
				line_g += vmg[ind1]; 
				line_b += vmb[ind1];
				line2 += m2[ind1];
				area[b] += line;
				area_r[b] += line_r;
				area_g[b] += line_g;
				area_b[b] += line_b;
				area2[b] += line2;
				ind2 = ind1 - 1089; /* [r-1][g][b] */
				vwt[ind1] = vwt[ind2] + area[b];
				vmr[ind1] = vmr[ind2] + area_r[b];
				vmg[ind1] = vmg[ind2] + area_g[b];
				vmb[ind1] = vmb[ind2] + area_b[b];
				m2[ind1] = m2[ind2] + area2[b];
			}
		}
	}
}


/* Compute sum over a box of any given statistic */
long int Vol(struct box *cube, long int mmt[33][33][33]) 
{
	return( mmt[cube->r1][cube->g1][cube->b1] 
		-mmt[cube->r1][cube->g1][cube->b0]
		-mmt[cube->r1][cube->g0][cube->b1]
		+mmt[cube->r1][cube->g0][cube->b0]
		-mmt[cube->r0][cube->g1][cube->b1]
		+mmt[cube->r0][cube->g1][cube->b0]
		+mmt[cube->r0][cube->g0][cube->b1]
		-mmt[cube->r0][cube->g0][cube->b0] );
}

/* The next two routines allow a slightly more efficient calculation
* of Vol() for a proposed subbox of a given box.  The sum of Top()
* and Bottom() is the Vol() of a subbox split in the given direction
* and with the specified new upper bound.
*/

/* Compute part of Vol(cube, mmt) that doesn't depend on r1, g1, or b1 */
/* (depending on dir) */
long int Bottom(struct box *cube, int dir, long int mmt[33][33][33])
{
	switch(dir){
	case RED:
		return( -mmt[cube->r0][cube->g1][cube->b1]
			+mmt[cube->r0][cube->g1][cube->b0]
			+mmt[cube->r0][cube->g0][cube->b1]
			-mmt[cube->r0][cube->g0][cube->b0] );
		break;
	case GREEN:
		return( -mmt[cube->r1][cube->g0][cube->b1]
			+mmt[cube->r1][cube->g0][cube->b0]
			+mmt[cube->r0][cube->g0][cube->b1]
			-mmt[cube->r0][cube->g0][cube->b0] );
		break;
	case BLUE:
		return( -mmt[cube->r1][cube->g1][cube->b0]
			+mmt[cube->r1][cube->g0][cube->b0]
			+mmt[cube->r0][cube->g1][cube->b0]
			-mmt[cube->r0][cube->g0][cube->b0] );
		break;
	}
	fprintf(stderr, "Bottom: Invalid dir.\n"); 
	exit( 1 );
}


long int Top(struct box *cube, int dir, int pos, long int mmt[33][33][33])
/* Compute remainder of Vol(cube, mmt), substituting pos for */
/* r1, g1, or b1 (depending on dir) */
{
	switch(dir){
	case RED:
		return( mmt[pos][cube->g1][cube->b1] 
			-mmt[pos][cube->g1][cube->b0]
			-mmt[pos][cube->g0][cube->b1]
			+mmt[pos][cube->g0][cube->b0] );
		break;
	case GREEN:
		return( mmt[cube->r1][pos][cube->b1] 
			-mmt[cube->r1][pos][cube->b0]
			-mmt[cube->r0][pos][cube->b1]
			+mmt[cube->r0][pos][cube->b0] );
		break;
	case BLUE:
		return( mmt[cube->r1][cube->g1][pos]
			-mmt[cube->r1][cube->g0][pos]
			-mmt[cube->r0][cube->g1][pos]
			+mmt[cube->r0][cube->g0][pos] );
		break;
	}
	fprintf(stderr, "Top: Invalid dir.\n"); 
	exit( 1 );
}


/* Compute the weighted variance of a box */
/* NB: as with the raw statistics, this is really the variance * size */
float Var(struct box *cube)
{
	float dr, dg, db, xx;

	dr = Vol(cube, mr); 
	dg = Vol(cube, mg); 
	db = Vol(cube, mb);
	xx =  m2[cube->r1][cube->g1][cube->b1] 
		-m2[cube->r1][cube->g1][cube->b0]
		-m2[cube->r1][cube->g0][cube->b1]
		+m2[cube->r1][cube->g0][cube->b0]
		-m2[cube->r0][cube->g1][cube->b1]
		+m2[cube->r0][cube->g1][cube->b0]
		+m2[cube->r0][cube->g0][cube->b1]
		-m2[cube->r0][cube->g0][cube->b0];

	return( xx - (dr*dr+dg*dg+db*db)/(float)Vol(cube,wt) );    
}

/* We want to minimize the sum of the variances of two subboxes.
* The sum(c^2) terms can be ignored since their sum over both subboxes
* is the same (the sum for the whole box) no matter where we split.
* The remaining terms have a minus sign in the variance formula,
* so we drop the minus sign and MAXIMIZE the sum of the two terms.
*/

float Maximize(struct box *cube, int dir, int first, int last, int *cut,
	long whole_r, long whole_g, long whole_b, long whole_w)
{
	register long int half_r, half_g, half_b, half_w;
	long int base_r, base_g, base_b, base_w;
	register int i;
	register float temp, max;

	base_r = Bottom(cube, dir, mr);
	base_g = Bottom(cube, dir, mg);
	base_b = Bottom(cube, dir, mb);
	base_w = Bottom(cube, dir, wt);
	max = 0.0;
	*cut = -1;
	for(i=first; i<last; ++i){
		half_r = base_r + Top(cube, dir, i, mr);
		half_g = base_g + Top(cube, dir, i, mg);
		half_b = base_b + Top(cube, dir, i, mb);
		half_w = base_w + Top(cube, dir, i, wt);
		/* now half_x is sum over lower half of box, if split at i */
		if (half_w == 0) {      /* subbox could be empty of pixels! */
			continue;             /* never split into an empty box */
		} else
			temp = ((float)half_r*half_r + (float)half_g*half_g +
				(float)half_b*half_b)/half_w;

		half_r = whole_r - half_r;
		half_g = whole_g - half_g;
		half_b = whole_b - half_b;
		half_w = whole_w - half_w;
		if (half_w == 0) {      /* subbox could be empty of pixels! */
			continue;             /* never split into an empty box */
		} else
			temp += ((float)half_r*half_r + (float)half_g*half_g +
				(float)half_b*half_b)/half_w;

		if (temp > max) {max=temp; *cut=i;}
	}
	return(max);
}

int Cut(struct box *set1, struct box *set2)
{
	int dir;
	int cutr, cutg, cutb;
	float maxr, maxg, maxb;
	long int whole_r, whole_g, whole_b, whole_w;

	whole_r = Vol(set1, mr);
	whole_g = Vol(set1, mg);
	whole_b = Vol(set1, mb);
	whole_w = Vol(set1, wt);

	maxr = Maximize(set1, RED, set1->r0+1, set1->r1, &cutr,
		whole_r, whole_g, whole_b, whole_w);
	maxg = Maximize(set1, GREEN, set1->g0+1, set1->g1, &cutg,
		whole_r, whole_g, whole_b, whole_w);
	maxb = Maximize(set1, BLUE, set1->b0+1, set1->b1, &cutb,
		whole_r, whole_g, whole_b, whole_w);

	if( (maxr>=maxg)&&(maxr>=maxb) ) {
		dir = RED;
		if (cutr < 0) return 0; /* can't split the box */
	}
	else
		if( (maxg>=maxr)&&(maxg>=maxb) ) 
			dir = GREEN;
		else
			dir = BLUE; 

	set2->r1 = set1->r1;
	set2->g1 = set1->g1;
	set2->b1 = set1->b1;

	switch (dir){
	case RED:
		set2->r0 = set1->r1 = cutr;
		set2->g0 = set1->g0;
		set2->b0 = set1->b0;
		break;
	case GREEN:
		set2->g0 = set1->g1 = cutg;
		set2->r0 = set1->r0;
		set2->b0 = set1->b0;
		break;
	case BLUE:
		set2->b0 = set1->b1 = cutb;
		set2->r0 = set1->r0;
		set2->g0 = set1->g0;
		break;
	}
	set1->vol=(set1->r1-set1->r0)*(set1->g1-set1->g0)*(set1->b1-set1->b0);
	set2->vol=(set2->r1-set2->r0)*(set2->g1-set2->g0)*(set2->b1-set2->b0);
	return 1;
}


void Mark(struct box *cube, int label, unsigned char *tag)
{
	register int r, g, b;

	for(r=cube->r0+1; r<=cube->r1; ++r)
		for(g=cube->g0+1; g<=cube->g1; ++g)
			for(b=cube->b0+1; b<=cube->b1; ++b)
				tag[(r<<10) + (r<<6) + r + (g<<5) + g + b] = label;
}


void cq ()
{
	struct box	cube[MAXCOLOR];
	unsigned char	*tag;
	int		next;
	register long int	i, weight;
	register int	k;
	float		vv[MAXCOLOR], temp;

	K = NUMPALETTE;

	Hist3d((long *)wt, (long *)mr, (long *)mg, (long *)mb, (float *)m2); 
	if (CHATTY) printf("Histogram done\n");

	M3d((long *)wt, (long *)mr, (long *)mg, (long *)mb, (float *)m2);    
	if (CHATTY) printf("Moments done\n");

	cube[0].r0 = cube[0].g0 = cube[0].b0 = 0;
	cube[0].r1 = cube[0].g1 = cube[0].b1 = 32;
	next = 0;
	for(i=1; i<K; ++i){
		if (Cut(&cube[next], &cube[i])) {
			/* volume test ensures we won't try to cut one-cell box */
			vv[next] = (cube[next].vol>1) ? Var(&cube[next]) : 0.0;
			vv[i] = (cube[i].vol>1) ? Var(&cube[i]) : 0.0;
		} else {
			vv[next] = 0.0;   /* don't try to split this box again */
			i--;              /* didn't create box i */
		}
		next = 0; temp = vv[0];
		for(k=1; k<=i; ++k)
			if (vv[k] > temp) {
				temp = vv[k]; next = k;
			}
		if (temp <= 0.0) {
			K = i+1;
			if (CHATTY) printf("Only got %d boxes\n", K);
			break;
		}
	}
	if (CHATTY) printf("Partition done\n");

	tag = (unsigned char *)malloc(33*33*33);
	if (tag==NULL) {fprintf(stderr, "Not enough space\n"); exit( 1 ); }
	for(k=0; k<K; ++k){
		Mark(&cube[k], k, tag);
		weight = Vol(&cube[k], wt);
		if (weight) {
			lut_r[k] = Vol(&cube[k], mr) / weight;
			lut_g[k] = Vol(&cube[k], mg) / weight;
			lut_b[k] = Vol(&cube[k], mb) / weight;
		}
		else{
			fprintf(stderr, "bogus box %d\n", k);
			lut_r[k] = lut_g[k] = lut_b[k] = 0;		
		}
	}

	for(i=0; i<size; ++i) Qadd[i] = tag[Qadd[i]];

	free( tag );
}

char pChar( int j ){

	char c = '.';
	c = palette[j];
	return c;
}

/* Safe version of strcat - will not overflow buffer */
size_t
strscat(char* dest, const char* src, size_t count)
{
	long sptr = 0;
	long dptr = strlen(dest);
	count -= dptr;

	if (count <= 0)
		return -E2BIG;

	while (count) {
		char c;

		c = src[sptr];
		dest[dptr] = c;
		if (!c)
			return dptr;
		sptr++;
		dptr++;
		count--;
	}

	/* Hit buffer length without finding a NUL; force NUL-termination. */
	if (dptr)
		dest[dptr - 1] = '\0';

	return -E2BIG;
}

int genXpm ( int icon, char* name, int width, int height ) {
	char xpmCode[100]; // XPM object name
	char tmpBuff[100]; // sprintf
	char c[2];
	int i, j;
	int x, y;
	RGBQUAD color;
	c[1] = 0;

	strcpy(xpmCode, name);
	for ( i = 0; i < strlen( xpmCode ); i++ ){
		if(xpmCode[i] == '-') 
			xpmCode[i] = '_';
	}
	
	strscat(xpmBuff, "static char *", sizeof(xpmBuff));
	strscat(xpmBuff, xpmCode, sizeof(xpmBuff));
	strscat(xpmBuff, "_x", sizeof(xpmBuff));
	sprintf(tmpBuff, "%d", height); strscat(xpmBuff, tmpBuff, sizeof(xpmBuff));
	strscat(xpmBuff, "[] = {\n", sizeof(xpmBuff));
	strscat(xpmBuff, "\t\"", sizeof(xpmBuff));
	sprintf(tmpBuff, "%d %d %d %d", width, height, (K+1), 1); strscat(xpmBuff, tmpBuff, sizeof(xpmBuff));
	strscat(xpmBuff, "\",\n\t\" \tc\tNone\",\n", sizeof(xpmBuff));
	for (i = 0; i < K; i++)
	{
		strscat(xpmBuff, "\t\"", sizeof(xpmBuff));
		sprintf(tmpBuff, "%c\tc\t#%02x%02x%02x", pChar(i), lut_r[i],lut_g[i],lut_b[i]); strscat(xpmBuff, tmpBuff, sizeof(xpmBuff));
		strscat(xpmBuff, "\",\n", sizeof(xpmBuff));
	}

	// Write the pixels
	i = 0;
	for ( y = height-1; y >= 0; y-- ){
		strscat(xpmBuff, "\t\"", sizeof(xpmBuff));
		for ( x = 0; x < width; x++ ){
			FreeImage_GetPixelColor( image, x, y, &color );
			if (color.rgbReserved >= ALPHATHRESH ){
				j = Qadd[i];
				c[0] = pChar(j);
				strscat(xpmBuff, c, sizeof(xpmBuff));
				i++;
			}
			else
			{
				strscat(xpmBuff, " ", sizeof(xpmBuff));
			}
		}

		if (y > 0)
		{
			strscat(xpmBuff, "\",\n", sizeof(xpmBuff));
		}
		else
		{
			strscat(xpmBuff, "\"};\n", sizeof(xpmBuff));
		}
	}

	if (icon == 32)
	{
		strscat(xpmBuff, "\nstatic char **", sizeof(xpmBuff));
		strscat(xpmBuff, xpmCode, sizeof(xpmBuff));
		strscat(xpmBuff, "_xpm[3] = { ", sizeof(xpmBuff));
		strscat(xpmBuff, xpmCode, sizeof(xpmBuff));
		strscat(xpmBuff, "_x16, ", sizeof(xpmBuff));
		strscat(xpmBuff, xpmCode, sizeof(xpmBuff));
		strscat(xpmBuff, "_x24, ", sizeof(xpmBuff));
		strscat(xpmBuff, xpmCode, sizeof(xpmBuff));
		strscat(xpmBuff, "_x32 };\n", sizeof(xpmBuff));
	}
	return 0;
}

int process( char* path, char* name, int icon ){
	int i;
	int w, h;
	int x, y;
	short a;
	float frac;
	short bg;
	char filename[1000];
	RGBQUAD color;


	memset(m2, 0, sizeof(m2));
	memset(wt, 0, sizeof(wt));
	memset(mr, 0, sizeof(mr));
	memset(mg, 0, sizeof(mg));
	memset(mb, 0, sizeof(mb));

	/* printf( "FreeImage version %s\n\n",FreeImage_GetVersion( ) ); */

	sprintf( filename,"%s/png/%s%d.png",path,name,icon );
#ifdef DEBUGPRINT
	fprintf(stdout, "PNG: %s\n", filename );
#endif

	image = FreeImage_Load(FIF_PNG, filename, PNG_DEFAULT);
	if ( image == NULL ){
		fprintf(stderr, "%s not found.\n", filename);
		exit( 1 );
	}

	if (FreeImage_GetBPP(image) != 32) 
	{
		FIBITMAP* tempImage = image;
		image = FreeImage_ConvertTo32Bits(tempImage);
	}

	w = FreeImage_GetWidth( image );
	h = FreeImage_GetHeight( image );

	// Count non-transparent pixels - this is the "size" of the image
	size = 0;
	for ( y = 0; y < h; y++ ){
		for ( x = 0; x < w; x++ ){
			FreeImage_GetPixelColor( image, x, y, &color );
			a = color.rgbReserved;
			if ( a >= ALPHATHRESH ){
				size++;
			}
		}
	}

	/* input R,G,B components into Ir, Ig, Ib */
	Ir = (unsigned char *)malloc(size);
	Ig = (unsigned char *)malloc(size);
	Ib = (unsigned char *)malloc(size);
	assert(Ir && Ig && Ib);
	i = 0;
	for ( y = h-1; y >= 0; y-- ){
		for ( x = 0; x < w; x++ ){
			FreeImage_GetPixelColor( image, x, y, &color );
			a = color.rgbReserved;
			if ( a >= ALPHATHRESH ){
				frac = (float)a / 255;
				bg = BACKGROUND * (1.0f - frac);
				Ir[i] = (unsigned char)min(255, color.rgbRed * frac + bg) & BGMASK;
				Ig[i] = (unsigned char)min(255, color.rgbGreen * frac + bg) & BGMASK;
				Ib[i] = (unsigned char)min(255, color.rgbBlue * frac + bg) & BGMASK;
				i++;
			}
		}
	}

	cq( );

	free( Ig ); free( Ib ); free( Ir ); /* */

	genXpm( icon, name, w, h );

	// Delete
	FreeImage_Unload(image);

	return 0;
}

int main( int argc, char *argv[] )
{
	char buffer[1000];
	char path[1000];
	char name[250];
	char *temp;
	char *ext;
	int i = 0, j = 0;
	int icon;

#ifdef DEBUGPRINT
	fprintf(stderr, "Begin pngtoxpm\n");
#endif

	if( argc < 2 ){
		printf("PngToXpm ver 0.2\nUsage: pngtoxpm filename\nfilename is the path to the resultant XPM\n");
		return 0;
	}

	// Get the file base name from path/name.ext
	strncpy( buffer, argv[1], sizeof(buffer) - 1);

	strncpy( path, argv[1], sizeof(path) - 1);
#ifdef DEBUGPRINT
	fprintf(stderr, "Path: %s\n", path);
#endif

	temp = strrchr( path, '/' );
	if (temp != NULL){
		temp++;
		*temp = '\0';
	}
	else {
		path[0] = '\0';
	}

#ifdef DEBUGPRINT
	fprintf(stderr, "Path: %s\n", path);
#endif

	(temp = strrchr(buffer, '/')) ? ++temp : (temp = buffer);

	ext = strrchr(temp, '.');
	if (ext != NULL) 
		*ext = '\0';
	strncpy(name, temp, sizeof(name) - 1);

#ifdef DEBUGPRINT
	fprintf(stdout, "In: %s %s ", path, name );
#endif


	for (icon=16; icon<=32; icon+=8)
	{
		process( path, name, icon );
	}

	// Write the xpm file
	strncpy(buffer, argv[1], sizeof(buffer) - 1);
#ifdef DEBUGPRINT
	fprintf(stdout, "XPM: %s\n", buffer );
#endif

	FILE* ptr; 
	ptr = fopen(buffer, "w");
	if ( ptr == NULL ){
		fprintf(stderr, "xpm file could not be created.\n");
		exit( 1 );
	}
	fprintf( ptr, xpmBuff );
	fclose( ptr );

	return 0;
}

