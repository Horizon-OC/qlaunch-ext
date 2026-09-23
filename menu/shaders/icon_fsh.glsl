#version 460

layout (location = 0) in vec2 fragUv;
layout (location = 1) in vec2 fragLocal;
layout (location = 2) in vec2 fragWH;
layout (location = 3) in float fragRad;
layout (location = 4) in vec4 fragColor;

layout (binding = 0) uniform sampler2D tex;

layout (location = 0) out vec4 outColor;

float rrect(vec2 p, vec2 b, float r)
{
    vec2 q = abs(p - b * 0.5) - (b * 0.5 - vec2(r));
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main()
{
    float r = min(fragRad, min(fragWH.x, fragWH.y) * 0.5);
    float dOuter = rrect(fragLocal, fragWH, r);
    float ao = 1.0 - smoothstep(-1.0, 1.0, dOuter);
    float bw = 5.0;
    vec2 pI = fragLocal - vec2(bw);
    vec2 bI = fragWH - vec2(bw * 2.0);
    float rI = max(r - bw, 0.0);
    float dInner = rrect(pI, bI, rI);
    float ai = 1.0 - smoothstep(-1.0, 1.0, dInner);
    vec4 tx = texture(tex, fragUv);
    vec3 col = mix(fragColor.rgb, tx.rgb, ai * tx.a);
    outColor = vec4(col, ao);
}

