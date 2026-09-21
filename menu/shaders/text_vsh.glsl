#version 460

layout (location = 0) in vec2 inPos;
layout (location = 1) in vec2 inUv;
layout (location = 2) in vec4 inColor;

layout (location = 0) out vec2 fragUv;
layout (location = 1) out vec4 fragColor;

void main()
{
    float nx = inPos.x * (2.0 / 1280.0) - 1.0;
    float ny = 1.0 - inPos.y * (2.0 / 720.0);
    gl_Position = vec4(nx, ny, 0.0, 1.0);
    fragUv = inUv;
    fragColor = inColor;
}

