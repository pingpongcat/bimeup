#version 460
#extension GL_EXT_ray_tracing : require

// Stage 9.4.a — RT sun-shadow miss. Ray reached the light unobstructed;
// mark the pixel fully lit.

layout(location = 0) rayPayloadInEXT float payload;

void main() {
    payload = 1.0;
}
