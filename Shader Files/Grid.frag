#version 330 core
layout (location = 0) out vec4 FragColor;     // normal scene
layout (location = 1) out vec4 BrightColor;   // glow image

uniform vec4 gridColor;

void main()
{
    FragColor = gridColor;
    BrightColor = vec4(0.0, 0.0, 0.0, 0.0);
}