#version 450
#extension GL_GOOGLE_include_directive : enable

#include "entity.h"

layout (set = 0, binding = 0) uniform Matrices {
	mat4 Projection3d;
	mat4 View;
	mat4 Sky;
	mat4 Projection2d;
};

layout (set = 1, binding = 0) buffer Entities {
	Entity      entities[];
};

layout (location = 0) in vec4 vertex;
layout (location = 2) in uint entid;

void
main (void)
{
	vec3        vert = vertex * entities[entid].transform;
	gl_Position = Projection3d * (View * vec4 (vert, 1));
}
