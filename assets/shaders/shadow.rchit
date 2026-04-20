#version 460
#extension GL_EXT_ray_tracing : require

// Stage 9.6.b — closest-hit for RT sun shadows. Runs when a non-ignored
// intersection is accepted (opaque wall, floor, ceiling, non-transparent
// element). Combined with `gl_RayFlagsTerminateOnFirstHitEXT` in the
// raygen, this fires at most once per ray and writes `payload = 0` to
// mark the fragment as shadowed. Glass any-hits that call
// `ignoreIntersectionEXT` bypass this stage and let the ray continue.

layout(location = 0) rayPayloadInEXT float payload;

void main() {
    payload = 0.0;
}
