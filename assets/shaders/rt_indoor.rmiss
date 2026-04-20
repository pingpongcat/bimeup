#version 460
#extension GL_EXT_ray_tracing : require

// Stage 9.7.a — RT indoor-fill miss. Ray escaped without hitting any
// opaque geometry → the fragment can see the overhead fill light, so
// mark fully lit. Raygen seeded `payload = 0` + set
// `SkipClosestHitShaderEXT`, so "no miss invocation" ⇒ occluded, "miss
// invocation" ⇒ lit.

layout(location = 0) rayPayloadInEXT float payload;

void main() {
    payload = 1.0;
}
