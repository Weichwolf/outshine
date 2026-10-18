#include "WaterAsset.h"
#include "WaterSurfaceMesh.h"
#include "Check.h"
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  WaterAsset source(
      {.Surfaces = {{.FirstRing = 0, .RingCount = 1, .LevelM = 999},
                    {.FirstRing = 1, .RingCount = 1, .LevelM = 888}},
       .Rings = {{.FirstPoint = 0, .PointCount = 3}, {.FirstPoint = 3, .PointCount = 3}},
       .Points = {-.001, -.001, -.001, .001, .001, 0, -.001, .003, -.001, .005, .001, .004},
       .Tiles = {{.Zoom = 0, .X = 0, .Y = 0, .FirstSurface = 0, .SurfaceCount = 2}},
       .ProcessedTiles = 1});
  CHECK(!source.SurfacesReady() && source.LevelAt({}) == std::optional(999.),
        "source water retains its prior before a contact mesh is prepared");
  constexpr double kHeight = .123456789123;
  const auto mesh = WaterSurfaceMesh::Build(
      {},
      {{{-100, -100, kHeight}}, {{100, -100, kHeight + .2}}, {{0, 100, kHeight + .1}}},
      {{.Vertices = {0, 1, 2}, .Surface = 0}});
  CHECK(mesh, "canonical fitted facet is available");
  if (!mesh) { return Report(); }
  const auto prepared = source.WithSurfaces({.Mesh = *mesh, .Rejections = {{.Surface = 1}}});
  CHECK(prepared && prepared->SurfacesReady(),
        "each original surface has facets or an explicit local omission");
  if (!prepared) { return Report(); }
  CHECK(prepared->Points().data() == source.Points().data() &&
            prepared->Surfaces().data() == source.Surfaces().data(),
        "preparing geometry shares immutable source data without copying it");
  const auto level = prepared->LevelAt({});
  CHECK(level && std::abs(*level - (kHeight + .1)) < 1e-12,
        "contact queries use fitted Float64 facets rather than the prior flat level");
  CHECK(!prepared->LevelAt({.LongitudeDeg = .004}),
        "an omitted surface has no invisible flat water contact");
  const auto bytes = prepared->EncodeNative(16384);
  CHECK(bytes, "ready surfaces fit the versioned water record");
  if (!bytes) { return Report(); }
  const auto restored = WaterAsset::DecodeNative(*bytes);
  CHECK(restored && restored->SurfacesReady() && restored->LevelAt({}) == level &&
            restored->PreparedSurfaces()->Rejections.size() == 1 &&
            restored->PreparedSurfaces()->Rejections[0].Surface == 1,
        "native cache replay preserves ready facets, precise contacts and omitted source IDs");
  CHECK(!source.WithSurfaces({.Mesh = *mesh}), "missing source outcomes are rejected");
  CHECK(!source.WithSurfaces({.Mesh = *mesh, .Rejections = {{.Surface = 0}, {.Surface = 1}}}),
        "a surface cannot be simultaneously rendered and refused");
  CHECK(!source.WithSurfaces({.Mesh = *mesh, .Rejections = {{.Surface = 1}, {.Surface = 1}}}),
        "duplicate source refusals are rejected");
  CHECK(!source.WithSurfaces({.Mesh = *mesh, .Rejections = {{.Surface = 2}}}),
        "a refusal must identify an existing source surface");
  const auto unknownReason = static_cast<WaterAsset::SurfaceRejectionReason>(255);
  CHECK(!source.WithSurfaces(
            {.Mesh = *mesh, .Rejections = {{.Surface = 1, .Reason = unknownReason}}}),
        "unnamed reason values are not accepted as omission evidence");
  const auto empty = WaterAsset{}.WithSurfaces({});
  CHECK(empty && empty->SurfacesReady() && empty->HeapBytes() > 0 && !empty->LevelAt({}),
        "a prepared empty water asset accounts for owned state and has no water contacts");
  return Report();
}
