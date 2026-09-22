#include "RuntimeScene.h"
#include "WorldCandidate.h"
#include "SubjectCullStage.h"
#include "Check.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

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
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"sceneLinear"};
    declaration.Haze = 0;
    declaration.KeyLux = 10;
    declaration.KeyElevationDeg = 90;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    if (!Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error)) {
      Unprepared(error.c_str());
      return Report();
    }
    const auto render = [&](float roughness, bool ground, int mode) {
      const bool classified = mode == 1;
      const bool mixed = mode == 2;
      Material material;
      material.BaseColour = {{0.2f, 0.2f, 0.2f, 1}};
      material.Roughness = ground ? 1.0f : roughness;
      Geometry geometry;
      const auto surface = geometry.addSurface("ground-or-native", material);
      CHECK(surface.has_value(), "material created");
      if (!surface) { return std::vector<float>{}; }
      const auto part = geometry.addPart("plane", *surface);
      CHECK(part.has_value(), "plane created");
      if (!part) { return std::vector<float>{}; }
      CHECK(geometry.setPositions(*part,
                                  std::array<float, 12>{-1, 0, 1, 1, 0, 1, 1, 0, -1, -1, 0, -1}),
            "horizontal plane positions");
      CHECK(geometry.setNormals(*part, std::array<float, 12>{0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0}),
            "upward normals");
      CHECK(geometry.setTexture(*part, std::array<float, 8>{-1, -1, 1, -1, 1, 1, -1, 1}),
            "metric ground coordinates");
      CHECK(geometry.setTriangles(*part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}), "triangles");
      Core::WorldCandidate candidate(renderer);
      const auto prepared = candidate.Prepare(*scene, nullptr);
      CHECK(prepared.has_value(), "private world prepared");
      if (!prepared) { return std::vector<float>{}; }
      candidate.GroundIs(ground ? surface->index() : -1);
      if (!candidate.SetGeometry(std::move(geometry), 0, material, error)) {
        CHECK(false, error.c_str());
        return std::vector<float>{};
      }
      std::array<uint32_t, 13> classes{};
      if (classified) {
        classes = {2,
                   0,
                   1,
                   1,
                   std::bit_cast<uint32_t>(-10.0f),
                   std::bit_cast<uint32_t>(-10.0f),
                   std::bit_cast<uint32_t>(20.0f),
                   11,
                   0,
                   0,
                   0,
                   0,
                   0};
      }
      const std::array<float, 14> palette{std::bit_cast<float>(1u),
                                          std::bit_cast<float>(0u),
                                          10,
                                          0,
                                          0.2f,
                                          0.2f,
                                          0.2f,
                                          classified ? roughness
                                          : mixed    ? roughness + 0.1f
                                                     : 1.0f,
                                          0.2f,
                                          0.2f,
                                          0.2f,
                                          classified ? 1.0f
                                          : mixed    ? roughness - 0.1f
                                                     : roughness,
                                          90,
                                          mixed ? -5.0f : 90.0f};
      CHECK(renderer.SetGroundClasses(classes, palette, error), "ground palette uploaded");
      const auto published = candidate.Publish(scene);
      CHECK(published.has_value(), "complete fixture published");
      if (!published) { return std::vector<float>{}; }
      Render::Viewpoint eye;
      eye.EyeM = {{0, 2, 0}};
      eye.Forward = {{0, -1, 0}};
      eye.Right = {{1, 0, 0}};
      eye.Up = {{0, 0, -1}};
      eye.Kind = Render::CameraKind::Orthographic;
      eye.XMagM = eye.YMagM = 2;
      eye.ZNearM = 0.1;
      eye.ZFarM = 10;
      scene->Eye(eye);
      for (int frame = 0; frame < 3; ++frame) {
        CHECK(scene->Advance(error) && scene->Draw(error), "fixture rendered");
        CHECK((Render::SubjectCullStage::JobsSweptTaken() > 0) == (frame == 0),
              "new worlds refresh visibility; unchanged frames reuse it");
      }
      std::vector<float> pixels;
      CHECK(renderer.ReadSceneLinear(pixels) == Render::ReadState::Ready, "linear pixels read");
      return pixels;
    };
    std::array<double, 2> centre{};
    size_t caseIndex = 0;
    for (float roughness : {0.25f, 0.8f}) {
      const auto native = render(roughness, false, 0);
      for (int mode : {0, 1, 2}) {
        const auto ground = render(roughness, true, mode);
        CHECK(native.size() == 32u * 32u * 4u && ground.size() == native.size(),
              "both material paths produced complete linear frames");
        if (native.size() != 32u * 32u * 4u || ground.size() != native.size()) { continue; }
        double worst = 0;
        for (size_t y = 12; y < 20; ++y) {
          for (size_t x = 12; x < 20; ++x) {
            for (size_t channel = 0; channel < 3; ++channel) {
              const size_t at = (y * 32u + x) * 4u + channel;
              CHECK(std::isfinite(ground[at]) && std::isfinite(native[at]), "finite lighting");
              worst = std::max(worst, std::abs(static_cast<double>(ground[at] - native[at])));
            }
          }
        }
        if (worst >= 0.002) {
          const size_t middle = (16u * 32u + 16u) * 4u;
          std::printf("roughness=%g mode=%d worst=%g native=%g ground=%g\n",
                      roughness,
                      mode,
                      worst,
                      native[middle],
                      ground[middle]);
        }
        CHECK(worst < 0.002, "ground palette and native material agree in linear light");
      }
      if (native.size() == 32u * 32u * 4u) { centre[caseIndex] = native[(16u * 32u + 16u) * 4u]; }
      ++caseIndex;
    }
    CHECK(std::abs(centre[0] - centre[1]) > 0.01,
          "lighting distinguishes roughness, so a constant factor cannot pass");
  }
  SDL_Quit();
  return Report();
}
