#include "Address.h"
#include "Check.h"
#include "SourcedTerrainFields.h"
#include "TerrainCertificate.h"
#include "TerrainGrid.h"
#include "TileGeodesy.h"
#include "TileSourceIdentity.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace {
using namespace outshine;
constexpr Data::TileId kWest{.Zoom = 1, .X = 0, .Y = 0};
constexpr Data::TileId kEast{.Zoom = 1, .X = 1, .Y = 0};
constexpr Data::TileId kChild{.Zoom = 2, .X = 0, .Y = 0};
constexpr float kRaisedM = 40;
constexpr std::array<uint8_t, 32> kExpectedBinding{
    214, 33,  51,  61, 82,  112, 50,  166, 138, 49, 187, 240, 128, 255, 87,  105,
    121, 202, 245, 74, 244, 220, 175, 120, 29,  76, 115, 223, 67,  221, 166, 183};

std::shared_ptr<Ground::TerrainField>
Field(Data::TileId tile, float heightM, std::string revision = "r1") {
  auto field = std::make_shared<Ground::TerrainField>(2, 2);
  for (uint32_t row = 0; row < 2; ++row) {
    for (uint32_t column = 0; column < 2; ++column) { field->SetM(row, column, heightM); }
  }
  field->AddSource({.From = Data::TileSourceIdentity::Origin::Declared,
                    .Kind = Data::DataKind::Elevation,
                    .Tile = tile,
                    .SourceId = "terrain-fixture",
                    .Revision = std::move(revision)});
  return field;
}

void CheckStableBindings() {
  const auto west = Field(kWest, 0);
  const auto east = Field(kEast, kRaisedM);
  const SourcedTerrainFields scope({{kWest, west}, {kEast, east}});
  const auto binding = scope.BindingAt(1);
  const SourcedTerrainFields reordered({{kEast, Field(kEast, kRaisedM)}, {kWest, Field(kWest, 0)}});
  const auto same = reordered.BindingAt(1);
  CHECK(binding && same && binding->Key == same->Key,
        "field order and ownership addresses cannot change the same captured input binding");
  if (!binding) { return; }
  CHECK(binding->Key == kExpectedBinding && binding->MetadataBytes == 252,
        "canonical little endian fields match the independently encoded reference vector");
  constexpr size_t kRasterBytes = size_t{2} * 4u * sizeof(float);
  const size_t copied = std::endian::native == std::endian::little ? 0 : kRasterBytes;
  CHECK(binding->Fields == 2 && binding->RasterBytes == kRasterBytes &&
            binding->CopiedRasterBytes == copied && binding->MetadataBytes > 0,
        "binding work reports exact raster reads and avoids raster copies on little endian hosts");
  const SourcedTerrainFields raised({{kWest, Field(kWest, kRaisedM)}, {kEast, east}});
  const auto changed = raised.BindingAt(1);
  CHECK(changed && changed->Key != binding->Key,
        "changed terrain data invalidates elevation content");
  const SourcedTerrainFields revised({{kWest, Field(kWest, 0, "r2")}, {kEast, east}});
  const auto provenance = revised.BindingAt(1);
  CHECK(provenance && provenance->Key != binding->Key,
        "a newly selected source revision is a persistent input even at the same height");
  auto recertified = Field(kWest, 0);
  recertified->SetCertificate(Ground::TerrainCertificate::FromDelivery(kWest, std::nullopt, 7));
  const SourcedTerrainFields runtimeState({{kWest, recertified}, {kEast, east}});
  const auto restarted = runtimeState.BindingAt(1);
  CHECK(restarted && restarted->Key == binding->Key,
        "process-local terrain certification cannot salt persistent content identity");
  std::cout << "TERRAIN_BINDING fields=" << binding->Fields
            << " raster_bytes=" << binding->RasterBytes
            << " metadata_bytes=" << binding->MetadataBytes
            << " copied_raster_bytes=" << binding->CopiedRasterBytes << '\n';
}

void CheckSamplingDetail() {
  const auto parent = Field(kWest, 0);
  const auto child = Field(kChild, kRaisedM);
  const SourcedTerrainFields coarse({{kWest, parent}});
  const SourcedTerrainFields refined({{kChild, child}, {kWest, parent}});
  const auto original = coarse.BindingAt(1);
  const auto irrelevant = refined.BindingAt(1);
  CHECK(original && irrelevant && original->Key == irrelevant->Key &&
            original->RasterBytes == irrelevant->RasterBytes,
        "unused finer fields cause neither coarser invalidation nor extra raster hashing");
  const auto ancestor = coarse.BindingAt(2);
  const auto local = refined.BindingAt(2);
  const auto at = Ground::TileFracToGeo({.X = 0.25, .Y = 0.25}, 2);
  const LongitudeLatitude point{.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg};
  CHECK(ancestor && local && ancestor->Key != local->Key && coarse.AslMAt(2, point) == 0 &&
            refined.AslMAt(2, point) == kRaisedM,
        "a child replacing its sampled ancestor changes both binding and actual elevation");
  auto unsourced = std::make_shared<Ground::TerrainField>(2, 2);
  std::fill_n(unsourced->Data(), 4, kRaisedM);
  const SourcedTerrainFields sampled({{kChild, unsourced}, {kWest, parent}});
  const auto withoutProvenance = sampled.BindingAt(2);
  CHECK(
      withoutProvenance && ancestor && withoutProvenance->Key != ancestor->Key &&
          sampled.AslMAt(2, point) == kRaisedM && unsourced->Sources().empty(),
      "sampling binds unsourced finer data instead of silently substituting its sourced ancestor");
  const SourcedTerrainFields masked({{kChild, nullptr}, {kChild, child}, {kWest, parent}});
  const auto firstMatch = masked.BindingAt(2);
  CHECK(firstMatch && ancestor && firstMatch->Key == ancestor->Key && masked.AslMAt(2, point) == 0,
        "duplicate tile entries follow the sampler's first-match fallback semantics");
  const auto invalid = coarse.BindingAt(-1);
  const auto excessive = coarse.BindingAt(Data::TileId::MaximumZoom + 1);
  CHECK(!invalid && invalid.error() == SourcedTerrainFields::CaptureError::InvalidRequest &&
            !excessive && excessive.error() == SourcedTerrainFields::CaptureError::InvalidRequest,
        "invalid sampling zooms cannot acquire a persistent input binding");
  const auto empty = SourcedTerrainFields{}.BindingAt(1);
  CHECK(empty && empty->Fields == 0 && empty->RasterBytes == 0,
        "an empty captured view has a defined binding without fabricated terrain inputs");
}
}

int main() {
  CheckStableBindings();
  CheckSamplingDetail();
  return outshine::Test::Report();
}
