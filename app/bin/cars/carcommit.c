/** \file carcommit.c
 * Atomic commit orchestration for the car edit dialog.
 */

/*  XTrkCad - Model Railroad CAD
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

/* CommitStaged sequences real object creation across carproto.c / carpart.c /
 * caritem.c, so it lives here rather than in careditlogic.c: the latter is the
 * dependency-free "pure logic" unit whose test (careditlogictest) links only
 * libc-level sources. This file is compiled into the cars library and linked
 * by carregistrytest, which already links carproto/carpart/listelem; the item
 * path (CarItemNew/CarItemDelete) is exercised there via cmocka --wrap mocks
 * so the track/draw engine need not be linked. See carsprivate.h for the
 * commitPlan_t / CommitStaged contract. */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "fileio.h"

#include "carsprivate.h"


/* --- atomic commit ------------------------------------------------------ *
 * See commitPlan_t / CommitStaged in carsprivate.h for the contract. The
 * function is deliberately UI-free: no dialog controls, no file opening, no
 * prefs. Persistence is the injected `save` hook; index/prefs/list refresh
 * are the caller's job, driven by the returned commitResult_t. */

/* Join the seven title fields into the tab string CarPartNew / CarItemNew
 * re-extract. Field order matches careditdlg.c:2497 and CarPartWrite's
 * extract: manuf, proto, desc, partno, roadname, repmark, number. Every
 * field emits its separator even when empty, so positions never shift.
 * numberOverride, when non-NULL, replaces the plan's `number` (used by the
 * quantity loop's per-car auto-numbering). Returns an owned buffer. */
static char *CommitBuildTitle( const commitPlan_t *plan,
                               const char *numberOverride )
{
	const char *n = numberOverride ? numberOverride : (plan->number ? plan->number :
	                "");
	const char *manuf    = plan->manuf     ? plan->manuf     : "";
	const char *proto    = plan->protoName ? plan->protoName : "";
	const char *desc     = plan->descField ? plan->descField : "";
	const char *partno   = plan->partno    ? plan->partno    : "";
	const char *roadname = plan->roadname  ? plan->roadname  : "";
	const char *repmark  = plan->repmark   ? plan->repmark   : "";
	size_t len = strlen(manuf) + strlen(proto) + strlen(desc) + strlen(partno) +
	             strlen(roadname) + strlen(repmark) + strlen(n) + 7 /* tabs */ + 1;
	char *title = (char *)MyMalloc( len );
	sprintf( title, "%s\t%s\t%s\t%s\t%s\t%s\t%s",
	         manuf, proto, desc, partno, roadname, repmark, n );
	return title;
}

/* Bump the trailing integer of `src` into `dst` (size dstSize) iff src is a
 * clean positive integer, mirroring careditdlg.c:2536-2542. Returns TRUE if
 * it bumped; on FALSE dst is a copy of src unchanged (all cars share the
 * number). dst and src may differ. */
static wBool_t CommitBumpNumber( char *dst, size_t dstSize, const char *src )
{
	char *cp;
	long v;
	errno = 0;
	v = strtol( src, &cp, 10 );
	if ( errno != ERANGE && cp && *cp == '\0' && v > 0 ) {
		snprintf( dst, dstSize, "%ld", v + 1 );
		return TRUE;
	}
	if ( dst != src ) {
		snprintf( dst, dstSize, "%s", src );
	}
	return FALSE;
}

BOOL_T
CommitStaged( const commitPlan_t *plan, commitSaveFn_t save, void *saveCtx,
              commitResult_t *out )
{
	commitResult_t res;
	carProto_p createdProto = NULL;   /* non-NULL only if WE minted it (rollback) */
	carPart_p  createdPart  = NULL;   /* non-NULL only if WE minted it (rollback) */
	carItem_p *createdItems = NULL;   /* items WE appended, for reverse rollback */
	long       nCreated = 0;
	long       i;
	char       numBuf[64];
	const char *numForItem;
	wBool_t    bumping;

	memset( &res, 0, sizeof res );

	/* ---- 1. prototype ------------------------------------------------ *
	 * Edit (updateProtoPtr set) mutates that record in place and is never
	 * rolled back. Create: snapshot pre-existence -- only schedule
	 * rollback-delete when the desc did NOT already resolve to a record,
	 * because CarProtoNew coalesces onto an existing custom proto (returns
	 * it mutated), which we must never delete on rollback. */
	if ( plan->makeProto && plan->protoName && plan->protoName[0] ) {
		carProto_p pre = ( plan->updateProtoPtr == NULL )
		                 ? CarProtoFind( plan->protoName ) : plan->updateProtoPtr;
		carProto_p proto = CarProtoNew( plan->updateProtoPtr, PARAM_CUSTOM,
		                                plan->protoName, plan->options, plan->type,
		                                &plan->protoDim, plan->protoSegCnt,
		                                plan->protoSegPtr );
		if ( proto == NULL ) {
			goto fail;              /* nothing created yet; segs freed at cleanup */
		}
		res.protoP = proto;
		if ( plan->updateProtoPtr == NULL && pre == NULL ) {
			createdProto = proto;   /* freshly minted -> ours to unwind */
		}
	}

	/* ---- 2. catalog part --------------------------------------------- *
	 * Same edit/create/coalesce rule as the prototype step. On edit, the
	 * pre-existence lookup is skipped (updatePartPtr is the record); on
	 * create, CarPartFind snapshots whether CarPartNew will coalesce. */
	if ( plan->makePart ) {
		char *title = CommitBuildTitle( plan, NULL );
		carPart_p pre;
		carPart_p part;
		if ( plan->updatePartPtr != NULL ) {
			pre = plan->updatePartPtr;
		} else {
			pre = CarPartFind( plan->manuf,
			                   (int)strlen(plan->manuf ? plan->manuf : ""),
			                   plan->partno,
			                   (int)strlen(plan->partno ? plan->partno : ""),
			                   plan->scaleInx );
		}
		part = CarPartNew( plan->updatePartPtr, PARAM_CUSTOM, plan->scaleInx, title,
		                   plan->options, plan->type, &plan->dim, plan->color );
		MyFree( title );
		if ( part == NULL ) {
			goto fail;
		}
		res.partP = part;
		if ( plan->updatePartPtr == NULL && pre == NULL ) {
			createdPart = part;
		}
	}

	/* ---- 3. roster cars ---------------------------------------------- */
	if ( plan->makeItems ) {
		long qty = ( plan->updateItemPtr != NULL ) ? 1 : plan->quantity;
		if ( qty < 1 ) { qty = 1; }

		/* Only genuinely-appended items need rollback tracking; an in-place
		 * edit (updateItemPtr) mutates an existing record and is never
		 * unwound by deletion here. */
		if ( plan->updateItemPtr == NULL ) {
			createdItems = (carItem_p *)MyMalloc( qty * sizeof *createdItems );
		}

		numForItem = plan->number ? plan->number : "";
		numBuf[0] = '\0';
		bumping = FALSE;

		for ( i = 0; i < qty; i++ ) {
			carItem_p item;
			char *title;

			/* First car uses the plan number; subsequent cars use the
			 * bumped copy when auto-numbering is active and the number is a
			 * clean positive int. Never mutate plan->number. */
			if ( i > 0 && plan->autoNumber && plan->updateItemPtr == NULL ) {
				if ( i == 1 ) {
					bumping = CommitBumpNumber( numBuf, sizeof numBuf, numForItem );
					numForItem = numBuf;
				} else if ( bumping ) {
					CommitBumpNumber( numBuf, sizeof numBuf, numBuf );
				}
			}

			title = CommitBuildTitle( plan,
			                          ( i > 0 && bumping ) ? numForItem : NULL );
			item = CarItemNew( plan->updateItemPtr, PARAM_CUSTOM,
			                   plan->itemIndexStart + i, plan->scaleInx, title,
			                   plan->options, plan->type,
			                   (carDim_t *)&plan->dim, /* CarItemNew's dim is non-const
			                    * but it only copies *dim; plan stays const */
			                   plan->color,
			                   plan->purchPrice, plan->currPrice, plan->condition,
			                   plan->purchDate, plan->serviceDate );
			MyFree( title );
			if ( item == NULL ) {
				goto fail;
			}

			/* notes: one field, applied to every car (see contract). */
			if ( plan->notesText && plan->notesText[0] ) {
				if ( item->data.notes ) { MyFree( item->data.notes ); }
				item->data.notes = MyStrdup( plan->notesText );
			} else if ( item->data.notes ) {
				MyFree( item->data.notes );
				item->data.notes = NULL;
			}

			if ( createdItems != NULL ) {
				createdItems[nCreated] = item;
			}
			nCreated++;
			res.lastItemP = item;
		}
	}

	/* ---- 4. persist (once, only after everything exists) ------------- */
	if ( save != NULL ) {
		if ( !save( saveCtx ) ) {
			goto fail;
		}
	}

	/* success */
	res.itemsCreated  = nCreated;
	res.nextItemIndex = plan->itemIndexStart + nCreated;
	if ( createdItems != NULL ) { MyFree( createdItems ); }
	if ( plan->protoSegPtr != NULL ) { MyFree( plan->protoSegPtr ); }
	if ( out != NULL ) { *out = res; }
	return TRUE;

fail:
	/* Reverse-order unwind of exactly what THIS commit minted. Items first
	 * (newest to oldest), then part, then proto. Coalesced-onto existing
	 * records (createdX == NULL) are left alone. */
	if ( createdItems != NULL ) {
		while ( nCreated > 0 ) {
			nCreated--;
			CarItemDelete( createdItems[nCreated] );
		}
		MyFree( createdItems );
	}
	if ( createdPart != NULL ) {
		CarPartDelete( createdPart );
	}
	if ( createdProto != NULL ) {
		CarProtoDelete( createdProto );
	}
	if ( plan->protoSegPtr != NULL ) { MyFree( plan->protoSegPtr ); }
	if ( out != NULL ) { memset( out, 0, sizeof *out ); }
	return FALSE;
}
