
/*
	cl_main.c

	@description@

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

#include "QF/msg.h"
#include "QF/cvar.h"
#include "client.h"
#include "chase.h"
#include "input.h"
#include "host.h"
#include "QF/va.h"
#include "host.h"
#include "server.h"
#include "QF/console.h"
#include "screen.h"
#include "QF/cmd.h"

// we need to declare some mouse variables here, because the menu system
// references them even when on a unix system.

// these two are not intended to be set directly
cvar_t     *cl_name;
cvar_t     *cl_color;

cvar_t     *cl_shownet;
cvar_t     *cl_nolerp;

cvar_t     *cl_sbar;
cvar_t     *cl_hudswap;

cvar_t     *cl_freelook;
cvar_t     *lookspring;
cvar_t     *lookstrafe;
cvar_t     *sensitivity;

cvar_t     *m_pitch;
cvar_t     *m_yaw;
cvar_t     *m_forward;
cvar_t     *m_side;

cvar_t     *show_fps;
cvar_t     *show_time;

int         fps_count;

client_static_t cls;
client_state_t cl;

// FIXME: put these on hunk?
efrag_t     cl_efrags[MAX_EFRAGS];
entity_t    cl_entities[MAX_EDICTS];
entity_t    cl_static_entities[MAX_STATIC_ENTITIES];
lightstyle_t cl_lightstyle[MAX_LIGHTSTYLES];
dlight_t    cl_dlights[MAX_DLIGHTS];

int         cl_numvisedicts;
entity_t   *cl_visedicts[MAX_VISEDICTS];

void
CL_InitCvars (void)
{
	show_fps =
		Cvar_Get ("show_fps", "0", CVAR_NONE, 0,
				  "display realtime frames per second");
	// Misty: I like to be able to see the time when I play
	show_time = Cvar_Get ("show_time", "0", CVAR_NONE, 0,
						  "display the current time");
	cl_warncmd =
		Cvar_Get ("cl_warncmd", "0", CVAR_NONE, 0,
				  "inform when execing a command");
	cl_name = Cvar_Get ("_cl_name", "player", CVAR_ARCHIVE, 0, "Player name");
	cl_color = Cvar_Get ("_cl_color", "0", CVAR_ARCHIVE, 0, "Player color");
	cl_upspeed =
		Cvar_Get ("cl_upspeed", "200", CVAR_NONE, 0, "swim/fly up/down speed");
	cl_forwardspeed =
		Cvar_Get ("cl_forwardspeed", "200", CVAR_ARCHIVE, 0, "forward speed");
	cl_backspeed =
		Cvar_Get ("cl_backspeed", "200", CVAR_ARCHIVE, 0, "backward speed");
	cl_sidespeed = Cvar_Get ("cl_sidespeed", "350", CVAR_NONE, 0, "strafe speed");
	cl_movespeedkey =
		Cvar_Get ("cl_movespeedkey", "2.0", CVAR_NONE, 0,
				  "move `run' speed multiplier");
	cl_yawspeed = Cvar_Get ("cl_yawspeed", "140", CVAR_NONE, 0, "turning speed");
	cl_pitchspeed =
		Cvar_Get ("cl_pitchspeed", "150", CVAR_NONE, 0, "look up/down speed");
	cl_anglespeedkey =
		Cvar_Get ("cl_anglespeedkey", "1.5", CVAR_NONE, 0,
				  "turn `run' speed multiplier");
	cl_shownet =
		Cvar_Get ("cl_shownet", "0", CVAR_NONE, 0,
				  "show network packets. 0=off, 1=basic, 2=verbose");
	cl_nolerp =
		Cvar_Get ("cl_nolerp", "0", CVAR_NONE, 0, "linear motion interpolation");
	cl_sbar = Cvar_Get ("cl_sbar", "0", CVAR_ARCHIVE, 0, "status bar mode");
	cl_hudswap =
		Cvar_Get ("cl_hudswap", "0", CVAR_ARCHIVE, 0, "new HUD on left side?");
	cl_freelook = Cvar_Get ("freelook", "0", CVAR_ARCHIVE, 0, "force +mlook");
	lookspring =
		Cvar_Get ("lookspring", "0", CVAR_ARCHIVE, 0,
				  "Snap view to center when moving and no mlook/klook");
	lookstrafe =
		Cvar_Get ("lookstrafe", "0", CVAR_ARCHIVE, 0,
				  "when mlook/klook on player will strafe");
	sensitivity =
		Cvar_Get ("sensitivity", "3", CVAR_ARCHIVE, 0,
				  "mouse sensitivity multiplier");
	m_pitch =
		Cvar_Get ("m_pitch", "0.022", CVAR_ARCHIVE, 0,
				  "mouse pitch (up/down) multipier");
	m_yaw =
		Cvar_Get ("m_yaw", "0.022", CVAR_ARCHIVE, 0,
				  "mouse yaw (left/right) multipiler");
	m_forward =
		Cvar_Get ("m_forward", "1", CVAR_ARCHIVE, 0, "mouse forward/back speed");
	m_side = Cvar_Get ("m_side", "0.8", CVAR_ARCHIVE, 0, "mouse strafe speed");
}

/*
=====================
CL_ClearState

=====================
*/
void
CL_ClearState (void)
{
	int         i;

	if (!sv.active)
		Host_ClearMemory ();

// wipe the entire cl structure
	memset (&cl, 0, sizeof (cl));

	SZ_Clear (&cls.message);

// clear other arrays   
	memset (cl_efrags, 0, sizeof (cl_efrags));
	memset (cl_entities, 0, sizeof (cl_entities));
	memset (cl_dlights, 0, sizeof (cl_dlights));
	memset (cl_lightstyle, 0, sizeof (cl_lightstyle));
	memset (cl_temp_entities, 0, sizeof (cl_temp_entities));
	memset (cl_beams, 0, sizeof (cl_beams));

//
// allocate the efrags and chain together into a free list
//
	cl.free_efrags = cl_efrags;
	for (i = 0; i < MAX_EFRAGS - 1; i++)
		cl.free_efrags[i].entnext = &cl.free_efrags[i + 1];
	cl.free_efrags[i].entnext = NULL;
}

/*
=====================
CL_Disconnect

Sends a disconnect message to the server
This is also called on Host_Error, so it shouldn't cause any errors
=====================
*/
void
CL_Disconnect (void)
{
// stop sounds (especially looping!)
	S_StopAllSounds (true);

// bring the console down and fade the colors back to normal
//  SCR_BringDownConsole ();

// if running a local server, shut it down
	if (cls.demoplayback)
		CL_StopPlayback ();
	else if (cls.state == ca_connected) {
		if (cls.demorecording)
			CL_Stop_f ();

		Con_DPrintf ("Sending clc_disconnect\n");
		SZ_Clear (&cls.message);
		MSG_WriteByte (&cls.message, clc_disconnect);
		NET_SendUnreliableMessage (cls.netcon, &cls.message);
		SZ_Clear (&cls.message);
		NET_Close (cls.netcon);

		cls.state = ca_disconnected;
		if (sv.active)
			Host_ShutdownServer (false);
	}

	cls.demoplayback = cls.timedemo = false;
	cls.signon = 0;
}

void
CL_Disconnect_f (void)
{
	CL_Disconnect ();
	if (sv.active)
		Host_ShutdownServer (false);
}




/*
=====================
CL_EstablishConnection

Host should be either "local" or a net address to be passed on
=====================
*/
void
CL_EstablishConnection (char *host)
{
	if (cls.state == ca_dedicated)
		return;

	if (cls.demoplayback)
		return;

	CL_Disconnect ();

	cls.netcon = NET_Connect (host);
	if (!cls.netcon)
		Host_Error ("CL_Connect: connect failed\n");
	Con_DPrintf ("CL_EstablishConnection: connected to %s\n", host);

	cls.demonum = -1;					// not in the demo loop now
	cls.state = ca_connected;
	cls.signon = 0;						// need all the signon messages
	// before playing
}

/*
=====================
CL_SignonReply

An svc_signonnum has been received, perform a client side setup
=====================
*/
void
CL_SignonReply (void)
{
	char        str[8192];

	Con_DPrintf ("CL_SignonReply: %i\n", cls.signon);

	switch (cls.signon) {
		case 1:
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, "prespawn");
		break;

		case 2:
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, va ("name \"%s\"\n", cl_name->string));

		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message,
						 va ("color %i %i\n", (cl_color->int_val) >> 4,
							 (cl_color->int_val) & 15));

		MSG_WriteByte (&cls.message, clc_stringcmd);
		snprintf (str, sizeof (str), "spawn %s", cls.spawnparms);
		MSG_WriteString (&cls.message, str);
		break;

		case 3:
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, "begin");
		Cache_Report ();				// print remaining memory
		break;

		case 4:
		SCR_EndLoadingPlaque ();		// allow normal screen updates
		break;
	}
}

/*
=====================
CL_NextDemo

Called to play the next demo in the demo loop
=====================
*/
void
CL_NextDemo (void)
{
	char        str[1024];

	if (cls.demonum == -1)
		return;							// don't play demos

	SCR_BeginLoadingPlaque ();

	if (!cls.demos[cls.demonum][0] || cls.demonum == MAX_DEMOS) {
		cls.demonum = 0;
		if (!cls.demos[cls.demonum][0]) {
			Con_Printf ("No demos listed with startdemos\n");
			cls.demonum = -1;
			return;
		}
	}

	snprintf (str, sizeof (str), "playdemo %s\n", cls.demos[cls.demonum]);
	Cbuf_InsertText (str);
	cls.demonum++;
}

/*
==============
CL_PrintEntities_f
==============
*/
void
CL_PrintEntities_f (void)
{
	entity_t   *ent;
	int         i;

	for (i = 0, ent = cl_entities; i < cl.num_entities; i++, ent++) {
		Con_Printf ("%3i:", i);
		if (!ent->model) {
			Con_Printf ("EMPTY\n");
			continue;
		}
		Con_Printf ("%s:%2i  (%5.1f,%5.1f,%5.1f) [%5.1f %5.1f %5.1f]\n",
					ent->model->name, ent->frame, ent->origin[0],
					ent->origin[1], ent->origin[2], ent->angles[0],
					ent->angles[1], ent->angles[2]);
	}
}


/*
===============
SetPal

Debugging tool, just flashes the screen
===============
*/
void
SetPal (int i)
{
#if 0
	static int  old;
	byte        pal[768];
	int         c;

	if (i == old)
		return;
	old = i;

	if (i == 0)
		VID_SetPalette (host_basepal);
	else if (i == 1) {
		for (c = 0; c < 768; c += 3) {
			pal[c] = 0;
			pal[c + 1] = 255;
			pal[c + 2] = 0;
		}
		VID_SetPalette (pal);
	} else {
		for (c = 0; c < 768; c += 3) {
			pal[c] = 0;
			pal[c + 1] = 0;
			pal[c + 2] = 255;
		}
		VID_SetPalette (pal);
	}
#endif
}

/*
===============
CL_AllocDlight

===============
*/
dlight_t   *
CL_AllocDlight (int key)
{
	int         i;
	dlight_t   *dl;

// first look for an exact key match
	if (key) {
		dl = cl_dlights;
		for (i = 0; i < MAX_DLIGHTS; i++, dl++) {
			if (dl->key == key) {
				memset (dl, 0, sizeof (*dl));
				dl->key = key;
				dl->color = dl->_color;
				return dl;
			}
		}
	}
// then look for anything else
	dl = cl_dlights;
	for (i = 0; i < MAX_DLIGHTS; i++, dl++) {
		if (dl->die < cl.time) {
			memset (dl, 0, sizeof (*dl));
			dl->key = key;
			dl->color = dl->_color;
			return dl;
		}
	}

	dl = &cl_dlights[0];
	memset (dl, 0, sizeof (*dl));
	dl->key = key;
	dl->color = dl->_color;
	return dl;
}

/*
===============
CL_NewDlight
===============
*/
void
CL_NewDlight (int key, float x, float y, float z, float radius, float time,
			  int type)
{
	dlight_t   *dl;

	dl = CL_AllocDlight (key);
	dl->origin[0] = x;
	dl->origin[1] = y;
	dl->origin[2] = z;
	dl->radius = radius;
	dl->die = cl.time + time;
	switch (type) {
		default:
		case 0:
		dl->color[0] = 0.4;
		dl->color[1] = 0.2;
		dl->color[2] = 0.05;
		break;
		case 1:						// blue
		dl->color[0] = 0.05;
		dl->color[1] = 0.05;
		dl->color[2] = 0.5;
		break;
		case 2:						// red
		dl->color[0] = 0.5;
		dl->color[1] = 0.05;
		dl->color[2] = 0.05;
		break;
		case 3:						// purple
		dl->color[0] = 0.5;
		dl->color[1] = 0.05;
		dl->color[2] = 0.5;
		break;
	}
}

/*
===============
CL_DecayLights

===============
*/
void
CL_DecayLights (void)
{
	int         i;
	dlight_t   *dl;
	float       time;

	time = cl.time - cl.oldtime;

	dl = cl_dlights;
	for (i = 0; i < MAX_DLIGHTS; i++, dl++) {
		if (dl->die < cl.time || !dl->radius)
			continue;

		dl->radius -= time * dl->decay;
		if (dl->radius < 0)
			dl->radius = 0;
	}
}


/*
===============
CL_LerpPoint

Determines the fraction between the last two messages that the objects
should be put at.
===============
*/
float
CL_LerpPoint (void)
{
	float       f, frac;

	f = cl.mtime[0] - cl.mtime[1];

	if (!f || cl_nolerp->int_val || cls.timedemo || sv.active) {
		cl.time = cl.mtime[0];
		return 1;
	}

	if (f > 0.1) {						// dropped packet, or start of demo
		cl.mtime[1] = cl.mtime[0] - 0.1;
		f = 0.1;
	}
	frac = (cl.time - cl.mtime[1]) / f;
//Con_Printf ("frac: %f\n",frac);
	if (frac < 0) {
		if (frac < -0.01) {
			SetPal (1);
			cl.time = cl.mtime[1];
//              Con_Printf ("low frac\n");
		}
		frac = 0;
	} else if (frac > 1) {
		if (frac > 1.01) {
			SetPal (2);
			cl.time = cl.mtime[0];
//              Con_Printf ("high frac\n");
		}
		frac = 1;
	} else
		SetPal (0);

	return frac;
}


/*
===============
CL_RelinkEntities
===============
*/
void
CL_RelinkEntities (void)
{
	entity_t   *ent;
	int         i, j;
	float       frac, f, d;
	vec3_t      delta;
	float       bobjrotate;
	vec3_t      oldorg;
	dlight_t   *dl;

// determine partial update time    
	frac = CL_LerpPoint ();

	cl_numvisedicts = 0;

//
// interpolate player info
//
	for (i = 0; i < 3; i++)
		cl.velocity[i] = cl.mvelocity[1][i] +
			frac * (cl.mvelocity[0][i] - cl.mvelocity[1][i]);

	if (cls.demoplayback) {
		// interpolate the angles   
		for (j = 0; j < 3; j++) {
			d = cl.mviewangles[0][j] - cl.mviewangles[1][j];
			if (d > 180)
				d -= 360;
			else if (d < -180)
				d += 360;
			cl.viewangles[j] = cl.mviewangles[1][j] + frac * d;
		}
	}

	bobjrotate = anglemod (100 * cl.time);

// start on the entity after the world
	for (i = 1, ent = cl_entities + 1; i < cl.num_entities; i++, ent++) {
		if (!ent->model) {				// empty slot
			if (ent->forcelink)
				R_RemoveEfrags (ent);	// just became empty
			continue;
		}
// if the object wasn't included in the last packet, remove it
		if (ent->msgtime != cl.mtime[0]) {
			ent->model = NULL;
			continue;
		}

		VectorCopy (ent->origin, oldorg);

		if (ent->forcelink) {			// the entity was not updated in the
			// last message
			// so move to the final spot
			VectorCopy (ent->msg_origins[0], ent->origin);
			VectorCopy (ent->msg_angles[0], ent->angles);
		} else {						// if the delta is large, assume a
			// teleport and don't lerp
			f = frac;
			for (j = 0; j < 3; j++) {
				delta[j] = ent->msg_origins[0][j] - ent->msg_origins[1][j];
				if (delta[j] > 100 || delta[j] < -100)
					f = 1;				// assume a teleportation, not a
				// motion
			}

			// interpolate the origin and angles
			for (j = 0; j < 3; j++) {
				ent->origin[j] = ent->msg_origins[1][j] + f * delta[j];

				d = ent->msg_angles[0][j] - ent->msg_angles[1][j];
				if (d > 180)
					d -= 360;
				else if (d < -180)
					d += 360;
				ent->angles[j] = ent->msg_angles[1][j] + f * d;
			}

		}

// rotate binary objects locally
		if (ent->model->flags & EF_ROTATE)
			ent->angles[1] = bobjrotate;

		if (ent->effects & EF_BRIGHTFIELD)
			R_EntityParticles (ent);
#ifdef QUAKE2
		if (ent->effects & EF_DARKFIELD)
			R_DarkFieldParticles (ent);
#endif
		if (ent->effects & EF_MUZZLEFLASH) {
			vec3_t      fv, rv, uv;

			dl = CL_AllocDlight (i);
			VectorCopy (ent->origin, dl->origin);
			dl->origin[2] += 16;
			AngleVectors (ent->angles, fv, rv, uv);

			VectorMA (dl->origin, 18, fv, dl->origin);
			dl->radius = 200 + (rand () & 31);
			dl->minlight = 32;
			dl->die = cl.time + 0.1;
			dl->color[0] = 0.2;
			dl->color[1] = 0.1;
			dl->color[2] = 0.05;
		}
		if ((ent->effects & (EF_BLUE | EF_RED)) == (EF_BLUE | EF_RED))
			CL_NewDlight (i, ent->origin[0], ent->origin[1], ent->origin[2],
						  200 + (rand () & 31), 0.1, 3);
		if (ent->effects & EF_BLUE)
			CL_NewDlight (i, ent->origin[0], ent->origin[1], ent->origin[2],
						  200 + (rand () & 31), 0.1, 1);
		if (ent->effects & EF_RED)
			CL_NewDlight (i, ent->origin[0], ent->origin[1], ent->origin[2],
						  200 + (rand () & 31), 0.1, 2);
		if (ent->effects & EF_BRIGHTLIGHT)
			CL_NewDlight (i, ent->origin[0], ent->origin[1],
						  ent->origin[2] + 16, 400 + (rand () & 31), 0.001, 0);
		if (ent->effects & EF_DIMLIGHT)
			CL_NewDlight (i, ent->origin[0], ent->origin[1], ent->origin[2],
						  200 + (rand () & 31), 0.001, 0);
#ifdef QUAKE2
		if (ent->effects & EF_DARKLIGHT) {
			dl = CL_AllocDlight (i);
			VectorCopy (ent->origin, dl->origin);
			dl->radius = 200.0 + (rand () & 31);
			dl->die = cl.time + 0.001;
			dl->dark = true;
		}
		if (ent->effects & EF_LIGHT) {
			dl = CL_AllocDlight (i);
			VectorCopy (ent->origin, dl->origin);
			dl->radius = 200;
			dl->die = cl.time + 0.001;
		}
#endif

		if (ent->model->flags & EF_GIB)
			R_RocketTrail (oldorg, ent->origin, 2, ent);
		else if (ent->model->flags & EF_ZOMGIB)
			R_RocketTrail (oldorg, ent->origin, 4, ent);
		else if (ent->model->flags & EF_TRACER)
			R_RocketTrail (oldorg, ent->origin, 3, ent);
		else if (ent->model->flags & EF_TRACER2)
			R_RocketTrail (oldorg, ent->origin, 5, ent);
		else if (ent->model->flags & EF_ROCKET) {
			R_RocketTrail (oldorg, ent->origin, 0, ent);
			dl = CL_AllocDlight (i);
			VectorCopy (ent->origin, dl->origin);
			dl->radius = 200;
			dl->die = cl.time + 0.01;
		} else if (ent->model->flags & EF_GRENADE)
			R_RocketTrail (oldorg, ent->origin, 1, ent);
		else if (ent->model->flags & EF_TRACER3)
			R_RocketTrail (oldorg, ent->origin, 6, ent);

		ent->forcelink = false;

		if (i == cl.viewentity && !chase_active->int_val)
			continue;

#ifdef QUAKE2
		if (ent->effects & EF_NODRAW)
			continue;
#endif
		if (cl_numvisedicts < MAX_VISEDICTS) {
			cl_visedicts[cl_numvisedicts] = ent;
			cl_numvisedicts++;
		}
	}

}


/*
===============
CL_ReadFromServer

Read all incoming data from the server
===============
*/
int
CL_ReadFromServer (void)
{
	int         ret;

	cl.oldtime = cl.time;
	cl.time += host_frametime;

	do {
		ret = CL_GetMessage ();
		if (ret == -1)
			Host_Error ("CL_ReadFromServer: lost server connection");
		if (!ret)
			break;

		cl.last_received_message = realtime;
		CL_ParseServerMessage ();
	} while (ret && cls.state == ca_connected);

	if (cl_shownet->int_val)
		Con_Printf ("\n");

	CL_RelinkEntities ();
	CL_UpdateTEnts ();

//
// bring the links up to date
//
	return 0;
}

/*
=================
CL_SendCmd
=================
*/
void
CL_SendCmd (void)
{
	usercmd_t   cmd;

	if (cls.state != ca_connected)
		return;

	if (cls.signon == SIGNONS) {
		// get basic movement from keyboard
		CL_BaseMove (&cmd);

		// allow mice or other external controllers to add to the move
		IN_Move (&cmd);

		// send the unreliable message
		CL_SendMove (&cmd);

	}

	if (cls.demoplayback) {
		SZ_Clear (&cls.message);
		return;
	}
// send the reliable message
	if (!cls.message.cursize)
		return;							// no message at all

	if (!NET_CanSendMessage (cls.netcon)) {
		Con_DPrintf ("CL_WriteToServer: can't send\n");
		return;
	}

	if (NET_SendMessage (cls.netcon, &cls.message) == -1)
		Host_Error ("CL_WriteToServer: lost server connection");

	SZ_Clear (&cls.message);
}

/*
=================
CL_Init
=================
*/
void
CL_Init (void)
{
	SZ_Alloc (&cls.message, 1024);

	CL_InitInput ();
	CL_InitTEnts ();

	Cmd_AddCommand ("entities", CL_PrintEntities_f, "No Description");
	Cmd_AddCommand ("disconnect", CL_Disconnect_f, "No Description");
	Cmd_AddCommand ("record", CL_Record_f, "No Description");
	Cmd_AddCommand ("stop", CL_Stop_f, "No Description");
	Cmd_AddCommand ("playdemo", CL_PlayDemo_f, "No Description");
	Cmd_AddCommand ("timedemo", CL_TimeDemo_f, "No Description");
	Cmd_AddCommand ("maplist", COM_Maplist_f, "No Description");
}
