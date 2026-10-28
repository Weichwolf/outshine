#include "BuildingPlaneCapture.h"
#include "BuildingSurfaceBlock.h"
#include "BuildingSurface.h"
#include "BuildingShape.h"
#include "Check.h"
#include "Geodesy.h"
#include "HeightField.h"
#include "PreparedStructureCodec.h"
#include "PreparedStructureTile.h"
#include "StructureBake.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include "ImpostorViewFrame.h"
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

struct Fixture {
  PreparedStructureTile Index;
  std::shared_ptr<std::array<size_t, 2>> Loads;
  std::shared_ptr<const PreparedStructureTile> Original;
  Vec3 North;
};

enum class Availability { Ready, Refused };
enum class Background { Grounded, Raised };

RawTile Inputs(Background background) {
  RawTile raw;
  raw.TileSpanM = 1000;
  GeoToEcef({.LongitudeDeg = 16, .LatitudeDeg = 48, .HeightM = 100}, raw.AnchorEcef);
  const double backHeight = background == Background::Raised ? 35.0 : 10.0;
  for (size_t at = 0; at < 2; ++at) {
    const double lat = at == 0 ? 48.001 : 48.002;
    const double width = at == 0 ? .0001 : .0006;
    const double lon = at == 0 && background == Background::Raised ? 16.00125 : 16.001;
    const std::array ring{lat, lon, lat, lon + width, lat + width, lon + width, lat + width, lon};
    const auto first = static_cast<uint32_t>(raw.LatLon.size() / 2);
    raw.LatLon.insert(raw.LatLon.end(), ring.begin(), ring.end());
    raw.Structures.push_back(
        {.LocalFirst = first,
         .PointCount = 4,
         .SourceFirst = first,
         .Cell = {.Index = 1, .Footprint = {}},
         .HeightM = at == 0 ? backHeight : 50.0,
         .MinimumHeightM = at == 0 && background == Background::Raised ? 25.0 : 0.0,
         .Pitched = 0,
         .WallColour = Vec3f{{.4f, .3f, .2f}},
         .Facade = {}});
  }
  return raw;
}

std::optional<Fixture> Make(Availability availability = Availability::Ready,
                            Background background = Background::Grounded,
                            const std::shared_ptr<std::stop_source> &stop = {}) {
  const auto raw = Inputs(background);
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes = {100, 100, 100, 100};
  const auto heights = Ground::HeightField::Of(0, {block});
  CHECK(heights, "constant coarse height input is available");
  if (!heights) { return std::nullopt; }
  auto base = PrepareStructureTile(raw, *heights);
  CHECK(base, "native analytical source forms prepare without a shell mesher");
  if (!base) { return std::nullopt; }
  const auto bytes = EncodePreparedStructureIndex(*base);
  CHECK(bytes, "the existing source-only index serializes");
  if (!bytes) { return std::nullopt; }
  auto index = DecodePreparedStructureIndex(*bytes, size_t{4} * 1024 * 1024);
  CHECK(index && index->Surfaces.size() == 2, "compact headers decode without their shape blocks");
  if (!index || index->Surfaces.size() != 2) { return std::nullopt; }
  const auto forms = std::make_shared<PreparedStructureTile>(std::move(*base));
  auto loads = std::make_shared<std::array<size_t, 2>>();
  for (uint32_t at = 0; at < 2; ++at) {
    index->Surfaces[at].BindShapes(
        std::make_shared<BuildingSurfaceBlock>(
            [forms, loads, at, availability, stop]
            -> std::expected<std::vector<BuildingSurface>, StructureMeshError> {
              ++(*loads)[at];
              if (stop) { stop->request_stop(); }
              if (availability == Availability::Refused) {
                return std::unexpected(StructureMeshError::PreparedSurfaceUnavailable);
              }
              return std::vector<BuildingSurface>{forms->Surfaces[at]};
            }),
        0);
  }
  return Fixture{.Index = std::move(*index),
                 .Loads = std::move(loads),
                 .Original = forms,
                 .North = EnuAxesEcef({.LongitudeDeg = 16, .LatitudeDeg = 48}).North};
}

BuildingPlaneCapture::View ViewOf(const Fixture &fixture) {
  return {.Centre = fixture.Index.Surfaces[1].Bounds().Middle(),
          .TowardEye = fixture.North,
          .HalfExtentM = 150,
          .Pixels = 64};
}

void LimitsPrecedeSourceIO(const BuildingPlaneCapture &capture, const Fixture &fixture) {
  const auto view = ViewOf(fixture);
  const auto tooMany = capture.Capture(view, {.Samples = 4095, .SourceTests = 100000});
  CHECK(!tooMany && tooMany.error().Kind == BuildingPlaneCapture::ErrorKind::WorkBudgetExceeded,
        "sample admission fails before allocating or reading source forms");
  const auto noWork = capture.Capture(view, {.Samples = 4096, .SourceTests = 0});
  CHECK(!noWork && noWork.error().Kind == BuildingPlaneCapture::ErrorKind::WorkBudgetExceeded,
        "candidate source admission precedes lazy source loading");
  std::stop_source stopped;
  stopped.request_stop();
  const auto cancelled =
      capture.Capture(view, {.Samples = 4096, .SourceTests = 100000}, stopped.get_token());
  CHECK(!cancelled && cancelled.error().Kind == BuildingPlaneCapture::ErrorKind::Cancelled,
        "an already withdrawn request performs no capture allocation or source IO");
  auto invalid = view;
  invalid.TowardEye = {};
  CHECK(!capture.Capture(invalid, {.Samples = 4096, .SourceTests = 100000}),
        "a singular fixed frame is rejected");
  invalid = view;
  invalid.Pixels = std::numeric_limits<uint32_t>::max();
  CHECK(!capture.Capture(invalid, {.Samples = 4096, .SourceTests = 100000}),
        "overflow-sized rasters never allocate their requested payload");
  CHECK((*fixture.Loads)[0] == 0 && (*fixture.Loads)[1] == 0,
        "invalid or unadmitted work leaves every shape block untouched");
}

std::array<size_t, 2> BruteSamples(const Fixture &fixture, const BuildingPlaneCapture::View &view) {
  const auto frame = ImpostorViewFrame::From(view.TowardEye);
  std::array<size_t, 2> hits{};
  if (!frame) {
    CHECK(false, "reference fixed-view axes are valid");
    return hits;
  }
  std::vector<double> cuts;
  for (uint32_t y = 0; y < view.Pixels; ++y) {
    for (uint32_t x = 0; x < view.Pixels; ++x) {
      const double step = 2 * view.HalfExtentM / view.Pixels;
      const Vec3 origin = view.Centre + view.TowardEye * (3 * view.HalfExtentM) +
                          frame->Right * ((x + .5) * step - view.HalfExtentM) +
                          frame->Up * (view.HalfExtentM - (y + .5) * step);
      double maximum = 5 * view.HalfExtentM;
      std::optional<size_t> owner;
      for (size_t at = 0; at < fixture.Original->Surfaces.size(); ++at) {
        const auto hit = fixture.Original->Surfaces[at].Trace(
            {.Origin = origin, .Direction = view.TowardEye * -1.0},
            view.HalfExtentM,
            maximum,
            cuts);
        if (hit && Dot(hit->Normal, view.TowardEye) > 0) {
          owner = at;
          maximum = hit->Along;
        }
      }
      if (owner) { ++hits[*owner]; }
    }
  }
  return hits;
}

bool BoundsBehindOneOpaqueFace(const Fixture &fixture, const BuildingPlaneCapture::View &view) {
  const auto &foreground = fixture.Original->Surfaces[1];
  const auto &background = fixture.Original->Surfaces[0].Bounds();
  std::optional<BuildingSurface::Hit> first;
  std::vector<double> cuts;
  for (unsigned corner = 0; corner < 8; ++corner) {
    const auto point = background.Corner(corner);
    const auto hit = foreground.Trace({.Origin = point + view.TowardEye * (6 * view.HalfExtentM),
                                       .Direction = view.TowardEye * -1.0},
                                      0,
                                      6 * view.HalfExtentM,
                                      cuts);
    if (!hit || hit->Gable || hit->Face < 2 || Dot(hit->Normal, view.TowardEye) <= 0 ||
        foreground.Shapes()[hit->Part].Roof != RoofKind::Flat) {
      return false;
    }
    if (first && (first->Part != hit->Part || first->Face != hit->Face)) { return false; }
    first = hit;
  }
  return first.has_value();
}

void ProvenHiddenBoundsAvoidIO() {
  auto fixture = Make(Availability::Ready, Background::Raised);
  if (!fixture) { return; }
  const std::array sources{fixture->Index.Surfaces.data(), &fixture->Index.Surfaces[1]};
  const auto capture = BuildingPlaneCapture::Prepare(sources);
  CHECK(capture.has_value(), "fully enclosed background metadata builds the same hierarchy");
  if (!capture) { return; }
  const auto view = ViewOf(*fixture);
  CHECK(BoundsBehindOneOpaqueFace(*fixture, view),
        "all background AABB corners lie behind the same convex opaque flat-roof wall");
  const auto result = capture->Capture(view, {.Samples = 4096, .SourceTests = 100000});
  CHECK(result && result->Sources == std::vector<uint32_t>{1} && (*fixture->Loads)[0] == 0 &&
            (*fixture->Loads)[1] == 1,
        "known opaque foreground rejects enclosed background bounds before source IO");
  auto opposite = view;
  opposite.TowardEye = opposite.TowardEye * -1.0;
  const auto reversed = capture->Capture(opposite, {.Samples = 4096, .SourceTests = 100000});
  CHECK(reversed && std::ranges::find(reversed->Sources, 0) != reversed->Sources.end() &&
            (*fixture->Loads)[0] == 1 && (*fixture->Loads)[1] == 1,
        "a newly visible source loads once while its sibling remains resident");
}

void CapturesAndReusesSources() {
  auto fixture = Make();
  if (!fixture) { return; }
  const std::array sources{fixture->Index.Surfaces.data(), &fixture->Index.Surfaces[1]};
  const auto capture = BuildingPlaneCapture::Prepare(sources);
  CHECK(capture && (*fixture->Loads)[0] == 0 && (*fixture->Loads)[1] == 0,
        "hierarchy preparation consumes only compact source bounds");
  if (!capture) { return; }
  LimitsPrecedeSourceIO(*capture, *fixture);
  const auto view = ViewOf(*fixture);
  const auto result = capture->Capture(view, {.Samples = 4096, .SourceTests = 100000});
  CHECK(result && result->Rays == 4096 && result->Covered > 0 && result->SourceTests <= 100000,
        "only the explicit fixed raster and bounded candidate raytests do work");
  if (!result) { return; }
  const auto reference = BruteSamples(*fixture, view);
  CHECK(reference[0] == 0 && reference[1] == result->Covered &&
            result->Sources == std::vector<uint32_t>{1},
        "brute-force source rays confirm that only the opaque foreground contributes patches");
  CHECK((*fixture->Loads)[0] == 1 && (*fixture->Loads)[1] == 1,
        "loose world AABBs remain unknown and may require a hidden source's forms");
  CHECK(!result->Mesh.WallRun.empty() && result->Mesh.WallRun.size() <= result->Covered * 6 &&
            result->Mesh.WallColours.size() == result->Mesh.WallCorners.size() * 4,
        "native patches preserve material factors with work bounded by occupied samples");
  auto opposite = view;
  opposite.TowardEye = opposite.TowardEye * -1.0;
  const auto reversed = capture->Capture(opposite, {.Samples = 4096, .SourceTests = 100000});
  CHECK(reversed && std::ranges::find(reversed->Sources, 0) != reversed->Sources.end() &&
            (*fixture->Loads)[0] == 1 && (*fixture->Loads)[1] == 1,
        "another fixed direction reuses the hierarchy and existing source blocks");
  const auto repeat = capture->Capture(view, {.Samples = 4096, .SourceTests = 100000});
  CHECK(repeat && repeat->Mesh.WallRun == result->Mesh.WallRun &&
            repeat->Mesh.WallColours == result->Mesh.WallColours &&
            repeat->Mesh.WallCorners.size() == result->Mesh.WallCorners.size(),
        "unchanged capture preserves native topology and source materials");
  if (!repeat || repeat->Mesh.WallCorners.size() != result->Mesh.WallCorners.size()) { return; }
  for (size_t at = 0; at < result->Mesh.WallCorners.size(); ++at) {
    CHECK(result->Mesh.WallCorners[at].pos == repeat->Mesh.WallCorners[at].pos,
          "repeated sampled source positions are deterministic");
  }
}

void CancellationDuringSourceLoadIsTerminal() {
  const auto stop = std::make_shared<std::stop_source>();
  auto fixture = Make(Availability::Ready, Background::Grounded, stop);
  if (!fixture) { return; }
  const std::array sources{fixture->Index.Surfaces.data(), &fixture->Index.Surfaces[1]};
  const auto capture = BuildingPlaneCapture::Prepare(sources);
  if (!capture) {
    CHECK(false, "cancellable source metadata prepares");
    return;
  }
  const auto result = capture->Capture(
      ViewOf(*fixture), {.Samples = 4096, .SourceTests = 100000}, stop->get_token());
  CHECK(!result && result.error().Kind == BuildingPlaneCapture::ErrorKind::Cancelled &&
            (*fixture->Loads)[0] + (*fixture->Loads)[1] == 1,
        "withdrawal during source IO returns no partial geometry and requests no next source");
}

void MissingSourcesAreNotPublished() {
  auto fixture = Make(Availability::Refused);
  if (!fixture) { return; }
  const std::array sources{fixture->Index.Surfaces.data(), &fixture->Index.Surfaces[1]};
  const auto capture = BuildingPlaneCapture::Prepare(sources);
  if (!capture) {
    CHECK(false, "metadata remains available for a missing native shape");
    return;
  }
  const auto result = capture->Capture(ViewOf(*fixture), {.Samples = 4096, .SourceTests = 100000});
  CHECK(!result && result.error().Kind == BuildingPlaneCapture::ErrorKind::SourceUnavailable &&
            result.error().Source == 1,
        "a missing required shape returns its source ordinal, never a valid partial parent");
  const std::array<const BuildingSurface *, 1> invalid{nullptr};
  const auto rejected = BuildingPlaneCapture::Prepare(invalid);
  CHECK(!rejected && rejected.error().Source == 0, "null source metadata is rejected explicitly");
}

void ExplicitFramesAreValidatedBeforeSourceIO() {
  auto fixture = Make();
  if (!fixture) { return; }
  const std::array sources{fixture->Index.Surfaces.data(), &fixture->Index.Surfaces[1]};
  const auto producer = BuildingPlaneCapture::Prepare(sources);
  if (!producer) { return; }
  auto view = ViewOf(*fixture);
  const auto standard = ImpostorViewFrame::From(view.TowardEye);
  CHECK(standard, "the original fixed direction has a valid capture basis");
  if (!standard) { return; }
  const ImpostorViewFrame rolled{.Right = standard->Up, .Up = standard->Right * -1.0};
  const std::array invalid{ImpostorViewFrame{.Right = rolled.Right * 2.0, .Up = rolled.Up},
                           ImpostorViewFrame{.Right = rolled.Right, .Up = rolled.Up * -1.0},
                           ImpostorViewFrame{.Right = view.TowardEye, .Up = rolled.Up}};
  for (const auto &frame : invalid) {
    view.Frame = frame;
    const auto result = producer->Capture(view, {.Samples = 4096, .SourceTests = 100000});
    CHECK(!result && result.error().Kind == BuildingPlaneCapture::ErrorKind::InvalidInput &&
              (*fixture->Loads)[0] + (*fixture->Loads)[1] == 0,
          "nonunit, reflected and nonorthogonal frames fail before shape IO");
  }
  view.Frame = rolled;
  const auto result = producer->Capture(view, {.Samples = 4096, .SourceTests = 100000});
  CHECK(result && result->Covered > 0 && result->Rays == 4096,
        "a right-handed rolled capture retains the requested source coordinate frame");
}
}

int main() {
  CapturesAndReusesSources();
  MissingSourcesAreNotPublished();
  ProvenHiddenBoundsAvoidIO();
  CancellationDuringSourceLoadIsTerminal();
  ExplicitFramesAreValidatedBeforeSourceIO();
  return Report();
}
