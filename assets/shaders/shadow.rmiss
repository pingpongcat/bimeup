#version 460
#extension GL_EXT_ray_tracing : require

// Stage 9.4.a → updated by 9.6.b. Miss is a no-op: raygen seeds the
// payload with 1.0 (fully lit) and the any-hit glass attenuator may
// reduce it as the ray passes through windows. If traversal reaches
// here without an accepted opaque hit, the accumulated transmission in
// the payload IS the answer.

layout(location = 0) rayPayloadInEXT float payload;

void main() {
}
