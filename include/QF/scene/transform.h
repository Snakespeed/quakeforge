/*
	transform.h

	Transform management

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/02/26

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

#ifndef __QF_scene_transform_h
#define __QF_scene_transform_h

#include "QF/darray.h"
#include "QF/qtypes.h"
#include "QF/scene/hierarchy.h"
#include "QF/simd/vec4f.h"
#include "QF/simd/mat4f.h"

/** \defgroup transform Transform management
	\ingroup utils
*/
///@{

typedef struct transform_s {
	hierarchy_t *hierarchy;
	struct scene_s *scene;	///< owning scene
	uint32_t    index;	///< index in hierarchy
	int32_t     id;		///< scene id
} transform_t;

#define XFORMINLINE GNU89INLINE inline __attribute__((pure))

transform_t *Transform_New (struct scene_s *scene, transform_t *parent);
/* Deletes all child transforms, and transform names */
void Transform_Delete (transform_t *transform);
transform_t *Transform_NewNamed (struct scene_s *scene, transform_t *parent,
								 const char *name);
XFORMINLINE uint32_t Transform_ChildCount (const transform_t *transform);
XFORMINLINE transform_t *Transform_GetChild (const transform_t *transform,
								 uint32_t childIndex);
void Transform_SetParent (transform_t *transform, transform_t *parent);
XFORMINLINE transform_t *Transform_GetParent (const transform_t *transform);
void Transform_SetName (transform_t *transform, const char *name);
XFORMINLINE const char *Transform_GetName (const transform_t *transform);
void Transform_SetTag (transform_t *transform, uint32_t tag);
XFORMINLINE uint32_t Transform_GetTag (const transform_t *transform);
GNU89INLINE inline void Transform_GetLocalMatrix (const transform_t *transform, mat4f_t mat);
GNU89INLINE inline void Transform_GetLocalInverse (const transform_t *transform, mat4f_t mat);
GNU89INLINE inline void Transform_GetWorldMatrix (const transform_t *transform, mat4f_t mat);
// XXX the pointer may be invalidated by hierarchy updates
XFORMINLINE const vec4f_t *Transform_GetWorldMatrixPtr (const transform_t *transform);
GNU89INLINE inline void Transform_GetWorldInverse (const transform_t *transform, mat4f_t mat);
XFORMINLINE vec4f_t Transform_GetLocalPosition (const transform_t *transform);
void Transform_SetLocalPosition (transform_t *transform, vec4f_t position);
XFORMINLINE vec4f_t Transform_GetLocalRotation (const transform_t *transform);
void Transform_SetLocalRotation (transform_t *transform, vec4f_t rotation);
XFORMINLINE vec4f_t Transform_GetLocalScale (const transform_t *transform);
void Transform_SetLocalScale (transform_t *transform, vec4f_t scale);
XFORMINLINE vec4f_t Transform_GetWorldPosition (const transform_t *transform);
void Transform_SetWorldPosition (transform_t *transform, vec4f_t position);
XFORMINLINE vec4f_t Transform_GetWorldRotation (const transform_t *transform);
void Transform_SetWorldRotation (transform_t *transform, vec4f_t rotation);
XFORMINLINE vec4f_t Transform_GetWorldScale (const transform_t *transform);
void Transform_SetLocalTransform (transform_t *transform, vec4f_t scale,
								  vec4f_t rotation, vec4f_t position);
// NOTE: these use X: forward, -Y: right, Z:up
// aslo, not guaranteed to be normalized or even orthogonal
XFORMINLINE vec4f_t Transform_Forward (const transform_t *transform);
XFORMINLINE vec4f_t Transform_Right (const transform_t *transform);
XFORMINLINE vec4f_t Transform_Up (const transform_t *transform);
// no SetWorldScale because after rotations, non uniform scale becomes shear

#undef XFORMINLINE
#ifndef IMPLEMENT_TRANSFORM_Funcs
#define XFORMINLINE GNU89INLINE inline
#else
#define XFORMINLINE VISIBLE
#endif

XFORMINLINE
uint32_t
Transform_ChildCount (const transform_t *transform)
{
	hierarchy_t *h = transform->hierarchy;
	return h->childCount.a[transform->index];
}

XFORMINLINE
transform_t *
Transform_GetChild (const transform_t *transform, uint32_t childIndex)
{
	hierarchy_t *h = transform->hierarchy;
	if (childIndex >= h->childCount.a[transform->index]) {
		return 0;
	}
	return h->transform.a[h->childIndex.a[transform->index] + childIndex];
}

XFORMINLINE
transform_t *
Transform_GetParent (const transform_t *transform)
{
	if (transform->index == 0) {
		return 0;
	}
	hierarchy_t *h = transform->hierarchy;
	return h->transform.a[h->parentIndex.a[transform->index]];
}

XFORMINLINE
const char *
Transform_GetName (const transform_t *transform)
{
	hierarchy_t *h = transform->hierarchy;
	return h->name.a[transform->index];
}

XFORMINLINE
uint32_t
Transform_GetTag (const transform_t *transform)
{
	hierarchy_t *h = transform->hierarchy;
	return h->tag.a[transform->index];
}

XFORMINLINE
void
Transform_GetLocalMatrix (const transform_t *transform, mat4f_t mat)
{
	hierarchy_t *h = transform->hierarchy;
	vec4f_t     *src = h->localMatrix.a[transform->index];
	mat[0] = src[0];
	mat[1] = src[1];
	mat[2] = src[2];
	mat[3] = src[3];
}

XFORMINLINE
void
Transform_GetLocalInverse (const transform_t *transform, mat4f_t mat)
{
	hierarchy_t *h = transform->hierarchy;
	vec4f_t     *src = h->localInverse.a[transform->index];
	mat[0] = src[0];
	mat[1] = src[1];
	mat[2] = src[2];
	mat[3] = src[3];
}

XFORMINLINE
void
Transform_GetWorldMatrix (const transform_t *transform, mat4f_t mat)
{
	hierarchy_t *h = transform->hierarchy;
	vec4f_t     *src = h->worldMatrix.a[transform->index];
	mat[0] = src[0];
	mat[1] = src[1];
	mat[2] = src[2];
	mat[3] = src[3];
}

XFORMINLINE
const vec4f_t *
Transform_GetWorldMatrixPtr (const transform_t *transform)
{
	hierarchy_t *h = transform->hierarchy;
	return h->worldMatrix.a[transform->index];
}

XFORMINLINE
void
Transform_GetWorldInverse (const transform_t *transform, mat4f_t mat)
{
	hierarchy_t *h = transform->hierarchy;
	vec4f_t     *src = h->worldInverse.a[transform->index];
	mat[0] = src[0];
	mat[1] = src[1];
	mat[2] = src[2];
	mat[3] = src[3];
}

XFORMINLINE
vec4f_t
Transform_GetLocalPosition (const transform_t *transform)
{
	hierarchy_t *h = transform->hierarchy;
	return h->localMatrix.a[transform->index][3];
}

XFORMINLINE
vec4f_t
Transform_GetLocalRotation (const transform_t *transform)
{
	hierarchy_t *h = transform->hierarchy;
	return h->localRotation.a[transform->index];
}

XFORMINLINE
vec4f_t
Transform_GetLocalScale (const transform_t *transform)
{
	hierarchy_t *h = transform->hierarchy;
	return h->localScale.a[transform->index];
}

XFORMINLINE
vec4f_t
Transform_GetWorldPosition (const transform_t *transform)
{
	hierarchy_t *h = transform->hierarchy;
	return h->worldMatrix.a[transform->index][3];
}

XFORMINLINE
vec4f_t
Transform_GetWorldRotation (const transform_t *transform)
{
	hierarchy_t *h = transform->hierarchy;
	return h->worldRotation.a[transform->index];
}

XFORMINLINE
vec4f_t
Transform_GetWorldScale (const transform_t *transform)
{
	hierarchy_t *h = transform->hierarchy;
	return h->worldScale.a[transform->index];
}

XFORMINLINE
vec4f_t
Transform_Forward (const transform_t *transform)
{
	hierarchy_t *h = transform->hierarchy;
	return h->worldMatrix.a[transform->index][0];
}

XFORMINLINE
vec4f_t
Transform_Right (const transform_t *transform)
{
	hierarchy_t *h = transform->hierarchy;
	return -h->worldMatrix.a[transform->index][1];
}

XFORMINLINE
vec4f_t
Transform_Up (const transform_t *transform)
{
	hierarchy_t *h = transform->hierarchy;
	return h->worldMatrix.a[transform->index][2];
}

///@}

#endif//__QF_scene_transform_h
