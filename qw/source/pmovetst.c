/*
	pmovetst.c

	(description)

	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
static const char rcsid[] = 
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/console.h"
#include "QF/model.h"
#include "QF/qtypes.h"
#include "QF/sys.h"

#include "compat.h"
#include "pmove.h"

static hull_t box_hull;
static dclipnode_t box_clipnodes[6];
static mplane_t box_planes[6];

extern vec3_t player_mins;
extern vec3_t player_maxs;


/*
	PM_InitBoxHull

	Set up the planes and clipnodes so that the six floats of a bounding box
	can just be stored out and get a proper hull_t structure.
*/
void
PM_InitBoxHull (void)
{
	int         side, i;

	box_hull.clipnodes = box_clipnodes;
	box_hull.planes = box_planes;
	box_hull.firstclipnode = 0;
	box_hull.lastclipnode = 5;

	for (i = 0; i < 6; i++) {
		box_clipnodes[i].planenum = i;

		side = i & 1;

		box_clipnodes[i].children[side] = CONTENTS_EMPTY;
		if (i != 5)
			box_clipnodes[i].children[side ^ 1] = i + 1;
		else
			box_clipnodes[i].children[side ^ 1] = CONTENTS_SOLID;

		box_planes[i].type = i >> 1;
		box_planes[i].normal[i >> 1] = 1;
	}

}

/*
	PM_HullForBox

	To keep everything totally uniform, bounding boxes are turned into small
	BSP trees instead of being compared directly.
*/
hull_t     *
PM_HullForBox (vec3_t mins, vec3_t maxs)
{
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = mins[0];
	box_planes[2].dist = maxs[1];
	box_planes[3].dist = mins[1];
	box_planes[4].dist = maxs[2];
	box_planes[5].dist = mins[2];

	return &box_hull;
}

int
PM_HullPointContents (hull_t *hull, int num, vec3_t p)
{
	dclipnode_t *node;
	float       d;
	mplane_t   *plane;

	while (num >= 0) {
		node = hull->clipnodes + num;
		plane = hull->planes + node->planenum;

		if (plane->type < 3)
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct (plane->normal, p) - plane->dist;
		if (d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}

	return num;
}

int
PM_PointContents (vec3_t p)
{
	dclipnode_t *node;
	float       d;
	hull_t     *hull;
	int         num;
	mplane_t   *plane;

	hull = &pmove.physents[0].model->hulls[0];

	num = hull->firstclipnode;

	while (num >= 0) {
		node = hull->clipnodes + num;
		plane = hull->planes + node->planenum;

		if (plane->type < 3)
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct (plane->normal, p) - plane->dist;
		if (d < 0)
			num = node->children[1];
		else
			num = node->children[0];
	}

	return num;
}

/* LINE TESTING IN HULLS */

// 1/32 epsilon to keep floating point happy
#define	DIST_EPSILON	(0.03125)

qboolean
PM_RecursiveHullCheck (hull_t *hull, int num, float p1f, float p2f, vec3_t p1,
					   vec3_t p2, pmtrace_t *trace)
{
	dclipnode_t *node;
	float       frac, midf, t1, t2;
	int         side, i;
	mplane_t   *plane;
	vec3_t      mid;

  loc0:
	// check for empty
	if (num < 0) {
		if (num != CONTENTS_SOLID) {
			trace->allsolid = false;
			if (num == CONTENTS_EMPTY)
				trace->inopen = true;
			else
				trace->inwater = true;
		} else
			trace->startsolid = true;
		return true;					// empty
	}

	// find the point distances
	node = hull->clipnodes + num;
	plane = hull->planes + node->planenum;

	if (plane->type < 3) {
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
	} else {
		t1 = DotProduct (plane->normal, p1) - plane->dist;
		t2 = DotProduct (plane->normal, p2) - plane->dist;
	}

	// LordHavoc: recursion optimization
	if (t1 >= 0 && t2 >= 0) {
		num = node->children[0];
		goto loc0;
	}
	if (t1 < 0 && t2 < 0) {
		num = node->children[1];
		goto loc0;
	}
	// put the crosspoint DIST_EPSILON pixels on the near side so that both
	// p1 and mid are on the same side of the plane
	side = (t1 < 0);
	if (side)
		frac = bound (0, (t1 + DIST_EPSILON) / (t1 - t2), 1);
	else
		frac = bound (0, (t1 - DIST_EPSILON) / (t1 - t2), 1);

	midf = p1f + (p2f - p1f) * frac;
	for (i = 0; i < 3; i++)
		mid[i] = p1[i] + frac * (p2[i] - p1[i]);

	// move up to the node
	if (!PM_RecursiveHullCheck (hull, node->children[side],
								p1f, midf, p1, mid, trace))
		return false;

#ifdef PARANOID
	if (PM_HullPointContents (pm_hullmodel, mid, node->children[side]) ==
		CONTENTS_SOLID) {
		Con_Printf ("mid PointInHullSolid\n");
		return false;
	}
#endif

	if (PM_HullPointContents (hull, node->children[side ^ 1],
							  mid) != CONTENTS_SOLID) {
		// go past the node
		return PM_RecursiveHullCheck (hull, node->children[side ^ 1], midf,
									  p2f, mid, p2, trace);
	}

	if (trace->allsolid)
		return false;					// never got out of the solid area

	//==================
	// the other side of the node is solid, this is the impact point
	//==================
	if (!side) {
		VectorCopy (plane->normal, trace->plane.normal);
		trace->plane.dist = plane->dist;
	} else {
		// invert plane paramterers
		trace->plane.normal[0] = -plane->normal[0];
		trace->plane.normal[1] = -plane->normal[1];
		trace->plane.normal[2] = -plane->normal[2];
		trace->plane.dist = -plane->dist;
	}

	while (PM_HullPointContents (hull, hull->firstclipnode,
								 mid) == CONTENTS_SOLID) {
		// shouldn't really happen, but does occasionally
		frac -= 0.1;
		if (frac < 0) {
			trace->fraction = midf;
			VectorCopy (mid, trace->endpos);
			Con_DPrintf ("backup past 0\n");
			return false;
		}
		midf = p1f + (p2f - p1f) * frac;
		for (i = 0; i < 3; i++)
			mid[i] = p1[i] + frac * (p2[i] - p1[i]);
	}

	trace->fraction = midf;
	VectorCopy (mid, trace->endpos);

	return false;
}

/*
	PM_TestPlayerPosition

	Returns false if the given player position is not valid (in solid)
*/
qboolean
PM_TestPlayerPosition (vec3_t pos)
{
	hull_t     *hull;
	int         i;
	physent_t  *pe;
	vec3_t      mins, maxs, test;

	for (i = 0; i < pmove.numphysent; i++) {
		pe = &pmove.physents[i];
		// get the clipping hull
		if (pe->model)
			hull = &pmove.physents[i].model->hulls[1];
		else {
			VectorSubtract (pe->mins, player_maxs, mins);
			VectorSubtract (pe->maxs, player_mins, maxs);
			hull = PM_HullForBox (mins, maxs);
		}

		VectorSubtract (pos, pe->origin, test);

		if (PM_HullPointContents (hull, hull->firstclipnode, test) ==
			CONTENTS_SOLID) return false;
	}

	return true;
}

/* PM_PlayerMove */
pmtrace_t
PM_PlayerMove (vec3_t start, vec3_t end)
{
	hull_t     *hull;
	int         i;
	physent_t  *pe;
	pmtrace_t   trace, total;
	vec3_t      maxs, mins, offset, start_l, end_l;

	// fill in a default trace
	memset (&total, 0, sizeof (pmtrace_t));

	total.fraction = 1;
	total.ent = -1;
	VectorCopy (end, total.endpos);

	for (i = 0; i < pmove.numphysent; i++) {
		pe = &pmove.physents[i];
		// get the clipping hull
		if (pe->hull) {
			hull = pe->hull;
		} else {
			if (pe->model) {
				hull = &pmove.physents[i].model->hulls[1];
			} else {
				VectorSubtract (pe->mins, player_maxs, mins);
				VectorSubtract (pe->maxs, player_mins, maxs);
				hull = PM_HullForBox (mins, maxs);
			}
		}

		// PM_HullForEntity (ent, mins, maxs, offset);
		VectorCopy (pe->origin, offset);

		VectorSubtract (start, offset, start_l);
		VectorSubtract (end, offset, end_l);

		// fill in a default trace
		memset (&trace, 0, sizeof (pmtrace_t));

		trace.fraction = 1;
		trace.allsolid = true;
//		trace.startsolid = true;
		VectorCopy (end, trace.endpos);

		// trace a line through the apropriate clipping hull
		PM_RecursiveHullCheck (hull, hull->firstclipnode, 0, 1, start_l, end_l,
							   &trace);

		if (trace.allsolid)
			trace.startsolid = true;
		if (trace.startsolid)
			trace.fraction = 0;

		// did we clip the move?
		if (trace.fraction < total.fraction) {
			// fix trace up by the offset
			VectorAdd (trace.endpos, offset, trace.endpos);
			total = trace;
			total.ent = i;
		}

	}

	return total;
}
