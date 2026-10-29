#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "WorldCandidate.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr size_t kSide = 32;

std::array<StoredVertex, 3> Triangle(const Vec3f &normal) {
  return {StoredVertex::Of({{-0.8f, -0.8f, 0}}, {{0, 0}}, normal),
          StoredVertex::Of({{0.8f, -0.8f, 0}}, {{0, 0}}, normal),
          StoredVertex::Of({{0, 0.8f, 0}}, {{0, 0}}, normal)};
}

std::vector<float> Normals(Render::SceneRenderer &renderer, Core::RuntimeScene &scene) {
  std::string error;
  CHECK(scene.Draw(error), "the coplanar product pieces draw");
  renderer.WaitForGpu();
  std::vector<float> pixels;
  CHECK(renderer.ReadShadingNormal(pixels) == Render::ReadState::Ready &&
            pixels.size() == kSide * kSide * 4,
        "actual GPU shading normals are available");
  return pixels;
}

std::vector<float> DrawProductOrder(bool reversed) {
  Geometry base;
  const auto surface = base.addSurface("offscreen", Material{});
  CHECK(surface.has_value(), "the fixture owns a native material");
  if (!surface) { return {}; }
  const auto part = base.addPart("offscreen", *surface);
  CHECK(part && base.setPositions(*part, std::array<float, 9>{10, 0, 0, 11, 0, 0, 10, 1, 0}) &&
            base.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
        "the base material does not cover the product pixels");
  Core::Declaration declaration;
  declaration.InitialGeometry = &base;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = static_cast<int>(kSide);
  declaration.Outputs = {"sceneShadingNormal", "sceneDepth"};
  Render::SceneRenderer renderer;
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), "world opens");
  if (!scene) { return {}; }
  Render::Viewpoint eye;
  eye.EyeM = {{0, 0, 4}};
  eye.Kind = Render::CameraKind::Orthographic;
  eye.XMagM = eye.YMagM = 2;
  eye.ZNearM = 0.1;
  eye.ZFarM = 10;
  scene->Eye(eye);
  Geometry materials;
  CHECK(materials.addSurface("products", Material{}), "the product material is independent");
  const auto registered = renderer.RegisterPieceMaterials(std::move(materials));
  CHECK(registered, "the product material survives removal of the base geometry");
  if (!registered) { return {}; }
  const std::array<std::array<StoredVertex, 3>, 2> vertices{Triangle({{0.6f, 0, 0.8f}}),
                                                            Triangle({{0, 0.6f, 0.8f}})};
  constexpr std::array<uint32_t, 3> indices{0, 1, 2};
  for (size_t step = 0; step < vertices.size(); ++step) {
    const size_t at = reversed ? vertices.size() - 1 - step : step;
    Render::PieceMesh mesh{};
    mesh.Verts = vertices[at];
    mesh.Indices = indices;
    mesh.Surface = Render::PieceSurface::Registered(*registered);
    mesh.ProductOrder = {7, 11, at};
    const auto placed = renderer.PlacePiece(mesh);
    CHECK(placed, "each actual product part keeps its key despite publication order");
    if (!placed) { return {}; }
  }
  const auto before = Normals(renderer, *scene);
  if (before.size() != kSide * kSide * 4) { return {}; }
  const size_t centre = (kSide / 2 * kSide + kSide / 2) * 4;
  CHECK(before[centre + 2] > 0.7f && (before[centre] > 0.5f || before[centre + 1] > 0.5f),
        "the probe samples a real differently shaded overlapping product");
  Core::WorldCandidate candidate(renderer);
  const auto prepared = candidate.Prepare(*scene, nullptr);
  CHECK(prepared, "world replacement prepares");
  if (!prepared) { return {}; }
  CHECK(candidate.SetGeometry(Geometry{}, 0, error), "the base is removed during replacement");
  const auto published = candidate.Publish(scene);
  CHECK(published, "owned product pieces restore into the replacement world");
  if (!published) { return {}; }
  scene->Eye(eye);
  const auto after = Normals(renderer, *scene);
  CHECK(after == before, "world restore preserves product order and every shaded pixel");
  return after;
}
}

int main() {
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    const auto forward = DrawProductOrder(false);
    const auto reverse = DrawProductOrder(true);
    CHECK(!forward.empty() && forward == reverse,
          "opposite publication orders of identical products render identical GPU pixels");
  }
  SDL_Quit();
  return Report();
}
