/*
	expr_vector.c

	vector expressions

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/04/27

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

#include <strings.h>
#include <stdlib.h>

#include "QF/alloc.h"
#include "QF/dstring.h"
#include "QF/mathlib.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/class.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/emit.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/idstuff.h"
#include "tools/qfcc/include/method.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/reloc.h"
#include "tools/qfcc/include/shared.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/struct.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

#include "tools/qfcc/source/qc-parse.h"

expr_t *
new_vector_list (expr_t *expr_list)
{
	type_t     *ele_type = type_default;

	// lists are built in reverse order
	expr_list = reverse_expr_list (expr_list);

	int         width = 0;
	int         count = 0;
	for (expr_t *e = expr_list; e; e = e->next) {
		count++;
		type_t     *t = get_type (e);
		if (!t) {
			return e;
		}
		if (!is_math (t)) {
			return error (e, "invalid type for vector element");
		}
		width += type_width (t);
		if (is_nonscalar (t)) {
			t = base_type (t);
		}
		if (type_promotes (t, ele_type)) {
			ele_type = t;
		}
	}
	if (width < 2) {
		return error (expr_list, "not a vector");
	}
	if (width > 4) {
		return error (expr_list, "resulting vector is too large: %d elements",
					  width);
	}

	int         all_constant = 1;
	int         all_implicit = 1;
	expr_t     *elements[count + 1];
	elements[count] = 0;
	count = 0;
	for (expr_t *e = expr_list; e; e = e->next) {
		int         cast_width = type_width (get_type (e));
		type_t     *cast_type = vector_type (ele_type, cast_width);
		all_implicit = all_implicit && e->implicit;
		elements[count] = cast_expr (cast_type, fold_constants (e));
		all_constant = all_constant && is_constant (elements[count]);
		count++;
	}

	switch (count) {
		case 4:
			// all scalars (otherwise width would be too large)
			break;
		case 3:
			// shuffle any vectors to the beginning of the list (there should
			// be only one, but futhre...)
			for (int i = 1; i < count; i++) {
				if (is_nonscalar (get_type (elements[i]))) {
					expr_t     *t = elements[i];
					int         j = i;
					for (; j > 0 && is_scalar (get_type (elements[j])); j--) {
						elements[j] = elements[j - 1];
					}
					elements[j] = t;
				}
			}
			break;
		case 2:
			if (is_scalar (get_type (elements[0]))
				&& is_nonscalar (get_type (elements[1]))) {
				// swap s, v to be v, s (ie, vector always comes before scalar)
				expr_t     *t = elements[0];
				elements[0] = elements[1];
				elements[1] = t;
			}
			break;
		case 1:
			if (is_scalar (get_type (elements[0]))) {
				internal_error (expr_list, "confused about vectors");
			}
			// it's already a vector
			return elements[0];
	}

	if (all_constant) {
		type_t     *vec_type = vector_type (ele_type, width);
		pr_type_t   value[type_size (vec_type)];

		for (int i = 0, offs = 0; i < count; i++) {
			type_t     *src_type = get_type (elements[i]);
			value_store (value + offs, src_type, elements[i]);
			offs += type_size (src_type);
		}

		expr_t     *vec = new_value_expr (new_type_value (vec_type, value));
		vec->implicit = all_implicit;
		return vec;
	}

	for (int i = 0; i < count; i++) {
		elements[i]->next = elements[i + 1];
	}

	expr_t     *vec = new_expr ();
	vec->type = ex_vector;
	vec->e.vector.type = vector_type (ele_type, width);
	vec->e.vector.list = elements[0];
	return vec;
}
