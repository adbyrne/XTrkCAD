/** \file jsonnoteui.c
 * View for the JSON note
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2026 XTrkCAD contributors
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

#include "cJSON.h"
#include "custom.h"
#include "dynstring.h"
#include "include/jsonnoteerror.h"
#include "misc.h"
#include "note.h"
#include "form.h"
#include "shortentext.h"
#include "track.h"
#include "cundo.h"

/* Debug log category for this file -- run with -d jsonnote=1 -l <file> and
 * tail it while clicking through the JSON Note dialog. Same pattern as
 * reports.c's log_reports (see docs/doxygen/creating-a-report.md's "Debug
 * logging" section). */
static int log_jsonnote = -1;
#define JSONNOTE_LOG(...) do { \
		if ( log_jsonnote < 0 ) { log_jsonnote = LogFindIndex( "jsonnote" ); } \
		LOG( log_jsonnote, 1, ( __VA_ARGS__ ) ) \
	} while (0)

/* JSON Note's text can hold, at most, this many raw bytes -- past this, the
 * ESCAPED-on-disk form ConvertToEscapedText() produces could overflow the
 * fixed-size decode buffer GetArgs()'s 'q' format code writes into
 * (STR_HUGE_SIZE, misc.c/common.h) on the next file load, which is a hard
 * CHECK()/AbortProg() crash, not a graceful truncation. Traced precisely
 * (not estimated): that decode buffer's usage equals the raw text's own
 * length plus one byte per literal backslash in it (the CSV "" quote-
 * doubling this same raw text needs on write exactly cancels out against
 * GetArgs()'s own "" un-doubling on read) -- so capping the raw length well
 * under STR_HUGE_SIZE is an exact, not approximate, guarantee. */
#define JSONNOTE_MAXTEXTLENGTH (STR_HUGE_SIZE - 1024)

struct {
	coOrd pos;
	int layer;
	track_p trk;
	long fieldObjectInx;
	char fieldName[STR_SHORT_SIZE];
	char fieldValue[STR_LONG_SIZE];
} jsonNoteData;

/* Structured field editor's Object dropdown: index 0 is always ROOT (an
 * empty key path); indices 1..jsonFieldObjectCount-1 are the keys of every
 * object-valued child found directly on ROOT (one level of nesting only --
 * see JsonFieldRebuildObjectList()'s doc comment for why deeper isn't
 * supported). Rebuilt from the note's *current* text -- see that function's
 * doc comment for the "text box is the only source of truth" contract this
 * whole editor follows. */
#define JSONFIELD_MAXOBJECTS (32)
static char jsonFieldObjectKeys[JSONFIELD_MAXOBJECTS][STR_SHORT_SIZE];
static int jsonFieldObjectCount;

/**
 * Precise (not just length-based) check for whether \p buf's fixed-size
 * decode buffer usage on the next file load would risk overflowing
 * GetArgs()'s 'q'-format message[STR_HUGE_SIZE]. ConvertToEscapedText()
 * (misc.c) doubles four characters on write -- backslash, newline, tab, and
 * double-quote -- but GetArgs()'s 'q' read-side loop only reduces the
 * doubled-quote pair back down; the doubled backslash/newline/tab pass
 * through that loop unreduced (their un-doubling happens later, in
 * ConvertFromEscapedText(), into a separate allocation that doesn't count
 * against this buffer). So each backslash/newline/tab byte in \p buf adds
 * one byte of decode-buffer usage the raw length alone doesn't account for
 * -- a JSON body can legitimately contain many of them (heavy backslash
 * content, or cJSON_Print()'s own real newlines/tabs from pretty-printing)
 * while staying short and perfectly valid.
 *
 * \param buf IN raw (unescaped) text
 * \param len IN length of buf
 * \return TRUE if safe to save
 */
static BOOL_T
JsonNoteLengthOk(const char *buf, int len)
{
	int extra = 0;
	for (int i = 0; i < len; i++) {
		if (buf[i] == '\\' || buf[i] == '\n' || buf[i] == '\t') {
			extra++;
		}
	}
	return (len + extra) < JSONNOTE_MAXTEXTLENGTH;
}

static void JsonNoteValidate(void *junk);
static void JsonNoteFormat(void *junk);
static void JsonFieldSave(void *junk);
static void JsonFieldDelete(void *junk);
static wBool_t JsonDlgUpdate(paramGroup_p pg, int inx, void *valueP);

static paramTextData_t jsonNoteTextData = { 300, 150 };
static paramFloatRange_t noRangeCheck = { 0.0, 0.0, 80, PDO_NORANGECHECK_HIGH | PDO_NORANGECHECK_LOW };

static paramData_t jsonNotePLs[] = {
#define I_ORIGX (0)
	/*0*/ { PD_FLOAT, &jsonNoteData.pos.x, "origx", PDO_DIM|PDO_NOPREF, &noRangeCheck },
#define I_ORIGY (1)
	/*1*/ { PD_FLOAT, &jsonNoteData.pos.y, "origy", PDO_DIM|PDO_NOPREF, &noRangeCheck},
#define I_LAYER (2)
	/*2*/ { PD_COMBOLIST, &jsonNoteData.layer, "layer", PDO_NOPREF, I2VP(150), "Layer", 0 },
#define I_TEXT (3)
	/*3*/ { PD_TEXT, NULL, "text", PDO_NOPREF, &jsonNoteTextData, N_("JSON") },
#define I_VALIDATE (4)
	/*4*/ { PD_BUTTON, JsonNoteValidate, "validate", 0L, NULL },
#define I_FORMAT (5)
	/*5*/ { PD_BUTTON, JsonNoteFormat, "format", PDO_DLGHORZ, NULL },
#define I_JSONOBJ (6)
	/*6*/ { PD_COMBOLIST, &jsonNoteData.fieldObjectInx, "jsonobj", PDO_NOPREF | PDO_LISTINDEX, I2VP(120), N_("Object") },
#define I_JSONNAME (7)
	/*7*/ { PD_STRING, &jsonNoteData.fieldName, "jsonname", PDO_NOPREF, I2VP(100), N_("Name"), 0, 0, sizeof jsonNoteData.fieldName },
#define I_JSONVALUE (8)
	/*8*/ { PD_STRING, &jsonNoteData.fieldValue, "jsonvalue", PDO_NOPREF, I2VP(140), N_("Value"), 0, 0, sizeof jsonNoteData.fieldValue },
#define I_JSONSAVE (9)
	/*9*/ { PD_BUTTON, JsonFieldSave, "jsonfieldsave", 0L, NULL },
#define I_JSONDELETE (10)
	/*10*/ { PD_BUTTON, JsonFieldDelete, "jsonfielddelete", PDO_DLGHORZ, NULL },
#define I_JSONSTATUS (11)
	/*11*/ { PD_MESSAGE, "", "jsonstatus", 0, I2VP(50) },
};

static paramGroup_t jsonNotePG = { "jsonNote", PGO_FULLDIALOGFROMBUILDER, jsonNotePLs, COUNT( jsonNotePLs ) };
static wControl_p jsonNoteW;

#define jsonTextEntry	(jsonNotePLs[I_TEXT].control)

BOOL_T IsJsonNote(track_p trk)
{
	const struct extraDataNote_t * xx = GET_EXTRA_DATA( trk, T_NOTE,
	                                    extraDataNote_t );

	return(xx->op == OP_NOTEJSON );
}

/**
 * Turn a parse failure into a short status message saying where parsing
 * stopped (and, for recognised mistakes, what's wrong), plus a longer
 * suggestion for the text box's tooltip. The status is shown in the dialog
 * itself so the reason is visible without hovering (dev-ML #4404).
 *
 * \param text IN the text that failed to parse
 * \param errPtr IN where cJSON_ParseWithOpts() stopped
 * \param tip OUT suggested fix, for the tooltip
 * \return the status message; both strings are in static buffers valid
 *         until the next call
 */
static const char *
JsonNoteDescribeError(const char *text, const char *errPtr, const char **tip)
{
	static char msg[256];
	static char tipBuf[400];
	jsonNoteErrorInfo_t info;
	const char *what = NULL;
	const char *fix = NULL;

	JsonNoteLocateError(text, errPtr, &info);
	switch (info.cause) {
	case JSONNOTEERR_EMPTY:
		snprintf(msg, sizeof msg, "%s", _("Empty -- enter a JSON object"));
		snprintf(tipBuf, sizeof tipBuf, "%s",
		         _("A JSON Note holds one JSON object, e.g. {\"kind\": \"station\", \"id\": \"WP\"}."));
		*tip = tipBuf;
		return msg;
	case JSONNOTEERR_UNEXPECTED_END:
		snprintf(msg, sizeof msg, "%s", _("Invalid JSON: the text ends too soon"));
		snprintf(tipBuf, sizeof tipBuf, "%s",
		         _("Every { needs a matching }, every [ a matching ], and every string a closing \". Check the end of the text."));
		*tip = tipBuf;
		return msg;
	case JSONNOTEERR_TYPOGRAPHIC_QUOTE:
		what = _("typographic (curly) quote");
		fix = _("Word processors and email programs replace \" with curly quotes. Retype the quote here as a plain \".");
		break;
	case JSONNOTEERR_NONBREAKING_SPACE:
		what = _("non-breaking space");
		fix = _("Text copied from web pages can contain non-breaking spaces. Delete it and type an ordinary space.");
		break;
	case JSONNOTEERR_SINGLE_QUOTE:
		what = _("single quote");
		fix = _("JSON strings and keys need double quotes: \"text\", not 'text'.");
		break;
	case JSONNOTEERR_MISSING_COMMA:
		snprintf(msg, sizeof msg,
		         _("Invalid JSON at line %d, column %d: missing comma before this?"),
		         info.line, info.column);
		snprintf(tipBuf, sizeof tipBuf,
		         _("Items in an object or array are separated by commas. Add a comma at line %d, column %d, after the previous item."),
		         info.hintLine, info.hintColumn);
		*tip = tipBuf;
		return msg;
	case JSONNOTEERR_MISSING_COLON:
		snprintf(msg, sizeof msg,
		         _("Invalid JSON at line %d, column %d: missing colon after the key?"),
		         info.line, info.column);
		snprintf(tipBuf, sizeof tipBuf,
		         _("Each key is followed by a colon and its value: \"key\": value. Add a colon at line %d, column %d."),
		         info.hintLine, info.hintColumn);
		*tip = tipBuf;
		return msg;
	case JSONNOTEERR_TRAILING_COMMA:
		snprintf(msg, sizeof msg,
		         _("Invalid JSON at line %d, column %d: comma before the closing bracket"),
		         info.hintLine, info.hintColumn);
		snprintf(tipBuf, sizeof tipBuf, "%s",
		         _("JSON doesn't allow a comma after the last item in an object or array. Delete that comma."));
		*tip = tipBuf;
		return msg;
	case JSONNOTEERR_UNQUOTED_KEY:
		what = _("key without double quotes");
		fix = _("Object keys must be in double quotes: {\"kind\": \"station\"}, not {kind: \"station\"}.");
		break;
	case JSONNOTEERR_BAD_BACKSLASH:
		what = _("backslash");
		fix = _("Inside a string, a backslash starts an escape such as \\n. For a Windows path, double each backslash (C:\\\\data) or use forward slashes (C:/data).");
		break;
	case JSONNOTEERR_WRONG_LITERAL:
		what = _("not a JSON value");
		fix = _("JSON's literal values are lowercase true, false and null. Words such as True, None or undefined, and NaN, aren't valid; quote them if they're meant as text.");
		break;
	case JSONNOTEERR_COMMENT:
		what = _("comment");
		fix = _("JSON has no comments. Remove the // or /* */ text, or keep the information as a field, e.g. \"comment\": \"...\".");
		break;
	case JSONNOTEERR_TRAILING_CONTENT:
		what = _("text after the object");
		fix = _("A JSON Note holds exactly one object. Put everything inside the outer { }, or use a separate note.");
		break;
	default:
		snprintf(msg, sizeof msg, _("Invalid JSON at line %d, column %d, near: %s"),
		         info.line, info.column, info.snippet);
		snprintf(tipBuf, sizeof tipBuf, "%s",
		         _("Check the text at that position. Common causes: a misspelled value, a missing quote, or an extra or missing bracket."));
		*tip = tipBuf;
		return msg;
	}
	snprintf(msg, sizeof msg, _("Invalid JSON at line %d, column %d: %s"),
	         info.line, info.column, what);
	snprintf(tipBuf, sizeof tipBuf, "%s", fix);
	*tip = tipBuf;
	return msg;
}

/**
 * Read the dialog's current text and report whether it's a valid JSON
 * *object* (not just valid JSON -- MCP's own note dispatch, and this
 * feature's whole premise, both require an object; a bare array/string/
 * number/bool/null would silently parse but then vanish from every
 * downstream report with no error surfaced anywhere) and within the
 * length this feature can safely round-trip. Nothing may follow the
 * object: a lenient parse would silently drop it on save.
 *
 * \param msg OUT short status for the dialog: the reason when returning
 *        FALSE; "Valid JSON object" or a duplicate-key warning when TRUE
 * \param tip OUT (may be NULL) longer suggestion for the tooltip
 * \return TRUE if the current text is acceptable to save
 */
static BOOL_T
JsonNoteIsValid(const char **msg, const char **tip)
{
	static char warnMsg[256];
	static char warnTip[300];
	const char *ignoredTip;
	if (tip == NULL) {
		tip = &ignoredTip;
	}

	int len = wTextGetSize(jsonTextEntry);
	char *buf = MyMalloc(len + 2);
	wTextGetText(jsonTextEntry, buf, len);

	if (!JsonNoteLengthOk(buf, len)) {
		MyFree(buf);
		*msg = _("Too long -- would risk a crash reopening this file");
		*tip = _("Split the information across several notes.");
		return FALSE;
	}

	const char *errPtr = NULL;
	cJSON *parsed = cJSON_ParseWithOpts(buf, &errPtr, TRUE);

	if (parsed == NULL) {
		*msg = JsonNoteDescribeError(buf, errPtr, tip);
		MyFree(buf);
		return FALSE;
	}
	MyFree(buf);
	if (!cJSON_IsObject(parsed)) {
		cJSON_Delete(parsed);
		*msg = _("Must be a JSON object, e.g. {\"kind\": \"...\"} -- not an array/string/number");
		*tip = _("Wrap the content in { } and give it a key, e.g. {\"items\": [1, 2]}.");
		return FALSE;
	}

	const char *dup = JsonNoteFindDuplicateKey(parsed);
	if (dup != NULL) {
		snprintf(warnMsg, sizeof warnMsg,
		         _("Valid, but key \"%s\" appears twice"), dup);
		snprintf(warnTip, sizeof warnTip,
		         _("Only the first \"%s\" is used; later ones are ignored. Remove or rename the duplicate."),
		         dup);
		*msg = warnMsg;
		*tip = warnTip;
	} else {
		*msg = _("Valid JSON object");
		*tip = _("Format re-indents the text. Done saves the note.");
	}
	cJSON_Delete(parsed);
	return TRUE;
}

/**
 * Rebuild the structured field editor's Object dropdown from \p root (ROOT
 * itself, plus every object-valued key found directly on ROOT).
 *
 * Deliberately one level deep only, not a recursive tree-walk: none of the
 * real JSON Note kinds (station/industry/storage/yard_track/house_track/
 * reference) nest an object inside an object, so deeper traversal would be
 * speculative complexity with no current caller -- revisit if a future kind
 * ever needs real nesting.
 *
 * \param root IN the note's current text, already parsed (not consumed;
 *        caller still owns and must cJSON_Delete it)
 */
static void
JsonFieldRebuildObjectList(cJSON *root)
{
	wControl_p ctrl = jsonNotePLs[I_JSONOBJ].control;
	wListClear(ctrl);
	wComboBoxAddValue(ctrl, _("ROOT"), I2VP(0));
	jsonFieldObjectKeys[0][0] = '\0';
	jsonFieldObjectCount = 1;

	cJSON *child = root->child;
	while (child && jsonFieldObjectCount < JSONFIELD_MAXOBJECTS) {
		if (cJSON_IsObject(child) && child->string) {
			wComboBoxAddValue(ctrl, child->string, I2VP(jsonFieldObjectCount));
			strncpy(jsonFieldObjectKeys[jsonFieldObjectCount], child->string,
			        STR_SHORT_SIZE - 1);
			jsonFieldObjectKeys[jsonFieldObjectCount][STR_SHORT_SIZE - 1] = '\0';
			jsonFieldObjectCount++;
		}
		child = child->next;
	}

	if (jsonNoteData.fieldObjectInx >= jsonFieldObjectCount) {
		jsonNoteData.fieldObjectInx = 0;
	}
	wListSetIndex(ctrl, (int)jsonNoteData.fieldObjectInx);
}

/**
 * The cJSON object node currently selected in the Object dropdown -- \p root
 * itself for ROOT (index 0), or the matching direct child for any other
 * index. Returns NULL if the previously-selected key no longer exists on
 * \p root (e.g. it was removed by a direct text-box edit since the dropdown
 * was last rebuilt).
 *
 * \param root IN the note's current text, already parsed
 * \return the selected object node, or NULL
 */
static cJSON *
JsonFieldSelectedObject(cJSON *root)
{
	int inx = (int)jsonNoteData.fieldObjectInx;
	if (inx <= 0 || inx >= jsonFieldObjectCount) {
		return root;
	}
	cJSON *obj = cJSON_GetObjectItemCaseSensitive(root, jsonFieldObjectKeys[inx]);
	return (obj && cJSON_IsObject(obj)) ? obj : NULL;
}

/**
 * Report a structured-field-editor error via the Name field's tooltip/
 * hilite, matching JsonNoteValidate()'s error-reporting mechanism for the
 * main text field. Does not touch FormDialogOkActive -- Save/Delete on this
 * row never blocks the dialog's own OK button, they simply refuse to act.
 *
 * \param msg IN user-facing reason
 */
static void
JsonFieldReportError(const char *msg)
{
	JSONNOTE_LOG("jsonfield: %s\n", msg);
	paramData_p p = &jsonNotePLs[I_JSONNAME];
	wTooltipSetText(p->control, msg);
	wControlHilite(p->control, TRUE);
}

/**
 * Shared Save/Delete implementation for the structured field editor. The
 * multi-line JSON text box is the single source of truth for the note's
 * content at all times -- this control never maintains its own parallel
 * data model. Every call re-parses the text box's *current* contents with
 * cJSON, mutates the selected object's requested key only, pretty-prints
 * the result (matching the Format button's own output), and writes it back
 * into the text box. Undo/Redo, the oversized-body guard
 * (JsonNoteLengthOk()), and Validate-on-save all operate purely on that
 * text and require no changes to support this control.
 *
 * The value field is untyped: on Save, an entered value that parses as a
 * bare JSON literal (a number, true, false, or null) is stored as that
 * literal; anything else -- including text that merely looks like a quoted
 * JSON string -- is stored as-is as a JSON string. Arrays, nested-object
 * creation, and any value more complex than a scalar are out of scope for
 * this control; author those directly in the text box instead.
 *
 * \param isDelete IN FALSE to add-or-update the Name/Value pair on the
 *        selected object, TRUE to remove Name from it (Value is ignored)
 */
static void
JsonFieldApplyEdit(BOOL_T isDelete)
{
	/* Object/Name/Value are plain GtkEntry/GtkComboBox widgets -- unlike
	 * I_TEXT (whose own "changed" signal we already can't rely on, see
	 * JsonDlgUpdate()'s comment), nothing pushes their live contents into
	 * jsonNoteData automatically on every keystroke. Pull the current
	 * widget values explicitly before reading them, matching the same
	 * pattern other button-triggered handlers elsewhere in the app use
	 * (e.g. dlayer.c's own I_LAYER-adjacent buttons). */
	FormFetchData(&jsonNotePG);

	const char *errMsg = NULL;
	if (!JsonNoteIsValid(&errMsg, NULL)) {
		JsonFieldReportError(errMsg);
		return;
	}
	const char *name = jsonNoteData.fieldName;
	if (name[0] == '\0') {
		JsonFieldReportError(_("Name is required"));
		return;
	}

	int len = wTextGetSize(jsonTextEntry);
	char *buf = MyMalloc(len + 2);
	wTextGetText(jsonTextEntry, buf, len);
	cJSON *root = cJSON_Parse(buf);
	MyFree(buf);
	if (root == NULL || !cJSON_IsObject(root)) {
		if (root) {
			cJSON_Delete(root);
		}
		JsonFieldReportError(_("Current JSON is not a valid object"));
		return;
	}

	cJSON *obj = JsonFieldSelectedObject(root);
	if (obj == NULL) {
		cJSON_Delete(root);
		JsonFieldReportError(
		        _("Selected object no longer exists -- Format/Validate first"));
		return;
	}

	if (isDelete) {
		if (!cJSON_HasObjectItem(obj, name)) {
			cJSON_Delete(root);
			JsonFieldReportError(_("No such name on the selected object"));
			return;
		}
		cJSON_DeleteItemFromObjectCaseSensitive(obj, name);
	} else {
		cJSON *valueNode;
		if (jsonNoteData.fieldValue[0] == '\0') {
			/* Blank Value is repurposed as "start a new nested object here"
			 * rather than the arguably-useless empty string -- the common
			 * real need (confirmed live, 2026-09-16: a user immediately
			 * wanted to build a nested "spots": {...} group) is starting a
			 * new sub-group to fill in via this same row afterward, not
			 * deliberately storing "". Since Save already re-syncs the
			 * Object dropdown on success (see below), the new object is
			 * immediately selectable with no extra step. */
			valueNode = cJSON_CreateObject();
		} else {
			valueNode = cJSON_Parse(jsonNoteData.fieldValue);
			if (valueNode != NULL &&
			    !(cJSON_IsNumber(valueNode) || cJSON_IsBool(valueNode)
			      || cJSON_IsNull(valueNode))) {
				cJSON_Delete(valueNode);
				valueNode = NULL;
			}
			if (valueNode == NULL) {
				valueNode = cJSON_CreateString(jsonNoteData.fieldValue);
			}
		}
		if (cJSON_HasObjectItem(obj, name)) {
			cJSON_ReplaceItemInObjectCaseSensitive(obj, name, valueNode);
		} else {
			cJSON_AddItemToObject(obj, name, valueNode);
		}
	}

	char *pretty = cJSON_Print(root);
	cJSON_Delete(root);
	if (pretty == NULL || !JsonNoteLengthOk(pretty, (int)strlen(pretty))) {
		if (pretty) {
			cJSON_free(pretty);
		}
		JsonFieldReportError(_("Too long -- would risk a crash reopening this file"));
		return;
	}

	wTextClear(jsonTextEntry);
	wTextAppend(jsonTextEntry, pretty);
	JSONNOTE_LOG("jsonfield: %s '%s' on object #%ld, %d bytes\n",
	             isDelete ? "deleted" : "saved", name, jsonNoteData.fieldObjectInx,
	             (int)strlen(pretty));
	cJSON_free(pretty);

	/* Success: clear the Name/Value row (Object selection is left as-is --
	 * adding several pairs to the same object in a row is the common case)
	 * and re-validate, which also clears any stale error hilite on I_TEXT/
	 * I_JSONNAME. Undo/Redo, Validate-on-save, and the oversized-body guard
	 * all continue to work unmodified -- see this function's own doc
	 * comment.
	 *
	 * wEntrySetValue() (wlib's entry.c) deliberately no-ops on a focused
	 * entry -- "contents should not be changed programmatically while the
	 * user is editing it". Worse, entry.c's own focus-out handler
	 * (entryFocusOutEvent) copies the *widget's still-displayed* text back
	 * into the bound struct field the moment focus leaves it -- so moving
	 * focus away *before* clearing the struct would just have the focus-out
	 * handler stomp the clear right back to the old text. Order matters:
	 * move focus away FIRST (onto the main JSON text box -- not the Object
	 * dropdown, which like the Layer combo it's modeled on has GTK
	 * can-focus=False in jsonNote.ui and can never actually receive focus)
	 * and wFlush() the resulting focus-out through, THEN clear the struct
	 * fields, THEN FormLoadControls() -- so nothing is left with focus (or
	 * pending a focus-out) that could re-sync stale text over the clear. */
	wControlSetFocus(jsonTextEntry);
	wFlush();
	jsonNoteData.fieldName[0] = '\0';
	jsonNoteData.fieldValue[0] = '\0';
	paramData_p nameField = &jsonNotePLs[I_JSONNAME];
	wControlHilite(nameField->control, FALSE);
	FormLoadControls(&jsonNotePG);
	JsonNoteValidate(NULL);
}

/**
 * Callback for the structured field editor's Save button (add-or-update,
 * an upsert -- there is no separate Add-only/Update-only action, since
 * distinguishing them would need the same existence check either way and
 * upsert removes an entire class of "wrong button for this key" user
 * error). See JsonFieldApplyEdit()'s doc comment for the full contract.
 *
 * \param junk unused
 */
static void
JsonFieldSave(void *junk)
{
	(void)junk;
	JsonFieldApplyEdit(FALSE);
}

/**
 * Callback for the structured field editor's Delete button. See
 * JsonFieldApplyEdit()'s doc comment for the full contract.
 *
 * \param junk unused
 */
static void
JsonFieldDelete(void *junk)
{
	(void)junk;
	JsonFieldApplyEdit(TRUE);
}

/**
 * Callback for the Validate button: check the current text and reflect the
 * result via the same bInvalid/hilite/OK-active mechanism filenoteui.c
 * already uses for its I_PATH field. Also re-syncs the structured field
 * editor's Object dropdown against the now-confirmed-valid text, since that
 * control has no other reliable hook to notice a direct text-box edit that
 * added/removed a nested object (see JsonDlgUpdate()'s I_TEXT case comment
 * on why "changed" doesn't fire on every keystroke).
 *
 * \param junk unused
 */
static void
JsonNoteValidate(void *junk)
{
	(void)junk;
	const char *msg = NULL;
	const char *tip = NULL;
	paramData_p p = &jsonNotePLs[I_TEXT];

	/* The status line says what's wrong (always visible); hovering over
	 * the text box shows a suggested fix. The tooltip is also reset when
	 * the text is valid, so it never keeps showing a fixed error. */
	if (!JsonNoteIsValid(&msg, &tip)) {
		JSONNOTE_LOG("jsonnote: invalid -- %s | tip: %s\n", msg, tip);
		p->bInvalid = TRUE;
		wTooltipSetText(p->control, tip);
		wControlHilite(p->control, TRUE);
		wMessageSetValue(jsonNotePLs[I_JSONSTATUS].control, msg);
		FormDialogOkActive(&jsonNotePG, FALSE);
	} else {
		JSONNOTE_LOG("jsonnote: valid -- %s\n", msg);
		p->bInvalid = FALSE;
		wTooltipSetText(p->control, tip);
		wControlHilite(p->control, FALSE);
		wMessageSetValue(jsonNotePLs[I_JSONSTATUS].control, msg);
		FormDialogOkActive(&jsonNotePG, TRUE);

		int len = wTextGetSize(jsonTextEntry);
		char *buf = MyMalloc(len + 2);
		wTextGetText(jsonTextEntry, buf, len);
		cJSON *root = cJSON_Parse(buf);
		MyFree(buf);
		if (root != NULL) {
			JsonFieldRebuildObjectList(root);
			cJSON_Delete(root);
		}
	}
}

/**
 * Callback for the Format button: pretty-print the current text via cJSON
 * if it's valid (same object-required check as Validate); otherwise
 * delegate to Validate's error-reporting path instead of silently doing
 * nothing.
 *
 * \param junk unused
 */
static void
JsonNoteFormat(void *junk)
{
	JSONNOTE_LOG("jsonnote: Format clicked\n");
	int len = wTextGetSize(jsonTextEntry);
	char *buf = MyMalloc(len + 2);
	wTextGetText(jsonTextEntry, buf, len);

	if (!JsonNoteLengthOk(buf, len)) {
		MyFree(buf);
		JsonNoteValidate(junk);
		return;
	}

	/* Strict, like Validate: a lenient parse would pretty-print only the
	 * first object and silently drop anything after it. */
	cJSON *parsed = cJSON_ParseWithOpts(buf, NULL, TRUE);
	MyFree(buf);

	if (parsed == NULL || !cJSON_IsObject(parsed)) {
		if (parsed) {
			cJSON_Delete(parsed);
		}
		JsonNoteValidate(junk);
		return;
	}

	char *pretty = cJSON_Print(parsed);
	cJSON_Delete(parsed);

	if (pretty == NULL || !JsonNoteLengthOk(pretty, (int)strlen(pretty))) {
		/* Pretty-printing indentation can push a body that was just under
		 * the length limit over it -- re-check after formatting, not just
		 * before, and don't silently keep an oversized result. */
		cJSON_free(pretty);
		JsonNoteValidate(junk);
		return;
	}

	wTextClear(jsonTextEntry);
	wTextAppend(jsonTextEntry, pretty);
	JSONNOTE_LOG("jsonnote: Format applied, %d bytes\n", (int)strlen(pretty));
	cJSON_free(pretty);

	/* Re-validate the formatted text: it's still valid (Format only changes
	 * whitespace), but this also refreshes the status line, the tooltip, and
	 * any duplicate-key warning instead of leaving a stale message. */
	JsonNoteValidate(junk);
}

/**
 * Handle the dialog's per-field-change events. Re-validates on every edit
 * to the JSON text itself, matching filenoteui.c's FileDlgUpdate()
 * precedent for its own must-be-well-formed field (I_PATH) -- block Save,
 * don't just warn, on invalid JSON.
 */
static wBool_t
JsonDlgUpdate(paramGroup_p pg, int inx, void *valueP)
{
	(void)pg;
	(void)valueP;

	switch (inx) {
	case I_TEXT:
		JsonNoteValidate(NULL);
		break;
	case I_ORIGX:
	case I_ORIGY:
		// TODO: Redraw bitmap at new location
		break;
	default:
		break;
	}

	return(TRUE);
}

/**
 * Handle OK button: create or update the note, then close the dialog.
 * Relies solely on the disabled-OK-button mechanism (JsonDlgUpdate/
 * JsonNoteValidate already keep it accurate on every keystroke) rather
 * than re-validating a third time here -- matching filenoteui.c's
 * FileEditOK(), which does the same.
 *
 * \param junk unused
 */
static void
JsonEditOK(void *junk)
{
	/* Defensive re-validation, not just relying on the disabled-OK-button
	 * mechanism (filenoteui.c's precedent this was originally modeled on):
	 * confirmed empirically that GtkTextView's "changed" signal (wlib's
	 * text.c textChanged()) only sets an internal flag, it does NOT fire
	 * this dialog's changeProc the way a plain GtkEntry field does (unlike
	 * filenoteui.c's I_PATH) -- so JsonDlgUpdate's I_TEXT case, and the
	 * FormDialogOkActive(FALSE) it would trigger, never actually runs from
	 * typing alone, only from an explicit Validate/Format button click.
	 * Without this check, a user who types invalid JSON and clicks Done
	 * directly (never clicking Validate first) could save invalid JSON. */
	const char *errMsg = NULL;
	if (!JsonNoteIsValid(&errMsg, NULL)) {
		JsonNoteValidate(junk);
		return;
	}

	(void)junk;
	track_p trk = jsonNoteData.trk;
	JSONNOTE_LOG("jsonnote: Done clicked, %s note, %d bytes\n",
	             trk == NULL ? "new" : "existing", wTextGetSize(jsonTextEntry));
	if ( trk == NULL ) {
		trk = NewNote( -1, jsonNoteData.pos, OP_NOTEJSON );
	} else {
		if ( ! descUndoStarted ) {
			UndoStart( _("Update JSON Note"), "Update JSON Note" );
			descUndoStarted = TRUE;
		}
		UndoModify( trk );
	}
	struct extraDataNote_t * xx = GET_EXTRA_DATA( trk, T_NOTE, extraDataNote_t );
	xx->pos = jsonNoteData.pos;
	SetTrkLayer( trk, jsonNoteData.layer );

	int len = wTextGetSize(jsonTextEntry);
	UndoDeferFree( xx->noteData.text );
	xx->noteData.text = (char*)MyMalloc(len + 2);
	wTextGetText(jsonTextEntry, xx->noteData.text, len);

	SetBoundingBox( trk, xx->pos, xx->pos );
	DrawNewTrack( trk );
	JSONNOTE_LOG("jsonnote: saved trk #%d at (%0.3f, %0.3f) layer=%d op=%d\n",
	             GetTrkIndex(trk), xx->pos.x, xx->pos.y, GetTrkLayer(trk), xx->op);
	wHide(jsonNoteW);
	ResetIfNotSticky();
	SetFileChanged();
}

/**
 * Create the edit dialog for JSON notes.
 *
 * \param title IN dialog title
 * \param textData IN note text
 */
static void
CreateEditJsonNote(char *title, const char *textData)
{
	if (!jsonNoteW) {
		FormRegister(&jsonNotePG);
		jsonNoteW = FormCreateDialog(&jsonNotePG, "",
		                             _("Done"), JsonEditOK,
		                             _("Cancel"), FormCancel_Current,
		                             TRUE, F_BLOCK,
		                             JsonDlgUpdate);
	}

	wWinSetTitle(jsonNotePG.win, MakeWindowTitle(title));

	wTextClear(jsonTextEntry);
	wTextAppend(jsonTextEntry, textData);
	wTextSetReadonly(jsonTextEntry, FALSE);
	int noteLayer = jsonNoteData.layer;
	FillLayerList(jsonNotePLs[I_LAYER].control);
	jsonNoteData.layer = noteLayer;

	/* Structured field editor: start each dialog open with a clean Name/
	 * Value row and ROOT selected -- otherwise a previous note's leftover
	 * values would appear to carry over into this one. JsonNoteValidate()
	 * below rebuilds the Object dropdown itself once wTextAppend() above has
	 * populated the text box for this note. */
	jsonNoteData.fieldObjectInx = 0;
	jsonNoteData.fieldName[0] = '\0';
	jsonNoteData.fieldValue[0] = '\0';
	paramData_p nameField = &jsonNotePLs[I_JSONNAME];
	wControlHilite(nameField->control, FALSE);

	FormLoadControls(&jsonNotePG);
	descTitle = title;

	/* Reflect the just-loaded text's validity immediately: an existing
	 * note's text is always valid JSON (it couldn't have been saved
	 * otherwise), and so is a new note's starter template (below). */
	JsonNoteValidate(NULL);

	wShow(jsonNoteW);
}

/**
 * Show details in statusbar. If running in Describe mode, the describe
 * dialog is opened for editing the note.
 *
 * \param trk IN the selected track (note)
 * \param str IN the buffer for the description string
 * \param len IN length of string buffer str
 */
void DescribeJsonNote(track_p trk, char * str, CSIZE_T len)
{
	struct extraDataNote_t *xx = GET_EXTRA_DATA( trk, T_NOTE, extraDataNote_t );
	DynString statusLine;
	DynStringMalloc(&statusLine, 100);

	cJSON *parsed = cJSON_Parse(xx->noteData.text);
	cJSON *kind = parsed ? cJSON_GetObjectItemCaseSensitive(parsed, "kind") : NULL;

	if (kind && cJSON_IsString(kind) && kind->valuestring) {
		cJSON *id = cJSON_GetObjectItemCaseSensitive(parsed, "id");
		if (id && cJSON_IsString(id) && id->valuestring) {
			DynStringPrintf(&statusLine,
			                _("JSON Note(%d) Layer=%d kind=%s id=%s"),
			                GetTrkIndex(trk),
			                GetTrkLayer(trk)+1,
			                kind->valuestring,
			                id->valuestring);
		} else {
			DynStringPrintf(&statusLine,
			                _("JSON Note(%d) Layer=%d kind=%s"),
			                GetTrkIndex(trk),
			                GetTrkLayer(trk)+1,
			                kind->valuestring);
		}
	} else {
		char *noteText = MyMalloc(strlen(xx->noteData.text) + 1);
		RemoveFormatChars(xx->noteData.text, noteText);
		EllipsizeString(noteText, NULL, 80);
		DynStringPrintf(&statusLine,
		                _("JSON Note(%d) Layer=%d %-.80s"),
		                GetTrkIndex(trk),
		                GetTrkLayer(trk)+1,
		                noteText);
		MyFree(noteText);
	}
	if (parsed) {
		cJSON_Delete(parsed);
	}

	strcpy(str, DynStringToCStr(&statusLine));
	DynStringFree(&statusLine);
	if ( ! inDescribeCmd ) {
		return;
	}

	jsonNoteData.pos = xx->pos;
	jsonNoteData.layer = GetTrkLayer( trk );
	jsonNoteData.trk = trk;

	CreateEditJsonNote(_("Update JSON Note"), xx->noteData.text );
}

/**
 * Show the UI for entering new JSON notes.
 *
 * \param pos Note position
 */
void NewJsonNoteUI(coOrd pos )
{
	/* A valid starter object to fill in, not an instruction sentence:
	 * Validate passes on the default text, and the template shows the
	 * expected shape, including a key with several values (dev-ML #4404).
	 * JSON syntax, so not translated. */
	const char *tmpPtrText =
	        "{\n\t\"kind\":\t\"\",\n\t\"id\":\t\"\",\n\t\"tags\":\t[\"\", \"\"]\n}";

	jsonNoteData.pos = pos;
	jsonNoteData.layer = curLayer;
	jsonNoteData.trk = NULL;

	CreateEditJsonNote(_("Create JSON Note"), tmpPtrText );
}
