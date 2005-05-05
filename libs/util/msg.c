/*
	msg.c

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

static __attribute__ ((unused)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/msg.h"
#include "QF/qendian.h"
#include "QF/sys.h"

#include "compat.h"

/*
	MESSAGE IO FUNCTIONS

	Handles byte ordering and avoids alignment errors
*/

// writing functions ==========================================================

void
MSG_WriteByte (sizebuf_t *sb, int c)
{
	byte	   *buf;

	buf = SZ_GetSpace (sb, 1);
	*buf = c;
}

void
MSG_WriteShort (sizebuf_t *sb, int c)
{
	byte	   *buf;

	buf = SZ_GetSpace (sb, 2);
	*buf++ = ((unsigned int) c) & 0xff;
	*buf = ((unsigned int) c) >> 8;
}

void
MSG_WriteLong (sizebuf_t *sb, int c)
{
	byte	   *buf;

	buf = SZ_GetSpace (sb, 4);
	*buf++ = ((unsigned int) c) & 0xff;
	*buf++ = (((unsigned int) c) >> 8) & 0xff;
	*buf++ = (((unsigned int) c) >> 16) & 0xff;
	*buf = ((unsigned int) c) >> 24;
}

void
MSG_WriteFloat (sizebuf_t *sb, float f)
{
	union {
		float       f;
		unsigned int l;
	} dat;

	dat.f = f;
	dat.l = LittleLong (dat.l);

	SZ_Write (sb, &dat.l, 4);
}

void
MSG_WriteString (sizebuf_t *sb, const char *s)
{
	if (!s)
		SZ_Write (sb, "", 1);
	else
		SZ_Write (sb, s, strlen (s) + 1);
}

void
MSG_WriteCoord (sizebuf_t *sb, float coord)
{
	MSG_WriteShort (sb, (int) (coord * 8.0));
}

void
MSG_WriteCoordV (sizebuf_t *sb, const vec3_t coord)
{
	byte        *buf;
	unsigned int i, j;

	buf = SZ_GetSpace (sb, 6);
	for (i = 0; i < 3; i++) {
		j = (int) (coord[i] * 8.0);
		*buf++ = j & 0xff;
		*buf++ = j >> 8;
	}
}

void
MSG_WriteCoordAngleV (sizebuf_t *sb, const vec3_t coord, const vec3_t angles)
{
	byte   *buf;
	int		i, j;

	buf = SZ_GetSpace (sb, 9);
	for (i = 0; i < 3; i++) {
		j = (int) (coord[i] * 8.0);
		*buf++ = j & 0xff;
		*buf++ = j >> 8;
		*buf++ = ((int) (angles[i] * (256.0 / 360.0)) & 255);
	}
}

void
MSG_WriteAngle (sizebuf_t *sb, float angle)
{
	MSG_WriteByte (sb, (int) (angle * (256.0 / 360.0)) & 255);
}

void
MSG_WriteAngleV (sizebuf_t *sb, const vec3_t angles)
{
	byte   *buf;
	int		i;

	buf = SZ_GetSpace (sb, 3);
	for (i = 0; i < 3; i++) {
		*buf++ = ((int) (angles[i] * (256.0 / 360.0)) & 255);
	}
}

void
MSG_WriteAngle16 (sizebuf_t *sb, float angle16)
{
	MSG_WriteShort (sb, (int) (angle16 * (65536.0 / 360.0)) & 65535);
}

void
MSG_WriteUTF8 (sizebuf_t *sb, unsigned utf8)
{
	byte	   *buf;
	int			count;

	if (utf8 & 0x80000000) {
		return;					// invalid (FIXME die?)
	} else if (utf8 & 0x7c000000) {
		buf = SZ_GetSpace (sb, count = 6);
		*buf = 0xfc | ((utf8 & 0x40000000) >> 30);	// 1 bit
		utf8 <<= 2;
	} else if (utf8 & 0x03e00000) {
		buf = SZ_GetSpace (sb, count = 5);
		*buf = 0xf8 | ((utf8 & 0x30000000) >> 28);	// 2 bits
		utf8 <<= 4;
	} else if (utf8 & 0x001f0000) {
		buf = SZ_GetSpace (sb, count = 4);
		*buf = 0xf0 | ((utf8 & 0x001c0000) >> 18);	// 3 bits
		utf8 <<= 14;
	} else if (utf8 & 0x0000f800) {
		buf = SZ_GetSpace (sb, count = 3);
		*buf = 0xe0 | ((utf8 & 0x0000f000) >> 12);	// 4 bits
		utf8 <<= 20;
	} else if (utf8 & 0x00000780) {
		buf = SZ_GetSpace (sb, count = 2);
		*buf = 0xc0 | ((utf8 & 0x00000780) >> 7);	// 5 bits
		utf8 <<= 25;
	} else {
		buf = SZ_GetSpace (sb, count = 1);
		*buf = utf8;
		return;
	}
	while (--count) {
		*buf++ = 0x80 | ((utf8 & 0xfc000000) >> 26);
		utf8 <<= 6;
	}
}

// reading functions ==========================================================

void
MSG_BeginReading (qmsg_t *msg)
{
	msg->readcount = 0;
	msg->badread = false;
}

int
MSG_GetReadCount (qmsg_t *msg)
{
	return msg->readcount;
}

int
MSG_ReadByte (qmsg_t *msg)
{
	if (msg->readcount + 1 <= msg->message->cursize)
		return (unsigned char) msg->message->data[msg->readcount++];

	msg->badread = true;
	return -1;
}

int
MSG_ReadShort (qmsg_t *msg)
{
	int         c;

	if (msg->readcount + 2 <= msg->message->cursize) {
		c = (short) (msg->message->data[msg->readcount]
					 + (msg->message->data[msg->readcount + 1] << 8));
		msg->readcount += 2;
		return c;
	}
	msg->readcount = msg->message->cursize;
	msg->badread = true;
	return -1;
}

int
MSG_ReadLong (qmsg_t *msg)
{
	int         c;

	if (msg->readcount + 4 <= msg->message->cursize) {
		c = msg->message->data[msg->readcount]
			+ (msg->message->data[msg->readcount + 1] << 8)
			+ (msg->message->data[msg->readcount + 2] << 16)
			+ (msg->message->data[msg->readcount + 3] << 24);
		msg->readcount += 4;
		return c;
	}
	msg->readcount = msg->message->cursize;
	msg->badread = true;
	return -1;
}

float
MSG_ReadFloat (qmsg_t *msg)
{
	union {
		byte        b[4];
		float       f;
		int         l;
	} dat;

	if (msg->readcount + 4 <= msg->message->cursize) {
		dat.b[0] = msg->message->data[msg->readcount];
		dat.b[1] = msg->message->data[msg->readcount + 1];
		dat.b[2] = msg->message->data[msg->readcount + 2];
		dat.b[3] = msg->message->data[msg->readcount + 3];
		msg->readcount += 4;

		dat.l = LittleLong (dat.l);

		return dat.f;
	}

	msg->readcount = msg->message->cursize;
	msg->badread = true;
	return -1;
}

const char *
MSG_ReadString (qmsg_t *msg)
{
	char   *string;
	size_t len, maxlen;

	if (msg->badread || msg->readcount + 1 > msg->message->cursize) {
		msg->badread = true;
		return "";
	}

	string = &msg->message->data[msg->readcount];

	maxlen = msg->message->cursize - msg->readcount;
	len = strnlen (string, maxlen);
	if (len == maxlen) {
		msg->readcount = msg->readcount;
		msg->badread = true;
		if (len + 1 > msg->badread_string_size) {
			if (msg->badread_string)
				free (msg->badread_string);
			msg->badread_string = malloc (len + 1);
			msg->badread_string_size = len + 1;
		}
		if (!msg->badread_string)
			Sys_Error ("MSG_ReadString: out of memory");
		strncpy (msg->badread_string, string, len);
		msg->badread_string[len] = 0;
		return msg->badread_string;
	}
	msg->readcount += len + 1;

	return string;
}

int
MSG_ReadBytes (qmsg_t *msg, void *buf, int len)
{
	if (msg->badread || len > msg->message->cursize - msg->readcount) {
		msg->badread = true;
		len = msg->message->cursize - msg->readcount;
	}
	memcpy (buf, msg->message->data + msg->readcount, len);
	msg->readcount += len;
	return len;
}

float
MSG_ReadCoord (qmsg_t *msg)
{
	return MSG_ReadShort (msg) * (1.0 / 8.0);
}

void
MSG_ReadCoordV (qmsg_t *msg, vec3_t coord)
{
	int		i;

	for (i = 0; i < 3; i++)
		coord[i] = MSG_ReadShort (msg) * (1.0 / 8.0);
}

void
MSG_ReadCoordAngleV (qmsg_t *msg, vec3_t coord, vec3_t angles)
{
	int		i;

	for (i = 0; i < 3; i++) {
		coord[i] = MSG_ReadShort (msg) * (1.0 / 8.0);
		angles[i] = ((signed char) MSG_ReadByte (msg)) * (360.0 / 256.0);
	}
}

float
MSG_ReadAngle (qmsg_t *msg)
{
	return ((signed char) MSG_ReadByte (msg)) * (360.0 / 256.0);
}

void
MSG_ReadAngleV (qmsg_t *msg, vec3_t angles)
{
	int		i;

	for (i = 0; i < 3; i++)
		angles[i] = ((signed char) MSG_ReadByte (msg)) * (360.0 / 256.0);
}

float
MSG_ReadAngle16 (qmsg_t *msg)
{
	return MSG_ReadShort (msg) * (360.0 / 65536.0);
}

int
MSG_ReadUTF8 (qmsg_t *msg)
{
	byte       *buf, *start, c;
	int         val = 0, count;

	if (msg->badread || msg->message->cursize == msg->readcount) {
		msg->badread = true;
		return -1;
	}
	buf = start = msg->message->data + msg->readcount;

	c = *buf++;
	if (c < 0x80) {
		val = c;
		count = 1;
	} else if (c < 0xc0) {
		msg->badread = true;
		return -1;
	} else if (c < 0xe0) {
		count = 2;
		val = c & 0x1f;
	} else if (c < 0xf0) {
		count = 3;
		val = c & 0x0f;
	} else if (c < 0xf8) {
		count = 4;
		val = c & 0x07;
	} else if (c < 0xfc) {
		count = 5;
		val = c & 0x03;
	} else if (c < 0xfe) {
		count = 6;
		val = c & 0x01;
	} else {
		msg->badread = true;
		return -1;
	}
	if (count > (msg->message->cursize - msg->readcount)) {
		msg->badread = true;
		return -1;
	}
	while (--count) {
		c = *buf++;
		if ((c & 0xc0) != 0x80) {
			msg->badread = true;
			return -1;
		}
		val <<= 6;
		val |= c & 0x3f;
	}
	msg->readcount += buf - start;
	return val;
}
