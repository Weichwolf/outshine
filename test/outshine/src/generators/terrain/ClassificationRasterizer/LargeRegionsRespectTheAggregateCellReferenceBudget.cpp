#include "ClassSeed.h"
#include "ClassificationRasterizer.h"
#include "GroundRegionAsset.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <memory>
#include <numbers>

namespace {
outshine::Generators::ClassificationRasterizer::Input Circle(uint32_t edges) {
  using Rasterizer = outshine::Generators::ClassificationRasterizer;
  Rasterizer::Input input;
  input.CellM = 16;
  input.HalfCells = 1;
  for (uint32_t edge = 0; edge < edges; ++edge) {
    const double angle = 2.0 * std::numbers::pi * edge / edges;
    input.Points.push_back(static_cast<float>(4 + std::cos(angle)));
    input.Points.push_back(static_cast<float>(4 + std::sin(angle)));
  }
  input.Rings = {{.First = 0, .Count = edges}};
  input.Features = {{.FirstRing = 0,
                     .RingCount = 1,
                     .Rank = 1,
                     .ClassRow = 7,
                     .Form = Rasterizer::Shape::Polygon,
                     .MinE = 3,
                     .MinN = 3,
                     .MaxE = 5,
                     .MaxN = 5}};
  return input;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Generators::ClassificationRasterizer rasterizer;
  const auto built = rasterizer.Build(Circle(400));
  CHECK(built && built->Overflow == 0,
        "a complete 400-edge region fits the existing cell work budget");
  if (!built) { return Report(); }
  const auto empty = std::make_shared<const ClassStructure::Grid>();
  const auto classes = std::make_shared<const ClassStructure>(
      TangentFrame::At({}), built->Grid, empty, ClassStructure::FromRun{});
  double distance = 0;
  CHECK(classes->Evaluate(4, 4, &distance, nullptr) == 7 && distance > .9998 && distance <= 1.0001,
        "the full reference counter retains polygon winding and its actual boundary distance");
  CHECK(classes->Evaluate(6, 4, nullptr, nullptr) == -1, "outside membership is unchanged");
  CHECK(classes->Measured().Seeds == 1 && built->Grid->Seeds.size() == kClassSeedWords &&
            built->Grid->Seeds[2] == 400,
        "one 16-byte seed carries more than 255 references without wrapping");
  GroundRegionAsset region;
  region.Anchor = {};
  region.GroundAlbedo = {{.2, .3, .1}};
  region.Classes = classes;
  const auto surface = region.Surfaces.addSurface("ground", Material{});
  CHECK(surface, "the cache fixture owns a native ground material");
  if (!surface) { return Report(); }
  region.GroundSurface = surface->index();
  const auto part = region.Surfaces.addPart("soil", *surface);
  constexpr std::array<float, 9> positions{0, 0, 0, 1, 0, 0, 0, 0, 1};
  constexpr std::array<uint32_t, 3> triangles{0, 1, 2};
  CHECK(part && region.Surfaces.setPositions(*part, positions) &&
            region.Surfaces.setTriangles(*part, triangles),
        "the complete region fixture includes native ground geometry");
  const auto encoded = EncodeGroundRegionAsset(region, 4 * 1024 * 1024);
  CHECK(encoded, "a complete large-reference classification persists");
  if (encoded) {
    const auto decoded = DecodeGroundRegionAsset(*encoded, 4 * 1024 * 1024);
    CHECK(decoded && decoded->Classes && decoded->Classes->Fine().Seeds == built->Grid->Seeds &&
              decoded->Classes->Evaluate(4, 4, nullptr, nullptr) == 7,
          "cache transfer preserves full reference counts and classification");
  }
  const auto oversized = rasterizer.Build(Circle(kClassReferencesPerCell + 1));
  CHECK(oversized && oversized->Overflow > 0, "the derived aggregate reference limit is enforced");
  if (oversized) {
    const ClassStructure rejected(TangentFrame::At({}), oversized->Grid, empty, {});
    CHECK(rejected.Evaluate(4, 4, nullptr, nullptr) == -1,
          "an over-budget fragment does not become a partially evaluated polygon");
  }
  auto shared = Circle(3000);
  const auto first = shared.Features.front();
  shared.Features.push_back(first);
  shared.Features.back().ClassRow = 8;
  shared.Features.back().Rank = 2;
  shared.Features.push_back(first);
  shared.Features.back().ClassRow = 9;
  shared.Features.back().Rank = 3;
  const auto aggregate = rasterizer.Build(shared);
  CHECK(aggregate && aggregate->Overflow == 1 &&
            aggregate->Grid->Seeds.size() == 2 * kClassSeedWords,
        "reference usage is shared by the whole cell rather than granted anew per region");
  if (aggregate) {
    const ClassStructure partial(TangentFrame::At({}), aggregate->Grid, empty, {});
    CHECK(partial.Evaluate(4, 4, nullptr, nullptr) == 8,
          "accepted regions retain their complete priority when the next region exceeds budget");
  }
  return Report();
}
