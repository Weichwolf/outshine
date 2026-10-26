#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <numbers>
#include <string>
#include <vector>
#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr int kExtent = 64;
constexpr std::array<Vec2f, 3> kCorners{{{{10.2f, 9.3f}}, {{53.4f, 17.1f}}, {{18.7f, 54.6f}}}};

float Side(Vec2f a, Vec2f b, float x, float y) {
  return (b[0] - a[0]) * (y - a[1]) - (b[1] - a[1]) * (x - a[0]);
}

float Coverage(int x, int y) {
  int inside = 0;
  constexpr int kSamples = 32;
  for (int row = 0; row < kSamples; ++row) {
    for (int col = 0; col < kSamples; ++col) {
      const float u = static_cast<float>(x) + (static_cast<float>(col) + 0.5f) / kSamples;
      const float v = static_cast<float>(y) + (static_cast<float>(row) + 0.5f) / kSamples;
      inside += Side(kCorners[0], kCorners[1], u, v) >= 0 &&
                Side(kCorners[1], kCorners[2], u, v) >= 0 &&
                Side(kCorners[2], kCorners[0], u, v) >= 0;
    }
  }
  return static_cast<float>(inside) / (kSamples * kSamples);
}

std::vector<float> RenderTriangle(bool temporal, double panPxPerFrame = 0) {
  Geometry geometry;
  Material material;
  material.Unlit = material.DoubleSided = true;
  material.BaseColour = {{1, 1, 1, 1}};
  const int part =
      geometry.addPart("triangle", geometry.addSurface("white", material).value()).value();
  std::array<float, 9> positions{};
  for (size_t i = 0; i < kCorners.size(); ++i) {
    positions[3 * i] = (kCorners[i][0] - 32.0f) / 16.0f;
    positions[3 * i + 1] = (32.0f - kCorners[i][1]) / 16.0f;
    positions[3 * i + 2] = -2.0f;
  }
  CHECK(geometry.setPositions(part, positions),
        "the coverage triangle has known screen coordinates");
  CHECK(geometry.setTriangles(part, std::array<uint32_t, 3>{0, 1, 2}),
        "the coverage face is declared");
  SceneRenderer renderer;
  Core::Declaration declaration;
  declaration.InitialGeometry = &geometry;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = kExtent;
  declaration.Outputs = {"surface", "sceneLinear"};
  declaration.Antialiasing = temporal ? "temporal" : "none";
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
        "the coverage scene opens");
  if (!scene) {
    std::printf("setup: %s\n", error.c_str());
    return {};
  }
  Viewpoint eye;
  eye.YfovRad = std::numbers::pi / 2;
  eye.ZNearM = 0.1;
  eye.ZFarM = 100;
  const int frames = temporal ? 60 : 1;
  for (int frame = 0; frame < frames; ++frame) {
    eye.EyeM[0] = static_cast<double>(frame - frames + 1) * panPxPerFrame / 16.0;
    scene->Eye(eye);
    CHECK(scene->Draw(error), "a frame accumulates at the declared camera pose");
  }
  std::vector<float> pixels;
  CHECK(renderer.ReadSceneLinear(pixels) == ReadState::Ready,
        "linear coverage reaches the readback");
  return pixels;
}

}

int main() {
  CHECK(SDL_SetHint(SDL_HINT_ASSERT, "abort"), "assertions fail without a dialog");
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video initializes");
  const auto baseline = RenderTriangle(false);
  for (const double pan : {0.0, 0.25}) {
    const auto temporal = RenderTriangle(true, pan);
    const size_t count = kExtent * kExtent * 4u;
    CHECK(baseline.size() == count && temporal.size() == count, "both frames contain every pixel");
    if (baseline.size() == count && temporal.size() == count) {
      double baselineError = 0, temporalError = 0, temporalAlphaError = 0;
      size_t fractionalAlpha = 0;
      for (int y = 0; y < kExtent; ++y) {
        for (int x = 0; x < kExtent; ++x) {
          const size_t at = (static_cast<size_t>(y) * kExtent + static_cast<size_t>(x)) * 4u;
          const double expected = Coverage(x, y);
          baselineError += std::pow(baseline[at] - expected, 2);
          temporalError += std::pow(temporal[at] - expected, 2);
          temporalAlphaError += std::pow(temporal[at + 3] - expected, 2);
          fractionalAlpha += temporal[at + 3] > 0.001f && temporal[at + 3] < 0.999f;
        }
      }
      std::printf("coverage squared error: pan %.2f px/frame, single sample %.6f, temporal %.6f\n",
                  pan,
                  baselineError,
                  temporalError);
      CHECK(baselineError > 1, "the analytical silhouette exposes single-sample aliasing");
      CHECK(fractionalAlpha > 0, "coverage alpha accumulates actual subpixel samples");
      CHECK(temporalAlphaError < baselineError * 0.6,
            "alpha retains the independently integrated silhouette");
      CHECK(temporalError < baselineError * 0.6,
            "TAA reduces coverage error by at least forty percent at the declared camera motion");
    }
  }
  SDL_Quit();
  return Report();
}
