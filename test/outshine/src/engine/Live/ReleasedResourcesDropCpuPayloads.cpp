#include "Live.h"
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
    const auto surface = base.addSurface("wall", Material{});
    CHECK(surface.has_value(), "fixture material exists");
    if (!surface) { return Report(); }
    const auto part = base.addPart("base", *surface);
    CHECK(part && base.setPositions(*part, std::array<float, 9>{0, 0, 0, 1, 0, 0, 0, 1, 0}) &&
              base.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
          "base geometry is complete");
    Core::Declaration declaration;
    declaration.InitialGeometry = &base;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    Render::SceneRenderer renderer;
    std::unique_ptr<Core::Live> scene;
    std::string error;
    CHECK(Core::Live::Open(renderer, declaration, nullptr, scene, error), "world opens");
    if (scene) {
      const std::array<StoredVertex, 3> vertices{
          StoredVertex::Of({{0, 0, 0}}, {{0, 0}}, {{0, 0, 1}}),
          StoredVertex::Of({{1, 0, 0}}, {{1, 0}}, {{0, 0, 1}}),
          StoredVertex::Of({{0, 1, 0}}, {{0, 1}}, {{0, 0, 1}})};
      const std::array<uint32_t, 3> indices{0, 1, 2};
      const std::array<Mat4, 2> instances{};
      Render::PieceMesh mesh;
      mesh.Verts = vertices;
      mesh.Indices = indices;
      mesh.Instances = instances;
      mesh.MaxInstances = instances.size();
      std::vector<float> nodes(Render::GroundLattice::kPageNodes, 3.0f);
      Render::PieceId stalePiece = Render::kNoPiece;
      Render::PageId stalePage = Render::kNoPage;
      for (int iteration = 0; iteration != 3; ++iteration) {
        const auto piece = scene->PlacePiece(mesh, error);
        const auto page = scene->PlaceHeightPage(nodes, error);
        CHECK(piece != Render::kNoPiece && page != Render::kNoPage, "streamed resources upload");
        CHECK(scene->PieceSourceBytes() >= sizeof(vertices) + sizeof(indices) + sizeof(instances) &&
                  scene->HeightPageSourceBytes() >= nodes.size() * sizeof(float),
              "diagnostics count independently sized input payloads including instance rows");
        if (iteration > 0) {
          const auto pieceBytes = scene->PieceSourceBytes();
          const auto pageBytes = scene->HeightPageSourceBytes();
          CHECK(!scene->SetPieceInstances(stalePiece, instances, error),
                "a released piece handle cannot update its successor");
          scene->ReleasePiece(stalePiece);
          scene->ReleaseHeightPage(stalePage);
          CHECK(scene->PieceSourceBytes() == pieceBytes &&
                    scene->HeightPageSourceBytes() == pageBytes,
                "releasing old handles cannot remove successor payloads");
        }
        scene->ReleasePiece(piece);
        scene->ReleaseHeightPage(page);
        CHECK(scene->PieceSourceBytes() == 0 && scene->HeightPageSourceBytes() == 0,
              "release returns all piece/page payload capacity despite retaining handle records");
        scene->ReleasePiece(piece);
        scene->ReleaseHeightPage(page);
        CHECK(scene->PieceSourceBytes() == 0 && scene->HeightPageSourceBytes() == 0,
              "repeated release remains harmless");
        stalePiece = piece;
        stalePage = page;
      }
      std::unique_ptr<Core::Live> candidate;
      const bool prepared =
          Core::Live::PreparesWorldReplacement(renderer, *scene, nullptr, candidate, error);
      CHECK(prepared, "world replacement accepts the remaining live resources");
      if (prepared) {
        CHECK(candidate->PieceSourceBytes() == 0 && candidate->HeightPageSourceBytes() == 0,
              "world snapshots do not copy retired resource payloads");
        CHECK(Core::Live::PublishesPreparedWorld(renderer, scene, candidate, error),
              "replacement publishes");
        CHECK(!scene->SetPieceInstances(stalePiece, instances, error),
              "released handles remain invalid after publication");
      }
    }
  }
  SDL_Quit();
  return Report();
}
