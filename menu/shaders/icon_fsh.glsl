#version 460

layout (location = 0) in vec2 fragUv;
layout (location = 1) in vec2 fragLocal;
layout (location = 2) in vec2 fragWH;
layout (location = 3) in float fragRad;
layout (location = 4) in vec4 fragColor;
layout (location = 6) in float fragBot;
layout (location = 5) in float fragPad;

layout (binding = 0) uniform sampler2D tex;

layout (location = 0) out vec4 outColor;

float rrect(vec2 p, vec2 b, float r)
{
    vec2 q = abs(p - b * 0.5) - (b * 0.5 - vec2(r));
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

float rrectTB(vec2 p, vec2 b, float rT, float rB)
{
    float dA = rrect(p, vec2(b.x, b.y * 0.5 + rT), rT);
    float dB = rrect(p - vec2(0.0, b.y * 0.5 - rB), vec2(b.x, b.y * 0.5 + rB), rB);
    return min(dA, dB);
}

void main()
{
    float r = min(fragRad, min(fragWH.x, fragWH.y) * 0.5);
    float rb = min(fragBot, min(fragWH.x, fragWH.y) * 0.5);
    float dOuter = rrectTB(fragLocal, fragWH, r, rb);
    float ao = 1.0 - smoothstep(-1.0, 1.0, dOuter);
    float bw = clamp(fragPad, 0.0, 16.0);
    vec2 pI = fragLocal - vec2(bw);
    vec2 bI = fragWH - vec2(bw * 2.0);
    float rI = max(r - bw, 0.0);
    float rIb = max(rb - bw, 0.0);
    float dInner = rrectTB(pI, bI, rI, rIb);
    float ai = 1.0 - smoothstep(-1.0, 1.0, dInner);
    vec4 tx = texture(tex, fragUv);
    float cov = ai * tx.a;
    vec3 col = mix(fragColor.rgb, tx.rgb * fragColor.rgb, cov);
    outColor = vec4(col, ao * cov);
}

