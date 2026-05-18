#version 450
#extension GL_EXT_buffer_reference   : require
#extension GL_EXT_buffer_reference2  : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_nonuniform_qualifier : require

layout(buffer_reference, scalar) readonly buffer CameraBuffer
{
	mat4 view_proj;
	vec3 camera_pos;
};

layout(buffer_reference, scalar) readonly buffer LightBuffer
{
	vec3 direction;
	float padding;
	vec3 color;
	float ambient_strength;
	float specular_strength;
	float shininess;
	vec2 padding2;
};

layout(push_constant) uniform PushConstants
{
	CameraBuffer camera;
	uint64_t scene;
	LightBuffer light;
} push;



layout(location = 0) in vec3 in_world_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in flat uint in_texture_index;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D textures[];

void main()
{
	vec3 surface_color = texture(textures[nonuniformEXT(in_texture_index)], in_uv).rgb;
	vec3 normal = normalize(in_normal);
	vec3 light_dir = normalize(-push.light.direction);

	//ambient
	vec3 ambient = push.light.ambient_strength * push.light.color;

	//diffuse
	float diff = max(dot(normal, light_dir), 0.0);
	vec3 diffuse = diff * push.light.color;

	//specular
	vec3 view_dir = normalize(push.camera.camera_pos - in_world_position);
	vec3 reflect_dir = reflect(-light_dir, normal);
	float spec = pow(max(dot(view_dir, reflect_dir), 0.0), push.light.shininess);
	vec3 specular = push.light.specular_strength * spec * surface_color;
	vec3 result = (ambient + diffuse + specular) * surface_color;
	out_color = vec4(result, 1.0);
}