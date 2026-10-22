#version 330 core
out vec4 FragColor;
in vec2 texCoords;

uniform sampler2D screenTexture;
uniform sampler2D bloomBlur;
uniform sampler2D bloomNarrow;
uniform sampler2D planetMask;

void main()
{
    vec3 scene = texture(screenTexture, texCoords).rgb;
    vec3 wide   = texture(bloomBlur, texCoords).rgb;     // faint, reaches far
    vec3 narrow = texture(bloomNarrow, texCoords).rgb;   // bright, close to the star
    vec3 bloom  = narrow * 1.5 * vec3(1.0, 0.95, 0.85)    // core: nearly white
                + wide   * 2.0 * vec3(1.0, 0.8, 0.55);    // outer glow: warmer 
    float mask = texture(planetMask, texCoords).a;
    const float glowOverPlanets = 0.25;   // 0 = no glow on planets, 1 = full glow as before
    bloom *= mix(1.0, glowOverPlanets, mask);
    vec3 color = scene + bloom;

    float noise = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
    color += (noise - 0.5) / 255.0;

FragColor = vec4(color, 1.0);
}