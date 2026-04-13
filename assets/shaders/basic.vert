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

void main() {
    gl_Position = camera.projection * camera.view * push.model * vec4(inPosition, 1.0);
    fragColor = inColor;

    // Transform normal into world space. Assumes model transform has no
    // non-uniform scale (true for current BIM scene); otherwise use inverse-transpose.
    fragNormalWorld = normalize(mat3(push.model) * inNormal);
}
