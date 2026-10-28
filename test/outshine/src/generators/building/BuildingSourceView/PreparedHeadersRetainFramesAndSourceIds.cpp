#include "BuildingSourceView.h"
#include "BuildingSurfaceBlock.h"
#include "PreparedStructureCodec.h"
#include "HeightField.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

struct Fixture {
  std::shared_ptr<PreparedStructureTile> Base;
  std::shared_ptr<size_t> Reads;
  BuildingCaptureFrame Frame;
};

enum class Availability { Ready, Missing };

std::optional<Fixture> Make(Availability availability) {
  RawTile raw;
  raw.TileSpanM = 1000;
  GeoToEcef({.LongitudeDeg = 16, .LatitudeDeg = 48, .HeightM = 100}, raw.AnchorEcef);
  raw.LatLon = {48.001, 16.001, 48.001, 16.0012, 48.0012, 16.0012, 48.0012, 16.001};
  raw.Structures.push_back({.LocalFirst = 0,
                            .PointCount = 4,
                            .SourceFirst = 0,
                            .Cell = {.Index = 1, .Footprint = {}},
                            .HeightM = 30,
                            .Pitched = 0,
                            .WallColour = std::nullopt,
                            .Facade = {}});
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes = {100, 100, 100, 100};
  const auto heights = Ground::HeightField::Of(0, {block});
  CHECK(heights != nullptr, "constant source heights exist");
  if (!heights) { return std::nullopt; }
  auto prepared = PrepareStructureTile(raw, *heights);
  CHECK(prepared.has_value(), "native source forms prepare");
  if (!prepared) { return std::nullopt; }
  const auto original = std::make_shared<PreparedStructureTile>(std::move(*prepared));
  const auto bytes = EncodePreparedStructureIndex(*original);
  CHECK(bytes.has_value(), "the source-only headers encode");
  if (!bytes) { return std::nullopt; }
  auto decoded = DecodePreparedStructureIndex(*bytes, size_t{4} * 1024 * 1024);
  CHECK(decoded && decoded->Surfaces.size() == 1, "headers decode independently of source forms");
  if (!decoded || decoded->Surfaces.size() != 1) { return std::nullopt; }
  const auto reads = std::make_shared<size_t>(0);
  decoded->Surfaces[0].BindShapes(
      std::make_shared<BuildingSurfaceBlock>(
          [original,
           reads,
           availability] -> std::expected<std::vector<BuildingSurface>, StructureMeshError> {
            ++*reads;
            if (availability == Availability::Missing) {
              return std::unexpected(StructureMeshError::PreparedSurfaceUnavailable);
            }
            return original->Surfaces;
          }),
      0);
  const BuildingCaptureFrame frame{.Axes = EnuAxesEcef({.LongitudeDeg = 16, .LatitudeDeg = 48}),
                                   .Origin = decoded->Surfaces[0].Bounds().Middle()};
  PreparedStructure unsupported;
  unsupported.Layout.Cell.Index = 1;
  decoded->Structures.insert(decoded->Structures.begin(), std::move(unsupported));
  decoded->Surfaces.insert(decoded->Surfaces.begin(), BuildingSurface{});
  return Fixture{.Base = std::make_shared<PreparedStructureTile>(std::move(*decoded)),
                 .Reads = reads,
                 .Frame = frame};
}

void Capture(Availability availability) {
  auto fixture = Make(availability);
  if (!fixture) { return; }
  const std::array<uint32_t, 2> cells{1, 1};
  auto view = BuildingSourceView::Open(fixture->Base, cells, fixture->Frame);
  CHECK(view && *fixture->Reads == 0, "opening a view reads no lazy forms");
  if (!view) { return; }
  CHECK(view->Sources().size() == 1 && view->Sources()[0] == 1 && view->Unsupported().size() == 1 &&
            view->Unsupported()[0] == 0,
        "supported and unresolved original ordinals remain distinct");
  auto invalid = fixture->Frame;
  invalid.Axes.Up = invalid.Axes.Up * -1.0;
  CHECK(!BuildingSourceView::Open(fixture->Base, cells, invalid),
        "a reflected source frame is rejected");
  const BuildingPlaneCapture::View camera{
      .Centre = {}, .TowardEye = {{0, 0, -1}}, .HalfExtentM = 40, .Pixels = 32};
  const std::array<Material, 3> materials{Material{}, Material{}, Material{}};
  auto refused = view->Capture(camera, {.Samples = 1023, .SourceTests = 100000}, materials);
  CHECK(!refused && refused.error().Kind == BuildingPlaneCapture::ErrorKind::WorkBudgetExceeded &&
            *fixture->Reads == 0,
        "work admission precedes source forms and mesh allocation");
  std::stop_source stop;
  stop.request_stop();
  auto cancelled =
      view->Capture(camera, {.Samples = 1024, .SourceTests = 100000}, materials, stop.get_token());
  CHECK(!cancelled && cancelled.error().Kind == BuildingPlaneCapture::ErrorKind::Cancelled &&
            *fixture->Reads == 0,
        "cancelled capture reads no forms");
  const std::weak_ptr<const PreparedStructureTile> retained = fixture->Base;
  fixture->Base.reset();
  CHECK(!retained.expired(), "the native view retains its descriptor and lazy source ownership");
  auto result = view->Capture(camera, {.Samples = 1024, .SourceTests = 100000}, materials);
  if (availability == Availability::Missing) {
    CHECK(!result && result.error().Kind == BuildingPlaneCapture::ErrorKind::SourceUnavailable &&
              result.error().Source == 1,
          "source failures retain the original ordinal after filtering");
  } else {
    CHECK(result && result->Rays == 1024 && result->Covered > 0 && result->Mesh.parts() > 0,
          "native source planes become renderable local geometry");
    if (result) {
      for (int part = 0; part < result->Mesh.parts(); ++part) {
        CHECK(std::ranges::all_of(result->Mesh.positionsOf(part),
                                  [](float x) { return std::isfinite(x) && std::abs(x) < 100; }),
              "source coordinates transform into the declared local frame");
      }
    }
  }
  CHECK(*fixture->Reads == 1, "the selected source form loads at most once");
  view = std::unexpected(BuildingSourceView::Error{.Message = "retired"});
  CHECK(retained.expired(), "retiring the final native view releases the descriptor");
}
}

int main() {
  Capture(Availability::Ready);
  Capture(Availability::Missing);
  return Report();
}
