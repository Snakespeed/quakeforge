/*
	connection.h

	connection management

	Copyright (C) 2004 Bill Currie <bill@taniwha.org>

	Author: Bill Currie
	Date: 2004/02/20

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

#ifndef __connection_h
#define __connection_h

#include "netchan.h"

typedef struct connection_s {
	netadr_t    address;
	void       *object;
	void      (*handler) (struct connection_s *con, void *obj);
} connection_t;

void Connection_Init (void);
connection_t *Connection_Add (netadr_t *address, void *object,
							  void (*handler)(connection_t *, void *));
void Connection_Del (connection_t *con);
connection_t *Connection_Find (netadr_t *address);

#endif//__connection_h
