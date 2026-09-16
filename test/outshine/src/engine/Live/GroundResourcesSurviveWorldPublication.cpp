#include "Live.h"
#include "SceneRenderer.h"
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
    Geometry geometry;
    const int part = geometry.addPart("triangle", MaterialInstance{}).value();
    CHECK(geometry.setPositions(part, std::array<float, 9>{-1, -1, 0, 1, -1, 0, 0, 1, 0}) &&
              geometry.setTriangles(part, std::array<uint32_t, 3>{0, 1, 2}),
          "replacement geometry is valid");
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    Render::SceneRenderer renderer;
    std::unique_ptr<Core::Live> scene;
    std::string error;
    CHECK(Core::Live::Open(renderer, declaration, nullptr, scene, error), "ground fixture opens");
    if (scene) {
      std::vector<float> nodes(Render::GroundLattice::kPageNodes, 3.0f);
      const Render::PageId page = scene->PlaceHeightPage(nodes, error);
      CHECK(page != Render::kNoPage, "a stable height-page handle is placed");
      std::vector<float> fractions(Render::GroundLattice::kSide);
      for (size_t at = 0; at < fractions.size(); ++at) {
        fractions[at] = static_cast<float>(at) / static_cast<float>(fractions.size() - 1u);
      }
      Render::GroundTile tile;
      tile.Instance.Corners = {{-1, -1, 1, -1, -1, 1, 1, 1}};
      tile.Instance.Page = static_cast<float>(page);
      tile.LowM = 3.0f;
      tile.HighM = 3.0f;
      CHECK(scene->SetGroundGrid(fractions, error) &&
                scene->SetGroundLattice({&tile, 1}, {}, error),
            "ground inputs retain stable page handles");
      CHECK(renderer.GroundLatticeTriangles() == Render::GroundLattice::kIndices / 3u,
            "one published ground tile has its full topology");
      std::unique_ptr<Core::Live> candidate;
      CHECK(Core::Live::PreparesWorldReplacement(renderer, *scene, nullptr, candidate, error) &&
                Core::Live::PublishesPreparedWorld(renderer, scene, candidate, error),
            "an empty declared world publishes its first streamed-ground candidate");
      CHECK(renderer.GroundLatticeTriangles() == Render::GroundLattice::kIndices / 3u,
            "first streamed-world publication retains the ground tile topology");
      CHECK(Core::Live::ReplacesGeometry(renderer, *scene, geometry.clone(), nullptr, scene, error),
            "geometry replacement recreates ground resources in its candidate");
      CHECK(renderer.GroundLatticeTriangles() == Render::GroundLattice::kIndices / 3u,
            "candidate publication retains the ground tile topology");
      scene->ReleaseHeightPage(page);
      CHECK(!scene->SetGroundLattice({&tile, 1}, {}, error),
            "released stable page handles cannot address a later resident page");
    }
  }
  SDL_Quit();
  return Report();
}
