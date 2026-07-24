// Default fragment shader for PREMULTIPLIED blend mode

void main()
{
	gl_FragColor = gl_Color;
	gl_FragColor.xyz *= gl_FragColor.w;
}
