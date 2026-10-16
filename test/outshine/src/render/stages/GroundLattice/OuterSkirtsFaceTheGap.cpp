#include "Check.h"
#include "GroundLattice.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    constexpr float high = 0.6f, low = 0.2f, eyeEast = 2, nearM = 0.1f;
    SceneRenderer renderer;
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"sceneLinear", "sceneDepth"};
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
          "terrain gap scene opens");
    if (scene) {
      SubjectMaterial material;
      material.Domain = SurfaceDomain::Ground;
      CHECK(renderer.SetSubjectMaterials(std::array{material}, error), "ground material installs");
      std::array<uint32_t, 3> classes{0, 0, ~0u};
      std::array<float, 30> palette{};
      palette[0] = std::bit_cast<float>(1u);
      palette[2] = 1;
      palette[12] = palette[13] = 90;
      CHECK(renderer.SetGroundClasses(classes, palette, error), "ground palette installs");
      std::vector<float> nodes(GroundLattice::kPageNodes, high);
      const auto highPage = renderer.PlaceHeightPage(nodes, error);
      std::ranges::fill(nodes, low);
      const auto lowPage = renderer.PlaceHeightPage(nodes, error);
      CHECK(highPage != kNoPage && lowPage != kNoPage, "neighboring height pages upload");
      const auto tile = [](PageId page, float west, float east, float height) {
        GroundTile result;
        result.Instance.Page = static_cast<float>(page);
        result.Instance.Corners = {west, 0.5f, east, 0.5f, west, -0.5f, east, -0.5f};
        result.Instance.StepE = result.Instance.StepN = 1.0f / (GroundLattice::kSide - 1);
        result.LowM = height - 0.5f;
        result.HighM = height;
        return result;
      };
      const std::array real{tile(highPage, -1, 0, high), tile(lowPage, 0, 1, low)};
      CHECK(renderer.SetGroundLattice(real, {}, error), "neighboring tiles install");
      Viewpoint eye;
      eye.EyeM = {{eyeEast, 0, 0.4}};
      eye.Forward = {{-1, 0, 0}};
      eye.Right = {{0, 1, 0}};
      eye.Up = {{0, 0, 1}};
      eye.YfovRad = 1;
      eye.ZNearM = nearM;
      scene->Eye(eye);
      CHECK(scene->Draw(error), "terrain gap renders");
      renderer.WaitForGpu();
      std::vector<float> depth;
      CHECK(renderer.ReadDepth(depth) == ReadState::Ready && depth.size() == 32u * 32u,
            "terrain gap depth is available");
      if (depth.size() == 32u * 32u) {
        CHECK_NEAR(depth[16u * 32u + 16u],
                   nearM / eyeEast,
                   1e-6f,
                   "reverse depth",
                   "the near outer wall closes the terrain gap");
      }
    }
  }
  SDL_Quit();
  return Report();
}
