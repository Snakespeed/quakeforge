/*
	pr_edict.c

	entity dictionary

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

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/crc.h"
#include "QF/cvar.h"
#include "QF/hash.h"
#include "QF/progs.h"
#include "QF/qdefs.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sys.h"	//XXX
#include "QF/zone.h"
#include "QF/va.h"

cvar_t     *pr_boundscheck;

int         type_size[8] = {
	1,
	sizeof (string_t) / 4,
	1,
	3,
	1,
	1,
	sizeof (func_t) / 4,
	sizeof (void *) / 4
};

char       *type_name[8] = {
	"void",
	"string",
	"float",
	"vector",
	"entity",
	"field",
	"function",
	"pointer",
};

ddef_t     *ED_FieldAtOfs (progs_t * pr, int ofs);
qboolean    ED_ParseEpair (progs_t * pr, pr_type_t *base, ddef_t *key, char *s);

#define	MAX_FIELD_LEN	64
#define GEFV_CACHESIZE	2

typedef struct {
	ddef_t     *pcache;
	char        field[MAX_FIELD_LEN];
} gefv_cache;

static gefv_cache gefvCache[GEFV_CACHESIZE] = { {NULL, ""}, {NULL, ""} };

/*
	ED_ClearEdict

	Sets everything to NULL
*/
void
ED_ClearEdict (progs_t * pr, edict_t *e)
{
	if (NUM_FOR_EDICT(pr,e)<*pr->reserved_edicts)
		printf("clearing reserved edict %d\n", NUM_FOR_EDICT(pr,e));
	memset (&e->v, 0, pr->progs->entityfields * 4);
	e->free = false;
}

/*
	ED_Alloc

	Either finds a free edict, or allocates a new one.
	Try to avoid reusing an entity that was recently freed, because it
	can cause the client to think the entity morphed into something else
	instead of being removed and recreated, which can cause interpolated
	angles and bad trails.
*/
edict_t    *
ED_Alloc (progs_t * pr)
{
	int         i;
	edict_t    *e;
	int         start = pr->reserved_edicts ? *pr->reserved_edicts : 0;
	int         max_edicts = pr->pr_edictareasize / pr->pr_edict_size;

	for (i = start + 1; i < *(pr)->num_edicts; i++) {
		e = EDICT_NUM (pr, i);
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if (e->free && (e->freetime < 2 || *(pr)->time - e->freetime > 0.5)) {
			ED_ClearEdict (pr, e);
			return e;
		}
	}

	if (i == max_edicts) {
		Con_Printf ("WARNING: ED_Alloc: no free edicts\n");
		i--;							// step on whatever is the last edict
		e = EDICT_NUM (pr, i);
		if (pr->unlink)
			pr->unlink (e);
	} else
		(*(pr)->num_edicts)++;
	e = EDICT_NUM (pr, i);
	ED_ClearEdict (pr, e);

	return e;
}

/*
	ED_Free

	Marks the edict as free
	FIXME: walk all entities and NULL out references to this entity
*/
void
ED_Free (progs_t * pr, edict_t *ed)
{
	if (pr->unlink)
		pr->unlink (ed);				// unlink from world bsp

	ED_ClearEdict (pr, ed);
	ed->free = true;
	ed->freetime = *(pr)->time;
}

//===========================================================================

/*
	ED_GlobalAtOfs
*/
ddef_t     *
ED_GlobalAtOfs (progs_t * pr, int ofs)
{
	ddef_t     *def;
	int         i;

	for (i = 0; i < pr->progs->numglobaldefs; i++) {
		def = &pr->pr_globaldefs[i];
		if (def->ofs == ofs)
			return def;
	}
	return NULL;
}

/*
	ED_FieldAtOfs
*/
ddef_t     *
ED_FieldAtOfs (progs_t * pr, int ofs)
{
	ddef_t     *def;
	int         i;

	for (i = 0; i < pr->progs->numfielddefs; i++) {
		def = &pr->pr_fielddefs[i];
		if (def->ofs == ofs)
			return def;
	}
	return NULL;
}

/*
	ED_FindField
*/
ddef_t     *
ED_FindField (progs_t * pr, const char *name)
{
	return  Hash_Find (pr->field_hash, name);
}

int
ED_GetFieldIndex (progs_t *pr, char *name)
{
	ddef_t     *def;

	def = ED_FindField (pr, name);
	if (def)
		return def->ofs;
	return -1;
}

/*
	PR_FindGlobal
*/
ddef_t     *
PR_FindGlobal (progs_t * pr, const char *name)
{
	return  Hash_Find (pr->global_hash, name);
}

eval_t *
PR_GetGlobalPointer (progs_t *pr, const char *name)
{
	ddef_t     *def;

	def = PR_FindGlobal (pr, name);
	if (def)
		return (eval_t*)&pr->pr_globals[def->ofs];
	PR_Error (pr, "undefined global %s", name);
	return 0;
}

func_t
PR_GetFunctionIndex (progs_t *pr, const char *name)
{
	dfunction_t *func = ED_FindFunction (pr, name);
	if (func)
		return func - pr->pr_functions;
	PR_Error (pr, "undefined function %s", name);
	return -1;
}

int
PR_GetFieldOffset (progs_t *pr, const char *name)
{
	ddef_t *def = ED_FindField (pr, name);
	if (def)
		return def->ofs;
	PR_Error (pr, "undefined field %s", name);
	return -1;
}

/*
	ED_FindFunction
*/
dfunction_t *
ED_FindFunction (progs_t * pr, const char *name)
{
	return  Hash_Find (pr->function_hash, name);
}

eval_t     *
GetEdictFieldValue (progs_t * pr, edict_t *ed, char *field)
{
	ddef_t     *def = NULL;
	int         i;
	static int  rep = 0;

	for (i = 0; i < GEFV_CACHESIZE; i++) {
		if (!strcmp (field, gefvCache[i].field)) {
			def = gefvCache[i].pcache;
			goto Done;
		}
	}

	def = ED_FindField (pr, field);

	if (strlen (field) < MAX_FIELD_LEN) {
		gefvCache[rep].pcache = def;
		strcpy (gefvCache[rep].field, field);
		rep ^= 1;
	}

  Done:
	if (!def)
		return NULL;

	return (eval_t *) ((char *) &ed->v + def->ofs * 4);
}

/*
	PR_ValueString

	Returns a string describing *data in a type specific manner
*/
char       *
PR_ValueString (progs_t * pr, etype_t type, pr_type_t *val)
{
	static char line[256];
	ddef_t     *def;
	dfunction_t *f;

	type &= ~DEF_SAVEGLOBAL;

	switch (type) {
		case ev_string:
			snprintf (line, sizeof (line), "%s",
					  PR_GetString (pr, val->string_var));
			break;
		case ev_entity:
			snprintf (line, sizeof (line), "entity %i",
					  NUM_FOR_EDICT (pr, PROG_TO_EDICT (pr, val->entity_var)));
			break;
		case ev_func:
			f = pr->pr_functions + val->func_var;
			snprintf (line, sizeof (line), "%s()",
					  PR_GetString (pr, f->s_name));
			break;
		case ev_field:
			def = ED_FieldAtOfs (pr, val->int_var);
			snprintf (line, sizeof (line), ".%s",
					  PR_GetString (pr, def->s_name));
			break;
		case ev_void:
			strcpy (line, "void");
			break;
		case ev_float:
			snprintf (line, sizeof (line), "%5.1f", val->float_var);
			break;
		case ev_vector:
			snprintf (line, sizeof (line), "'%5.1f %5.1f %5.1f'",
					  val->vector_var[0], val->vector_var[1], val->vector_var[2]);
			break;
		case ev_pointer:
			strcpy (line, "pointer");
			break;
		default:
			snprintf (line, sizeof (line), "bad type %i", type);
			break;
	}

	return line;
}

/*
	PR_UglyValueString

	Returns a string describing *data in a type specific manner
	Easier to parse than PR_ValueString
*/
char       *
PR_UglyValueString (progs_t * pr, etype_t type, eval_t *val)
{
	static char line[256];
	ddef_t     *def;
	dfunction_t *f;

	type &= ~DEF_SAVEGLOBAL;

	switch (type) {
		case ev_string:
			snprintf (line, sizeof (line), "%s",
					  PR_GetString (pr, val->string));
			break;
		case ev_entity:
			snprintf (line, sizeof (line), "%i",
					  NUM_FOR_EDICT (pr, PROG_TO_EDICT (pr, val->edict)));
			break;
		case ev_func:
			f = pr->pr_functions + val->function;
			snprintf (line, sizeof (line), "%s", PR_GetString (pr, f->s_name));
			break;
		case ev_field:
			def = ED_FieldAtOfs (pr, val->_int);
			snprintf (line, sizeof (line), "%s",
					  PR_GetString (pr, def->s_name));
			break;
		case ev_void:
			strcpy (line, "void");
			break;
		case ev_float:
			snprintf (line, sizeof (line), "%f", val->_float);
			break;
		case ev_vector:
			snprintf (line, sizeof (line), "%f %f %f", val->vector[0],
					  val->vector[1], val->vector[2]);
			break;
		default:
			snprintf (line, sizeof (line), "bad type %i", type);
			break;
	}

	return line;
}

/*
	PR_GlobalString

	Returns a string with a description and the contents of a global,
	padded to 20 field width
*/
char       *
PR_GlobalString (progs_t * pr, int ofs)
{
	char       *s;
	int         i;
	ddef_t     *def;
	void       *val;
	static char line[128];

	val = (void *) &pr->pr_globals[ofs];
	def = ED_GlobalAtOfs (pr, ofs);
	if (!def)
		snprintf (line, sizeof (line), "%i(?)", ofs);
	else {
		s = PR_ValueString (pr, def->type, val);
		snprintf (line, sizeof (line), "%i(%s)%s", ofs,
				  PR_GetString (pr, def->s_name), s);
	}

	i = strlen (line);
	for (; i < 20; i++)
		strncat (line, " ", sizeof (line) - strlen (line));
	strncat (line, " ", sizeof (line) - strlen (line));

	return line;
}

char       *
PR_GlobalStringNoContents (progs_t * pr, int ofs)
{
	int         i;
	ddef_t     *def;
	static char line[128];

	def = ED_GlobalAtOfs (pr, ofs);
	if (!def)
		snprintf (line, sizeof (line), "%i(?)", ofs);
	else
		snprintf (line, sizeof (line), "%i(%s)", ofs,
				  PR_GetString (pr, def->s_name));

	i = strlen (line);
	for (; i < 20; i++)
		strncat (line, " ", sizeof (line) - strlen (line));
	strncat (line, " ", sizeof (line) - strlen (line));

	return line;
}


/*
	ED_Print

	For debugging
*/
void
ED_Print (progs_t * pr, edict_t *ed)
{
	int         l;
	ddef_t     *d;
	pr_type_t  *v;
	int         i, j;
	char       *name;
	int         type;

	if (ed->free) {
		Con_Printf ("FREE\n");
		return;
	}

	Con_Printf ("\nEDICT %i:\n", NUM_FOR_EDICT (pr, ed));
	for (i = 1; i < pr->progs->numfielddefs; i++) {
		d = &pr->pr_fielddefs[i];
		name = PR_GetString (pr, d->s_name);
		if (name[strlen (name) - 2] == '_')
			continue;					// skip _x, _y, _z vars

		v = ed->v + d->ofs;

		// if the value is still all 0, skip the field
		type = d->type & ~DEF_SAVEGLOBAL;

		for (j = 0; j < type_size[type]; j++)
			if (v[j].int_var)
				break;
		if (j == type_size[type])
			continue;

		Con_Printf ("%s", name);
		l = strlen (name);
		while (l++ < 15)
			Con_Printf (" ");

		Con_Printf ("%s\n", PR_ValueString (pr, d->type, v));
	}
}

/*
	ED_Write

	For savegames
*/
void
ED_Write (progs_t * pr, QFile *f, edict_t *ed)
{
	ddef_t     *d;
	int        *v;
	int         i, j;
	char       *name;
	int         type;

	Qprintf (f, "{\n");

	if (ed->free) {
		Qprintf (f, "}\n");
		return;
	}

	for (i = 1; i < pr->progs->numfielddefs; i++) {
		d = &pr->pr_fielddefs[i];
		name = PR_GetString (pr, d->s_name);
		if (name[strlen (name) - 2] == '_')
			continue;					// skip _x, _y, _z vars

		v = (int *) ((char *) &ed->v + d->ofs * 4);

		// if the value is still all 0, skip the field
		type = d->type & ~DEF_SAVEGLOBAL;
		for (j = 0; j < type_size[type]; j++)
			if (v[j])
				break;
		if (j == type_size[type])
			continue;

		Qprintf (f, "\"%s\" ", name);
		Qprintf (f, "\"%s\"\n", PR_UglyValueString (pr, d->type, (eval_t *) v));
	}

	Qprintf (f, "}\n");
}

void
ED_PrintNum (progs_t * pr, int ent)
{
	ED_Print (pr, EDICT_NUM (pr, ent));
}

/*
	ED_PrintEdicts

	For debugging, prints all the entities in the current server
*/
void
ED_PrintEdicts (progs_t * pr)
{
	int         i;

	Con_Printf ("%i entities\n", *(pr)->num_edicts);
	for (i = 0; i < *(pr)->num_edicts; i++) {
		Con_Printf ("\nEDICT %i:\n", i);
		ED_PrintNum (pr, i);
	}
}

/*
	ED_Count

	For debugging
*/
void
ED_Count (progs_t * pr)
{
	int         i;
	edict_t    *ent;
	int         active, models, solid, step;
	ddef_t     *solid_def;
	ddef_t     *model_def;

	solid_def = ED_FindField (pr, "solid");
	model_def = ED_FindField (pr, "model");
	active = models = solid = step = 0;
	for (i = 0; i < *(pr)->num_edicts; i++) {
		ent = EDICT_NUM (pr, i);
		if (ent->free)
			continue;
		active++;
		if (solid_def && ent->v[solid_def->ofs].float_var)
			solid++;
		if (model_def && ent->v[model_def->ofs].float_var)
			models++;
	}

	Con_Printf ("num_edicts:%3i\n", *(pr)->num_edicts);
	Con_Printf ("active    :%3i\n", active);
	Con_Printf ("view      :%3i\n", models);
	Con_Printf ("touch     :%3i\n", solid);

}

/*
	ARCHIVING GLOBALS

	FIXME: need to tag constants, doesn't really work
*/

/*
	ED_WriteGlobals
*/
void
ED_WriteGlobals (progs_t * pr, QFile *f)
{
	ddef_t     *def;
	int         i;
	char       *name;
	int         type;

	Qprintf (f, "{\n");
	for (i = 0; i < pr->progs->numglobaldefs; i++) {
		def = &pr->pr_globaldefs[i];
		type = def->type;
		if (!(def->type & DEF_SAVEGLOBAL))
			continue;
		type &= ~DEF_SAVEGLOBAL;

		if (type != ev_string && type != ev_float && type != ev_entity)
			continue;

		name = PR_GetString (pr, def->s_name);
		Qprintf (f, "\"%s\" ", name);
		Qprintf (f, "\"%s\"\n",
				 PR_UglyValueString (pr, type,
									 (eval_t *) &pr->pr_globals[def->ofs]));
	}
	Qprintf (f, "}\n");
}

/*
	ED_ParseGlobals
*/
void
ED_ParseGlobals (progs_t * pr, char *data)
{
	char        keyname[64];
	ddef_t     *key;

	while (1) {
		// parse key
		data = COM_Parse (data);
		if (com_token[0] == '}')
			break;
		if (!data)
			PR_Error (pr, "ED_ParseEntity: EOF without closing brace");

		strcpy (keyname, com_token);

		// parse value  
		data = COM_Parse (data);
		if (!data)
			PR_Error (pr, "ED_ParseEntity: EOF without closing brace");

		if (com_token[0] == '}')
			PR_Error (pr, "ED_ParseEntity: closing brace without data");

		key = PR_FindGlobal (pr, keyname);
		if (!key) {
			Con_Printf ("'%s' is not a global\n", keyname);
			continue;
		}

		if (!ED_ParseEpair (pr, pr->pr_globals, key, com_token))
			PR_Error (pr, "ED_ParseGlobals: parse error");
	}
}

//============================================================================


/*
	ED_NewString
*/
char       *
ED_NewString (progs_t * pr, char *string)
{
	char       *new, *new_p;
	int         i, l;

	l = strlen (string) + 1;
	new = Hunk_Alloc (l);
	new_p = new;

	for (i = 0; i < l; i++) {
		if (string[i] == '\\' && i < l - 1) {
			i++;
			if (string[i] == 'n')
				*new_p++ = '\n';
			else
				*new_p++ = '\\';
		} else
			*new_p++ = string[i];
	}

	return new;
}


/*
	ED_ParseEval

	Can parse either fields or globals
	returns false if error
*/
qboolean
ED_ParseEpair (progs_t * pr, pr_type_t *base, ddef_t *key, char *s)
{
	int         i;
	char        string[128];
	ddef_t     *def;
	char       *v, *w;
	eval_t     *d;
	dfunction_t *func;

	d = (eval_t*)&base[key->ofs];

	switch (key->type & ~DEF_SAVEGLOBAL) {
		case ev_string:
			d->string = PR_SetString (pr, ED_NewString (pr, s));
			break;

		case ev_float:
			d->_float = atof (s);
			break;

		case ev_vector:
			strcpy (string, s);
			v = string;
			w = string;
			for (i = 0; i < 3; i++) {
				while (*v && *v != ' ')
					v++;
				*v = 0;
				d->vector[i] = atof (w);
				w = v = v + 1;
			}
			break;

		case ev_entity:
			d->edict = EDICT_TO_PROG (pr, EDICT_NUM (pr, atoi (s)));
			break;

		case ev_field:
			def = ED_FindField (pr, s);
			if (!def) {
				Con_Printf ("Can't find field %s\n", s);
				return false;
			}
			d->_int = G_INT (pr, def->ofs);
			break;

		case ev_func:
			func = ED_FindFunction (pr, s);
			if (!func) {
				Con_Printf ("Can't find function %s\n", s);
				return false;
			}
			d->function = func - pr->pr_functions;
			break;

		default:
			break;
	}
	return true;
}

/*
	ED_ParseEdict

	Parses an edict out of the given string, returning the new position
	ed should be a properly initialized empty edict.
	Used for initial level load and for savegames.
*/
char       *
ED_ParseEdict (progs_t * pr, char *data, edict_t *ent)
{
	ddef_t     *key;
	qboolean    anglehack;
	qboolean    init;
	char        keyname[256];
	int         n;

	init = false;

// clear it
	if (ent != *(pr)->edicts)			// hack
		memset (&ent->v, 0, pr->progs->entityfields * 4);

// go through all the dictionary pairs
	while (1) {
		// parse key
		data = COM_Parse (data);
		if (com_token[0] == '}')
			break;
		if (!data)
			PR_Error (pr, "ED_ParseEntity: EOF without closing brace");

		// anglehack is to allow QuakeEd to write single scalar angles
		// and allow them to be turned into vectors. (FIXME...)
		if (!strcmp (com_token, "angle")) {
			strcpy (com_token, "angles");
			anglehack = true;
		} else
			anglehack = false;

		// FIXME: change light to _light to get rid of this hack
		if (!strcmp (com_token, "light"))
			strcpy (com_token, "light_lev");	// hack for single light def

		strcpy (keyname, com_token);

		// another hack to fix heynames with trailing spaces
		n = strlen (keyname);
		while (n && keyname[n - 1] == ' ') {
			keyname[n - 1] = 0;
			n--;
		}

		// parse value  
		data = COM_Parse (data);
		if (!data)
			PR_Error (pr, "ED_ParseEntity: EOF without closing brace");

		if (com_token[0] == '}')
			PR_Error (pr, "ED_ParseEntity: closing brace without data");

		init = true;

// keynames with a leading underscore are used for utility comments,
// and are immediately discarded by quake
		if (keyname[0] == '_')
			continue;

		key = ED_FindField (pr, keyname);
		if (!key) {
			if (!pr->parse_field || !pr->parse_field (pr, keyname, com_token)) {
				Con_Printf ("'%s' is not a field\n", keyname);
				continue;
			}
		} else {
			int         ret;

			if (anglehack) {
				ret = ED_ParseEpair (pr, ent->v, key, va ("0 %s 0", com_token));
			} else {
				ret = ED_ParseEpair (pr, ent->v, key, com_token);
			}
			if (!ret)
				PR_Error (pr, "ED_ParseEdict: parse error");
		}
	}

	if (!init)
		ent->free = true;

	return data;
}


/*
	ED_LoadFromFile

	The entities are directly placed in the array, rather than allocated with
	ED_Alloc, because otherwise an error loading the map would have entity
	number references out of order.

	Creates a server's entity / program execution context by
	parsing textual entity definitions out of an ent file.

	Used for both fresh maps and savegame loads.  A fresh map would also need
	to call ED_CallSpawnFunctions () to let the objects initialize themselves.
*/
void
ED_LoadFromFile (progs_t * pr, char *data)
{
	edict_t    *ent;
	int         inhibit;
	dfunction_t *func;
	eval_t     *classname;
	ddef_t     *def;

	ent = NULL;
	inhibit = 0;

	*pr->globals.time = *(pr)->time;

	// parse ents
	while (1) {
		// parse the opening brace  
		data = COM_Parse (data);
		if (!data)
			break;
		if (com_token[0] != '{')
			PR_Error (pr, "ED_LoadFromFile: found %s when expecting {", com_token);

		if (!ent)
			ent = EDICT_NUM (pr, 0);
		else
			ent = ED_Alloc (pr);
		data = ED_ParseEdict (pr, data, ent);

		// remove things from different skill levels or deathmatch
		if (pr->prune_edict && pr->prune_edict (pr, ent)) {
			ED_Free (pr, ent);
			inhibit++;
			continue;
		}
		//
		// immediately call spawn function
		//
		def = ED_FindField (pr, "classname");
		if (!def) {
			Con_Printf ("No classname for:\n");
			ED_Print (pr, ent);
			ED_Free (pr, ent);
			continue;
		}
		classname = (eval_t*)&ent->v[def->ofs];
		// look for the spawn function
		func = ED_FindFunction (pr, PR_GetString (pr, classname->string));

		if (!func) {
			Con_Printf ("No spawn function for:\n");
			ED_Print (pr, ent);
			ED_Free (pr, ent);
			continue;
		}

		*pr->globals.self = EDICT_TO_PROG (pr, ent);
		PR_ExecuteProgram (pr, func - pr->pr_functions);
		if (pr->flush)
			pr->flush ();
	}

	Con_DPrintf ("%i entities inhibited\n", inhibit);
}

static char *
function_get_key (void *f, void *_pr)
{
	progs_t *pr = (progs_t*)_pr;
	dfunction_t *func = (dfunction_t*)f;
	return PR_GetString (pr, func->s_name);
}

static char *
var_get_key (void *d, void *_pr)
{
	progs_t *pr = (progs_t*)_pr;
	ddef_t *def = (ddef_t*)d;
	return PR_GetString (pr, def->s_name);
}

/*
	PR_LoadProgs
*/
void
PR_LoadProgs (progs_t * pr, char *progsname)
{
	int         i;
	dstatement_t *st;

// flush the non-C variable lookup cache
	for (i = 0; i < GEFV_CACHESIZE; i++)
		gefvCache[i].field[0] = 0;

	pr->progs = (dprograms_t *) COM_LoadHunkFile (progsname);
	if (!pr->progs)
		return;

	Con_DPrintf ("Programs occupy %iK.\n", com_filesize / 1024);

	// store prog crc
	pr->crc = CRC_Block ((byte *) pr->progs, com_filesize);

	// byte swap the header
	for (i = 0; i < sizeof (*pr->progs) / 4; i++)
		((int *) pr->progs)[i] = LittleLong (((int *) pr->progs)[i]);

	if (pr->progs->version != PROG_VERSION)
		PR_Error (pr, "%s has wrong version number (%i should be %i)",
				  progsname, pr->progs->version, PROG_VERSION);

	pr->pr_functions =
		(dfunction_t *) ((byte *) pr->progs + pr->progs->ofs_functions);
	pr->pr_strings = (char *) pr->progs + pr->progs->ofs_strings;
	pr->pr_stringsize = LittleLong (pr->progs->numstrings);
	pr->pr_globaldefs =
		(ddef_t *) ((byte *) pr->progs + pr->progs->ofs_globaldefs);
	pr->pr_fielddefs =
		(ddef_t *) ((byte *) pr->progs + pr->progs->ofs_fielddefs);
	pr->pr_statements =
		(dstatement_t *) ((byte *) pr->progs + pr->progs->ofs_statements);

	pr->pr_globals =
		(pr_type_t *) ((byte *) pr->progs + pr->progs->ofs_globals);

	pr->pr_edict_size =

		pr->progs->entityfields * 4 + sizeof (edict_t) - sizeof (pr_type_t);

	pr->pr_edictareasize = 0;

	if (pr->function_hash) {
		Hash_FlushTable (pr->function_hash);
	} else {
		pr->function_hash = Hash_NewTable (1021, function_get_key, 0, pr);
	}
	if (pr->global_hash) {
		Hash_FlushTable (pr->global_hash);
	} else {
		pr->global_hash = Hash_NewTable (1021, var_get_key, 0, pr);
	}
	if (pr->field_hash) {
		Hash_FlushTable (pr->field_hash);
	} else {
		pr->field_hash = Hash_NewTable (1021, var_get_key, 0, pr);
	}

// byte swap the lumps
	for (i = 0; i < pr->progs->numstatements; i++) {
		pr->pr_statements[i].op = LittleShort (pr->pr_statements[i].op);
		pr->pr_statements[i].a = LittleShort (pr->pr_statements[i].a);
		pr->pr_statements[i].b = LittleShort (pr->pr_statements[i].b);
		pr->pr_statements[i].c = LittleShort (pr->pr_statements[i].c);
	}

	for (i = 0; i < pr->progs->numfunctions; i++) {
		pr->pr_functions[i].first_statement =
			LittleLong (pr->pr_functions[i].first_statement);
		pr->pr_functions[i].parm_start =
			LittleLong (pr->pr_functions[i].parm_start);
		pr->pr_functions[i].s_name = LittleLong (pr->pr_functions[i].s_name);
		pr->pr_functions[i].s_file = LittleLong (pr->pr_functions[i].s_file);
		pr->pr_functions[i].numparms =
			LittleLong (pr->pr_functions[i].numparms);
		pr->pr_functions[i].locals = LittleLong (pr->pr_functions[i].locals);
		Hash_Add (pr->function_hash, &pr->pr_functions[i]);
	}

	for (i = 0; i < pr->progs->numglobaldefs; i++) {
		pr->pr_globaldefs[i].type = LittleShort (pr->pr_globaldefs[i].type);
		pr->pr_globaldefs[i].ofs = LittleShort (pr->pr_globaldefs[i].ofs);
		pr->pr_globaldefs[i].s_name = LittleLong (pr->pr_globaldefs[i].s_name);
		Hash_Add (pr->global_hash, &pr->pr_globaldefs[i]);
	}

	for (i = 0; i < pr->progs->numfielddefs; i++) {
		pr->pr_fielddefs[i].type = LittleShort (pr->pr_fielddefs[i].type);
		if (pr->pr_fielddefs[i].type & DEF_SAVEGLOBAL)
			PR_Error (pr, "PR_LoadProgs: pr_fielddefs[i].type & DEF_SAVEGLOBAL");
		pr->pr_fielddefs[i].ofs = LittleShort (pr->pr_fielddefs[i].ofs);
		pr->pr_fielddefs[i].s_name = LittleLong (pr->pr_fielddefs[i].s_name);
		Hash_Add (pr->field_hash, &pr->pr_fielddefs[i]);
	}

	for (i = 0; i < pr->progs->numglobals; i++)
		((int *) pr->pr_globals)[i] = LittleLong (((int *) pr->pr_globals)[i]);

	if (!(pr->globals.time = (float*)PR_GetGlobalPointer (pr, "time")))
		PR_Error (pr, "%s: undefined symbol: time", progsname);
	if (!(pr->globals.self = (int*)PR_GetGlobalPointer (pr, "self")))
		PR_Error (pr, "%s: undefined symbol: self", progsname);
	if ((pr->fields.nextthink = ED_GetFieldIndex (pr, "nextthink")) == -1)
		PR_Error (pr, "%s: undefined field: nextthink", progsname);
	if ((pr->fields.frame = ED_GetFieldIndex (pr, "frame")) == -1)
		PR_Error (pr, "%s: undefined field: frame", progsname);
	if ((pr->fields.think = ED_GetFieldIndex (pr, "think")) == -1)
		PR_Error (pr, "%s: undefined field: think", progsname);

	// initialise the strings managment code
	PR_LoadStrings (pr);

	// LordHavoc: bounds check anything static
	for (i = 0, st = pr->pr_statements; i < pr->progs->numstatements; i++, st++) {
		switch (st->op) {
			case OP_IF:
			case OP_IFNOT:
				if ((unsigned short) st->a >= pr->progs->numglobals
					|| st->b + i < 0 || st->b + i >= pr->progs->numstatements)
					PR_Error
						(pr, "PR_LoadProgs: out of bounds IF/IFNOT (statement %d)\n",
						 i);
				break;
			case OP_GOTO:
				if (st->a + i < 0 || st->a + i >= pr->progs->numstatements)
					PR_Error
						(pr, "PR_LoadProgs: out of bounds GOTO (statement %d)\n",
						 i);
				break;
				// global global global
			case OP_ADD_F:
			case OP_ADD_V:
			case OP_SUB_F:
			case OP_SUB_V:
			case OP_MUL_F:
			case OP_MUL_V:
			case OP_MUL_FV:
			case OP_MUL_VF:
			case OP_DIV_F:
			case OP_BITAND:
			case OP_BITOR:
			case OP_GE:
			case OP_LE:
			case OP_GT:
			case OP_LT:
			case OP_AND:
			case OP_OR:
			case OP_EQ_F:
			case OP_EQ_V:
			case OP_EQ_S:
			case OP_EQ_E:
			case OP_EQ_FNC:
			case OP_NE_F:
			case OP_NE_V:
			case OP_NE_S:
			case OP_NE_E:
			case OP_NE_FNC:
			case OP_ADDRESS:
			case OP_LOAD_F:
			case OP_LOAD_FLD:
			case OP_LOAD_ENT:
			case OP_LOAD_S:
			case OP_LOAD_FNC:
			case OP_LOAD_V:
				if ((unsigned short) st->a >= pr->progs->numglobals
					|| (unsigned short) st->b >= pr->progs->numglobals
					|| (unsigned short) st->c >= pr->progs->numglobals)
					PR_Error
						(pr, "PR_LoadProgs: out of bounds global index (statement %d)\n",
						 i);
				break;
				// global none global
			case OP_NOT_F:
			case OP_NOT_V:
			case OP_NOT_S:
			case OP_NOT_FNC:
			case OP_NOT_ENT:
				if ((unsigned short) st->a >= pr->progs->numglobals
					|| (unsigned short) st->c >= pr->progs->numglobals)
					PR_Error
						(pr, "PR_LoadProgs: out of bounds global index (statement %d)\n",
						 i);
				break;
				// 2 globals
			case OP_STOREP_F:
			case OP_STOREP_ENT:
			case OP_STOREP_FLD:
			case OP_STOREP_S:
			case OP_STOREP_FNC:
			case OP_STORE_F:
			case OP_STORE_ENT:
			case OP_STORE_FLD:
			case OP_STORE_S:
			case OP_STORE_FNC:
			case OP_STATE:
			case OP_STOREP_V:
			case OP_STORE_V:
				if ((unsigned short) st->a >= pr->progs->numglobals
					|| (unsigned short) st->b >= pr->progs->numglobals)
					PR_Error
						(pr, "PR_LoadProgs: out of bounds global index (statement %d)\n",
						 i);
				break;
				// 1 global
			case OP_CALL0:
			case OP_CALL1:
			case OP_CALL2:
			case OP_CALL3:
			case OP_CALL4:
			case OP_CALL5:
			case OP_CALL6:
			case OP_CALL7:
			case OP_CALL8:
			case OP_DONE:
			case OP_RETURN:
				if ((unsigned short) st->a >= pr->progs->numglobals)
					PR_Error
						(pr, "PR_LoadProgs: out of bounds global index (statement %d)\n",
						 i);
				break;
			default:
				PR_Error (pr, "PR_LoadProgs: unknown opcode %d at statement %d\n",
						  st->op, i);
				break;
		}
	}
}

void
PR_Init_Cvars (void)
{
	pr_boundscheck =
		Cvar_Get ("pr_boundscheck", "1", CVAR_NONE,
				  "Server progs bounds checking");
}

void
PR_Init (void)
{
}

edict_t    *
EDICT_NUM (progs_t * pr, int n)
{
	int offs = n * pr->pr_edict_size;
	if (offs < 0 || n >= pr->pr_edictareasize)
		PR_Error (pr, "EDICT_NUM: bad number %i", n);
	return PROG_TO_EDICT (pr, offs);
}

int
NUM_FOR_EDICT (progs_t * pr, edict_t *e)
{
	int         b;

	b = (byte *) e - (byte *) * (pr)->edicts;
	b = b / pr->pr_edict_size;

	if (b < 0 || b >= *(pr)->num_edicts)
		PR_Error (pr, "NUM_FOR_EDICT: bad pointer");
	return b;
}

void
PR_Error (progs_t *pr, const char *error, ...)
{
	va_list     argptr;
	char        string[1024];

	va_start (argptr, error);
	vsnprintf (string, sizeof (string), error, argptr);
	va_end (argptr);

	Sys_Error ("%s", string);
}

int
PR_AccessField (progs_t *pr, const char *name, etype_t type,
				const char *file, int line)
{
	ddef_t *def = ED_FindField (pr, name);

	if (!def)
		PR_Error (pr, "undefined field %s accessed at %s:%d", name, file, line);
	if (def->type != type)
		PR_Error (pr, "bad type access to %s as %s (should be %s) at %s:%d",
				  name, type_name[type], type_name[def->type], file, line);
	return def->ofs;
}
