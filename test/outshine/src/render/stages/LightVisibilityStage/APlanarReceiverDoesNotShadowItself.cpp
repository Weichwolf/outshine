#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {
bool AddOccluder(outshine::Render::SceneRenderer &renderer, std::string &error) {
  using namespace outshine;
  const std::array<Vec3f, 8> positions{{{{-4, 4, -4}},
                                        {{4, 4, -4}},
                                        {{4, 12, -4}},
                                        {{-4, 12, -4}},
                                        {{-4, 4, 4}},
                                        {{4, 4, 4}},
                                        {{4, 12, 4}},
                                        {{-4, 12, 4}}}};
  std::array<StoredVertex, 8> vertices{};
  for (size_t at = 0; at < vertices.size(); ++at) {
    vertices[at] = StoredVertex::Of(positions[at], {{0, 0}}, {{0, 1, 0}});
  }
  constexpr std::array<uint32_t, 36> indices{0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4,
                                             3, 7, 6, 3, 6, 2, 0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5};
  return renderer.PlacePiece({.Verts = vertices, .Indices = indices}, error) != Render::kNoPiece;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for independent receiver shading");
  {
    Geometry geometry;
    Material material;
    material.BaseColour = {{0.5f, 0.5f, 0.5f, 1}};
    material.Roughness = 1;
    const auto surface = geometry.addSurface("matte-plane", material);
    CHECK(surface.has_value(), "matte receiver material exists");
    if (!surface) { return Report(); }
    const auto part = geometry.addPart("unoccluded-plane", *surface);
    CHECK(part.has_value(), "analytical receiver exists");
    if (!part) { return Report(); }
    const std::array<float, 12> vertices{-64, -16, -64, 64, 0, -64, 64, 16, 64, -64, 0, 64};
    const float normalY = std::sqrt(32.0f / 33.0f);
    const float normalXZ = -0.125f * normalY;
    std::array<float, 12> normals{};
    for (size_t at = 0; at < normals.size(); ++at) {
      normals[at] = at % 3 == 1 ? normalY : normalXZ;
    }
    const bool valid = geometry.setPositions(*part, vertices) &&
                       geometry.setNormals(*part, normals) &&
                       geometry.setTriangles(*part, std::array<uint32_t, 6>{0, 2, 1, 0, 3, 2});
    CHECK(valid, "one planar sheet has a constant normal and no independent occluder");
    if (!valid) { return Report(); }
    Core::Declaration declaration;
    declaration.InitialGeometry = &geometry;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 128;
    declaration.Outputs = {"surface", "sceneLinear", "shadowAtlas"};
    declaration.DrawsSky = true;
    declaration.KeyLux = 10000;
    declaration.KeyElevationDeg = 45;
    declaration.ShadowRadiusM = 240000;
    SceneRenderer renderer;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
          "a receiver in a full-radius world opens");
    if (!scene) { return Report(); }
    auto eye = Viewpoint::LookAt({.EyeM = {{100, 100, 50}}, .AimM = {{0, 0, 0}}}, 0.0);
    CHECK(eye.has_value(), "receiver camera has an orthonormal basis");
    if (!eye) { return Report(); }
    eye->Kind = CameraKind::Orthographic;
    eye->XMagM = eye->YMagM = 30;
    eye->ZNearM = 1;
    eye->ZFarM = 1000;
    scene->Eye(*eye);
    renderer.SetShadowFrame({{0, 0.70710677f, 0.70710677f}}, {{0, 1, 0}}, 240000, true);
    renderer.CastsBelow(0);
    CHECK(scene->Draw(error), "receiver draws with no caster");
    renderer.WaitForGpu();
    std::vector<float> unoccluded;
    CHECK(renderer.ReadSceneLinear(unoccluded) == ReadState::Ready,
          "unoccluded radiance supplies the independent shading control");
    renderer.CastsBelow(1);
    CHECK(scene->Draw(error), "the same sheet also writes its analytical caster depth");
    renderer.WaitForGpu();
    std::vector<float> atlas;
    CHECK(renderer.ReadShadowAtlas(atlas) == ReadState::Ready &&
              std::ranges::any_of(atlas, [](float depth) { return depth > 0; }),
          "the receiver genuinely participates in the GPU shadow pass");
    std::vector<float> receiving;
    CHECK(renderer.ReadSceneLinear(receiving) == ReadState::Ready &&
              receiving.size() == unoccluded.size() && receiving.size() == 128u * 128u * 4u,
          "receiver and control have equal full-resolution linear buffers");
    if (receiving.size() == unoccluded.size() && receiving.size() == 128u * 128u * 4u) {
      double worstDrop = 0;
      double meanRadiance = 0;
      for (size_t y = 32; y < 96; ++y) {
        for (size_t x = 32; x < 96; ++x) {
          for (size_t channel = 0; channel < 3; ++channel) {
            const size_t at = (y * 128 + x) * 4 + channel;
            meanRadiance += unoccluded[at];
            worstDrop = std::max(worstDrop,
                                 (double(unoccluded[at]) - receiving[at]) /
                                     std::max(0.01, double(unoccluded[at])));
          }
        }
      }
      meanRadiance /= 64 * 64 * 3;
      std::printf(
          "PLANAR_RECEIVER worst_relative_drop=%.9f mean_radiance=%.9f\n", worstDrop, meanRadiance);
      CHECK(meanRadiance > 1 && worstDrop < 0.01,
            "a sunlit plane has no self-shadow darkening beyond one percent raster tolerance");
    }
    CHECK(AddOccluder(renderer, error) && scene->Draw(error), "a separate opaque box casts");
    renderer.WaitForGpu();
    CHECK(renderer.ReadSceneLinear(receiving) == ReadState::Ready &&
              receiving.size() == unoccluded.size(),
          "the genuinely shadowed receiver reads");
    if (receiving.size() == 128u * 128u * 4u && receiving.size() == unoccluded.size()) {
      const Vec3 delta = Vec3{{0, -8.0 / 7.0, -64.0 / 7.0}} - eye->EyeM;
      double screenX = 0;
      double screenY = 0;
      for (size_t axis = 0; axis < 3; ++axis) {
        screenX += eye->Right[axis] * delta[axis];
        screenY += eye->Up[axis] * delta[axis];
      }
      const auto x = static_cast<size_t>(64 + 64 * screenX / 30);
      const auto y = static_cast<size_t>(64 - 64 * screenY / 30);
      const size_t at = (y * 128 + x) * 4;
      CHECK(receiving[at] < 0.5f * unoccluded[at],
            "the box's analytical shadow remains dark with the same rounding allowance");
    }
  }
  SDL_Quit();
  return Report();
}
