/*
	flow.c

	PVS PHS generator tool

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2002 Colin Thompson

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
		Boston, MA	02111-1307, USA

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <getopt.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "QF/alloc.h"
#include "QF/bspfile.h"
#include "QF/cmd.h"
#include "QF/mathlib.h"
#include "QF/quakefs.h"
#include "QF/sys.h"

#include "vis.h"
#include "options.h"

static int
CheckStack (cluster_t *cluster, threaddata_t *thread)
{
	pstack_t   *portal;

	for (portal = thread->pstack_head.next; portal; portal = portal->next)
		if (portal->cluster == cluster) {
			printf ("CheckStack: cluster recursion\n");
			return 1;
		}
	return 0;
}

static sep_t *
new_separator (threaddata_t *thread)
{
	sep_t      *sep;

	ALLOC (128, sep_t, thread->sep, sep);
	return sep;
}

static void
delete_separator (threaddata_t *thread, sep_t *sep)
{
	FREE (thread->sep, sep);
}

static void
free_separators (threaddata_t *thread, sep_t *sep_list)
{
	while (sep_list) {
		sep_t      *sep = sep_list;
		sep_list = sep->next;
		delete_separator (thread, sep);
	}
}

/*
	Find the planes separating source from pass. The planes form a double
	pyramid with source as the base (ie, source's edges will all be in one
	plane each) and the vertex of the pyramid is between source and pass.
	Edges from pass may or may not be in a plane, but each vertex will be in
	at least one plane.

	If flip is false, the planes will be such that the space enclosed by the
	planes and on the pass side of the vertex are on the front sides of the
	planes. If flip is true, then the space on the source side of the vertex
	and enclosed by the planes is on the front side of the planes.
*/
static sep_t *
FindSeparators (threaddata_t *thread, winding_t *source, winding_t *pass,
				int flip)
{
	float       d;
	int         i, j, k, l;
	int         counts[3];
	qboolean    fliptest;
	plane_t     plane;
	vec3_t      v1, v2;
	vec_t       length;

	sep_t      *separators = 0;

	for (i = 0; i < source->numpoints; i++) {
		l = (i + 1) % source->numpoints;
		VectorSubtract (source->points[l], source->points[i], v1);

		// find a vertex of pass that makes a plane that puts all of the
		// vertexes of pass on the front side and all of the vertexes of
		// source on the back side
		for (j = 0; j < pass->numpoints; j++) {
			VectorSubtract (pass->points[j], source->points[i], v2);

			plane.normal[0] = v1[1] * v2[2] - v1[2] * v2[1];
			plane.normal[1] = v1[2] * v2[0] - v1[0] * v2[2];
			plane.normal[2] = v1[0] * v2[1] - v1[1] * v2[0];

			length = DotProduct (plane.normal, plane.normal);

			// if points don't make a valid plane, skip it
			if (length < ON_EPSILON)
				continue;

			length = 1 / sqrt (length);
			VectorScale (plane.normal, length, plane.normal);

			plane.dist = DotProduct (pass->points[j], plane.normal);

			// find out which side of the generated seperating plane has the
			// source portal
			fliptest = false;
			for (k = 0; k < source->numpoints; k++) {
				if (k == i || k == l)
					continue;
				d = DotProduct (source->points[k], plane.normal) - plane.dist;
				if (d < -ON_EPSILON) {
					// source is on the negative side, so we want all
					// pass and target on the positive side
					fliptest = false;
					break;
				} else if (d > ON_EPSILON) {
					// source is on the positive side, so we want all
					// pass and target on the negative side
					fliptest = true;
					break;
				}
			}
			if (k == source->numpoints)
				continue;	// planar with source portal

			// flip the normal if the source portal is backwards
			if (fliptest) {
				VectorNegate (plane.normal, plane.normal);
				plane.dist = -plane.dist;
			}

			// if all of the pass portal points are now on the positive side,
			// this is the seperating plane
			counts[0] = counts[1] = counts[2] = 0;
			for (k = 0; k < pass->numpoints; k++) {
				if (k == j)
					continue;
				d = DotProduct (pass->points[k], plane.normal) - plane.dist;
				if (d < -ON_EPSILON)
					break;
				else if (d > ON_EPSILON)
					counts[0]++;
				else
					counts[2]++;
			}
			if (k != pass->numpoints)
				continue;	// points on negative side, not a seperating plane

			if (!counts[0]) {
				continue;	// planar with seperating plane
			}

			// flip the normal if we want the back side
			if (flip) {
				VectorNegate (plane.normal, plane.normal);
				plane.dist = -plane.dist;
			}

			sep_t *sep = new_separator (thread);
			sep->plane = plane;
			sep->next = separators;
			separators = sep;
			break;
		}
	}
	return separators;
}

static winding_t *
ClipToSeparators (const sep_t *separators, winding_t *target)
{
	const sep_t *sep;

	for (sep = separators; target && sep; sep = sep->next) {
		target = ClipWinding (target, &sep->plane, false);
	}
	return target;
}

static inline set_t *
select_test_set (portal_t *portal, threaddata_t *thread)
{
	set_t      *test;

	if (portal->status == stat_done) {
		thread->stats.vistest++;
		test = portal->visbits;
	} else {
		thread->stats.mighttest++;
		test = portal->mightsee;
	}
	return test;
}

static inline int
mightsee_more (set_t *might, const set_t *prev_might, const set_t *test,
			   const set_t *vis)
{
	set_assign (might, prev_might);
	set_intersection (might, test);
	return !set_is_subset (vis, might);
}

/*
	RecursiveClusterFlow

	Flood fill through the clusters
	If src_portal is NULL, this is the originating cluster
*/
static void
RecursiveClusterFlow (int clusternum, threaddata_t *thread, pstack_t *prevstack)
{
	int         i;
	set_t      *might;
	const set_t *test, *vis;
	cluster_t  *cluster;
	pstack_t    stack;
	portal_t   *portal;
	plane_t     backplane;
	winding_t  *source, *target;

	thread->stats.chains++;

	cluster = &clusters[clusternum];
	if (CheckStack(cluster, thread))
		return;

	// mark the cluster as visible
	if (!set_is_member (thread->clustervis, clusternum)) {
		set_add (thread->clustervis, clusternum);
		thread->base->numcansee++;
	}

	prevstack->next = &stack;
	stack.next = NULL;
	stack.cluster = cluster;
	stack.portal = NULL;
	LOCK;
	stack.mightsee = set_new_size (portalclusters);
	UNLOCK;
	stack.separators[0] = 0;
	stack.separators[1] = 0;
	might = stack.mightsee;
	vis = thread->clustervis;

	// check all portals for flowing into other clusters
	for (i = 0; i < cluster->numportals; i++) {
		portal = cluster->portals[i];

		if (!set_is_member (prevstack->mightsee, portal->cluster))
			continue;		// can't possibly see it

		// if the portal can't see anything we haven't already seen, skip it
		test = select_test_set (portal, thread);
		if (!mightsee_more (might, prevstack->mightsee, test, vis)) {
			// can't see anything new
			continue;
		}

		// get plane of portal, point normal into the neighbor cluster
		stack.portalplane = portal->plane;
		VectorNegate (portal->plane.normal, backplane.normal);
		backplane.dist = -portal->plane.dist;

		if (_VectorCompare (prevstack->portalplane.normal, backplane.normal))
			continue;		// can't go out a coplanar face

		thread->stats.portalcheck++;

		stack.portal = portal;
		stack.next = NULL;

		target = ClipWinding (portal->winding,
							  &thread->pstack_head.portalplane, false);
		if (!target)
			continue;

		if (!prevstack->pass) {
			// the second cluster can be blocked only if coplanar

			stack.source = prevstack->source;
			stack.pass = target;
			RecursiveClusterFlow (portal->cluster, thread, &stack);
			FreeWinding (target);
			continue;
		}

		target = ClipWinding (target, &prevstack->portalplane, false);
		if (!target)
			continue;

		source = CopyWinding (prevstack->source);

		source = ClipWinding (source, &backplane, false);
		if (!source) {
			FreeWinding (target);
			continue;
		}

		thread->stats.portaltest++;
		thread->stats.targettested++;

		if (options.level > 0) {
			// clip target to the image that would be formed by a laser
			// pointing from the edges of source passing though the corners of
			// pass
			winding_t  *old = target;
			if (!stack.separators[0])
				stack.separators[0] = FindSeparators (thread, source,
													  prevstack->pass, 0);
			target = ClipToSeparators (stack.separators[0], target);
			if (!target) {
				thread->stats.targetclipped++;
				FreeWinding (source);
				continue;
			}
			if (target != old)
				thread->stats.targettrimmed++;
		}

		if (options.level > 1) {
			// now pass the laser along the edges of pass from the corners of
			// source. the resulting image will have a smaller aree. The
			// resulting shape will be the light image produced by a backlit
			// source shining past pass. eg, if source and pass are equilateral
			// triangles rotated 60 (or 180) degrees relative to each other,
			// parallel and in line, target will wind up being a hexagon.
			winding_t  *old = target;
			if (!stack.separators[1])
				stack.separators[1] = FindSeparators (thread, prevstack->pass,
													  source, 1);
			target = ClipToSeparators (stack.separators[1], target);
			if (!target) {
				thread->stats.targetclipped++;
				FreeWinding (source);
				continue;
			}
			if (target != old)
				thread->stats.targettrimmed++;
		}

		thread->stats.sourcetested++;
		// now do the same as for levels 1 and 2, but trimming source using
		// the trimmed target
		if (options.level > 2) {
			winding_t  *old = source;
			sep_t      *sep;
			sep = FindSeparators (thread, target, prevstack->pass, 0);
			source = ClipToSeparators (sep, source);
			free_separators (thread, sep);
			if (!source) {
				thread->stats.sourceclipped++;
				FreeWinding (target);
				continue;
			}
			if (source != old)
				thread->stats.sourcetrimmed++;
		}

		if (options.level > 3) {
			winding_t  *old = source;
			sep_t      *sep;
			sep = FindSeparators (thread, prevstack->pass, target, 1);
			source = ClipToSeparators (sep, source);
			free_separators (thread, sep);
			if (!source) {
				thread->stats.sourceclipped++;
				FreeWinding (target);
				continue;
			}
			if (source != old)
				thread->stats.sourcetrimmed++;
		}

		stack.source = source;
		stack.pass = target;

		thread->stats.portalpass++;

		// flow through it for real
		RecursiveClusterFlow (portal->cluster, thread, &stack);

		FreeWinding (source);
		FreeWinding (target);
	}
	free_separators (thread, stack.separators[1]);
	free_separators (thread, stack.separators[0]);

	LOCK;
	set_delete (stack.mightsee);
	UNLOCK;
}

void
PortalFlow (threaddata_t *data, portal_t *portal)
{
	LOCK;
	if (portal->status != stat_selected)
		Sys_Error ("PortalFlow: reflowed");
	portal->status = stat_working;
	portal->visbits = set_new_size (portalclusters);
	UNLOCK;

	data->clustervis = portal->visbits;
	data->base = portal;

	memset (&data->pstack_head, 0, sizeof (data->pstack_head));
	data->pstack_head.portal = portal;
	data->pstack_head.source = portal->winding;
	data->pstack_head.portalplane = portal->plane;
	data->pstack_head.mightsee = portal->mightsee;

	RecursiveClusterFlow (portal->cluster, data, &data->pstack_head);
}
