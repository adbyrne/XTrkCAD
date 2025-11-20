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

#define FormControlShow(a,b,c) 

static int log_carInvDlg = 0;

static wIndex_t carInvInx;

static wIndex_t carInvSort[] = { 0, 1, 2, 3 };
#define N_SORT			(COUNT( carInvSort ))

static void CsvFormatLong(FILE* f, long val, const char* sep);
static void CsvFormatFloat(FILE* f, FLOAT_T val, int digits, const char* sep);
static void CsvFormatString(FILE* f, char* str, int len, const char* sep);

static void CarInvDlgAdd(void);
static void CarInvDlgEdit(void);
static void CarInvDlgDeleteShelve(void);
static carItem_p CarInvDlgFindCurrentItem(void);
static void CarInvDlgImportCsv(void);
static void CarInvDlgExportCsv(void);
static void CarInvDlgSaveText(void);
static void CarInvListLoad(void);
static void CarInvLoadItem(carItem_p item);

static wWinPix_t carInvColumnWidths[] = {
	-40, 30, 100, -50, 50, 130, 120, 100,
	-50, -50, 60, 55, 55, 40, 200
};

/*
static const char* carInvColumnTitles[] = {
	N_("Index"), N_("Scale"), N_("Manufacturer"), N_("Part No"), N_("Type"),
	N_("Description"), N_("Roadname"), N_("Rep Marks"), N_("Purc Price"),
	N_("Curr Price"), N_("Condition"), N_("Purc Date"), N_("Srvc Date"),
	N_("Locat'n"), N_("Notes")
};
static char* sortOrders[] = {
	N_("Index"), N_("Scale"), N_("Manufacturer"), N_("Part No"), N_("Type"),
	N_("Description"), N_("Roadname"), N_("RepMarks"), N_("Purch Price"),
	N_("Curr Price"), N_("Condition"), N_("Purch Date"), N_("Service Date")
}; */

#define S_INDEX			(0)
#define S_SCALE			(1)
#define S_MANUF			(2)
#define S_PARTNO		(3)
#define S_TYPE			(4)
#define S_DESC			(5)
#define S_ROADNAME		(6)
#define S_REPMARKS		(7)
#define S_PURCHPRICE	(8)
#define S_CURRPRICE		(9)
#define S_CONDITION		(10)
#define S_PURCHDATE		(11)
#define S_SRVDATE		(12)
static paramListData_t carInvListData = { 30, 600 };
static paramData_t carInvPLs[] = {
#define I_CI_SORT		(0)
	{ PD_DROPLIST, &carInvSort[0], "sort1", PDO_LISTINDEX, I2VP(110), N_("Sort By") },
	{ PD_DROPLIST, &carInvSort[1], "sort2", PDO_LISTINDEX | PDO_DLGHORZ, I2VP(110), "" },
	{ PD_DROPLIST, &carInvSort[2], "sort3", PDO_LISTINDEX | PDO_DLGHORZ, I2VP(110), "" },
	{ PD_DROPLIST, &carInvSort[3], "sort4", PDO_LISTINDEX | PDO_DLGHORZ, I2VP(110), "" },
#define S				(4)
#define I_CI_LIST		(S+0)
	{ PD_LIST, &carInvInx, "list", PDO_LISTINDEX | PDO_DLGRESIZE | PDO_DLGNOLABELALIGN | PDO_DLGRESETMARGIN, &carInvListData, NULL, BO_READONLY | BL_MANY },
#define I_CI_EDIT		(S+1)
	{ PD_BUTTON, CarInvDlgEdit, "edit", PDO_DLGCMDBUTTON, NULL },
#define I_CI_ADD		(S+2)
	{ PD_BUTTON, CarInvDlgAdd, "add", 0, NULL  },
#define I_CI_DELETE		(S+3)
	{ PD_BUTTON, CarInvDlgDeleteShelve, "delete", PDO_DLGWIDE, NULL },
#define I_CI_IMPORT_CSV	(S+4)
	{ PD_BUTTON, CarInvDlgImportCsv, "import", PDO_DLGWIDE, NULL},
#define I_CI_EXPORT_CSV	(S+5)
	{ PD_BUTTON, CarInvDlgExportCsv, "export", 0, NULL},
#define I_CI_PRINT		(S+6)
	{ PD_BUTTON, CarInvDlgSaveText, "savetext", 0, NULL}
};
paramGroup_t carInvPG = { "carinv", PGO_FULLDIALOGFROMBUILDER, carInvPLs, COUNT(carInvPLs) };

static void CarInvDlgAdd(void)
{
	if (carProto_da.cnt <= 0) {
		NoticeMessage(MSG_NO_CARPROTO, _("Ok"), NULL);
		return;
	}
	carDlgUpdateItemPtr = NULL;

	CarDlgAddItem();
}


static void CarInvDlgEdit(void)
{
	carDlgUpdateItemPtr = CarInvDlgFindCurrentItem();
	if (carDlgUpdateItemPtr == NULL) {
		return;
	}
	CarDlgUpdItem();
}


static void CarInvDlgDeleteShelve(void)
{
	carItem_p item;
	wIndex_t inx, inx1, cnt, selcnt;
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
			if (item->title) { MyFree(item->title); }
			if (item->data.number) { MyFree(item->data.number); }
			MyFree(item);
			for (inx1 = inx; inx1 < carItemInfo_da.cnt - 1; inx1++) {
				carItemInfo(inx1) = carItemInfo(inx1 + 1);
			}
			carItemInfo_da.cnt -= 1;
			inx--;
			cnt--;
			bNeedReload = TRUE; //DB
		}
	}
	if (bNeedReload) {
		CarInvListLoad();
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
	FormDialogOkActive(&carInvPG, FALSE);
}

static carItem_p CarInvDlgFindCurrentItem(void)
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
	tabString_t tabs1[7], tabs2[7];
	int inx;
	int rc;

	TabStringExtract(item1->title, 7, tabs1);
	TabStringExtract(item2->title, 7, tabs2);
	for (inx = 0, rc = 0; inx < N_SORT && rc == 0; inx++) {
		switch (carInvSort[inx]) {
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

static void CarInvListLoad(void)
{
	wIndex_t selected; 

	qsort(&carItemInfo(0), carItemInfo_da.cnt, sizeof carItemInfo(0),
	      Cmp_carInvItem);
	FormControlShow(&carInvPG, I_CI_LIST, FALSE);
	wListClear(carInvPLs[I_CI_LIST].control);
	for (int inx = 0; inx < carItemInfo_da.cnt; inx++) {
		carItem_p item;
		item = carItemInfo(inx);
		CarInvLoadItem(item);
	}

	selected = wListGetIndex(carInvPLs[I_CI_LIST].control);
	FormControlShow(&carInvPG, I_CI_LIST, TRUE);
	FormControlActive(&carInvPG, I_CI_EDIT, selected >= 0 );
	FormControlActive(&carInvPG, I_CI_DELETE, selected >= 0);
	//wButtonSetLabel((wButton_p)(carInvPLs[I_CI_DELETE].control), "");
	FormControlActive(&carInvPG, I_CI_EXPORT_CSV, carItemInfo_da.cnt > 0);
	FormControlActive(&carInvPG, I_CI_PRINT, carItemInfo_da.cnt > 0);
	FormDialogOkActive(&carInvPG, FALSE);
}

static void CarInvLoadItem(
        carItem_p item)
{
	/* "Index", "Scale", "Manufacturer", "Type", "Part No", "Description", "Roadname", "RepMarks",
	   "Purch Price", "Curr Price", "Condition", "Purch Date", "Service Date", "Location", "Notes" */
	char* condition;
	char* location;
	char* manuf;
	char* road;
	char notes[100];
	tabString_t tabs[7];

	TabStringExtract(item->title, 7, tabs);
	if (item->data.notes) {
		strncpy(notes, item->data.notes, sizeof notes - 1);
		notes[sizeof notes - 1] = '\0';
	} else {
		notes[0] = '\0';
	}
	condition =
	        (item->data.condition < 10) ? N_("N/A") :
	        (item->data.condition < 30) ? N_("Poor") :
	        (item->data.condition < 50) ? N_("Fair") :
	        (item->data.condition < 70) ? N_("Good") :
	        (item->data.condition < 90) ? N_("Excellent") :
	        N_("Mint");

	char carLocation[30];
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
	sprintf(message,
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
	wListAddValue(carInvPLs[I_CI_LIST].control, message, NULL, item);
}

static void CarInvDlgUpdate(
        paramGroup_p pg,
        int inx,
        void* valueP)
{
	carItem_p item = NULL;
	wIndex_t cnt, selinx, selcnt;

	if (inx >= I_CI_SORT && inx < I_CI_SORT + N_SORT) {
		item = CarInvDlgFindCurrentItem();
		CarInvListLoad();
		if (item) {
			carInvInx = (wIndex_t)CarItemFindIndex(item);
			if (carInvInx >= 0) {
				FormLoadSingleControl(&carInvPG, I_CI_LIST);
			}
		}
	} else if (inx == I_CI_LIST) {
		cnt = wListGetCount(carInvPLs[I_CI_LIST].control);
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
		FormDialogOkActive(pg, nOnLayout == 1 && nOnShelf == 0);
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

static int CarInvSaveText(
        int files,
        char** fileName,
        void* data)
{
	FILE* f;
	carItem_p item;
	int inx;
	unsigned int widths[9];
	tabString_t tabs[7];
	char* cp0, * cp1;
	int len;

	CHECK(fileName != NULL);
	CHECK(files == 1);

	SetCurrentPath(CARSPATHKEY, fileName[0]);
	f = fopen(fileName[0], "w");
	if (f == NULL) {
		NoticeMessage(MSG_OPEN_FAIL, _("Continue"), NULL, _("Car Inventory"),
		              fileName[0], strerror(errno));
		return FALSE;
	}

	memset(widths, 0, sizeof widths);
	for (inx = 0; inx < carItemInfo_da.cnt; inx++) {
		unsigned int width;
		item = carItemInfo(inx);
		TabStringExtract(item->title, 7, tabs);
		sprintf(message, "%ld", item->index);
		width = (int)strlen(message);
		if (width > widths[0]) { widths[0] = width; }
		width = (int)strlen(GetScaleName(item->scaleInx)) + 1 + tabs[T_MANUF].len + 1 +
		        tabs[T_PART].len;
		if (width > widths[1]) { widths[1] = width; }
		if (tabs[T_PROTO].len > widths[2]) { widths[2] = tabs[T_PROTO].len; }
		width = tabs[T_REPMARK].len + tabs[T_NUMBER].len;
		if (tabs[T_REPMARK].len > 0 && tabs[T_NUMBER].len > 0) {
			width += 1;
		}
		if (width > widths[3]) { widths[3] = width; }
		if (item->data.purchDate > 0) { widths[4] = 8; }
		if (item->data.purchPrice > 0) {
			sprintf(message, "%0.2f", item->data.purchPrice);
			width = (int)strlen(message);
			if (width > widths[5]) { widths[5] = width; }
		}
		if (item->data.condition != 0) {
			widths[6] = 5;
		}
		if (item->data.currPrice > 0) {
			sprintf(message, "%0.2f", item->data.currPrice);
			width = (int)strlen(message);
			if (width > widths[7]) { widths[7] = width; }
		}
		if (item->data.serviceDate > 0) { widths[8] = 8; }
	}
	fprintf(f, "%-*.*s %-*.*s %-*.*s %-*.*s", widths[0], widths[0], "#", widths[1],
	        widths[1], "Part", widths[2], widths[2], "Description", widths[3], widths[3],
	        "Rep Mark");
	if (widths[4]) { fprintf(f, " %-*.*s", widths[4], widths[4], "PurDate"); }
	if (widths[5]) { fprintf(f, " %-*.*s", widths[5], widths[5], "PurPrice"); }
	if (widths[6]) { fprintf(f, " %-*.*s", widths[6], widths[6], "Cond"); }
	if (widths[7]) { fprintf(f, " %-*.*s", widths[7], widths[7], "CurPrice"); }
	if (widths[8]) { fprintf(f, " %-*.*s", widths[8], widths[8], "SrvDate"); }
	fprintf(f, "\n");

	for (inx = 0; inx < carItemInfo_da.cnt; inx++) {
		item = carItemInfo(inx);
		TabStringExtract(item->title, 7, tabs);
		sprintf(message, "%ld", item->index);
		fprintf(f, "%.*s", widths[0], message);
		sprintf(message, "%s %.*s %.*s", GetScaleName(item->scaleInx),
		        tabs[T_MANUF].len, tabs[T_MANUF].ptr, tabs[T_PART].len, tabs[T_PART].ptr);
		fprintf(f, " %-*s", widths[1], message);
		fprintf(f, " %-*.*s", widths[2], tabs[T_PROTO].len, tabs[T_PROTO].ptr);

		sprintf(message, "%.*s%s%.*s", tabs[T_REPMARK].len, tabs[T_REPMARK].ptr,
		        (tabs[T_REPMARK].len > 0
		         && tabs[T_NUMBER].len > 0) ? " " : "", tabs[T_NUMBER].len, tabs[T_NUMBER].ptr);
		fprintf(f, " %-*s", widths[3], message);
		if (widths[4] > 0) {
			if (item->data.purchDate > 0) {
				sprintf(message, "%ld", item->data.purchDate);
				fprintf(f, " %*.*s", widths[4], widths[4], message);
			} else {
				fprintf(f, " %*s", widths[4], " ");
			}
		}
		if (widths[5] > 0) {
			if (item->data.purchPrice > 0) {
				sprintf(message, "%0.2f", item->data.purchPrice);
				fprintf(f, " %*.*s", widths[5], widths[5], message);
			} else {
				fprintf(f, " %*s", widths[5], " ");
			}
		}
		if (widths[6] > 0) {
			if (item->data.condition != 0) {
				fprintf(f, " %-*.*s", widths[6], widths[6],
				        condListMap[MapCondition(item->data.condition)].name);
			} else {
				fprintf(f, " %*s", widths[6], " ");
			}
		}
		if (widths[7] > 0) {
			if (item->data.purchPrice > 0) {
				sprintf(message, "%0.2f", item->data.purchPrice);
				fprintf(f, " %*.*s", widths[7], widths[7], message);
			} else {
				fprintf(f, " %*s", widths[7], " ");
			}
		}
		if (widths[8] > 0) {
			if (item->data.serviceDate > 0) {
				sprintf(message, "%ld", item->data.serviceDate);
				fprintf(f, " %*.*s", widths[8], widths[8], message);
			} else {
				fprintf(f, " %*s", widths[8], " ");
			}
		}
		fprintf(f, "\n");
		if (item->data.notes) {
			cp0 = item->data.notes;
			while (1) {
				cp1 = strchr(cp0, '\n');
				if (cp1) {
					len = (int)(cp1 - cp0);
				} else {
					len = (int)strlen(cp0);
					if (len == 0) {
						break;
					}
				}
				fprintf(f, "%*.*s %*.*s\n", widths[0], widths[0], " ", len, len, cp0);
				if (cp1 == NULL) {
					break;
				}
				cp0 = cp1 + 1;
			}
		}
	}
	fclose(f);
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
        const int* map)
{
	int elem = 0;
	char* cp, * cq, * ptr;
	int rc, len;

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
	return elem;
}

static int CarInvImportCsv(
        int files,
        char** fileName,
        void* data)
{
	FILE* f;
	carItem_p item;
	tabString_t tabs[40], partTabs[7];
	int map[40];
	int i, j, numCol, len, rc;
	char* cp, * cq;
	long index;

	char title[STR_LONG_SIZE];

	carDim_t dim;
	FLOAT_T purchPrice, currPrice;
	int duplicateIndexError = 0;
	SCALEINX_T scale;
	carPart_p partP;
	int requiredCols;

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

	if (fgets(message, sizeof message, f) == NULL) {
		NoticeMessage(MSG_CARIMP_NO_DATA, _("Continue"), NULL);
		fclose(f);
		SetUserLocale();
		return FALSE;
	}
	for (j = 0; j < 40; j++) { map[j] = j; }
	numCol = ParseCsvLine(message, 40, tabs, map);
	if (numCol <= 0) {
		fclose(f);
		SetUserLocale();
		return FALSE;
	}
	for (j = 0; j < 40; j++) { map[j] = -1; }
	requiredCols = 0;
	for (i = 0; i < numCol; i++) {
		for (j = 0; j < COUNT(carCsvColumnTitles); j++) {
			if (TabStringCmp(carCsvColumnTitles[j], &tabs[i]) == 0) {
				if (map[i] >= 0) {
					NoticeMessage(MSG_CARIMP_DUP_COLUMNS, _("Continue"), NULL,
					              carCsvColumnTitles[j]);
					fclose(f);
					SetUserLocale();
					return FALSE;
				}
				map[i] = j;
				/*j = COUNT( carCsvColumnTitles );*/
				if (j == M_SCALE || j == M_PROTO || j == M_MANUF || j == M_PARTNO) {
					requiredCols++;
				}
			}
		}
		if (map[i] == -1) {
			tabs[i].ptr[tabs[i].len] = '\0';
			NoticeMessage(MSG_CARIMP_IGNORED_COLUMN, _("Continue"), NULL, tabs[i].ptr);
			tabs[i].ptr[tabs[i].len] = ',';
		}
	}
	if (requiredCols != 4) {
		NoticeMessage(MSG_CARIMP_MISSING_COLUMNS, _("Continue"), NULL);
		fclose(f);
		SetUserLocale();
		return FALSE;
	}
	while (fgets(message, sizeof message, f) != NULL) {
		int cnt;
		long type = 0;
		long options, color, condition, purchDate, srvcDate;

		cnt = ParseCsvLine(message, 40, tabs, map);
		if (cnt == -1) {
			NoticeMessage(MSG_CARIMP_MISSING_COLUMNS, _("OK"), NULL);
			fclose(f);
			SetUserLocale();
			return FALSE;
		}
		if (cnt > numCol) { cnt = numCol; }
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
				if (!duplicateIndexError) {
					NoticeMessage(MSG_CARIMP_DUP_INDEX, _("Ok"), NULL);
					duplicateIndexError++;
				}
				carDlgItemIndex = index;
			}
		}

		dim.carLength = TabGetFloat(&tabs[M_CARLENGTH]);
		dim.carWidth = TabGetFloat(&tabs[M_CARWIDTH]);
		dim.coupledLength = TabGetFloat(&tabs[M_CPLDLENGTH]);
		dim.truckCenter = TabGetFloat(&tabs[M_TRKCENTER]);
		dim.truckCenterOffset = TabGetFloat(&tabs[M_TRKOFFSET]);
		partP = NULL;
		if (tabs[M_MANUF].len > 0 && tabs[M_PARTNO].len > 0) {
			partP = CarPartFind(tabs[M_MANUF].ptr, tabs[M_MANUF].len, tabs[M_PARTNO].ptr,
			                    tabs[M_PARTNO].len, scale);
		}
		if (partP) {
			TabStringExtract(partP->title, 7, partTabs);
			if (tabs[M_PROTO].len == 0 && partTabs[T_PROTO].len > 0) { tabs[M_PROTO].ptr = partTabs[T_PROTO].ptr; tabs[M_PROTO].len = partTabs[T_PROTO].len; }
			if (tabs[M_DESC].len == 0 && partTabs[T_DESC].len > 0) { tabs[M_DESC].ptr = partTabs[T_DESC].ptr; tabs[M_DESC].len = partTabs[T_DESC].len; }
			if (tabs[M_ROADNAME].len == 0 && partTabs[T_ROADNAME].len > 0) { tabs[M_ROADNAME].ptr = partTabs[T_ROADNAME].ptr; tabs[M_ROADNAME].len = partTabs[T_ROADNAME].len; }
			if (tabs[M_REPMARK].len == 0 && partTabs[T_REPMARK].len > 0) { tabs[M_REPMARK].ptr = partTabs[T_REPMARK].ptr; tabs[M_REPMARK].len = partTabs[T_REPMARK].len; }
			if (tabs[M_NUMBER].len == 0 && partTabs[T_NUMBER].len > 0) { tabs[M_NUMBER].ptr = partTabs[T_NUMBER].ptr; tabs[M_NUMBER].len = partTabs[T_NUMBER].len; }
			if (dim.carLength <= 0) { dim.carLength = partP->dim.carLength; }
			if (dim.carWidth <= 0) { dim.carWidth = partP->dim.carWidth; }
			if (dim.coupledLength <= 0) { dim.coupledLength = partP->dim.coupledLength; }
			if (dim.truckCenter <= 0) { dim.truckCenter = partP->dim.truckCenter; }
			if (dim.truckCenterOffset < 0) { dim.truckCenterOffset = partP->dim.truckCenterOffset; }
		}
		if (dim.truckCenterOffset < 0) { dim.truckCenterOffset = 0; }
		cp = TabStringCpy(title, &tabs[M_MANUF]);
		*cp++ = '\t';
		cp = TabStringCpy(cp, &tabs[M_PROTO]);
		*cp++ = '\t';
		cp = TabStringCpy(cp, &tabs[M_DESC]);
		*cp++ = '\t';
		cp = TabStringCpy(cp, &tabs[M_PARTNO]);
		*cp++ = '\t';
		cp = TabStringCpy(cp, &tabs[M_ROADNAME]);
		*cp++ = '\t';
		cp = TabStringCpy(cp, &tabs[M_REPMARK]);
		*cp++ = '\t';
		cp = TabStringCpy(cp, &tabs[M_NUMBER]);
		*cp = '\0';
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
			rc = NoticeMessage(MSG_CARIMP_MISSING_DIMS, _("Yes"), _("No"), message);
			if (rc <= 0) {
				fclose(f);
				SetUserLocale();
				return FALSE;
			}
			continue;
		}
		item = CarItemNew(NULL, PARAM_CUSTOM, index, scale, title, options, type,
		                  &dim, wDrawFindColor(color),
		                  purchPrice, currPrice, condition, purchDate, srvcDate);
		if (tabs[M_NOTES].len > 0) {
			item->data.notes = cp = MyMalloc((tabs[M_NOTES].len + 2));
			for (cq = tabs[M_NOTES].ptr, len = tabs[M_NOTES].len; *cq && len; ) {
				if (strncmp(cq, "<NL>", 4) == 0) {
					*cp++ = '\n';
					cq += 4;
					len -= 4;
				} else {
					*cp++ = *cq++;
					len -= 1;
				}
			}
		}
		SetFileChanged();
	}
	fclose(f);
	SetUserLocale();
	CarInvListLoad();
	return TRUE;
}

static int CarInvExportCsv(
        int files,
        char** fileName,
        void* data)
{
	FILE* f;
	tabString_t tabs[7];

	CHECK(fileName != NULL);
	CHECK(files == 1);
	SetCurrentPath(CARSPATHKEY, fileName[0]);

	f = fopen(fileName[0], "w");
	if (f == NULL) {
		NoticeMessage(MSG_OPEN_FAIL, _("Continue"), NULL, _("Export Cars"),
		              fileName[0], strerror(errno));
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
	return TRUE;
}


static struct wFilSel_t* carInvExportCsv_fs;
static void CarInvDlgExportCsv(void)
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
static void CarInvDlgSaveText(void)
{
	if (carInvSaveText_fs == NULL)
		carInvSaveText_fs = wFilSelCreate(mainW, FS_SAVE, 0, _("List Cars"),
		                                  "Text (*.txt)|*.txt", CarInvSaveText, NULL);
	wFilSelect(carInvSaveText_fs, GetCurrentPath(CARSPATHKEY));
}




static struct wFilSel_t* carInvImportCsv_fs;
static void CarInvDlgImportCsv(void)
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
	CarInvListLoad();
	carInvInx = (wIndex_t)CarItemFindIndex(item);
	if (carInvInx >= 0) {
		FormLoadSingleControl(&carInvPG, I_CI_LIST);
	}
}


void CarInvListUpdate(carItem_p item)
{
	CarInvListLoad();
	carInvInx = (wIndex_t)CarItemFindIndex(item);
	if (carInvInx >= 0) {
		FormLoadSingleControl(&carInvPG, I_CI_LIST);
	}
}

static void CarInvDlgFind(void* unused)
{
	carItem_p item = CarInvDlgFindCurrentItem();
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

EXPORT void DoCarDlg(void* unused)
{
	if (carInvPG.win == NULL) {
		FormCreateDialog(&carInvPG, MakeWindowTitle(_("Car Inventory")), 
						  _("Find"), CarInvDlgFind, 
						  _("Done"), FormCancel_Current, TRUE, 
		                  0, 
						  CarInvDlgUpdate);

		FormDialogOkActive(&carInvPG, FALSE);
	}
	CarInvListLoad();
	wShow(carInvPG.win);
}


void InitCarInvDlg(void) 
{
	FormRegister( &carInvPG );
	log_carInvDlg = LogFindIndex("carInvDlg");
}