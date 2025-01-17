#version 450

layout (set = 0, binding = 0) uniform Matrices {
	mat4 Projection3d;
	mat4 View;
	mat4 Sky;
	mat4 Projection2d;
};

layout (push_constant) uniform PushConstants {
	mat4 Model;
	float blend;
};

layout (location = 0) in vec4 vertexa;
layout (location = 1) in vec3 normala;
layout (location = 2) in vec4 vertexb;
layout (location = 3) in vec3 normalb;
layout (location = 4) in vec2 uv;

layout (location = 0) out vec2 st;
layout (location = 1) out vec4 position;
layout (location = 2) out vec3 normal;

void
main (void)
{
	vec4        vertex;
	vec3        norm;
	vec4        pos;

	vertex = mix (vertexa, vertexb, blend);
	norm = mix (normala, normalb, blend);
	pos = (Model * vertex);
	gl_Position = Projection3d * (View * pos);
	position = pos;
	normal = normalize (mat3 (Model) * norm);
	st = uv;
}
