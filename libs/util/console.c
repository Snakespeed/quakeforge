/*
	console.c

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

#include <stdarg.h>

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/sys.h"

#include "compat.h"

#define	MAXPRINTMSG	4096


/*
	Con_Printf

	Handles cursor positioning, line wrapping, etc
*/
void
Con_Printf (const char *fmt, ...)
{
	va_list     argptr;

	va_start (argptr, fmt);
	Con_Print (fmt, argptr);
	va_end (argptr);
}

/*
	Con_DPrintf

	A Con_Printf that only shows up if the "developer" cvar is set
*/
void
Con_DPrintf (const char *fmt, ...)
{
	va_list     argptr;

	if (!developer->int_val)
		return;							// don't confuse non-developers with
	// techie stuff...

	va_start (argptr, fmt);
	Con_Print (fmt, argptr);
	va_end (argptr);
}

/*
	Con_DisplayList

	New function for tab-completion system
	Added by EvilTypeGuy
	MEGA Thanks to Taniwha

*/
void
Con_DisplayList(const char **list, int con_linewidth)
{
	int	i = 0;
	int	pos = 0;
	int	len = 0;
	int	maxlen = 0;
	int	width = (con_linewidth - 4);
	const char	**walk = list;

	while (*walk) {
		len = strlen(*walk);
		if (len > maxlen)
			maxlen = len;
		walk++;
	}
	maxlen += 1;

	while (*list) {
		len = strlen(*list);
		if (pos + maxlen >= width) {
			Con_Printf("\n");
			pos = 0;
		}

		Con_Printf("%s", *list);
		for (i = 0; i < (maxlen - len); i++)
			Con_Printf(" ");

		pos += maxlen;
		list++;
	}

	if (pos)
		Con_Printf("\n\n");
}
