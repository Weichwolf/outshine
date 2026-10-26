#include "TerrainLattice.h"
#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <numbers>
#include <print>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr int kFrameSide = 128;
constexpr double kTileHalfWidthM = 128;
constexpr double kEyeHeightM = 100;
constexpr double kFarPlaneM = 1000;
constexpr double kSunLux = 10000;
constexpr double kSunElevationDeg = 45;
constexpr double kShadowRadiusM = 256;
constexpr float kBaseHeightM = 2;
constexpr float kRippleHeightM = 0.25f;
constexpr float kRipplePhase = std::numbers::pi_v<float> / 2;
constexpr Vec3f kToSun{{0, 0.70710677f, 0.70710677f}};
constexpr size_t kSampleFirst = 24;
constexpr size_t kSampleLimit = 104;
constexpr double kMaximumRelativeDrop = 0.02;
constexpr size_t kFallbackPaletteFloats = 17;
constexpr float kVerticalSlopeDeg = 90;

bool PlaceTerrain(SceneRenderer &renderer, std::string &error) {
  MaterialBinding material;
  material.Domain = SurfaceDomain::Ground;
  material.Row.BaseColour = {{0.5f, 0.5f, 0.5f, 1}};
  material.Row.Roughness = 1;
  if (!renderer.SetGeometryMaterials({&material, 1}, error)) { return false; }
  const std::array<uint32_t, 4> classes{{0, 0, std::numeric_limits<uint32_t>::max(), 0}};
  std::array<float, kFallbackPaletteFloats> palette{};
  std::ranges::copy(material.Row.BaseColour, palette.begin() + 4);
  palette[8] = kVerticalSlopeDeg;
  if (!renderer.SetGroundClasses(classes, palette, error)) { return false; }
  std::vector<float> nodes(TerrainLattice::kPageNodes);
  for (int y = 0; y < TerrainLattice::kPageSide; ++y) {
    for (int x = 0; x < TerrainLattice::kPageSide; ++x) {
      nodes[static_cast<size_t>(y) * TerrainLattice::kPageSide + static_cast<size_t>(x)] =
          kBaseHeightM - kRippleHeightM * std::cos(static_cast<float>(x - 1) * kRipplePhase) *
                             std::cos(static_cast<float>(y - 1) * kRipplePhase);
    }
  }
  const auto page = renderer.PlaceHeightPage(nodes);
  CHECK(page.has_value(), "fine source heights are resident");
  if (!page) { return false; }
  std::vector<float> fractions(TerrainLattice::kSide);
  for (size_t at = 0; at < fractions.size(); ++at) {
    fractions[at] = static_cast<float>(at) / static_cast<float>(fractions.size() - 1u);
  }
  TerrainTile tile;
  tile.Row = {{1, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 1}};
  tile.Page = *page;
  tile.Corners = {{-kTileHalfWidthM,
                   kTileHalfWidthM,
                   kTileHalfWidthM,
                   kTileHalfWidthM,
                   -kTileHalfWidthM,
                   -kTileHalfWidthM,
                   kTileHalfWidthM,
                   -kTileHalfWidthM}};
  tile.LowM = kBaseHeightM - kRippleHeightM;
  tile.HighM = kBaseHeightM + kRippleHeightM;
  tile.StepE = tile.StepN = 2 * kTileHalfWidthM / (TerrainLattice::kSide - 1);
  return renderer.SetGroundGrid(fractions, error) &&
         renderer.SetTerrainTiles({&tile, 1}, {}, error) && renderer.Settle(error);
}

void CheckUnoccluded(const std::vector<float> &reference, const std::vector<float> &receiving) {
  constexpr size_t components = size_t{kFrameSide} * kFrameSide * 4;
  CHECK(reference.size() == components && receiving.size() == components,
        "both controls contain the complete linear frame");
  if (reference.size() != components || receiving.size() != components) { return; }
  const auto finite = [](float value) { return std::isfinite(value) && value >= 0; };
  CHECK(std::ranges::all_of(reference, finite) && std::ranges::all_of(receiving, finite),
        "both lighting controls have finite nonnegative radiance");
  double worstDrop = 0;
  double leastReference = std::numeric_limits<double>::infinity();
  for (size_t y = kSampleFirst; y < kSampleLimit; ++y) {
    for (size_t x = kSampleFirst; x < kSampleLimit; ++x) {
      for (size_t channel = 0; channel < 3; ++channel) {
        const size_t at = (y * kFrameSide + x) * 4 + channel;
        leastReference = std::min(leastReference, static_cast<double>(reference[at]));
        worstDrop = std::max(worstDrop,
                             (static_cast<double>(reference[at]) - receiving[at]) /
                                 std::max(1.0, static_cast<double>(reference[at])));
      }
    }
  }
  std::println("TERRAIN_RECEIVER worst_relative_drop={:.9f} least_reference={:.9f}",
               worstDrop,
               leastReference);
  CHECK(leastReference > 0 && worstDrop < kMaximumRelativeDrop,
        "a sun-facing fine heightfield casts no false shadows onto its adaptive receiver");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for adaptive terrain shadows");
  {
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = kFrameSide;
    declaration.Outputs = {"surface", "sceneLinear", "sceneDepth", "shadowAtlas"};
    declaration.Stages = {"sky", "opaqueGeometry", "ground", "lightVisibility"};
    declaration.DrawsSky = true;
    declaration.KeyLux = kSunLux;
    declaration.KeyElevationDeg = kSunElevationDeg;
    SceneRenderer renderer;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
          "an isolated terrain receiver opens");
    if (!scene) { return Report(); }
    const bool placed = PlaceTerrain(renderer, error);
    CHECK(placed, "the independent fine caster is complete");
    if (!placed) {
      std::println(stderr, "terrain receiver setup: {}", error);
      return Report();
    }
    auto eye = Viewpoint::LookAt({.EyeM = {{0, kEyeHeightM, kEyeHeightM}}, .AimM = {{0, 0, 0}}}, 0);
    CHECK(eye.has_value(), "the downward camera has a valid frame");
    if (!eye) { return Report(); }
    eye->Kind = CameraKind::Orthographic;
    eye->XMagM = eye->YMagM = kTileHalfWidthM;
    eye->ZNearM = 1;
    eye->ZFarM = kFarPlaneM;
    scene->Eye(*eye);
    renderer.SetShadowFrame(kToSun, {{0, 1, 0}}, 0, true, 0);
    CHECK(scene->Draw(error), "the same adaptive surface renders without shadows");
    renderer.WaitForGpu();
    std::vector<float> depth;
    constexpr size_t centre = size_t{kFrameSide / 2} * kFrameSide + kFrameSide / 2;
    CHECK(renderer.ReadDepth(depth) == ReadState::Ready && depth.size() > centre &&
              depth[centre] > 0,
          "the adaptive terrain is actually rendered in the colour pass");
    const auto draws = renderer.ReadGroundDrawArguments();
    CHECK(draws.has_value() && std::any_of(draws->Patches.begin() + 1,
                                           draws->Patches.end(),
                                           [](const auto &draw) { return draw.InstanceCount > 0; }),
          "the colour pass genuinely uses coarser patches than the fine caster");
    std::vector<float> reference;
    CHECK(renderer.ReadSceneLinear(reference) == ReadState::Ready, "unoccluded radiance reads");
    renderer.SetShadowFrame(kToSun, {{0, 1, 0}}, kShadowRadiusM, true, 0);
    CHECK(scene->Draw(error), "the fine heightfield casts onto its adaptive representation");
    renderer.WaitForGpu();
    std::vector<float> receiving;
    std::vector<float> atlas;
    CHECK(renderer.ReadSceneLinear(receiving) == ReadState::Ready, "shadowed radiance reads");
    CHECK(renderer.ReadShadowAtlas(atlas) == ReadState::Ready &&
              std::ranges::any_of(atlas, [](float value) { return value > 0; }),
          "the test genuinely renders fine terrain into the native shadow atlas");
    CheckUnoccluded(reference, receiving);
    std::array<float, TerrainLattice::kSide> nonuniform;
    for (size_t at = 0; at < nonuniform.size(); ++at) {
      const float offset = at % 2 != 0 ? kRippleHeightM : 0;
      nonuniform[at] = (static_cast<float>(at) + offset) / (TerrainLattice::kSide - 1);
    }
    CHECK(renderer.SetGroundGrid(nonuniform, error) && scene->Draw(error),
          "nonuniform terrain keeps its source grid");
    renderer.WaitForGpu();
    const auto fine = renderer.ReadGroundDrawArguments();
    CHECK(fine.has_value() && std::none_of(fine->Patches.begin() + 1,
                                           fine->Patches.end(),
                                           [](const auto &draw) { return draw.InstanceCount > 0; }),
          "nonuniform grids remain fine and never use the uniform coarse-receiver contract");
  }
  SDL_Quit();
  return Report();
}
