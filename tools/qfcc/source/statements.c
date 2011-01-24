/*
	statements.c

	Internal statements

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/06/18

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
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>

#include "QF/va.h"

#include "expr.h"
#include "options.h"
#include "qc-parse.h"
#include "qfcc.h"
#include "statements.h"
#include "strpool.h"
#include "symtab.h"
#include "type.h"

static __attribute__ ((used)) const char rcsid[] = "$Id$";

static void
print_operand (operand_t *op)
{
	switch (op->op_type) {
		case op_symbol:
			if (op->type != op->o.symbol->type->type)
				printf ("(%s) ", pr_type_name[op->type]);
			printf ("%s", op->o.symbol->name);
			break;
		case op_value:
			if (op->type != op->o.value->type)
				printf ("(%s) ", pr_type_name[op->type]);
			switch (op->o.value->type) {
				case ev_string:
					printf ("\"%s\"", op->o.value->v.string_val);
					break;
				case ev_float:
					printf ("%g", op->o.value->v.float_val);
					break;
				case ev_vector:
					printf ("'%g", op->o.value->v.vector_val[0]);
					printf (" %g", op->o.value->v.vector_val[1]);
					printf (" %g'", op->o.value->v.vector_val[2]);
					break;
				case ev_quat:
					printf ("'%g", op->o.value->v.quaternion_val[0]);
					printf (" %g", op->o.value->v.quaternion_val[1]);
					printf (" %g", op->o.value->v.quaternion_val[2]);
					printf (" %g'", op->o.value->v.quaternion_val[3]);
					break;
				case ev_pointer:
					printf ("(%s)[%d]",
							pr_type_name[op->o.value->v.pointer.type->type],
							op->o.value->v.pointer.val);
					break;
				case ev_field:
					printf ("%d", op->o.value->v.pointer.val);
					break;
				case ev_entity:
				case ev_func:
				case ev_integer:
					printf ("%d", op->o.value->v.integer_val);
					break;
				case ev_short:
					printf ("%d", op->o.value->v.short_val);
					break;
				case ev_void:
				case ev_invalid:
				case ev_type_count:
					internal_error (0, "weird value type");
			}
			break;
		case op_label:
			printf ("block %p", op->o.label->dest);
			break;
		case op_temp:
			printf ("tmp %p", op);
			break;
	}
}

void
print_statement (statement_t *s)
{
	printf ("(%s, ", s->opcode);
	if (s->opa)
		print_operand (s->opa);
	printf (", ");
	if (s->opb)
		print_operand (s->opb);
	printf (", ");
	if (s->opc)
		print_operand (s->opc);
	printf (")\n");
}

static sblock_t *free_sblocks;
static statement_t *free_statements;
static operand_t *free_operands;

static sblock_t *
new_sblock (void)
{
	sblock_t   *sblock;
	ALLOC (256, sblock_t, sblocks, sblock);
	sblock->tail = &sblock->statements;
	return sblock;
}

static void
sblock_add_statement (sblock_t *sblock, statement_t *statement)
{
	// this should normally be null, but might be inserting
	statement->next = *sblock->tail;
	*sblock->tail = statement;
	sblock->tail = &statement->next;
}

static statement_t *
new_statement (const char *opcode)
{
	statement_t *statement;
	ALLOC (256, statement_t, statements, statement);
	statement->opcode = save_string (opcode);
	return statement;
}

static operand_t *
new_operand (op_type_e op)
{
	operand_t *operand;
	ALLOC (256, operand_t, operands, operand);
	operand->op_type = op;
	return operand;
}

static const char *
convert_op (int op)
{
	switch (op) {
		case PAS:	return ".=";
		case OR:	return "||";
		case AND:	return "&&";
		case EQ:	return "==";
		case NE:	return "!=";
		case LE:	return "<=";
		case GE:	return ">=";
		case LT:	return "<";
		case GT:	return ">";
		case '=':	return "=";
		case '+':	return "+";
		case '-':	return "-";
		case '*':	return "*";
		case '/':	return "/";
		case '%':	return "%";
		case '&':	return "&";
		case '|':	return "|";
		case '^':	return "^";
		case '~':	return "~";
		case '!':	return "!";
		case SHL:	return "<<";
		case SHR:	return ">>";
		case '.':	return ".";
		case 'i':	return "<IF>";
		case 'n':	return "<IFNOT>";
		case IFBE:	return "<IFBE>";
		case IFB:	return "<IFB>";
		case IFAE:	return "<IFAE>";
		case IFA:	return "<IFA>";
		case 'C':	return "=";
		case 'M':	return "<MOVE>";
		default:
			return 0;
	}
}

typedef sblock_t *(*statement_f) (sblock_t *, expr_t *);
typedef sblock_t *(*expr_f) (sblock_t *, expr_t *, operand_t **);

static sblock_t *statement_subexpr (sblock_t *sblock, expr_t *e,
									operand_t **op);
static sblock_t *statement_slist (sblock_t *sblock, expr_t *e);

static sblock_t *
statement_branch (sblock_t *sblock, expr_t *e)
{
	statement_t *s = 0;
	const char *opcode;

	if (e->type == ex_uexpr && e->e.expr.op == 'g') {
		s = new_statement ("<GOTO>");
		s->opa = new_operand (op_label);
		s->opa->o.label = &e->e.expr.e1->e.label;
	} else {
		opcode = convert_op (e->e.expr.op);
		s = new_statement (opcode);
		sblock = statement_subexpr (sblock, e->e.expr.e1, &s->opa);
		s->opb = new_operand (op_label);
		s->opb->o.label = &e->e.expr.e2->e.label;
	}

	sblock_add_statement (sblock, s);
	sblock->next = new_sblock ();
	return sblock->next;
}

static sblock_t *
statement_assign (sblock_t *sblock, expr_t *e)
{
	statement_t *s;
	expr_t     *src_expr = e->e.expr.e2;
	expr_t     *dst_expr = e->e.expr.e1;
	operand_t  *src = 0;
	operand_t  *dst = 0;
	operand_t  *ofs = 0;
	const char *opcode = convert_op (e->e.expr.op);

	s = new_statement (opcode);
	if (e->e.expr.op == '=') {
		sblock = statement_subexpr (sblock, dst_expr, &dst);
		sblock = statement_subexpr (sblock, src_expr, &src);
		ofs = 0;
	} else {
		sblock = statement_subexpr (sblock, src_expr, &src);
		if (dst_expr->type == ex_expr
			&& extract_type (dst_expr->e.expr.e1) == ev_pointer
			&& !is_constant (dst_expr->e.expr.e1)) {
			sblock = statement_subexpr (sblock, dst_expr->e.expr.e1, &dst);
			sblock = statement_subexpr (sblock, dst_expr->e.expr.e2, &ofs);
		} else {
			sblock = statement_subexpr (sblock, dst_expr, &dst);
			ofs = 0;
		}
	}
	s->opa = src;
	s->opb = dst;
	s->opc = ofs;
	sblock_add_statement (sblock, s);
	return sblock;
}

static sblock_t *
vector_call (sblock_t *sblock, expr_t *earg, expr_t *param, int ind,
			 operand_t **op)
{
	expr_t     *a, *v, *n;
	int         i;
	static const char *names[] = {"x", "y", "z"};

	for (i = 0; i < 3; i++) {
		n = new_name_expr (names[i]);
		v = new_float_expr (earg->e.value.v.vector_val[i]);
		a = assign_expr (binary_expr ('.', param, n), v);
		param = new_param_expr (get_type (earg), ind);
		a->line = earg->line;
		a->file = earg->file;
		sblock = statement_slist (sblock, a);
	}
	sblock = statement_subexpr (sblock, param, op);
	return sblock;
}


static sblock_t *
expr_call (sblock_t *sblock, expr_t *call, operand_t **op)
{
	expr_t     *func = call->e.expr.e1;
	expr_t     *args = call->e.expr.e2;
	expr_t     *a;
	expr_t     *param;
	operand_t  *arguments[2] = {0, 0};
	int         count = 0;
	int         ind;
	const char *opcode;
	const char *pref = "";
	statement_t *s;

	for (a = args; a; a = a->next)
		count++;
	ind = count;
	for (a = args; a; a = a->next) {
		ind--;
		param = new_param_expr (get_type (a), ind);
		if (count && options.code.progsversion != PROG_ID_VERSION && ind < 2) {
			pref = "R";
			sblock = statement_subexpr (sblock, param, &arguments[ind]);
			if (options.code.vector_calls && a->type == ex_value
				&& a->e.value.type == ev_vector)
				sblock = vector_call (sblock, a, param, ind, &arguments[ind]);
			else
				sblock = statement_subexpr (sblock, a, &arguments[ind]);
			continue;
		}
		if (is_struct (get_type (param))) {
			expr_t     *mov = assign_expr (param, a);
			mov->line = a->line;
			mov->file = a->file;
			sblock = statement_slist (sblock, mov);
		} else {
			if (options.code.vector_calls && a->type == ex_value 
				&& a->e.value.type == ev_vector) {
				sblock = vector_call (sblock, a, param, ind, 0);
			} else {
				operand_t  *p;
				operand_t  *arg;
				sblock = statement_subexpr (sblock, param, &p);
				arg = p;
				sblock = statement_subexpr (sblock, a, &arg);
				if (arg != p) {
					s = new_statement ("=");
					s->opa = arg;
					s->opb = p;
					sblock_add_statement (sblock, s);
				}
			}
		}
	}
	opcode = va ("<%sCALL%d>", pref, count);
	s = new_statement (opcode);
	sblock = statement_subexpr (sblock, func, &s->opa);
	s->opb = arguments[0];
	s->opc = arguments[1];
	sblock_add_statement (sblock, s);
	sblock->next = new_sblock ();
	return sblock->next;
}

static sblock_t *
expr_address (sblock_t *sblock, expr_t *e, operand_t **op)
{
	if (e->type == ex_uexpr) {
		sblock = statement_subexpr (sblock, e->e.expr.e1, op);
		(*op)->type = ev_void;
	}
	return sblock;
}

static sblock_t *
expr_deref (sblock_t *sblock, expr_t *e, operand_t **op)
{
	type_t     *type = e->e.expr.type;

	e = e->e.expr.e1;
	if (e->type == ex_uexpr && e->e.expr.op == '&'
		&& e->e.expr.e1->type == ex_symbol) {
		*op = new_operand (op_symbol);
		(*op)->type = type->type;
		(*op)->o.symbol = e->e.expr.e1->e.symbol;
	}
	return sblock;
}

static sblock_t *
expr_block (sblock_t *sblock, expr_t *e, operand_t **op)
{
	if (!e->e.block.result)
		internal_error (e, "block sub-expression without result");
	sblock = statement_slist (sblock, e->e.block.head);
	sblock = statement_subexpr (sblock, e->e.block.result, op);
	return sblock;
}

static sblock_t *
expr_expr (sblock_t *sblock, expr_t *e, operand_t **op)
{
	const char *opcode;
	statement_t *s;

	switch (e->e.expr.op) {
		case 'c':
			sblock = expr_call (sblock, e, op);
			break;
		default:
			opcode = convert_op (e->e.expr.op);
			if (!opcode)
				internal_error (e, "ice ice baby");
			s = new_statement (opcode);
			sblock = statement_subexpr (sblock, e->e.expr.e1, &s->opa);
			sblock = statement_subexpr (sblock, e->e.expr.e2, &s->opb);
			if (!*op) {
				*op = new_operand (op_temp);
				(*op)->type = e->e.expr.type->type;
			}
			s->opc = *op;
			sblock_add_statement (sblock, s);
			break;
	}
	return sblock;
}

static sblock_t *
expr_cast (sblock_t *sblock, expr_t *e, operand_t **op)
{
	if (!*op) {
		(*op) = new_operand (op_temp);
		(*op)->type = e->e.expr.type->type;
	}
	sblock = statement_subexpr (sblock, e->e.expr.e1, op);
	return sblock;
}

static sblock_t *
expr_uexpr (sblock_t *sblock, expr_t *e, operand_t **op)
{
	switch (e->e.expr.op) {
		case '&':
			sblock = expr_address (sblock, e, op);
			break;
		case '.':
			sblock = expr_deref (sblock, e, op);
			break;
		case 'C':
			sblock = expr_cast (sblock, e, op);
			break;
		default:
			;
	}
	return sblock;
}

static sblock_t *
expr_symbol (sblock_t *sblock, expr_t *e, operand_t **op)
{
	*op = new_operand (op_symbol);
	(*op)->type = e->e.symbol->type->type;
	(*op)->o.symbol = e->e.symbol;
	return sblock;
}

static sblock_t *
expr_temp (sblock_t *sblock, expr_t *e, operand_t **op)
{
	if (!e->e.temp.op) {
		e->e.temp.op = new_operand (op_temp);
		e->e.temp.op->type = e->e.temp.type->type;
	}
	*op = e->e.temp.op;
	return sblock;
}

static sblock_t *
expr_value (sblock_t *sblock, expr_t *e, operand_t **op)
{
	*op = new_operand (op_value);
	(*op)->type = e->e.value.type;
	(*op)->o.value = &e->e.value;
	return sblock;
}

static sblock_t *
statement_subexpr (sblock_t *sblock, expr_t *e, operand_t **op)
{
	static expr_f sfuncs[] = {
		0,					// ex_error
		0,					// ex_state
		0,					// ex_bool
		0,					// ex_label
		expr_block,			// ex_block
		expr_expr,
		expr_uexpr,
		expr_symbol,
		expr_temp,
		0,					// ex_nil
		expr_value,
	};
	if (!e) {
		*op = 0;
		return sblock;
	}

	if (e->type < 0 || e->type > ex_value)
		internal_error (e, "bad expression type");
	if (!sfuncs[e->type])
		internal_error (e, "unexpected expression type");

	sblock = sfuncs[e->type] (sblock, e, op);
	return sblock;
}

static sblock_t *
statement_ignore (sblock_t *sblock, expr_t *e)
{
	return sblock;
}

static sblock_t *
statement_state (sblock_t *sblock, expr_t *e)
{
	statement_t *s;

	s = new_statement ("<STATE>");
	sblock = statement_subexpr (sblock, e->e.state.frame, &s->opa);
	sblock = statement_subexpr (sblock, e->e.state.think, &s->opb);
	sblock = statement_subexpr (sblock, e->e.state.step, &s->opc);
	sblock_add_statement (sblock, s);
	return sblock;
}

static void
build_bool_block (expr_t *block, expr_t *e)
{
	switch (e->type) {
		case ex_bool:
			build_bool_block (block, e->e.bool.e);
			return;
		case ex_label:
			e->next = 0;
			append_expr (block, e);
			return;
		case ex_expr:
			if (e->e.expr.op == OR || e->e.expr.op == AND) {
				build_bool_block (block, e->e.expr.e1);
				build_bool_block (block, e->e.expr.e2);
			} else if (e->e.expr.op == 'i') {
				e->next = 0;
				append_expr (block, e);
			} else if (e->e.expr.op == 'n') {
				e->next = 0;
				append_expr (block, e);
			}
			return;
		case ex_uexpr:
			if (e->e.expr.op == 'g') {
				e->next = 0;
				append_expr (block, e);
				return;
			}
			break;
		case ex_block:
			if (!e->e.block.result) {
				expr_t     *t;
				for (e = e->e.block.head; e; e = t) {
					t = e->next;
					build_bool_block (block, e);
				}
				return;
			}
			break;
		default:
			;
	}
	internal_error (e, "bad boolean");
}

static int
is_goto (expr_t *e)
{
	return e && e->type == ex_uexpr && e->e.expr.op == 'g';
}

static int
is_if (expr_t *e)
{
	return e && e->type == ex_expr && e->e.expr.op == 'i';
}

static int
is_ifnot (expr_t *e)
{
	return e && e->type == ex_expr && e->e.expr.op == 'n';
}

static sblock_t *
statement_bool (sblock_t *sblock, expr_t *e)
{
	expr_t    **s;
	expr_t     *l;
	expr_t     *block = new_block_expr ();

	build_bool_block (block, e);

	s = &block->e.block.head;
	while (*s) {
		if (is_if (*s) && is_goto ((*s)->next)) {
			l = (*s)->e.expr.e2;
			for (e = (*s)->next->next; e && e->type == ex_label; e = e->next) {
				if (e == l) {
					e = *s;
					e->e.expr.op = 'n';
					e->e.expr.e2 = e->next->e.expr.e1;
					e->next = e->next->next;
					break;
				}
			}
			s = &(*s)->next;
		} else if (is_ifnot (*s) && is_goto ((*s)->next)) {
			l = (*s)->e.expr.e2;
			for (e = (*s)->next->next; e && e->type == ex_label; e = e->next) {
				if (e == l) {
					e = *s;
					e->e.expr.op = 'i';
					e->e.expr.e2 = e->next->e.expr.e1;
					e->next = e->next->next;
					break;
				}
			}
			s = &(*s)->next;
		} else if (is_goto (*s)) {
			l = (*s)->e.expr.e1;
			for (e = (*s)->next; e && e->type == ex_label; e = e->next) {
				if (e == l) {
					*s = (*s)->next;
					l = 0;
					break;
				}
			}
			if (l)
				s = &(*s)->next;
		} else {
			s = &(*s)->next;
		}
	}
	sblock = statement_slist (sblock, block->e.block.head);
	return sblock;
}

static sblock_t *
statement_label (sblock_t *sblock, expr_t *e)
{
	if (sblock->statements) {
		sblock->next = new_sblock ();
		sblock = sblock->next;
	}
	e->e.label.dest = sblock;
	return sblock;
}

static sblock_t *
statement_block (sblock_t *sblock, expr_t *e)
{
	if (sblock->statements) {
		sblock->next = new_sblock ();
		sblock = sblock->next;
	}
	sblock = statement_slist (sblock, e->e.block.head);
	return sblock;
}

static sblock_t *
statement_expr (sblock_t *sblock, expr_t *e)
{
	switch (e->e.expr.op) {
		case 'c':
			sblock = expr_call (sblock, e, 0);
			break;
		case 'i':
		case 'n':
		case IFBE:
		case IFB:
		case IFAE:
		case IFA:
			sblock = statement_branch (sblock, e);
			break;
		case '=':
		case PAS:
			sblock = statement_assign (sblock, e);
			break;
		default:
			if (e->e.expr.op < 256)
				notice (e, "e %c", e->e.expr.op);
			else
				notice (e, "e %d", e->e.expr.op);
			if (options.warnings.executable)
				warning (e, "Non-executable statement;"
						 " executing programmer instead.");
	}
	return sblock;
}

static sblock_t *
statement_uexpr (sblock_t *sblock, expr_t *e)
{
	const char *opcode;
	statement_t *s;

	switch (e->e.expr.op) {
		case 'r':
			notice (e, "RETURN");
			opcode = "<RETURN>";
			if (!e->e.expr.e1 && !options.traditional)
				opcode = "<RETURN_V>";
			s = new_statement (opcode);
			sblock = statement_subexpr (sblock, e->e.expr.e1, &s->opa);
			sblock_add_statement (sblock, s);
			sblock->next = new_sblock ();
			sblock = sblock->next;
			break;
		case 'g':
			sblock = statement_branch (sblock, e);
			break;
		default:
			notice (e, "e ue %d", e->e.expr.op);
			if (options.warnings.executable)
				warning (e, "Non-executable statement;"
						 " executing programmer instead.");
	}
	return sblock;
}

static sblock_t *
statement_nonexec (sblock_t *sblock, expr_t *e)
{
	if (options.warnings.executable)
		warning (e, "Non-executable statement; executing programmer instead.");
	return sblock;
}

static sblock_t *
statement_slist (sblock_t *sblock, expr_t *e)
{
	static statement_f sfuncs[] = {
		statement_ignore,	// ex_error
		statement_state,
		statement_bool,
		statement_label,
		statement_block,
		statement_expr,
		statement_uexpr,
		statement_nonexec,	// ex_symbol
		statement_nonexec,	// ex_temp
		statement_nonexec,	// ex_nil
		statement_nonexec,	// ex_value
	};

	for (/**/; e; e = e->next) {
		if (e->type < 0 || e->type > ex_value)
			internal_error (e, "bad expression type");
		sblock = sfuncs[e->type] (sblock, e);
	}
	return sblock;
}

sblock_t *
make_statements (expr_t *e)
{
	sblock_t   *sblock = new_sblock ();
//	print_expr (e);
	statement_slist (sblock, e);
//	print_flow (sblock);
	return sblock;
}