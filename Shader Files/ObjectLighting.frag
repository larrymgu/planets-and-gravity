#version 330 core
layout (location = 0) out vec4 FragColor;     // normal scene
layout (location = 1) out vec4 BrightColor;   // glow image

in vec3 Normal;  
in vec3 FragPos;  
  
uniform vec3 lightPos; 
uniform vec3 viewPos; 
uniform vec3 lightColor;
uniform vec3 objectColor;
uniform float sunRadius;

void main()
{
    // ambient
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;
  	
    // diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    diff = diff * 0.6;
    vec3 diffuse = diff * lightColor;
    

    // specular
    float specularStrength = 0.2;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    vec3 halfwayVec = normalize(viewDir + lightDir);
    float spec = pow(max(dot(norm, halfwayVec), 0.0), 4);
    vec3 specular = specularStrength * spec * lightColor;  
        
    vec3 result = (ambient + diffuse + specular) * objectColor;
    FragColor = vec4(result, 1.0);

    float planetDistance = length(viewPos - FragPos);
    float sunDistance    = length(viewPos - lightPos);
    float inFront = smoothstep(-sunRadius, sunRadius, sunDistance - planetDistance);
    BrightColor = vec4(0.0, 0.0, 0.0, inFront);
} 