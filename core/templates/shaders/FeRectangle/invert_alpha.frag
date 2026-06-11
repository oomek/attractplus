// Default fragment shader for INVERT_ALPHA blend mode

void main()
{
	gl_FragColor = gl_Color;

	// this must be the last line in this shader
	gl_FragColor.xyz *= gl_FragColor.w;
}
