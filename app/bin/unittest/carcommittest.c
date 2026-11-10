/** \file carcommittest.c
 * Unit tests for cars/carcommit.c: CommitStaged(), the one press of the car
 * dialog's Ok. It creates a prototype, a catalog part and N roster cars, in
 * that order, then saves once. If anything fails, it deletes, newest first,
 * exactly what this commit created, and never a record it edited or found
 * already there.
 *
 * Only carcommit.c is linked. The record functions it calls are stubs that
 * log each call, so a test can check what was created and what was deleted,
 * and in which order. Nothing tested this before: car dialog playback lost
 * every car to a rollback and no test noticed.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>   /* cmocka.h needs it */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cmocka.h>

#include "common.h"
#include "cars/carsprivate.h"
#include "fileio.h"   /* PARAM_CUSTOM */

/* -----------------------------------------------------------------------
 * Call log
 * ----------------------------------------------------------------------- */

#define MAX_EVENTS 32
static char events[MAX_EVENTS][128];
static int nEvents;

static void LogEvent(const char *fmt, ...)
{
	va_list ap;
	assert_true(nEvents < MAX_EVENTS);
	va_start(ap, fmt);
	vsnprintf(events[nEvents++], sizeof events[0], fmt, ap);
	va_end(ap);
}

/* What the stubs return or do. */
static carProto_p protoFindRet;
static carPart_p partFindRet;
static BOOL_T protoNewFails;
static BOOL_T partNewFails;
static long itemNewFailsAt;     /* 0-based call that fails, -1 none */
static long itemNewCalls;
static BOOL_T itemNewLeavesNotes;
static BOOL_T saveRet;
static int saveCalls;
static void *segFreed;          /* protoSegPtr once MyFree() has it */
static void *segPtr;

static carProto_t existingProto;
static carPart_t existingPart;
static carItem_t existingItem;

/* -----------------------------------------------------------------------
 * Stubs
 * ----------------------------------------------------------------------- */

void *MyMalloc(size_t n) { return calloc(1, n); }
void MyFree(void *p)
{
	if (p != NULL && p == segPtr) { segFreed = p; }
	free(p);
}
char *MyStrdup(const char *s) { return s ? strdup(s) : NULL; }

carProto_p CarProtoFind(char *desc)
{
	(void)desc;
	return protoFindRet;
}

carProto_p CarProtoNew(carProto_p proto, int paramFileIndex, char *desc,
                       long options, long type, const carDim_t *dim, wIndex_t segCnt,
                       trkSeg_p segs)
{
	(void)options; (void)type; (void)dim; (void)segCnt; (void)segs;
	assert_int_equal(paramFileIndex, PARAM_CUSTOM);
	LogEvent("proto+ %s%s", desc, proto ? " (edit)" : "");
	if (protoNewFails) { return NULL; }
	if (proto) { return proto; }
	if (protoFindRet) { return protoFindRet; }   /* coalesced */
	return (carProto_p)calloc(1, sizeof(carProto_t));
}

void CarProtoDelete(carProto_p protoP)
{
	LogEvent("proto-");
	assert_ptr_not_equal(protoP, &existingProto);
	free(protoP);
}

carPart_p CarPartFind(char *manufP, int manufL, char *partnoP, int partnoL,
                      SCALEINX_T scale)
{
	(void)manufP; (void)manufL; (void)partnoP; (void)partnoL; (void)scale;
	return partFindRet;
}

carPart_p CarPartNew(carPart_p partP, int paramFileIndex, SCALEINX_T scaleInx,
                     char *title, long options, long type, const carDim_p dim,
                     wDrawColor color)
{
	(void)scaleInx; (void)options; (void)type; (void)dim; (void)color;
	assert_int_equal(paramFileIndex, PARAM_CUSTOM);
	LogEvent("part+ %s%s", title, partP ? " (edit)" : "");
	if (partNewFails) { return NULL; }
	if (partP) { return partP; }
	if (partFindRet) { return partFindRet; }     /* coalesced */
	return (carPart_p)calloc(1, sizeof(carPart_t));
}

void CarPartDelete(carPart_p partP)
{
	LogEvent("part-");
	assert_ptr_not_equal(partP, &existingPart);
	free(partP);
}

carItem_p CarItemNew(carItem_p item, int paramFileIndex, long itemIndex,
                     SCALEINX_T scale, char *title, long options, long type,
                     carDim_t *dim, wDrawColor color, FLOAT_T purchPrice,
                     FLOAT_T currPrice, long condition, long purchDate,
                     long serviceDate)
{
	carItem_p it;
	(void)scale; (void)options; (void)type; (void)dim; (void)color;
	(void)purchPrice; (void)currPrice; (void)condition; (void)purchDate;
	(void)serviceDate;
	assert_int_equal(paramFileIndex, PARAM_CUSTOM);
	if (itemNewCalls++ == itemNewFailsAt) {
		LogEvent("item+ %ld failed", itemIndex);
		return NULL;
	}
	LogEvent("item+ %ld %s%s", itemIndex, title, item ? " (edit)" : "");
	it = item ? item : (carItem_p)calloc(1, sizeof(carItem_t));
	it->index = itemIndex;
	if (itemNewLeavesNotes) {
		it->data.notes = strdup("old notes");
	}
	return it;
}

void CarItemDelete(carItem_p item)
{
	LogEvent("item- %ld", item->index);
	assert_ptr_not_equal(item, &existingItem);
	free(item->data.notes);
	free(item);
}

static BOOL_T SaveStub(void *ctx)
{
	(void)ctx;
	saveCalls++;
	LogEvent("save");
	return saveRet;
}

/* -----------------------------------------------------------------------
 * Helpers
 * ----------------------------------------------------------------------- */

static int Setup(void **state)
{
	(void)state;
	nEvents = 0;
	protoFindRet = NULL;
	partFindRet = NULL;
	protoNewFails = FALSE;
	partNewFails = FALSE;
	itemNewFailsAt = -1;
	itemNewCalls = 0;
	itemNewLeavesNotes = FALSE;
	saveRet = TRUE;
	saveCalls = 0;
	segFreed = NULL;
	segPtr = NULL;
	memset(&existingProto, 0, sizeof existingProto);
	memset(&existingPart, 0, sizeof existingPart);
	memset(&existingItem, 0, sizeof existingItem);
	return 0;
}

/* A new car of a new part of a new prototype: qty cars from index 10. */
static commitPlan_t NewPlan(long qty, const char *number, BOOL_T autoNumber)
{
	commitPlan_t plan;
	memset(&plan, 0, sizeof plan);
	plan.makeProto = TRUE;
	plan.protoName = "Boxcar";
	segPtr = calloc(1, 16);
	plan.protoSegPtr = (trkSeg_p)segPtr;
	plan.manuf = "Ken";
	plan.descField = "ken1 desc";
	plan.partno = "ken1";
	plan.roadname = "Road";
	plan.repmark = "RM";
	plan.number = (char *)number;
	plan.makePart = TRUE;
	plan.makeItems = TRUE;
	plan.quantity = qty;
	plan.itemIndexStart = 10;
	plan.autoNumber = autoNumber;
	return plan;
}

static void ExpectEvents(const char **expected, int n)
{
	for (int i = 0; i < n && i < nEvents; i++) {
		assert_string_equal(events[i], expected[i]);
	}
	assert_int_equal(nEvents, n);
}

static void FreeResult(commitResult_t *res, long nItems)
{
	(void)nItems;
	/* The stubs' objects are only freed by the delete stubs; a successful
	 * commit keeps them, as the real registries would. Leaks are harmless
	 * in a test process. */
	(void)res;
}

#define T(n) "Ken\tBoxcar\tken1 desc\tken1\tRoad\tRM\t" n

/* -----------------------------------------------------------------------
 * Tests
 * ----------------------------------------------------------------------- */

static void CreatesInOrderThenSavesOnce(void **state)
{
	(void)state;
	commitPlan_t plan = NewPlan(3, "41", TRUE);
	commitResult_t res;
	static const char *exp[] = {
		"proto+ Boxcar",
		"part+ " T("41"),
		"item+ 10 " T("41"),
		"item+ 11 " T("42"),
		"item+ 12 " T("43"),
		"save",
	};

	plan.notesText = "a note\n";
	assert_true(CommitStaged(&plan, SaveStub, NULL, &res));
	ExpectEvents(exp, 6);
	assert_int_equal(res.itemsCreated, 3);
	assert_int_equal(res.nextItemIndex, 13);
	assert_non_null(res.protoP);
	assert_non_null(res.partP);
	assert_int_equal(res.lastItemP->index, 12);
	assert_string_equal(res.lastItemP->data.notes, "a note\n");
	assert_ptr_equal(segFreed, segPtr);
	assert_string_equal(plan.number, "41");   /* never changed */
	FreeResult(&res, 3);
}

static void SaveFailureDeletesNewestFirst(void **state)
{
	(void)state;
	commitPlan_t plan = NewPlan(3, "41", TRUE);
	commitResult_t res;
	static const char *exp[] = {
		"proto+ Boxcar",
		"part+ " T("41"),
		"item+ 10 " T("41"),
		"item+ 11 " T("42"),
		"item+ 12 " T("43"),
		"save",
		"item- 12",
		"item- 11",
		"item- 10",
		"part-",
		"proto-",
	};

	saveRet = FALSE;
	memset(&res, 0xAB, sizeof res);
	assert_false(CommitStaged(&plan, SaveStub, NULL, &res));
	ExpectEvents(exp, 11);
	assert_null(res.protoP);
	assert_null(res.partP);
	assert_null(res.lastItemP);
	assert_int_equal(res.itemsCreated, 0);
	assert_ptr_equal(segFreed, segPtr);
}

static void ItemFailureUndoesWhatWasMade(void **state)
{
	(void)state;
	commitPlan_t plan = NewPlan(3, "41", TRUE);
	commitResult_t res;
	static const char *exp[] = {
		"proto+ Boxcar",
		"part+ " T("41"),
		"item+ 10 " T("41"),
		"item+ 11 failed",
		"item- 10",
		"part-",
		"proto-",
	};

	itemNewFailsAt = 1;
	assert_false(CommitStaged(&plan, SaveStub, NULL, &res));
	ExpectEvents(exp, 7);
	assert_int_equal(saveCalls, 0);
}

static void PartFailureUndoesPrototypeOnly(void **state)
{
	(void)state;
	commitPlan_t plan = NewPlan(1, "41", FALSE);
	commitResult_t res;
	static const char *exp[] = {
		"proto+ Boxcar",
		"part+ " T("41"),
		"proto-",
	};

	partNewFails = TRUE;
	assert_false(CommitStaged(&plan, SaveStub, NULL, &res));
	ExpectEvents(exp, 3);
}

static void ProtoFailureCreatesNothing(void **state)
{
	(void)state;
	commitPlan_t plan = NewPlan(2, "41", TRUE);
	commitResult_t res;
	static const char *exp[] = { "proto+ Boxcar" };

	protoNewFails = TRUE;
	assert_false(CommitStaged(&plan, SaveStub, NULL, &res));
	ExpectEvents(exp, 1);
	assert_int_equal(saveCalls, 0);
	assert_ptr_equal(segFreed, segPtr);
}

/* CarProtoNew/CarPartNew return the record that's already there when the
 * name matches one. Rollback must leave those alone. */
static void ExistingRecordsSurviveRollback(void **state)
{
	(void)state;
	commitPlan_t plan = NewPlan(2, "41", TRUE);
	commitResult_t res;
	static const char *exp[] = {
		"proto+ Boxcar",
		"part+ " T("41"),
		"item+ 10 " T("41"),
		"item+ 11 " T("42"),
		"save",
		"item- 11",
		"item- 10",
	};

	protoFindRet = &existingProto;
	partFindRet = &existingPart;
	saveRet = FALSE;
	assert_false(CommitStaged(&plan, SaveStub, NULL, &res));
	ExpectEvents(exp, 7);
}

/* Editing changes records in place: one car whatever the quantity, and
 * nothing is deleted when the save fails. */
static void EditsAreNeverDeleted(void **state)
{
	(void)state;
	commitPlan_t plan = NewPlan(5, "41", TRUE);
	commitResult_t res;
	static const char *exp[] = {
		"proto+ Boxcar (edit)",
		"part+ " T("41") " (edit)",
		"item+ 10 " T("41") " (edit)",
		"save",
	};

	plan.updateProtoPtr = &existingProto;
	plan.updatePartPtr = &existingPart;
	plan.updateItemPtr = &existingItem;
	saveRet = FALSE;
	assert_false(CommitStaged(&plan, SaveStub, NULL, &res));
	ExpectEvents(exp, 4);
	free(existingItem.data.notes);
}

/* Only a plain positive number counts up; anything else is shared. */
static void NumbersThatDontCountUp(void **state)
{
	static const char *numbers[] = { "A12", "0", "-3", "12b", "" };
	(void)state;

	for (size_t n = 0; n < sizeof numbers / sizeof numbers[0]; n++) {
		commitPlan_t plan;
		commitResult_t res;
		char exp1[128], exp2[128];

		Setup(NULL);
		plan = NewPlan(2, numbers[n], TRUE);
		plan.makeProto = FALSE;
		plan.makePart = FALSE;
		assert_true(CommitStaged(&plan, NULL, NULL, &res));
		snprintf(exp1, sizeof exp1, "item+ 10 Ken\tBoxcar\tken1 desc\tken1\tRoad\tRM\t%s",
		         numbers[n]);
		snprintf(exp2, sizeof exp2, "item+ 11 Ken\tBoxcar\tken1 desc\tken1\tRoad\tRM\t%s",
		         numbers[n]);
		assert_int_equal(nEvents, 2);
		assert_string_equal(events[0], exp1);
		assert_string_equal(events[1], exp2);
		assert_ptr_equal(segFreed, segPtr);   /* CommitStaged owns it */
	}
}

static void NoAutoNumberSharesTheNumber(void **state)
{
	(void)state;
	commitPlan_t plan = NewPlan(2, "41", FALSE);
	commitResult_t res;
	static const char *exp[] = {
		"proto+ Boxcar",
		"part+ " T("41"),
		"item+ 10 " T("41"),
		"item+ 11 " T("41"),
	};

	/* No save hook: nothing to persist, still a success. */
	assert_true(CommitStaged(&plan, NULL, NULL, &res));
	ExpectEvents(exp, 4);
	assert_int_equal(res.nextItemIndex, 12);
}

static void EmptyNotesClearOldNotes(void **state)
{
	(void)state;
	commitPlan_t plan = NewPlan(1, "41", FALSE);
	commitResult_t res;

	itemNewLeavesNotes = TRUE;
	plan.notesText = "";
	assert_true(CommitStaged(&plan, SaveStub, NULL, &res));
	assert_null(res.lastItemP->data.notes);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup(CreatesInOrderThenSavesOnce, Setup),
		cmocka_unit_test_setup(SaveFailureDeletesNewestFirst, Setup),
		cmocka_unit_test_setup(ItemFailureUndoesWhatWasMade, Setup),
		cmocka_unit_test_setup(PartFailureUndoesPrototypeOnly, Setup),
		cmocka_unit_test_setup(ProtoFailureCreatesNothing, Setup),
		cmocka_unit_test_setup(ExistingRecordsSurviveRollback, Setup),
		cmocka_unit_test_setup(EditsAreNeverDeleted, Setup),
		cmocka_unit_test_setup(NumbersThatDontCountUp, Setup),
		cmocka_unit_test_setup(NoAutoNumberSharesTheNumber, Setup),
		cmocka_unit_test_setup(EmptyNotesClearOldNotes, Setup),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
