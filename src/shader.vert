#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require  // add this

layout(buffer_reference, scalar) readonly buffer CameraBuffer
{
    mat4 mvp;
};

layout(push_constant) uniform PushConstants
{
	CameraBuffer camera;
} push;

layout(location = 0) in vec3 pos;
layout(location = 0) out vec4 color;

void main()
{
	gl_Position = push.camera.mvp * vec4(pos, 1.0);
	color = vec4(1.0, 0.15, 0.15, 1.0);
}