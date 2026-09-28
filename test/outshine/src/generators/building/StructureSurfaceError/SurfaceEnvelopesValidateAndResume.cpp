#include "StructureSurfaceError.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace {

void AddTriangle(outshine::Raised &mesh, std::array<outshine::Vec3f, 3> points, bool roof = false) {
  auto &vertices = roof ? mesh.RoofCorners : mesh.WallCorners;
  auto &indices = roof ? mesh.RoofRun : mesh.WallRun;
  for (const auto &point : points) {
    indices.push_back(static_cast<uint32_t>(vertices.size()));
    vertices.push_back({.pos = point});
  }
}

void AddRectangle(outshine::Raised &mesh, float left, float right, float bottom, float top) {
  AddTriangle(mesh, {{{{left, bottom, 0}}, {{right, bottom, 0}}, {{right, top, 0}}}}, true);
  AddTriangle(mesh, {{{{left, bottom, 0}}, {{right, top, 0}}, {{left, top, 0}}}}, true);
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  Raised reference;
  Raised variant;
  AddTriangle(reference, {{{{0, 0, 3}}, {{2, 0, 3}}, {{0, 2, 3}}}});
  AddTriangle(variant, {{{{0, 0, 0}}, {{2, 0, 0}}, {{0, 2, 0}}}}, true);
  StructureSurfaceErrorTask task;
  CHECK(task.Reset({.Reference = reference, .Variant = variant, .SourceKey = 7}),
        "same-source native wall and roof streams start a bounded comparison");
  const auto paused = task.Step(7, {.MaxCorners = 0});
  CHECK(paused && *paused == StructureSurfaceErrorProgress::Pending && task.WorkUnits() == 0,
        "zero work budget never performs hidden geometry work");
  for (size_t step = 0; step < 12; ++step) {
    const size_t before = task.WorkUnits();
    const auto progress = task.Step(7, {.MaxCorners = 1});
    CHECK(progress && task.WorkUnits() == before + 1,
          "each one-unit slice validates or bounds exactly one indexed corner");
    CHECK((step == 11) == task.Bound(7).has_value(),
          "certificate appears only after both complete validation and distance passes");
  }
  const auto bound = task.Bound(7);
  const double envelope = std::sqrt(13.0);
  CHECK(bound && bound->ReferenceToVariantM >= envelope &&
            bound->ReferenceToVariantM < envelope + 1e-12 &&
            bound->VariantToReferenceM >= envelope && task.VertexDistanceQueries() == 6,
        "independent parallel-plane envelope bounds both directed distances");
  CHECK(bound && bound->UpperDistanceM() >= 3,
        "complete triangle interiors remain enclosed, not only inspected vertices");
  CHECK(!task.Bound(8), "certificate cannot be read for a different source key");
  const auto changed = task.Step(8, {.MaxCorners = 1});
  CHECK(!changed && changed.error() == StructureSurfaceErrorFailure::SourceChanged &&
            !task.Bound(7),
        "source replacement invalidates even a completed certificate");

  Raised cap;
  Raised frame;
  AddRectangle(cap, 0, 3, 0, 3);
  AddRectangle(frame, 0, 1, 0, 3);
  AddRectangle(frame, 2, 3, 0, 3);
  AddRectangle(frame, 1, 2, 0, 1);
  AddRectangle(frame, 1, 2, 2, 3);
  CHECK(task.Reset({.Reference = cap, .Variant = frame, .SourceKey = 9}), "opening case starts");
  const auto opening = task.Step(9, {.MaxCorners = 128});
  const auto openingBound = task.Bound(9);
  CHECK(opening && *opening == StructureSurfaceErrorProgress::Complete && openingBound &&
            openingBound->ReferenceToVariantM >= 0.5 && openingBound->UpperDistanceM() > 0,
        "cap corners coincide with frame but its filled opening still has nonzero surface error");
  CHECK(task.Reset({.Reference = frame, .Variant = cap, .SourceKey = 10}),
        "reversed input direction starts independently");
  CHECK(task.Step(10, {.MaxCorners = 128}) && task.Bound(10) && openingBound &&
            task.Bound(10)->UpperDistanceM() == openingBound->UpperDistanceM(),
        "input reversal swaps directed bounds without changing the maximum");

  CHECK(task.Reset({.Reference = cap, .Variant = frame, .SourceKey = 11}), "restart is reusable");
  CHECK(task.Step(11, {.MaxCorners = 1}) && !task.Bound(11),
        "incomplete input coverage cannot certify a bound");
  task.Cancel();
  const auto cancelled = task.Step(11, {.MaxCorners = 128});
  CHECK(!cancelled && cancelled.error() == StructureSurfaceErrorFailure::Cancelled &&
            !task.Bound(11),
        "cancelled work cannot publish a certificate");

  Raised broken = variant;
  broken.RoofRun.back() = 100;
  CHECK(task.Reset({.Reference = reference, .Variant = broken, .SourceKey = 12}),
        "index validation remains paced rather than scanning inside Reset");
  const auto invalid = task.Step(12, {.MaxCorners = 128});
  CHECK(!invalid && invalid.error() == StructureSurfaceErrorFailure::InvalidGeometry &&
            !task.Bound(12) && task.VertexDistanceQueries() == 0,
        "a late invalid target index fails before any proof is exposed");
  broken = variant;
  broken.RoofCorners.back().pos[2] = std::numeric_limits<float>::quiet_NaN();
  CHECK(task.Reset({.Reference = reference, .Variant = broken, .SourceKey = 13}),
        "nonfinite corner case starts paced validation");
  CHECK(!task.Step(13, {.MaxCorners = 128}) && !task.Bound(13),
        "referenced nonfinite corners fail closed");
  broken = variant;
  broken.RoofRun.pop_back();
  CHECK(!task.Reset({.Reference = reference, .Variant = broken, .SourceKey = 14}),
        "incomplete triangle index runs are refused at the boundary");
  CHECK(task.Reset({.Reference = reference, .Variant = variant, .SourceKey = 17}),
        "copy and move case captures the native input pair");
  CHECK(task.Step(17, {.MaxCorners = 2}) && !task.Bound(17),
        "partially validated task is copyable");
  auto copied = task;
  auto moved = std::move(copied);
  CHECK(task.Step(17, {.MaxCorners = 128}) && moved.Step(17, {.MaxCorners = 128}) &&
            task.Bound(17) && moved.Bound(17) &&
            task.Bound(17)->UpperDistanceM() == moved.Bound(17)->UpperDistanceM() &&
            task.WorkUnits() == moved.WorkUnits(),
        "copy and move preserve independent progress over the same immutable geometry");
  Raised empty;
  CHECK(!task.Reset({.Reference = reference, .Variant = empty, .SourceKey = 15}),
        "empty and nonempty surfaces never imply zero error");
  CHECK(task.Reset({.Reference = empty, .Variant = empty, .SourceKey = 16}) && task.Bound(16) &&
            task.Bound(16)->UpperDistanceM() == 0,
        "two empty surfaces have exact zero geometric error");
  CHECK(!task.Reset({.Reference = empty, .Variant = empty, .SourceKey = 0}) && !task.Bound(0),
        "unspecified source identity never receives certification");
  return Report();
}
