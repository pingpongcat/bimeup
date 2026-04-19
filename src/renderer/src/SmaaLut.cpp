#include "renderer/SmaaLut.h"

#include "Textures/AreaTex.h"
#include "Textures/SearchTex.h"

static_assert(
    sizeof(areaTexBytes) == AREATEX_SIZE,
    "vendored AreaTex byte array size must match AREATEX_SIZE");
static_assert(
    sizeof(searchTexBytes) == SEARCHTEX_SIZE,
    "vendored SearchTex byte array size must match SEARCHTEX_SIZE");

namespace bimeup::renderer {

const unsigned char* SmaaAreaTex::Data() {
    return areaTexBytes;
}

const unsigned char* SmaaSearchTex::Data() {
    return searchTexBytes;
}

}  // namespace bimeup::renderer
