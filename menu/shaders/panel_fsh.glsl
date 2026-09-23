#version 460

layout (location = 0) in vec2 fragLocal;
layout (location = 1) in vec2 fragWH;
layout (location = 2) in float fragRad;
layout (location = 3) in vec4 fragColor;

layout (location = 0) out vec4 outColor;

float rrect(vec2 p, vec2 b, float r)
{
    vec2 q = abs(p - b * 0.5) - (b * 0.5 - vec2(r));
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main()
{
    float r = min(fragRad, min(fragWH.x, fragWH.y) * 0.5);
    float d = rrect(fragLocal, fragWH, r);
    float a = 1.0 - smoothstep(-1.0, 1.0, d);
    outColor = vec4(fragColor.rgb, fragColor.a * a);
}

