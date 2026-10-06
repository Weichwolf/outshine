#include "PreparedStructureTile.h"
#include "PreparedStructurePlan.h"
#include "BuildingScratch.h"
#include "BuildingMesh.h"
#include "Check.h"
#include "Geodesy.h"
#include "math/Units.h"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

RawTile Inputs() {
  RawTile raw;
  raw.TileSpanM = 1000;
  GeoToEcef({.LongitudeDeg = 9, .LatitudeDeg = 47}, raw.AnchorEcef);
  const Ground::GeoBounds region{
      .MinLonDeg = 9, .MinLatDeg = 47, .MaxLonDeg = 9.02, .MaxLatDeg = 47.02};
  for (uint32_t index = 0; index < 3; ++index) {
    const double lon = 9.001 + static_cast<double>(index) * 0.002;
    const std::array ring{47.001, lon, 47.001, lon + 0.0002, 47.0012, lon + 0.0002, 47.0012, lon};
    const auto cell = StructureCellOf(region, ring);
    CHECK(cell.has_value(), "prepared fixture has a native cell");
    if (!cell) { return {}; }
    const auto first = static_cast<uint32_t>(raw.LatLon.size() / 2);
    raw.LatLon.insert(raw.LatLon.end(), ring.begin(), ring.end());
    raw.Structures.push_back({.LocalFirst = first,
                              .PointCount = 4,
                              .SourceFirst = first,
                              .Cell = *cell,
                              .HeightM = index == 0 ? 0.0 : 12.0,
                              .MinimumHeightM = index == 2 ? 3.0 : 0.0,
                              .Pitched = 0});
  }
  raw.Ways.push_back({.LocalFirst = static_cast<uint32_t>(raw.LatLon.size() / 2),
                      .PointCount = 2,
                      .HalfWidthM = 3.0f});
  raw.LatLon.insert(raw.LatLon.end(), {47.00096, 9.0, 47.00096, 9.02});
  return raw;
}

std::shared_ptr<const Ground::HeightField> Heights() {
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes = {125.125f, 131.25f, 142.5f, 149.75f};
  return Ground::HeightField::Of(0, {block});
}

void ReadyForms(const PreparedStructureTile &base, const BuildingMesh &mesher) {
  for (size_t index = 0; index < base.Structures.size(); ++index) {
    const auto &record = base.Structures[index];
    const auto corners =
        std::span(base.CornerAslM).subspan(record.CornerFirst, record.Layout.PointCount);
    auto plan =
        PreparedStructurePlan(record, base.PointsLatLon, base.Holes, corners, base.AnchorEcef);
    plan.Prepared = &base.Surfaces[index];
    for (auto detail : {LevelOfDetail::Fine, LevelOfDetail::Shell, LevelOfDetail::Massed}) {
      plan.Coarseness = detail;
      BuildingScratch scratch;
      Raised mesh;
      CHECK(mesher.SourceEnvelopeBounds(plan, scratch) &&
                mesher.ShellSurfaceErrorM(plan, scratch) && mesher.Mesh(plan, scratch, mesh),
            "prepared native forms supply bounds, error and every mesh envelope");
      CHECK(scratch.Parts.Count() == 0, "ready house forms never repeat intrinsic shape planning");
    }
  }
}

}

int main() {
  auto raw = Inputs();
  auto heights = Heights();
  const auto base = PrepareStructureTile(raw, *heights);
  CHECK(base && base->Structures.size() == 3 && base->CornerAslM.size() == 12 &&
            base->Surfaces.size() == 3,
        "one cold pass resolves every source footprint and terrain corner");
  if (!base) { return Report(); }
  CHECK(base->Structures[0].Standing.HeightM > 0 &&
            base->Structures[0].Standing.Source == Ground::BuildingHeightSource::Generated &&
            base->Structures[0].Standing.Street.Known,
        "unknown height and road frontage are fully enriched before runtime selection");
  CHECK(base->Structures[2].Standing.MinimumHeightM == 3.0f,
        "prepared raised parts retain their clearance");
  BuildingMesh mesher;
  std::vector<RawTile> views;
  std::vector<BakedTile> references;
  for (const auto detail : {LevelOfDetail::Fine, LevelOfDetail::Shell, LevelOfDetail::Massed}) {
    RawTile view;
    view.RequestedDetail = detail;
    view.Eye = {.LongitudeDeg = 10, .LatitudeDeg = 47};
    views.push_back(view);
    auto original = raw;
    original.RequestedDetail = view.RequestedDetail;
    original.Eye = view.Eye;
    auto scratch = mesher.Scratch();
    BakedTile reference;
    CHECK(BakeStructures(original, *heights, mesher, *scratch, reference).has_value(),
          "cold native path supplies the geometry reference at every envelope");
    references.push_back(std::move(reference));
  }
  ReadyForms(*base, mesher);
  raw = {};
  heights.reset();
  for (size_t index = 0; index < views.size(); ++index) {
    auto scratch = mesher.Scratch();
    const auto built = BakePreparedStructures(*base, views[index], mesher, *scratch);
    CHECK(built && built->Digest == references[index].Digest &&
              built->Prints == references[index].Prints && built->NoGround == 0,
          "prepared bases reproduce cold geometry and contacts after source/height owners vanish");
    CHECK(built && built->Coordinates && built->Coordinates->Points == base->PointsLatLon,
          "runtime native footprints carry their own geometry rather than source-cache views");
  }
  std::atomic_bool stopping{true};
  auto scratch = mesher.Scratch();
  const auto cancelled = BakePreparedStructures(*base, views[0], mesher, *scratch, &stopping);
  CHECK(!cancelled && cancelled.error() == StructureBakeError(StructureBakeErrorKind::Cancelled),
        "prepared native generation still obeys cancellation");
  return Report();
}
