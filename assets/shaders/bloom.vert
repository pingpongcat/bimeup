#version 450

// Fullscreen triangle — three vertices at NDC (-1,-1), (3,-1), (-1,3) cover
// the screen with no vertex buffer. UV maps [0,1]² onto the on-screen quad.
// Same shape as fxaa.vert / outline.vert / tonemap.vert — shared by both
// bloom_down.frag and bloom_up.frag in the RP.10 dual-filter pyramid.
layout(location = 0) out vec2 outUv;

void main() {
    outUv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(outUv * 2.0 - 1.0, 0.0, 1.0);
}
