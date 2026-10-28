#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {
outshine::Geometry Source() {
  using namespace outshine;
  Geometry source;
  Material material;
  material.Unlit = true;
  material.BaseColourMap.Image =
      source.addImage(1, 1, std::array<uint8_t, 4>{32, 128, 64, 255}).value();
  (void)source.addSurface("owned image", material).value();
  return source;
}

std::vector<float> Image(outshine::Render::SceneRenderer &renderer) {
  using namespace outshine::Render;
  using namespace outshine::Test;
  CHECK(renderer.RenderFrame(), "the live piece view submits");
  renderer.WaitForGpu();
  std::vector<float> result;
  CHECK(renderer.ReadSceneLinear(result) == ReadState::Ready &&
            result.size() == size_t{32} * 32 * 4,
        "native material colours read after synchronization");
  return result;
}

void Exercise() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  SceneRenderer renderer;
  Core::Declaration declaration;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
  declaration.Outputs = {"sceneLinear"};
  declaration.Stages = {"opaqueGeometry"};
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
        "source ownership scene opens");
  if (!scene) { return; }
  Viewpoint eye;
  eye.EyeM = {{0, 0, 4}};
  eye.Kind = CameraKind::Orthographic;
  eye.XMagM = eye.YMagM = 2;
  eye.ZNearM = .1;
  eye.ZFarM = 10;
  scene->Eye(eye);
  CHECK(scene->Draw(error), "the declared native view is active");
  auto registration = renderer.RegisterPieceMaterials(Source());
  CHECK(registration.has_value(), "immutable texture source registers");
  if (!registration) { return; }
  const std::weak_ptr<const MaterialSource> held = registration->Source;
  const auto firstMaterial = registration->First;
  const std::array<StoredVertex, 4> vertices{StoredVertex::Of({{-1, -1, 0}}, {{0, 1}}, {{0, 0, 1}}),
                                             StoredVertex::Of({{1, -1, 0}}, {{1, 1}}, {{0, 0, 1}}),
                                             StoredVertex::Of({{1, 1, 0}}, {{1, 0}}, {{0, 0, 1}}),
                                             StoredVertex::Of({{-1, 1, 0}}, {{0, 0}}, {{0, 0, 1}})};
  constexpr std::array<uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
  PieceMesh mesh;
  mesh.Verts = vertices;
  mesh.Indices = indices;
  mesh.Textured = true;
  mesh.Surface = PieceSurface::Registered(*registration);
  const auto first = renderer.PlacePiece(mesh);
  const auto second = renderer.PlacePiece(mesh);
  CHECK(first && second, "two live pieces share the same immutable material source");
  if (!first || !second) { return; }
  registration->Source.reset();
  mesh.Surface = {};
  CHECK(!held.expired(),
        "live pieces retain image bytes after the producer releases its registration");
  const auto before = Image(renderer);
  CHECK(!before.empty() && before[(size_t{16} * 32 + 16) * 4 + 1] > .1f,
        "the actual surviving source is green and nonempty");
  renderer.ReleasePiece(*first);
  CHECK(!held.expired(), "one retired piece does not retire its live sibling's source");
  mesh.Surface = PieceSurface::Registered(firstMaterial);
  CHECK(!renderer.PlacePiece(mesh), "a raw registered slot cannot invent source ownership");
  CHECK(Image(renderer) == before,
        "sibling retirement and rejected aliases preserve actual colours");
  renderer.ReleasePiece(*second);
  CHECK(held.expired(), "the last actual consumer releases immutable CPU material and image data");
  const auto empty = Image(renderer);
  double radiance = 0;
  for (size_t at = 0; at < empty.size(); ++at) {
    if (at % 4 != 3) { radiance += empty[at]; }
  }
  CHECK(!empty.empty() && radiance == 0 && renderer.PieceSourceCount() == 0,
        "retired pieces leave neither visible geometry nor owned material sources");
}
}

int main() {
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for material source ownership");
  if (SDL_WasInit(SDL_INIT_VIDEO) != 0) { Exercise(); }
  SDL_Quit();
  return Report();
}
