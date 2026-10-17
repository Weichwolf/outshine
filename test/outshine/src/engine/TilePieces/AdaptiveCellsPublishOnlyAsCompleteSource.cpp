#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "TilePieces.h"
#include "StructureSourcePublication.h"
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

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr int kSide = 32;
constexpr uint32_t kTile = 7;
constexpr uint64_t kOldSource = 91;
constexpr uint64_t kNewSource = 92;
constexpr ProjectedErrorBudget kProjection{.FocalPx = 720};
constexpr std::array<TilePieces::CellSelection, 2> kCells{
    {{.Cell = 1, .Detail = std::nullopt}, {.Cell = 2, .Detail = std::nullopt}}};

struct Fixture {
  Geometry Base;
  Render::SceneRenderer Renderer;
  std::unique_ptr<Core::RuntimeScene> Scene;
  TilePieces Pieces;
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
    Pieces.Into(&Renderer);
    Pieces.Framed(Frame);
    Pieces.SetSurfaces({.Walls = Render::PieceSurface(0), .Roofs = Render::PieceSurface(0)});
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

  std::vector<TilePieces::DigestRecord> Visible() const {
    std::vector<TilePieces::DigestRecord> records;
    Pieces.ForEachDigest([&](const auto &record) { records.push_back(record); });
    return records;
  }
};

Generators::BakedTile Mesh(float northM, std::optional<uint32_t> cell) {
  Generators::BakedTile tile;
  tile.Digest = cell.value_or(0) + 1u;
  tile.RequestedCell = cell;
  tile.OccupiedCells = cell ? uint64_t{1} << (*cell - 1u) : 3;
  tile.SelectedView = Generators::StructureSelectionView{.Eye = {}, .Projection = kProjection};
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
  CHECK(fixture.Pieces.StageCell(
            kTile, 1, fine, fixture.Frame.OriginEcef(), fixture.Error, kNewSource) &&
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
  CHECK(pieces.Hands(kTile, whole, origin, error, kOldSource),
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
  CHECK(!pieces.StageCell(kTile, 1, first, origin, error, 0) && pieces.Digest() == oldDigest,
        "adaptive cell addresses still require a source revision");
  CHECK(PublishStructureSource(pieces, firstLanding, error) && fixture.Depth() == oldDepth &&
            fixture.Visible().size() == 1,
        "the first adaptive child remains hidden behind the previous revision");
  CHECK(PublishStructureSource(pieces, firstLanding, error) && fixture.Depth() == oldDepth &&
            fixture.Visible().size() == 1,
        "restarting an incomplete source releases hidden children and preserves the old world");
  CHECK(!pieces.HasProjectedTile(kTile, {.Key = kNewSource, .Occupied = 3}, {}, kProjection),
        "an incomplete hidden source cannot claim a ready projected tile");
  CHECK(!pieces.ActivateCells(kTile, {.Key = kNewSource, .Occupied = 3}, kCells, error) &&
            fixture.Depth() == oldDepth && pieces.Digest() == oldDigest,
        "missing children cannot replace the old complete world");
  CHECK(pieces.StageCell(kTile, 2, second, origin, error, kNewSource),
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
  CHECK(PublishStructureSource(pieces, initial, error) && fixture.Visible().size() == 2,
        "an initial source can stage without constructing a whole-tile safety mesh");
  initial.Baked = &second;
  initial.SourceComplete = true;
  CHECK(PublishStructureSource(pieces, initial, error) && fixture.Visible().size() == 4 &&
            pieces.CellsActive(kInitialTile, kCells, kNewSource),
        "the first complete adaptive cell set publishes without a monolithic predecessor");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Fixture fixture;
    const bool opened = fixture.Open();
    CHECK(opened, "the world fixture opens");
    if (opened) { Publish(fixture); }
  }
  SDL_Quit();
  return Report();
}
