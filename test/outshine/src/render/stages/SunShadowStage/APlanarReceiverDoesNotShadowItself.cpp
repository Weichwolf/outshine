#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <print>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {
constexpr int kFrameSidePx = 128;
constexpr size_t kColourComponents = static_cast<size_t>(kFrameSidePx) * kFrameSidePx * 4;
constexpr double kShadowRadiusM = 240000;
constexpr double kKeyLux = 10000;
constexpr double kKeyElevationDeg = 45;
constexpr outshine::Vec3 kInitialEyeM{{100, 100, 50}};
constexpr outshine::Vec3f kToSun{{0, 0.70710677f, 0.70710677f}};
constexpr double kOrthoHalfSpanM = 30;
constexpr double kDetailHalfSpanM = 10;
constexpr double kFarPlaneM = 1000;
constexpr double kReceiverSlope = 0.125;
constexpr double kBoxCentreHeightM = 8;
constexpr double kShadowEdgeXM = 3.75;
constexpr size_t kSampleFirst = 32;
constexpr size_t kSampleLimit = 96;
constexpr size_t kSampleComponents =
    (kSampleLimit - kSampleFirst) * (kSampleLimit - kSampleFirst) * 3;
constexpr double kMinimumControlRadiance = 0.01;
constexpr double kSelfShadowRelativeTolerance = 0.01;

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
  Render::PieceMesh piece;
  piece.Verts = vertices;
  piece.Indices = indices;
  return renderer.PlacePiece(piece, error) != Render::kNoPiece;
}

std::optional<size_t> ShadowPixel(const outshine::Render::SceneRenderer &renderer,
                                  const outshine::Render::Viewpoint &eye,
                                  double worldX = 0) {
  const double travel = (kBoxCentreHeightM - kReceiverSlope * worldX) / (1 - kReceiverSlope);
  const outshine::Vec3 point{{worldX, kBoxCentreHeightM - travel, -travel}};
  const outshine::Vec3 delta = point - eye.EyeM;
  double screenX = 0;
  double screenY = 0;
  for (size_t axis = 0; axis < 3; ++axis) {
    screenX += eye.Right[axis] * delta[axis];
    screenY += eye.Up[axis] * delta[axis];
  }
  const int width = renderer.SceneW();
  const int height = renderer.SceneH();
  const double x = width * 0.5 * (1 + screenX / eye.XMagM);
  const double y = height * 0.5 * (1 - screenY / eye.YMagM);
  if (!std::isfinite(x) || !std::isfinite(y) || x < 0 || x >= width || y < 0 || y >= height) {
    return std::nullopt;
  }
  return (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
}

void CheckSelfShadow(const std::vector<float> &receiving, const std::vector<float> &unoccluded) {
  using namespace outshine::Test;
  double worstDrop = 0;
  double meanRadiance = 0;
  for (size_t y = kSampleFirst; y < kSampleLimit; ++y) {
    for (size_t x = kSampleFirst; x < kSampleLimit; ++x) {
      for (size_t channel = 0; channel < 3; ++channel) {
        const size_t at = (y * kFrameSidePx + x) * 4 + channel;
        meanRadiance += unoccluded[at];
        worstDrop =
            std::max(worstDrop,
                     (static_cast<double>(unoccluded[at]) - receiving[at]) /
                         std::max(kMinimumControlRadiance, static_cast<double>(unoccluded[at])));
      }
    }
  }
  meanRadiance /= kSampleComponents;
  std::println(
      "PLANAR_RECEIVER worst_relative_drop={:.9f} mean_radiance={:.9f}", worstDrop, meanRadiance);
  CHECK(meanRadiance > 1 && worstDrop < kSelfShadowRelativeTolerance,
        "a sunlit plane has no self-shadow darkening beyond one percent raster tolerance");
}

void TranslatedReceiver(outshine::Core::RuntimeScene &scene,
                        outshine::Render::SceneRenderer &renderer,
                        outshine::Render::Viewpoint eye,
                        float unoccludedRadiance) {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  std::vector<float> incremental;
  std::vector<float> reference;
  std::string error;
  bool shifted = false;
  eye.XMagM = eye.YMagM = kDetailHalfSpanM;
  for (const Vec3 &delta :
       std::array<Vec3, 3>{{{{0.5, 0.5, 0}}, {{16, 16, 0}}, {{-16.5, -16.5, 0}}}}) {
    eye.EyeM = eye.EyeM + delta;
    scene.Eye(eye);
    CHECK(scene.Draw(error), "translated receiver submits with incremental shadows");
    renderer.WaitForGpu();
    const auto &region = renderer.ShadowRegions()[0];
    shifted = shifted || region.Depth[2] != 0;
    std::println("RING_RECEIVER origin_x={} origin_y={}", region.Depth[2], region.Depth[3]);
    CHECK(renderer.ReadSceneLinear(incremental) == ReadState::Ready,
          "translated receiver radiance is readable");
    for (const double worldX : std::array<double, 2>{-kShadowEdgeXM, kShadowEdgeXM}) {
      const auto at = ShadowPixel(renderer, eye, worldX);
      CHECK(at && *at < incremental.size() && incremental[*at] < 0.5f * unoccludedRadiance,
            "the analytical box shadow stays dark at its fixed world point after translation");
    }
    renderer.SetShadowFrame({{1, 0, 0}}, {{0, 1, 0}}, kShadowRadiusM, true, 0);
    renderer.SetShadowFrame(kToSun, {{0, 1, 0}}, kShadowRadiusM, true, 0);
    CHECK(scene.Draw(error), "current receiver submits with a forced complete shadow rebuild");
    renderer.WaitForGpu();
    CHECK(renderer.ReadSceneLinear(reference) == ReadState::Ready && reference == incremental,
          "incremental receiver radiance exactly matches the complete current reference");
  }
  CHECK(shifted, "the real fragment consumer exercises a nonzero physical ring origin");
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
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = kFrameSidePx;
    declaration.Outputs = {"surface", "sceneLinear", "shadowAtlas"};
    declaration.DrawsSky = true;
    declaration.KeyLux = kKeyLux;
    declaration.KeyElevationDeg = kKeyElevationDeg;
    declaration.ShadowRadiusM = kShadowRadiusM;
    SceneRenderer renderer;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
          "a receiver in a full-radius world opens");
    if (!scene) { return Report(); }
    auto eye = Viewpoint::LookAt({.EyeM = kInitialEyeM, .AimM = {{0, 0, 0}}}, 0.0);
    CHECK(eye.has_value(), "receiver camera has an orthonormal basis");
    if (!eye) { return Report(); }
    eye->Kind = CameraKind::Orthographic;
    eye->XMagM = eye->YMagM = kOrthoHalfSpanM;
    eye->ZNearM = 1;
    eye->ZFarM = kFarPlaneM;
    scene->Eye(*eye);
    renderer.SetShadowFrame(kToSun, {{0, 1, 0}}, kShadowRadiusM, true, 0);
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
              receiving.size() == unoccluded.size() && receiving.size() == kColourComponents,
          "receiver and control have equal full-resolution linear buffers");
    if (receiving.size() == unoccluded.size() && receiving.size() == kColourComponents) {
      CheckSelfShadow(receiving, unoccluded);
    }
    CHECK(AddOccluder(renderer, error) && scene->Draw(error), "a separate opaque box casts");
    renderer.WaitForGpu();
    CHECK(renderer.ReadSceneLinear(receiving) == ReadState::Ready &&
              receiving.size() == unoccluded.size(),
          "the genuinely shadowed receiver reads");
    if (receiving.size() == kColourComponents && receiving.size() == unoccluded.size()) {
      const auto at = ShadowPixel(renderer, *eye);
      CHECK(at && *at < receiving.size() && receiving[*at] < 0.5f * unoccluded[*at],
            "the box's analytical shadow remains dark with the same rounding allowance");
      if (at && *at < unoccluded.size()) {
        TranslatedReceiver(*scene, renderer, *eye, unoccluded[*at]);
      }
    }
  }
  SDL_Quit();
  return Report();
}
