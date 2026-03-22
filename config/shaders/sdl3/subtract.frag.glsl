#version 450

layout(location = 0) in vec2 frag_texcoord;
layout(location = 1) in vec4 frag_color;

layout(location = 0) out vec4 out_color;

layout(set = 2, binding = 0) uniform sampler2D image_texture;

void main()
{
	vec2 texture_size = vec2( textureSize( image_texture, 0 ) );
	vec2 uv = frag_texcoord;
	if ( texture_size.x > 0.0 && texture_size.y > 0.0 )
		uv /= texture_size;
	out_color = texture( image_texture, uv ) * frag_color;
}
