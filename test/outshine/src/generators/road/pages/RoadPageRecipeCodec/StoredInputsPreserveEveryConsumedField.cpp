#include "RoadPageRecipeCodec.h"
#include "RoadPageAsset.h"
#include "HeightSolid.h"
#include "ProfiledRoadMesher.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

RoadPageRecipe Recipe() {
  const std::array<PlanarPoint, 3> points{{{-1, -1}, {1, -1}, {0, 1}}};
  const std::array<PlanarFace, 1> faces{{{0, 1, 2}}};
  const std::array<std::array<double, 3>, 1> heights{{{2, 2, 2}}};
  const std::array<double, 3> bottoms{1, 1, 1};
  auto solid = BuildHeightSolid(
      {.Points = points, .Faces = faces, .TopHeights = heights, .BottomHeights = bottoms});
  CHECK(solid, "the stored body is a complete native height solid");
  const auto body = solid ? std::make_shared<HeightSolid>(std::move(*solid)) : nullptr;
  RoadPageRecipe recipe;
  for (uint32_t edge = 0; edge < 2; ++edge) {
    recipe.Intervals.push_back(
        {.Edge = edge,
         .Lane = 17,
         .Stations = {{.EastM = .123456789, .GradeM = 2, .Node = UINT64_MAX},
                      {.EastM = 100, .GradeM = 3, .Node = UINT64_MAX - 1}},
         .Surface = {.HalfWidthM = 3,
                     .Profile = RoadProfile::Kerbed,
                     .WearsLinear = {{.2f, .3f, .4f}},
                     .Crossfall = .0123456789,
                     .Form = RibbonForm::Surface,
                     .EndPorts = {RoadPort{.Gate = {.HalfWidthM = 3, .ThicknessM = .2},
                                           .Plane = {.SlopeE = .03, .SlopeN = .04}},
                                  std::nullopt}},
         .Body = body});
  }
  recipe.Junctions.push_back(
      {.Index = 3,
       .Node = UINT64_MAX,
       .Gates = {{.HalfWidthM = 3, .ThicknessM = .2}, {.HalfWidthM = 2, .ThicknessM = .3}},
       .Plane = {.SlopeE = .03, .SlopeN = .04},
       .WearsLinear = {{.4f, .5f, .6f}}});
  return recipe;
}

void CheckRoundTrip() {
  const auto recipe = Recipe();
  const std::string key(64, 'a');
  const auto bytes = EncodeRoadPageRecipe(key, recipe, size_t{1024} * 1024);
  CHECK(bytes, "the complete input has its own compact native representation");
  if (!bytes) { return; }
  const auto capacity = RoadPageRecipeDecodeCapacity(bytes->size());
  CHECK(capacity, "wire size derives a finite decoded ownership reservation");
  if (!capacity) { return; }
  const RoadPageRecipeCodecLimits limits{.BytesMost = bytes->size(), .CapacityMost = *capacity};
  const auto decoded = DecodeRoadPageRecipe(key, *bytes, limits);
  CHECK(decoded && EncodeRoadPageRecipe(key, *decoded, bytes->size()) == bytes,
        "every serialized parameter, identifier and native body word survives unchanged");
  if (!decoded) { return; }
  CHECK(decoded->Intervals[0].Body == decoded->Intervals[1].Body &&
            decoded->Intervals[0].Stations[0].Node == UINT64_MAX &&
            decoded->Junctions[0].Node == UINT64_MAX &&
            RoadPageRecipeCapacityBytes(*decoded) <= *capacity,
        "shared body ownership and full-width source identities survive within the reservation");
  const auto original = MeshRoadPage(ProfiledRoadMesher{}, recipe);
  const auto restored = MeshRoadPage(ProfiledRoadMesher{}, *decoded);
  CHECK(original && restored && original->Mesh.PositionM == restored->Mesh.PositionM &&
            original->Mesh.NormalM == restored->Mesh.NormalM &&
            original->Mesh.ColourRgba == restored->Mesh.ColourRgba &&
            original->Mesh.Index == restored->Mesh.Index,
        "the restored input produces the same complete mesh attributes and topology");
  for (size_t count = 0; count < bytes->size(); ++count) {
    CHECK(!DecodeRoadPageRecipe(key, std::span(*bytes).first(count), limits),
          "truncated source input never becomes an available recipe");
  }
  CHECK(!DecodeRoadPageRecipe(std::string(64, 'b'), *bytes, limits),
        "another source identity cannot consume this input");
  CHECK(!DecodeRoadPageRecipe(key, *bytes, {.BytesMost = bytes->size(), .CapacityMost = 0}),
        "a source decode cannot allocate outside its explicit ownership reservation");
  auto malformed = *bytes;
  malformed[0] ^= 1;
  CHECK(!DecodeRoadPageRecipe(key, malformed, limits),
        "another framing version remains unavailable");
  for (size_t at = 76; at < 80; ++at) { malformed[at] = UINT8_MAX; }
  malformed[0] = (*bytes)[0];
  CHECK(!DecodeRoadPageRecipe(key, malformed, limits),
        "a forged body count is rejected before source-body allocation");
  CHECK(!RoadPageRecipeDecodeCapacity(std::numeric_limits<size_t>::max()),
        "capacity derivation cannot wrap around on unaddressable input");
}

void CheckIdentity() {
  RoadPageAddress address{.PlanKey = std::string(64, 'a'),
                          .MaterialKey = std::string(64, 'b'),
                          .Anchor = {},
                          .Parts = {{.Kind = RoadPagePartKind::Interval, .Index = 7}}};
  const auto key = RoadPageRecipeKey(address);
  address.Level = 7;
  address.PositionErrorM = .05;
  address.DisplayPolicy = RoadDisplayPolicy::StoredSurfaceEnvelope;
  address.Clustered = true;
  CHECK(key.size() == 64 && RoadPageRecipeKey(address) == key,
        "render level, topology layout and error request share one canonical input");
  address.MaterialKey.assign(64, 'c');
  CHECK(RoadPageRecipeKey(address) != key, "different source material parameters cannot alias");
}
}

int main() {
  CheckRoundTrip();
  CheckIdentity();
  return Report();
}
