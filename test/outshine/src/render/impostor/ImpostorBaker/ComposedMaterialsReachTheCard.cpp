#include "ImpostorBaker.h"
#include "ImpostorSurface.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "Check.h"
#include "math/Srgb.h"

#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;

enum class Finish { Textured, Facade, Mapped };

Geometry Fixture(Finish finish) {
  const bool facade = finish == Finish::Facade;
  using namespace outshine::Test;
  Geometry mesh;
  Material material;
  material.DoubleSided = true;
  material.BaseColour = {{0.8f, 0.6f, 0.4f, 1}};
  material.Metalness = 0.8f;
  material.Roughness = 0.75f;
  if (facade) {
    material.Pattern = SurfacePattern::Facade;
  } else {
    material.BaseColourMap.Image = *mesh.addImage(1, 1, std::array<uint8_t, 4>{188, 137, 225, 255});
    material.MetalRoughMap.Image = *mesh.addImage(1, 1, std::array<uint8_t, 4>{255, 128, 64, 255});
  }
  if (finish == Finish::Mapped) {
    material.NormalMap.Image = *mesh.addImage(1, 1, std::array<uint8_t, 4>{218, 218, 128, 255});
  }
  const auto surface = *mesh.addSurface("composed", material);
  const auto part = *mesh.addPart("triangle", surface);
  CHECK(mesh.setPositions(part, std::array<float, 9>{-0.5f, -0.5f, 0, 0.5f, -0.5f, 0, 0, 0.5f, 0}),
        "positions");
  CHECK(mesh.setNormals(part, std::array<float, 9>{0, 0, 1, 0, 0, 1, 0, 0, 1}), "normals");
  CHECK(mesh.setTriangles(part, std::array<uint32_t, 3>{0, 1, 2}), "triangles");
  CHECK(mesh.setColours(part,
                        std::array<float, 12>{
                            0.5f, 0.75f, 0.25f, 1, 0.5f, 0.75f, 0.25f, 1, 0.5f, 0.75f, 0.25f, 1}),
        "colours");
  CHECK(mesh.setTexture(part,
                        facade ? std::array<float, 6>{-9, 0, -9, 1, -9, 2}
                               : std::array<float, 6>{0, 0, 1, 0, 0.5f, 1}),
        "UVs");
  if (finish == Finish::Mapped) {
    CHECK(mesh.setTangents(part, std::array<float, 12>{1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1}),
          "mapped tangent basis");
  }
  return mesh;
}

void CheckCurrentLighting(const Geometry &card) {
  using namespace outshine::Test;
  Core::Declaration declaration;
  declaration.InitialGeometry = &card;
  declaration.KeyLux = 100;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 64;
  declaration.Outputs = {"sceneLinear", "sceneDepth"};
  Render::SceneRenderer renderer;
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), "card world opens");
  if (!scene) { return; }
  Render::Viewpoint eye;
  eye.EyeM = {{0, 0, 4}};
  eye.Kind = Render::CameraKind::Orthographic;
  eye.XMagM = eye.YMagM = 1;
  eye.ZNearM = 0.1;
  eye.ZFarM = 10;
  scene->Eye(eye);
  CHECK(scene->Draw(error), "camera and card publish through the runtime scene");
  renderer.SetSubjectEnvironment({});
  CHECK(renderer.SetSubjectLights({}, error), "capture light is absent from runtime lighting");
  CHECK(renderer.RenderFrame(), "unlit card frame draws");
  renderer.WaitForGpu();
  std::vector<float> before, after, depth;
  CHECK(renderer.ReadSceneLinear(before) == Render::ReadState::Ready &&
            renderer.ReadDepth(depth) == Render::ReadState::Ready,
        "card pixels read back");
  Render::SubjectLight key;
  key.Light.Kind = LightKind::Directional;
  key.Light.Intensity = 100;
  const float axis = -1 / std::sqrt(3.0f);
  key.Light.Direction = {{axis, axis, axis}};
  CHECK(renderer.SetSubjectLights(std::span(&key, 1), error),
        "current light changes without recapture");
  CHECK(renderer.RenderFrame(), "relit card frame draws");
  renderer.WaitForGpu();
  CHECK(renderer.ReadSceneLinear(after) == Render::ReadState::Ready, "relit pixels read back");
  size_t relit = 0;
  if (before.size() == after.size() && before.size() == depth.size() * 4) {
    for (size_t at = 0; at < depth.size(); ++at) {
      if (depth[at] == 0) { continue; }
      CHECK(before[at * 4] < 0.001f, "material capture contains no frozen incident light");
      if (after[at * 4] > before[at * 4] + 0.001f) { ++relit; }
    }
  }
  CHECK(relit > 30, "native card responds to current lighting through its material maps");
}

void CheckCapture(Finish finish) {
  const bool facade = finish == Finish::Facade;
  using namespace outshine::Test;
  Render::ImpostorCapture capture;
  capture.LeastM = {{-0.5, -0.5, -0.1}};
  capture.MostM = {{0.5, 0.5, 0.1}};
  capture.Sources.push_back({.Mesh = Fixture(finish), .Instances = {Mat4{}}});
  std::string error;
  const auto atlas =
      Render::ImpostorBaker::Bake(std::move(capture), {.Pixels = 17, .Views = 1}, error);
  CHECK(atlas.has_value(), error.c_str());
  if (!atlas) { return; }
  const auto card = Render::BuildImpostorSurface(*atlas, 0);
  CHECK(card && card->wellFormed(), "captured material channels reach a native card");
  if (!card) { return; }
  const Vec3f expected = facade ? Vec3f{{0.8f * 0.68f, 0.6f * 0.70f, 0.4f * 0.72f}}
                                : Vec3f{{0.8f * ColourSpace::LinearFromSrgb(188.0f / 255) * 0.5f,
                                         0.6f * ColourSpace::LinearFromSrgb(137.0f / 255) * 0.75f,
                                         0.4f * ColourSpace::LinearFromSrgb(225.0f / 255) * 0.25f}};
  const float roughness = facade ? 0.91f : 0.75f * 128 / 255;
  const float metalness = facade ? 0.8f : 0.8f * 64 / 255;
  const auto &view = atlas->Views()[0];
  CHECK(view.Materials.size() == view.Texels.size(),
        "every capture pixel owns resolved material values");
  size_t covered = 0;
  for (size_t at = 0; at < view.Texels.size(); ++at) {
    if (view.Texels[at].Surface == 0) { continue; }
    ++covered;
    const auto &sample = view.Materials[at];
    if (finish == Finish::Mapped) {
      CHECK(view.Texels[at].Normal[0] > 0.70f && view.Texels[at].Normal[1] > 0.70f,
            "capture resolves the normal map through the uploaded tangent frame");
    }
    for (size_t c = 0; c < 3; ++c) {
      CHECK(
          std::abs(sample.BaseColour[c] - expected[c]) < 0.001f,
          "linear capture preserves texture, vertex paint or facade composition without lighting");
      const int byte =
          static_cast<int>(std::lround(255 * ColourSpace::SrgbFromLinear(expected[c])));
      CHECK(std::abs(static_cast<int>(card->imageAt(0).Rgba[at * 4 + c]) - byte) <= 1,
            "card consumes composed colour rather than the material row's base factor");
    }
    CHECK(std::abs(sample.Roughness - roughness) < 0.001f &&
              std::abs(sample.Metalness - metalness) < 0.001f,
          "capture keeps independently composed roughness and metallic channels");
    CHECK(std::abs(static_cast<int>(card->imageAt(2).Rgba[at * 4 + 1]) -
                   static_cast<int>(std::lround(roughness * 255))) <= 1 &&
              std::abs(static_cast<int>(card->imageAt(2).Rgba[at * 4 + 2]) -
                       static_cast<int>(std::lround(metalness * 255))) <= 1,
          "card maps retain the capture's material response");
  }
  CHECK(covered > 30, "fixture checks a rendered surface, not an empty capture");
  CheckCurrentLighting(*card);
}
}

int main() {
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  CheckCapture(Finish::Textured);
  CheckCapture(Finish::Facade);
  CheckCapture(Finish::Mapped);
  SDL_Quit();
  return Report();
}
