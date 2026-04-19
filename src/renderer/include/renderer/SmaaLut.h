#pragma once

#include <cstddef>

namespace bimeup::renderer {

/// Wrapper over the SMAA 1x AreaTex LUT vendored from `external/smaa`
/// (`Textures/AreaTex.h`). Stored in R8G8, sampled by `smaa_weights.frag`
/// (RP.11b.3) with linear filtering + CLAMP_TO_EDGE to look up edge-traversal
/// area weights.
struct SmaaAreaTex {
    static constexpr std::size_t kWidth = 160;
    static constexpr std::size_t kHeight = 560;
    static constexpr std::size_t kChannels = 2;
    static constexpr std::size_t kSizeBytes = kWidth * kHeight * kChannels;

    [[nodiscard]] static const unsigned char* Data();
};

/// Wrapper over the SMAA 1x SearchTex LUT vendored from `external/smaa`
/// (`Textures/SearchTex.h`). Stored in R8, sampled by `smaa_weights.frag`
/// (RP.11b.3) during the edge-search traversal.
struct SmaaSearchTex {
    static constexpr std::size_t kWidth = 64;
    static constexpr std::size_t kHeight = 16;
    static constexpr std::size_t kChannels = 1;
    static constexpr std::size_t kSizeBytes = kWidth * kHeight * kChannels;

    [[nodiscard]] static const unsigned char* Data();
};

}  // namespace bimeup::renderer
