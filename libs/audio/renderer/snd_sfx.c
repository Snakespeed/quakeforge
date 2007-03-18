/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2004 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id: snd_dma.c 11380 2007-03-10 14:17:52Z taniwha $";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/hash.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "snd_render.h"

#define MAX_SFX		512
static sfx_t    snd_sfx[MAX_SFX];
static int      snd_num_sfx;
static hashtab_t *snd_sfx_hash;

static cvar_t  *precache;

static const char *
snd_sfx_getkey (void *sfx, void *unused)
{
	return ((sfx_t *) sfx)->name;
}

static void
snd_sfx_free (void *_sfx, void *unused)
{
	sfx_t      *sfx = (sfx_t *) _sfx;
	free ((char *) sfx->name);
	sfx->name = 0;
}

void
SND_SFX_Cache (sfx_t *sfx, char *realname, wavinfo_t info,
			   cache_loader_t loader)
{
	sfxblock_t *block = calloc (1, sizeof (sfxblock_t));

	sfx->data = block;
	sfx->wavinfo = SND_CacheWavinfo;
	sfx->touch = SND_CacheTouch;
	sfx->retain = SND_CacheRetain;
	sfx->release = SND_CacheRelease;

	block->sfx = sfx;
	block->file = realname;
	block->wavinfo = info;

	Cache_Add (&block->cache, block, loader);
}

void
SND_SFX_Stream (sfx_t *sfx, char *realname, wavinfo_t info,
				sfx_t *(*open) (sfx_t *sfx))
{
	sfxstream_t *stream = calloc (1, sizeof (sfxstream_t));
	sfx->open = open;
	sfx->wavinfo = SND_CacheWavinfo;
	sfx->touch = sfx->retain = SND_StreamRetain;
	sfx->release = SND_StreamRelease;
	sfx->data = stream;

	stream->file = realname;
	stream->wavinfo = info;
}

sfx_t *
SND_SFX_StreamOpen (sfx_t *sfx, void *file,
					int (*read)(void *, byte *, int, wavinfo_t *),
					int (*seek)(void *, int, wavinfo_t *),
					void (*close) (sfx_t *))
{
	sfxstream_t *stream = (sfxstream_t *) sfx->data;
	wavinfo_t  *info = &stream->wavinfo;
	int         samples;
	int         size;

	sfx_t      *new_sfx = calloc (1, sizeof (sfx_t));

	new_sfx->name = sfx->name;
	new_sfx->wavinfo = SND_CacheWavinfo;
	new_sfx->touch = new_sfx->retain = SND_StreamRetain;
	new_sfx->release = SND_StreamRelease;
	new_sfx->close = close;

	samples = snd_shm->speed * 0.3;
	size = samples = (samples + 255) & ~255;
	if (!snd_loadas8bit->int_val)
		size *= 2;
	if (info->channels == 2)
		size *= 2;

	stream = calloc (1, sizeof (sfxstream_t) + size);
	new_sfx->data = stream;
	memcpy (stream->buffer.data + size, "\xde\xad\xbe\xef", 4);
	stream->file = file;
	stream->sfx = new_sfx;
	stream->resample = info->channels == 2 ? SND_ResampleStereo
										   : SND_ResampleMono;
	stream->read = read;
	stream->seek = seek;

	stream->buffer.length = samples;
	stream->buffer.advance = SND_StreamAdvance;
	stream->buffer.setpos = SND_StreamSetPos;
	stream->buffer.sfx = new_sfx;

	stream->resample (&stream->buffer, 0, 0, 0);	// get sfx setup properly
	stream->seek (stream->file, 0, &stream->wavinfo);

	stream->buffer.advance (&stream->buffer, 0);

	return new_sfx;
}

sfx_t *
SND_LoadSound (const char *name)
{
	sfx_t      *sfx;

	if (!snd_sfx_hash)
		return 0;
	if ((sfx = (sfx_t *) Hash_Find (snd_sfx_hash, name)))
		return sfx;

	if (snd_num_sfx == MAX_SFX)
		Sys_Error ("s_load_sound: out of sfx_t");

	sfx = &snd_sfx[snd_num_sfx++];
	sfx->name = strdup (name);
	SND_Load (sfx);
	Hash_Add (snd_sfx_hash, sfx);
	return sfx;
}

void
SND_TouchSound (const char *name)
{
	sfx_t      *sfx;

	if (!name)
		Sys_Error ("SND_TouchSound: NULL");

	sfx = SND_LoadSound (va ("sound/%s", name));
	if (sfx)
		sfx->touch (sfx);
}

sfx_t *
SND_PrecacheSound (const char *name)
{
	sfx_t      *sfx;

	if (!name)
		Sys_Error ("SND_PrecacheSound: NULL");

	sfx = SND_LoadSound (va ("sound/%s", name));
	if (sfx && precache->int_val) {
		if (sfx->retain (sfx))
			sfx->release (sfx);
	}
	return sfx;
}

static void
s_gamedir (void)
{
	snd_num_sfx = 0;
}

static void
s_soundlist_f (void)
{
	int			load, total, i;
	sfx_t	   *sfx;

	if (Cmd_Argc() >= 2 && Cmd_Argv (1)[0])
		load = 1;
	else
		load = 0;

	total = 0;
	for (sfx = snd_sfx, i = 0; i < snd_num_sfx; i++, sfx++) {
		if (load) {
			if (!sfx->retain (sfx))
				continue;
		} else {
			if (!sfx->touch (sfx))
				continue;
		}
		total += sfx->length;
		Sys_Printf ("%6d %6d %s\n", sfx->loopstart, sfx->length, sfx->name);

		if (load)
			sfx->release (sfx);
	}
	Sys_Printf ("Total resident: %i\n", total);
}

void
SND_SFX_Init (void)
{
	snd_sfx_hash = Hash_NewTable (511, snd_sfx_getkey, snd_sfx_free, 0);
	precache = Cvar_Get ("precache", "1", CVAR_NONE, NULL,
						 "Toggle the use of a precache");

	QFS_GamedirCallback (s_gamedir);

	Cmd_AddCommand ("soundlist", s_soundlist_f,
					"Reports a list of sounds in the cache");
}
