#include "BuildingMesh.h"
#include "Check.h"
#include "Geodesy.h"
#include "StructurePlanSelection.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

namespace {
class ObservedMesher final : public outshine::StructureMesher {
public:
  std::unique_ptr<outshine::MeshScratch> Scratch() const override { return Native.Scratch(); }

  std::optional<outshine::Box>
  SourceEnvelopeBounds(const outshine::StructurePlan &plan,
                       outshine::MeshScratch &scratch) const noexcept override {
    return Native.SourceEnvelopeBounds(plan, scratch);
  }

  std::optional<double> ShellSurfaceErrorM(const outshine::StructurePlan &plan,
                                           outshine::MeshScratch &scratch) const noexcept override {
    return Native.ShellSurfaceErrorM(plan, scratch);
  }

  std::expected<void, outshine::StructureMeshError>
  Mesh(const outshine::StructurePlan &plan,
       outshine::MeshScratch &scratch,
       outshine::Raised &into) const noexcept override {
    Recessed = plan.RecessedOpenings;
    Detail = plan.Coarseness;
    ++Calls;
    return Native.Mesh(plan, scratch, into);
  }

  outshine::Generators::BuildingMesh Native;
  mutable bool Recessed = false;
  mutable outshine::LevelOfDetail Detail = outshine::LevelOfDetail::Massed;
  mutable size_t Calls = 0;
};

void CheckAt(double latitude, bool expectedRecess) {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const std::array ring{
      latitude, 9.0, latitude, 9.0001, latitude + 0.0001, 9.0001, latitude + 0.0001, 9.0};
  RawTile raw;
  raw.TileSpanM = 16000;
  raw.Eye = {.LongitudeDeg = 9, .LatitudeDeg = 47};
  raw.Projection = {.FocalPx = 691.554, .AllowedErrorPx = 1};
  GeoToEcef({.LongitudeDeg = 9, .LatitudeDeg = 47}, raw.AnchorEcef);
  raw.EyeEcef = raw.AnchorEcef;
  StructurePlan plan;
  plan.RingLatLon = ring;
  plan.RingPointsLatLon = ring;
  plan.HeightM = 12;
  plan.HeightMeasured = true;
  plan.AnchorEcef = raw.AnchorEcef;
  plan.Facade = FacadeStyle::Block;
  ObservedMesher mesher;
  auto scratch = mesher.Scratch();
  const auto bounds = mesher.SourceEnvelopeBounds(plan, *scratch);
  CHECK(bounds.has_value(), "native building bounds are available before detail emission");
  if (!bounds) { return; }
  Vec3 offset{};
  for (size_t axis = 0; axis < 3; ++axis) {
    offset[axis] = std::clamp(0.0, bounds->Min[axis], bounds->Max[axis]);
  }
  const double distance = std::hypot(offset[0], offset[1], offset[2]);
  CHECK(StructureSelectionView{}.EyeRadiusM == 64.0,
        "translation reuse is limited to the promised 64 metres");
  CHECK(expectedRecess || raw.Projection.Allows(0.16, distance - 64.0),
        "omitted recesses remain subpixel after the full allowed movement toward the wall");
  CHECK(expectedRecess || !raw.Projection.Allows(0.16, distance - 128.0),
        "this case distinguishes actual reuse from the old doubled reserve");
  StructurePlanSelection selection;
  CHECK(selection
            .Add(plan,
                 {.LowLat = latitude,
                  .HighLat = latitude + 0.0001,
                  .LowLon = 9.0,
                  .HighLon = 9.0001,
                  .Count = 1,
                  .Cell = 1},
                 0,
                 mesher,
                 *scratch)
            .has_value(),
        "the native surface error is available");
  BakedTile out;
  out.FootprintDetails.resize(1);
  CHECK(selection.Select(raw, mesher, *scratch, out, nullptr).has_value(),
        "building detail is selected without emitting geometry");
  const auto emitted = selection.Emit(raw, mesher, *scratch, out, 1, nullptr);
  CHECK(emitted && *emitted && mesher.Calls == 1, "the selected native building is emitted");
  CHECK(mesher.Detail == LevelOfDetail::Fine, "the structural envelope remains Fine");
  CHECK(mesher.Recessed == expectedRecess, "only screen relevant opening depth is expanded");
}
}

int main() {
  CheckAt(47.0018, false);
  CheckAt(47.0012, true);
  return outshine::Test::Report();
}
