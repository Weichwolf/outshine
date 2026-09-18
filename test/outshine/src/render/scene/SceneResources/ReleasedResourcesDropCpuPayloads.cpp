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
      Render::PieceHandle stalePiece;
      Render::HeightPageHandle stalePage;
      size_t slotBytes = 0;
      size_t pageSlotBytes = 0;
      for (int iteration = 0; iteration != 3; ++iteration) {
        const auto piece = renderer.PlacePiece(mesh);
        const auto page = renderer.PlaceHeightPage(nodes);
        CHECK(piece.has_value() && page.has_value(), "streamed resources upload");
        if (!piece || !page) { return Report(); }
        CHECK(renderer.PieceSlots() == 1,
              "one simultaneously resident piece reuses its slot across releases");
        CHECK(renderer.HeightPageSlots() == 1,
              "one simultaneously resident height page reuses its slot across releases");
        CHECK(renderer.PieceSourceBytes() >=
                      sizeof(vertices) + sizeof(indices) + sizeof(instances) &&
                  renderer.HeightPageSourceBytes() >= nodes.size() * sizeof(float),
              "diagnostics count independently sized input payloads including instance rows");
        if (iteration == 0) {
          slotBytes = renderer.PieceSlotBytes();
          pageSlotBytes = renderer.HeightPageSlotBytes();
        }
        if (iteration > 0) {
          CHECK(page->Slot == stalePage.Slot && page->Generation != stalePage.Generation &&
                    renderer.HeightPageSlotBytes() == pageSlotBytes,
                "height slot reuse changes identity without growing metadata capacity");
          CHECK(piece->Slot == stalePiece.Slot && piece->Generation != stalePiece.Generation &&
                    renderer.PieceSlotBytes() == slotBytes,
                "slot reuse changes identity without growing metadata capacity");
          const auto pieceBytes = renderer.PieceSourceBytes();
          const auto pageBytes = renderer.HeightPageSourceBytes();
          CHECK(!renderer.SetPieceInstances(stalePiece, instances, error),
                "a released piece handle cannot update its successor");
          renderer.ReleasePiece(stalePiece);
          renderer.ReleaseHeightPage(stalePage);
          CHECK(renderer.PieceSourceBytes() == pieceBytes &&
                    renderer.HeightPageSourceBytes() == pageBytes,
                "releasing old handles cannot remove successor payloads");
        }
        renderer.ReleasePiece(*piece);
        renderer.ReleaseHeightPage(*page);
        CHECK(renderer.PieceSourceBytes() == 0 && renderer.HeightPageSourceBytes() == 0,
              "release returns all piece/page payload capacity despite retaining handle records");
        renderer.ReleasePiece(*piece);
        renderer.ReleaseHeightPage(*page);
        CHECK(renderer.PieceSourceBytes() == 0 && renderer.HeightPageSourceBytes() == 0,
              "repeated release remains harmless");
        stalePiece = *piece;
        stalePage = *page;
      }
      std::unique_ptr<Core::Live> candidate;
      const bool prepared =
          Core::Live::PreparesWorldReplacement(renderer, *scene, nullptr, candidate, error);
      CHECK(prepared, "world replacement accepts the remaining live resources");
      if (prepared) {
        CHECK(renderer.PieceSourceBytes() == 0 && renderer.HeightPageSourceBytes() == 0,
              "world snapshots do not copy retired resource payloads");
        CHECK(Core::Live::PublishesPreparedWorld(renderer, scene, candidate, error),
              "replacement publishes");
        CHECK(!renderer.SetPieceInstances(stalePiece, instances, error),
              "released handles remain invalid after publication");
      }
    }
  }
  SDL_Quit();
  return Report();
}
