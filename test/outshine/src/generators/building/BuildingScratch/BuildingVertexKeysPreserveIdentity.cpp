#include "src/generators/building/BuildingScratch.h"
#include "Check.h"
#include <array>
#include <cstdint>

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  BuildingScratch scratch;
  constexpr BuildingPositionKey east{.EastMm = 1000, .NorthMm = 1000, .HeightMm = 0};
  constexpr BuildingPositionKey west{.EastMm = -1000, .NorthMm = -1000, .HeightMm = 0};
  CHECK(BuildingPositionHash{}(east) == BuildingPositionHash{}(west),
        "fixture exercises an actual position hash collision");
  const auto eastInserted = scratch.Welded.Emplace(east, 7);
  const auto westInserted = scratch.Welded.Emplace(west, 8);
  CHECK(eastInserted && eastInserted->second && westInserted && westInserted->second,
        "opposite positions remain distinct despite equal hashes");
  CHECK(*scratch.Welded.Find(east) == 7 && *scratch.Welded.Find(west) == 8,
        "position lookup returns the matching complete key");

  struct Seam {
    BuildingCornerKey Key;
    const char *Claim;
  };

  constexpr std::array seams{Seam{.Key = {.Position = 7,
                                          .Normal = 11,
                                          .TextureU = 13,
                                          .TextureV = 23,
                                          .MaterialU = 31,
                                          .MaterialV = 37},
                                  .Claim = "the first complete vertex enters the corner map"},
                             Seam{.Key = {.Position = 7,
                                          .Normal = 11,
                                          .TextureU = 17,
                                          .TextureV = 23,
                                          .MaterialU = 31,
                                          .MaterialV = 37},
                                  .Claim = "texture U preserves its seam"},
                             Seam{.Key = {.Position = 7,
                                          .Normal = 11,
                                          .TextureU = 13,
                                          .TextureV = 29,
                                          .MaterialU = 31,
                                          .MaterialV = 37},
                                  .Claim = "texture V preserves its seam"},
                             Seam{.Key = {.Position = 7,
                                          .Normal = 19,
                                          .TextureU = 13,
                                          .TextureV = 23,
                                          .MaterialU = 31,
                                          .MaterialV = 37},
                                  .Claim = "normal identity preserves its seam"},
                             Seam{.Key = {.Position = 7,
                                          .Normal = 11,
                                          .TextureU = 13,
                                          .TextureV = 23,
                                          .MaterialU = 41,
                                          .MaterialV = 37},
                                  .Claim = "material metre U preserves its seam"},
                             Seam{.Key = {.Position = 7,
                                          .Normal = 11,
                                          .TextureU = 13,
                                          .TextureV = 23,
                                          .MaterialU = 31,
                                          .MaterialV = 43},
                                  .Claim = "material metre V preserves its seam"}};
  auto &corners = scratch.Corners[0];
  uint32_t vertex = 0;
  for (const auto &seam : seams) {
    const auto inserted = corners.Emplace(seam.Key, vertex);
    CHECK(inserted && inserted->second, seam.Claim);
    CHECK(corners.Find(seam.Key) && *corners.Find(seam.Key) == vertex,
          "every attribute combination retains its own vertex identity");
    ++vertex;
  }
  const auto duplicate = corners.Emplace(seams[0].Key, vertex);
  CHECK(duplicate && !duplicate->second && *corners.Find(seams[0].Key) == 0,
        "identical stored attributes reuse their vertex");
  scratch.ClearWelds();
  CHECK(scratch.Welded.Empty() && corners.Empty(), "new building discards prior identities");
  return Report();
}
