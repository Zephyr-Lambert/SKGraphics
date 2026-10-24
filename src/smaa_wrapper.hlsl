// SKGraphics integration: SMAA 1x, no temporal history or predication.
float4 metrics : register(c0);
float4 settings : register(c1);
#define SMAA_RT_METRICS metrics
#define SMAA_HLSL_3 1
#define SMAA_AREATEX_SELECT(sample) sample.rg
// Build script inserts the unmodified reference SMAA.hlsl here.
// @SMAA_REFERENCE@
sampler2D input0 : register(s0);
sampler2D input1 : register(s1);
sampler2D input2 : register(s2);
float4 edges(float2 uv : TEXCOORD0) : COLOR0 {
    float4 offsets[3];
    SMAAEdgeDetectionVS(uv, offsets);
    return float4(SMAALumaEdgeDetectionPS(uv, offsets, input0), 0, 0);
}
float4 weights(float2 uv : TEXCOORD0) : COLOR0 {
    float2 pixcoord; float4 offsets[3];
    SMAABlendingWeightCalculationVS(uv, pixcoord, offsets);
    return SMAABlendingWeightCalculationPS(uv, pixcoord, offsets,
                                          input0, input1, input2, float4(0,0,0,0));
}
float4 blend(float2 uv : TEXCOORD0) : COLOR0 {
    float4 offset;
    SMAANeighborhoodBlendingVS(uv, offset);
    float4 original = tex2Dlod(input0, float4(uv,0,0));
    float4 filtered = SMAANeighborhoodBlendingPS(uv, offset, input0, input1);
    return float4(lerp(original.rgb, filtered.rgb, settings.x), original.a);
}
