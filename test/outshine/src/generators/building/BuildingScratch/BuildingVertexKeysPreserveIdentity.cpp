#include "src/generators/building/BuildingScratch.h"
#include "Check.h"

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  BuildingScratch scratch;
  const BuildingPositionKey east{1000, 1000, 0};
  const BuildingPositionKey west{-1000, -1000, 0};
  CHECK(BuildingPositionHash{}(east) == BuildingPositionHash{}(west),
        "fixture exercises an actual position hash collision");
  CHECK(scratch.Welded.Emplace(east, 7).second && scratch.Welded.Emplace(west, 8).second,
        "opposite positions remain distinct despite equal hashes");
  CHECK(*scratch.Welded.Find(east) == 7 && *scratch.Welded.Find(west) == 8,
        "position lookup returns the matching complete key");
  auto &corners = scratch.Corners[0];
  CHECK(corners.Emplace({7, 11, 13}, 1).second && corners.Emplace({7, 11, 17}, 2).second,
        "a texture seam retains separate vertices at the same position and normal");
  CHECK(corners.Emplace({7, 19, 13}, 3).second,
        "a normal seam retains separate vertices at the same position and texture");
  CHECK(!corners.Emplace({7, 11, 13}, 4).second && *corners.Find({7, 11, 13}) == 1,
        "identical stored attributes reuse their vertex");
  scratch.ClearWelds();
  CHECK(scratch.Welded.Empty() && corners.Empty(), "new building discards prior identities");
  return Report();
}
