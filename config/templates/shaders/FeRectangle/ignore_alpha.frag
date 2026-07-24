// Default fragment shader for IGNORE_ALPHA blend mode

void main()
{
	gl_FragColor = gl_Color;
	gl_FragColor.xyz *= gl_FragColor.w;
}
