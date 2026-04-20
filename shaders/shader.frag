#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 color;

layout(location = 0) out vec4 outColor;

layout(set=0, binding = 0) uniform UniformBufferObject {
	mat4 cam;
	vec4 view_pos;
	vec4 light_pos;
	float aspectRatio;
} camera;

void main() {
	// Blinn-Phong
	vec3 lightDir   = normalize(camera.light_pos.xyz - fragPos.xyz);
	vec3 viewDir    = normalize(camera.view_pos.xyz - fragPos.xyz);
	vec3 halfwayDir = normalize(lightDir + viewDir);
	float diffuse = max(dot(fragNormal, lightDir), 0.0f);
	float specular = pow(max(dot(fragNormal, halfwayDir), 0.0f), 16.0f);
	float light = diffuse + specular;

	outColor = vec4(light / (light + 1.0));
}
