struct VSInput {
    float3 inPosition;
    float3 inColor;
    float2 inTexCoord;
};

struct UniformBuffer {
    float4x4 view;
    float4x4 proj;
};
ConstantBuffer<UniformBuffer> ubo;

Sampler2D texture;

struct ObjectData {
  float4x4 model;
  float3 color;
};
StructuredBuffer<ObjectData> objects;

struct VSOutput {
    float4 pos : SV_Position;
    float3 fragColor;
    float2 fragTexCoord;
};

[shader("vertex")]
VSOutput vertMain(VSInput input, uint instanceID : SV_InstanceID) {
    VSOutput output;
    output.pos = mul(ubo.proj, mul(ubo.view, mul(objects[instanceID].model, float4(input.inPosition, 1.0))));
    output.fragColor = objects[instanceID].color;
    // output.fragColor = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
  return float4(vertIn.fragColor * texture.Sample(vertIn.fragTexCoord).rgb, 1.0);
}
