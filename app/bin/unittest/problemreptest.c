/** \file problemreptest.c
 * Unit tests for the privacy filters of the problem report (problemrep.c):
 * before the settings and the layout go into the report, the user's name and
 * home directory are replaced with placeholders. Everything else has to come
 * through unchanged.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>

#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../problemrep.c"

/* The directories and user name the filters look for */
#define TEST_HOME "/home/jsmith"
#define TEST_LIBDIR "/usr/share/xtrkcad"
#define TEST_WORKDIR "/home/jsmith/.xtrkcad"

const char *wGetUserHomeDir( void ) { return TEST_HOME; }
const char *wGetUserHomeRootDir( void ) { return TEST_HOME; }
const char *wGetAppLibDir( void ) { return TEST_LIBDIR; }
const char *wGetAppWorkDir( void ) { return TEST_WORKDIR; }
char *wGetUserID( void ) { return "jsmith"; }

/* Stubs: the rest of problemrep.c, not called by these tests */
void AbortProg( const char *a, const char *b, int c, const char *d ) { }
int Copyfile( const char *fn1, const char *fn2 ) { return 0; }
BOOL_T CreateArchive( const char *dir_path, const char *fileName ) { return TRUE; }
BOOL_T DeleteDirectory( const char *dir_path ) { return TRUE; }
char *GetLayoutFilename( void ) { return "layout.xtc"; }
char *GetLayoutFullPath( void ) { return "/tmp/layout.xtc"; }
int GetParamFileCount() { return 0; }
char *GetParamFileName( int fileInx ) { return ""; }
bool IsParamFileValid( int fileInx ) { return false; }
void MakeFullpath( char **str, ... ) { *str = NULL; }
void ProblemrepCreateW( void *ptr ) { }
void ProblemrepUpdateW( char *text, ... ) { }
int ProblemSaveLayout( void ) { return 0; }
void Rdump( FILE *f ) { }
char *wGetOSVersion( void ) { return ""; }
const char *wGetPlatformVersion( void ) { return ""; }
char *wGetProfileFilename( void ) { return ""; }
char *wGetTempPath( void ) { return "/tmp"; }
void wPrefTokenize( char *line, char **section, char **name, char **value ) { }
void wPrefFormatLine( const char *section, const char *name, const char *value,
                      char *result ) { }

char *FindFilename( char *path )
{
	char *p = strrchr( path, '/' );
	return p ? p + 1 : path;
}

void *MyMalloc( size_t n ) { return calloc( 1, n ); }
void *MyRealloc( void *p, size_t n ) { return realloc( p, n ); }
void MyFree( void *p ) { free( p ); }
char *MyStrdup( const char *s ) { return strdup( s ); }

/* The escaping of misc.c: \ and the control characters are escaped with a
 * backslash, a quote is doubled. ConvertFromEscapedText() doesn't undo the
 * doubled quote. */
char *ConvertToEscapedText( const char *text )
{
	char *out = malloc( 2 * strlen( text ) + 1 );
	char *o = out;
	for ( ; *text; text++ ) {
		switch ( *text ) {
		case '\n': *o++ = '\\'; *o++ = 'n'; break;
		case '\t': *o++ = '\\'; *o++ = 't'; break;
		case '\\': *o++ = '\\'; *o++ = '\\'; break;
		case '"': *o++ = '"'; *o++ = '"'; break;
		default: *o++ = *text;
		}
	}
	*o = '\0';
	return out;
}

char *ConvertFromEscapedText( const char *text )
{
	char *out = malloc( strlen( text ) + 1 );
	char *o = out;
	for ( ; *text; text++ ) {
		if ( *text == '\\' && text[1] ) {
			text++;
			*o++ = *text == 'n' ? '\n' : *text == 't' ? '\t' : *text;
		} else {
			*o++ = *text;
		}
	}
	*o = '\0';
	return out;
}

/** Run a layout line through FilterLayoutLine() and return what it writes. */
static char *
FilterLine( const char *line )
{
	static char buffer[1024];
	FILE *f = tmpfile();
	assert_non_null( f );
	FilterLayoutLine( f, line );
	rewind( f );
	size_t n = fread( buffer, 1, sizeof buffer - 1, f );
	buffer[n] = '\0';
	fclose( f );
	return buffer;
}

/* ReplaceUserID() keeps what follows the name */
static void
UserIDKeepsTheRest( void **state )
{
	DynString result;
	DynStringMalloc( &result, 64 );
	assert_true( ReplaceUserID( &result, "/data/jsmith/layouts/x.xtc",
	                            "<<user>>" ) );
	assert_string_equal( DynStringToCStr( &result ),
	                     "/data/<<user>>/layouts/x.xtc" );
	assert_false( ReplaceUserID( &result, "/data/other/x.xtc", "<<user>>" ) );
	DynStringFree( &result );
}

/* A settings value: the library directory, the work directory (inside the
 * home directory, so checked first), the home directory, then the name */
static void
ConfigLine( void **state )
{
	DynString result;
	DynStringMalloc( &result, 64 );

	assert_true( FilterConfigLine( &result, "dir", TEST_LIBDIR "/params" ) );
	assert_string_equal( DynStringToCStr( &result ), "<<applibdir>>/params" );

	assert_true( FilterConfigLine( &result, "dir", TEST_WORKDIR "/xtrkcad.cus" ) );
	assert_string_equal( DynStringToCStr( &result ), "<<workdir>>/xtrkcad.cus" );

	assert_true( FilterConfigLine( &result, "dir", TEST_HOME "/layouts" ) );
	assert_string_equal( DynStringToCStr( &result ), "<<home>>/layouts" );

	assert_true( FilterConfigLine( &result, "dir", "/mnt/jsmith/layouts" ) );
	assert_string_equal( DynStringToCStr( &result ), "/mnt/<<user>>/layouts" );

	assert_false( FilterConfigLine( &result, "scale", "HO" ) );
	assert_string_equal( DynStringToCStr( &result ), "HO" );

	DynStringFree( &result );
}

/* Lines that hold nothing private come through as they are */
static void
LayoutLinesUnchanged( void **state )
{
	const char *lines[] = {
		"VERSION 13 5.4.0\n",
		"SCALE HO\n",
		"LAYERS CURRENT 0\n",
		"LAYERS 0 1 0 1 255 0 0 0 0 \"Main\" 1 0 9.750000 0.000000\n",
		// The main note, old and new format
		"NOTE MAIN 0 0 0 0 1850\n",
		"NOTE MAIN 0 0 0 0 0 \"A layout\\nwith notes\"\n",
		// A text note, its text with spaces
		"NOTE 5 0 0 0 10.000000 20.000000 0 0 \"Two words\"\n",
		// A file note outside the home directory
		"NOTE 6 0 0 0 10.000000 20.000000 0 2 \"/opt/doc/plan.pdf\" \"Plan\"\n",
	};
	for ( size_t i = 0; i < sizeof lines / sizeof lines[0]; i++ ) {
		assert_string_equal( FilterLine( lines[i] ), lines[i] );
	}
}

/* A file note in the home directory: the path's start becomes <<home>>,
 * and the rest of the line, its title included, stays */
static void
LayoutFileNote( void **state )
{
	assert_string_equal(
	        FilterLine( "NOTE 6 0 0 0 10.000000 20.000000 0 2 \""
	                    TEST_HOME "/doc/plan.pdf\" \"Plan\"\n" ),
	        "NOTE 6 0 0 0 10.000000 20.000000 0 2 \"<<home>>/doc/plan.pdf\" \"Plan\"\n" );
	// A path with spaces
	assert_string_equal(
	        FilterLine( "NOTE 7 0 0 0 1.000000 2.000000 0 2 \""
	                    TEST_HOME "/My Docs/a plan.pdf\" \"A plan\"\n" ),
	        "NOTE 7 0 0 0 1.000000 2.000000 0 2 \"<<home>>/My Docs/a plan.pdf\" \"A plan\"\n" );
}

/* A layer's settings file in the home directory */
static void
LayoutLayerSettings( void **state )
{
	assert_string_equal(
	        FilterLine( "LAYERS SET 3 \"" TEST_HOME "/my settings.xset\"\n" ),
	        "LAYERS SET 3 \"<<home>>/my settings.xset\"\n" );
}

int main( void )
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test( UserIDKeepsTheRest ),
		cmocka_unit_test( ConfigLine ),
		cmocka_unit_test( LayoutLinesUnchanged ),
		cmocka_unit_test( LayoutFileNote ),
		cmocka_unit_test( LayoutLayerSettings ),
	};
	return cmocka_run_group_tests( tests, NULL, NULL );
}
