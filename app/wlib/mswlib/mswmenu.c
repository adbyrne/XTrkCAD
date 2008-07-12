#define OEMRESOURCE

#include <windows.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <commdlg.h>
#include <math.h>
#include <ctype.h>
#include <assert.h>
#include "mswint.h"

/*
 *****************************************************************************
 *
 * Menus
 *
 *****************************************************************************
 */

typedef enum { M_MENU, M_SEPARATOR, M_PUSH, M_LIST, M_LISTITEM, M_TOGGLE, M_RADIO } mtype_e;
typedef enum { MM_BUTT, MM_MENU, MM_BAR, MM_POPUP } mmtype_e;

typedef struct wMenuItem_t * wMenuItem_p;

struct radioButtonGroup {
		int firstButton;	/* id of first button in group */
		int lastButton;		/* id of last button in group */	
};

/* NOTE: first field must be the same as WOBJ_COMMON */
#define MOBJ_COMMON \
		WOBJ_COMMON \
		int index; \
		mtype_e mtype; \
		wMenu_p parentMenu; \
		wMenuItem_p mnext;

struct wMenuItem_t {
		MOBJ_COMMON
		};

struct wMenu_t {
		MOBJ_COMMON
		mmtype_e mmtype;
		wMenuItem_p first, last;
		struct radioButtonGroup *radioGroup;
		HMENU menu;
		wButton_p button;
		wMenuTraceCallBack_p traceFunc;
		void * traceData;
		};

struct wMenuPush_t {
		MOBJ_COMMON
		wMenu_p mparent;
		wMenuCallBack_p action;
		long acclKey;
		wBool_t enabled;
		};

struct wMenuRadio_t {
		MOBJ_COMMON
		wMenu_p mparent;
		wMenuCallBack_p action;
		long acclKey;
		wBool_t enabled;
		};
struct wMenuToggle_t {
		MOBJ_COMMON
		wMenu_p mparent;
		wMenuToggleCallBack_p action;
		long acclKey;
		wBool_t enabled;
		};

typedef struct wMenuListItem_t * wMenuListItem_p;
struct wMenuList_t {
		MOBJ_COMMON
		wMenuListItem_p left, right;
		wMenu_p mlparent;
		int max;
		int count;
		wMenuListCallBack_p action;
		};

struct wMenuListItem_t {
		MOBJ_COMMON
		wMenuListItem_p left, right;
		wMenuListCallBack_p action;
		};

#define UNCHECK (0)
#define CHECK	(1)
#define RADIOCHECK (2)
#define RADIOUNCHECK (3)

static 	HBITMAP checked;
static	HBITMAP unchecked;
static  HBITMAP checkedRadio;
static 	HBITMAP uncheckedRadio;


/*
 *****************************************************************************
 *
 * Internal Functions
 *
 *****************************************************************************
 */

char * mswStrdup( const char * str )
{
	char * ret;
	if (str) {
		ret = (char*)malloc( strlen(str)+1 );
		strcpy( ret, str );
	} else
		ret = NULL;
	return ret;
}

static LRESULT menuPush(
		wControl_p b,
		HWND hWnd,
		UINT message,
		WPARAM wParam,
		LPARAM lParam )
{
	wMenuItem_p m = (wMenuItem_p)b;
	wBool_t set;

	mswAllowBalloonHelp = TRUE;

	switch( message ) {

	case WM_COMMAND:

		switch (m->mtype) {
		default:
			mswFail( "pushMenu" );
			break;
		case M_PUSH:
			if (((wMenuPush_p)m)->action)
				((wMenuPush_p)m)->action(((wMenuPush_p)m)->data);
			break;
		case M_TOGGLE:
			set = wMenuToggleGet((wMenuToggle_p)m);
			set = !set;
			wMenuToggleSet((wMenuToggle_p)m,set);
			if (((wMenuToggle_p)m)->action)
				((wMenuToggle_p)m)->action(set, ((wMenuPush_p)m)->data);
			break;
		case M_LISTITEM:
			if (((wMenuListItem_p)m)->action)
				((wMenuListItem_p)m)->action(0, "", ((wMenuListItem_p)m)->data);
			break;
		case M_RADIO:
			if (((wMenuRadio_p)m)->action)
				((wMenuRadio_p)m)->action(((wMenuRadio_p)m)->data);
			break;
		}
		return 0L;
	}
	if ( (m->parentMenu)->traceFunc ) {
		(m->parentMenu)->traceFunc( m->parentMenu, m->labelStr, ((wMenu_p)m->parentMenu)->traceData );
	}
	return DefWindowProc( hWnd, message, wParam, lParam );
}

static void menuDone( wControl_p b )
{
	wMenuItem_p m = (wMenuItem_p)b;
	switch ( m->mtype ) {
	case M_MENU:
		if ( ((wMenu_p)m)->mmtype == MM_BUTT ||
			 ((wMenu_p)m)->mmtype == MM_POPUP )
			DestroyMenu( ((wMenu_p)m)->menu );
		break;
	}
}

static callBacks_t menuItemCallBacks = {
		NULL,
		menuDone,
		menuPush };


static wMenuItem_p createMenuItem(
		wMenu_p m,
		mtype_e mtype,
		const char * helpStr,
		const char * labelStr,
		int size )
{
	wMenuItem_p mi;

	mi = (wMenuItem_p)calloc( 1, size );
	mi->type = B_MENUITEM;
	/*mi->messageProc = menuPush;*/
	mi->index = mswRegister( (wControl_p)mi );
	mi->mtype = mtype;
	if (m) {
		if (m->last != NULL) {
			m->last->mnext = mi;
		} else {
			m->first = m->last = mi;
		}
		m->last = mi;
	}
	mi->mnext = NULL;
	mi->labelStr = mswStrdup( labelStr );
//	if (helpStr != NULL) {
//		char *string;
//		string = malloc( strlen(helpStr) + 1 );
//		strcpy( string, helpStr );
//		/*xv_set(mi->menu_item, XV_HELP_DATA, string, 0 );*/
//	}
	mswCallBacks[B_MENUITEM] = &menuItemCallBacks;
	return mi;
}

/*
 *****************************************************************************
 *
 * Accelerators
 *
 *****************************************************************************
 */


typedef struct {
		long acclKey;
		wMenuPush_p mp;
		wAccelKeyCallBack_p action;
		wAccelKey_e key;
		void * data;
		} acclTable_t, *acclTable_p;
dynArr_t acclTable_da;
#define acclTable(N) DYNARR_N( acclTable_t, acclTable_da, N )


int mswMenuAccelerator(
		wWin_p win,
		long acclKey )
{
	acclTable_p at;
	if ( ((wControl_p)win)->type != W_MAIN &&
		 ((wControl_p)win)->type != W_POPUP )
		return 0;
	for ( at = &acclTable(0); at<&acclTable(acclTable_da.cnt); at++ ) {
		if (at->acclKey == acclKey) {
			if (at->mp) {
				if (at->mp->enabled && at->mp->action)
					at->mp->action(at->mp->data);
				return 1;
			} else if (at->action) {
				at->action( at->key, at->data );
				return 1;
			} else {
				return 0;
			}
		}
	}
	return 0;
}



static long acclKeyMap[] = {
			0,					/* wAccelKey_None, */
			VK_DELETE,			/* wAccelKey_Del, */
			VK_INSERT,			/* wAccelKey_Ins, */
			VK_HOME,			/* wAccelKey_Home, */
			VK_END,				/* wAccelKey_End, */
			VK_PRIOR,			/* wAccelKey_Pgup, */
			VK_NEXT,			/* wAccelKey_Pgdn, */
			VK_UP,				/* wAccelKey_Up, */
			VK_DOWN,			/* wAccelKey_Down, */
			VK_RIGHT,			/* wAccelKey_Right, */
			VK_LEFT,			/* wAccelKey_Left, */
			VK_BACK,			/* wAccelKey_Back, */
			VK_F1,				/* wAccelKey_F1, */
			VK_F2,				/* wAccelKey_F2, */
			VK_F3,				/* wAccelKey_F3, */
			VK_F4,				/* wAccelKey_F4, */
			VK_F5,				/* wAccelKey_F5, */
			VK_F6,				/* wAccelKey_F6, */
			VK_F7,				/* wAccelKey_F7, */
			VK_F8,				/* wAccelKey_F8, */
			VK_F9,				/* wAccelKey_F9, */
			VK_F10,				/* wAccelKey_F10, */
			VK_F11,				/* wAccelKey_F11, */
			VK_F12				/* wAccelKey_F12, */
		};


void wAttachAccelKey(
		wAccelKey_e key,
		int modifier,
		wAccelKeyCallBack_p action,
		void * data )
{
	acclTable_t * ad;
	if ( key < 1 || key > wAccelKey_F12 ) {
		mswFail( "wAttachAccelKey: key out of range" );
		return;
	}
	DYNARR_APPEND( acclTable_t, acclTable_da, 10 );
	ad = &acclTable(acclTable_da.cnt-1);
	ad->acclKey = acclKeyMap[key] | (modifier<<8);
	ad->key = key;
	ad->action = action;
	ad->data = data;
	ad->mp = NULL;
}

/*
 *****************************************************************************
 *
 * Menu Item Create
 *
 *****************************************************************************
 */

HBITMAP GetMyCheckBitmaps(UINT fuCheck) 
{ 
    COLORREF crBackground;  /* background color */                 
    HBRUSH hbrBackground;   /* background brush */                 
    HBRUSH hbrTargetOld;    /* original background brush */
    HDC hdcSource;          /* source device context */       
    HDC hdcTarget;          /* target device context */          
    HBITMAP hbmpCheckboxes; /* handle to check-box bitmap */
    BITMAP bmCheckbox;      /* structure for bitmap data */      
    HBITMAP hbmpSourceOld;  /* handle to original source bitmap */
    HBITMAP hbmpTargetOld;  /* handle to original target bitmap */
    HBITMAP hbmpCheck;      /* handle to check-mark bitmap */
    RECT rc;                /* rectangle for check-box bitmap */    
    WORD wBitmapX;          /* width of check-mark bitmap */       
    WORD wBitmapY;          /* height of check-mark bitmap */      
 
    /* Get the menu background color and create a solid brush 
       with that color. */
 
    crBackground = GetSysColor(COLOR_MENU); 
    hbrBackground = CreateSolidBrush(crBackground); 
 
    /* Create memory device contexts for the source and 
       destination bitmaps. */
 
    hdcSource = CreateCompatibleDC((HDC) NULL); 
    hdcTarget = CreateCompatibleDC(hdcSource); 
 
    /* Get the size of the system default check-mark bitmap and 
       create a compatible bitmap of the same size. */
 
    wBitmapX = GetSystemMetrics(SM_CXMENUCHECK); 
    wBitmapY = GetSystemMetrics(SM_CYMENUCHECK); 
 
    hbmpCheck = CreateCompatibleBitmap(hdcSource, wBitmapX, 
        wBitmapY); 
 
    /* Select the background brush and bitmap into the target DC. */
 
    hbrTargetOld = SelectObject(hdcTarget, hbrBackground); 
    hbmpTargetOld = SelectObject(hdcTarget, hbmpCheck); 
 
    /* Use the selected brush to initialize the background color 
       of the bitmap in the target device context. */
 
    PatBlt(hdcTarget, 0, 0, wBitmapX, wBitmapY, PATCOPY); 
 
    /* Load the predefined check box bitmaps and select it 
       into the source DC. */
 
    hbmpCheckboxes = LoadBitmap((HINSTANCE) NULL, 
        (LPTSTR) OBM_CHECKBOXES); 
 
    hbmpSourceOld = SelectObject(hdcSource, hbmpCheckboxes); 
 
    /* Fill a BITMAP structure with information about the 
       check box bitmaps, and then find the upper-left corner of 
       the unchecked check box or the checked check box. */
 
    GetObject(hbmpCheckboxes, sizeof(BITMAP), &bmCheckbox); 
 
	switch( fuCheck ) {
		
	case UNCHECK:
        rc.left = 0; 
        rc.right = (bmCheckbox.bmWidth / 4); 
		rc.top = 0; 
		rc.bottom = (bmCheckbox.bmHeight / 3); 
		break;  
	case CHECK:  
        rc.left = (bmCheckbox.bmWidth / 4); 
        rc.right = (bmCheckbox.bmWidth / 4) * 2; 
	    rc.top = 0; 
	    rc.bottom = (bmCheckbox.bmHeight / 3); 
		break;
	case RADIOCHECK:
        rc.left = (bmCheckbox.bmWidth / 4); 
        rc.right = (bmCheckbox.bmWidth / 4) * 2; 
		rc.top = (bmCheckbox.bmHeight / 3) + 1;
	    rc.bottom = (bmCheckbox.bmHeight / 3) * 2; 
		break;
	case RADIOUNCHECK:
		rc.top = (bmCheckbox.bmHeight / 3) + 1;
	    rc.bottom = (bmCheckbox.bmHeight / 3) * 2; 
        rc.left = 0; 
        rc.right = (bmCheckbox.bmWidth / 4); 

		break;
	}
    
    /* Copy the appropriate bitmap into the target DC. If the 
       check-box bitmap is larger than the default check-mark 
       bitmap, use StretchBlt to make it fit; otherwise, just 
       copy it. */
 
    if (((rc.right - rc.left) > (int) wBitmapX) || 
            ((rc.bottom - rc.top) > (int) wBitmapY)) 
    {
        StretchBlt(hdcTarget, 0, 0, wBitmapX, wBitmapY, 
            hdcSource, rc.left, rc.top, rc.right - rc.left, 
            rc.bottom - rc.top, SRCCOPY); 
    }
 
    else 
    {
        BitBlt(hdcTarget, 0, 0, rc.right - rc.left, 
            rc.bottom - rc.top, 
            hdcSource, rc.left, rc.top, SRCCOPY); 
    }
 
    /* Select the old source and destination bitmaps into the 
       source and destination DCs, and then delete the DCs and 
       the background brush. */
 
    SelectObject(hdcSource, hbmpSourceOld); 
    SelectObject(hdcTarget, hbrTargetOld); 
    hbmpCheck = SelectObject(hdcTarget, hbmpTargetOld); 
 
    DeleteObject(hbrBackground); 
    DeleteObject(hdcSource); 
    DeleteObject(hdcTarget); 
 
    /* Return a handle to the new check-mark bitmap. */
 
    return hbmpCheck; 
} 

void mswCreateCheckBitmaps()
{
	checked = GetMyCheckBitmaps( CHECK );
	unchecked = GetMyCheckBitmaps( UNCHECK );
	checkedRadio = GetMyCheckBitmaps( RADIOCHECK );
	uncheckedRadio = GetMyCheckBitmaps( RADIOUNCHECK );

}

wMenuRadio_p wMenuRadioCreate(
		wMenu_p m, 
		const char * helpStr,
		const char * labelStr,
		long acclKey,
		wMenuCallBack_p action,
		void	*data )
{
	wMenuRadio_p mi;
	int rc;
	char label[80];
	char *cp;
	char ac;
	UINT vk;
	long modifier;

	mi = (wMenuRadio_p)createMenuItem( m, M_RADIO, helpStr, labelStr, sizeof *mi );
	mi->action = action;
	mi->data = data;
	mi->mparent = m;
	mi->acclKey = acclKey;
	mi->enabled = TRUE;
	strcpy( label, mi->labelStr );
	modifier = 0;
	if ( acclKey != 0 ) {
		DYNARR_APPEND( acclTable_t, acclTable_da, 10 );
		cp = label + strlen( label );
		*cp++ = '\t';
		if (acclKey & WCTL ) {
			strcpy( cp, "Ctrl+" );
			cp += 5;
			modifier |= WKEY_CTRL;
		}
		if (acclKey & WALT ) {
			strcpy( cp, "Alt+" );
			cp += 4;
			modifier |= WKEY_ALT;
		}
		if (acclKey & WSHIFT ) {
			strcpy( cp, "Shift+" );
			cp += 6;
			modifier |= WKEY_SHIFT;
		}
		*cp++ = toupper( (char)(acclKey & 0xFF) );
		*cp++ = '\0';
		ac = (char)(acclKey & 0xFF);
		if (isalpha(ac)) {
			ac = tolower( ac );
		}
		vk = VkKeyScan( ac );
		if ( vk & 0xFF00 )
			modifier |= WKEY_SHIFT;
		acclTable(acclTable_da.cnt-1).acclKey = (modifier<<8) | (vk&0x00FF);
		acclTable(acclTable_da.cnt-1).mp = (wMenuPush_p)mi;
	}
	rc = AppendMenu( m->menu, MF_STRING, mi->index, label );

	/* add the correct bitmaps for radio buttons */

    rc = SetMenuItemBitmaps(m->menu, mi->index, FALSE, uncheckedRadio, checkedRadio ); 

	if( m->radioGroup == NULL ) {
		m->radioGroup = malloc( sizeof( struct radioButtonGroup ));
		assert( m->radioGroup );
		m->radioGroup->firstButton = mi->index;
	} else {
		m->radioGroup->lastButton = mi->index;
	}

	return mi;
}

void wMenuRadioSetActive(wMenuRadio_p mi )
{
	BOOL rc;

	rc = CheckMenuRadioItem( mi->mparent->menu, 
							 mi->mparent->radioGroup->firstButton, 
							 mi->mparent->radioGroup->lastButton,
							 mi->index,
							 MF_BYCOMMAND );
} 


wMenuPush_p wMenuPushCreate(
		wMenu_p m, 
		const char * helpStr,
		const char * labelStr,
		long acclKey,
		wMenuCallBack_p action,
		void	*data )
{
	wMenuPush_p mi;
	int rc;
	char label[80];
	char *cp;
	char ac;
	UINT vk;
	long modifier;

	mi = (wMenuPush_p)createMenuItem( m, M_PUSH, helpStr, labelStr, sizeof *mi );
	mi->action = action;
	mi->data = data;
	mi->mparent = m;
	mi->acclKey = acclKey;
	mi->enabled = TRUE;
	strcpy( label, mi->labelStr );
	modifier = 0;
	if ( acclKey != 0 ) {
		DYNARR_APPEND( acclTable_t, acclTable_da, 10 );
		cp = label + strlen( label );
		*cp++ = '\t';
		if (acclKey & WCTL ) {
			strcpy( cp, "Ctrl+" );
			cp += 5;
			modifier |= WKEY_CTRL;
		}
		if (acclKey & WALT ) {
			strcpy( cp, "Alt+" );
			cp += 4;
			modifier |= WKEY_ALT;
		}
		if (acclKey & WSHIFT ) {
			strcpy( cp, "Shift+" );
			cp += 6;
			modifier |= WKEY_SHIFT;
		}
		*cp++ = toupper( (char)(acclKey & 0xFF) );
		*cp++ = '\0';
		ac = (char)(acclKey & 0xFF);
		if (isalpha(ac)) {
			ac = tolower( ac );
		}
		vk = VkKeyScan( ac );
		if ( vk & 0xFF00 )
			modifier |= WKEY_SHIFT;
		acclTable(acclTable_da.cnt-1).acclKey = (modifier<<8) | (vk&0x00FF);
		acclTable(acclTable_da.cnt-1).mp = mi;
	}
	rc = AppendMenu( m->menu, MF_STRING, mi->index, label );
	return mi;
}


void wMenuPushEnable(
		wMenuPush_p mi,
		BOOL_T enable )
{
	EnableMenuItem( mi->mparent->menu, mi->index,
		MF_BYCOMMAND|(enable?MF_ENABLED:(MF_DISABLED|MF_GRAYED)) );
	mi->enabled = enable;
}


wMenu_p wMenuMenuCreate(
		wMenu_p m, 
		const char * helpStr,
		const char * labelStr )
{
	wMenu_p mm;
	int rc;

	mm = (wMenu_p)createMenuItem( NULL, M_MENU, NULL, labelStr, sizeof *mm );
	mm->menu = CreatePopupMenu();
	mm->mmtype = MM_MENU;
	/*mm->parent = (wControl_p)m;*/
	mm->first = mm->last = NULL;

	rc = AppendMenu( m->menu, MF_STRING|MF_ENABLED|MF_POPUP, (UINT)mm->menu, mm->labelStr );

	return mm;
}

void wMenuSeparatorCreate(
		wMenu_p m ) 
{
	int rc;
	createMenuItem( m, M_SEPARATOR, NULL, NULL, sizeof *(wMenuItem_p)NULL );
	rc = AppendMenu( m->menu, MF_SEPARATOR, (UINT)0, NULL );
}

/*
 *****************************************************************************
 *
 * Menu List
 *
 *****************************************************************************
 */


static void appendItem(
		wMenuListItem_p ml,
		wMenuListItem_p mi )
{
	mi->right = ml->right;
	ml->right->left = mi;
	mi->left = ml;
	ml->right = mi;
}


static void removeItem(
		wMenuListItem_p mi )
{
	mi->left->right = mi->right;
	mi->right->left = mi->left;
	mi->right = mi->left = mi;
}


wMenuList_p wMenuListCreate(
		wMenu_p m, 
		const char * helpStr,
		int max,
		wMenuListCallBack_p action )
{
	wMenuList_p mi;
	mi = (wMenuList_p)createMenuItem( m, M_LIST, helpStr, NULL, sizeof *mi );
	mi->count = 0;
	mi->max = max;
	mi->mlparent = m;
	mi->action = action;
	mi->right = mi->left = (wMenuListItem_p)mi;
	return mi;
}


int getMlistOrigin( wMenu_p m, wMenuList_p ml )
{
	wMenuItem_p mi;
	int count;
	count = 0;
	for ( mi = m->first; mi != NULL; mi = mi->mnext ) {
		switch( mi->mtype ) {
		case M_SEPARATOR:
		case M_PUSH:
		case M_MENU:
			count++;
			break;
		case M_LIST:
			if (mi == (wMenuItem_p)ml)
				return count;
			count += ((wMenuList_p)mi)->count;
			break;
		default:
			mswFail( "getMlistOrigin" );
		}
	}
	return count;
}


void wMenuListAdd(
		wMenuList_p ml,
		int index,
		const char * labelStr,
		void * data )
{
	int origin;
	wMenuListItem_p wl_p;
	wMenuListItem_p mi;
	int count;
	int rc;

	origin = getMlistOrigin(ml->mlparent, ml);
	for ( count=0,wl_p=ml->right; wl_p!=(wMenuListItem_p)ml; count++,wl_p=wl_p->right ) {
		if (wl_p->labelStr != NULL && strcmp( labelStr, wl_p->labelStr ) == 0) {
			/* move item */
			if (count != index) {
				RemoveMenu( ml->mlparent->menu, origin+count, MF_BYPOSITION );
				removeItem( wl_p );
				goto add;
			}
			((wMenuListItem_p)wl_p)->data = data;
			return;
		}
	}
	if (ml->max > 0 && ml->count >= ml->max) {
		RemoveMenu( ml->mlparent->menu, origin+ml->count-1, MF_BYPOSITION );
		wl_p = ml->left;
		removeItem( ml->left );
add:
		ml->count--;
		if (wl_p->labelStr )
			free( CAST_AWAY_CONST wl_p->labelStr );
		wl_p->labelStr = mswStrdup( labelStr );
	} else {
		wl_p = (wMenuListItem_p)createMenuItem( NULL, M_LISTITEM, NULL,
								labelStr, sizeof *wl_p );
	}
	((wMenuListItem_p)wl_p)->data = data;
	((wMenuListItem_p)wl_p)->action = ml->action;
	if (index < 0 || index > ml->count)
		index = ml->count;
	for ( mi=(wMenuListItem_p)ml,count=0; count<index; mi=mi->right,count++);
	rc = InsertMenu( ml->mlparent->menu, origin+index,
					 MF_BYPOSITION|MF_STRING, wl_p->index, wl_p->labelStr );
	appendItem( mi, wl_p );
	ml->count++;
}


void wMenuListDelete(
		wMenuList_p ml,
		const char * labelStr )
{
	int origin, count;
	wMenuListItem_p wl_p;

	origin = getMlistOrigin(ml->mlparent, ml);
	for ( count=0,wl_p=ml->right; wl_p!=(wMenuListItem_p)ml; count++,wl_p=wl_p->right ) {
		if (wl_p->labelStr != NULL && strcmp( labelStr, wl_p->labelStr ) == 0) {
			/* delete item */
			mswUnregister( wl_p->index );
			RemoveMenu( ml->mlparent->menu, origin+count, MF_BYPOSITION );
			removeItem( wl_p );
			ml->count--;
			free( wl_p );
			return;
		}
	}
}


const char * wMenuListGet(
		wMenuList_p ml,
		int index,
		void ** data )
{
	int origin, count;
	wMenuListItem_p wl_p;

	if (index >= ml->count)
		return NULL;
	origin = getMlistOrigin(ml->mlparent, ml);
	for ( count=0,wl_p=ml->right; wl_p&&count<index; count++,wl_p=wl_p->right );
	if (wl_p==NULL)
		return NULL;
	if ( data )
		*data = wl_p->data;
	return wl_p->labelStr;
}


void wMenuListClear(
		wMenuList_p ml )
{
	int origin, count;
	wMenuListItem_p wl_p, wl_q;

	origin = getMlistOrigin(ml->mlparent, ml);
	for ( count=0,wl_p=ml->right; count<ml->count; count++,wl_p=wl_q ) {
		/* delete item */
		mswUnregister( wl_p->index );
		RemoveMenu( ml->mlparent->menu, origin, MF_BYPOSITION );
		wl_q = wl_p->right;
		free( wl_p );
	}
	ml->count = 0;
	ml->right = ml->left = (wMenuListItem_p)ml;
}



wMenuToggle_p wMenuToggleCreate(
		wMenu_p m, 
		const char * helpStr,
		const char * labelStr,
		long acclKey,
		wBool_t set,
		wMenuToggleCallBack_p action,
		void * data )
{
	wMenuToggle_p mt;
	int rc;

	mt = (wMenuToggle_p)createMenuItem( m, M_TOGGLE, helpStr, labelStr, sizeof *mt );
	/*setAcclKey( m->parent, m->menu, mt->menu_item, acclKey );*/
	mt->action = action;
	mt->data = data;
	mt->mparent = m;
	mt->enabled = TRUE;
	mt->parentMenu = m;
	rc = AppendMenu( m->menu, MF_STRING, mt->index, labelStr );
	wMenuToggleSet( mt, set );
	return mt;
}


wBool_t wMenuToggleGet(
		wMenuToggle_p mt )
{
	return (GetMenuState( mt->mparent->menu, mt->index, MF_BYCOMMAND ) & MF_CHECKED) != 0;
}


wBool_t wMenuToggleSet(
		wMenuToggle_p mt,
		wBool_t set )
{
	wBool_t rc;
	CheckMenuItem( mt->mparent->menu, mt->index, MF_BYCOMMAND|(set?MF_CHECKED:MF_UNCHECKED) );
	rc = (GetMenuState( mt->mparent->menu, mt->index, MF_BYCOMMAND ) & MF_CHECKED) != 0;
	return rc;
}

void wMenuToggleEnable(
		wMenuToggle_p mt,
		wBool_t enable )
{
	EnableMenuItem( mt->mparent->menu, mt->index,
		MF_BYCOMMAND|(enable?MF_ENABLED:(MF_DISABLED|MF_GRAYED)) );
	mt->enabled = enable;
}	  

/*
 *****************************************************************************
 *
 * Menu Create
 *
 *****************************************************************************
 */


void mswMenuMove(
		wMenu_p m,
		wPos_t x,
		wPos_t y )
{
	wControl_p b;
	b = (wControl_p)m->parent;
	if (b && b->hWnd)
		if (!SetWindowPos( b->hWnd, HWND_TOP, x, y,
				CW_USEDEFAULT, CW_USEDEFAULT,
				SWP_NOSIZE|SWP_NOZORDER))
				mswFail("mswMenuMove");
}


static void pushMenuButt(
		void * data )
{
	wMenu_p m = (wMenu_p)data;
	RECT rect;
	mswAllowBalloonHelp = FALSE;
	GetWindowRect( m->hWnd, &rect );
	TrackPopupMenu( m->menu, TPM_LEFTALIGN, rect.left, rect.bottom,
						0, ((wControl_p)(m->parent))->hWnd, NULL );
}


wMenu_p wMenuCreate(
		wWin_p	parent,
		POS_T	x,
		POS_T	y,
		const char	* helpStr,
		const char	* labelStr,
		long	option )
{
	wMenu_p m;
	wControl_p b;
	long buttOption = 0;
	const char * label = labelStr;

	if (option & BM_ICON) {
		buttOption = BO_ICON;
		label = "ICON";
	}
	m = (wMenu_p)createMenuItem( NULL, M_MENU, helpStr, label, sizeof *m );
	m->button = wButtonCreate( parent, x, y, helpStr, labelStr,
						buttOption, 0, pushMenuButt, (void*)m );
	b = (wControl_p)m->button;
	m->parent = b->parent;
	m->x = b->x;
	m->y = b->y;
	m->w = b->w;
	m->h = b->h;
	m->hWnd = b->hWnd;
	m->helpStr = b->helpStr;

	m->menu = CreatePopupMenu();
	m->mmtype = MM_BUTT;
	m->first = m->last = NULL;

	return m;
}


wMenu_p wMenuBarAdd(
		wWin_p w,
		const char * helpStr,
		const char * labelStr )
{
	HMENU menu;
	wMenu_p m;
	int rc;

	menu = GetMenu( ((wControl_p)w)->hWnd );
	if (menu == (HMENU)0) {
		menu = CreateMenu();
		SetMenu( ((wControl_p)w)->hWnd, menu );
	}

	m = (wMenu_p)createMenuItem( NULL, M_MENU, helpStr, labelStr, sizeof *m );
	m->menu = CreateMenu();
	m->parent = w;
	m->mmtype = MM_BAR;
	m->first = m->last = NULL;

	rc = AppendMenu( menu, MF_STRING|MF_POPUP|MF_ENABLED, (UINT)m->menu, labelStr );

	DrawMenuBar( ((wControl_p)w)->hWnd );
	return m;
}



wMenu_p wMenuPopupCreate(
		wWin_p w,
		const char * labelStr )
{
	wMenu_p m;
	long buttOption = 0;
	const char * label = labelStr;

	m = (wMenu_p)createMenuItem( NULL, M_MENU, NULL, label, sizeof *m );
	m->button = NULL;
	m->parent = w;
	m->x = 0;
	m->y = 0;
	m->w = 0;
	m->h = 0;
	m->hWnd = ((wControl_p)w)->hWnd;
	m->helpStr = NULL;

	m->menu = CreatePopupMenu();
	m->mmtype = MM_POPUP;
	m->first = m->last = NULL;

	return m;
}


void wMenuPopupShow( wMenu_p mp )
{
	POINT pt;
	GetCursorPos( &pt );
	TrackPopupMenu( mp->menu, TPM_LEFTALIGN, pt.x, pt.y, 0, mp->hWnd, NULL );
}
		
/*-----------------------------------------------------------------*/

void wMenuSetTraceCallBack(
		wMenu_p m,
		wMenuTraceCallBack_p func,
		void * data )
{
	m->traceFunc = func;
	m->traceData = data;
}

wBool_t wMenuAction(
		wMenu_p m,
		const char * label )
{
	wMenuItem_p mi;
	wMenuToggle_p mt;
	wBool_t set;
	for ( mi = m->first; mi != NULL; mi = (wMenuItem_p)mi->mnext ) {
		if ( mi->labelStr != NULL && strcmp( mi->labelStr, label ) == 0 ) {
			switch( mi->mtype ) {
			case M_SEPARATOR:
				break;
			case M_PUSH:
				if ( ((wMenuPush_p)mi)->enabled == FALSE )
					wBeep();
				else
					((wMenuPush_p)mi)->action( ((wMenuPush_p)mi)->data );
				break;
			case M_TOGGLE:
				mt = (wMenuToggle_p)mi;
				if ( mt->enabled == FALSE ) {
					wBeep();
				} else {
					set = wMenuToggleGet( mt );
					wMenuToggleSet( mt, !set );
					mt->action( set, mt->data );
				}
				break;
			case M_MENU:
				break;
			case M_LIST:
				break;
			default:
				fprintf(stderr, "Oops: wMenuAction\n");
			}
			return TRUE;
		}
	}
	return FALSE;
}
