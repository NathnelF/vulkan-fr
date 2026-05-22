#version 450
#extension GL_EXT_buffer_reference  : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

layout(buffer_reference, scalar) readonly buffer LightBuffer
{
    vec3  direction;
    float padding;
    vec3  color;
    float ambient_strength;
    float specular_strength;
    float shininess;
    vec2  padding2;
    mat4  light_view_proj;
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
    uint64_t   camera;
    SceneBuffer scene;
    LightBuffer light;
} push;

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec4 tangent;

void main()
{
    GpuData entity  = push.scene.data[gl_InstanceIndex];
    vec4 world_pos  = entity.transform * vec4(pos, 1.0);
    gl_Position     = push.light.light_view_proj * world_pos;
}
