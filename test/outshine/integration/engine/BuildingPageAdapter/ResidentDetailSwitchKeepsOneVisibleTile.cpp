#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "GeometryPageResidency.h"
#include "BuildingPageAdapter.h"
#include "WorldCandidate.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr uint64_t kShellDigest = 22;
constexpr uint64_t kRevisedDigest = 44, kAutomaticDigest = 33, kRefreshedDigest = 34;
constexpr uint64_t kCellDigest = 55, kNextFirstDigest = 77, kNextSecondDigest = 88;
constexpr uint64_t kProjectedDigest = 99, kWholeDigest = 111;
constexpr double kLatitudeDeg = 47, kFocalPx = 720, kOutsideLongitudeDeg = 9.01;

struct MeshInput {
  float NorthM;
  uint64_t Digest;
  LevelOfDetail Detail;
};

Generators::BakedTile MakeMesh(MeshInput input) {
  Generators::BakedTile tile;
  tile.RequestedDetail = input.Detail;
  tile.Digest = input.Digest;
  tile.Built.WallCorners = {
      StoredVertex::Of({{-0.5f, -0.5f, input.NorthM}}, {{1, 1}}, {{0, 0, -1}}),
      StoredVertex::Of({{-0.5f, 0.5f, input.NorthM}}, {{1, 1}}, {{0, 0, -1}}),
      StoredVertex::Of({{0.5f, 0, input.NorthM}}, {{1, 1}}, {{0, 0, -1}})};
  tile.Built.WallRun = {0, 1, 2};
  return tile;
}

struct Fixture {
  Render::SceneRenderer renderer;
  std::unique_ptr<Core::RuntimeScene> scene;
  Render::GeometryPageResidency pieces;
  BuildingPageMaterials materials{.Walls = Render::PieceSurface(0),
                                  .Roofs = Render::PieceSurface(0)};
  std::string error;
  TangentFrame frame = TangentFrame::At({});
  Render::Viewpoint eye;
  Generators::BakedTile fine, shell, revised, automatic, stagedOne, stagedTwo, stagedShell;
  std::array<Render::GeometryPageResidency::CellSelection, 2> cells{};
  float cellFineDepth = 0;

  bool Open() {
    Geometry base;
    const auto surface = base.addSurface("structure", Material{});
    if (!surface) { return false; }
    const auto part = base.addPart("offscreen", *surface);
    constexpr float kOutsideM = 100;
    if (!part ||
        !base.setPositions(
            *part, std::array<float, 9>{kOutsideM, 0, 0, kOutsideM + 1, 0, 0, kOutsideM, 1, 0}) ||
        !base.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2})) {
      return false;
    }
    Core::Declaration declaration;
    declaration.InitialGeometry = &base;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"sceneDepth"};
    if (!Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error)) { return false; }
    eye.EyeM = {{0, 0, 4}};
    eye.Kind = Render::CameraKind::Orthographic;
    eye.XMagM = eye.YMagM = 2;
    constexpr double kNearM = 0.1;
    eye.ZNearM = kNearM;
    eye.ZFarM = 10;
    scene->Eye(eye);
    renderer.SetNativePieceSurfaces(std::array<uint32_t, 1>{0});
    pieces.BindRenderer(&renderer);
    pieces.SetFrame(frame);
    return true;
  }

  float Depth() {
    std::vector<float> values;
    if (!scene->Draw(error)) { return -1; }
    renderer.WaitForGpu();
    if (renderer.ReadDepth(values) != Render::ReadState::Ready ||
        values.size() != size_t{32} * 32) {
      return -1;
    }
    return values[size_t{16} * 32 + 16];
  }

  [[nodiscard]] auto Visible() const {
    std::vector<Render::GeometryPageResidency::DigestRecord> records;
    pieces.ForEachDigest(
        [&](Render::GeometryPageResidency::DigestRecord record) { records.push_back(record); });
    return records;
  }
};

void CheckFixedDetails(Fixture &fixture) {
  fixture.fine = MakeMesh({.NorthM = 0.0f, .Digest = 11, .Detail = LevelOfDetail::Fine});
  fixture.shell =
      MakeMesh({.NorthM = 1.0f, .Digest = kShellDigest, .Detail = LevelOfDetail::Shell});
  CHECK(!fixture.pieces.Upload(7,
                               BuildingPageView(fixture.fine, fixture.materials).View(),
                               fixture.frame.OriginEcef(),
                               fixture.error) &&
            fixture.renderer.PiecesStanding() == 0,
        "an unkeyed explicit variant cannot enter residency");
  CHECK(fixture.pieces.Upload(7,
                              BuildingPageView(fixture.fine, fixture.materials).View(),
                              fixture.frame.OriginEcef(),
                              fixture.error,
                              1),
        "first keyed detail becomes visible");
  const float fineDepth = fixture.Depth();
  CHECK(fineDepth > 0.0f && fixture.pieces.Digest() != 0, "fine product covers the probe");
  const auto fineDigest = fixture.pieces.Digest();
  CHECK(fixture.pieces.Upload(7,
                              BuildingPageView(fixture.shell, fixture.materials).View(),
                              fixture.frame.OriginEcef(),
                              fixture.error,
                              1),
        "second detail from the same source becomes resident");
  CHECK(fixture.renderer.PiecesStanding() == 2 && fixture.pieces.Handles().size() == 2 &&
            fixture.pieces.Digest() == fineDigest && fixture.Depth() == fineDepth,
        "hidden detail neither replaces nor duplicates the visible image");
  CHECK(fixture.Visible().size() == 1 && fixture.Visible().front().SourceKey == 1 &&
            fixture.Visible().front().Detail == LevelOfDetail::Fine,
        "capture provenance names only the visible source and detail");
  Core::WorldCandidate candidate(fixture.renderer);
  CHECK(candidate.Prepare(*fixture.scene, nullptr).has_value(), "candidate restores both variants");
  CHECK(candidate.Publish(fixture.scene).has_value(), "candidate publishes resident variants");
  fixture.scene->Eye(fixture.eye);
  CHECK(fixture.scene->Advance(fixture.error), "published camera advances");
  CHECK(fixture.Depth() == fineDepth, "publication keeps the hidden alternative hidden");
  CHECK(fixture.pieces.SelectDetail(7, LevelOfDetail::Shell, fixture.error),
        "shell becomes selected");
  const float shellDepth = fixture.Depth();
  CHECK(shellDepth > 0.0f && shellDepth != fineDepth && fixture.pieces.Digest() != fineDigest,
        "selected shell replaces fine at the probe");
  CHECK(fixture.Visible().size() == 1 && fixture.Visible().front().Digest == 22 &&
            fixture.Visible().front().Detail == LevelOfDetail::Shell,
        "provenance follows the selected resident product");
  CHECK(!fixture.pieces.SelectDetail(7, LevelOfDetail::Massed, fixture.error) &&
            fixture.Depth() == shellDepth,
        "missing detail leaves the selected product visible");
  CHECK(fixture.pieces.SelectDetail(7, LevelOfDetail::Fine, fixture.error) &&
            fixture.Depth() == fineDepth,
        "switching back restores the same fine geometry");
  fixture.revised =
      MakeMesh({.NorthM = -0.5f, .Digest = kRevisedDigest, .Detail = LevelOfDetail::Fine});
  for (StoredVertex &corner : fixture.revised.Built.WallCorners) { corner.texture = {{0, 0}}; }
  CHECK(!fixture.pieces.Upload(7,
                               BuildingPageView(fixture.revised, fixture.materials).View(),
                               fixture.frame.OriginEcef(),
                               fixture.error,
                               2) &&
            fixture.renderer.PiecesStanding() == 2 && fixture.Depth() == fineDepth,
        "failed new-source upload preserves both old resident variants");
  fixture.revised =
      MakeMesh({.NorthM = -0.5f, .Digest = kRevisedDigest, .Detail = LevelOfDetail::Fine});
  CHECK(fixture.pieces.Upload(7,
                              BuildingPageView(fixture.revised, fixture.materials).View(),
                              fixture.frame.OriginEcef(),
                              fixture.error,
                              2) &&
            fixture.renderer.PiecesStanding() == 1 && fixture.Depth() != fineDepth &&
            fixture.Depth() > 0.0f,
        "new source atomically replaces all old variants");
  CHECK(!fixture.pieces.SelectDetail(7, LevelOfDetail::Shell, fixture.error),
        "a retired source detail cannot be selected");
  fixture.automatic =
      MakeMesh({.NorthM = 0.5f, .Digest = kAutomaticDigest, .Detail = LevelOfDetail::Fine});
  fixture.automatic.RequestedDetail.reset();
  CHECK(fixture.pieces.Upload(7,
                              BuildingPageView(fixture.automatic, fixture.materials).View(),
                              fixture.frame.OriginEcef(),
                              fixture.error) &&
            fixture.renderer.PiecesStanding() == 1 && fixture.Depth() > 0.0f,
        "automatic replacement removes both variants without leaving the tile hidden");
  CHECK(fixture.Visible().size() == 1 && fixture.Visible().front().SourceKey == 0 &&
            !fixture.Visible().front().Detail,
        "automatic camera-dependent geometry is explicit in capture provenance");
}

void CheckInitialCells(Fixture &fixture) {
  fixture.automatic.OccupiedCells = 3;
  CHECK(fixture.pieces.Upload(7,
                              BuildingPageView(fixture.automatic, fixture.materials).View(),
                              fixture.frame.OriginEcef(),
                              fixture.error,
                              11),
        "source-keyed whole-tile safety net is resident");
  const float legacyDepth = fixture.Depth();
  fixture.stagedOne = fixture.fine;
  fixture.stagedOne.RequestedCell = 1;
  fixture.stagedOne.OccupiedCells = 1;
  fixture.stagedTwo = fixture.shell;
  fixture.stagedTwo.RequestedCell = 2;
  fixture.stagedTwo.OccupiedCells = 2;
  CHECK(!fixture.pieces.Upload(7,
                               1,
                               BuildingPageView(fixture.stagedOne, fixture.materials).View(),
                               fixture.frame.OriginEcef(),
                               fixture.error,
                               11) &&
            fixture.pieces.StageCell(7,
                                     1,
                                     BuildingPageView(fixture.stagedOne, fixture.materials).View(),
                                     fixture.frame.OriginEcef(),
                                     fixture.error,
                                     11) &&
            fixture.Depth() == legacyDepth && fixture.Visible().size() == 1,
        "a staged cell cannot duplicate the visible whole-tile safety net");
  auto refreshedWhole = fixture.automatic;
  refreshedWhole.Digest = kRefreshedDigest;
  CHECK(fixture.pieces.Upload(7,
                              BuildingPageView(refreshedWhole, fixture.materials).View(),
                              fixture.frame.OriginEcef(),
                              fixture.error,
                              11) &&
            fixture.pieces.HasCell(7, 1, LevelOfDetail::Fine, 11) &&
            fixture.renderer.PiecesStanding() == 2 && fixture.Depth() == legacyDepth,
        "same-source fallback refresh preserves the hidden cell product");
  fixture.cells = {
      {{.Cell = 1, .Detail = LevelOfDetail::Fine}, {.Cell = 2, .Detail = LevelOfDetail::Shell}}};
  CHECK(!fixture.pieces.CellsActive(7, fixture.cells, 11),
        "one hidden cell cannot claim complete active residency");
  CHECK(!fixture.pieces.ActivateCells(
            7, {.Key = 11, .Occupied = 3}, std::span(fixture.cells.data(), 1), fixture.error) &&
            fixture.Depth() == legacyDepth && fixture.Visible().size() == 1,
        "a missing source cell cannot retire the safety net");
  CHECK(fixture.pieces.StageCell(7,
                                 2,
                                 BuildingPageView(fixture.stagedTwo, fixture.materials).View(),
                                 fixture.frame.OriginEcef(),
                                 fixture.error,
                                 11) &&
            !fixture.pieces.ActivateCells(
                7, {.Key = 12, .Occupied = 3}, fixture.cells, fixture.error) &&
            fixture.Depth() == legacyDepth,
        "a stale source key leaves both staged cells hidden");
  CHECK(fixture.pieces.ActivateCells(7, {.Key = 11, .Occupied = 3}, fixture.cells, fixture.error) &&
            fixture.renderer.PiecesStanding() == 2 && fixture.Visible().size() == 2 &&
            fixture.Visible()[0].Cell == 1 && fixture.Visible()[1].Cell == 2 &&
            fixture.Depth() != legacyDepth,
        "one visibility transaction replaces the safety net with complete cell geometry");
  CHECK(fixture.pieces.CellsActive(7, fixture.cells, 11) &&
            !fixture.pieces.CellsActive(7, fixture.cells, 12),
        "active selection includes the source revision and every chosen detail");
}

void CheckCellDetails(Fixture &fixture) {
  fixture.cellFineDepth = fixture.Depth();
  fixture.stagedShell =
      MakeMesh({.NorthM = -0.5f, .Digest = kCellDigest, .Detail = LevelOfDetail::Shell});
  fixture.stagedShell.RequestedCell = 1;
  fixture.stagedShell.OccupiedCells = 1;
  CHECK(fixture.pieces.StageCell(7,
                                 1,
                                 BuildingPageView(fixture.stagedShell, fixture.materials).View(),
                                 fixture.frame.OriginEcef(),
                                 fixture.error,
                                 11) &&
            fixture.renderer.PiecesStanding() == 3 && fixture.Depth() == fixture.cellFineDepth,
        "another detail of the same source stages without changing the visible image");
  CHECK(fixture.pieces.HasCell(7, 1, LevelOfDetail::Fine, 11) &&
            fixture.pieces.HasCell(7, 1, LevelOfDetail::Shell, 11) &&
            !fixture.pieces.HasCell(7, 1, LevelOfDetail::Massed, 11) &&
            !fixture.pieces.HasCell(7, 1, LevelOfDetail::Shell, 12) &&
            !fixture.pieces.HasCell(7, 0, LevelOfDetail::Fine, 11),
        "residency distinguishes hidden detail, source revision and invalid cell address");
  CHECK(!fixture.pieces.StageCell(7,
                                  1,
                                  BuildingPageView(fixture.stagedShell, fixture.materials).View(),
                                  fixture.frame.OriginEcef(),
                                  fixture.error,
                                  11) &&
            fixture.renderer.PiecesStanding() == 3 && fixture.Depth() == fixture.cellFineDepth,
        "a duplicate staged cell detail is rejected without replacing the resident");
  fixture.cells[0].Detail = LevelOfDetail::Shell;
  CHECK(fixture.pieces.ActivateCells(7, {.Key = 11, .Occupied = 3}, fixture.cells, fixture.error) &&
            fixture.Visible().size() == 2 && fixture.Visible()[0].Digest == kCellDigest &&
            fixture.Depth() > 0.0f && fixture.Depth() != fixture.cellFineDepth,
        "a later detail product replaces only its selected cell at the probe");
  fixture.cells[0].Detail = LevelOfDetail::Fine;
  CHECK(fixture.pieces.ActivateCells(7, {.Key = 11, .Occupied = 3}, fixture.cells, fixture.error) &&
            fixture.Visible()[0].Digest == 11 && fixture.Depth() == fixture.cellFineDepth,
        "the prior detail remains resident for a reversible view selection");
}

void CheckSourceRevision(Fixture &fixture) {
  auto nextOne = fixture.stagedShell;
  nextOne.Digest = kNextFirstDigest;
  auto nextTwo = fixture.stagedTwo;
  nextTwo.Digest = kNextSecondDigest;
  CHECK(fixture.pieces.StageCell(7,
                                 1,
                                 BuildingPageView(nextOne, fixture.materials).View(),
                                 fixture.frame.OriginEcef(),
                                 fixture.error,
                                 12) &&
            fixture.Depth() == fixture.cellFineDepth,
        "a new source stages beside the visible revision");
  auto nextAgain = nextOne;
  nextAgain.Digest = kProjectedDigest;
  for (StoredVertex &corner : nextAgain.Built.WallCorners) { corner.texture = {{0, 0}}; }
  CHECK(!fixture.pieces.StageCell(7,
                                  1,
                                  BuildingPageView(nextAgain, fixture.materials).View(),
                                  fixture.frame.OriginEcef(),
                                  fixture.error,
                                  13) &&
            fixture.renderer.PiecesStanding() == 4 && fixture.Depth() == fixture.cellFineDepth,
        "failed newer-source upload retains the old image and prior staged revision");
  nextAgain = nextOne;
  CHECK(fixture.pieces.StageCell(7,
                                 1,
                                 BuildingPageView(nextAgain, fixture.materials).View(),
                                 fixture.frame.OriginEcef(),
                                 fixture.error,
                                 13) &&
            fixture.renderer.PiecesStanding() == 4 && fixture.Depth() == fixture.cellFineDepth &&
            fixture.pieces.StageCell(7,
                                     1,
                                     BuildingPageView(nextOne, fixture.materials).View(),
                                     fixture.frame.OriginEcef(),
                                     fixture.error,
                                     12) &&
            fixture.renderer.PiecesStanding() == 4,
        "successful newer-source stage retires only superseded hidden products");
  CHECK(fixture.pieces.StageCell(7,
                                 2,
                                 BuildingPageView(nextTwo, fixture.materials).View(),
                                 fixture.frame.OriginEcef(),
                                 fixture.error,
                                 12) &&
            !fixture.pieces.ActivateCells(
                7, {.Key = 12, .Occupied = 3}, fixture.cells, fixture.error) &&
            fixture.Depth() == fixture.cellFineDepth,
        "an incomplete new source selection retains the old visible cells");
  fixture.cells[0].Detail = LevelOfDetail::Shell;
  CHECK(fixture.pieces.ActivateCells(7, {.Key = 12, .Occupied = 3}, fixture.cells, fixture.error) &&
            fixture.renderer.PiecesStanding() == 2 && fixture.Visible().size() == 2 &&
            fixture.Visible()[0].SourceKey == 12 && fixture.Visible()[1].SourceKey == 12 &&
            fixture.Visible()[0].Digest == kNextFirstDigest &&
            fixture.Visible()[1].Digest == kNextSecondDigest,
        "complete new-source cells replace and retire all old variants in one transaction");
  CHECK(!fixture.pieces.CellsActive(7, fixture.cells, 11) &&
            fixture.pieces.CellsActive(7, fixture.cells, 12),
        "publication revokes the old active source selection");
  CHECK(!fixture.pieces.HasCell(7, 1, LevelOfDetail::Fine, 11) &&
            fixture.pieces.HasCell(7, 1, LevelOfDetail::Shell, 12),
        "retired source variants cannot satisfy a later detail request");
}

void CheckWholeFallback(Fixture &fixture) {
  auto whole = fixture.automatic;
  whole.Digest = kWholeDigest;
  for (StoredVertex &corner : whole.Built.WallCorners) { corner.texture = {{0, 0}}; }
  const float cellDepth = fixture.Depth();
  CHECK(!fixture.pieces.Upload(7,
                               BuildingPageView(whole, fixture.materials).View(),
                               fixture.frame.OriginEcef(),
                               fixture.error,
                               13) &&
            fixture.renderer.PiecesStanding() == 2 && fixture.Depth() == cellDepth,
        "failed whole-tile fallback leaves active cells visible");
  whole = fixture.automatic;
  whole.Digest = kWholeDigest;
  CHECK(fixture.pieces.Upload(7,
                              BuildingPageView(whole, fixture.materials).View(),
                              fixture.frame.OriginEcef(),
                              fixture.error,
                              13) &&
            fixture.renderer.PiecesStanding() == 1 && fixture.Visible().size() == 1 &&
            fixture.Visible()[0].Cell == 0 && fixture.Visible()[0].SourceKey == 13 &&
            fixture.Visible()[0].Digest == kWholeDigest && fixture.Depth() > 0.0f,
        "new whole tile retires every visible and hidden cell product after upload");
  fixture.pieces.Retire(7);
  CHECK(fixture.renderer.PiecesStanding() == 0 && fixture.Visible().empty(),
        "legacy whole-tile geometry retires before cell products are published");
}

void CheckIndependentCells(Fixture &fixture) {
  auto fineCellOne = fixture.fine;
  fineCellOne.RequestedCell = 1;
  auto shellCellOne = fixture.shell;
  shellCellOne.RequestedCell = 1;
  auto fineCellTwo = fixture.fine;
  fineCellTwo.RequestedCell = 2;
  CHECK(!fixture.pieces.Upload(7,
                               2,
                               BuildingPageView(fineCellOne, fixture.materials).View(),
                               fixture.frame.OriginEcef(),
                               fixture.error,
                               11) &&
            fixture.renderer.PiecesStanding() == 0,
        "a product cannot be published under another cell address");
  CHECK(fixture.pieces.Upload(7,
                              1,
                              BuildingPageView(fineCellOne, fixture.materials).View(),
                              fixture.frame.OriginEcef(),
                              fixture.error,
                              11) &&
            fixture.pieces.Upload(7,
                                  1,
                                  BuildingPageView(shellCellOne, fixture.materials).View(),
                                  fixture.frame.OriginEcef(),
                                  fixture.error,
                                  11) &&
            fixture.pieces.Upload(7,
                                  2,
                                  BuildingPageView(fineCellTwo, fixture.materials).View(),
                                  fixture.frame.OriginEcef(),
                                  fixture.error,
                                  22),
        "two cells retain their own source-keyed detail products");
  CHECK(fixture.renderer.PiecesStanding() == 3 && fixture.Visible().size() == 2 &&
            fixture.Visible()[0].Cell == 1 && fixture.Visible()[1].Cell == 2 &&
            fixture.Visible()[0].Digest == 11 && fixture.Visible()[1].Digest == 11,
        "only one detail per cell is visible");
  CHECK(fixture.pieces.SelectDetail(7, 1, LevelOfDetail::Shell, fixture.error) &&
            fixture.Visible()[0].Digest == 22 && fixture.Visible()[1].Digest == 11,
        "switching one cell leaves its neighbour selected");
  CHECK(!fixture.pieces.SelectDetail(7, 2, LevelOfDetail::Shell, fixture.error) &&
            fixture.Visible()[1].Digest == 11,
        "missing neighbour detail leaves that cell visible");
  auto invalidCell =
      MakeMesh({.NorthM = -0.5f, .Digest = kCellDigest, .Detail = LevelOfDetail::Fine});
  invalidCell.RequestedCell = 1;
  for (StoredVertex &corner : invalidCell.Built.WallCorners) { corner.texture = {{0, 0}}; }
  CHECK(!fixture.pieces.Upload(7,
                               1,
                               BuildingPageView(invalidCell, fixture.materials).View(),
                               fixture.frame.OriginEcef(),
                               fixture.error,
                               kAutomaticDigest) &&
            fixture.renderer.PiecesStanding() == 3 && fixture.Visible()[0].Digest == 22 &&
            fixture.Visible()[1].Digest == 11,
        "failed cell upload preserves its previous detail and the neighbour");
  fixture.revised.RequestedCell = 1;
  CHECK(fixture.pieces.Upload(7,
                              1,
                              BuildingPageView(fixture.revised, fixture.materials).View(),
                              fixture.frame.OriginEcef(),
                              fixture.error,
                              kAutomaticDigest) &&
            fixture.renderer.PiecesStanding() == 2 &&
            fixture.Visible()[0].SourceKey == kAutomaticDigest &&
            fixture.Visible()[1].SourceKey == 22,
        "a new source replaces only the addressed cell");
  fixture.pieces.RetireCell(7, 1);
  CHECK(fixture.renderer.PiecesStanding() == 1 && fixture.Visible().size() == 1 &&
            fixture.Visible()[0].Cell == 2,
        "evicting one cell preserves other products in the tile");
  fixture.pieces.Retire(7);
  CHECK(fixture.renderer.PiecesStanding() == 0 && fixture.pieces.Handles().empty() &&
            fixture.Depth() == 0.0f,
        "forgetting the tile releases both resident variants");
}

void CheckProjectedCoverage(Fixture &fixture) {
  auto projected =
      MakeMesh({.NorthM = 0.0f, .Digest = kProjectedDigest, .Detail = LevelOfDetail::Fine});
  projected.RequestedDetail.reset();
  projected.OccupiedCells = 3;
  projected.SelectedView = Generators::StructureSelectionDomain(
      {.LongitudeDeg = 9, .LatitudeDeg = kLatitudeDeg}, {.FocalPx = kFocalPx});
  CHECK(fixture.pieces.Upload(7,
                              BuildingPageView(projected, fixture.materials).View(),
                              fixture.frame.OriginEcef(),
                              fixture.error,
                              kAutomaticDigest),
        "a selected plan product publishes as one native tile");
  CHECK(fixture.pieces.HasProjectedTile(7,
                                        {.Key = kAutomaticDigest, .Occupied = 3},
                                        {.LongitudeDeg = 9, .LatitudeDeg = kLatitudeDeg},
                                        {.FocalPx = kFocalPx}),
        "the matching view reuses complete roundabout geometry without forced cell variants");
  CHECK(!fixture.pieces.HasProjectedTile(
            7,
            {.Key = kAutomaticDigest, .Occupied = 3},
            {.LongitudeDeg = kOutsideLongitudeDeg, .LatitudeDeg = kLatitudeDeg},
            {.FocalPx = kFocalPx}) &&
            !fixture.pieces.HasProjectedTile(7,
                                             {.Key = kAutomaticDigest, .Occupied = 1},
                                             {.LongitudeDeg = 9, .LatitudeDeg = kLatitudeDeg},
                                             {.FocalPx = kFocalPx}) &&
            !fixture.pieces.HasProjectedTile(7,
                                             {.Key = kAutomaticDigest, .Occupied = 3},
                                             {.LongitudeDeg = 9, .LatitudeDeg = kLatitudeDeg},
                                             {.FocalPx = 1440}),
        "movement, incomplete source coverage and changed projection cannot reuse the product");
  fixture.pieces.Retire(7);
}

}

int main() {
  using namespace outshine::Test;
  const bool video = SDL_Init(SDL_INIT_VIDEO);
  CHECK(video, "video initializes");
  if (video) {
    Fixture fixture;
    const bool opened = fixture.Open();
    CHECK(opened, "native geometry and camera fixture opens");
    if (opened) {
      CheckFixedDetails(fixture);
      CheckInitialCells(fixture);
      CheckCellDetails(fixture);
      CheckSourceRevision(fixture);
      CheckWholeFallback(fixture);
      CheckIndependentCells(fixture);
      CheckProjectedCoverage(fixture);
    }
  }
  SDL_Quit();
  return Report();
}
