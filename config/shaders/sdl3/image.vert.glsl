#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_texcoord;
layout(location = 2) in vec4 in_color;

layout(location = 0) out vec2 frag_texcoord;
layout(location = 1) out vec4 frag_color;

layout(set = 1, binding = 0) uniform DrawUniforms
{
	mat4 projection;
	float viewport_width;
	float viewport_height;
	float plane_distance;
	float reserved;
} ubo;

void main()
{
	vec3 view_position = vec3(
		in_position.x - ( ubo.viewport_width * 0.5 ),
		( ubo.viewport_height * 0.5 ) - in_position.y,
		-( ubo.plane_distance - in_position.z ) );
	gl_Position = ubo.projection * vec4( view_position, 1.0 );
	frag_texcoord = in_texcoord;
	frag_color = in_color;
}
