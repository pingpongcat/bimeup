#version 450

// RP.18.2 — same light-space transform as `shadow.vert`. Position-only input;
// transparent geometry is rendered from the light's POV into the transmission
// colour attachment so `basic.frag` can sample the tinted sun attenuation.
// Normal/colour attributes on the shared vertex buffer are ignored here.
layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform PushConstants {
    layout(offset = 0) mat4 lightSpaceModel;
} push;

void main() {
    gl_Position = push.lightSpaceModel * vec4(inPosition, 1.0);
}
