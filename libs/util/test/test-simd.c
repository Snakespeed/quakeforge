#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "QF/mathlib.h"
#include "QF/simd/vec4d.h"
#include "QF/simd/vec4f.h"
#include "QF/simd/mat4f.h"

#define right   { 1, 0, 0 }
#define forward { 0, 1, 0 }
#define up      { 0, 0, 1 }
#define one     { 1, 1, 1, 1 }
#define half    { 0.5, 0.5, 0.5, 0.5 }
#define zero    { 0, 0, 0, 0 }
#define qident  { 0, 0, 0, 1 }
#define qtest   { 0.64, 0.48, 0, 0.6 }

#define nright   { -1,  0,  0 }
#define nforward {  0, -1,  0 }
#define nup      {  0,  0, -1 }
#define none     { -1, -1, -1, -1 }
#define nqident  {  0,  0,  0, -1 }

#define identity \
	{	{ 1, 0, 0, 0 }, \
		{ 0, 1, 0, 0 }, \
		{ 0, 0, 1, 0 }, \
		{ 0, 0, 0, 1 } }
#define rotate120 \
	{	{ 0, 1, 0, 0 }, \
		{ 0, 0, 1, 0 }, \
		{ 1, 0, 0, 0 }, \
		{ 0, 0, 0, 1 } }
#define rotate240 \
	{	{ 0, 0, 1, 0 }, \
		{ 1, 0, 0, 0 }, \
		{ 0, 1, 0, 0 }, \
		{ 0, 0, 0, 1 } }

#define s05 0.70710678118654757

typedef  struct {
	vec4d_t   (*op) (vec4d_t a, vec4d_t b);
	vec4d_t     a;
	vec4d_t     b;
	vec4d_t     expect;
	vec4d_t     ulp_errors;
} vec4d_test_t;

typedef  struct {
	vec4f_t   (*op) (vec4f_t a, vec4f_t b);
	vec4f_t     a;
	vec4f_t     b;
	vec4f_t     expect;
	vec4f_t     ulp_errors;
} vec4f_test_t;

typedef  struct {
	void   (*op) (mat4f_t c, const mat4f_t a, const mat4f_t b);
	mat4f_t     a;
	mat4f_t     b;
	mat4f_t     expect;
	mat4f_t     ulp_errors;
} mat4f_test_t;

typedef  struct {
	vec4f_t   (*op) (const mat4f_t a, vec4f_t b);
	mat4f_t     a;
	vec4f_t     b;
	vec4f_t     expect;
	vec4f_t     ulp_errors;
} mv4f_test_t;

typedef  struct {
	void   (*op) (mat4f_t m, vec4f_t q);
	vec4f_t     q;
	mat4f_t     expect;
	mat4f_t     ulp_errors;
} mq4f_test_t;

static vec4d_t tvtruncd (vec4d_t v, vec4d_t ignore)
{
	return vtruncd (v);
}

static vec4d_t tvceild (vec4d_t v, vec4d_t ignore)
{
	return vceild (v);
}

static vec4d_t tvfloord (vec4d_t v, vec4d_t ignore)
{
	return vfloord (v);
}

static vec4d_t tqconjd (vec4d_t v, vec4d_t ignore)
{
	return qconjd (v);
}

static vec4f_t tvtruncf (vec4f_t v, vec4f_t ignore)
{
	return vtruncf (v);
}

static vec4f_t tvceilf (vec4f_t v, vec4f_t ignore)
{
	return vceilf (v);
}

static vec4f_t tvfloorf (vec4f_t v, vec4f_t ignore)
{
	return vfloorf (v);
}

static vec4f_t tqconjf (vec4f_t v, vec4f_t ignore)
{
	return qconjf (v);
}

static vec4d_test_t vec4d_tests[] = {
	// 3D dot products
	{ dotd, right,   right,   one  },
	{ dotd, right,   forward, zero },
	{ dotd, right,   up,      zero },
	{ dotd, forward, right,   zero },
	{ dotd, forward, forward, one  },
	{ dotd, forward, up,      zero },
	{ dotd, up,      right,   zero },
	{ dotd, up,      forward, zero },
	{ dotd, up,      up,      one  },

	// one is 4D, so its self dot product is 4
	{ dotd, one,     one,     { 4,  4,  4,  4} },
	{ dotd, one,    none,     {-4, -4, -4, -4} },

	// 3D cross products
	{ crossd, right,   right,    zero    },
	{ crossd, right,   forward,  up      },
	{ crossd, right,   up,      nforward },
	{ crossd, forward, right,   nup      },
	{ crossd, forward, forward,  zero    },
	{ crossd, forward, up,       right   },
	{ crossd, up,      right,    forward },
	{ crossd, up,      forward, nright   },
	{ crossd, up,      up,       zero    },
	// double whammy tests: cross product with an angled vector and
	// ensuring that a 4d vector (non-zero w component) does not affect
	// the result, including the result's w component remaining zero.
	{ crossd, right,   one,     { 0, -1,  1} },
	{ crossd, forward, one,     { 1,  0, -1} },
	{ crossd, up,      one,     {-1,  1,  0} },
	{ crossd, one,     right,   { 0,  1, -1} },
	{ crossd, one,     forward, {-1,  0,  1} },
	{ crossd, one,     up,      { 1, -1,  0} },
	// This one fails when optimizing with -mfma (which is why fma is not
	// used): ulp errors in z and w
	{ crossd, qtest,   qtest,   {0, 0, 0, 0} },

	{ qmuld, qident,  qident,   qident  },
	{ qmuld, qident,  right,    right   },
	{ qmuld, qident,  forward,  forward },
	{ qmuld, qident,  up,       up      },
	{ qmuld, right,   qident,   right   },
	{ qmuld, forward, qident,   forward },
	{ qmuld, up,      qident,   up      },
	{ qmuld, right,   right,   nqident  },
	{ qmuld, right,   forward,  up      },
	{ qmuld, right,   up,      nforward },
	{ qmuld, forward, right,   nup      },
	{ qmuld, forward, forward, nqident  },
	{ qmuld, forward, up,       right   },
	{ qmuld, up,      right,    forward },
	{ qmuld, up,      forward, nright   },
	{ qmuld, up,      up,      nqident  },
	{ qmuld, one,     one,     { 2, 2, 2, -2 } },
	{ qmuld, one,     { 2, 2, 2, -2 }, { 0, 0, 0, -8 } },
	// This one fails when optimizing with -mfma (which is why fma is not
	// used): ulp error in z
	{ qmuld, qtest,   qtest,   {0.768, 0.576, 0, -0.28} },

	// The one vector is not unit (magnitude 2), so using it as a rotation
	// quaternion results in scaling by 4. However, it still has the effect
	// of rotating 120 degrees around the axis equidistant from the three
	// orthogonal axes such that x->y->z->x
	{ qvmuld, one,    right,     { 0, 4, 0, 0 } },
	{ qvmuld, one,    forward,   { 0, 0, 4, 0 } },
	{ qvmuld, one,    up,        { 4, 0, 0, 0 } },
	{ qvmuld, one,    {1,1,1,0}, { 4, 4, 4, 0 } },
	{ qvmuld, one,    one,       { 4, 4, 4, -2 } },
	// inverse rotation, so x->z->y->x
	{ vqmuld, right,     one,    { 0, 0, 4, 0 } },
	{ vqmuld, forward,   one,    { 4, 0, 0, 0 } },
	{ vqmuld, up,        one,    { 0, 4, 0, 0 } },
	{ vqmuld, {1,1,1,0}, one,    { 4, 4, 4, 0 } },
	{ vqmuld, one,       one,    { 4, 4, 4, -2 } },
	// The half vector is unit.
	{ qvmuld, half,   right,     forward },
	{ qvmuld, half,   forward,   up      },
	{ qvmuld, half,   up,        right   },
	{ qvmuld, half,   {1,1,1,0}, { 1, 1, 1, 0 } },
	// inverse
	{ vqmuld, right,     half,   up      },
	{ vqmuld, forward,   half,   right   },
	{ vqmuld, up,        half,   forward },
	{ vqmuld, {1,1,1,0}, half,   { 1, 1, 1, 0 } },
	// one is a 4D vector and qvmuld is meant for 3D vectors. However, it
	// seems that the vector's w has no effect on the 3d portion of the
	// result, but the result's w is cosine of the full rotation angle
	// scaled by quaternion magnitude and vector w
	{ qvmuld, half,   one,       { 1, 1, 1, -0.5 } },
	{ qvmuld, half,   {2,2,2,2}, { 2, 2, 2, -1 } },
	{ qvmuld, qtest,  right,     {0.5392, 0.6144, -0.576, 0} },
	{ qvmuld, qtest,  forward,   {0.6144, 0.1808, 0.768, 0},
	                             {0, -2.7e-17, 0, 0} },
	{ qvmuld, qtest,  up,        {0.576, -0.768, -0.28, 0} },
	// inverse
	{ vqmuld, one,       half,   { 1, 1, 1, -0.5 } },
	{ vqmuld, {2,2,2,2}, half,   { 2, 2, 2, -1 } },
	{ vqmuld, right,     qtest,  {0.5392, 0.6144, 0.576, 0} },
	{ vqmuld, forward,   qtest,  {0.6144, 0.1808, -0.768, 0},
	                             {0, -2.7e-17, 0, 0} },
	{ vqmuld, up,        qtest,  {-0.576, 0.768, -0.28, 0} },

	{ qrotd, right,   right,    qident },
	{ qrotd, right,   forward,  {    0,    0,  s05,  s05 },
	                            {0, 0, -1.1e-16, 0} },
	{ qrotd, right,   up,       {    0, -s05,    0,  s05 },
	                            {0, 1.1e-16, 0, 0} },
	{ qrotd, forward, right,    {    0,    0, -s05,  s05 },
	                            {0, 0, 1.1e-16, 0} },
	{ qrotd, forward, forward,  qident },
	{ qrotd, forward, up,       {  s05,    0,    0,  s05 },
	                            {-1.1e-16, 0, 0, 0} },
	{ qrotd, up,      right,    {    0,  s05,    0,  s05 },
	                            {0, -1.1e-16, 0, 0} },
	{ qrotd, up,      forward,  { -s05,    0,    0,  s05 },
	                            { 1.1e-16, 0, 0, 0} },
	{ qrotd, up,      up,       qident },

	{ tvtruncd, { 1.1, 2.9, -1.1, -2.9 }, {}, { 1, 2, -1, -2 } },
	{ tvceild,  { 1.1, 2.9, -1.1, -2.9 }, {}, { 2, 3, -1, -2 } },
	{ tvfloord, { 1.1, 2.9, -1.1, -2.9 }, {}, { 1, 2, -2, -3 } },
	{ tqconjd,  one, {}, { -1, -1, -1, 1 } },
};
#define num_vec4d_tests (sizeof (vec4d_tests) / (sizeof (vec4d_tests[0])))

static vec4f_test_t vec4f_tests[] = {
	// 3D dot products
	{ dotf, right,   right,   one  },
	{ dotf, right,   forward, zero },
	{ dotf, right,   up,      zero },
	{ dotf, forward, right,   zero },
	{ dotf, forward, forward, one  },
	{ dotf, forward, up,      zero },
	{ dotf, up,      right,   zero },
	{ dotf, up,      forward, zero },
	{ dotf, up,      up,      one  },

	// one is 4D, so its self dot product is 4
	{ dotf, one,     one,     { 4,  4,  4,  4} },
	{ dotf, one,    none,     {-4, -4, -4, -4} },

	// 3D cross products
	{ crossf, right,   right,    zero    },
	{ crossf, right,   forward,  up      },
	{ crossf, right,   up,      nforward },
	{ crossf, forward, right,   nup      },
	{ crossf, forward, forward,  zero    },
	{ crossf, forward, up,       right   },
	{ crossf, up,      right,    forward },
	{ crossf, up,      forward, nright   },
	{ crossf, up,      up,       zero    },
	// double whammy tests: cross product with an angled vector and
	// ensuring that a 4d vector (non-zero w component) does not affect
	// the result, including the result's w component remaining zero.
	{ crossf, right,   one,     { 0, -1,  1} },
	{ crossf, forward, one,     { 1,  0, -1} },
	{ crossf, up,      one,     {-1,  1,  0} },
	{ crossf, one,     right,   { 0,  1, -1} },
	{ crossf, one,     forward, {-1,  0,  1} },
	{ crossf, one,     up,      { 1, -1,  0} },
	{ crossf, qtest,   qtest,   {0, 0, 0, 0} },

	{ qmulf, qident,  qident,   qident  },
	{ qmulf, qident,  right,    right   },
	{ qmulf, qident,  forward,  forward },
	{ qmulf, qident,  up,       up      },
	{ qmulf, right,   qident,   right   },
	{ qmulf, forward, qident,   forward },
	{ qmulf, up,      qident,   up      },
	{ qmulf, right,   right,   nqident  },
	{ qmulf, right,   forward,  up      },
	{ qmulf, right,   up,      nforward },
	{ qmulf, forward, right,   nup      },
	{ qmulf, forward, forward, nqident  },
	{ qmulf, forward, up,       right   },
	{ qmulf, up,      right,    forward },
	{ qmulf, up,      forward, nright   },
	{ qmulf, up,      up,      nqident  },
	{ qmulf, one,     one,     { 2, 2, 2, -2 } },
	{ qmulf, one,     { 2, 2, 2, -2 }, { 0, 0, 0, -8 } },
	{ qmulf, qtest,   qtest,   {0.768, 0.576, 0, -0.28},
	                           {0, 6e-8, 0, 3e-8} },

	// The one vector is not unit (magnitude 2), so using it as a rotation
	// quaternion results in scaling by 4. However, it still has the effect
	// of rotating 120 degrees around the axis equidistant from the three
	// orthogonal axes such that x->y->z->x
	{ qvmulf, one,    right,     { 0, 4, 0, 0 } },
	{ qvmulf, one,    forward,   { 0, 0, 4, 0 } },
	{ qvmulf, one,    up,        { 4, 0, 0, 0 } },
	{ qvmulf, one,    {1,1,1,0}, { 4, 4, 4, 0 } },
	{ qvmulf, one,    one,       { 4, 4, 4, -2 } },
	// inverse rotation, so x->z->y->x
	{ vqmulf, right,     one,    { 0, 0, 4, 0 } },
	{ vqmulf, forward,   one,    { 4, 0, 0, 0 } },
	{ vqmulf, up,        one,    { 0, 4, 0, 0 } },
	{ vqmulf, {1,1,1,0}, one,    { 4, 4, 4, 0 } },
	{ vqmulf, one,       one,    { 4, 4, 4, -2 } },
	//
	{ qvmulf, qtest,  right,     {0.5392, 0.6144, -0.576, 0},
	                             {0, -5.9e-8, -6e-8, 0} },
	{ qvmulf, qtest,  forward,   {0.6144, 0.1808, 0.768, 0},
	                             {-5.9e-8, 1.5e-8, 0, 0} },
	{ qvmulf, qtest,  up,        {0.576, -0.768, -0.28, 0},
	                             {6e-8, 0, 3e-8, 0} },
	{ vqmulf, right,     qtest,  {0.5392, 0.6144, 0.576, 0},
	                             {0, -5.9e-8, 5.9e-8, 0} },
	{ vqmulf, forward,   qtest,  {0.6144, 0.1808, -0.768, 0},
	                             {-5.9e-8, 1.5e-8, 0, 0} },
	{ vqmulf, up,        qtest,  {-0.576, 0.768, -0.28, 0},
	                             {-5.9e-8, 0, 3e-8, 0} },

	{ qrotf, right,   right,    qident },
	{ qrotf, right,   forward,  {    0,    0,  s05,  s05 } },
	{ qrotf, right,   up,       {    0, -s05,    0,  s05 } },
	{ qrotf, forward, right,    {    0,    0, -s05,  s05 } },
	{ qrotf, forward, forward,  qident },
	{ qrotf, forward, up,       {  s05,    0,    0,  s05 } },
	{ qrotf, up,      right,    {    0,  s05,    0,  s05 } },
	{ qrotf, up,      forward,  { -s05,    0,    0,  s05 } },
	{ qrotf, up,      up,       qident },

	{ tvtruncf, { 1.1, 2.9, -1.1, -2.9 }, {}, { 1, 2, -1, -2 } },
	{ tvceilf,  { 1.1, 2.9, -1.1, -2.9 }, {}, { 2, 3, -1, -2 } },
	{ tvfloorf, { 1.1, 2.9, -1.1, -2.9 }, {}, { 1, 2, -2, -3 } },
	{ tqconjf,  one, {}, { -1, -1, -1, 1 } },
};
#define num_vec4f_tests (sizeof (vec4f_tests) / (sizeof (vec4f_tests[0])))

static mat4f_test_t mat4f_tests[] = {
	{ mmulf, identity, identity, identity },
	{ mmulf, rotate120, identity, rotate120 },
	{ mmulf, identity, rotate120, rotate120 },
	{ mmulf, rotate120, rotate120, rotate240 },
	{ mmulf, rotate120, rotate240, identity },
	{ mmulf, rotate240, rotate120, identity },
};
#define num_mat4f_tests (sizeof (mat4f_tests) / (sizeof (mat4f_tests[0])))

static mv4f_test_t mv4f_tests[] = {
	{ mvmulf, identity, { 1, 0, 0, 0 }, { 1, 0, 0, 0 } },
	{ mvmulf, identity, { 0, 1, 0, 0 }, { 0, 1, 0, 0 } },
	{ mvmulf, identity, { 0, 0, 1, 0 }, { 0, 0, 1, 0 } },
	{ mvmulf, identity, { 0, 0, 0, 1 }, { 0, 0, 0, 1 } },
	{ mvmulf, rotate120, { 1, 2, 3, 4 }, { 3, 1, 2, 4 } },
	{ mvmulf, rotate240, { 1, 2, 3, 4 }, { 2, 3, 1, 4 } },
};
#define num_mv4f_tests (sizeof (mv4f_tests) / (sizeof (mv4f_tests[0])))

// expect filled in using non-simd QuatToMatrix (has its own tests)
static mq4f_test_t mq4f_tests[] = {
	{ mat4fquat, { 0, 0, 0, 1 } },
	{ mat4fquat, {  0.5,  0.5,  0.5, 0.5 } },
	{ mat4fquat, {  0.5,  0.5, -0.5, 0.5 } },
	{ mat4fquat, {  0.5, -0.5,  0.5, 0.5 } },
	{ mat4fquat, {  0.5, -0.5, -0.5, 0.5 } },
	{ mat4fquat, { -0.5,  0.5,  0.5, 0.5 } },
	{ mat4fquat, { -0.5,  0.5, -0.5, 0.5 } },
	{ mat4fquat, { -0.5, -0.5,  0.5, 0.5 } },
	{ mat4fquat, { -0.5, -0.5, -0.5, 0.5 } },
};
#define num_mq4f_tests (sizeof (mq4f_tests) / (sizeof (mq4f_tests[0])))

static int
run_vec4d_tests (void)
{
	int         ret = 0;

	for (size_t i = 0; i < num_vec4d_tests; i++) {
		__auto_type test = &vec4d_tests[i];
		vec4d_t     result = test->op (test->a, test->b);
		vec4d_t     expect = test->expect + test->ulp_errors;
		vec4l_t     res = result != expect;
		if (res[0] || res[1] || res[2] || res[3]) {
			ret |= 1;
			printf ("\nrun_vec4d_tests %zd\n", i);
			printf ("a: " VEC4D_FMT "\n", VEC4_EXP(test->a));
			printf ("b: " VEC4D_FMT "\n", VEC4_EXP(test->b));
			printf ("r: " VEC4D_FMT "\n", VEC4_EXP(result));
			printf ("t: " VEC4L_FMT "\n", VEC4_EXP(res));
			printf ("E: " VEC4D_FMT "\n", VEC4_EXP(expect));
			printf ("e: " VEC4D_FMT "\n", VEC4_EXP(test->expect));
			printf ("u: " VEC4D_FMT "\n", VEC4_EXP(test->ulp_errors));
		}
	}
	return ret;
}

static int
run_vec4f_tests (void)
{
	int         ret = 0;

	for (size_t i = 0; i < num_vec4f_tests; i++) {
		__auto_type test = &vec4f_tests[i];
		vec4f_t     result = test->op (test->a, test->b);
		vec4f_t     expect = test->expect + test->ulp_errors;
		vec4i_t     res = result != expect;
		if (res[0] || res[1] || res[2] || res[3]) {
			ret |= 1;
			printf ("\nrun_vec4f_tests %zd\n", i);
			printf ("a: " VEC4F_FMT "\n", VEC4_EXP(test->a));
			printf ("b: " VEC4F_FMT "\n", VEC4_EXP(test->b));
			printf ("r: " VEC4F_FMT "\n", VEC4_EXP(result));
			printf ("t: " VEC4I_FMT "\n", VEC4_EXP(res));
			printf ("E: " VEC4F_FMT "\n", VEC4_EXP(expect));
			printf ("e: " VEC4F_FMT "\n", VEC4_EXP(test->expect));
			printf ("u: " VEC4F_FMT "\n", VEC4_EXP(test->ulp_errors));
		}
	}
	return ret;
}

static int
run_mat4f_tests (void)
{
	int         ret = 0;

	for (size_t i = 0; i < num_mat4f_tests; i++) {
		__auto_type test = &mat4f_tests[i];
		mat4f_t     result;
		mat4f_t     expect;
		mat4i_t     res = {};

		test->op (result, test->a, test->b);
		maddf (expect, test->expect, test->ulp_errors);

		int         fail = 0;
		for (int j = 0; j < 4; j++) {
			res[j] = result[j] != expect[j];
			fail |= res[j][0] || res[j][1] || res[j][2] || res[j][3];
		}
		if (fail) {
			ret |= 1;
			printf ("\nrun_mat4f_tests %zd\n", i);
			printf ("a: " VEC4F_FMT "\n", MAT4_ROW(test->a, 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->a, 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->a, 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->a, 3));
			printf ("b: " VEC4F_FMT "\n", MAT4_ROW(test->b, 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->b, 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->b, 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->b, 3));
			printf ("r: " VEC4F_FMT "\n", MAT4_ROW(result, 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(result, 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(result, 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(result, 3));
			printf ("t: " VEC4I_FMT "\n", MAT4_ROW(res, 0));
			printf ("   " VEC4I_FMT "\n", MAT4_ROW(res, 1));
			printf ("   " VEC4I_FMT "\n", MAT4_ROW(res, 2));
			printf ("   " VEC4I_FMT "\n", MAT4_ROW(res, 3));
			printf ("E: " VEC4F_FMT "\n", MAT4_ROW(expect, 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(expect, 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(expect, 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(expect, 3));
			printf ("e: " VEC4F_FMT "\n", MAT4_ROW(test->expect, 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->expect, 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->expect, 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->expect, 3));
			printf ("u: " VEC4F_FMT "\n", MAT4_ROW(test->ulp_errors, 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->ulp_errors, 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->ulp_errors, 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->ulp_errors, 3));
		}
	}
	return ret;
}

static int
run_mv4f_tests (void)
{
	int         ret = 0;

	for (size_t i = 0; i < num_mv4f_tests; i++) {
		__auto_type test = &mv4f_tests[i];
		vec4f_t     result = test->op (test->a, test->b);
		vec4f_t     expect = test->expect + test->ulp_errors;
		vec4i_t     res = result != expect;

		if (res[0] || res[1] || res[2] || res[3]) {
			ret |= 1;
			printf ("\nrun_mv4f_tests %zd\n", i);
			printf ("a: " VEC4F_FMT "\n", MAT4_ROW(test->a, 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->a, 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->a, 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->a, 3));
			printf ("b: " VEC4F_FMT "\n", VEC4_EXP(test->b));
			printf ("r: " VEC4F_FMT "\n", VEC4_EXP(result));
			printf ("t: " VEC4I_FMT "\n", VEC4_EXP(res));
			printf ("E: " VEC4F_FMT "\n", VEC4_EXP(expect));
			printf ("e: " VEC4F_FMT "\n", VEC4_EXP(test->expect));
			printf ("u: " VEC4F_FMT "\n", VEC4_EXP(test->ulp_errors));
		}
	}
	return ret;
}

static int
run_mq4f_tests (void)
{
	int         ret = 0;

	for (size_t i = 0; i < num_mq4f_tests; i++) {
		__auto_type test = &mq4f_tests[i];
		quat_t      q;
		vec_t       m[16];
		memcpy (q, &test->q, sizeof (quat_t));
		QuatToMatrix (q, m, 1, 1);
		memcpy (&test->expect, m, sizeof (mat4f_t));
	}
	for (size_t i = 0; i < num_mq4f_tests; i++) {
		__auto_type test = &mq4f_tests[i];
		mat4f_t     result;
		mat4f_t     expect;
		mat4i_t     res = {};

		test->op (result, test->q);
		maddf (expect, test->expect, test->ulp_errors);
		memcpy (expect, (void *) &test->expect, sizeof (mat4f_t));

		int         fail = 0;
		for (int j = 0; j < 4; j++) {
			res[j] = result[j] != expect[j];
			fail |= res[j][0] || res[j][1] || res[j][2] || res[j][3];
		}
		if (fail) {
			ret |= 1;
			printf ("\nrun_mq4f_tests %zd\n", i);
			printf ("q: " VEC4F_FMT "\n", VEC4_EXP(test->q));
			printf ("r: " VEC4F_FMT "\n", MAT4_ROW(result, 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(result, 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(result, 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(result, 3));
			printf ("t: " VEC4I_FMT "\n", MAT4_ROW(res, 0));
			printf ("   " VEC4I_FMT "\n", MAT4_ROW(res, 1));
			printf ("   " VEC4I_FMT "\n", MAT4_ROW(res, 2));
			printf ("   " VEC4I_FMT "\n", MAT4_ROW(res, 3));
			printf ("E: " VEC4F_FMT "\n", MAT4_ROW(expect, 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(expect, 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(expect, 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(expect, 3));
			printf ("e: " VEC4F_FMT "\n", MAT4_ROW(test->expect, 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->expect, 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->expect, 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->expect, 3));
			printf ("u: " VEC4F_FMT "\n", MAT4_ROW(test->ulp_errors, 0));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->ulp_errors, 1));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->ulp_errors, 2));
			printf ("   " VEC4F_FMT "\n", MAT4_ROW(test->ulp_errors, 3));
		}
	}
	return ret;
}

int
main (void)
{
	int         ret = 0;
	ret |= run_vec4d_tests ();
	ret |= run_vec4f_tests ();
	ret |= run_mat4f_tests ();
	ret |= run_mv4f_tests ();
	ret |= run_mq4f_tests ();
	return ret;
}