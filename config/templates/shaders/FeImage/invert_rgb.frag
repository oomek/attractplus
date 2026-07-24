// Default fragment shader for INVERT_RGB blend mode

uniform sampler2D texture;

void main()
{
	vec4 pixel = texture2D(texture, gl_TexCoord[0].xy);
	gl_FragColor = gl_Color * pixel;
	gl_FragColor.xyz *= gl_FragColor.w;
}
