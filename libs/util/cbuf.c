/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2002 #AUTHOR#

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
#include <ctype.h>
#include <stdlib.h>

#include "QF/cbuf.h"
#include "QF/dstring.h"
#include "QF/qtypes.h"

#include "compat.h"

static dstring_t *_com_token;
const char *com_token;

cbuf_args_t *
Cbuf_ArgsNew (void)
{
	return calloc (1, sizeof (cbuf_args_t));
}

void
Cbuf_ArgsDelete (cbuf_args_t *args)
{
	int         i;

	for (i = 0; i < args->argv_size; i++)
		dstring_delete (args->argv[i]);
	free (args->argv);
	free (args->args);
	free (args);
}

void
Cbuf_ArgsAdd (cbuf_args_t *args, const char *arg)
{
	int         i;

	if (args->argc == args->argv_size) {
		args->argv_size += 4;
		args->argv = realloc (args->argv,
							  args->argv_size * sizeof (dstring_t *));
		args->args = realloc (args->args, args->argv_size * sizeof (char *));
		for (i = args->argv_size - 4; i < args->argv_size; i++) {
			args->argv[i] = dstring_newstr ();
			args->args[i] = 0;
		}
	}
	dstring_clearstr (args->argv[args->argc]);
	dstring_appendstr (args->argv[args->argc], arg);
	args->argc++;
}

const char *
COM_Parse (const char *data)
{
	int         c;
	int         i;

	if (!_com_token) {
		_com_token = dstring_newstr ();
	} else {
		dstring_clearstr (_com_token);
	}
	com_token = _com_token->str;
	
	if (!data)
		return 0;

skipwhite:
	while (isspace ((byte) *data))
		data++;
	if (!*data)
		return 0;
	if (data[0] == '/' && data[1] == '/') {
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}
	if (*data == '"') {
		data++;
		i = 0;
		while (1) {
			c = data[i++];
			if (c == '"' || !c) {
				dstring_insert (_com_token, data, i - 1, 0);
				goto done;
			}
		}
	}
	i = 0;
	do {
		i++;
	} while (data[i] && !isspace ((byte) data[i]));
	dstring_insert (_com_token, data, i, 0);
done:
	com_token = _com_token->str;
	return data + i;
}

void
COM_TokenizeString (const char *str, cbuf_args_t *args)
{
	const char *s;

	args->argc = 0;

	while (1) {
		while (isspace ((byte)*str) && *str != '\n')
			str++;
		if (*str == '\n') {
			str++;
			break;
		}

		if (!*str)
			return;

		s = COM_Parse (str);
		if (!s)
			return;

		Cbuf_ArgsAdd (args, com_token);
		args->args[args->argc - 1] = str;
		str = s;
	}
	return;
}

static void
extract_line (cbuf_t *cbuf)
{
	int         i;
	int         len = cbuf->buf->size - 1;
	char       *text = cbuf->buf->str;
	int         quotes = 0;

	dstring_clearstr (cbuf->line);
	for (i = 0; i < len; i++) {
		if (text[i] == '"')
			quotes++;
		if (!(quotes & 1)) {
			if (text[i] == ';')
				// don't break if inside a quoted string
				break;
			if (text[i] == '/' && text[i + 1] == '/') {
				int         j = i;
				while (j < len && text[j] != '\n' && text[j] != '\r')
					i++;
				dstring_snip (cbuf->buf, i, j - i);
				len -= j - i;
			}
		}
		if (text[i] == '\n' || text[i] == '\r')
			break;
	}
	if (i)
		dstring_insert (cbuf->line, text, i, 0);
	if (text[i]) {
		dstring_snip (cbuf->buf, 0, i + 1);
	} else {
		// We've hit the end of the buffer, just clear it
		dstring_clearstr (cbuf->buf);
	}
}

static void
parse_line (cbuf_t *cbuf)
{
	COM_TokenizeString (cbuf->line->str, cbuf->args);;
}

cbuf_t *
Cbuf_New (void)
{
	cbuf_t     *cbuf = calloc (1, sizeof (cbuf_t));

	cbuf->buf = dstring_newstr ();
	cbuf->line = dstring_newstr ();
	cbuf->args = Cbuf_ArgsNew ();
	cbuf->extract_line = extract_line;
	cbuf->parse_line = parse_line;
	return cbuf;
}

void
CBuf_Delete (cbuf_t *cbuf)
{
	if (!cbuf)
		return;
	dstring_delete (cbuf->buf);
	dstring_delete (cbuf->line);
	Cbuf_ArgsDelete (cbuf->args);
	free (cbuf);
}

void
Cbuf_AddText (cbuf_t *cbuf, const char *text)
{
	dstring_appendstr (cbuf->buf, text);
}

void
Cbuf_InsertText (cbuf_t *cbuf, const char *text)
{
	dstring_insertstr (cbuf->buf, "\n", 0);
	dstring_insertstr (cbuf->buf, text, 0);
}

void
Cbuf_Execute (cbuf_t *cbuf)
{
	while (cbuf->buf->str[0]) {
		cbuf->extract_line (cbuf);
		cbuf->parse_line (cbuf);
		if (!cbuf->args->argc)
			continue;
		Cmd_ExecCmd (cbuf->args);
	}
}

void
Cbuf_Execute_Sets (cbuf_t *cbuf)
{
	while (cbuf->buf->str[0]) {
		cbuf->extract_line (cbuf);
		cbuf->parse_line (cbuf);
		if (!cbuf->args->argc)
			continue;
		if (strequal (cbuf->args->argv[0]->str, "set")
			|| strequal (cbuf->args->argv[0]->str, "setrom"))
			Cmd_ExecCmd (cbuf);
	}
}
