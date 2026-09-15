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
} jsonNoteData;

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
 * Read the dialog's current text and report whether it's a valid JSON
 * *object* (not just valid JSON -- MCP's own note dispatch, and this
 * feature's whole premise, both require an object; a bare array/string/
 * number/bool/null would silently parse but then vanish from every
 * downstream report with no error surfaced anywhere) and within the
 * length this feature can safely round-trip.
 *
 * \param errMsg OUT set to a user-facing reason when returning FALSE; left
 *        untouched when returning TRUE
 * \return TRUE if the current text is acceptable to save
 */
static BOOL_T
JsonNoteIsValid(const char **errMsg)
{
	int len = wTextGetSize(jsonTextEntry);
	char *buf = MyMalloc(len + 2);
	wTextGetText(jsonTextEntry, buf, len);

	if (!JsonNoteLengthOk(buf, len)) {
		MyFree(buf);
		*errMsg = _("Too long -- would risk a crash reopening this file");
		return FALSE;
	}

	const char *errPtr = NULL;
	cJSON *parsed = cJSON_ParseWithOpts(buf, &errPtr, FALSE);
	MyFree(buf);

	if (parsed == NULL) {
		*errMsg = _("Invalid JSON");
		return FALSE;
	}
	if (!cJSON_IsObject(parsed)) {
		cJSON_Delete(parsed);
		*errMsg = _("Must be a JSON object, e.g. {\"kind\": \"...\"} -- not an array/string/number");
		return FALSE;
	}
	cJSON_Delete(parsed);
	return TRUE;
}

/**
 * Callback for the Validate button: check the current text and reflect the
 * result via the same bInvalid/hilite/OK-active mechanism filenoteui.c
 * already uses for its I_PATH field.
 *
 * \param junk unused
 */
static void
JsonNoteValidate(void *junk)
{
	(void)junk;
	const char *errMsg = NULL;
	paramData_p p = &jsonNotePLs[I_TEXT];

	if (!JsonNoteIsValid(&errMsg)) {
		JSONNOTE_LOG("jsonnote: invalid -- %s\n", errMsg);
		p->bInvalid = TRUE;
		wTooltipSetText(p->control, errMsg);
		wControlHilite(p->control, TRUE);
		FormDialogOkActive(&jsonNotePG, FALSE);
	} else {
		JSONNOTE_LOG("jsonnote: valid\n");
		p->bInvalid = FALSE;
		wControlHilite(p->control, FALSE);
		FormDialogOkActive(&jsonNotePG, TRUE);
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

	cJSON *parsed = cJSON_Parse(buf);
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

	/* The now-formatted text is still valid (Format never changes meaning,
	 * only whitespace) -- clear any stale invalid state directly rather
	 * than re-running the parse a third time. */
	paramData_p p = &jsonNotePLs[I_TEXT];
	p->bInvalid = FALSE;
	wControlHilite(p->control, FALSE);
	FormDialogOkActive(&jsonNotePG, TRUE);
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
	if (!JsonNoteIsValid(&errMsg)) {
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
	FormLoadControls(&jsonNotePG);
	descTitle = title;

	/* Reflect the just-loaded text's validity immediately -- an existing
	 * note's text is always valid JSON (it couldn't have been saved
	 * otherwise), but a freshly-created note's placeholder text (below)
	 * is not, and should show as such rather than looking accidentally OK
	 * until the user's first edit. */
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
	const char *tmpPtrText =
	        _("Replace this text with a JSON object, e.g. {\"kind\": \"station\", \"id\": \"WP\"}");

	jsonNoteData.pos = pos;
	jsonNoteData.layer = curLayer;
	jsonNoteData.trk = NULL;

	CreateEditJsonNote(_("Create JSON Note"), tmpPtrText );
}
