#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "GeometryPageResidency.h"
#include "BuildingPageAdapter.h"
#include "StructureSourcePublication.h"
#include "BuildingSourceCoverage.h"
#include "BuildingField.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr int kSide = 32;
constexpr uint32_t kTile = 7;
constexpr uint64_t kOldSource = 91;
constexpr uint64_t kNewSource = 92;
constexpr ProjectedErrorBudget kProjection{.FocalPx = 720};
constexpr std::array<Render::GeometryPageResidency::CellSelection, 2> kCells{
    {{.Cell = 1, .Detail = std::nullopt}, {.Cell = 2, .Detail = std::nullopt}}};

struct Fixture {
  Geometry Base;
  Render::SceneRenderer Renderer;
  std::unique_ptr<Core::RuntimeScene> Scene;
  Render::GeometryPageResidency Pieces;
  BuildingPageMaterials Materials;
  TangentFrame Frame = TangentFrame::At({});
  std::string Error;

  bool Open() {
    const auto surface = Base.addSurface("building", Material{});
    if (!surface) { return false; }
    const auto part = Base.addPart("offscreen", *surface);
    constexpr float kOutsideM = 100;
    if (!part ||
        !Base.setPositions(
            *part, std::array<float, 9>{kOutsideM, 0, 0, kOutsideM + 1, 0, 0, kOutsideM, 1, 0}) ||
        !Base.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2})) {
      return false;
    }
    Core::Declaration declaration;
    declaration.InitialGeometry = &Base;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = kSide;
    declaration.Outputs = {"sceneDepth"};
    if (!Core::RuntimeScene::Open(Renderer, declaration, nullptr, Scene, Error)) { return false; }
    Render::Viewpoint eye;
    eye.EyeM = {{0, 0, 4}};
    eye.Kind = Render::CameraKind::Orthographic;
    eye.XMagM = eye.YMagM = 2;
    constexpr double kNearM = 0.1;
    eye.ZNearM = kNearM;
    eye.ZFarM = 10;
    Scene->Eye(eye);
    Renderer.SetNativePieceSurfaces(std::array<uint32_t, 1>{0});
    Pieces.BindRenderer(&Renderer);
    Pieces.SetFrame(Frame);
    Materials = {.Walls = Render::PieceSurface(0), .Roofs = Render::PieceSurface(0)};
    return true;
  }

  float Depth() {
    std::vector<float> values;
    if (!Scene->Draw(Error)) { return -1; }
    Renderer.WaitForGpu();
    if (Renderer.ReadDepth(values) != Render::ReadState::Ready ||
        values.size() != size_t{kSide} * kSide) {
      return -1;
    }
    return values[(kSide / 2) * kSide + kSide / 2];
  }

  [[nodiscard]] std::vector<Render::GeometryPageResidency::DigestRecord> Visible() const {
    std::vector<Render::GeometryPageResidency::DigestRecord> records;
    Pieces.ForEachDigest([&](const auto &record) { records.push_back(record); });
    return records;
  }
};

Generators::BakedTile Mesh(float northM, std::optional<uint32_t> cell) {
  Generators::BakedTile tile;
  tile.Digest = cell.value_or(0) + 1u;
  tile.RequestedCell = cell;
  tile.OccupiedCells = cell ? uint64_t{1} << (*cell - 1u) : 3;
  tile.SelectedView = Generators::StructureSelectionDomain({}, kProjection);
  tile.Built.WallCorners = {StoredVertex::Of({{-0.5f, -0.5f, northM}}, {{1, 1}}, {{0, 0, -1}}),
                            StoredVertex::Of({{-0.5f, 0.5f, northM}}, {{1, 1}}, {{0, 0, -1}}),
                            StoredVertex::Of({{0.5f, 0, northM}}, {{1, 1}}, {{0, 0, -1}})};
  tile.Built.WallRun = {0, 1, 2};
  return tile;
}

void RefineAndRestore(Fixture &fixture) {
  auto fine = Mesh(1, 1);
  fine.RequestedDetail = LevelOfDetail::Fine;
  auto selected = kCells;
  selected.front().Detail = LevelOfDetail::Fine;
  const auto depth = fixture.Depth();
  CHECK(fixture.Pieces.StageCell(kTile,
                                 1,
                                 BuildingPageView(fine, fixture.Materials).View(),
                                 fixture.Frame.OriginEcef(),
                                 fixture.Error,
                                 kNewSource) &&
            fixture.Depth() == depth,
        "fixed detail can coexist with its visible adaptive source cell");
  CHECK(fixture.Pieces.ActivateCells(
            kTile, {.Key = kNewSource, .Occupied = 3}, selected, fixture.Error) &&
            fixture.Pieces.CellsActive(kTile, selected, kNewSource) &&
            !fixture.Pieces.HasProjectedTile(
                kTile, {.Key = kNewSource, .Occupied = 3}, {}, kProjection),
        "a mixed adaptive/fixed selection is complete without claiming fully adaptive readiness");
  CHECK(fixture.Pieces.ActivateCells(
            kTile, {.Key = kNewSource, .Occupied = 3}, kCells, fixture.Error) &&
            fixture.Depth() == depth &&
            fixture.Pieces.HasProjectedTile(
                kTile, {.Key = kNewSource, .Occupied = 3}, {}, kProjection),
        "resident adaptive details restore without rebuilding or duplicating visible cells");
}

void Publish(Fixture &fixture) {
  auto &pieces = fixture.Pieces;
  auto &error = fixture.Error;
  const auto &origin = fixture.Frame.OriginEcef();
  const auto whole = Mesh(0, std::nullopt);
  CHECK(pieces.Upload(
            kTile, BuildingPageView(whole, fixture.Materials).View(), origin, error, kOldSource),
        "the previous complete source revision is visible");
  const auto oldDepth = fixture.Depth();
  const auto oldDigest = pieces.Digest();
  CHECK(oldDepth > 0, "the old revision has real depth coverage");
  const auto first = Mesh(1, 1);
  const auto second = Mesh(1, 2);
  const StructureBuildQueue::Landing firstLanding{.Tile = kTile,
                                                  .Baked = &first,
                                                  .AnchorEcef = origin,
                                                  .SourceKey = kNewSource,
                                                  .InitialCells = 3,
                                                  .SourceComplete = false,
                                                  .Footprints = std::nullopt};
  CHECK(!pieces.StageCell(
            kTile, 1, BuildingPageView(first, fixture.Materials).View(), origin, error, 0) &&
            pieces.Digest() == oldDigest,
        "adaptive cell addresses still require a source revision");
  CHECK(PublishStructureSource(pieces, firstLanding, fixture.Materials, error) &&
            fixture.Depth() == oldDepth && fixture.Visible().size() == 1,
        "the first adaptive child remains hidden behind the previous revision");
  CHECK(PublishStructureSource(pieces, firstLanding, fixture.Materials, error) &&
            fixture.Depth() == oldDepth && fixture.Visible().size() == 1,
        "restarting an incomplete source releases hidden children and preserves the old world");
  CHECK(!pieces.HasProjectedTile(kTile, {.Key = kNewSource, .Occupied = 3}, {}, kProjection),
        "an incomplete hidden source cannot claim a ready projected tile");
  CHECK(!pieces.ActivateCells(kTile, {.Key = kNewSource, .Occupied = 3}, kCells, error) &&
            fixture.Depth() == oldDepth && pieces.Digest() == oldDigest,
        "missing children cannot replace the old complete world");
  CHECK(
      pieces.StageCell(
          kTile, 2, BuildingPageView(second, fixture.Materials).View(), origin, error, kNewSource),
      "the remaining adaptive child can stage without selecting a fixed LOD");
  CHECK(pieces.ActivateCells(kTile, {.Key = kNewSource, .Occupied = 3}, kCells, error) &&
            fixture.Depth() > 0 && fixture.Depth() != oldDepth,
        "one complete visibility transaction replaces the old source");
  const auto visible = fixture.Visible();
  CHECK(visible.size() == kCells.size() &&
            std::ranges::all_of(visible,
                                [](const auto &record) {
                                  return record.SourceKey == kNewSource && record.Cell != 0 &&
                                         !record.Detail;
                                }),
        "visible capture provenance contains exactly the adaptive source cells");
  CHECK(pieces.CellsActive(kTile, kCells, kNewSource) &&
            pieces.HasProjectedTile(kTile, {.Key = kNewSource, .Occupied = 3}, {}, kProjection),
        "the full active adaptive set satisfies the existing projected readiness query");
  CHECK(!pieces.HasProjectedTile(kTile, {.Key = kOldSource, .Occupied = 3}, {}, kProjection) &&
            !pieces.HasProjectedTile(kTile, {.Key = kNewSource, .Occupied = 1}, {}, kProjection),
        "readiness rejects the old revision and incomplete source masks");
  CHECK(
      !pieces.HasProjectedTile(kTile,
                               {.Key = kNewSource, .Occupied = 3},
                               {.LongitudeDeg = 1, .LatitudeDeg = 0},
                               kProjection) &&
          !pieces.HasProjectedTile(
              kTile, {.Key = kNewSource, .Occupied = 3}, {}, {.FocalPx = kProjection.FocalPx / 2}),
      "adaptive readiness keeps camera-reuse and projection limits");
  RefineAndRestore(fixture);
  constexpr uint32_t kInitialTile = kTile + 1;
  StructureBuildQueue::Landing initial{.Tile = kInitialTile,
                                       .Baked = &first,
                                       .AnchorEcef = origin,
                                       .SourceKey = kNewSource,
                                       .InitialCells = 3,
                                       .SourceComplete = false,
                                       .Footprints = std::nullopt};
  CHECK(PublishStructureSource(pieces, initial, fixture.Materials, error) &&
            fixture.Visible().size() == 2,
        "an initial source can stage without constructing a whole-tile safety mesh");
  initial.Baked = &second;
  initial.SourceComplete = true;
  CHECK(PublishStructureSource(pieces, initial, fixture.Materials, error) &&
            fixture.Visible().size() == 4 && pieces.CellsActive(kInitialTile, kCells, kNewSource),
        "the first complete adaptive cell set publishes without a monolithic predecessor");
}

void CoverageTracksLiveSourceInputs(Fixture &fixture) {
  auto &pieces = fixture.Pieces;
  const auto reset = [&] {
    pieces.Clear();
    pieces.BindRenderer(&fixture.Renderer);
    pieces.SetFrame(fixture.Frame);
  };
  reset();
  Generators::Osm::BuildingField footprints;
  BuildingSourceCoverage coverage;
  Generators::Osm::BuildingField::Baked accepted;
  accepted.OccupiedCells = 3;
  const Data::TileSourceIdentity vector{.Kind = Data::DataKind::VectorMap,
                                        .Tile = {.Zoom = 14, .X = 7, .Y = 8},
                                        .SourceId = "coverage-source",
                                        .Revision = "one"};
  const auto accept = [&](uint64_t heightDigest) {
    Generators::Osm::BuildingField::BakeInputs input;
    input.HeightRasterDigest = heightDigest;
    input.PreparedAssetKey = "native-basis";
    auto pending = footprints.PrepareAcceptance(kTile, accepted, {}, true, vector, input);
    if (footprints.InputOfTile(kTile)) {
      footprints.ReplaceAcceptance(std::move(pending), accepted);
    } else {
      footprints.CommitAcceptance(std::move(pending), accepted);
    }
    return footprints.InputOfTile(kTile)->SourceKey;
  };
  const auto upload = [&](const Generators::BakedTile &baked, uint64_t source) {
    return pieces.Upload(kTile,
                         BuildingPageView(baked, fixture.Materials).View(),
                         fixture.Frame.OriginEcef(),
                         fixture.Error,
                         source);
  };
  auto whole = Mesh(0, std::nullopt);
  const auto firstSource = accept(1);
  CHECK(upload(whole, firstSource) && coverage.Update(pieces, footprints) &&
            coverage.Regions().size() == 1 && !coverage.Update(pieces, footprints),
        "a complete published source is cached until either input changes");
  const auto geometryRevision = pieces.Digest();
  const auto secondSource = accept(2);
  CHECK(firstSource != secondSource && pieces.Digest() == geometryRevision &&
            coverage.Update(pieces, footprints) && coverage.Regions().empty(),
        "a changed semantic input withdraws stale coverage without a geometry publication");
  CHECK(upload(whole, secondSource) && coverage.Update(pieces, footprints) &&
            coverage.Regions().size() == 1,
        "matching replacement geometry restores coverage for the new source");
  reset();
  const auto first = Mesh(0, 1);
  const auto second = Mesh(0, 2);
  const auto stage = [&](uint32_t cell, const Generators::BakedTile &baked) {
    return pieces.StageCell(kTile,
                            cell,
                            BuildingPageView(baked, fixture.Materials).View(),
                            fixture.Frame.OriginEcef(),
                            fixture.Error,
                            secondSource);
  };
  CHECK(stage(1, first) && coverage.Update(pieces, footprints) && coverage.Regions().empty(),
        "a staged child cannot suppress its original parent region");
  CHECK(stage(2, second), "the remaining source child stages");
  (void)coverage.Update(pieces, footprints);
  CHECK(coverage.Regions().empty(), "complete hidden children still publish no coverage");
  CHECK(pieces.ActivateCells(kTile, {.Key = secondSource, .Occupied = 3}, kCells, fixture.Error) &&
            coverage.Update(pieces, footprints) && coverage.Regions().size() == 1,
        "only the complete activated cell union covers its source");
  pieces.RetireCell(kTile, 1);
  CHECK(coverage.Update(pieces, footprints) && coverage.Regions().empty(),
        "a remaining partial source cannot claim whole-tile coverage");
  reset();
  whole.FallbackHeights = true;
  CHECK(upload(whole, secondSource) && coverage.Update(pieces, footprints) &&
            coverage.Regions().empty(),
        "fallback heights do not establish qualified source coverage");
  whole.FallbackHeights = false;
  CHECK(upload(whole, secondSource) && coverage.Update(pieces, footprints) &&
            coverage.Regions().size() == 1,
        "refined heights restore valid source coverage");
  footprints.ResetDerived();
  CHECK(coverage.Update(pieces, footprints) && coverage.Regions().empty(),
        "reset semantic inputs cannot leave old regions published");
  reset();
}

}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Fixture fixture;
    const bool opened = fixture.Open();
    CHECK(opened, "the world fixture opens");
    if (opened) {
      Publish(fixture);
      CoverageTracksLiveSourceInputs(fixture);
    }
  }
  SDL_Quit();
  return Report();
}
