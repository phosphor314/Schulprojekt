#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNorm;
layout(location = 2) out vec3 fragColor;

layout(set=0, binding = 0) uniform UniformBufferObject {
	mat4 cam;
	vec4 view_pos;
	vec4 light_pos;
	float aspectRatio;
} camera;

struct EnemyStorageBufferStruct{
	mat4 transformMat;
	mat4 lowerEyelidTransform;
	mat4 upperEyelidTransform;
};

layout(set=0, binding = 1) readonly buffer SSBO{
	EnemyStorageBufferStruct enemyData[];
};

void main() {    
    gl_Position = camera.cam*enemyData[gl_InstanceIndex].transformMat*vec4(inPosition, 1.0);
    fragPos = (enemyData[gl_InstanceIndex].transformMat*vec4(inPosition, 1.0)).xyz;
    fragNorm = (enemyData[gl_InstanceIndex].transformMat*vec4(inNormal, 0.0)).xyz;
    fragColor = inColor;
}
