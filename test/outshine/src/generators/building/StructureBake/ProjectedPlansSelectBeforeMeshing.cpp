#include "BuildingMesh.h"
#include "StructureBake.h"
#include "Check.h"
#include "Geodesy.h"
#include "math/Units.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <memory>

namespace {
class CountingMesher final : public outshine::StructureMesher {
public:
  std::unique_ptr<outshine::MeshScratch> Scratch() const override { return Native.Scratch(); }

  std::expected<void, outshine::StructureMeshError>
  Mesh(const outshine::StructurePlan &plan,
       outshine::MeshScratch &scratch,
       outshine::Raised &out) const noexcept override {
    ++Calls;
    return Native.Mesh(plan, scratch, out);
  }

  std::optional<outshine::Box>
  SourceEnvelopeBounds(const outshine::StructurePlan &plan,
                       outshine::MeshScratch &scratch) const noexcept override {
    if (UnknownFirst && plan.HeightMeasured && plan.RingLatLon[0] == 47 &&
        plan.RingLatLon[1] == 9) {
      return std::nullopt;
    }
    return Native.SourceEnvelopeBounds(plan, scratch);
  }

  std::optional<double> ShellSurfaceErrorM(const outshine::StructurePlan &plan,
                                           outshine::MeshScratch &scratch) const noexcept override {
    return Native.ShellSurfaceErrorM(plan, scratch);
  }

  outshine::Generators::BuildingMesh Native;
  mutable size_t Calls = 0;
  bool UnknownFirst = false;
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  RawTile raw;
  raw.TileSpanM = 1000;
  raw.Projection.FocalPx = 720;
  const Ground::GeoBounds bounds{
      .MinLonDeg = 9, .MinLatDeg = 47, .MaxLonDeg = 9.02, .MaxLatDeg = 47.02};
  const double lonScale = kMPerDegLon * std::cos(47 * kDeg2Rad);
  for (uint32_t row = 0; row < 16; ++row) {
    for (uint32_t column = 0; column < 16; ++column) {
      const double lat = 47 + row * 20 / kMPerDegLat;
      const double lon = 9 + column * 20 / lonScale;
      const uint32_t first = static_cast<uint32_t>(raw.LatLon.size() / 2);
      const std::array ring{lat,
                            lon,
                            lat,
                            lon + 12 / lonScale,
                            lat + 16 / kMPerDegLat,
                            lon + 12 / lonScale,
                            lat + 16 / kMPerDegLat,
                            lon};
      const auto cell = StructureCellOf(bounds, ring);
      CHECK(cell, "each original source footprint has a cell");
      if (!cell) { return Report(); }
      raw.LatLon.insert(raw.LatLon.end(), ring.begin(), ring.end());
      raw.Structures.push_back(
          {.LocalFirst = first, .PointCount = 4, .Cell = *cell, .HeightM = 9, .Pitched = 0});
    }
  }
  GeoToEcef({.LongitudeDeg = 9, .LatitudeDeg = 47}, raw.AnchorEcef);
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 0.0f);
  const auto heights = Ground::HeightField::Of(0, {block});
  size_t previousTriangles = 0;
  for (const double longitude : {9.0, 9.03, 12.0}) {
    raw.Eye = {.LongitudeDeg = longitude, .LatitudeDeg = 47};
    CountingMesher mesher;
    auto scratch = mesher.Scratch();
    StructureBakeProgress progress;
    const auto planned = progress.AdvanceStructures(raw, *heights, mesher, *scratch, 256);
    CHECK(planned && !*planned && mesher.Calls == 0,
          "source planning and parent selection allocate no child mesh");
    bool complete = false;
    for (size_t slice = 0; slice < 256; ++slice) {
      const size_t before = mesher.Calls;
      const auto emitted = progress.AdvanceStructures(raw, *heights, mesher, *scratch, 1);
      CHECK(emitted && mesher.Calls <= before + 1, "an emission slice builds at most one product");
      if (!emitted) { return Report(); }
      if (*emitted) {
        complete = true;
        break;
      }
    }
    CHECK(complete, "bounded emission finishes");
    if (!complete) { return Report(); }
    auto built = progress.Finalize(raw, mesher, *scratch);
    CHECK(built && built->Prints.size() == 256 && built->NoGround == 0,
          "all original semantic footprints survive selection");
    if (!built) { return Report(); }
    const size_t triangles = (built->Built.WallRun.size() + built->Built.RoofRun.size()) / 3;
    CHECK(previousTriangles == 0 || triangles <= previousTriangles,
          "moving away never increases generated geometry in the same city");
    previousTriangles = triangles;
    if (longitude == 9.03) {
      CHECK(triangles == 256 * 12,
            "middle distance generates shells directly without Fine products");
    } else if (longitude == 12.0) {
      CHECK(mesher.Calls < 256 / 4 && built->Lumped == 256,
            "far parents replace individual source meshes before allocation");
    }
    std::printf("longitude %.2f: %zu mesh calls, %zu triangles, %d grouped sources\n",
                longitude,
                mesher.Calls,
                triangles,
                built->Lumped);
  }
  raw.Eye = {.LongitudeDeg = 9, .LatitudeDeg = 47};
  Vec3 elevatedEye;
  GeoToEcef({.LongitudeDeg = 9, .LatitudeDeg = 47, .HeightM = 80000}, elevatedEye);
  raw.EyeEcef = elevatedEye;
  CountingMesher elevatedMesher;
  auto elevatedScratch = elevatedMesher.Scratch();
  BakedTile elevated;
  CHECK(BakeStructures(raw, *heights, elevatedMesher, *elevatedScratch, elevated),
        "the demand also evaluates actual camera altitude");
  CHECK(elevated.Lumped == 256 && elevatedMesher.Calls < 64,
        "a camera above the city selects far parents without horizontal displacement");
  CHECK(elevated.SelectedView &&
            elevated.SelectedView->Contains(raw.Eye, raw.Projection, raw.EyeEcef),
        "the emitted view records its full spatial demand");
  elevatedEye[0] += 128;
  CHECK(elevated.SelectedView &&
            !elevated.SelectedView->Contains(raw.Eye, raw.Projection, elevatedEye),
        "position changes beyond the guarded reuse radius require a new selection");
  raw.Eye = {.LongitudeDeg = 12, .LatitudeDeg = 47};
  raw.EyeEcef.reset();
  CountingMesher mixed;
  mixed.UnknownFirst = true;
  auto mixedScratch = mixed.Scratch();
  BakedTile mixedProduct;
  CHECK(BakeStructures(raw, *heights, mixed, *mixedScratch, mixedProduct),
        "a mixed cell keeps its unbounded source alongside proven neighbours");
  CHECK(mixedProduct.Prints.size() == 256 &&
            mixedProduct.FootprintDetails.front() == LevelOfDetail::Fine &&
            mixedProduct.Lumped == 255 && mixed.Calls < 64,
        "one unknown source stays Fine without blocking distant aggregation of all known sources");
  return Report();
}
