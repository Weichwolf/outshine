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
    Core::Declaration declaration;
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
      const Render::PieceId first = scene->PlacePiece(mesh, error);
      const Render::PieceId second = scene->PlacePiece(mesh, error);
      CHECK(first != Render::kNoPiece && second != Render::kNoPiece, "two pieces become resident");
      const std::array<Mat4, 1> row{};
      const std::array<Core::Live::PieceRows, 2> initial{
          {{.Piece = first, .Rows = row}, {.Piece = second, .Rows = row}}};
      CHECK(scene->SetPieceInstances(initial, error), "a complete instance batch applies");
      const std::array<Core::Live::PieceRows, 2> invalid{
          {{.Piece = first, .Rows = {}}, {.Piece = Render::kNoPiece, .Rows = row}}};
      CHECK(!scene->SetPieceInstances(invalid, error), "an unknown piece rejects the full batch");
      CHECK(scene->SetPieceInstances(second, row, error),
            "a rejected batch preserves the earlier resident pieces");
      const std::array<Core::Live::PieceRows, 2> repeated{
          {{.Piece = first, .Rows = row}, {.Piece = first, .Rows = row}}};
      CHECK(!scene->SetPieceInstances(repeated, error), "a batch cannot assign one piece twice");
    }
  }
  SDL_Quit();
  return Report();
}
