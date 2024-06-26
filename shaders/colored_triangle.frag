//glsl version 4.5
#version 450

layout (location = 0) in vec3 inColor;

//output write
layout (location = 0) out vec4 outFragColor;

layout( push_constant ) uniform constants
{
	vec4 data;
	mat4 render_matrix;
} PushConstants;

void main()
{
	//return red
	outFragColor = vec4(inColor, 1.); 
}
