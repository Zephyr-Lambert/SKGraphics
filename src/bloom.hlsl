sampler2D Source : register(s0);
float4 Parameters : register(c0);
float4 Options : register(c1); // x: 0 = brightest channel, 1 = luminance

float3 bright(float3 color) {
    // Luminance lets vivid but not truly bright colors (pure red, blue) bloom less.
    float level = Options.x > 0.5 ? dot(color, float3(0.2126, 0.7152, 0.0722))
                                  : max(color.r, max(color.g, color.b));
    float knee = max(Parameters.w, 0.00001);
    float soft = clamp(level - Parameters.z + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee);
    return color * (max(level - Parameters.z, soft) / max(level, 0.00001));
}
float4 extract(float2 uv : TEXCOORD0) : COLOR0 {
    float2 d = Parameters.xy;
    float3 c = bright(tex2D(Source, uv + float2(-d.x, -d.y)).rgb);
    c += bright(tex2D(Source, uv + float2(d.x, -d.y)).rgb);
    c += bright(tex2D(Source, uv + float2(-d.x, d.y)).rgb);
    c += bright(tex2D(Source, uv + float2(d.x, d.y)).rgb);
    return float4(c * 0.25, 0.0);
}
float4 blur(float2 uv : TEXCOORD0) : COLOR0 {
    float2 d = Parameters.xy;
    float3 c = tex2D(Source, uv).rgb * 0.2270270270;
    c += tex2D(Source, uv + d * 1.3846153846).rgb * 0.3162162162;
    c += tex2D(Source, uv - d * 1.3846153846).rgb * 0.3162162162;
    c += tex2D(Source, uv + d * 3.2307692308).rgb * 0.0702702703;
    c += tex2D(Source, uv - d * 3.2307692308).rgb * 0.0702702703;
    return float4(c, 0.0);
}
float4 composite(float2 uv : TEXCOORD0) : COLOR0 {
    return float4(tex2D(Source, uv).rgb * Parameters.x, 0.0);
}

// Preserve the pre-character image when a screen draws 2D elements first.
sampler2D Background : register(s1);
float4 copyScene(float2 uv : TEXCOORD0) : COLOR0 {
    return tex2D(Source, uv);
}
float3 changedBright(float2 uv) {
    float3 c = tex2D(Source, uv).rgb;
    float3 difference = abs(c - tex2D(Background, uv).rgb);
    float changed = step(0.003921569, max(difference.r, max(difference.g, difference.b)));
    return bright(c) * changed;
}
float4 extractChanged(float2 uv : TEXCOORD0) : COLOR0 {
    float2 d = Parameters.xy;
    float3 c = changedBright(uv + float2(-d.x,-d.y));
    c += changedBright(uv + float2(d.x,-d.y));
    c += changedBright(uv + float2(-d.x,d.y));
    c += changedBright(uv + float2(d.x,d.y));
    return float4(c * 0.25, 0.0);
}

// Tone-mapped composite. The scene is expanded with the inverse of the curve
// 1 - exp(-x), bloom is added, and the result is compressed with the same curve.
// Pixels without bloom come back unchanged; added light rolls off towards white
// instead of clipping. Working on the brightest channel keeps each color's hue.
float4 compositeToneMapped(float2 uv : TEXCOORD0) : COLOR0 {
    float3 scene = tex2D(Background, uv).rgb;
    float3 bloom = tex2D(Source, uv).rgb * Parameters.x;
    float v = max(scene.r, max(scene.g, scene.b));
    float expand = v > 0.00001 ? -log(1.0 - min(v, 0.999)) / v : 1.0;
    float3 x = scene * expand + bloom;
    float vx = max(x.r, max(x.g, x.b));
    float compress = vx > 0.00001 ? (1.0 - exp(-vx)) / vx : 1.0;
    return float4(x * compress, 1.0);
}
