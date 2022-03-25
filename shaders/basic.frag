#version 410 core

in vec3 fPosition;
in vec3 fNormal;
in vec2 fTexCoords;

out vec4 fColor;

//matrices
uniform mat4 model;
uniform mat4 view;
uniform mat3 normalMatrix;

//lighting
uniform vec3 lightDir;
uniform vec3 lightColor;

//fog
uniform vec3 fogDensity;

// textures
uniform sampler2D diffuseTexture;
uniform sampler2D specularTexture;

//point light
uniform vec3 lightPosition;
uniform vec3 lightPosOn;

//components
vec3 ambient;
float ambientStrength = 0.2f;
vec3 diffuse;
vec3 specular;
float specularStrength = 0.5f;

vec3 ambient_point;
vec3 diffuse_point;
vec3 specular_point;

vec3 color_point = glm::vec3(1.0f,0.0f,0.0f);

//shadow computation
uniform mat4 lightSpaceTrMatrix;
in vec4 fragPosLightSpace;

vec4 fPosEye;
void computeDirLight()
{
    //compute eye space coordinates
    fPosEye = view * model * vec4(fPosition, 1.0f);
    vec3 normalEye = normalize(normalMatrix * fNormal);

    //normalize light direction
    vec3 lightDirN = vec3(normalize(view * vec4(lightDir, 0.0f)));

    //compute view direction (in eye coordinates, the viewer is situated at the origin
    vec3 viewDir = normalize(- fPosEye.xyz);

    //compute ambient light
    ambient = ambientStrength * lightColor;

    //compute diffuse light
    diffuse = max(dot(normalEye, lightDirN), 0.0f) * lightColor;

    //compute specular light
    vec3 reflectDir = reflect(-lightDirN, normalEye);
    float specCoeff = pow(max(dot(viewDir, reflectDir), 0.0f), 32);
    specular = specularStrength * specCoeff * lightColor;
}

float constant = 1.0f;
float linear = 0.0025f;
float quadratic = 0.0037f;

void computePointLight()
{
    //compute eye space coordinates
    fPosEye = view * model * vec4(fPosition, 1.0f);
    vec3 normalEye = normalize(normalMatrix * fNormal);

    //normalize light direction
    //vec3 lightDirN = vec3(normalize(view * vec4(lightDir, 0.0f)));
    vec3 lightDirN = normalize(lightPosition - fPosEye.xyz);
    //vec3 lightDirN = vec3(normalize(view * vec4(lightPosition - fPosEye.xyz, 0.0f)));

    //compute view direction (in eye coordinates, the viewer is situated at the origin
    vec3 viewDir = normalize(- fPosEye.xyz);

    //compute distance to light
	float dist = length(lightPosition - fPosEye.xyz);
	//compute attenuation
	float att = 1.0f / (constant + linear * dist + quadratic * (dist * dist));

    //compute ambient light
    ambient_point = att * ambientStrength * color_point;

    //compute diffuse light
    diffuse_point = att * max(dot(normalEye, lightDirN), 0.0f) * color_point;

    //compute specular light
    vec3 reflectDir = reflect(-lightDirN, normalEye);
    float specCoeff = pow(max(dot(viewDir, reflectDir), 0.0f), 32);
    specular_point = att * specularStrength * specCoeff * color_point;
}

uniform sampler2D shadowMap;

float computeShadow(){
    //perform perspective divide
	vec3 normalizedCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // Transform to [0,1] range
	normalizedCoords = normalizedCoords * 0.5 + 0.5;
    if (normalizedCoords.z > 1.0f)
		return 0.0f;

    // Get closest depth value from light's perspective
	float closestDepth = texture(shadowMap, normalizedCoords.xy).r;
	// Get depth of current fragment from light's perspective
	float currentDepth = normalizedCoords.z;

	// Check whether current frag pos is in shadow
	float bias = 0.005f;
    //float bias = max(0.05f * (1.0f - dot(fNormal, lightDir)), 0.005f);
	float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;

	return shadow;
}

float computeFog()
{
 float fragmentDistance = length(fPosEye);
 float fogFactor = exp(-pow(fragmentDistance * fogDensity.x, 2));
 
 return clamp(fogFactor, 0.0f, 1.0f);
}
void main() 
{
    computeDirLight();

    computePointLight();

    //for shadow
    float shadow = computeShadow();
    vec3 color = min((ambient + (1.0f - shadow) *diffuse) * texture(diffuseTexture, fTexCoords).rgb + (1.0f-shadow) * specular * texture(specularTexture, fTexCoords).rgb, 1.0f);

    
    vec3 color_point = min((ambient_point + diffuse_point) * texture(diffuseTexture, fTexCoords).rgb + specular_point * texture(specularTexture, fTexCoords).rgb, 1.0f);
    vec3 combined_color;
    if(lightPosOn.x == 0){
        combined_color = color;
    }else{
       combined_color = color + color_point; 
    }
    //compute final vertex color
    //vec3 color = min((ambient + diffuse) * texture(diffuseTexture, fTexCoords).rgb + specular * texture(specularTexture, fTexCoords).rgb, 1.0f);

    //for fog
    float fogFactor = computeFog();
	vec4 fogColor = vec4(0.5f, 0.5f, 0.5f, 1.0f);
    //vec4 color4= vec4(color,0.0f);
    vec4 color4= vec4(combined_color,0.0f);
    fColor = mix(fogColor, color4, fogFactor);

    //fColor = vec4(color, 1.0f);
}
