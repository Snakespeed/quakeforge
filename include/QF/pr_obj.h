/*
	pr_obj.h

	progs obj support

	Copyright (C) 2002       Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/5/10

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

#ifndef __pr_obj_h
#define __pr_obj_h

#include "QF/pr_comp.h"

#define PR_BITS_PER_INT (sizeof (int) * 8)

#define __PR_CLS_INFO(cls) ((cls)->info)
#define __PR_CLS_ISINFO(cls, mask) ((__PR_CLS_INFO (cls) & (mask)) == (mask))
#define __PR_CLS_SETINFO(cls, mask) (__PR_CLS_INFO (cls) |= (mask))

/*
	The structure is of type MetaClass
*/
#define _PR_CLS_META 0x2
#define PR_CLS_ISMETA(cls) ((cls) && __PR_CLS_ISINFO (cls, _PR_CLS_META))

/*
	The structure is of type Class
*/
#define _PR_CLS_CLASS 0x1
#define PR_CLS_ISCLASS(cls) ((cls) && __PR_CLS_ISINFO (cls, _PR_CLS_CLASS))

/*
	The class is initialized within the runtime.  This means that
	it has had correct super and sublinks assigned
*/
#define _PR_CLS_RESOLV 0x8
#define PR_CLS_ISRESOLV(cls) __PR_CLS_ISINFO (cls, _PR_CLS_RESOLV)
#define PR_CLS_SETRESOLV(cls) __PR_CLS_SETINFO (cls, _PR_CLS_RESOLV)

/*
	The class has been sent a +initialize message or such is not
	defined for this class
*/
#define _PR_CLS_INITIALIZED 0x8
#define PR_CLS_ISINITIALIZED(cls) __PR_CLS_ISINFO (cls, _PR_CLS_INITIALIZED)
#define PR_CLS_SETINITIALIZED(cls) __PR_CLS_SETINFO (cls, _PR_CLS_INITIALIZED)

/*
	The class number of this class.  This must be the same for both the
	class and its meta class boject
*/
#define PR_CLS_GETNUMBER(cls) (__CLS_INFO (cls) >> (PR_BITS_PER_INT / 2))
#define PR_CLS_SETNUMBER(cls, num) \
  (__PR_CLS_INFO (cls) = __PR_CLS_INFO (cls) & (~0U >> (PR_BITS_PER_INT / 2)) \
   						| (num) << (PR_BITS_PER_INT / 2))

typedef struct pr_sel_s {
	pointer_t   sel_id;
	string_t    sel_types;
} pr_sel_t;

typedef struct pr_id_s {
	pointer_t   class_pointer;		// pr_class_t
} pr_id_t;

typedef struct pr_class_s {
	pointer_t   class_pointer;		// pr_class_t
	pointer_t   super_class;		// pr_class_t
	string_t    name;
	int         version;
	unsigned int info;
	int         instance_size;
	pointer_t   ivars;				// pr_ivar_list_t
	pointer_t   methods;			// pr_method_list_t
	pointer_t   dtable;
	pointer_t   subclass_list;		// pr_class_t
	pointer_t   sibling_class;		// pr_class_t
	pointer_t   protocols;			// pr_protocol_list_t
	pointer_t   gc_object_type;
} pr_class_t;

typedef struct pr_protocol_s {
	pointer_t   class_pointer;		// pr_class_t
	string_t    protocol_name;
	pointer_t   protocol_list;		// pr_protocol_list_t
	pointer_t   instance_methods;	// pr_method_list_t
	pointer_t   class_methods;		// pr_method_list_t
} pr_protocol_t;

typedef struct pr_category_s {
	string_t    category_name;
	string_t    class_name;
	pointer_t   instance_methods;	// pr_method_list_t
	pointer_t   class_methods;		// pr_method_list_t
	pointer_t   protocols;			// pr_protocol_list_t
} pr_category_t;

typedef struct pr_protocol_list_s {
	pointer_t   next;
	int         count;
	pointer_t   list[1];
} pr_protocol_list_t;

typedef struct pr_method_list_s {
	pointer_t   method_next;
	int         method_count;
	struct pr_method_s {
		pr_sel_t    method_name;
		string_t    method_types;
		func_t      method_imp;		// typedef id (id, SEL, ...) IMP
	} method_list[1];
} pr_method_list_t;
typedef struct pr_method_s pr_method_t;

typedef struct pr_ivar_list_s {
	int         ivar_count;
	struct pr_ivar_s {
		string_t    ivar_name;
		string_t    ivar_type;
		int         ivar_offset;
	} ivar_list[1];
} pr_ivar_list_t;
typedef struct pr_ivar_s pr_ivar_t;

typedef struct pr_static_instances_s {
	// one per staticly instanced class per module (eg, 3 instances of Object
	// will produce one of these structs with 3 pointers to those instances in
	// instances[]
	string_t    class_name;
	pointer_t   instances[1];		// null terminated array of pr_id_t
} pr_static_instances_t;

typedef struct pr_symtab_s {
	int         sel_ref_cnt;
	pointer_t   refs;				// pr_sel_t
	int         cls_def_cnt;
	int         cat_def_cnt;
	pointer_t   defs[1];			// variable array of class pointers then
									// category pointers
} pr_symtab_t;

typedef struct pr_module_s {
	int         version;
	int         size;
	string_t    name;
	pointer_t   symtab;				// pr_symtab_t
} pr_module_t;

typedef struct pr_super_s {
	pointer_t   self;
	pointer_t   class;
} pr_super_t;

#endif//__pr_obj_h
