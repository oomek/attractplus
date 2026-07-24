// Default fragment shader for INVERT_RGB blend mode

void main()
{
	gl_FragColor = gl_Color;
	gl_FragColor.xyz *= gl_FragColor.w;
}
