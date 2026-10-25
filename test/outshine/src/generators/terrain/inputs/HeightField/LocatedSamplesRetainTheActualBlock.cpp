#include "HeightField.h"
#include "Check.h"
#include "CppAllocationProbe.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Ground;
using namespace outshine::Test;

HeightField::Block Block(long x) {
  HeightField::Block block;
  block.At = {.Zoom = 1, .X = x, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 0);
  block.Sources = {{.Kind = Data::DataKind::Elevation,
                    .Tile = {.Zoom = 1, .X = static_cast<uint32_t>(x), .Y = 0},
                    .SourceId = "located-dem",
                    .Revision = "r1"}};
  return block;
}

void CheckLocation() {
  constexpr float eastAslM = 1250;
  auto eastBlock = Block(1);
  eastBlock.Nodes.assign(4, eastAslM);
  const auto field = HeightField::Of(1, {Block(0), std::move(eastBlock)});
  constexpr LongitudeLatitude west{.LongitudeDeg = -90, .LatitudeDeg = 30};
  constexpr LongitudeLatitude east{.LongitudeDeg = 90, .LatitudeDeg = 30};
  BeginCppAllocationProbe();
  const auto first = field->SampleAt(west);
  const auto second = field->SampleAt(east);
  const auto allocation = EndCppAllocationProbe();
  CHECK(first && second && first->BlockIndex == 0 && second->BlockIndex == 1 &&
            first->Height.AslM() == 0 && second->Height.AslM() == eastAslM &&
            field->At(west).AslM() == first->Height.AslM() &&
            field->At(east).AslM() == second->Height.AslM() && allocation.Calls == 0,
        "located and legacy sampling retain exact source values including genuine zero without "
        "allocating");
  CHECK(field->Zoom() == 1 && first && field->Blocks()[first->BlockIndex].Sources[0].Tile.X == 0,
        "the selected native block gives direct access to its actual provenance");
  const auto wrongZoom = HeightField::Of(0, {Block(0)});
  CHECK(!wrongZoom->Qualified() && !wrongZoom->SampleAt(west) && !wrongZoom->At(west).AslM(),
        "matching XY from another zoom cannot masquerade as a delivered geographic sample");
}

void CheckInvalidRaster() {
  auto shortRaster = Block(0);
  shortRaster.Nodes.resize(1);
  const auto shortField = HeightField::Of(1, {shortRaster});
  CHECK(!shortField->Qualified() &&
            !shortField->At({.LongitudeDeg = -90, .LatitudeDeg = 30}).AslM() &&
            shortField->CaptureRequest().Tiles.size() == 1,
        "truncated raster storage is never read or promoted and retains its original request");
  const auto malformedRaster =
      std::make_shared<TerrainField>(HeightRaster{.Rows = 2, .Cols = 2, .Meters = {1.0f}});
  HeightField::Block copied;
  CHECK(!malformedRaster->Meshable() &&
            !HeightField::CopiesField(*malformedRaster, {.Zoom = 1, .X = 0, .Y = 0}, copied),
        "native field transfers reject truncated backing storage before forming a copy range");
  auto emptyRaster = Block(0);
  emptyRaster.Raster = {};
  emptyRaster.Nodes.clear();
  const auto emptyField = HeightField::Of(1, {emptyRaster});
  CHECK(!emptyField->Qualified() &&
            !emptyField->At({.LongitudeDeg = -90, .LatitudeDeg = 30}).AslM(),
        "an empty native raster is missing data instead of an invented zero height");
  auto badPostings = Block(0);
  badPostings.Raster.Postings = 1;
  const auto invalid = HeightField::Of(1, {badPostings});
  CHECK(!invalid->Qualified() && !invalid->At({.LongitudeDeg = -90, .LatitudeDeg = 30}).AslM(),
        "invalid interpolation dimensions cannot divide by zero or read native nodes");
  auto repeatedPostings = Block(0);
  repeatedPostings.Raster.Side = 3;
  repeatedPostings.Nodes.assign(9, 0);
  const auto repeated = HeightField::Of(1, {repeatedPostings});
  CHECK(!repeated->Qualified() && !repeated->SampleAt({.LongitudeDeg = -90, .LatitudeDeg = 30}),
        "more interpolation nodes than source postings cannot qualify a zero-width cell");
  const auto badZoom = HeightField::Of(-1, {});
  CHECK(!badZoom->At({}).AslM() && !badZoom->SampleAt({}),
        "an invalid native grid zoom is rejected before coordinate wrapping");
  const auto field = HeightField::Of(1, {Block(0)});
  CHECK(!field->SampleAt({.LatitudeDeg = std::numeric_limits<double>::quiet_NaN()}) &&
            !field->At({.LongitudeDeg = std::numeric_limits<double>::infinity()}).AslM(),
        "nonfinite positions cannot enter tile integer conversion");
}
}

int main() {
  CheckLocation();
  CheckInvalidRaster();
  return Report();
}
