#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include "renderer/ClipPlane.h"
#include "renderer/ClipPlaneManager.h"
#include "scene/AxisSectionController.h"

using bimeup::renderer::ClipPlane;
using bimeup::renderer::ClipPlaneManager;
using bimeup::scene::Axis;
using bimeup::scene::AxisSectionController;
using bimeup::scene::AxisSectionSlot;
using bimeup::scene::MakeAxisEquation;
using bimeup::scene::SectionMode;

namespace {

bool NearEq(const glm::vec4& a, const glm::vec4& b, float eps = 1e-5F) {
    return glm::length(a - b) <= eps;
}

}  // namespace

TEST(Scene_AxisSectionController, MakeAxisEquation_XFront_NormalPlusX_DIsMinusOffset) {
    const glm::vec4 eq = MakeAxisEquation(Axis::X, SectionMode::CutFront, 5.0F);
    EXPECT_TRUE(NearEq(eq, {1.0F, 0.0F, 0.0F, -5.0F}));
}

TEST(Scene_AxisSectionController, MakeAxisEquation_XBack_FlipsSign) {
    const glm::vec4 eq = MakeAxisEquation(Axis::X, SectionMode::CutBack, 5.0F);
    EXPECT_TRUE(NearEq(eq, {-1.0F, 0.0F, 0.0F, 5.0F}));
}

TEST(Scene_AxisSectionController, MakeAxisEquation_SectionOnly_SameAsCutFront) {
    const glm::vec4 front = MakeAxisEquation(Axis::Y, SectionMode::CutFront, 2.0F);
    const glm::vec4 section = MakeAxisEquation(Axis::Y, SectionMode::SectionOnly, 2.0F);
    EXPECT_TRUE(NearEq(front, section));
}

TEST(Scene_AxisSectionController, MakeAxisEquation_YAndZ_PickCorrectBasisVector) {
    EXPECT_TRUE(
        NearEq(MakeAxisEquation(Axis::Y, SectionMode::CutFront, 3.0F),
               {0.0F, 1.0F, 0.0F, -3.0F}));
    EXPECT_TRUE(
        NearEq(MakeAxisEquation(Axis::Z, SectionMode::CutBack, 7.0F),
               {0.0F, 0.0F, -1.0F, 7.0F}));
}

TEST(Scene_AxisSectionController, Empty_NoSlotsAndNoSectionOnly) {
    AxisSectionController c;
    EXPECT_EQ(c.SlotCount(), 0U);
    EXPECT_FALSE(c.AnySectionOnly());
    EXPECT_FALSE(c.HasSlot(Axis::X));
    EXPECT_FALSE(c.GetSlot(Axis::Y).has_value());
}

TEST(Scene_AxisSectionController, SetSlot_AddsAndReplaces) {
    AxisSectionController c;
    c.SetSlot(Axis::X, {5.0F, SectionMode::CutFront});
    EXPECT_EQ(c.SlotCount(), 1U);
    ASSERT_TRUE(c.HasSlot(Axis::X));
    EXPECT_FLOAT_EQ(c.GetSlot(Axis::X)->offset, 5.0F);
    EXPECT_EQ(c.GetSlot(Axis::X)->mode, SectionMode::CutFront);

    c.SetSlot(Axis::X, {8.0F, SectionMode::CutBack});
    EXPECT_EQ(c.SlotCount(), 1U);
    EXPECT_FLOAT_EQ(c.GetSlot(Axis::X)->offset, 8.0F);
    EXPECT_EQ(c.GetSlot(Axis::X)->mode, SectionMode::CutBack);
}

TEST(Scene_AxisSectionController, ClearSlot_Removes) {
    AxisSectionController c;
    c.SetSlot(Axis::Y, {0.0F, SectionMode::CutFront});
    c.ClearSlot(Axis::Y);
    EXPECT_EQ(c.SlotCount(), 0U);
    EXPECT_FALSE(c.HasSlot(Axis::Y));
}

TEST(Scene_AxisSectionController, AnySectionOnly_TrueOnlyWhenASlotIsSectionOnly) {
    AxisSectionController c;
    c.SetSlot(Axis::X, {0.0F, SectionMode::CutFront});
    c.SetSlot(Axis::Y, {0.0F, SectionMode::CutBack});
    EXPECT_FALSE(c.AnySectionOnly());

    c.SetSlot(Axis::Z, {0.0F, SectionMode::SectionOnly});
    EXPECT_TRUE(c.AnySectionOnly());

    c.ClearSlot(Axis::Z);
    EXPECT_FALSE(c.AnySectionOnly());
}

TEST(Scene_AxisSectionController, SyncTo_AddsPlaneWithSectionFill) {
    AxisSectionController c;
    ClipPlaneManager m;
    c.SetSlot(Axis::X, {5.0F, SectionMode::CutFront});

    const bool changed = c.SyncTo(m);

    EXPECT_TRUE(changed);
    ASSERT_EQ(m.Count(), 1U);
    const ClipPlane& plane = m.Planes()[0].plane;
    EXPECT_TRUE(NearEq(plane.equation, {1.0F, 0.0F, 0.0F, -5.0F}));
    EXPECT_TRUE(plane.sectionFill);
    EXPECT_TRUE(plane.enabled);
}

TEST(Scene_AxisSectionController, SyncTo_NoChangesReturnsFalse) {
    AxisSectionController c;
    ClipPlaneManager m;
    c.SetSlot(Axis::Y, {2.0F, SectionMode::CutFront});
    c.SyncTo(m);

    const bool changed = c.SyncTo(m);

    EXPECT_FALSE(changed);
    EXPECT_EQ(m.Count(), 1U);
}

TEST(Scene_AxisSectionController, SyncTo_ModeFlipUpdatesEquationInPlace) {
    AxisSectionController c;
    ClipPlaneManager m;
    c.SetSlot(Axis::X, {5.0F, SectionMode::CutFront});
    c.SyncTo(m);

    c.SetSlot(Axis::X, {5.0F, SectionMode::CutBack});
    const bool changed = c.SyncTo(m);

    EXPECT_TRUE(changed);
    ASSERT_EQ(m.Count(), 1U);
    EXPECT_TRUE(NearEq(m.Planes()[0].plane.equation, {-1.0F, 0.0F, 0.0F, 5.0F}));
}

TEST(Scene_AxisSectionController, SyncTo_OffsetUpdateChangesD) {
    AxisSectionController c;
    ClipPlaneManager m;
    c.SetSlot(Axis::X, {5.0F, SectionMode::CutFront});
    c.SyncTo(m);

    c.SetSlot(Axis::X, {9.0F, SectionMode::CutFront});
    c.SyncTo(m);

    ASSERT_EQ(m.Count(), 1U);
    EXPECT_TRUE(NearEq(m.Planes()[0].plane.equation, {1.0F, 0.0F, 0.0F, -9.0F}));
}

TEST(Scene_AxisSectionController, SyncTo_ClearedSlotRemovesPlane) {
    AxisSectionController c;
    ClipPlaneManager m;
    c.SetSlot(Axis::Z, {1.0F, SectionMode::CutFront});
    c.SyncTo(m);
    ASSERT_EQ(m.Count(), 1U);

    c.ClearSlot(Axis::Z);
    const bool changed = c.SyncTo(m);

    EXPECT_TRUE(changed);
    EXPECT_EQ(m.Count(), 0U);
}

TEST(Scene_AxisSectionController, SyncTo_ThreeAxesAllSectionFill) {
    AxisSectionController c;
    ClipPlaneManager m;
    c.SetSlot(Axis::X, {1.0F, SectionMode::CutFront});
    c.SetSlot(Axis::Y, {2.0F, SectionMode::CutBack});
    c.SetSlot(Axis::Z, {3.0F, SectionMode::SectionOnly});

    c.SyncTo(m);

    EXPECT_EQ(m.Count(), 3U);
    for (const auto& e : m.Planes()) {
        EXPECT_TRUE(e.plane.sectionFill);
        EXPECT_TRUE(e.plane.enabled);
    }
}

TEST(Scene_AxisSectionController, SyncTo_SurvivesExternalManagerRemoval) {
    AxisSectionController c;
    ClipPlaneManager m;
    c.SetSlot(Axis::X, {0.0F, SectionMode::CutFront});
    c.SyncTo(m);

    // An external party drops the plane; controller should re-add on next sync.
    const std::uint32_t id = m.Planes()[0].id;
    m.RemovePlane(id);
    ASSERT_EQ(m.Count(), 0U);

    const bool changed = c.SyncTo(m);

    EXPECT_TRUE(changed);
    EXPECT_EQ(m.Count(), 1U);
}
