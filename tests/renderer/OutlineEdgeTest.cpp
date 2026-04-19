#include <gtest/gtest.h>

#include <renderer/OutlineEdge.h>

#include <array>
#include <cmath>
#include <cstdint>

namespace {

using bimeup::renderer::EdgeFromStencil;
using bimeup::renderer::SobelMagnitude;

constexpr float kEps = 1e-5F;

// 3x3 patch layout (row-major, top-left at index 0):
//   [0] [1] [2]
//   [3] [4] [5]   <- [4] is the center pixel
//   [6] [7] [8]

TEST(OutlineEdgeTest, SobelMagnitudeZeroOnFlatPatch) {
    // Flat patches produce no gradient → a Sobel response of exactly 0. This
    // is the load-bearing test: the outline shader discards anything below a
    // small epsilon, so a stray non-zero on flat regions would turn the whole
    // screen into outline.
    std::array<float, 9> flatZero = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    std::array<float, 9> flatOne = {1, 1, 1, 1, 1, 1, 1, 1, 1};
    std::array<float, 9> flatNeg = {-2, -2, -2, -2, -2, -2, -2, -2, -2};
    EXPECT_NEAR(SobelMagnitude(flatZero), 0.0F, kEps);
    EXPECT_NEAR(SobelMagnitude(flatOne), 0.0F, kEps);
    EXPECT_NEAR(SobelMagnitude(flatNeg), 0.0F, kEps);
}

TEST(OutlineEdgeTest, SobelMagnitudeHorizontalStepIsFour) {
    // Left column = 0, right column = 1 → Gx = 4, Gy = 0, |G| = 4.
    // Pins down the Gx kernel weights ([-1 0 1] / [-2 0 2] / [-1 0 1]) so the
    // GLSL mirror in outline.frag can't drift from the CPU test.
    std::array<float, 9> patch = {
        0, 0, 1,
        0, 0, 1,
        0, 0, 1,
    };
    EXPECT_NEAR(SobelMagnitude(patch), 4.0F, kEps);
}

TEST(OutlineEdgeTest, SobelMagnitudeVerticalStepIsFour) {
    // Top row = 0, bottom row = 1 → Gx = 0, Gy = 4, |G| = 4.
    // Pins down the Gy kernel weights ([-1 -2 -1] / [0 0 0] / [1 2 1]).
    std::array<float, 9> patch = {
        0, 0, 0,
        0, 0, 0,
        1, 1, 1,
    };
    EXPECT_NEAR(SobelMagnitude(patch), 4.0F, kEps);
}

TEST(OutlineEdgeTest, SobelMagnitudeDiagonalStepIsSqrt18) {
    // Diagonal discontinuity with Gx = Gy = 3 → |G| = sqrt(18) ≈ 4.2426.
    // Exercises both kernels at once.
    std::array<float, 9> patch = {
        0, 0, 1,
        0, 1, 1,
        1, 1, 1,
    };
    EXPECT_NEAR(SobelMagnitude(patch), std::sqrt(18.0F), kEps);
}

TEST(OutlineEdgeTest, SobelMagnitudeInvariantToSign) {
    // |G(-p)| == |G(p)|: sign flip negates both gradient components, magnitude
    // is unchanged. The outline shader feeds Sobel depth differences which can
    // be either sign depending on which side of the discontinuity is closer.
    std::array<float, 9> patch = {
        0.1F, 0.3F, 0.8F,
        0.2F, 0.5F, 0.9F,
        0.1F, 0.4F, 0.7F,
    };
    std::array<float, 9> neg{};
    for (std::size_t i = 0; i < patch.size(); ++i) {
        neg[i] = -patch[i];
    }
    EXPECT_NEAR(SobelMagnitude(patch), SobelMagnitude(neg), kEps);
}

TEST(OutlineEdgeTest, EdgeFromStencilFlatZeroHasNoEdge) {
    // Empty background (stencil 0 everywhere) → no outline category.
    std::array<std::uint8_t, 9> patch = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    EXPECT_EQ(EdgeFromStencil(patch), 0U);
}

TEST(OutlineEdgeTest, EdgeFromStencilInteriorOfSelectionHasNoEdge) {
    // Inside a selected element (stencil 1 everywhere) → no outline. The
    // outline must draw only at the boundary, not fill the whole element — a
    // bug that would make RP.6 behave like the fill-only highlight it replaces.
    std::array<std::uint8_t, 9> patch = {1, 1, 1, 1, 1, 1, 1, 1, 1};
    EXPECT_EQ(EdgeFromStencil(patch), 0U);
}

TEST(OutlineEdgeTest, EdgeFromStencilSelectedOnBackgroundReturnsSelected) {
    // Center pixel is on a selected element, one neighbour is background →
    // edge. Category is the highest non-equal stencil present in the window
    // (here: 1 = selected).
    std::array<std::uint8_t, 9> patch = {
        0, 0, 0,
        0, 1, 1,
        0, 1, 1,
    };
    EXPECT_EQ(EdgeFromStencil(patch), 1U);
}

TEST(OutlineEdgeTest, EdgeFromStencilHoveredOnBackgroundReturnsHovered) {
    // Hover category (2) bordering background.
    std::array<std::uint8_t, 9> patch = {
        2, 2, 0,
        2, 2, 0,
        2, 2, 0,
    };
    EXPECT_EQ(EdgeFromStencil(patch), 2U);
}

TEST(OutlineEdgeTest, EdgeFromStencilHoverBeatsSelectWhenBothPresent) {
    // Hover-over-selected case: both 1 and 2 appear in the window. Hover (2)
    // wins so the UI feedback for the cursor is visible even when it sits on
    // an already-selected element.
    std::array<std::uint8_t, 9> patch = {
        1, 1, 2,
        1, 1, 2,
        1, 1, 2,
    };
    EXPECT_EQ(EdgeFromStencil(patch), 2U);
}

TEST(OutlineEdgeTest, EdgeFromStencilEdgeVisibleFromBackgroundSide) {
    // Center is background but a neighbour is selected. The edge must still
    // fire so the outline occupies a full 2-px band (one pixel on each side
    // of the boundary) rather than a hairline.
    std::array<std::uint8_t, 9> patch = {
        0, 0, 0,
        1, 0, 0,
        0, 0, 0,
    };
    EXPECT_EQ(EdgeFromStencil(patch), 1U);
}

TEST(OutlineEdgeTest, EdgeFromStencilThreeCategoriesReturnsMax) {
    // Mixed 0 / 1 / 2 → hover (highest) still wins.
    std::array<std::uint8_t, 9> patch = {
        0, 1, 2,
        0, 1, 2,
        0, 1, 2,
    };
    EXPECT_EQ(EdgeFromStencil(patch), 2U);
}

// RP.12b: bit 4 = "transparent surface" so stencil values become
// {0, 1, 2, 4, 5, 6}. The outline edge detector must mask out bit 4 before
// the max-reduction so a transparent layer over a selected element still
// shows the selection outline (and a glass-only patch produces no edge).

TEST(OutlineEdgeTest, EdgeFromStencilGlassOnlyHasNoEdge) {
    // Pure transparent surface (bit 4 set, base id 0) bordering background
    // (0). After masking bit 4 the patch is uniform 0 → no edge. Without the
    // mask this would fire as an edge with category 4 and pick a bogus colour.
    std::array<std::uint8_t, 9> patch = {
        0, 0, 0,
        0, 4, 4,
        0, 4, 4,
    };
    EXPECT_EQ(EdgeFromStencil(patch), 0U);
}

TEST(OutlineEdgeTest, EdgeFromStencilSelectionSurvivesGlass) {
    // A selected element (id 1) viewed through glass (bit 4 set) bordered by
    // background (0). Stencil values seen: 0 = bg, 5 = selected-through-glass.
    // After masking bit 4 → {0, 1} → edge fires with category 1 (selected),
    // i.e. the outline survives the glass overlay.
    std::array<std::uint8_t, 9> patch = {
        0, 0, 5,
        0, 5, 5,
        0, 5, 5,
    };
    EXPECT_EQ(EdgeFromStencil(patch), 1U);
}

TEST(OutlineEdgeTest, EdgeFromStencilHoverSurvivesGlass) {
    // Hovered element (id 2) seen through glass (bit 4 set → 6) bordered by
    // background. After masking bit 4 → {0, 2} → category 2 (hover).
    std::array<std::uint8_t, 9> patch = {
        0, 0, 0,
        0, 6, 6,
        0, 6, 6,
    };
    EXPECT_EQ(EdgeFromStencil(patch), 2U);
}

TEST(OutlineEdgeTest, EdgeFromStencilHoverBeatsSelectThroughGlass) {
    // Both selected (1 / 5) and hovered (2 / 6) layers are visible. After
    // masking bit 4 across all taps the reduction sees {1, 2} → hover wins.
    std::array<std::uint8_t, 9> patch = {
        1, 5, 2,
        1, 5, 6,
        1, 5, 2,
    };
    EXPECT_EQ(EdgeFromStencil(patch), 2U);
}

TEST(OutlineEdgeTest, EdgeFromStencilUniformGlassOverSelectionHasNoEdge) {
    // Entire 3×3 patch is selected-through-glass (5). Masked → {1} uniform →
    // no edge (interior of a selection that happens to be behind glass — same
    // contract as the existing InteriorOfSelection test, but with the bit set).
    std::array<std::uint8_t, 9> patch = {5, 5, 5, 5, 5, 5, 5, 5, 5};
    EXPECT_EQ(EdgeFromStencil(patch), 0U);
}

}  // namespace
