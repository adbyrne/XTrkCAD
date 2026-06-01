/** \file undostreamtest.c
 * CMocka unit tests for the five stream primitives in undostream.c:
 * ReadStream, WriteStream, TrimStream, ClearStream, TruncateStream.
 *
 * Stubs replace the full application's memory and logging infrastructure
 * so the stream buffer logic can be tested without any UI or GTK dependency.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "common.h"

/* -----------------------------------------------------------------------
 * Stubs for undostream.c's external dependencies
 * ----------------------------------------------------------------------- */

/* LOG macro references logTable_da even when the log level is 0 */
dynArr_t logTable_da;

/* lprintf expands to LogPrintf (misc.h:302) */
void LogPrintf(const char *fmt, ...) { (void)fmt; }

void *MyMalloc(size_t n)
{
	void *p = malloc(n);
	if (!p) {
		abort();
	}
	return p;
}

void *MyRealloc(void *p, size_t n)
{
	void *q = realloc(p, n);
	if (!q && n) {
		abort();
	}
	return q;
}

void MyFree(void *p) { free(p); }

/*
 * UndoFail is called when a stream assertion fires (UASSERT macro).
 * Tests check g_undo_fail_count to verify whether it was triggered.
 */
static int g_undo_fail_count = 0;
BOOL_T UndoFail(char *cause, uintptr_t val, char *fileName, int lineNumber)
{
	(void)cause; (void)val; (void)fileName; (void)lineNumber;
	g_undo_fail_count++;
	return FALSE;
}

#include "../undostream.c"

/* -----------------------------------------------------------------------
 * Helpers
 * ----------------------------------------------------------------------- */

static stream_t make_stream(void)
{
	stream_t s;
	memset(&s, 0, sizeof s);
	return s;
}

/* Reset the failure counter before each test */
static int setup(void **state)
{
	(void)state;
	g_undo_fail_count = 0;
	return 0;
}

/* -----------------------------------------------------------------------
 * WriteStream / ReadStream
 * ----------------------------------------------------------------------- */

static void test_write_read_int_roundtrip(void **state)
{
	(void)state;
	stream_t s = make_stream();
	int written = 0x12345678;

	assert_true(WriteStream(&s, &written, sizeof written));
	s.curr = 0;
	int read = 0;
	assert_true(ReadStream(&s, &read, sizeof read));
	assert_int_equal(written, read);
	assert_int_equal(0, g_undo_fail_count);
	ClearStream(&s);
}

static void test_write_read_multiple_objects(void **state)
{
	(void)state;
	stream_t s = make_stream();
	int a = 111, b = 222, c = 333;

	assert_true(WriteStream(&s, &a, sizeof a));
	assert_true(WriteStream(&s, &b, sizeof b));
	assert_true(WriteStream(&s, &c, sizeof c));

	s.curr = 0;
	int ra = 0, rb = 0, rc = 0;
	assert_true(ReadStream(&s, &ra, sizeof ra));
	assert_true(ReadStream(&s, &rb, sizeof rb));
	assert_true(ReadStream(&s, &rc, sizeof rc));
	assert_int_equal(a, ra);
	assert_int_equal(b, rb);
	assert_int_equal(c, rc);
	assert_int_equal(0, g_undo_fail_count);
	ClearStream(&s);
}

static void test_write_read_spans_block_boundary(void **state)
{
	(void)state;
	stream_t s = make_stream();

	/* Two full blocks worth of data — exercises the multi-block copy path */
	size_t len = BSTREAM_SIZE * 2;
	char *buf = malloc(len);
	assert_non_null(buf);
	for (size_t i = 0; i < len; i++) {
		buf[i] = (char)(i & 0xFF);
	}

	assert_true(WriteStream(&s, buf, (int)len));
	s.curr = 0;
	char *out = calloc(1, len);
	assert_non_null(out);
	assert_true(ReadStream(&s, out, (int)len));
	assert_memory_equal(buf, out, len);
	assert_int_equal(0, g_undo_fail_count);

	free(buf);
	free(out);
	ClearStream(&s);
}

static void test_write_read_straddles_block_boundary(void **state)
{
	(void)state;
	stream_t s = make_stream();

	/* Write a small pad so the interesting data straddles a block edge */
	int pad = 0;
	int pad_size = BSTREAM_SIZE - (int)sizeof(int);  /* land 4 bytes before end of block 0 */
	assert_true(WriteStream(&s, &pad, pad_size));

	/* This int will split across block 0 and block 1 */
	int val = 0xDEADBEEF;
	assert_true(WriteStream(&s, &val, sizeof val));

	s.curr = pad_size;  /* skip past the padding */
	int got = 0;
	assert_true(ReadStream(&s, &got, sizeof got));
	assert_int_equal(val, (int)got);
	assert_int_equal(0, g_undo_fail_count);
	ClearStream(&s);
}

static void test_read_overrun_returns_false(void **state)
{
	(void)state;
	stream_t s = make_stream();
	int val = 42;
	assert_true(WriteStream(&s, &val, sizeof val));

	s.curr = 0;
	char big[100];
	/* Reading more bytes than were written must fail */
	BOOL_T result = ReadStream(&s, big, (int)sizeof big);
	assert_false(result);
	assert_int_equal(1, g_undo_fail_count);  /* UndoFail was invoked */
	ClearStream(&s);
}

static void test_write_zero_bytes_is_noop(void **state)
{
	(void)state;
	stream_t s = make_stream();
	int dummy = 99;
	assert_true(WriteStream(&s, &dummy, 0));
	assert_int_equal(0, (int)s.end);
	assert_int_equal(0, g_undo_fail_count);
}

/* -----------------------------------------------------------------------
 * ClearStream
 * ----------------------------------------------------------------------- */

static void test_clear_resets_stream(void **state)
{
	(void)state;
	stream_t s = make_stream();
	int val = 7;
	assert_true(WriteStream(&s, &val, sizeof val));
	assert_true(s.end > 0);

	ClearStream(&s);
	assert_int_equal(0, (int)s.end);
	assert_int_equal(0, (int)s.curr);
	assert_int_equal(0, s.stream_da.cnt);
	assert_int_equal(0, g_undo_fail_count);
}

/* -----------------------------------------------------------------------
 * TruncateStream
 * ----------------------------------------------------------------------- */

static void test_truncate_shortens_stream(void **state)
{
	(void)state;
	stream_t s = make_stream();
	int a = 1, b = 2;
	assert_true(WriteStream(&s, &a, sizeof a));
	uintptr_t cut = s.end;  /* mark position after first int */
	assert_true(WriteStream(&s, &b, sizeof b));

	assert_true(TruncateStream(&s, cut));
	assert_int_equal((int)cut, (int)s.end);

	/* reading past the truncation point must fail */
	s.curr = 0;
	int ra = 0, rb = 0;
	assert_true(ReadStream(&s, &ra, sizeof ra));
	assert_int_equal(a, ra);
	BOOL_T result = ReadStream(&s, &rb, sizeof rb);
	assert_false(result);
	assert_int_equal(1, g_undo_fail_count);
	ClearStream(&s);
}

/* -----------------------------------------------------------------------
 * TrimStream
 * ----------------------------------------------------------------------- */

static void test_trim_discards_leading_blocks(void **state)
{
	(void)state;
	stream_t s = make_stream();

	/* Fill two complete blocks so TrimStream has something to free */
	char *fill = calloc(1, BSTREAM_SIZE);
	assert_non_null(fill);
	assert_true(WriteStream(&s, fill, BSTREAM_SIZE));
	free(fill);

	int sentinel = 0xCAFEBABE;
	assert_true(WriteStream(&s, &sentinel, sizeof sentinel));

	uintptr_t trim_off = BSTREAM_SIZE;  /* trim everything up to block boundary */
	assert_true(TrimStream(&s, trim_off));
	assert_int_equal(1, s.stream_da.cnt);   /* one block remains */
	assert_int_equal(0, g_undo_fail_count);

	/* The sentinel written after the trim point must still be readable */
	s.curr = trim_off;
	int got = 0;
	assert_true(ReadStream(&s, &got, sizeof got));
	assert_int_equal(sentinel, got);

	ClearStream(&s);
}

/* -----------------------------------------------------------------------
 * Test registration
 * ----------------------------------------------------------------------- */

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup(test_write_read_int_roundtrip,          setup),
		cmocka_unit_test_setup(test_write_read_multiple_objects,        setup),
		cmocka_unit_test_setup(test_write_read_spans_block_boundary,    setup),
		cmocka_unit_test_setup(test_write_read_straddles_block_boundary,setup),
		cmocka_unit_test_setup(test_read_overrun_returns_false,         setup),
		cmocka_unit_test_setup(test_write_zero_bytes_is_noop,           setup),
		cmocka_unit_test_setup(test_clear_resets_stream,                setup),
		cmocka_unit_test_setup(test_truncate_shortens_stream,           setup),
		cmocka_unit_test_setup(test_trim_discards_leading_blocks,       setup),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
