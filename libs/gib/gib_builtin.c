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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((unused)) const char rcsid[] =
        "$Id$";

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <fnmatch.h>
#include <errno.h>

#include "QF/cvar.h"
#include "QF/quakeio.h"
#include "QF/quakefs.h"
#include "QF/zone.h"
#include "QF/va.h"
#include "QF/sys.h"
#include "QF/cmd.h"
#include "QF/cbuf.h"
#include "QF/hash.h"
#include "QF/dstring.h"
#include "QF/gib_parse.h"
#include "QF/gib_execute.h"
#include "QF/gib_builtin.h"
#include "QF/gib_buffer.h"
#include "QF/gib_function.h"
#include "QF/gib_vars.h"
#include "QF/gib_regex.h"
#include "QF/gib_thread.h"
#include "regex.h"

char gib_null_string[] = "";

hashtab_t *gib_builtins;

/*
	Hashtable callbacks
*/
static const char * 
GIB_Builtin_Get_Key (void *ele, void *ptr)
{
	return ((gib_builtin_t *)ele)->name->str;
}
static void
GIB_Builtin_Free (void *ele, void *ptr)
{
	gib_builtin_t *b;
	b = (gib_builtin_t *) ele;
	dstring_delete (b->name);
	free (b);
}

/*
	GIB_Builtin_Add
	
	Registers a new builtin GIB command.
*/
void
GIB_Builtin_Add (const char *name, void (*func) (void))
{
	gib_builtin_t *new;
	
	if (!gib_builtins)
		gib_builtins = Hash_NewTable (1024, GIB_Builtin_Get_Key, GIB_Builtin_Free, 0);
	
	new = calloc (1, sizeof (gib_builtin_t));
	new->func = func;
	new->name = dstring_newstr();
	dstring_appendstr (new->name, name);
	Hash_Add (gib_builtins, new);
}

/*
	GIB_Builtin_Find
	
	Looks up the builtin name in the builtin hash,
	returning a pointer to the struct on success,
	zero otherwise.
*/
gib_builtin_t *
GIB_Builtin_Find (const char *name)
{
	if (!gib_builtins)
		return 0;
	return (gib_builtin_t *) Hash_Find (gib_builtins, name);
}

dstring_t *
GIB_Return (const char *str)
{
	dstring_t *dstr;
	if (GIB_DATA(cbuf_active)->waitret) {
		dstr = GIB_Buffer_Dsarray_Get (cbuf_active);
		dstring_clearstr (dstr);
		if (!str) 
			return dstr;
		else
			dstring_appendstr (dstr, str);
	}
	return 0;
}

/*
	GIB Builtin functions
	
	See GIB docs for information.
*/
static void
GIB_Function_f (void)
{
	gib_tree_t *program;

	if (GIB_Argc () != 3)
		GIB_USAGE ("name program");
	else {
		// Is the function program already tokenized?
		if (GIB_Argm (2)->delim != '{') {
			// Parse on the fly
			if (!(program = GIB_Parse_Lines (GIB_Argv(2), TREE_NORMAL))) {
				// Error!
				Cbuf_Error ("parse", "Parse error while defining function '%s'.", GIB_Argv(1));
				return;
			}
		} else
			program = GIB_Argm (2)->children;
		GIB_Function_Define (GIB_Argv(1), GIB_Argv(2), program, GIB_DATA(cbuf_active)->globals);
	}
}			

static void
GIB_Function_Get_f (void)
{
	if (GIB_Argc () != 2)
		GIB_USAGE ("name");
	else {
		gib_function_t *f;
		if ((f = GIB_Function_Find (GIB_Argv (1))))
			GIB_Return (f->text->str);
		else
			GIB_Return ("");
	}
}

static void
GIB_Local_f (void)
{
	gib_var_t *var;
	unsigned int index;
	hashtab_t *zero = 0;
	
	if (GIB_Argc () < 2)
		GIB_USAGE ("var [= value1 value2 ...]");
	else {
		var = GIB_Var_Get_Complex (&GIB_DATA(cbuf_active)->locals, &zero, GIB_Argv(1), &index, true);
		if (GIB_Argc() >= 3)
			GIB_Var_Assign (var, index, cbuf_active->args->argv+3, GIB_Argc() - 3);
	}
}


static void
GIB_Global_f (void)
{
	gib_var_t *var;
	unsigned int index;
	hashtab_t *zero = 0;
	
	if (GIB_Argc () < 2)
		GIB_USAGE ("var [= value1 value2 ...]");
	else {
		var = GIB_Var_Get_Complex (&GIB_DATA(cbuf_active)->globals, &zero, GIB_Argv(1), &index, true);
		if (GIB_Argc() >= 3)
			GIB_Var_Assign (var, index, cbuf_active->args->argv+3, GIB_Argc() - 3);
	}
}

static void
GIB_Domain_f (void)
{
	if (GIB_Argc() != 2)
		GIB_USAGE ("domain");
	else
		GIB_DATA(cbuf_active)->globals = GIB_Domain_Get (GIB_Argv(1));
}

static void
GIB_Return_f (void)
{
	cbuf_t *sp = cbuf_active->up;
	
	GIB_DATA(cbuf_active)->done = true;
	
	if (GIB_Argc() > 1 && sp && sp->interpreter == &gib_interp && GIB_DATA(sp)->waitret) {
		unsigned int i;
		dstring_t *dstr;
		for (i = 1; i < GIB_Argc(); i++) {
			dstr = GIB_Buffer_Dsarray_Get (sp);
			dstring_clearstr (dstr);
			dstring_appendstr (dstr, GIB_Argv(i));
		}
	}
}

/*
static void
GIB_Field_Get_f (void)
{
	unsigned int field;
	char *list, *end;
	const char *ifs;
	if (GIB_Argc() < 3 || GIB_Argc() > 4) {
		GIB_USAGE ("list element [fs]");
		return;
	}
	field = atoi (GIB_Argv(2));
	
	if (GIB_Argc() == 4)
		ifs = GIB_Argv (3);
	else if (!(ifs = GIB_Var_Get_Local (cbuf_active, "ifs")))
		ifs = " \t\n\r";
	if (!*ifs) {
		if (field < strlen(GIB_Argv(1))) {
			GIB_Argv(1)[field+1] = 0;
			GIB_Return (GIB_Argv(1)+field);
		} else
			GIB_Return ("");
		return;
	}
	for (list = GIB_Argv(1); *list && strchr(ifs, *list); list++);
	while (field) {
		while (!strchr(ifs, *list))
			list++;
		while (*list && strchr(ifs, *list))
			list++;
		if (!*list) {
			GIB_Return ("");
			return;
		}
		field--;
	}
	for (end = list; !strchr(ifs, *end); end++);
	*end = 0;
	GIB_Return (list);
}
*/
	
static void
GIB_For_f (void)
{
	dstring_t *dstr;
	unsigned int i;
	if (GIB_Argc() < 5) {
		GIB_DATA(cbuf_active)->ip = GIB_DATA(cbuf_active)->ip->jump;
		return;
	}
	GIB_Buffer_Push_Sstack (cbuf_active);
	dstr = GIB_Buffer_Dsarray_Get (cbuf_active);
	dstring_clearstr (dstr);
	dstring_appendstr (dstr, GIB_Argv(1));
	for (i = GIB_Argc()-2; i > 2; i--) {
		dstr = GIB_Buffer_Dsarray_Get (cbuf_active);
		dstring_appendstr (dstr, GIB_Argv(i));
	}
	GIB_Execute_For_Next (cbuf_active);
}

static void
GIB_Break_f (void)
{
	if (!GIB_DATA(cbuf_active)->ip->jump) {
		Cbuf_Error ("loop", "Break command attempted outside of a loop.");
		return;
	}
	if (!GIB_DATA(cbuf_active)->ip->jump->flags & TREE_COND) // In a for loop?
		GIB_Buffer_Pop_Sstack (cbuf_active); // Kill it
	GIB_DATA(cbuf_active)->ip = GIB_DATA(cbuf_active)->ip->jump->jump;
}

static gib_tree_t gib_cont = {
	"",
	' ',
	0, 0, 0, 0,
	TREE_NORMAL
};

static void
GIB_Continue_f (void)
{
	if (!GIB_DATA(cbuf_active)->ip->jump) {
		Cbuf_Error ("loop", "Continue command attempted outside of a loop.");
		return;
	}
	if (GIB_DATA(cbuf_active)->ip->jump->flags & TREE_COND) {
		gib_cont.next = GIB_DATA(cbuf_active)->ip->jump;
		GIB_DATA(cbuf_active)->ip = &gib_cont;
	} else {
		GIB_Execute_For_Next (cbuf_active);
		GIB_DATA(cbuf_active)->ip = GIB_DATA(cbuf_active)->ip->jump;
	}
}

// Note: this is a standard console command, not a GIB builtin
static void
GIB_Runexported_f (void)
{
	gib_function_t *f;
	
	if (!(f = GIB_Function_Find (Cmd_Argv (0))))
		Sys_Printf ("Error:  No function found for exported command \"%s\".\n"
					"This is most likely a bug, please report it to"
					"The QuakeForge developers.", Cmd_Argv(0));
	else {
		cbuf_t *sub = Cbuf_New (&gib_interp);
		GIB_Function_Execute (sub, f, cbuf_active->args);
		cbuf_active->down = sub;
		sub->up = cbuf_active;
		cbuf_active->state = CBUF_STATE_STACK;
	}
}

static void
GIB_Function_Export_f (void)
{
	gib_function_t *f;
	int i;
	
	if (GIB_Argc() < 2)
		GIB_USAGE ("function1 [function2 function3 ...]");
	for (i = 1; i < GIB_Argc(); i++) {
		if (!(f = GIB_Function_Find (GIB_Argv (i))))
			Cbuf_Error ("function", "function::export: function '%s' not found", GIB_Argv (i));
		else if (!f->exported) {
			Cmd_AddCommand (f->name, GIB_Runexported_f, "Exported GIB function.");
			f->exported = true;
		}
	}
}

static void
GIB_String_Length_f (void)
{
	dstring_t *ret;
	if (GIB_Argc() != 2)
		GIB_USAGE ("string");
	else if ((ret = GIB_Return (0)))
		dsprintf (ret, "%i", (int) strlen(GIB_Argv(1)));
}

static void
GIB_String_Equal_f (void)
{
	if (GIB_Argc() != 3)
		GIB_USAGE ("string1 string2");
	else if (strcmp(GIB_Argv(1), GIB_Argv(2)))
		GIB_Return ("0");
	else
		GIB_Return ("1");
}

static void
GIB_String_Sub_f (void)
{
	dstring_t *ret;
	int start, end, len;
	if (GIB_Argc() != 4)
		GIB_USAGE ("string start end");
	else {
		len = strlen (GIB_Argv(1));
		start = atoi (GIB_Argv(2));
		end = atoi (GIB_Argv(3));
		while (start < 0)
			start += len-1;
		while (end < 0)
			end += len;
		if (start >= len)
			return;
		if (end > len || !end)
			end = len;
		if ((ret = GIB_Return (0)))
			dstring_appendsubstr (ret, GIB_Argv(1)+start, end-start);
	}
}

static void
GIB_String_Find_f (void)
{
	dstring_t *ret;
	char *haystack, *res;
	if (GIB_Argc() != 3) {
		GIB_USAGE ("haystack needle");
		return;
	}
	haystack = GIB_Argv(1);
	if ((res = strstr(haystack, GIB_Argv(2)))) {
		if ((ret = GIB_Return (0)))
		dsprintf (ret, "%lu", (unsigned long int)(res - haystack));
	} else
		GIB_Return ("-1");
}
	
static void
GIB_Regex_Match_f (void)
{
	regex_t *reg;
	
	if (GIB_Argc() != 4) {
		GIB_USAGE ("string regex options");
		return;
	}
	
	if (!(reg = GIB_Regex_Compile (GIB_Argv(2), REG_EXTENDED | GIB_Regex_Translate_Options (GIB_Argv(3)))))
		Cbuf_Error ("regex", "%s: %s", GIB_Argv(0), GIB_Regex_Error ());
	else if (regexec(reg, GIB_Argv(1), 0, 0, 0))
		GIB_Return ("0");
	else
		GIB_Return ("1");
}

static void
GIB_Regex_Replace_f (void)
{
	regex_t *reg;
	int ofs, len;
	regmatch_t match[10];
	
	if (GIB_Argc() != 5) {
		GIB_USAGE ("string regex options replacement");
		return;
	}
	
	ofs = 0;
	len = strlen (GIB_Argv(4));
	
	if (!(reg = GIB_Regex_Compile (GIB_Argv(2), REG_EXTENDED | GIB_Regex_Translate_Options (GIB_Argv(3)))))
		Cbuf_Error ("regex", "%s: %s", GIB_Argv(0), GIB_Regex_Error ());
	else if (strchr(GIB_Argv(3), 'g'))
		while (!regexec(reg, GIB_Argv(1)+ofs, 10, match, ofs > 0 ? REG_NOTBOL : 0) && match[0].rm_eo)
			ofs += GIB_Regex_Apply_Match (match, GIB_Argd(1), ofs, GIB_Argv(4));
	else if (!regexec(reg, GIB_Argv(1), 10, match, 0) && match[0].rm_eo)
		GIB_Regex_Apply_Match (match, GIB_Argd(1), 0, GIB_Argv(4));
	GIB_Return (GIB_Argv(1));
}

static void
GIB_Regex_Extract_f (void)
{
	regex_t *reg;
	regmatch_t *match;
	dstring_t *ret;
	int i;
	char o;
	
	if (GIB_Argc() != 4) {
		GIB_USAGE ("string regex options");
		return;
	}
	match = calloc (32, sizeof(regmatch_t));
	
	if (!(reg = GIB_Regex_Compile (GIB_Argv(2), REG_EXTENDED | GIB_Regex_Translate_Options (GIB_Argv(3)))))
		Cbuf_Error ("regex", "%s: %s", GIB_Argv(0), GIB_Regex_Error ());
	else if (!regexec(reg, GIB_Argv(1), 32, match, 0) && match[0].rm_eo) {
		if (!(ret = GIB_Return(0)))
			return;
		dsprintf (ret, "%lu", (unsigned long) match[0].rm_eo);
		for (i = 0; i < 32; i++) {
			if (match[i].rm_so != -1) {
				o = GIB_Argv(1)[match[i].rm_eo];
				GIB_Argv(1)[match[i].rm_eo] = 0;
				GIB_Return (GIB_Argv(1)+match[i].rm_so);
				GIB_Argv(1)[match[i].rm_eo] = o;
			}
		}
	} else
		GIB_Return ("-1");
	free (match);
}

static void
GIB_Thread_Create_f (void)
{
	dstring_t *ret;
	if (GIB_Argc() != 2)
		GIB_USAGE ("program");
	else {
		gib_thread_t *thread = GIB_Thread_New ();
		Cbuf_AddText (thread->cbuf, GIB_Argv(1));
		GIB_Thread_Add (thread);
		if ((ret = GIB_Return (0)))
			dsprintf (ret, "%lu", thread->id);
	}
}

static void
GIB_Thread_Kill_f (void)
{
	if (GIB_Argc() != 2)
		GIB_USAGE ("id");
	else {
		gib_thread_t *thread;
		unsigned long int id = strtoul (GIB_Argv(1), 0, 10);
		thread = GIB_Thread_Find (id);
		if (!thread) {
			Cbuf_Error ("thread", "thread.kill: thread %lu does not exist.", id);
			return;
		}
	}
}

static void
GIB_Event_Register_f (void)
{
	gib_function_t *func;
	if (GIB_Argc() != 3)
		GIB_USAGE ("event function");
	else if (!(func = GIB_Function_Find (GIB_Argv(2))) && GIB_Argv(2)[0])
		Cbuf_Error ("function", "Function %s not found.", GIB_Argv(2));
	else if (GIB_Event_Register (GIB_Argv(1), func))
		Cbuf_Error ("event", "Event %s not found.", GIB_Argv(1));
}

/* File access */

int (*GIB_File_Transform_Path) (dstring_t *path) = NULL;

static int
GIB_File_Transform_Path_Null (dstring_t *path)
{
	char *s;
	
	// Convert backslash to forward slash
	for (s = strchr(path->str, '\\'); s; s = strchr (s, '\\'))
		*s = '/';
	return 0;
}

static int
GIB_File_Transform_Path_Secure (dstring_t *path)
{
	char *s /* , e_dir[MAX_OSPATH] */;
	
	for (s = strchr(path->str, '\\'); s; s = strchr (s, '\\'))
		*s = '/';
	if (Sys_PathType (path->str) != PATHTYPE_RELATIVE_BELOW)
		return -1;
	/* Qexpand_squiggle (fs_userpath->string, e_dir); */
	dstring_insertstr (path, 0, "/");
	dstring_insertstr (path, 0, /* e_dir */ com_gamedir);
	return 0;
}
		
static void
GIB_File_Read_f (void)
{
	QFile      *file;
	char       *path;
	int        len;
	dstring_t  *ret;

	if (GIB_Argc () != 2) {
		GIB_USAGE ("file");
		return;
	}
	if (!*GIB_Argv (1)) {
		Cbuf_Error ("file",
		  "file::read: null filename provided");
		return;
	}
	if (GIB_File_Transform_Path (GIB_Argd(1))) {
		Cbuf_Error ("access",
		  "file::read: access to %s denied", GIB_Argv(1));
		return;
	}
	if (!(ret = GIB_Return (0)))
		return;
	path = GIB_Argv (1);
	file = Qopen (path, "r");
	if (file) {
		len = Qfilesize (file);
		ret->size = len+1;
		dstring_adjust (ret);
		Qread (file, ret->str, len);
		ret->str[len] = 0;
		Qclose (file);
	}
	if (!file) {
		Cbuf_Error ("file",
		  "file::read: could not read %s: %s", path, strerror (errno));
		return;
	}
}

static void
GIB_File_Write_f (void)
{
	QFile      *file;
	char       *path;
	
	if (GIB_Argc () != 3) {
		GIB_USAGE ("file data");
		return;
	}
	if (!*GIB_Argv(1)) {
		Cbuf_Error ("file",
		  "file::write: null filename provided");
		return;
	}
	if (GIB_File_Transform_Path (GIB_Argd(1))) {
		Cbuf_Error ("access",
		  "file::write: access to %s denied", GIB_Argv(1));
		return;
	}
	path = GIB_Argv(1);
	if (!(file = Qopen (path, "w"))) {
		Cbuf_Error ("file",
		  "file::write: could not open %s for writing: %s", path, strerror (errno));
		return;
	}
	Qprintf (file, "%s", GIB_Argv (2));
	Qclose (file);
}

static void
GIB_File_Find_f (void)
{
	DIR        *directory;
	struct dirent *entry;
	const char       *path, *glob = 0;
	char *s;

	if (GIB_Argc () != 2) {
		GIB_USAGE ("glob");
		return;
	}
	if (GIB_File_Transform_Path (GIB_Argd(1))) {
		Cbuf_Error ("access", 
		  "file::find: access to %s denied", GIB_Argv(1));
		return;
	}
	path = GIB_Argv(1);
	s = strrchr (path, '/');
	if (!s) { // No slash in path
		glob = path; // The glob is the entire argument
		path = "."; // The path is the current directory
	} else if (s == path) // Unix filesystem root (carne only)
		path = "/";
	else {
		*s = 0; // Split the string at the final slash
		glob = s+1;
	}
	directory = opendir (path);
	if (!directory) {
		Cbuf_Error ("file",
		  "file.find: could not open directory %s: %s", path, strerror (errno));
		return;
	}
	while ((entry = readdir (directory)))
		if (strcmp (entry->d_name, ".") && strcmp (entry->d_name, "..") && !fnmatch (glob, entry->d_name, 0))
			GIB_Return (entry->d_name);
	closedir (directory);
}

static void
GIB_File_Move_f (void)
{
	char *path1, *path2;

	if (GIB_Argc () != 3) {
		GIB_USAGE ("from_file to_file");
		return;
	}
	if (GIB_File_Transform_Path (GIB_Argd(1))) {
		Cbuf_Error ("access",
		  "file::move: access to %s denied", GIB_Argv(1));
		return;
	}
	if (GIB_File_Transform_Path (GIB_Argd(2))) {
		Cbuf_Error ("access",
		  "file::move: access to %s denied", GIB_Argv(2));
		return;
	}
	path1 = GIB_Argv(1);
	path2 = GIB_Argv(2);
	if (Qrename (path1, path2))
		Cbuf_Error ("file",
		  "file::move: could not move %s to %s: %s",
	      path1, path2, strerror(errno));
}

static void
GIB_File_Delete_f (void)
{
	char *path;

	if (GIB_Argc () != 2) {
		GIB_USAGE ("file");
		return;
	}
	if (GIB_File_Transform_Path (GIB_Argd(1))) {
		Cbuf_Error ("access",
		  "file::delete: access to %s denied", GIB_Argv(1));
		return;
	}
	path = GIB_Argv (1);
	if (Qremove(path))
		Cbuf_Error ("file",
		  "file::delete: could not delete %s: %s",
	      path, strerror(errno));
}

static void
GIB_Range_f (void)
{
	double i, inc, start, limit;
	dstring_t *dstr;
	if (GIB_Argc () < 3 || GIB_Argc () > 4) {
		GIB_USAGE ("lower upper [step]");
		return;
	}
	limit = atof(GIB_Argv(2));
	start = atof(GIB_Argv(1));
	if (GIB_Argc () == 4 && (inc = atof(GIB_Argv(3))) == 0.0)
		return;
	else
		inc = limit < start ? -1.0 : 1.0;
	for (i = atof(GIB_Argv(1)); inc < 0 ? i >= limit : i <= limit; i += inc) {
		if (!(dstr = GIB_Return(0)))
			return;
		dsprintf(dstr, "%.10g", i);
	}
}

static void
GIB_Print_f (void)
{
	if (GIB_Argc() != 2) {
		GIB_USAGE ("text");
		return;
	}
	Sys_Printf ("%s", GIB_Argv(1));
}

static void
GIB_Random_f (void)
{
	dstring_t *ret;
	if (GIB_Argc() != 1)
		GIB_USAGE ("");
	else if ((ret = GIB_Return (0)))
		dsprintf (ret, "%.10g", (double) random()/(double) RAND_MAX);
}

void
GIB_Builtin_Init (qboolean sandbox)
{

	if (sandbox)
		GIB_File_Transform_Path = GIB_File_Transform_Path_Secure;
	else
		GIB_File_Transform_Path = GIB_File_Transform_Path_Null;
	
	GIB_Builtin_Add ("function", GIB_Function_f);
	GIB_Builtin_Add ("function::get", GIB_Function_Get_f);
	GIB_Builtin_Add ("function::export", GIB_Function_Export_f);
	GIB_Builtin_Add ("local", GIB_Local_f);
	GIB_Builtin_Add ("global", GIB_Global_f);
	GIB_Builtin_Add ("domain", GIB_Domain_f);
	GIB_Builtin_Add ("return", GIB_Return_f);
	GIB_Builtin_Add ("for", GIB_For_f);
	GIB_Builtin_Add ("break", GIB_Break_f);
	GIB_Builtin_Add ("continue", GIB_Continue_f);
	GIB_Builtin_Add ("string::length", GIB_String_Length_f);
	GIB_Builtin_Add ("string::equal", GIB_String_Equal_f);
	GIB_Builtin_Add ("string::sub", GIB_String_Sub_f);
	GIB_Builtin_Add ("string::find", GIB_String_Find_f);
	GIB_Builtin_Add ("regex::match", GIB_Regex_Match_f);
	GIB_Builtin_Add ("regex::replace", GIB_Regex_Replace_f);
	GIB_Builtin_Add ("regex::extract", GIB_Regex_Extract_f);
	GIB_Builtin_Add ("thread::create", GIB_Thread_Create_f);
	GIB_Builtin_Add ("thread::kill", GIB_Thread_Kill_f);
	GIB_Builtin_Add ("event::register", GIB_Event_Register_f);
	GIB_Builtin_Add ("file::read", GIB_File_Read_f);
	GIB_Builtin_Add ("file::write", GIB_File_Write_f);
	GIB_Builtin_Add ("file::find", GIB_File_Find_f);
	GIB_Builtin_Add ("file::move", GIB_File_Move_f);
	GIB_Builtin_Add ("file::delete", GIB_File_Delete_f);
	GIB_Builtin_Add ("range", GIB_Range_f);
	GIB_Builtin_Add ("print", GIB_Print_f);
	GIB_Builtin_Add ("random", GIB_Random_f);
}
