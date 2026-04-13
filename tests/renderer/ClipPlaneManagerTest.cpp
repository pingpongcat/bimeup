#include <gtest/gtest.h>
#include <renderer/ClipPlane.h>
#include <renderer/ClipPlaneManager.h>

using bimeup::renderer::ClipPlane;
using bimeup::renderer::ClipPlaneManager;
using bimeup::renderer::ClipPlanesUbo;
using bimeup::renderer::PackClipPlanes;

TEST(ClipPlaneManagerTest, StartsEmpty) {
    ClipPlaneManager mgr;
    EXPECT_EQ(mgr.Count(), 0U);
    EXPECT_TRUE(mgr.Planes().empty());
}

TEST(ClipPlaneManagerTest, AddPlaneReturnsNonZeroIdAndStoresEquation) {
    ClipPlaneManager mgr;
    const glm::vec4 eq{1.0F, 0.0F, 0.0F, -2.0F};
    const std::uint32_t id = mgr.AddPlane(eq);
    EXPECT_NE(id, ClipPlaneManager::kInvalidId);
    EXPECT_EQ(mgr.Count(), 1U);
    ASSERT_TRUE(mgr.Contains(id));
    const ClipPlane* p = mgr.Find(id);
    ASSERT_NE(p, nullptr);
    EXPECT_FLOAT_EQ(p->equation.x, 1.0F);
    EXPECT_FLOAT_EQ(p->equation.w, -2.0F);
    EXPECT_TRUE(p->enabled);
}

TEST(ClipPlaneManagerTest, AddPlaneIdsAreUnique) {
    ClipPlaneManager mgr;
    const std::uint32_t a = mgr.AddPlane({1.0F, 0.0F, 0.0F, 0.0F});
    const std::uint32_t b = mgr.AddPlane({0.0F, 1.0F, 0.0F, 0.0F});
    const std::uint32_t c = mgr.AddPlane({0.0F, 0.0F, 1.0F, 0.0F});
    EXPECT_NE(a, b);
    EXPECT_NE(b, c);
    EXPECT_NE(a, c);
    EXPECT_EQ(mgr.Count(), 3U);
}

TEST(ClipPlaneManagerTest, CapsAtSixPlanes) {
    ClipPlaneManager mgr;
    for (std::size_t i = 0; i < ClipPlaneManager::kMaxPlanes; ++i) {
        EXPECT_NE(mgr.AddPlane({1.0F, 0.0F, 0.0F, static_cast<float>(i)}),
                  ClipPlaneManager::kInvalidId);
    }
    EXPECT_EQ(mgr.Count(), ClipPlaneManager::kMaxPlanes);
    // 7th AddPlane must fail.
    EXPECT_EQ(mgr.AddPlane({1.0F, 0.0F, 0.0F, 99.0F}), ClipPlaneManager::kInvalidId);
    EXPECT_EQ(mgr.Count(), ClipPlaneManager::kMaxPlanes);
}

TEST(ClipPlaneManagerTest, RemovePlaneDropsEntry) {
    ClipPlaneManager mgr;
    const std::uint32_t a = mgr.AddPlane({1.0F, 0.0F, 0.0F, 0.0F});
    const std::uint32_t b = mgr.AddPlane({0.0F, 1.0F, 0.0F, 0.0F});
    EXPECT_TRUE(mgr.RemovePlane(a));
    EXPECT_EQ(mgr.Count(), 1U);
    EXPECT_FALSE(mgr.Contains(a));
    EXPECT_TRUE(mgr.Contains(b));
}

TEST(ClipPlaneManagerTest, RemovePlaneWithUnknownIdIsNoOp) {
    ClipPlaneManager mgr;
    const std::uint32_t a = mgr.AddPlane({1.0F, 0.0F, 0.0F, 0.0F});
    EXPECT_FALSE(mgr.RemovePlane(a + 999U));
    EXPECT_FALSE(mgr.RemovePlane(ClipPlaneManager::kInvalidId));
    EXPECT_EQ(mgr.Count(), 1U);
    EXPECT_TRUE(mgr.Contains(a));
}

TEST(ClipPlaneManagerTest, RemoveFreesSlotSoNewPlaneFits) {
    ClipPlaneManager mgr;
    std::uint32_t ids[ClipPlaneManager::kMaxPlanes];
    for (std::size_t i = 0; i < ClipPlaneManager::kMaxPlanes; ++i) {
        ids[i] = mgr.AddPlane({1.0F, 0.0F, 0.0F, static_cast<float>(i)});
    }
    EXPECT_EQ(mgr.AddPlane({0.0F, 1.0F, 0.0F, 0.0F}), ClipPlaneManager::kInvalidId);
    EXPECT_TRUE(mgr.RemovePlane(ids[2]));
    const std::uint32_t newId = mgr.AddPlane({0.0F, 1.0F, 0.0F, 0.0F});
    EXPECT_NE(newId, ClipPlaneManager::kInvalidId);
    EXPECT_NE(newId, ids[2]);  // ids should not be reused
    EXPECT_EQ(mgr.Count(), ClipPlaneManager::kMaxPlanes);
}

TEST(ClipPlaneManagerTest, SetEnabledTogglesFlag) {
    ClipPlaneManager mgr;
    const std::uint32_t id = mgr.AddPlane({1.0F, 0.0F, 0.0F, 0.0F});
    ASSERT_TRUE(mgr.Find(id)->enabled);
    EXPECT_TRUE(mgr.SetEnabled(id, false));
    EXPECT_FALSE(mgr.Find(id)->enabled);
    EXPECT_TRUE(mgr.SetEnabled(id, true));
    EXPECT_TRUE(mgr.Find(id)->enabled);
}

TEST(ClipPlaneManagerTest, SetEnabledUnknownIdReturnsFalse) {
    ClipPlaneManager mgr;
    EXPECT_FALSE(mgr.SetEnabled(42U, true));
}

TEST(ClipPlaneManagerTest, UpdatePlaneReplacesEquationKeepingEnabled) {
    ClipPlaneManager mgr;
    const std::uint32_t id = mgr.AddPlane({1.0F, 0.0F, 0.0F, 0.0F});
    mgr.SetEnabled(id, false);
    EXPECT_TRUE(mgr.UpdatePlane(id, {0.0F, 0.0F, 1.0F, -5.0F}));
    const ClipPlane* p = mgr.Find(id);
    ASSERT_NE(p, nullptr);
    EXPECT_FLOAT_EQ(p->equation.x, 0.0F);
    EXPECT_FLOAT_EQ(p->equation.z, 1.0F);
    EXPECT_FLOAT_EQ(p->equation.w, -5.0F);
    EXPECT_FALSE(p->enabled);  // enabled flag preserved
}

TEST(ClipPlaneManagerTest, UpdatePlaneUnknownIdReturnsFalse) {
    ClipPlaneManager mgr;
    EXPECT_FALSE(mgr.UpdatePlane(99U, {1.0F, 0.0F, 0.0F, 0.0F}));
}

// --- PackClipPlanes: builds the std140 UBO the shader reads from. -------------

TEST(ClipPlanesUboTest, SizeMatchesStd140Layout) {
    // std140: vec4 planes[6] = 6*16 = 96 bytes, ivec4 count = 16 bytes → 112 total.
    EXPECT_EQ(sizeof(ClipPlanesUbo), 112U);
}

TEST(ClipPlanesUboTest, EmptyManagerProducesZeroCount) {
    ClipPlaneManager mgr;
    const ClipPlanesUbo ubo = PackClipPlanes(mgr);
    EXPECT_EQ(ubo.count.x, 0);
}

TEST(ClipPlanesUboTest, SingleEnabledPlaneCopiesEquation) {
    ClipPlaneManager mgr;
    const glm::vec4 eq{1.0F, 0.0F, 0.0F, -2.0F};
    mgr.AddPlane(eq);

    const ClipPlanesUbo ubo = PackClipPlanes(mgr);
    EXPECT_EQ(ubo.count.x, 1);
    EXPECT_FLOAT_EQ(ubo.planes[0].x, 1.0F);
    EXPECT_FLOAT_EQ(ubo.planes[0].y, 0.0F);
    EXPECT_FLOAT_EQ(ubo.planes[0].z, 0.0F);
    EXPECT_FLOAT_EQ(ubo.planes[0].w, -2.0F);
}

TEST(ClipPlanesUboTest, DisabledPlaneIsSkipped) {
    ClipPlaneManager mgr;
    const std::uint32_t a = mgr.AddPlane({1.0F, 0.0F, 0.0F, 0.0F});
    mgr.AddPlane({0.0F, 1.0F, 0.0F, -3.0F});
    mgr.SetEnabled(a, false);

    const ClipPlanesUbo ubo = PackClipPlanes(mgr);
    EXPECT_EQ(ubo.count.x, 1);
    EXPECT_FLOAT_EQ(ubo.planes[0].y, 1.0F);
    EXPECT_FLOAT_EQ(ubo.planes[0].w, -3.0F);
}

TEST(ClipPlanesUboTest, MultipleEnabledPlanesPackSequentially) {
    ClipPlaneManager mgr;
    mgr.AddPlane({1.0F, 0.0F, 0.0F, -1.0F});
    mgr.AddPlane({0.0F, 1.0F, 0.0F, -2.0F});
    mgr.AddPlane({0.0F, 0.0F, 1.0F, -3.0F});

    const ClipPlanesUbo ubo = PackClipPlanes(mgr);
    EXPECT_EQ(ubo.count.x, 3);
    EXPECT_FLOAT_EQ(ubo.planes[0].w, -1.0F);
    EXPECT_FLOAT_EQ(ubo.planes[1].w, -2.0F);
    EXPECT_FLOAT_EQ(ubo.planes[2].w, -3.0F);
}

TEST(ClipPlanesUboTest, AllDisabledYieldsZeroCount) {
    ClipPlaneManager mgr;
    const std::uint32_t a = mgr.AddPlane({1.0F, 0.0F, 0.0F, 0.0F});
    const std::uint32_t b = mgr.AddPlane({0.0F, 1.0F, 0.0F, 0.0F});
    mgr.SetEnabled(a, false);
    mgr.SetEnabled(b, false);

    const ClipPlanesUbo ubo = PackClipPlanes(mgr);
    EXPECT_EQ(ubo.count.x, 0);
}

TEST(ClipPlaneManagerTest, PlanesExposesAllEntriesInInsertionOrder) {
    ClipPlaneManager mgr;
    const std::uint32_t a = mgr.AddPlane({1.0F, 0.0F, 0.0F, 0.0F});
    const std::uint32_t b = mgr.AddPlane({0.0F, 1.0F, 0.0F, -1.0F});
    mgr.SetEnabled(b, false);
    const auto& entries = mgr.Planes();
    ASSERT_EQ(entries.size(), 2U);
    EXPECT_EQ(entries[0].id, a);
    EXPECT_TRUE(entries[0].plane.enabled);
    EXPECT_EQ(entries[1].id, b);
    EXPECT_FALSE(entries[1].plane.enabled);
    EXPECT_FLOAT_EQ(entries[1].plane.equation.w, -1.0F);
}
