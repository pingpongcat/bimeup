#version 450

layout(push_constant) uniform PushConstants {
    layout(offset = 64) vec4 edgeColor;
} push;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = push.edgeColor;
}
