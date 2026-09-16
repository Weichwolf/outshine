#include "Check.h"
#include "Live.h"
#include "SceneRenderer.h"
#include <SDL3/SDL.h>
#include <array>
#include <memory>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Geometry base;
    const auto surface = base.addSurface("wall", Material{});
    CHECK(surface.has_value(), "fixture material exists");
    if (!surface) { return Report(); }
    const auto part = base.addPart("base", *surface);
    CHECK(part && base.setPositions(*part, std::array<float, 9>{0, 0, 0, 1, 0, 0, 0, 1, 0}) &&
              base.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
          "fixture registers the native surface used by pieces");
    Core::Declaration declaration;
    declaration.InitialGeometry = &base;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    Render::SceneRenderer renderer;
    std::unique_ptr<Core::Live> scene;
    std::string error;
    CHECK(Core::Live::Open(renderer, declaration, nullptr, scene, error), "fixture opens");
    if (scene) {
      const std::array<StoredVertex, 3> vertices{
          {StoredVertex::Of({{0, 0, 0}}, {{0, 0}}, {{0, 1, 0}}),
           StoredVertex::Of({{1, 0, 0}}, {{1, 0}}, {{0, 1, 0}}),
           StoredVertex::Of({{0, 1, 0}}, {{0, 1}}, {{0, 1, 0}})}};
      const std::array<uint32_t, 3> indices{0, 1, 2};
      const Render::PieceMesh mesh{.Verts = vertices, .Indices = indices};
      const auto first = scene->PlacePiece(mesh);
      const auto second = scene->PlacePiece(mesh);
      CHECK(first && second, "two pieces become resident");
      if (!first || !second) { return Report(); }
      const std::array<Mat4, 1> row{};
      const std::array<Core::Live::PieceRows, 2> initial{
          {{.Piece = *first, .Rows = row}, {.Piece = *second, .Rows = row}}};
      CHECK(scene->SetPieceInstances(initial, error), "a complete instance batch applies");
      const std::array<Core::Live::PieceRows, 2> invalid{
          {{.Piece = *first, .Rows = {}}, {.Piece = {}, .Rows = row}}};
      CHECK(!scene->SetPieceInstances(invalid, error), "an unknown piece rejects the full batch");
      CHECK(scene->SetPieceInstances(*second, row, error),
            "a rejected batch preserves the earlier resident pieces");
      const std::array<Core::Live::PieceRows, 2> repeated{
          {{.Piece = *first, .Rows = row}, {.Piece = *first, .Rows = row}}};
      CHECK(!scene->SetPieceInstances(repeated, error), "a batch cannot assign one piece twice");
    }
  }
  SDL_Quit();
  return Report();
}
