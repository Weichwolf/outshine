#include "StructureSurfaceRefinement.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;

void Rectangle(Raised &mesh, float left, float right, float bottom, float top) {
  const std::array<Vec3f, 6> corners{{{{left, bottom, 0}},
                                      {{right, bottom, 0}},
                                      {{right, top, 0}},
                                      {{left, bottom, 0}},
                                      {{right, top, 0}},
                                      {{left, top, 0}}}};
  for (const auto &corner : corners) {
    mesh.RoofRun.push_back(static_cast<uint32_t>(mesh.RoofCorners.size()));
    mesh.RoofCorners.push_back({.pos = corner});
  }
}

bool Revoked(StructureSurfaceRefinementTask &task) {
  const auto progress = task.Step(7, {.MaxWorkUnits = 1});
  return !task.Bound(7) && !progress && progress.error() == StructureSurfaceErrorFailure::Cancelled;
}

bool Same(const StructureSurfaceRefinementTask &a, const StructureSurfaceRefinementTask &b) {
  const auto x = a.Bound(7);
  const auto y = b.Bound(7);
  return a.WorkUnits() == b.WorkUnits() && a.TriangleQueries() == b.TriangleQueries() &&
         a.Splits() == b.Splits() && a.RegionCount() == b.RegionCount() &&
         x.has_value() == y.has_value() &&
         (!x || (x->Upper.SourceKey == y->Upper.SourceKey &&
                 x->Upper.ReferenceToVariantM == y->Upper.ReferenceToVariantM &&
                 x->Upper.VariantToReferenceM == y->Upper.VariantToReferenceM &&
                 x->ReferenceToVariantLowerM == y->ReferenceToVariantLowerM &&
                 x->VariantToReferenceLowerM == y->VariantToReferenceLowerM));
}
}

int main() {
  using namespace outshine::Test;
  Raised cap;
  Raised frame;
  Raised empty;
  Rectangle(cap, 0, 3, 0, 3);
  Rectangle(frame, 0, 1, 0, 3);
  Rectangle(frame, 2, 3, 0, 3);
  Rectangle(frame, 1, 2, 0, 1);
  Rectangle(frame, 1, 2, 2, 3);
  StructureSurfaceRefinementTask moving;
  CHECK(moving.Reset({cap, frame, 7}), "opening task starts");
  auto baseline = moving;
  bool complete = false;
  bool equal = true;
  bool reusable = true;
  bool safe = true;
  size_t transfers = 0;
  for (; transfers < 300000; ++transfers) {
    auto destination = std::move(moving);
    const bool revoked = Revoked(moving);
    CHECK(revoked, "move construction revokes the source before it can advance an empty heap");
    if (!revoked) { return Report(); }
    moving.Cancel();
    if (transfers == 100 || transfers == 300) {
      reusable &= moving.Reset({frame, cap, 17}).has_value() &&
                  moving.Step(17, {.MaxWorkUnits = 100}).has_value();
    } else {
      reusable &= moving.Reset({empty, empty, 17}).has_value() && moving.Bound(17) &&
                  moving.Bound(17)->UpperDistanceM() == 0;
    }
    moving = std::move(destination);
    const bool assignmentRevoked = Revoked(destination);
    CHECK(assignmentRevoked, "move assignment revokes its source and replaces prior progress");
    if (!assignmentRevoked) { return Report(); }
    auto *self = &moving;
    moving = std::move(*self);
    equal &= Same(moving, baseline);
    const auto actual = moving.Step(7, {.MaxWorkUnits = 1});
    const auto expected = baseline.Step(7, {.MaxWorkUnits = 1});
    equal &= actual == expected && Same(moving, baseline);
    if (const auto bound = moving.Bound(7)) {
      safe &= bound->LowerDistanceM() <= 0.5 && bound->UpperDistanceM() >= 0.5;
    }
    if (!actual || !expected || !equal || !reusable || !safe) { break; }
    if (*actual == StructureSurfaceErrorProgress::Complete) {
      complete = true;
      break;
    }
  }
  CHECK(complete && transfers + 1 == moving.WorkUnits() && moving.TriangleQueries() > 0 &&
            moving.Splits() > 0,
        "single-unit transfers cover validation, seeds, split preparation and partial children");
  CHECK(equal, "copy and every transfer preserve the unshifted task's exact progress and bounds");
  CHECK(reusable, "moved-from tasks can cancel, reset and prepare independent inputs");
  CHECK(safe, "all transferred opening certificates contain the independent 0.5-metre distance");
  auto completed = std::move(moving);
  CHECK(Revoked(moving) && Same(completed, baseline), "completed proof transfers and revokes");
  moving = std::move(completed);
  CHECK(Revoked(completed) && Same(moving, baseline), "completed assignment retains the proof");
  auto *self = &moving;
  moving = std::move(*self);
  CHECK(Same(moving, baseline), "completed self-move retains the certificate");
  StructureSurfaceRefinementTask invalid;
  auto movedInvalid = std::move(invalid);
  const auto initialFailure = movedInvalid.Step(7);
  CHECK(Revoked(invalid) && !initialFailure &&
            initialFailure.error() == StructureSurfaceErrorFailure::InvalidSource,
        "uninitialized state moves with its original failure");
  CHECK(!moving.Step(8), "source change revokes the completed reference");
  completed = std::move(moving);
  const auto sourceFailure = completed.Step(7);
  CHECK(Revoked(moving) && !sourceFailure &&
            sourceFailure.error() == StructureSurfaceErrorFailure::SourceChanged,
        "failed progress transfers its failure without reviving coverage");
  CHECK(!moving.Reset({cap, frame, 7}, {.MaxRegions = 0}), "invalid budget fails reset");
  completed = std::move(moving);
  const auto budgetFailure = completed.Step(7);
  CHECK(Revoked(moving) && !budgetFailure &&
            budgetFailure.error() == StructureSurfaceErrorFailure::InvalidBudget,
        "move assignment preserves a reset failure");
  return Report();
}
