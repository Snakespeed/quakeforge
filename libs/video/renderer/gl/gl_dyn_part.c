/*
	gl_dyn_part.c

	OpenGL particle system.

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

	$Id$
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

#include <stdlib.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/vfs.h"
#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"

#include "compat.h"
#include "r_cvar.h"
#include "r_dynamic.h"
#include "r_shared.h"
#include "varrays.h"

static particle_t *particles, **freeparticles;
static short r_numparticles, numparticles;

int         ramp[8] = { 0x6d, 0x6b, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01 };

extern int  part_tex_dot;
extern int  part_tex_spark;
extern int  part_tex_smoke[8];
extern int  part_tex_smoke_ring[8];

extern cvar_t *cl_max_particles;


inline particle_t *
particle_new (ptype_t type, int texnum, vec3_t org, float scale, vec3_t vel,
			  float die, byte color, byte alpha)
{
	particle_t *part;

	if (numparticles >= r_numparticles) {
		// Con_Printf("FAILED PARTICLE ALLOC!\n");
		return NULL;
	}

	part = &particles[numparticles++];

	part->type = type;
	VectorCopy (org, part->org);
	VectorCopy (vel, part->vel);
	part->die = die;
	part->color = color;
	part->alpha = alpha;
	part->tex = texnum;
	part->scale = scale;

	return part;
}

inline particle_t *
particle_new_random (ptype_t type, int texnum, vec3_t org, int org_fuzz,
					 float scale, int vel_fuzz, float die, byte color,
					 byte alpha)
{
	int         j;
	vec3_t      porg, pvel;

	for (j = 0; j < 3; j++) {
		if (org_fuzz)
			porg[j] = lhrandom (-org_fuzz, org_fuzz) + org[j];
		if (vel_fuzz)
			pvel[j] = lhrandom (-vel_fuzz, vel_fuzz);
	}
	return particle_new (type, texnum, porg, scale, pvel, die, color, alpha);
}

/*
	R_MaxParticlesCheck

	Misty-chan: Dynamically change the maximum amount of particles on the fly.
	Thanks to a LOT of help from Taniwha, Deek, Mercury, Lordhavoc, and
	lots of others.
*/
void
R_MaxParticlesCheck (cvar_t *var)
{
/*
	Catchall. If the user changed the setting to a number less than zero
	*or* if we had a wacky cfg get past the init code check, this will
	make sure we don't have problems. Also note that grabbing the
	var->int_val is IMPORTANT:

	Prevents a segfault since if we grabbed the int_val of
	cl_max_particles we'd sig11 right here at startup.
*/
	r_numparticles = max(var->int_val, 0);

/*
	Be very careful the next time we do something like this.
	calloc/free are IMPORTANT and the compiler doesn't know when we
	do bad things with them.
*/
	free (particles);
	free (freeparticles);

	particles = (particle_t *)
		calloc (r_numparticles, sizeof (particle_t));
	freeparticles = (particle_t **)
		calloc (r_numparticles, sizeof (particle_t*));			

	R_ClearParticles();
}

void
R_Particles_Init_Cvars (void)
{
/*
	Misty-chan: This is a cvar that does callbacks. Whenever it
	changes, it calls the function R_MaxParticlesCheck and therefore
	is very nifty.
*/
	Cvar_Get ("cl_max_particles", "2048", CVAR_ARCHIVE, R_MaxParticlesCheck,
			  "Maximum amount of particles to display. No maximum, minimum "
			  "is 0, although it's best to use r_particles 0 instead.");
}

inline void
R_ClearParticles (void)
{
	numparticles = 0;
}

void
R_ReadPointFile_f (void)
{
	char        name[MAX_OSPATH], *mapname, *t1;
	int         c, r;
	vec3_t      org;
	VFile      *f;

	mapname = strdup (r_worldentity.model->name);
	if (!mapname)
		Sys_Error ("Can't duplicate mapname!");
	t1 = strrchr (mapname, '.');
	if (!t1)
		Sys_Error ("Can't find .!");
	t1[0] = '\0';

	snprintf (name, sizeof (name), "%s.pts", mapname);
	free (mapname);

	COM_FOpenFile (name, &f);
	if (!f) {
		Con_Printf ("couldn't open %s\n", name);
		return;
	}

	Con_Printf ("Reading %s...\n", name);
	c = 0;
	for (;;) {
		char        buf[64];

		Qgets (f, buf, sizeof (buf));
		r = sscanf (buf, "%f %f %f\n", &org[0], &org[1], &org[2]);
		if (r != 3)
			break;
		c++;

		if (!particle_new (pt_static, part_tex_dot, org, 1.5, vec3_origin,
						   99999, (-c) & 15, 255)) {
			Con_Printf ("Not enough free particles\n");
			break;
		}
	}

	Qclose (f);
	Con_Printf ("%i points read\n", c);
}

void
R_ParticleExplosion (vec3_t org)
{
	if (!r_particles->int_val)
		return;

	particle_new_random (pt_smokecloud, part_tex_smoke[rand () & 7], org, 4,
						 30, 8, r_realtime + 5, (rand () & 7) + 8,
						 128 + (rand () & 63));
}

void
R_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength)
{
	int         i;
	int         colorMod = 0;

	if (!r_particles->int_val)
		return;
	
	for (i = 0; i < 512; i++) {
		particle_new_random (pt_blob, part_tex_dot, org, 16, 2, 256,
							 (r_realtime + 0.3),
							 (colorStart + (colorMod % colorLength)), 255);
		colorMod++;
	}
}

void
R_BlobExplosion (vec3_t org)
{
	int         i;

	if (!r_particles->int_val)
		return;

	for (i = 0; i < 512; i++) {
		particle_new_random (pt_blob, part_tex_dot, org, 12, 2, 256,
							 (r_realtime + 1 + (rand () & 8) * 0.05),
							 (66 + rand () % 6), 255);
	}
	for (i = 0; i < 512; i++) {
		particle_new_random (pt_blob2, part_tex_dot, org, 12, 2, 256,
							 (r_realtime + 1 + (rand () & 8) * 0.05),
							 (150 + rand () % 6), 255);
	}
}

static void
R_RunSparkEffect (vec3_t org, int count, int ofuzz)
{
	particle_new (pt_smokecloud, part_tex_smoke[rand () & 7], org, 
				  (ofuzz / 8) * .75, vec3_origin, r_realtime + 99, 
				  12 + (rand () & 3), 96);
	while (count--)
		particle_new_random (pt_fallfadespark, part_tex_spark, org,
							 ofuzz * .75, 1, 96, r_realtime + 5,
							 ramp[rand () % 6], lhrandom (0, 255));
}

inline static void
R_RunGunshotEffect (vec3_t org, int count)
{
	int         scale;

	if (count > 6)
		scale = 24;
	else
		scale = 16;

	R_RunSparkEffect (org, count * 10, scale);
	return;
}

inline static void
R_BloodPuff (vec3_t org, int count)
{
	particle_new (pt_bloodcloud, part_tex_smoke[rand () & 7], org, 9,
				  vec3_origin, r_realtime + 99, 68 + (rand () & 3), 128);
}

void
R_RunPuffEffect (vec3_t org, particle_effect_t type, byte count)
{
	if (!r_particles->int_val)
		return;

	switch (type) {
		case PE_GUNSHOT:
			R_RunGunshotEffect (org, count);
			break;
		case PE_BLOOD:
			R_BloodPuff (org, count);
			break;
		case PE_LIGHTNINGBLOOD:
			R_BloodPuff (org, 5 + (rand () & 1));
			break;
		default:
			break;
	}
}

void
R_RunParticleEffect (vec3_t org, int color, int count)
{
	int         scale, i, j;
	vec3_t      porg;

	if (!r_particles->int_val)
		return;

	if (count > 130)
		scale = 3;
	else if (count > 20)
		scale = 2;
	else
		scale = 1;

	for (i = 0; i < count; i++) {
		for (j = 0; j < 3; j++) {
			porg[j] = org[j] + scale * ((rand () & 15) - 8);
		}
		particle_new (pt_grav, part_tex_dot, porg, 1.5, vec3_origin,
					  (r_realtime + 0.1 * (rand () % 5)),
					  (color & ~7) + (rand () & 7), 255);
	}
}

void
R_RunSpikeEffect (vec3_t org, particle_effect_t type)
{
	if (!r_particles->int_val)
		return;

	switch (type) {
		case PE_SPIKE:
			R_RunSparkEffect (org, 5, 8);
			break;
		case PE_SUPERSPIKE:
			R_RunSparkEffect (org, 10, 8);
			break;
		case PE_KNIGHTSPIKE:
			R_RunSparkEffect (org, 10, 8);
			break;
		case PE_WIZSPIKE:
			R_RunSparkEffect (org, 15, 16);
			break;
		default:
			break;
	}
}

void
R_LavaSplash (vec3_t org)
{
	float       vel;
	int         i, j;
	vec3_t      dir, porg, pvel;

	if (!r_particles->int_val)
		return;

	for (i = -128; i < 128; i+=16) {
		for (j = -128; j < 128; j+=16) {
			dir[0] = j + (rand () & 7);
			dir[1] = i + (rand () & 7);
			dir[2] = 256;

			porg[0] = org[0] + dir[0];
			porg[1] = org[1] + dir[1];
			porg[2] = org[2] + (rand () & 63);

			VectorNormalize (dir);
			vel = 50 + (rand () & 63);
			VectorScale (dir, vel, pvel);
			particle_new (pt_grav, part_tex_dot, porg, 3, pvel,
						  (r_realtime + 2 + (rand () & 31) * 0.02),
						  (224 + (rand () & 7)), 193);
		}
	}
}

void
R_TeleportSplash (vec3_t org)
{
	float       vel;
	int         i, j, k;
	vec3_t      dir, porg, pvel;

	if (!r_particles->int_val)
		return;

	for (i = -16; i < 16; i += 4)
		for (j = -16; j < 16; j += 4)
			for (k = -24; k < 32; k += 4) {
				dir[0] = j * 8;
				dir[1] = i * 8;
				dir[2] = k * 8;

				porg[0] = org[0] + i + (rand () & 3);
				porg[1] = org[1] + j + (rand () & 3);
				porg[2] = org[2] + k + (rand () & 3);

				VectorNormalize (dir);
				vel = 50 + (rand () & 63);
				VectorScale (dir, vel, pvel);
				particle_new (pt_grav, part_tex_spark, porg, 0.6, pvel,
							  (r_realtime + 0.2 + (rand () & 7) * 0.02),
							  (7 + (rand () & 7)), 255);
			}
}

void
R_RocketTrail (int type, entity_t *ent)
{
	byte        palpha, pcolor;
	float       dist, len, pdie, pscale, pscalenext;
	int         ptex, j;
	ptype_t     ptype;
	vec3_t      porg, pvel, subtract, vec;

	if (type == 0)
		R_AddFire (ent->old_origin, ent->origin, ent);

	if (!r_particles->int_val)
		return;

	VectorSubtract (ent->origin, ent->old_origin, vec);
	len = VectorNormalize (vec);
	pdie = r_realtime + 2;
	ptex = part_tex_dot;
	ptype = pt_static;
	palpha = 255;
	pcolor = 0;
	pscale = pscalenext = 3;
	dist = 3;
	switch (type) {
		case 0:					// rocket trail
			pdie = r_realtime + 0.4;
			pscale = lhrandom (1, 2);
			ptype = pt_smoke;
			break;
		case 1:					// grenade trail
			pscale = lhrandom (4, 9);
			ptype = pt_smoke;
			break;
		case 2:					// blood
			pscale = lhrandom (4, 10);
			ptype = pt_grav;
			break;
		case 4:					// slight blood
			palpha = 192;
			pdie = r_realtime + 1.3;
			pscale = lhrandom (1, 6);
			ptype = pt_grav;
			break;
		case 3:					// green tracer
			pdie = r_realtime + 0.5;
			ptype = pt_fire;
			break;
		case 5:					// flame tracer
			pdie = r_realtime + 0.5;
			ptype = pt_fire;
			break;
		case 6:					// voor trail
			pdie = r_realtime + 0.3;
			ptex = part_tex_dot;
			ptype = pt_static;
			break;
	}

	while (len > 0) {
		VectorCopy (vec3_origin, pvel);

		switch (type) {
			case 0:					// rocket trail
				pscalenext = lhrandom (1, 2);
				dist =  (pscale + pscalenext) * 4;
//				pcolor = (rand () & 255); // Misty-chan's Easter Egg
				pcolor = (rand () & 3) + 12;
				palpha = 128 + (rand () & 31);
//				VectorVectors(vec, right, up); // Mercury's Rings
				VectorCopy (ent->old_origin, porg);
//				ptype = pt_smokering; // Mercury's Rings
//				ptex = part_tex_smoke_ring[rand () & 7]; // Mercury's Rings
				ptex = part_tex_smoke[rand () & 7];
				break;
			case 1:					// grenade trail
				pscalenext = lhrandom (4, 9);
				dist = (pscale + pscalenext) * 4;
//				pcolor = (rand () & 255); // Misty-chan's Easter Egg
				pcolor = (rand () & 3);
				palpha = 128 + (rand () & 31);
				VectorCopy (ent->old_origin, porg);
				ptex = part_tex_smoke[rand () & 7];
				break;
			case 2:					// blood
				pscalenext = lhrandom (4, 10);
				dist = (pscale + pscalenext) * 4;
				ptex = part_tex_smoke[rand () & 7];
				pcolor = 68 + (rand () & 3);
				for (j = 0; j < 3; j++) {
					pvel[j] = lhrandom (-3, 3) * type;
					porg[j] = ent->old_origin[j] + lhrandom (-1.5, 1.5);
				}
				break;
			case 4:					// slight blood
				pscalenext = lhrandom (1, 6);
				dist = (pscale + pscalenext) * 4;
				ptex = part_tex_smoke[rand () & 7];
				pcolor = 68 + (rand () & 3);
				for (j = 0; j < 3; j++) {
					pvel[j] = lhrandom (-3, 3) * type;
					porg[j] = ent->old_origin[j] + lhrandom (-1.5, 1.5);
				}
				break;
			case 3:					// green tracer
			case 5:					// flame tracer
			{
				static int  tracercount;

				ptex = part_tex_smoke[rand () & 7];
				pscale = lhrandom (1, 2);
				if (type == 3)
					pcolor = 52 + ((tracercount & 4) << 1);
				else
					pcolor = 234;

				tracercount++;

				VectorCopy (ent->old_origin, porg);
				if (tracercount & 1) {
					pvel[0] = 30 * vec[1];
					pvel[1] = 30 * -vec[0];
				} else {
					pvel[0] = 30 * -vec[1];
					pvel[1] = 30 * vec[0];
				}
			}
			break;
			case 6:					// voor trail
				pcolor = 9 * 16 + 8 + (rand () & 3);
				pscale = lhrandom (.75, 1.5);
				for (j = 0; j < 3; j++)
					porg[j] = ent->old_origin[j] + lhrandom (-8, 8);
				break;
		}

		VectorScale (vec, min(dist, len), subtract);
		VectorAdd (ent->old_origin, subtract, ent->old_origin);
		len -= dist;

		particle_new (ptype, ptex, porg, pscale, pvel, pdie, pcolor, palpha);
		pscale = pscalenext;
	}
}

void
R_DrawParticles (void)
{
	byte        alpha, i;
	unsigned char *at;
	float       dvel, grav, fast_grav, minparticledist, scale;
	int         activeparticles, maxparticle, j, k;
	particle_t *part;
	vec3_t		up, right;
	vec3_t		up_scale, right_scale, up_right_scale, down_right_scale;
	
	// LordHavoc: particles should not affect zbuffer
	qfglDepthMask (GL_FALSE);

	VectorScale (vup, 1.5, up);
	VectorScale (vright, 1.5, right);

	varray[0].texcoord[0] = 0; varray[0].texcoord[1] = 1;
	varray[1].texcoord[0] = 0; varray[1].texcoord[1] = 0;
	varray[2].texcoord[0] = 1; varray[2].texcoord[1] = 0;
	varray[3].texcoord[0] = 1; varray[3].texcoord[1] = 1;

	grav = (fast_grav = r_frametime * 800) * 0.05;
	dvel = 4 * r_frametime;

	minparticledist = DotProduct (r_refdef.vieworg, vpn) + 32.0f;

	activeparticles = 0;
	maxparticle = -1;
	j = 0;

	for (k = 0, part = particles; k < numparticles; k++, part++) {
		// LordHavoc: this is probably no longer necessary, as it is
		// checked at the end, but could still happen on weird particle
		// effects, left for safety...
		if (part->die <= r_realtime) {
			freeparticles[j++] = part;
			continue;
		}
		maxparticle = k;
		activeparticles++;

		// Don't render particles too close to us.
		// Note, we must still do physics and such on them.
		if (!(DotProduct (part->org, vpn) < minparticledist)) {
			at = (byte *) & d_8to24table[(byte) part->color];
			alpha = part->alpha;

			varray[0].color[0] = (float) at[0] / 255;
			varray[0].color[1] = (float) at[1] / 255;
			varray[0].color[2] = (float) at[2] / 255;
			varray[0].color[3] = (float) alpha / 255;

			memcpy(varray[1].color, varray[0].color, sizeof(varray[0].color));
			memcpy(varray[2].color, varray[0].color, sizeof(varray[0].color));
			memcpy(varray[3].color, varray[0].color, sizeof(varray[0].color));

			scale = part->scale;

			VectorScale    (up,    scale, up_scale);
			VectorScale    (right, scale, right_scale);

			VectorAdd      (right_scale, up_scale, up_right_scale);
			VectorSubtract (right_scale, up_scale, down_right_scale);

			VectorAdd      (part->org, up_right_scale,   varray[0].vertex);
			VectorAdd      (part->org, down_right_scale, varray[1].vertex);
			VectorSubtract (part->org, up_right_scale,   varray[2].vertex);
			VectorSubtract (part->org, down_right_scale, varray[3].vertex);

			qfglBindTexture (GL_TEXTURE_2D, part->tex);
			qfglDrawArrays (GL_QUADS, 0, 4);
		}

		for (i = 0; i < 3; i++)
			part->org[i] += part->vel[i] * r_frametime;

		switch (part->type) {
			case pt_static:
				break;
			case pt_blob:
				for (i = 0; i < 3; i++)
					part->vel[i] += part->vel[i] * dvel;
				part->vel[2] -= grav;
				break;
			case pt_blob2:
				for (i = 0; i < 2; i++)
					part->vel[i] -= part->vel[i] * dvel;
				part->vel[2] -= grav;
				break;
			case pt_grav:
				part->vel[2] -= grav;
				break;
			case pt_smoke:
				if ((part->alpha -= r_frametime * 96) < 1)
					part->die = -1;
				part->scale += r_frametime * 4;
//				part->org[2] += r_frametime * 30 - grav;
				break;
			case pt_smokering:
				if ((part->alpha -= r_frametime * 128) < 1)
					part->die = -1;
				part->scale += r_frametime * 10;
//				part->org[2] += r_frametime * 30 - grav;
				break;
			case pt_smokecloud:
				if ((part->alpha -= r_frametime * 128) < 1)
				{
					part->die = -1;
					break;
				}
				part->scale += r_frametime * 60;
				part->org[2] += r_frametime * 30;
				break;
			case pt_bloodcloud:
				if ((part->alpha -= r_frametime * 64) < 1)
				{
					part->die = -1;
					break;
				}
				part->scale += r_frametime * 4;
				part->vel[2] -= grav;
				break;
			case pt_fadespark:
				if ((part->alpha -= r_frametime * 256) < 1)
					part->die = -1;
				part->vel[2] -= grav;
				break;
			case pt_fadespark2:
				if ((part->alpha -= r_frametime * 512) < 1)
					part->die = -1;
				part->vel[2] -= grav;
				break;
			case pt_fallfadespark:
				if ((part->alpha -= r_frametime * 256) < 1)
					part->die = -1;
				part->vel[2] -= fast_grav;
				break;
			case pt_fire:
				if ((part->alpha -= r_frametime * 32) < 1)
					part->die = -1;
				part->scale -= r_frametime * 2;
				break;
			default:
				Con_DPrintf ("unhandled particle type %d\n", part->type);
				break;
		}
	}
	k = 0;
	while (maxparticle >= activeparticles) {
		*freeparticles[k++] = particles[maxparticle--];
		while (maxparticle >= activeparticles && 
				particles[maxparticle].die <= r_realtime)
			maxparticle--;
	}
	numparticles = activeparticles;

	qfglColor3ubv (color_white);
	qfglDepthMask (GL_TRUE);
}
