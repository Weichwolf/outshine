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
  const auto eastInserted = scratch.Welded.Emplace(east, 7);
  const auto westInserted = scratch.Welded.Emplace(west, 8);
  CHECK(eastInserted && eastInserted->second && westInserted && westInserted->second,
        "opposite positions remain distinct despite equal hashes");
  CHECK(*scratch.Welded.Find(east) == 7 && *scratch.Welded.Find(west) == 8,
        "position lookup returns the matching complete key");
  auto &corners = scratch.Corners[0];
  const auto firstTexture = corners.Emplace({7, 11, 13, 23}, 1);
  const auto secondTextureU = corners.Emplace({7, 11, 17, 23}, 2);
  const auto secondTextureV = corners.Emplace({7, 11, 13, 29}, 5);
  CHECK(firstTexture && firstTexture->second && secondTextureU && secondTextureU->second &&
            secondTextureV && secondTextureV->second,
        "either texture coordinate retains a seam at the same position and normal");
  const auto secondNormal = corners.Emplace({7, 19, 13, 23}, 3);
  CHECK(secondNormal && secondNormal->second,
        "a normal seam retains separate vertices at the same position and texture");
  const auto duplicate = corners.Emplace({7, 11, 13, 23}, 4);
  CHECK(duplicate && !duplicate->second && *corners.Find({7, 11, 13, 23}) == 1,
        "identical stored attributes reuse their vertex");
  scratch.ClearWelds();
  CHECK(scratch.Welded.Empty() && corners.Empty(), "new building discards prior identities");
  return Report();
}
