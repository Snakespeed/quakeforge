/*
	r_main.c

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#include <math.h>

#include "QF/cmd.h"

#include "QF/scene/entity.h"
#include "QF/scene/scene.h"

#include "compat.h"
#include "mod_internal.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_sw.h"

#ifdef PIC
# undef USE_INTEL_ASM //XXX asm pic hack
#endif

const byte *r_colormap;
int         r_numallocatededges;
qboolean    r_drawpolys;
qboolean    r_drawculledpolys;
qboolean    r_worldpolysbacktofront;
qboolean    r_recursiveaffinetriangles = true;
int         r_pixbytes = 1;
int         r_outofsurfaces;
int         r_outofedges;

qboolean    r_viewchanged;

int         c_surf;
int         r_maxsurfsseen, r_maxedgesseen;
static int  r_cnumsurfs;
static qboolean    r_surfsonstack;
int         r_clipflags;

static byte       *r_stack_start;

// screen size info
float       xcenter, ycenter;
float       xscale, yscale;
float       xscaleinv, yscaleinv;
float       xscaleshrink, yscaleshrink;
float       aliasxscale, aliasyscale, aliasxcenter, aliasycenter;

int         screenwidth;

float       pixelAspect;

plane_t     screenedge[4];

// refresh flags
int         r_polycount;
int         r_drawnpolycount;

int        *pfrustum_indexes[4];
int         r_frustum_indexes[4 * 6];

vec3_t      vup, base_vup;
vec3_t      vfwd, base_vfwd;
vec3_t      vright, base_vright;
float       r_viewmatrix[3][4];

float       r_aliastransition, r_resfudge;

void
sw_R_Init (void)
{
	int         dummy;

	// get stack position so we can guess if we are going to overflow
	r_stack_start = (byte *) & dummy;

	R_Init_Cvars ();

	Draw_Init ();
	SCR_Init ();
	R_SetFPCW ();
#ifdef USE_INTEL_ASM
	R_InitVars ();
#endif

	R_InitTurb ();

	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f, "Tests the current "
					"refresh rate for the current location");
	Cmd_AddCommand ("loadsky", R_LoadSky_f, "Load a skybox");

	r_maxedges = NUMSTACKEDGES;
	r_maxsurfs = NUMSTACKSURFACES;

	view_clipplanes[0].leftedge = true;
	view_clipplanes[1].rightedge = true;
	view_clipplanes[1].leftedge = view_clipplanes[2].leftedge =
		view_clipplanes[3].leftedge = false;
	view_clipplanes[0].rightedge = view_clipplanes[2].rightedge =
		view_clipplanes[3].rightedge = false;

// TODO: collect 386-specific code in one place
#ifdef USE_INTEL_ASM
	Sys_MakeCodeWriteable ((long) R_EdgeCodeStart,
						   (long) R_EdgeCodeEnd - (long) R_EdgeCodeStart);
#endif // USE_INTEL_ASM
	D_Init ();

	Skin_Init ();
}

void
R_NewScene (scene_t *scene)
{
	model_t    *worldmodel = scene->worldmodel;
	mod_brush_t *brush = &worldmodel->brush;

	r_refdef.worldmodel = worldmodel;

	// clear out efrags in case the level hasn't been reloaded
	for (unsigned i = 0; i < brush->modleafs; i++)
		brush->leafs[i].efrags = NULL;

	if (brush->skytexture)
		R_InitSky (brush->skytexture);

	// Force a vis update
	R_MarkLeaves (0, 0, 0, 0);

	R_ClearParticles ();

	r_cnumsurfs = r_maxsurfs;

	if (r_cnumsurfs <= MINSURFACES)
		r_cnumsurfs = MINSURFACES;

	if (r_cnumsurfs > NUMSTACKSURFACES) {
		surfaces = Hunk_AllocName (0, r_cnumsurfs * sizeof (surf_t),
								   "surfaces");

		surface_p = surfaces;
		surf_max = &surfaces[r_cnumsurfs];
		r_surfsonstack = false;
		// surface 0 doesn't really exist; it's just a dummy because index 0
		// is used to indicate no edge attached to surface
		surfaces--;
		R_SurfacePatch ();
	} else {
		r_surfsonstack = true;
	}

	r_maxedgesseen = 0;
	r_maxsurfsseen = 0;

	r_numallocatededges = r_maxedges;

	if (r_numallocatededges < MINEDGES)
		r_numallocatededges = MINEDGES;

	if (r_numallocatededges <= NUMSTACKEDGES) {
		auxedges = NULL;
	} else {
		auxedges = Hunk_AllocName (0, r_numallocatededges * sizeof (edge_t),
								   "edges");
	}

	r_dowarpold = false;
	r_viewchanged = false;
}

void
R_SetColormap (const byte *cmap)
{
	r_colormap = cmap;
// TODO: collect 386-specific code in one place
#ifdef USE_INTEL_ASM
	Sys_MakeCodeWriteable ((long) R_Surf8Start,
						   (long) R_Surf8End - (long) R_Surf8Start);
	R_SurfPatch ();
#endif // USE_INTEL_ASM
}

static inline void
draw_sprite_entity (entity_t *ent)
{
	R_DrawSprite (ent);
}

static inline void
setup_lighting (entity_t *ent, alight_t *lighting)
{
	float       minlight = 0;
	int         j;
	// FIXME: remove and do real lighting
	vec3_t      dist;
	float       add;
	float       lightvec[3] = { -1, 0, 0 };

	minlight = max (ent->renderer.model->min_light, ent->renderer.min_light);

	// 128 instead of 255 due to clamping below
	j = max (R_LightPoint (&r_refdef.worldmodel->brush, r_entorigin),
			 minlight * 128);

	lighting->ambientlight = j;
	lighting->shadelight = j;

	VectorCopy (lightvec, lighting->lightvec);

	for (unsigned lnum = 0; lnum < r_maxdlights; lnum++) {
		if (r_dlights[lnum].die >= vr_data.realtime) {
			VectorSubtract (r_entorigin, r_dlights[lnum].origin, dist);
			add = r_dlights[lnum].radius - VectorLength (dist);

			if (add > 0)
				lighting->ambientlight += add;
		}
	}

	// clamp lighting so it doesn't overbright as much
	if (lighting->ambientlight > 128)
		lighting->ambientlight = 128;
	if (lighting->ambientlight + lighting->shadelight > 192)
		lighting->shadelight = 192 - lighting->ambientlight;
}

static inline void
draw_alias_entity (entity_t *ent)
{
	// see if the bounding box lets us trivially reject, also
	// sets trivial accept status
	ent->visibility.trivial_accept = 0;	//FIXME
	if (R_AliasCheckBBox (ent)) {
		alight_t    lighting;
		setup_lighting (ent, &lighting);
		R_AliasDrawModel (ent, &lighting);
	}
}

static inline void
draw_iqm_entity (entity_t *ent)
{
	// see if the bounding box lets us trivially reject, also
	// sets trivial accept status
	ent->visibility.trivial_accept = 0;	//FIXME

	alight_t    lighting;
	setup_lighting (ent, &lighting);
	R_IQMDrawModel (ent, &lighting);
}

void
R_DrawEntitiesOnList (entqueue_t *queue)
{
	if (!r_drawentities)
		return;

	R_LowFPPrecision ();
#define RE_LOOP(type_name) \
	do { \
		for (size_t i = 0; i < queue->ent_queues[mod_##type_name].size; \
			 i++) { \
			entity_t   *ent = queue->ent_queues[mod_##type_name].a[i]; \
			r_entorigin = Transform_GetWorldPosition (ent->transform); \
			draw_##type_name##_entity (ent); \
		} \
	} while (0)

	RE_LOOP (alias);
	RE_LOOP (iqm);
	RE_LOOP (sprite);

	R_HighFPPrecision ();
}

static void
R_DrawViewModel (void)
{
	// FIXME: remove and do real lighting
	int         j;
	unsigned int lnum;
	vec3_t      dist;
	float       add;
	float       minlight;
	dlight_t   *dl;
	entity_t   *viewent;
	alight_t    lighting;

	if (vr_data.inhibit_viewmodel
		|| !r_drawviewmodel
		|| !r_drawentities)
		return;

	viewent = vr_data.view_model;
	if (!viewent->renderer.model)
		return;

	VectorCopy (Transform_GetWorldPosition (viewent->transform), r_entorigin);

	VectorNegate (vup, lighting.lightvec);

	minlight = max (viewent->renderer.min_light,
					viewent->renderer.model->min_light);

	j = max (R_LightPoint (&r_refdef.worldmodel->brush,
						   r_entorigin), minlight * 128);

	lighting.ambientlight = j;
	lighting.shadelight = j;

	// add dynamic lights
	for (lnum = 0; lnum < r_maxdlights; lnum++) {
		dl = &r_dlights[lnum];
		if (!dl->radius)
			continue;
		if (!dl->radius)
			continue;
		if (dl->die < vr_data.realtime)
			continue;

		VectorSubtract (r_entorigin, dl->origin, dist);
		add = dl->radius - VectorLength (dist);
		if (add > 0)
			lighting.ambientlight += add;
	}

	// clamp lighting so it doesn't overbright as much
	if (lighting.ambientlight > 128)
		lighting.ambientlight = 128;
	if (lighting.ambientlight + lighting.shadelight > 192)
		lighting.shadelight = 192 - lighting.ambientlight;

	R_AliasDrawModel (viewent, &lighting);
}

static int
R_BmodelCheckBBox (entity_t *ent, model_t *clmodel, float *minmaxs)
{
	int         i, *pindex, clipflags;
	vec3_t      acceptpt, rejectpt;
	double      d;
	mat4f_t     mat;

	clipflags = 0;

	Transform_GetWorldMatrix (ent->transform, mat);
	if (mat[0][0] != 1 || mat[1][1] != 1 || mat[2][2] != 1) {
		for (i = 0; i < 4; i++) {
			d = DotProduct (mat[3], view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d <= -clmodel->radius)
				return BMODEL_FULLY_CLIPPED;

			if (d <= clmodel->radius)
				clipflags |= (1 << i);
		}
	} else {
		for (i = 0; i < 4; i++) {
			// generate accept and reject points
			// FIXME: do with fast look-ups or integer tests based on the
			// sign bit of the floating point values

			pindex = pfrustum_indexes[i];

			rejectpt[0] = minmaxs[pindex[0]];
			rejectpt[1] = minmaxs[pindex[1]];
			rejectpt[2] = minmaxs[pindex[2]];

			d = DotProduct (rejectpt, view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d <= 0)
				return BMODEL_FULLY_CLIPPED;

			acceptpt[0] = minmaxs[pindex[3 + 0]];
			acceptpt[1] = minmaxs[pindex[3 + 1]];
			acceptpt[2] = minmaxs[pindex[3 + 2]];

			d = DotProduct (acceptpt, view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d <= 0)
				clipflags |= (1 << i);
		}
	}

	return clipflags;
}

static void
R_DrawBrushEntitiesOnList (entqueue_t *queue)
{
	int         j, clipflags;
	unsigned int k;
	vec3_t      origin;
	model_t    *clmodel;
	float       minmaxs[6];

	if (!r_drawentities)
		return;

	insubmodel = true;

	for (size_t i = 0; i < queue->ent_queues[mod_brush].size; i++) {
		entity_t   *ent = queue->ent_queues[mod_brush].a[i];

		VectorCopy (Transform_GetWorldPosition (ent->transform), origin);
		clmodel = ent->renderer.model;

		// see if the bounding box lets us trivially reject, also
		// sets trivial accept status
		for (j = 0; j < 3; j++) {
			minmaxs[j] = origin[j] + clmodel->mins[j];
			minmaxs[3 + j] = origin[j] + clmodel->maxs[j];
		}

		clipflags = R_BmodelCheckBBox (ent, clmodel, minmaxs);

		if (clipflags != BMODEL_FULLY_CLIPPED) {
			mod_brush_t *brush = &clmodel->brush;
			VectorCopy (origin, r_entorigin);
			VectorSubtract (r_refdef.frame.position, r_entorigin, modelorg);

			r_pcurrentvertbase = brush->vertexes;

			// FIXME: stop transforming twice
			R_RotateBmodel (ent->transform);

			// calculate dynamic lighting for bmodel if it's not an
			// instanced model
			if (brush->firstmodelsurface != 0) {
				for (k = 0; k < r_maxdlights; k++) {
					if ((r_dlights[k].die < vr_data.realtime) ||
						(!r_dlights[k].radius)) {
						continue;
					}

					vec4f_t     lightorigin;
					VectorSubtract (r_dlights[k].origin, origin, lightorigin);
					lightorigin[3] = 1;
					R_RecursiveMarkLights (brush, lightorigin,
										   &r_dlights[k], k,
										   brush->hulls[0].firstclipnode);
				}
			}
			// if the driver wants polygons, deliver those.
			// Z-buffering is on at this point, so no clipping to the
			// world tree is needed, just frustum clipping
			if (r_drawpolys | r_drawculledpolys) {
				R_ZDrawSubmodelPolys (ent, clmodel);
			} else {
				int         topnode_id = ent->visibility.topnode_id;
				mod_brush_t *brush = &r_refdef.worldmodel->brush;

				if (topnode_id >= 0) {
					// not a leaf; has to be clipped to the world
					// BSP
					mnode_t    *node = brush->nodes + topnode_id;
					r_clipflags = clipflags;
					R_DrawSolidClippedSubmodelPolygons (ent, clmodel, node);
				} else {
					// falls entirely in one leaf, so we just put
					// all the edges in the edge list and let 1/z
					// sorting handle drawing order
					mleaf_t    *leaf = brush->leafs + ~topnode_id;
					R_DrawSubmodelPolygons (ent, clmodel, clipflags, leaf);
				}
			}

			// put back world rotation and frustum clipping
			// FIXME: R_RotateBmodel should just work off base_vxx
			VectorCopy (base_vfwd, vfwd);
			VectorCopy (base_vup, vup);
			VectorCopy (base_vright, vright);
			VectorCopy (base_modelorg, modelorg);
			R_TransformFrustum ();
		}
	}

	insubmodel = false;
}

static void
R_EdgeDrawing (entqueue_t *queue)
{
	edge_t      ledges[NUMSTACKEDGES +
					   ((CACHE_SIZE - 1) / sizeof (edge_t)) + 1];
	surf_t      lsurfs[NUMSTACKSURFACES +
					   ((CACHE_SIZE - 1) / sizeof (surf_t)) + 1];

	if (auxedges) {
		r_edges = auxedges;
	} else {
		r_edges = (edge_t *)
			(((intptr_t) &ledges[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
	}

	if (r_surfsonstack) {
		surfaces = (surf_t *)
			(((intptr_t) &lsurfs[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
		surf_max = &surfaces[r_cnumsurfs];
		// surface 0 doesn't really exist; it's just a dummy because index 0
		// is used to indicate no edge attached to surface
		surfaces--;
		R_SurfacePatch ();
	}

	R_BeginEdgeFrame ();

	R_RenderWorld ();

	if (r_drawculledpolys)
		R_ScanEdges ();

	// only the world can be drawn back to front with no z reads or compares,
	// just z writes, so have the driver turn z compares on now
	D_TurnZOn ();

	R_DrawBrushEntitiesOnList (queue);

	if (!(r_drawpolys | r_drawculledpolys))
		R_ScanEdges ();
}

/*
	R_RenderView

	r_refdef must be set before the first call
*/
static void
R_RenderView_ (void)
{
	if (r_norefresh)
		return;
	if (!r_refdef.worldmodel) {
		return;
	}

	R_SetupFrame ();

// make FDIV fast. This reduces timing precision after we've been running for a
// while, so we don't do it globally.  This also sets chop mode, and we do it
// here so that setup stuff like the refresh area calculations match what's
// done in screen.c
	R_LowFPPrecision ();

	R_EdgeDrawing (r_ent_queue);

	if (vr_data.view_model) {
		R_DrawViewModel ();
	}

	if (r_aliasstats)
		R_PrintAliasStats ();

	// back to high floating-point precision
	R_HighFPPrecision ();
}

void
R_RenderView (void)
{
	int         dummy;
	int         delta;

	delta = (byte *) & dummy - r_stack_start;
	if (delta < -10000 || delta > 10000)
		Sys_Error ("R_RenderView: called without enough stack");

	if (Hunk_LowMark (0) & 3)
		Sys_Error ("Hunk is missaligned");

	if ((intptr_t) (&dummy) & 3)
		Sys_Error ("Stack is missaligned");

	if ((intptr_t) (&r_colormap) & 3)
		Sys_Error ("Globals are missaligned");

	R_RenderView_ ();
}

void
R_InitTurb (void)
{
	int         i;

	for (i = 0; i < (SIN_BUFFER_SIZE); i++) {
		sintable[i] = AMP + sin (i * 3.14159 * 2 / CYCLE) * AMP;
		intsintable[i] = AMP2 + sin (i * 3.14159 * 2 / CYCLE) * AMP2;
		// AMP2 not 20
	}
}

void
R_ClearState (void)
{
	r_refdef.worldmodel = 0;
	R_ClearEfrags ();
	R_ClearDlights ();
	R_ClearParticles ();
}
