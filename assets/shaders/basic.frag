#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec3 fragNormalWorld;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform LightingUBO {
    vec4 keyDirectionIntensity;
    vec4 keyColorEnabled;
    vec4 fillDirectionIntensity;
    vec4 fillColorEnabled;
    vec4 rimDirectionIntensity;
    vec4 rimColorEnabled;
    vec4 ambient;
} lights;

vec3 lambertContribution(vec3 n, vec4 dirI, vec4 colE) {
    float enabled = colE.w;
    vec3 toLight = normalize(-dirI.xyz);
    float ndotl = max(dot(n, toLight), 0.0);
    return colE.rgb * (dirI.w * ndotl * enabled);
}

void main() {
    vec3 n = normalize(fragNormalWorld);

    vec3 lit = lights.ambient.rgb;
    lit += lambertContribution(n, lights.keyDirectionIntensity, lights.keyColorEnabled);
    lit += lambertContribution(n, lights.fillDirectionIntensity, lights.fillColorEnabled);
    lit += lambertContribution(n, lights.rimDirectionIntensity, lights.rimColorEnabled);

    outColor = vec4(fragColor.rgb * lit, fragColor.a);
}
