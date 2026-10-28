#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "WorldCandidate.h"
#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

Geometry Palette(std::span<const Vec4f> colours) {
  Geometry source;
  for (const auto &colour : colours) {
    Material surface;
    surface.BaseColour = colour;
    surface.Unlit = true;
    CHECK(source.addSurface("owned", surface), "the explicit unlit palette is valid");
  }
  return source;
}

std::expected<PieceHandle, std::string>
Place(SceneRenderer &renderer, PieceSurface surface, double x) {
  const std::array vertices{StoredVertex::Of({{-.4, -.4, 0}}, {{0, 0}}, {{0, 0, 1}}),
                            StoredVertex::Of({{.4, -.4, 0}}, {{1, 0}}, {{0, 0, 1}}),
                            StoredVertex::Of({{.4, .4, 0}}, {{1, 1}}, {{0, 0, 1}}),
                            StoredVertex::Of({{-.4, .4, 0}}, {{0, 1}}, {{0, 0, 1}})};
  constexpr std::array<uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
  const Mat4 model = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x, 0, 0, 1}};
  PieceMesh mesh;
  mesh.Verts = vertices;
  mesh.Indices = indices;
  mesh.Instances = std::span(&model, 1);
  mesh.Surface = std::move(surface);
  return renderer.PlacePiece(mesh);
}

std::vector<float> Read(SceneRenderer &renderer) {
  CHECK(renderer.RenderFrame(), "the sparse material view submits");
  renderer.WaitForGpu();
  std::vector<float> image;
  CHECK(renderer.ReadSceneLinear(image) == ReadState::Ready && image.size() == size_t{32} * 32 * 4,
        "the actual colour frame is readable");
  return image;
}

void Exercise() {
  SceneRenderer renderer;
  Core::Declaration declaration;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
  declaration.Outputs = {"sceneLinear"};
  declaration.Stages = {"opaqueGeometry", "transmissiveGeometry", "compositeTransmission"};
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
        "the normal frame pipeline opens");
  if (!scene) { return; }
  CHECK(renderer.Plan().Holds(Stage::TransmissiveGeometry),
        "both material render paths are active");
  Viewpoint eye;
  eye.EyeM = {{0, 0, 4}};
  eye.Kind = CameraKind::Orthographic;
  eye.XMagM = eye.YMagM = 2;
  eye.ZNearM = .1;
  eye.ZFarM = 10;
  scene->Eye(eye);
  CHECK(scene->Draw(error), "the native camera is configured");
  constexpr std::array<Vec4f, 1> neutral{{{{.2f, .3f, .4f, 1}}}};
  auto first = renderer.RegisterPieceMaterials(Palette(neutral));
  auto hole = renderer.RegisterPieceMaterials(Palette(neutral));
  auto middle = renderer.RegisterPieceMaterials(Palette(neutral));
  auto tail = renderer.RegisterPieceMaterials(Palette(neutral));
  CHECK(first && hole && middle && tail, "four independent source leases reserve rows");
  if (!first || !hole || !middle || !tail) { return; }
  const uint32_t oldHole = hole->First;
  hole->Source.reset();
  tail->Source.reset();
  constexpr std::array<Vec4f, 2> colours{{{{.8f, .2f, .1f, 1}}, {{.1f, .7f, .3f, 1}}}};
  auto palette = renderer.RegisterPieceMaterials(Palette(colours));
  CHECK(palette && palette->At(1) != palette->First + 1,
        "two material offsets genuinely occupy nonadjacent recycled rows");
  if (!palette) { return; }
  CHECK(palette->At(2) == kNoMaterialSlot &&
            palette->At(std::numeric_limits<uint32_t>::max()) == kNoMaterialSlot,
        "invalid offsets cannot alias another live source");
  const auto left = Place(renderer, PieceSurface::Registered(*palette), -1);
  const auto right = Place(renderer, PieceSurface::Registered(*palette, 1), 1);
  CHECK(left && right, "both registered offsets reach actual native geometry");
  if (!left || !right) { return; }
  const auto before = Read(renderer);
  if (before.size() != size_t{32} * 32 * 4) { return; }
  for (size_t channel = 0; channel < 3; ++channel) {
    CHECK(std::abs(before[(size_t{16} * 32 + 8) * 4 + channel] - colours[0][channel]) < .001f &&
              std::abs(before[(size_t{16} * 32 + 24) * 4 + channel] - colours[1][channel]) < .001f,
          "both native pixels use their supplied source-relative material colours");
  }
  CHECK(!Place(renderer, PieceSurface::Registered(oldHole), 0),
        "a stale raw index never acquires the new owner");
  CHECK(renderer.BeginsWorldCandidate(error), "a candidate holds the same live sources");
  renderer.AbandonsWorldCandidate();
  CHECK(Read(renderer) == before, "candidate abandonment preserves both actual colours");
  std::array<MaterialBinding, 2> native;
  CHECK(renderer.SetGeometryMaterials(native, error) && renderer.RestorePieceMaterials(error),
        "a changed native material base restores all sparse registered rows");
  CHECK(Read(renderer) == before, "native base changes preserve every existing pixel");
  {
    Core::WorldCandidate candidate(renderer);
    CHECK(candidate.Prepare(*scene, nullptr) && candidate.SetGeometry(Geometry{}, 0, error) &&
              candidate.Publish(scene),
          "a new world restores and publishes the sparse live material sources");
  }
  scene->Eye(eye);
  CHECK(scene->Draw(error), "the published camera is active");
  CHECK(Read(renderer) == before, "world publication keeps both material offset bindings");
  renderer.ReleasePiece(*left);
  renderer.ReleasePiece(*right);
  palette->Source.reset();
  first->Source.reset();
  middle->Source.reset();
  (void)Read(renderer);
  CHECK(renderer.PiecesStanding() == 0 && renderer.PieceSourceBytes() == 0,
        "the sparse sources release all actual geometry consumers");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for sparse material ownership");
  if (SDL_WasInit(SDL_INIT_VIDEO) != 0) { Exercise(); }
  SDL_Quit();
  return outshine::Test::Report();
}
