#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "TilePieces.h"
#include "WorldCandidate.h"
#include "Check.h"

#include <SDL3/SDL.h>
#include <array>
#include <memory>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Geometry base;
    const auto surface = base.addSurface("structure", Material{});
    CHECK(surface.has_value(), "fixture material exists");
    if (!surface) { return Report(); }
    const auto part = base.addPart("offscreen", *surface);
    CHECK(part && base.setPositions(*part, std::array<float, 9>{100, 0, 0, 101, 0, 0, 100, 1, 0}) &&
              base.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
          "offscreen base provides the native material slot");
    Core::Declaration declaration;
    declaration.InitialGeometry = &base;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"sceneDepth"};
    Render::SceneRenderer renderer;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), "scene opens");
    if (scene) {
      Render::Viewpoint eye;
      eye.EyeM = {{0, 0, 4}};
      eye.Kind = Render::CameraKind::Orthographic;
      eye.XMagM = eye.YMagM = 2;
      eye.ZNearM = 0.1;
      eye.ZFarM = 10;
      scene->Eye(eye);
      renderer.SetNativePieceSurfaces(std::array<uint32_t, 1>{0});
      TilePieces pieces;
      pieces.Into(&renderer);
      const auto visible = [&]() {
        std::vector<TilePieces::DigestRecord> records;
        pieces.ForEachDigest([&](TilePieces::DigestRecord record) { records.push_back(record); });
        return records;
      };
      const TangentFrame frame = TangentFrame::At({});
      pieces.Framed(frame);
      pieces.Wears({.Walls = Render::PieceSurface(0), .Roofs = Render::PieceSurface(0)});
      const auto depth = [&]() {
        std::vector<float> values;
        if (!scene->Draw(error)) { return -1.0f; }
        renderer.WaitForGpu();
        if (renderer.ReadDepth(values) != Render::ReadState::Ready || values.size() != 32u * 32u) {
          return -1.0f;
        }
        return values[16u * 32u + 16u];
      };
      const auto mesh = [](float ecefNorthM, uint64_t digest, LevelOfDetail detail) {
        Generators::BakedTile tile;
        tile.RequestedDetail = detail;
        tile.Digest = digest;
        tile.Built.WallCorners = {
            StoredVertex::Of({{-0.5f, -0.5f, ecefNorthM}}, {{1, 1}}, {{0, 0, -1}}),
            StoredVertex::Of({{-0.5f, 0.5f, ecefNorthM}}, {{1, 1}}, {{0, 0, -1}}),
            StoredVertex::Of({{0.5f, 0, ecefNorthM}}, {{1, 1}}, {{0, 0, -1}})};
        tile.Built.WallRun = {0, 1, 2};
        return tile;
      };
      const auto fine = mesh(0.0f, 11, LevelOfDetail::Fine);
      const auto shell = mesh(1.0f, 22, LevelOfDetail::Shell);
      CHECK(!pieces.Hands(7, fine, frame.OriginEcef(), error) && renderer.PiecesStanding() == 0,
            "an unkeyed explicit variant cannot enter residency");
      CHECK(pieces.Hands(7, fine, frame.OriginEcef(), error, 1),
            "first keyed detail becomes visible");
      const float fineDepth = depth();
      CHECK(fineDepth > 0.0f && pieces.Digest() != 0, "fine product covers the probe");
      const auto fineDigest = pieces.Digest();
      CHECK(pieces.Hands(7, shell, frame.OriginEcef(), error, 1),
            "second detail from the same source becomes resident");
      CHECK(renderer.PiecesStanding() == 2 && pieces.Handles().size() == 2 &&
                pieces.Digest() == fineDigest && depth() == fineDepth,
            "hidden detail neither replaces nor duplicates the visible image");
      CHECK(visible().size() == 1 && visible().front().SourceKey == 1 &&
                visible().front().Detail == LevelOfDetail::Fine,
            "capture provenance names only the visible source and detail");
      Core::WorldCandidate candidate(renderer);
      CHECK(candidate.Prepare(*scene, nullptr).has_value(), "candidate restores both variants");
      CHECK(candidate.Publish(scene).has_value(), "candidate publishes resident variants");
      scene->Eye(eye);
      CHECK(scene->Advance(error), "published camera advances");
      CHECK(depth() == fineDepth, "publication keeps the hidden alternative hidden");
      CHECK(pieces.SelectDetail(7, LevelOfDetail::Shell, error), "shell becomes selected");
      const float shellDepth = depth();
      CHECK(shellDepth > 0.0f && shellDepth != fineDepth && pieces.Digest() != fineDigest,
            "selected shell replaces fine at the probe");
      CHECK(visible().size() == 1 && visible().front().Digest == 22 &&
                visible().front().Detail == LevelOfDetail::Shell,
            "provenance follows the selected resident product");
      CHECK(!pieces.SelectDetail(7, LevelOfDetail::Massed, error) && depth() == shellDepth,
            "missing detail leaves the selected product visible");
      CHECK(pieces.SelectDetail(7, LevelOfDetail::Fine, error) && depth() == fineDepth,
            "switching back restores the same fine geometry");
      auto revised = mesh(-0.5f, 44, LevelOfDetail::Fine);
      for (StoredVertex &corner : revised.Built.WallCorners) { corner.texture = {{0, 0}}; }
      CHECK(!pieces.Hands(7, revised, frame.OriginEcef(), error, 2) &&
                renderer.PiecesStanding() == 2 && depth() == fineDepth,
            "failed new-source upload preserves both old resident variants");
      revised = mesh(-0.5f, 44, LevelOfDetail::Fine);
      CHECK(pieces.Hands(7, revised, frame.OriginEcef(), error, 2) &&
                renderer.PiecesStanding() == 1 && depth() != fineDepth && depth() > 0.0f,
            "new source atomically replaces all old variants");
      CHECK(!pieces.SelectDetail(7, LevelOfDetail::Shell, error),
            "a retired source detail cannot be selected");
      auto automatic = mesh(0.5f, 33, LevelOfDetail::Fine);
      automatic.RequestedDetail.reset();
      CHECK(pieces.Hands(7, automatic, frame.OriginEcef(), error) &&
                renderer.PiecesStanding() == 1 && depth() > 0.0f,
            "automatic replacement removes both variants without leaving the tile hidden");
      CHECK(visible().size() == 1 && visible().front().SourceKey == 0 && !visible().front().Detail,
            "automatic camera-dependent geometry is explicit in capture provenance");
      pieces.Forgets(7);
      CHECK(renderer.PiecesStanding() == 0 && visible().empty(),
            "legacy whole-tile geometry retires before cell products are published");
      CHECK(pieces.Hands(7, 1, fine, frame.OriginEcef(), error, 11) &&
                pieces.Hands(7, 1, shell, frame.OriginEcef(), error, 11) &&
                pieces.Hands(7, 2, fine, frame.OriginEcef(), error, 22),
            "two cells retain their own source-keyed detail products");
      CHECK(renderer.PiecesStanding() == 3 && visible().size() == 2 && visible()[0].Cell == 1 &&
                visible()[1].Cell == 2 && visible()[0].Digest == 11 && visible()[1].Digest == 11,
            "only one detail per cell is visible");
      CHECK(pieces.SelectDetail(7, 1, LevelOfDetail::Shell, error) && visible()[0].Digest == 22 &&
                visible()[1].Digest == 11,
            "switching one cell leaves its neighbour selected");
      CHECK(!pieces.SelectDetail(7, 2, LevelOfDetail::Shell, error) && visible()[1].Digest == 11,
            "missing neighbour detail leaves that cell visible");
      auto invalidCell = mesh(-0.5f, 55, LevelOfDetail::Fine);
      for (StoredVertex &corner : invalidCell.Built.WallCorners) { corner.texture = {{0, 0}}; }
      CHECK(!pieces.Hands(7, 1, invalidCell, frame.OriginEcef(), error, 33) &&
                renderer.PiecesStanding() == 3 && visible()[0].Digest == 22 &&
                visible()[1].Digest == 11,
            "failed cell upload preserves its previous detail and the neighbour");
      CHECK(pieces.Hands(7, 1, revised, frame.OriginEcef(), error, 33) &&
                renderer.PiecesStanding() == 2 && visible()[0].SourceKey == 33 &&
                visible()[1].SourceKey == 22,
            "a new source replaces only the addressed cell");
      pieces.ForgetsCell(7, 1);
      CHECK(renderer.PiecesStanding() == 1 && visible().size() == 1 && visible()[0].Cell == 2,
            "evicting one cell preserves other products in the tile");
      pieces.Forgets(7);
      CHECK(renderer.PiecesStanding() == 0 && pieces.Handles().empty() && depth() == 0.0f,
            "forgetting the tile releases both resident variants");
    }
  }
  SDL_Quit();
  return Report();
}
