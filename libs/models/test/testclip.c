#include <stdlib.h>
#include "QF/va.h"

#include "getopt.h"
#include "world.h"

#undef DIST_EPSILON
#define DIST_EPSILON 0

#ifdef TEST_ID
# include "trace-id.c"
#elif defined(TEST_QF_BAD)
# include "trace-qf-bad.c"
#else
# include "../trace.c"
#endif

//  0,0
//   |\   .
//   |s\  .
//   |ss\ .
//   0   1

mclipnode_t clipnodes_simple_wedge[] = {
	{  0, {             1, CONTENTS_EMPTY}},
	{  1, {CONTENTS_EMPTY, CONTENTS_SOLID}},
};

mplane_t planes_simple_wedge[] = {
	{{1, 0, 0}, 0, 0, 0},		//  0
	{{0.8, 0, 0.6}, 0, 4, 0},	//  1 
};

hull_t hull_simple_wedge = {
	clipnodes_simple_wedge,
	planes_simple_wedge,
	0,
	1,
	{0, 0, 0},
	{0, 0, 0},
};

//    -32  32  48
//  sss|sss|   |sss
//  sss|sss|   |sss
//     0   1   2

mclipnode_t clipnodes_tpp1[] = {
	{  0, {             1, CONTENTS_SOLID}},
	{  1, {             2, CONTENTS_SOLID}},
	{  2, {CONTENTS_SOLID, CONTENTS_EMPTY}},
};

mplane_t planes_tpp1[] = {
	{{1, 0, 0}, -32, 0, 0},
	{{1, 0, 0},  32, 0, 0},
	{{1, 0, 0},  48, 0, 0},
};

hull_t hull_tpp1 = {
	clipnodes_tpp1,
	planes_tpp1,
	0,
	2,
	{0, 0, 0},
	{0, 0, 0},
};

//    -32  32  48
//  sss|sss|   |sss
//  sss|sss|   |sss
//     1   0   2

mclipnode_t clipnodes_tpp2[] = {
	{  0, {             2,              1}},
	{  1, {CONTENTS_SOLID, CONTENTS_SOLID}},
	{  2, {CONTENTS_SOLID, CONTENTS_EMPTY}},
};

mplane_t planes_tpp2[] = {
	{{1, 0, 0},  32, 0, 0},
	{{1, 0, 0}, -32, 0, 0},
	{{1, 0, 0},  48, 0, 0},
};

hull_t hull_tpp2 = {
	clipnodes_tpp2,
	planes_tpp2,
	0,
	2,
	{0, 0, 0},
	{0, 0, 0},
};

//    -32  32  48
//  sss|   |www|sss
//  sss|   |www|sss
//     1   0   2

mclipnode_t clipnodes_tppw[] = {
	{  0, {             2,              1}},
	{  1, {CONTENTS_EMPTY, CONTENTS_SOLID}},
	{  2, {CONTENTS_SOLID, CONTENTS_WATER}},
};

mplane_t planes_tppw[] = {
	{{1, 0, 0},  32, 0, 0},
	{{1, 0, 0}, -32, 0, 0},
	{{1, 0, 0},  48, 0, 0},
};

hull_t hull_tppw = {
	clipnodes_tppw,
	planes_tppw,
	0,
	2,
	{0, 0, 0},
	{0, 0, 0},
};

typedef struct {
	vec3_t      extents;
} box_t;

typedef struct {
	const char *desc;
	box_t      *box;
	hull_t     *hull;
	vec3_t      start;
	vec3_t      end;
	struct {
		float       frac;
		qboolean    allsolid;
		qboolean    startsolid;
		qboolean    inopen;
		qboolean    inwater;
	}           expect;
} test_t;

box_t point = { {0, 0, 0} };
box_t player = { {16, 16, 28} };

test_t tests[] = {
	{"Point, Three parallel planes 1", &point, &hull_tpp1,
		{-64, 0, 0}, { 64, 0, 0}, {     1, 1, 1, 0, 0}},
	{"Point, Three parallel planes 1", &point, &hull_tpp1,
		{  0, 0, 0}, { 40, 0, 0}, {     1, 0, 1, 1, 0}},
	{"Point, Three parallel planes 1", &point, &hull_tpp1,
		{ 40, 0, 0}, {-88, 0, 0}, {0.0625, 0, 0, 1, 0}},
	{"Point, Three parallel planes 1", &point, &hull_tpp1,
		{  0, 0, 0}, { 64, 0, 0}, {  0.75, 0, 1, 1, 0}},
	{"Point, Three parallel planes 1", &point, &hull_tpp1,
		{  0, 0, 0}, {  0, 8, 0}, {     1, 1, 1, 0, 0}},
	{"Point, Three parallel planes 1", &point, &hull_tpp1,
		{ 40, 0, 0}, { 40, 8, 0}, {     1, 0, 0, 1, 0}},

	{"Point, Three parallel planes 2", &point, &hull_tpp2,
		{-64, 0, 0}, { 64, 0, 0}, {     1, 1, 1, 0, 0}},
	{"Point, Three parallel planes 2", &point, &hull_tpp2,
		{  0, 0, 0}, { 40, 0, 0}, {     1, 0, 1, 1, 0}},
	{"Point, Three parallel planes 2", &point, &hull_tpp2,
		{ 40, 0, 0}, {-88, 0, 0}, {0.0625, 0, 0, 1, 0}},
	{"Point, Three parallel planes 2", &point, &hull_tpp2,
		{  0, 0, 0}, { 64, 0, 0}, {  0.75, 0, 1, 1, 0}},

	{"Point, Three parallel planes with water", &point, &hull_tppw,
		{-64, 0, 0}, { 64, 0, 0}, { 0.875, 0, 1, 1, 1}},
	{"Point, Three parallel planes with water", &point, &hull_tppw,
		{  0, 0, 0}, { 40, 0, 0}, {     1, 0, 0, 1, 1}},
	{"Point, Three parallel planes with water", &point, &hull_tppw,
		{ 40, 0, 0}, {-88, 0, 0}, {0.5625, 0, 0, 1, 1}},
	{"Point, Three parallel planes with water", &point, &hull_tppw,
		{  0, 0, 0}, { 64, 0, 0}, {  0.75, 0, 0, 1, 1}},
	{"Point, Three parallel planes with water", &point, &hull_tppw,
		{  0, 0, 0}, {  0, 8, 0}, {     1, 0, 0, 1, 0}},
	{"Point, Three parallel planes with water", &point, &hull_tppw,
		{ 40, 0, 0}, { 40, 8, 0}, {     1, 0, 0, 0, 1}},
	{"Point, Three parallel planes with water", &point, &hull_tppw,
		{  0, 0, 0}, { 40, 0, 0}, {     1, 0, 0, 1, 1}},
};
#define num_tests (sizeof (tests) / sizeof (tests[0]))

static int test_enabled[num_tests] = { 0 };

int verbose = 0;

static trace_t
do_trace (box_t *box, hull_t *hull, vec3_t start, vec3_t end)
{
	trace_t trace;

	trace.allsolid = true;
	trace.startsolid = false;
	trace.inopen = false;
	trace.inwater = false;
	trace.fraction = 1;
	VectorCopy (box->extents, trace.extents);
	trace.isbox = true;
	VectorCopy (end, trace.endpos);
	MOD_TraceLine (hull, 0, start, end, &trace);
	return trace;
}

static int
run_test (test_t *test)
{
	const char *desc;
	vec3_t      end;
	int         res = 0;
	char       *expect;
	char       *got;
	static int  output = 0;

	VectorSubtract (test->end, test->start, end);
	VectorMultAdd (test->start, test->expect.frac, end, end);
	expect = nva ("expect: (%g %g %g) -> (%g %g %g) => (%g %g %g)"
				  " %3g %d %d %d %d",
				  test->start[0], test->start[1], test->start[2],
				  test->end[0], test->end[1], test->end[2],
				  end[0], end[1], end[2],
				  test->expect.frac, 
				  test->expect.allsolid, test->expect.startsolid,
				  test->expect.inopen, test->expect.inwater);
	trace_t trace = do_trace (test->box, test->hull, test->start, test->end);
	got = nva ("   got: (%g %g %g) -> (%g %g %g) => (%g %g %g)"
			   " %3g %d %d %d %d",
			   test->start[0], test->start[1], test->start[2],
			   test->end[0], test->end[1], test->end[2],
			   trace.endpos[0], trace.endpos[1], trace.endpos[2],
			   trace.fraction, 
			   trace.allsolid, trace.startsolid,
			   trace.inopen, trace.inwater);
	if (VectorCompare (end, trace.endpos)
		&& test->expect.frac == trace.fraction
		&& test->expect.allsolid == trace.allsolid
		&& test->expect.startsolid == trace.startsolid
		&& test->expect.inopen == trace.inopen
		&& test->expect.inwater == trace.inwater)
		res = 1;

	if (test->desc)
		desc = va ("(%d) %s", (int)(long)(test - tests), test->desc);
	else
		desc = va ("test #%d", (int)(long)(test - tests));
	if (verbose >= 0 || !res) {
		if (output)
			puts("");
		output = 1;
		puts (expect);
		puts (got);
		printf ("%s: %s\n", res ? "PASS" : "FAIL", desc);
	}
	free (expect);
	free (got);
	return res;
}

int
main (int argc, char **argv)
{
//	vec3_t      start, end;
	int         c;
	size_t      i, test;
	int         pass = 1;

	while ((c = getopt (argc, argv, "qvt:")) != EOF) {
		switch (c) {
			case 'q':
				verbose--;
				break;
			case 'v':
				verbose++;
				break;
			case 't':
				test = atoi (optarg);
				if (test < num_tests) {
					test_enabled[test] = 1;
				} else {
					fprintf (stderr, "Bad test number (0 - %zd)\n", num_tests);
					return 1;
				}
				break;
			default:
				fprintf (stderr, "-q (quiet) -v (verbose) and/or -t TEST "
								 "(test number)\n");
				return 1;
		}
	}

	for (i = 0; i < num_tests; i++)
		if (test_enabled[i])
			break;
	if (i == num_tests) {
		for (i = 0; i < num_tests; i++)
			test_enabled[i] = 1;
	}

	if (verbose > 0)
		printf ("start -> end => stop frac allsolid startsolid inopen "
				"inwater\n");
	for (i = 0; i < num_tests; i++) {
		if (!test_enabled[i])
			continue;
		pass &= run_test (&tests[i]);
	}

	return !pass;
}
