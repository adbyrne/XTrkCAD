/** \file fileselector.c
 * Create and handle file selectors
 */

/*  XTrkCad - Model Railroad CAD
 *  Copyright (C) 2005 Dave Bullis
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#define GTK_DISABLE_SINGLE_INCLUDES
#define GDK_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define GSEAL_ENABLE

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include "wlib.h"
#include "gtkint.h"
#include "i18n.h"

/** Key under which the default extension is stored on a GtkFileFilter */
#define DEFAULTEXTENSION "defaultext"

struct wFilSel_t {
	wType_e type;                       /**< widget type, always W_FILESELECT */
	GtkFileChooserNative * window;      /**< file selector handle */
	wFilSelCallBack_p action;           /**< callback executed after selection */
	void * context;                     /**< user context passed to callback */
	GPtrArray* filters;                 /**< file type filters (GtkFileFilter) */
	wFilSelMode_e mode;                 /**< load or save */
	int opt;                            /**< see FSO_ options */
	char * title;                       /**< dialog box title */
	wControl_p parent;                  /**< parent window */
};

/**
 * Create a new, empty filter that is owned by the caller (floating
 * reference is sunk).
 *
 * \param name  IN  display name of the filter
 * \return          new filter, release with g_object_unref()
 */

static GtkFileFilter *
NewFilter(const char *name)
{
	GtkFileFilter *filter = g_object_ref_sink(gtk_file_filter_new());
	gtk_file_filter_set_name(filter, name);
	return filter;
}

/**
 * Derive the default extension from a filter pattern.
 *
 * A pattern qualifies if it has the form "*.ext" where ext is not empty
 * and contains no wildcard characters ('*', '?', '['). Examples:
 *   "*.txt"     -> "txt"
 *   "*.tar.gz"  -> "tar.gz"
 *   "*"         -> NULL
 *   "*.t*"      -> NULL
 *   "*.[ch]"    -> NULL
 *
 * \param pattern  IN  single filter pattern, may be NULL
 * \return         pointer into pattern (not a copy), or NULL if none
 */

static const char *
GetDefaultExtension(const char *pattern)
{
	if (pattern == NULL || pattern[0] != '*' || pattern[1] != '.'
	    || pattern[2] == '\0') {
		return NULL;
	}
	if (strpbrk(pattern + 2, "*?[") != NULL) {
		return NULL;
	}
	return pattern + 2;
}

/**
 * Create filters from pattern string. The pattern string consists of pairs
 * of filter definitions. First is the name of the filter, second the select
 * pattern list. Several patterns in one list are separated by a semicolon.
 * The parts are delimited by a vertical bar. A trailing bar is allowed.
 * Eg: "Text Files|*.txt;*.text|All Files|*|"
 *
 * The default extension of a filter is taken from the first pattern of the
 * filter if it has the form "*.ext" (see GetDefaultExtension()). It is
 * stored on the filter and used when saving a file without extension.
 *
 * Restrictions:
 * - The characters '|' and ';' cannot be part of a name or pattern.
 * - A malformed pattern string (name without patterns, or a pattern list
 *   without any pattern) is a programming error and aborts the program.
 * - If FSO_PICTURES is set, the pattern string is ignored and the filters
 *   are created from the supported image formats. These filters have no
 *   default extension.
 *
 * \param fileSelect    IN  file selector widget
 * \param filterPattern IN  the pattern string, ignored if FSO_PICTURES is set
 */

static void
CreateFilters(struct wFilSel_t* fileSelect, const char* filterPattern)
{
	fileSelect->filters = g_ptr_array_new_with_free_func(g_object_unref);

	if (fileSelect->opt & FSO_PICTURES) {
		GtkFileFilter *filter = NewFilter("Supported Image Formats");
		gtk_file_filter_add_pixbuf_formats(filter);
		g_ptr_array_add(fileSelect->filters, filter);

		filter = NewFilter("All files");
		gtk_file_filter_add_pattern(filter, "*");
		g_ptr_array_add(fileSelect->filters, filter);
		return;
	}

	char **parts = g_strsplit(filterPattern, "|", 0);

	// an empty name (e.g. from a trailing '|') ends the list
	for (int i = 0; parts[i] != NULL && parts[i][0] != '\0'; i += 2) {
		const char *name = parts[i];

		if (parts[i + 1] == NULL) {
			g_error("CreateFilters(): Patterns missing for '%s' in '%s'",
			        name, filterPattern);
		}

		char **patternList = g_strsplit(parts[i + 1], ";", 0);
		GtkFileFilter *filter = NewFilter(name);
		int added = 0;

		for (char **p = patternList; *p != NULL; p++) {
			if (**p == '\0') {
				continue;                       // ignore empty entries
			}
			if (added == 0) {
				const char *ext = GetDefaultExtension(*p);
				if (ext) {
					g_object_set_data_full(G_OBJECT(filter), DEFAULTEXTENSION,
					                       g_strdup(ext), g_free);
				}
			}
			gtk_file_filter_add_pattern(filter, *p);
			added++;
		}
		g_strfreev(patternList);

		if (added == 0) {
			g_error("CreateFilters(): Patterns are empty for '%s' in '%s'",
			        name, filterPattern);
		}
		g_ptr_array_add(fileSelect->filters, filter);
	}
	g_strfreev(parts);
}

/**
 * Add the already created filters to the dialog. The dialog takes its own
 * reference, the first filter added becomes the active one.
 *
 * \param selector  IN  file selector with created filters and dialog
 */

static void
AddFilters(struct wFilSel_t* selector)
{
	for (guint i = 0; i < selector->filters->len; i++) {
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(selector->window),
		                            GTK_FILE_FILTER(g_ptr_array_index(selector->filters, i)));
	}
}

/**
 * Append the default extension of the given filter to a file name that has
 * none. The name is replaced in place, the old string is freed.
 *
 * Rules:
 * - Only the last path component is examined.
 * - A name has an extension if its last component contains a dot that is
 *   not the first character. ".bashrc" therefore has no extension,
 *   "archive.tar.gz" and "foo.bar" have one.
 * - A trailing dot ("foo.") explicitly requests an empty extension. The
 *   name is left unchanged.
 * - Nothing happens if the filter is NULL or has no default extension
 *   (e.g. "All files").
 * - The existing extension is not compared with the filter patterns. A
 *   name like "Plan v1.2" counts as having the extension "2".
 *
 * \param fileName  IN/OUT  file name in GLib file name encoding, must be
 *                          allocated with g_malloc() or compatible
 * \param filter    IN      currently selected filter, may be NULL
 * \return          TRUE if the name was changed, FALSE otherwise
 */

static gboolean
EnsureExtension(char **fileName, GtkFileFilter *filter)
{
	if (filter == NULL) {
		return FALSE;
	}

	// the extension is owned by the filter, do not free
	const char *extension = g_object_get_data(G_OBJECT(filter), DEFAULTEXTENSION);
	if (extension == NULL) {
		return FALSE;
	}

	char *baseName = g_path_get_basename(*fileName);
	const char *dot = strrchr(baseName, '.');
	gboolean hasExtension = (dot != NULL && dot != baseName);
	g_free(baseName);

	if (hasExtension) {
		return FALSE;
	}

	char *newName = g_strconcat(*fileName, ".", extension, NULL);
	g_free(*fileName);
	*fileName = newName;
	return TRUE;
}

/**
 * Get the files selected in the chooser as NULL terminated array of file
 * names in GLib file name encoding.
 *
 * Entries that cannot be converted from their URI (e.g. remote files) are
 * skipped with a warning, so the array may be shorter than the selection
 * and may be empty.
 *
 * \param chooser  IN  file chooser after an accepted response
 * \return         newly allocated array, release with FreeFileNames()
 */

static char **
BuildFileNames(GtkFileChooser *chooser)
{
	GSList *uris = gtk_file_chooser_get_uris(chooser);
	char **names = g_new0(char *, g_slist_length(uris) + 1);
	guint count = 0;

	for (GSList *item = uris; item != NULL; item = item->next) {
		GError *err = NULL;
		char *fileName = g_filename_from_uri((const char *)item->data, NULL, &err);

		if (fileName == NULL) {
			g_warning("BuildFileNames(): cannot convert '%s': %s",
			          (const char *)item->data, err->message);
			g_clear_error(&err);
			continue;
		}
		names[count++] = fileName;
	}
	g_slist_free_full(uris, g_free);
	return names;
}

/**
 * Convert all names in the array in place to the locale encoding. A name
 * that cannot be converted is left unchanged and a warning is logged.
 *
 * \param names  IN/OUT  NULL terminated array from BuildFileNames()
 */

static void
ConvertFileNamesToLocale(char **names)
{
	for (char **p = names; *p != NULL; p++) {
		GError *err = NULL;
		char *converted = g_locale_from_utf8(*p, -1, NULL, NULL, &err);

		if (converted == NULL) {
			g_warning("ConvertFileNamesToLocale(): '%s': %s", *p, err->message);
			g_clear_error(&err);
			continue;
		}
		g_free(*p);
		*p = converted;
	}
}

/**
 * Release an array created by BuildFileNames().
 *
 * \param names  IN  array to free, may be NULL
 */

static void
FreeFileNames(char **names)
{
	g_strfreev(names);
}

/**
 * Create a new file selector. Only the internal structures are
 * set up, no dialog is created.
 *
 * If the FSO_PICTURES flag is set in opt, the filters are automatically
 * created from the supported image formats. Any pattern passed in
 * pattList is ignored in that case.
 * After executing the file selector, the action callback is executed. The
 * callback receives three arguments:
 * - int files number of selected files (1 or more if FSO_MULTIPLEFILES)
 * - char ** names an array of file names in locale encoding
 * - void * context the context passed to wFilSelCreate()
 *
 * Restrictions:
 * - When saving (FS_SAVE or FS_UPDATE), a default extension is appended to a
 *   name without extension, but only for the first selected file. Combining
 *   a save mode with FSO_MULTIPLEFILES is not supported.
 * - The file selector cannot be destroyed, title and filters live as long
 *   as the program.
 *
 * \param w         IN  parent window
 * \param mode      IN  FS_LOAD, FS_SAVE or FS_UPDATE (save into an existing
 *                      file, no overwrite confirmation)
 * \param opt       IN  FSO_ flags: FSO_PICTURES (filters from image formats),
 *                      FSO_MULTIPLEFILES (multi selection),
 *                      FSO_SETFOLDERALWAYS (set start folder on every call)
 * \param title     IN  dialog title
 * \param pattList  IN  list of filter patterns, see CreateFilters(), may be
 *                      NULL if FSO_PICTURES is set or no filter is wanted
 * \param action    IN  callback
 * \param context   IN  see description
 * \return          the newly created file selector structure
 */

struct wFilSel_t * wFilSelCreate(
        wControl_p w,
        wFilSelMode_e mode,
        int opt,
        const char * title,
        const char * pattList,
        wFilSelCallBack_p action,
        void * context )
{
	struct wFilSel_t	*fs;

	fs = (struct wFilSel_t*)g_malloc0(sizeof *fs);

	fs->type = W_FILESELECT;
	fs->parent = w;
	fs->window = NULL;
	fs->mode = mode;
	fs->opt = opt;
	fs->title = g_strdup( title );
	fs->action = action;
	fs->context = context;

	if (pattList || (opt & FSO_PICTURES)) {
		CreateFilters(fs, pattList);
	}
	return fs;
}

/**
 * Show and handle the file selection dialog.
 *
 * When saving (FS_SAVE or FS_UPDATE) and the selected filter has a default
 * extension, the extension is appended to a name that has none (see
 * EnsureExtension()). For FS_SAVE, GTK has only asked for overwrite
 * confirmation for the name as typed. If the completed name already exists,
 * the dialog is shown again with the completed name so that GTK asks for
 * confirmation for the real target. Cancel ends the function without calling
 * the action. FS_UPDATE writes into an existing file, so no confirmation is
 * requested.
 *
 * Restrictions:
 * - The action callback is called at most once and only after an accepted
 *   selection.
 * - Existence of the completed file is checked before writing, the file can
 *   still change before the caller writes it.
 *
 * \param fs        IN  file selection
 * \param dirName   IN  starting directory, may be NULL
 * \return          always TRUE
 */

int wFilSelect( struct wFilSel_t * fs, const char * dirName )
{
	g_assert(fs->type == W_FILESELECT);

	if (fs->window == NULL) {

		fs->window = gtk_file_chooser_native_new(fs->title,
		             NULL,
		             (fs->mode == FS_LOAD ? GTK_FILE_CHOOSER_ACTION_OPEN :
		              GTK_FILE_CHOOSER_ACTION_SAVE),
		             (fs->mode == FS_LOAD ? ("Open") : ("Save")),
		             ("Cancel"));

		if (fs->window == NULL) {
			abort();
		}

		if (fs->mode == FS_SAVE) {
			// get confirmation before overwriting an existing file
			gtk_file_chooser_set_do_overwrite_confirmation(
			        GTK_FILE_CHOOSER(fs->window), TRUE);
		}

		// NOTE: this is not the recommended way to set the folder
		// See https://docs.gtk.org/gtk3/method.FileChooser.set_current_folder.html
		// However this works for now
		// TODO: revisit how to set the current folder
		if (dirName) {
			gtk_file_chooser_set_current_folder(
			        GTK_FILE_CHOOSER(fs->window), dirName );
		}

		if (fs->opt & FSO_MULTIPLEFILES) {
			gtk_file_chooser_set_select_multiple(
			        GTK_FILE_CHOOSER(fs->window), TRUE);
		}

		if (fs->filters) {
			AddFilters(fs);
		}
	} else if ((fs->opt & FSO_SETFOLDERALWAYS) && dirName) {
		gtk_file_chooser_set_current_folder(
		        GTK_FILE_CHOOSER(fs->window), dirName );
	}

	GtkFileChooser *chooser = GTK_FILE_CHOOSER(fs->window);

	for (;;) {
		int resp = gtk_native_dialog_run(GTK_NATIVE_DIALOG(fs->window));

		if (resp != GTK_RESPONSE_ACCEPT && resp != GTK_RESPONSE_APPLY) {
			break;                                  // cancelled
		}

		char **names = BuildFileNames(chooser);

		if (names[0] == NULL) {
			FreeFileNames(names);
			break;                                  // nothing usable selected
		}

		if (fs->mode != FS_LOAD) {
			GtkFileFilter *filter = gtk_file_chooser_get_filter(chooser);

			// FS_UPDATE writes into an existing file on purpose, no
			// overwrite confirmation is needed
			if (EnsureExtension(&names[0], filter)
			    && fs->mode == FS_SAVE
			    && g_file_test(names[0], G_FILE_TEST_EXISTS)) {
				// The completed name exists but GTK did not ask about it.
				// Show the dialog again with the completed name, GTK will
				// ask for confirmation on the next accept.
				char *dir = g_path_get_dirname(names[0]);
				char *base = g_path_get_basename(names[0]);
				char *display = g_filename_display_name(base);

				gtk_file_chooser_set_current_folder(chooser, dir);
				gtk_file_chooser_set_current_name(chooser, display);

				g_free(display);
				g_free(base);
				g_free(dir);
				FreeFileNames(names);
				continue;
			}
		}

		ConvertFileNamesToLocale(names);

		if (fs->action) {
			fs->action((int)g_strv_length(names), names, fs->context);
		}
		FreeFileNames(names);
		break;
	}

	return TRUE;
}
