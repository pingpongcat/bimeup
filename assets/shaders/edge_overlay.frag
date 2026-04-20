#version 450

// RP.17.8.a — axis clip planes must cut the edge overlay as well as the shaded
// surface, otherwise feature edges keep drawing on the hidden side of an active
// section plane. Mirrors the discard loop in basic.frag against the same
// ClipPlanesUBO at set 0 / binding 3.
layout(location = 0) in vec3 fragWorldPos;

layout(set = 0, binding = 3) uniform ClipPlanesUBO {
    vec4 planes[6];  // ax + by + cz + d = 0; a point is kept when dot(n,p) + d >= 0
    ivec4 count;     // x = number of active planes
} clipPlanes;

layout(push_constant) uniform PushConstants {
    layout(offset = 64) vec4 edgeColor;
} push;

layout(location = 0) out vec4 outColor;

void main() {
    for (int i = 0; i < clipPlanes.count.x; ++i) {
        vec4 eq = clipPlanes.planes[i];
        if (dot(eq.xyz, fragWorldPos) + eq.w < 0.0) {
            discard;
        }
    }
    outColor = push.edgeColor;
}
