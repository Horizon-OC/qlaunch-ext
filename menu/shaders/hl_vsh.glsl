#version 460

layout (std140, binding = 0) uniform ResBlock {
    vec2 u_res;
};

layout (location = 0) in vec2 inPos;
layout (location = 1) in vec2 inOrg;
layout (location = 2) in vec4 inShape;
layout (location = 3) in vec4 inC0;
layout (location = 4) in vec4 inC1;
layout (location = 5) in vec4 inC2;
layout (location = 6) in vec4 inC3;
layout (location = 7) in float inRot;

layout (location = 0) out vec2 fragLocal;
layout (location = 1) out vec2 fragWH;
layout (location = 2) out vec2 fragRadThick;
layout (location = 3) out vec4 fragC0;
layout (location = 4) out vec4 fragC1;
layout (location = 5) out vec4 fragC2;
layout (location = 6) out vec4 fragC3;
layout (location = 7) out float fragRot;

void main()
{
    float nx = inPos.x * (2.0 / u_res.x) - 1.0;
    float ny = 1.0 - inPos.y * (2.0 / u_res.y);
    gl_Position = vec4(nx, ny, 0.0, 1.0);
    fragLocal = inPos - inOrg;
    fragWH = inShape.xy;
    fragRadThick = inShape.zw;
    fragC0 = inC0;
    fragC1 = inC1;
    fragC2 = inC2;
    fragC3 = inC3;
    fragRot = inRot;
}

