#pragma once

#include <glm/glm.hpp>

namespace bimeup::renderer {

// Octahedron-encode a unit-length normal into a 2D vector in [-1, 1]^2 — the
// Cigolle et al. (JCGT 2014) "signed encoding" used by modern deferred/SSAO
// renderers feeding an R16G16_SNORM G-buffer. Input should be (approximately)
// unit length; non-unit inputs are renormalised via L1 division internally.
// CPU mirror of the `OctPack` helper that the Stage RP MRT variant of
// `basic.frag` will emit.
glm::vec2 OctPackNormal(const glm::vec3& n);

// Inverse of `OctPackNormal`. Returns a unit-length vector for any input inside
// the octahedron square [-1, 1]^2.
glm::vec3 OctUnpackNormal(const glm::vec2& e);

}  // namespace bimeup::renderer
