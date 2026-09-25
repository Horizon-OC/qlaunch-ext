#version 460

layout (location = 0) in vec2 fragLocal;
layout (location = 1) in vec2 fragWH;
layout (location = 2) in float fragRad;
layout (location = 3) in vec4 fragColor;

layout (location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(fragColor.rgb, 1.0);
}

