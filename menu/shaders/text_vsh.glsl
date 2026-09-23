#version 460

layout (std140, binding = 0) uniform ResBlock {
    vec2 u_res;
};


layout (location = 0) in vec2 inPos;
layout (location = 1) in vec2 inUv;
layout (location = 2) in vec4 inColor;

layout (location = 0) out vec2 fragUv;
layout (location = 1) out vec4 fragColor;

void main()
{
    float nx = inPos.x * (2.0 / u_res.x) - 1.0;
    float ny = 1.0 - inPos.y * (2.0 / u_res.y);
    gl_Position = vec4(nx, ny, 0.0, 1.0);
    fragUv = inUv;
    fragColor = inColor;
}

