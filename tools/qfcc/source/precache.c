/*  Copyright (C) 1996-1997  Id Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
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

#include <QF/crc.h>
#include <QF/dstring.h>

#include "cmdlib.h"
#include "qfcc.h"
#include "expr.h"
#include "options.h"
#include "type.h"

#define	MAX_SOUNDS		1024
#define	MAX_MODELS		1024
#define	MAX_FILES		1024
#define	MAX_DATA_PATH	64

static char precache_sounds[MAX_SOUNDS][MAX_DATA_PATH];
static int  precache_sounds_block[MAX_SOUNDS];
static int  numsounds;

static char precache_models[MAX_MODELS][MAX_DATA_PATH];
static int  precache_models_block[MAX_SOUNDS];
static int  nummodels;

static char precache_files[MAX_FILES][MAX_DATA_PATH];
static int  precache_files_block[MAX_SOUNDS];
static int  numfiles;

void
PrecacheSound (def_t *e, int ch)
{
	char       *n;
	int         i;

	if (!e->ofs)
		return;

	n = G_STRING (e->ofs);

	for (i = 0; i < numsounds; i++) {
		if (!strcmp (n, precache_sounds[i])) {
			return;
		}
	}

	if (numsounds == MAX_SOUNDS) {
		error (0, "PrecacheSound: numsounds == MAX_SOUNDS");
		return;
	}

	strcpy (precache_sounds[i], n);
	if (ch >= '1' && ch <= '9')
		precache_sounds_block[i] = ch - '0';
	else
		precache_sounds_block[i] = 1;

	numsounds++;
}

void
PrecacheModel (def_t *e, int ch)
{
	char       *n;
	int         i;

	if (!e->ofs)
		return;

	n = G_STRING (e->ofs);

	for (i = 0; i < nummodels; i++) {
		if (!strcmp (n, precache_models[i])) {
			return;
		}
	}

	if (nummodels == MAX_MODELS) {
		error (0, "PrecacheModels: nummodels == MAX_MODELS");
		return;
	}

	strcpy (precache_models[i], n);
	if (ch >= '1' && ch <= '9')
		precache_models_block[i] = ch - '0';
	else
		precache_models_block[i] = 1;

	nummodels++;
}

void
PrecacheFile (def_t *e, int ch)
{
	char       *n;
	int         i;

	if (!e->ofs)
		return;

	n = G_STRING (e->ofs);

	for (i = 0; i < numfiles; i++) {
		if (!strcmp (n, precache_files[i])) {
			return;
		}
	}

	if (numfiles == MAX_FILES) {
		error (0, "PrecacheFile: numfiles == MAX_FILES");
		return;
	}

	strcpy (precache_files[i], n);
	if (ch >= '1' && ch <= '9')
		precache_files_block[i] = ch - '0';
	else
		precache_files_block[i] = 1;

	numfiles++;
}

/*
	WriteFiles

	Generates files.dat, which contains all of the data files actually used by
	the game, to be processed by qfiles
*/
void
WriteFiles (const char *sourcedir)
{
	FILE       *f;
	int         i;
	dstring_t  *filename = dstring_newstr ();

	dsprintf (filename, "%s%cfiles.dat", sourcedir, PATH_SEPARATOR);
	f = fopen (filename->str, "w");
	if (!f)
		Error ("Couldn't open %s", filename->str);

	fprintf (f, "%i\n", numsounds);
	for (i = 0; i < numsounds; i++)
		fprintf (f, "%i %s\n", precache_sounds_block[i], precache_sounds[i]);

	fprintf (f, "%i\n", nummodels);
	for (i = 0; i < nummodels; i++)
		fprintf (f, "%i %s\n", precache_models_block[i], precache_models[i]);

	fprintf (f, "%i\n", numfiles);
	for (i = 0; i < numfiles; i++)
		fprintf (f, "%i %s\n", precache_files_block[i], precache_files[i]);

	fclose (f);
	dstring_delete (filename);
}

/*
	PR_WriteProgdefs

	Writes the global and entity structures out.
	Returns a crc of the header, to be stored in the progs file for comparison
	at load time.
*/
int
WriteProgdefs (char *filename)
{
	def_t      *d;
	FILE       *f;
	unsigned short crc;
	int         c;

	if (options.verbosity >= 1)
		printf ("writing %s\n", filename);
	f = fopen (filename, "w");

	// print global vars until the first field is defined
	fprintf (f,
			 "\n/* file generated by qcc, do not modify */\n\ntypedef struct\n{\tint\tpad[%i];\n",
			 RESERVED_OFS);

	for (d = pr.def_head.def_next; d; d = d->def_next) {
		if (!strcmp (d->name, "end_sys_globals"))
			break;

		switch (d->type->type) {
			case ev_float:
				fprintf (f, "\tfloat\t%s;\n", d->name);
				break;
			case ev_vector:
				fprintf (f, "\tvec3_t\t%s;\n", d->name);
				d = d->def_next->def_next->def_next;	// skip the elements
				break;
			case ev_string:
				fprintf (f, "\tstring_t\t%s;\n", d->name);
				break;
			case ev_func:
				fprintf (f, "\tfunc_t\t%s;\n", d->name);
				break;
			case ev_entity:
				fprintf (f, "\tint\t%s;\n", d->name);
				break;
			default:
				fprintf (f, "\tint\t%s;\n", d->name);
				break;
		}
	}
	fprintf (f, "} globalvars_t;\n\n");

	// print all fields
	fprintf (f, "typedef struct\n{\n");
	for (d = pr.def_head.def_next; d; d = d->def_next) {
		if (!strcmp (d->name, "end_sys_fields"))
			break;

		if (d->type->type != ev_field)
			continue;

		switch (d->type->aux_type->type) {
			case ev_float:
				fprintf (f, "\tfloat\t%s;\n", d->name);
				break;
			case ev_vector:
				fprintf (f, "\tvec3_t\t%s;\n", d->name);
				d = d->def_next->def_next->def_next;	// skip the elements
				break;
			case ev_string:
				fprintf (f, "\tstring_t\t%s;\n", d->name);
				break;
			case ev_func:
				fprintf (f, "\tfunc_t\t%s;\n", d->name);
				break;
			case ev_entity:
				fprintf (f, "\tint\t%s;\n", d->name);
				break;
			default:
				fprintf (f, "\tint\t%s;\n", d->name);
				break;
		}
	}
	fprintf (f, "} entvars_t;\n\n");

	fclose (f);

	// do a crc of the file
	CRC_Init (&crc);
	f = fopen (filename, "r+");
	while ((c = fgetc (f)) != EOF)
		CRC_ProcessByte (&crc, (byte) c);

	fprintf (f, "#define PROGHEADER_CRC %i\n", crc);
	fclose (f);

	return crc;
}
