#version 460
#extension GL_EXT_ray_tracing : require

// Stage 9.5.a — RT AO miss. Ray escaped the radius without hitting
// anything; mark the pixel fully lit. The raygen clears the payload to
// 0 before `traceRayEXT`, so the no-CH contract (skip CH flag) together
// with no miss invocation ⇒ occluded; miss invocation ⇒ lit.

layout(location = 0) rayPayloadInEXT float payload;

void main() {
    payload = 1.0;
}
