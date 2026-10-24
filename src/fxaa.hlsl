// Original compact directional FXAA-style filter. Not NVIDIA FXAA 3.11.
sampler2D image : register(s0);
float4 pixel : register(c0); // inverse width/height, relative/minimum luma threshold
float4 tune : register(c1);  // max span in pixels, reduction scale/minimum, blend strength
float luminance(float3 c) { return dot(c, float3(0.299, 0.587, 0.114)); }
float4 main(float2 uv : TEXCOORD0) : COLOR0 {
    float4 center = tex2D(image, uv);
    float nw = luminance(tex2D(image, uv + pixel.xy * float2(-1,-1)).rgb);
    float ne = luminance(tex2D(image, uv + pixel.xy * float2( 1,-1)).rgb);
    float sw = luminance(tex2D(image, uv + pixel.xy * float2(-1, 1)).rgb);
    float se = luminance(tex2D(image, uv + pixel.xy * float2( 1, 1)).rgb);
    float mid = luminance(center.rgb);
    float lo = min(mid, min(min(nw,ne), min(sw,se)));
    float hi = max(mid, max(max(nw,ne), max(sw,se)));
    if (hi-lo < max(pixel.w, hi*pixel.z)) return center;
    float2 tangent = float2(sw+se-nw-ne, nw+sw-ne-se);
    float stabilizer = max((nw+ne+sw+se)*0.25*tune.y, tune.z);
    tangent /= min(abs(tangent.x), abs(tangent.y)) + stabilizer;
    tangent = clamp(tangent, -tune.x, tune.x) * pixel.xy;
    float3 narrow = 0.5 * (tex2D(image, uv-tangent/6).rgb + tex2D(image, uv+tangent/6).rgb);
    float3 wide = narrow*0.5 + 0.25 * (tex2D(image, uv-tangent*0.5).rgb + tex2D(image, uv+tangent*0.5).rgb);
    float candidate = luminance(wide);
    float3 filtered = (candidate<lo || candidate>hi) ? narrow : wide;
    return float4(lerp(center.rgb, filtered, tune.w), center.a);
}
