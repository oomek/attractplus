// Default fragment shader for OVERLAY blend mode
// Source color higher than 127,127,127 brightens the scene, lower than 127,127,127 darkens it.

void main()
{
	gl_FragColor = gl_Color;
	gl_FragColor = mix(vec4(0.5,0.5,0.5,1.0), gl_FragColor, gl_FragColor.w);
}
