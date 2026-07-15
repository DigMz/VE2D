#version 450

layout(location = 0) in VS_OUT {
    vec3 color;
    vec2 texCoord;
} fs_in;

layout(set = 0, binding = 1) uniform sampler2D texSampler;
layout(location = 0) out vec4 outColor;

void main() {
    vec4 tex = texture(texSampler, fs_in.texCoord);
    outColor = tex * vec4(fs_in.color, 1.0);
}
