/**
 * \file   carinvdlg.c
 * \brief  Car Inventory Dialog
 */

/*  XTrackCad - Model Railroad CAD
 *  Copyright (C) 2005, 2025 Dave Bullis
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <wlib.h>
#include "custom.h"
#include "fileio.h"
#include "messages.h"
#include "form.h"
#include "paths.h"

#include "include/cars.h"
#include "carsprivate.h"
#include "misc.h"

#define FormControlShow(a,b,c)

static int log_carInvDlg = 0;

static wIndex_t carInvInx;

unsigned selected;

#define COMMA_SEP_EXT ".csv"
#define LIST_EXP_EXT  ".txt"

#define MAX_SUBSTRING_COUNT 10
#define SMALL_STRING_LEN 100
#define LARGE_STRING_LEN 1024
#define MAX_TEXT_COLUMNS 9
#define MAX_CSV_COLUMNS  40
#define CONDITION_DEFAULT_LEN 5
#define MAXIMUM_LENGTH_DATE	8		//yy/mm/dd

#define UPDATE_MAX_WIDTH(index, current_width) \
    if ((unsigned int)(current_width) > widths[(index)]) { \
        widths[(index)] = (current_width); \
    }


static void CsvFormatLong(FILE* f, long val, const char* sep);
static void CsvFormatFloat(FILE* f, FLOAT_T val, int digits, const char* sep);
static void CsvFormatString(FILE* f, char* str, int len, const char* sep);

static void AddItem(void);
static void SelectOrder(void *context);
static void DeleteTag(void *context );
static void LaunchItemEditor(void);
static void DeleteOrShelveSelected(void);
static carItem_p FindCurrentItem(void);
static void ImportCsv(void);
static void ExportCsv(void);
static void SaveText(void);
static void LoadSortedList(void);
static void FormatDisplayItem(carItem_p item);

typedef enum {
	CONDITION_NA = 0,
	CONDITION_POOR = 30,
	CONDITION_FAIR = 50,
	CONDITION_GOOD = 70,
	CONDITION_EXCELLENT = 90,
	CONDITION_MINT = 100
} CarCondition;

enum {
	I_CI_LIST,
	I_CI_FIND,
	I_CI_EDIT,
	I_CI_ADD,
	I_CI_DELETE,
	I_CI_IMPORT_CSV,
	I_CI_EXPORT_CSV,
	I_CI_PRINT,
	I_CI_SORT1,
	I_CI_SORT2,
	I_CI_SORT3,
	I_CI_SORT4,
	I_CI_SELECT_SORT_MENU,
	I_CI_SELECT_SORT,
	S_INDEX,
	S_SCALE,
	S_MANUF,
	S_PARTNO,
	S_TYPE,
	S_DESC,
	S_ROADNAME,
	S_REPMARKS,
	S_PURCHPRICE,
	S_CURRPRICE,
	S_CONDITION,
	S_PURCHDATE,
	S_SRVDATE,
};


static paramData_t carInvPLs[] = {
	[I_CI_SORT1] = { PD_TAG, DeleteTag, "sort1", .context = I2VP(0)},
	[I_CI_SORT2] = { PD_TAG, DeleteTag, "sort2", .context = I2VP(1)},
	[I_CI_SORT3] = { PD_TAG, DeleteTag, "sort3", .context = I2VP(2)},
	[I_CI_SORT4] = { PD_TAG, DeleteTag, "sort4", .context = I2VP(3)},
	[I_CI_LIST] = { PD_LIST, &carInvInx, "list", 0, NULL, NULL, 0 },
	[I_CI_FIND] = { PD_BUTTON, FindCurrentItem, "find", 0, NULL },
	[I_CI_EDIT] = { PD_BUTTON, LaunchItemEditor, "edit", 0, NULL },
	[I_CI_ADD] = { PD_BUTTON, AddItem, "add", 0, NULL  },
	[I_CI_DELETE] = { PD_BUTTON, DeleteOrShelveSelected, "delete", 0, NULL },
	[I_CI_IMPORT_CSV] = { PD_BUTTON, ImportCsv, "import", 0, NULL},
	[I_CI_EXPORT_CSV] = { PD_BUTTON, ExportCsv, "export", 0, NULL},
	[I_CI_PRINT] = { PD_BUTTON, SaveText, "savetext", 0, NULL},
	[I_CI_SELECT_SORT_MENU] = { PD_MENU, &selected, "sort_order" },
	[I_CI_SELECT_SORT] = { PD_BUTTON, NULL, "select_sort"},
	[ S_INDEX ] = { PD_MENUITEM, SelectOrder, "s-index", .context = I2VP(S_INDEX) },
	[ S_SCALE ] = { PD_MENUITEM, SelectOrder, "s-scale", .context = I2VP(S_SCALE) },
	[ S_MANUF ] = { PD_MENUITEM, SelectOrder, "s-manuf", .context = I2VP(S_MANUF) },
	[ S_PARTNO ] = { PD_MENUITEM, SelectOrder, "s-partno", .context = I2VP(S_PARTNO) },
	[ S_TYPE ] = { PD_MENUITEM, SelectOrder, "s-type", .context = I2VP(S_TYPE) },
	[ S_DESC ] = { PD_MENUITEM, SelectOrder, "s-desc", .context = I2VP(S_DESC) },
	[ S_ROADNAME ] = { PD_MENUITEM, SelectOrder, "s-roadname", .context = I2VP(S_ROADNAME) },
	[ S_REPMARKS ] = { PD_MENUITEM, SelectOrder, "s-repmarks", .context = I2VP(S_REPMARKS) },
	[ S_PURCHPRICE ] = { PD_MENUITEM, SelectOrder, "s-purchprice", .context = I2VP(S_PURCHPRICE) },
	[ S_CURRPRICE ] = { PD_MENUITEM, SelectOrder, "s-currprice", .context = I2VP(S_CURRPRICE) },
	[ S_CONDITION ] = { PD_MENUITEM, SelectOrder, "s-condition", .context = I2VP(S_CONDITION) },
	[ S_PURCHDATE ] = { PD_MENUITEM, SelectOrder, "s-purchdate", .context = I2VP(S_PURCHDATE) },
	[ S_SRVDATE	] = { PD_MENUITEM, SelectOrder, "s-srvdate", .context = I2VP(S_SRVDATE) },
};
paramGroup_t carInvPG = { "carinv", PGO_FULLDIALOGFROMBUILDER, carInvPLs, COUNT(carInvPLs) };

#define MAX_SORT_CRITERIA 4
static struct {
	int criteria;
	unsigned carInvSort[MAX_SORT_CRITERIA];
} sortorder = {
	.criteria = 0,
};

#define GET_TAG_FROM_COUNT(x) ((carInvPLs + I_CI_SORT1 + (x))->control)

/**
 * Remove a tag from the list and update the ui. The tags to the right of the deleted tag are
 * moved one slot to the left.
 *
 * \param	context	the position of the tag in the list 0...MAX_SORT_CRITERIA - 1
 */

static void
DeleteTag(void *context )
{
	int thisTag = (int)((long)context);
	paramData_p menuItem = carInvPLs+sortorder.carInvSort[thisTag];

	if(sortorder.criteria == 0 || thisTag >= sortorder.criteria) {
		return;
	}

	for( int i = thisTag; i < sortorder.criteria - 1; i++ ) {
		const char *label = wTagGetLabel(GET_TAG_FROM_COUNT(i+1));
		wTagSetLabel(GET_TAG_FROM_COUNT(i), label);
		sortorder.carInvSort[i] = sortorder.carInvSort[i+1];
	}
	wControlActive(menuItem->control, TRUE);
	wControlShow(GET_TAG_FROM_COUNT(sortorder.criteria-1), FALSE);
	sortorder.criteria--;

	LoadSortedList();

	FormControlActive(&carInvPG, I_CI_SELECT_SORT, TRUE );

}

static void
SelectOrder(void *context)
{
	unsigned sortby = (unsigned int )((long)context);

	if(sortorder.criteria < MAX_SORT_CRITERIA) {
		const char *label;
		paramData_p menuItem = carInvPLs+sortby;
		paramData_p currentTag = carInvPLs+I_CI_SORT1+sortorder.criteria;

		sortorder.carInvSort[sortorder.criteria++] = sortby;

		label = wMenuGetLabel(menuItem->control);
		wControlActive(menuItem->control, FALSE);

		wTagSetLabel(currentTag->control, label);
		wControlShow(currentTag->control, TRUE);
	}

	LoadSortedList();

	if(sortorder.criteria == MAX_SORT_CRITERIA) {
		FormControlActive(&carInvPG, I_CI_SELECT_SORT, FALSE );
	}
}

static void
AddItem(void)
{
	if (carProto_da.cnt <= 0) {
		NoticeMessage(MSG_NO_CARPROTO, _("Ok"), NULL);
		return;
	}
	carDlgUpdateItemPtr = NULL;
	CarDlgAddItem();

	LoadSortedList();
}


// static void
// AddNew(void *context)
// {
// 	unsigned index = (unsigned)((long)context);
// 	void(*handler)(void);
// 	const char *label;
// 	paramData_p menuItem = carInvPLs+index;

// 	// get selected function
// 	switch(index) {
// 	case I_CI_NEWITEM:
// 		handler = AddItem;
// 		break;
// 	case I_CI_NEWPART:
// 		handler = CarDlgAddDesc;
// 		break;
// 	case I_CI_NEWPROTO:
// 		handler = CarDlgAddProto;
// 		break;
// 	}
// 	label = wMenuGetLabel(menuItem->control);

// 	// configure the button
// 	wButtonSetLabel((carInvPLs+I_CI_ADD)->control, label);
// 	carInvPLs[I_CI_ADD].valueP = handler;
// }


static void
LaunchItemEditor(void)
{
	carDlgUpdateItemPtr = FindCurrentItem();
	if (carDlgUpdateItemPtr == NULL) {
		return;
	}
	CarDlgUpdItem();
}


static void
DeleteOrShelveSelected(void)
{
	carItem_p item;
	wIndex_t inx, cnt, selcnt;
	wBool_t bShowMsg = FALSE;
	wBool_t bNeedReload = FALSE;

	selcnt = wListGetSelectedCount(carInvPLs[I_CI_LIST].control);
	if (selcnt == 0) {
		return;
	}
	cnt = wListGetCount(carInvPLs[I_CI_LIST].control);
	for (inx = 0; inx < cnt; inx++) {
		if (!wListGetItemSelected(carInvPLs[I_CI_LIST].control, inx)) {
			continue;
		}
		item = (carItem_p)wListGetItemContext(carInvPLs[I_CI_LIST].control,
		                                      inx);
		if (item == NULL) {
			continue;
		}
		if (item->car && !IsTrackDeleted(item->car)) {
			// Shelve car from Layout
			CarItemShelve(item);
			bNeedReload = TRUE;
		} else {
			// Delete car from Inventory
			if (!bShowMsg) {
				if (NoticeMessage(MSG_CARINV_DELETE_CONFIRM,
				                  _("Yes"), _("No"), selcnt) <= 0) {
					return;
				}
				bShowMsg = TRUE;
			}
			LOG(log_carInvDlg, 1, ("CarInvDlgDeleteShelve( %d, %s\n", inx,
			                       item->title));
			wListDelete((wList_p)carInvPLs[I_CI_LIST].control, inx);
			CarItemDelete(item);
			inx--;
			cnt--;
			bNeedReload = TRUE; //DB
		}
	}
	if (bNeedReload) {
		LoadSortedList();
		ChangeHotBar(CHANGE_SCALE);
		MainRedraw(); // Shelve Car from layout
	}
	SetFileChanged();
	carInvInx = -1;
	FormLoadSingleControl(&carInvPG, I_CI_LIST);
	FormControlActive(&carInvPG, I_CI_EDIT, FALSE);
	FormControlActive(&carInvPG, I_CI_DELETE, FALSE);
	wButtonSetLabel((wButton_p)(carInvPLs[I_CI_DELETE].control), "");
	FormControlActive(&carInvPG, I_CI_EXPORT_CSV, carItemInfo_da.cnt > 0);
	FormControlActive(&carInvPG, I_CI_PRINT, carItemInfo_da.cnt > 0);
}

static carItem_p FindCurrentItem(void)
{
	wIndex_t selcnt = wListGetSelectedCount(carInvPLs[I_CI_LIST].control);
	wIndex_t inx, cnt;

	if (selcnt != 1) { return NULL; }
	cnt = wListGetCount(carInvPLs[I_CI_LIST].control);
	for (inx = 0; inx < cnt; inx++)
		if (wListGetItemSelected(carInvPLs[I_CI_LIST].control, inx)) {
			break;
		}
	if (inx >= cnt) { return NULL; }
	return (carItem_p)wListGetItemContext(carInvPLs[I_CI_LIST].control,
	                                      inx);
}

static int Cmp_carInvItem(
        const void* ptr1,
        const void* ptr2)
{
	carItem_p item1 = *(carItem_p*)ptr1;
	carItem_p item2 = *(carItem_p*)ptr2;
	tabString_t tabs1[MAX_SUBSTRING_COUNT], tabs2[MAX_SUBSTRING_COUNT];
	int inx;
	int rc;

	TabStringExtract(item1->title, MAX_SUBSTRING_COUNT, tabs1);
	TabStringExtract(item2->title, MAX_SUBSTRING_COUNT, tabs2);
	for (inx = 0, rc = 0; inx < sortorder.criteria && rc == 0; inx++) {
		switch (sortorder.carInvSort[inx]) {
		case S_INDEX:
			rc = (int)(item1->index - item2->index);
			break;
		case S_SCALE:
			rc = (int)(item1->scaleInx - item2->scaleInx);
			break;
		case S_MANUF:
			rc = strncasecmp(tabs1[T_MANUF].ptr, tabs2[T_MANUF].ptr,
			                 max(tabs1[T_MANUF].len, tabs2[T_MANUF].len));
			break;
		case S_TYPE:
			rc = (int)(item1->type - item2->type);
			break;
		case S_PARTNO:
			rc = strncasecmp(tabs1[T_PART].ptr, tabs2[T_PART].ptr, max(tabs1[T_PART].len,
			                 tabs2[T_PART].len));
			break;
		case S_DESC:
			rc = strncasecmp(tabs1[T_PROTO].ptr, tabs2[T_PROTO].ptr,
			                 max(tabs1[T_PROTO].len, tabs2[T_PROTO].len));
			if (rc != 0) {
				break;
			}
			rc = strncasecmp(tabs1[T_DESC].ptr, tabs2[T_DESC].ptr, max(tabs1[T_DESC].len,
			                 tabs2[T_DESC].len));
			break;
		case S_ROADNAME:
			rc = strncasecmp(tabs1[T_ROADNAME].ptr, tabs2[T_ROADNAME].ptr,
			                 max(tabs1[T_ROADNAME].len, tabs2[T_ROADNAME].len));
			break;
		case S_REPMARKS:
			rc = strncasecmp(tabs1[T_REPMARK].ptr, tabs2[T_REPMARK].ptr,
			                 max(tabs1[T_REPMARK].len, tabs2[T_REPMARK].len));
			break;
		case S_PURCHPRICE:
			rc = (int)(item1->data.purchPrice - item2->data.purchPrice);
			break;
		case S_CURRPRICE:
			rc = (int)(item1->data.currPrice - item2->data.currPrice);
			break;
		case S_CONDITION:
			rc = (int)(item1->data.condition - item2->data.condition);
			break;
		case S_PURCHDATE:
			rc = (int)(item1->data.purchDate - item2->data.purchDate);
			break;
		case S_SRVDATE:
			rc = (int)(item1->data.serviceDate - item2->data.serviceDate);
			break;
		default:
			break;
		}
	}
	return rc;
}

static void LoadSortedList(void)
{
	wIndex_t selectedItem;

	qsort(&carItemInfo(0), carItemInfo_da.cnt, sizeof carItemInfo(0),
	      Cmp_carInvItem);
	FormControlShow(&carInvPG, I_CI_LIST, FALSE);
	wListClear(carInvPLs[I_CI_LIST].control);
	for (int inx = 0; inx < carItemInfo_da.cnt; inx++) {
		carItem_p item;
		item = carItemInfo(inx);
		FormatDisplayItem(item);
	}

	selectedItem = wListGetIndex(carInvPLs[I_CI_LIST].control);
	FormControlActive(&carInvPG, I_CI_FIND, TRUE);
	FormControlActive(&carInvPG, I_CI_EDIT, selectedItem >= 0 );
	FormControlActive(&carInvPG, I_CI_DELETE, selectedItem >= 0);
	FormControlActive(&carInvPG, I_CI_EXPORT_CSV, carItemInfo_da.cnt > 0);
	FormControlActive(&carInvPG, I_CI_PRINT, carItemInfo_da.cnt > 0);
}

static void FormatDisplayItem(carItem_p item)
{
	/* "Index", "Scale", "Manufacturer", "Type", "Part No", "Description", "Roadname", "RepMarks",
	   "Purch Price", "Curr Price", "Condition", "Purch Date", "Service Date", "Location", "Notes" */
	char* condition;
	char* location;
	char* manuf;
	char* road;
	char notes[SMALL_STRING_LEN];
	char carLocation[SMALL_STRING_LEN];
	tabString_t tabs[MAX_SUBSTRING_COUNT];
	char line[LARGE_STRING_LEN];

	TabStringExtract(item->title, MAX_SUBSTRING_COUNT, tabs);
	if (item->data.notes) {
		strncpy(notes, item->data.notes, sizeof notes - 1);
		notes[sizeof notes - 1] = '\0';
	} else {
		notes[0] = '\0';
	}
	condition =
	        (item->data.condition < CONDITION_NA) ? N_("N/A") :
	        (item->data.condition < CONDITION_POOR) ? N_("Poor") :
	        (item->data.condition < CONDITION_FAIR) ? N_("Fair") :
	        (item->data.condition < CONDITION_GOOD) ? N_("Good") :
	        (item->data.condition < CONDITION_EXCELLENT) ? N_("Excellent") :
	        N_("Mint");


	if (item->car && !IsTrackDeleted(item->car)) {
		coOrd hi, lo;
		GetBoundingBox(item->car, &hi, &lo);
		snprintf(carLocation, sizeof carLocation, "%0.0fx%0.0f",
		         PutDim((lo.x + hi.x) / 2.0),
		         PutDim((lo.y + hi.y) / 2.0));
		location = carLocation;
	} else {
		location = N_("Shelf");
	}

	manuf = TabStringDup(&tabs[T_MANUF]);
	road = TabStringDup(&tabs[T_ROADNAME]);
	snprintf(line, LARGE_STRING_LEN,
	         "%ld\t%s\t%s\t%.*s\t%s\t%.*s%s%.*s\t%s\t%.*s%s%.*s\t%0.2f\t%0.2f\t%s\t%ld\t%ld\t%s\t%s",
	         item->index, GetScaleName(item->scaleInx),
	         _(manuf),
	         tabs[T_PART].len, tabs[T_PART].ptr,
	         _(typeListMap[CarProtoFindTypeCode(item->type)].name),
	         tabs[T_PROTO].len, tabs[T_PROTO].ptr,
	         (tabs[T_PROTO].len > 0 && tabs[T_DESC].len) ? "/" : "",
	         tabs[T_DESC].len, tabs[T_DESC].ptr,
	         _(road),
	         tabs[T_REPMARK].len, tabs[T_REPMARK].ptr,
	         (tabs[T_REPMARK].len > 0 && tabs[T_NUMBER].len > 0) ? " " : "",
	         tabs[T_NUMBER].len, tabs[T_NUMBER].ptr,
	         item->data.purchPrice, item->data.currPrice, _(condition), item->data.purchDate,
	         item->data.serviceDate, _(location), notes);
	if (manuf) { MyFree(manuf); }
	if (road) { MyFree(road); }
	wListAddValue(carInvPLs[I_CI_LIST].control, line, NULL, item);
}

static void CarInvDlgUpdate(
        paramGroup_p pg,
        int inx,
        void* valueP)
{
	carItem_p item = NULL;

	if (inx >= I_CI_SORT1 && inx <= I_CI_SORT4 ) {
		item = FindCurrentItem();
		LoadSortedList();
		if (item) {
			carInvInx = (wIndex_t)CarItemFindIndex(item);
			if (carInvInx >= 0) {
				FormLoadSingleControl(&carInvPG, I_CI_LIST);
			}
		}
	} else if (inx == I_CI_LIST) {
		wIndex_t cnt = wListGetCount(carInvPLs[I_CI_LIST].control);
		wIndex_t selinx;
		wIndex_t selcnt;
		wIndex_t nOnShelf = 0;
		wIndex_t nOnLayout = 0;
		for (selinx = selcnt = 0; selinx < cnt; selinx++) {
			if (wListGetItemSelected(carInvPLs[I_CI_LIST].control, selinx)) {
				selcnt++;
				item = (carItem_p)wListGetItemContext(carInvPLs[I_CI_LIST].control,
				                                      selinx);
				if (!item) { continue; }
				if (item->car && !IsTrackDeleted(item->car)) {
					nOnLayout++;
				} else {
					nOnShelf++;
				}
			}
		}
		// Enable Find if 1 selected car is on Layout
		FormControlActive(pg, I_CI_FIND, nOnLayout == 1 && nOnShelf == 0);
		// Enable Edit if 1 selected car is on Shelf
		FormControlActive(&carInvPG, I_CI_EDIT, nOnLayout == 0 && nOnShelf == 1);
		wBool_t bEnableDelete = nOnLayout + nOnShelf > 0 &&
		                        (nOnLayout == 0 || nOnShelf == 0);
		wButtonSetLabel((wButton_p)(carInvPLs[I_CI_DELETE].control),
		                bEnableDelete == FALSE ? "" :
		                nOnLayout > 0 ? _("Shelve") :
		                _("Delete"));
		FormControlActive(&carInvPG, I_CI_DELETE, bEnableDelete);
	}
}


typedef enum {
	INDEX,
	ITEMDESCRIPTION,
	PROTOTYPE,
	CARNUMBER,
	PURCHASEDATE,
	PURCHASEPRICE,
	CONDITION,
	CURRENTPRICE,
	SERVICEDATE
} ExportColumns;

char *columnHeaders[] = {
	N_("#"),
	N_("Part"),
	N_("Prototype"),
	N_("Rep Mark"),
	N_("PurDate"),
	N_("PurPrice"),
	N_("Cond"),
	N_("CurPrice"),
	N_("SrvDate"),
	NULL
};

static void UpdateColumnWidths(unsigned int *widths, size_t count,
                               char** columnHeaders)
{
	for(int inx = 0; inx < (int)count; inx++) {
		if(widths[inx]) {
			UPDATE_MAX_WIDTH(inx, strlen(columnHeaders[inx]));
		}
	}
}

static void CalculateColumnWidths(unsigned int *widths, size_t count )
{
	tabString_t tabs[MAX_SUBSTRING_COUNT];

	memset(widths, 0, sizeof(unsigned int ) * count);

	for (int inx = 0; inx < carItemInfo_da.cnt; inx++) {
		unsigned int width;
		char buffer[SMALL_STRING_LEN];
		carItem_p item;

		item = carItemInfo(inx);
		TabStringExtract(item->title, MAX_SUBSTRING_COUNT, tabs);

		sprintf(buffer, "%ld", item->index);
		UPDATE_MAX_WIDTH(INDEX, (int)strlen(buffer));

		width = (int)strlen(GetScaleName(item->scaleInx)) + 1 + tabs[T_MANUF].len + 1 +
		        tabs[T_PART].len;
		UPDATE_MAX_WIDTH(ITEMDESCRIPTION, width);

		UPDATE_MAX_WIDTH(PROTOTYPE, tabs[T_PROTO].len);

		width = tabs[T_REPMARK].len + tabs[T_NUMBER].len;
		if (tabs[T_REPMARK].len > 0 && tabs[T_NUMBER].len > 0) {
			width += 1;
		}
		UPDATE_MAX_WIDTH(CARNUMBER, width);

		if (item->data.purchDate > 0) { widths[PURCHASEDATE] = MAXIMUM_LENGTH_DATE; }

		if (item->data.purchPrice > 0) {
			sprintf(buffer, "%0.2f", item->data.purchPrice);
			UPDATE_MAX_WIDTH(PURCHASEPRICE, (int)strlen(buffer));
		}

		if (item->data.condition != 0) {
			widths[CONDITION] = CONDITION_DEFAULT_LEN;
		}

		if (item->data.currPrice > 0) {
			sprintf(buffer, "%0.2f", item->data.currPrice);
			UPDATE_MAX_WIDTH(CURRENTPRICE, (int)strlen(buffer));
		}

		if (item->data.serviceDate > 0) { widths[SERVICEDATE] = MAXIMUM_LENGTH_DATE; }
	}
}

static void
TextExportEOL(FILE *fh)
{
	fputc('\n', fh);
}

static void
TextExportString(FILE *fh, int width, const char *string)
{
	fprintf(fh, "%-*.*s ", width, width, string);
}

/**
 * \todo this is wrong, it prints a number instead of a date!
 */

static void
TextExportDate(FILE *fh, int width, unsigned long dateValue)
{
	if (dateValue > 0) {
		char buffer[SMALL_STRING_LEN];
		snprintf(buffer, SMALL_STRING_LEN, "%lu", dateValue);
		fprintf(fh, "%*.*s ", width, width, buffer);
	} else {
		fprintf(fh, "%*s ", width, " ");
	}
}

static void
TextExportValue(FILE *fh, int width, double value)
{
	if (value > 0) {
		char buffer[SMALL_STRING_LEN];
		snprintf(buffer, SMALL_STRING_LEN, "%0.2f", value);
		fprintf(fh, "%*.*s ", width, width, buffer);
	} else {
		fprintf(fh, "%*s ", width, " ");
	}
}

static void
WriteExportHeader( FILE* fh, const unsigned int *widths, size_t count,
                   char **label )
{
	for(int inx = 0; inx < (int)count; inx++) {
		TextExportString(fh, widths[inx], label[inx]);
	}
	TextExportEOL(fh);
}

static void
WriteExportItem(FILE *fh, const unsigned int *widths, unsigned count,
                carItem_p item)
{
	tabString_t tabs[MAX_SUBSTRING_COUNT];
	char buffer[SMALL_STRING_LEN];

	TabStringExtract(item->title, MAX_TEXT_COLUMNS, tabs);

	snprintf(buffer, SMALL_STRING_LEN, "%ld", item->index);
	TextExportString(fh, widths[INDEX], buffer);

	snprintf(buffer, SMALL_STRING_LEN, "%s %.*s %.*s", GetScaleName(item->scaleInx),
	         tabs[T_MANUF].len, tabs[T_MANUF].ptr, tabs[T_PART].len, tabs[T_PART].ptr);
	TextExportString(fh, widths[ITEMDESCRIPTION], buffer);

	TextExportString(fh,  widths[PROTOTYPE], tabs[T_PROTO].ptr);

	snprintf(buffer, SMALL_STRING_LEN, "%.*s%s%.*s", tabs[T_REPMARK].len,
	         tabs[T_REPMARK].ptr,
	         (tabs[T_REPMARK].len > 0
	          && tabs[T_NUMBER].len > 0) ? " " : "", tabs[T_NUMBER].len, tabs[T_NUMBER].ptr);
	TextExportString(fh, widths[CARNUMBER], buffer);

	if (widths[PURCHASEDATE] > 0) {
		TextExportDate(fh, widths[PURCHASEDATE], item->data.purchDate);
	}

	if (widths[PURCHASEPRICE] > 0) {
		TextExportValue(fh, widths[PURCHASEPRICE], item->data.purchPrice);
	}

	if (widths[CONDITION] > 0) {
		if (item->data.condition != 0) {
			TextExportString( fh, widths[CONDITION],
			                  condListMap[MapCondition(item->data.condition)].name);
		} else {
			TextExportString(fh, widths[CONDITION], " ");
		}
	}

	if (widths[CURRENTPRICE] > 0) {
		TextExportValue(fh, widths[CURRENTPRICE], item->data.currPrice);
	}

	if (widths[SERVICEDATE] > 0) {
		TextExportDate(fh, widths[SERVICEDATE], item->data.serviceDate);
	}

	fprintf(fh, "\n");
}

static void
WriteExportNotes(FILE *fh, const char *notes, unsigned indent)
{
	const char *cp1;
	const char *cp0;

	if (notes) {
		cp0 = notes;
		while (1) {
			int len;

			cp1 = strchr(cp0, '\n');
			if (cp1) {
				len = (int)(cp1 - cp0);
			} else {
				len = (int)strlen(cp0);
				if (len == 0) {
					break;
				}
			}
			fprintf(fh, "%*.*s %*.*s\n", indent, indent, " ", len, len, cp0);
			if (cp1 == NULL) {
				break;
			}
			cp0 = cp1 + 1;
		}
	}
}

static int CarInvSaveText(
        int files,
        char** fileName,
        void* data)
{
	FILE* f;
	unsigned int widths[MAX_TEXT_COLUMNS];
	char *exportfile;

	CHECK(fileName != NULL);
	CHECK(files == 1);

	exportfile = MyMalloc(strlen(fileName[0])+sizeof(LIST_EXP_EXT) + 1);
	strcpy(exportfile, fileName[0]);
	AddDefaultExtension(exportfile, LIST_EXP_EXT);

	SetCurrentPath(CARSPATHKEY, fileName[0]);
	f = fopen(exportfile, "w");
	if (f == NULL) {
		NoticeMessage(MSG_OPEN_FAIL, _("Continue"), NULL, _("Car Inventory"),
		              fileName[0], strerror(errno));

		MyFree(exportfile);
		return FALSE;
	}

	CalculateColumnWidths(widths, MAX_TEXT_COLUMNS);

	UpdateColumnWidths(widths, MAX_TEXT_COLUMNS, columnHeaders);
	WriteExportHeader(f, widths, MAX_TEXT_COLUMNS, columnHeaders);

	for (int inx = 0; inx < carItemInfo_da.cnt; inx++) {
		carItem_p item = carItemInfo(inx);

		WriteExportItem(f, widths, MAX_TEXT_COLUMNS, item);

		WriteExportNotes(f, item->data.notes, widths[INDEX]);

	}
	fclose(f);
	MyFree(exportfile);
	return TRUE;
}

static char* carCsvColumnTitles[] = {
	"Index", "Scale", "Manufacturer", "Type", "Partno", "Prototype",
	"Description", "Roadname", "Repmark", "Number", "Options", "CarLength",
	"CarWidth", "CoupledLength", "TruckOffset", "TruckCenter", "Color", "PurchPrice",
	"CurrPrice", "Condition", "PurchDate", "ServiceDate", "Notes"
};
#define M_INDEX			(0)
#define M_SCALE			(1)
#define M_MANUF			(2)
#define M_TYPE			(3)
#define M_PARTNO		(4)
#define M_PROTO			(5)
#define M_DESC			(6)
#define M_ROADNAME		(7)
#define M_REPMARK		(8)
#define M_NUMBER		(9)
#define M_OPTIONS		(10)
#define M_CARLENGTH		(11)
#define M_CARWIDTH		(12)
#define M_CPLDLENGTH	(13)
#define M_TRKOFFSET     (14)
#define M_TRKCENTER		(15)
#define M_COLOR			(16)
#define M_PURCHPRICE	(17)
#define M_CURRPRICE		(18)
#define M_CONDITION		(19)
#define M_PURCHDATE		(20)
#define M_SRVDATE		(21)
#define M_NOTES			(22)

static int ParseCsvLine(
        char* line,
        int max_elem,
        tabString_t* tabs,
        const int* map,
        BOOL_T* truncated)
{
	int elem = 0;
	char* cp, * cq, * ptr;
	int rc, len;

	if (truncated) { *truncated = FALSE; }

	cp = line;
	for (cq = cp + strlen(cp) - 1; cq > cp && isspace((unsigned char)*cq); cq--);
	cq[1] = '\0';
	for (elem = 0; elem < max_elem; elem++) {
		tabs[elem].ptr = "";
		tabs[elem].len = 0;
	}
	elem = 0;
	while (*cp && elem < max_elem) {
		while (*cp == ' ') { cp++; }
		if (*cp == ',') {
			ptr = "";
			len = 0;
		} else if (*cp == '"') {
			cp++;
			ptr = cq = cp;
			while (1) {
				while (*cp != '"') {
					if (*cp == '\0') {
						rc = NoticeMessage(MSG_CARIMP_EOL, _("Continue"), _("Stop"), ptr);
						return (rc < 1) ? -1 : elem;
					}
					*cq++ = *cp++;
				}
				cp++;
				if (*cp != '"') { break; }
				*cq++ = *cp++;
			}
			if (*cp && *cp != ',') {
				rc = NoticeMessage(MSG_CARIMP_MISSING_COMMA, _("Continue"), _("Stop"), ptr);
				return (rc < 1) ? -1 : elem;
			}
			len = (int)(cq - ptr);
		} else {
			ptr = cp;
			while (*cp && *cp != ',') { cp++; }
			len = (int)(cp - ptr);
		}
		if (map[elem] >= 0) {
			tabs[map[elem]].ptr = ptr;
			tabs[map[elem]].len = len;
		}
		if (*cp) { cp++; }
		elem++;
	}
	if (truncated && elem >= max_elem && *cp) { *truncated = TRUE; }
	return elem;
}

/* ============================================================================
 * CSV IMPORT - Column Validation Helpers
 * ============================================================================ */

/* Define which columns are required for import */
static const BOOL_T requiredColumnFlags[COUNT(carCsvColumnTitles)] = {
	[M_SCALE]  = TRUE,
	[M_MANUF]  = TRUE,
	[M_PARTNO] = TRUE,
	[M_PROTO]  = TRUE,
};

/**
 * Find which known column title matches the CSV column
 * Returns column index or -1 if not found
 */
static int FindColumnTitleMatch(const tabString_t* columnName)
{
	for (int j = 0; j < COUNT(carCsvColumnTitles); j++) {
		if (TabStringCmp(carCsvColumnTitles[j], columnName) == 0) {
			return j;
		}
	}
	return -1;
}

/**
 * Check if a column is required for import
 */
static BOOL_T IsRequiredColumn(int columnIndex)
{
	if (columnIndex < 0 || columnIndex >= COUNT(carCsvColumnTitles)) {
		return FALSE;
	}
	return requiredColumnFlags[columnIndex];
}

/**
 * Display warning for unrecognized column
 */
static void WarnUnrecognizedColumn(const tabString_t* columnName)
{
	char savedChar = columnName->ptr[columnName->len];
	columnName->ptr[columnName->len] = '\0';
	NoticeMessage(MSG_CARIMP_IGNORED_COLUMN, _("Continue"), NULL,
	              columnName->ptr);
	columnName->ptr[columnName->len] = savedChar;
}

/**
 * Process a single CSV column header
 * Returns TRUE if processing should continue, FALSE to abort
 */
static BOOL_T ProcessColumnHeader(int csvColumnIndex,
                                  const tabString_t* columnName,
                                  int* map,
                                  BOOL_T* columnUsed,
                                  int* requiredColCount)
{
	int columnIndex = FindColumnTitleMatch(columnName);

	if (columnIndex < 0) {
		WarnUnrecognizedColumn(columnName);
		return TRUE;
	}

	if (columnUsed[columnIndex]) {
		NoticeMessage(MSG_CARIMP_DUP_COLUMNS, _("Continue"), NULL,
		              carCsvColumnTitles[columnIndex]);
		return FALSE;
	}

	columnUsed[columnIndex] = TRUE;
	map[csvColumnIndex] = columnIndex;

	if (IsRequiredColumn(columnIndex)) {
		(*requiredColCount)++;
	}

	return TRUE;
}

/**
 * Validate CSV header row and build column mapping
 */
static BOOL_T
ValidateImportHeaders(FILE *fh, char *csvLine, int *map, int mapSize)
{
	tabString_t tabs[MAX_CSV_COLUMNS];
	BOOL_T columnUsed[COUNT(carCsvColumnTitles)] = {FALSE};
	int numCol;
	int requiredCols = 0;
	BOOL_T truncated;

	/* Initialize map to identity */
	for (int j = 0; j < mapSize; j++) {
		map[j] = j;
	}

	/* Parse header line */
	numCol = ParseCsvLine(csvLine, mapSize, tabs, map, &truncated);
	if (numCol <= 0) {
		return FALSE;
	}

	if (truncated) {
		NoticeMessage(MSG_CARIMP_TOO_MANY_COLUMNS, _("Continue"), NULL);
		return FALSE;
	}

	/* Reset map for column name matching */
	for (int j = 0; j < mapSize; j++) {
		map[j] = -1;
	}

	/* Process each CSV column */
	for (int i = 0; i < numCol; i++) {
		if (!ProcessColumnHeader(i, &tabs[i], map, columnUsed, &requiredCols)) {
			return FALSE;
		}
	}

	/* Verify all required columns present */
	if (requiredCols != 4) {
		NoticeMessage(MSG_CARIMP_MISSING_COLUMNS, _("Continue"), NULL);
		return FALSE;
	}

	return TRUE;
}

static void
UseDefaultsFromPart(carPart_p part, tabString_p tabs, carDim_t *dim)
{
	tabString_t partTabs[MAX_CSV_COLUMNS];

	TabStringExtract(part->title, MAX_SUBSTRING_COUNT, partTabs);
	if (tabs[M_PROTO].len == 0 && partTabs[T_PROTO].len > 0) {
		tabs[M_PROTO].ptr = partTabs[T_PROTO].ptr;
		tabs[M_PROTO].len = partTabs[T_PROTO].len;
	}
	if (tabs[M_DESC].len == 0 && partTabs[T_DESC].len > 0) {
		tabs[M_DESC].ptr = partTabs[T_DESC].ptr;
		tabs[M_DESC].len = partTabs[T_DESC].len;
	}
	if (tabs[M_ROADNAME].len == 0 && partTabs[T_ROADNAME].len > 0) {
		tabs[M_ROADNAME].ptr = partTabs[T_ROADNAME].ptr;
		tabs[M_ROADNAME].len = partTabs[T_ROADNAME].len;
	}
	if (tabs[M_REPMARK].len == 0 && partTabs[T_REPMARK].len > 0) {
		tabs[M_REPMARK].ptr = partTabs[T_REPMARK].ptr;
		tabs[M_REPMARK].len = partTabs[T_REPMARK].len;
	}
	if (tabs[M_NUMBER].len == 0 && partTabs[T_NUMBER].len > 0) {
		tabs[M_NUMBER].ptr = partTabs[T_NUMBER].ptr;
		tabs[M_NUMBER].len = partTabs[T_NUMBER].len;
	}
	if (dim->carLength <= 0) {
		dim->carLength = part->dim.carLength;
	}
	if (dim->carWidth <= 0) {
		dim->carWidth = part->dim.carWidth;
	}
	if (dim->coupledLength <= 0) {
		dim->coupledLength = part->dim.coupledLength;
	}
	if (dim->truckCenter <= 0) {
		dim->truckCenter = part->dim.truckCenter;
	}
	if (dim->truckCenterOffset < 0) {
		dim->truckCenterOffset = part->dim.truckCenterOffset;
	}
}

/* Copy at most (end-dst) bytes of tab->ptr into dst, truncating rather than
 * overflowing when a CSV field is longer than the destination has room for.
 * Returns the position just past the copied bytes (never past end). */
static char *
BoundedTabCopy(char *dst, char *end, const tabString_t *tab)
{
	int len = tab->len;
	if (dst + len > end) { len = (int)(end - dst); }
	if (len > 0) { memcpy(dst, tab->ptr, len); }
	return dst + len;
}

static void
BuildTitle(char *title, size_t titleSize, tabString_t *tabs)
{
	char *nextPos = title;
	char *end = title + titleSize - 1;   /* leave room for the final NUL */
	static const int fields[] = {
		M_MANUF, M_PROTO, M_DESC, M_PARTNO, M_ROADNAME, M_REPMARK, M_NUMBER
	};

	for (unsigned i = 0; i < COUNT(fields); i++) {
		nextPos = BoundedTabCopy(nextPos, end, &tabs[fields[i]]);
		if (i + 1 < COUNT(fields) && nextPos < end) { *nextPos++ = '\t'; }
	}
	*nextPos = '\0';
}

/**
 * Convert notes from CSV format to internal format (allocates memory)
 *
 * Replaces \<NL\> markers with actual newlines.
 * Returns allocated string that caller must free with MyFree().
 *
 * @param notesTab  Tab string containing CSV notes field
 * @return          Allocated string with converted notes, or NULL if empty
 */
static char* ConvertNotesFromCsv(const tabString_t* notesTab)
{
	const char* src;
	char* result;
	char* dst;
	int srcLen;

	CHECK(notesTab != NULL);

	/* Handle empty notes */
	if (notesTab->len == 0 || notesTab->ptr == NULL) {
		return NULL;
	}

	/*
	 * Allocate buffer
	 * Maximum size needed: original length + 1 for null terminator
	 * (We're replacing 4-char "<NL>" with 1-char '\n', so we never need more)
	 */
	result = MyMalloc(notesTab->len + 1);
	if (result == NULL) {
		return NULL;
	}

	src = notesTab->ptr;
	dst = result;
	srcLen = notesTab->len;

	/* Convert <NL> markers to newlines */
	while (srcLen > 0 && *src) {                                    // +1
		if (srcLen >= 4 && strncmp(src, "<NL>", 4) == 0) {          // +1
			/* Found <NL> marker - convert to newline */
			*dst++ = '\n';
			src += 4;
			srcLen -= 4;
		} else {
			/* Copy character as-is */
			*dst++ = *src++;
			srcLen--;
		}
	}

	/* Null-terminate */
	*dst = '\0';

	return result;
}

static carItem_p
CreateItemFromCsv(tabString_t *tabs, long *dlgIndex,
                  BOOL_T *duplicateIndexError)
{
	SCALEINX_T scale;
	long index;
	carDim_t dim;
	carPart_p partP = NULL;
	char title[STR_LONG_SIZE];
	carItem_p item;

	FLOAT_T purchPrice, currPrice;
	long type = 0;
	long options, color, condition, purchDate, srvcDate;

	tabs[M_SCALE].ptr[tabs[M_SCALE].len] = '\0';
	scale = LookupScale(tabs[M_SCALE].ptr);
	tabs[M_SCALE].ptr[tabs[M_SCALE].len] = ',';

	index = TabGetLong(&tabs[M_INDEX]);
	if (index == 0) {
		CheckCarDlgItemIndex(&carDlgItemIndex);
		index = carDlgItemIndex;
	} else {
		carDlgItemIndex = index;
		if (!CheckCarDlgItemIndex(&index)) {
			// report only once per import
			if (!*duplicateIndexError) {
				NoticeMessage(MSG_CARIMP_DUP_INDEX, _("Ok"), NULL);
				*duplicateIndexError = TRUE;
			}
			carDlgItemIndex = index;
		}
	}

	dim.carLength = TabGetFloat(&tabs[M_CARLENGTH]);
	dim.carWidth = TabGetFloat(&tabs[M_CARWIDTH]);
	dim.coupledLength = TabGetFloat(&tabs[M_CPLDLENGTH]);
	dim.truckCenter = TabGetFloat(&tabs[M_TRKCENTER]);
	dim.truckCenterOffset = TabGetFloat(&tabs[M_TRKOFFSET]);
	if (dim.truckCenterOffset < 0) {
		dim.truckCenterOffset = 0;
	}

	if (tabs[M_MANUF].len > 0 && tabs[M_PARTNO].len > 0) {
		partP = CarPartFind(tabs[M_MANUF].ptr, tabs[M_MANUF].len, tabs[M_PARTNO].ptr,
		                    tabs[M_PARTNO].len, scale);

		if (partP) {
			UseDefaultsFromPart(partP, tabs, &dim);
		}
	}

	BuildTitle(title, sizeof title, tabs );

	options = TabGetLong(&tabs[M_OPTIONS]);
	type = TabGetLong(&tabs[M_TYPE]);
	color = TabGetLong(&tabs[M_COLOR]);
	purchPrice = TabGetFloat(&tabs[M_PURCHPRICE]);
	currPrice = TabGetFloat(&tabs[M_CURRPRICE]);
	condition = TabGetLong(&tabs[M_CONDITION]);
	purchDate = TabGetLong(&tabs[M_PURCHDATE]);
	srvcDate = TabGetLong(&tabs[M_SRVDATE]);
	if (dim.carLength <= 0 || dim.carWidth <= 0 || dim.coupledLength <= 0
	    || dim.truckCenter <= 0) {
		char buffer[ SMALL_STRING_LEN];
		snprintf(buffer, SMALL_STRING_LEN, N_("Dimensions must be greater than 0"));
		int rc = NoticeMessage(MSG_CARIMP_MISSING_DIMS, _("Yes"), _("No"), buffer);
		if (rc <= 0) {
			return NULL;
		}
	}
	item = CarItemNew(NULL, PARAM_CUSTOM, index, scale, title, options, type,
	                  &dim, wDrawFindColor(color),
	                  purchPrice, currPrice, condition, purchDate, srvcDate);

	if (item&&tabs[M_NOTES].len > 0) {
		item->data.notes = ConvertNotesFromCsv(&tabs[M_NOTES]);
	}

	return item;
}

static int
CarInvImportCsv(
        int files,
        char** fileName,
        void* data)
{
	FILE* f;
	char buffer[LARGE_STRING_LEN];

	tabString_t tabs[MAX_CSV_COLUMNS];
	int map[MAX_CSV_COLUMNS];;
	BOOL_T duplicateIndexError = FALSE;

	CHECK(fileName != NULL);
	CHECK(files == 1);

	SetCurrentPath(CARSPATHKEY, fileName[0]);
	f = fopen(fileName[0], "r");
	if (f == NULL) {
		NoticeMessage(MSG_OPEN_FAIL, _("Continue"), NULL, _("Import Cars"),
		              fileName[0], strerror(errno));
		return FALSE;
	}

	SetCLocale();

	if (fgets(buffer, LARGE_STRING_LEN, f) == NULL) {
		NoticeMessage(MSG_CARIMP_NO_DATA, _("Continue"), NULL);
		fclose(f);
		SetUserLocale();
		return FALSE;
	}

	if(!ValidateImportHeaders(f, buffer, map, MAX_CSV_COLUMNS)) {
		fclose(f);
		SetUserLocale();
		return FALSE;
	}

	while (fgets(buffer, LARGE_STRING_LEN, f) != NULL) {
		carItem_p item;

		int cnt;

		cnt = ParseCsvLine(buffer, MAX_CSV_COLUMNS, tabs, map, NULL);
		if (cnt == -1) {
			NoticeMessage(MSG_CARIMP_MISSING_COLUMNS, _("OK"), NULL);
			fclose(f);
			SetUserLocale();
			return FALSE;
		}

		item = CreateItemFromCsv(tabs, &carDlgItemIndex, &duplicateIndexError);
		if(!item) {
			fclose(f);
			SetUserLocale();
			return FALSE;
		}

		SetFileChanged();
	}
	fclose(f);
	SetUserLocale();
	LoadSortedList();
	return TRUE;
}

static int CarInvExportCsv(
        int files,
        char** fileName,
        void* data)
{
	FILE* f;
	tabString_t tabs[MAX_SUBSTRING_COUNT];
	char *exportfile;

	CHECK(fileName != NULL);
	CHECK(files == 1);
	SetCurrentPath(CARSPATHKEY, fileName[0]);

	exportfile = MyMalloc(strlen(fileName[0])+sizeof(COMMA_SEP_EXT) + 1);
	strcpy(exportfile, fileName[0]);
	AddDefaultExtension(exportfile, COMMA_SEP_EXT);

	f = fopen(exportfile, "w");
	if (f == NULL) {
		NoticeMessage(MSG_OPEN_FAIL, _("Continue"), NULL, _("Export Cars"),
		              fileName[0], strerror(errno));
		MyFree(exportfile);
		return FALSE;
	}

	SetCLocale();

	for (long inx = 0; inx < COUNT(carCsvColumnTitles); inx++) {
		CsvFormatString(f, carCsvColumnTitles[inx],
		                (int)strlen(carCsvColumnTitles[inx]),
		                inx < (COUNT(carCsvColumnTitles)) - 1 ? "," : "\n");
	}
	for (long inx = 0; inx < carItemInfo_da.cnt; inx++) {
		carItem_p item;
		char* sp;

		item = carItemInfo(inx);
		TabStringExtract(item->title, 7, tabs);
		CsvFormatLong(f, item->index, ",");
		sp = GetScaleName(item->scaleInx);
		CsvFormatString(f, sp, (int)strlen(sp), ",");
		CsvFormatString(f, tabs[T_MANUF].ptr, tabs[T_MANUF].len, ",");
		CsvFormatLong(f, item->type, ",");
		CsvFormatString(f, tabs[T_PART].ptr, tabs[T_PART].len, ",");
		CsvFormatString(f, tabs[T_PROTO].ptr, tabs[T_PROTO].len, ",");
		CsvFormatString(f, tabs[T_DESC].ptr, tabs[T_DESC].len, ",");
		CsvFormatString(f, tabs[T_ROADNAME].ptr, tabs[T_ROADNAME].len, ",");
		CsvFormatString(f, tabs[T_REPMARK].ptr, tabs[T_REPMARK].len, ",");
		CsvFormatString(f, tabs[T_NUMBER].ptr, tabs[T_NUMBER].len, ",");
		CsvFormatLong(f, item->options, ",");
		CsvFormatFloat(f, item->dim.carLength, 3, ",");
		CsvFormatFloat(f, item->dim.carWidth, 3, ",");
		CsvFormatFloat(f, item->dim.coupledLength, 3, ",");
		CsvFormatFloat(f, item->dim.truckCenterOffset, 3, ",");
		CsvFormatFloat(f, item->dim.truckCenter, 3, ",");
		CsvFormatLong(f, wDrawGetRGB(item->color), ",");
		CsvFormatFloat(f, item->data.purchPrice, 2, ",");
		CsvFormatFloat(f, item->data.currPrice, 2, ",");
		CsvFormatLong(f, item->data.condition, ",");
		CsvFormatLong(f, item->data.purchDate, ",");
		CsvFormatLong(f, item->data.serviceDate, ",");
		if (item->data.notes) {
			CsvFormatString(f, item->data.notes, (int)strlen(item->data.notes), "\n");
		} else {
			CsvFormatString(f, "", (int)strlen(""), "\n");
		}
	}
	fclose(f);
	SetUserLocale();

	MyFree(exportfile);
	return TRUE;
}

static struct wFilSel_t* carInvExportCsv_fs;
static void ExportCsv(void)
{
	if (carItemInfo_da.cnt <= 0) {
		return;
	}
	if (carInvExportCsv_fs == NULL)
		carInvExportCsv_fs = wFilSelCreate(mainW, FS_SAVE, 0, _("Export Cars"),
		                                   _("Comma-Separated-Values (*.csv)|*.csv"), CarInvExportCsv, NULL);
	wFilSelect(carInvExportCsv_fs, GetCurrentPath(CARSPATHKEY));
}

static struct wFilSel_t* carInvSaveText_fs;
static void SaveText(void)
{
	if (carInvSaveText_fs == NULL)
		carInvSaveText_fs = wFilSelCreate(mainW, FS_SAVE, 0, _("List Cars"),
		                                  "Text (*.txt)|*.txt", CarInvSaveText, NULL);
	wFilSelect(carInvSaveText_fs, GetCurrentPath(CARSPATHKEY));
}

static struct wFilSel_t* carInvImportCsv_fs;
static void ImportCsv(void)
{
	if (carInvImportCsv_fs == NULL)
		carInvImportCsv_fs = wFilSelCreate(mainW, FS_LOAD, 0, _("Import Cars"),
		                                   _("Comma-Separated-Values (*.csv)|*.csv"), CarInvImportCsv, NULL);
	wFilSelect(carInvImportCsv_fs, GetCurrentPath(CARSPATHKEY));
}


static void CsvFormatString(
        FILE* f,
        char* str,
        int len,
        const char* sep)
{
	CHECK(str != NULL );

	while (str && len > 0 && str[len - 1] == '\n') { len--; }
	if (*str && len) {
		fputc('"', f);
		for (; *str && len; str++, len--) {
			if (!iscntrl((unsigned char)*str)) {
				if (*str == '"') {
					fputc('"', f);
				}
				fputc(*str, f);
			} else if (*str == '\n' && str[1] && len > 1) {
				fprintf(f, "<NL>");
			}
		}
		fputc('"', f);
	}
	fprintf(f, "%s", sep);
}

static void CsvFormatLong(
        FILE* f,
        long val,
        const char* sep)
{
	if (val != 0) {
		fprintf(f, "%ld", val);
	}
	fprintf(f, "%s", sep);
}


static void CsvFormatFloat(
        FILE* f,
        FLOAT_T val,
        int digits,
        const char* sep)
{
	if (val != 0.0) {
		fprintf(f, "%0.*f", digits, val);
	}
	fprintf(f, "%s", sep);
}


/*
 * Car Inventory List
 */


void CarInvListAdd(	carItem_p item)
{
	LoadSortedList();
	carInvInx = (wIndex_t)CarItemFindIndex(item);
	if (carInvInx >= 0) {
		FormLoadSingleControl(&carInvPG, I_CI_LIST);
	}
}


void CarInvListUpdate(carItem_p item)
{
	LoadSortedList();
	carInvInx = (wIndex_t)CarItemFindIndex(item);
	if (carInvInx >= 0) {
		FormLoadSingleControl(&carInvPG, I_CI_LIST);
	}
}

#ifdef TODO_UNUSED
static void DlgFind(void* unused)
{
	carItem_p item = FindCurrentItem();
	coOrd pos;
	ANGLE_T angle;
	if (item == NULL || item->car == NULL || IsTrackDeleted(item->car)) { return; }
	CarGetPos(item->car, &pos, &angle);
	CarSetVisible(item->car);
	panCenter = pos;
	LOG(log_carInvDlg, 2, ("PanCenter:%d %0.3f %0.3f\n", __LINE__, panCenter.x,
	                       panCenter.y));
	PanHere(I2VP(0));		// CarInvDlgFind
}
#endif

static void ButtonOk(paramGroup_p group)
{

}

/* The car editor (careditdlg.c) is a separate, non-modal window -- adding or
 * editing a car there doesn't close this dialog, so its list only refreshed
 * on next open. carItemListChanged (set by CarItemNew) plus this CHANGE_PARAMS
 * handler mirrors dcustmgm.c's CustMgmChange/CarCustMgmChanged pattern to
 * pick that up live while the roster stays open in the background. */
static void CarInvDlgChange(long changes)
{
	if ((changes & CHANGE_PARAMS) == 0 ||
	    carInvPG.win == NULL || !wWinIsVisible(carInvPG.win)) {
		return;
	}
	if (carItemListChanged) {
		carItemListChanged = FALSE;
		LoadSortedList();
	}
}

EXPORT void DoCarDlg(void* unused)
{
	if (carInvPG.win == NULL) {
		FormCreateDialog(&carInvPG, MakeWindowTitle(_("Car Inventory")),
		                 _("Done"), ButtonOk,
		                 NULL, NULL, TRUE,
		                 0,
		                 CarInvDlgUpdate);
//		AddNew(I2VP(I_CI_NEWITEM));
	}
	LoadSortedList();
	wShow(carInvPG.win);
}


void InitCarInvDlg(void)
{
	FormRegister( &carInvPG );
	log_carInvDlg = LogFindIndex("carInvDlg");
	RegisterChangeNotification( CarInvDlgChange );
}
