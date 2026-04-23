#version 450

layout(location = 0) in vec3 pos;
layout(location = 0) out vec4 color;

void main()
{
	gl_Position = vec4(pos, 1.0);
	color = vec4(1.0, 0.15, 0.15, 1.0);
}