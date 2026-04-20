#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 lineCenter;

layout(location = 0) out vec4 outColor;

layout(set=0, binding = 0) uniform UniformBufferObject {
	mat4 cam;
	vec4 view_pos;
	vec4 light_pos;
	float aspectRatio;
} camera;

void main() {
	if (dot(fragPos - lineCenter, fragPos - lineCenter) > 0.2){
		discard;
	}
	outColor = vec4(1.0, 0.0, 0.0, 1.0);
}
