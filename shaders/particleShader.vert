#version 450

layout(location = 0) in vec3 inPosition;

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
  gl_Position = camera.cam*vec4(inPosition, 1.0);
  gl_PointSize = min(particleInfo.size / gl_Position.z, 1000.0f);
}
