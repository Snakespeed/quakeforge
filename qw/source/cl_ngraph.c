/*
	cl_ngraph.c

	client network diagnostic graph

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

#include "QF/draw.h"
#include "QF/render.h"

#include "cl_parse.h"
#include "client.h"
#include "sbar.h"

void
CL_NetGraph (void)
{
	int         a, l, x, y, h, i;
	int         lost;
	char        st[80];

	x = cl_hudswap->int_val ? vid.width - (NET_TIMINGS + 16): 0;
	y = vid.height - sb_lines - 24 - r_graphheight->int_val - 1;

	h = r_graphheight->int_val % 8;

	Draw_TextBox (x, y, NET_TIMINGS / 8, r_graphheight->int_val / 8 + 1);

	lost = CL_CalcNet ();
	x = cl_hudswap->int_val ? vid.width - (NET_TIMINGS + 8) : 8;
	y = vid.height - sb_lines - 9;

	l = NET_TIMINGS;
	if (l > r_refdef.vrect.width - 8)
		l = r_refdef.vrect.width - 8;
	i = (cls.netchan.outgoing_sequence - a) & NET_TIMINGSMASK;
	a = i - l;
	if (a < 0) {
		R_LineGraph (x, y, &packet_latency[a + NET_TIMINGS], -a);
		x -= a;
		l += a;
		a = 0;
	}
	R_LineGraph (x, y, &packet_latency[a], l);

	y = vid.height - sb_lines - 24 - r_graphheight->int_val + 7;
	snprintf (st, sizeof (st), "%3i%% packet loss", lost);
	if (cl_hudswap->int_val) {
		Draw_String8 (vid.width - ((strlen (st) * 8) + 8), y, st);
	} else {
		Draw_String8 (8, y, st);
	}
}
