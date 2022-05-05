#version 450

layout (set = 1, binding = 0) uniform sampler2DArray Skin;

layout (push_constant) uniform PushConstants {
	layout (offset = 68)
	uint        colorA;
	uint        colorB;
	vec4        base_color;
	vec4        fog;
};

layout (location = 0) in vec2 texcoord;
layout (location = 1) in vec4 position;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec3 tangent;
layout (location = 4) in vec3 bitangent;
layout (location = 5) in vec3 color;

layout (location = 0) out vec4 frag_color;
layout (location = 1) out vec4 frag_emission;
layout (location = 2) out vec4 frag_normal;
layout (location = 3) out vec4 frag_position;

void
main (void)
{
	vec4        c;
	vec4        e;
	vec3        n;
	int         i;
	mat3        tbn = mat3 (tangent, bitangent, normal);

	c = texture (Skin, vec3 (texcoord, 0)) * base_color;
	c += texture (Skin, vec3 (texcoord, 1)) * unpackUnorm4x8(colorA);
	c += texture (Skin, vec3 (texcoord, 2)) * unpackUnorm4x8(colorB);
	e = texture (Skin, vec3 (texcoord, 3));
	n = texture (Skin, vec3 (texcoord, 4)).xyz * 2 - 1;

	frag_color = c;
	frag_emission = e;
	frag_normal = vec4(tbn * n, 1);
	frag_position = position;
}
