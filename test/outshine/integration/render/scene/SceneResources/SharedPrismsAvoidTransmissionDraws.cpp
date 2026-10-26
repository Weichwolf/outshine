#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "WorldCandidate.h"
#include "Vec4.h"
#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr uint32_t kSide = 32;
constexpr size_t kCentre = size_t{kSide / 2} * kSide + kSide / 2;
constexpr Vec4f kRed{{.7f, .2f, .1f, 1}};

Geometry NativeOwner() {
  Geometry geometry;
  Material glass;
  glass.Transmission = 1;
  for (const auto &material : {Material{}, glass}) {
    const auto *name = material.Transmission > 0 ? "glass owner" : "opaque owner";
    const auto surface = geometry.addSurface(name, material);
    CHECK(surface, "the native owner declares both opaque and glass materials");
    if (!surface) { return {}; }
    const auto part = geometry.addPart(name, *surface);
    constexpr float kDistantM = 100000;
    constexpr std::array<float, 9> positions{kDistantM, 0, 0, kDistantM + 1, 0, 0, kDistantM, 1, 0};
    CHECK(part && geometry.setPositions(*part, positions) &&
              geometry.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
          "the material owner remains outside the camera");
  }
  return geometry;
}

struct Image {
  std::vector<float> Colour, Depth, Velocity;
};

Image Draw(Core::RuntimeScene &scene, SceneRenderer &renderer) {
  std::string error;
  CHECK(scene.Draw(error), error.empty() ? "the mixed scene submits" : error.c_str());
  Image image;
  CHECK(renderer.ReadSceneLinear(image.Colour) == ReadState::Ready &&
            renderer.ReadDepth(image.Depth) == ReadState::Ready &&
            renderer.ReadSceneVelocity(image.Velocity) == ReadState::Ready,
        "colour, depth and velocity reach the actual native readbacks");
  CHECK(image.Colour.size() == size_t{kSide} * kSide * 4 &&
            image.Depth.size() == size_t{kSide} * kSide &&
            image.Velocity.size() == size_t{kSide} * kSide * 2,
        "all native attachments have complete extents");
  return image;
}

void CheckPublication(std::unique_ptr<Core::RuntimeScene> &scene,
                      SceneRenderer &renderer,
                      PrismGridHandle prism) {
  {
    Core::WorldCandidate abandoned(renderer);
    CHECK(abandoned.Prepare(*scene, nullptr), "a private replacement restores the prism source");
    const auto published = Draw(*scene, renderer);
    CHECK(published.Depth.size() > kCentre && published.Depth[kCentre] > 0,
          "published geometry remains valid while another scene owns candidate products");
  }
  CHECK(renderer.HasPrismGrid(prism), "discarding a candidate retains the published handle");
  Core::WorldCandidate replacement(renderer);
  CHECK(replacement.Prepare(*scene, nullptr) && replacement.Publish(scene),
        "the actual world publication moves pass borrowers with their stable scene owner");
  CHECK(renderer.HasPrismGrid(prism), "the same handle resolves the newly published product");
  const auto restored = Draw(*scene, renderer);
  CHECK(restored.Depth.size() > kCentre && restored.Depth[kCentre] > 0 &&
            renderer.Spent(Stage::TransmissiveGeometry).DeviceBytes == 0,
        "the published prism still renders and is counted once");
}

void Exercise() {
  SceneRenderer renderer;
  const auto native = NativeOwner();
  Core::Declaration declaration;
  declaration.InitialGeometry = &native;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = kSide;
  declaration.Outputs = {"surface", "sceneLinear", "sceneDepth", "sceneVelocity"};
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), error.c_str());
  if (!scene) { return; }
  CHECK(renderer.Plan().Holds(Stage::TransmissiveGeometry),
        "the same frame really contains the transmission pass");
  Viewpoint eye;
  eye.EyeM = {{0, 0, 10}};
  eye.Kind = CameraKind::Orthographic;
  eye.XMagM = eye.YMagM = 4;
  constexpr double kNearM = .1;
  constexpr double kFarM = 100;
  eye.ZNearM = kNearM;
  eye.ZFarM = kFarM;
  scene->Eye(eye);
  (void)Draw(*scene, renderer);
  const auto baseline = renderer.Spent(Stage::Geometry).DeviceBytes;
  const auto glassDraws = renderer.Spent(Stage::TransmissiveGeometry).Draws;
  Geometry materials;
  Material red;
  red.BaseColour = kRed;
  CHECK(materials.addSurface("prism", red), "an opaque PBR prism material exists");
  const auto material = renderer.RegisterPieceMaterials(std::move(materials));
  CHECK(material, "the prism material reaches the shared scene");
  if (!material) { return; }
  const auto source = std::make_shared<const PrismGrid>(
      PrismGrid{.Samples = std::make_shared<const std::vector<PrismSpan>>(
                    std::vector<PrismSpan>{{.Lower = -1, .Upper = 1}}),
                .Patches = {{.Columns = 1, .Rows = 1, .PitchX = 2, .PitchZ = 2}}});
  PrismInstance instance;
  instance.Current[12] = instance.Current[14] = -1;
  instance.Previous = instance.Current;
  const auto prism = renderer.PlacePrismGrid(source, std::span(&instance, 1), *material);
  CHECK(prism, "one prism becomes resident");
  if (!prism) { return; }
  SubjectLight light;
  constexpr float kLux = 10000;
  light.Light.Intensity = kLux;
  light.Light.Direction = {{0, 0, -1}};
  CHECK(renderer.SetGeometryLights(std::span(&light, 1), error), "a frontal key reaches the prism");
  const auto first = Draw(*scene, renderer);
  if (first.Colour.size() <= kCentre * 4 || first.Depth.size() <= kCentre) { return; }
  CHECK(first.Depth[kCentre] > 0 && first.Colour[kCentre * 4] > first.Colour[kCentre * 4 + 1],
        "the shared prism really reaches depth and red PBR colour");
  CHECK(renderer.Spent(Stage::Geometry).DeviceBytes ==
                baseline + sizeof(PrismSpan) + sizeof(GpuPrismPatch) &&
            renderer.Spent(Stage::TransmissiveGeometry).DeviceBytes == 0 &&
            renderer.Spent(Stage::TransmissiveGeometry).Draws == glassDraws,
        "one owner counts the payload once and glass records no duplicate opaque prism draw");
  constexpr float kMovedX = -.5f;
  instance.Current[12] = kMovedX;
  CHECK(renderer.SetPrismGridInstances(*prism, std::span(&instance, 1)), "the same prism moves");
  const auto moved = Draw(*scene, renderer);
  constexpr float kMinimumMotion = .01f;
  CHECK(moved.Velocity.size() > kCentre * 2 &&
            std::abs(moved.Velocity[kCentre * 2]) > kMinimumMotion,
        "the shared residency preserves distinct previous/current placements");
  CheckPublication(scene, renderer, *prism);
  renderer.ReleasePrismGrid(*prism);
  const auto released = Draw(*scene, renderer);
  CHECK(released.Depth.size() > kCentre && released.Depth[kCentre] == 0 &&
            renderer.Spent(Stage::Geometry).DeviceBytes == baseline,
        "releasing the shared product removes its pixels and resident byte count");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  if (SDL_WasInit(SDL_INIT_VIDEO) != 0) { Exercise(); }
  SDL_Quit();
  return Report();
}
