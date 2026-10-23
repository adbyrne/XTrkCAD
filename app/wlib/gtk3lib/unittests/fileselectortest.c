/** \file fileselectortest.c
* Unit tests for the file selector helpers, fileselector.c
*
* The helpers are static, so the source file is included directly. For that
* reason this test does not link xtrkcad-wlib (wFilSelCreate()/wFilSelect()
* would be defined twice). No dialog is shown, so no display is needed.
*/

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>

#include <cmocka.h>

#include "../fileselector.c"

/**
 * Create a filter with the given default extension, NULL for none.
 */

static GtkFileFilter *
FilterWithExtension(const char *ext)
{
	GtkFileFilter *filter = NewFilter("test");

	if (ext) {
		g_object_set_data_full(G_OBJECT(filter), DEFAULTEXTENSION,
		                       g_strdup(ext), g_free);
	}
	return filter;
}

/**
 * Run EnsureExtension() on a copy of name and check the result.
 */

static void
CheckEnsure(const char *name, const char *ext, const char *expected,
            gboolean changed)
{
	GtkFileFilter *filter = FilterWithExtension(ext);
	char *fileName = g_strdup(name);

	assert_int_equal(EnsureExtension(&fileName, filter), changed);
	assert_string_equal(fileName, expected);

	g_free(fileName);
	g_object_unref(filter);
}

/**
 * Return the default extension stored on filter n, NULL for none.
 */

static const char *
FilterExtension(struct wFilSel_t *fs, guint n)
{
	return g_object_get_data(G_OBJECT(g_ptr_array_index(fs->filters, n)),
	                         DEFAULTEXTENSION);
}

/**
 * Release a selector created by wFilSelCreate(). wlib has no destroy
 * function because selectors live as long as the program, but the test must
 * not leak under AddressSanitizer.
 */

static void
FreeSelector(struct wFilSel_t *fs)
{
	if (fs->filters) {
		g_ptr_array_unref(fs->filters);
	}
	g_free(fs->title);
	g_free(fs);
}

static void DefaultExtensionFromPattern(void **state)
{
	(void)state;

	assert_string_equal(GetDefaultExtension("*.txt"), "txt");
	assert_string_equal(GetDefaultExtension("*.tar.gz"), "tar.gz");
	assert_string_equal(GetDefaultExtension("*.xtce"), "xtce");
}

static void NoDefaultExtensionFromPattern(void **state)
{
	(void)state;

	assert_null(GetDefaultExtension(NULL));
	assert_null(GetDefaultExtension(""));
	assert_null(GetDefaultExtension("*"));
	assert_null(GetDefaultExtension("*."));
	assert_null(GetDefaultExtension("*.t*"));
	assert_null(GetDefaultExtension("*.t?t"));
	assert_null(GetDefaultExtension("*.[ch]"));
	assert_null(GetDefaultExtension("foo.txt"));
	assert_null(GetDefaultExtension(".txt"));
}

static void ExtensionAppended(void **state)
{
	(void)state;

	CheckEnsure("foo", "xtc", "foo.xtc", TRUE);
	CheckEnsure("/home/user/foo", "xtc", "/home/user/foo.xtc", TRUE);
	// a dot in a directory name does not count
	CheckEnsure("/home/user/dir.d/foo", "xtc", "/home/user/dir.d/foo.xtc",
	            TRUE);
	// a leading dot is a hidden file, not an extension
	CheckEnsure(".bashrc", "xtc", ".bashrc.xtc", TRUE);
	CheckEnsure("/tmp/.hidden", "xtc", "/tmp/.hidden.xtc", TRUE);
}

static void ExtensionKept(void **state)
{
	(void)state;

	CheckEnsure("foo.xtc", "xtc", "foo.xtc", FALSE);
	CheckEnsure("foo.xtce", "xtc", "foo.xtce", FALSE);
	CheckEnsure("archive.tar.gz", "xtc", "archive.tar.gz", FALSE);
	// a trailing dot requests an empty extension
	CheckEnsure("foo.", "xtc", "foo.", FALSE);
	// documented restriction: any dot counts as an extension
	CheckEnsure("Plan v1.2", "xtc", "Plan v1.2", FALSE);
}

static void NoExtensionWithoutDefault(void **state)
{
	(void)state;
	char *fileName = g_strdup("foo");

	// no filter selected
	assert_false(EnsureExtension(&fileName, NULL));
	assert_string_equal(fileName, "foo");
	g_free(fileName);

	// filter without default extension, e.g. "All files"
	CheckEnsure("foo", NULL, "foo", FALSE);
}

static void FiltersFromPatternString(void **state)
{
	(void)state;
	struct wFilSel_t *fs = wFilSelCreate(NULL, FS_SAVE, 0, "Save",
	                                     "Trackplan (*.xtc)|*.xtc|"
	                                     "Extended (*.xtce)|*.xtce|"
	                                     "All Files (*)|*",
	                                     NULL, NULL);

	assert_int_equal(fs->filters->len, 3);
	assert_string_equal(FilterExtension(fs, 0), "xtc");
	assert_string_equal(FilterExtension(fs, 1), "xtce");
	assert_null(FilterExtension(fs, 2));
	assert_string_equal(gtk_file_filter_get_name(
	                            g_ptr_array_index(fs->filters, 1)),
	                    "Extended (*.xtce)");
	FreeSelector(fs);
}

static void FirstPatternGivesExtension(void **state)
{
	(void)state;
	struct wFilSel_t *fs = wFilSelCreate(NULL, FS_SAVE, 0, "Save",
	                                     "All (*.xtc,*.xtce)|*.xtc;*.xtce|"
	                                     "Wild|*.t*;*.txt|"
	                                     "Empty entries|;;*.dxf;",
	                                     NULL, NULL);

	assert_int_equal(fs->filters->len, 3);
	assert_string_equal(FilterExtension(fs, 0), "xtc");
	// only the first pattern is considered
	assert_null(FilterExtension(fs, 1));
	// empty entries are skipped, the first real pattern counts
	assert_string_equal(FilterExtension(fs, 2), "dxf");
	FreeSelector(fs);
}

static void TrailingBarAllowed(void **state)
{
	(void)state;
	struct wFilSel_t *fs = wFilSelCreate(NULL, FS_SAVE, 0, "Save",
	                                     "Text Files|*.txt|", NULL, NULL);

	assert_int_equal(fs->filters->len, 1);
	assert_string_equal(FilterExtension(fs, 0), "txt");
	FreeSelector(fs);
}

static void PictureFiltersHaveNoExtension(void **state)
{
	(void)state;
	struct wFilSel_t *fs = wFilSelCreate(NULL, FS_LOAD, FSO_PICTURES, "Load",
	                                     "ignored|*.xtc", NULL, NULL);

	assert_int_equal(fs->filters->len, 2);
	assert_null(FilterExtension(fs, 0));
	assert_null(FilterExtension(fs, 1));
	FreeSelector(fs);
}

static void NoPatternNoFilters(void **state)
{
	(void)state;
	struct wFilSel_t *fs = wFilSelCreate(NULL, FS_LOAD, 0, "Load", NULL,
	                                     NULL, NULL);

	assert_null(fs->filters);
	FreeSelector(fs);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(DefaultExtensionFromPattern),
		cmocka_unit_test(NoDefaultExtensionFromPattern),
		cmocka_unit_test(ExtensionAppended),
		cmocka_unit_test(ExtensionKept),
		cmocka_unit_test(NoExtensionWithoutDefault),
		cmocka_unit_test(FiltersFromPatternString),
		cmocka_unit_test(FirstPatternGivesExtension),
		cmocka_unit_test(TrailingBarAllowed),
		cmocka_unit_test(PictureFiltersHaveNoExtension),
		cmocka_unit_test(NoPatternNoFilters),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
