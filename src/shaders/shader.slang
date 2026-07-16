struct VSInput {
    float3 inPosition;
    float3 inColor;
    float2 inTexCoord;
};

struct UniformBuffer {
    float4x4 view;
    float4x4 proj;
};
[[vk_binding(0)]]
ConstantBuffer<UniformBuffer> ubo;

#pragma warning(disable : 39001)
// Combined Image Sampler used, shared binding is intentional
[[vk_binding(1)]]
Texture2D textures[];
[[vk_binding(1)]]
SamplerState samplerState;

struct ObjectData {
  float4x4 model;
  float3 color;
  uint32_t textureIndex;
};
[[vk_binding(2)]]
StructuredBuffer<ObjectData> objects;

struct VSOutput {
    float4 pos : SV_Position;
    float3 fragColor;
    float2 fragTexCoord;
    uint32_t textureIndex;
};

[shader("vertex")]
VSOutput vertMain(VSInput input, uint instanceID : SV_InstanceID) {
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(objects[instanceID].model, float4(input.inPosition, 1.0))));
    output.fragColor = objects[instanceID].color;
    // output.fragColor = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    output.textureIndex = objects[instanceID].textureIndex;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
  float4 texColor = textures[vertIn.textureIndex].Sample(samplerState, vertIn.fragTexCoord);
  return float4(vertIn.fragColor * texColor.rgb, 1.0);
}
