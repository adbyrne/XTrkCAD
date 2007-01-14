/*
 * $Header: /home/dmarkle/xtrkcad-fork-cvs/xtrkcad/app/wlib/mswlib/mswcolor.c,v 1.2 2007-01-14 08:43:32 m_fischer Exp $
 */

#include <windows.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <commdlg.h>
#include <math.h>
#include <windows.h>

#include "mswint.h"

#include "square10.bmp"

/*
 *****************************************************************************
 *
 * Color
 *
 *****************************************************************************
 */

#define NUM_GRAYS (16)
#define NUM_COLORS (256)

wDrawColor wDrawColorWhite = 0;
wDrawColor wDrawColorBlack = 1;

#define MAX_COLOR_DISTANCE (3)

static void mswGetCustomColors( void );


static struct {
		WORD palVersion;
		WORD palNumEntries;
		PALETTEENTRY palPalEntry[NUM_COLORS];
		} colorPalette = {
		0x300,
		2,
		{
		{ 255, 255, 255 },		/* White */
		{	0,	 0,	  0 }		/* Black */
		} };

COLORREF mappedColors[NUM_COLORS];


static long flipRGB( long rgb )
{
	rgb = ((rgb>>16)&0xFF) | (rgb&0x00FF00) | ((rgb&0xFF)<<16);
	return rgb;
}


static void getpalette( void )
{

	HDC hdc;
	int inx, cnt;
	PALETTEENTRY pe[256];
	FILE * f;
	hdc = GetDC(mswHWnd);
	if (!(GetDeviceCaps( hdc, RASTERCAPS) & RC_PALETTE)) {
		ReleaseDC( mswHWnd, hdc );
		return;
	}
	cnt = GetDeviceCaps(hdc, SIZEPALETTE);
	GetSystemPaletteEntries( hdc, 0, cnt, pe );
	f = fopen( "palette.txt", "w" );
	for (inx=0;inx<cnt;inx++)
		fprintf(f, "%d [ %d %d %d %d ]\n", inx, pe[inx].peRed, pe[inx].peGreen, pe[inx].peBlue, pe[inx].peFlags );
	fclose(f);
	ReleaseDC( mswHWnd, hdc );
}


static int findColor( int r0, int g0, int b0 )
{
	int r1, g1, b1;
	int d0, d1;
	int c, cc;
	PALETTEENTRY *pal;

	pal = colorPalette.palPalEntry;
	cc = (int)wDrawColorBlack;
	d0 = 256*3;

	for ( c = 0; c < (int)colorPalette.palNumEntries; c++ ) {
		r1 = pal[c].peRed;
		b1 = pal[c].peBlue;
		g1 = pal[c].peGreen;
		d1 = abs(r0-r1) + abs(g0-g1) + abs(b0-b1);
		if (d1 == 0)
			return c;
		if (d1 < d0) {
			d0 = d1;
			cc = c;
		}
	}
	if ( colorPalette.palNumEntries < 128 ) {
		pal[colorPalette.palNumEntries].peRed = r0;
		pal[colorPalette.palNumEntries].peGreen = g0;
		pal[colorPalette.palNumEntries].peBlue = b0;
		if ( mswPalette ) {
			ResizePalette( mswPalette, colorPalette.palNumEntries+1 );
			SetPaletteEntries( mswPalette, colorPalette.palNumEntries, 1, &pal[colorPalette.palNumEntries] );
		}
		return colorPalette.palNumEntries++;
	}
	return cc;
}


int mswGetPaletteClock( void )
{
	return colorPalette.palNumEntries;
}


void mswInitColorPalette( void )
{
	static int initted = FALSE;
	HDC hDc;
	int cnt;
	int rc;
	static struct {
		WORD palVersion;
		WORD palNumEntries;
		PALETTEENTRY palPalEntry[256];
	} pe;

	if (initted)
		return;

	initted = TRUE;
	mswGetCustomColors();
	mswFontInit();
	hDc = GetDC(mswHWnd);
	if (!(GetDeviceCaps( hDc, RASTERCAPS) & RC_PALETTE)) {
		ReleaseDC( mswHWnd, hDc );
		return;
	}
	cnt = GetDeviceCaps(hDc, SIZEPALETTE);
	rc = GetSystemPaletteEntries( hDc, 0, cnt, pe.palPalEntry );
	mswPalette = CreatePalette( (const LOGPALETTE FAR*)&colorPalette );
	ReleaseDC( mswHWnd, hDc );

}


HPALETTE mswCreatePalette( void )
{
	return CreatePalette( (const LOGPALETTE FAR*)&colorPalette );
}


int mswGetColorList( RGBQUAD * colors )
{
	int i;
	for (i=0;i<(int)colorPalette.palNumEntries;i++) {
		colors[i].rgbBlue = colorPalette.palPalEntry[i].peBlue;
		colors[i].rgbGreen = colorPalette.palPalEntry[i].peGreen;
		colors[i].rgbRed = colorPalette.palPalEntry[i].peRed;
		colors[i].rgbReserved = 0;
	}
	return NUM_COLORS;
}


COLORREF mswGetColor( wBool_t hasPalette, wDrawColor color )
{
	if ( hasPalette )
		return PALETTEINDEX(color);
	else
		return RGB( colorPalette.palPalEntry[color].peRed,	colorPalette.palPalEntry[color].peGreen,  colorPalette.palPalEntry[color].peBlue );
}


wDrawColor wDrawColorGray(
		int percent )
{
	int n;
	n = (percent * NUM_GRAYS) / 100;
	if ( n <= 0 )
		return wDrawColorBlack;
	else if ( n > NUM_GRAYS )
		return wDrawColorWhite;
	else {
		n = (n*256)/NUM_GRAYS;
		return wDrawFindColor( wRGB(n,n,n) );
	}
}

wDrawColor wDrawFindColor(
		long rgb0 )
{
	static long saved_rgb = wRGB(255,255,255);
	static wDrawColor saved_color = 0;
	int r0, g0, b0;

	if (rgb0 == saved_rgb)
		return saved_color;
	r0 = (int)(rgb0>>16)&0xFF;
	g0 = (int)(rgb0>>8)&0xFF;
	b0 = (int)(rgb0)&0xFF;
	saved_rgb = rgb0;
	return saved_color = findColor( r0, g0, b0 );
}


long wDrawGetRGB(
		wDrawColor color )
{
	long rgb;
	int r, g, b;
	r = colorPalette.palPalEntry[color].peRed;
	g = colorPalette.palPalEntry[color].peGreen;
	b = colorPalette.palPalEntry[color].peBlue;
	rgb = wRGB(r,g,b);
	return rgb;
}


static CHOOSECOLOR chooseColor;
static COLORREF aclrCust[16];

static void mswGetCustomColors( void )
{
	int inx;
	char colorName[10];
	long rgb;

	strcpy( colorName, "custom-" );
	for ( inx=0; inx<16; inx++ ) {
		sprintf( colorName+7, "%d", inx );
		wPrefGetInteger( "mswcolor", colorName, &rgb, 0 );
		aclrCust[inx] = flipRGB(rgb);
	}
}


void mswPutCustomColors( void )
{
	int inx;
	char colorName[10];
	long rgb;

	strcpy( colorName, "custom-" );
	for ( inx=0; inx<16; inx++ ) {
		rgb = flipRGB(aclrCust[inx]);
		if ( rgb != 0 ) {
			sprintf( colorName+7, "%d", inx );
			wPrefSetInteger( "mswcolor", colorName, rgb );
		}
	}
}


wBool_t wColorSelect(
		const char * title,
		wDrawColor * color )
{
	long rgb;

	memset( &chooseColor, 0, sizeof chooseColor );
	rgb = flipRGB(wDrawGetRGB(*color));
	chooseColor.lStructSize = sizeof chooseColor;
	chooseColor.hwndOwner = mswHWnd;
	chooseColor.hInstance = NULL;
	chooseColor.rgbResult = rgb;
	chooseColor.lpCustColors = aclrCust;
	chooseColor.Flags = CC_RGBINIT;
	chooseColor.lCustData = 0L;
	chooseColor.lpfnHook = NULL;
	chooseColor.lpTemplateName = (LPSTR)NULL;
	if ( ChooseColor( &chooseColor ) ) {
		rgb = flipRGB(chooseColor.rgbResult);
		*color = wDrawFindColor(rgb);
		return TRUE;
	}
	return FALSE;
}


typedef struct {
		wDrawColor * valueP;
		wColorSelectButtonCallBack_p action;
		const char * labelStr;
		void * data;
		wDrawColor color;
		wButton_p button;
		wIcon_p bm;
		} colorData_t;


static void doColorButton(
		void * data )
{
	colorData_t * cd = (colorData_t*)data;
	wDrawColor newColor;

	newColor = cd->color;
	if (wColorSelect( cd->labelStr, &newColor )) {
		cd->color = newColor;
		wColorSelectButtonSetColor( cd->button, newColor );
		if (cd->valueP)
			*cd->valueP = newColor;
		if (cd->action)
			cd->action( cd->data, newColor );
	}
}


wButton_p wColorSelectButtonCreate(
		wWin_p win,
		wPos_t x,
		wPos_t y,
		const char * helpStr,
		const char * labelStr,
		long option,
		wPos_t width,
		wDrawColor * color,
		wColorSelectButtonCallBack_p action,
		void * data )
{
	wButton_p bb;
	wIcon_p bm;
	colorData_t * cd;
	bm = wIconCreateBitMap( square10_width, square10_height, square10_bits, (color?*color:0) );
	cd = malloc( sizeof *cd );
	cd->valueP = color;
	cd->action = action;
	cd->data = data;
	cd->labelStr = labelStr;
	cd->color = (color?*color:0);
	cd->bm = bm;
	bb = wButtonCreate( win, x, y, helpStr, (char*)bm, option|BO_ICON, width, doColorButton, cd );
	cd->button = bb;
	if ( labelStr )
				wControlSetLabel( (wControl_p)bb, labelStr );
	return bb;
}


void wColorSelectButtonSetColor(
		wButton_p bb,
		wDrawColor color )
{
	((colorData_t*)((wControl_p)bb)->data)->color = color;
	wIconSetColor( ((colorData_t*)((wControl_p)bb)->data)->bm, color );
	InvalidateRect( ((wControl_p)bb)->hWnd, NULL, TRUE );
}


wDrawColor wColorSelectButtonGetColor(
		wButton_p bb )
{
	return ((colorData_t*)((wControl_p)bb)->data)->color;
}
