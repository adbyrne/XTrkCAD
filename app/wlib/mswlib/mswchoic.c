#include <windows.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <commdlg.h>
#include <math.h>
#include "i18n.h"
#include "mswint.h"

/*
 *****************************************************************************
 *
 * Choice Boxes
 *
 *****************************************************************************
 */

int CHOICE_HEIGHT=(17);
int CHOICE_MIN_WIDTH=25;

static XWNDPROC oldChoiceItemProc = NULL;
static XWNDPROC newChoiceItemProc;

typedef struct {
		WOBJ_COMMON
		wChoice_p owner;
		} wChoiceItem_t, * wChoiceItem_p;

struct wChoice_t {
		WOBJ_COMMON
		const char * * labels;
		wChoiceItem_p *buttList;
		long *valueP;
		long oldVal;
		wChoiceCallBack_p action;
		HWND hBorder;
		};

static FARPROC oldChoiceProc;

void wRadioSetValue(
		wChoice_p bc,
		long val )
{
	const char ** labels;
	long cnt;
	wChoiceItem_p * butts;

	butts = (wChoiceItem_p*)bc->buttList;
	for (labels = bc->labels, cnt=0; *labels; labels++, cnt++, butts++ )
		SendMessage( (*butts)->hWnd, BM_SETCHECK,
				(val==cnt)?1:0, 0L );
	bc->oldVal = val;
	if (bc->valueP)
		*bc->valueP = val;
}

long wRadioGetValue(
		wChoice_p bc )
{
	return bc->oldVal;
}



void wToggleSetValue(
		wChoice_p bc,
		long val )
{
	const char ** labels;
	long cnt;
	wChoiceItem_p * butts;

	butts = (wChoiceItem_p*)bc->buttList;
	for (labels = bc->labels, cnt=0; *labels; labels++, cnt++, butts++ )
		SendMessage( (*butts)->hWnd, BM_SETCHECK,
				(val & (1L<<cnt)) != 0, 0L );
	bc->oldVal = val;
	if (bc->valueP)
		*bc->valueP = val;
}


long wToggleGetValue(
		wChoice_p bc )
{
	return bc->oldVal;
}


static void choiceSetBusy(
		wControl_p b,
		BOOL_T busy)
{
	wChoiceItem_p * butts;
	wChoice_p bc = (wChoice_p)b;

	for (butts = (wChoiceItem_p*)bc->buttList; *butts; butts++ )
		EnableWindow( (*butts)->hWnd, !(BOOL)busy );
}

static void choiceShow(
		wControl_p b,
		BOOL_T show)
{
	wChoice_p bc = (wChoice_p)b;
	wChoiceItem_p * butts;

	if ((bc->option & BC_NOBORDER)==0)
		ShowWindow( bc->hBorder, show?SW_SHOW:SW_HIDE );

	for (butts = (wChoiceItem_p*)bc->buttList; *butts; butts++ )
		ShowWindow( (*butts)->hWnd, show?SW_SHOW:SW_HIDE );
}

static void choiceSetPos(
		wControl_p b,
		wPos_t x,
		wPos_t y )
{
	wChoice_p bc = (wChoice_p)b;
	wChoiceItem_p * butts;
	wPos_t dx, dy;

	dx = x - bc->x;
	dy = y - bc->y;
	if ((bc->option & BC_NOBORDER)==0)
		SetWindowPos( bc->hBorder, HWND_TOP, x, y, CW_USEDEFAULT, CW_USEDEFAULT,
				SWP_NOSIZE|SWP_NOZORDER );

	for (butts = (wChoiceItem_p*)bc->buttList; *butts; butts++ ) {
		SetWindowPos( (*butts)->hWnd, HWND_TOP,
						(*butts)->x+=dx, (*butts)->y+=dy,
						CW_USEDEFAULT, CW_USEDEFAULT,
						SWP_NOSIZE|SWP_NOZORDER );
	}
	bc->x = x;
	bc->y = y;
}

long FAR PASCAL _export pushChoiceItem(
		HWND hWnd,
		UINT message,
		UINT wParam,
		LONG lParam )
{
	/* Catch <Return> and cause focus to leave control */
#ifdef WIN32
	long inx = GetWindowLong( hWnd, GWL_ID );
#else
	short inx = GetWindowWord( hWnd, GWW_ID );
#endif

	wControl_p b = mswMapIndex( inx );

	switch (message) {
	case WM_CHAR:
		if ( b != NULL) {
			switch( wParam ) {
			case 0x0D:
			case 0x1B:
			case 0x09:
				SetFocus( ((wControl_p)(b->parent))->hWnd ); 
				SendMessage( ((wControl_p)(b->parent))->hWnd, WM_CHAR,
						wParam, lParam );
				/*SendMessage( ((wControl_p)(b->parent))->hWnd, WM_COMMAND,
						inx, MAKELONG( hWnd, EN_KILLFOCUS ) );*/
				return 0L;
			}
		}
		break;
	}
	return CallWindowProc( oldChoiceItemProc, hWnd, message, wParam, lParam );
}

LRESULT choiceItemProc(
		wControl_p b,
		HWND hWnd,
		UINT message,
		WPARAM wParam,
		LPARAM lParam )
{			   
	wChoiceItem_p me = (wChoiceItem_p)b, *rest;
	wChoice_p bc;
	int num;

	switch( message ) {
	
	case WM_COMMAND:
		switch (WCMD_PARAM_NOTF) {
		case BN_CLICKED:
			bc = me->owner;
			num = -1;
			for (rest = (wChoiceItem_p*)bc->buttList; *rest; rest++ ) {
				switch (bc->type) {
				case B_TOGGLE:
					num = rest-(wChoiceItem_p*)bc->buttList;
					if (*rest == me) {
						bc->oldVal ^= (1L<<num);
					}
					SendMessage( (*rest)->hWnd, BM_SETCHECK,
						(bc->oldVal & (1L<<num)) != 0, 0L );
					break;
	
				case B_RADIO:
					if (*rest != me) {
						SendMessage( (*rest)->hWnd, BM_SETCHECK, 0, 0L );
					} else {
						bc->oldVal = rest-(wChoiceItem_p*)bc->buttList;
						SendMessage( (*rest)->hWnd, BM_SETCHECK, 1, 0L );
					}
					break;
				}
			}
			if (bc->valueP)
				*bc->valueP = bc->oldVal;
			if (bc->action)
				bc->action( bc->oldVal, bc->data );
			break;

		}
		break;
	}													  
	
	return DefWindowProc( hWnd, message, wParam, lParam );
}					


static callBacks_t choiceCallBacks = {
		mswRepaintLabel,
		NULL,
		NULL,
		choiceSetBusy,
		choiceShow,
		choiceSetPos };

static callBacks_t choiceItemCallBacks = {
		NULL,
		NULL,
		choiceItemProc };

/**
 * Creates choice buttons. This function is used to create a group of 
 * radio buttons and checkboxes.
 *
 * \param type IN type of button
 * \param parent IN parent window
 * \param x, y IN position of group
 * \param helpStr IN index string to find help
 * \param labelStr IN label for group
 * \param option IN ?
 * \param labels IN labels for individual choices
 * \param valueP OUT pointer for return value
 * \param action IN ?
 * \param data IN ?
 * \return    created choice button group
 */

static wChoice_p choiceCreate(
		wType_e type,
		wWin_p	parent,
		POS_T	x,
		POS_T	y,
		const char	* helpStr,
		const char	* labelStr,
		long	option,
		const char	**labels,
		long	*valueP,
		wChoiceCallBack_p action,
		void	*data )
{
	wChoice_p b;
	const char ** lp;
	int cnt;
	wChoiceItem_p * butts;
	int ppx, ppy;
	int bs;
	HDC hDc;
	HWND hButt;
	int lab_l;
	DWORD dw;
	int w, maxW;
	int pw, ph;
	int index;
	char * helpStrCopy;
	HFONT hFont;

	b = mswAlloc( parent, type, mswStrdup(labelStr), sizeof *b, data, &index );
	mswComputePos( (wControl_p)b, x, y );
	b->option = option;
	b->valueP = valueP;
	b->action = action;
	b->labels = labels;
	b->labelY += 6;
		
	ppx = b->x;
	ppy = b->y;

	switch (b->type) {
	case B_TOGGLE:
			bs = BS_CHECKBOX;
			break;
	case B_RADIO:
			bs = BS_RADIOBUTTON;
			break;
	}
	for (lp = b->labels,cnt=0; *lp; lp++,cnt++ );
	butts = (wChoiceItem_p*)malloc( (cnt+1) * sizeof *butts );
	b->buttList = butts;
	b->oldVal = (b->valueP?*b->valueP:0);
	ph = pw = 2;
	maxW = 0;
	if (helpStr)
		helpStrCopy = mswStrdup( helpStr );
	for (lp = b->labels, cnt=0; *lp; lp++, cnt++, butts++ ) {			
			*butts = (wChoiceItem_p)mswAlloc( parent, B_CHOICEITEM,
				mswStrdup(_((char *)*lp)), sizeof( wChoiceItem_t ), data, &index );
			(*butts)->owner = b;
			(*butts)->hWnd = hButt = CreateWindow( "BUTTON", (*butts)->labelStr,
						bs | WS_CHILD | WS_VISIBLE | mswGetBaseStyle(parent), b->x+pw, b->y+ph,
						80, CHOICE_HEIGHT,
						((wControl_p)parent)->hWnd, (HMENU)index, mswHInst, NULL );
			if ( hButt == (HWND)0 ) {
				mswFail( "choiceCreate button" );
				return b;
			}
			(*butts)->x = b->x+pw;
			(*butts)->y = b->y+ph;
			if (b->hWnd == 0)
				b->hWnd = (*butts)->hWnd;
			(*butts)->helpStr = helpStrCopy;

			hDc = GetDC( hButt );
			lab_l = strlen((*butts)->labelStr);
			
			if (!mswThickFont) {hFont = SelectObject( hDc, mswLabelFont );}
			dw = GetTextExtent( hDc, (char *)((*butts)->labelStr), lab_l );
			if (!mswThickFont) {SelectObject( hDc, hFont );}
		
			w = LOWORD(dw) + CHOICE_MIN_WIDTH; 

			if (w > maxW)
				maxW = w;
			SetBkMode( hDc, TRANSPARENT );
			ReleaseDC( hButt, hDc );
			if (b->option & BC_HORZ) {
				pw += w;
			} else {
				ph += CHOICE_HEIGHT;
			}
			if (!SetWindowPos( hButt, HWND_TOP, 0, 0,
				w, CHOICE_HEIGHT, SWP_NOMOVE|SWP_NOZORDER)) {
				mswFail("Create CHOICE: SetWindowPos");
			}
			mswChainFocus( (wControl_p)*butts );
			newChoiceItemProc = MakeProcInstance( (XWNDPROC)pushChoiceItem, mswHInst );
			oldChoiceItemProc = (XWNDPROC)GetWindowLong( (*butts)->hWnd, GWL_WNDPROC );
			SetWindowLong( (*butts)->hWnd, GWL_WNDPROC, (LONG)newChoiceItemProc );
			if ( !mswThickFont )
				SendMessage( (*butts)->hWnd, WM_SETFONT, (WPARAM)mswLabelFont, 0L ); 
	}
	*butts = NULL;
	switch (b->type) {
	case B_TOGGLE:
		wToggleSetValue( b, (b->valueP?*b->valueP:0L) );
		break;
	case B_RADIO:
		wRadioSetValue( b, (b->valueP?*b->valueP:0L) );
		break;
	}
	if (b->option & BC_HORZ) {
		ph = CHOICE_HEIGHT;
	} else {
		pw = maxW;
	}
	pw += 4; ph += 4;
	b->w = pw;
	b->h = ph;								  

#define FRAME_STYLE		SS_ETCHEDFRAME

	if ((b->option & BC_NOBORDER)==0) {
		b->hBorder = CreateWindow( "STATIC", NULL, WS_CHILD | WS_VISIBLE | FRAME_STYLE,
			b->x, b->y, pw, ph, ((wControl_p)parent)->hWnd, 0, mswHInst, NULL );
	}
	mswAddButton( (wControl_p)b, TRUE, helpStr );
	mswCallBacks[ B_CHOICEITEM ] = &choiceItemCallBacks;
	mswCallBacks[ type ] = &choiceCallBacks;
	return b;
}


wChoice_p wRadioCreate(
		wWin_p	parent,
		POS_T	x,
		POS_T	y,
		const char	* helpStr,
		const char	* labelStr,
		long	option,
		const char	**labels,
		long	*valueP,
		wChoiceCallBack_p action,
		void	*data )
{
	return choiceCreate( B_RADIO, parent, x, y, helpStr, labelStr,
		option, labels, valueP, action, data );
}

wChoice_p wToggleCreate(
		wWin_p	parent,
		POS_T	x,
		POS_T	y,
		const char	* helpStr,
		const char	* labelStr,
		long	option,
		const char	**labels,
		long	*valueP,
		wChoiceCallBack_p action,
		void	*data )
{
	return choiceCreate( B_TOGGLE, parent, x, y, helpStr, labelStr,
		option, labels, valueP, action, data );
}
