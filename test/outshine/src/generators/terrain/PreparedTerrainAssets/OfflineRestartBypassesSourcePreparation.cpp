#include "Check.h"
#include "PreparedTerrainAssets.h"
#include "PreparedTerrainCodec.h"
#include "ByteArchive.h"
#include "SourceSet.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <utility>

namespace {
using namespace outshine;

class Fields final : public Ground::TerrainSource {
public:
  Ground::TerrainBytes Take(Data::TileId at) override {
    ++Calls;
    if (!Available) {
      return Ground::TerrainBytes::Wire(
          Data::FetchFailure{.Requested = Data::Address::At(at),
                             .SourceId = "fixture",
                             .Reason = Data::FetchFailureReason::OfflineMiss});
    }
    Ground::TerrainField field(5, 5);
    for (uint32_t row = 0; row < 5; ++row) {
      for (uint32_t col = 0; col < 5; ++col) {
        field.SetM(row, col, static_cast<float>(100.0 + at.X * 2.0 + at.Y * 4.0 + col * 0.5 + row));
      }
    }
    return Ground::TerrainBytes::From(at,
                                      std::move(field),
                                      {.From = Data::TileSourceIdentity::Origin::Provider,
                                       .Kind = Data::DataKind::Elevation,
                                       .Tile = at,
                                       .NativeCell = Data::CellId{.SouthDeg = 45, .WestDeg = 8},
                                       .SourceId = "fixture",
                                       .Revision = "fixture-v1"});
  }

  size_t Calls = 0;
  bool Available = true;
};

Ground::TerrainTiles::Config
Resolver(const std::shared_ptr<Generators::PreparedTerrainAssets> &assets) {
  return {.PreparedFields = [assets](Data::TileId at,
                                     const Ground::TerrainTiles::Shaped &shape,
                                     const Ground::TerrainTiles::FieldFactory &factory) {
    return assets->Resolve(at, shape, factory);
  }};
}

bool Equal(const Ground::TerrainField &first, const Ground::TerrainField &second) {
  if (first.Rows() != second.Rows() || first.Cols() != second.Cols() ||
      first.HasMissingBoundary() != second.HasMissingBoundary() ||
      !std::ranges::equal(first.Sources(), second.Sources())) {
    return false;
  }
  const size_t nodes = static_cast<size_t>(first.Rows()) * first.Cols();
  return std::ranges::equal(std::span(first.Data(), nodes), std::span(second.Data(), nodes));
}

}

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-native-terrain-test-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Data::ContentStore store({.Directory = (root / "sources").string()});
  Data::SourceSet sources(store);
  constexpr Data::TileId at{.Zoom = 3, .X = 3, .Y = 3};
  Fields original;
  Ground::TerrainTiles plain(original, Ground::EnuFrame::At(Ground::Geo{}), {});
  auto reference = plain.StitchedGrid(at.Zoom, at.X, at.Y);
  CHECK(reference.TryField() != nullptr && original.Calls > 0,
        "cold reference performs source acquisition and edge/corner stitching");
  if (!reference.TryField()) { return outshine::Test::Report(); }
  auto opened = Generators::PreparedTerrainAssets::Open((root / "assets").string(), sources);
  CHECK(opened.has_value(), "native terrain uses the common persistent spatial cache");
  if (!opened) { return outshine::Test::Report(); }
  auto assets = std::move(*opened);
  {
    Ground::TerrainTiles cold(original, Ground::EnuFrame::At(Ground::Geo{}), Resolver(assets));
    auto prepared = cold.StitchedGrid(at.Zoom, at.X, at.Y);
    CHECK(prepared.TryField() && Equal(*prepared.TryField(), *reference.TryField()),
          "cache miss publishes and reloads every sample, boundary flag and source identity");
    auto held = prepared.ShareField();
    const auto calls = original.Calls;
    auto repeated = cold.StitchedGrid(at.Zoom, at.X, at.Y);
    CHECK(repeated.ShareField() == held && original.Calls == calls && assets->Costs().Resident == 1,
          "resident requests share the native field without IO, preparation or array copies");
    CHECK(assets->Costs().Writes == 1, "one completed miss writes one native terrain package");
  }
  assets.reset();
  opened = Generators::PreparedTerrainAssets::Open((root / "assets").string(), sources);
  CHECK(opened.has_value(), "fresh cache owner reopens the stored native package");
  if (!opened) { return outshine::Test::Report(); }
  assets = std::move(*opened);
  Fields offline;
  offline.Available = false;
  Ground::TerrainTiles warm(offline, Ground::EnuFrame::At(Ground::Geo{}), Resolver(assets));
  auto ready = warm.StitchedGrid(at.Zoom, at.X, at.Y);
  CHECK(ready.TryField() && Equal(*ready.TryField(), *reference.TryField()) && offline.Calls == 0,
        "offline restart resolves native fields before invoking any source or stitching factory");
  CHECK(assets->Costs().Hits == 1 && assets->Costs().Misses == 0 && assets->Costs().Writes == 0,
        "warm hit consumes exactly one stored package with no generation or rewrite");
  auto encoded = Generators::EncodePreparedTerrain(at, *reference.TryField());
  CHECK(encoded.has_value(), "native field codec accepts the complete reference");
  if (encoded) {
    CHECK(!Generators::DecodePreparedTerrain(at, *encoded, 1),
          "resident byte cap is enforced before allocation");
    CHECK(!Generators::DecodePreparedTerrain(
              {.Zoom = 3, .X = 4, .Y = 3}, *encoded, Generators::kPreparedTerrainBytesMost),
          "a native field cannot masquerade as a different spatial request");
    encoded->pop_back();
    CHECK(!Generators::DecodePreparedTerrain(at, *encoded, Generators::kPreparedTerrainBytesMost),
          "truncated native samples are refused");
  }
  auto missing = *reference.TryField();
  missing.MarkMissingBoundary();
  encoded = Generators::EncodePreparedTerrain(at, missing);
  const auto decoded =
      encoded
          ? Generators::DecodePreparedTerrain(at, *encoded, Generators::kPreparedTerrainBytesMost)
          : std::nullopt;
  CHECK(decoded && decoded->HasMissingBoundary(),
        "persistent replay does not promote missing boundary data to known geometry");
  Ground::TerrainTiles::Shaped changed;
  changed.Seed = 1;
  auto miss = assets->Resolve(at, changed, [] { return Ground::TerrainGrid::Deferred(); });
  CHECK(miss.Where() == Ground::TerrainGrid::State::Deferred && assets->Costs().Misses == 1 &&
            assets->Costs().Writes == 0,
        "changed generation parameters cause a real miss and pending work cannot publish a partial "
        "asset");
  const auto unknown = assets->Resolve(at, {}, [] { return Ground::TerrainGrid::Refused(); });
  CHECK(unknown.TryField() && offline.Calls == 0,
        "pending generation does not overwrite the existing complete asset");
  assets.reset();
  ready = Ground::TerrainGrid::NotHere();
  std::filesystem::remove_all(root);
  return outshine::Test::Report();
}
