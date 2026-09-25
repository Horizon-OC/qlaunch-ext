#version 460

layout (std140, binding = 0) uniform ResBlock {
    vec2 u_res;
};

layout (location = 0) in vec2 inPos;
layout (location = 1) in vec2 inUv;
layout (location = 2) in vec2 inOrg;
layout (location = 3) in vec4 inShape;
layout (location = 4) in vec4 inColor;
layout (location = 6) in float inBot;

layout (location = 0) out vec2 fragUv;
layout (location = 1) out vec2 fragLocal;
layout (location = 2) out vec2 fragWH;
layout (location = 3) out float fragRad;
layout (location = 4) out vec4 fragColor;
layout (location = 6) out float fragBot;
layout (location = 5) out float fragPad;

void main()
{
    float nx = inPos.x * (2.0 / u_res.x) - 1.0;
    float ny = 1.0 - inPos.y * (2.0 / u_res.y);
    gl_Position = vec4(nx, ny, 0.0, 1.0);
    fragUv = inUv;
    fragLocal = inPos - inOrg;
    fragWH = inShape.xy;
    fragRad = inShape.z;
    fragPad = inShape.w;
    fragColor = inColor;
    fragBot = inBot;
}

