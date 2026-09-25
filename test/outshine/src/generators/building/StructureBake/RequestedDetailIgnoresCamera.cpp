#include "src/generators/building/BuildingMesh.h"
#include "src/generators/building/StructureBake.h"
#include "Check.h"

#include <array>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  Generators::RawTile raw;
  raw.LatLon = {47.0, 9.0, 47.0, 9.0001, 47.0001, 9.0001, 47.0001, 9.0};
  raw.Structures.push_back({.PointCount = 4, .HeightM = 12.0});
  raw.TileSpanM = 1000.0;
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 0.0f);
  const auto heights = Ground::HeightField::Of(0, {block});
  Generators::BuildingMesh mesher;

  std::array<uint64_t, 3> digests{};
  for (const LevelOfDetail detail :
       {LevelOfDetail::Fine, LevelOfDetail::Shell, LevelOfDetail::Massed}) {
    raw.RequestedDetail = detail;
    raw.Eye = {.LongitudeDeg = 9.0, .LatitudeDeg = 47.0};
    raw.FocalPx = 1000.0;
    auto nearScratch = mesher.Scratch();
    Generators::BakedTile near;
    CHECK(Generators::BakeStructures(raw, *heights, mesher, *nearScratch, near).has_value(),
          "requested structure detail bakes near the source");

    raw.Eye = {.LongitudeDeg = 0.0, .LatitudeDeg = 0.0};
    raw.FocalPx = 1.0;
    auto farScratch = mesher.Scratch();
    Generators::BakedTile far;
    CHECK(Generators::BakeStructures(raw, *heights, mesher, *farScratch, far).has_value(),
          "the same requested detail bakes for another camera");
    CHECK(near.RequestedDetail == detail && far.RequestedDetail == detail &&
              near.Prints.size() == 1 && far.Prints.size() == 1 &&
              near.Prints.front().Coarseness == detail && far.Prints.front().Coarseness == detail &&
              near.Digest == far.Digest,
          "explicit detail geometry and product identity ignore eye and focal length");
    digests[static_cast<size_t>(detail)] = near.Digest;
  }
  CHECK(digests[0] != digests[1], "Fine and Shell are distinct source-keyed products");

  raw.RequestedDetail = LevelOfDetail::Fine;
  raw.Structures.front().HeightM = 18.0;
  auto changedScratch = mesher.Scratch();
  Generators::BakedTile changed;
  CHECK(Generators::BakeStructures(raw, *heights, mesher, *changedScratch, changed).has_value() &&
            changed.Digest != digests[0],
        "source geometry changes the requested product digest");

  raw.RequestedDetail = LevelOfDetail::Skyline;
  auto invalidScratch = mesher.Scratch();
  Generators::BakedTile invalid;
  const auto rejected = Generators::BakeStructures(raw, *heights, mesher, *invalidScratch, invalid);
  CHECK(!rejected &&
            std::get_if<Generators::StructureBakeErrorKind>(&rejected.error()) != nullptr &&
            *std::get_if<Generators::StructureBakeErrorKind>(&rejected.error()) ==
                Generators::StructureBakeErrorKind::InvalidDetail,
        "unsupported detail requests fail before any product publication");

  raw.RequestedDetail = LevelOfDetail::Fine;
  raw.Structures.push_back(raw.Structures.front());
  Generators::StructureBakeProgress progress;
  auto firstScratch = mesher.Scratch();
  const auto first = progress.AdvanceStructures(raw, *heights, mesher, *firstScratch, 1);
  CHECK(first && !*first && progress.BakedStructures() == 1,
        "the first slice pins its requested detail");
  raw.RequestedDetail = LevelOfDetail::Shell;
  const auto mixed = progress.AdvanceStructures(raw, *heights, mesher, *firstScratch, 1);
  CHECK(!mixed && std::get_if<Generators::StructureBakeErrorKind>(&mixed.error()) != nullptr &&
            *std::get_if<Generators::StructureBakeErrorKind>(&mixed.error()) ==
                Generators::StructureBakeErrorKind::ChangedDetail,
        "one tile cannot mix detail requests across worker slices");
  return Report();
}
