#include "PreparedStructureTile.h"
#include "PreparedStructureCodec.h"
#include "BuildingSurfaceBlock.h"
#include "BuildingMesh.h"
#include "BuildingScratch.h"
#include "HeightField.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

template <typename T> bool Equal(const std::vector<T> &a, const std::vector<T> &b) {
  return std::ranges::equal(std::as_bytes(std::span(a)), std::as_bytes(std::span(b)));
}

std::expected<PreparedStructureTile, StructureBakeError> Prepare() {
  RawTile raw;
  raw.TileSpanM = 1000;
  GeoToEcef({.LongitudeDeg = 16, .LatitudeDeg = 48, .HeightM = 100}, raw.AnchorEcef);
  for (uint32_t at = 0; at < 3; ++at) {
    const double lon = 16.001 + at * 0.001;
    raw.LatLon.insert(raw.LatLon.end(),
                      {48.001, lon, 48.001, lon + .0002, 48.0012, lon + .0002, 48.0012, lon});
    raw.Structures.push_back({.LocalFirst = at * 4,
                              .PointCount = 4,
                              .SourceFirst = 100 + at * 4,
                              .Cell = {.Index = 1, .Footprint = {}},
                              .HeightM = 30.0 + at,
                              .Pitched = 0,
                              .WallColour = {},
                              .Facade = {}});
  }
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes = {100, 100, 100, 100};
  const auto heights = Ground::HeightField::Of(0, {block});
  if (!heights) { return std::unexpected(StructureBakeErrorKind::InvalidHeightInput); }
  return PrepareStructureTile(raw, *heights);
}
}

int main() {
  auto base = Prepare();
  CHECK(base, "three prepared sources share one physical storage cell");
  if (!base) { return Report(); }
  RawTile view;
  view.RequestedDetail = LevelOfDetail::Fine;
  view.RequestedCell = 1;
  BuildingScratch scratch;
  const BuildingMesh mesher;
  auto single = *base;
  single.Structures = {base->Structures[1]};
  single.Surfaces = {base->Surfaces[1]};
  const auto reference = BakePreparedStructures(single, view, mesher, scratch);
  const auto bytes = EncodePreparedStructureIndex(*base);
  CHECK(reference && bytes, "the independent single-source reference and lazy headers exist");
  if (!reference || !bytes) { return Report(); }
  auto index = DecodePreparedStructureIndex(*bytes, size_t{4} * 1024 * 1024);
  CHECK(index, "prepared selection headers restore without their forms");
  if (!index) { return Report(); }
  size_t opened = 0;
  const auto selected = std::make_shared<BuildingSurfaceBlock>([&] {
    ++opened;
    return std::expected<std::vector<BuildingSurface>, StructureMeshError>{{base->Surfaces[1]}};
  });
  size_t unselected = 0;
  const auto poison = std::make_shared<BuildingSurfaceBlock>([&] {
    ++unselected;
    return std::expected<std::vector<BuildingSurface>, StructureMeshError>{
        std::unexpected(StructureMeshError::PreparedSurfaceUnavailable)};
  });
  for (uint32_t at = 0; at < 3; ++at) {
    index->Surfaces[at].BindShapes(at == 1 ? selected : poison, 0);
  }
  const std::array<uint32_t, 1> sources{1};
  const auto group = BakePreparedStructureSources(*index, view, sources, mesher, scratch);
  CHECK(group && opened == 1 && unselected == 0,
        "only the exact requested source opens its form, despite shared cell ownership");
  if (!group) { return Report(); }
  CHECK(group->SourceCount == 3 && std::ranges::equal(group->Sources, sources) &&
            group->AnchorEcef == base->AnchorEcef && group->Detail == view.RequestedDetail,
        "the render group retains exact source ordinals, frame and detail without cell coverage");
  const auto &actual = group->Mesh;
  const auto &expected = reference->Built;
  CHECK(Equal(actual.WallCorners, expected.WallCorners) &&
            Equal(actual.RoofCorners, expected.RoofCorners) && actual.WallRun == expected.WallRun &&
            actual.RoofRun == expected.RoofRun,
        "native selected geometry equals the independent single-source bake");
  view.RequestedDetail.reset();
  view.Eye = {.LongitudeDeg = 16, .LatitudeDeg = 48};
  view.EyeEcef = base->AnchorEcef;
  view.Projection = {.FocalPx = 1000, .AllowedErrorPx = 0};
  const auto projectedReference = BakePreparedStructures(single, view, mesher, scratch);
  const auto projected = BakePreparedStructureSources(*index, view, sources, mesher, scratch);
  CHECK(projected && projectedReference &&
            Equal(projected->Mesh.WallCorners, projectedReference->Built.WallCorners) &&
            Equal(projected->Mesh.RoofCorners, projectedReference->Built.RoofCorners) &&
            projected->Mesh.WallRun == projectedReference->Built.WallRun &&
            projected->Mesh.RoofRun == projectedReference->Built.RoofRun &&
            projected->SelectedView.has_value() == projectedReference->SelectedView.has_value(),
        "view-dependent selection and emission retain the single-source geometry contract");
  view.RequestedDetail = LevelOfDetail::Fine;
  CHECK(!StructureBakeProgress::ForSources(std::array<uint32_t, 2>{1, 1}) &&
            !StructureBakeProgress::ForSources(std::array<uint32_t, 2>{2, 1}) &&
            !StructureBakeProgress::ForSources({}),
        "empty, duplicate and unordered source manifests are refused");
  CHECK(!BakePreparedStructureSources(*index, view, std::array<uint32_t, 1>{3}, mesher, scratch),
        "an out-of-range ordinal fails before form access");
  view.RequestedCell = 2;
  CHECK(!BakePreparedStructureSources(*index, view, sources, mesher, scratch),
        "a source manifest cannot silently lose members through a conflicting cell filter");
  view.RequestedCell = 1;
  const std::atomic_bool stopped{true};
  const auto cancelled =
      BakePreparedStructureSources(*index, view, sources, mesher, scratch, &stopped);
  CHECK(!cancelled && cancelled.error() == StructureBakeError{StructureBakeErrorKind::Cancelled} &&
            opened == 1 && unselected == 0,
        "invalid or cancelled work neither opens neighbours nor publishes partial coverage");
  return Report();
}
