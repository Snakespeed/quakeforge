/*
	transform.c

	General transform handling

	Copyright (C) 2021 Bill Currke

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

#define IMPLEMENT_TRANSFORM_Funcs

#include "QF/scene/hierarchy.h"
#include "QF/scene/scene.h"
#include "QF/scene/transform.h"

#include "scn_internal.h"

transform_t *
__transform_alloc (scene_t *scene)
{
	scene_resources_t *res = scene->resources;
	transform_t *transform = PR_RESNEW_NC (res->transforms);
	transform->scene = scene;
	transform->id = PR_RESINDEX (res->transforms, transform);
	transform->hierarchy = 0;
	transform->index = 0;
	return transform;
}

transform_t *
Transform_New (scene_t *scene, transform_t *parent)
{
	transform_t *transform = __transform_alloc (scene);

	if (parent) {
		transform->hierarchy = parent->hierarchy;
		transform->index = Hierarchy_InsertHierarchy (parent->hierarchy, 0,
													  parent->index, 0);
	} else {
		transform->hierarchy = Hierarchy_New (scene, 1);
		transform->index = 0;
	}
	transform->hierarchy->transform.a[transform->index] = transform;
	Hierarchy_UpdateMatrices (transform->hierarchy);
	return transform;
}

void
Transform_Delete (transform_t *transform)
{
	if (transform->index != 0) {
		// The transform is not the root, so pull it out of its current
		// hierarchy so deleting it is easier
		Transform_SetParent (transform, 0);
	}
	// Takes care of freeing the transforms
	Hierarchy_Delete (transform->hierarchy);
}

transform_t *
Transform_NewNamed (scene_t *scene, transform_t *parent, const char *name)
{
	transform_t *transform = Transform_New (scene, parent);
	Transform_SetName (transform, name);
	return transform;
}

void
Transform_SetParent (transform_t *transform, transform_t *parent)
{
	if (parent) {
		hierarchy_t *hierarchy = transform->hierarchy;
		uint32_t    index = transform->index;
		Hierarchy_InsertHierarchy (parent->hierarchy, hierarchy,
								   parent->index, index);
		Hierarchy_RemoveHierarchy (hierarchy, index);
		if (!hierarchy->name.size) {
			Hierarchy_Delete (hierarchy);
		}
	} else {
		// null parent -> make transform root
		if (!transform->index) {
			// already root
			return;
		}
		hierarchy_t *hierarchy = transform->hierarchy;
		uint32_t    index = transform->index;

		hierarchy_t *new_hierarchy = Hierarchy_New (transform->scene, 0);
		Hierarchy_InsertHierarchy (new_hierarchy, hierarchy, null_transform,
								   index);
		Hierarchy_RemoveHierarchy (hierarchy, index);
	}
}

void
Transform_SetName (transform_t *transform, const char *name)
{
	hierarchy_t *h = transform->hierarchy;
	//FIXME create a string pool (similar to qfcc's, or even move that to util)
	if (h->name.a[transform->index]) {
		free (h->name.a[transform->index]);
	}
	h->name.a[transform->index] = strdup (name);
}

void
Transform_SetTag (transform_t *transform, uint32_t tag)
{
	hierarchy_t *h = transform->hierarchy;
	h->tag.a[transform->index] = tag;
}

void
Transform_SetLocalPosition (transform_t *transform, vec4f_t position)
{
	hierarchy_t *h = transform->hierarchy;
	h->localMatrix.a[transform->index][3] = position;
	h->modified.a[transform->index] = 1;
	Hierarchy_UpdateMatrices (h);
}

void
Transform_SetLocalRotation (transform_t *transform, vec4f_t rotation)
{
	hierarchy_t *h = transform->hierarchy;
	vec4f_t     scale = h->localScale.a[transform->index];

	mat4f_t     mat;
	mat4fquat (mat, rotation);

    h->localRotation.a[transform->index] = rotation;
	h->localMatrix.a[transform->index][0] = mat[0] * scale[0];
	h->localMatrix.a[transform->index][1] = mat[1] * scale[1];
	h->localMatrix.a[transform->index][2] = mat[2] * scale[2];
	h->modified.a[transform->index] = 1;
	Hierarchy_UpdateMatrices (h);
}

void
Transform_SetLocalScale (transform_t *transform, vec4f_t scale)
{
	hierarchy_t *h = transform->hierarchy;
	vec4f_t     rotation = h->localRotation.a[transform->index];

	mat4f_t     mat;
	mat4fquat (mat, rotation);

    h->localScale.a[transform->index] = scale;
	h->localMatrix.a[transform->index][0] = mat[0] * scale[0];
	h->localMatrix.a[transform->index][1] = mat[1] * scale[1];
	h->localMatrix.a[transform->index][2] = mat[2] * scale[2];
	h->modified.a[transform->index] = 1;
	Hierarchy_UpdateMatrices (h);
}

void
Transform_SetWorldPosition (transform_t *transform, vec4f_t position)
{
	if (transform->index) {
		hierarchy_t *h = transform->hierarchy;
		uint32_t    parent = h->parentIndex.a[transform->index];
		position = mvmulf (h->worldInverse.a[parent], position);
	}
	Transform_SetLocalPosition (transform, position);
}

void
Transform_SetWorldRotation (transform_t *transform, vec4f_t rotation)
{
	if (transform->index) {
		hierarchy_t *h = transform->hierarchy;
		uint32_t    parent = h->parentIndex.a[transform->index];
		rotation = qmulf (qconjf (h->worldRotation.a[parent]), rotation);
	}
	Transform_SetLocalRotation (transform, rotation);
}

void
Transform_SetLocalTransform (transform_t *transform, vec4f_t scale,
							 vec4f_t rotation, vec4f_t position)
{
	hierarchy_t *h = transform->hierarchy;
	mat4f_t     mat;
	mat4fquat (mat, rotation);

	position[3] = 1;
    h->localRotation.a[transform->index] = rotation;
    h->localScale.a[transform->index] = scale;
	h->localMatrix.a[transform->index][0] = mat[0] * scale[0];
	h->localMatrix.a[transform->index][1] = mat[1] * scale[1];
	h->localMatrix.a[transform->index][2] = mat[2] * scale[2];
	h->localMatrix.a[transform->index][3] = position;
	h->modified.a[transform->index] = 1;
	Hierarchy_UpdateMatrices (h);
}
