#include "Check.h"
#include "CppAllocationProbe.h"
#include "HeightField.h"
#include "TerrainGrid.h"
#include "TileGeodesy.h"
#include "Units.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Ground;
using namespace outshine::Test;

HeightField::Block Block(long x = 0, long y = 0) {
  HeightField::Block block;
  block.At = {.Zoom = 1, .X = x, .Y = y};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes = {0, 0, 0, 16};
  block.Sources = {
      {.Kind = Data::DataKind::Elevation,
       .Tile = {.Zoom = 1, .X = static_cast<uint32_t>(x), .Y = static_cast<uint32_t>(y)},
       .SourceId = "native-dem",
       .Revision = "r1"}};
  return block;
}

LongitudeLatitude Point(double x, double y) {
  const auto geo = TileFracToGeo({.X = x, .Y = y}, 1);
  return {.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg};
}

std::optional<double> Height(const HeightField &field, LongitudeLatitude point) {
  const auto sample = field.SampleBilinearAt(point);
  return sample ? sample->Height.AslM() : std::nullopt;
}

void CheckInterpolation() {
  const auto field = HeightField::Of(1, {Block()});
  const auto point = Point(.25, .25);
  BeginCppAllocationProbe();
  const auto sample = field->SampleBilinearAt(point);
  const auto allocations = EndCppAllocationProbe();
  CHECK(sample && sample->BlockIndex == 0 && allocations.Calls == 0,
        "native bilinear sampling locates the original leased block without allocation");
  if (!sample) { return; }
  const auto height = sample->Height.AslM();
  CHECK(height.has_value(), "a returned located sample has a known original height");
  if (!height) { return; }
  CHECK_NEAR(
      *height, 1., 1e-12, "m", "the independently derived bilinear cross term is 16*0.25*0.25");
  CHECK(field->At(point).AslM() == 4.,
        "the existing render-triangle sampler retains its distinct established value");
  auto precise = Block();
  precise.Nodes = {1234.125f, 1234.375f, 1234.5f, 1234.875f};
  const auto detailed = HeightField::Of(1, {std::move(precise)});
  constexpr double u = .123456789;
  constexpr double v = .731234567;
  const auto exact = Height(*detailed, Point(u, v));
  CHECK(exact.has_value(), "a sub-float interpolation result is available");
  if (exact) {
    const double value = *exact;
    CHECK_NEAR(value,
               1234.125 + .25 * u + .375 * v + .125 * u * v,
               1e-11,
               "m",
               "all interpolation weights and intermediate heights remain double precision");
    CHECK(std::abs(value - static_cast<double>(static_cast<float>(value))) > 1e-8,
          "the output is not silently rounded back to float");
  }
  auto sparse = Block();
  sparse.Raster = {.Side = 3, .Postings = 6};
  sparse.Nodes = {0, 20, 50, 200, 220, 250, 500, 520, 550};
  const auto posted = Height(*HeightField::Of(1, {std::move(sparse)}), Point(.6, .2));
  CHECK(posted.has_value(), "sparse native postings remain a supported explicit raster mapping");
  if (posted) {
    CHECK_NEAR(*posted,
               130.,
               1e-10,
               "m",
               "integer posting positions 0,2,5 are preserved instead of inventing a uniform "
               "0,2.5,5 grid");
  }
}

void CheckCoverage() {
  auto west = Block();
  west.Nodes.assign(4, 10);
  auto east = Block(1);
  east.Nodes.assign(4, 20);
  auto south = Block(0, 1);
  south.Nodes.assign(4, 0);
  const auto field = HeightField::Of(1, {west, east, south});
  const auto seam = field->SampleBilinearAt({.LongitudeDeg = 0, .LatitudeDeg = 30});
  CHECK(seam && seam->BlockIndex == 1 && seam->Height.AslM() == 20,
        "an exact vertical boundary belongs to the eastern tile");
  const auto horizontal = field->SampleBilinearAt({.LongitudeDeg = -90, .LatitudeDeg = 0});
  CHECK(horizontal && horizontal->BlockIndex == 2 && horizontal->Height.AslM() == 0,
        "an exact horizontal boundary belongs to the southern tile and a genuine zero stays known");
  CHECK(!HeightField::Of(1, {west})->SampleBilinearAt({.LongitudeDeg = 0, .LatitudeDeg = 30}),
        "missing canonical tile coverage never borrows a neighboring boundary value");
  const auto dateLine = field->SampleBilinearAt({.LongitudeDeg = 180, .LatitudeDeg = 30});
  CHECK(dateLine && dateLine->Height.AslM() == 10,
        "the dateline uses the existing wrapped western raster anchor");
  const auto north =
      field->SampleBilinearAt({.LongitudeDeg = -90, .LatitudeDeg = kMercatorLatMaxDeg});
  CHECK(north && north->Height.AslM() == 10,
        "the global northern raster edge retains its first original post");
  const auto pole =
      field->SampleBilinearAt({.LongitudeDeg = -90, .LatitudeDeg = -kMercatorLatMaxDeg});
  CHECK(pole && pole->Height.AslM() == 0,
        "the global southern raster edge retains its last original post");
  CHECK(!field->SampleBilinearAt({.LongitudeDeg = -90, .LatitudeDeg = 89}) &&
            !field->SampleBilinearAt({.LatitudeDeg = std::numeric_limits<double>::quiet_NaN()}),
        "out-of-Mercator and nonfinite queries never clamp into invented coverage");
  CHECK(!HeightField::Of(1, {west, east, west})->SampleBilinearAt(Point(.25, .25)),
        "ambiguous duplicate tile delivery cannot depend on block order");
}

void CheckUnknownsAndLease() {
  for (const float missing : std::array{std::numeric_limits<float>::quiet_NaN(),
                                        std::numeric_limits<float>::infinity(),
                                        -32768.f}) {
    auto block = Block();
    block.Nodes[3] = missing;
    CHECK(!HeightField::Of(1, {std::move(block)})->SampleBilinearAt(Point(.25, .25)),
          "NoData in a required original stencil remains unknown");
  }
  auto partial = Block();
  partial.MissingBoundary = true;
  CHECK(!HeightField::Of(1, {std::move(partial)})->SampleBilinearAt(Point(.25, .25)),
        "partially prepared boundaries cannot qualify as an original complete raster");
  auto anonymous = Block();
  anonymous.Sources.clear();
  CHECK(!HeightField::Of(1, {std::move(anonymous)})->SampleBilinearAt(Point(.25, .25)),
        "unbound raster values cannot masquerade as sourced DEM observations");
  CHECK(!HeightField::Of(1, {Block()}, true)->SampleBilinearAt(Point(.25, .25)),
        "a fallback snapshot cannot silently change the bound source procedure");
  auto truncated = Block();
  truncated.Nodes.resize(1);
  CHECK(!HeightField::Of(1, {std::move(truncated)})->SampleBilinearAt(Point(.25, .25)),
        "truncated backing storage is refused before stencil reads");
  auto raster = std::make_shared<TerrainField>(2, 2);
  raster->SetM(1, 1, 16);
  raster->AddSources(Block().Sources);
  const std::weak_ptr<const TerrainField> weak = raster;
  HeightField::Block block;
  CHECK(HeightField::SharesField(raster, {.Zoom = 1, .X = 0, .Y = 0}, block),
        "the source array is shared through the existing immutable terrain lease");
  const auto field = HeightField::Of(1, {std::move(block)});
  raster.reset();
  const auto sample = field->SampleBilinearAt(Point(.25, .25));
  CHECK(!weak.expired() && field->Blocks()[0].Nodes.empty() && sample,
        "sampling survives provider release without a copied raster");
  const auto height = Height(*field, Point(.25, .25));
  CHECK(height.has_value(), "the pinned sample retains its resolved state");
  if (height) {
    CHECK_NEAR(*height,
               1.,
               1e-12,
               "m",
               "the original pinned samples reach the production interpolation path");
  }
}
}

int main() {
  CheckInterpolation();
  CheckCoverage();
  CheckUnknownsAndLease();
  return Report();
}
