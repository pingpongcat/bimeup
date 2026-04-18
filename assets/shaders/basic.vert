#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
} camera;

layout(push_constant) uniform PushConstants {
    mat4 model;
} push;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec3 fragNormalWorld;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out vec3 fragNormalView;

void main() {
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    gl_Position = camera.projection * camera.view * worldPos;
    fragColor = inColor;
    fragWorldPos = worldPos.xyz;

    // Transform normal into world space. Assumes model transform has no
    // non-uniform scale (true for current BIM scene); otherwise use inverse-transpose.
    vec3 normalWorld = normalize(mat3(push.model) * inNormal);
    fragNormalWorld = normalWorld;
    fragNormalView = mat3(camera.view) * normalWorld;
}
