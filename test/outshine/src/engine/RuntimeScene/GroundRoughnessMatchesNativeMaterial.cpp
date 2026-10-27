#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "WorldCandidate.h"
#include "GeometryCullStage.h"
#include "Check.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <print>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <cstddef>
#include <cstdint>
#include <utility>
#include <span>

namespace {
using namespace outshine;
using namespace outshine::Test;

enum class PaletteMode { Fallback = 0, Classified = 1, Mixed = 2 };

void UploadPalette(Render::SceneRenderer &renderer,
                   float roughness,
                   PaletteMode mode,
                   std::string &error) {
  const bool classified = mode == PaletteMode::Classified;
  const bool mixed = mode == PaletteMode::Mixed;
  float mappedRoughness = 1.0f;
  float fallbackRoughness = roughness;
  if (classified) {
    mappedRoughness = roughness;
    fallbackRoughness = 1.0f;
  } else if (mixed) {
    mappedRoughness = roughness + 0.1f;
    fallbackRoughness = roughness - 0.1f;
  }
  constexpr auto kUnmapped = std::numeric_limits<uint32_t>::max();
  std::array<uint32_t, 18> classes{};
  classes[2] = kUnmapped;
  if (classified) {
    classes = {4,
               0,
               kUnmapped,
               0,
               1,
               1,
               std::bit_cast<uint32_t>(-10.0f),
               std::bit_cast<uint32_t>(-10.0f),
               std::bit_cast<uint32_t>(20.0f),
               16,
               0,
               0,
               0,
               0,
               0,
               0,
               0,
               0};
  }
  const std::array<float, 30> palette{std::bit_cast<float>(1u),
                                      std::bit_cast<float>(0u),
                                      10,
                                      0,
                                      0.2f,
                                      0.2f,
                                      0.2f,
                                      mappedRoughness,
                                      0.2f,
                                      0.2f,
                                      0.2f,
                                      fallbackRoughness,
                                      90,
                                      mixed ? -5.0f : 90.0f};
  CHECK(renderer.SetGroundClasses(classes, palette, error), "ground palette uploaded");
}

double CentreDifference(std::span<const float> native, std::span<const float> ground) {
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
  return worst;
}

struct MaterialFixture {
  Render::SceneRenderer renderer;
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;

  bool Open() {
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"sceneLinear"};
    declaration.Haze = 0;
    declaration.KeyLux = 10;
    declaration.KeyElevationDeg = 90;
    return Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error);
  }

  std::vector<float>
  RenderMaterial(float roughness, bool ground, PaletteMode mode, bool paletteFirst = false) {
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
    CHECK(
        geometry.setPositions(*part, std::array<float, 12>{-1, 0, 1, 1, 0, 1, 1, 0, -1, -1, 0, -1}),
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
    if (paletteFirst) {
      UploadPalette(renderer, roughness, mode, error);
      Geometry intermediate = geometry.clone();
      Material clear;
      clear.Transmission = 1.0f;
      CHECK(intermediate.addSurface("transmission-plan", clear).has_value(),
            "candidate temporarily requires a transmission plan");
      if (!candidate.SetGeometry(std::move(intermediate), 0, material, error)) {
        CHECK(false, error.c_str());
        return std::vector<float>{};
      }
    }
    if (!candidate.SetGeometry(std::move(geometry), 0, material, error)) {
      CHECK(false, error.c_str());
      return std::vector<float>{};
    }
    if (!paletteFirst) { UploadPalette(renderer, roughness, mode, error); }
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
      CHECK((Render::GeometryCullStage::JobsSweptTaken() > 0) == (frame < 2),
            "new worlds cull, refine with previous depth, then reuse unchanged visibility");
    }
    std::vector<float> pixels;
    CHECK(renderer.ReadSceneLinear(pixels) == Render::ReadState::Ready, "linear pixels read");
    return pixels;
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared(SDL_GetError());
    return Report();
  }
  {
    MaterialFixture fixture;
    if (!fixture.Open()) {
      Unprepared(fixture.error.c_str());
      return Report();
    }
    std::array<double, 2> centre{};
    size_t caseIndex = 0;
    for (const float roughness : {0.25f, 0.8f}) {
      const auto native = fixture.RenderMaterial(roughness, false, PaletteMode::Fallback);
      for (const PaletteMode mode :
           {PaletteMode::Fallback, PaletteMode::Classified, PaletteMode::Mixed}) {
        const auto ground = fixture.RenderMaterial(roughness, true, mode);
        const auto earlyPalette = fixture.RenderMaterial(roughness, true, mode, true);
        CHECK(ground == earlyPalette,
              "classification uploaded before geometry retains the same ground pixels");
        CHECK(native.size() == size_t{32} * 32u * 4u && ground.size() == native.size(),
              "both material paths produced complete linear frames");
        if (native.size() != size_t{32} * 32u * 4u || ground.size() != native.size()) { continue; }
        const double worst = CentreDifference(native, ground);
        if (worst >= 0.002) {
          const size_t middle = (size_t{16} * 32u + 16u) * 4u;
          std::println("roughness={:.6g} mode={} worst={:.6g} native={:.6g} ground={:.6g}",
                       roughness,
                       static_cast<int>(mode),
                       worst,
                       native[middle],
                       ground[middle]);
        }
        CHECK(worst < 0.002, "ground palette and native material agree in linear light");
      }
      if (native.size() == size_t{32} * 32u * 4u) {
        centre[caseIndex] = native[(size_t{16} * 32u + 16u) * 4u];
      }
      ++caseIndex;
    }
    CHECK(std::abs(centre[0] - centre[1]) > 0.01,
          "lighting distinguishes roughness, so a constant factor cannot pass");
  }
  SDL_Quit();
  return Report();
}
