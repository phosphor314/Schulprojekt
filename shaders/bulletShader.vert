#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inForward;

layout(location = 0) out vec3 position;
layout(location = 1) out vec3 lineCenter;

layout(set=0, binding = 0) uniform UniformBufferObject {
	mat4 cam;
	vec4 view_pos;
	vec4 light_pos;
	float aspectRatio;
} camera;

void main() {
	vec3 perp = normalize(cross(inPosition - camera.view_pos.xyz, inForward));
    if (gl_VertexIndex == 0){
    	position = inPosition + perp;
    	lineCenter = inPosition;
    }
    else if (gl_VertexIndex == 1) {
    	position = inPosition + inForward;
    	lineCenter = inPosition + inForward;
    }
    else {
    	position = inPosition - perp;
    	lineCenter = inPosition;
    }
    
    gl_Position = camera.cam*vec4(position, 1.0);
}
