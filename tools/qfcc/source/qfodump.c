/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2001 #AUTHOR#

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

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif
#include <getopt.h>

#include "QF/dstring.h"
#include "QF/vfile.h"

#include "class.h"
#include "def.h"
#include "emit.h"
#include "function.h"
#include "obj_file.h"
#include "options.h"
#include "qfcc.h"
#include "type.h"

int num_linenos;
pr_lineno_t *linenos;
pr_info_t pr;
defspace_t *new_defspace (void) {return 0;}
scope_t *new_scope (scope_type type, defspace_t *space, scope_t *parent) {return 0;}
string_t ReuseString (const char *str) {return 0;}
void encode_type (dstring_t *str, type_t *type) {}
codespace_t *codespace_new (void) {return 0;}
void codespace_addcode (codespace_t *codespace, struct statement_s *code, int size) {}
type_t *parse_type (const char *str) {return 0;}
int function_parms (function_t *f, byte *parm_size) {return 0;}
pr_auxfunction_t *new_auxfunction (void) {return 0;}

static const struct option long_options[] = {
	{NULL, 0, NULL, 0},
};

int
main (int argc, char **argv)
{
	int         c;
	qfo_t      *qfo;
	qfo_def_t  *def;

	while ((c = getopt_long (argc, argv, "", long_options, 0)) != EOF) {
		switch (c) {
			default:
				return 1;
		}
	}
	while (optind < argc) {
		qfo = qfo_read (argv[optind++]);
		if (!qfo)
			return 1;
		for (def = qfo->defs; def - qfo->defs < qfo->num_defs; def++) {
			printf ("%4d %4x %d %s %s %d %d %s:%d\n",
					def->ofs,
					def->flags,
					def->basic_type,
					qfo->types + def->full_type,
					qfo->strings + def->name,
					def->relocs, def->num_relocs,
					qfo->strings + def->file, def->line);
			if (def->flags & (QFOD_LOCAL | QFOD_EXTERNAL))
				continue;
			if (def->basic_type == ev_string) {
				printf ("    %4d %s\n", qfo->data[def->ofs].string_var,
						qfo->strings + qfo->data[def->ofs].string_var);
			} else if (def->basic_type == ev_func) {
				printf ("    %4d %s\n", qfo->data[def->ofs].func_var,
						qfo->strings + qfo->functions[qfo->data[def->ofs].func_var - 1].name);
			} else {
				printf ("    %4d\n", qfo->data[def->ofs].integer_var);
			}
		}
	}
	return 0;
}
