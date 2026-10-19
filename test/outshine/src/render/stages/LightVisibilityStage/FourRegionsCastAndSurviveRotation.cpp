#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include <SDL3/SDL.h>
#include <array>
#include <memory>
#include <expected>
#include <utility>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {
constexpr float kHalfSizeM = 20;
constexpr size_t kCoordinates = 24;
constexpr size_t kTriangleIndices = 36;
constexpr double kRadiusM = 8192;
constexpr double kKeyLux = 10000;
constexpr double kKeyElevationDeg = 45;
constexpr double kEyeHeightM = 100;
constexpr double kNearM = 0.1;
constexpr double kFarM = 20000;
constexpr double kTranslationStartM = 10.125;
constexpr double kRetryTranslationM = 0.25;
constexpr float kDistantCasterM = 6000;

bool Cube(outshine::Geometry &geometry, float x) {
  const auto part = geometry.addPart("caster", outshine::MaterialInstance{});
  if (!part) { return false; }
  std::array<float, kCoordinates> vertices{
      -kHalfSizeM, -kHalfSizeM, -kHalfSizeM, kHalfSizeM,  -kHalfSizeM, -kHalfSizeM,
      kHalfSizeM,  kHalfSizeM,  -kHalfSizeM, -kHalfSizeM, kHalfSizeM,  -kHalfSizeM,
      -kHalfSizeM, -kHalfSizeM, kHalfSizeM,  kHalfSizeM,  -kHalfSizeM, kHalfSizeM,
      kHalfSizeM,  kHalfSizeM,  kHalfSizeM,  -kHalfSizeM, kHalfSizeM,  kHalfSizeM};
  for (size_t at = 0; at < vertices.size(); at += 3) { vertices[at] += x; }
  return geometry.setPositions(*part, vertices) &&
         geometry.setTriangles(
             *part, std::array<uint32_t, kTriangleIndices>{0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
                                                           0, 1, 5, 0, 5, 4, 3, 7, 6, 3, 6, 2,
                                                           0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5});
}

bool Occupied(const std::vector<float> &atlas, size_t region, bool leftQuarter = false) {
  const size_t firstX = (region % 2) * outshine::Render::kShadowTilePx;
  const size_t firstY = (region / 2) * outshine::Render::kShadowTilePx;
  const size_t width = outshine::Render::kShadowTilePx / (leftQuarter ? 4 : 1);
  if (atlas.size() !=
      static_cast<size_t>(outshine::Render::kShadowAtlasPx) * outshine::Render::kShadowAtlasPx) {
    return false;
  }
  for (size_t y = firstY; y < firstY + outshine::Render::kShadowTilePx; ++y) {
    for (size_t x = firstX; x < firstX + width; ++x) {
      if (atlas[y * outshine::Render::kShadowAtlasPx + x] > 0.0f) { return true; }
    }
  }
  return false;
}

bool SameTile(const std::vector<float> &a, const std::vector<float> &b, size_t region) {
  using namespace outshine::Render;
  if (a.size() != b.size()) { return false; }
  const size_t firstX = (region % 2) * kShadowTilePx;
  const size_t firstY = (region / 2) * kShadowTilePx;
  for (size_t y = firstY; y < firstY + kShadowTilePx; ++y) {
    for (size_t x = firstX; x < firstX + kShadowTilePx; ++x) {
      const size_t at = y * kShadowAtlasPx + x;
      if (a[at] != b[at]) { return false; }
    }
  }
  return true;
}

void Translation(outshine::Core::RuntimeScene &scene,
                 outshine::Render::SceneRenderer &renderer,
                 outshine::Render::Viewpoint eye,
                 bool &reject) {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  std::string error;
  const auto rebuild = [&] {
    renderer.SetShadowFrame({{1, 0, 0}}, {{0, 1, 0}}, kRadiusM, true, 0);
    renderer.SetShadowFrame({{0, 0, 1}}, {{0, 1, 0}}, kRadiusM, true, 0);
    CHECK(scene.Draw(error), "forced full shadow reference submits");
    renderer.WaitForGpu();
  };
  eye.EyeM[0] = kTranslationStartM;
  scene.Eye(eye);
  rebuild();
  const size_t full = renderer.ShadowCastCount();
  CHECK(full > 0, "full reference draws casters");
  std::vector<float> before;
  std::vector<float> partial;
  std::vector<float> reference;
  CHECK(renderer.ReadShadowAtlas(before) == ReadState::Ready, "initial reference is readable");
  eye.EyeM[0] += 0.5;
  scene.Eye(eye);
  CHECK(scene.Draw(error), "half-metre camera translation submits");
  renderer.WaitForGpu();
  CHECK(renderer.ShadowCastCount() * 4 == full, "only the changed nearest region draws casters");
  CHECK(renderer.ReadShadowAtlas(partial) == ReadState::Ready, "partial update is readable");
  CHECK(!SameTile(before, partial, 0), "nearest shadow depths follow the translated projection");
  for (size_t region = 1; region < 4; ++region) {
    CHECK(SameTile(before, partial, region), "unchanged regions preserve every depth texel");
  }
  rebuild();
  CHECK(renderer.ShadowCastCount() == full, "explicit light invalidation updates all regions");
  CHECK(renderer.ReadShadowAtlas(reference) == ReadState::Ready && reference == partial,
        "partial atlas equals the complete current-camera reference");
  eye.EyeM[0] += kRetryTranslationM;
  scene.Eye(eye);
  reject = true;
  CHECK(!scene.Draw(error) && !reject, "partial shadow submission is actually rejected");
  CHECK(scene.Draw(error), "rejected partial update retries successfully");
  renderer.WaitForGpu();
  CHECK(renderer.ShadowCastCount() == full, "retry conservatively rebuilds every shadow region");
  CHECK(renderer.ReadShadowAtlas(partial) == ReadState::Ready, "retried atlas is readable");
  rebuild();
  CHECK(renderer.ReadShadowAtlas(reference) == ReadState::Ready && reference == partial,
        "rejected partial submission preserves the complete reference on retry");
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for regional shadow rendering");
  {
    Geometry geometry;
    CHECK(Cube(geometry, 0) && Cube(geometry, kDistantCasterM),
          "near and distant closed casters are valid");
    Core::Declaration declaration;
    declaration.InitialGeometry = &geometry;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface", "sceneLinear", "shadowAtlas"};
    declaration.DrawsSky = true;
    declaration.ShadowRadiusM = kRadiusM;
    declaration.KeyLux = kKeyLux;
    declaration.KeyElevationDeg = kKeyElevationDeg;
    bool reject = false;
    SceneRenderer renderer(
        {.Context = &reject,
         .Submit =
             [](void *context, Native::Device *device, std::unique_ptr<Native::Commands> commands)
             -> Native::Result<Native::Completion> {
           if (std::exchange(*static_cast<bool *>(context), false)) {
             return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Driver,
                                                  .Message = "injected shadow submit failure"});
           }
           return device->Submit(std::move(commands));
         }});
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
          "caster world opens");
    if (scene) {
      Viewpoint eye;
      eye.EyeM = {{0, 0, kEyeHeightM}};
      eye.YfovRad = 1;
      eye.ZNearM = kNearM;
      eye.ZFarM = kFarM;
      scene->Eye(eye);
      renderer.SetShadowFrame({{0, 0, 1}}, {{0, 1, 0}}, kRadiusM, true, 0);
      CHECK(scene->Draw(error), "four regional shadow passes submit");
      renderer.WaitForGpu();
      std::vector<float> first;
      CHECK(renderer.ReadShadowAtlas(first) == ReadState::Ready, "the depth atlas is readable");
      for (size_t region = 0; region < 4; ++region) {
        CHECK(Occupied(first, region), "each independent atlas tile contains its caster depths");
      }
      CHECK(Occupied(first, 3, true), "the far tile retains the six-kilometre offscreen caster");
      eye.Forward = {{1, 0, 0}};
      eye.Right = {{0, 0, 1}};
      scene->Eye(eye);
      CHECK(scene->Draw(error), "camera rotation renders without changing world coverage");
      renderer.WaitForGpu();
      std::vector<float> second;
      CHECK(renderer.ReadShadowAtlas(second) == ReadState::Ready && first == second,
            "rotation preserves every submitted depth texel");
      Translation(*scene, renderer, eye, reject);
    }
  }
  SDL_Quit();
  return Report();
}
