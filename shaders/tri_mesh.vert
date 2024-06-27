#version 450

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec3 vColor;

layout (location = 0) out vec3 outColor;

layout(set = 0, binding = 0) uniform UniformBufferObject {
	mat4 modelViewProjection; 
} ubo; 

void main()
{
	gl_Position = ubo.modelViewProjection * vec4(vPosition, 1.0f);
	outColor = vColor;
}	
