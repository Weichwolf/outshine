#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "WaterSurfaceBuilder.h"
#include "Geometry.h"
#include "Material.h"

#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <cstddef>
#include <print>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {
void SetGeometryPlanes(outshine::Geometry &geometry,
                       outshine::MaterialInstance bed,
                       outshine::MaterialInstance water) {
  using namespace outshine::Test;
  for (int layer = 0; layer < 2; ++layer) {
    const auto part =
        geometry.addPart(layer == 0 ? "background" : "interface", layer == 0 ? bed : water).value();
    const float z = layer == 0 ? -0.5f : 0.0f;
    CHECK(geometry.setPositions(part,
                                std::array<float, 12>{-1, -1, z, 1, -1, z, 1, 1, z, -1, 1, z}) &&
              geometry.setTriangles(part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}),
          "background and interface overlap in a controlled viewport");
  }
}

void CheckWork(const outshine::Render::SceneRenderer &renderer) {
  using namespace outshine::Render;
  using namespace outshine::Test;
  for (const auto stage : {Stage::OpaqueGeometry, Stage::TransmissiveGeometry}) {
    const auto &work = renderer.Spent(stage);
    CHECK(work.Draws == 1, "each material pass records only its own draw request");
    CHECK(work.DirectTriangles + work.IndirectInputTriangles == 2,
          "each pass references only its own quad, without terrain");
  }
}

void CheckEnergy(const std::array<outshine::Vec3, 2> &samples,
                 const outshine::Vec4f &background,
                 float metalness,
                 float cosine) {
  using namespace outshine::Test;
  constexpr double kTolerance = 0.002;
  const double ratio = (1.333 - 1.0) / (1.333 + 1.0);
  const double f0 = ratio * ratio;
  const double fresnel = f0 + (1.0 - f0) * std::pow(1.0 - cosine, 5);
  for (size_t channel = 0; channel < 3; ++channel) {
    const double measured = samples[1][channel] - samples[0][channel];
    const double expected = background[channel] * (1.0 - metalness) * (1.0 - fresnel);
    if (std::abs(measured - expected) >= kTolerance) {
      std::println("cosine={:g} metalness={:g} channel={} measured={:g} expected={:g}",
                   cosine,
                   metalness,
                   channel,
                   measured,
                   expected);
    }
    CHECK(std::isfinite(measured) && std::abs(measured - expected) < kTolerance,
          "background contribution retains only unreflected dielectric energy");
  }
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared(SDL_GetError());
    return Report();
  }
  {
    Render::SceneRenderer renderer;
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 16;
    declaration.Outputs = {"sceneLinear"};
    declaration.IndirectLight = {1, 1, 1};
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), error.c_str());
    if (!scene) { return Report(); }
    Geometry geometry;
    Material back;
    back.Unlit = true;
    const auto bed = geometry.addSurface("background", back).value();
    const auto water = geometry.addSurface("interface", Generators::WaterSurfaceMaterial()).value();
    SetGeometryPlanes(geometry, bed, water);
    Render::Viewpoint eye;
    eye.EyeM = {{0, 0, 2}};
    eye.Forward = {{0, 0, -1}};
    eye.Right = {{1, 0, 0}};
    eye.Up = {{0, 1, 0}};
    eye.Kind = Render::CameraKind::Orthographic;
    eye.XMagM = eye.YMagM = 2;
    constexpr double kNearM = 0.1;
    eye.ZNearM = kNearM;
    eye.ZFarM = 10;
    const Vec4f background = {{0.25f, 0.5f, 0.75f, 1}};
    for (const float cosine : {1.0f, 0.2f}) {
      const float slope = std::sqrt(1.0f - cosine * cosine);
      CHECK(geometry.setNormals(
                1,
                std::array<float, 12>{
                    slope, 0, cosine, slope, 0, cosine, slope, 0, cosine, slope, 0, cosine}),
            "analytic shading normal sets the interface incidence angle");
      for (const float metalness : {0.0f, 0.5f, 1.0f}) {
        std::array<Vec3, 2> samples{};
        for (size_t sample = 0; sample < samples.size(); ++sample) {
          back.BaseColour = sample == 0 ? Vec4f{{0, 0, 0, 1}} : background;
          Material interface = Generators::WaterSurfaceMaterial();
          interface.Metalness = metalness;
          CHECK(geometry.setSurface(bed, back) && geometry.setSurface(water, interface),
                "background changes without changing the interface lighting");
          CHECK(scene->SetGeometry(geometry.clone(), 2, back, error), error.c_str());
          scene->Eye(eye);
          CHECK(scene->Advance(error) && scene->Draw(error), error.c_str());
          CheckWork(renderer);
          std::vector<float> pixels;
          CHECK(renderer.ReadSceneLinear(pixels) == Render::ReadState::Ready &&
                    pixels.size() == size_t{16} * 16 * 4,
                "linear interface image is complete");
          if (pixels.size() != size_t{16} * 16 * 4) { return Report(); }
          for (size_t channel = 0; channel < 3; ++channel) {
            samples[sample][channel] = pixels[(size_t{8} * 16 + 8) * 4 + channel];
          }
        }
        CheckEnergy(samples, background, metalness, cosine);
      }
    }
  }
  SDL_Quit();
  return Report();
}
