/*
	sv_progs.c

	Quick QuakeC server code

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
#include "string.h"
#endif
#ifdef HAVE_STRINGS_H
#include "strings.h"
#endif

#include "cmd.h"
#include "console.h"
#include "host.h"
#include "server.h"
#include "sv_progs.h"
#include "world.h"

progs_t     sv_pr_state;
sv_globals_t sv_globals;
sv_funcs_t sv_funcs;
sv_fields_t sv_fields;

cvar_t     *r_skyname;
cvar_t     *sv_progs;
cvar_t     *nomonsters;
cvar_t     *gamecfg;
cvar_t     *scratch1;
cvar_t     *scratch2;
cvar_t     *scratch3;
cvar_t     *scratch4;
cvar_t     *savedgamecfg;
cvar_t     *saved1;
cvar_t     *saved2;
cvar_t     *saved3;
cvar_t     *saved4;



func_t      EndFrame;
func_t      SpectatorConnect;
func_t      SpectatorDisconnect;
func_t      SpectatorThink;

void
FindEdictFieldOffsets (progs_t * pr)
{
	dfunction_t *f;

	if (pr == &sv_pr_state) {
		// Zoid, find the spectator functions
		SpectatorConnect = SpectatorThink = SpectatorDisconnect = 0;

		if ((f = ED_FindFunction (&sv_pr_state, "SpectatorConnect")) != NULL)
			SpectatorConnect = (func_t) (f - sv_pr_state.pr_functions);
		if ((f = ED_FindFunction (&sv_pr_state, "SpectatorThink")) != NULL)
			SpectatorThink = (func_t) (f - sv_pr_state.pr_functions);
		if ((f = ED_FindFunction (&sv_pr_state, "SpectatorDisconnect")) != NULL)
			SpectatorDisconnect = (func_t) (f - sv_pr_state.pr_functions);

		// 2000-01-02 EndFrame function by Maddes/FrikaC
		EndFrame = 0;
		if ((f = ED_FindFunction (&sv_pr_state, "EndFrame")) != NULL)
			EndFrame = (func_t) (f - sv_pr_state.pr_functions);
	}
}

int
ED_Prune_Edict (progs_t *pr, edict_t *ent)
{
	// remove things from different skill levels or deathmatch
	if (deathmatch->int_val) {
		if (((int) SVFIELD (ent, spawnflags, float) & SPAWNFLAG_NOT_DEATHMATCH)) {
			return 1;
		}
	} else if (
			   (current_skill == 0
				&& ((int) SVFIELD (ent, spawnflags, float) & SPAWNFLAG_NOT_EASY))
			   || (current_skill == 1
				   && ((int) SVFIELD (ent, spawnflags, float) & SPAWNFLAG_NOT_MEDIUM))
			   || (current_skill >= 2
				   && ((int) SVFIELD (ent, spawnflags, float) & SPAWNFLAG_NOT_HARD))) {
		return 1;
	}
	return 0;
}

void
ED_PrintEdicts_f (void)
{
	ED_PrintEdicts (&sv_pr_state);
}

/*
	ED_PrintEdict_f

	For debugging, prints a single edicy
*/
void
ED_PrintEdict_f (void)
{
	int         i;

	i = atoi (Cmd_Argv (1));
	Con_Printf ("\n EDICT %i:\n", i);
	ED_PrintNum (&sv_pr_state, i);
}

void
ED_Count_f (void)
{
	ED_Count (&sv_pr_state);
}

void
PR_Profile_f (void)
{
	PR_Profile (&sv_pr_state);
}

int
ED_Parse_Extra_Fields (progs_t * pr, char *key, char *value)
{
	return 0;
}

void
SV_LoadProgs (void)
{
	PR_LoadProgs (&sv_pr_state, sv_progs->string);
	if (!sv_pr_state.progs)
		Host_Error ("SV_LoadProgs: couldn't load %s", sv_progs->string);
}

void
SV_Progs_Init (void)
{
	sv_pr_state.edicts = &sv.edicts;
	sv_pr_state.num_edicts = &sv.num_edicts;
	sv_pr_state.time = &sv.time;
	sv_pr_state.reserved_edicts = &svs.maxclients;
	sv_pr_state.unlink = SV_UnlinkEdict;

	Cmd_AddCommand ("edict", ED_PrintEdict_f,
					"Report information on a given edict in the game. (edict (edict number))");
	Cmd_AddCommand ("edicts", ED_PrintEdicts_f,
					"Display information on all edicts in the game.");
	Cmd_AddCommand ("edictcount", ED_Count_f,
					"Display summary information on the edicts in the game.");
	Cmd_AddCommand ("profile", PR_Profile_f,
					"FIXME: Report information about QuakeC Stuff (???) No Description");
}

void
SV_Progs_Init_Cvars (void)
{
	r_skyname = Cvar_Get ("r_skyname", "", CVAR_SERVERINFO, "name of skybox");
	sv_progs = Cvar_Get ("sv_progs", "progs.dat", CVAR_ROM,
						 "Allows selectable game progs if you have several "
						 "of them in the gamedir");
	nomonsters = Cvar_Get ("nomonsters", "0", CVAR_NONE, "No Description");
	gamecfg = Cvar_Get ("gamecfg", "0", CVAR_NONE, "No Description");
	scratch1 = Cvar_Get ("scratch1", "0", CVAR_NONE, "No Description");
	scratch2 = Cvar_Get ("scratch2", "0", CVAR_NONE, "No Description");
	scratch3 = Cvar_Get ("scratch3", "0", CVAR_NONE, "No Description");
	scratch4 = Cvar_Get ("scratch4", "0", CVAR_NONE, "No Description");
	savedgamecfg = Cvar_Get ("savedgamecfg", "0", CVAR_ARCHIVE, "No Description");
	saved1 = Cvar_Get ("saved1", "0", CVAR_ARCHIVE, "No Description");
	saved2 = Cvar_Get ("saved2", "0", CVAR_ARCHIVE, "No Description");
	saved3 = Cvar_Get ("saved3", "0", CVAR_ARCHIVE, "No Description");
	saved4 = Cvar_Get ("saved4", "0", CVAR_ARCHIVE, "No Description");
}
