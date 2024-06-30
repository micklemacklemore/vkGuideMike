//glsl version 4.5
#version 450

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 tex; 

//output write
layout (location = 0) out vec4 outFragColor;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

void main()
{
	//return red
	outFragColor = texture(texSampler, tex); 
}
