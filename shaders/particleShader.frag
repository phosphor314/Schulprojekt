#version 450

layout(location = 0) out vec4 outColor;

layout(set=0, binding = 0) uniform UniformBufferObject {
	mat4 cam;
	vec4 view_pos;
	vec4 light_pos;
	float aspectRatio;
} camera;

layout(set=1, binding=0) uniform ParticleUniform {
  vec4 color;
  float size;
} particleInfo;

void main() {
	outColor = particleInfo.color;
}
