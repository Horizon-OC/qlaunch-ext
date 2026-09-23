#version 460

layout (location = 0) in vec2 fragLocal;
layout (location = 1) in vec2 fragWH;
layout (location = 2) in float fragRad;
layout (location = 3) in vec4 fragColor;

layout (location = 0) out vec4 outColor;

void main()
{
    vec2 g = fract(fragLocal / 16.0) - 0.5;
    float m = smoothstep(0.20, 0.10, length(g));
    vec3 col = mix(fragColor.rgb, fragColor.rgb * 0.955, m);
    outColor = vec4(col, 1.0);
}

