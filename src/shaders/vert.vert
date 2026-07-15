#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(set = 0, binding = 0) uniform CameraBlock {
    mat4 view;
    mat4 proj;
} camera;

struct GPUObject {
    mat4 model;
};

layout(set = 0, binding = 2) buffer ObjectBuffer {
    GPUObject objects[];
} objectBuffer;

layout(location = 0) out VS_OUT {
    vec3 color;
    vec2 texCoord;
} vs_out;

void main() {
    mat4 model = objectBuffer.objects[gl_InstanceIndex].model;
    gl_Position = camera.proj * camera.view * model * vec4(inPosition, 1.0);
    vs_out.color = inColor;
    vs_out.texCoord = inTexCoord;
}
