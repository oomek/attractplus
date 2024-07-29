// Default fragment shader for MULTIPLY blend mode

uniform sampler2D texture;

void main()
{
	// lookup the pixel in the texture
	vec4 pixel = texture2DProj(texture, gl_TexCoord[0]);

	// multiply it by the color
	gl_FragColor = gl_Color * pixel;

	// this must be the last line in this shader
	gl_FragColor.xyz *= gl_FragColor.w;
}
