#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

layout(buffer_reference, scalar) readonly buffer CameraBuffer
{
    mat4 view_proj;
    vec3 camera_pos;
};

struct GpuData
{
	mat4 transform;
	uint mesh_index;
	uint texture_index;
	uint ao_index;
	uint normal_index;
};

layout(buffer_reference, scalar) readonly buffer SceneBuffer
{
	GpuData data[];
};

layout(push_constant) uniform PushConstants
{
	CameraBuffer camera;
	SceneBuffer scene;
	uint64_t light;
} push;

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec4 tangent;

layout(location = 0) out vec3 out_world_position;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec2 out_uv;
layout(location = 3) out vec3 out_tangent;
layout(location = 4) out vec3 out_bitangent;
layout(location = 5) out flat uint out_texture_index;
layout(location = 6) out flat uint out_ao_index;
layout(location = 7) out flat uint out_normal_index;

void main()
{
	GpuData entity = push.scene.data[gl_InstanceIndex];
	vec4 world_position = entity.transform * vec4(pos, 1.0);
	gl_Position = push.camera.view_proj * world_position;
	out_world_position = world_position.xyz;

	mat3 model_mat3 = mat3(entity.transform);
	mat3 normal_mat = transpose(inverse(model_mat3));

	vec3 T = normalize(model_mat3 * tangent.xyz);
	vec3 N = normalize(normal_mat * normal);
	T = normalize(T - dot(T, N) * N);
	vec3 B = cross(N, T) * tangent.w;

	out_normal = N;
	out_uv = uv;
	out_tangent = T;
	out_bitangent = B;
	out_texture_index = entity.texture_index;
	out_ao_index = entity.ao_index;
	out_normal_index = entity.normal_index;
}