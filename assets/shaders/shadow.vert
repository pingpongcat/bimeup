#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;

layout(push_constant) uniform PushConstants {
    mat4 lightSpaceModel;  // lightSpaceMatrix * model, premultiplied on CPU
} push;

void main() {
    gl_Position = push.lightSpaceModel * vec4(inPosition, 1.0);
}
