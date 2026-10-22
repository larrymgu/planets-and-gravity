#version 330 core
layout (location = 0) out vec4 FragColor;     // normal scene
layout (location = 1) out vec4 BrightColor;   // glow image

uniform vec3 objectColor;
uniform float glowStrength;

void main()
{
    FragColor = vec4(objectColor, 1.0);
    BrightColor = vec4(objectColor * glowStrength, 0.0);
}