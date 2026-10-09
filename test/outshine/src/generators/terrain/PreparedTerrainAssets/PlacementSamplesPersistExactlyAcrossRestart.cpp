#include "Check.h"
#include "PreparedTerrainAssets.h"
#include "PreparedGroundPatchCodec.h"
#include "SourceSet.h"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <filesystem>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-ground-patch-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Data::ContentStore store({.Directory = (root / "sources").string()});
  Data::SourceSet sources(store);
  constexpr Data::TileId at{.Zoom = 14, .X = 8937, .Y = 5683};
  const Tile region(at.Zoom, static_cast<int>(at.X), static_cast<int>(at.Y));
  const std::array<double, 9> values{123.45678901234567,
                                     -0.0,
                                     0.0,
                                     -11500.0,
                                     9500.0,
                                     80.0,
                                     0.000000001,
                                     -123.76543210987654,
                                     1234.12345};
  std::array<GroundPatch::Posting, 9> postings;
  for (size_t i = 0; i < values.size(); ++i) { postings[i].Height = GroundSample::At(values[i]); }
  const auto original = GroundPatch::Complete(region, 3, postings);
  CHECK(original != nullptr, "complete placement samples are a native product");
  if (!original) { return Test::Report(); }
  auto encoded = EncodePreparedGroundPatch(at, *original);
  CHECK(encoded && encoded->size() == 24 + values.size() * sizeof(double),
        "native sampling patch stores one spatial header and exact doubles, no source fields");
  auto decoded = encoded ? DecodePreparedGroundPatch(at, 3, *encoded) : nullptr;
  CHECK(decoded && decoded->Region() == at &&
            std::ranges::equal(values,
                               decoded->HeightsAslM(),
                               [](double a, double b) {
                                 return std::bit_cast<uint64_t>(a) == std::bit_cast<uint64_t>(b);
                               }),
        "all sample bits, including signed zero and Earth-height limits, survive the codec");
  if (encoded) {
    CHECK(!DecodePreparedGroundPatch(at, 4, *encoded), "another grid cannot use these samples");
    CHECK(!DecodePreparedGroundPatch({.Zoom = 14, .X = at.X + 1, .Y = at.Y}, 3, *encoded),
          "another geographic tile cannot load these samples");
    CHECK(!EncodePreparedGroundPatch({.Zoom = 14, .X = at.X + 1, .Y = at.Y}, *original),
          "same-latitude neighboring patches cannot be published under the wrong tile");
    auto broken = *encoded;
    broken.push_back(0);
    CHECK(!DecodePreparedGroundPatch(at, 3, broken), "trailing bytes are refused");
    broken = *encoded;
    broken.pop_back();
    CHECK(!DecodePreparedGroundPatch(at, 3, broken), "truncated samples are refused");
    broken = *encoded;
    std::fill_n(broken.begin() + 20, 4, uint8_t{255});
    CHECK(!DecodePreparedGroundPatch(at, 3, broken),
          "unbounded malicious sample count is refused before allocation");
    broken = *encoded;
    const auto invalid = std::bit_cast<uint64_t>(std::numeric_limits<double>::quiet_NaN());
    for (size_t i = 0; i < sizeof(double); ++i) {
      broken[24 + i] = static_cast<uint8_t>(invalid >> (8 * i));
    }
    CHECK(!DecodePreparedGroundPatch(at, 3, broken), "invalid Earth heights remain invalid");
  }
  auto opened = PreparedTerrainAssets::Open((root / "assets").string(), sources);
  CHECK(opened.has_value(), "sampling patches use the existing native asset cache");
  if (!opened) { return Test::Report(); }
  auto assets = std::move(*opened);
  const auto key = assets->PatchKey(at, {}, 3, 13);
  auto missing = assets->LoadPatch(key, at, 3);
  CHECK(missing && !*missing && assets->Costs().PatchMisses == 1,
        "absent samples are a miss, never fabricated geometry");
  CHECK(assets->StorePatch(key, at, *original).has_value() && assets->Costs().PatchWrites == 1,
        "a complete miss atomically publishes the native patch");
  CHECK(assets->PatchKey(at, {}, 4, 13) != key && assets->PatchKey(at, {}, 3, 12) != key,
        "grid size and source sampling resolution are independent request parameters");
  outshine::Ground::TerrainTiles::Shaped shape;
  shape.Seed = 1;
  CHECK(assets->PatchKey(at, shape, 3, 13) != key,
        "terrain generation parameters separate placement products");
  assets.reset();
  opened = PreparedTerrainAssets::Open((root / "assets").string(), sources);
  CHECK(opened.has_value(), "fresh cache owner reopens the persisted sampling patch");
  if (!opened) { return Test::Report(); }
  assets = std::move(*opened);
  auto loaded = assets->LoadPatch(key, at, 3);
  CHECK(loaded && *loaded && std::ranges::equal((*loaded)->HeightsAslM(), values) &&
            assets->Costs().PatchHits == 1 && assets->Costs().PatchMisses == 0 &&
            assets->Costs().PatchWrites == 0 && assets->Costs().PatchReadBytes == encoded->size(),
        "restart loads only the small ready sampling asset without native height-field reads");
  if (loaded && *loaded) {
    for (const auto point : std::array<EastNorth, 3>{
             EastNorth{},
             {.EastM = region.SpanEm() * .371, .NorthM = region.SpanNm() * .623},
             {.EastM = region.SpanEm(), .NorthM = region.SpanNm()}}) {
      CHECK((*loaded)->HeightAslM(point) == original->HeightAslM(point) &&
                (*loaded)->GradientAt(point).PerEastM == original->GradientAt(point).PerEastM &&
                (*loaded)->GradientAt(point).PerNorthM == original->GradientAt(point).PerNorthM,
            "runtime heights and slopes remain exactly equal at interior and boundary points");
    }
  }
  assets.reset();
  std::filesystem::remove_all(root);
  return Test::Report();
}
