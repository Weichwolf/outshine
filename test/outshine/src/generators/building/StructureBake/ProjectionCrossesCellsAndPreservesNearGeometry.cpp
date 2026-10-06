#include "BuildingMesh.h"
#include "StructureBake.h"
#include "Geodesy.h"
#include "Check.h"

#include <algorithm>
#include <array>
#include <memory>

namespace {
class ObservedMesher final : public outshine::StructureMesher {
public:
  std::unique_ptr<outshine::MeshScratch> Scratch() const override { return Native.Scratch(); }

  bool HasSurfaceProjection() const noexcept override { return true; }

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
    ++MeshCalls;
    return Native.Mesh(plan, scratch, into);
  }

  std::expected<outshine::ProjectedStructureMesh, outshine::StructureMeshError>
  Project(std::span<const outshine::StructurePlan> plans,
          const outshine::Vec3 &eye,
          outshine::ProjectedErrorBudget projection,
          outshine::MeshScratch &scratch) const override {
    ++ProjectionCalls;
    ProjectedPlans = plans.size();
    for (const auto &plan : plans) {
      if (plan.Coarseness != outshine::LevelOfDetail::Fine) { continue; }
      ++FinePlans;
      if (!Native.Mesh(plan, scratch, FineReference)) {
        return std::unexpected(outshine::StructureMeshError::BuildFailed);
      }
    }
    return Native.Project(plans, eye, projection, scratch);
  }

  outshine::Generators::BuildingMesh Native;
  mutable size_t MeshCalls = 0, ProjectionCalls = 0, ProjectedPlans = 0, FinePlans = 0;
  mutable outshine::Raised FineReference;
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  RawTile raw;
  raw.TileSpanM = 16000;
  raw.Eye = {.LongitudeDeg = 9, .LatitudeDeg = 47};
  raw.Projection = {.FocalPx = 720, .AllowedErrorPx = 1};
  GeoToEcef({.LongitudeDeg = 9, .LatitudeDeg = 47}, raw.AnchorEcef);
  Vec3 eye;
  GeoToEcef({.LongitudeDeg = 9, .LatitudeDeg = 47, .HeightM = 40}, eye);
  raw.EyeEcef = eye;
  const Ground::GeoBounds bounds{
      .MinLonDeg = 9, .MinLatDeg = 47, .MaxLonDeg = 9.2, .MaxLatDeg = 47.2};
  for (size_t at = 0; at < 68; ++at) {
    const double lon = at < 4 ? 9.0002 : 9.08 + static_cast<double>((at - 4) % 8) * 0.004;
    const double lat = at < 4 ? 47.0002 + static_cast<double>(at) * 0.0002
                              : 47.01 + static_cast<double>((at - 4) / 8) * 0.004;
    const std::array ring{
        lat, lon, lat, lon + 0.0001, lat + 0.0001, lon + 0.0001, lat + 0.0001, lon};
    const auto cell = StructureCellOf(bounds, ring);
    CHECK(cell, "every source belongs to its semantic terrain cell");
    if (!cell) { return Report(); }
    const auto first = static_cast<uint32_t>(raw.LatLon.size() / 2);
    raw.LatLon.insert(raw.LatLon.end(), ring.begin(), ring.end());
    raw.Structures.push_back(
        {.LocalFirst = first, .PointCount = 4, .Cell = *cell, .HeightM = 9, .Pitched = 0});
  }
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 0.0f);
  const auto ground = Ground::HeightField::Of(0, {block});
  ObservedMesher mesher;
  auto scratch = mesher.Scratch();
  StructureBakeProgress progress;
  const auto planned = progress.AdvanceStructures(raw, *ground, mesher, *scratch, 68);
  CHECK(planned && !*planned && mesher.ProjectionCalls == 0 && mesher.MeshCalls == 0,
        "all-cell selection finishes before any render mesh is requested");
  const auto emitted = progress.AdvanceStructures(raw, *ground, mesher, *scratch, 1);
  CHECK(emitted && *emitted, "one projection command spans the entire mixed-detail tile");
  CHECK(mesher.ProjectionCalls == 1 && mesher.ProjectedPlans == 68 && mesher.FinePlans == 4,
        "cell boundaries do not split visibility and nearby buildings remain Fine");
  auto built = progress.Finalize(raw, mesher, *scratch);
  CHECK(built && built->Prints.size() == 68, "all source footprints survive projection");
  if (!built) { return Report(); }
  for (const auto &expected : mesher.FineReference.WallCorners) {
    CHECK(std::ranges::any_of(built->Built.WallCorners,
                              [&](const StoredVertex &vertex) {
                                return vertex.pos == expected.pos &&
                                       vertex.texture == expected.texture &&
                                       vertex.normWord == expected.normWord;
                              }),
          "every native near wall retains position, facade coordinates and normal");
  }
  CHECK(built->SelectedView && built->SelectedView->EyeRadiusM == 0,
        "the projected tile claims no unproved translation reuse");
  return Report();
}
