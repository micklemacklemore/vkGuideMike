//we will be using glsl version 4.5 syntax
#version 450

layout (location = 0) out vec3 outColor;

void main()
{
	//const array of positions for the triangle
	const vec3 positions[3] = vec3[3](
		vec3(1.,1., 0.),
		vec3(-1.,1., 0.),
		vec3(0.,-1., 0.)
	);

	const vec3 color[3] = vec3[3](
		vec3(1.,0., 0.),
		vec3(0.,1., 0.),
		vec3(0.,0., 1.)
	);

	vec3 new_positions[3]; 

	for (int i = 0; i < 3; i++) {
		new_positions[i] = positions[i] * vec3(.5);
	}

	//output the position of each vertex
	gl_Position = vec4(new_positions[gl_VertexIndex], 1.0);
	outColor = color[gl_VertexIndex];
}
