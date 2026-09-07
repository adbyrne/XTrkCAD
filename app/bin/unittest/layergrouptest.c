/** \file layergrouptest.c
 * Unit tests for dlayergroup.c's pure group data-model/registry logic
 * (Layer Groups, SF #222 phase 0, SF #782). Written test-first, per
 * feedback_test_first_where_practical, before dlayergroup.c's real
 * implementation existed.
 *
 * dlayergroup.c deliberately carries no dependency on common.h/dynarray.h/
 * wlib (see its header comment), so -- like reportstest.c/reportsformat.c --
 * this links the real source directly with no stubs.
 *
 * Each test starts with LayerGroupResetAll() since the group registry is
 * process-global state shared across every test in this file's single
 * cmocka_run_group_tests() run.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>

#include <cmocka.h>

#include <string.h>

#include "../include/dlayergroup.h"

static void test_starts_empty(void **state)
{
	(void) state;
	LayerGroupResetAll();

	assert_int_equal(LayerGroupCount(), 0);
}

static void test_create_returns_increasing_index(void **state)
{
	(void) state;
	LayerGroupResetAll();

	int a = LayerGroupCreate("Level 1");
	int b = LayerGroupCreate("Level 2");

	assert_int_equal(a, 0);
	assert_int_equal(b, 1);
	assert_int_equal(LayerGroupCount(), 2);
	assert_string_equal(LayerGroupName(a), "Level 1");
	assert_string_equal(LayerGroupName(b), "Level 2");
}

static void test_create_null_or_empty_name_fails(void **state)
{
	(void) state;
	LayerGroupResetAll();

	assert_int_equal(LayerGroupCreate(NULL), -1);
	assert_int_equal(LayerGroupCreate(""), -1);
	assert_int_equal(LayerGroupCount(), 0);
}

static void test_name_on_invalid_index_is_null(void **state)
{
	(void) state;
	LayerGroupResetAll();

	assert_null(LayerGroupName(0));
	assert_null(LayerGroupName(-1));
	LayerGroupCreate("Only");
	assert_null(LayerGroupName(1));
}

static void test_rename(void **state)
{
	(void) state;
	LayerGroupResetAll();
	int g = LayerGroupCreate("Old Name");

	assert_int_equal(LayerGroupRename(g, "New Name"), 1);
	assert_string_equal(LayerGroupName(g), "New Name");
}

static void test_rename_invalid_index_or_name_fails(void **state)
{
	(void) state;
	LayerGroupResetAll();
	int g = LayerGroupCreate("Name");

	assert_int_equal(LayerGroupRename(-1, "X"), 0);
	assert_int_equal(LayerGroupRename(1, "X"), 0);
	assert_int_equal(LayerGroupRename(g, NULL), 0);
	assert_int_equal(LayerGroupRename(g, ""), 0);
	assert_string_equal(LayerGroupName(g), "Name");
}

static void test_delete_shifts_later_indices_down(void **state)
{
	(void) state;
	LayerGroupResetAll();
	int a = LayerGroupCreate("A");
	int b = LayerGroupCreate("B");
	int c = LayerGroupCreate("C");
	(void) a;

	assert_int_equal(LayerGroupDelete(b), 1);
	assert_int_equal(LayerGroupCount(), 2);
	assert_string_equal(LayerGroupName(0), "A");
	/* what was C, index 2, is now index 1 */
	assert_string_equal(LayerGroupName(1), "C");
	(void) c;
}

static void test_delete_invalid_index_fails(void **state)
{
	(void) state;
	LayerGroupResetAll();
	LayerGroupCreate("A");

	assert_int_equal(LayerGroupDelete(-1), 0);
	assert_int_equal(LayerGroupDelete(1), 0);
	assert_int_equal(LayerGroupCount(), 1);
}

static void test_membership_add_has_remove(void **state)
{
	(void) state;
	LayerGroupResetAll();
	int g = LayerGroupCreate("Staging");

	assert_int_equal(LayerGroupHasMember(g, 5), 0);
	assert_int_equal(LayerGroupAddMember(g, 5), 1);
	assert_int_equal(LayerGroupHasMember(g, 5), 1);
	assert_int_equal(LayerGroupMemberCount(g), 1);

	assert_int_equal(LayerGroupRemoveMember(g, 5), 1);
	assert_int_equal(LayerGroupHasMember(g, 5), 0);
	assert_int_equal(LayerGroupMemberCount(g), 0);
}

/* A layer can be in any, all, or none of the defined groups -- flat,
 * independent membership per Q1/Q6 of the design doc. */
static void test_layer_can_be_in_multiple_groups(void **state)
{
	(void) state;
	LayerGroupResetAll();
	int level1 = LayerGroupCreate("Level 1");
	int main_g = LayerGroupCreate("Main");

	LayerGroupAddMember(level1, 3);
	LayerGroupAddMember(main_g, 3);

	assert_int_equal(LayerGroupHasMember(level1, 3), 1);
	assert_int_equal(LayerGroupHasMember(main_g, 3), 1);
}

static void test_add_member_is_idempotent(void **state)
{
	(void) state;
	LayerGroupResetAll();
	int g = LayerGroupCreate("G");

	assert_int_equal(LayerGroupAddMember(g, 2), 1);
	assert_int_equal(LayerGroupAddMember(g, 2), 1);
	assert_int_equal(LayerGroupMemberCount(g), 1);
}

static void test_remove_nonmember_still_succeeds(void **state)
{
	(void) state;
	LayerGroupResetAll();
	int g = LayerGroupCreate("G");

	assert_int_equal(LayerGroupRemoveMember(g, 99), 1);
	assert_int_equal(LayerGroupMemberCount(g), 0);
}

static void test_membership_ops_on_invalid_group(void **state)
{
	(void) state;
	LayerGroupResetAll();

	assert_int_equal(LayerGroupAddMember(0, 1), 0);
	assert_int_equal(LayerGroupRemoveMember(0, 1), 0);
	assert_int_equal(LayerGroupHasMember(0, 1), 0);
	assert_int_equal(LayerGroupMemberCount(0), -1);
	assert_int_equal(LayerGroupMemberAt(0, 0), -1);
}

static void test_member_at_preserves_insertion_order(void **state)
{
	(void) state;
	LayerGroupResetAll();
	int g = LayerGroupCreate("G");

	LayerGroupAddMember(g, 7);
	LayerGroupAddMember(g, 2);
	LayerGroupAddMember(g, 9);

	assert_int_equal(LayerGroupMemberAt(g, 0), 7);
	assert_int_equal(LayerGroupMemberAt(g, 1), 2);
	assert_int_equal(LayerGroupMemberAt(g, 2), 9);
	assert_int_equal(LayerGroupMemberAt(g, 3), -1);
}

/* File-format round-trip: LayerGroupFormatMembers/LayerGroupParseMembers
 * use the same semicolon-joined 1-based-index convention as the existing
 * layerLinkList mechanism (GetLayerLinkString/PutLayerListArray). */
static void test_format_members_empty(void **state)
{
	(void) state;
	LayerGroupResetAll();
	int g = LayerGroupCreate("G");
	char buf[64];

	LayerGroupFormatMembers(g, buf, sizeof buf);

	assert_string_equal(buf, "");
}

static void test_format_members_list(void **state)
{
	(void) state;
	LayerGroupResetAll();
	int g = LayerGroupCreate("G");
	LayerGroupAddMember(g, 2);
	LayerGroupAddMember(g, 5);
	LayerGroupAddMember(g, 7);
	char buf[64];

	LayerGroupFormatMembers(g, buf, sizeof buf);

	assert_string_equal(buf, "2;5;7");
}

static void test_format_members_invalid_group_is_empty(void **state)
{
	(void) state;
	LayerGroupResetAll();
	char buf[64] = "unchanged";

	LayerGroupFormatMembers(0, buf, sizeof buf);

	assert_string_equal(buf, "");
}

static void test_parse_members_round_trips_format(void **state)
{
	(void) state;
	LayerGroupResetAll();
	int src = LayerGroupCreate("Src");
	LayerGroupAddMember(src, 2);
	LayerGroupAddMember(src, 5);
	LayerGroupAddMember(src, 7);
	char buf[64];
	LayerGroupFormatMembers(src, buf, sizeof buf);

	int dst = LayerGroupCreate("Dst");
	LayerGroupParseMembers(dst, buf);

	assert_int_equal(LayerGroupMemberCount(dst), 3);
	assert_int_equal(LayerGroupMemberAt(dst, 0), 2);
	assert_int_equal(LayerGroupMemberAt(dst, 1), 5);
	assert_int_equal(LayerGroupMemberAt(dst, 2), 7);
}

/* PutLayerListArray accepts ",", ";", and " " as separators and skips
 * self-references / out-of-range values -- LayerGroupParseMembers doesn't
 * need the self-reference exclusion (a group has no "self" layer the way
 * a per-layer link list does), but should accept the same separator set. */
static void test_parse_members_accepts_mixed_separators(void **state)
{
	(void) state;
	LayerGroupResetAll();
	int g = LayerGroupCreate("G");

	LayerGroupParseMembers(g, "3,7;9 12");

	assert_int_equal(LayerGroupMemberCount(g), 4);
	assert_int_equal(LayerGroupMemberAt(g, 0), 3);
	assert_int_equal(LayerGroupMemberAt(g, 1), 7);
	assert_int_equal(LayerGroupMemberAt(g, 2), 9);
	assert_int_equal(LayerGroupMemberAt(g, 3), 12);
}

static void test_parse_members_replaces_existing(void **state)
{
	(void) state;
	LayerGroupResetAll();
	int g = LayerGroupCreate("G");
	LayerGroupAddMember(g, 1);
	LayerGroupAddMember(g, 2);

	LayerGroupParseMembers(g, "9");

	assert_int_equal(LayerGroupMemberCount(g), 1);
	assert_int_equal(LayerGroupMemberAt(g, 0), 9);
}

static void test_parse_members_empty_string_clears(void **state)
{
	(void) state;
	LayerGroupResetAll();
	int g = LayerGroupCreate("G");
	LayerGroupAddMember(g, 1);

	LayerGroupParseMembers(g, "");

	assert_int_equal(LayerGroupMemberCount(g), 0);
}

/* --- Migration from the old per-layer Linked-Layers mechanism --------- */

/* No layer has a link list -- migration creates nothing. */
static void test_migrate_no_links_creates_nothing(void **state)
{
	(void) state;
	LayerGroupResetAll();
	int linkCounts[3] = {0, 0, 0};

	int created = LayerGroupMigrateFromLinkLists(3, NULL, linkCounts);

	assert_int_equal(created, 0);
	assert_int_equal(LayerGroupCount(), 0);
}

/* One layer (0-based index 0, i.e. 1-based layer 1) links to layers 6 and
 * 7 -- one group created containing {1, 6, 7} (the layer itself plus its
 * link-list targets, 1-based). */
static void test_migrate_single_layer_link_list(void **state)
{
	(void) state;
	LayerGroupResetAll();
	int layer0Links[2] = {6, 7};
	int *linkLists[1] = { layer0Links };
	int linkCounts[1] = {2};

	int created = LayerGroupMigrateFromLinkLists(1, linkLists, linkCounts);

	assert_int_equal(created, 1);
	assert_int_equal(LayerGroupCount(), 1);
	assert_int_equal(LayerGroupMemberCount(0), 3);
	assert_int_equal(LayerGroupHasMember(0, 1), 1);
	assert_int_equal(LayerGroupHasMember(0, 6), 1);
	assert_int_equal(LayerGroupHasMember(0, 7), 1);
}

/* A mutual link (layer 1 links to layer 2, and layer 2 links to layer 1)
 * migrates to a single group, not two duplicates -- the common real-world
 * shape (e.g. the shipped "Ondaville..." example's LAYERS LINK usage). */
static void test_migrate_mutual_link_dedupes_to_one_group(void **state)
{
	(void) state;
	LayerGroupResetAll();
	int layer0Links[1] = {2};
	int layer1Links[1] = {1};
	int *linkLists[2] = { layer0Links, layer1Links };
	int linkCounts[2] = {1, 1};

	int created = LayerGroupMigrateFromLinkLists(2, linkLists, linkCounts);

	assert_int_equal(created, 1);
	assert_int_equal(LayerGroupCount(), 1);
	assert_int_equal(LayerGroupMemberCount(0), 2);
	assert_int_equal(LayerGroupHasMember(0, 1), 1);
	assert_int_equal(LayerGroupHasMember(0, 2), 1);
}

/* Two unrelated link lists -- two distinct groups, auto-named in order. */
static void test_migrate_multiple_distinct_groups(void **state)
{
	(void) state;
	LayerGroupResetAll();
	int layer0Links[1] = {3};
	int layer4Links[1] = {6};
	int *linkLists[5] = { layer0Links, NULL, NULL, NULL, layer4Links };
	int linkCounts[5] = {1, 0, 0, 0, 1};

	int created = LayerGroupMigrateFromLinkLists(5, linkLists, linkCounts);

	assert_int_equal(created, 2);
	assert_int_equal(LayerGroupCount(), 2);
	assert_string_equal(LayerGroupName(0), "Group 1");
	assert_string_equal(LayerGroupName(1), "Group 2");
}

/* Migrated group names must not collide with a group that already exists
 * (e.g. one the user already created by hand before migration runs). */
static void test_migrate_names_avoid_existing_groups(void **state)
{
	(void) state;
	LayerGroupResetAll();
	LayerGroupCreate("Group 1");
	int layer0Links[1] = {3};
	int *linkLists[1] = { layer0Links };
	int linkCounts[1] = {1};

	int created = LayerGroupMigrateFromLinkLists(1, linkLists, linkCounts);

	assert_int_equal(created, 1);
	assert_string_equal(LayerGroupName(0), "Group 1");
	assert_string_equal(LayerGroupName(1), "Group 2");
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_starts_empty),
		cmocka_unit_test(test_create_returns_increasing_index),
		cmocka_unit_test(test_create_null_or_empty_name_fails),
		cmocka_unit_test(test_name_on_invalid_index_is_null),
		cmocka_unit_test(test_rename),
		cmocka_unit_test(test_rename_invalid_index_or_name_fails),
		cmocka_unit_test(test_delete_shifts_later_indices_down),
		cmocka_unit_test(test_delete_invalid_index_fails),
		cmocka_unit_test(test_membership_add_has_remove),
		cmocka_unit_test(test_layer_can_be_in_multiple_groups),
		cmocka_unit_test(test_add_member_is_idempotent),
		cmocka_unit_test(test_remove_nonmember_still_succeeds),
		cmocka_unit_test(test_membership_ops_on_invalid_group),
		cmocka_unit_test(test_member_at_preserves_insertion_order),
		cmocka_unit_test(test_format_members_empty),
		cmocka_unit_test(test_format_members_list),
		cmocka_unit_test(test_format_members_invalid_group_is_empty),
		cmocka_unit_test(test_parse_members_round_trips_format),
		cmocka_unit_test(test_parse_members_accepts_mixed_separators),
		cmocka_unit_test(test_parse_members_replaces_existing),
		cmocka_unit_test(test_parse_members_empty_string_clears),
		cmocka_unit_test(test_migrate_no_links_creates_nothing),
		cmocka_unit_test(test_migrate_single_layer_link_list),
		cmocka_unit_test(test_migrate_mutual_link_dedupes_to_one_group),
		cmocka_unit_test(test_migrate_multiple_distinct_groups),
		cmocka_unit_test(test_migrate_names_avoid_existing_groups),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
